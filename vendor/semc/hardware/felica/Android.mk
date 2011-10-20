ifeq (true, $(SEMC_CFG_FELICA_ENABLED))

LOCAL_PATH:= $(call my-dir)
SAVED_LOCAL_PATH_FLC:= $(LOCAL_PATH)

include $(CLEAR_VARS)

# now the part that is actually built

LOCAL_PATH_ABS_FLC := $(shell cd $(SAVED_LOCAL_PATH_FLC) && pwd)

BUILD_ROOT_FLC := $(TARGET_OUT_INTERMEDIATES)/FELICA_OBJ

BUILD_SUBDIRS_FLC := $(shell cd $(SAVED_LOCAL_PATH_FLC) && find . -type d)
BUILD_SRC_FLC := $(shell cd $(SAVED_LOCAL_PATH_FLC) && find . -name \*[ch])
BUILD_MK_FLC := $(shell cd $(SAVED_LOCAL_PATH_FLC) && find . -name Makefile)

REL_SUBDIRS_FLC := $(addprefix $(SAVED_LOCAL_PATH_FLC)/, $(BUILD_SUBDIRS_FLC))
REL_SRC_FLC := $(addprefix $(SAVED_LOCAL_PATH_FLC)/, $(BUILD_SRC_FLC))
REL_MK_FLC := $(addprefix $(SAVED_LOCAL_PATH_FLC)/, $(BUILD_MK_FLC))

$(BUILD_ROOT_FLC)/semc_felica.ko: $(PRODUCT_OUT)/kernel prep_flc
	@(cd $(BUILD_ROOT_FLC); $(MAKE) KERNEL_DIR=$(ANDROID_PRODUCT_OUT)/obj/KERNEL_OBJ -f Makefile)

prep_flc: subdirs_flc src_flc mk_flc

subdirs_flc: $(REL_SUBDIRS_FLC)
	@(for i in $(BUILD_SUBDIRS_FLC); do mkdir -p $(BUILD_ROOT_FLC)/$$i; done)

src_flc: $(REL_SRC_FLC) subdirs_flc
	@(for i in $(BUILD_SRC_FLC); do test -e $(BUILD_ROOT_FLC)/$$i || ln -sf $(LOCAL_PATH_ABS_FLC)/$$i $(BUILD_ROOT_FLC)/$$i; done)

mk_flc: $(REL_MK_FLC) subdirs_flc
	@(for i in $(BUILD_MK_FLC); do test -e $(BUILD_ROOT_FLC)/$$i || ln -sf $(LOCAL_PATH_ABS_FLC)/$$i $(BUILD_ROOT_FLC)/$$i; done)

# copy the modules

files := semc_felica.ko

copy_to := $(addprefix $(TARGET_OUT)/lib/modules/,$(files))
copy_from := $(addprefix $(BUILD_ROOT_FLC)/,$(files))

$(TARGET_OUT)/lib/modules/%.ko : $(BUILD_ROOT_FLC)/%.ko | $(ACP)
	$(transform-prebuilt-to-target)

ALL_PREBUILT += $(copy_to)

endif
