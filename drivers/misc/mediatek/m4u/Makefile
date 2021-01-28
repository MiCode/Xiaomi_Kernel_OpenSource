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


ifeq ($(CONFIG_MTK_M4U),y)

#platform code
$(info M4U platform folder:$(CONFIG_MTK_PLATFORM))
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

#version code
ifneq (,$(filter $(strip $(CONFIG_MTK_PLATFORM)), "mt6761" "mt6765"))
    $(info M4U version:2.0)
    obj-y += 2.0/
else ifneq (,$(filter $(strip $(CONFIG_MTK_PLATFORM)), "mt6779"))
    $(info M4U version:3.2)
    obj-y += 3.2/
else
    $(error M4U error: platform error)
endif


#m4u related macro define
ifeq ($(CONFIG_FPGA_EARLY_PORTING),y)
    subdir-ccflags-y += -DM4U_FPGAPORTING
endif

endif

