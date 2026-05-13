#include "i2c_interrupt.h"

/* ── Internal state ──────────────────────────────────────────────────────────
 * Stored here so the HAL callbacks can access them without extra parameters.
 * volatile prevents the compiler from caching values that the ISR can change.
 */
static I2C_HandleTypeDef *s_hi2c     = NULL;
static I2C_IT_Callback    s_callback = NULL;

/* Transfer-complete flag — 1 means bus is idle, 0 means transfer in flight.
 * Set to 0 before each transfer, set back to 1 inside the completion callback. */
static volatile uint8_t s_complete = 1;

/* Pointer and length of the active RX buffer — passed to the user callback */
static uint8_t  *s_rxBuf = NULL;
static uint16_t  s_rxLen = 0;

/* ─────────────────────────────────────────
 * Internal helpers
 * ───────────────────────────────────────── */
static I2C_IT_Status translate_error(void)
{
    uint32_t err = HAL_I2C_GetError(s_hi2c);

    if (err & HAL_I2C_ERROR_AF) return I2C_IT_NACK;   // address not acknowledged

    return I2C_IT_ERROR;
}

static void transfer_complete_tx(void)
{
    s_complete = 1;

    if (s_callback != NULL)
    {
        s_callback(NULL, 0);
    }
}

static void transfer_complete_rx(void)
{
    s_complete = 1;

    if (s_callback != NULL)
    {
        s_callback(s_rxBuf, s_rxLen);
    }
}

/* ─────────────────────────────────────────
 * Init
 * ───────────────────────────────────────── */
I2C_IT_Status I2C_IT_Init(I2C_HandleTypeDef *hi2c,
                            uint32_t clockSpeed,
                            I2C_IT_Callback callback)
{
    if (hi2c == NULL) return I2C_IT_ERROR;

    s_hi2c     = hi2c;
    s_callback = callback;
    s_complete = 1;

    hi2c->Init.OwnAddress1     = 0x00;
    hi2c->Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c->Init.OwnAddress2     = 0xFF;
    hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c->Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    /*
     * Timing register — pre-computed values for 80 MHz APB1 clock (HSI default
     * on NUCLEO-L476RG after SystemClock_Config). If your clock differs, use
     * CubeMX → I2C → Configuration to regenerate the correct TIMINGR value.
     *
     * 100 kHz standard : 0x10909CEC
     * 400 kHz fast     : 0x00702991
     */
    hi2c->Init.Timing = (clockSpeed <= 100000UL) ? 0x10909CEC : 0x00702991;

    if (HAL_I2C_Init(hi2c) != HAL_OK)
    {
        return I2C_IT_ERROR;
    }

    return I2C_IT_OK;
}

/* ─────────────────────────────────────────
 * HAL callbacks
 * ─────────────────────────────────────────
 * HAL calls these from inside the I2C IRQ handler automatically.
 * Four callbacks cover the four transfer types:
 *   MemTxCplt  — HAL_I2C_Mem_Write_IT finished
 *   MemRxCplt  — HAL_I2C_Mem_Read_IT finished
 *   MasterTxCplt — HAL_I2C_Master_Transmit_IT finished
 *   MasterRxCplt — HAL_I2C_Master_Receive_IT finished
 * ErrorCallback fires for NACK, bus error, or arbitration loss.
 * Keep all callbacks short — no blocking code, no heavy processing.
 * ───────────────────────────────────────── */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == s_hi2c->Instance)
    {
        transfer_complete_tx();
    }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == s_hi2c->Instance)
    {
        transfer_complete_rx();
    }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == s_hi2c->Instance)
    {
        transfer_complete_tx();
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == s_hi2c->Instance)
    {
        transfer_complete_rx();
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == s_hi2c->Instance)
    {
        s_complete = 1;

        if (s_callback != NULL)
        {
            s_callback(NULL, 0);
        }
    }
}

/* ─────────────────────────────────────────
 * IsDeviceReady — blocking probe
 * ─────────────────────────────────────────
 * No interrupt-based version exists in HAL. Call once at startup to confirm
 * the slave is wired and present before launching async transfers.
 * ───────────────────────────────────────── */
I2C_IT_Status I2C_IT_IsDeviceReady(uint16_t devAddr, uint32_t trials)
{
    if (s_hi2c == NULL) return I2C_IT_ERROR;

    HAL_StatusTypeDef result = HAL_I2C_IsDeviceReady(s_hi2c, devAddr, trials, 100);

    if (result == HAL_OK) return I2C_IT_OK;

    return translate_error();
}

