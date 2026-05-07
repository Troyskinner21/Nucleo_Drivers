/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate CubeMX project for NUCLEO-L476RG with SPI1 enabled
 *    - Mode: Full-Duplex Master, NSS: Disable
 * 2. In CubeMX SPI1 → DMA Settings tab:
 *    - Add SPI1_TX: Memory to Peripheral, Normal mode, byte increment
 *    - Add SPI1_RX: Peripheral to Memory, Normal mode, byte increment
 * 3. Enable DMA1 global interrupt in NVIC tab (CubeMX does this automatically)
 * 4. Configure PA4 as GPIO_OUTPUT for CS ("CS_Pin")
 * 5. Copy spi_dma.h into Core/Inc, spi_dma.c into Core/Src
 * 6. Build and flash from CubeIDE
 *
 * Key difference from interrupt mode:
 * — Interrupt mode: one CPU interrupt per byte transferred
 * — DMA mode: one CPU interrupt for the entire transfer, regardless of size
 * — DMA is the right choice for large payloads: display framebuffers,
 *   flash pages (256 bytes), bulk sensor reads
 * — For tiny transfers (1-4 bytes) the DMA setup overhead makes blocking
 *   or interrupt mode faster in practice
 *
 * Buffer rules (stricter than interrupt mode):
 * — Buffers must be in normal SRAM — DMA cannot access CCM on some STM32 parts
 * — Buffers must remain valid until IsComplete() returns 1
 * — Declare them static or at file scope — never on the stack of a function
 *   that might return before the DMA transfer finishes
 *
 * SPI pinout on NUCLEO-L476RG (Arduino header / SPI1):
 *   PA5 → SCK
 *   PA6 → MISO
 *   PA7 → MOSI
 *   PA4 → CS (manual GPIO, output push-pull, default HIGH)
 */

#include "spi_dma.h"

extern SPI_HandleTypeDef hspi1;

#define CS_PORT  GPIOA
#define CS_PIN   GPIO_PIN_4

/* ── Transfer complete callback ──────────────────────────────────────────────
 * Called from the DMA IRQ handler when the full transfer is done.
 * For TX-only: rxData is NULL, len is 0.
 * For TX+RX:  rxData[1..len-1] holds register response data (rxData[0] garbage).
 * Keep short — no blocking calls, no HAL_Delay.
 */
static void OnTransferComplete(uint8_t *rxData, uint16_t len)
{
    SPI_DMA_CS_Deselect(CS_PORT, CS_PIN);

    if (rxData == NULL)
    {
        /* TX-only DMA complete — e.g. full framebuffer has been sent */
    }
    else
    {
        /* TX+RX DMA complete — rxData[1..len-1] has register values */
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();      /* Must be called before SPI init — CubeMX generates this */
    MX_SPI1_Init();

    SPI_DMA_Init(&hspi1,
                 SPI_BAUDRATEPRESCALER_16,
                 SPI_POLARITY_LOW,
                 SPI_PHASE_1EDGE,           // Mode 0
                 OnTransferComplete);

    SPI_DMA_CS_Deselect(CS_PORT, CS_PIN);  // CS starts high (deasserted)


    /* ── TX example: write a large block to a slave ──────────────────────────
     *
     * Classic use case: sending a 128x64 display framebuffer (1024 bytes).
     * With blocking that ties up the CPU for the full transfer duration.
     * With DMA, one function call kicks it off and the CPU is free immediately.
     *
     * static — must not be on the stack of a function that can return early
     */
    static uint8_t framebuffer[1024];

    /* Fill with some pattern */
    for (uint16_t i = 0; i < sizeof(framebuffer); i++)
    {
        framebuffer[i] = (uint8_t)(i & 0xFF);
    }

    SPI_DMA_CS_Select(CS_PORT, CS_PIN);
    SPI_DMA_Transmit(framebuffer, sizeof(framebuffer));
    /* CPU is free here — DMA is streaming all 1024 bytes with zero CPU cycles */
    /* Callback fires once when done, deselects CS                             */


    /* ── Full-duplex example: read a burst of registers ─────────────────────
     *
     * txBuf[0]    = register address | 0x80  (read bit set)
     * txBuf[1..n] = 0xFF dummy bytes         (keep clock running)
     * rxBuf[0]    = garbage                  (ignore)
     * rxBuf[1..n] = register data            (your result)
     *
     * Both buffers static — DMA accesses them directly from memory.
     */
    static uint8_t txBuf[7] = { 0x28 | 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static uint8_t rxBuf[7] = { 0 };

    /* Busy-waiting here for simplicity — in production check this flag
     * in the main loop instead of spinning. With FreeRTOS, replace with
     * a task notification: the callback calls xTaskNotifyFromISR() and
     * this line becomes ulTaskNotifyTake(), freeing the CPU entirely.   */
    while (!SPI_DMA_IsComplete()) {}

    SPI_DMA_CS_Select(CS_PORT, CS_PIN);
    SPI_DMA_TransmitReceive(txBuf, rxBuf, sizeof(txBuf));
    /* DMA streams all 7 bytes out and simultaneously captures 7 bytes in
     * CPU is free — callback fires once at the end with rxBuf[1..6] filled  */


    /* ── Error handling example ──────────────────────────────────────────────
     *
     * Always deselect CS if launch fails so the bus is not left locked.
     */
    static uint8_t statusTx[2] = { 0x27 | 0x80, 0xFF };
    static uint8_t statusRx[2] = { 0 };

    /* Busy-waiting here for simplicity — see comment above */
    while (!SPI_DMA_IsComplete()) {}

    SPI_DMA_CS_Select(CS_PORT, CS_PIN);

    if (SPI_DMA_TransmitReceive(statusTx, statusRx, sizeof(statusTx)) != SPI_DMA_OK)
    {
        SPI_DMA_CS_Deselect(CS_PORT, CS_PIN);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }


    while (1)
    {
        /* Main loop — CPU is free to run other logic while DMA handles SPI.
         * Poll IsComplete() before launching the next transfer.              */
        HAL_Delay(10);
    }
}
