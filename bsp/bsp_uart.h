/**
 * @file    bsp_uart.h
 * @brief   UART BSP — USART1 dual-mode + USART2 RS485.
 *
 * USART1 compile-time mode (set in drv_co_infwin.h):
 *   USART1_DEBUG_MODE   — 115200 baud TX, interrupt-driven debug output
 *   USART1_SENSOR_MODE  — 9600 baud bidirectional CO sensor communication
 *
 * USART2 — always RS485 Modbus RTU @ 9600 baud (PA2/PA3, DE on PB1)
 *
 * BRR @ PCLK = 64 MHz:
 *   USART1 debug : 64 000 000 / 115 200 = 556   → 115 108 baud (−0.08%)
 *   USART1 sensor: 64 000 000 /   9 600 = 6 667 →   9 600 baud (exact)
 *   USART2 RS485 : 64 000 000 /   9 600 = 6 667 →   9 600 baud (exact)
 */
#ifndef BSP_UART_H
#define BSP_UART_H

/* Pull in mode selection macros (USART1_DEBUG_MODE / USART1_SENSOR_MODE) */
#include "drivers/drv_co_infwin.h"
#include "common/common_types.h"

/* ---- Buffer sizes (must be powers of 2) ---- */
#define DBG_TX_BUF_SIZE   (256U)  /* Debug TX ring buffer  */
#define CO_RX_BUF_SIZE    (16U)   /* CO sensor RX buffer   */
#define RS485_RX_BUF_SIZE (64U)   /* RS485 RX ring buffer  */

/* =========================================================================
 * USART1 — Debug Mode (USART1_DEBUG_MODE)
 * =========================================================================*/
#ifdef USART1_DEBUG_MODE
void     BSP_DBG_Init(void);
uint16_t BSP_DBG_Write(const uint8_t *data, uint16_t len);
void     BSP_DBG_WriteStr(const char *str);
void     BSP_DBG_WriteInt(const char *prefix, int32_t val, const char *suffix);
void     BSP_DBG_WriteFixed(const char *prefix, int32_t raw, int32_t scale,
                             uint8_t decimals, const char *suffix);
void     BSP_DBG_WriteHex(const char *prefix, const uint8_t *buf,
                           uint8_t len, const char *suffix);
#endif /* USART1_DEBUG_MODE */

/* =========================================================================
 * USART1 — CO Sensor Mode (USART1_SENSOR_MODE)
 * =========================================================================*/
#ifdef USART1_SENSOR_MODE
void     BSP_CO_UART_Init(void);
uint16_t BSP_CO_UART_Write(const uint8_t *data, uint16_t len);
uint16_t BSP_CO_UART_RxCount(void);
uint8_t  BSP_CO_UART_RxGet(void);
void     BSP_CO_UART_RxFlush(void);
#endif /* USART1_SENSOR_MODE */

/* Shared USART1 IRQ forwarder (defined in bsp_uart.c) */
void BSP_USART1_IRQHandler(void);

/* =========================================================================
 * USART2 — RS485 Modbus RTU
 * =========================================================================*/
void     BSP_RS485_Init(void);
void     BSP_RS485_Send(const uint8_t *data, uint16_t len);
uint16_t BSP_RS485_RxCount(void);
uint8_t  BSP_RS485_RxGet(void);
void     BSP_RS485_RxFlush(void);
void     BSP_USART2_IRQHandler(void);

#endif /* BSP_UART_H */
