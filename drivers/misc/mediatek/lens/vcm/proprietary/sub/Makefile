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

MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek


subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/vcm/proprietary/sub/inc

obj-$(CONFIG_MTK_LENS)    += subaf.o

subaf-y  += sub_lens.o
subaf-y  += common/bu6424af/BU6424AF.o
subaf-y  += common/bu6429af/BU6429AF.o
subaf-y  += common/dw9714af/DW9714AF.o
subaf-y  += common/dw9814af/DW9814AF.o
subaf-y  += common/dw9718af/DW9718AF.o
subaf-y  += common/lc898214af/LC898214AF.o
subaf-y  += common/wv511aaf/WV511AAF.o
subaf-y  += common/ak7371af/AK7371AF.o