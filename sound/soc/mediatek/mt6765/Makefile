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
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
obj-y += mtk-auddrv-devtree-parser.o
#obj-y += mtk-auddrv-underflow-mach.o

obj-$(CONFIG_MTK_HDMI_TDM) += mtk-soc-pcm-hdmi.o


# New kernel architecture
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)

# MENU Governor Predict(low power)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/

# Common object
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-speaker-amp.o
mtk-soc-mt6765-sound-cycle-dependent-objs += mtk-soc-afe-control.o mt6765-sound.o mtk-soc-afe-connection.o mtk-soc-pcm-dl1-i2s0.o \
                                             mtk-soc-pcm-dl2.o mtk-soc-pcm-voice-md2-bt.o mtk-soc-pcm-voice-md2.o \
                                             mtk-soc-pcm-voice-md1.o mtk-soc-pcm-voice-md1-bt.o mtk-soc-pcm-voice-usb.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-mt6765-sound-cycle-dependent.o
mtk-soc-mt6765-sound-objs += mtk-auddrv-afe.o mtk-soc-pcm-common.o mtk-soc-pcm-dummy.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-mt6765-sound.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-auddrv-gpio.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-capture.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-dl1.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-routing.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-capture2.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-i2s2-adc2.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-voice-usb-echoref.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-dl1-i2s0Dl1.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-i2s0-awb.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-uldlloopback.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-deep-buffer-dl.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-mrgrx.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-mrgrx-awb.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-fm-i2s.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-fm-i2s-awb.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-dl1-awb.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-dl1-bt.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-bt-dai.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-dai-stub.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-dai-routing.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-codec-dummy.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-fmtx.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-tdm-capture.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-pcm-hp-impedance.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-soc-machine.o
obj-$(CONFIG_SND_SOC_MT6765) += mtk-auddrv-clk.o
endif
