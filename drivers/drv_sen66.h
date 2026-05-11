/**
 * @file  drv_sen66.h
 * @brief Sensirion SEN66 driver — I2C 0x6B, 100 kHz.
 *
 * Register map (FC03 addr=0, qty=9):
 *   [0] PM1.0  ÷10 µg/m³
 *   [1] PM2.5  ÷10 µg/m³
 *   [2] PM4.0  ÷10 µg/m³
 *   [3] PM10   ÷10 µg/m³
 *   [4] RH     ÷100 %RH
 *   [5] Temp   ÷10 °C
 *   [6] VOC    ÷10
 *   [7] NOx    ÷10
 *   [8] CO2    ppm
 */

#ifndef DRV_SEN66_H
#define DRV_SEN66_H

#include "common/common_types.h"

#define SEN66_I2C_ADDR         (0x6BU)
#define SEN66_CMD_START_MEAS   (0x0021U)
#define SEN66_CMD_STOP_MEAS    (0x0104U)
#define SEN66_CMD_DATA_READY   (0x0202U)
#define SEN66_CMD_READ_VALUES  (0x0300U)
#define SEN66_CMD_READ_PM_NC   (0x0316U)
#define SEN66_CMD_READ_PRODUCT (0xD014U)
#define SEN66_CMD_RESET        (0xD304U)

#define SEN66_STARTUP_MS       (100U)
#define SEN66_READ_DELAY_MS    (20U)
#define SEN66_STOP_DELAY_MS    (1400U)
#define SEN66_RESET_DELAY_MS   (1200U)
#define SEN66_WARMUP_MS        (60000UL)

#define SEN66_MEAS_PAYLOAD_BYTES (27U)
#define SEN66_NC_PAYLOAD_BYTES   (15U)
#define SEN66_MAX_ERRORS         (5U)

#define SEN66_UNAVAIL_U16  (0xFFFFU)
#define SEN66_INIT_U16     (0xFFFEU)
#define SEN66_UNAVAIL_I16  ((int16_t)0x7FFF)

typedef enum
{
    SEN66_STATE_UNINIT     = 0U,
    SEN66_STATE_IDLE       = 1U,
    SEN66_STATE_WARMING_UP = 2U,
    SEN66_STATE_RUNNING    = 3U,
    SEN66_STATE_ERROR      = 4U
} Sen66_State_t;

typedef struct
{
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm4_0;
    uint16_t pm10;
    uint16_t nc0_5;
    uint16_t nc1_0;
    uint16_t nc2_5;
    uint16_t nc4_0;
    uint16_t nc10;
    uint16_t typ_size;
    int16_t  rh;
    int16_t  temp;
    int16_t  voc;
    int16_t  nox;
    uint16_t co2_ppm;
    bool     pm_valid;
    bool     env_valid;
    bool     gas_valid;
    bool     co2_valid;
} Sen66_Data_t;

typedef struct
{
    Sen66_State_t state;
    Sen66_Data_t  data;
    uint32_t      start_tick;
    uint32_t      last_read_tick;
    uint8_t       error_count;
    bool          data_fresh;
    uint8_t       raw_meas[SEN66_MEAS_PAYLOAD_BYTES];
} Sen66_Ctx_t;

Status_t      DRV_SEN66_Init(Sen66_Ctx_t *ctx);
Status_t      DRV_SEN66_StartMeasurement(Sen66_Ctx_t *ctx);
Status_t      DRV_SEN66_StopMeasurement(Sen66_Ctx_t *ctx);
Status_t      DRV_SEN66_Poll(Sen66_Ctx_t *ctx);
Status_t      DRV_SEN66_Reset(Sen66_Ctx_t *ctx);
Sen66_State_t DRV_SEN66_GetState(const Sen66_Ctx_t *ctx);
uint8_t       DRV_SEN66_CalcCRC(const uint8_t data[2U]);

#endif /* DRV_SEN66_H */