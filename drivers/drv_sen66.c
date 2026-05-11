/**
 * @file  drv_sen66.c
 * @brief Sensirion SEN66 driver.
 */
#include "drivers/drv_sen66.h"
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_systick.h"
#include <stddef.h>
#include <string.h>

static uint8_t calc_crc(const uint8_t data[2U])
{
    uint8_t crc = 0xFFU;
    uint8_t i, bit;
    for (i = 0U; i < 2U; i++)
    {
        crc ^= data[i];
        for (bit = 8U; bit > 0U; bit--)
        {
            crc = ((crc & 0x80U) != 0U) ? (uint8_t)((crc << 1U) ^ 0x31U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

uint8_t DRV_SEN66_CalcCRC(const uint8_t data[2U]) { return calc_crc(data); }

static bool crc_ok(const uint8_t *buf)
{
    uint8_t pair[2U] = { buf[0U], buf[1U] };
    return (calc_crc(pair) == buf[2U]);
}

static Status_t handle_err(Sen66_Ctx_t *ctx, Status_t st)
{
    if (ctx->error_count < SEN66_MAX_ERRORS) { ctx->error_count++; }
    if (ctx->error_count >= SEN66_MAX_ERRORS) { ctx->state = SEN66_STATE_ERROR; }
    return st;
}

static bool is_unavail_u16(uint16_t v) { return (v == SEN66_UNAVAIL_U16) || (v == SEN66_INIT_U16); }
static bool is_unavail_i16(int16_t v)  { return (v == SEN66_UNAVAIL_I16); }

static void parse_meas(const uint8_t *buf, Sen66_Data_t *d)
{
    uint16_t pm1  = BYTES_TO_U16(buf[0U],  buf[1U]);
    uint16_t pm25 = BYTES_TO_U16(buf[3U],  buf[4U]);
    uint16_t pm40 = BYTES_TO_U16(buf[6U],  buf[7U]);
    uint16_t pm10 = BYTES_TO_U16(buf[9U],  buf[10U]);
    int16_t  rh   = BYTES_TO_I16(buf[12U], buf[13U]);
    int16_t  tmp  = BYTES_TO_I16(buf[15U], buf[16U]);
    int16_t  voc  = BYTES_TO_I16(buf[18U], buf[19U]);
    int16_t  nox  = BYTES_TO_I16(buf[21U], buf[22U]);
    uint16_t co2  = BYTES_TO_U16(buf[24U], buf[25U]);

    d->pm_valid  = !is_unavail_u16(pm1);
    d->env_valid = !is_unavail_i16(rh);
    d->gas_valid = (!is_unavail_i16(voc)) && (!is_unavail_i16(nox));
    d->co2_valid = !is_unavail_u16(co2);

    d->pm1_0   = d->pm_valid  ? pm1  : 0U;
    d->pm2_5   = d->pm_valid  ? pm25 : 0U;
    d->pm4_0   = d->pm_valid  ? pm40 : 0U;
    d->pm10    = d->pm_valid  ? pm10 : 0U;
    d->rh      = d->env_valid ? rh   : 0;
    d->temp    = d->env_valid ? (int16_t)(tmp / (int16_t)20) : 0;
    d->voc     = d->gas_valid ? voc  : 0;
    d->nox     = d->gas_valid ? nox  : 0;
    d->co2_ppm = d->co2_valid ? co2  : 0U;
}

static void parse_nc(const uint8_t *buf, Sen66_Data_t *d)
{
    d->nc0_5    = BYTES_TO_U16(buf[0U],  buf[1U]);
    d->nc1_0    = BYTES_TO_U16(buf[3U],  buf[4U]);
    d->nc2_5    = BYTES_TO_U16(buf[6U],  buf[7U]);
    d->nc4_0    = BYTES_TO_U16(buf[9U],  buf[10U]);
    d->nc10     = BYTES_TO_U16(buf[12U], buf[13U]);
    d->typ_size = 0U;
}

static Status_t verify_crc_words(const uint8_t *buf, uint8_t num_words)
{
    uint8_t i;
    for (i = 0U; i < num_words; i++)
    {
        if (!crc_ok(&buf[(uint8_t)(i * 3U)])) { return STATUS_ERR_CRC; }
    }
    return STATUS_OK;
}

Status_t DRV_SEN66_Init(Sen66_Ctx_t *ctx)
{
    Status_t st;
    uint8_t  prod_buf[48U];

    if (ctx == NULL) { return STATUS_ERR_PARAM; }
    (void)memset(ctx, 0, sizeof(Sen66_Ctx_t));
    ctx->state = SEN66_STATE_UNINIT;

    BSP_SysTick_DelayMs(SEN66_STARTUP_MS);

    st = BSP_I2C_WriteReadData(SEN66_I2C_ADDR, SEN66_CMD_READ_PRODUCT,
                               prod_buf, (uint8_t)sizeof(prod_buf));
    if (st != STATUS_OK) { return handle_err(ctx, st); }
    if (verify_crc_words(prod_buf, 16U) != STATUS_OK) { return handle_err(ctx, STATUS_ERR_CRC); }

    ctx->state       = SEN66_STATE_IDLE;
    ctx->error_count = 0U;
    ctx->data_fresh  = false;
    return STATUS_OK;
}

Status_t DRV_SEN66_StartMeasurement(Sen66_Ctx_t *ctx)
{
    Status_t st;
    if (ctx == NULL) { return STATUS_ERR_PARAM; }
    if ((ctx->state != SEN66_STATE_IDLE) && (ctx->state != SEN66_STATE_ERROR))
    {
        return STATUS_ERR_STATE;
    }
    st = BSP_I2C_WriteCmd(SEN66_I2C_ADDR, SEN66_CMD_START_MEAS);
    if (st != STATUS_OK) { return handle_err(ctx, st); }
    BSP_SysTick_DelayMs(50U);
    ctx->state       = SEN66_STATE_WARMING_UP;
    ctx->start_tick  = BSP_SysTick_GetTick();
    ctx->data_fresh  = false;
    ctx->error_count = 0U;
    return STATUS_OK;
}

Status_t DRV_SEN66_StopMeasurement(Sen66_Ctx_t *ctx)
{
    Status_t st;
    if (ctx == NULL) { return STATUS_ERR_PARAM; }
    st = BSP_I2C_WriteCmd(SEN66_I2C_ADDR, SEN66_CMD_STOP_MEAS);
    BSP_SysTick_DelayMs(SEN66_STOP_DELAY_MS);
    ctx->state = SEN66_STATE_IDLE;
    return st;
}

Status_t DRV_SEN66_Poll(Sen66_Ctx_t *ctx)
{
    Status_t st;
    uint8_t  ready_buf[3U];
    uint8_t  meas_buf[SEN66_MEAS_PAYLOAD_BYTES];
    uint8_t  nc_buf[SEN66_NC_PAYLOAD_BYTES];

    if (ctx == NULL) { return STATUS_ERR_PARAM; }
    if ((ctx->state != SEN66_STATE_WARMING_UP) && (ctx->state != SEN66_STATE_RUNNING))
    {
        return STATUS_ERR_STATE;
    }

    if ((ctx->state == SEN66_STATE_WARMING_UP) &&
        BSP_SysTick_Elapsed(ctx->start_tick, SEN66_WARMUP_MS))
    {
        ctx->state = SEN66_STATE_RUNNING;
    }

    st = BSP_I2C_WriteReadData(SEN66_I2C_ADDR, SEN66_CMD_DATA_READY, ready_buf, 3U);
    if (st != STATUS_OK) { return handle_err(ctx, st); }
    if (!crc_ok(ready_buf)) { return handle_err(ctx, STATUS_ERR_CRC); }
    if (ready_buf[1U] != 0x01U) { return STATUS_NOT_READY; }

    st = BSP_I2C_WriteReadData(SEN66_I2C_ADDR, SEN66_CMD_READ_VALUES,
                               meas_buf, SEN66_MEAS_PAYLOAD_BYTES);
    if (st != STATUS_OK) { return handle_err(ctx, st); }
    if (verify_crc_words(meas_buf, 9U) != STATUS_OK) { return handle_err(ctx, STATUS_ERR_CRC); }
    (void)memcpy(ctx->raw_meas, meas_buf, SEN66_MEAS_PAYLOAD_BYTES);

    st = BSP_I2C_WriteReadData(SEN66_I2C_ADDR, SEN66_CMD_READ_PM_NC,
                               nc_buf, SEN66_NC_PAYLOAD_BYTES);
    if (st != STATUS_OK) { return handle_err(ctx, st); }
    if (verify_crc_words(nc_buf, 5U) != STATUS_OK) { return handle_err(ctx, STATUS_ERR_CRC); }

    parse_meas(meas_buf, &ctx->data);
    parse_nc(nc_buf, &ctx->data);

    ctx->data_fresh     = true;
    ctx->last_read_tick = BSP_SysTick_GetTick();
    ctx->error_count    = 0U;
    return STATUS_OK;
}

Status_t DRV_SEN66_Reset(Sen66_Ctx_t *ctx)
{
    Status_t st;
    if (ctx == NULL) { return STATUS_ERR_PARAM; }
    (void)BSP_I2C_WriteCmd(SEN66_I2C_ADDR, SEN66_CMD_RESET);
    BSP_SysTick_DelayMs(SEN66_RESET_DELAY_MS);
    st = DRV_SEN66_Init(ctx);
    if (st != STATUS_OK)
    {
        ctx->state       = SEN66_STATE_ERROR;
        ctx->error_count = SEN66_MAX_ERRORS;
    }
    return st;
}

Sen66_State_t DRV_SEN66_GetState(const Sen66_Ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->state : SEN66_STATE_UNINIT;
}
