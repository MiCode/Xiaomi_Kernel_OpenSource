#
# Makefile for nanohub
#

obj-$(CONFIG_NANOHUB) += nanohub.o
nanohub-y := main.o bl.o comms.o
# nanohub-$(CONFIG_NANOHUB_SPI) += spi.o
# nanohub-$(CONFIG_NANOHUB_I2C) += i2c.o
nanohub-$(CONFIG_NANOHUB_MTK_IPI) += nanohub-mtk.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)/
ccflags-$(CONFIG_MACH_MT6761) += -I$(srctree)/drivers/misc/mediatek/scp/mt6765
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
#ccflags-y += -I$(srctree)/arch/arm/mach-mt2601/pike/dct/dct
