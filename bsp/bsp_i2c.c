/**
 * @file  bsp_i2c.c
 * @brief I2C2 driver for STM32G030F6P6.
 *        100 kHz @ 64 MHz PCLK — TIMINGR = 0x9032191F
 */
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_systick.h"
#include "stm32g0xx.h"

#define I2C2_TIMINGR     (0x9032191FUL)
#define I2C_FLAG_TIMEOUT (5000UL)

static Status_t wait_flag_set(volatile const uint32_t *reg, uint32_t mask, uint32_t timeout)
{
    uint32_t cnt = 0U;
    while (cnt < timeout)
    {
        if ((*reg & mask) != 0UL) { return STATUS_OK; }
        cnt++;
    }
    return STATUS_ERR_TIMEOUT;
}

static Status_t check_nack(void)
{
    if ((I2C2->ISR & I2C_ISR_NACKF) != 0UL)
    {
        SET_BIT(I2C2->ICR, I2C_ICR_NACKCF);
        SET_BIT(I2C2->CR2, I2C_CR2_STOP);
        return STATUS_ERR_I2C;
    }
    return STATUS_OK;
}

void BSP_I2C_Init(void)
{
    SET_BIT(RCC->APBENR1, RCC_APBENR1_I2C2EN);
    (void)RCC->APBENR1;
    SET_BIT(RCC->APBRSTR1, RCC_APBRSTR1_I2C2RST);
    CLR_BIT(RCC->APBRSTR1, RCC_APBRSTR1_I2C2RST);
    CLR_BIT(I2C2->CR1, I2C_CR1_PE);
    I2C2->TIMINGR = I2C2_TIMINGR;
    I2C2->CR1     = 0U;
    SET_BIT(I2C2->CR1, I2C_CR1_PE);
}

void BSP_I2C_BusRecover(void)
{
    uint8_t i;
    GPIOA->MODER &= ~(3UL << (11U * 2U));
    GPIOA->MODER |=  (1UL << (11U * 2U));
    for (i = 0U; i < 9U; i++)
    {
        GPIOA->BSRR = (1UL << 11U);
        BSP_SysTick_DelayMs(1U);
        GPIOA->BSRR = (1UL << (11U + 16U));
        BSP_SysTick_DelayMs(1U);
    }
    GPIOA->MODER &= ~(3UL << (11U * 2U));
    GPIOA->MODER |=  (2UL << (11U * 2U));
}

Status_t BSP_I2C_WriteCmd(uint8_t addr7, uint16_t cmd)
{
    Status_t st;
    uint8_t  cmd_hi = (uint8_t)((cmd >> 8U) & 0xFFU);
    uint8_t  cmd_lo = (uint8_t)(cmd & 0xFFU);

    I2C2->ICR = 0xFFFFFFFFUL;

    if ((I2C2->ISR & I2C_ISR_BUSY) != 0UL)
    {
        BSP_I2C_BusRecover();
        BSP_SysTick_DelayMs(10U);
    }

    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr7 << 1U) | (2UL << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_FLAG_TIMEOUT);
    if (st != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_hi;

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_FLAG_TIMEOUT);
    if (st != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_lo;

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_FLAG_TIMEOUT);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    return st;
}

Status_t BSP_I2C_WriteReadData(uint8_t addr7, uint16_t cmd, uint8_t *buf, uint8_t len)
{
    Status_t st;
    uint8_t  cmd_hi = (uint8_t)((cmd >> 8U) & 0xFFU);
    uint8_t  cmd_lo = (uint8_t)(cmd & 0xFFU);
    uint8_t  i;

    if ((buf == NULL) || (len == 0U)) { return STATUS_ERR_PARAM; }

    I2C2->ICR = 0xFFFFFFFFUL;

    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr7 << 1U) | (2UL << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_FLAG_TIMEOUT);
    if (st != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_hi;

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_FLAG_TIMEOUT);
    if (st != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_lo;

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_FLAG_TIMEOUT);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    if (st != STATUS_OK) { return STATUS_ERR_I2C; }

    BSP_SysTick_DelayMs(20U);

    I2C2->ICR = 0xFFFFFFFFUL;
    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr7 << 1U) | ((uint32_t)len << I2C_CR2_NBYTES_Pos) |
               I2C_CR2_RD_WRN | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    for (i = 0U; i < len; i++)
    {
        st = wait_flag_set(&I2C2->ISR, I2C_ISR_RXNE, I2C_FLAG_TIMEOUT);
        if (st != STATUS_OK) { return STATUS_ERR_I2C; }
        buf[i] = (uint8_t)(I2C2->RXDR & 0xFFUL);
    }

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_FLAG_TIMEOUT);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    return st;
}

Status_t BSP_I2C_ReadData(uint8_t addr7, uint8_t *buf, uint8_t len)
{
    Status_t st;
    uint8_t  i;

    if ((buf == NULL) || (len == 0U)) { return STATUS_ERR_PARAM; }

    I2C2->ICR = 0xFFFFFFFFUL;
    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr7 << 1U) | ((uint32_t)len << I2C_CR2_NBYTES_Pos) |
               I2C_CR2_RD_WRN | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    for (volatile uint32_t d = 0; d < 100U; d++) { }

    for (i = 0U; i < len; i++)
    {
        st = wait_flag_set(&I2C2->ISR, I2C_ISR_RXNE, I2C_FLAG_TIMEOUT);
        if (st != STATUS_OK) { return STATUS_ERR_I2C; }
        buf[i] = (uint8_t)(I2C2->RXDR & 0xFFUL);
    }

    st = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_FLAG_TIMEOUT);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    return st;
}

Status_t BSP_I2C_Scan(uint8_t addr7)
{
    I2C2->ICR = 0xFFFFFFFFUL;
    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr7 << 1U) | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    uint32_t timeout = I2C_FLAG_TIMEOUT;
    while (timeout > 0U)
    {
        if ((I2C2->ISR & I2C_ISR_NACKF) != 0UL)
        {
            SET_BIT(I2C2->ICR, I2C_ICR_NACKCF | I2C_ICR_STOPCF);
            return STATUS_ERR_I2C;
        }
        if ((I2C2->ISR & I2C_ISR_STOPF) != 0UL)
        {
            SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
            return STATUS_OK;
        }
        timeout--;
    }
    return STATUS_ERR_TIMEOUT;
}
