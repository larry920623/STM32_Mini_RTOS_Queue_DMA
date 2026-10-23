#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include "os.h"

#define MSG_QUEUE_CAPACITY 4U

typedef struct {
    uint32_t seq;
    uint32_t value;
} Message_t;

typedef enum {
    QUEUE_OK = 0,
    QUEUE_FULL,
    QUEUE_EMPTY
} QueueStatus_t;

typedef struct {
    Message_t buffer[MSG_QUEUE_CAPACITY];

    uint32_t head;
    uint32_t tail;
    uint32_t count;

    TCB_t *blocked_receiver;
    TCB_t *blocked_sender;

    uint32_t receive_wait_count;
    uint32_t send_wait_count;
} MsgQueue_t;

void Queue_Init(MsgQueue_t *q);

QueueStatus_t Queue_TrySend(
    MsgQueue_t *q,
    const Message_t *msg
);

QueueStatus_t Queue_TryReceive(
    MsgQueue_t *q,
    Message_t *msg
);

void Queue_Receive(
    MsgQueue_t *q,
    Message_t *msg
);

void Queue_Send(
    MsgQueue_t *q,
    const Message_t *msg
);

#endif /* MSG_QUEUE_H */
