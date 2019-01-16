include $(srctree)/drivers/misc/mediatek/Makefile.custom

#ccflags-y += -I$(MTK_CUSTOM_PATH)/touchpanel/$(ARCH_MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/mms134/
#ccflags-y += -I$(MTK_CUSTOM_PATH)/touchpanel/

ccflags-y += -I$(MTK_PROJECT_PATH_ROOT)/touchpanel/mms128/

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/$(ARCH_MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/mms128/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/


ifeq ($(CONFIG_ARCH_MTK_PLATFORM),mt6572)
    ccflags-y += -DMT6572
endif

obj-y	+=  mms100_ISP_download.o
obj-y	+=  mms128_driver.o


