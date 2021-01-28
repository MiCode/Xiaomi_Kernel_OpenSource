# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6779-afe-objs := \
	mt6779-afe-pcm.o \
	mt6779-afe-clk.o \
	mt6779-afe-gpio.o \
	mt6779-dai-adda.o \
	mt6779-afe-control.o \
	mt6779-dai-i2s.o \
	mt6779-dai-hw-gain.o \
	mt6779-dai-src.o \
	mt6779-dai-pcm.o \
	mt6779-dai-tdm.o \
	mt6779-dai-hostless.o \
	mt6779-misc-control.o

obj-$(CONFIG_SND_SOC_MT6779) += snd-soc-mt6779-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6779_MT6359) += mt6779-mt6359.o

# MENU Governor Predict(low power)
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
