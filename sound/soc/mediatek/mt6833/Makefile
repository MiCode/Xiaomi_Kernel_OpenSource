# SPDX-License-Identifier: GPL-2.0

# MENU Governor Predict(low power) include path
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp

# spm include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# platform driver
obj-$(CONFIG_SND_SOC_MT6833) += snd-soc-mt6833-afe.o
snd-soc-mt6833-afe-objs += \
	mt6833-afe-pcm.o \
	mt6833-afe-clk.o \
	mt6833-afe-gpio.o \
	mt6833-dai-adda.o \
	mt6833-afe-control.o \
	mt6833-dai-i2s.o \
	mt6833-dai-hw-gain.o \
	mt6833-dai-src.o \
	mt6833-dai-pcm.o \
	mt6833-dai-hostless.o \
	mt6833-misc-control.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6833_MT6359P) += mt6833-mt6359p.o