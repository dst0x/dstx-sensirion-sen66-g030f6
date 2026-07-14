/**
 * @file  main.c
 * @brief SEN66 + CO Infwin → Modbus RTU gateway on STM32G031F8P6.
 *
 * Pinout:
 *   PA0/PA1    SP4T DeMux Control
 *              - USART1_DEBUG_MODE:  PA1=1, PA0=1 (11=TTL) → USART1 debug via MUX
 *              - USART1_SENSOR_MODE: PA1=0, PA0=0 (00=RS485) → USART1 to CO sensor
 *   PA2        USART2_TX  9600 baud   RS485 Modbus slave ID=1
 *   PA3        USART2_RX  9600 baud   RS485 Modbus slave ID=1
 *   PA4        LED
 *   PA11/PA12  I2C2 SCL/SDA 100 kHz  SEN66
 *   PB1        RS485 DE control (HIGH=drive, LOW=receive) for USART2
 *   PB6        USART1_TX  — debug 115200 (DEBUG_MODE) OR CO sensor 9600 (SENSOR_MODE)
 *   PB7        USART1_RX  — debug 115200 (DEBUG_MODE) OR CO sensor 9600 (SENSOR_MODE)
 *
 * USART1 Mode (set in drivers/drv_co_infwin.h):
 *   #define USART1_DEBUG_MODE   — USART1 = serial debug output @ 115200 baud
 *   #define USART1_SENSOR_MODE  — USART1 = Infwin CO sensor @ 9600 baud
 *
 * Modbus register map:
 *   0x00–0x10  SEN66 data (see modbus/mb_slave.h)
 *   0x11       CO ppm  (Infwin, USART1_SENSOR_MODE only; 0 in debug mode)
 */

#include "bsp/bsp_clock.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_iwdg.h"
#include "bsp/bsp_systick.h"
#include "bsp/bsp_uart.h"
#include "drivers/drv_sen66.h"
#include "drivers/drv_co_infwin.h"
#include "modbus/mb_slave.h"
#include "common/common_types.h"
#include <string.h>

/* =========================================================================
 * Timing constants
 * =========================================================================*/
#define SENSOR_POLL_MS   (1000U)
#define SENSOR_RETRY_MS  (5000U)
#define LED_BLINK_OK_MS  (500U)
#define LED_BLINK_ERR_MS (100U)
#define DEBUG_PRINT_MS   (2000U)

/* =========================================================================
 * Global contexts
 *
 * g_co_sensor is declared as a plain extern in bsp_uart.c (SENSOR_MODE)
 * so the USART1 IRQ can forward received bytes directly to the driver.
 * =========================================================================*/
static Sen66_Ctx_t    g_sensor;
static MB_Slave_Ctx_t g_mb;

#ifdef USART1_SENSOR_MODE
CO_Ctx_t g_co_sensor;   /* extern CO_Ctx_t g_co_sensor; referenced by bsp_uart.c */
#endif

/* =========================================================================
 * Debug helpers (compiled away in SENSOR_MODE)
 * =========================================================================*/
#ifdef USART1_DEBUG_MODE

static void print_fixed(const char *prefix, int32_t raw, int32_t scale,
                        uint8_t decimals, const char *suffix)
{
    BSP_DBG_WriteFixed(prefix, raw, scale, decimals, suffix);
}

static void debug_print_sensor(const Sen66_Ctx_t *s)
{
    const Sen66_Data_t *d = &s->data;
    BSP_DBG_WriteStr("[SEN66] ");
    print_fixed("PM1.0=",  (int32_t)d->pm1_0,  10,  1, " | ");
    print_fixed("PM2.5=",  (int32_t)d->pm2_5,  10,  1, " | ");
    print_fixed("PM4.0=",  (int32_t)d->pm4_0,  10,  1, " | ");
    print_fixed("PM10=",   (int32_t)d->pm10,   10,  1, " ug/m3 | ");
    print_fixed("RH=",     (int32_t)d->rh,    100,  1, "%%RH | ");
    print_fixed("Temp=",   (int32_t)d->temp,   10,  1, "C | ");
    print_fixed("VOC=",    (int32_t)d->voc,    10,  1, " | ");
    print_fixed("NOx=",    (int32_t)d->nox,    10,  1, " | ");
    BSP_DBG_WriteInt("CO2=", (int32_t)d->co2_ppm, "ppm ");
    BSP_DBG_WriteInt("[Err=", (int32_t)s->error_count, "]\r\n");
}

