# Integrasi Sensor CO Infwin via USART1

## Overview

Firmware ini telah diupgrade untuk mendukung pembacaan sensor CO Infwin melalui USART1 dengan sistem **dual-mode compile-time** yang memungkinkan USART1 beralih fungsi antara:

- **Mode Debug** (USART1_DEBUG_MODE): USART1 @ 115200 baud untuk output serial debug
- **Mode Sensor** (USART1_SENSOR_MODE): USART1 @ 9600 baud untuk komunikasi dengan sensor CO Infwin

## Konfigurasi Mode

Mode dipilih saat compile dengan mengedit file `drivers/drv_co_infwin.h`:

```c
/* Production Mode: USART1 reads CO sensor */
#define USART1_SENSOR_MODE

/* Development Mode: USART1 outputs debug messages */
// #define USART1_DEBUG_MODE
```

**⚠️ PENTING**: Hanya satu mode yang boleh aktif (uncomment salah satu saja).

## Hardware Configuration

### DeMux SP4T (PA0/PA1) Setting

| Mode            | PA1 | PA0 | Fungsi USART1                          |
|-----------------|-----|-----|----------------------------------------|
| DEBUG_MODE      | 1   | 1   | TTL debug output via MUX               |
| SENSOR_MODE     | 0   | 0   | RS485 port untuk sensor CO Infwin      |

Firmware secara otomatis mengatur PA0/PA1 sesuai mode yang dipilih saat boot.

### Pin Assignment

```
PA0/PA1    SP4T DeMux Control (auto-configured by firmware)
PA2        USART2_TX  9600 baud   RS485 Modbus slave ID=1
PA3        USART2_RX  9600 baud   RS485 Modbus slave ID=1
PA4        LED (blink 500ms normal, 100ms error)
PA11/PA12  I2C2 SCL/SDA  100 kHz  SEN66 sensor
PB1        RS485 DE control for USART2 (HIGH=TX, LOW=RX)
PB6        USART1_TX  — Debug OR CO sensor (mode-dependent)
PB7        USART1_RX  — Debug OR CO sensor (mode-dependent)
```

## Protokol Sensor CO Infwin

### Request Frame (8 bytes)
```
0x62 0x03 0x00 0x03 0x00 0x01 [CRC_L] [CRC_H]
```

### Response Frame (7 bytes)
```
0x62 0x03 0x02 [CO_H] [CO_L] [CRC_L] [CRC_H]
```

- **Protokol**: Modbus RTU subset
- **Baud rate**: 9600
- **Interval polling**: 1 detik
- **Timeout response**: 500 ms
- **CRC**: Modbus CRC16 (polynomial 0x8005, init 0xFFFF)

## Register Modbus

Firmware menyediakan 18 register Modbus (address 0x00–0x11):

| Address | Parameter        | Scale  | Unit    | Sensor   |
|---------|------------------|--------|---------|----------|
| 0x00    | PM1.0            | ÷10    | µg/m³   | SEN66    |
| 0x01    | PM2.5            | ÷10    | µg/m³   | SEN66    |
| 0x02    | PM4.0            | ÷10    | µg/m³   | SEN66    |
| 0x03    | PM10             | ÷10    | µg/m³   | SEN66    |
| 0x04    | RH               | ÷100   | %RH     | SEN66    |
| 0x05    | Temperature      | ÷10    | °C      | SEN66    |
| 0x06    | VOC index        | ÷10    | —       | SEN66    |
| 0x07    | NOx index        | ÷10    | —       | SEN66    |
| 0x08    | CO₂              | ×1     | ppm     | SEN66    |
| 0x09    | NC0.5            | ÷10    | #/cm³   | SEN66    |
| 0x0A    | NC1.0            | ÷10    | #/cm³   | SEN66    |
| 0x0B    | NC2.5            | ÷10    | #/cm³   | SEN66    |
| 0x0C    | NC4.0            | ÷10    | #/cm³   | SEN66    |
| 0x0D    | NC10             | ÷10    | #/cm³   | SEN66    |
| 0x0E    | Status flags     | —      | bitmask | SEN66    |
| 0x0F    | Sensor state     | —      | 0–4     | SEN66    |
| 0x10    | Error count      | —      | —       | SEN66    |
| 0x11    | CO               | x1     | ppm     | Infwin   |

### Register 0x11 — CO Concentration

