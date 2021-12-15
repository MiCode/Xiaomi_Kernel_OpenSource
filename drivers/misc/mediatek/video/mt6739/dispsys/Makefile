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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/rdma10/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/wdma10/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/rdma20/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/wdma20/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/ \
             -I$(srctree)/drivers/misc/mediatek/base/power/include/ \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/gpu/ged/include/


subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
		    -I$(srctree)/drivers/misc/mediatek/m4u/2.0

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/ \
		    -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/

subdir-ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc


ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/tplay/Tltplay/public/

include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif
endif

#driver
obj-y += ddp_ovl.o
obj-y += ddp_rdma_ex.o
obj-y += ddp_wdma_ex.o
#obj-y += ddp_rsz.o
obj-y += ddp_dsi.o
#obj-y += ddp_split.o
obj-y += ddp_clkmgr.o
#common
obj-y += ddp_drv.o
obj-y += ddp_path.o
obj-y += ddp_mutex.o
obj-y += ddp_m4u.o
obj-y += ddp_debug.o
obj-y += ddp_dump.o

obj-y += ddp_manager.o
obj-y += ddp_irq.o
obj-y += ddp_info.o
obj-y += ddp_color_format.o
obj-y += ddp_pwm_mux.o

#obj-y += data_rgb888_64x64.o
#obj-y += data_rgb888_64x64_golden.o

obj-y += ddp_mmp.o
obj-y += ddp_met.o
obj-y += display_recorder.o

ifeq ($(CONFIG_MMPROFILE),y)
ccflags-y += -DDEFAULT_MMP_ENABLE
endif

ifeq ($(MTK_MIRAVISION_SUPPORT), yes)
    ccflags-y += -DCONFIG_FOR_SOURCE_PQ
endif

ccflags-y += -DMET_USER_EVENT_SUPPORT

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI), "MT8193")
ccflags-y +=	-DHDMI_MT8193_SUPPORT
endif

# EOF
