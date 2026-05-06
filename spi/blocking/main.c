/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate CubeMX project for NUCLEO-L476RG with SPI1 enabled
 *    - Mode: Full-Duplex Master
 *    - NSS: Disable (we manage CS manually)
 *    - Prescaler: adjust to suit your device (e.g. /16 → ~5 MHz)
 *    - CPOL / CPHA: match your slave device's datasheet (most use Mode 0)
 * 2. Configure a GPIO output pin for CS (e.g. PA4 → "CS_Pin")
 * 3. Copy spi_blocking.h into Core/Inc, spi_blocking.c into Core/Src
 * 4. Build and flash from CubeIDE
 *
 * SPI pinout on NUCLEO-L476RG (Arduino header / SPI1):
 *   PA5 → SCK
 *   PA6 → MISO
 *   PA7 → MOSI
 *   PA4 → CS (manual GPIO, configured as output push-pull, default HIGH)
 */

#include "spi_blocking.h"

extern SPI_HandleTypeDef hspi1;

/* CS pin — configure PA4 as GPIO_OUTPUT in CubeMX, label it "CS_Pin" */
#define CS_PORT  GPIOA
#define CS_PIN   GPIO_PIN_4

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();

    /* Optional: override CubeMX defaults with explicit mode/prescaler */
    SPI_Blocking_Init(&hspi1, SPI_BAUDRATEPRESCALER_16,
                               SPI_POLARITY_LOW,
                               SPI_PHASE_1EDGE);   // Mode 0 — CPOL=0, CPHA=0

    /* Make sure CS starts deasserted (high) */
    SPI_Blocking_CS_Deselect(CS_PORT, CS_PIN);


    /* ── TX example: write a register on a slave device ─────────────────
     *
     * Most SPI devices expect:
     *   byte 0 → register address (with write bit, e.g. addr & 0x7F)
     *   byte 1 → value to write
     *
     * CS must bracket the entire transaction.
     */
    uint8_t writeCmd[] = { 0x20, 0xA7 };   // e.g. write 0xA7 to register 0x20

    SPI_Blocking_CS_Select(CS_PORT, CS_PIN);
    SPI_Blocking_Transmit(&hspi1, writeCmd, sizeof(writeCmd), 100);
    SPI_Blocking_CS_Deselect(CS_PORT, CS_PIN);


    /* ── RX example: read a register ────────────────────────────────────
     *
     * Most devices flag a read by setting the MSB of the address byte.
     * After clocking out the address the slave shifts data back on MISO
     * for every subsequent byte — use dummy 0xFF bytes to keep the clock
     * running while you capture the response.
     */
    uint8_t readAddr  = 0x0F | 0x80;  // register 0x0F with read bit set
    uint8_t whoAmI    = 0;

    SPI_Blocking_CS_Select(CS_PORT, CS_PIN);
    SPI_Blocking_TransmitByte(&hspi1, readAddr, 100);   // send address
    SPI_Blocking_ReceiveByte (&hspi1, &whoAmI,  100);   // clock in response
    SPI_Blocking_CS_Deselect(CS_PORT, CS_PIN);


    /* ── Full-duplex example: read multiple registers at once ────────────
     *
     * TransmitReceive sends and receives every byte simultaneously.
     * txBuf[0] = register address (read bit set), txBuf[1..n] = 0xFF dummies.
     * rxBuf[0] = garbage (clocked out while address was sent),
     * rxBuf[1..n] = actual register data.
     */
    uint8_t txBuf[3] = { 0x28 | 0x80, 0xFF, 0xFF };  // read 2 bytes starting at reg 0x28
    uint8_t rxBuf[3] = { 0 };

    SPI_Blocking_CS_Select(CS_PORT, CS_PIN);
    SPI_Blocking_TransmitReceive(&hspi1, txBuf, rxBuf, sizeof(txBuf), 100);
    SPI_Blocking_CS_Deselect(CS_PORT, CS_PIN);

    /* rxBuf[1] and rxBuf[2] now hold the two register values */
    uint8_t regLow  = rxBuf[1];
    uint8_t regHigh = rxBuf[2];


    /* ── Error handling example ──────────────────────────────────────────
     *
     * Always deselect CS even on failure so the bus is not left locked.
     */
    uint8_t statusReg = 0;
    uint8_t statusAddr = 0x27 | 0x80;

    SPI_Blocking_CS_Select(CS_PORT, CS_PIN);

    if (SPI_Blocking_TransmitByte(&hspi1, statusAddr, 100) != SPI_BLOCKING_OK ||
        SPI_Blocking_ReceiveByte (&hspi1, &statusReg, 100) != SPI_BLOCKING_OK)
    {
        SPI_Blocking_CS_Deselect(CS_PORT, CS_PIN);
        // Blink onboard LED (PA5) to signal error
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
    else
    {
        SPI_Blocking_CS_Deselect(CS_PORT, CS_PIN);
    }


    while (1)
    {
        /* Main loop — add periodic SPI reads here if needed */
        HAL_Delay(100);
    }
}
