#ifndef OS_SYNC_H
#define OS_SYNC_H

#include "os.h"

/* Semaphore */
void Sem_Init(Semaphore_t *s, int32_t initial_count);
void Sem_Wait(Semaphore_t *s);
void Sem_Signal(Semaphore_t *s);

/* Mutex */
void Mutex_Init(Mutex_t *m);
void Mutex_Lock(Mutex_t *m);
void Mutex_Unlock(Mutex_t *m);

#endif /* OS_SYNC_H */
