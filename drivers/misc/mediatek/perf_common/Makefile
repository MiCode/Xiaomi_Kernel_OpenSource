NUXINCLUDE += -include $(srctree)/kernel/sched/sched.h
ccflags-y += -I$(src)              # needed for trace events
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/

ifeq ($(CONFIG_MTK_GPU_SWPM_SUPPORT), y)
MTK_GPU_VERSION = $(subst ",,$(CONFIG_MTK_GPU_VERSION))
$(info *MTK_GPU_VERSION 2 = $(word 2,$(MTK_GPU_VERSION)))
$(info *MTK_GPU_VERSION 3 = $(word 3,$(MTK_GPU_VERSION)))
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpu_mali/mali_$(word 2,$(MTK_GPU_VERSION))/mali-$(word 3,$(MTK_GPU_VERSION))/drivers/gpu/arm/midgard/platform/mtk_platform_common/
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_PERF_COMMON) += mtk_perf_common.o
mtk_perf_common-y += perf_common.o
# a tracker for performance index
mtk_perf_common-$(CONFIG_MTK_PERF_TRACKER)+= perf_tracker.o
