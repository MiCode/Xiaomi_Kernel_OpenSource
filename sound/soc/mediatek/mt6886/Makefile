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
obj-$(CONFIG_SND_SOC_MT6886) += snd-soc-mt6886-afe.o
snd-soc-mt6886-afe-objs += \
	mt6886-afe-pcm.o \
	mt6886-afe-clk.o \
	mt6886-afe-gpio.o \
	mt6886-dai-adda.o \
	mt6886-afe-control.o \
	mt6886-dai-i2s.o \
	mt6886-dai-hw-gain.o \
	mt6886-dai-src.o \
	mt6886-dai-pcm.o \
	mt6886-dai-hostless.o \
	mt6886-misc-control.o

	#	mt6886-dai-tdm.o \

# machine driver
obj-$(CONFIG_SND_SOC_MT6886_MT6368) += mt6886-mt6368.o
