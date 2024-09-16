LOCAL_PATH := $(call my-dir)

ifeq ($(MTK_WLAN_SUPPORT), yes)

# to decide using which generation wlan driver is in wlan_product_package.mk
# (gen2/gen3/gen4/connac/...)

include $(CLEAR_VARS)
LOCAL_MODULE := wlan_drv_gen2.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_REQUIRED_MODULES := wmt_chrdev_wifi.ko
include $(MTK_KERNEL_MODULE)

endif
