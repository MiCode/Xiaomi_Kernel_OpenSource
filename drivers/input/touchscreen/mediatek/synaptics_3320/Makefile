include $(srctree)/drivers/misc/mediatek/Makefile.custom


ccflags-y += -I$(MTK_PROJECT_PATH_ROOT)/touchpanel/synaptics_3320/

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/synaptics_3320/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/


ifeq ($(CONFIG_MTK_PLATFORM),"mt6582")
    ccflags-y += -DMT6582
endif
obj-y   +=  s3320_driver.o
obj-y   +=  s3320_fw_update.o
obj-y +=  RefCode_CustomerImplementation.o
obj-y +=  RefCode_F54.o

