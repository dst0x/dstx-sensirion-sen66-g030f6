/**
 * @file  bsp_gpio.c
 * @brief GPIO init for STM32G031F8P6 (TSSOP20).
 *
 * PA0  CD4052 DeMux Control A (bit 0) - SET HIGH for debug/TTL mode
 * PA1  CD4052 DeMux Control B (bit 1) - SET HIGH for debug/TTL mode
 * PA2  LPUART1_TX AF6 RS485 TX (9600 baud)
 * PA3  LPUART1_RX AF6 RS485 RX (9600 baud)
 * PA4  LED output
 * PA11 I2C2_SCL  AF6  SEN66
 * PA12 I2C2_SDA  AF6  SEN66
 * PB1  RS485_DE/RE    RS485 Mode Control (HIGH=Drive, LOW=Logging)
 * PB6  USART1_TX AF0  Debug via MUX (115200 baud) - routed via TTL mode
 * PB7  USART1_RX AF0  Debug via MUX (115200 baud) - routed via TTL mode
 *
 * SP4T DeMux Modes (PA1:PA0):
 *   00 = RS485  - Route to RS485 transceiver
 *   01 = RS232  - Route to RS232 transceiver
 *   10 = SDI-12 - Route to SDI-12 interface
 *   11 = TTL    - DEBUG MODE (route USART1 for debugging)
 *
 * RS485 Transceiver Control (PB1):
 *   HIGH = Drive Mode (DE enabled, transmit active)
 *   LOW  = Logging Mode (RE enabled, receive/monitor active)
 *
 * Note: 
 * - MUX initialized to TTL mode (11) enables USART1 debugging
 * - LPUART1 on PA2/PA3 (AF6) directly connected to RS485 transceiver
 * - PB1 control is automatic in BSP_RS485_Send()
 * - Default state: Logging Mode (LOW) for continuous monitoring
 */
#include "bsp/bsp_gpio.h"
#include "stm32g0xx.h"

#define PIN_MODE_AF(p)     (2UL << ((p) * 2U))
#define PIN_MODE_OUT(p)    (1UL << ((p) * 2U))
#define PIN_MODE_MASK(p)   (3UL << ((p) * 2U))
#define PIN_OT_OD(p)       (1UL << (p))
#define PIN_OSPEED_HIGH(p) (2UL << ((p) * 2U))
#define PIN_OSPEED_MASK(p) (3UL << ((p) * 2U))
#define PIN_PUPD_PU(p)     (1UL << ((p) * 2U))
#define PIN_PUPD_MASK(p)   (3UL << ((p) * 2U))
#define AFR_SET(p, af)     ((uint32_t)(af) << (((p) % 8U) * 4U))
#define AFR_MASK(p)        (0xFUL << (((p) % 8U) * 4U))

#define PIN_DEMUX_A     (0U)
#define PIN_DEMUX_B     (1U)
#define PIN_USART2_TX   (2U)
#define PIN_USART2_RX   (3U)
#define PIN_LED         (4U)
#define PIN_I2C_SCL     (11U)
#define PIN_I2C_SDA     (12U)
#define PIN_RS485_DE    (1U)
#define PIN_USART1_TX   (6U)
#define PIN_USART1_RX   (7U)

#define AF_I2C2      (6U)
#define AF_USART1    (0U)
#define AF_USART2    (1U)