- **SENSOR_MODE**: Nilai aktual dari sensor CO Infwin (ppm)
- **DEBUG_MODE**: Selalu 0 (sensor tidak tersambung)
- **Last Known Good Value**: Jika sensor mengirim 0, register akan mempertahankan nilai valid terakhir untuk mencegah false alarm

## Fitur Last Known Good Value

Kedua sensor (SEN66 dan CO) menerapkan mekanisme **last known good value**:

- Jika sensor mengirim nilai 0, sistem akan menggunakan nilai valid terakhir yang non-zero
- Ini mencegah glitch komunikasi sesaat dari mempropagasi nilai error ke Modbus clients
- Data fresh dan error counter tetap di-update untuk monitoring

## Build Instructions

### 1. Pilih Mode

Edit `drivers/drv_co_infwin.h`:

```c
// Untuk production dengan sensor CO:
#define USART1_SENSOR_MODE

// Untuk development/debugging:
#define USART1_DEBUG_MODE
```

### 2. Build

**Windows PowerShell:**
```powershell
.\build_flash.ps1 -Clean
```

**Linux/Mac (Makefile):**
```bash
make clean
make
```

### 3. Flash (Optional)

**PowerShell dengan STM32CubeProgrammer:**
```powershell
.\build_flash.ps1 -Flash
```

**Makefile dengan OpenOCD:**
```bash
make flash
```

## Memory Usage

Build dengan SENSOR_MODE aktif:

```
Flash: 6744 bytes (10.29% of 64 KB)
RAM  : 2120 bytes (25.88% of 8 KB)
```

## Testing

### Mode Sensor (USART1_SENSOR_MODE)

1. Hubungkan sensor CO Infwin ke USART1 via DeMux port RS485
2. Pastikan DeMux PA0=0, PA1=0 (firmware set otomatis)
3. Query Modbus register 0x11:
   ```
   Request:  01 03 00 11 00 01 [CRC]
   Response: 01 03 02 [CO_H] [CO_L] [CRC]
   ```

### Mode Debug (USART1_DEBUG_MODE)

1. Hubungkan USB-TTL adapter ke USART1 (115200 baud)
2. Pastikan DeMux PA0=1, PA1=1 (firmware set otomatis)
3. Monitor output:
   ```
   [SEN66] PM1.0=12.3 | PM2.5=25.1 | ... | CO2=450ppm [Err=0]
   ```
4. Register 0x11 akan mengembalikan 0

## Troubleshooting

### Sensor CO tidak terbaca (register 0x11 = 0)

1. Cek mode: pastikan `USART1_SENSOR_MODE` aktif saat compile
2. Cek DeMux: PA0=0, PA1=0 (gunakan oscilloscope/multimeter)
3. Cek wiring: USART1 TX/RX ke RS485 transceiver A/B
4. Cek baud rate: 9600 baud (BRR=6667 @ 64MHz PCLK)
5. Cek protokol: sensor harus respond dengan address 0x62

### LED blink cepat (100ms)

- SEN66 error state (bukan CO sensor)
- Cek I2C wiring, power supply SEN66
- Monitor register 0x10 (error count)

### Build error "conflicting types for MB_Slave_Process"

- Pastikan hanya satu mode aktif di `drv_co_infwin.h`
- Clean build: `.\build_flash.ps1 -Clean`

## File Structure

```
drivers/
  drv_co_infwin.h       Mode selection + API declarations
  drv_co_infwin.c       CO sensor driver implementation
  drv_sen66.h/c         SEN66 I2C driver

bsp/
  bsp_uart.h/c          Dual-mode USART1 + USART2 RS485
  bsp_gpio.h/c          DeMux control (PA0/PA1)

modbus/
  mb_slave.h/c          Extended register map (0x00–0x11)

main.c                  Main loop with dual-mode init
```

## Version History

- **v1.3** (2026-07-12): Added CO Infwin sensor support via USART1
  - Dual-mode USART1 (debug/sensor)
  - Register 0x11 for CO ppm
  - Automatic DeMux configuration
  - Last known good value mechanism for CO sensor

- **v1.2**: Added last known good value for SEN66
- **v1.1**: USART2 RS485 Modbus slave
- **v1.0**: SEN66 I2C + basic Modbus gateway

---

**Firmware**: SEN66+CO-G031 v1.3  
**Target**: STM32G031F8P6  
**Compiler**: arm-none-eabi-gcc  
**Build Date**: 2026-07-12
