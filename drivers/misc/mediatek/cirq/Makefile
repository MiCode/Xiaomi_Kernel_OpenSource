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

ifeq ($(CONFIG_MTK_SYS_CIRQ),y)
obj-y += mt_sys_cirq.o
# include platform-dependent headers
# ccflags-y += -I$(srctree)/drivers/misc/mediatek/cirq/$(CONFIG_MTK_PLATFORM)/inc/
ifeq ($(CONFIG_MT_ENG_BUILD),y)
CFLAGS_mt_sys_cirq.o := -DDEBUG
endif
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
