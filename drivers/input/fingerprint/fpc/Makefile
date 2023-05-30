#
# Makefile for the fingerprint device.
#

# Each configuration option enables a list of files.
#include $(srctree)/drivers/misc/mediatek/Makefile.custom



ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)

subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/400/tz_driver/include/
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/400/common/include/

#obj-$(CONFIG_FPC_FINGERPRINT) := fpc1022_tee.o
obj-m := fpc1022_tee.o

$(warning "start compling fp")
$(warning $(CONFIG_BUILD_PRODUCT_NAME))
ifeq ($(CONFIG_BUILD_PRODUCT_NAME), "rubens")
CDEFINES += -DCONFIG_PRODUCT_RUBENS_FP
KBUILD_CPPFLAGS += $(CDEFINES)
endif
ifeq ($(CONFIG_BUILD_PRODUCT_NAME), "xaga")
CDEFINES += -DCONFIG_PRODUCT_XAGA_FP
KBUILD_CPPFLAGS += $(CDEFINES)
endif
