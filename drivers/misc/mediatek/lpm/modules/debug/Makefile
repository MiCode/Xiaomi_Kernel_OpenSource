LPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/lpm/modules/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/cpc_v3/

#Get platform name
LPM_MODULE_DEBUG_PLAT_NAME = $(strip $(subst ",, $(CONFIG_MTK_LPM_MODULE_NAME)))

#mtk-lpm debug common source files
ifneq ($(LPM_MODULE_DEBUG_PLAT_NAME), )
include $(LPM_MODULES_FOLDER)/debug/v1/Makefile
include $(LPM_MODULES_FOLDER)/debug/cpc_v3/Makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/cpc_v3/
LPM_MODULE_DEBUG_COMMON = $(lpm_modules_debug_common_objs:%.o=v1/%.o)
LPM_MODULE_DEBUG_COMMON += $(mtk_lpm_debug_cpc_v3_objs:%.o=cpc_v3/%.o)

#mtk-lpm debug platform source files
include $(LPM_MODULES_FOLDER)/debug/${LPM_MODULE_DEBUG_PLAT_NAME}/Makefile
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/${LPM_MODULE_DEBUG_PLAT_NAME}/
ccflags-y += ${lpm_modules_debug_cflags}
LPM_MODULE_PLAT_DEBUG = $(lpm_modules_debug_objs:%.o=${LPM_MODULE_DEBUG_PLAT_NAME}/%.o)

#Generic build
BUILD_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-${LPM_MODULE_DEBUG_PLAT_NAME}
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${BUILD_LPM_DBG_MODUDLE_NAME}.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += $(LPM_MODULE_DEBUG_COMMON) $(LPM_MODULE_PLAT_DEBUG)
endif
