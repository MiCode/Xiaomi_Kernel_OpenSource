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


subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/sub2/inc

ifeq ($(CONFIG_MTK_LENS_USE_MODULE), y)

SUB2_CFILES += sub2_lens.c

#SUB2_CFILES  += common/ak7371af/AK7371AF.c

obj-m := sub2af.o
sub2af-objs := $(SUB2_CFILES:.c=.o)

else

obj-y += sub2_lens.o
#obj-y += common/ak7371af/


endif