
##
##
## Application Make Flags
##
##
#

#
# Statically link the CU
#
STATIC_LIB ?= y

#
# FW
# 
FW ?= 1273

#
# Full Async Mode
# 
FULL_ASYNC ?= n

#
# Build bmtrace performance tool
# 
BMTRACE ?= n

#
# Full Async Mode
# 
USE_IRQ_ACTIVE_HIGH ?= n

#
# bus test-driver
#
TEST ?= n

#
# Eth Support
#
ETH_SUPPORT ?= n

##
##
## File lists and locations
##
##

#
# DK_ROOT must be set prior to including common.inc
# Android.mk must use static path
DK_ROOT = $(ROOT_DIR)
DK_NEW_ROOT=../../..
#
# Includes common definitions and source file list
#
ifneq ($(KERNELRELEASE),)
    include $(M)/$(DK_ROOT)/stad/build/linux/common.inc
else
    include $(DK_ROOT)/stad/build/linux/common.inc
endif

#
# Location and filename of the driver .lib file.
#
DRIVER_LIB_DIR = $(DK_ROOT)/stad/build/linux
DRIVER_LIB = $(DK_NEW_ROOT)/stad/build/linux/libestadrv_ap.a
#
# Location and filename of the OS .lib file.
#
OS_COMMON_DIR = $(DK_ROOT)/platforms/os/common/build/linux
OS_COMMON = $(DK_NEW_ROOT)/platforms/os/common/build/linux/libuadrv.a

#
# Location and filename of the linux OS object file.
#
OS_LINUX_DIR = $(DK_ROOT)/platforms/os/linux/build
OS_LINUX = $(OS_LINUX_DIR)/tiap_drv_stub.o
OS_AUXILIARY_LIBS = ../$(DRIVER_LIB)\ ../$(OS_COMMON)

#
# Location and filename of the wlan user-mode programs root directory.
#
WLAN_CUDK_DIR = $(DK_ROOT)/CUDK

#
# Location and filename of the wlan configuraion utility CLI program.
#
WLAN_CU_CLI_DIR = $(DK_ROOT)/CUDK/configurationutility
WLAN_CU_CLI = $(WLAN_CUDK_DIR)/output/wlan_cu

#
# Location and filename of the wlan logger utility program.
#
WLAN_LOGGER_DIR = $(DK_ROOT)/CUDK/logger
WLAN_LOGGER = $(WLAN_CUDK_DIR)/output/wlan_logger

#
# Location and filename of the WLAN loader utility
#
WLAN_LOADER_DIR = $(DK_ROOT)/CUDK/tiwlan_loader/
WLAN_LOADER = $(WLAN_CUDK_DIR)/output/tiwlan_loader

#
# Location and filename of the Linux Wireless Tools
#

#LINUX_WIRELESS_TOOLS_DIR = $(DK_ROOT)/CUDK/wireless_tools
#LINUX_WIRELESS_TOOLS = $(LINUX_WIRELESS_TOOLS_DIR)/iwconfig

#
# The combined linux module file.
#
OUTPUT_DIR = $(DK_ROOT)/platforms/os/linux
OUTPUT_FILE = $(OUTPUT_DIR)/tiap_drv.ko

##
##
## Build process
##
##

#ifneq ($(KERNELRELEASE),)


##
##
## This is the kernel build phase - set the appropriate arguments
##
##

#
# Intermediate object name - this should be renamed to the desired object name
# after the kernel makefile finishes its work.
#
#       obj-m = linux.o

#
# List of object files the kernel makefile needs to compile.
#
#       linux-y = $(DRIVER_LIB) $(OS_COMMON) $(OS_LINUX)


#else   # ifneq ($(KERNELRELEASE),)


##
##
## This is the regular build phase - act according to the make actions
##
##

#
# Used to check if the necessary packages are present.
#
HAVE_NONGPL = $(wildcard $(DRIVER_LIB_DIR)/Makefile)
HAVE_GPL = $(wildcard $(OS_LINUX_DIR)/Makefile)
.PHONY: all
all: .depend $(OUTPUT_FILE) apps.tar
#
# Create the images
#
.PHONY: apps.tar

MODULES_LIST = tiap_drv.ko

