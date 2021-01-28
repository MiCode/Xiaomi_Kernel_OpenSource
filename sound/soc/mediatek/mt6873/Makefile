# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6873-afe-objs := \
	mt6873-afe-pcm.o \
	mt6873-afe-clk.o \
	mt6873-afe-gpio.o \
	mt6873-dai-adda.o \
	mt6873-afe-control.o \
	mt6873-dai-i2s.o \
	mt6873-dai-hw-gain.o \
	mt6873-dai-src.o \
	mt6873-dai-pcm.o \
	mt6873-dai-tdm.o \
	mt6873-dai-hostless.o \
	mt6873-misc-control.o

obj-$(CONFIG_SND_SOC_MT6873) += snd-soc-mt6873-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6873_MT6359) += mt6873-mt6359.o

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