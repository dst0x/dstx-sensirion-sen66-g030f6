/**
 * @file    bsp_clock.h
 * @brief   System clock initialisation API (STM32G030F6P6 @ 64 MHz).
 */
#ifndef BSP_CLOCK_H
#define BSP_CLOCK_H

#include "common/common_types.h"

/**
 * @brief  Configure SYSCLK to 64 MHz from HSI16 via PLL.
 *         Flash latency set to 2 WS; HCLK = PCLK = 64 MHz.
 * @return STATUS_OK or STATUS_ERR_TIMEOUT.
 */
Status_t BSP_Clock_Init(void);

#endif /* BSP_CLOCK_H */
