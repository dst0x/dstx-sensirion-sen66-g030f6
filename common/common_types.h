/**
 * @file    common_types.h
 * @brief   Project-wide types, status codes, and utility macros.
 *          Target: STM32G030F6P6 (Cortex-M0+, 64 MHz).
 * @note    MISRA C:2012 compliant. No dynamic allocation.
 */
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* =========================================================================
 * Status codes
 * =========================================================================*/
typedef enum
{
    STATUS_OK           =  0,
    STATUS_ERR_TIMEOUT  = -1,
    STATUS_ERR_CRC      = -2,
    STATUS_ERR_I2C      = -3,
    STATUS_ERR_PARAM    = -4,
    STATUS_ERR_STATE    = -5,
    STATUS_ERR_SENSOR   = -6,
    STATUS_NOT_READY    = -7,
    STATUS_ERR_GENERIC  = -8
} Status_t;

/* =========================================================================
 * Utility macros
 * =========================================================================*/
#define ARRAY_SIZE(arr)         ((uint32_t)(sizeof(arr) / sizeof((arr)[0U])))
#define UNUSED(x)               ((void)(x))

#ifndef SET_BIT
#define SET_BIT(reg, bit)       ((reg) |=  (uint32_t)(bit))
#endif
#ifndef CLR_BIT
#define CLR_BIT(reg, bit)       ((reg) &= ~(uint32_t)(bit))
#endif
#ifndef TST_BIT
#define TST_BIT(reg, bit)       (((reg) & (uint32_t)(bit)) != 0UL)
#endif
#ifndef MODIFY_REG
#define MODIFY_REG(reg, msk, val) ((reg) = (((reg) & ~(uint32_t)(msk)) | (uint32_t)(val)))
#endif

#define BYTES_TO_U16(msb, lsb)  (((uint16_t)(msb) << 8U) | (uint16_t)(lsb))
#define BYTES_TO_I16(msb, lsb)  ((int16_t)(BYTES_TO_U16((msb),(lsb))))

/* =========================================================================
 * Clock constants — STM32G030F6P6 @ 64 MHz (HSI16 + PLL)
 * =========================================================================*/
#define HSI_VALUE_HZ            (16000000UL)  /*!< HSI oscillator, Hz            */
#define SYSCLK_HZ               (64000000UL)  /*!< SYSCLK after PLL (max G030)   */
#define PCLK_HZ                 (64000000UL)  /*!< G0 has single APB = SYSCLK    */

#endif /* COMMON_TYPES_H */
