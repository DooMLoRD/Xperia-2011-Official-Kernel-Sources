ifeq ($(BOARD_HAVE_BLUETOOTH),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for,bluez)/lib/ \

LOCAL_CFLAGS:= \
	-DVERSION=\"2.0\"

LOCAL_SRC_FILES:= \
	parser/att.c \
	parser/avctp.c \
	parser/avdtp.c \
	parser/bnep.c \
	parser/bpa.c \
	parser/capi.c \
	parser/cmtp.c \
	parser/csr.c \
	parser/ericsson.c \
	parser/hci.c \
	parser/hcrp.c \
	parser/hidp.c \
	parser/l2cap.c \
	parser/lmp.c \
	parser/obex.c \
	parser/parser.c \
	parser/ppp.c \
	parser/rfcomm.c \
	parser/sdp.c \
	parser/tcpip.c \
	src/hcidump.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE:=hcidump

include $(BUILD_EXECUTABLE)
endif
