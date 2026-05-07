#ifndef SPI_INTERRUPT_H
#define SPI_INTERRUPT_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/* Status codes */
typedef enum
{
    SPI_IT_OK      = 0,
    SPI_IT_ERROR   = 1,
    SPI_IT_BUSY    = 2   /* Transfer still in flight — try again later */
} SPI_IT_Status;

/*
 * Callback type — user provides this function and it is called when a
 * transfer completes. Receives a pointer to the rx data and byte count.
 * Called from ISR context — keep it short.
 */
typedef void (*SPI_IT_TransferCallback)(uint8_t *rxData, uint16_t len);

/*
 * Init — configure peripheral, store handle, register user callback
 * prescaler   : SPI_BAUDRATEPRESCALER_2 … SPI_BAUDRATEPRESCALER_256
 * clkPolarity : SPI_POLARITY_LOW (CPOL=0) or SPI_POLARITY_HIGH (CPOL=1)
 * clkPhase    : SPI_PHASE_1EDGE (CPHA=0) or SPI_PHASE_2EDGE  (CPHA=1)
 * callback    : called on TxRx complete, pass NULL if not needed
 */
SPI_IT_Status SPI_IT_Init(SPI_HandleTypeDef *hspi,
                           uint32_t prescaler,
                           uint32_t clkPolarity,
                           uint32_t clkPhase,
                           SPI_IT_TransferCallback callback);

/* Returns 1 if no transfer is in flight and the bus is free, 0 if busy */
uint8_t SPI_IT_IsComplete(void);

/*
 * CS helpers — active-low, same as blocking driver
 * Call CS_Select before launching a transfer, CS_Deselect inside the callback
 * after the transfer completes (or poll IsComplete() then deselect in main loop)
 */
void SPI_IT_CS_Select  (GPIO_TypeDef *csPort, uint16_t csPin);
void SPI_IT_CS_Deselect(GPIO_TypeDef *csPort, uint16_t csPin);

/*
 * TX only — non-blocking
 * HAL holds buf pointer until transfer completes — buf must stay valid
 * Returns SPI_IT_BUSY if a previous transfer is still in flight
 */
SPI_IT_Status SPI_IT_Transmit(uint8_t *buf, uint16_t len);

/*
 * Full-duplex TX + RX — non-blocking
 * Sends txBuf and simultaneously fills rxBuf — both must be len bytes
 * Both buffers must remain valid until IsComplete() returns 1
 * This is the primary function for register reads (send address + dummies,
 * get response in rxBuf)
 */
SPI_IT_Status SPI_IT_TransmitReceive(uint8_t *txBuf, uint8_t *rxBuf, uint16_t len);

#endif /* SPI_INTERRUPT_H */
