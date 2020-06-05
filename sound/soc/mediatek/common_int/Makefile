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
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
obj-y += mtk-auddrv-devtree-parser.o
#obj-y += mtk-auddrv-underflow-mach.o

ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),)
# MTK Codec Chip Support
MTK_CODEC := $(subst ",,$(CONFIG_MTK_SND_SOC_CODEC))
# PMIC include path
ifneq  ($(MTK_CODEC),)
obj-y += mtk-auddrv-gpio.o
endif
endif

obj-$(CONFIG_MTK_HDMI_TDM) += mtk-soc-pcm-hdmi.o

# Feature support
ifeq ($(CONFIG_SND_SOC_MTK_AUDIO_DSP),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
obj-y += mtk-soc-offload-common.o
endif
ifeq ($(CONFIG_MTK_AUDIODSP_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)
endif


# New kernel architecture
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)

# Feature support
ifeq ($(CONFIG_SND_SOC_MTK_AUDIO_DSP),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
endif

# MENU Governor Predict(low power)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/

# Feature support
ifeq ($(CONFIG_SND_SOC_MTK_SCP_SMARTPA),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/tasks/spkprotect
endif
# Common object
obj-y += mtk-soc-speaker-amp.o
obj-y += mtk-auddrv-afe.o mtk-auddrv-gpio.o
obj-y += mtk-soc-afe-control.o mtk-soc-pcm-common.o
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += mtk-soc-offload-common.o
obj-$(CONFIG_SND_SOC_MTK_SCP_SMARTPA) += mtk-soc-spkprotect-common.o mtk-soc-pcm-dl1-scpspk.o mtk-soc-pcm-voice-scp.o
obj-y += mtk-soc-pcm-capture.o mtk-soc-pcm-dl1.o mtk-soc-pcm-dummy.o mtk-soc-pcm-routing.o mtk-soc-pcm-capture2.o mtk-soc-pcm-i2s2-adc2.o
obj-y += mtk-soc-pcm-voice-md1.o mtk-soc-pcm-voice-md2.o mtk-soc-pcm-voice-md1-bt.o mtk-soc-pcm-voice-md2-bt.o
obj-y += mtk-soc-pcm-voice-usb.o mtk-soc-pcm-voice-usb-echoref.o
obj-y += mtk-soc-pcm-dl1-i2s0.o mtk-soc-pcm-dl1-i2s0Dl1.o mtk-soc-pcm-i2s0-awb.o
#obj-y += mtk-soc-pcm-hdmi.o
obj-y += mtk-soc-pcm-uldlloopback.o mtk-soc-pcm-dl2.o mtk-soc-pcm-deep-buffer-dl.o
#obj-y += mtk-soc-pcm-mod-dai.o
#obj-y += mtk-soc-pcm_anc.o
#obj-y += mtk-soc-pcm-voice-ultra.o
obj-y += mtk-soc-pcm-mrgrx.o mtk-soc-pcm-mrgrx-awb.o mtk-soc-pcm-fm-i2s.o mtk-soc-pcm-fm-i2s-awb.o
obj-y += mtk-soc-pcm-dl1-awb.o mtk-soc-pcm-dl1-bt.o mtk-soc-pcm-bt-dai.o
obj-y += mtk-soc-dai-stub.o mtk-soc-dai-routing.o mtk-soc-codec-dummy.o mtk-soc-pcm-fmtx.o mtk-soc-tdm-capture.o
obj-y += mtk-soc-pcm-hp-impedance.o
obj-y += mtk-soc-machine.o

endif
