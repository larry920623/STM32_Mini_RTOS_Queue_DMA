#include "uart_dma.h"
#include <stddef.h>

extern UART_HandleTypeDef huart1;


static TCB_t * volatile uart_dma_waiter = NULL;
static volatile HAL_StatusTypeDef uart_dma_result = HAL_OK;

volatile uint32_t uart_dma_done_count = 0U;
volatile uint32_t uart_dma_error_count = 0U;
volatile HAL_StatusTypeDef uart_dma_last_status = HAL_OK;


HAL_StatusTypeDef UART_SendDMAAndWait(
    const uint8_t *data,
    uint16_t length)
{
    if ((__get_IPSR() != 0U) ||
        (__get_PRIMASK() != 0U) ||
        (current_tcb == NULL) ||
        (data == NULL) ||
        (length == 0U)) {
        return HAL_ERROR;
    }

    uint32_t saved_primask = __get_PRIMASK();

    __disable_irq();

    if (uart_dma_waiter != NULL) {
        __set_PRIMASK(saved_primask);
        return HAL_BUSY;
    }

    TCB_t *self = current_tcb;

    uart_dma_result = HAL_BUSY;
    uart_dma_waiter = self;

    HAL_StatusTypeDef status =
        HAL_UART_Transmit_DMA(&huart1, data, length);

    if (status != HAL_OK) {
        uart_dma_waiter = NULL;
        __set_PRIMASK(saved_primask);
        return status;
    }

    while (uart_dma_waiter == self) {
        self->state = TASK_BLOCKED;

        OS_Schedule();

        __set_PRIMASK(saved_primask);

        __DSB();
        __ISB();

        __disable_irq();
    }

    status = uart_dma_result;

    __set_PRIMASK(saved_primask);

    return status;
}

static void UART_DMA_Finish(HAL_StatusTypeDef result)
{
    uint32_t saved_primask = __get_PRIMASK();

    __disable_irq();

    if (uart_dma_waiter != NULL) {
        TCB_t *task = uart_dma_waiter;

        uart_dma_result = result;

        if (result == HAL_OK) {
            uart_dma_done_count++;
        }

        uart_dma_waiter = NULL;
        task->state = TASK_READY;
    }

    __set_PRIMASK(saved_primask);

}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        UART_DMA_Finish(HAL_OK);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        UART_DMA_Finish(HAL_ERROR);
    }
}

uint8_t UART_DMA_IsTaskWaiting(const TCB_t *task)
{
    uint32_t saved_primask = __get_PRIMASK();

    __disable_irq();

    uint8_t waiting =
        (task != NULL) &&
        (uart_dma_waiter == task) &&
        (task->state == TASK_BLOCKED);

    __set_PRIMASK(saved_primask);

    return waiting;
}
