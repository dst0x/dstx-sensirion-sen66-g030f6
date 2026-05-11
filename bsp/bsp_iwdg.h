/**
 * @file  bsp_iwdg.h
 * @brief IWDG driver — timeout 2 s (PR=64, RELOAD=1000, LSI=32kHz).
 */
#ifndef BSP_IWDG_H
#define BSP_IWDG_H

void BSP_IWDG_Init(void);
void BSP_IWDG_Refresh(void);

#endif /* BSP_IWDG_H */
