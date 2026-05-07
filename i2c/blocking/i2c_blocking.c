#include "i2c_blocking.h"

/* ─────────────────────────────────────────
 * Init
 * ─────────────────────────────────────────
 * clockSpeed : I2C_SPEED_STANDARD (100 kHz) or I2C_SPEED_FAST (400 kHz)
 *
 * Standard mode (100 kHz) — safe default, works with all I2C devices
 * Fast mode (400 kHz)     — supported by most modern sensors (BME280, MPU-6050)
 *                           check your device datasheet before enabling
 *
 * The STM32L4 I2C peripheral uses a timing register (TIMINGR) rather than a
 * simple clock divider. CubeMX calculates and sets this value automatically
 * based on the APB clock and the target speed. The clockSpeed parameter here
 * is used to select the correct preset so we do not have to compute it manually.
 *
 * If CubeMX already configured the timing, calling Init again is safe —
 * HAL_I2C_Init re-applies all settings idempotently.
 * ───────────────────────────────────────── */
I2C_Blocking_Status I2C_Blocking_Init(I2C_HandleTypeDef *hi2c, uint32_t clockSpeed)
{
    if (hi2c == NULL) return I2C_BLOCKING_ERROR;

    hi2c->Init.OwnAddress1      = 0x00;              // master has no fixed address
    hi2c->Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c->Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c->Init.OwnAddress2      = 0xFF;
    hi2c->Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c->Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;  // allow clock stretching

    /*
     * Timing register — pre-computed values for 80 MHz APB1 clock (HSI default
     * on NUCLEO-L476RG after SystemClock_Config). If your clock differs, use
     * CubeMX → I2C → Configuration to regenerate the correct TIMINGR value.
     *
     * 100 kHz standard : 0x10909CEC
     * 400 kHz fast     : 0x00702991
     */
    if (clockSpeed <= 100000UL)
    {
        hi2c->Init.Timing = 0x10909CEC;
    }
    else
    {
        hi2c->Init.Timing = 0x00702991;
    }

    if (HAL_I2C_Init(hi2c) != HAL_OK)
    {
        return I2C_BLOCKING_ERROR;
    }

    return I2C_BLOCKING_OK;
}

/* ─────────────────────────────────────────
 * Translate HAL errors to driver status
 * ─────────────────────────────────────────
 * HAL_I2C_GetError() returns a bitmask of error flags. NACK and TIMEOUT are
 * the two you handle differently — NACK usually means wrong address or device
 * absent; TIMEOUT usually means the bus is locked (pull-ups missing, slave
 * holding SDA low after a power glitch).
 * ───────────────────────────────────────── */
static I2C_Blocking_Status translate_error(I2C_HandleTypeDef *hi2c)
{
    uint32_t err = HAL_I2C_GetError(hi2c);

    if (err & HAL_I2C_ERROR_AF)      return I2C_BLOCKING_NACK;
    if (err & HAL_I2C_ERROR_TIMEOUT) return I2C_BLOCKING_TIMEOUT;
    if (err & HAL_I2C_ERROR_BERR)    return I2C_BLOCKING_BUSY;   // bus error / arbitration lost

    return I2C_BLOCKING_ERROR;
}

/* ─────────────────────────────────────────
 * IsDeviceReady
 * ─────────────────────────────────────────
 * Sends the device address and listens for an ACK.
 * Returns I2C_BLOCKING_OK if the device acknowledges, I2C_BLOCKING_NACK if not.
 * Useful at startup to confirm a sensor is wired correctly before attempting
 * to configure it.
 * ───────────────────────────────────────── */
I2C_Blocking_Status I2C_Blocking_IsDeviceReady(I2C_HandleTypeDef *hi2c,
                                                uint16_t devAddr,
                                                uint32_t trials,
                                                uint32_t timeout)
{
    if (hi2c == NULL) return I2C_BLOCKING_ERROR;

    HAL_StatusTypeDef result = HAL_I2C_IsDeviceReady(hi2c, devAddr, trials, timeout);

    if (result == HAL_OK)      return I2C_BLOCKING_OK;
    if (result == HAL_TIMEOUT) return I2C_BLOCKING_TIMEOUT;

    return translate_error(hi2c);
}

/* ─────────────────────────────────────────
 * WriteRegister
 * ─────────────────────────────────────────
 * The most common I2C operation — configure a sensor register.
 *
 * HAL_I2C_Mem_Write handles the full transaction internally:
 *   [START] [devAddr | W] [ACK] [regAddr] [ACK] [data...] [ACK] [STOP]
 *
 * I2C_MEMADD_SIZE_8BIT covers virtually all sensors (8-bit register map).
 * Use I2C_MEMADD_SIZE_16BIT for EEPROMs or devices with a 16-bit address space.
 * ───────────────────────────────────────── */
