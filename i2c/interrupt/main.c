/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate a CubeMX project for NUCLEO-L476RG with I2C1 enabled
 *    - Mode: I2C
 *    - Speed: Standard (100 kHz) or Fast (400 kHz)
 * 2. Enable I2C1 event interrupt and I2C1 error interrupt in NVIC tab
 *    Both are needed — event IRQ handles data, error IRQ handles NACK/bus error
 * 3. Copy i2c_interrupt.h into Core/Inc, i2c_interrupt.c into Core/Src
 * 4. Build and flash from CubeIDE
 *
 * Key difference from blocking:
 * — HAL_I2C_Mem_Write_IT / HAL_I2C_Mem_Read_IT return immediately
 * — The I2C peripheral clocks out bytes in hardware; an IRQ fires when done
 * — CPU is free between launching a transfer and the completion callback
 * — Buffers passed to the driver must remain valid until IsComplete() is 1
 *
 * I2C pinout on NUCLEO-L476RG (Arduino header / I2C1):
 *   PB8 → SCL
 *   PB9 → SDA
 *   Both lines need external pull-up resistors (typically 4.7 kΩ to 3.3 V)
 */

#include "i2c_interrupt.h"

extern I2C_HandleTypeDef hi2c1;

/* BME280 sensor — 7-bit address 0x76 (SDO pin tied to GND) */
#define SENSOR_ADDR  I2C_ADDR(0x76)

/* BME280 register addresses */
#define REG_CHIP_ID   0xD0
#define REG_CTRL_MEAS 0xF4
#define REG_PRESS_MSB 0xF7

/* rx buffer — must stay valid until the callback fires */
static uint8_t rxBuf[6];

/* tx buffer — must stay valid until IsComplete() returns 1 */
static uint8_t txBuf[1];

/* flag set inside the callback to let the main loop know new data arrived */
static volatile uint8_t dataReady = 0;

/* ── Transfer complete callback ──────────────────────────────────────────────
 * Called from ISR context when a transfer finishes or when an error occurs.
 * For writes: rxData is NULL, len is 0.
 * For reads:  rxData points to the filled buffer, len is the byte count.
 * Keep this short — no HAL_Delay, no printf, no heavy logic.
 */
static void OnTransferComplete(uint8_t *rxData, uint16_t len)
{
    if (rxData != NULL && len > 0)
    {
        /* Signal the main loop that new sensor bytes are ready to process */
        dataReady = 1;
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    I2C_IT_Init(&hi2c1, 400000, OnTransferComplete);

    /* ── Startup probe ───────────────────────────────────────────────────────
     * IsDeviceReady is blocking — intentional. Confirm the sensor is present
     * before launching any async transfers. Trap here if it never responds
     * (wrong address, missing pull-ups, or device not powered).
     */
    while (I2C_IT_IsDeviceReady(SENSOR_ADDR, 3) != I2C_IT_OK)
    {
        /* hang — check wiring and pull-up resistors */
    }

    /* ── Read chip ID (sanity check) ─────────────────────────────────────────
     * BME280 should return 0x60. This read is launched non-blocking; the main
     * loop spins on IsComplete() here for illustration — in real firmware you
     * would do useful work while the transfer runs in the background.
     */
    I2C_IT_ReadRegister(SENSOR_ADDR, REG_CHIP_ID, rxBuf, 1);

    while (!I2C_IT_IsComplete()) { /* CPU free — do other work here */ }

    /* rxBuf[0] == 0x60 for a genuine BME280 */
    uint8_t chipId = rxBuf[0];
    (void)chipId;

    /* ── Configure sensor ────────────────────────────────────────────────────
     * Write CTRL_MEAS: osrs_t=1x, osrs_p=1x, mode=normal (0x27)
     * txBuf must stay valid until IsComplete() returns 1.
     */
    txBuf[0] = 0x27;
    I2C_IT_WriteRegister(SENSOR_ADDR, REG_CTRL_MEAS, txBuf, 1);

    while (!I2C_IT_IsComplete()) { /* wait for write to finish */ }

    /* ── Main loop ───────────────────────────────────────────────────────────
     * Kick off a 6-byte read of the raw pressure + temperature registers.
     * The callback sets dataReady when the bytes land in rxBuf.
     * The CPU is free to do other work while the I2C peripheral runs.
     */
    while (1)
    {
        /* Launch the read — returns immediately */
        if (I2C_IT_ReadRegister(SENSOR_ADDR, REG_PRESS_MSB, rxBuf, 6) == I2C_IT_BUSY)
        {
            /* Previous transfer still running — try again next iteration */
            continue;
        }

        /* Do other work here while the I2C peripheral fills rxBuf in hardware */

        /* Wait for data before processing */
        while (!dataReady) {}
        dataReady = 0;

        /* rxBuf[0..2] = raw pressure bytes, rxBuf[3..5] = raw temperature bytes
         * Pass through the BME280 compensation formulas to get real values.  */

        HAL_Delay(100);
    }
}
