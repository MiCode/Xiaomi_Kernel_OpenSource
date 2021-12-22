#
# Copyright (C) 2019 MediaTek Inc.
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

obj-y += pob_main.o

obj-y += fpsgo_notify.o
obj-y += qos_notify.o
obj-y += nn_notify.o
obj-y += xpu_notify.o
obj-y += eara_thrm_notify.o

ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += platform/mt6781/
else
obj-y += platform/$(MTK_PLATFORM)/
endif
