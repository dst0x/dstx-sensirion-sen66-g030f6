/**
 * @file  main.c
 * @brief SEN66 → Modbus RTU gateway on STM32G030F6P6.
 *
 * Pinout:
 *   PA2        USART2_TX  115200 baud  debug
 *   PA4        LED
 *   PA11/PA12  I2C2 SCL/SDA  100 kHz  SEN66
 *   PB6/PB7    USART1 TX/RX  9600 baud  RS485 Modbus slave ID=1
 */
#include "bsp/bsp_clock.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_iwdg.h"
#include "bsp/bsp_systick.h"
#include "bsp/bsp_uart.h"
#include "drivers/drv_sen66.h"
#include "modbus/mb_slave.h"
#include "common/common_types.h"
#include <string.h>

#define SENSOR_POLL_MS   (1000U)
#define SENSOR_RETRY_MS  (5000U)
#define LED_BLINK_OK_MS  (500U)
#define LED_BLINK_ERR_MS (100U)
#define DEBUG_PRINT_MS   (2000U)

static Sen66_Ctx_t    g_sensor;
static MB_Slave_Ctx_t g_mb;

static void print_fixed(const char *prefix, int32_t raw, int32_t scale,
                        uint8_t decimals, const char *suffix)
{
    BSP_DBG_WriteFixed(prefix, raw, scale, decimals, suffix);
}

static void debug_print_sensor(const Sen66_Ctx_t *s)
{
    const Sen66_Data_t *d = &s->data;
    BSP_DBG_WriteStr("[SEN66] ");
    print_fixed("PM1.0 : ",  (int32_t)d->pm1_0, 10,  1, " | ");
    print_fixed("PM2.5 : ",  (int32_t)d->pm2_5, 10,  1, " | ");
    print_fixed("PM4.0 : ",  (int32_t)d->pm4_0, 10,  1, " | ");
    print_fixed("PM10 : ",   (int32_t)d->pm10,  10,  1, " ug/m3 | ");
    print_fixed("RH : ",     (int32_t)d->rh,   100,  1, " %RH | ");
    print_fixed("Temp : ",   (int32_t)d->temp,  10,  1, " ℃ | ");
    print_fixed("VOC : ",    (int32_t)d->voc,   10,  1, " | ");
    print_fixed("NOx : ",    (int32_t)d->nox,   10,  1, " | ");
    BSP_DBG_WriteInt("CO2 : ", (int32_t)d->co2_ppm, " ppm ");
    BSP_DBG_WriteInt("[ErrCnt=", (int32_t)s->error_count, "]\r\n");
}

static void modbus_service(void)
{
    uint16_t tx_len = 0U;

    while (BSP_RS485_RxCount() > 0U)
    {
        uint8_t  byte = BSP_RS485_RxGet();
        uint32_t tick = BSP_SysTick_GetTick();
        MB_Slave_RxByte(&g_mb, byte, tick);
    }

    if ((g_mb.rx_len > 0U) &&
        BSP_SysTick_Elapsed(g_mb.last_rx_tick, MB_FRAME_TIMEOUT_MS))
    {
        MB_Slave_Process(&g_mb, &g_sensor, &tx_len);
        if (tx_len > 0U)
        {
            BSP_SysTick_DelayMs(2U);
            BSP_RS485_Send(g_mb.tx_buf, tx_len);
        }
    }
}

int main(void)
{
    Status_t st;
    uint32_t last_poll_tick  = 0U;
    uint32_t last_led_tick   = 0U;
    uint32_t last_dbg_tick   = 0U;
    uint32_t last_retry_tick = 0U;
    bool     led_state       = false;
    bool     sensor_ok       = false;

    BSP_SysTick_Init();
    (void)BSP_Clock_Init();
    BSP_GPIO_Init();
    BSP_DBG_Init();
    BSP_SysTick_DelayMs(10U);
    BSP_DBG_WriteStr("\r\nSEN66-G030 v1.0\r\n");

    BSP_I2C_Init();
    BSP_RS485_Init();
    BSP_IWDG_Init();
    MB_Slave_Init(&g_mb);

    {
        uint8_t attempt;
        st = STATUS_ERR_GENERIC;
        for (attempt = 0U; attempt < 3U; attempt++)
        {
            BSP_IWDG_Refresh();
            st = DRV_SEN66_Init(&g_sensor);
            if (st == STATUS_OK) { break; }
            BSP_DBG_WriteStr("SEN66 init failed, retrying...\r\n");
            BSP_SysTick_DelayMs(500U);
        }
        BSP_IWDG_Refresh();

        if (st == STATUS_OK)
        {
            st = DRV_SEN66_StartMeasurement(&g_sensor);
            if (st == STATUS_OK)
            {
                sensor_ok = true;
                BSP_DBG_WriteStr("SEN66 OK. Warming up 60s...\r\n");
            }
            else
            {
                BSP_DBG_WriteStr("SEN66 start failed.\r\n");
            }
        }
        else
        {
            BSP_DBG_WriteStr("SEN66 not detected.\r\n");
        }
    }

    last_poll_tick  = BSP_SysTick_GetTick();
    last_led_tick   = BSP_SysTick_GetTick();
    last_dbg_tick   = BSP_SysTick_GetTick();
    last_retry_tick = BSP_SysTick_GetTick();

    for (;;)
    {
        uint32_t now = BSP_SysTick_GetTick();

        if (sensor_ok && BSP_SysTick_Elapsed(last_poll_tick, SENSOR_POLL_MS))
        {
            last_poll_tick = now;
            st = DRV_SEN66_Poll(&g_sensor);
            if ((st != STATUS_OK) && (st != STATUS_NOT_READY) && (st != STATUS_ERR_STATE))
            {
                BSP_DBG_WriteStr("SEN66 offline.\r\n");
                sensor_ok = false;
                last_retry_tick = now;
                (void)memset(&g_sensor.data, 0, sizeof(g_sensor.data));
                g_sensor.data_fresh = false;
            }
        }

        if (!sensor_ok && BSP_SysTick_Elapsed(last_retry_tick, SENSOR_RETRY_MS))
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
                    BSP_DBG_WriteStr("SEN66 reconnected.\r\n");
                }
            }
            BSP_IWDG_Refresh();
        }

        modbus_service();

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

        if (BSP_SysTick_Elapsed(last_dbg_tick, DEBUG_PRINT_MS))
        {
            last_dbg_tick = now;
            if (g_sensor.data_fresh) { debug_print_sensor(&g_sensor); }
        }

        BSP_IWDG_Refresh();
    }

    return 0;
}
