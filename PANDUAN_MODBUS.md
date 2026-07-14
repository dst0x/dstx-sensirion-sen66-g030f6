# Panduan Penggunaan Modbus RTU - SEN66 Gateway

## 📋 Daftar Isi
1. [Pengenalan](#pengenalan)
2. [Konfigurasi Hardware](#konfigurasi-hardware)
3. [Peta Register Modbus](#peta-register-modbus)
4. [Cara Membaca Data](#cara-membaca-data)
5. [Konversi Data](#konversi-data)
6. [Contoh Praktis](#contoh-praktis)
7. [Troubleshooting](#troubleshooting)

---

## 🔍 Pengenalan

Proyek ini mengimplementasikan **Modbus RTU Slave** pada STM32G030F6P6 yang berfungsi sebagai gateway untuk sensor SEN66. Device ini membaca data dari sensor SEN66 melalui I2C dan menyediakan data tersebut melalui protokol Modbus RTU.

### Spesifikasi Modbus
- **Protokol**: Modbus RTU
- **Mode**: Slave (Server)
- **Slave ID**: `0x01` (dapat diubah di `mb_slave.h`)
- **Baudrate**: 9600 bps
- **Data bits**: 8
- **Parity**: None
- **Stop bits**: 1
- **Interface**: RS485 (USART1 - PB6/PB7)
- **Function Code**: FC03 (Read Holding Registers) dan FC04 (Read Input Registers)

---

## ⚙️ Konfigurasi Hardware

### Pinout STM32G030F6P6
```
PA2         → USART2_TX (Debug, 115200 baud)
PA4         → LED Status
PA11/PA12   → I2C2 SCL/SDA (SEN66 Sensor, 100 kHz)
PB6/PB7     → USART1 TX/RX (RS485 Modbus, 9600 baud)
```

### Koneksi RS485
```
Master Device (PC/PLC)     STM32 Slave
    A+ ←------------------→ A+
    B- ←------------------→ B-
    GND ←-----------------→ GND
```

---

## 📊 Peta Register Modbus

Device ini menyediakan **17 register** (alamat 0x0000 - 0x0010) yang dapat dibaca menggunakan Function Code 03 atau 04.

| Alamat | Parameter | Satuan | Tipe Data | Skala Konversi | Deskripsi |
|--------|-----------|--------|-----------|----------------|-----------|
| 0x0000 | PM1.0 | µg/m³ | uint16 | ÷ 10 | Particulate Matter 1.0 |
| 0x0001 | PM2.5 | µg/m³ | uint16 | ÷ 10 | Particulate Matter 2.5 |
| 0x0002 | PM4.0 | µg/m³ | uint16 | ÷ 10 | Particulate Matter 4.0 |
| 0x0003 | PM10 | µg/m³ | uint16 | ÷ 10 | Particulate Matter 10 |
| 0x0004 | Humidity | %RH | uint16 | ÷ 100 | Kelembaban Relatif |
| 0x0005 | Temperature | °C | uint16 | ÷ 10 | Suhu (signed) |
| 0x0006 | VOC Index | - | uint16 | ÷ 10 | Volatile Organic Compounds |
| 0x0007 | NOx Index | - | uint16 | ÷ 10 | Nitrogen Oxides |
| 0x0008 | CO2 | ppm | uint16 | × 1 | Karbon Dioksida |
| 0x0009 | NC0.5 | #/cm³ | uint16 | ÷ 10 | Number Concentration 0.5µm |
| 0x000A | NC1.0 | #/cm³ | uint16 | ÷ 10 | Number Concentration 1.0µm |
| 0x000B | NC2.5 | #/cm³ | uint16 | ÷ 10 | Number Concentration 2.5µm |
| 0x000C | NC4.0 | #/cm³ | uint16 | ÷ 10 | Number Concentration 4.0µm |
| 0x000D | NC10 | #/cm³ | uint16 | ÷ 10 | Number Concentration 10µm |
| 0x000E | Status Flags | - | uint16 | bit field | Status validitas data |
| 0x000F | Sensor State | - | uint16 | enum | Status sensor (0-4) |
| 0x0010 | Error Count | - | uint16 | × 1 | Jumlah error komunikasi |

### Status Flags (Register 0x000E)
```
Bit 0: pm_valid  → 1 = Data PM valid
Bit 1: env_valid → 1 = Data Environment (RH, Temp) valid
Bit 2: gas_valid → 1 = Data Gas (VOC, NOx) valid
Bit 3: co2_valid → 1 = Data CO2 valid
```

### Sensor State (Register 0x000F)
```
0 = IDLE
1 = INIT
2 = WARMUP
3 = READY
4 = ERROR
```

---

## 📖 Cara Membaca Data

### Format Request Modbus RTU

**Function Code 03 (Read Holding Registers):**
```
[Slave ID] [FC] [Start Addr Hi] [Start Addr Lo] [Qty Hi] [Qty Lo] [CRC Lo] [CRC Hi]
```

**Contoh - Baca 1 register (PM2.5) dari alamat 0x0001:**
```
Request:  01 03 00 01 00 01 [CRC]
          │  │  │  │  │  │
          │  │  │  │  │  └─ Quantity = 1 register
          │  │  │  └──────── Start Address = 0x0001
          │  └─────────────── Function Code = 03
          └────────────────── Slave ID = 01
```

**Response:**
```
Response: 01 03 02 00 C8 [CRC]
          │  │  │  │  │
          │  │  │  │  └─ Data Lo = 0xC8 (200 dalam desimal)
          │  │  └──────── Data Hi = 0x00
          │  └─────────── Byte Count = 2
          └────────────── Slave ID = 01

Nilai Raw = 0x00C8 = 200
Nilai Aktual = 200 ÷ 10 = 20.0 µg/m³
```

### Membaca Multiple Registers

**Contoh - Baca semua data PM (4 register) dari alamat 0x0000:**
```
Request:  01 03 00 00 00 04 [CRC]
          │  │  │  │  │  │
          │  │  │  │  │  └─ Quantity = 4 registers
          │  │  │  └──────── Start Address = 0x0000
          │  └─────────────── Function Code = 03
          └────────────────── Slave ID = 01

Response: 01 03 08 00 64 00 C8 00 FA 01 2C [CRC]
          │  │  │  └──────────────────────┘
          │  │  │         Data (8 bytes)
          │  │  └─ Byte Count = 8 (4 registers × 2 bytes)
          │  └──── Function Code = 03
          └─────── Slave ID = 01

Register 0x0000 (PM1.0)  = 0x0064 = 100 → 10.0 µg/m³
Register 0x0001 (PM2.5)  = 0x00C8 = 200 → 20.0 µg/m³
Register 0x0002 (PM4.0)  = 0x00FA = 250 → 25.0 µg/m³
Register 0x0003 (PM10)   = 0x012C = 300 → 30.0 µg/m³
```

---

## 🔢 Konversi Data

### 1. PM (Particulate Matter) - Register 0x0000 - 0x0003
```c
// Nilai raw dari Modbus (uint16)
uint16_t pm25_raw = 235;  // Contoh nilai dari register 0x0001

// Konversi ke µg/m³
float pm25_actual = (float)pm25_raw / 10.0f;
// Hasil: 23.5 µg/m³
```

### 2. Humidity (Kelembaban) - Register 0x0004
```c
// Nilai raw dari Modbus
uint16_t rh_raw = 5432;  // Contoh nilai

// Konversi ke %RH
float rh_actual = (float)rh_raw / 100.0f;
// Hasil: 54.32 %RH
```

### 3. Temperature (Suhu) - Register 0x0005
```c
// Nilai raw dari Modbus (signed!)
int16_t temp_raw = 245;  // Contoh nilai positif

// Konversi ke °C
float temp_actual = (float)temp_raw / 10.0f;
// Hasil: 24.5 °C

// Contoh suhu negatif
int16_t temp_raw_neg = -50;  // -5.0°C
float temp_neg = (float)temp_raw_neg / 10.0f;
// Hasil: -5.0 °C
```

### 4. VOC & NOx Index - Register 0x0006, 0x0007
```c
// Nilai raw dari Modbus
uint16_t voc_raw = 1250;

// Konversi ke index
float voc_index = (float)voc_raw / 10.0f;
// Hasil: 125.0 (index VOC)
```

### 5. CO2 - Register 0x0008
```c
// Nilai raw dari Modbus (tidak perlu konversi)
uint16_t co2_raw = 450;

// Nilai langsung dalam ppm
uint16_t co2_ppm = co2_raw;
// Hasil: 450 ppm
```

### 6. Number Concentration - Register 0x0009 - 0x000D
```c
// Nilai raw dari Modbus
uint16_t nc25_raw = 1234;

// Konversi ke #/cm³
float nc25_actual = (float)nc25_raw / 10.0f;
// Hasil: 123.4 #/cm³
```

### 7. Status Flags - Register 0x000E
```c
// Nilai raw dari Modbus
uint16_t status_flags = 0x000F;  // Binary: 0000 0000 0000 1111

// Ekstrak bit individual
bool pm_valid  = (status_flags & 0x0001) != 0;  // true
bool env_valid = (status_flags & 0x0002) != 0;  // true
bool gas_valid = (status_flags & 0x0004) != 0;  // true
bool co2_valid = (status_flags & 0x0008) != 0;  // true
```

### 8. Sensor State - Register 0x000F
```c
// Nilai raw dari Modbus
uint16_t state = 3;

// Interpretasi
const char* state_str[] = {
    "IDLE",    // 0
    "INIT",    // 1
    "WARMUP",  // 2
    "READY",   // 3
    "ERROR"    // 4
};

printf("Sensor State: %s\n", state_str[state]);
// Output: "Sensor State: READY"
```

---

## 💡 Contoh Praktis

### Contoh 1: Membaca PM2.5 dengan Python (pymodbus)

```python
from pymodbus.client import ModbusSerialClient

# Konfigurasi koneksi
client = ModbusSerialClient(
    port='COM3',        # Sesuaikan dengan port Anda
    baudrate=9600,
    bytesize=8,
    parity='N',
    stopbits=1,
    timeout=1
)

# Koneksi ke device
if client.connect():
    print("Terhubung ke Modbus Slave")
    
    # Baca register PM2.5 (alamat 0x0001, 1 register)
    result = client.read_holding_registers(
        address=0x0001,
        count=1,
        slave=1
    )
    
    if not result.isError():
        pm25_raw = result.registers[0]
        pm25_actual = pm25_raw / 10.0
        print(f"PM2.5: {pm25_actual} µg/m³")
    else:
        print("Error membaca data")
    
    client.close()
else:
    print("Gagal terhubung")
```

### Contoh 2: Membaca Semua Data Sensor

```python
from pymodbus.client import ModbusSerialClient

client = ModbusSerialClient(
    port='COM3',
    baudrate=9600,
    bytesize=8,
    parity='N',
    stopbits=1,
    timeout=1
)

if client.connect():
    # Baca semua 17 register (0x0000 - 0x0010)
    result = client.read_holding_registers(
        address=0x0000,
        count=17,
        slave=1
    )
    
    if not result.isError():
        regs = result.registers
        
        # Konversi data
        pm1_0 = regs[0x00] / 10.0
        pm2_5 = regs[0x01] / 10.0
        pm4_0 = regs[0x02] / 10.0
        pm10  = regs[0x03] / 10.0
        rh    = regs[0x04] / 100.0
        temp  = regs[0x05] / 10.0  # Bisa negatif
        voc   = regs[0x06] / 10.0
        nox   = regs[0x07] / 10.0
        co2   = regs[0x08]
        nc0_5 = regs[0x09] / 10.0
        nc1_0 = regs[0x0A] / 10.0
        nc2_5 = regs[0x0B] / 10.0
        nc4_0 = regs[0x0C] / 10.0
        nc10  = regs[0x0D] / 10.0
        
        # Status flags
        flags = regs[0x0E]
        pm_valid  = (flags & 0x01) != 0
        env_valid = (flags & 0x02) != 0
        gas_valid = (flags & 0x04) != 0
        co2_valid = (flags & 0x08) != 0
        
        # State dan error count
        state = regs[0x0F]
        error_count = regs[0x10]
        
        # Tampilkan data
        print(f"PM1.0: {pm1_0:.1f} µg/m³")
        print(f"PM2.5: {pm2_5:.1f} µg/m³")
        print(f"PM4.0: {pm4_0:.1f} µg/m³")
        print(f"PM10:  {pm10:.1f} µg/m³")
        print(f"RH:    {rh:.2f} %")
        print(f"Temp:  {temp:.1f} °C")
        print(f"VOC:   {voc:.1f}")
        print(f"NOx:   {nox:.1f}")
        print(f"CO2:   {co2} ppm")
        print(f"State: {state} | Errors: {error_count}")
        print(f"Valid: PM={pm_valid} ENV={env_valid} GAS={gas_valid} CO2={co2_valid}")
    
    client.close()
```

### Contoh 3: Menggunakan ModPoll (Windows)

```bash
# Baca 1 register PM2.5
modpoll -m rtu -b 9600 -p none -s 1 -a 1 -r 2 -c 1 COM3

# Baca semua 17 register
modpoll -m rtu -b 9600 -p none -s 1 -a 1 -r 1 -c 17 COM3

# Parameter:
# -m rtu     : Mode RTU
# -b 9600    : Baudrate 9600
# -p none    : No parity
# -s 1       : 1 stop bit
# -a 1       : Slave address 1
# -r 2       : Start dari register 2 (alamat 0x0001)
# -c 1       : Baca 1 register
# COM3       : Serial port
```

### Contoh 4: Arduino/ESP32

```cpp
#include <ModbusMaster.h>

// Inisialisasi Modbus Master
ModbusMaster node;

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600);  // RS485 pada Serial2
    
    // Komunikasi dengan slave ID 1
    node.begin(1, Serial2);
}

void loop() {
    uint8_t result;
    uint16_t data[17];
    
    // Baca 17 register mulai dari alamat 0x0000
    result = node.readHoldingRegisters(0x0000, 17);
    
    if (result == node.ku8MBSuccess) {
        // Ambil data dari buffer
        for (int i = 0; i < 17; i++) {
            data[i] = node.getResponseBuffer(i);
        }
        
        // Konversi dan tampilkan
        float pm25 = data[0x01] / 10.0;
        float temp = data[0x05] / 10.0;
        float rh = data[0x04] / 100.0;
        uint16_t co2 = data[0x08];
        
        Serial.printf("PM2.5: %.1f µg/m³\n", pm25);
        Serial.printf("Temp: %.1f °C\n", temp);
        Serial.printf("RH: %.2f %%\n", rh);
        Serial.printf("CO2: %d ppm\n", co2);
    } else {
        Serial.println("Modbus error!");
    }
    
    delay(2000);
}
```

---

## 🔧 Troubleshooting

### 1. Tidak Ada Response dari Slave

**Kemungkinan Penyebab:**
- Slave ID tidak sesuai (default: 0x01)
- Baudrate tidak cocok (harus 9600 bps)
- Koneksi RS485 A+/B- terbalik
- CRC error pada request

**Solusi:**
```python
# Pastikan konfigurasi benar
client = ModbusSerialClient(
    port='COM3',
    baudrate=9600,      # HARUS 9600
    bytesize=8,
    parity='N',
    stopbits=1,
    timeout=2           # Tambah timeout
)

# Cek dengan slave ID yang benar
result = client.read_holding_registers(0x0000, 1, slave=1)
```

### 2. Data Tidak Masuk Akal

**Kemungkinan Penyebab:**
- Sensor masih dalam mode WARMUP (60 detik setelah power on)
- Data belum valid (cek status flags)
- Konversi skala salah

**Solusi:**
```python
# Selalu cek status flags
flags = regs[0x0E]
state = regs[0x0F]

if state == 3:  # READY
    if (flags & 0x01):  # PM valid
        pm25 = regs[0x01] / 10.0  # Jangan lupa bagi 10
        print(f"PM2.5: {pm25} µg/m³")
    else:
        print("Data PM belum valid")
else:
    print(f"Sensor state: {state} (belum READY)")
```

### 3. Exception Response

**Exception Code:**
- `0x01` - Illegal Function (FC tidak didukung)
- `0x02` - Illegal Address (alamat register tidak valid)
- `0x03` - Illegal Value (jumlah register tidak valid)

**Contoh:**
```
Request:  01 03 00 20 00 01 [CRC]  # Alamat 0x0020 tidak ada
Response: 01 83 02 [CRC]            # Exception 0x02 (Illegal Address)
          │  │  │
          │  │  └─ Exception Code
          │  └──── FC + 0x80 (0x03 + 0x80 = 0x83)
          └─────── Slave ID
```

**Solusi:**
- Gunakan alamat 0x0000 - 0x0010 saja
- Maksimal baca 17 register sekaligus
- Gunakan FC 03 atau FC 04

### 4. CRC Error

**Penyebab:**
- Noise pada jalur RS485
- Baudrate tidak stabil
- Kabel terlalu panjang tanpa termination

**Solusi:**
- Gunakan kabel twisted pair untuk RS485
- Tambahkan termination resistor 120Ω di kedua ujung
- Maksimalkan panjang kabel 1200m (untuk 9600 bps)
- Gunakan ground yang sama

### 5. Timeout

**Penyebab:**
- Slave sedang sibuk (polling sensor)
- Timeout terlalu pendek
- Koneksi fisik bermasalah

**Solusi:**
```python
# Tambah timeout dan retry
client = ModbusSerialClient(
    port='COM3',
    baudrate=9600,
    timeout=2  # 2 detik
)

# Retry mechanism
max_retries = 3
for attempt in range(max_retries):
    result = client.read_holding_registers(0x0000, 17, slave=1)
    if not result.isError():
        break
    time.sleep(0.5)
```

---

## 📝 Catatan Penting

1. **Warm-up Period**: Sensor SEN66 memerlukan waktu warm-up 60 detik setelah power on. Data mungkin tidak akurat selama periode ini.

2. **Polling Rate**: Sensor di-poll setiap 1 detik (SENSOR_POLL_MS). Tidak perlu membaca Modbus lebih cepat dari ini.

3. **Watchdog**: Device menggunakan IWDG (Independent Watchdog). Jika terjadi hang, device akan auto-reset.

4. **LED Indicator**:
   - Blink 500ms: Normal operation
   - Blink 100ms: Error state

5. **Debug Output**: USART2 (PA2) mengeluarkan debug info setiap 2 detik pada 115200 baud.

6. **Temperature Handling**: Register temperature (0x0005) adalah signed integer. Nilai negatif menggunakan two's complement.

7. **Maximum Registers**: Maksimal baca 17 register dalam satu request (0x0000 - 0x0010).

---

## 📚 Referensi

- **Modbus Protocol**: [Modbus.org](https://modbus.org/)
- **SEN66 Datasheet**: Sensirion SEN66 Environmental Sensor Node
- **STM32G030**: STMicroelectronics Reference Manual

---

**Dibuat untuk proyek SEN66-G030 Gateway**  
**Versi: 1.0**  
**Tanggal: 2026**
