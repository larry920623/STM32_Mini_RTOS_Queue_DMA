# STM32 Mini RTOS — Message Queue & UART DMA

以 **STM32F407 / ARM Cortex-M4** 實作的小型 RTOS，從 Task Context Switch 延伸到 **阻塞式 Message Queue、Mutex 所有權交接，以及 UART TX DMA 與 Task 的同步**。

RTOS 核心由 C 與組合語言自行實作；時脈、GPIO、UART、DMA 等周邊初始化使用 STM32CubeMX 與 STM32 HAL，未使用 FreeRTOS。

目前提供兩種可切換的實機 Demo：

- **Queue + DMA**：Producer 傳送資料，Consumer 接收後透過 UART DMA 輸出；Queue 滿／空或等待 DMA 時，對應 Task 進入阻塞狀態。
- **Mutex Contention Test**：兩個 Task 競爭同一把 Mutex，檢查所有權、互斥與交接後的持續執行。

## 核心功能

| 功能 | 實作方式 |
|---|---|
| Context Switch | 使用 PSP 管理 Task Stack，由 PendSV 與 naked assembly 保存、還原 `R4–R11`，例外堆疊由 Cortex-M4 硬體處理 |
| Tick 與排程 | SysTick 每 1 ms 呼叫 `OS_Tick()`，目前固定管理兩個工作 Task 與一個 Idle Task |
| Task Delay | `OS_Delay()` 設定 `delay_ticks` 並請求排程，延遲期間不選取該工作 Task |
| Idle Task | 工作 Task 都無法執行時，使用 `__WFI()` 等待中斷 |
| Message Queue | 固定容量環形緩衝區，以值複製方式保存訊息，提供非阻塞與阻塞 API |
| Mutex | 阻塞等待；解鎖時若有等待者，先交接所有權，再將等待者設為 READY |
| Semaphore | 提供 `Sem_Init()`、`Sem_Wait()`、`Sem_Signal()`，支援計數與單一等待者 |
| UART TX DMA | Task 啟動 DMA 後阻塞，完成／錯誤回呼記錄結果並將等待 Task 設為 READY |
| 模組化 | RTOS 核心、同步原語、Queue、UART DMA 與應用 Task 分開維護 |

Queue、Mutex 與 UART DMA 等模組使用短臨界區保護共享狀態，並保存、恢復 PRIMASK。Semaphore 的中斷狀態處理仍列於後續改善項目。

## 硬體與開發環境

| 項目 | 設定 |
|---|---|
| MCU | STM32F407VET6，ARM Cortex-M4 |
| SYSCLK | 168 MHz |
| IDE | STM32CubeIDE 1.16.0 |
| 燒錄／除錯 | ST-Link，SWD 介面 |
| 終端連線 | CP2102 USB-to-TTL、Tera Term |
| UART | USART1，115200 baud，8N1，無流量控制 |
| TX DMA | DMA2 Stream7 / Channel4，Memory-to-Peripheral，Normal mode |
| Task Stack | 每個 Task 靜態配置 512 個 `uint32_t`，即 2048 bytes |

### 接線

| STM32 | 連接對象 | 用途 |
|---|---|---|
| PA13 | ST-Link SWDIO | 燒錄與除錯 |
| PA14 | ST-Link SWCLK | 燒錄與除錯 |
| GND | ST-Link GND | 共地 |
| PA9 / USART1_TX | USB-to-TTL RX | 終端輸出 |
| PA10 / USART1_RX | USB-to-TTL TX | 接收腳位，目前 Demo 未使用接收功能 |
| GND | USB-to-TTL GND | 共地 |

使用與 STM32 相容的 3.3 V UART 邏輯電位，開發板供電依板卡接法配置。

## 模組與檔案

| 檔案 | 責任 |
|---|---|
| [`Core/Src/main.c`](Core/Src/main.c) | 周邊初始化、Task Stack / TCB、Demo Task、模式選擇與啟動流程 |
| [`Core/Src/os.c`](Core/Src/os.c) / [`os.h`](Core/Inc/os.h) | Task 初始化、啟動、PendSV、延遲、Tick 與排程 |
| [`Core/Src/os_sync.c`](Core/Src/os_sync.c) / [`os_sync.h`](Core/Inc/os_sync.h) | Semaphore 與 Mutex |
| [`Core/Src/msg_queue.c`](Core/Src/msg_queue.c) / [`msg_queue.h`](Core/Inc/msg_queue.h) | Queue 資料結構、讀寫與等待者喚醒 |
| [`Core/Src/uart_dma.c`](Core/Src/uart_dma.c) / [`uart_dma.h`](Core/Inc/uart_dma.h) | DMA 傳輸、等待者管理、完成與錯誤回呼 |
| [`Core/Src/stm32f4xx_it.c`](Core/Src/stm32f4xx_it.c) | SysTick、USART1 與 DMA 中斷入口 |
| [`Core/Src/stm32f4xx_hal_msp.c`](Core/Src/stm32f4xx_hal_msp.c) | GPIO Alternate Function、DMA Handle 連結與 NVIC 設定 |
| [`Mini_RTOS.ioc`](Mini_RTOS.ioc) | STM32CubeMX 周邊與腳位設定 |
| `Drivers/` | STM32 HAL 與 CMSIS |

