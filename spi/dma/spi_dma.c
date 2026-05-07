#include "spi_dma.h"

/* ── Internal state ──────────────────────────────────────────────────────────
 * Stored here so HAL callbacks can reach driver state without extra parameters.
 * volatile prevents the compiler from caching values that the ISR can change.
 */
static SPI_HandleTypeDef      *s_hspi     = NULL;
static SPI_DMA_TransferCallback s_callback = NULL;

/* Transfer-complete flag — 1 means bus is idle, 0 means DMA is running.
 * Cleared before each transfer, set back to 1 inside the HAL callback.      */
static volatile uint8_t s_complete = 1;

/* Pointer and length of the active RX buffer — passed to the user callback. */
static uint8_t  *s_rxBuf = NULL;
static uint16_t  s_rxLen = 0;

/* ─────────────────────────────────────────
 * Init
 * ─────────────────────────────────────────
 * CubeMX must have DMA configured for SPI1_TX and SPI1_RX before calling this.
 * In CubeMX: SPI peripheral → DMA Settings → Add → select SPIx_TX and SPIx_RX
 * Direction: Memory to Peripheral (TX), Peripheral to Memory (RX)
 * Mode: Normal for one-shot transfers
 * ───────────────────────────────────────── */
SPI_DMA_Status SPI_DMA_Init(SPI_HandleTypeDef *hspi,
                              uint32_t prescaler,
                              uint32_t clkPolarity,
                              uint32_t clkPhase,
                              SPI_DMA_TransferCallback callback)
{
    if (hspi == NULL) return SPI_DMA_ERROR;

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
        return SPI_DMA_ERROR;
    }

    return SPI_DMA_OK;
}

/* ─────────────────────────────────────────
 * HAL callbacks
 * ─────────────────────────────────────────
 * HAL calls these from inside the DMA IRQ handler automatically.
 * The DMA controller raises the interrupt — not the SPI peripheral.
 * The key difference vs interrupt mode: only ONE interrupt fires for the
 * entire transfer regardless of how many bytes were moved. In interrupt mode
 * one IRQ fires per byte. For large buffers (e.g. 4096-byte flash page) that
 * difference is significant CPU load saved.
 * ───────────────────────────────────────── */

/* Called when a TX-only DMA transfer completes */
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

/* Called when a full-duplex DMA transfer completes */
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
uint8_t SPI_DMA_IsComplete(void)
{
    return s_complete;
}

/* ─────────────────────────────────────────
 * Chip Select
 * ───────────────────────────────────────── */
void SPI_DMA_CS_Select(GPIO_TypeDef *csPort, uint16_t csPin)
{
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_RESET);
}

void SPI_DMA_CS_Deselect(GPIO_TypeDef *csPort, uint16_t csPin)
{
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_SET);
}

/* ─────────────────────────────────────────
 * TX — non-blocking
 * ─────────────────────────────────────────
 * HAL_SPI_Transmit_DMA kicks off the DMA controller and returns immediately.
 * DMA reads bytes from buf in memory and feeds them to the SPI TX register
 * with zero CPU involvement — the CPU never touches individual bytes.
 * The DMA IRQ fires once when the last byte has been transferred.
 *
 * buf must remain valid until IsComplete() returns 1.
 * Typical use: writing a full framebuffer to a display, or a 256-byte page
 * to a flash chip — transfers that would be very expensive byte-by-byte.
 * ───────────────────────────────────────── */
SPI_DMA_Status SPI_DMA_Transmit(uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) return SPI_DMA_ERROR;
    if (!s_complete)             return SPI_DMA_BUSY;

    s_complete = 0;

    if (HAL_SPI_Transmit_DMA(s_hspi, buf, len) != HAL_OK)
    {
        s_complete = 1;
        return SPI_DMA_ERROR;
    }

    return SPI_DMA_OK;
}

/* ─────────────────────────────────────────
 * Full-Duplex TX + RX — non-blocking
 * ─────────────────────────────────────────
 * HAL_SPI_TransmitReceive_DMA kicks off two DMA channels simultaneously:
 *   TX channel: streams txBuf from memory → SPI MOSI
 *   RX channel: streams SPI MISO → rxBuf in memory
 * CPU is completely uninvolved until the DMA IRQ fires at the end.
 *
 * Both buffers must be in normal SRAM and remain valid until IsComplete() == 1.
 *
 * Register read pattern:
 *   txBuf[0] = register address | 0x80   (read bit set)
 *   txBuf[1..n] = 0xFF                   (dummy bytes to clock in response)
 *   rxBuf[0]    = garbage                (ignore — clocked while address sent)
 *   rxBuf[1..n] = register data          (your actual result)
 * ───────────────────────────────────────── */
SPI_DMA_Status SPI_DMA_TransmitReceive(uint8_t *txBuf, uint8_t *rxBuf, uint16_t len)
{
    if (txBuf == NULL || rxBuf == NULL || len == 0) return SPI_DMA_ERROR;
    if (!s_complete)                                return SPI_DMA_BUSY;

    s_rxBuf    = rxBuf;
    s_rxLen    = len;
    s_complete = 0;

    if (HAL_SPI_TransmitReceive_DMA(s_hspi, txBuf, rxBuf, len) != HAL_OK)
    {
        s_complete = 1;
        return SPI_DMA_ERROR;
    }

    return SPI_DMA_OK;
}
