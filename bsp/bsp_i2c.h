/**
 * @file  bsp_i2c.h
 * @brief I2C2 driver — PA11=SCL, PA12=SDA, 100 kHz.
 */
#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "common/common_types.h"

void     BSP_I2C_Init(void);
void     BSP_I2C_BusRecover(void);
Status_t BSP_I2C_WriteCmd(uint8_t addr7, uint16_t cmd);
Status_t BSP_I2C_WriteReadData(uint8_t addr7, uint16_t cmd, uint8_t *buf, uint8_t len);
Status_t BSP_I2C_ReadData(uint8_t addr7, uint8_t *buf, uint8_t len);
Status_t BSP_I2C_Scan(uint8_t addr7);

#endif /* BSP_I2C_H */
