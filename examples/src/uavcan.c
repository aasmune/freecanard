#include "uavcan.h"

#include "console.h"
#include <stdio.h>

#define MEMORY_POOL_BUS_0_SIZE 8196
static uint8_t memory_pool_bus_0[MEMORY_POOL_BUS_0_SIZE];
static freecanard_cookie_t cookie_0;

static int8_t send(const freecanard_frame_t *const frame, const bool can_fd);

void uavcan_init()
{
    freecanard_init(
        &bus_0,
        &cookie_0,
        1,
        (uint8_t *)memory_pool_bus_0,
        MEMORY_POOL_BUS_0_SIZE,
        tskIDLE_PRIORITY,
        10,
        send);
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
    printf("\n");
    fflush(stdout);
}