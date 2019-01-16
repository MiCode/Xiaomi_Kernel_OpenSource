include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(MTK_CUSTOM_PATH)/mmc-host/$(MTK_PLATFORM)
ccflags-y += -I$(MTK_CUSTOM_PATH)/videox/$(MTK_PLATFORM)

ifeq ($(CONFIG_ARM64), y)
obj-y	+= mt_vcore_dvfs_64.o
else
obj-y	+= mt_vcore_dvfs.o
endif
