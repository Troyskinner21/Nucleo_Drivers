/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate CubeMX project for NUCLEO-L476RG with SPI1 enabled
 *    - Mode: Full-Duplex Master
 *    - NSS: Disable (we manage CS manually)
 *    - Prescaler: adjust to suit your device (e.g. /16 → ~5 MHz)
 *    - CPOL / CPHA: match your slave device's datasheet (most use Mode 0)
 * 2. Enable SPI1 global interrupt in NVIC tab in CubeMX
 * 3. Configure a GPIO output pin for CS (e.g. PA4 → "CS_Pin")
 * 4. Copy spi_interrupt.h into Core/Inc, spi_interrupt.c into Core/Src
 * 5. Build and flash from CubeIDE
 *
 * Key difference from blocking:
 * — HAL_SPI_Transmit_IT / HAL_SPI_TransmitReceive_IT return immediately
 * — The SPI peripheral shifts bytes in hardware; an IRQ fires when done
 * — CPU is free between launching a transfer and the completion callback
 * — CS must still bracket each transaction but deselect happens in callback
 *   or by polling SPI_IT_IsComplete() in the main loop
 *
 * SPI pinout on NUCLEO-L476RG (Arduino header / SPI1):
 *   PA5 → SCK
 *   PA6 → MISO
 *   PA7 → MOSI
 *   PA4 → CS (manual GPIO, output push-pull, default HIGH)
 */

#include "spi_interrupt.h"

extern SPI_HandleTypeDef hspi1;

#define CS_PORT  GPIOA
#define CS_PIN   GPIO_PIN_4

/* ── Transfer complete callback ──────────────────────────────────────────────
 * Called from ISR context when a transfer finishes.
 * For TX-only transfers rxData is NULL and len is 0.
 * For TX+RX transfers rxData points to the rx buffer and len is the byte count.
 * Keep this short — no HAL_Delay, no printf, no heavy logic.
 */
static void OnTransferComplete(uint8_t *rxData, uint16_t len)
{
    /* Deselect CS here for lowest latency — transfer is finished */
    SPI_IT_CS_Deselect(CS_PORT, CS_PIN);

    if (rxData != NULL && len > 1)
    {
        /* rxData[0] is garbage (clocked out while address byte was sent)
         * rxData[1..len-1] hold the actual register data — process them here
         * or set a flag and let the main loop handle it                      */
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();

    SPI_IT_Init(&hspi1,
                SPI_BAUDRATEPRESCALER_16,
                SPI_POLARITY_LOW,
                SPI_PHASE_1EDGE,          // Mode 0
                OnTransferComplete);

    /* CS starts deasserted (high) */
    SPI_IT_CS_Deselect(CS_PORT, CS_PIN);


    /* ── TX example: write a register ───────────────────────────────────────
     *
     * txCmd must stay valid until the callback fires (ISR sets complete flag).
     * Here it is static so it lives for the entire program.
     */
    static uint8_t txCmd[] = { 0x20, 0xA7 };   // write 0xA7 to register 0x20

    SPI_IT_CS_Select(CS_PORT, CS_PIN);
    SPI_IT_Transmit(txCmd, sizeof(txCmd));
    /* ... CPU is free here while bytes shift out in hardware ... */


    /* ── Full-duplex RX example: read two registers ──────────────────────────
     *
     * txBuf[0] = register address with read bit set
     * txBuf[1..n] = 0xFF dummy bytes to keep the clock running
     * rxBuf[1..n] = response bytes (rxBuf[0] is garbage)
     *
     * Both buffers must be static (or remain valid until callback fires).
     */
    static uint8_t txBuf[3] = { 0x28 | 0x80, 0xFF, 0xFF };
    static uint8_t rxBuf[3] = { 0 };

    /* Wait for previous TX to finish before starting a new transfer */
    while (!SPI_IT_IsComplete()) { /* could do other work here */ }

    SPI_IT_CS_Select(CS_PORT, CS_PIN);
    SPI_IT_TransmitReceive(txBuf, rxBuf, sizeof(txBuf));
    /* ... CPU is free here while bytes shift in hardware ... */
    /* Callback fires when done, deselects CS, and can inspect rxBuf          */


    /* ── Error handling example ──────────────────────────────────────────────
     *
     * SPI_IT_BUSY means a transfer is still in flight — do not start another.
     * Always deselect CS if launch fails so the bus is not left locked.
     */
    static uint8_t statusTx[2] = { 0x27 | 0x80, 0xFF };
    static uint8_t statusRx[2] = { 0 };

    while (!SPI_IT_IsComplete()) { /* wait for bus to free up */ }

    SPI_IT_CS_Select(CS_PORT, CS_PIN);

    if (SPI_IT_TransmitReceive(statusTx, statusRx, sizeof(statusTx)) != SPI_IT_OK)
    {
        SPI_IT_CS_Deselect(CS_PORT, CS_PIN);  // release bus on failure
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
    /* On success the callback handles CS deselect and result processing */


    while (1)
    {
        /* Main loop — launch periodic transfers here, poll IsComplete() to
         * check if the previous one finished before starting the next.       */
        HAL_Delay(10);
    }
}
