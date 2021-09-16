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
obj-$(CONFIG_SND_SOC_MT6879) += snd-soc-mt6879-afe.o
snd-soc-mt6879-afe-objs += \
	mt6879-afe-pcm.o \
	mt6879-afe-clk.o \
	mt6879-afe-gpio.o \
	mt6879-dai-adda.o \
	mt6879-afe-control.o \
	mt6879-dai-i2s.o \
	mt6879-dai-hw-gain.o \
	mt6879-dai-src.o \
	mt6879-dai-pcm.o \
	mt6879-dai-hostless.o \
	mt6879-misc-control.o

	#	mt6879-dai-tdm.o \

# machine driver
obj-$(CONFIG_SND_SOC_MT6879_MT6368) += mt6879-mt6368.o
