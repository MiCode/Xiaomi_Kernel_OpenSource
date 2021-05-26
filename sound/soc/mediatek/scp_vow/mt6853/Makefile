# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))


# scp spk include path
ifeq ($(CONFIG_MTK_VOW_BARGE_IN_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
endif

# platform code
obj-$(CONFIG_MTK_VOW_BARGE_IN_SUPPORT) += mtk-scp-vow-platform.o
