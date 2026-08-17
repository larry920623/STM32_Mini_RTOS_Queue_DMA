# Mini_RTOS: A Custom RTOS for ARM Cortex-M4

## Overview
**Mini_RTOS** is a lightweight, real-time operating system built entirely from scratch for the ARM Cortex-M4 architecture, without relying on third-party libraries like FreeRTOS. This project demonstrates deep low-level system programming, including hardware-level context switching, memory layout management, and inter-process communication (IPC).

## Hardware & Development Environment
* **MCU:** STM32F407VETx (ARM Cortex-M4 @ 168MHz)
* **IDE:** STM32CubeIDE 1.16.0 (C & Naked Assembly)
* **Tools:** ST-Link V2 (SWD), CP2102 (USB to TTL, 115200 bps)

## Key Features & Core Mechanisms

* **Hardware-level Context Switching:** 
  Utilizes the `PendSV` handler and `__attribute__((naked))` assembly to manually manipulate the Process Stack Pointer (PSP) and push/pop CPU registers (`R4-R11`), achieving seamless task switching.
* **Non-blocking Scheduler:** 
  A SysTick-driven scheduler (`OS_Schedule`) that completely replaces busy-waiting (`HAL_Delay`) with a yielding mechanism (`OS_Delay`). It includes an **Idle Task** that executes the `WFI` (Wait For Interrupt) instruction for power efficiency.
* **Mutex (Mutual Exclusion):** 
  Safely protects shared resources (e.g., UART `printf`) by implementing a blocking state machine, preventing task starvation and hardware-level lockups (`HAL_BUSY`).
* **Semaphore (Task Synchronization):** 
  Implements a robust signaling mechanism allowing tasks to wait in a zero-CPU-cost blocked state until awakened by events from other tasks.

## Demo: Inter-Process Communication (IPC)
The following output demonstrates successful task synchronization using Semaphores and Mutexes. Task 1 enters a blocked state waiting for a signal, while Task 2 simulates workloads and sends signals to instantly wake Task 1.

![Semaphore Task Synchronization](https://hackmd.io/_uploads/r1DRAnxwfl.png)

## Troubleshooting & Debugging Highlights

Building this RTOS involved solving several complex low-level system bugs:

1. **HardFault via Interrupt Preemption:**
   * *Issue:* The system crashed due to `PendSV` interrupting the `SysTick` handler, corrupting the interrupt context.
   * *Solution:* Adjusted the NVIC priority of `PendSV` to the lowest possible level (15), ensuring context switches only occur when no other critical hardware interrupts are active.
2. **Invisible Stack Overflow:**
   * *Issue:* Unpredictable crashes after a few context switches. 
   * *Solution:* Identified that the standard C library's `printf` consumed excessive memory (1~2KB). Increased each task's stack size to 2048 Bytes, successfully preventing memory corruption (TCB and register states overwriting).
3. **ST-Link Connection Failure in Deep Sleep:**
   * *Issue:* The IDE threw a `Target is not responding` error because the MCU was spending 99% of its time in the `WFI` deep-sleep state via the Idle Task.
   * *Solution:* Used hardware reset interception—holding the physical reset button and releasing it precisely upon IDE connection—to flash the chip before it enters deep sleep.

---
*Built with passion for embedded systems and low-level software engineering.*