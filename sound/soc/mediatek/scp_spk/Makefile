# SPDX-License-Identifier: GPL-2.0

# MTK Platform Support
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

# scp spk include path
ifeq ($(CONFIG_SND_SOC_MTK_SCP_SMARTPA),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
ifeq ($(CONFIG_MACH_MT6779),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/framework
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/tasks/spkprotect
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/framework
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/tasks/spkprotect
endif
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_spk/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/utility
endif

ifeq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
endif

obj-y += $(MTK_PLATFORM)/

# common code
snd-soc-mtk-scp-spk-objs := \
	mtk-scp-spk-common.o \
	mtk-scp-spk-pcm.o \
	mtk-scp-spk-mem-control.o \
	mtk-scp-spk-platform-driver.o

obj-$(CONFIG_SND_SOC_MTK_SCP_SMARTPA) += snd-soc-mtk-scp-spk.o
