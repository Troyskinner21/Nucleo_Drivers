#include "spi_interrupt.h"

/* ── Internal state ──────────────────────────────────────────────────────────
 * Stored here so the HAL callback can access them without extra parameters.
 * volatile prevents the compiler from caching values that the ISR can change.
 */
static SPI_HandleTypeDef       *s_hspi     = NULL;
static SPI_IT_TransferCallback  s_callback = NULL;

/* Transfer-complete flag — 1 means bus is idle, 0 means transfer in flight.
 * Set to 0 before each transfer, set back to 1 inside HAL_SPI_TxRxCpltCallback. */
static volatile uint8_t s_complete = 1;

/* Pointer to the active RX buffer — passed to the user callback on completion */
static uint8_t  *s_rxBuf = NULL;
static uint16_t  s_rxLen = 0;

/* ─────────────────────────────────────────
 * Init
 * ───────────────────────────────────────── */
SPI_IT_Status SPI_IT_Init(SPI_HandleTypeDef *hspi,
                           uint32_t prescaler,
                           uint32_t clkPolarity,
                           uint32_t clkPhase,
                           SPI_IT_TransferCallback callback)
{
    if (hspi == NULL) return SPI_IT_ERROR;

    s_hspi     = hspi;
    s_callback = callback;
    s_complete = 1;

    hspi->Init.Mode              = SPI_MODE_MASTER;
    hspi->Init.Direction         = SPI_DIRECTION_2LINES;
    hspi->Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi->Init.CLKPolarity       = clkPolarity;
    hspi->Init.CLKPhase          = clkPhase;
    hspi->Init.NSS               = SPI_NSS_SOFT;
    hspi->Init.BaudRatePrescaler = prescaler;
    hspi->Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi->Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi->Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi->Init.CRCPolynomial     = 7;

    if (HAL_SPI_Init(hspi) != HAL_OK)
    {
        return SPI_IT_ERROR;
    }

    return SPI_IT_OK;
}

/* ─────────────────────────────────────────
 * HAL callbacks
 * ─────────────────────────────────────────
 * HAL calls these from inside the SPI IRQ handler automatically.
 * Keep them short — no blocking code, no heavy processing.
 * ───────────────────────────────────────── */

/* Called when a TX-only transfer completes */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == s_hspi->Instance)
    {
        s_complete = 1;

        if (s_callback != NULL)
        {
            s_callback(NULL, 0);  // TX only — no RX data
        }
    }
}

/* Called when a full-duplex TX+RX transfer completes */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == s_hspi->Instance)
    {
        s_complete = 1;

        if (s_callback != NULL)
        {
            s_callback(s_rxBuf, s_rxLen);
        }
    }
}

/* ─────────────────────────────────────────
 * Status
 * ───────────────────────────────────────── */
uint8_t SPI_IT_IsComplete(void)
{
    return s_complete;
}

/* ─────────────────────────────────────────
 * Chip Select
 * ─────────────────────────────────────────
 * CS is active-low. Select before launching a transfer.
 * Deselect inside the callback (ISR context) or after polling IsComplete().
 *
 * Deselecting inside the callback is lower latency but requires the callback
 * to know which port/pin to release — pass them in via a static or a struct.
 * Deselecting after polling in the main loop is simpler and fine for most uses.
 * ───────────────────────────────────────── */
void SPI_IT_CS_Select(GPIO_TypeDef *csPort, uint16_t csPin)
{
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_RESET);
}

void SPI_IT_CS_Deselect(GPIO_TypeDef *csPort, uint16_t csPin)
{
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_SET);
}

/* ─────────────────────────────────────────
 * TX — non-blocking
 * ─────────────────────────────────────────
 * HAL_SPI_Transmit_IT returns immediately.
 * The ISR fires when all bytes have shifted out and calls TxCpltCallback.
 * buf must remain valid until IsComplete() returns 1.
 * ───────────────────────────────────────── */
SPI_IT_Status SPI_IT_Transmit(uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) return SPI_IT_ERROR;
    if (!s_complete)             return SPI_IT_BUSY;

    s_complete = 0;

    if (HAL_SPI_Transmit_IT(s_hspi, buf, len) != HAL_OK)
    {
        s_complete = 1;
        return SPI_IT_ERROR;
    }

    return SPI_IT_OK;
}

/* ─────────────────────────────────────────
 * Full-Duplex TX + RX — non-blocking
 * ─────────────────────────────────────────
 * HAL_SPI_TransmitReceive_IT returns immediately.
 * The ISR fires when all bytes are done and calls TxRxCpltCallback.
 * Both buffers must remain valid until IsComplete() returns 1.
 *
 * Pattern for a register read:
 *   txBuf[0] = reg address | 0x80  (read bit)
 *   txBuf[1..n] = 0xFF             (dummies to clock in response)
 *   rxBuf[1..n] = response data    (rxBuf[0] is garbage, ignore it)
 * ───────────────────────────────────────── */
SPI_IT_Status SPI_IT_TransmitReceive(uint8_t *txBuf, uint8_t *rxBuf, uint16_t len)
{
    if (txBuf == NULL || rxBuf == NULL || len == 0) return SPI_IT_ERROR;
    if (!s_complete)                                return SPI_IT_BUSY;

    s_rxBuf    = rxBuf;
    s_rxLen    = len;
    s_complete = 0;

    if (HAL_SPI_TransmitReceive_IT(s_hspi, txBuf, rxBuf, len) != HAL_OK)
    {
        s_complete = 1;
        return SPI_IT_ERROR;
    }

    return SPI_IT_OK;
}
