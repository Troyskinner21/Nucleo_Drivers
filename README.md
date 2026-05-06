Yocto                        ← Layer 8 (ongoing)
        ↑
U-Boot                       ← Layer 7
        ↑
Linux Kernel Drivers         ← Layer 6
        ↑
Buildroot + Linux Basics     ← Layer 5
        ↑
FreeRTOS                     ← Layer 4
        ↑
Bootloader Project           ← Layer 3
        ↑
Driver Reference Repo        ← Layer 2
        ↑
Protocol Refresher           ← Layer 1
        ↑
You are here — automotive AUTOSAR application layer






## 1. UART/SPI/I2C drivers first — on the Nucleo arriving tomorrow
You're already mid-stream on this. Finish what you started, get data flowing through PuTTY, validate all three methods work on real hardware. This is days not weeks given where you are.
## 2. FreeRTOS second
Take your UART interrupt driver and add FreeRTOS on top of it. Create a task that reads from the ring buffer and another that transmits. Now you have two real things talking to each other through a queue. Much more interesting than the hello world FreeRTOS examples and directly builds on what you just built.
## 3. Bootloader third
Now you have solid driver knowledge and RTOS exposure. The bootloader uses your UART driver for receiving firmware and your SPI driver for the W25Q128 flash chip. Everything feeds into it naturally.
## 4. WiFi weather station fourth
This is your capstone project that ties everything together:

UART → ESP8266 WiFi module
I2C → BME280 temperature/humidity sensor
SPI → W25Q128 to cache readings offline
FreeRTOS → separate tasks for sensor reading, WiFi transmission, data logging
Bootloader → OTA firmware updates over WiFi

That last project is genuinely impressive because it uses everything you've built. Every single layer you learned has a reason to exist in one cohesive system.

## 5. Linux
This is the cherry on top. You can take your UART/SPI/I2C drivers and port them to Linux kernel modules. Then you can write user-space applications that interact with those drivers, maybe even expose a simple REST API to read sensor data or trigger actions. This is where you see how all the embedded concepts scale up to real-world applications. Build a simple web server on the Nucleo that serves sensor data in real-time, and you have a full-stack embedded project that goes from bare-metal to Linux. You can build using Buildroot to create a custom Linux image for the Nucleo, and then use Yocto to take it even further with more complex applications and optimizations. This is where you can really show off your skills and have fun with it


# SPI vs. UART vs. I2C

## UART
- **Wires:** 2 (TX, RX)
- **How it works:** Asynchronous — no shared clock. Both sides agree on baud rate ahead of time. Bytes just stream out; there's no inherent framing so the application defines message boundaries (e.g. newline, fixed length, length-prefix byte).
- **Use when:** Talking to a PC terminal, GPS module, Bluetooth/WiFi module, or any device where you want a simple point-to-point byte stream. Only one device on each end — UART is not a bus.

## SPI
- **Wires:** 4 (SCK, MOSI, MISO, CS per slave)
- **How it works:** Synchronous — master drives the clock. Transactions are explicit: assert CS, clock out/in an exact number of bytes, deassert CS. Full-duplex, meaning TX and RX happen at the same time on every clock edge.
- **Use when:** You need speed or full-duplex on-chip peripherals: flash memory (W25Q128), SD cards, high-speed sensors, displays. Multiple slaves are supported by giving each its own CS line.

## I2C
- **Wires:** 2 (SDA, SCL) — shared by all devices on the bus
- **How it works:** Synchronous — master drives the clock. Every slave has a 7-bit address baked in at the factory. The master sends the address at the start of each transaction; only the matching slave responds. Half-duplex (data flows one direction at a time).
- **Use when:** You have several low-speed peripherals and want to minimise wire count. Typical targets: temperature/humidity sensors (BME280), IMUs (MPU-6050), EEPROMs, real-time clocks. Not suitable for fast or large data transfers.

## Quick comparison

| | UART | SPI | I2C |
|---|---|---|---|
| Wires | 2 | 4 + 1 CS/slave | 2 |
| Clock | None (async) | Master (sync) | Master (sync) |
| Duplex | Full | Full | Half |
| Multi-slave | No | Yes (one CS each) | Yes (by address) |
| Speed | Low–medium | Fast | Low–medium |
| Typical use | Debug, modules | Flash, displays | Sensors, EEPROMs |

