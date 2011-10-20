#
# Makefile for semc_msm_irda_module
#
# Copyright 2011 Sony Ericsson Mobile Communications Japan Inc.
# All rights, including trade secret rights, reserved.
#

# environment variables
CROSS_COMPILE  := arm-eabi-
ARCH           := arm
PWD := $(shell pwd)

obj-m = semc_msm_irda.o
semc_msm_irda_module-objs = semc_msm_irda.o

all:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) -C $(KERNEL_DIR) M=$(PWD) clean
