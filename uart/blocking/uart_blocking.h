#ifndef UART_BLOCKING_H
#define UART_BLOCKING_H

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <string.h>

/* Status codes */
typedef enum
{
    UART_BLOCKING_OK      = 0,
    UART_BLOCKING_ERROR   = 1,
    UART_BLOCKING_TIMEOUT = 2
} UART_Blocking_Status;

/* Init */
UART_Blocking_Status UART_Blocking_Init(UART_HandleTypeDef *huart, uint32_t baudrate);

/* TX */
UART_Blocking_Status UART_Blocking_TransmitString(UART_HandleTypeDef *huart, const char *str, uint32_t timeout);
UART_Blocking_Status UART_Blocking_TransmitBuffer(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t len, uint32_t timeout);
UART_Blocking_Status UART_Blocking_TransmitNumber(UART_HandleTypeDef *huart, int32_t number, uint32_t timeout);

/* RX */
UART_Blocking_Status UART_Blocking_ReceiveByte(UART_HandleTypeDef *huart, uint8_t *byte, uint32_t timeout);
UART_Blocking_Status UART_Blocking_ReceiveBuffer(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t len, uint32_t timeout);
UART_Blocking_Status UART_Blocking_ReceiveUntil(UART_HandleTypeDef *huart, uint8_t *buf, uint16_t maxLen, uint8_t terminator, uint32_t timeout);

#endif /* UART_BLOCKING_H */