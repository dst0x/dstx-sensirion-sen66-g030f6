/**
 * @file  mb_slave.h
 * @brief Modbus RTU Slave — SEN66 register map.
 *
 * FC03/FC04 Read Holding/Input Registers (read-only):
 *   Addr  Description          Scale
 *   0x00  PM1.0  µg/m³         ÷10
 *   0x01  PM2.5  µg/m³         ÷10
 *   0x02  PM4.0  µg/m³         ÷10
 *   0x03  PM10   µg/m³         ÷10
 *   0x04  RH     %RH           ÷100
 *   0x05  Temp   °C            ÷10
 *   0x06  VOC index            ÷10
 *   0x07  NOx index            ÷10
 *   0x08  CO2    ppm           ×1
 *   0x09  NC0.5  #/cm³         ÷10
 *   0x0A  NC1.0  #/cm³         ÷10
 *   0x0B  NC2.5  #/cm³         ÷10
 *   0x0C  NC4.0  #/cm³         ÷10
 *   0x0D  NC10   #/cm³         ÷10
 *   0x0E  Status flags         bit0=pm,1=env,2=gas,3=co2
 *   0x0F  Sensor state         0-4
 *   0x10  Error count
 */
#ifndef MB_SLAVE_H
#define MB_SLAVE_H

#include "common/common_types.h"
#include "drivers/drv_sen66.h"

#ifndef MB_SLAVE_ID
#define MB_SLAVE_ID         (0x01U)
#endif

#define MB_FRAME_TIMEOUT_MS (5U)
#define MB_RX_BUF_SIZE      (32U)
#define MB_TX_BUF_SIZE      (64U)
#define MB_REG_FIRST        (0x0000U)
#define MB_REG_LAST         (0x0010U)
#define MB_REG_COUNT        (MB_REG_LAST - MB_REG_FIRST + 1U)
#define MB_FC_READ_HOLDING  (0x03U)
#define MB_FC_READ_INPUT    (0x04U)
#define MB_EX_ILLEGAL_FUNC  (0x01U)
#define MB_EX_ILLEGAL_ADDR  (0x02U)
#define MB_EX_ILLEGAL_VALUE (0x03U)

typedef struct
{
    uint8_t  rx_buf[MB_RX_BUF_SIZE];
    uint8_t  tx_buf[MB_TX_BUF_SIZE];
    uint16_t rx_len;
    uint32_t last_rx_tick;
    bool     frame_ready;
} MB_Slave_Ctx_t;

void MB_Slave_Init(MB_Slave_Ctx_t *ctx);
void MB_Slave_RxByte(MB_Slave_Ctx_t *ctx, uint8_t byte, uint32_t tick);
void MB_Slave_Process(MB_Slave_Ctx_t *ctx, const Sen66_Ctx_t *sensor_ctx, uint16_t *tx_len);

#endif /* MB_SLAVE_H */
