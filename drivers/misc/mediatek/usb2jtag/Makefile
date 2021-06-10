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

obj-y += usb2jtag_v1.o
ifeq ($(CONFIG_MTK_ENG_BUILD),y)
CFLAGS_usb2jtag_v1.o := -DDEBUG
endif
ifeq ($(CONFIG_MACH_MT6761),y)
obj-y += mt6765/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif