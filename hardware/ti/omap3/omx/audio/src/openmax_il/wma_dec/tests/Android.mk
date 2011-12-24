ifeq ($(BUILD_WMA_DEC_TEST),1)
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)



LOCAL_SRC_FILES:= \
	WmaDecTest.c \
	
LOCAL_C_INCLUDES := \
	$(TI_OMX_AUDIO)/wma_dec/inc \
	$(TI_OMX_COMP_C_INCLUDES)

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES)

	
LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DOMX_DEBUG

LOCAL_MODULE:= WmaDecTest_common

include $(BUILD_EXECUTABLE)
endif
