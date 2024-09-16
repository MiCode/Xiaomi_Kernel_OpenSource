LOCAL_PATH := $(call my-dir)

ifeq ($(MTK_BT_SUPPORT),yes)
ifneq ($(MTK_BT_CHIP), $(filter $(MTK_BT_CHIP), MTK_CONSYS_MT6885 MTK_CONSYS_MT6893))

include $(CLEAR_VARS)
LOCAL_MODULE := bt_drv.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_INIT_RC := init.bt_drv.rc
LOCAL_REQUIRED_MODULES := wmt_drv.ko

include $(MTK_KERNEL_MODULE)

ifeq ($(MTK_BT_CHIP), $(filter $(MTK_BT_CHIP), MTK_CONSYS_MT6873 MTK_CONSYS_MT6853 MTK_CONSYS_MT6833))
  BT_OPTS += CFG_BT_PM_QOS_CONTROL=y
endif
$(linked_module): OPTS += $(BT_OPTS)

endif
endif
