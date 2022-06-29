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
obj-$(CONFIG_SND_SOC_MT6985) += snd-soc-mt6985-afe.o
snd-soc-mt6985-afe-objs += \
	mt6985-afe-pcm.o \
	mt6985-afe-clk.o \
	mt6985-afe-gpio.o \
	mt6985-dai-adda.o \
	mt6985-afe-control.o \
	mt6985-afe-cm.o \
	mt6985-dai-i2s.o \
	mt6985-dai-hw-gain.o \
	mt6985-dai-src.o \
	mt6985-dai-pcm.o \
	mt6985-dai-tdm.o \
	mt6985-dai-etdm.o \
	mt6985-dai-hostless.o \
	mt6985-misc-control.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6985_MT6338) += mt6985-mt6338.o
