#
# Copyright (C) 2020 MediaTek Inc.
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
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/aee/mrdump
subdir-ccflags-y += -I$(srctree)/include/
subdir-ccflags-y += -I$(srctree)/kernel/

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

obj-$(CONFIG_MTK_AEE_HANGDET)	+= aee_hangdet.o