目前 TCB 的 `state` 欄位只有 `TASK_READY`、`TASK_BLOCKED`；延遲另外使用 `delay_ticks` 管理。

## Build 與執行

1. 下載或 clone 本 repo：

   ```bash
   git clone https://github.com/larry920623/STM32_Mini_RTOS_Queue_DMA.git
   ```

2. 開啟 STM32CubeIDE，選擇：

   **File → Import → General → Existing Projects into Workspace**

   指定包含 `.project`、`.cproject` 與 `Mini_RTOS.ioc` 的 repo 根目錄。IDE 內的專案名稱仍為 `Mini_RTOS`。

3. 在 `Core/Src/main.c` 設定 `RUN_MUTEX_TEST`，執行 **Build Project**。

4. 透過 ST-Link 燒錄並啟動 Debug。若停在 `main()`，按 **Resume / F8**。

5. 用 Tera Term 開啟 USB-to-TTL 對應 COM 埠，設定：

   ```text
   Baud rate：115200
   Data：8 bit
   Parity：None
   Stop：1 bit
   Flow control：None
   ```

6. 程式啟動時先以阻塞式 UART 輸出 `=== BOOT ===`，再啟動 RTOS；Demo 文字則使用 UART DMA。

每次切換測試模式都需要重新 Build、燒錄。`Debug/`、`Release/` 與編譯產物由 `.gitignore` 排除。

## Demo 1：Blocking Queue + UART DMA

在 `main.c` 設定：

```c
#define RUN_MUTEX_TEST 0
```

### 執行流程

1. **Task2 / Producer** 產生 `{seq, value}`，其中 `value = seq × 10`，呼叫 `Queue_Send()`，成功後才增加序號。
2. **Task1 / Consumer** 呼叫 `Queue_Receive()`，使用 `snprintf()` 產生輸出文字。
3. Consumer 呼叫 `UART_SendDMAAndWait()` 啟動傳輸並進入 BLOCKED；緩衝區保留到傳輸完成，期間不重新寫入。
4. DMA 與 USART 中斷透過 HAL 完成傳輸處理，`HAL_UART_TxCpltCallback()` 更新完成數並將 Consumer 設為 READY。
5. 後續 SysTick 排程決定 Consumer 何時恢復執行；Consumer 傳輸完成後再延遲 1000 ticks。
6. Producer 成功送入後延遲 1 tick。由於 Consumer 較慢，Queue 填滿後 Producer 會阻塞，直到 Consumer 取出訊息並喚醒它。

Queue 容量為 **4 筆**，每筆訊息包含兩個 `uint32_t`。`head`、`tail`、`count` 管理環形緩衝區，傳入的資料會被複製到 Queue。

### Queue API

| API | 行為 |
|---|---|
| `Queue_Init()` | 啟動排程前初始化 Queue |
| `Queue_TrySend()` | 成功回傳 `QUEUE_OK`，滿載回傳 `QUEUE_FULL` |
| `Queue_TryReceive()` | 成功回傳 `QUEUE_OK`，無資料回傳 `QUEUE_EMPTY` |
| `Queue_Send()` | 滿載時阻塞，等待可用位置後重試 |
| `Queue_Receive()` | 無資料時阻塞，收到通知後重試 |

阻塞 API 需要由工作 Task 在中斷開啟時呼叫，並傳入有效指標。它們不是 ISR API，也沒有逾時參數。

### 實機輸出節錄

```text
=== BOOT ===
[DMA Consumer] seq=1, value=10, rx_wait=1, tx_wait=0, dma_done=0, other_runs=0
[DMA Consumer] seq=2, value=20, rx_wait=1, tx_wait=1, dma_done=1, other_runs=5
[DMA Consumer] seq=3, value=30, rx_wait=1, tx_wait=2, dma_done=2, other_runs=6
```

| 欄位 | 意義 |
|---|---|
| `seq` | 本筆訊息編號 |
| `value` | 本筆資料內容 |
| `rx_wait` | Consumer 因 Queue 空而登記等待的累計次數 |
| `tx_wait` | Producer 因 Queue 滿而登記等待的累計次數 |
| `dma_done` | 格式化本行文字前，已完成的 DMA 傳輸次數 |
| `other_runs` | Consumer 阻塞等待 DMA 通知時，Producer 執行到檢查點的累計次數 |

`dma_done` 在本行開始傳送前就已取樣，因此正常情況下會比本行 `seq` 少 1。

`other_runs` 是 Task 執行進度的觀察值，不能換算成 CPU 使用率或效能提升百分比。

## Demo 2：Mutex 所有權交接與競爭測試

