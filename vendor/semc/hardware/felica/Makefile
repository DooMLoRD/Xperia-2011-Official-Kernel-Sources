#
# Makefile for FeliCa kernel module
#
# Copyright 2010 Sony Ericsson Mobile Communications Japan Inc.
# All rights, including trade secret rights, reserved.
#

# environment variables
CROSS_COMPILE  := arm-eabi-
ARCH           := arm
PWD := $(shell pwd)

obj-m = semc_felica.o

semc_felica-objs = felica_master.o \
                   felica_uart.o felica_statemachine.o \
                   msm_uartdm.o felica_rxbuf.o felica_txbuf.o msm_uartmux.o \
                   msm_felica_cen.o \
                   felica_pon.o \
                   felica_rfs.o \
                   felica_int.o \
                   felica_rws.o

all:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) -C $(KERNEL_DIR) M=$(PWD) clean

