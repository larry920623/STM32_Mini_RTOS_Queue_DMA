/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "msg_queue.h"
#include "os_sync.h"
#include "uart_dma.h"
#include "os.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TASK_STACK_SIZE 512
#define RUN_MUTEX_TEST 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */
uint32_t task1_stack[TASK_STACK_SIZE];
uint32_t task2_stack[TASK_STACK_SIZE];

TCB_t tcb1, tcb2;

uint32_t idle_stack[TASK_STACK_SIZE];
TCB_t tcb_idle;

Mutex_t uart_mutex;
Semaphore_t sync_sem;

MsgQueue_t data_queue;
volatile uint32_t queue_full_count = 0;

volatile uint32_t producer_during_dma_count = 0U;

Mutex_t test_mutex;

volatile uint32_t mutex_test_inside = 0U;
volatile uint32_t mutex_test_total = 0U;
volatile uint32_t mutex_test_t1 = 0U;
volatile uint32_t mutex_test_t2 = 0U;
volatile uint32_t mutex_test_handoffs = 0U;
volatile uint32_t mutex_test_errors = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */




void Idle_Task_Entry(void) {
    while(1) {
    	__WFI();
    }
}





void Task1_Entry(void)
{
    Message_t msg;

    static char tx_buffer[160];

    while (1) {
        Queue_Receive(&data_queue, &msg);

        uint32_t saved_primask = __get_PRIMASK();

        __disable_irq();

        uint32_t rx_wait =
            data_queue.receive_wait_count;

        uint32_t tx_wait =
            data_queue.send_wait_count;

        uint32_t dma_done =
            uart_dma_done_count;

        uint32_t other_runs =
            producer_during_dma_count;

        __set_PRIMASK(saved_primask);

        int length = snprintf(
            tx_buffer,
            sizeof(tx_buffer),
            "[DMA Consumer] seq=%lu, value=%lu, "
            "rx_wait=%lu, tx_wait=%lu, "
            "dma_done=%lu, other_runs=%lu\r\n",
            (unsigned long)msg.seq,
            (unsigned long)msg.value,
            (unsigned long)rx_wait,
            (unsigned long)tx_wait,
            (unsigned long)dma_done,
            (unsigned long)other_runs
        );

        if ((length <= 0) ||
            (length >= (int)sizeof(tx_buffer))) {
            uart_dma_last_status = HAL_ERROR;
            uart_dma_error_count++;

            OS_Delay(1000);
            continue;
        }

        uart_dma_last_status = UART_SendDMAAndWait(
            (const uint8_t *)tx_buffer,
            (uint16_t)length
        );

        if (uart_dma_last_status != HAL_OK) {
            uart_dma_error_count++;
        }

        OS_Delay(1000);
    }
}

void Task2_Entry(void)
{
    uint32_t next_seq = 1U;
    Message_t msg;

    while (1) {
        uint32_t saved_primask = __get_PRIMASK();

        __disable_irq();

        if (UART_DMA_IsTaskWaiting(&tcb1)) {
            producer_during_dma_count++;
        }

        __set_PRIMASK(saved_primask);

        msg.seq = next_seq;
        msg.value = next_seq * 10U;

        Queue_Send(&data_queue, &msg);

        next_seq++;

        OS_Delay(1);
    }
}

static void Mutex_TestStep(uint32_t task_id)
{
    Mutex_Lock(&test_mutex);

    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();

    if ((test_mutex.owner != current_tcb) ||
        (test_mutex.locked == 0U) ||
        (mutex_test_inside != 0U)) {
        mutex_test_errors++;
    }

    mutex_test_inside++;

    __set_PRIMASK(saved_primask);


    OS_Delay(100);

    saved_primask = __get_PRIMASK();
    __disable_irq();


    if ((test_mutex.owner != current_tcb) ||
        (test_mutex.locked == 0U) ||
        (mutex_test_inside != 1U)) {
        mutex_test_errors++;
    }

    mutex_test_total++;

    if (task_id == 1U) {
        mutex_test_t1++;
    } else {
        mutex_test_t2++;
    }

    mutex_test_inside--;


    if ((test_mutex.owner == current_tcb) &&
        (test_mutex.blocked_task != NULL)) {
        mutex_test_handoffs++;
    }


    Mutex_Unlock(&test_mutex);

    __set_PRIMASK(saved_primask);

    __DSB();
    __ISB();
}

void Mutex_TestTask1_Entry(void)
{
    static char buffer[160];

    while (1) {
        Mutex_TestStep(1U);

        uint32_t saved_primask = __get_PRIMASK();
        __disable_irq();

        uint32_t t1 = mutex_test_t1;
        uint32_t t2 = mutex_test_t2;
        uint32_t total = mutex_test_total;
        uint32_t handoffs = mutex_test_handoffs;
        uint32_t errors = mutex_test_errors;

        __set_PRIMASK(saved_primask);

        int length = snprintf(
            buffer,
            sizeof(buffer),
            "[Mutex Test] T1=%lu, T2=%lu, total=%lu, "
            "handoff=%lu, errors=%lu\r\n",
            (unsigned long)t1,
            (unsigned long)t2,
            (unsigned long)total,
            (unsigned long)handoffs,
            (unsigned long)errors
        );

        if ((length > 0) &&
            (length < (int)sizeof(buffer))) {
            uart_dma_last_status = UART_SendDMAAndWait(
                (const uint8_t *)buffer,
                (uint16_t)length
            );

            if (uart_dma_last_status != HAL_OK) {
                uart_dma_error_count++;
            }
        }

        OS_Delay(10);
    }
}

void Mutex_TestTask2_Entry(void)
{
    while (1) {
        Mutex_TestStep(2U);

        OS_Delay(1);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  Mutex_Init(&uart_mutex);
  Mutex_Init(&test_mutex);
  Sem_Init(&sync_sem, 0);
  Queue_Init(&data_queue);
  #if RUN_MUTEX_TEST

  Task_Init(
    &tcb1,
    Mutex_TestTask1_Entry,
    task1_stack,
    TASK_STACK_SIZE
  );

  Task_Init(
    &tcb2,
    Mutex_TestTask2_Entry,
    task2_stack,
    TASK_STACK_SIZE
  );

  #else

  Task_Init(
    &tcb1,
    Task1_Entry,
    task1_stack,
    TASK_STACK_SIZE
  );

  Task_Init(
    &tcb2,
    Task2_Entry,
    task2_stack,
    TASK_STACK_SIZE
  );

  #endif
  Task_Init(&tcb_idle, Idle_Task_Entry, idle_stack, TASK_STACK_SIZE);

  static const uint8_t boot_msg[] =
      "\r\n=== BOOT ===\r\n";

  HAL_StatusTypeDef boot_status =
      HAL_UART_Transmit(
          &huart1,
          boot_msg,
          sizeof(boot_msg) - 1U,
          1000
      );

  current_tcb = &tcb1;
  OS_Start();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
