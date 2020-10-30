LPM_BUILD_FOLDER=$(srctree)/drivers/misc/mediatek/lpm

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(LPM_BUILD_FOLDER)/inc/

subdir-ccflags-y += -I$(LPM_BUILD_FOLDER)/inc/
subdir-ccflags-y += -I$(LPM_BUILD_FOLDER)/lpm_trace_event/
subdir-ccflags-y += -I$(LPM_BUILD_FOLDER)/mtk_lpm_fs/

ifeq ($(CONFIG_MTK_LOW_POWER_MODULE),$(filter $(CONFIG_MTK_LOW_POWER_MODULE),m))
ccflags-y += -DMTK_LPM_MODE_MODULE
subdir-ccflags-y += -DMTK_LPM_MODE_MODULE
endif


#mtk-lpm fs source files
include $(LPM_BUILD_FOLDER)/mtk_lpm_fs/Makefile
ccflags-y += -I$(LPM_BUILD_FOLDER)/mtk_lpm_fs/
MTK_LPM_FS = $(mtk_lpm_fs_objs:%.o=mtk_lpm_fs/%.o)


#mtk-lpm platform source files
include $(LPM_BUILD_FOLDER)/platform/Makefile
ccflags-y += -I$(LPM_BUILD_FOLDER)/platform/
LPM_PLATFORM = $(lpm_platform_objs:%.o=platform/%.o)


#mtk-lpm trace evetn source files
include $(LPM_BUILD_FOLDER)/lpm_trace_event/Makefile
ccflags-y += -I$(LPM_BUILD_FOLDER)/lpm_trace_event/
ccflags-y += -I$(LPM_BUILD_FOLDER)/inc/lpm_trace_event/
ccflags-y += ${lpm_trace_event_cflags}
LPM_TRACEEVENT = $(lpm_trace_event_objs:%.o=lpm_trace_event/%.o)


#mtk-lpm common source files
LPM_MODULE += lpm_module.o
LPM_MODULE += lpm_call.o
LPM_MODULE += lpm_registry.o


#Generic build
LPM_SRC = $(LPM_MODULE) $(MTK_LPM_FS) $(LPM_PLATFORM) $(LPM_TRACEEVENT)
obj-$(CONFIG_MTK_LOW_POWER_MODULE) = mtk-lpm.o
mtk-lpm-objs += $(LPM_SRC)


#Generic subdir build
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += modules/
