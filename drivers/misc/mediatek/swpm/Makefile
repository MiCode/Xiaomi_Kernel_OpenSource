SWPM_BUILD_FOLDER=$(srctree)/drivers/misc/mediatek/swpm

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V3),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v3
endif

ifeq ($(CONFIG_MTK_QOS_FRAMEWORK),m)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(SWPM_BUILD_FOLDER)/inc/

subdir-ccflags-y += -I$(SWPM_BUILD_FOLDER)/inc/
#subdir-ccflags-y += -I$(SWPM_BUILD_FOLDER)/swpm_trace_event/
subdir-ccflags-y += -I$(SWPM_BUILD_FOLDER)/mtk_swpm_fs/

ifeq ($(CONFIG_MTK_SWPM_MODULE),$(filter $(CONFIG_MTK_SWPM_MODULE),m))
ccflags-y += -DMTK_SWPM_KERNEL_MODULE
subdir-ccflags-y += -DMTK_SWPM_KERNEL_MODULE
endif


#mtk-swpm fs source files
include $(SWPM_BUILD_FOLDER)/mtk_swpm_fs/Makefile
ccflags-y += -I$(SWPM_BUILD_FOLDER)/mtk_swpm_fs/
MTK_SWPM_FS = $(mtk_swpm_fs_objs:%.o=mtk_swpm_fs/%.o)


#mtk-swpm platform source files
#include $(SWPM_BUILD_FOLDER)/platform/Makefile
#ccflags-y += -I$(SWPM_BUILD_FOLDER)/platform/
#SWPM_PLATFORM = $(swpm_platform_objs:%.o=platform/%.o)


#mtk-swpm trace evetn source files
#include $(SWPM_BUILD_FOLDER)/swpm_trace_event/Makefile
#ccflags-y += -I$(SWPM_BUILD_FOLDER)/swpm_trace_event/
#ccflags-y += -I$(SWPM_BUILD_FOLDER)/inc/swpm_trace_event/
#ccflags-y += ${swpm_trace_event_cflags}
#SWPM_TRACEEVENT = $(swpm_trace_event_objs:%.o=swpm_trace_event/%.o)


#mtk-swpm common source files
SWPM_MODULE += swpm_module.o
SWPM_MODULE += swpm_module_ext.o
#SWPM_MODULE += swpm_call.o
#SWPM_MODULE += swpm_registry.o


#Generic build
SWPM_SRC = $(SWPM_MODULE) $(MTK_SWPM_FS)
obj-$(CONFIG_MTK_SWPM_MODULE) += mtk-swpm.o
mtk-swpm-objs := $(SWPM_SRC)

#PERF ARM PMU with CAP_SYS_ADMIN privilege
obj-$(CONFIG_MTK_SWPM_PERF_ARMV8_PMU) += mtk-swpm-perf-arm-pmu.o
mtk-swpm-perf-arm-pmu-objs := swpm_perf_arm_pmu.o

#Generic subdir build
obj-$(CONFIG_MTK_SWPM_MODULE) += modules/
