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
obj-$(CONFIG_SND_SOC_MT6885) += snd-soc-mt6885-afe.o
snd-soc-mt6885-afe-objs += \
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

# machine driver
obj-$(CONFIG_SND_SOC_MT6885_MT6359P) += mt6885-mt6359p.o
