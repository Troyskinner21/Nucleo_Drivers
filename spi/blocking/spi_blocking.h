#ifndef SPI_BLOCKING_H
#define SPI_BLOCKING_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/* Status codes */
typedef enum
{
    SPI_BLOCKING_OK      = 0,
    SPI_BLOCKING_ERROR   = 1,
    SPI_BLOCKING_TIMEOUT = 2
} SPI_Blocking_Status;

/* Init */
SPI_Blocking_Status SPI_Blocking_Init(SPI_HandleTypeDef *hspi,
                                      uint32_t prescaler,
                                      uint32_t clkPolarity,
                                      uint32_t clkPhase);

/* Chip select helpers — CS is active-low */
void SPI_Blocking_CS_Select  (GPIO_TypeDef *csPort, uint16_t csPin);
void SPI_Blocking_CS_Deselect(GPIO_TypeDef *csPort, uint16_t csPin);

/* TX */
SPI_Blocking_Status SPI_Blocking_TransmitByte  (SPI_HandleTypeDef *hspi, uint8_t  byte,             uint32_t timeout);
SPI_Blocking_Status SPI_Blocking_Transmit      (SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len, uint32_t timeout);

/* RX */
SPI_Blocking_Status SPI_Blocking_ReceiveByte   (SPI_HandleTypeDef *hspi, uint8_t *byte,             uint32_t timeout);
SPI_Blocking_Status SPI_Blocking_Receive       (SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len, uint32_t timeout);

/* Full-duplex TX + RX simultaneously */
SPI_Blocking_Status SPI_Blocking_TransmitReceive(SPI_HandleTypeDef *hspi,
                                                  uint8_t *txBuf,
                                                  uint8_t *rxBuf,
                                                  uint16_t len,
                                                  uint32_t timeout);

#endif /* SPI_BLOCKING_H */
