/**
 * @file    bsp_clock.c
 * @brief   Clock configuration for STM32G030F6P6 — 64 MHz from HSI16 via PLL.
 *
 * PLL configuration (HSI16 = 16 MHz):
 *   PLLM = /1  → VCO input  = 16 MHz
 *   PLLN = x8  → VCO output = 128 MHz
 *   PLLR = /2  → PLLRCLK    = 64 MHz  (SYSCLK source)
 *
 * Flash latency: 2 WS required for SYSCLK > 48 MHz (RM0444 §3.3.4).
 *
 * @note  MISRA C:2012 compliant.
 */
#include "bsp/bsp_clock.h"
#include "stm32g0xx.h"

#define CLOCK_TIMEOUT   (100000UL)

/* PLL parameters */
#define BSP_PLLM    (0U)   /* 000b → divide by 1 */
#define BSP_PLLN    (8U)   /* N = 8               */
#define BSP_PLLR    (1U)   /* 001b → divide by 2  */

static Status_t wait_flag(volatile const uint32_t *reg, uint32_t mask, uint32_t timeout)
{
    uint32_t cnt = 0U;
    while (cnt < timeout)
    {
        if ((*reg & mask) == mask) { return STATUS_OK; }
        cnt++;
    }
    return STATUS_ERR_TIMEOUT;
}

Status_t BSP_Clock_Init(void)
{
    Status_t st;

    /* 1. Enable HSI16 */
    SET_BIT(RCC->CR, RCC_CR_HSION);
    st = wait_flag(&RCC->CR, RCC_CR_HSIRDY, CLOCK_TIMEOUT);
    if (st != STATUS_OK) { return STATUS_ERR_TIMEOUT; }

    /* 2. Flash latency: 2 WS for VCORE range 1, SYSCLK ≤ 64 MHz */
    MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_2);
    if ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_2) { return STATUS_ERR_GENERIC; }

    /* 3. Enable prefetch */
    SET_BIT(FLASH->ACR, FLASH_ACR_PRFTEN);

    /* 4. Disable PLL before configuring */
    CLR_BIT(RCC->CR, RCC_CR_PLLON);
    while ((RCC->CR & RCC_CR_PLLRDY) != 0UL) { /* wait for PLL to stop */ }

    /* 5. Configure PLL: source=HSI16, M=1, N=8, R=2 */
    RCC->PLLCFGR = (RCC_PLLCFGR_PLLSRC_HSI) |
                   (BSP_PLLM << RCC_PLLCFGR_PLLM_Pos) |
                   (BSP_PLLN << RCC_PLLCFGR_PLLN_Pos) |
                   (BSP_PLLR << RCC_PLLCFGR_PLLR_Pos) |
                   RCC_PLLCFGR_PLLREN;

    /* 6. Enable PLL and wait for lock */
    SET_BIT(RCC->CR, RCC_CR_PLLON);
    st = wait_flag(&RCC->CR, RCC_CR_PLLRDY, CLOCK_TIMEOUT);
    if (st != STATUS_OK) { return STATUS_ERR_TIMEOUT; }

    /* 7. Switch SYSCLK to PLLRCLK (SW = 010) */
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_1);
    st = wait_flag(&RCC->CFGR, RCC_CFGR_SWS_1, CLOCK_TIMEOUT);
    if (st != STATUS_OK) { return STATUS_ERR_TIMEOUT; }

    return STATUS_OK;
}
