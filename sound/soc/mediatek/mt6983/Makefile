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
obj-$(CONFIG_SND_SOC_MT6983) += snd-soc-mt6983-afe.o
snd-soc-mt6983-afe-objs += \
	mt6983-afe-pcm.o \
	mt6983-afe-clk.o \
	mt6983-afe-gpio.o \
	mt6983-dai-adda.o \
	mt6983-afe-control.o \
	mt6983-afe-cm.o \
	mt6983-dai-i2s.o \
	mt6983-dai-hw-gain.o \
	mt6983-dai-src.o \
	mt6983-dai-pcm.o \
	mt6983-dai-tdm.o \
	mt6983-dai-etdm.o \
	mt6983-dai-hostless.o \
	mt6983-misc-control.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6983_MT6338) += mt6983-mt6338.o