#endif /* USART1_DEBUG_MODE */

/* =========================================================================
 * Modbus service
 * =========================================================================*/
static void modbus_service(void)
{
    uint16_t tx_len = 0U;

    /* Drain RS485 RX FIFO into Modbus frame assembler */
    while (BSP_RS485_RxCount() > 0U)
    {
        uint8_t  byte = BSP_RS485_RxGet();
        uint32_t tick = BSP_SysTick_GetTick();
        MB_Slave_RxByte(&g_mb, byte, tick);
    }

    /* Process frame once inter-frame silence detected */
    if ((g_mb.rx_len > 0U) &&
        BSP_SysTick_Elapsed(g_mb.last_rx_tick, MB_FRAME_TIMEOUT_MS))
    {
#ifdef USART1_SENSOR_MODE
        MB_Slave_Process(&g_mb, &g_sensor, &g_co_sensor, &tx_len);
#else
        MB_Slave_Process(&g_mb, &g_sensor, &tx_len);
#endif
        if (tx_len > 0U)
        {
            BSP_SysTick_DelayMs(2U);
            BSP_RS485_Send(g_mb.tx_buf, tx_len);
        }
    }
}

/* =========================================================================
 * main
 * =========================================================================*/
int main(void)
{
    Status_t st;
    uint32_t last_poll_tick  = 0U;
    uint32_t last_led_tick   = 0U;
    uint32_t last_dbg_tick   = 0U;
    uint32_t last_retry_tick = 0U;
    bool     led_state       = false;
    bool     sensor_ok       = false;

    /* -----------------------------------------------------------------
     * Hardware init
     * ----------------------------------------------------------------- */
    BSP_SysTick_Init();
    (void)BSP_Clock_Init();
    BSP_GPIO_Init();

#ifdef USART1_DEBUG_MODE
    BSP_GPIO_SetCommMode(COMM_MODE_TTL);  /* PA1=1, PA0=1 → MUX routes USART1 to TTL */
    BSP_DBG_Init();
    BSP_SysTick_DelayMs(10U);
    BSP_DBG_WriteStr("\r\n=================================\r\n");
    BSP_DBG_WriteStr("SEN66+CO-G031 v1.3 [DEBUG MODE]\r\n");
    BSP_DBG_WriteInt("Modbus Slave ID: 0x", (int32_t)MB_SLAVE_ID, "\r\n");
    BSP_DBG_WriteStr("USART1 = debug output (115200)\r\n");
    BSP_DBG_WriteStr("=================================\r\n");
#endif

#ifdef USART1_SENSOR_MODE
    /* Set DeMux to RS485 mode (PA1=0, PA0=0) → routes USART1 to CO sensor */
    BSP_GPIO_SetCommMode(COMM_MODE_RS485);
#endif

    BSP_I2C_Init();
    BSP_RS485_Init();
    BSP_IWDG_Init();
    MB_Slave_Init(&g_mb);

    /* -----------------------------------------------------------------
     * SEN66 init (3 attempts)
     * ----------------------------------------------------------------- */
    {
        uint8_t attempt;
        st = STATUS_ERR_GENERIC;
        for (attempt = 0U; attempt < 3U; attempt++)
        {
            BSP_IWDG_Refresh();
            st = DRV_SEN66_Init(&g_sensor);
            if (st == STATUS_OK) { break; }
#ifdef USART1_DEBUG_MODE
            BSP_DBG_WriteStr("SEN66 init failed, retrying...\r\n");
#endif
            BSP_SysTick_DelayMs(500U);
        }
        BSP_IWDG_Refresh();

        if (st == STATUS_OK)
        {
            st = DRV_SEN66_StartMeasurement(&g_sensor);
            if (st == STATUS_OK)
            {
                sensor_ok = true;
#ifdef USART1_DEBUG_MODE
                BSP_DBG_WriteStr("SEN66 OK. Warming up 60s...\r\n");
#endif
            }
#ifdef USART1_DEBUG_MODE
            else { BSP_DBG_WriteStr("SEN66 start failed.\r\n"); }
#endif
        }
#ifdef USART1_DEBUG_MODE
        else { BSP_DBG_WriteStr("SEN66 not detected.\r\n"); }
#endif
    }

    /* -----------------------------------------------------------------
     * CO sensor init (USART1_SENSOR_MODE only)
     * ----------------------------------------------------------------- */
#ifdef USART1_SENSOR_MODE
    {
        st = DRV_CO_Init(&g_co_sensor);
        /* DRV_CO_Init calls BSP_CO_UART_Init internally */
        (void)st;   /* Non-fatal if CO sensor absent; readings stay 0 */
    }
#endif

    /* -----------------------------------------------------------------
     * Tick snapshots
     * ----------------------------------------------------------------- */
    last_poll_tick  = BSP_SysTick_GetTick();
    last_led_tick   = BSP_SysTick_GetTick();
    last_dbg_tick   = BSP_SysTick_GetTick();
    last_retry_tick = BSP_SysTick_GetTick();

    /* =================================================================
     * Main loop
     * ================================================================= */
    for (;;)
    {
        uint32_t now = BSP_SysTick_GetTick();

        /* ---- SEN66 polling ----------------------------------------- */
        if (sensor_ok && BSP_SysTick_Elapsed(last_poll_tick, SENSOR_POLL_MS))
        {
            last_poll_tick = now;
            if (DRV_SEN66_IsDataReady(&g_sensor))
            {
                st = DRV_SEN66_Poll(&g_sensor);
                if ((st != STATUS_OK) &&
                    (st != STATUS_NOT_READY) &&
                    (st != STATUS_ERR_STATE))
                {
#ifdef USART1_DEBUG_MODE
                    BSP_DBG_WriteStr("SEN66 offline.\r\n");
#endif
                    sensor_ok = false;
                    last_retry_tick = now;
                    (void)memset(&g_sensor.data, 0, sizeof(g_sensor.data));
                    g_sensor.data_fresh = false;
                }
            }
        }

        /* ---- SEN66 reconnect --------------------------------------- */
        if (!sensor_ok &&
            BSP_SysTick_Elapsed(last_retry_tick, SENSOR_RETRY_MS))
        {
            last_retry_tick = now;
            BSP_IWDG_Refresh();
            st = DRV_SEN66_Init(&g_sensor);
            if (st == STATUS_OK)
            {
                st = DRV_SEN66_StartMeasurement(&g_sensor);
                if (st == STATUS_OK)
                {
                    sensor_ok = true;
                    last_poll_tick = now;
#ifdef USART1_DEBUG_MODE
                    BSP_DBG_WriteStr("SEN66 reconnected.\r\n");
#endif
                }
            }
            BSP_IWDG_Refresh();
        }

        /* ---- CO sensor polling (SENSOR_MODE only) ------------------ */
#ifdef USART1_SENSOR_MODE
        (void)DRV_CO_Poll(&g_co_sensor);
#endif

        /* ---- Modbus service ---------------------------------------- */
        modbus_service();

        /* ---- LED blink --------------------------------------------- */
        {
            uint32_t blink = (g_sensor.state == SEN66_STATE_ERROR)
                             ? LED_BLINK_ERR_MS : LED_BLINK_OK_MS;
            if (BSP_SysTick_Elapsed(last_led_tick, blink))
            {
                last_led_tick = now;
                led_state = !led_state;
                if (led_state) { BSP_GPIO_LedOn();  }
                else           { BSP_GPIO_LedOff(); }
            }
        }

        /* ---- Periodic debug print (DEBUG_MODE only) ---------------- */
#ifdef USART1_DEBUG_MODE
        if (BSP_SysTick_Elapsed(last_dbg_tick, DEBUG_PRINT_MS))
        {
            last_dbg_tick = now;
            if (g_sensor.data_fresh) { debug_print_sensor(&g_sensor); }
        }
#else
        (void)last_dbg_tick;   /* suppress unused-variable warning */
#endif

        BSP_IWDG_Refresh();
    }

    return 0;
}
