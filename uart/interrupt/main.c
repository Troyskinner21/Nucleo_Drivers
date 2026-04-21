/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate CubeMX project for NUCLEO-L476RG with USART2 enabled
 * 2. Enable UART global interrupt in NVIC tab in CubeMX
 * 3. Copy uart_interrupt.h and uart_interrupt.c into Core/Src and Core/Inc
 * 4. Add ring_buffer.h and ring_buffer.c from buffers/
 * 5. Build and flash from CubeIDE
 *
 * Key difference from blocking:
 * — TX and RX happen in the background via interrupts
 * — CPU is free to do other work while data transfers
 * — ISR fires when TX complete or RX byte received
 * — Ring buffer holds incoming RX data until main loop reads it
 * — HAL_Delay() is gone — use HAL_GetTick() for non blocking periodic tasks
 *
 * The driver (uart_interrupt.c) owns:
 * — txComplete flag
 * — RX ring buffer and staging byte
 * — HAL_UART_TxCpltCallback / HAL_UART_RxCpltCallback
 * Main just calls the driver API — no HAL or ring buffer calls directly
 *
 * Nucleo to PC: 115200 baud, 8N1, no flow control
 */

#include "uart_interrupt.h"

extern UART_HandleTypeDef huart2;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* ── Init driver ────────────────────────────────────────────────────────
     * Configures peripheral, inits ring buffer, arms first RX interrupt
     * Must be called after MX_USART2_UART_Init so the handle is valid
     */
    UART_IT_Init(&huart2, 115200);

    /* ── TX examples ────────────────────────────────────────────────────────
     * All TX calls return immediately — the transfer happens in background
     * UART_IT_BUSY means the previous TX is still in flight; retry next loop tick
     */

    // Send a string — error checking example
    // Unlike blocking, there is no timeout — BUSY means try again later
    if (UART_IT_TransmitString("UART Interrupt Driver Init\r\n") == UART_IT_BUSY)
    {
        // Previous TX still in flight — blink LED to signal contention
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }

    // Send a number — uses internal static buffer so no caller buffer needed
    // Safe to call and move on; HAL holds the internal buffer until TX complete
    UART_IT_TransmitNumber(42);

    // Send a raw byte buffer
    // IMPORTANT: txBuf must remain in scope until UART_IT_IsTxComplete() == 1
    // Here it is local to main so it is safe; be careful with stack buffers in
    // functions that return before TX finishes
    uint8_t txBuf[] = {0x01, 0x02, 0x03, 0x04};
    while (UART_IT_TransmitBuffer(txBuf, sizeof(txBuf)) == UART_IT_BUSY)
    {
        // Spin until previous TX drains — only acceptable before the main loop
        // In the loop use a flag and retry on the next iteration instead
    }

    /* ── RX examples — checked from the main loop ───────────────────────────
     * RX arrives asynchronously into the ring buffer via the ISR
     * All receive calls are non-blocking — UART_IT_EMPTY means no data yet
     */

    // Receive a single byte — check and set an error flag, handle it later
    uint8_t rxError = 0;
    uint8_t byte    = 0;
    if (UART_IT_ReceiveByte(&byte) != UART_IT_OK)
    {
        rxError = 1; // no data available yet — handle downstream, do not block
    }

    // Receive a fixed number of bytes — read however many are currently available
    uint8_t  rxBuf[10];
    uint16_t rxRead = 0;
    if (UART_IT_ReceiveBuffer(rxBuf, sizeof(rxBuf), &rxRead) != UART_IT_OK)
    {
        UART_IT_TransmitString("[ERROR] ReceiveBuffer — no data available\r\n");
    }

    // Receive until newline — returns OK when '\n' found, EMPTY if not yet complete
    // Call repeatedly from the loop; accumulate into lineBuf across iterations
    uint8_t  lineBuf[64];
    uint16_t lineLen = 0;
    UART_IT_ReceiveUntil(lineBuf, sizeof(lineBuf), '\n', &lineLen);

    /* ── Echo loop ──────────────────────────────────────────────────────────
     * Non blocking — no HAL_Delay anywhere in this loop
     * ReceiveUntil polls the ring buffer each iteration; when a full line
     * arrives (terminator found) it transmits the line back
     * LED toggle runs concurrently — CPU never stalls waiting for UART
     *
     * This pattern replaces HAL_Delay for periodic tasks without an RTOS
     * Once you have many of these timestamp checks FreeRTOS tasks become
     * the cleaner solution — each task blocks on its own without affecting others
     */
    uint8_t  echoBuf[64];
    uint16_t echoLen    = 0;
    uint32_t lastToggle = 0;

    while (1)
    {
        /* ── Echo a full line back when newline received ────────────────── 
         * ReceiveUntil drains the ring buffer each call looking for '\n'
         * Returns UART_IT_OK only when the terminator is found
         * The buffer accumulates across calls — echoLen resets after each send
         */
        if (UART_IT_ReceiveUntil(echoBuf, sizeof(echoBuf), '\n', &echoLen) == UART_IT_OK)
        {
            // Full line ready — write it into the TX buffer
            // Returns UART_IT_BUSY if TX still in flight; retry next loop iteration
            // In production: queue the line or set a pending flag and retry
            UART_IT_TransmitBuffer((uint8_t *)echoBuf, echoLen);
            echoLen = 0;
        }

        /* ── Non blocking LED toggle every 500ms ───────────────────────── 
         * HAL_GetTick() returns ms since boot
         * Comparing against saved timestamp gives periodic behavior
         * without ever blocking the CPU
         */
        if (HAL_GetTick() - lastToggle >= 500)
        {
            lastToggle = HAL_GetTick();
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }
    }
}