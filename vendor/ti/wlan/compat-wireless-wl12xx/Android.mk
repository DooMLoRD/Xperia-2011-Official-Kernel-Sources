ifeq ($(BOARD_WLAN_DEVICE),wl12xx_mac80211)

LOCAL_PATH := $(call my-dir)
WL12XX_LOCAL_PATH := $(LOCAL_PATH)
include $(CLEAR_VARS)

WL12XX_BUILD_PATH = $(TARGET_OUT_INTERMEDIATES)/WL12XX_OBJ
KERNEL_PATH = $(abspath $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ)

prep:
	rsync -r -u $(WL12XX_LOCAL_PATH)/* $(WL12XX_BUILD_PATH)

$(WL12XX_BUILD_PATH)/compat/compat.ko: prep $(PRODUCT_OUT)/kernel.si_
	(cd $(WL12XX_BUILD_PATH) && make KLIB=$(KERNEL_PATH) KLIB_BUILD=$(KERNEL_PATH) ARCH=arm CROSS_COMPILE=arm-eabi-)

$(WL12XX_BUILD_PATH)/drivers/net/wireless/wl12xx/wl12xx_sdio.ko: $(WL12XX_BUILD_PATH)/drivers/net/wireless/wl12xx/wl12xx.ko

$(WL12XX_BUILD_PATH)/drivers/net/wireless/wl12xx/wl12xx.ko: $(WL12XX_BUILD_PATH)/net/mac80211/mac80211.ko

$(WL12XX_BUILD_PATH)/net/mac80211/mac80211.ko: $(WL12XX_BUILD_PATH)/net/wireless/cfg80211.ko

$(WL12XX_BUILD_PATH)/net/wireless/cfg80211.ko: $(WL12XX_BUILD_PATH)/compat/compat.ko

files := drivers/net/wireless/wl12xx/wl12xx_sdio.ko drivers/net/wireless/wl12xx/wl12xx.ko compat/compat.ko net/wireless/cfg80211.ko net/mac80211/mac80211.ko

files_to_strip := compat/compat.ko net/wireless/cfg80211.ko net/mac80211/mac80211.ko

copy_from := $(addprefix $(WL12XX_BUILD_PATH)/,$(files))
copy_to := $(addprefix $(TARGET_OUT)/lib/modules/,$(files))

$(TARGET_OUT)/lib/modules/%.ko : $(WL12XX_BUILD_PATH)/%.ko | $(ACP)
	$(transform-prebuilt-to-target)
	$(if $(findstring $@,$(addprefix $(TARGET_OUT)/lib/modules/,$(files_to_strip))), $(TARGET_STRIP) -dx $@)

ALL_PREBUILT += $(copy_to)

endif


