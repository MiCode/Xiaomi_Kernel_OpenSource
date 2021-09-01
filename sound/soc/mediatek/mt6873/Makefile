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
obj-$(CONFIG_SND_SOC_MT6873) += snd-soc-mt6873-afe.o
snd-soc-mt6873-afe-objs += \
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

# machine driver
obj-$(CONFIG_SND_SOC_MT6873_MT6359P) += mt6873-mt6359p.o
