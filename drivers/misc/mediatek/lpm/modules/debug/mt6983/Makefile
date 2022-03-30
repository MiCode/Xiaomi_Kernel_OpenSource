
LPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/lpm/modules/

MTK_LPM_DBG_VERSION = v1
MTK_LPM_CPC_VERSION = cpc_v3

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_DBG_VERSION)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_CPC_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/mt6983/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(LPM_MODULES_FOLDER)/include/mt6983/

BUILD_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-mt6983
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${BUILD_LPM_DBG_MODUDLE_NAME}.o

#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6983_dbg_init_test.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6983_dbg_init.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6983_dbg_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6983_dbg_spm_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6983_lpm_logger.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6983_lpm_trace_event.o

ifeq (${CONFIG_MTK_LPM_GS_DUMP_SUPPORT},y)
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6983_dbg_power_gs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6363_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6373_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_dcm_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_cg_array.o
endif
