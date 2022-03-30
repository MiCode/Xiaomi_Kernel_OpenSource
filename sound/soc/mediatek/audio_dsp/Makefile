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

# MTK Platform Support

# adsp include path
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

# platform include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common

# object code
snd-soc-audiodsp-common-objs := audio_buf.o mtk-dsp-dai.o mtk-dsp-pcm.o mtk-dsp-common.o mtk-dsp-platform-driver.o mtk-dsp-mem-control.o
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += snd-soc-audiodsp-common.o

# Offload support
obj-$(CONFIG_SND_SOC_MTK_OFFLOAD) += mtk-soc-offload-common.o
