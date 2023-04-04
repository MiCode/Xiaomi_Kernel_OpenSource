// SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# Old kernel architecture
ifdef CONFIG_SND_SOC_MT6739
MTK_PLATFORM := mt6739
else
MTK_PLATFORM := mt6765
endif
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2
obj-y += mtk-auddrv-devtree-parser.o
#obj-y += mtk-auddrv-underflow-mach.o

obj-$(CONFIG_MTK_HDMI_TDM) += mtk-soc-pcm-hdmi.o


# New kernel architecture
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)

# MENU Governor Predict(low power)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcdi/include

# spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/spm

# Common object
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-speaker-amp.o
# To handle circular dependency between common_int and mt6765 APIS
ifdef CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON
mtk-soc-sound-cycle-dependent-objs += mtk-soc-afe-control.o \
				      ./../$(MTK_PLATFORM)/$(MTK_PLATFORM)-sound.o \
				      ./../$(MTK_PLATFORM)/mtk-soc-afe-connection.o \
				      ./../$(MTK_PLATFORM)/mtk-auddrv-clk.o \
				      mtk-soc-pcm-dl1-i2s0.o \
				      mtk-soc-pcm-dl2.o mtk-soc-pcm-voice-md2-bt.o mtk-soc-pcm-voice-md2.o \
				      mtk-soc-pcm-voice-md1.o mtk-soc-pcm-voice-md1-bt.o mtk-soc-pcm-voice-usb.o
endif
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-sound-cycle-dependent.o
mtk-soc-sound-objs += mtk-auddrv-afe.o mtk-soc-pcm-common.o mtk-soc-pcm-dummy.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-sound.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-auddrv-gpio.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-capture.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-dl1.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-routing.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-capture2.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-i2s2-adc2.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-voice-usb-echoref.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-dl1-i2s0Dl1.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-i2s0-awb.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-uldlloopback.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-deep-buffer-dl.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-mrgrx.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-mrgrx-awb.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-fm-i2s.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-fm-i2s-awb.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-dl1-awb.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-dl1-bt.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-bt-dai.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-dai-stub.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-dai-routing.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-codec-dummy.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-fmtx.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-tdm-capture.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-pcm-hp-impedance.o
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mtk-soc-machine.o
endif
