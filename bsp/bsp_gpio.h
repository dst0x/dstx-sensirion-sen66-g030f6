/**
 * @file    bsp_gpio.h
 * @brief   GPIO initialisation API.
 *
 * Pin mapping (STM32G031F8P6, TSSOP20):
 *   SP4T DeMux  : PA0 (bit A), PA1 (bit B) — Communication mode select
 *                 DEFAULT: 11 (TTL mode) for USART1 debugging
 *   LPUART1 TX  : PA2 (AF6)  — RS485 TX to transceiver
 *   LPUART1 RX  : PA3 (AF6)  — RS485 RX from transceiver
 *   LED         : PA4 (output) — activity LED
 *   I2C2  SCL   : PA11 (AF6)  — SEN66 clock
 *   I2C2  SDA   : PA12 (AF6)  — SEN66 data
 *   RS485 DE/RE : PB1 (output) — RS485 Mode Control
 *                                 HIGH = Drive Mode (Transmit)
 *                                 LOW  = Logging Mode (Receive/Monitor)
 *   USART1 TX   : PB6 (AF0)  — Debug output (via MUX in TTL mode)
 *   USART1 RX   : PB7 (AF0)  — Debug input (via MUX in TTL mode)
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "common/common_types.h"

/**
 * @brief Communication mode selection for SP4T DeMux
 */
typedef enum {
    COMM_MODE_RS485  = 0x00U,  /* PA1:PA0 = 00 */
    COMM_MODE_RS232  = 0x01U,  /* PA1:PA0 = 01 */
    COMM_MODE_SDI12  = 0x02U,  /* PA1:PA0 = 10 */
    COMM_MODE_TTL    = 0x03U   /* PA1:PA0 = 11 */
} CommMode_t;

void BSP_GPIO_Init(void);
void BSP_GPIO_LedOn(void);
void BSP_GPIO_LedOff(void);
void BSP_GPIO_LedToggle(void);
void BSP_GPIO_RS485_DriveMode(void);    /* HIGH = Transmit/Drive mode */
void BSP_GPIO_RS485_LoggingMode(void);  /* LOW = Receive/Logging mode */
void BSP_GPIO_SetCommMode(CommMode_t mode);

#endif /* BSP_GPIO_H */
