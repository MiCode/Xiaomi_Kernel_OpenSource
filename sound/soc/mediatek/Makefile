# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_SND_SOC_MEDIATEK) += common/
obj-$(CONFIG_SND_SOC_MT2701) += mt2701/
obj-$(CONFIG_SND_SOC_MT6797) += mt6797/
obj-$(CONFIG_SND_SOC_MT6853) += mt6853/
obj-$(CONFIG_SND_SOC_MT6873) += mt6873/
obj-$(CONFIG_SND_SOC_MT6885) += mt6885/
obj-$(CONFIG_SND_SOC_MT8173) += mt8173/
obj-$(CONFIG_SND_SOC_MT8183) += mt8183/

# vow related code
obj-$(CONFIG_MTK_VOW_SUPPORT) += vow/

# audio sound dsp related code
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_dsp/

# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
