# SEN66 Modbus Gateway - STM32G031F8P6

Firmware untuk SEN66 Air Quality Sensor Gateway dengan komunikasi Modbus RTU pada STM32G031F8P6.

## 📋 Spesifikasi Hardware

### MCU: STM32G031F8P6 (TSSOP20)
- **Core**: ARM Cortex-M0+ @ 64 MHz
- **Flash**: 64 KB
- **SRAM**: 8 KB
- **Package**: TSSOP20

### Pinout Konfigurasi

| Pin  | Function      | Description                              | Config        |
|------|---------------|------------------------------------------|---------------|
| PA0  | DEMUX_A       | SP4T DeMux Control bit 0                 | Output        |
| PA1  | DEMUX_B       | SP4T DeMux Control bit 1 (MSB)           | Output        |
| PA2  | LPUART1_TX    | RS485/RS232/SDI-12/TTL TX (9600 baud)    | AF6           |
| PA3  | LPUART1_RX    | RS485/RS232/SDI-12/TTL RX (9600 baud)    | AF6, Pull-up  |
| PA4  | LED           | Activity LED                             | Output        |
| PA11 | I2C2_SCL      | SEN66 I2C Clock                          | AF6, OD, PU   |
| PA12 | I2C2_SDA      | SEN66 I2C Data                           | AF6, OD, PU   |
| PB1  | RS485_DE      | RS485 Driver Enable                      | Output        |
| PB6  | USART1_TX     | Debug UART TX (115200 baud)              | AF0           |
| PB7  | USART1_RX     | Debug UART RX (115200 baud)              | AF0, Pull-up  |

## 🔌 SP4T DeMux (CD4052) - Mode Komunikasi

Multiplexer SP4T dikendalikan oleh pin PA0 (bit A) dan PA1 (bit B) untuk memilih mode komunikasi:

| PA1 (B) | PA0 (A) | Mode    | Description                           |
|---------|---------|---------|---------------------------------------|
| 0       | 0       | RS485   | Modbus RTU over RS485 (default)       |
| 0       | 1       | RS232   | Serial RS232 communication            |
| 1       | 0       | SDI-12  | SDI-12 environmental sensor protocol  |
| 1       | 1       | TTL     | Direct TTL level serial               |

### Mode Selection dalam Kode
```c
#include "bsp/bsp_gpio.h"

// Set mode komunikasi
BSP_GPIO_SetCommMode(COMM_MODE_RS485);  // Default
BSP_GPIO_SetCommMode(COMM_MODE_RS232);
BSP_GPIO_SetCommMode(COMM_MODE_SDI12);
BSP_GPIO_SetCommMode(COMM_MODE_TTL);
```

## 🌡️ SEN66 Air Quality Sensor

**Interface**: I2C2 (PA11/PA12) @ 100 kHz

**Parameters yang diukur:**
- PM1.0, PM2.5, PM4.0, PM10 (µg/m³)
- Humidity (% RH)
- Temperature (°C)
- VOC Index
- NOx Index
- CO2 (ppm) - estimated

## 📡 Modbus RTU Communication

**Interface**: LPUART1 (PA2/PA3) @ 9600 baud, 8N1  
**RS485 DE Control**: PB1 (automatic TX/RX switching)

### Modbus Configuration
- **Slave ID**: 1
- **Baud Rate**: 9600
- **Data Format**: 8 data bits, No parity, 1 stop bit
- **Supported Functions**: 
  - 0x03 (Read Holding Registers)
  - 0x04 (Read Input Registers)

### Register Map
Lihat file `PANDUAN_MODBUS.md` untuk detail lengkap register Modbus.

## 🐛 Debug UART

**Interface**: USART1 (PB6/PB7) @ 115200 baud, 8N1

**Output Debug**:
- Status sensor SEN66
- Pembacaan sensor real-time
- Error messages
- Modbus transaction logs (optional)

**Terminal Settings**:
```
Baud Rate: 115200
Data bits: 8
Parity: None
Stop bits: 1
Flow control: None
```

## 🔧 Build Instructions

### Prerequisites
- `arm-none-eabi-gcc` toolchain
- `make` utility
- OpenOCD + ST-Link (untuk flashing)

### Compile
```bash
make clean
make
```

### Flash ke MCU
```bash
make flash
```

Atau menggunakan PowerShell script:
```powershell
.\build_flash.ps1
```

### Memory Usage
```bash
make size
```

### Generate Disassembly
```bash
make dump
```

## 📊 Memory Configuration

**Linker Script**: `STM32G031F8PX_FLASH.ld`

```
FLASH: 64 KB @ 0x08000000
SRAM:  8 KB  @ 0x20000000
Stack: 1.5 KB
Heap:  256 B (unused, bare-metal)
```

