# SPDX-License-Identifier: GPL-2.0

# MTK Platform Support
ifeq ($(CONFIG_MACH_MT6893),y)
MTK_PLATFORM := mt6885
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

# scp vow include path
#ifeq ($(CONFIG_MTK_VOW_BARGE_IN_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_vow/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_vow/
#endif

ifeq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
endif

obj-y += $(MTK_PLATFORM)/

obj-$(CONFIG_MTK_VOW_BARGE_IN_SUPPORT) += mtk-scp-vow-common.o
