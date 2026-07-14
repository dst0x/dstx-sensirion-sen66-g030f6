/**
 * @file  bsp_uart.c
 * @brief USART1 dual-mode (debug/sensor) + USART2 RS485 (9600) for STM32G031F8P6.
 *        RS485 Mode: HIGH=Drive(TX), LOW=Logging(RX)
 * 
 * USART1 Mode Selection (compile-time):
 *   - USART1_DEBUG_MODE:  115200 baud, TX-only for debug output
 *   - USART1_SENSOR_MODE: 9600 baud, bidirectional for CO sensor
 */
#include "bsp/bsp_uart.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_iwdg.h"
#include "stm32g0xx.h"
#ifdef USART1_SENSOR_MODE
#include "drivers/drv_co_infwin.h"
#endif
#include <stddef.h>

#define USART1_BRR_115200  (556U)
#define USART1_BRR_9600    (6667U)
/* USART2 BRR = fCLK / baud_rate (standard UART, no 256x prescaler)
 * For fCLK = 64MHz, baud = 9600:
 * BRR = 64000000 / 9600 = 6666.67 ≈ 6667 (0x1A0B)
 */
#define USART2_BRR_9600    (6667U)
#define DBG_TX_MASK       (DBG_TX_BUF_SIZE   - 1U)
#define CO_RX_MASK        (CO_RX_BUF_SIZE    - 1U)
#define RS485_RX_MASK     (RS485_RX_BUF_SIZE - 1U)

typedef char _dbg_pow2[(((DBG_TX_BUF_SIZE)  & (DBG_TX_BUF_SIZE  - 1U)) == 0U) ? 1 : -1];
typedef char _rx_pow2 [(((RS485_RX_BUF_SIZE) & (RS485_RX_BUF_SIZE - 1U)) == 0U) ? 1 : -1];

