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


subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/mtk/main2/inc
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/mtk/main2/common/lc898212xdaf_f/inc

obj-$(CONFIG_MTK_LENS)    += main2af.o

main2af-y  += main2_lens.o
main2af-y  += common/ak7371af/AK7371AF.o
main2af-y  += common/ak7374af/AK7374AF.o
main2af-y  += common/bu64748af/bu64748_function.o
main2af-y  += common/bu64748af/bu64748af.o
main2af-y  += common/lc898212xdaf_f/LC89821x_STMV.o
main2af-y  += common/lc898212xdaf_f/LC898212XDAF_F.o
main2af-y  += common/lc898217af/LC898217AF.o
main2af-y  += common/lc898217afa/LC898217AFA.o
main2af-y  += common/lc898217afb/LC898217AFB.o
main2af-y  += common/lc898217afc/LC898217AFC.o
main2af-y  += common/dw9718taf/DW9718TAF.o
main2af-y  += common/gt9772af/GT9772AF.o