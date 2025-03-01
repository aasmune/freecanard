#include "freecanard.h"
#include <task.h>

#include <string.h>

/**
 * @brief CAN(-FD) data frame.
 * 
 */
typedef struct
{
    uint32_t id;
    uint8_t data[CANARD_MTU_CAN_FD];
    size_t data_len;
} freecanard_frame_t;

static void freecanard_take_mutex(SemaphoreHandle_t *const mutex);
static void freecanard_give_mutex(SemaphoreHandle_t *const mutex);

static void *memory_allocate(CanardInstance *ins, size_t amount);
static void memory_free(CanardInstance *ins, void *pointer);

static void freecanard_to_canard_frame(const freecanard_frame_t *const can_frame, CanardFrame *const canard_frame);
static void canard_to_freecanard_frame(const CanardFrame *const canard_frame, freecanard_frame_t *const can_frame);

static void freecanard_transmit_transfer(CanardInstance *const ins, const CanardTransfer *const transfer);
static void freecanard_processing_task(void *canard_instance);

typedef struct
{
    const freecanard_frame_t frame_;
    CanardMicrosecond timestamp_usec;
    const uint8_t redundant_transport_index_;
} freecanard_frame_queue_item_t;

void freecanard_init(
    CanardInstance *const ins,
    freecanard_cookie_t *const cookie,
    const uint8_t canard_node_id,
    const size_t mtu_bytes,
    uint8_t *memory_pool,
    const size_t memory_pool_size,
    const UBaseType_t processing_task_priority,
    const UBaseType_t processing_task_size,
    freecanard_platform_send platform_send,
    freecanard_on_transfer_received on_transfer_received)
{
    cookie->_mutex = xSemaphoreCreateMutex();
    cookie->_processing_task_queue = xQueueCreate(processing_task_size, sizeof(freecanard_frame_queue_item_t));
    cookie->_platform_send = platform_send;
    cookie->_on_transfer_received = on_transfer_received;

    xTaskCreate(
        freecanard_processing_task,
        "FreecanardProcessingTask",
        configMINIMAL_STACK_SIZE,
        (void *)ins,
        processing_task_priority,
        NULL);

    freecanard_take_mutex(&cookie->_mutex);
    cookie->_o1heap = o1heapInit(
        memory_pool,
        memory_pool_size,
        NULL,  // We don't need mutexes as heap accesses ...
        NULL); // ... will always be performed within the freecanard mutexes
    *ins = canardInit(memory_allocate, memory_free);
    ins->node_id = canard_node_id;
    ins->mtu_bytes = mtu_bytes;
    ins->user_reference = (void *)cookie;
    freecanard_give_mutex(&cookie->_mutex);
}

void freecanard_set_node_id(CanardInstance *const ins, const uint8_t node_id)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->_mutex);

    ins->node_id = node_id;

    freecanard_give_mutex(&cookie->_mutex);
}

void freecanard_set_mtu_bytes(CanardInstance *const ins, const size_t mtu_bytes)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->_mutex);

    ins->mtu_bytes = mtu_bytes;

    freecanard_give_mutex(&cookie->_mutex);
}

void freecanard_set_user_reference(CanardInstance *const ins, void *user_reference)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;

    cookie->user_reference_ = user_reference;
}

uint8_t freecanard_get_node_id(CanardInstance *const ins)
{
    return ins->node_id;
}

size_t freecanard_get_mtu_bytes(CanardInstance *const ins)
{
    return ins->mtu_bytes;
}

void *freecanard_get_user_reference(CanardInstance *const ins)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    return cookie->user_reference_;
}

int8_t freecanard_subscribe(
    CanardInstance *const ins,
    const CanardTransferKind transfer_kind,
    const CanardPortID port_id,
    const size_t extent,
    const CanardMicrosecond transfer_id_timeout_usec,
    CanardRxSubscription *const out_subscription)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->_mutex);

    int8_t res = canardRxSubscribe(
        ins,
        transfer_kind,
        port_id,
        extent,
        transfer_id_timeout_usec,
        out_subscription);
    freecanard_give_mutex(&cookie->_mutex);
    return res;
}

