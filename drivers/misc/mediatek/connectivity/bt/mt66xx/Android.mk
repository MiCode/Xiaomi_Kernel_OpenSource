ifeq ($(MTK_BT_SUPPORT), yes)
ifeq ($(filter MTK_MT76%, $(MTK_BT_CHIP)),)
$(info MTK_BT_CHIP=$(MTK_BT_CHIP))

ifeq ($(MTK_BT_CHIP), $(filter $(MTK_BT_CHIP), MTK_CONSYS_MT6885 MTK_CONSYS_MT6893))
	include $(call all-subdir-makefiles, connac2)
else
	include $(call all-subdir-makefiles, legacy)
endif
endif
endif
