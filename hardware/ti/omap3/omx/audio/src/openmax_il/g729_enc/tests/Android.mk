ifeq ($(BUILD_G729_ENC_TEST),1)
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)



LOCAL_SRC_FILES:= \
    G729EncTest.c \
    
LOCAL_C_INCLUDES := \
    $(TI_OMX_SYSTEM)/common/inc \
    $(TI_OMX_COMP_C_INCLUDES) \
    $(TI_OMX_AUDIO)/g729_enc/inc

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES)

    
LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DOMX_DEBUG

LOCAL_MODULE:= G729EncTest_common

include $(BUILD_EXECUTABLE)
endif
