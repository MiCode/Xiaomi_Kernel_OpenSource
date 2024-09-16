LOCAL_PATH := $(call my-dir)

ifeq ($(MTK_WLAN_SUPPORT), yes)

include $(CLEAR_VARS)
LOCAL_MODULE := wmt_chrdev_wifi.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_INIT_RC := init.wlan_drv.rc
ifeq ($(WIFI_CHIP), CONNAC2X2_SOC3_0)
LOCAL_REQUIRED_MODULES := conninfra.ko
else
LOCAL_REQUIRED_MODULES := wmt_drv.ko
endif
WIFI_ADAPTOR_OPTS := ADAPTOR_OPTS=$(WIFI_CHIP)
include $(MTK_KERNEL_MODULE)
$(linked_module): OPTS += $(WIFI_ADAPTOR_OPTS)

endif
