
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.


# Include headers.
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6763/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

include $(srctree)/drivers/misc/mediatek/teei/Makefile.include

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
#	include $(srctree)/drivers/misc/mediatek/Makefile.include
	subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/tz_driver/include
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/300/common/include
endif

# Early-logging level for ff_hal, ff_ta and driver as well. Note that this
# logging level only takes effect before ff_config module is loaded.
#
# Options:
# 0 - FF_LOG_LEVEL_ALL, All
# 1 - FF_LOG_LEVEL_VBS, Verbose
# 2 - FF_LOG_LEVEL_DBG, Debug
# 3 - FF_LOG_LEVEL_INF, Info
# 4 - FF_LOG_LEVEL_WRN, Warning
# 5 - FF_LOG_LEVEL_ERR, Error
ccflags-y += -D__FF_EARLY_LOG_LEVEL=2

# Source files. $(CONFIG_FOCALTECH_FINGERPRINT)
obj-y := ff_ctl.o plat-mt6762.o chip-ft9348.o ff_spi.o spidev.o
#focaltech_fp-objs := ff_ctl.o chip-ft9348.o ff_spi.o spidev.o
#focaltech_fp-objs += plat-mt6762.o




