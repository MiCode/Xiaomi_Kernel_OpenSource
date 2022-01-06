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

GED_TOP = $(srctree)/drivers/gpu/mediatek/ged
MTK_TOP = $(srctree)/drivers/misc/mediatek/
GPU_TOP = $(srctree)/drivers/gpu/mediatek/

subdir-ccflags-y += -Wno-error


#
# mt6781 config
#
ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -DMTK_GPUFREQ_V1
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
ccflags-y += -DGED_ENABLE_DVFS_LOADING_MODE
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6781
ccflags-y += \
    -I$(MTK_TOP)/base/power/mt6781/ \
    -I$(srctree)/drivers/misc/mediatek/video/mt6781/videox \
    -I$(srctree)/drivers/misc/mediatek/video/mt6781/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/mt6781
endif

$(info $$GED: CONFIG_MTK_PLATFORM is [${CONFIG_MTK_PLATFORM}])
#
# mt6853 family config (mt6833)
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6853"))
ccflags-y += -DMTK_GPUFREQ_V1
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST
$(info $$GED: CONFIG_MTK_PLATFORM is defined [${CONFIG_MTK_PLATFORM}])

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
ccflags-y += -DGED_ENABLE_DVFS_LOADING_MODE
$(info $$GED: CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT is defined [${CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT}])
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3

ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6833
ccflags-y += \
    -I$(MTK_TOP)/base/power/mt6833/ \
    -I$(srctree)/drivers/misc/mediatek/video/mt6833/videox \
    -I$(srctree)/drivers/misc/mediatek/video/mt6833/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/mt6833
endif

ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6877
ccflags-y += \
    -I$(MTK_TOP)/base/power/mt6877/ \
    -I$(srctree)/drivers/misc/mediatek/video/mt6877/videox \
    -I$(srctree)/drivers/misc/mediatek/video/mt6877/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/mt6877
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6885 family config (mt6893)
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6885"))
ccflags-y += -DMTK_GPUFREQ_V1
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
ccflags-y += -DGED_ENABLE_DVFS_LOADING_MODE
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3

ifeq ($(CONFIG_MACH_MT6893),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6893
ccflags-y += \
    -I$(MTK_TOP)/base/power/mt6893/ \
    -I$(srctree)/drivers/misc/mediatek/video/mt6893/videox \
    -I$(srctree)/drivers/misc/mediatek/video/mt6893/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/mt6893
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6873 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6873"))
ccflags-y += -DMTK_GPUFREQ_V1
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
ccflags-y += -DGED_ENABLE_DVFS_LOADING_MODE
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6785 config
#
ifeq ($(CONFIG_MACH_MT6785),y)
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6768 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
endif

#ccflags-y += -DCONFIG_MTK_QOS_V1_SUPPORT

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6771 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6771"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
endif

#ccflags-y += -DGED_KPI_CPU_INFO
#ccflags-y += -DGED_KPI_DFRC
#ccflags-y += -DCONFIG_MTK_QOS_V1_SUPPORT

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6763 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6763"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
endif

#ccflags-y += -DGED_KPI_DFRC

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6739 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
endif

#ccflags-y += -DGED_KPI_CPU_SINGLE_CLUSTER

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6775 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6775"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
endif

#ccflags-y += -DGED_KPI_CPU_INFO
#ccflags-y += -DGED_KPI_DFRC

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6799 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6799"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST
#ccflags-y += -DGED_KPI_DFRC
#ccflags-y += -DGED_KPI_CPU_TRI_CLUSTER

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6759 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6759"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST
#ccflags-y += -DGED_KPI_DFRC
#ccflags-y += -DGED_KPI_CPU_TRI_CLUSTER

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6758 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6758"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt8168 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8168"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt3967 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt3967"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6757 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6757"))
ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/cmdq/v2 \
    -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt8167 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8167"))
ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/cmdq/v2 \
    -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# mt6761 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif

