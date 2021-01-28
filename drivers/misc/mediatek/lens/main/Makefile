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


subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/main/inc
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/main/common/lc898122af
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/main/common/lc898212xdaf/inc
ifeq ($(CONFIG_MTK_LENS_BU63165AF_SUPPORT), y)
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/main/common/bu63165af/inc
else
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/main/common/bu63169af/inc
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/timer/timesync/


obj-y  += main_lens.o
obj-y  += common/fp5510e2af/
obj-y  += common/bu6424af/
obj-y  += common/bu6429af/
obj-y  += common/bu64748af/
obj-y  += common/dw9714af/
obj-y  += common/dw9763af/
obj-y  += common/dw9814af/
obj-y  += common/dw9718af/
obj-y  += common/dw9718saf/
obj-y  += common/dw9719taf/
obj-y  += common/lc898212af/
obj-y  += common/lc898212xdaf/
obj-y  += common/lc898214af/
obj-y  += common/lc898217af/
obj-y  += common/lc898217afa/
obj-y  += common/lc898217afb/
obj-y  += common/lc898217afc/
obj-y  += common/lc898122af/
obj-y  += common/wv511aaf/
obj-y  += common/ak7371af/
ifeq ($(CONFIG_MTK_LENS_BU63165AF_SUPPORT), y)
  obj-y  += common/bu63165af/
else
  obj-y  += common/bu63169af/
endif

