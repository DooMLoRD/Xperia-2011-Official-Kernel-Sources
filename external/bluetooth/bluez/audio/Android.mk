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
	media.c \
	module-bluetooth-sink.c \
	sink.c \
	source.c \
	telephony-dummy.c \
	transport.c \
	unix.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.93\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DCONFIGDIR=\"/etc/bluetooth\" \
	-DANDROID \
	-D__S_IFREG=0100000  # missing from bionic stat.h

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../gdbus \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../btio \
	$(call include-path-for, glib) \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libbluetoothd \
	libbtio \
	libdbus \
	libglib


LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE := audio

include $(BUILD_SHARED_LIBRARY)

#
# liba2dp
# This is linked to Audioflinger so **LGPL only**

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	android_audio_hw.c \
	liba2dp.c \
	ipc.c \
	../sbc/sbc_primitives.c \
	../sbc/sbc_primitives_neon.c

ifeq ($(TARGET_ARCH),x86)
LOCAL_SRC_FILES+= \
	../sbc/sbc_primitives_mmx.c \
	../sbc/sbc.c
else
LOCAL_SRC_FILES+= \
	../sbc/sbc.c.arm \
	../sbc/sbc_primitives_armv6.c
endif

# to improve SBC performance
LOCAL_CFLAGS:= -funroll-loops

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../sbc \
	../../../../frameworks/base/include \
	system/bluetooth/bluez-clean-headers

LOCAL_SHARED_LIBRARIES := \
	libcutils

ifneq ($(wildcard system/bluetooth/legacy.mk),)
LOCAL_STATIC_LIBRARIES := \
	libpower

LOCAL_MODULE := liba2dp
else
LOCAL_SHARED_LIBRARIES += \
	libpower

LOCAL_MODULE := audio.a2dp.default
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
