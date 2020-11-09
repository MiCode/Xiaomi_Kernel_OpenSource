# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif
DSP_VERSION := hifi3_v2

# platform driver
snd-soc-mtk-common-objs := mtk-afe-platform-driver.o mtk-afe-fe-dai.o
obj-$(CONFIG_SND_SOC_MEDIATEK) += snd-soc-mtk-common.o

snd-soc-mtk-sram-manager-objs := mtk-sram-manager.o
obj-$(CONFIG_SND_SOC_MTK_SRAM) += snd-soc-mtk-sram-manager.o
obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += mtk-btcvsd.o

snd-soc-mtk-sp-common-objs := mtk-sp-pcm-ops.o mtk-afe-debug.o
ifeq ($(CONFIG_MTK_ECCCI_DRIVER),m)
snd-soc-mtk-sp-common-objs += mtk-usip.o
endif
obj-$(CONFIG_SND_SOC_MTK_SMART_PHONE) += snd-soc-mtk-sp-common.o
# adsp include path
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(DSP_VERSION)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/common
