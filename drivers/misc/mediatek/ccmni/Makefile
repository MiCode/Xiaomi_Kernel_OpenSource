#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ifeq ($(CONFIG_MTK_NET_CCMNI), y)
CCCI_CCMNI_OBJ=y
else ifeq ($(CONFIG_MTK_NET_CCMNI), m)
CCCI_CCMNI_OBJ=m
ccflags-y += -DCCCI_CCMNI_MODULE
else
CCCI_CCMNI_OBJ=n
endif

ifneq ($(CCCI_CCMNI_OBJ), n)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/c2k_ccci/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CCCI_CCMNI_OBJ) += ccmni.o

ccflags-y += -DMET_USER_EVENT_SUPPORT

else
obj-  := dummy.o # avoid build error
endif
