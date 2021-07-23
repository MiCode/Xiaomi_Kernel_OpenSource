#
# Makefile for the fingerprint device.
#

# Each configuration option enables a list of files.
#include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
#ccflags-y += -Wno-errors=strict-prototypes
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)


subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/tz_driver/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/common/include/

#obj-$(CONFIG_GOODIX_FINGERPRINT)	+= gf_tee.o
#gf_tee-y := gf_spi_tee.o gf_spi_factory.o
obj-y := gf_spi_tee.o gf_spi_factory.o
