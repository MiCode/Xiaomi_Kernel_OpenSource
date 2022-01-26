#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#
# Makefile for misc devices that really don't fit anywhere else.
#

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek


subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/sub/inc

ifeq ($(CONFIG_MTK_LENS_USE_MODULE), y)

SUB_CFILES += sub_lens.c

SUB_CFILES  += common/bu6424af/BU6424AF.c
SUB_CFILES  += common/bu6429af/BU6429AF.c
SUB_CFILES += common/dw9714af/DW9714AF.c
SUB_CFILES  += common/dw9814af/DW9814AF.c
SUB_CFILES  += common/dw9718af/DW9718AF.c
SUB_CFILES  += common/lc898214af/LC898214AF.c
SUB_CFILES  += common/wv511aaf/WV511AAF.c
SUB_CFILES  += common/ak7371af/AK7371AF.c

obj-m := subaf.o
subaf-objs := $(SUB_CFILES:.c=.o)

else

obj-y  += sub_lens.o
obj-y  += common/bu6424af/
obj-y  += common/bu6429af/
obj-y  += common/dw9714af/
obj-y  += common/dw9814af/
obj-y  += common/dw9718af/
obj-y  += common/lc898214af/
obj-y  += common/wv511aaf/
obj-y  += common/ak7371af/

endif