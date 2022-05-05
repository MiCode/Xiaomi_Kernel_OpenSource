# SPDX-License-Identifier: GPL-2.0

# MENU Governor Predict(low power) include path
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
# spm include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# platform driver
obj-$(CONFIG_SND_SOC_MT6789) += snd-soc-mt6789-afe.o
snd-soc-mt6789-afe-objs += \
	mt6789-afe-pcm.o \
	mt6789-afe-clk.o \
	mt6789-afe-gpio.o \
	mt6789-dai-adda.o \
	mt6789-afe-control.o \
	mt6789-dai-i2s.o \
	mt6789-dai-hw-gain.o \
	mt6789-dai-src.o \
	mt6789-dai-pcm.o \
	mt6789-dai-hostless.o \
	mt6789-misc-control.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6789_MT6366) += mt6789-mt6366.o
