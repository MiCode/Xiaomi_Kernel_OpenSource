MTK_LPM_BUILD_FOLDER=$(srctree)/drivers/misc/mediatek/lpm

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/inc/

subdir-ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/inc/
subdir-ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/mtk_lpm_trace_event/
subdir-ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/mtk_lpm_fs/

ifeq ($(CONFIG_MTK_LOW_POWER_MODULE),$(filter $(CONFIG_MTK_LOW_POWER_MODULE),m))
ccflags-y += -DMTK_LPM_MODE_MODULE
subdir-ccflags-y += -DMTK_LPM_MODE_MODULE
endif


#mtk-lpm fs source files
include $(MTK_LPM_BUILD_FOLDER)/mtk_lpm_fs/build.mk
ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/mtk_lpm_fs/
ccflags-y += ${mtk_lpm_mtk_lpm_fs_cflags}
MTK_LPM_FS = $(mtk_lpm_mtk_lpm_fs_objs:%.o=mtk_lpm_fs/%.o)


#mtk-lpm platform source files
include $(MTK_LPM_BUILD_FOLDER)/platform/build.mk
ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/platform/
ccflags-y += ${mtk_lpm_platform_cflags}
MTK_LPM_PLATFORM = $(mtk_lpm_platform_objs:%.o=platform/%.o)


#mtk-lpm trace evetn source files
include $(MTK_LPM_BUILD_FOLDER)/mtk_lpm_trace_event/build.mk
ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/mtk_lpm_trace_event/
ccflags-y += -I$(MTK_LPM_BUILD_FOLDER)/inc/mtk_lpm_trace_event/
ccflags-y += ${mtk_lpm_trace_event_cflags}
MTK_LPM_TRACEEVENT = $(mtk_lpm_trace_event_objs:%.o=mtk_lpm_trace_event/%.o)


#mtk-lpm common source files
MTK_LPM_MODULE += mtk_lpm_module.o
MTK_LPM_MODULE += mtk_lpm_call.o
MTK_LPM_MODULE += mtk_lpm_registry.o

#Generic build
MTK_LPM_SRC = $(MTK_LPM_MODULE) $(MTK_LPM_FS) $(MTK_LPM_PLATFORM) $(MTK_LPM_TRACEEVENT)

obj-$(CONFIG_MTK_LOW_POWER_MODULE) = mtk-lpm.o
mtk-lpm-objs += $(MTK_LPM_SRC)


#Generic subdir build
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += modules/

