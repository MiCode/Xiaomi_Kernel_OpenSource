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

# drivers/dispsys

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/ \
             -I$(srctree)/drivers/misc/mediatek/ext_disp/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/ \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/lcm/inc \
             -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/leds/
ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
#cmdq driver header need followings for secure case
ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)/include/trustzone
ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
#driver
obj-y += ddp_ovl.o
obj-y += ddp_color.o
obj-y += ddp_rdma.o
obj-y += ddp_wdma.o
obj-y += ddp_aal.o
obj-y += ddp_pwm.o
obj-y += ddp_gamma.o
obj-y += ddp_dither.o
obj-y += ddp_od.o
obj-y += ddp_dsi.o
obj-y += ddp_dpi.o
#obj-y += ddp_dpi1.o
obj-y += ddp_split.o

#common
#obj-y += ddp_drv.o
obj-y += ddp_path.o
obj-y += ddp_debug.o
obj-y += ddp_dump.o

obj-y += ddp_manager.o
obj-y += ddp_irq.o
obj-y += ddp_info.o
obj-y += ddp_color_format.o


obj-y += ddp_mmp.o

#should remove
#obj-y += ddp_misc.o

#new add
obj-y += ddp_ufoe.o ddp_met.o

obj-y += display_recorder.o

ifeq ($(CONFIG_MMPROFILE),y)
ccflags-y += -DDEFAULT_MMP_ENABLE
endif

ccflags-y += -DMET_USER_EVENT_SUPPORT


ifeq ($(CONFIG_MTK_ALPS_BOX_SUPPORT), y)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
else
#ccflags-y += -DMTK_DISP_IDLE_LP
endif

# EOF
