ifeq ($(BUILD_WBAMR_ENC_TEST),1)
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)



LOCAL_SRC_FILES:= \
	WbAmrEncTest.c \
	
LOCAL_C_INCLUDES := \
	$(TI_OMX_AUDIO)/wbamr_enc/inc \
	$(TI_OMX_COMP_C_INCLUDES)

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES)

	
LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DOMX_DEBUG

LOCAL_MODULE:= WBAMREncoder_Test

include $(BUILD_EXECUTABLE)
endif
