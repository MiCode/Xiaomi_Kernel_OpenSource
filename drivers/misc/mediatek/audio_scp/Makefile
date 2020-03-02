# SPDX-License-Identifier: GPL-2.0

# MTK Platform Support
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

ifeq ($(CONFIG_MTK_AUDIO_SCP_RECOVERY_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
endif

obj-$(CONFIG_MTK_AUDIO_SCP_RECOVERY_SUPPORT) += mtk-audio-scp-misc-driver.o
