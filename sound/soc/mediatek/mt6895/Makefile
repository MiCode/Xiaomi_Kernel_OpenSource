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
obj-$(CONFIG_SND_SOC_MT6895) += snd-soc-mt6895-afe.o
snd-soc-mt6895-afe-objs += \
	mt6895-afe-pcm.o \
	mt6895-afe-clk.o \
	mt6895-afe-gpio.o \
	mt6895-dai-adda.o \
	mt6895-afe-control.o \
	mt6895-dai-i2s.o \
	mt6895-dai-hw-gain.o \
	mt6895-dai-src.o \
	mt6895-dai-pcm.o \
	mt6895-dai-hostless.o \
	mt6895-misc-control.o \
	mt6895-dai-tdm.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6895_MT6368) += mt6895-mt6368.o
