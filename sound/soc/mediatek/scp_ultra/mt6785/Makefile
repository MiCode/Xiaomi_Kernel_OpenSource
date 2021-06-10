# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))


# scp ultrasound include path
ifeq ($(CONFIG_MTK_ULTRASND_PROXIMITY),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_ultra/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/utility

endif

# platform code
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += mtk-scp-ultra-platform-mem-control.o

