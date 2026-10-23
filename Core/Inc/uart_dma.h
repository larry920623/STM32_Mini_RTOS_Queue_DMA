#ifndef UART_DMA_H
#define UART_DMA_H

#include "stm32f4xx_hal.h"
#include "os.h"


extern volatile uint32_t uart_dma_done_count;
extern volatile uint32_t uart_dma_error_count;
extern volatile HAL_StatusTypeDef uart_dma_last_status;


HAL_StatusTypeDef UART_SendDMAAndWait(
    const uint8_t *data,
    uint16_t length
);

uint8_t UART_DMA_IsTaskWaiting(const TCB_t *task);

#endif /* UART_DMA_H */
