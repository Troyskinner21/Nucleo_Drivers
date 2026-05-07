#ifndef SPI_DMA_H
#define SPI_DMA_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/* Status codes */
typedef enum
{
    SPI_DMA_OK      = 0,
    SPI_DMA_ERROR   = 1,
    SPI_DMA_BUSY    = 2   /* DMA transfer still in flight — try again later */
} SPI_DMA_Status;

/*
 * Callback type — called once when a DMA transfer completes.
 * For TX-only: rxData is NULL, len is 0.
 * For TX+RX:   rxData points to the filled rx buffer, len is byte count.
 * Called from ISR context — keep it short, no blocking code.
 */
typedef void (*SPI_DMA_TransferCallback)(uint8_t *rxData, uint16_t len);

/*
 * Init — configure peripheral, store handle, register user callback.
 * DMA channels for SPI TX and RX must already be configured in CubeMX
 * before calling this (DMA Settings tab on the SPI peripheral).
 *
 * prescaler   : SPI_BAUDRATEPRESCALER_2 … SPI_BAUDRATEPRESCALER_256
 * clkPolarity : SPI_POLARITY_LOW (CPOL=0) or SPI_POLARITY_HIGH (CPOL=1)
 * clkPhase    : SPI_PHASE_1EDGE (CPHA=0) or SPI_PHASE_2EDGE  (CPHA=1)
 * callback    : called on transfer complete, pass NULL if not needed
 */
SPI_DMA_Status SPI_DMA_Init(SPI_HandleTypeDef *hspi,
                             uint32_t prescaler,
                             uint32_t clkPolarity,
                             uint32_t clkPhase,
                             SPI_DMA_TransferCallback callback);

/* Returns 1 if no transfer is in flight and the bus is free, 0 if busy */
uint8_t SPI_DMA_IsComplete(void);

/* CS helpers — active-low, same pattern as blocking and interrupt drivers */
void SPI_DMA_CS_Select  (GPIO_TypeDef *csPort, uint16_t csPin);
void SPI_DMA_CS_Deselect(GPIO_TypeDef *csPort, uint16_t csPin);

/*
 * TX only — non-blocking, DMA moves data from memory to SPI with zero CPU involvement.
 * buf must remain valid until IsComplete() returns 1.
 * Returns SPI_DMA_BUSY if a previous transfer is still in flight.
 * Best used for writing large blocks to a slave (e.g. display framebuffer, flash page).
 */
SPI_DMA_Status SPI_DMA_Transmit(uint8_t *buf, uint16_t len);

/*
 * Full-duplex TX + RX — non-blocking.
 * DMA simultaneously streams txBuf to the slave and fills rxBuf from MISO.
 * Both buffers must be len bytes and remain valid until IsComplete() returns 1.
 * This is the primary function for register reads at high throughput.
 *
 * IMPORTANT: buffers must be in normal RAM (SRAM). DMA cannot access
 * the core-coupled memory (CCM) region on some STM32 parts.
 */
SPI_DMA_Status SPI_DMA_TransmitReceive(uint8_t *txBuf, uint8_t *rxBuf, uint16_t len);

#endif /* SPI_DMA_H */