在 `main.c` 設定：

```c
#define RUN_MUTEX_TEST 1
```

兩個 Task 呼叫 `Mutex_TestStep()` 競爭 `test_mutex`。每輪取得鎖後，刻意持有 Mutex 並延遲 100 ticks，讓另一個 Task 有機會進入等待。

測試檢查進入與離開時的所有權、鎖定狀態，以及是否有人同時進入受保護區。

`Mutex_Unlock()` 若發現等待者，會保留 `locked = 1`、把 `owner` 交給等待 Task，然後將該 Task 設為 READY。沒有等待者時才釋放鎖。

Task1 在釋放測試 Mutex 後，以 UART DMA 輸出統計。

### 實機輸出節錄

```text
[Mutex Test] T1=1, T2=0, total=1, handoff=1, errors=0
[Mutex Test] T1=2, T2=1, total=3, handoff=3, errors=0
[Mutex Test] T1=3, T2=2, total=5, handoff=5, errors=0
```

| 欄位 | 意義與檢查方式 |
|---|---|
| `T1`、`T2` | 各 Task 完成受保護操作的次數，兩者應持續增加 |
| `total` | 完成操作總數，應等於同一行的 `T1 + T2` |
| `handoff` | 測試在解鎖前，觀察到持有者有等待 Task 的累計次數 |
| `errors` | 所有權、鎖定狀態或獨占檢查失敗次數，預期維持 `0` |

上述紀錄是開發期間的實機功能驗證，不代表所有競態、負載與錯誤情境都已覆蓋。

Semaphore API 保留在同步模組中，目前兩種 Demo 沒有獨立驗證其等待／通知流程。

## 中斷與執行限制

目前使用 `NVIC_PRIORITYGROUP_4`：

| 中斷 | Preemption Priority |
|---|---:|
| DMA2 Stream7 | 5 |
| USART1 | 5 |
| SysTick | 15 |
| PendSV | 15 |

- DMA 完成回呼只更新傳輸結果、計數與 Task 狀態；實際切換交由後續 SysTick 與 PendSV。喚醒不代表零延遲恢復執行。
- UART DMA 由單一 Consumer / Logger Task 使用。其他 Task 不應在 DMA 期間對同一個 USART1 呼叫 `printf()` 或啟動另一筆傳輸。
- Queue 每個方向僅保存一個等待者；Mutex 與 Semaphore 也各只有一個等待者欄位，尚未提供一般化的多等待者佇列。
- Mutex 不支援遞迴上鎖與 priority inheritance；Task 不應重複取得自己已持有的鎖。
- Task 必須持續執行或自行阻塞，不支援直接從 Task entry 返回。
- Context Switch 尚未實作完整的浮點 Task 上下文保存，目前 Demo 使用整數訊息與格式化。
- Queue、同步等待與 DMA 等待尚未加入逾時；完成通知未到達時，等待者可能持續阻塞。
- `__WFI()` 用於 Idle 等待中斷；本專案未量測功耗，沒有 CPU 使用率或省電百分比的實測結果。

## 除錯紀錄與排查方式

### PendSV 優先級

將 PendSV 設為最低優先級，避免在較高優先級 ISR 中途進行 Task Context Switch。

### Mutex 所有權交接

修正只喚醒等待者、卻未轉移 `owner` 的問題，並以雙 Task 競爭測試檢查交接。

### 等待／喚醒時序

Queue 在同一段臨界區內檢查條件與登記等待者；DMA 在允許完成中斷處理前，先登記等待 Task。

### 終端沒有輸出

先確認已燒錄、Debug 未停在 `main()`、COM 與 UART 設定正確，再用 BOOT 訊息區分周邊初始化與 Task 執行階段。

BOOT 輸出失敗時，可在其後設定斷點並檢查 `boot_status`。

### 重複 BOOT

每次重新啟動都會重設計數，Tera Term 會保留前次輸出。若未主動重啟卻反覆出現 BOOT，應進一步檢查 reset 原因。

### WFI 與除錯連線

開發期間曾暫時用 `__NOP()` 排查，再恢復 `__WFI()` 並測試持續輸出與 Suspend / Resume。

先前失聯的根因尚未完全確認，不能只由現象歸因於 WFI。

## 後續改善

- 強化啟動階段的中斷保護，處理 `current_tcb` 已設定、首個 Task 上下文尚未完成啟動的窗口。
- 讓 Semaphore 保存、恢復原本的中斷狀態，並補上獨立驗證模式。
- 加入 DMA 與 IPC 等待逾時、錯誤恢復與回歸測試。
- 擴充通用 Task 管理與 FIFO 等待者佇列。
- 加入 Stack High-Water Mark、Stack Overflow 偵測與 Context Switch 延遲量測。

## 第三方元件

自行實作的 RTOS 核心位於 `Core/`；`Drivers/` 內的 STM32 HAL 與 CMSIS 使用各自附帶的授權條款，請保留原始版權與授權檔案。
