#ifndef FREECANARD_H
#define FREECANARD_H

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>

#include <stdint.h>

#include "canard.h"
#include "o1heap.h"

#define FREECANARD_DEFAULT_PROCESSING_TASK_QUEUE_SIZE 10

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

/**
 * @brief Platform agnostic function that are used to send a frame over your 
 * interface of choice.
 * 
 * Nodes with redundant transports should replicate the frame into 
 * each of the transport interfaces.
 * 
 * The function need to satisfy the following requirements:
 * @param frame    Frame that should be sent.
 * @param can_fd   True if message should be transmitted as CAN-FD, false otherwise.
 * 
 * @return 0                Success.
 * 
 * @return <0               In case of error.
 */
typedef int8_t (*freecanard_platform_send)(
    const freecanard_frame_t *const frame,
    const bool can_fd);

/**
 * @brief Callback function which will be executed whenever a valid 
 * UAVCAN transfer have been received.
 * 
 * @note This callback function is called from normal context, i.e. a task, 
 * NOT from within an Interrupt Service Routine (ISR).
 * 
 * @param ins The instance for which the transfer have been processed.
 * @param transfer The received transfer. @note The transfer is invalidated 
 * after the point of return of this function.
 */
typedef void (*freecanard_on_transfer_received)(
    CanardInstance *ins,
    const CanardTransfer *const transfer);

/**
 * @brief Everybody's favorite delicious cookie.
 * 
 * This struct contains additionally necessary states and resources needed 
 * for a thread-safe FreeRTOS Libcanard interface. 
 * 
 * It is the user's responsibility to declare and provide an unique and
 * static cookie per @ref CanardInstance. 
 * 
 * @note When using Freecanard, the @ref user_reference pointer in @ref 
 * CanardInstance is reserved for storing the cookie, and is consequently NOT
 * available for use by the application. Instead, the application may access 
 * an application dedicated @ref user_reference_ contained within each cookie.
 * 
 * @warning The application must only interact with the cookie, except the
 * @ref user_reference_ through the public API for thread-safety and to avoid 
 * any data/race conditions.
 */
typedef struct
{
    /**
     * User pointer available for the application as the @ref CanardInstance
     * @ref user_reference is reserved for Freecanard's internal use. 
     * 
     * The @ref user reference may_ either be accessed directly by the 
     * application, or through the freecanard_get/set_user_reference API.
     * 
     * @warning It is the application's responsibility to ensure proper guarding
     * under concurrent access to the user_reference to avoid race conditions.
     */
    void *user_reference_;

    /**
     * These fields are for internal use only. 
     * No not access from the application.
     */
    O1HeapInstance *_o1heap;
    SemaphoreHandle_t _mutex;
    QueueHandle_t _processing_task_queue;
    freecanard_platform_send _platform_send;
    freecanard_on_transfer_received _on_transfer_received;
} freecanard_cookie_t;

/**
 * @brief Initialize the Freecanard driver.
 * 
 * @note A new task, dedicated for processing incoming UAVCAN frames for the
 * given CanardInstance, is created during the initialization. 
 * 
 * The task is released whenever a new frame is received, and is therefore
 * not released on a periodic basis by the scheduler. 
 * 
 * If any of the pointers are NULL, the behaviour is undefined.
 * 
 * @param ins The CanardInstance.
 * 
 * @param cooke A delicious cookie containing necessary resources.
 * 
 * @param canard_node_id The desired node ID.
 * 
 * @param mtu_bytes The maximum transmission unit (MTU) of the desired
 * transport-layer. Only the standard values CANARD_MTU_* should be used.
 * 
 * @param memory_pool Buffer used for dynamic heap allocation. 
 * @warning The buffer shall be aligned at @ref O1HEAP_ALIGNMENT
 * 
 * @param memory_pool_size Size of the memory pool in bytes.
 *
 * @param processing_task_priority Priority of the processing task.To avoid 
 * starvation by high-priority tasks waiting for UAVCAN transfers, the 
 * processing_task_priority should at least be equal to the highest priority 
 * task that depends on incoming UAVCAN messages.
 * 
 * @param processing_task_queue_size Size of queue for incoming frames to
 * the processing task. If unsure, use FREECANARD_DEFAULT_PROCESSING_TASK_QUEUE_SIZE
 * 
 * @param platform_send Platform agnostic function that are used to send a frame over your 
 * interface of choice.
 * 
 * @param on_transfer_received Callback function which is called whenever a valid
 * full UAVCAN transfer is received. 
 */
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
    freecanard_on_transfer_received on_transfer_received);

