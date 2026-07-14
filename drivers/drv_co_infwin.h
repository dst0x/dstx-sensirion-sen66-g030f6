/**
 * @file  drv_co_infwin.h
 * @brief Infwin CO sensor driver via USART1.
 *
 * Protocol: Modbus RTU-like request/response
 * Request:  0x62 0x03 0x00 0x03 0x00 0x01 [CRC_L] [CRC_H]
 * Response: 0x62 0x03 0x02 [CO_H] [CO_L] [CRC_L] [CRC_H]
 *
 * CO concentration is in PPM (parts per million).
 *
 * ============================================================================
 * MODE CONFIGURATION (Choose ONE before compile):
 * ============================================================================
 * 
 * Option 1: USART1 as CO Sensor Interface (Production Mode)
 *   #define USART1_SENSOR_MODE
 *   - USART1 sends request to CO sensor periodically
 *   - Parses response and updates CO reading
 *   - Debug output disabled on USART1
 *
 * Option 2: USART1 as Debug Output (Development Mode - DEFAULT)
 *   #define USART1_DEBUG_MODE
 *   - USART1 used for printf-style debug messages
 *   - CO sensor reading disabled
 *   - Useful for troubleshooting
 *
 * To switch modes, edit main.c or this file and recompile.
 */

#ifndef DRV_CO_INFWIN_H
#define DRV_CO_INFWIN_H

#include "common/common_types.h"

/* ============================================================================
 * MODE SELECTION - CONFIGURE HERE
 * ============================================================================
 * Uncomment ONE of the following definitions:
 */

/* Production Mode: USART1 reads CO sensor */
#define USART1_SENSOR_MODE

/* Development Mode: USART1 outputs debug messages (DEFAULT) */
// #define USART1_DEBUG_MODE

/* Validate configuration */
#if defined(USART1_SENSOR_MODE) && defined(USART1_DEBUG_MODE)
    #error "Cannot enable both USART1_SENSOR_MODE and USART1_DEBUG_MODE simultaneously!"
#endif

#if !defined(USART1_SENSOR_MODE) && !defined(USART1_DEBUG_MODE)
    #error "Must define either USART1_SENSOR_MODE or USART1_DEBUG_MODE!"
#endif

/* ============================================================================
 * Driver Configuration
 * ============================================================================ */

#define CO_SENSOR_ADDR          (0x62U)
#define CO_REQUEST_INTERVAL_MS  (1000U)   /* Query sensor every 1 second */
#define CO_RESPONSE_TIMEOUT_MS  (500U)    /* Max wait time for response */
#define CO_MAX_ERRORS           (5U)      /* Error threshold before ERROR state */

#define CO_REQUEST_LEN          (8U)      /* Request frame length with CRC */
#define CO_RESPONSE_LEN         (7U)      /* Response frame length with CRC */

/* CO sensor unavailable/init markers (same as SEN66 convention) */
#define CO_UNAVAIL_U16          (0xFFFFU)
#define CO_INIT_U16             (0xFFFEU)

/* ============================================================================
 * Data Types
 * ============================================================================ */

typedef enum
{
    CO_STATE_UNINIT     = 0U,
    CO_STATE_IDLE       = 1U,
    CO_STATE_WARMING_UP = 2U,
    CO_STATE_RUNNING    = 3U,
    CO_STATE_ERROR      = 4U
} CO_State_t;

typedef struct
{
    uint16_t co_ppm;          /* CO concentration in PPM */
    bool     valid;           /* True if data is valid */
    uint32_t request_count;   /* DEBUG: Total requests sent */
    uint32_t response_count;  /* DEBUG: Total responses received */
    uint32_t timeout_count;   /* DEBUG: Total timeouts */
    uint32_t crc_error_count; /* DEBUG: Total CRC errors */
} CO_Data_t;

typedef struct
{
    CO_State_t state;
    CO_Data_t  data;
    CO_Data_t  last_valid_data;   /* Last known-good non-zero reading */
    uint32_t   last_request_tick;
    uint32_t   request_sent_tick;
    uint8_t    error_count;
    bool       data_fresh;
    bool       has_valid_data;    /* True once at least one valid reading stored */
    bool       waiting_response;
    uint8_t    rx_buf[CO_RESPONSE_LEN];
    uint8_t    rx_idx;
    uint8_t    last_response[CO_RESPONSE_LEN];  /* Store last successful response for debugging */
} CO_Ctx_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

#ifdef USART1_SENSOR_MODE

/**
 * @brief Initialize CO sensor driver (USART1 configured for sensor comm).
 */
Status_t DRV_CO_Init(CO_Ctx_t *ctx);

/**
 * @brief Send request to CO sensor.
 */
Status_t DRV_CO_SendRequest(CO_Ctx_t *ctx);

/**
 * @brief Poll CO sensor - call periodically in main loop.
 * @note Manages request timing and response parsing automatically.
 */
Status_t DRV_CO_Poll(CO_Ctx_t *ctx);

/**
 * @brief Get current sensor state.
 */
CO_State_t DRV_CO_GetState(const CO_Ctx_t *ctx);

/**
 * @brief Process received byte from USART1 (called from IRQ handler).
 */
void DRV_CO_RxByte(CO_Ctx_t *ctx, uint8_t byte);

/**
 * @brief Get last successful response (for debugging).
 * @note Only valid after at least one successful read (ctx->data_fresh == true)
 */
const uint8_t* DRV_CO_GetLastResponse(const CO_Ctx_t *ctx);

#endif /* USART1_SENSOR_MODE */

#endif /* DRV_CO_INFWIN_H */
