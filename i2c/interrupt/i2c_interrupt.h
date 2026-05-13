#ifndef I2C_INTERRUPT_H
#define I2C_INTERRUPT_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/* Status codes */
typedef enum
{
    I2C_IT_OK    = 0,
    I2C_IT_ERROR = 1,
    I2C_IT_BUSY  = 2,  /* Transfer still in flight — try again later */
    I2C_IT_NACK  = 3   /* Slave did not acknowledge — wrong address or not ready */
} I2C_IT_Status;

/*
 * Address convention — IMPORTANT
 * HAL expects the 7-bit device address shifted left by 1 (the 8-bit form).
 * Example: BME280 default address is 0x76 → pass (0x76 << 1) = 0xEC
 * The LSB (read/write bit) is set automatically by HAL.
 * Passing the raw 7-bit address is the single most common I2C mistake.
 */
#define I2C_ADDR(addr7bit)  ((uint16_t)((addr7bit) << 1))

/*
 * Callback type — user provides this function; it is called when a
 * transfer completes or when an error occurs.
 * For write operations: rxData is NULL, len is 0.
 * For read  operations: rxData points to the filled buffer, len is byte count.
 * Called from ISR context — keep it short, no blocking code.
 */
typedef void (*I2C_IT_Callback)(uint8_t *rxData, uint16_t len);

/*
 * Init — configure peripheral, store handle, register user callback
 * clockSpeed : target bus speed in Hz — e.g. 100000 (standard) or 400000 (fast)
 * callback   : called on transfer complete or error, pass NULL if not needed
 *
 * The STM32L4 I2C peripheral uses a timing register (TIMINGR) rather than a
 * simple clock divider. Init selects a pre-computed value for the 80 MHz APB1
 * clock (HSI default on NUCLEO-L476RG). If your APB1 clock differs, regenerate
 * the TIMINGR value with CubeMX → I2C → Configuration.
 */
I2C_IT_Status I2C_IT_Init(I2C_HandleTypeDef *hi2c,
                            uint32_t clockSpeed,
                            I2C_IT_Callback callback);

/*
 * IsDeviceReady — blocking probe; confirms the slave is present and ACKs
 * No interrupt version exists in HAL — this is intentionally synchronous.
 * Call once at startup before launching any async transfers.
 * trials : number of address attempts before giving up (typically 3)
 */
I2C_IT_Status I2C_IT_IsDeviceReady(uint16_t devAddr, uint32_t trials);

/* Returns 1 if no transfer is in flight and the bus is free, 0 if busy */
uint8_t I2C_IT_IsComplete(void);

/*
 * WriteRegister — non-blocking register write
 * Standard sensor configuration pattern:
 *   master sends: [devAddr W] [regAddr] [data0] [data1] ... [STOP]
 * HAL_I2C_Mem_Write_IT handles the register address byte internally.
 * buf must remain valid until IsComplete() returns 1.
 * Returns I2C_IT_BUSY if a previous transfer is still in flight.
 */
I2C_IT_Status I2C_IT_WriteRegister(uint16_t devAddr,
                                     uint8_t  regAddr,
                                     uint8_t *buf,
                                     uint16_t len);

/*
 * ReadRegister — non-blocking register read
 * Standard sensor read pattern:
 *   master sends: [devAddr W] [regAddr] [repeated START] [devAddr R] → receives data [STOP]
 * HAL_I2C_Mem_Read_IT handles the repeated start automatically.
 * buf must remain valid until the callback fires (IsComplete() returns 1).
 * Returns I2C_IT_BUSY if a previous transfer is still in flight.
 */
I2C_IT_Status I2C_IT_ReadRegister(uint16_t devAddr,
                                    uint8_t  regAddr,
                                    uint8_t *buf,
                                    uint16_t len);

/*
 * MasterTransmit — non-blocking raw byte transmit, no register address
 * Use when the slave protocol does not use register addressing
 * (e.g. some OLED controllers, command-based devices).
 * buf must remain valid until IsComplete() returns 1.
 */
I2C_IT_Status I2C_IT_MasterTransmit(uint16_t devAddr, uint8_t *buf, uint16_t len);

/*
 * MasterReceive — non-blocking raw byte receive, no register address
 * Use when the slave sends data without a preceding register pointer.
 * buf must remain valid until the callback fires (IsComplete() returns 1).
 */
I2C_IT_Status I2C_IT_MasterReceive(uint16_t devAddr, uint8_t *buf, uint16_t len);

#endif /* I2C_INTERRUPT_H */
