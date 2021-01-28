# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6853-afe-objs := \
	mt6853-afe-pcm.o \
	mt6853-afe-clk.o \
	mt6853-afe-gpio.o \
	mt6853-dai-adda.o \
	mt6853-afe-control.o \
	mt6853-dai-i2s.o \
	mt6853-dai-hw-gain.o \
	mt6853-dai-src.o \
	mt6853-dai-pcm.o \
	mt6853-dai-tdm.o \
	mt6853-dai-hostless.o \
	mt6853-misc-control.o

obj-$(CONFIG_SND_SOC_MT6853) += snd-soc-mt6853-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6853_MT6359) += mt6853-mt6359.o

# MENU Governor Predict(low power)
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

ifeq ($(CONFIG_MTK_AUDIODSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/common
endif

# spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# external spk amp
ifeq ($(CONFIG_SND_SOC_AW87339),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/codecs/aw87339/$(MTK_PROJECT)/
endif