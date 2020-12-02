LPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/lpm/modules/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

#Get platform name
LPM_MODULE_DEBUG_PLAT_NAME = $(strip $(subst ",, $(CONFIG_MTK_LPM_MODULE_NAME)))

ifneq ($(or $(findstring mt6853, $(LPM_MODULE_DEBUG_PLAT_NAME)),\
	$(findstring mt6873, $(LPM_MODULE_DEBUG_PLAT_NAME))),)
MTK_LPM_DBG_VERSION = v1
MTK_LPM_CPC_VERSION = cpc_v3
endif

#mtk-lpm debug common source files
ifneq ($(LPM_MODULE_DEBUG_PLAT_NAME), )
include $(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_DBG_VERSION)/Makefile
include $(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_CPC_VERSION)/Makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_DBG_VERSION)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_CPC_VERSION)/
LPM_MODULE_DEBUG_COMMON = $(lpm_modules_debug_common_objs:%.o=$(MTK_LPM_DBG_VERSION)/%.o)
LPM_MODULE_DEBUG_COMMON += $(mtk_lpm_debug_cpc_objs:%.o=$(MTK_LPM_CPC_VERSION)/%.o)

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
