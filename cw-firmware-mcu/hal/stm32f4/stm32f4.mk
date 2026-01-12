# SPDX-License-Identifier: Apache-2.0 or CC0-1.0

CDEFS   += -DSTM32F405xx -DSTM32F4 -DSTM32 -DDEBUG
CPPDEFS += -DSTM32F405xx -DSTM32F4 -DSTM32 -DDEBUG

SRC +=  stm32f4_hal.c \
	stm32f4_sysmem.c \
	stm32f4xx_hal.c \
	stm32f4xx_hal_rng.c \
	stm32f4xx_hal_gpio.c \
	stm32f4xx_hal_rcc.c \
	stm32f4xx_hal_rcc_ex.c \
	stm32f4xx_hal_uart.c

ifneq ($(filter $(CDEFS), -DSTM32F415xx),)
  SRC += stm32f4xx_hal_cryp.c
endif

ASRC += stm32f4_startup.S
LDSCRIPT = $(FIRMWAREPATH)/hal/$(TARGET)/LinkerScript.ld

MCU_FLAGS = -mcpu=cortex-m4

#Output Format = Binary for this target
FORMAT = binary

#FPUUSE=AAAAA
ifeq ($(FPUUSE),)
  CFLAGS += -mthumb -mfloat-abi=soft -fmessage-length=0 -ffunction-sections
  CPPFLAGS += -mthumb -mfloat-abi=soft -fmessage-length=0 -ffunction-sections
  ASFLAGS += -mthumb -mfloat-abi=soft -fmessage-length=0 -ffunction-sections
else
  CDEFS += -DSTM32F4FPU
  CFLAGS += -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fmessage-length=0 -ffunction-sections
  CPPFLAGS += -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fmessage-length=0 -ffunction-sections
  ASFLAGS += -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fmessage-length=0 -ffunction-sections
endif

ifeq ($(MCU_CLK), INT)
  CFLAGS += -DUSE_INTERNAL_CLK
endif

ifeq ($(STM32F4_WLCSP), 1)
  CFLAGS += -DSTM32F4_WLCSP
endif
