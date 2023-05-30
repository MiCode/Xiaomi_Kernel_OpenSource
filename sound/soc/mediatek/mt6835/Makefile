# SPDX-License-Identifier: GPL-2.0

# MENU Governor Predict(low power) include path
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp

# spm include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# platform driver
obj-$(CONFIG_SND_SOC_MT6835) += snd-soc-mt6835-afe.o
snd-soc-mt6835-afe-objs += \
	mt6835-afe-pcm.o \
	mt6835-afe-clk.o \
	mt6835-afe-gpio.o \
	mt6835-dai-adda.o \
	mt6835-afe-control.o \
	mt6835-dai-i2s.o \
	mt6835-dai-hw-gain.o \
	mt6835-dai-src.o \
	mt6835-dai-pcm.o \
	mt6835-dai-hostless.o \
	mt6835-misc-control.o

	#	mt6835-dai-tdm.o \

# machine driver
obj-$(CONFIG_SND_SOC_MT6835_MT6368) += mt6835-mt6368.o
obj-$(CONFIG_SND_SOC_MT6835_MT6368) += mt6835-mt6377.o