#ifdef USART1_DEBUG_MODE
static struct {
    uint8_t           data[DBG_TX_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} g_dbg_tx;
#endif

#ifdef USART1_SENSOR_MODE
static struct {
    uint8_t           data[CO_RX_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} g_co_rx;
#endif

static struct {
    uint8_t           data[RS485_RX_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} g_rs485_rx;

static volatile uint16_t g_rs485_echo_count = 0U;

/* =========================================================================
 * USART1 MODE-SPECIFIC IMPLEMENTATIONS
 * =========================================================================*/

/* -------------------------------------------------------------------------
 * Debug UART (USART1) — TX interrupt-driven @ 115200 baud
 * Integer formatting helpers included here; only needed in debug mode.
 * -------------------------------------------------------------------------*/
#ifdef USART1_DEBUG_MODE

static uint8_t u32_to_dec(uint32_t val, char *buf)
{
    char    tmp[11U];
    uint8_t pos = 0U, len, i;
    if (val == 0U) { buf[0U] = '0'; buf[1U] = '\0'; return 1U; }
    while (val > 0U) { tmp[pos++] = (char)('0' + (uint8_t)(val % 10U)); val /= 10U; }
    len = pos;
    for (i = 0U; i < len; i++) { buf[i] = tmp[len - 1U - i]; }
    buf[len] = '\0';
    return len;
}

void BSP_DBG_Init(void)
{
    SET_BIT(RCC->APBENR2, RCC_APBENR2_USART1EN);
    (void)RCC->APBENR2;
    USART1->CR1 = 0U;
    USART1->BRR = USART1_BRR_115200;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;  /* Enable TX and RX */
    NVIC_SetPriority(USART1_IRQn, 2U);
    NVIC_EnableIRQ(USART1_IRQn);
}

uint16_t BSP_DBG_Write(const uint8_t *data, uint16_t len)
{
    uint16_t i, written = 0U;
    if (data == NULL) { return 0U; }
    for (i = 0U; i < len; i++)
    {
        uint16_t next = (uint16_t)((g_dbg_tx.head + 1U) & DBG_TX_MASK);
        if (next == g_dbg_tx.tail) { break; }
        g_dbg_tx.data[g_dbg_tx.head] = data[i];
        g_dbg_tx.head = next;
        written++;
    }
    if (written > 0U) { SET_BIT(USART1->CR1, USART_CR1_TXEIE_TXFNFIE); }
    return written;
}

void BSP_DBG_WriteStr(const char *str)
{
    uint16_t len = 0U;
    if (str == NULL) { return; }
    while ((str[len] != '\0') && (len < (uint16_t)DBG_TX_BUF_SIZE)) { len++; }
    (void)BSP_DBG_Write((const uint8_t *)str, len);
}

void BSP_DBG_WriteInt(const char *prefix, int32_t val, const char *suffix)
{
    char    buf[13U];
    uint8_t pos = 0U;
    uint32_t uval;
    BSP_DBG_WriteStr(prefix);
    if (val < 0) { buf[pos++] = '-'; uval = (uint32_t)(-(val + 1)) + 1U; }
    else         { uval = (uint32_t)val; }
    (void)u32_to_dec(uval, &buf[pos]);
    BSP_DBG_WriteStr(buf);
    BSP_DBG_WriteStr(suffix);
}

void BSP_DBG_WriteFixed(const char *prefix, int32_t raw, int32_t scale,
                        uint8_t decimals, const char *suffix)
{
    char     ibuf[12U], fbuf[5U], ftmp[4U];
    int32_t  ip, fp;
    uint32_t fa, fs;
    uint8_t  n, fi, fl;
    static const uint32_t div[4U] = {1U, 10U, 100U, 1000U};
    if ((scale == 0) || (decimals > 3U)) { return; }
    BSP_DBG_WriteStr(prefix);
    ip = raw / scale;
    fp = raw % scale;
    if ((raw < 0) && (ip == 0)) { BSP_DBG_WriteStr("-"); }
    n = u32_to_dec((ip < 0) ? (uint32_t)(-ip) : (uint32_t)ip, ibuf);
    ibuf[n] = '\0';
    if ((raw < 0) && (ip < 0)) { BSP_DBG_WriteStr("-"); }
    BSP_DBG_WriteStr(ibuf);
    if (decimals > 0U)
    {
        BSP_DBG_WriteStr(".");
        fa = (uint32_t)((fp < 0) ? (-fp) : fp);
        fs = div[decimals];
        fa = (uint32_t)((fa * fs) / (uint32_t)scale);
        fl = u32_to_dec(fa, ftmp);
        for (fi = 0U; fi < ((uint8_t)decimals - fl); fi++) { fbuf[fi] = '0'; }
        for (fi = 0U; fi < fl; fi++) { fbuf[(uint8_t)decimals - fl + fi] = ftmp[fi]; }
        fbuf[decimals] = '\0';
        BSP_DBG_WriteStr(fbuf);
    }
    BSP_DBG_WriteStr(suffix);
}

void BSP_DBG_WriteHex(const char *prefix, const uint8_t *buf, uint8_t len, const char *suffix)
{
    static const char lut[16U] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    char    pair[3U];
    uint8_t i;
    if (buf == NULL) { return; }
    BSP_DBG_WriteStr(prefix);
    for (i = 0U; i < len; i++)
    {
        pair[0U] = lut[(buf[i] >> 4U) & 0x0FU];
        pair[1U] = lut[buf[i] & 0x0FU];
        pair[2U] = '\0';
        BSP_DBG_WriteStr(pair);
        if (i < (len - 1U)) { BSP_DBG_WriteStr(" "); }
    }
    BSP_DBG_WriteStr(suffix);
}

void BSP_USART1_IRQHandler(void)
{
    if ((USART1->ISR & USART_ISR_TXE_TXFNF) != 0UL)
    {
        if (g_dbg_tx.tail != g_dbg_tx.head)
        {
            USART1->TDR = (uint32_t)g_dbg_tx.data[g_dbg_tx.tail];
            g_dbg_tx.tail = (uint16_t)((g_dbg_tx.tail + 1U) & DBG_TX_MASK);
        }
        else { CLR_BIT(USART1->CR1, USART_CR1_TXEIE_TXFNFIE); }
    }
}
#endif /* USART1_DEBUG_MODE */

#ifdef USART1_SENSOR_MODE
/* -------------------------------------------------------------------------
 * CO Sensor UART (USART1) — Bidirectional @ 9600 baud
 * -------------------------------------------------------------------------*/

/* Forward declaration — full type available via drv_co_infwin.h (pulled in
 * transitively through bsp_uart.h → drv_co_infwin.h).                      */
extern CO_Ctx_t g_co_sensor;   /* Defined in main.c */

void BSP_CO_UART_Init(void)
{
    SET_BIT(RCC->APBENR2, RCC_APBENR2_USART1EN);
    (void)RCC->APBENR2;
    USART1->CR1 = 0U;
    USART1->BRR = USART1_BRR_9600;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE;
    NVIC_SetPriority(USART1_IRQn, 2U);
    NVIC_EnableIRQ(USART1_IRQn);
}

uint16_t BSP_CO_UART_Write(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint32_t timeout_counter;
    
    if (data == NULL) { return 0U; }
    
    for (i = 0U; i < len; i++)
    {
        timeout_counter = 0U;
        while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0UL)
        {
            timeout_counter++;
            if ((timeout_counter & 0xFFFU) == 0U) { BSP_IWDG_Refresh(); }
            if (timeout_counter > 100000UL) { return i; }  /* Timeout */
        }
        USART1->TDR = (uint32_t)data[i];
    }
    
