# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y := -Werror $(call cc-disable-warning, unused-but-set-variable)

# scp smartpa related code
obj-$(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT) += scp_spk/
obj-$(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT) += utility/

# scp vow bargein related code
obj-$(CONFIG_MTK_VOW_SUPPORT) += scp_vow/

#scp ultra sound related code
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += scp_ultra/

# dsp related code
ifeq ($(CONFIG_SND_SOC_MTK_AUDIO_DSP),y)
ifeq ($(CONFIG_MACH_MT6779),y)
obj-y += audio_dsp/v1/
obj-$(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT) += common_int/
subdir-ccflags-y += -DAUDIO_DSP_V1
else
obj-y += audio_dsp/v2/
obj-y += common_int/
endif
endif
#obj-$(CONFIG_MTK_VOW_BARGE_IN_SUPPORT) += scp_vow/
# btcvsd
ifeq ($(CONFIG_SND_SOC_MT6779_MT6359),m)
subdir-ccflags-y += -DASOC_TEMP_BYPASS
endif
obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += common/
obj-$(CONFIG_SND_SOC_MEDIATEK) += common/
obj-$(CONFIG_SND_SOC_MT2701) += mt2701/
obj-$(CONFIG_SND_SOC_MT6768) += mt6768/
obj-$(CONFIG_SND_SOC_MT6785) += mt6785/
obj-$(CONFIG_SND_SOC_MT6797) += mt6797/
obj-$(CONFIG_SND_SOC_MT6779) += mt6779/
obj-$(CONFIG_SND_SOC_MT8173) += mt8173/
obj-$(CONFIG_SND_SOC_MT6877) += mt6877/
obj-$(CONFIG_SND_SOC_MT6873) += mt6873/
obj-$(CONFIG_SND_SOC_MT6853) += mt6853/
obj-$(CONFIG_SND_SOC_MT6833) += mt6833/
obj-$(CONFIG_SND_SOC_MT6885) += mt6885/
obj-$(CONFIG_SND_SOC_MT6781) += mt6781/
obj-$(CONFIG_SND_SOC_AW87519) += aw87519/

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

# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

# adsp
ifeq ($(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)/
ifeq ($(CONFIG_MACH_MT6779),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/framework/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/tasks/call/
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/framework/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/tasks/call/
endif
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp
endif

ifeq ($(CONFIG_SND_SOC_MTK_AUDIO_DSP),y)
ifeq ($(CONFIG_MACH_MT6779),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/tasks/audplayback/
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/tasks/audplayback/
endif
endif

ifeq ($(CONFIG_SND_SOC_MTK_SMART_PHONE),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
endif

# New architecture
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)

# MTK Platform Support
MTK_PLATFORM := $(subst $(quote),,$(CONFIG_MTK_PLATFORM))

# MTK Codec Chip Support
MTK_CODEC := $(subst $(quote),,$(CONFIG_MTK_SND_SOC_CODEC))

# flags for not ready feature, uncomment after ready
subdir-ccflags-y += -DAUDIO_USING_WRAP_DRIVER
subdir-ccflags-y += -D_MT_IDLE_HEADER
subdir-ccflags-y += -D_REG_SYNC_WRITE

ifeq ($(CONFIG_SND_SOC_AW87519),y)
subdir-ccflags-y += -DSND_SOC_AW87519
endif

# include path
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/accdet/mt6358
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
subdir-ccflags-${CONFIG_MACH_MT6833} += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6833/include
subdir-ccflags-${CONFIG_MACH_MT6833} += -I$(srctree)/drivers/misc/mediatek/base/power/mt6833/
subdir-ccflags-${CONFIG_MACH_MT6833} += -I$(srctree)/drivers/misc/mediatek/dramc/mt6833/

ifeq ($(CONFIG_MACH_MT6833),y)
AUDIO_PLATFORM := mt6833
else ifeq ($(CONFIG_MACH_MT6761),y)
AUDIO_PLATFORM := mt6765
else
AUDIO_PLATFORM := $(subst $(quote),,$(CONFIG_MTK_PLATFORM))
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(AUDIO_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/$(AUDIO_PLATFORM)/


# Common include path
subdir-ccflags-y += -I$(src)/common_int

# PMIC include path
ifneq ($(MTK_CODEC),)
subdir-ccflags-y += -I$(src)/codec/$(MTK_CODEC)
endif

subdir-ccflags-y += -Wno-incompatible-pointer-types

# Update pmic built flow to avoid codec init later than codec probe
ifneq ($(MTK_CODEC),)
obj-y += codec/$(MTK_CODEC)/
endif
obj-y += common_int/

ifeq ($(MTK_PLATFORM),mt6758)
  obj-y += mt6758_int/
  subdir-ccflags-y += -I$(src)/mt6758_int
else
  obj-y += $(AUDIO_PLATFORM)/

# AP include path
  subdir-ccflags-y += -I$(src)/$(AUDIO_PLATFORM)
endif
endif
