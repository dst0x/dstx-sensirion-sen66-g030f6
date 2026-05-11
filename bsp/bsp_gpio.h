/**
 * @file    bsp_gpio.h
 * @brief   GPIO initialisation API.
 *
 * Pin mapping (STM32G030F6P6, TSSOP20):
 *   I2C1  SCL  : PA1 (AF6)  — SEN66 clock
 *   I2C1  SDA  : PA0 (AF6)  — SEN66 data
 *   USART2 TX  : PA2 (AF1)  — Debug output
 *   USART2 RX  : PA3 (AF1)  — Debug input
 *   USART1 TX  : PA11 (AF1, SYSCFG PA_REMAP) — RS485 TX
 *   USART1 RX  : PA12 (AF1, SYSCFG PA_REMAP) — RS485 RX
 *   LED        : PA4 (output) — activity LED
 *
 * Note: USART1 uses SYSCFG PA11/PA12 remap (SYSCFG_CFGR1 bit 3).
 *       When remapped, PA11 acts as PA9 (USART1_TX) and PA12 as PA10 (USART1_RX).
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "common/common_types.h"

void BSP_GPIO_Init(void);
void BSP_GPIO_LedOn(void);
void BSP_GPIO_LedOff(void);
void BSP_GPIO_LedToggle(void);

#endif /* BSP_GPIO_H */
