#include "freecanard.h"

#include <string.h>

/* Converts a time in ticks to a time in microseconds. */
#define pdTICKS_TO_US(xTimeInTicks)                                 \
    ((TickType_t)(((TickType_t)1e+6 * (TickType_t)(xTimeInTicks)) / \
                  (TickType_t)(configTICK_RATE_HZ)))

static inline TickType_t freecanard_get_us();

static void freecanard_take_mutex(SemaphoreHandle_t *const mutex);
static void freecanard_give_mutex(SemaphoreHandle_t *const mutex);

static void *memory_allocate(CanardInstance *ins, size_t amount);
static void memory_free(CanardInstance *ins, void *pointer);

static void freecanard_to_canard_frame(const freecanard_frame_t *const can_frame, CanardFrame *const canard_frame);
static void canard_to_freecanard_frame(const CanardFrame *const canard_frame, freecanard_frame_t *const can_frame);

static void freecanard_transmit_transfer(CanardInstance *const ins, const CanardTransfer *const transfer);

void freecanard_init(
    CanardInstance *const ins,
    freecanard_cookie_t *const cookie,
    const uint8_t node_id,
    uint8_t *memory_pool,
    const size_t memory_pool_size,
    const UBaseType_t processing_task_priority,
    const UBaseType_t processing_task_size,
    freecanard_platform_send platform_send)
{
    cookie->mutex_ = xSemaphoreCreateMutex();
    cookie->processing_task_queue_ = xQueueCreate(processing_task_size, sizeof(freecanard_frame_t));
    cookie->platformSend_ = platform_send;

    freecanard_take_mutex(&cookie->mutex_);
    cookie->o1heap_ = o1heapInit(
        memory_pool,
        memory_pool_size,
        NULL,  // We don't need mutexes as heap accesses ...
        NULL); // ... will always be performed within the freecanard mutexes
    *ins = canardInit(memory_allocate, memory_free);
    ins->node_id = node_id;
    ins->user_reference = (void *)cookie;
    freecanard_give_mutex(&cookie->mutex_);
}

void freecanard_transmit_subject(
    CanardInstance *const ins,
    uint16_t message_id,
    CanardPriority priority,
    const void *payload,
    size_t payload_len,
    uint8_t *transfer_id)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->mutex_);

    CanardTransfer transfer;
    transfer.timestamp_usec = freecanard_get_us();
    transfer.priority = (CanardPriority)priority;
    transfer.transfer_kind = CanardTransferKindMessage;
    transfer.port_id = message_id;
    transfer.remote_node_id = CANARD_NODE_ID_UNSET;
    transfer.transfer_id = (*transfer_id)++;
    transfer.payload = payload;
    transfer.payload_size = payload_len;
    freecanard_transmit_transfer(ins, &transfer);

    freecanard_give_mutex(&cookie->mutex_);
}

void freecanard_transmit_request(
    CanardInstance *const ins,
    uint8_t destination_node_id,
    uint8_t request_id,
    CanardPriority priority,
    const void *payload,
    size_t payload_len,
    uint8_t *transfer_id)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->mutex_);

    CanardTransfer transfer;
    transfer.timestamp_usec = freecanard_get_us();
    transfer.priority = (CanardPriority)priority;
    transfer.transfer_kind = CanardTransferKindRequest;
    transfer.port_id = request_id;
    transfer.remote_node_id = destination_node_id;
    transfer.transfer_id = (*transfer_id)++;
    transfer.payload = payload;
    transfer.payload_size = payload_len;
    freecanard_transmit_transfer(ins, &transfer);

    freecanard_give_mutex(&cookie->mutex_);
}

void freecanard_transmit_response(
    CanardInstance *const ins,
    uint8_t destination_node_id,
    uint16_t response_id,
    CanardPriority priority,
    const void *payload,
    size_t payload_len,
    uint8_t *transfer_id)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->mutex_);

    CanardTransfer transfer;
    transfer.timestamp_usec = freecanard_get_us();
    transfer.priority = (CanardPriority)priority;
    transfer.transfer_kind = CanardTransferKindResponse;
    transfer.port_id = response_id;
    transfer.remote_node_id = destination_node_id;
    transfer.transfer_id = (*transfer_id)++;
    transfer.payload = payload;
    transfer.payload_size = payload_len;
    freecanard_transmit_transfer(ins, &transfer);

    freecanard_give_mutex(&cookie->mutex_);
}

/* Private helper functions */

static inline TickType_t freecanard_get_us()
{
    return pdTICKS_TO_US(xTaskGetTickCount());
}

static void freecanard_take_mutex(SemaphoreHandle_t *const mutex)
{

    xSemaphoreTake(*mutex, portMAX_DELAY);
}

static void freecanard_give_mutex(SemaphoreHandle_t *const mutex)
{
    xSemaphoreGive(*mutex);
}

static void *memory_allocate(CanardInstance *ins, size_t amount)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    return o1heapAllocate(cookie->o1heap_, amount);
};

static void memory_free(CanardInstance *ins, void *pointer)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    return o1heapFree(cookie->o1heap_, pointer);
}

static void freecanard_to_canard_frame(const freecanard_frame_t *const freecanardFrame, CanardFrame *const canardFrame)
{
    canardFrame->extended_can_id = freecanardFrame->id;
    canardFrame->payload_size = freecanardFrame->data_len;
    canardFrame->payload = (const void *)freecanardFrame->data;
}

static void canard_to_freecanard_frame(const CanardFrame *const canardFrame, freecanard_frame_t *const freecanardFrame)
{
    freecanardFrame->id = canardFrame->extended_can_id;
    freecanardFrame->data_len = canardFrame->payload_size;
    memcpy(freecanardFrame->data, canardFrame->payload, canardFrame->payload_size);
}

/**
 * Transmit transfer by first pushing the transfer to the TX queue, before 
 * dequeueing all frames, and transmit them one by one.
 *
 * Note: This function is NOT thread safe.
 *
 * @param ins   Canard instance to transmit all frames from.
 */
static void freecanard_transmit_transfer(CanardInstance *const ins, const CanardTransfer *const transfer)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;

    canardTxPush(ins, transfer);
    freecanard_frame_t frame;
    for (const CanardFrame *txf = NULL; (txf = canardTxPeek(ins)) != NULL;) // Look at the top of the TX queue.
    {
        bool can_fd = ins->mtu_bytes == CANARD_MTU_CAN_FD ? true : false;
        canard_to_freecanard_frame(txf, &frame);
        const int16_t res = cookie->platformSend_(&frame, can_fd); // Send the frame.
        if (res)
        {
            break; // If the driver is busy, break and retry later.
        }
        canardTxPop(ins);                          // Remove the frame from the queue after it's transmitted.
        ins->memory_free(ins, (CanardFrame *)txf); // Deallocate the dynamic memory afterwards.
    }
}