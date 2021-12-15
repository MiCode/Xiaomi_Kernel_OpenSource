# SPDX-License-Identifier: GPL-2.0

# MTK Platform Support
ifeq ($(CONFIG_MACH_MT6781),y)
AUDIO_PLATFORM := mt6785
else
AUDIO_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

# include path
ifeq ($(CONFIG_MTK_ULTRASND_PROXIMITY),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/utility
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(AUDIO_PLATFORM)/
ifeq ($(CONFIG_MACH_MT6779),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/framework
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/utility
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/framework
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/utility
endif
endif

snd-soc-mtk-scp-ultra-objs := ultra_ipi.o
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += snd-soc-mtk-scp-ultra.o
