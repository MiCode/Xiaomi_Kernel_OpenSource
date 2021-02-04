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

GED_TOP = $(srctree)/drivers/misc/mediatek/gpu/ged
MTK_TOP = $(srctree)/drivers/misc/mediatek/

VER:=eng

#ccflags-y += -DGED_KPI_DEBUG
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6759" "mt6799" "mt6763" "mt6739" "mt6758" "mt6775" "mt6771" "mt6765" "mt6761" "mt3967" "mt6779"))
ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6775" "mt6771" "mt6765" "mt6761" "mt6763" "mt6779"))
ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
endif
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6775" "mt6771"))
ccflags-y += -DGED_KPI_CPU_INFO
endif

ifeq ($(CONFIG_MTK_DYNAMIC_FPS_FRAMEWORK_SUPPORT),y)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6759" "mt6799" "mt6763" "mt6775" "mt6771"))
ccflags-y += -DGED_KPI_DFRC
endif
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6759" "mt6799"))
ccflags-y += -DGED_KPI_CPU_TRI_CLUSTER
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739"))
ccflags-y += -DGED_KPI_CPU_SINGLE_CLUSTER
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6765"))
#ccflags-y += -DCONFIG_MTK_QOS_SUPPORT
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761" "mt6765" "mt6779"))
ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
endif
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6779"))
ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_MODULE_LATE_INIT
endif
endif

ifneq ($(strip $(TARGET_BUILD_VARIANT)),)
ifneq ($(strip $(TARGET_BUILD_VARIANT)),eng)
VER:=user
ifeq ($(CONFIG_EXTREME_LOW_RAM), y)
ccflags-y += -DGED_LOG_SIZE_LIMITED
ccflags-y += -DGED_BUFFER_LOG_DISABLE
endif
endif
endif

ccflags-y += -DMET_USER_EVENT_SUPPORT

ccflags-y += \
	     -I$(srctree)/include/ \
	     -I$(MTK_TOP)/include/ \
	     -I$(GED_TOP)/include/ \
	     -I$(MTK_TOP)/base/power/$(MTK_PLATFORM)/ \
	     -I$(srctree)/drivers/misc/mediatek/video/include \
	     -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
	     -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
		 -I$(srctree)/drivers/misc/mediatek/lcm/inc/ \
         -I$(srctree)/drivers/misc/mediatek/dfrc/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6757" "mt8167" "kiboplus"))
ccflags-y += \
		 -I$(srctree)/drivers/misc/mediatek/cmdq/v2 \
		 -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(subst ",,$(CONFIG_MTK_PLATFORM))
else
#$(info *CONFIG_MTK_PLATFORM 1 = $(word 1,$(CONFIG_MTK_PLATFORM)))
ccflags-y += \
		 -I$(srctree)/drivers/misc/mediatek/cmdq/v3

ifeq ($(CONFIG_MACH_MT6761),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6765/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif
endif

#$(info $$CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT is [${CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT}])

#ccflags-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += -DGED_SSPM
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT),y)
#ccflags-y += -DGED_SSPM
endif

ifneq ($(CONFIG_FPGA_EARLY_PORTING), y)
ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DENABLE_COMMON_DVFS \
             -DENABLE_TIMER_BACKUP \
             -DGED_DVFS_ENABLE
else
ccflags-y += -DGED_FDVFS_ENABLE
endif
else
$(info $[GED]CONFIG_FPGA_EARLY_PORTING is [${CONFIG_FPGA_EARLY_PORTING}])
endif

ifeq ($(CONFIG_MTK_FTRACE_DEFAULT_ENABLE),y)
ccflags-y += -DENABLE_GED_SYSTRACE_UTIL
endif


ccflags-y += -DGED_DVFS_DEBUG_BUF

ifeq ($(VER),eng)
ccflags-y += -DGED_DEBUG_IOCTL_LOCK
endif

ccflags-y += \
	-include $(GED_TOP)/include/config_kernel_$(VER).h

obj-y += ged.o

ged-y += \
	src/ged.o \
	src/ged_base.o \
	src/ged_main.o \
	src/ged_debugFS.o \
	src/ged_log.o \
	src/ged_hal.o \
	src/ged_bridge.o \
	src/ged_profile_dvfs.o \
	src/ged_monitor_3D_fence.o \
	src/ged_notify_sw_vsync.o \
	src/ged_hashtable.o \
	src/ged_thread.o \
	src/ged_ge.o \
	src/ged_dvfs.o \
	src/ged_kpi.o \
	src/ged_fdvfs.o \
	src/ged_gpu_tuner.o

