# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_SND_SOC_MEDIATEK) += common/
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt2701),)
obj-$(CONFIG_SND_SOC_MT2701) += mt2701/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6789),)
obj-$(CONFIG_SND_SOC_MT6789) += mt6789/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6797),)
obj-$(CONFIG_SND_SOC_MT6797) += mt6797/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6833),)
obj-$(CONFIG_SND_SOC_MT6833) += mt6833/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6853),)
obj-$(CONFIG_SND_SOC_MT6853) += mt6853/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6873),)
obj-$(CONFIG_SND_SOC_MT6873) += mt6873/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6885),)
obj-$(CONFIG_SND_SOC_MT6885) += mt6885/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6983),)
obj-$(CONFIG_SND_SOC_MT6983) += mt6983/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6879),)
obj-$(CONFIG_SND_SOC_MT6879) += mt6879/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6855),)
obj-$(CONFIG_SND_SOC_MT6855) += mt6855/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt6895),)
obj-$(CONFIG_SND_SOC_MT6895) += mt6895/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt8173),)
obj-$(CONFIG_SND_SOC_MT8173) += mt8173/
endif
ifneq ($(wildcard $(srctree)/sound/soc/mediatek/mt8183),)
obj-$(CONFIG_SND_SOC_MT8183) += mt8183/
endif

# vow related code
obj-$(CONFIG_MTK_VOW_SUPPORT) += vow/

# audio sound dsp related code
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_dsp/

#scp ultra sound related code
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += ultrasound/

obj-$(CONFIG_MTK_SCP_AUDIO) += audio_scp/

# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include

