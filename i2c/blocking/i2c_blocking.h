#ifndef I2C_BLOCKING_H
#define I2C_BLOCKING_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/* Status codes */
typedef enum
{
    I2C_BLOCKING_OK      = 0,
    I2C_BLOCKING_ERROR   = 1,
    I2C_BLOCKING_TIMEOUT = 2,
    I2C_BLOCKING_BUSY    = 3,  /* Bus held by another master (multi-master) or
                                   slave is clock-stretching                   */
    I2C_BLOCKING_NACK    = 4   /* Slave did not acknowledge — wrong address or
                                   device not present / not ready              */
} I2C_Blocking_Status;

/*
 * Address convention — IMPORTANT
 * HAL expects the 7-bit device address shifted left by 1 (the 8-bit form).
 * Example: BME280 default address is 0x76 → pass (0x76 << 1) = 0xEC
 * The LSB (read/write bit) is set automatically by HAL.
 * Passing the raw 7-bit address is the single most common I2C mistake.
 */
#define I2C_ADDR(addr7bit)  ((uint16_t)((addr7bit) << 1))

/* Init */
I2C_Blocking_Status I2C_Blocking_Init(I2C_HandleTypeDef *hi2c, uint32_t clockSpeed);

/*
 * IsDeviceReady — probe the bus for a slave at the given address
 * Useful for startup checks and bus scanning
 * trials : number of attempts before giving up (typically 3)
 */
I2C_Blocking_Status I2C_Blocking_IsDeviceReady(I2C_HandleTypeDef *hi2c,
                                                uint16_t devAddr,
                                                uint32_t trials,
                                                uint32_t timeout);

/*
 * WriteRegister — write one or more bytes to a device register
 * This is the standard pattern for configuring a sensor:
 *   master sends: [devAddr W] [regAddr] [data0] [data1] ... [STOP]
 * Uses HAL_I2C_Mem_Write which handles the register address byte internally
 * and issues a repeated start, matching the behaviour expected by most sensors.
 */
I2C_Blocking_Status I2C_Blocking_WriteRegister(I2C_HandleTypeDef *hi2c,
                                                uint16_t devAddr,
                                                uint8_t  regAddr,
                                                uint8_t *data,
                                                uint16_t len,
                                                uint32_t timeout);

/*
 * ReadRegister — read one or more bytes from a device register
 * Standard sensor read pattern:
 *   master sends: [devAddr W] [regAddr] [repeated START] [devAddr R] → receives data [STOP]
 * HAL_I2C_Mem_Read handles the repeated start automatically.
 */
I2C_Blocking_Status I2C_Blocking_ReadRegister(I2C_HandleTypeDef *hi2c,
                                               uint16_t devAddr,
                                               uint8_t  regAddr,
                                               uint8_t *data,
                                               uint16_t len,
                                               uint32_t timeout);

/*
 * MasterTransmit — raw byte transmit to a slave, no register address
 * Use when the slave protocol does not use register addressing
 * (e.g. some OLED controllers, command-based devices)
 */
I2C_Blocking_Status I2C_Blocking_MasterTransmit(I2C_HandleTypeDef *hi2c,
                                                  uint16_t devAddr,
                                                  uint8_t *buf,
                                                  uint16_t len,
                                                  uint32_t timeout);

/*
 * MasterReceive — raw byte receive from a slave, no register address
 * Use when the slave sends data without a preceding register pointer
 */
I2C_Blocking_Status I2C_Blocking_MasterReceive(I2C_HandleTypeDef *hi2c,
                                                uint16_t devAddr,
                                                uint8_t *buf,
                                                uint16_t len,
                                                uint32_t timeout);

#endif /* I2C_BLOCKING_H */
