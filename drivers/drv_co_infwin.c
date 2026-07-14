/**
 * @file  drv_co_infwin.c
 * @brief Infwin CO sensor driver — USART1 @ 9600 baud.
 *
 * Active only when USART1_SENSOR_MODE is defined (see drv_co_infwin.h).
 *
 * Protocol (Modbus RTU subset):
 *   Request  (8 bytes): 62 03 00 03 00 01 [CRC_L] [CRC_H]
 *   Response (7 bytes): 62 03 02 [CO_H] [CO_L] [CRC_L] [CRC_H]
 */

#include "drivers/drv_co_infwin.h"

#ifdef USART1_SENSOR_MODE

#include "bsp/bsp_uart.h"
#include "bsp/bsp_systick.h"
#include "modbus/mb_crc.h"
#include <stddef.h>
#include <string.h>

/* ============================================================================
 * Private helpers
 * ============================================================================ */

/**
 * @brief Build a fresh request frame with correct CRC each call.
 *        The base 6 bytes are constant; CRC covers those 6 bytes.
 */
static void prepare_request_frame(uint8_t frame[CO_REQUEST_LEN])
{
    frame[0U] = 0x62U;
    frame[1U] = 0x03U;
    frame[2U] = 0x00U;
    frame[3U] = 0x03U;
    frame[4U] = 0x00U;
    frame[5U] = 0x01U;
    uint16_t crc  = MB_CRC16(frame, 6U);
    frame[6U] = (uint8_t)(crc & 0x00FFU);         /* CRC low  */
    frame[7U] = (uint8_t)((crc >> 8U) & 0x00FFU); /* CRC high */
}

/**
 * @brief Validate CRC of a complete response frame.
 */
static bool validate_response_crc(const uint8_t *buf)
{
    uint16_t crc_calc = MB_CRC16(buf, (uint16_t)(CO_RESPONSE_LEN - 2U));
    uint16_t crc_rx   = (uint16_t)buf[CO_RESPONSE_LEN - 2U] |
                        ((uint16_t)buf[CO_RESPONSE_LEN - 1U] << 8U);
    return (crc_calc == crc_rx);
}

/**
 * @brief Increment error counter; transition to ERROR state at threshold.
 */
static Status_t handle_err(CO_Ctx_t *ctx, Status_t st)
{
    if (ctx->error_count < CO_MAX_ERRORS) { ctx->error_count++; }
    if (ctx->error_count >= CO_MAX_ERRORS) { ctx->state = CO_STATE_ERROR; }
    return st;
}

/**
 * @brief Replace a zero CO reading with last known-good value (if available).
 */
