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

    // Send a string
    UART_Blocking_TransmitString(&huart2, "UART Blocking Driver Init\r\n", 1000);

    // Send a number
    UART_Blocking_TransmitNumber(&huart2, 42, 1000);

    // Send a raw buffer
    uint8_t txBuf[] = {0x01, 0x02, 0x03, 0x04};
    UART_Blocking_TransmitBuffer(&huart2, txBuf, sizeof(txBuf), 1000);





    /* ── RX examples ────────────────────────── */

    // Receive a single byte
    uint8_t byte;
    UART_Blocking_ReceiveByte(&huart2, &byte, 1000);

    // Receive fixed length buffer
    uint8_t rxBuf[10];
    UART_Blocking_ReceiveBuffer(&huart2, rxBuf, sizeof(rxBuf), 1000);

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
        if (UART_Blocking_ReceiveUntil(&huart2, echoBuf, sizeof(echoBuf), '\n', 5000) == UART_BLOCKING_OK)
        {
            // Echo it back
            UART_Blocking_TransmitString(&huart2, (char *)echoBuf, 1000);
        }
    }
}