void BSP_GPIO_Init(void)
{
    SET_BIT(RCC->IOPENR, RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN);
    (void)RCC->IOPENR;

    /* SP4T DeMux Control — PA0/PA1, push-pull output, start 11 (TTL mode for debug) */
    GPIOA->MODER  &= ~(PIN_MODE_MASK(PIN_DEMUX_A) | PIN_MODE_MASK(PIN_DEMUX_B));
    GPIOA->MODER  |=  (PIN_MODE_OUT(PIN_DEMUX_A)  | PIN_MODE_OUT(PIN_DEMUX_B));
    GPIOA->OTYPER &= ~(PIN_OT_OD(PIN_DEMUX_A)     | PIN_OT_OD(PIN_DEMUX_B));
    GPIOA->ODR    |=  ((1UL << PIN_DEMUX_A) | (1UL << PIN_DEMUX_B));  /* 11 = TTL (debug mode) */

    /* USART2 TX/RX — PA2/PA3, AF1, push-pull (RS485) */
    GPIOA->MODER   &= ~(PIN_MODE_MASK(PIN_USART2_TX) | PIN_MODE_MASK(PIN_USART2_RX));
    GPIOA->MODER   |=  (PIN_MODE_AF(PIN_USART2_TX)   | PIN_MODE_AF(PIN_USART2_RX));
    GPIOA->OTYPER  &= ~(PIN_OT_OD(PIN_USART2_TX)     | PIN_OT_OD(PIN_USART2_RX));
    GPIOA->OSPEEDR &= ~(PIN_OSPEED_MASK(PIN_USART2_TX) | PIN_OSPEED_MASK(PIN_USART2_RX));
    GPIOA->OSPEEDR |=  (PIN_OSPEED_HIGH(PIN_USART2_TX) | PIN_OSPEED_HIGH(PIN_USART2_RX));
    GPIOA->PUPDR   &= ~(PIN_PUPD_MASK(PIN_USART2_TX) | PIN_PUPD_MASK(PIN_USART2_RX));
    GPIOA->PUPDR   |=   PIN_PUPD_PU(PIN_USART2_RX);
    GPIOA->AFR[0]  &= ~(AFR_MASK(PIN_USART2_TX) | AFR_MASK(PIN_USART2_RX));
    GPIOA->AFR[0]  |=  (AFR_SET(PIN_USART2_TX, AF_USART2) | AFR_SET(PIN_USART2_RX, AF_USART2));

    /* LED — PA4, push-pull output, start OFF */
    GPIOA->MODER  &= ~PIN_MODE_MASK(PIN_LED);
    GPIOA->MODER  |=  PIN_MODE_OUT(PIN_LED);
    GPIOA->OTYPER &= ~PIN_OT_OD(PIN_LED);
    GPIOA->ODR    &= ~(1UL << PIN_LED);

    /* I2C2 SCL/SDA — PA11/PA12, AF6, open-drain, pull-up */
    GPIOA->MODER   &= ~(PIN_MODE_MASK(PIN_I2C_SCL) | PIN_MODE_MASK(PIN_I2C_SDA));
    GPIOA->MODER   |=  (PIN_MODE_AF(PIN_I2C_SCL)   | PIN_MODE_AF(PIN_I2C_SDA));
    GPIOA->OTYPER  |=  (PIN_OT_OD(PIN_I2C_SCL)     | PIN_OT_OD(PIN_I2C_SDA));
    GPIOA->OSPEEDR &= ~(PIN_OSPEED_MASK(PIN_I2C_SCL) | PIN_OSPEED_MASK(PIN_I2C_SDA));
    GPIOA->OSPEEDR |=  (PIN_OSPEED_HIGH(PIN_I2C_SCL) | PIN_OSPEED_HIGH(PIN_I2C_SDA));
    GPIOA->PUPDR   &= ~(PIN_PUPD_MASK(PIN_I2C_SCL) | PIN_PUPD_MASK(PIN_I2C_SDA));
    GPIOA->PUPDR   |=  (PIN_PUPD_PU(PIN_I2C_SCL)   | PIN_PUPD_PU(PIN_I2C_SDA));
    GPIOA->AFR[1]  &= ~(AFR_MASK(PIN_I2C_SCL) | AFR_MASK(PIN_I2C_SDA));
    GPIOA->AFR[1]  |=  (AFR_SET(PIN_I2C_SCL, AF_I2C2) | AFR_SET(PIN_I2C_SDA, AF_I2C2));

    /* RS485 Mode Control — PB1, push-pull output, start LOW (Logging mode) */
    GPIOB->MODER  &= ~PIN_MODE_MASK(PIN_RS485_DE);
    GPIOB->MODER  |=  PIN_MODE_OUT(PIN_RS485_DE);
    GPIOB->OTYPER &= ~PIN_OT_OD(PIN_RS485_DE);
    GPIOB->ODR    &= ~(1UL << PIN_RS485_DE);  /* Default: Logging Mode (LOW) */

    /* USART1 TX/RX — PB6/PB7, AF0, push-pull (Debug) */
    GPIOB->MODER   &= ~(PIN_MODE_MASK(PIN_USART1_TX) | PIN_MODE_MASK(PIN_USART1_RX));
    GPIOB->MODER   |=  (PIN_MODE_AF(PIN_USART1_TX)   | PIN_MODE_AF(PIN_USART1_RX));
    GPIOB->OTYPER  &= ~(PIN_OT_OD(PIN_USART1_TX)     | PIN_OT_OD(PIN_USART1_RX));
    GPIOB->OSPEEDR &= ~(PIN_OSPEED_MASK(PIN_USART1_TX) | PIN_OSPEED_MASK(PIN_USART1_RX));
    GPIOB->OSPEEDR |=  (PIN_OSPEED_HIGH(PIN_USART1_TX) | PIN_OSPEED_HIGH(PIN_USART1_RX));
    GPIOB->PUPDR   &= ~(PIN_PUPD_MASK(PIN_USART1_TX) | PIN_PUPD_MASK(PIN_USART1_RX));
    GPIOB->PUPDR   |=   PIN_PUPD_PU(PIN_USART1_RX);
    GPIOB->AFR[0]  &= ~(AFR_MASK(PIN_USART1_TX) | AFR_MASK(PIN_USART1_RX));
    GPIOB->AFR[0]  |=  (AFR_SET(PIN_USART1_TX, AF_USART1) | AFR_SET(PIN_USART1_RX, AF_USART1));
}

void BSP_GPIO_LedOn(void)     { GPIOA->BSRR = (1UL << PIN_LED); }
void BSP_GPIO_LedOff(void)    { GPIOA->BSRR = (1UL << (PIN_LED + 16U)); }
void BSP_GPIO_LedToggle(void) { GPIOA->ODR  ^= (1UL << PIN_LED); }

/* RS485 Transceiver Control */
void BSP_GPIO_RS485_DriveMode(void)   { GPIOB->BSRR = (1UL << PIN_RS485_DE); }   /* HIGH = Drive (TX) */
void BSP_GPIO_RS485_LoggingMode(void) { GPIOB->BSRR = (1UL << (PIN_RS485_DE + 16U)); } /* LOW = Logging (RX) */

void BSP_GPIO_SetCommMode(CommMode_t mode)
{
    uint32_t demux_val = (uint32_t)mode & 0x03U;
    
    /* Clear both bits first */
    GPIOA->BSRR = ((1UL << PIN_DEMUX_A) << 16U) | ((1UL << PIN_DEMUX_B) << 16U);
    
    /* Set bits according to mode */
    if ((demux_val & 0x01U) != 0U) { GPIOA->BSRR = (1UL << PIN_DEMUX_A); }
    if ((demux_val & 0x02U) != 0U) { GPIOA->BSRR = (1UL << PIN_DEMUX_B); }
}
