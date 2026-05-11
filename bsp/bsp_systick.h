/**
 * @file    bsp_systick.h
 * @brief   SysTick 1 ms timebase API.
 */
#ifndef BSP_SYSTICK_H
#define BSP_SYSTICK_H

#include "common/common_types.h"

void     BSP_SysTick_Init(void);
void     BSP_SysTick_DelayMs(uint32_t ms);
uint32_t BSP_SysTick_GetTick(void);
bool     BSP_SysTick_Elapsed(uint32_t start_tick, uint32_t period_ms);

#endif /* BSP_SYSTICK_H */