/* ─────────────────────────────────────────
 * Status
 * ───────────────────────────────────────── */
uint8_t I2C_IT_IsComplete(void)
{
    return s_complete;
}

/* ─────────────────────────────────────────
 * WriteRegister — non-blocking
 * ─────────────────────────────────────────
 * HAL_I2C_Mem_Write_IT returns immediately. The I2C peripheral clocks out
 * the address byte, register pointer, and data entirely in hardware. The ISR
 * fires when the STOP condition is issued and calls HAL_I2C_MemTxCpltCallback.
 * buf must remain valid until IsComplete() returns 1.
 * ───────────────────────────────────────── */
I2C_IT_Status I2C_IT_WriteRegister(uint16_t devAddr,
                                     uint8_t  regAddr,
                                     uint8_t *buf,
                                     uint16_t len)
{
    if (buf == NULL || len == 0) return I2C_IT_ERROR;
    if (!s_complete)             return I2C_IT_BUSY;

    s_complete = 0;

    if (HAL_I2C_Mem_Write_IT(s_hi2c,
                               devAddr,
                               (uint16_t)regAddr,
                               I2C_MEMADD_SIZE_8BIT,
                               buf,
                               len) != HAL_OK)
    {
        s_complete = 1;
        return translate_error();
    }

    return I2C_IT_OK;
}

/* ─────────────────────────────────────────
 * ReadRegister — non-blocking
 * ─────────────────────────────────────────
 * HAL_I2C_Mem_Read_IT returns immediately. HAL issues a repeated START after
 * sending the register address, then clocks in the response bytes. The ISR
 * fires after the final byte is received and calls HAL_I2C_MemRxCpltCallback.
 * buf must remain valid until the callback fires.
 * ───────────────────────────────────────── */
I2C_IT_Status I2C_IT_ReadRegister(uint16_t devAddr,
                                    uint8_t  regAddr,
                                    uint8_t *buf,
                                    uint16_t len)
{
    if (buf == NULL || len == 0) return I2C_IT_ERROR;
    if (!s_complete)             return I2C_IT_BUSY;

    s_rxBuf    = buf;
    s_rxLen    = len;
    s_complete = 0;

    if (HAL_I2C_Mem_Read_IT(s_hi2c,
                              devAddr,
                              (uint16_t)regAddr,
                              I2C_MEMADD_SIZE_8BIT,
                              buf,
                              len) != HAL_OK)
    {
        s_complete = 1;
        return translate_error();
    }

    return I2C_IT_OK;
}

/* ─────────────────────────────────────────
 * MasterTransmit — non-blocking raw TX
 * ─────────────────────────────────────────
 * For devices that do not use register addressing. Sends buf as a raw byte
 * stream to devAddr. ISR fires on completion and calls MasterTxCpltCallback.
 * buf must remain valid until IsComplete() returns 1.
 * ───────────────────────────────────────── */
I2C_IT_Status I2C_IT_MasterTransmit(uint16_t devAddr, uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) return I2C_IT_ERROR;
    if (!s_complete)             return I2C_IT_BUSY;

    s_complete = 0;

    if (HAL_I2C_Master_Transmit_IT(s_hi2c, devAddr, buf, len) != HAL_OK)
    {
        s_complete = 1;
        return translate_error();
    }

    return I2C_IT_OK;
}

/* ─────────────────────────────────────────
 * MasterReceive — non-blocking raw RX
 * ─────────────────────────────────────────
 * For devices that send data without a preceding register pointer. Fills buf
 * with len bytes from devAddr. ISR fires on completion and calls
 * MasterRxCpltCallback. buf must remain valid until the callback fires.
 * ───────────────────────────────────────── */
I2C_IT_Status I2C_IT_MasterReceive(uint16_t devAddr, uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) return I2C_IT_ERROR;
    if (!s_complete)             return I2C_IT_BUSY;

    s_rxBuf    = buf;
    s_rxLen    = len;
    s_complete = 0;

    if (HAL_I2C_Master_Receive_IT(s_hi2c, devAddr, buf, len) != HAL_OK)
    {
        s_complete = 1;
        return translate_error();
    }

    return I2C_IT_OK;
}
