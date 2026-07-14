# RS485 Troubleshooting - Baud Rate Issues

## Problem
Data corruption: Sent `01 03 00 00 00 01` but received `0C 7C 00 00 00 0C`

## Root Cause Analysis
This pattern indicates **BAUD RATE MISMATCH** or **FRAMING ERROR**.

## Solution Steps

### 1. Verify USB-to-RS485 Adapter Settings
**CHECK YOUR PC/ADAPTER BAUD RATE!**
- Open your Modbus Master software
- Verify baud rate is set to **9600**
- Verify settings: **8N1** (8 data bits, No parity, 1 stop bit)
- Verify RS485 mode (not RS232)

### 2. Try Different BRR Values (if baud rate is correct)

Edit `bsp/bsp_uart.c` line 13, try these values one by one:

#### Current Value (for 9600 baud with LPUART @ 64MHz):
```c
#define LPUART1_BRR_9600   (1706667U)  // 0x1A0B0B
```

#### Alternative Values to Try:

**If clock is actually different:**
```c
// If using HSI16 (16MHz) directly:
#define LPUART1_BRR_9600   (426667U)   // 16MHz * 256 / 9600

// If using PCLK/2 (32MHz):
#define LPUART1_BRR_9600   (853333U)   // 32MHz * 256 / 9600

// If using PCLK/4 (16MHz):
#define LPUART1_BRR_9600   (426667U)   // 16MHz * 256 / 9600
```

**Common baud rates if adapter is set wrong:**
```c
// For 19200 baud:
#define LPUART1_BRR_9600   (853333U)   // 64MHz * 256 / 19200

// For 4800 baud:
#define LPUART1_BRR_9600   (3413333U)  // 64MHz * 256 / 4800

// For 115200 baud:
#define LPUART1_BRR_9600   (142222U)   // 64MHz * 256 / 115200
```

### 3. Hardware Check
- Verify RS485 A/B wiring (try swapping if not working)
- Ensure GND is connected between STM32 and RS485 transceiver
- Check if termination resistor (120Ω) is needed
- Verify PA2 (TX) and PA3 (RX) are connected to DI/RO of RS485 transceiver
- Verify PB1 (DE/RE) is connected to transceiver control pin

### 4. Check Binary Pattern

Your data showed:
```
Sent: 01 = 00000001 → Received: 0C = 00001100
Sent: 03 = 00000011 → Received: 7C = 01111100
```

This looks like **bit sampling at wrong timing** = BAUD RATE ISSUE!

## Most Likely Solution
**CHECK YOUR MODBUS MASTER SOFTWARE BAUD RATE SETTING!**
It should be set to **9600 baud, 8N1, no flow control**.

