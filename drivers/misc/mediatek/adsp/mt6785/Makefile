# SPDX-License-Identifier: GPL-2.0

ADSP_VERSION := common
ADSP_VERSION_PATH := ../$(ADSP_VERSION)

obj-y += $(ADSP_VERSION_PATH)/
obj-y += adsp_awake.o
obj-y += adsp_clk.o
obj-y += adsp_bus_monitor.o
obj-y += adsp_reserved_mem.o
obj-y += adsp_platform_driver.o adsp_platform.o
obj-y += adsp_ipi.o
obj-y += adsp_excep.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_MTK_ENG_BUILD),y)
CFLAGS_adsp_excep.o += -DDEBUG
endif