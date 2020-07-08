MTK_LPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/lpm/modules/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

#Get platform name
MTK_LPM_MODULE_DEBUG_PLAT_NAME = $(strip $(subst ",, $(CONFIG_MTK_LPM_MODULE_NAME)))

#mtk-lpm debug common source files
ifneq ($(MTK_LPM_MODULE_DEBUG_PLAT_NAME), )
include $(MTK_LPM_MODULES_FOLDER)/debug/common/build.mk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/common/
ccflags-y += ${mtk_lpm_modules_debug_common_cflags}
MTK_LPM_MODULE_DEBUG_COMMON = $(mtk_lpm_modules_debug_common_objs:%.o=common/%.o)

#mtk-lpm debug platform source files
include $(MTK_LPM_MODULES_FOLDER)/debug/${MTK_LPM_MODULE_DEBUG_PLAT_NAME}/build.mk
ccflags-y += -I$(MTK_LPM_MODULES_FOLDER)/debug/${MTK_LPM_MODULE_DEBUG_PLAT_NAME}/
ccflags-y += ${mtk_lpm_modules_debug_cflags}
MTK_LPM_MODULE_PLAT_DEBUG = $(mtk_lpm_modules_debug_objs:%.o=${MTK_LPM_MODULE_DEBUG_PLAT_NAME}/%.o)

#Generic build
BUILD_MTK_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-${MTK_LPM_MODULE_DEBUG_PLAT_NAME}
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${BUILD_MTK_LPM_DBG_MODUDLE_NAME}.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-objs += $(MTK_LPM_MODULE_DEBUG_COMMON) $(MTK_LPM_MODULE_PLAT_DEBUG)
endif
