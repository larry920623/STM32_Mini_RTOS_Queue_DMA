#ifndef OS_H
#define OS_H

#include <stdint.h>

#define TASK_READY   0
#define TASK_BLOCKED 1

typedef struct {
    uint32_t *psp;
    volatile uint32_t delay_ticks;
    volatile uint8_t state;
} TCB_t;

typedef struct {
    volatile uint8_t locked;
    TCB_t *owner;
    TCB_t *blocked_task;
} Mutex_t;

typedef struct {
    volatile int32_t count;
    TCB_t *blocked_task;
} Semaphore_t;

extern TCB_t tcb1;
extern TCB_t tcb2;
extern TCB_t tcb_idle;

extern TCB_t *current_tcb;
extern TCB_t *next_tcb;

void Task_Init(
    TCB_t *tcb,
    void (*task_entry)(void),
    uint32_t *stack,
    uint32_t stack_size
);

void OS_Start(void);
void OS_Delay(uint32_t ticks);
void OS_Schedule(void);
void OS_Tick(void);

#endif /* OS_H */