## ⚙️ System Configuration

### Clock Configuration
- **Source**: HSI16 (16 MHz internal oscillator)
- **PLL**: 
  - PLLM = /1  → 16 MHz
  - PLLN = x8  → 128 MHz
  - PLLR = /2  → 64 MHz (SYSCLK)
- **Flash Latency**: 2 wait states
- **Prefetch**: Enabled

### Watchdog (IWDG)
- **Enabled**: Yes
- **Timeout**: ~1 second
- **Purpose**: Auto-recovery dari hang/crash

### LED Behavior
- **Blink Normal** (500ms): Sensor OK
- **Blink Fast** (100ms): Sensor error/offline

## 🚀 Getting Started

### 1. Hardware Setup
1. Connect SEN66 sensor ke I2C2 (PA11/PA12)
2. Connect RS485 transceiver:
   - LPUART1_TX (PA2) → RS485 DI
   - LPUART1_RX (PA3) → RS485 RO
   - RS485_DE (PB1) → RS485 DE/RE
3. Set SP4T DeMux ke mode RS485 (PA0=0, PA1=0) via hardware atau software
4. Connect debug UART (PB6 TX) ke USB-Serial adapter
5. Connect power supply (3.3V)

### 2. Flash Firmware
```bash
make flash
```

### 3. Verify via Debug UART
Connect serial terminal ke USART1 @ 115200 baud:
```
SEN66-G031 v1.0
SEN66 OK. Warming up 60s...
[SEN66] PM1.0 : 5.2 | PM2.5 : 8.1 | PM4.0 : 9.3 | PM10 : 10.2 ug/m3 | ...
```

### 4. Test Modbus Communication
Gunakan Modbus master tool (ModbusPoll, pymodbus, dll):
```python
# Example: Read PM2.5 value
slave_id = 1
register = 1  # PM2.5 register
value = modbus_client.read_holding_registers(register, 1, unit=slave_id)
pm25 = value.registers[0] / 10.0  # Convert to µg/m³
```

## 📝 File Structure

```
.
├── bsp/                    # Board Support Package
│   ├── bsp_clock.c/h      # Clock configuration
│   ├── bsp_gpio.c/h       # GPIO & SP4T DeMux control
│   ├── bsp_i2c.c/h        # I2C driver untuk SEN66
│   ├── bsp_iwdg.c/h       # Watchdog timer
│   ├── bsp_systick.c/h    # SysTick timer & delay
│   ├── bsp_uart.c/h       # UART drivers (debug & RS485)
│   └── startup_stm32g031xx.c  # Startup & vector table
├── drivers/                # Peripheral drivers
│   └── drv_sen66.c/h      # SEN66 sensor driver
├── modbus/                 # Modbus protocol
│   ├── mb_crc.c/h         # Modbus CRC calculation
│   └── mb_slave.c/h       # Modbus RTU slave implementation
├── common/
│   └── common_types.h     # Common type definitions
├── CMSIS/                  # ARM CMSIS headers
├── main.c                  # Main application
├── Makefile                # Build configuration
├── STM32G031F8PX_FLASH.ld # Linker script
└── PANDUAN_MODBUS.md      # Modbus register documentation
```

## 🔍 Troubleshooting

### SEN66 tidak terdeteksi
- Check koneksi I2C (SCL/SDA)
- Check power supply sensor (5V)
- Check pull-up resistors pada I2C bus (4.7kΩ)

### Modbus tidak respond
- Verify RS485 DE control (PB1)
- Check baudrate setting (9600)
- Verify slave ID (default = 1)
- Check SP4T DeMux setting (harus mode RS485: PA1=0, PA0=0)
- Check RS485 bus termination (120Ω)

### Debug UART tidak muncul output
- Check koneksi PB6 (TX)
- Verify baudrate (115200)
- Check USB-Serial adapter compatibility

### Firmware tidak jalan setelah flash
- Check power supply (3.3V stable)
- Verify correct linker script untuk STM32G031F8P6
- Check BOOT0 pin (harus LOW untuk run dari flash)

## 📖 References

- [STM32G031F8P6 Datasheet](https://www.st.com/resource/en/datasheet/stm32g031f8.pdf)
- [STM32G0 Reference Manual (RM0444)](https://www.st.com/resource/en/reference_manual/rm0444-stm32g0x1-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [SEN66 Datasheet](https://sensirion.com/products/catalog/SEN66/)
- [Modbus RTU Specification](https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf)

## 📄 License

Proprietary - Internal use only

## 👥 Author

Embedded Firmware Team

---

**Version**: 1.0  
**Last Updated**: 2026-06-24  
**MCU**: STM32G031F8P6  
**Toolchain**: arm-none-eabi-gcc
