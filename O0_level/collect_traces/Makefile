# use with
# make TESTFILE=fpr_smallint_and_FFT_f_one_layer build

OPT?=-O0
TARGET?=stm32f4
SS_VER=SS_VER_1_1

SRC += $(TESTFILE).c
SRC += fpr.c
CFLAGS += -I../../PQClean/common -I../../PQClean/crypto_sign/falcon-512/clean/
VPATH += ../../PQClean/crypto_sign/falcon-512/clean/

FIRMWAREPATH = ../../cw-firmware-mcu
include $(FIRMWAREPATH)/simpleserial/simpleserial.mk
include $(FIRMWAREPATH)/tracewhisperer/tracewhisperer.mk
include $(FIRMWAREPATH)/config.mk
include $(FIRMWAREPATH)/rules.mk
