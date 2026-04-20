#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

typedef enum
{
    RING_BUFFER_OK   = 0,
    RING_BUFFER_FULL = 1,
    RING_BUFFER_EMPTY = 2
} RingBuffer_Status;

typedef struct
{
    uint8_t         *buffer;
    uint16_t         size;
    volatile uint16_t head; // write index (ISR writes here)
    volatile uint16_t tail; // read index  (main loop reads here)
} RingBuffer_t;

void             RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint16_t size);
RingBuffer_Status RingBuffer_Write(RingBuffer_t *rb, uint8_t byte);
RingBuffer_Status RingBuffer_Read(RingBuffer_t *rb, uint8_t *byte);
uint16_t         RingBuffer_Available(RingBuffer_t *rb);

#endif /* RING_BUFFER_H */
