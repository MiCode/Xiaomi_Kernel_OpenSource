$(info cam_cal: makefile start)
MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(CONFIG_MTK_PLATFORM)/camera_hw \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc

LEGACY_PLATFORM = mt6572 mt6580 mt6582 mt6735 mt6752 mt6795 mt6755
CURRENT_PLATFORM = $(subst ",,$(CONFIG_MTK_PLATFORM))

ifneq (, $(findstring $(CURRENT_PLATFORM), $(LEGACY_PLATFORM)))
$(info cam_cal: use legacy folder)
obj-y += legacy/$(subst ",,$(CONFIG_MTK_PLATFORM))/
else
$(info cam_cal: use common folder)
obj-y += cam_cal_drv.o
obj-y += cam_cal_list.o
obj-y += common/
endif
$(info cam_cal: makefile end)

