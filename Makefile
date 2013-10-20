CROSS_COMPILE=arm-none-eabi-
QEMU_STM32 ?= ../qemu_stm32/arm-softmmu/qemu-system-arm

ARCH=CM3
VENDOR=ST
PLAT=STM32F10x
CODEBASE= freertos
CMSIS_LIB_DIR=$(CODEBASE)/libraries/CMSIS/$(ARCH)
STM32_LIB=$(CODEBASE)/libraries/STM32F10x_StdPeriph_Driver

CMSIS_PLAT_SRC_DIR = $(CMSIS_LIB_DIR)/DeviceSupport/$(VENDOR)/$(PLAT)

FREERTOS_SRC_DIR = $(CODEBASE)/libraries/FreeRTOS
FREERTOS_INC = $(FREERTOS_SRC_DIR)/include/
FREERTOS_PORT_INC = $(FREERTOS_SRC_DIR)/portable/GCC/ARM_$(ARCH)/

CMSIS_SRCS = \
		$(CMSIS_LIB_DIR)/CoreSupport/core_cm3.c \
		$(CMSIS_PLAT_SRC_DIR)/system_stm32f10x.c \
		$(CMSIS_PLAT_SRC_DIR)/startup/gcc_ride7/startup_stm32f10x_md.s

STM32_SRCS = \
		$(STM32_LIB)/src/stm32f10x_rcc.c \
		$(STM32_LIB)/src/stm32f10x_gpio.c \
		$(STM32_LIB)/src/stm32f10x_usart.c \
		$(STM32_LIB)/src/stm32f10x_exti.c \
		$(STM32_LIB)/src/misc.c

FREERTOS_SRC =  \
		$(FREERTOS_SRC_DIR)/croutine.c \
		$(FREERTOS_SRC_DIR)/list.c \
		$(FREERTOS_SRC_DIR)/queue.c \
		$(FREERTOS_SRC_DIR)/tasks.c \
		$(FREERTOS_SRC_DIR)/portable/GCC/ARM_CM3/port.c \
		./heap_ww.c

SRCS= \
		$(CMSIS_SRCS)   \
		$(STM32_SRCS)   \
		$(FREERTOS_SRC) \
		stm32_p103.c    \
		romfs.c         \
		hash-djb2.c     \
		filesystem.c    \
		fio.c           \
		osdebug.c       \
		string.c        \
		util.c          \
		main.c         \
		shell.c         \
		tryit.c        \

INCS= \
		-I. -I$(FREERTOS_INC) -I$(FREERTOS_PORT_INC) \
		-I$(CODEBASE)/libraries/CMSIS/CM3/CoreSupport \
		-I$(CODEBASE)/libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
		-I$(CODEBASE)/libraries/STM32F10x_StdPeriph_Driver/inc

HEADERS= \
		filesystem.h     \
		fio.h            \
		FreeRTOSConfig.h \
		hash-djb2.h      \
		osdebug.h        \
		romfs.h          \
		stm32f10x_conf.h \
		stm32_p103.h     \
		string.h         \
		util.h           \
		shell.h

CFLAGS = \
		-fno-common -O0 \
		-mcpu=cortex-m3 -mthumb \
		-Wall -std=c99 -pedantic \
		-fno-builtin-printf

# Options and actions
BUILD_TYPE    ?= DEBUG
USE_UNIT_TEST ?= NO
USE_SEMIHOST  ?= YES

ifeq ($(BUILD_TYPE), DEBUG)
	CFLAGS += -gdwarf-2 -g3

	ifeq ($(USE_UNIT_TEST), YES)
		CFLAGS  +=-DRT_TEST
		SRCS    += unit_tests.c
		HEADERS += unit_tests.h
	endif
endif

ifeq ($(USE_SEMIHOST),YES)
	CFLAGS  += -DUSE_SEMIHOST
	SRCS    += host.c
	HEADERS += host.h
	QEMU_SMH_PARAM_SUFFIX=-semihosting
endif

# Trick to get obj file name
# Filter out path -> Renname *.c to *.o -> Rename *.s to *.o
SRC_STRIP_PATH=$(notdir $(SRCS))
C_OBJS=$(patsubst %.c,%.o,$(SRC_STRIP_PATH))
OBJS=$(patsubst %.s,%.o,$(C_OBJS))

all: main.bin

objs: $(SRCS) $(HEADERS)
	$(CROSS_COMPILE)gcc $(CFLAGS) $(INCS) -c $(SRCS)

main.bin: objs test-romfs.o 
	$(CROSS_COMPILE)ld -Tmain.ld -nostartfiles -o main.elf $(OBJS)
	$(CROSS_COMPILE)objcopy -Obinary main.elf main.bin
	$(CROSS_COMPILE)objdump -S main.elf > main.list


mkromfs:
	gcc -o mkromfs mkromfs.c

CPU=arm
TARGET_FORMAT = elf32-littlearm
TARGET_OBJCOPY_BIN = $(CROSS_COMPILE)objcopy -I binary -O $(TARGET_FORMAT) --binary-architecture $(CPU)

test-romfs.o: mkromfs
	./mkromfs -d test-romfs test-romfs.bin
	$(TARGET_OBJCOPY_BIN) --prefix-sections '.romfs' test-romfs.bin test-romfs.o


qemu: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 -kernel main.bin  \
		-monitor tcp:localhost:4444,server,nowait \
		$(QEMU_SMH_PARAM_SUFFIX)

ifeq ($(BUILD_TYPE), DEBUG)
qemudbg: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 -gdb tcp::3333 -S \
		$(QEMU_SMH_PARAM_SUFFIX) -kernel main.bin \
		-monitor tcp:localhost:4444,server,nowait
endif

clean:
	rm -f *.o *.elf *.bin *.list mkromfs