/**
 * @brief Set canard node ID.
 * 
 * @note This function is thread-safe.
 */
void freecanard_set_node_id(CanardInstance *const ins, const uint8_t node_id);

/**
 * @brief Set canard MTU bytes.
 * 
 * @note This function is thread-safe.
 */
void freecanard_set_mtu_bytes(CanardInstance *const ins, const size_t mtu_bytes);

/**
 * @brief Set application specific user_reference stored within the cookie. 
 * 
 * @warning It is the application's responsibility to ensure proper guarding
 * under concurrent access to the user_reference to avoid race conditions.
 */
void freecanard_set_user_reference(CanardInstance *const ins, void *user_reference);

/**
 * @brief Get canard node ID.
 */
uint8_t freecanard_get_node_id(CanardInstance *const ins);

/**
 * @brief Get canard MTU bytes.
 */
size_t freecanard_get_mtu_bytes(CanardInstance *const ins);

/**
 * @brief Get application specific user_reference stored within the cookie. 
 * 
 * @warning It is the application's responsibility to ensure proper guarding
 * under concurrent access to the user_reference to avoid race conditions.
 */
void *freecanard_get_user_reference(CanardInstance *const ins);

/**
 * @brief Create a new canard subscription.
 * 
 * See canardRxSubscribe in canard.h for more details.
 * 
 * @note This function is thread-safe, and may be called concurrently
 * from several tasks.
 * 
 * @warning This function shall not be called from an 
 * Interrupt Service Routine (ISR).
 */
int8_t freecanard_subscribe(
    CanardInstance *const ins,
    const CanardTransferKind transfer_kind,
    const CanardPortID port_id,
    const size_t extent,
    const CanardMicrosecond transfer_id_timeout_usec,
    CanardRxSubscription *const out_subscription);

/**
 * @brief Remove a canard subscription.
 * 
 * See canardRxUnsubscribe in canard.h for more details.
 * 
 * @note This function is thread-safe, and may be called concurrently
 * from several tasks.
 * 
 * @warning This function shall not be called from an 
 * Interrupt Service Routine (ISR).
 */
int8_t freecanard_unsubscribe(
    CanardInstance *const ins,
    const CanardTransferKind transfer_kind,
    const CanardPortID port_id);

/**
 * @brief Transmit an UAVCAN message.
 * 
 * @note This function is thread-safe, and may be called concurrently
 * from several tasks.
 * 
 * @warning This function shall not be called from an 
 * Interrupt Service Routine (ISR).
 * 
 * @param ins Canard instance
 * 
 * @param subject_id Subject-ID of the message.
 * 
 * @param priority The priority level of the message.
 * 
 * @param payload The serialized UAVCAN message. 
 * 
 * @param payload_len Length of serialized UAVCAN message.
 * 
 * @param transfer_id The transfer ID of the message.
 * @note The transfer ID must be persistent i.e. either declared in 
 * global scope, or as static!
 */
void freecanard_transmit_message(
    CanardInstance *const ins,
    uint16_t subject_id,
    CanardPriority priority,
    const void *payload,
    size_t payload_len,
    uint8_t *transfer_id);

