
LPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/lpm/modules/

#ifneq ($(or $(findstring mt6853, $(LPM_MODULE_DEBUG_PLAT_NAME)),\
#	$(findstring mt6853, $(LPM_MODULE_DEBUG_PLAT_NAME))),)
MTK_LPM_DBG_VERSION = v1
MTK_LPM_CPC_VERSION = cpc_v3
#endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_DBG_VERSION)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_CPC_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/mt6853/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(LPM_MODULES_FOLDER)/include/mt6853/

BUILD_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-mt6853
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${BUILD_LPM_DBG_MODUDLE_NAME}.o

#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_dbg_init_test.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_dbg_init.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_dbg_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_dbg_spm_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_lpm_logger.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_lpm_trace_event.o

# FIXME
#ifeq (${CONFIG_MTK_LPM_GS_DUMP_SUPPORT},y)
#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_dbg_power_gs.o
#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6359_array.o
#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6315_1_array.o
#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6315_2_array.o
#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6315_3_array.o
#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_dcm_array.o
#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_cg_array.o
#endif

