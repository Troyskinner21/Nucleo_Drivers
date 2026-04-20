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
 * Nucleo to PC: 115200 baud, 8N1, no flow control
 */

#include "uart_interrupt.h"
#include "ring_buffer.h"

extern UART_HandleTypeDef huart2;

/* ── TX complete flag ───────────────────────────────────────────────────────
 * volatile because it is written in the ISR and read in main context
 * without volatile the compiler may optimize it out and never see the update
 */
volatile uint8_t txComplete = 1;

/* ── RX ring buffer ─────────────────────────────────────────────────────────
 * ISR writes incoming bytes into the ring buffer
 * Main loop reads from it at its own pace
 * Decouples arrival rate from processing rate
 * If buffer fills up incoming bytes are dropped — handle overflow in production
 */
static RingBuffer_t rxRingBuf;
static uint8_t      rxStorage[128];

/* ── Single byte RX staging buffer ─────────────────────────────────────────
 * HAL_UART_Receive_IT receives one byte at a time into here
 * Callback pushes it into ring buffer then re-arms for next byte
 */
static uint8_t rxByte;

/* ── TX complete callback ───────────────────────────────────────────────────
 * Called by HAL when all TX bytes have been sent
 * Set flag so main knows it can transmit again
 * Keep ISR callbacks SHORT — no heavy processing in here
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        txComplete = 1;
    }
}

/* ── RX complete callback ───────────────────────────────────────────────────
 * Called by HAL every time one byte is received
 * Push byte into ring buffer then immediately re-arm for next byte
 * HAL_UART_Receive_IT only fires once — must re-arm here every time
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        RingBuffer_Write(&rxRingBuf, rxByte);

        // Re-arm for next byte
        HAL_UART_Receive_IT(&huart2, &rxByte, 1);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* ── Init ring buffer ───────────────────────────────────────────────── */
    RingBuffer_Init(&rxRingBuf, rxStorage, sizeof(rxStorage));

    /* ── Arm first RX interrupt ─────────────────────────────────────────── 
     * Arm once here — callback re-arms automatically on every received byte
     * From this point every received byte triggers HAL_UART_RxCpltCallback
     */
    HAL_UART_Receive_IT(&huart2, &rxByte, 1);

    /* ── TX example — interrupt driven ─────────────────────────────────────
     * HAL_UART_Transmit_IT returns immediately — TX happens in background
     * Guard with txComplete to avoid starting TX while previous is still active
     */
    uint8_t txBuf[] = "UART Interrupt Driver Init\r\n";

    if (txComplete)
    {
        txComplete = 0;
        if (HAL_UART_Transmit_IT(&huart2, txBuf, sizeof(txBuf) - 1) != HAL_OK)
        {
            // TX failed to start — HAL busy or error
            // Reset flag so we can try again rather than getting stuck
            txComplete = 1;
        }
    }

    /* ── Echo loop ──────────────────────────────────────────────────────────
     * Non blocking — no HAL_Delay anywhere in this loop
     * Ring buffer check and LED toggle both use timestamp comparison
     * CPU is always available — never sleeping waiting for something
     *
     * This pattern replaces HAL_Delay for periodic tasks without an RTOS
     * Once you have many of these timestamp checks FreeRTOS tasks become
     * the cleaner solution — each task blocks on its own without affecting others
     */
    uint8_t  echoChar   = 0;
    uint32_t lastToggle = 0;

    while (1)
    {
        /* ── Echo received bytes back ───────────────────────────────────── */
        if (RingBuffer_Read(&rxRingBuf, &echoChar) == RING_BUFFER_OK)
        {
            // Only transmit if previous TX is complete
            if (txComplete)
            {
                txComplete = 0;
                if (HAL_UART_Transmit_IT(&huart2, &echoChar, 1) != HAL_OK)
                {
                    txComplete = 1; // reset on failure
                }
            }
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