I2C_Blocking_Status I2C_Blocking_WriteRegister(I2C_HandleTypeDef *hi2c,
                                                uint16_t devAddr,
                                                uint8_t  regAddr,
                                                uint8_t *data,
                                                uint16_t len,
                                                uint32_t timeout)
{
    if (hi2c == NULL || data == NULL || len == 0) return I2C_BLOCKING_ERROR;

    HAL_StatusTypeDef result = HAL_I2C_Mem_Write(hi2c,
                                                   devAddr,
                                                   (uint16_t)regAddr,
                                                   I2C_MEMADD_SIZE_8BIT,
                                                   data,
                                                   len,
                                                   timeout);
    if (result == HAL_OK)      return I2C_BLOCKING_OK;
    if (result == HAL_TIMEOUT) return I2C_BLOCKING_TIMEOUT;

    return translate_error(hi2c);
}

/* ─────────────────────────────────────────
 * ReadRegister
 * ─────────────────────────────────────────
 * The most common I2C read — fetch register data from a sensor.
 *
 * HAL_I2C_Mem_Read handles the repeated start internally:
 *   [START] [devAddr | W] [ACK] [regAddr] [ACK]
 *   [repeated START] [devAddr | R] [ACK] [data...] [NACK] [STOP]
 *
 * The NACK before STOP is correct — the master NACKs the last byte to
 * signal to the slave that no more data is needed.
 * ───────────────────────────────────────── */
I2C_Blocking_Status I2C_Blocking_ReadRegister(I2C_HandleTypeDef *hi2c,
                                               uint16_t devAddr,
                                               uint8_t  regAddr,
                                               uint8_t *data,
                                               uint16_t len,
                                               uint32_t timeout)
{
    if (hi2c == NULL || data == NULL || len == 0) return I2C_BLOCKING_ERROR;

    HAL_StatusTypeDef result = HAL_I2C_Mem_Read(hi2c,
                                                  devAddr,
                                                  (uint16_t)regAddr,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  data,
                                                  len,
                                                  timeout);

    if (result == HAL_OK)      return I2C_BLOCKING_OK;
    if (result == HAL_TIMEOUT) return I2C_BLOCKING_TIMEOUT;

    return translate_error(hi2c);
}

/* ─────────────────────────────────────────
 * MasterTransmit
 * ─────────────────────────────────────────
 * Raw transmit — no register address byte.
 * Use for devices that interpret the first data byte as a command rather than
 * a register pointer (e.g. some OLED controllers, DACs, IO expanders).
 *
 * Transaction: [START] [devAddr | W] [ACK] [buf[0]] [ACK] ... [STOP]
 * ───────────────────────────────────────── */
I2C_Blocking_Status I2C_Blocking_MasterTransmit(I2C_HandleTypeDef *hi2c,
                                                  uint16_t devAddr,
                                                  uint8_t *buf,
                                                  uint16_t len,
                                                  uint32_t timeout)
{
    if (hi2c == NULL || buf == NULL || len == 0) return I2C_BLOCKING_ERROR;

    HAL_StatusTypeDef result = HAL_I2C_Master_Transmit(hi2c, devAddr, buf, len, timeout);

    if (result == HAL_OK)      return I2C_BLOCKING_OK;
    if (result == HAL_TIMEOUT) return I2C_BLOCKING_TIMEOUT;

    return translate_error(hi2c);
}

/* ─────────────────────────────────────────
 * MasterReceive
 * ─────────────────────────────────────────
 * Raw receive — no register address byte.
 * Use for devices that send data unprompted after a plain address read,
 * or when following a previous MasterTransmit that already set the register
 * pointer.
 *
 * Transaction: [START] [devAddr | R] [ACK] [data...] [NACK] [STOP]
 * ───────────────────────────────────────── */
I2C_Blocking_Status I2C_Blocking_MasterReceive(I2C_HandleTypeDef *hi2c,
                                                uint16_t devAddr,
                                                uint8_t *buf,
                                                uint16_t len,
                                                uint32_t timeout)
{
    if (hi2c == NULL || buf == NULL || len == 0) return I2C_BLOCKING_ERROR;

    HAL_StatusTypeDef result = HAL_I2C_Master_Receive(hi2c, devAddr, buf, len, timeout);

    if (result == HAL_OK)      return I2C_BLOCKING_OK;
    if (result == HAL_TIMEOUT) return I2C_BLOCKING_TIMEOUT;

    return translate_error(hi2c);
}
