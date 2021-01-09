#include "uavcan.h"

#include "console.h"
#include <stdio.h>

#include "uavcan/node/Heartbeat_1_0.h"

#define NODE_ID 1

#define MEMORY_POOL_BUS_0_SIZE 8196
static uint8_t memory_pool_bus_0[MEMORY_POOL_BUS_0_SIZE] __attribute__ ((aligned (O1HEAP_ALIGNMENT)));
static freecanard_cookie_t cookie_0;

CanardRxSubscription heartbeat_subscription;

static int8_t send(const freecanard_frame_t *const frame, const bool can_fd);
static void uavcan_on_transfer_received(CanardInstance *ins, const CanardTransfer *const transfer);

void uavcan_init()
{
    freecanard_init(
        &bus_0,
        &cookie_0,
        NODE_ID,
        CANARD_MTU_CAN_FD,
        (uint8_t *)memory_pool_bus_0,
        MEMORY_POOL_BUS_0_SIZE,
        tskIDLE_PRIORITY,
        FREECANARD_DEFAULT_PROCESSING_TASK_QUEUE_SIZE,
        send,
        uavcan_on_transfer_received);

    freecanard_subscribe(
        &bus_0,
        CanardTransferKindMessage,
        uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_,
        uavcan_node_Heartbeat_1_0_EXTENT_BYTES_,
        CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC,
        &heartbeat_subscription);
}

static int8_t send(const freecanard_frame_t *const frame, const bool can_fd)
{
    printf("Sending msg: \n");
    printf("ID: %x\n", frame->id);
    printf("data: ");
    for (int i = 0; i < frame->data_len; i++)
    {
        printf("%x, ", frame->data[i]);
    }
    printf("\n\n");
    fflush(stdout);
}

static void uavcan_on_transfer_received(CanardInstance *ins, const CanardTransfer *const transfer)
{
    if (transfer->transfer_kind == CanardTransferKindMessage)
    {
        // Fill in which broadcasts to accept below
        switch (transfer->port_id)
        {
        case uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_:
        {
            uavcan_node_Heartbeat_1_0 heartbeat;
            size_t length = transfer->payload_size;
            uavcan_node_Heartbeat_1_0_deserialize_(&heartbeat, transfer->payload, &length);

            printf("Received following heartbeat message: \n");
            printf("Health: %d\n", heartbeat.health.value);
            printf("Mode: %d\n", heartbeat.mode.value);
            printf("Uptime: %d\n\n", heartbeat.uptime);
        }
        }
    }
}