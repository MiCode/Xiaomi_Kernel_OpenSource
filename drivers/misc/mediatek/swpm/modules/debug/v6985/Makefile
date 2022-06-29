
SWPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/swpm/modules/

ifeq ($(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE), m)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cpufreq_lite
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V3),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v3
endif

ifeq ($(CONFIG_MTK_QOS_FRAMEWORK),m)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos
endif

ifeq ($(CONFIG_MTK_THERMAL),y)
ccflags-y += -I$(srctree)/drivers/thermal/mediatek/
endif

MTK_SWPM_DBG_VERSION = v1

ccflags-y += -I$(srctree)/drivers/misc/mediatek/swpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/swpm/mtk_swpm_fs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/swpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/swpm/modules/debug/$(MTK_SWPM_DBG_VERSION)/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/debug/v6985/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/debug/v6985/subsys/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/include/v6985/

BUILD_SWPM_DBG_MODUDLE_NAME=mtk-swpm-dbg-v6985
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6985.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6985_ext.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6985_dbg_init.o

#BUILD_SWPM_CPU_DBG_MODUDLE_NAME=mtk-swpm-cpu-dbg-v6985
#obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_CPU_DBG_MODUDLE_NAME}.o
#${BUILD_SWPM_CPU_DBG_MODUDLE_NAME}-objs += subsys/swpm_cpu_v6985.o
#${BUILD_SWPM_CPU_DBG_MODUDLE_NAME}-objs += subsys/swpm_cpu_v6985_dbg_init.o

BUILD_SWPM_MEM_DBG_MODUDLE_NAME=mtk-swpm-mem-dbg-v6985
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_MEM_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_MEM_DBG_MODUDLE_NAME}-objs += subsys/swpm_mem_v6985.o
${BUILD_SWPM_MEM_DBG_MODUDLE_NAME}-objs += subsys/swpm_mem_v6985_dbg_init.o

BUILD_SWPM_CORE_DBG_MODUDLE_NAME=mtk-swpm-core-dbg-v6985
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_CORE_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_CORE_DBG_MODUDLE_NAME}-objs += subsys/swpm_core_v6985.o
${BUILD_SWPM_CORE_DBG_MODUDLE_NAME}-objs += subsys/swpm_core_v6985_dbg_init.o

BUILD_SMAP_DBG_MODUDLE_NAME=mtk-smap-dbg-v6985
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SMAP_DBG_MODUDLE_NAME}.o
${BUILD_SMAP_DBG_MODUDLE_NAME}-objs += subsys/smap_v6985.o
${BUILD_SMAP_DBG_MODUDLE_NAME}-objs += subsys/smap_v6985_dbg_init.o
