/**
 * @file  mb_crc.h
 * @brief Modbus CRC-16 (polynomial 0xA001).
 */
#ifndef MB_CRC_H
#define MB_CRC_H

#include "common/common_types.h"

uint16_t MB_CRC16(const uint8_t *buf, uint16_t len);

#endif /* MB_CRC_H */
