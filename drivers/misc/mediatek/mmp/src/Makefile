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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/

obj-y += mmprofile.o

# while the project is not GMO RAM optimize, other kernel modules can start MMP
#   at init (which will occupy some memory at system boot-up)
ifeq ($(CONFIG_MTK_GMO_RAM_OPTIMIZE),y)
ccflags-y += -DFORBID_MMP_START
endif
