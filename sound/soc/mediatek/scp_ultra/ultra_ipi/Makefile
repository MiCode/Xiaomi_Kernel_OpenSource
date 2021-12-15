# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_ULTRASND_PROXIMITY),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/utility
endif

snd-soc-mtk-scp-ultra-objs := ultra_ipi.o
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += snd-soc-mtk-scp-ultra.o
