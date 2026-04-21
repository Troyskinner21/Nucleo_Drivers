#include "uart_dma.h"

/* ── Internal state ──────────────────────────────────────────────────────────
 * Stored here so HAL callbacks can reach driver state without extra parameters
 */
static UART_HandleTypeDef *s_huart = NULL;

/* TX complete flag — set to 1 by TxCpltCallback, cleared before each transfer */
static volatile uint8_t s_txComplete = 1;

/* ─────────────────────────────────────────
 * Init
 * ───────────────────────────────────────── */
UART_DMA_Status UART_DMA_Init(UART_HandleTypeDef *huart, uint32_t baudrate)
{
    if (huart == NULL) return UART_DMA_ERROR;

    s_huart = huart;

    huart->Init.BaudRate     = baudrate;
    huart->Init.WordLength   = UART_WORDLENGTH_8B;
    huart->Init.StopBits     = UART_STOPBITS_1;
    huart->Init.Parity       = UART_PARITY_NONE;
    huart->Init.Mode         = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(huart) != HAL_OK)
    {
        return UART_DMA_ERROR;
    }

    s_txComplete = 1;

    return UART_DMA_OK;
}

/* ─────────────────────────────────────────
 * HAL callbacks
 * ───────────────────────────────────────── */

/* ── Normal mode TX complete ────────────────────────────────────────────────
 * Called by HAL once the DMA has transferred all bytes to the UART TX FIFO
 * Also called at the end of each circular loop — set flag so caller can
 * decide whether to stop or refill the second half of the buffer            */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == s_huart->Instance)
    {
        s_txComplete = 1;
    }
}

/* ── Circular mode half-transfer ────────────────────────────────────────────
 * Called by HAL when DMA reaches the midpoint of the buffer
 * Refill the first half here while DMA is still reading from the second half
 * This is the double-buffer pattern — one half is always being transmitted
 * while the other half is being prepared by the CPU                         */
void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == s_huart->Instance)
    {
        /* Caller is responsible for refilling the first half of the buffer
         * This weak callback is overridden in the application when needed   */
    }
}

/* ─────────────────────────────────────────
 * TX — Normal Mode
 * ───────────────────────────────────────── */

/* DMA reads the entire string from memory and streams it to UART
 * Returns immediately — transfer happens with zero CPU involvement
 * The buffer pointer must remain valid until s_txComplete == 1             */
UART_DMA_Status UART_DMA_TransmitString(const char *str)
{
    if (str == NULL)   return UART_DMA_ERROR;
    if (!s_txComplete) return UART_DMA_BUSY;

    uint16_t len = (uint16_t)strlen(str);
    if (len == 0) return UART_DMA_ERROR;

    s_txComplete = 0;

    /* Cast away const — HAL takes uint8_t* but only reads from the buffer */
    if (HAL_UART_Transmit_DMA(s_huart, (uint8_t *)str, len) != HAL_OK)
    {
        s_txComplete = 1;   /* reset so caller can retry */
        return UART_DMA_ERROR;
    }

    return UART_DMA_OK;
}

UART_DMA_Status UART_DMA_TransmitBuffer(const uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) return UART_DMA_ERROR;
    if (!s_txComplete)           return UART_DMA_BUSY;

    s_txComplete = 0;

    if (HAL_UART_Transmit_DMA(s_huart, (uint8_t *)buf, len) != HAL_OK)
    {
        s_txComplete = 1;
        return UART_DMA_ERROR;
    }

    return UART_DMA_OK;
}

uint8_t UART_DMA_IsTxComplete(void)
{
    return s_txComplete;
}

/* ─────────────────────────────────────────
 * TX — Circular Mode
 * ───────────────────────────────────────── */

/* Start continuous streaming — DMA loops over the buffer indefinitely
 * HAL_UART_TxHalfCpltCallback fires at the midpoint — refill first half
 * HAL_UART_TxCpltCallback fires at the end — refill second half
 * The application overrides those callbacks to supply fresh data each cycle  */
UART_DMA_Status UART_DMA_StartCircular(uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) return UART_DMA_ERROR;

    s_txComplete = 0;

    if (HAL_UART_Transmit_DMA(s_huart, buf, len) != HAL_OK)
    {
        s_txComplete = 1;
        return UART_DMA_ERROR;
    }

    return UART_DMA_OK;
}

/* Stop an in-progress circular DMA transfer */
UART_DMA_Status UART_DMA_StopCircular(void)
{
    if (HAL_UART_DMAStop(s_huart) != HAL_OK)
    {
        return UART_DMA_ERROR;
    }

    s_txComplete = 1;

    return UART_DMA_OK;
}