int8_t freecanard_unsubscribe(
    CanardInstance *const ins,
    const CanardTransferKind transfer_kind,
    const CanardPortID port_id)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->_mutex);

    int8_t res = canardRxUnsubscribe(ins, transfer_kind, port_id);
    freecanard_give_mutex(&cookie->_mutex);
    return res;
}

void freecanard_transmit(CanardInstance *const ins, const CanardTransfer *const transfer)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    freecanard_take_mutex(&cookie->_mutex);

    freecanard_transmit_transfer(ins, transfer);

    freecanard_give_mutex(&cookie->_mutex);
}

void freecanard_process_received_frame(
    CanardInstance *const ins,
    const CanardFrame *const frame,
    const uint8_t redundant_transport_index,
    TickType_t timeout)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;

    freecanard_frame_t freecanard_frame;
    canard_to_freecanard_frame(frame, &freecanard_frame);

    freecanard_frame_queue_item_t queue_item = (freecanard_frame_queue_item_t){
        .frame_ = freecanard_frame,
        .timestamp_usec = frame->timestamp_usec,
        .redundant_transport_index_ = redundant_transport_index};
    xQueueSendToBack(cookie->_processing_task_queue, &queue_item, timeout);
}

void freecanard_process_received_frame_from_ISR(
    CanardInstance *const ins,
    const CanardFrame *const frame,
    const uint8_t redundant_transport_index)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;

    freecanard_frame_t freecanard_frame;
    canard_to_freecanard_frame(frame, &freecanard_frame);

    BaseType_t HigherPriorityTaskWoken = pdFALSE;
    freecanard_frame_queue_item_t queue_item = (freecanard_frame_queue_item_t){
        .frame_ = freecanard_frame,
        .timestamp_usec = frame->timestamp_usec,
        .redundant_transport_index_ = redundant_transport_index};

    xQueueSendToBackFromISR(
        cookie->_processing_task_queue,
        &queue_item,
        &HigherPriorityTaskWoken);
    portYIELD_FROM_ISR(HigherPriorityTaskWoken);
}

static void freecanard_processing_task(void *canard_instance)
{
    CanardInstance *ins = (CanardInstance *)canard_instance;
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;

    freecanard_frame_queue_item_t queue_item;

    while (1)
    {
        // Block until new frame is received
        bool ret = xQueueReceive(cookie->_processing_task_queue, &queue_item, portMAX_DELAY);

        if (ret != pdTRUE)
        {
            // This should not happen, if it does drop the frame
            continue;
        }

        CanardFrame canard_frame;
        freecanard_to_canard_frame(&queue_item.frame_, &canard_frame);
        canard_frame.timestamp_usec = queue_item.timestamp_usec;

        freecanard_take_mutex(&cookie->_mutex);
        CanardTransfer transfer;
        int8_t res = canardRxAccept(
            ins,
            &canard_frame,
            queue_item.redundant_transport_index_,
            &transfer);

        if (res == 1)
        {
            if (cookie->_on_transfer_received)
            {
                cookie->_on_transfer_received(ins, &transfer);
            }
            ins->memory_free(ins, (void *)transfer.payload);
        }
        else
        {
            // An error has occured
            freecanard_give_mutex(&cookie->_mutex);
            continue;
        }
        freecanard_give_mutex(&cookie->_mutex);
    }
}

/* Private helper functions */

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
    return o1heapAllocate(cookie->_o1heap, amount);
}

static void memory_free(CanardInstance *ins, void *pointer)
{
    freecanard_cookie_t *cookie = (freecanard_cookie_t *)ins->user_reference;
    o1heapFree(cookie->_o1heap, pointer);
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
    for (const CanardFrame *txf = NULL; (txf = canardTxPeek(ins)) != NULL;) // Look at the top of the TX queue.
    {
        bool can_fd = ins->mtu_bytes == CANARD_MTU_CAN_FD ? true : false;
        const int16_t res = cookie->_platform_send(txf, can_fd); // Send the frame.
        if (res)
        {
            break; // If the driver is busy, break and retry later.
        }
        canardTxPop(ins);                          // Remove the frame from the queue after it's transmitted.
        ins->memory_free(ins, (CanardFrame *)txf); // Deallocate the dynamic memory afterwards.
    }
}