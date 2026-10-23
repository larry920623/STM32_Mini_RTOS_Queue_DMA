#include "os.h"
#include "stm32f4xx.h"
#include <stddef.h>

TCB_t *current_tcb = NULL;
TCB_t *next_tcb = NULL;

void OS_Delay(uint32_t ticks) {
    current_tcb->delay_ticks = ticks;
    OS_Schedule();
}
__attribute__((naked)) void OS_Start(void) {
    __asm volatile(
        "LDR R1, =current_tcb \n"
        "LDR R2, [R1] \n"
        "LDR R0, [R2] \n"

        "LDMIA R0!, {R4-R11} \n"
        "MSR PSP, R0 \n"

        "MOV R0, #2 \n"
        "MSR CONTROL, R0 \n"
        "ISB \n"

        "POP {R0-R3} \n"
        "POP {R12, LR} \n"
        "POP {R1, R2} \n"
        "BX R1 \n"
    );
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile(
        "MRS R0, PSP \n"
        "STMDB R0!, {R4-R11} \n"

        "LDR R1, =current_tcb \n"
        "LDR R2, [R1] \n"
        "STR R0, [R2] \n"

        "LDR R3, =next_tcb \n"
        "LDR R4, [R3] \n"
        "STR R4, [R1] \n"

        "LDR R0, [R4] \n"
        "LDMIA R0!, {R4-R11} \n"
        "MSR PSP, R0 \n"

        "BX LR \n"
    );
}

void Task_Init(TCB_t *tcb, void (*task_entry)(void), uint32_t *stack, uint32_t stack_size) {
	tcb->delay_ticks = 0;
	tcb->state = TASK_READY;
    uint32_t *sp = &stack[stack_size];
    sp -= 16;
    sp[15] = 0x01000000;
    sp[14] = (uint32_t)task_entry;
    sp[13] = 0x00000000;
    tcb->psp = sp;
}

void OS_Schedule(void) {
    if (current_tcb == &tcb1) {
        if (tcb2.delay_ticks == 0 && tcb2.state == TASK_READY) next_tcb = &tcb2;
        else if (tcb1.delay_ticks == 0 && tcb1.state == TASK_READY) next_tcb = &tcb1;
        else next_tcb = &tcb_idle;
    } else if (current_tcb == &tcb2) {
        if (tcb1.delay_ticks == 0 && tcb1.state == TASK_READY) next_tcb = &tcb1;
        else if (tcb2.delay_ticks == 0 && tcb2.state == TASK_READY) next_tcb = &tcb2;
        else next_tcb = &tcb_idle;
    } else {
        if (tcb1.delay_ticks == 0 && tcb1.state == TASK_READY) next_tcb = &tcb1;
        else if (tcb2.delay_ticks == 0 && tcb2.state == TASK_READY) next_tcb = &tcb2;
        else next_tcb = &tcb_idle;
    }

    if (current_tcb != next_tcb) {
        *(volatile uint32_t *)0xE000ED04 |= (1 << 28);
    }
}

void OS_Tick(void)
{
    if (current_tcb == NULL) {
        return;
    }

    if (tcb1.delay_ticks > 0U) {
        tcb1.delay_ticks--;
    }

    if (tcb2.delay_ticks > 0U) {
        tcb2.delay_ticks--;
    }

    OS_Schedule();
}

