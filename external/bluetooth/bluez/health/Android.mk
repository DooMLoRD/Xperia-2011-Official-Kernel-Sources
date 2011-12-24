LOCAL_PATH:= $(call my-dir)

# health plugin

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	hdp.c \
	hdp_main.c \
	hdp_manager.c \
	hdp_util.c \
	mcap.c \
	mcap_sync.c \

LOCAL_CFLAGS:= \
	-DVERSION=\"4.93\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DCONFIGDIR=\"/etc/bluetooth\" \

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../btio \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../gdbus \
	$(call include-path-for, glib) \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetoothd \
	libbluetooth \
	libbtio \
	libdbus \
	libcutils \
	libglib \

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE := bluetooth-health
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
