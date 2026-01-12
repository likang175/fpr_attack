# SPDX-License-Identifier: Apache-2.0 or CC0-1.0

SRC +=  stm32f3_hal.c \
	stm32f3_sysmem.c \
	stm32f3xx_hal.c \
	stm32f3xx_hal_cortex.c \
	stm32f3xx_hal_gpio.c \
	stm32f3xx_hal_rcc.c \
	stm32f3xx_hal_uart.c

ifeq ($(DEMO),SECCAN)
	SRC += stm32f3_hal_seccan.c stm32f3xx_hal_adc_ex.c stm32f3xx_hal_tim.c stm32f3xx_hal_can.c
endif

ASRC += stm32f3_startup.S
LDSCRIPT = $(FIRMWAREPATH)/hal/$(TARGET)/LinkerScript.ld

MCU_FLAGS = -mcpu=cortex-m4

ifeq ($(MCU_CLK), INT)
  CFLAGS += -DUSE_INTERNAL_CLK
endif

#Output Format = Binary for this target
FORMAT = binary

CFLAGS += -mthumb -mfloat-abi=soft -fmessage-length=0 -ffunction-sections
CPPFLAGS += -mthumb -mfloat-abi=soft -fmessage-length=0 -ffunction-sections
ASFLAGS += -mthumb -mfloat-abi=soft -fmessage-length=0 -ffunction-sections

CDEFS += -DSTM32F303x8 -DSTM32F3 -DSTM32 -DDEBUG
CPPDEFS += -DSTM32F303x8 -DSTM32F3 -DSTM32 -DDEBUG