static void apply_last_valid(CO_Ctx_t *ctx)
{
    if ((ctx->data.co_ppm == 0U) && ctx->has_valid_data)
    {
        ctx->data.co_ppm = ctx->last_valid_data.co_ppm;
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

Status_t DRV_CO_Init(CO_Ctx_t *ctx)
{
    if (ctx == NULL) { return STATUS_ERR_PARAM; }

    (void)memset(ctx, 0, sizeof(CO_Ctx_t));

    /* USART1 hardware init at 9600 baud (called once; safe to call here) */
    BSP_CO_UART_Init();

    ctx->state             = CO_STATE_WARMING_UP;
    ctx->last_request_tick = BSP_SysTick_GetTick();

    return STATUS_OK;
}

Status_t DRV_CO_SendRequest(CO_Ctx_t *ctx)
{
    uint8_t frame[CO_REQUEST_LEN];

    if (ctx == NULL)                     { return STATUS_ERR_PARAM; }
    if (ctx->state == CO_STATE_UNINIT)   { return STATUS_ERR_STATE; }

    prepare_request_frame(frame);

    uint16_t sent = BSP_CO_UART_Write(frame, CO_REQUEST_LEN);
    if (sent != CO_REQUEST_LEN) { return handle_err(ctx, STATUS_ERR_GENERIC); }

    ctx->waiting_response  = true;
    ctx->request_sent_tick = BSP_SysTick_GetTick();
    ctx->rx_idx            = 0U;
    
    /* Increment request counter */
    ctx->data.request_count++;

    return STATUS_OK;
}

/* Called from USART1 IRQ — stores one byte into the response buffer. */
void DRV_CO_RxByte(CO_Ctx_t *ctx, uint8_t byte)
{
    if (ctx == NULL) { return; }
    if (!ctx->waiting_response) { return; }

    if (ctx->rx_idx < CO_RESPONSE_LEN)
    {
        ctx->rx_buf[ctx->rx_idx] = byte;
        ctx->rx_idx++;
    }
}

Status_t DRV_CO_Poll(CO_Ctx_t *ctx)
{
    if (ctx == NULL) { return STATUS_ERR_PARAM; }

    uint32_t now = BSP_SysTick_GetTick();

    /* Promote WARMING_UP → RUNNING on first poll */
    if (ctx->state == CO_STATE_WARMING_UP) { ctx->state = CO_STATE_RUNNING; }

    /* ---- Time to fire a new request? ---- */
    if (!ctx->waiting_response &&
        BSP_SysTick_Elapsed(ctx->last_request_tick, CO_REQUEST_INTERVAL_MS))
    {
        ctx->last_request_tick = now;
        Status_t st = DRV_CO_SendRequest(ctx);
        if (st != STATUS_OK) { return st; }
    }

    /* ---- Response timeout check ---- */
    if (ctx->waiting_response &&
        BSP_SysTick_Elapsed(ctx->request_sent_tick, CO_RESPONSE_TIMEOUT_MS))
    {
        ctx->waiting_response = false;
        ctx->rx_idx           = 0U;
        
        /* Increment timeout counter */
        ctx->data.timeout_count++;
        
        return handle_err(ctx, STATUS_ERR_TIMEOUT);
    }

    /* ---- Process complete response ---- */
    if (ctx->waiting_response && (ctx->rx_idx >= CO_RESPONSE_LEN))
    {
        ctx->waiting_response = false;

        /* Validate CRC */
        if (!validate_response_crc(ctx->rx_buf))
        {
            ctx->rx_idx = 0U;
            
            /* Increment CRC error counter */
            ctx->data.crc_error_count++;
            
            return handle_err(ctx, STATUS_ERR_CRC);
        }

        /* Validate address & function code */
        if ((ctx->rx_buf[0U] != CO_SENSOR_ADDR) || (ctx->rx_buf[1U] != 0x03U))
        {
            ctx->rx_idx = 0U;
            return handle_err(ctx, STATUS_ERR_GENERIC);
        }

        /* Increment response counter (valid response received) */
        ctx->data.response_count++;

        /* Store response for debugging */
        (void)memcpy(ctx->last_response, ctx->rx_buf, CO_RESPONSE_LEN);

        /* Parse CO value: Try different interpretations based on sensor behavior
         * 
         * Standard Modbus: Big-Endian, response[3]=MSB, response[4]=LSB
         * Example: [62 03 02 00 0A xx xx] → 0x000A = 10 ppm
         * 
         * ACTIVE INTERPRETATION (uncomment ONE of the following):
         */
        
        /* Option 1: Standard Big-Endian (MSB first) — DEFAULT */
        uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[3U], ctx->rx_buf[4U]);
        
        /* Option 2: Little-Endian (LSB first) — uncomment if values seem swapped */
        // uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[4U], ctx->rx_buf[3U]);
        
        /* Option 3: Scaled value (÷10) — uncomment if sensor uses decimal format */
        // uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[3U], ctx->rx_buf[4U]);
        // co_raw = co_raw * 10U;  /* If sensor sends 0.1 ppm units */
        
        /* Option 4: Scaled value (×10) — uncomment if values too small */
        // uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[3U], ctx->rx_buf[4U]);
        // co_raw = co_raw / 10U;  /* If sensor sends in 0.1 ppm resolution */
        
        ctx->data.co_ppm = co_raw;
        ctx->data.valid  = true;

        /* Apply last known-good substitution for zero readings */
        apply_last_valid(ctx);

        /* Update snapshot only when we have a genuine non-zero reading */
        if (ctx->data.co_ppm != 0U)
        {
            (void)memcpy(&ctx->last_valid_data, &ctx->data, sizeof(CO_Data_t));
            ctx->has_valid_data = true;
        }

        ctx->data_fresh  = true;
        ctx->error_count = 0U;
        ctx->rx_idx      = 0U;

        return STATUS_OK;
    }

    return STATUS_NOT_READY;
}

CO_State_t DRV_CO_GetState(const CO_Ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->state : CO_STATE_UNINIT;
}

const uint8_t* DRV_CO_GetLastResponse(const CO_Ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->last_response : NULL;
}

#endif /* USART1_SENSOR_MODE */
