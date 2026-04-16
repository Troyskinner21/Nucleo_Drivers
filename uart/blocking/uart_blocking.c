#include "uart_blocking.h"
#include <stdio.h>

/* ─────────────────────────────────────────
 * Init
 * ───────────────────────────────────────── */
UART_Blocking_Status UART_Blocking_Init(UART_HandleTypeDef *huart, uint32_t baudrate)
{
    huart->Init.BaudRate     = baudrate;
    huart->Init.WordLength   = UART_WORDLENGTH_8B;
    huart->Init.StopBits     = UART_STOPBITS_1;
    huart->Init.Parity       = UART_PARITY_NONE;
    huart->Init.Mode         = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(huart) != HAL_OK)
    {
        return UART_BLOCKING_ERROR;
    }

    return UART_BLOCKING_OK;
}

/* ─────────────────────────────────────────
 * TX
 * ───────────────────────────────────────── */

UART_Blocking_Status UART_Blocking_TransmitString(UART_HandleTypeDef *huart,
                                                   const char *str,
                                                   uint32_t timeout)
{
    if (str == NULL) return UART_BLOCKING_ERROR;

    uint16_t len = (uint16_t)strlen(str);

    if (HAL_UART_Transmit(huart, (uint8_t *)str, len, timeout) != HAL_OK)
    {
        return UART_BLOCKING_TIMEOUT;
    }

    return UART_BLOCKING_OK;
}

UART_Blocking_Status UART_Blocking_TransmitBuffer(UART_HandleTypeDef *huart,
                                                   uint8_t *buf,
                                                   uint16_t len,
                                                   uint32_t timeout)
{
    if (buf == NULL || len == 0) return UART_BLOCKING_ERROR;

    if (HAL_UART_Transmit(huart, buf, len, timeout) != HAL_OK)
    {
        return UART_BLOCKING_TIMEOUT;
    }

    return UART_BLOCKING_OK;
}

UART_Blocking_Status UART_Blocking_TransmitNumber(UART_HandleTypeDef *huart,
                                                   int32_t number,
                                                   uint32_t timeout)
{
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%ld\r\n", number);

    if (len <= 0) return UART_BLOCKING_ERROR;

    if (HAL_UART_Transmit(huart, (uint8_t *)buf, (uint16_t)len, timeout) != HAL_OK)
    {
        return UART_BLOCKING_TIMEOUT;
    }

    return UART_BLOCKING_OK;
}

/* ─────────────────────────────────────────
 * RX
 * ───────────────────────────────────────── */

UART_Blocking_Status UART_Blocking_ReceiveByte(UART_HandleTypeDef *huart,
                                                uint8_t *byte,
                                                uint32_t timeout)
{
    if (byte == NULL) return UART_BLOCKING_ERROR;

    if (HAL_UART_Receive(huart, byte, 1, timeout) != HAL_OK)
    {
        return UART_BLOCKING_TIMEOUT;
    }

    return UART_BLOCKING_OK;
}

UART_Blocking_Status UART_Blocking_ReceiveBuffer(UART_HandleTypeDef *huart,
                                                  uint8_t *buf,
                                                  uint16_t len,
                                                  uint32_t timeout)
{
    if (buf == NULL || len == 0) return UART_BLOCKING_ERROR;

    if (HAL_UART_Receive(huart, buf, len, timeout) != HAL_OK)
    {
        return UART_BLOCKING_TIMEOUT;
    }

    return UART_BLOCKING_OK;
}

/* Receive bytes until terminator character is found or buffer is full */
UART_Blocking_Status UART_Blocking_ReceiveUntil(UART_HandleTypeDef *huart,
                                                  uint8_t *buf,
                                                  uint16_t maxLen,
                                                  uint8_t terminator,
                                                  uint32_t timeout)
{
    if (buf == NULL || maxLen == 0) return UART_BLOCKING_ERROR;

    uint16_t index = 0;
    uint8_t  byte  = 0;

    while (index < maxLen - 1)
    {
        if (HAL_UART_Receive(huart, &byte, 1, timeout) != HAL_OK)
        {
            return UART_BLOCKING_TIMEOUT;
        }

        buf[index++] = byte;

        if (byte == terminator)
        {
            break;
        }
    }

    buf[index] = '\0'; // null terminate

    return UART_BLOCKING_OK;
}