/**
 * @brief Transmit an UAVCAN service request.
 * 
 * @note This function is thread-safe, and may be called concurrently
 * from several tasks.
 * 
 * @warning This function shall not be called from an 
 * Interrupt Service Routine (ISR).
 * 
 * @param ins Canard instance
 * 
 * @param destination_node_id ID of the node which should receive the request.
 * 
 * @param service_id Subject-ID of the service request.
 * 
 * @param priority The priority level of the service request.
 * 
 * @param payload The serialized UAVCAN service request. 
 * 
 * @param payload_len Length of serialized UAVCAN service request.
 * 
 * @param transfer_id The transfer ID of the service request.
 * @note The transfer ID must be persistent i.e. either declared in 
 * global scope, or as static!
 */
void freecanard_transmit_request(
    CanardInstance *const ins,
    uint8_t destination_node_id,
    uint8_t service_id,
    CanardPriority priority,
    const void *payload,
    size_t payload_len,
    uint8_t *transfer_id);

/**
 * @brief Transmit an UAVCAN service response.
 * 
 * @note This function is thread-safe, and may be called concurrently
 * from several tasks.
 * 
 * @warning This function shall not be called from an 
 * Interrupt Service Routine (ISR).
 * 
 * @param ins Canard instance
 * 
 * @param destination_node_id ID of the node which should receive the response.
 * 
 * @param service_id Subject-ID of the service response.
 * 
 * @param priority The priority level of the service response.
 * 
 * @param payload The serialized UAVCAN service response. 
 * 
 * @param payload_len Length of serialized UAVCAN service response.
 * 
 * @param transfer_id The transfer ID of the service response.
 * @note For service responces, the transfer_id shall be identical to the 
 * request, see UAVCAN specification for more details.
 */
void freecanard_transmit_response(
    CanardInstance *const ins,
    uint8_t destination_node_id,
    uint16_t service_id,
    CanardPriority priority,
    const void *payload,
    size_t payload_len,
    uint8_t *transfer_id);



/**
 * @brief process received CAN(-FD) frame.
 * 
 * This function enqueues the newly received CAN(-FD) frame for 
 * internal processing by libcanard according to the UAVCAN specification. 
 * 
 * If the frame completes a valid UAVCAN transfer, the provided @ref
 * freecanard_on_transfer_received callback function will be called with the
 * newly reassembled transfer. 
 * 
 * @note This function shall not be called from an 
 * Interrupt Service Routine (ISR).
 * 
 * @param ins Canard instance.
 * 
 * @param frame Received CAN(-FD) frame to be processed.
 * @note The MTU of the received frame is not limited to the MTU setting of
 * the local canard node, i.e. frames of any MTU (e.g. CAN-FD) may be accepted 
 * even if local canard node is configured for Classical CAN 2.0 only.
 * 
 * @param redundant_transport_index Transport index for which the frame is 
 * received. 
 * 
 * @param timeout The maximum amount of time in ticks the task should block
 * waiting for space to become available on the processing queue.
 */
void freecanard_process_received_frame(
    CanardInstance *const ins,
    const freecanard_frame_t *const frame,
    const uint8_t redundant_transport_index,
    TickType_t timeout);

    /**
 * @brief process received CAN(-FD) frame from ISR.
 * 
 * This function is similar in functionality to @ref
 * freecanard_process_received_frame only that it may be called from an
 * Interrupt Service Routine (ISR).
 * 
 * @note This function may be called from an Interrupt Service Routine (ISR).
 * 
 * @param ins Canard instance.
 * 
 * @param frame Received CAN(-FD) frame to be processed.
 * @note The MTU of the received frame is not limited to the MTU setting of
 * the local canard node, i.e. frames of any MTU (e.g. CAN-FD) may be accepted 
 * even if local canard node is configured for Classical CAN 2.0 only.
 * 
 * @param redundant_transport_index Transport index for which the frame is 
 * received. 
 */
void freecanard_process_received_frame_from_ISR(
    CanardInstance *const ins,
    const freecanard_frame_t *const frame,
    const uint8_t redundant_transport_index);

#endif // FREECANARD_H