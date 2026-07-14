# Konfigurasi Final STM32G031F8P6 - SEN66 Modbus Gateway

## ✅ BUILD SUCCESS!

### 📊 Memory Usage
```
FLASH: 6,976 bytes / 64 KB (10.64% used)
SRAM:  2,312 bytes / 8 KB  (28.22% used)
```

### 📁 Output Files
- `build/sen66_g031f8.elf` - ELF executable
- `build/sen66_g031f8.hex` - Intel HEX format
- `build/sen66_g031f8.bin` - Binary format (untuk flash)
- `build/sen66_g031f8.map` - Memory map

---

## 🔌 Pinout Configuration

### MCU: STM32G031F8P6 (TSSOP20)
- **Core**: ARM Cortex-M0+ @ 64 MHz
- **Flash**: 64 KB
- **SRAM**: 8 KB

### Pin Assignments

| Pin  | Function      | AF  | Description                              |
|------|---------------|-----|------------------------------------------|
| **PA0** | **DEMUX_A**   | GPIO | **SP4T DeMux Control bit 0 (SET HIGH)** |
| **PA1** | **DEMUX_B**   | GPIO | **SP4T DeMux Control bit 1 (SET HIGH)** |
| PA2  | USART2_TX     | AF1 | RS485 Modbus TX (9600 baud)              |
| PA3  | USART2_RX     | AF1 | RS485 Modbus RX (9600 baud)              |
| PA4  | LED           | GPIO | Activity LED                            |
| PA11 | I2C2_SCL      | AF6 | SEN66 Sensor Clock (100 kHz)             |
| PA12 | I2C2_SDA      | AF6 | SEN66 Sensor Data (100 kHz)              |
| **PB1** | **RS485_DE**  | GPIO | **RS485 Driver Enable (TX/RX control)** |
| **PB6** | **USART1_TX** | AF0 | **Debug UART TX (115200 baud) via MUX**  |
| **PB7** | **USART1_RX** | AF0 | **Debug UART RX (115200 baud) via MUX**  |

---

## 🎛️ SP4T DeMux Configuration (CD4052)

### Control Pins
- **PA0** (A): Control bit 0 - **SET TO HIGH**
- **PA1** (B): Control bit 1 - **SET TO HIGH**

### Mode Selection Table

| PA1 (B) | PA0 (A) | Mode    | Description                           |
|---------|---------|---------|---------------------------------------|
| 0       | 0       | RS485   | Modbus RTU over RS485                 |
| 0       | 1       | RS232   | Serial RS232 communication            |
| 1       | 0       | SDI-12  | SDI-12 environmental sensor protocol  |
| **1**   | **1**   | **TTL** | **USART1 Debug Mode (ACTIVE)**        |

### ⚠️ **IMPORTANT: Current Configuration**
**MUX is set to TTL mode (11) at startup:**
- PA1 = **HIGH**
- PA0 = **HIGH**
- This routes USART1 (PB6/PB7) untuk debugging
- Untuk menggunakan RS485 Modbus, ubah ke mode RS485 (00) dengan:
  ```c
  BSP_GPIO_SetCommMode(COMM_MODE_RS485);
  ```

---

## 🔧 UART Configuration

### Debug UART (USART1) - Via MUX TTL Mode
- **TX**: PB6 (AF0)
- **RX**: PB7 (AF0)
- **Baudrate**: 115200
- **Format**: 8N1 (8 data bits, No parity, 1 stop bit)
- **Mode**: Bidirectional (TX interrupt-driven, RX polled)
- **Routing**: Via SP4T DeMux in TTL mode (PA1=1, PA0=1)

**Terminal Settings:**
```
Baud Rate: 115200
Data bits: 8
Parity: None
Stop bits: 1
Flow control: None
```

**Expected Output:**
```
SEN66-G031 v1.0 [Debug Mode]
SEN66 OK. Warming up 60s...
[SEN66] PM1.0 : 5.2 | PM2.5 : 8.1 | ... 
```

