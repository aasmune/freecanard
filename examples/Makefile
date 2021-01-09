CC := gcc
BIN := freecanard_demo

BUILD_DIR := build

FREERTOS_DIR_REL := ./FreeRTOS
FREERTOS_DIR := $(abspath $(FREERTOS_DIR_REL))

SOURCE_DIR_REL := ./src
SOURCE_DIR := $(abspath $(SOURCE_DIR_REL))

FREECANARD_DIR_REL := ../freecanard
FREECANARD_DIR := $(abspath $(FREECANARD_DIR_REL))

LIBCANARD_DIR_REL := ./libcanard/libcanard
LIBCANARD_DIR := $(abspath $(LIBCANARD_DIR_REL))

O1HEAP_DIR_REL := ./o1heap/o1heap
O1HEAP_DIR := $(abspath $(O1HEAP_DIR_REL))

DSDL_DIR := $(abspath $(SOURCE_DIR)/dsdl)

INCLUDE_DIRS := -I${SOURCE_DIR}
INCLUDE_DIRS += -I${FREERTOS_DIR}/Source/include
INCLUDE_DIRS += -I${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix
INCLUDE_DIRS += -I${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix/utils
INCLUDE_DIRS += -I${FREERTOS_DIR}/common/include
INCLUDE_DIRS += -I${LIBCANARD_DIR}
INCLUDE_DIRS += -I${O1HEAP_DIR}
INCLUDE_DIRS += -I${FREECANARD_DIR}
INCLUDE_DIRS += -I${DSDL_DIR}

SOURCE_FILES := $(wildcard ${SOURCE_DIR}/*.c)
SOURCE_FILES += $(wildcard ${FREECANARD_DIR}/*.c)
SOURCE_FILES += $(wildcard ${LIBCANARD_DIR}/*.c)
SOURCE_FILES += $(wildcard ${O1HEAP_DIR}/*.c)
SOURCE_FILES += $(wildcard ${FREERTOS_DIR}/Source/*.c)


# Memory manager (use malloc() / free() )
SOURCE_FILES += ${FREERTOS_DIR}/Source/portable/MemMang/heap_3.c
# posix port
SOURCE_FILES += ${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
SOURCE_FILES += ${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix/port.c

# Demo library.
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/AbortDelay.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/BlockQ.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/blocktim.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/countsem.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/death.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/dynamic.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/flop.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/GenQTest.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/integer.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/IntSemTest.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/MessageBufferAMP.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/PollQ.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/QPeek.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/QueueOverwrite.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/QueueSet.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/QueueSetPolling.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/recmutex.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/semtest.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/StaticAllocation.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/StreamBufferInterrupt.c
SOURCE_FILES += ${FREERTOS_DIR}/common/Minimal/TaskNotify.c

CFLAGS := -ggdb3 -O0 -DprojCOVERAGE_TEST=0 -D_WINDOWS_ -D"NUNAVUT_ASSERT(x)=assert(x)" -pedantic
LDFLAGS := -ggdb3 -O0 -pthread -lpcap -pedantic

OBJ_FILES = $(SOURCE_FILES:%.c=$(BUILD_DIR)/%.o)

DEP_FILE = $(OBJ_FILES:%.o=%.d)

${BIN} : $(BUILD_DIR)/$(BIN)

${BUILD_DIR}/${BIN} : ${OBJ_FILES}
	-mkdir -p ${@D}
	$(CC) $^ $(CFLAGS) $(INCLUDE_DIRS) ${LDFLAGS} -o $@


-include ${DEP_FILE}

${BUILD_DIR}/%.o : %.c	
	-mkdir -p $(@D)
	$(CC) $(CFLAGS) ${INCLUDE_DIRS} -MMD -c $< -o $@

.PHONY: clean

clean:
	-rm -rf $(BUILD_DIR)
