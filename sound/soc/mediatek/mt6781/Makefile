# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6781-afe-objs := \
	mt6781-afe-pcm.o \
	mt6781-afe-clk.o \
	mt6781-afe-gpio.o \
	mt6781-dai-adda.o \
	mt6781-afe-control.o \
	mt6781-dai-i2s.o \
	mt6781-dai-hw-gain.o \
	mt6781-dai-pcm.o \
	mt6781-dai-src.o \
	mt6781-dai-hostless.o \
	mt6781-misc-control.o

obj-$(CONFIG_SND_SOC_MT6781) += snd-soc-mt6781-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6781_MT6366) += mt6781-mt6366.o

# MENU Governor Predict(low power)
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

ifeq ($(CONFIG_MTK_AUDIODSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/common_v2
endif

# spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

