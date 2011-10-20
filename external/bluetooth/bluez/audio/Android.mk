LOCAL_PATH:= $(call my-dir)

# A2DP plugin

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	a2dp.c \
	avdtp.c \
	control.c \
	device.c \
	gateway.c \
	headset.c \
	ipc.c \
	main.c \
	manager.c \
	module-bluetooth-sink.c \
	sink.c \
	source.c \
	telephony-dummy.c \
	unix.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.69\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DCONFIGDIR=\"/etc/bluetooth\" \
	-DANDROID \
	-D__S_IFREG=0100000  # missing from bionic stat.h

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../gdbus \
	$(LOCAL_PATH)/../src \
	$(call include-path-for, glib) \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libbluetoothd \
	libdbus


LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE := audio

include $(BUILD_SHARED_LIBRARY)

#
# liba2dp
# This is linked to Audioflinger so **LGPL only**

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	liba2dp.c \
	ipc.c \
	../sbc/sbc.c.arm \
	../sbc/sbc_primitives.c \
	../sbc/sbc_primitives_neon.c

# to improve SBC performance
LOCAL_CFLAGS:= -funroll-loops

# temp deoptimization to be compatibe with 4.4.0 compiler on ginger-ste-dev
ifeq ($(BOARD_BYPASSES_AUDIOFLINGER_A2DP),true)
LOCAL_CFLAGS+= -O1
endif

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../sbc \
    ../../../../frameworks/base/include \
	system/bluetooth/bluez-clean-headers

LOCAL_SHARED_LIBRARIES := \
	libcutils

LOCAL_MODULE := liba2dp

include $(BUILD_SHARED_LIBRARY)


#
# ALSA plugins
#
ifeq ($(BOARD_BYPASSES_AUDIOFLINGER_A2DP),true)

# These should be LGPL code only

#
# bluez plugin to ALSA PCM
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= pcm_bluetooth.c ipc.c

LOCAL_CFLAGS:= -DPIC -D_POSIX_SOURCE

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../alsa-lib/include/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../bluez/lib
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../sbc

LOCAL_SHARED_LIBRARIES := libcutils libasound
LOCAL_STATIC_LIBRARIES := libsbc

# don't prelink this library
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/lib/alsa-lib
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_UNSTRIPPED)/usr/lib/alsa-lib
LOCAL_MODULE := libasound_module_pcm_bluetooth
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)


#
# bluez plugin to ALSA CTL
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= ctl_bluetooth.c ipc.c

LOCAL_CFLAGS:= -DPIC -D_POSIX_SOURCE

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../alsa-lib/include/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../lib
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../sbc

LOCAL_SHARED_LIBRARIES := libcutils libasound

LOCAL_LDFLAGS := -module -avoid-version

# don't prelink this library
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/lib/alsa-lib
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_UNSTRIPPED)/usr/lib/alsa-lib
LOCAL_MODULE := libasound_module_ctl_bluetooth
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_BYPASSES_AUDIOFLINGER_A2DP