    /* Wait for transmission complete */
    timeout_counter = 0U;
    while ((USART1->ISR & USART_ISR_TC) == 0UL)
    {
        timeout_counter++;
        if ((timeout_counter & 0xFFFU) == 0U) { BSP_IWDG_Refresh(); }
        if (timeout_counter > 100000UL) { break; }
    }
    
    return len;
}

uint16_t BSP_CO_UART_RxCount(void)
{
    return (uint16_t)((g_co_rx.head - g_co_rx.tail) & CO_RX_MASK);
}

uint8_t BSP_CO_UART_RxGet(void)
{
    uint8_t byte = 0U;
    if (g_co_rx.tail != g_co_rx.head)
    {
        byte = g_co_rx.data[g_co_rx.tail];
        g_co_rx.tail = (uint16_t)((g_co_rx.tail + 1U) & CO_RX_MASK);
    }
    return byte;
}

void BSP_CO_UART_RxFlush(void)
{
    g_co_rx.tail = g_co_rx.head;
}

void BSP_USART1_IRQHandler(void)
{
    /* RX interrupt */
    if ((USART1->ISR & USART_ISR_RXNE_RXFNE) != 0UL)
    {
        uint8_t byte = (uint8_t)(USART1->RDR & 0xFFUL);
        /* Forward byte directly to driver; avoids void* cast issues */
        DRV_CO_RxByte(&g_co_sensor, byte);
    }

    /* Clear any framing / overrun / noise errors */
    if ((USART1->ISR & (USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE)) != 0UL)
    {
        SET_BIT(USART1->ICR, USART_ICR_FECF | USART_ICR_ORECF | USART_ICR_NECF);
    }
}
#endif /* USART1_SENSOR_MODE */

void USART1_IRQHandler(void) 
{ 
    BSP_USART1_IRQHandler(); 
}

/* -------------------------------------------------------------------------
 * RS485 UART (USART2) — RX interrupt, TX polled, echo suppression
 * -------------------------------------------------------------------------*/
void BSP_RS485_Init(void)
{
    /* USART2 uses PCLK (64MHz) */
    SET_BIT(RCC->APBENR1, RCC_APBENR1_USART2EN);
    (void)RCC->APBENR1;
    USART2->CR1 = 0U;
    USART2->BRR = USART2_BRR_9600;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE;
    NVIC_SetPriority(USART2_IRQn, 1U);
    NVIC_EnableIRQ(USART2_IRQn);
}

void BSP_RS485_Send(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint32_t timeout_counter;
    if (data == NULL) { return; }
    
    /* Flush RX buffer before transmission to avoid echo/garbage */
    BSP_RS485_RxFlush();
    
    /* Small delay to ensure line is stable */
    for (volatile uint32_t d = 0U; d < 1000U; d++) { }
    
    BSP_GPIO_RS485_DriveMode();  /* HIGH = Enable RS485 driver (transmit mode) */
    g_rs485_echo_count = len;
    
    /* Small delay after mode switch */
    for (volatile uint32_t d = 0U; d < 500U; d++) { }
    
    for (i = 0U; i < len; i++)
    {
        timeout_counter = 0U;
        while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0UL)
        {
            timeout_counter++;
            if ((timeout_counter & 0xFFFU) == 0U) { BSP_IWDG_Refresh(); }
        }
        USART2->TDR = (uint32_t)data[i];
    }
    
