/**
 * @file    bsp_uart.h
 * @brief   UART driver API.
 *
 *   USART2 (PA2/PA3, 115200 baud) — debug output (TX only, interrupt-driven)
 *   USART1 (PA11/PA12, 9600 baud) — RS485 Modbus RTU (RX interrupt, TX polled)
 *
 * BRR calculations @ PCLK = 64 MHz:
 *   USART2: BRR = 64,000,000 / 115,200 = 556   (0x022C)  → 115,108 baud (-0.08%)
 *   USART1: BRR = 64,000,000 /   9,600 = 6,667 (0x1A0B)  →   9,599 baud (-0.01%)
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#include "common/common_types.h"

/* Debug UART (USART2) TX ring buffer size — must be power of 2 */
#define DBG_TX_BUF_SIZE    (256U)

/* RS485 UART (USART1) RX buffer size — must be power of 2 */
#define RS485_RX_BUF_SIZE  (64U)

/* -------------------------------------------------------------------------
 * Debug UART (USART2) — write-only
 * -------------------------------------------------------------------------*/
void     BSP_DBG_Init(void);
uint16_t BSP_DBG_Write(const uint8_t *data, uint16_t len);
void     BSP_DBG_WriteStr(const char *str);
void     BSP_DBG_WriteInt(const char *prefix, int32_t val, const char *suffix);
void     BSP_DBG_WriteFixed(const char *prefix, int32_t raw, int32_t scale,
                             uint8_t decimals, const char *suffix);
void     BSP_DBG_WriteHex(const char *prefix, const uint8_t *buf,
                           uint8_t len, const char *suffix);

/* -------------------------------------------------------------------------
 * RS485 UART (USART1) — Modbus RTU receive / transmit
 * -------------------------------------------------------------------------*/
void     BSP_RS485_Init(void);
void     BSP_RS485_Send(const uint8_t *data, uint16_t len);
uint16_t BSP_RS485_RxCount(void);
uint8_t  BSP_RS485_RxGet(void);
void     BSP_RS485_RxFlush(void);

/* Called from USART1 IRQ — forward in startup or ISR file */
void     BSP_RS485_IRQHandler(void);
/* Called from USART2 IRQ — forward in startup or ISR file */
void     BSP_DBG_IRQHandler(void);

#endif /* BSP_UART_H */
