#ifndef UART_DMA_H
#define UART_DMA_H

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <string.h>

/* Status codes */
typedef enum
{
    UART_DMA_OK    = 0,
    UART_DMA_ERROR = 1,
    UART_DMA_BUSY  = 2   /* DMA transfer still in progress — try again later */
} UART_DMA_Status;

/*
 * Init — configure peripheral
 * Call once before any TX functions
 * DMA streams must already be configured in CubeMX before calling this
 */
UART_DMA_Status UART_DMA_Init(UART_HandleTypeDef *huart, uint32_t baudrate);

/* ── TX — Normal Mode ────────────────────────────────────────────────────────
 * DMA transfers the entire buffer from memory to UART in the background
 * CPU is completely free during the transfer — no interrupts per byte
 * TxCpltCallback fires once when the full transfer is done
 *
 * IMPORTANT: buffer must remain valid until UART_DMA_IsTxComplete() returns 1
 *            DMA reads directly from memory — modifying the buffer mid-transfer
 *            corrupts the outgoing data
 */
UART_DMA_Status UART_DMA_TransmitString(const char *str);
UART_DMA_Status UART_DMA_TransmitBuffer(const uint8_t *buf, uint16_t len);

/* Returns 1 if DMA TX is idle and safe to start a new transfer, 0 if busy */
uint8_t UART_DMA_IsTxComplete(void);

/* ── TX — Circular Mode ──────────────────────────────────────────────────────
 * DMA continuously streams the buffer in a loop without CPU involvement
 * HalfCpltCallback fires when DMA reaches the midpoint — refill first half
 * TxCpltCallback fires when DMA reaches the end — refill second half
 * Call UART_DMA_StopCircular() to stop the stream
 *
 * Use case: continuous data streaming (audio, sensor feeds, data loggers)
 * The buffer acts as a double buffer — one half plays while the other refills
 */
UART_DMA_Status UART_DMA_StartCircular(uint8_t *buf, uint16_t len);
UART_DMA_Status UART_DMA_StopCircular(void);

#endif /* UART_DMA_H */
