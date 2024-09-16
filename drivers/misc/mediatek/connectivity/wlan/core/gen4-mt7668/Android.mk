LOCAL_PATH := $(call my-dir)
ifeq ($(MTK_WLAN_SUPPORT), yes)
ifneq (true,$(strip $(TARGET_NO_KERNEL)))
ifeq (MT7668,$(strip $(MTK_COMBO_CHIP)))

include $(CLEAR_VARS)
LOCAL_MODULE := wlan_drv_gen4_mt7668_prealloc.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_INIT_RC := init.wlan_mt7668_drv.rc
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(shell find $(LOCAL_PATH) -type f -name '*.[cho]')) Makefile
include $(MTK_KERNEL_MODULE)

WIFI_OPTS := MTK_COMBO_HIF=$(MTK_COMBO_IF)
$(linked_module): OPTS += $(WIFI_OPTS)

include $(CLEAR_VARS)
LOCAL_MODULE := wlan_drv_gen4_mt7668.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_INIT_RC := init.wlan_mt7668_drv.rc
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(shell find $(LOCAL_PATH) -type f -name '*.[cho]')) Makefile
#LOCAL_REQUIRED_MODULES := wmt_chrdev_wifi.ko
include $(MTK_KERNEL_MODULE)


WIFI_OPTS := MTK_COMBO_HIF=$(MTK_COMBO_IF)
$(linked_module): OPTS += $(WIFI_OPTS)

### Copy Module.symvers from $(LOCAL_REQUIRED_MODULES) to this module #######
### For symbol link (when CONFIG_MODVERSIONS is defined)
#LOCAL_SRC_EXPORT_SYMBOL := $(subst $(LOCAL_MODULE),$(LOCAL_REQUIRED_MODULES),$(intermediates)/LINKED)/Module.symvers
#$(LOCAL_SRC_EXPORT_SYMBOL): $(subst $(LOCAL_MODULE),$(LOCAL_REQUIRED_MODULES),$(linked_module))
#LOCAL_EXPORT_SYMBOL := $(intermediates)/LINKED/Module.symvers
#$(LOCAL_EXPORT_SYMBOL): $(intermediates)/LINKED/% : $(LOCAL_SRC_EXPORT_SYMBOL)
#	$(copy-file-to-target)
#$(linked_module): $(LOCAL_EXPORT_SYMBOL) $(LOCAL_SRC_EXPORT_SYMBOL) $(AUTO_CONF) $(AUTOCONF_H)
endif
endif
endif

