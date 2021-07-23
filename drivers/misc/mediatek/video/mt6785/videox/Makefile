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

# drivers/video/mtk/Makefile
#

obj-$(CONFIG_MTK_FB) += mtkfb_console.o		\
			disp_assert_layer.o	\
			mtkfb.o			\
			mtkfb_fence.o		\
			fbconfig_kdebug.o	\
			mtkfb_vsync.o		\
			debug.o			\
			primary_display.o	\
			disp_lowpower.o		\
			disp_recovery.o		\
			disp_lcm.o		\
			disp_utils.o		\
			mtk_disp_mgr.o		\
			compat_mtk_disp_mgr.o	\
			disp_helper.o		\
			disp_dts_gpio.o		\
			disp_rect.o		\
			disp_arr.o		\
			disp_partial.o		\
			mtk_ovl.o		\
			layering_rule.o		\
			frame_queue.o		\
			disp_pm_qos.o		\
			disp_cust.o 		\
			disp_tphint.o

#ifneq ($(CONFIG_MTK_FB), y) # already in video/common/
#obj-y += mtkfb_dummy.o
#endif

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/mtdummy/pq_dummy/
ifneq ($(CONFIG_MTK_ION), y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/mtdummy/ion_mtk_dummy/
endif
ifneq ($(CONFIG_MTK_M4U), y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/mtdummy/m4u_dummy/
endif
ifneq ($(CONFIG_MTK_CMDQ), y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/mtdummy/cmdq_dummy/
endif
ifeq ($(wildcard $(srctree)/drivers/misc/mediatek/ext_disp/$(MTK_PLATFORM)/),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/mtdummy/ext_disp_dummy/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/		\
             -I$(srctree)/drivers/misc/mediatek/video/common/		\
             -I$(srctree)/drivers/misc/mediatek/video/common/rdma20/	\
             -I$(srctree)/drivers/misc/mediatek/video/common/wdma20/	\
             -I$(srctree)/drivers/misc/mediatek/video/common/layering_rule_base/v1.2/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/	\
             -I$(srctree)/drivers/misc/mediatek/sync/			\
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach/ \
             -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/base/power/include	\
             -I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v2 \
             -I$(srctree)/drivers/misc/mediatek/smi/			\
             -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
             -I$(srctree)/drivers/misc/mediatek/gpu/ged/include/	\
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/	\
             -I$(srctree)/drivers/devfreq/ \
	     -I$(srctree)/drivers/iommu/

subdir-ccflags-y += -I$(srctree)/drivers/staging/android/ion \
		    -I$(srctree)/drivers/staging/android/ion/mtk

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
		    -I$(srctree)/drivers/misc/mediatek/m4u/2.0

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/ \
		    -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/ext_disp \
	     -I$(srctree)/drivers/misc/mediatek/ext_disp/v2 \
	     -I$(srctree)/drivers/misc/mediatek/ext_disp/$(MTK_PLATFORM)

ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/
endif
ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mhltx/
endif
ifeq ($(CONFIG_MTK_DITHERING_SUPPORT), y)
ccflags-y += -DDITHERING_SUPPORT
endif

ifeq ($(CONFIG_MTK_GPU_SUPPORT), y)
ccflags-y += -DHWGPU_SUPPORT
endif

ifeq ($(MTK_MIRAVISION_SUPPORT), yes)
    ccflags-y += -DCONFIG_FOR_SOURCE_PQ
endif

ccflags-$(CONFIG_MTK_LCM_DEVICE_TREE_SUPPORT) += -DMTK_LCM_DEVICE_TREE_SUPPORT
