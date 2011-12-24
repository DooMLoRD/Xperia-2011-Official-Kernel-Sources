ifeq ($(BUILD_VIDEO_ENC_TEST),1)
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)



LOCAL_SRC_FILES:= \
	VideoEncTest.c \
	
LOCAL_C_INCLUDES := \
	$(TI_OMX_VIDEO)/video_encode/inc \
	$(TI_OMX_COMP_C_INCLUDES)

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES)

	
LOCAL_CFLAGS := $(TI_OMX_CFLAGS)

LOCAL_MODULE:= VideoEncTest

include $(BUILD_EXECUTABLE)
endif
