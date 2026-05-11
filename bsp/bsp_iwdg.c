/**
 * @file  bsp_iwdg.c
 * @brief IWDG driver for STM32G030F6P6.
 *        Timeout = (64 × 1000) / 32000 = 2.0 s
 */
#include "bsp/bsp_iwdg.h"
#include "stm32g0xx.h"

#define IWDG_KEY_ENABLE  (0xCCCCU)
#define IWDG_KEY_WRITE   (0x5555U)
#define IWDG_KEY_REFRESH (0xAAAAU)
#define IWDG_PR_DIV64    (4U)
#define IWDG_RELOAD      (1000U)

void BSP_IWDG_Init(void)
{
    IWDG->KR  = IWDG_KEY_ENABLE;
    IWDG->KR  = IWDG_KEY_WRITE;
    IWDG->PR  = IWDG_PR_DIV64;
    IWDG->RLR = IWDG_RELOAD;
    while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0UL) { }
    IWDG->KR  = IWDG_KEY_REFRESH;
}

void BSP_IWDG_Refresh(void)
{
    IWDG->KR = IWDG_KEY_REFRESH;
}
