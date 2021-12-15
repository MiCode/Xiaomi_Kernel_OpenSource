# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

# platform driver
snd-soc-mtk-common-objs := mtk-afe-platform-driver.o mtk-afe-fe-dai.o
ifeq (, $(filter y,$(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6779)))
obj-$(CONFIG_MTK_ION) += mtk-mmap-ion.o
ccflags-y += -DMMAP_SUPPORT
endif
obj-$(CONFIG_SND_SOC_MEDIATEK) += snd-soc-mtk-common.o
obj-$(CONFIG_SND_SOC_MTK_SRAM) += mtk-sram-manager.o

snd-soc-mtk-sp-common-objs := mtk-sp-pcm-ops.o mtk-afe-debug.o mtk-sp-spk-amp.o
obj-$(CONFIG_SND_SOC_MTK_SMART_PHONE) += snd-soc-mtk-sp-common.o mtk-usip.o

obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += mtk-btcvsd.o

subdir-ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
subdir-ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/mtk

ifeq ($(CONFIG_SND_SOC_MTK_AUDIO_DSP),y)
ifeq ($(CONFIG_MACH_MT6779),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp/v1
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/framework/
else
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp/v2
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/framework/
endif
endif

ifeq ($(CONFIG_MTK_AUDIODSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)
endif

ifeq ($(CONFIG_SND_SOC_AW87339),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/codecs/aw87339/$(MTK_PROJECT)/
endif