### RS485 Modbus UART (USART2)
- **TX**: PA2 (AF1)
- **RX**: PA3 (AF1)
- **DE**: PB1 (GPIO output - automatic control)
- **Baudrate**: 9600
- **Format**: 8N1
- **Mode**: Half-duplex with automatic DE control
- **Protocol**: Modbus RTU Slave (ID = 1)

**Supported Functions:**
- 0x03: Read Holding Registers
- 0x04: Read Input Registers

---

## 🌡️ SEN66 Sensor (I2C2)

### Interface
- **SCL**: PA11 (AF6)
- **SDA**: PA12 (AF6)
- **Speed**: 100 kHz (Standard Mode)
- **Protocol**: I2C

### Measured Parameters
1. PM1.0 (µg/m³)
2. PM2.5 (µg/m³)
3. PM4.0 (µg/m³)
4. PM10 (µg/m³)
5. Humidity (% RH)
6. Temperature (°C)
7. VOC Index
8. NOx Index
9. CO2 (ppm) - estimated

---

## 💾 How to Flash

### Option 1: STM32CubeProgrammer (Recommended)
1. Buka STM32CubeProgrammer
2. Connect ST-Link
3. Select file: `build/sen66_g031f8.bin`
4. Start address: `0x08000000`
5. Click "Download"

### Option 2: ST-Link Utility
1. Buka ST-Link Utility
2. Connect to target
3. Load file: `build/sen66_g031f8.hex`
4. Program & Verify

### Option 3: OpenOCD (jika sudah install)
```bash
.\build_flash.ps1 -Flash
```
atau
```bash
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
  -c "program build/sen66_g031f8.bin verify reset exit 0x08000000"
```

### Option 4: Command Line (Windows)
```powershell
# Menggunakan STM32_Programmer_CLI
STM32_Programmer_CLI -c port=SWD -w build\sen66_g031f8.bin 0x08000000 -v -rst
```

---

## 🧪 Testing & Verification

### 1. Debug UART Test
```bash
# Connect serial terminal ke USART1 @ 115200 baud
# Expected output:
SEN66-G031 v1.0 [Debug Mode]
SEN66 OK. Warming up 60s...
[SEN66] PM1.0 : 5.2 | PM2.5 : 8.1 | PM4.0 : 9.3 | ...
```

### 2. LED Behavior
- **Slow blink (500ms)**: Sensor OK, normal operation
- **Fast blink (100ms)**: Sensor error or offline

### 3. I2C Sensor Check
- Check debug output untuk status SEN66 initialization
- Verify sensor data updates setiap 1 detik

### 4. Modbus Communication (setelah switch ke RS485 mode)
```python
# Example using pymodbus
from pymodbus.client import ModbusSerialClient

client = ModbusSerialClient(
    port='COM3',
    baudrate=9600,
    parity='N',
    stopbits=1,
    bytesize=8,
    timeout=1
)

# Read PM2.5 (register 1)
result = client.read_holding_registers(1, 1, slave=1)
pm25 = result.registers[0] / 10.0
print(f"PM2.5: {pm25} µg/m³")
```

---

## 🔄 Mode Switching

### Untuk Switch ke RS485 Mode (Modbus)
Tambahkan di main.c setelah init:
```c
// Switch MUX to RS485 mode
BSP_GPIO_SetCommMode(COMM_MODE_RS485);  // PA1=0, PA0=0
BSP_DBG_WriteStr("Switching to RS485 mode\r\n");
```

### Untuk Kembali ke Debug Mode (TTL)
```c
// Switch MUX back to TTL mode  
BSP_GPIO_SetCommMode(COMM_MODE_TTL);  // PA1=1, PA0=1
BSP_DBG_WriteStr("Debug mode active\r\n");
```

### API Functions
```c
typedef enum {
    COMM_MODE_RS485  = 0x00U,  // PA1=0, PA0=0
    COMM_MODE_RS232  = 0x01U,  // PA1=0, PA0=1
    COMM_MODE_SDI12  = 0x02U,  // PA1=1, PA0=0
    COMM_MODE_TTL    = 0x03U   // PA1=1, PA0=1 (DEFAULT)
} CommMode_t;

void BSP_GPIO_SetCommMode(CommMode_t mode);
```

---

## ⚙️ System Configuration

