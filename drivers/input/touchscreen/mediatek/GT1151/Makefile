# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT1151/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT1151/include/$(CONFIG_GTP_FIRMWARE_GOODIX)/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT1151/include/$(CONFIG_GTP_CONFIG_GOODIX)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-y	+=  gt1x_extents.o
obj-y	+=  gt1x_generic.o
obj-y	+=  gt1x_tools.o
obj-y	+=  gt1x_tpd.o
obj-y	+=  gt1x_update.o
obj-y	+=  gt1x_test.o
obj-y	+=  test_function.o

