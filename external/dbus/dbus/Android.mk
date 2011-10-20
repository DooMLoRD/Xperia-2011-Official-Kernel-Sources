# Set to true to write libdbus logs to logcat instead of stderr
# See also config.h to turn on verbose logs
LOG_TO_ANDROID_LOGCAT := true

ifneq ($(TARGET_SIMULATOR),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
dbus-address.c \
dbus-auth.c \
dbus-bus.c \
dbus-connection.c \
dbus-dataslot.c \
dbus-errors.c \
dbus-hash.c \
dbus-internals.c \
dbus-keyring.c \
dbus-list.c \
dbus-mainloop.c \
dbus-marshal-basic.c \
dbus-marshal-byteswap.c \
dbus-marshal-header.c \
dbus-marshal-recursive.c \
dbus-marshal-validate.c \
dbus-mempool.c \
dbus-memory.c \
dbus-message.c \
dbus-pending-call.c \
dbus-resources.c \
dbus-server.c \
dbus-server-socket.c \
dbus-server-unix.c \
dbus-sha.c \
dbus-shell.c \
dbus-signature.c \
dbus-spawn.c \
dbus-string.c \
dbus-string-util.c \
dbus-sysdeps.c \
dbus-sysdeps-pthread.c \
dbus-sysdeps-unix.c \
dbus-sysdeps-util-unix.c \
dbus-timeout.c \
dbus-threads.c \
dbus-transport.c \
dbus-transport-socket.c \
dbus-transport-unix.c \
dbus-object-tree.c \
dbus-userdb.c \
dbus-userdb-util.c \
dbus-watch.c

LOCAL_C_INCLUDES+= \
	$(call include-path-for, dbus)

LOCAL_MODULE:=libdbus

LOCAL_CFLAGS+= \
	-DDBUS_COMPILATION \
	-DANDROID_MANAGED_SOCKET \
	-DDBUS_MACHINE_UUID_FILE=\"/etc/machine-id\"

ifeq ($(LOG_TO_ANDROID_LOGCAT),true)
LOCAL_CFLAGS+= -DDBUS_ANDROID_LOG
LOCAL_SHARED_LIBRARIES+= libcutils
endif

include $(BUILD_SHARED_LIBRARY)

endif
