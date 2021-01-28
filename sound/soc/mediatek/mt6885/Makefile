# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6885-afe-objs := \
	mt6885-afe-pcm.o \
	mt6885-afe-clk.o \
	mt6885-afe-gpio.o \
	mt6885-dai-adda.o \
	mt6885-afe-control.o \
	mt6885-dai-i2s.o \
	mt6885-dai-hw-gain.o \
	mt6885-dai-src.o \
	mt6885-dai-pcm.o \
	mt6885-dai-tdm.o \
	mt6885-dai-hostless.o \
	mt6885-misc-control.o

obj-$(CONFIG_SND_SOC_MT6885) += snd-soc-mt6885-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6885_MT6359) += mt6885-mt6359.o

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