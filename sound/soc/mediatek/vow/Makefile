# SPDX-License-Identifier: GPL-2.0

# scp vow include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/vow

ifneq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
endif

obj-$(CONFIG_MTK_VOW_SUPPORT) += mtk-scp-vow.o
