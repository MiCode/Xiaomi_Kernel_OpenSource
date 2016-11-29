LOCAL_PATH := $(call my-dir)
BCM43xx_PATH := $(LOCAL_PATH)

$(info building bcmdhd module for $(COMBO_CHIP))
ifneq ($(findstring 4324, $(COMBO_CHIP)), $(empty))
$(info building bcmdhd module for SDIO)
$(eval $(call build_kernel_module,$(LOCAL_PATH)/,bcmdhd,CONFIG_BCMDHD=m CONFIG_BCMDHD_SDIO=y CONFIG_BCM43241=y CONFIG_DHD_USE_SCHED_SCAN=y CONFIG_DHD_2G_ONLY=$(BAND_5G_FORBIDDEN)))
else ifneq ($(findstring 4356, $(COMBO_CHIP)), $(empty))
$(info building bcmdhd module for PCIE)
$(eval $(call build_kernel_module,$(LOCAL_PATH)/,bcmdhd,CONFIG_BCMDHD=m CONFIG_BCMDHD_SDIO= CONFIG_BCMDHD_PCIE=y CONFIG_BCM4356=y CONFIG_DHD_USE_SCHED_SCAN=y CONFIG_DHD_2G_ONLY=$(BAND_5G_FORBIDDEN)))
else
$(info trying to build bcmdhd driver for unknown chip, please enable it here...)
endif