### Clock Tree
- **Source**: HSI16 (16 MHz internal oscillator)
- **PLL Configuration**:
  - PLLM: /1 → 16 MHz
  - PLLN: x8 → 128 MHz
  - PLLR: /2 → **64 MHz** (SYSCLK)
- **Peripherals**:
  - AHB:  64 MHz
  - APB:  64 MHz
- **Flash Latency**: 2 wait states
- **Prefetch**: Enabled

### Watchdog (IWDG)
- **Enabled**: Yes
- **Timeout**: ~1 second
- **Purpose**: Auto-recovery dari hang/crash

### Interrupts Priority
- **USART2 (RS485)**: Priority 1 (highest)
- **USART1 (Debug)**: Priority 2
- **SysTick**: Default

---

## 📝 Build Commands

### Clean Build
```bash
.\build_flash.ps1 -Clean
```

### Build Only
```bash
.\build_flash.ps1
```

### Build and Flash
```bash
.\build_flash.ps1 -Flash
```
*(requires OpenOCD installed)*

### Using Makefile (Linux/WSL/MinGW)
```bash
make clean
make
make flash
make size   # Show memory usage
make dump   # Generate disassembly
```

---

## 🐛 Troubleshooting

### Debug UART tidak muncul output
- ✅ Check koneksi PB6 (TX) ke USB-Serial adapter
- ✅ Verify baudrate: 115200
- ✅ **Pastikan MUX dalam mode TTL (PA0=HIGH, PA1=HIGH)**
- ✅ Check power supply (3.3V stable)

### SEN66 tidak terdeteksi
- ✅ Check koneksi I2C (PA11/PA12)
- ✅ Check sensor power (5V for SEN66)
- ✅ Verify pull-up resistors (4.7kΩ recommended)

### Modbus tidak respond
- ✅ **Switch MUX ke RS485 mode dulu!**
- ✅ Check RS485 DE control (PB1)
- ✅ Verify baudrate: 9600
- ✅ Check slave ID: 1
- ✅ Check RS485 bus termination (120Ω)

### LED tidak blink
- ✅ Check PA4 connection
- ✅ Verify firmware running (check via debugger)

---

## 📚 Documentation Files

- `README_G031.md` - Dokumentasi lengkap project
- `PANDUAN_MODBUS.md` - Register map Modbus
- `KONFIGURASI_FINAL.md` - File ini
- `STM32G031F8PX_FLASH.ld` - Linker script
- `Makefile` - Build configuration

---

## 🎯 Quick Start Checklist

- [x] Build firmware: `.\build_flash.ps1 -Clean`
- [x] Flash to MCU menggunakan ST-Link
- [x] Connect debug UART (PB6 TX) ke USB-Serial @ 115200 baud
- [x] Verify startup message: "SEN66-G031 v1.0 [Debug Mode]"
- [x] Check LED blinking (500ms normal, 100ms error)
- [x] Verify sensor data di debug UART
- [ ] **Optional**: Switch ke RS485 mode untuk Modbus testing

---

## ✨ Key Features

✅ **64 KB Flash / 8 KB SRAM** - Cukup untuk development  
✅ **SP4T DeMux** - Flexible mode switching  
✅ **Debug Mode Active** - USART1 via TTL untuk debugging  
✅ **RS485 with DE Control** - Automatic TX/RX switching  
✅ **Modbus RTU Slave** - Function 0x03 & 0x04 support  
✅ **SEN66 Support** - 9 environmental parameters  
✅ **Watchdog Protection** - Auto-recovery  
✅ **Low Memory Usage** - Only 10.64% flash used  

---

**Build Date**: 2026-06-25  
**Version**: 1.0  
**Status**: ✅ **READY FOR DEPLOYMENT**

---

## 🚀 Deploy ke Production

Untuk production deployment:
1. Test semua fungsi secara menyeluruh
2. Switch MUX ke RS485 mode di startup jika diperlukan
3. Disable debug messages untuk menghemat bandwidth
4. Enable semua watchdog dan error recovery
5. Document register map untuk end user

**Current Mode**: **DEBUG MODE (TTL)** - Sesuai untuk development dan testing
