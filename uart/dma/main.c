/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate CubeMX project for NUCLEO-L476RG with USART2 enabled
 * 2. In CubeMX DMA Settings tab: add DMA request for USART2_TX, direction
 *    Memory to Peripheral, mode Normal (or Circular for streaming)
 * 3. Copy uart_dma.h and uart_dma.c into Core/Src and Core/Inc
 * 4. Build and flash from CubeIDE
 *
 * Key difference from interrupt:
 * — DMA transfers the entire buffer directly from memory to UART hardware
 * — CPU is not involved at all during the transfer — zero interrupts per byte
 * — One interrupt fires at the end of the full transfer (normal mode)
 * — Two interrupts fire per loop in circular mode (half and full complete)
 * — Ideal for large or continuous transfers where interrupt overhead matters
 *
 * The driver (uart_dma.c) owns:
 * — txComplete flag
 * — HAL_UART_TxCpltCallback / HAL_UART_TxHalfCpltCallback
 * Main just calls the driver API and overrides the half/full callbacks
 * when using circular mode to refill buffer halves
 *
 * Nucleo to PC: 115200 baud, 8N1, no flow control
 */

#include "uart_dma.h"

extern UART_HandleTypeDef huart2;

/* File scope so both main and the circular callbacks can access it directly
 * extern is for cross-translation-unit references — not needed within one file */
static uint8_t streamBuf[10240];

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_DMA_Init();  /* CubeMX generates this — must be called before UART init */

    /* ── Init driver ────────────────────────────────────────────────────────
     * Configures peripheral — DMA streams are already set up by CubeMX
     * Must be called after MX_DMA_Init and MX_USART2_UART_Init
     */
    UART_DMA_Init(&huart2, 115200);

    /* ── TX examples — Normal Mode ──────────────────────────────────────────
     * DMA reads the buffer from memory and streams it to UART autonomously
     * CPU is free the entire time — returns immediately like interrupt mode
     * but with far less overhead for large transfers
     *
     * IMPORTANT: the buffer must stay valid until UART_DMA_IsTxComplete() == 1
     * DMA reads directly from the memory address — if the buffer is modified
     * or goes out of scope mid-transfer the outgoing data is corrupted
     */

    // Send a string — buffer is a string literal in flash, always valid
    UART_DMA_TransmitString("UART DMA Driver Init\r\n");

    // Wait for completion before reusing the TX path
    // Polling like this is only acceptable before the main loop
    // In the loop use UART_DMA_IsTxComplete() as a non-blocking check instead
    while (!UART_DMA_IsTxComplete()) {}

    // Send a raw buffer — must remain in scope until transfer completes
    // Here it is static so it is safe; avoid stack buffers that may go out
    // of scope before DMA finishes reading them
    static uint8_t txBuf[10240];
    for (uint32_t i = 0; i < sizeof(txBuf); i++)
    {
        txBuf[i] = (uint8_t)(i & 0xFF);
    }

    if (UART_DMA_TransmitBuffer(txBuf, sizeof(txBuf)) == UART_DMA_BUSY)
    {
        // Previous transfer still in progress — check UART_DMA_IsTxComplete()
        // before retrying rather than blocking here
    }

    /* ── TX example — Circular Mode ─────────────────────────────────────────
     * DMA streams the buffer in an endless loop — CPU never re-initiates
     * HalfCpltCallback fires at the midpoint — application refills first half
     * TxCpltCallback fires at the end — application refills second half
     * This is the double-buffer (ping-pong) pattern:
     *   DMA transmits half B → CPU writes fresh data into half A
     *   DMA transmits half A → CPU writes fresh data into half B
     *   ...repeat indefinitely until UART_DMA_StopCircular() is called
     *
     * Use case: continuous streams — audio, sensor feeds, data loggers
     * The CPU only wakes twice per full buffer cycle, not once per byte
     */
    // Pre-fill with initial data before starting
    for (uint32_t i = 0; i < sizeof(streamBuf); i++)
    {
        streamBuf[i] = (uint8_t)(i & 0xFF);
    }

    UART_DMA_StartCircular(streamBuf, sizeof(streamBuf)); //similiar to interrupt where you only call once to start the process and then the callbacks handle the rest

    /* ── Main loop ───────────────────────────────────────────────────────────
     * CPU is completely free while DMA handles transmission
     * The half/full callbacks (defined below) refill the buffer each cycle
     * LED toggles at full speed — no UART work happening here at all
     */
    uint32_t lastToggle = 0;

    while (1)
    {
        /* ── Non blocking LED toggle every 500ms ───────────────────────── */
        if (HAL_GetTick() - lastToggle >= 500)
        {
            lastToggle = HAL_GetTick();
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }
    }
}

/* ── Circular mode callbacks ─────────────────────────────────────────────────
 * Override the weak HAL callbacks here to refill buffer halves
 * Keep these short — they run in ISR context
 *
 * Half-complete: DMA just finished transmitting the first half (indices 0–5119)
 * and is now reading from the second half (5120–10239)
 * Refill the first half here with the next block of data
 *
 * Full-complete: DMA just finished the second half and is wrapping back to 0
 * Refill the second half here
 * Call UART_DMA_StopCircular() here when done streaming
 */

void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* Refill first half — DMA is currently transmitting second half */
        for (uint32_t i = 0; i < 5120; i++)
        {
            streamBuf[i] = /* next data byte */ 0xAA;
        }
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* Refill second half — DMA is wrapping back to transmit first half */
        for (uint32_t i = 5120; i < 10240; i++)
        {
            streamBuf[i] = /* next data byte */ 0xBB;
        }

        /* Stop streaming after desired number of cycles — or run forever */
        // UART_DMA_StopCircular();
    }
}
