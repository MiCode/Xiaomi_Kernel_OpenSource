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

SSW_DIR=$(srctree)/drivers/misc/mediatek/ssw/$(subst ",,$(CONFIG_CUSTOM_KERNEL_SSW))
ssw_exist=$(shell if [ -d ${SSW_DIR} ]; then echo "y"; else echo "n"; fi;)
ifeq ($(ssw_exist), y)
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/ssw/inc
obj-y += $(subst ",,$(CONFIG_CUSTOM_KERNEL_SSW))/
$(warning "SSW_DIR=$(SSW_DIR)ssw_exist=$(ssw_exist)")
else
$(warning "no $(SSW_DIR) folder, need check config ssw_exist=$(ssw_exist)")
obj-  := dummy.o # avoid build error
endif

