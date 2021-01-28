# SPDX-License-Identifier: GPL-2.0
# platform driver
snd-soc-mtk-common-objs := mtk-afe-platform-driver.o mtk-afe-fe-dai.o
obj-$(CONFIG_SND_SOC_MEDIATEK) += snd-soc-mtk-common.o
obj-$(CONFIG_SND_SOC_MTK_SRAM) += mtk-sram-manager.o

snd-soc-mtk-sp-common-objs := mtk-sp-pcm-ops.o mtk-afe-debug.o mtk-sp-spk-amp.o
obj-$(CONFIG_SND_SOC_MTK_SMART_PHONE) += snd-soc-mtk-sp-common.o mtk-usip.o

obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += mtk-btcvsd.o

ifeq ($(CONFIG_SND_SOC_MTK_AUDIO_DSP),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework/
endif

ifeq ($(CONFIG_MTK_AUDIODSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)
endif
