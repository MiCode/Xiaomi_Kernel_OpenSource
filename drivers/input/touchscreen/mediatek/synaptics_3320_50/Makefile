# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/synaptics_3320_50/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/synaptics_3320_50/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/
ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/wdt/
ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/wdt/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/i2c/$(MTK_PLATFORM)/

obj-y   +=  s3320_driver.o
obj-y   +=  s3320_fw_update.o
obj-y   +=  RefCode_CustomerImplementation.o
obj-y   +=  RefCode_F54.o
