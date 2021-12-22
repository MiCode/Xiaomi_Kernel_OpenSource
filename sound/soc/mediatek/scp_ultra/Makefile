# SPDX-License-Identifier: GPL-2.0

# MTK Platform Support

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

ifeq ($(CONFIG_MTK_ULTRASND_PROXIMITY),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/utility
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include

#compatible
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6785" "mt6781"))
ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_ultra/audio_ipi
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += audio_ipi/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/cm4
else
ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_ultra/ultra_ipi
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += ultra_ipi/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/rv
endif
endif

# common code
snd-soc-mtk-scp-ultra-objs := \
	mtk-scp-ultra-common.o \
	mtk-scp-ultra-pcm.o \
	mtk-scp-ultra-mem-control.o \
	mtk-scp-ultra_dump.o \
	mtk-scp-ultra-platform-driver.o

obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += snd-soc-mtk-scp-ultra.o
