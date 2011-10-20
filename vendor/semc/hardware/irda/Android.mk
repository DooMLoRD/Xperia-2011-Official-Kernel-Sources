ifeq ($(SEMC_CFG_IRDA_DRIVER),true)

LOCAL_PATH:= $(call my-dir)
SAVED_LOCAL_PATH_IR:= $(LOCAL_PATH)

include $(CLEAR_VARS)

# now the part that is actually built

LOCAL_PATH_ABS_IR := $(shell cd $(SAVED_LOCAL_PATH_IR) && pwd)

BUILD_ROOT_IR := $(TARGET_OUT_INTERMEDIATES)/SEMC_MSM_IRDA_OBJ

BUILD_SUBDIRS_IR := $(shell cd $(SAVED_LOCAL_PATH_IR) && find . -type d)
BUILD_SRC_IR := $(shell cd $(SAVED_LOCAL_PATH_IR) && find . -name \*[ch])
BUILD_MK_IR := $(shell cd $(SAVED_LOCAL_PATH_IR) && find . -name Makefile)

REL_SUBDIRS_IR := $(addprefix $(SAVED_LOCAL_PATH_IR)/, $(BUILD_SUBDIRS_IR))
REL_SRC_IR := $(addprefix $(SAVED_LOCAL_PATH_IR)/, $(BUILD_SRC_IR))
REL_MK_IR := $(addprefix $(SAVED_LOCAL_PATH_IR)/, $(BUILD_MK_IR))


$(BUILD_ROOT_IR)/semc_msm_irda.ko: $(PRODUCT_OUT)/kernel irprep
	@(cd $(BUILD_ROOT_IR); $(MAKE) KERNEL_DIR=$(ANDROID_PRODUCT_OUT)/obj/KERNEL_OBJ -f Makefile)


irprep: irsubdirs irsrc irmk

irsubdirs: $(REL_SUBDIRS_IR)
	@(for i in $(BUILD_SUBDIRS_IR); do mkdir -p $(BUILD_ROOT_IR)/$$i; done)

irsrc: $(REL_SRC_IR) irsubdirs
	@(for i in $(BUILD_SRC_IR); do test -e $(BUILD_ROOT_IR)/$$i || ln -sf $(LOCAL_PATH_ABS_IR)/$$i $(BUILD_ROOT_IR)/$$i; done)

irmk: $(REL_MK_IR) irsubdirs
	@(for i in $(BUILD_MK_IR); do test -e $(BUILD_ROOT_IR)/$$i || ln -sf $(LOCAL_PATH_ABS_IR)/$$i $(BUILD_ROOT_IR)/$$i; done)

# copy the modules
files := semc_msm_irda.ko

copy_to := $(addprefix $(TARGET_OUT)/lib/modules/,$(files))
copy_from := $(addprefix $(BUILD_ROOT_IR)/,$(files))

$(TARGET_OUT)/lib/modules/%.ko : $(BUILD_ROOT_IR)/%.ko | $(ACP)
	$(transform-prebuilt-to-target)

ALL_PREBUILT += $(copy_to)

endif
