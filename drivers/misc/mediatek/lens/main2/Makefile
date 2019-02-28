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


subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/main2/inc
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/main2/common/lc898212xdaf_f/inc

ifeq ($(CONFIG_MTK_LENS_USE_MODULE), y)

MAIN2_CFILES += main2_lens.c

MAIN2_CFILES  += common/lc898212xdaf_f/LC898212XDAF_F.c
MAIN2_CFILES  += common/lc898212xdaf_f/LC89821x_STMV.c
MAIN2_CFILES  += common/lc898217af/LC898217AF.c
MAIN2_CFILES  += common/lc898217afa/LC898217AFA.c
MAIN2_CFILES  += common/lc898217afb/LC898217AFB.c
MAIN2_CFILES  += common/lc898217afc/LC898217AFC.c
MAIN2_CFILES  += common/ak7371af/AK7371AF.c
MAIN2_CFILES  += common/bu6429af/bu64748af.c
MAIN2_CFILES  += common/bu6429af/bu64748_function.c

obj-m := main2af.o
main2af-objs := $(MAIN2_CFILES:.c=.o)

else

obj-y += main2_lens.o
obj-y += common/lc898212xdaf_f/
obj-y += common/ak7371af/
obj-y += common/bu64748af/
obj-y += common/lc898217af/
obj-y  += common/lc898217afa/
obj-y  += common/lc898217afb/
obj-y  += common/lc898217afc/

endif