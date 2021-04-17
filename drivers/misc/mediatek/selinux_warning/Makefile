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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
obj-$(CONFIG_MTK_SELINUX_AEE_WARNING) += mtk_selinux_aee_warning.o

ifneq ($(wildcard $(srctree)/../vendor/mediatek/internal/selinux_warning_enable),)
mtk_selinux_aee_warning-$(CONFIG_MTK_SELINUX_AEE_WARNING) += mtk_selinux_warning.o
else
mtk_selinux_aee_warning-$(CONFIG_MTK_SELINUX_AEE_WARNING) += dummy.o
endif
