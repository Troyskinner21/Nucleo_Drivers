/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate CubeMX project for NUCLEO-L476RG with I2C1 enabled
 *    - Speed Mode: Standard (100 kHz) or Fast (400 kHz)
 *    - Leave all other settings at CubeMX defaults
 * 2. Copy i2c_blocking.h into Core/Inc, i2c_blocking.c into Core/Src
 * 3. Build and flash from CubeIDE
 *
 * I2C pinout on NUCLEO-L476RG (Arduino header / I2C1):
 *   PB8 → SCL
 *   PB9 → SDA
 *   Both lines need pull-up resistors to VCC (typically 4.7 kΩ at 100 kHz,
 *   2.2 kΩ at 400 kHz). Many breakout boards include these on-board.
 *
 * Key differences from SPI:
 * — No CS pin — slaves are addressed by a 7-bit address on the bus
 * — Half-duplex — SDA carries data in one direction at a time
 * — Slave can hold SCL low (clock stretching) while it prepares data —
 *   this is why BUSY is a valid status code
 * — ACK/NACK: every byte is acknowledged by the receiver; NACK means
 *   something went wrong (wrong address, slave not ready, bus fault)
 *
 * Address convention — the single most common I2C mistake:
 * — Datasheets publish the 7-bit address (e.g. BME280 = 0x76)
 * — HAL expects that address shifted left by 1: (0x76 << 1) = 0xEC
 * — Use the I2C_ADDR() macro from i2c_blocking.h to avoid confusion
 * — Example: I2C_ADDR(0x76) expands to 0xEC
 *
 * Examples below use the BME280 (temp/humidity/pressure sensor, 7-bit addr 0x76)
 * and MPU-6050 (IMU, 7-bit addr 0x68) as concrete reference targets since both
 * appear later in the capstone project.
 */

#include "i2c_blocking.h"

extern I2C_HandleTypeDef hi2c1;

/* Device addresses — shift once here, use the macros everywhere below */
#define BME280_ADDR  I2C_ADDR(0x76)   // SDO pin low  → 0x76; high → 0x77
#define MPU6050_ADDR I2C_ADDR(0x68)   // AD0 pin low  → 0x68; high → 0x69

/* BME280 register map (partial) */
#define BME280_REG_ID       0xD0   // chip ID — always reads 0x60 on genuine parts
#define BME280_REG_RESET    0xE0   // write 0xB6 to soft-reset the device
#define BME280_REG_CTRL_HUM 0xF2   // humidity oversampling config
#define BME280_REG_CTRL_MSR 0xF4   // temp + pressure oversampling + mode
#define BME280_REG_CONFIG   0xF5   // standby time, IIR filter
#define BME280_REG_PRESS_MSB 0xF7  // start of 6-byte raw pressure+temp+humidity burst

