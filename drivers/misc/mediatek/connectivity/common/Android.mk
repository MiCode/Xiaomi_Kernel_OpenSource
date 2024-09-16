LOCAL_PATH := $(call my-dir)

ifneq ($(filter yes,$(sort $(MTK_WLAN_SUPPORT) $(MTK_BT_SUPPORT) $(MTK_GPS_SUPPORT) $(MTK_FM_SUPPORT))),)
WLAN_MT76XX_CHIPS := MT7668 MT7663

ifeq ($(filter $(WLAN_MT76XX_CHIPS), $(MTK_COMBO_CHIP)),)
ifneq (true,$(strip $(TARGET_NO_KERNEL)))
ifneq ($(filter yes,$(MTK_COMBO_SUPPORT)),)

include $(CLEAR_VARS)
LOCAL_MODULE := wmt_drv.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_INIT_RC := init.wmt_drv.rc
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(shell find $(LOCAL_PATH) -type f -name '*.[cho]')) Makefile
LOCAL_REQUIRED_MODULES :=

include $(MTK_KERNEL_MODULE)

WMT_OPTS := MTK_CONSYS_ADIE=$(MTK_CONSYS_ADIE)
WMT_OPTS += MTK_PLATFORM_WMT=$(MTK_PLATFORM)
WMT_OPTS += TARGET_BOARD_PLATFORM_WMT=$(TARGET_BOARD_PLATFORM)

$(linked_module): OPTS += $(WMT_OPTS)

else
        $(warning wmt_drv-MTK_COMBO_SUPPORT: [$(MTK_COMBO_SUPPORT)])
endif
endif

else
# MT76XX
$(warning skip wmt_drv)
$(warning wmt_drv-MTK_COMBO_CHIP: [$(MTK_COMBO_CHIP)])
endif
endif
