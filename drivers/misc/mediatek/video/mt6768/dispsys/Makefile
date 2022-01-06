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

CMDQ_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifeq ($(CONFIG_MTK_PLATFORM), "mt6768")
	MTK_PLATFORM := mt6768
else
	MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/rdma20/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/wdma20/ \
             -I$(srctree)/drivers/misc/mediatek/video/common/layering_rule_base/v1.1/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/ \
             -I$(srctree)/drivers/misc/mediatek/base/power/include/ \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/gpu/ged/include/ \
             -I$(srctree)/drivers/iommu/ \
             -I$(srctree)/drivers/misc/mediatek/smi/variant/ \
			 -I$(srctree)/drivers/staging/android/mtk_ion \
		     -I$(srctree)/drivers/staging/android/mtk_ion/mtk \
             -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM) \
			 -I$(srctree)/drivers/misc/mediatek/cmdq/v3/


subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
		    -I$(srctree)/drivers/misc/mediatek/m4u/2.0

subdir-ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion \
		    -I$(srctree)/drivers/staging/android/mtk_ion/mtk

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/ \
		    -I$(srctree)/drivers/misc/mediatek/cmdq/bridge

ifeq ($(CONFIG_MACH_MT6761),y)
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6765/
else
    subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/

subdir-ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc


ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/tplay/Tltplay/public/

include $(srctree)/drivers/tee/gud/Makefile.include
endif

ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/tplay/Tltplay/public/
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
  include $(srctree)/drivers/tee/gud/Makefile.include
endif
ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
  include $(srctree)/drivers/tee/teei/Makefile.include
endif
endif

endif

#driver
obj-y += ddp_ovl.o
obj-y += ddp_rdma_ex.o
obj-y += ddp_wdma_ex.o
obj-y += ddp_dsi.o
#obj-y += ddp_dpi.o
obj-y += ddp_clkmgr.o
obj-y += ddp_rsz.o
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
ifeq ($(CONFIG_MACH_MT6768),y)
obj-y += ddp_disp_bdg.o
endif
ifeq ($(CONFIG_MMPROFILE), y)
obj-y += ddp_mmp.o
endif
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

ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"DISP")
    ccflags-y += -DDISP_COLOR_ON
else ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"OFF")
    ccflags-y += -DDISP_COLOR_ON
endif

ifeq ($(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/public/
endif
subdir-ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion \
		    -I$(srctree)/drivers/staging/android/mtk_ion/mtk
# EOF