    timeout_counter = 0U;
    while ((USART2->ISR & USART_ISR_TC) == 0UL)
    {
        timeout_counter++;
        if ((timeout_counter & 0xFFFU) == 0U) { BSP_IWDG_Refresh(); }
    }
    
    /* Wait a bit more to ensure all bits transmitted */
    for (volatile uint32_t d = 0U; d < 1000U; d++) { }
    
    BSP_GPIO_RS485_LoggingMode();  /* LOW = Return to logging/receive mode */
    
    /* Small delay after mode switch */
    for (volatile uint32_t d = 0U; d < 500U; d++) { }
    
    SET_BIT(USART2->ICR, USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF);
    g_rs485_echo_count = 0U;
    
    /* Final flush to remove any echo that got through */
    BSP_RS485_RxFlush();
}

uint16_t BSP_RS485_RxCount(void)
{
    return (uint16_t)((g_rs485_rx.head - g_rs485_rx.tail) & RS485_RX_MASK);
}

uint8_t BSP_RS485_RxGet(void)
{
    uint8_t byte = 0U;
    if (g_rs485_rx.tail != g_rs485_rx.head)
    {
        byte = g_rs485_rx.data[g_rs485_rx.tail];
        g_rs485_rx.tail = (uint16_t)((g_rs485_rx.tail + 1U) & RS485_RX_MASK);
    }
    return byte;
}

void BSP_RS485_RxFlush(void)
{
    g_rs485_rx.tail = g_rs485_rx.head;
}

void BSP_RS485_IRQHandler(void)
{
    if ((USART2->ISR & USART_ISR_RXNE_RXFNE) != 0UL)
    {
        uint8_t byte = (uint8_t)(USART2->RDR & 0xFFUL);
        
        if (g_rs485_echo_count > 0U)
        {
            g_rs485_echo_count--;
        }
        else
        {
            uint16_t next = (uint16_t)((g_rs485_rx.head + 1U) & RS485_RX_MASK);
            if (next != g_rs485_rx.tail)
            {
                g_rs485_rx.data[g_rs485_rx.head] = byte;
                g_rs485_rx.head = next;
            }
        }
    }
    if ((USART2->ISR & (USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE)) != 0UL)
    {
        SET_BIT(USART2->ICR, USART_ICR_FECF | USART_ICR_ORECF | USART_ICR_NECF);
    }
}

void BSP_USART2_IRQHandler(void) 
{ 
    BSP_RS485_IRQHandler(); 
}

void USART2_IRQHandler(void) { BSP_USART2_IRQHandler(); }
