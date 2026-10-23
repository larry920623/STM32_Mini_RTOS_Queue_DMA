#include "main.h"
#include "os_sync.h"

void Sem_Init(Semaphore_t *s, int32_t initial_count) {
    s->count = initial_count;
    s->blocked_task = NULL;
}

void Sem_Wait(Semaphore_t *s) {
    __disable_irq();

    if (s->count > 0) {
        s->count--;
    } else {
        current_tcb->state = TASK_BLOCKED;
        s->blocked_task = current_tcb;
        OS_Schedule();
    }

    __enable_irq();
}

void Sem_Signal(Semaphore_t *s) {
    __disable_irq();

    s->count++;

    if (s->blocked_task != NULL) {
        s->blocked_task->state = TASK_READY;
        s->blocked_task = NULL;
        s->count--;

        OS_Schedule();
    }

    __enable_irq();
}

void Mutex_Init(Mutex_t *m)
{
    m->locked = 0U;
    m->owner = NULL;
    m->blocked_task = NULL;
}

void Mutex_Lock(Mutex_t *m)
{
    uint32_t saved_primask = __get_PRIMASK();

    __disable_irq();

    if (m->locked == 0U) {
        m->locked = 1U;
        m->owner = current_tcb;
    } else {
        current_tcb->state = TASK_BLOCKED;
        m->blocked_task = current_tcb;

        OS_Schedule();
    }

    __set_PRIMASK(saved_primask);

    __DSB();
    __ISB();
}

void Mutex_Unlock(Mutex_t *m)
{
    uint32_t saved_primask = __get_PRIMASK();

    __disable_irq();

    if (m->owner == current_tcb) {
        if (m->blocked_task != NULL) {
            TCB_t *waiter = m->blocked_task;

            m->blocked_task = NULL;
            m->locked = 1U;
            m->owner = waiter;

            waiter->state = TASK_READY;

            OS_Schedule();
        } else {
            m->locked = 0U;
            m->owner = NULL;
        }
    }

    __set_PRIMASK(saved_primask);

    __DSB();
    __ISB();
}
