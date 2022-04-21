# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_SND_SOC_MEDIATEK) += common/
ifneq ($(wildcard $(srctree)/$(src)/mt2701/Makefile),)
obj-$(CONFIG_SND_SOC_MT2701) += mt2701/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6797/Makefile),)
obj-$(CONFIG_SND_SOC_MT6797) += mt6797/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6853/Makefile),)
obj-$(CONFIG_SND_SOC_MT6853) += mt6853/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6873/Makefile),)
obj-$(CONFIG_SND_SOC_MT6873) += mt6873/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8173/Makefile),)
obj-$(CONFIG_SND_SOC_MT8173) += mt8173/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8183/Makefile),)
obj-$(CONFIG_SND_SOC_MT8183) += mt8183/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8188/Makefile),)
obj-$(CONFIG_SND_SOC_MT8188) += mt8188/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8192/Makefile),)
obj-$(CONFIG_SND_SOC_MT8192) += mt8192/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8195/Makefile),)
obj-$(CONFIG_SND_SOC_MT8195) += mt8195/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6983/Makefile),)
obj-$(CONFIG_SND_SOC_MT6983) += mt6983/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6985/Makefile),)
obj-$(CONFIG_SND_SOC_MT6985) += mt6985/
endif

# vow related code
obj-$(CONFIG_MTK_VOW_SUPPORT) += vow/

# audio sound dsp related code
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_dsp/

#scp ultra sound related code
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += ultrasound/

# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
