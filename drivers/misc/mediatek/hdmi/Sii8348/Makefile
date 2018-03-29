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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/inc  \
             -I$(srctree)/drivers/misc/mediatek/include/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/ \
             -I$(srctree)/drivers/misc/mediatek/ext_disp/$(MTK_PLATFORM)/

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/

obj-y	+= hdmi_drv.o mhl_linux_tx.o mhl_rcp_inputdev.o mhl_supp.o platform.o si_8348_drv.o si_mdt_inputdev.o si_mhl2_edid_3d.o

