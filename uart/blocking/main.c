/*
 * REFERENCE ONLY — not a standalone buildable project
 * To use:
 * 1. Generate CubeMX project for NUCLEO-L476RG with USART2 enabled
 * 2. Copy uart_blocking.h and uart_blocking.c into Core/Src and Core/Inc
 * 3. Call UART_Blocking_Init() or let CubeMX handle init and pass &huart2
 * 4. Build and flash from CubeIDE
 */


 //nucleo to pc: 115200 baud, 8N1, no flow control
 //normally uart logic will always have rx and tx for each device. So for two mcu's communicating each will have both tx and rx in its code.
#include "uart_blocking.h"

extern UART_HandleTypeDef huart2;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* ── TX examples ────────────────────────── */

    // Send a string and also error checking example. The function still gets called inside the if condition. If it successfully transmits the block will be skipped.
    if (UART_Blocking_TransmitString(&huart2, "UART Blocking Driver Init\r\n", 1000) != UART_BLOCKING_OK)
    {
        // Blink onboard LED (PA5) to signal TX failure
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(200);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }

    // Send a number
    UART_Blocking_TransmitNumber(&huart2, 42, 1000);

    // Send a raw buffer
    uint8_t txBuf[] = {0x01, 0x02, 0x03, 0x04};
    UART_Blocking_TransmitBuffer(&huart2, txBuf, sizeof(txBuf), 1000);





    /* ── RX examples ────────────────────────── */

    // Receive a single byte — error flag example
    // If RX fails, set a flag and check it later instead of blocking with a delay
    uint8_t rxError = 0;
    uint8_t byte;
    if (UART_Blocking_ReceiveByte(&huart2, &byte, 1000) != UART_BLOCKING_OK)
    {
        rxError = 1; // flag is set, handle it later in your main loop
    }

    // Receive fixed length buffer — error logging example
    // On failure, transmit an error message back to the PC terminal so you can see it
    uint8_t rxBuf[10];
    if (UART_Blocking_ReceiveBuffer(&huart2, rxBuf, sizeof(rxBuf), 1000) != UART_BLOCKING_OK)
    {
        UART_Blocking_TransmitString(&huart2, "[ERROR] ReceiveBuffer timed out\r\n", 1000);
    }

    // Receive a string until newline terminator
    uint8_t lineBuf[64];
    UART_Blocking_ReceiveUntil(&huart2, lineBuf, sizeof(lineBuf), '\n', 5000);

    /*Echo loop. This basically is just sending back whatever is receieved by the nucleo and echoed back to the PC's terminal 
    so we can view what the nucleo is receiving in real-time
     */
    uint8_t echoBuf[64];

    while (1)
    {
        // Wait for a line terminated by newline
        if (UART_Blocking_ReceiveUntil(&huart2, echoBuf, sizeof(echoBuf), '\r', 5000) == UART_BLOCKING_OK)
        {
            // Echo it back
            UART_Blocking_TransmitString(&huart2, (char *)echoBuf, 1000);
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        }
    }
}