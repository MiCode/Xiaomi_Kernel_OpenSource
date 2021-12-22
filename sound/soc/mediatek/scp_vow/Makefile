# SPDX-License-Identifier: GPL-2.0

# MTK Platform Support
ifeq ($(CONFIG_MACH_MT6893),y)
MTK_PLATFORM := mt6885
else ifeq ($(CONFIG_MACH_MT6877),y)
MTK_PLATFORM := mt6853
else ifeq ($(CONFIG_MACH_MT6781),y)
MTK_PLATFORM := mt6781
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

# scp vow include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_vow/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_vow/

ifeq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
endif

obj-y += $(MTK_PLATFORM)/

obj-y += mtk-scp-vow-common.o
