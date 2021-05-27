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
subdir-ccflags-y += -Werror

# scp smartpa related code
obj-$(CONFIG_SND_SOC_MTK_SCP_SMARTPA) += scp_spk/
obj-$(CONFIG_SND_SOC_MTK_SCP_SMARTPA) += utility/

# scp vow bargein related code
obj-$(CONFIG_MTK_VOW_SUPPORT) += scp_vow/

#scp ultra sound related code
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += scp_ultra/

# dsp related code
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_dsp/
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += common_int/

# btcvsd
obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += common/
obj-$(CONFIG_SND_SOC_MEDIATEK) += common/
obj-$(CONFIG_SND_SOC_MT2701) += mt2701/
obj-$(CONFIG_SND_SOC_MT6768) += mt6768/
obj-$(CONFIG_SND_SOC_MT8168) += mt8168/
obj-$(CONFIG_SND_SOC_MT6785) += mt6785/
obj-$(CONFIG_SND_SOC_MT8173) += mt8173/
obj-$(CONFIG_SND_SOC_MT6877) += mt6877/
obj-$(CONFIG_SND_SOC_MT6833) += mt6833/
obj-$(CONFIG_SND_SOC_MT6853) += mt6853/
obj-$(CONFIG_SND_SOC_MT6873) += mt6873/
obj-$(CONFIG_SND_SOC_MT6885) += mt6885/
obj-$(CONFIG_SND_SOC_MT8167) += mt8167/
obj-$(CONFIG_SND_SOC_MT6781) += mt6781/

# mtk new arch
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),)
# MTK Codec Chip Support
MTK_CODEC := $(subst ",,$(CONFIG_MTK_SND_SOC_CODEC))

# PMIC include path
ifneq  ($(MTK_CODEC),)
subdir-ccflags-y += -Werror -Werror -Icodec/$(MTK_CODEC)/
subdir-ccflags-y += -Werror -I$(src)/common_int

# Update pmic built flow to avoid codec init later than codec probe
obj-y += codec/$(MTK_CODEC)/
obj-y += common_int/

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
# flags for not ready feature, uncomment after ready
subdir-ccflags-y += -D_GIT318_PMIC_READY
subdir-ccflags-y += -DAUDIO_USING_WRAP_DRIVER
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/
endif
endif

# adsp
ifeq ($(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/tasks/call/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp
endif

ifeq ($(CONFIG_SND_SOC_MTK_AUDIO_DSP),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/tasks/audplayback/
endif

ifeq ($(CONFIG_SND_SOC_MTK_SMART_PHONE),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
endif

# New architecture
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)

# MTK Platform Support
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

# MTK Codec Chip Support
MTK_CODEC := $(subst ",,$(CONFIG_MTK_SND_SOC_CODEC))

# flags for not ready feature, uncomment after ready
subdir-ccflags-y += -DAUDIO_USING_WRAP_DRIVER
subdir-ccflags-y += -D_MT_IDLE_HEADER
subdir-ccflags-y += -D_REG_SYNC_WRITE

# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/accdet/mt6358
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-${CONFIG_MACH_MT6833} += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6833/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-${CONFIG_MACH_MT6833} += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/mt6833/
subdir-ccflags-${CONFIG_MACH_MT6833} += -Werror -I$(srctree)/drivers/misc/mediatek/dramc/mt6833/
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/

ifeq ($(CONFIG_MACH_MT6833),y)
AUDIO_PLATFORM := mt6833
else
AUDIO_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/scp/$(AUDIO_PLATFORM)/
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/audio_ipi/$(AUDIO_PLATFORM)/

# Common include path
subdir-ccflags-y += -Werror -I$(src)/common_int

# PMIC include path
ifneq  ($(MTK_CODEC),)
subdir-ccflags-y += -Werror -I$(src)/codec/$(MTK_CODEC)
endif

subdir-ccflags-y += -Werror -Wno-incompatible-pointer-types

# Update pmic built flow to avoid codec init later than codec probe
ifneq  ($(MTK_CODEC),)
obj-y += codec/$(MTK_CODEC)/
endif
obj-y += common_int/


obj-y += $(AUDIO_PLATFORM)/

# AP include path
subdir-ccflags-y += -Werror -I$(src)/$(AUDIO_PLATFORM)
endif