apps.tar: apps
ifeq ($(STRIP),y)
	@echo stripping...
	cd $(OUTPUT_DIR) && $(CROSS_COMPILE)strip -g --strip-unneeded $(MODULES_LIST)
#	cd $(DK_ROOT)/external_drivers/$(HOST_PLATFORM)/Linux/$(BUS_DRV) && $(CROSS_COMPILE)strip -g $(BUS_DRIVER_MODULE)
endif

apps: $(OUTPUT_FILE)
#	rm -f $(OUTPUT_DIR)/$(BUS_DRV_REMOVE).ko $(OUTPUT_DIR)/$(BUS_DRV_REMOVE)_test
#	cp -f $(WLAN_CU_CLI) $(OUTPUT_DIR)
#	cp -f $(WLAN_LOADER) $(OUTPUT_DIR)

#
# Recursively cleans the driver, OS, bus and CLI files
#
.PHONY: clean
clean:
	$(MAKE) -C $(DRIVER_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) clean
	$(MAKE) -C $(OS_COMMON_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) clean
	$(MAKE) -C $(OS_LINUX_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) clean
	@rm -f $(OUTPUT_DIR)/tiap_drv.ko
	@rm -rf *.o *.a \.*.o.cmd *~ *.~* core .depend dep

# in order to remove all .*.o.cmd 
	@find ../../../. -type f -print | grep .o.cmd | xargs rm -f
# in order to remove all *.order and *.symvers
	@find ../../../. -type f -print | grep .order | xargs rm -f
	@find ../../../. -type f -print | grep .symvers | xargs rm -f
# in order to remove Module.markers file
	@find ../../../. -type f -print | grep Module.markers | xargs rm -f
# in order to remove the binaries tar
#	@rm -rf $(TAR_FILE)


#
# Verifies that all necessary packages are present.
#
.PHONY: verifypackages
verifypackages:
ifeq ($(strip $(HAVE_GPL)),)
	@echo "*** The GPL package does not seem to be present. You will need both the"
	@echo "*** GPL package and the Non GPL package to execute this makefile."
	exit 1
endif

ifeq ($(strip $(HAVE_NONGPL)),)
	@echo "*** The Non GPL package does not seem to be present. You will need both the"
	@echo "*** GPL package and the Non GPL package to execute this makefile."
	exit 1
endif


# Builds the WSPI or SDIO driver
#
$(BUS_DRV):
	$(MAKE) -C $(DK_ROOT)/external_drivers/$(HOST_PLATFORM)/Linux/$(BUS_DRV) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) KERNEL_DIR=$(KERNEL_DIR) OUTPUT_DIR=$(shell pwd)/$(OUTPUT_DIR) all


#
# Causes the driver and the configuration utility object files to get rebuilt
#
.depend:
	rm -f $(OUTPUT_FILE) $(DRIVER_LIB) $(OS_COMMON) $(OS_LINUX)

#
# Recursively builds the driver lib file
#
$(DRIVER_LIB): .depend
	$(MAKE) -C $(DRIVER_LIB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB)


#
# Recursively builds the OS lib file
#
$(OS_COMMON): $(DRIVER_LIB)
	$(MAKE) -C $(OS_COMMON_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) OS_AUXILIARY_LIBS+=../../$(DRIVER_LIB)


#
# Recursively builds the linux OS stub object file
#
$(OS_LINUX): $(OS_COMMON)
	$(MAKE) -C $(OS_LINUX_DIR) CROSS_COMPILE=$(CROSS_COMPILE) DEBUG=$(DEBUG) WSPI=$(WSPI) INTR=$(INTR) XCC=$(XCC) INFO=$(INFO) STATIC_LIB=$(STATIC_LIB) OS_AUXILIARY_LIBS+=../$(DRIVER_LIB) OS_AUXILIARY_LIBS+=../$(OS_COMMON)


#
# Recursively builds the driver object file
#
$(OUTPUT_FILE): $(OS_LINUX)
	mv $(OS_LINUX) $(OUTPUT_FILE)

.PHONY: link
link:
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) modules
	mv tiap_drv.ko $(OUTPUT_FILE)
#endif  # ifneq ($(KERNELRELEASE),)
