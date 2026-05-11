/**
 * @file    bsp_systick.c
 * @brief   SysTick 1 ms timebase for STM32G030F6P6 @ 64 MHz.
 * @note    MISRA C:2012 compliant.
 */
#include "bsp/bsp_systick.h"
#include "stm32g0xx.h"

static volatile uint32_t g_tick = 0U;

void BSP_SysTick_Init(void)
{
    /* Configure SysTick for 1 ms interrupt at 64 MHz */
    (void)SysTick_Config(SYSCLK_HZ / 1000UL);
    /* Priority 3 — lower than I2C/UART (which are 1–2) */
    NVIC_SetPriority(SysTick_IRQn, 3U);
}

void SysTick_Handler(void)
{
    g_tick++;
}

uint32_t BSP_SysTick_GetTick(void)
{
    return g_tick;
}

void BSP_SysTick_DelayMs(uint32_t ms)
{
    uint32_t start = g_tick;
    while ((g_tick - start) < ms) { /* spin */ }
}

bool BSP_SysTick_Elapsed(uint32_t start_tick, uint32_t period_ms)
{
    return ((g_tick - start_tick) >= period_ms);
}
