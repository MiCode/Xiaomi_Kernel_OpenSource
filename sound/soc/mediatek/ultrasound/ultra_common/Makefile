# SPDX-License-Identifier: GPL-2.0

# scp ultra include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/ultrasound/ultra_common

ifneq ($(CONFIG_MTK_ULTRASND_PROXIMITY),)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
endif

obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += mtk-scp-ultra.o
