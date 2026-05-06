#include "spi_blocking.h"

/* ─────────────────────────────────────────
 * Init
 * ─────────────────────────────────────────
 * prescaler    : SPI_BAUDRATEPRESCALER_2 … SPI_BAUDRATEPRESCALER_256
 * clkPolarity  : SPI_POLARITY_LOW  (CPOL=0)  or SPI_POLARITY_HIGH (CPOL=1)
 * clkPhase     : SPI_PHASE_1EDGE   (CPHA=0)  or SPI_PHASE_2EDGE  (CPHA=1)
 *
 * CPOL=0, CPHA=0 → SPI Mode 0  (most common — works with many sensors)
 * CPOL=0, CPHA=1 → SPI Mode 1
 * CPOL=1, CPHA=0 → SPI Mode 2
 * CPOL=1, CPHA=1 → SPI Mode 3
 * ───────────────────────────────────────── */
SPI_Blocking_Status SPI_Blocking_Init(SPI_HandleTypeDef *hspi,
                                      uint32_t prescaler,
                                      uint32_t clkPolarity,
                                      uint32_t clkPhase)
{
    hspi->Init.Mode              = SPI_MODE_MASTER;
    hspi->Init.Direction         = SPI_DIRECTION_2LINES;
    hspi->Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi->Init.CLKPolarity       = clkPolarity;
    hspi->Init.CLKPhase          = clkPhase;
    hspi->Init.NSS               = SPI_NSS_SOFT;      // CS managed manually via GPIO
    hspi->Init.BaudRatePrescaler = prescaler;
    hspi->Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi->Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi->Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi->Init.CRCPolynomial     = 7;

    if (HAL_SPI_Init(hspi) != HAL_OK)
    {
        return SPI_BLOCKING_ERROR;
    }

    return SPI_BLOCKING_OK;
}

/* ─────────────────────────────────────────
 * Chip Select
 * ─────────────────────────────────────────
 * CS is active-low: pull low to select a slave, high to release.
 * Call CS_Select before a transaction and CS_Deselect after.
 * ───────────────────────────────────────── */
void SPI_Blocking_CS_Select(GPIO_TypeDef *csPort, uint16_t csPin)
{
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_RESET); // pull low → slave selected
}

void SPI_Blocking_CS_Deselect(GPIO_TypeDef *csPort, uint16_t csPin)
{
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_SET);   // pull high → slave released
}

/* ─────────────────────────────────────────
 * TX
 * ───────────────────────────────────────── */

SPI_Blocking_Status SPI_Blocking_TransmitByte(SPI_HandleTypeDef *hspi,
                                               uint8_t byte,
                                               uint32_t timeout)
{
    if (HAL_SPI_Transmit(hspi, &byte, 1, timeout) != HAL_OK)
    {
        return SPI_BLOCKING_TIMEOUT;
    }

    return SPI_BLOCKING_OK;
}

SPI_Blocking_Status SPI_Blocking_Transmit(SPI_HandleTypeDef *hspi,
                                           uint8_t *buf,
                                           uint16_t len,
                                           uint32_t timeout)
{
    if (buf == NULL || len == 0) return SPI_BLOCKING_ERROR;

    if (HAL_SPI_Transmit(hspi, buf, len, timeout) != HAL_OK)
    {
        return SPI_BLOCKING_TIMEOUT;
    }

    return SPI_BLOCKING_OK;
}

/* ─────────────────────────────────────────
 * RX
 * ─────────────────────────────────────────
 * In SPI, the master drives the clock for every byte — even when receiving.
 * HAL_SPI_Receive clocks out 0xFF dummy bytes automatically so the slave
 * can shift its data back on MISO. You don't need to think about this; just
 * pass the buffer you want filled.
 * ───────────────────────────────────────── */

SPI_Blocking_Status SPI_Blocking_ReceiveByte(SPI_HandleTypeDef *hspi,
                                              uint8_t *byte,
                                              uint32_t timeout)
{
    if (byte == NULL) return SPI_BLOCKING_ERROR;

    if (HAL_SPI_Receive(hspi, byte, 1, timeout) != HAL_OK)
    {
        return SPI_BLOCKING_TIMEOUT;
    }

    return SPI_BLOCKING_OK;
}

SPI_Blocking_Status SPI_Blocking_Receive(SPI_HandleTypeDef *hspi,
                                          uint8_t *buf,
                                          uint16_t len,
                                          uint32_t timeout)
{
    if (buf == NULL || len == 0) return SPI_BLOCKING_ERROR;

    if (HAL_SPI_Receive(hspi, buf, len, timeout) != HAL_OK)
    {
        return SPI_BLOCKING_TIMEOUT;
    }

    return SPI_BLOCKING_OK;
}

/* ─────────────────────────────────────────
 * Full-Duplex TX + RX
 * ─────────────────────────────────────────
 * Transmits txBuf and simultaneously captures whatever the slave sends
 * back into rxBuf. Both buffers must be the same length.
 * Useful for reading sensor registers: send command byte + dummy bytes,
 * get response bytes back at the same time.
 * ───────────────────────────────────────── */

SPI_Blocking_Status SPI_Blocking_TransmitReceive(SPI_HandleTypeDef *hspi,
                                                   uint8_t *txBuf,
                                                   uint8_t *rxBuf,
                                                   uint16_t len,
                                                   uint32_t timeout)
{
    if (txBuf == NULL || rxBuf == NULL || len == 0) return SPI_BLOCKING_ERROR;

    if (HAL_SPI_TransmitReceive(hspi, txBuf, rxBuf, len, timeout) != HAL_OK)
    {
        return SPI_BLOCKING_TIMEOUT;
    }

    return SPI_BLOCKING_OK;
}
