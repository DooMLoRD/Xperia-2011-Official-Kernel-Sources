LOCAL_PATH:= $(call my-dir)

# libsbc

include $(CLEAR_VARS)

# These files are LGPL and will generate an LGPL library,
# the remaining files are GPL - keep them out.

LOCAL_SRC_FILES:= \
	sbc.c \
	sbc_primitives.c \
	sbc_primitives_mmx.c \
	sbc_primitives_neon.c

LOCAL_CFLAGS:= \
	-finline-functions \
	-fgcse-after-reload \
	-funswitch-loops \
	-funroll-loops

LOCAL_MODULE := libsbc
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

