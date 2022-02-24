
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
ccflags-y += -I$(SWPM_MODULES_FOLDER)/debug/v6855/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/include/v6855/

BUILD_SWPM_DBG_MODUDLE_NAME=mtk-swpm-dbg-v6855
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_DBG_MODUDLE_NAME}.o

${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6855.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6855_ext.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6855_dbg_init.o
