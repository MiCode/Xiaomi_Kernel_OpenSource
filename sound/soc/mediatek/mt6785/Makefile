# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6785-afe-objs := \
	mt6785-afe-pcm.o \
	mt6785-afe-clk.o \
	mt6785-afe-gpio.o \
	mt6785-dai-adda.o \
	mt6785-afe-control.o \
	mt6785-dai-i2s.o \
	mt6785-dai-hw-gain.o \
	mt6785-dai-src.o \
	mt6785-dai-pcm.o \
	mt6785-dai-tdm.o \
	mt6785-dai-hostless.o \
	mt6785-misc-control.o

obj-$(CONFIG_SND_SOC_MT6785) += snd-soc-mt6785-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6785_MT6359) += mt6785-mt6359.o

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