/* MPU-6050 register map (partial) */
#define MPU6050_REG_PWR_MGMT_1  0x6B   // power management — write 0x00 to wake up
#define MPU6050_REG_WHO_AM_I    0x75   // identity register — always reads 0x68
#define MPU6050_REG_ACCEL_XOUT_H 0x3B  // start of 14-byte raw accel+temp+gyro burst

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    /* Optional: re-apply settings explicitly — useful if you change speed */
    I2C_Blocking_Init(&hi2c1, 400000);  // 400 kHz fast mode


    /* ── Bus scan / device presence check ───────────────────────────────────
     *
     * Always confirm the device is on the bus at startup.
     * NACK means wrong address, missing pull-ups, or device not powered.
     * 3 trials gives the device time to finish any internal startup cycle.
     */
    if (I2C_Blocking_IsDeviceReady(&hi2c1, BME280_ADDR, 3, 100) != I2C_BLOCKING_OK)
    {
        /* Blink LED to signal no device found — check wiring and address */
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }


    /* ── ReadRegister: read the BME280 chip ID ───────────────────────────────
     *
     * Chip ID register always returns 0x60 for a genuine BME280.
     * Reading it first is a cheap sanity check that communication works
     * before attempting any configuration.
     */
    uint8_t chipId = 0;
    if (I2C_Blocking_ReadRegister(&hi2c1, BME280_ADDR, BME280_REG_ID,
                                   &chipId, 1, 100) != I2C_BLOCKING_OK)
    {
        /* Failed to read — check pull-ups, address, and clock speed */
    }
    /* chipId should now be 0x60 */


    /* ── WriteRegister: configure the BME280 ────────────────────────────────
     *
     * Write one byte to a single register — the most common I2C operation.
     * Set humidity oversampling to ×1 (must be written before ctrl_meas).
     */
    uint8_t humConfig = 0x01;   // osrs_h = 001 → oversampling ×1
    I2C_Blocking_WriteRegister(&hi2c1, BME280_ADDR, BME280_REG_CTRL_HUM,
                                &humConfig, 1, 100);

    /* Set temp ×1, pressure ×1, forced mode — triggers one measurement */
    uint8_t measConfig = 0x25;  // osrs_t=001, osrs_p=001, mode=01 (forced)
    I2C_Blocking_WriteRegister(&hi2c1, BME280_ADDR, BME280_REG_CTRL_MSR,
                                &measConfig, 1, 100);


    /* ── ReadRegister: burst-read raw sensor data ────────────────────────────
     *
     * Reading multiple consecutive registers in one call is more efficient
     * than reading them one at a time — one START/STOP pair moves all bytes.
     * BME280 raw output is 6 bytes starting at 0xF7:
     *   [0] press_msb  [1] press_lsb  [2] press_xlsb
     *   [3] temp_msb   [4] temp_lsb   [5] temp_xlsb
     */
    HAL_Delay(10);  // wait for forced mode measurement to complete (~9 ms typical)

    uint8_t rawData[6] = { 0 };
    if (I2C_Blocking_ReadRegister(&hi2c1, BME280_ADDR, BME280_REG_PRESS_MSB,
                                   rawData, sizeof(rawData), 100) != I2C_BLOCKING_OK)
    {
        /* Read failed — set an error flag and handle in the main loop */
    }
    /* rawData[3..5] now holds the raw temperature — apply BME280 compensation
     * formula from the datasheet to convert to Celsius                        */


    /* ── Wake up the MPU-6050 ────────────────────────────────────────────────
     *
     * MPU-6050 starts in sleep mode. Write 0x00 to PWR_MGMT_1 to wake it.
     * This is the mandatory first step before any reads will return real data.
     */
    uint8_t wakeUp = 0x00;
    I2C_Blocking_WriteRegister(&hi2c1, MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1,
                                &wakeUp, 1, 100);

    /* Verify identity */
    uint8_t whoAmI = 0;
    I2C_Blocking_ReadRegister(&hi2c1, MPU6050_ADDR, MPU6050_REG_WHO_AM_I,
                               &whoAmI, 1, 100);
    /* whoAmI should be 0x68 */


    /* ── MasterTransmit / MasterReceive: raw transaction ────────────────────
     *
     * Use when the device protocol does not follow the register-pointer model.
     * Example: some OLED controllers expect a command byte followed by data
     * bytes, all in one transaction, without a separate register address.
     *
     * Equivalent to calling WriteRegister but you manage the command byte
     * yourself in the buffer.
     */
    uint8_t cmd[] = { 0xAE };   // display off command (SSD1306 OLED example)
    I2C_Blocking_MasterTransmit(&hi2c1, I2C_ADDR(0x3C), cmd, sizeof(cmd), 100);


    /* ── Error handling example ──────────────────────────────────────────────
     *
     * Check every return value in production. NACK is the most actionable:
     * it tells you the device is absent or the address is wrong.
     * TIMEOUT usually means a bus lockup — SCL/SDA stuck low.
     */
    uint8_t reg = 0;
    I2C_Blocking_Status status = I2C_Blocking_ReadRegister(
        &hi2c1, BME280_ADDR, BME280_REG_ID, &reg, 1, 100);

    switch (status)
    {
        case I2C_BLOCKING_OK:
            break;
        case I2C_BLOCKING_NACK:
            /* Device not found — check address and wiring */
            break;
        case I2C_BLOCKING_TIMEOUT:
            /* Bus locked — check pull-up resistors, try I2C bus recovery */
            break;
        default:
            break;
    }


    while (1)
    {
        HAL_Delay(100);
    }
}