#
# mt6765 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6765"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif

#
# mt6779 config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6779"))
#ccflags-y += -DGED_KPI_MET_DEBUG
ccflags-y += -DMTK_GED_KPI
ccflags-y += -DGED_KPI_CPU_BOOST

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DGED_ENABLE_FB_DVFS
ccflags-y += -DGED_ENABLE_DYNAMIC_DVFS_MARGIN
ccflags-y += -DGED_CONFIGURE_LOADING_BASE_DVFS_STEP
ccflags-y += -DGED_ENABLE_TIMER_BASED_DVFS_MARGIN
ccflags-y += -DGED_MODULE_LATE_INIT
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif

#
# kiboplus config
#
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "kiboplus"))
ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/cmdq/v2 \
    -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += \
    -I$(MTK_TOP)/base/power/$(subst ",,$(CONFIG_MTK_PLATFORM))/ \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/videox \
    -I$(srctree)/drivers/misc/mediatek/video/$(subst ",,$(CONFIG_MTK_PLATFORM))/dispsys \
    -I$(srctree)/drivers/misc/mediatek/sspm/$(subst ",,$(CONFIG_MTK_PLATFORM))
endif


#
# common config
#
ccflags-y += -DMET_USER_EVENT_SUPPORT -DMTK_GPU_DVFS
ccflags-y += -DGED_DVFS_DEBUG_BUF
ccflags-y += -DCONFIG_MTK_GPU_OPP_STATS_SUPPORT
ccflags-y += -DGED_KPI_DEBUG

VER := eng

ifneq ($(CONFIG_MTK_ENG_BUILD),y)
VER := user
ifeq ($(CONFIG_MTK_ENABLE_GMO), y)
ccflags-y += -DGED_LOG_SIZE_LIMITED
ccflags-y += -DGED_BUFFER_LOG_DISABLE
endif
endif

ifeq ($(VER),eng)
ccflags-y += -DGED_DEBUG_IOCTL_LOCK
endif

ifeq ($(CONFIG_MTK_FTRACE_DEFAULT_ENABLE),y)
ccflags-y += -DENABLE_GED_SYSTRACE_UTIL
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT),y)
#ccflags-y += -DGED_SSPM
endif

#$(info $$CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT is [${CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT}])

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

ccflags-y += \
    -include $(GED_TOP)/include/config_kernel_$(VER).h

ccflags-y += \
    -I$(srctree)/include/ \
    -I$(MTK_TOP)/include/ \
	-I$(MTK_TOP)/include/mt-plat/ \
    -I$(GED_TOP)/include/ \
    -I$(GPU_TOP)/hal/ \
    -I$(GPU_TOP)/ \
    -I$(MTK_TOP)/base/power/include/ \
    -I$(srctree)/drivers/misc/mediatek/video/include \
    -I$(srctree)/drivers/misc/mediatek/lcm/inc/ \
    -I$(srctree)/drivers/misc/mediatek/dfrc/ \
    -I$(srctree)/drivers/gpu/mediatek/gpufreq/ \
    -I$(srctree)/drivers/misc/mediatek/sspm/

obj-$(CONFIG_MTK_GPU_SUPPORT) += ged.o

ifeq ($(CONFIG_DEBUG_FS),y)
ccflags-y += -DGED_DEBUG_FS
ged-y += \
    src/ged_debugFS.o
endif

ged-y += \
    src/ged.o \
    src/ged_base.o \
    src/ged_main.o \
    src/ged_sysfs.o \
    src/ged_hal.o \
    src/ged_log.o \
    src/ged_bridge.o \
    src/ged_monitor_3D_fence.o \
    src/ged_notify_sw_vsync.o \
    src/ged_hashtable.o \
    src/ged_thread.o \
    src/ged_ge.o \
    src/ged_dvfs.o \
    src/ged_gpu_tuner.o \
    src/ged_kpi.o
