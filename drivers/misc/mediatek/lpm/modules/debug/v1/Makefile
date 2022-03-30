MTK_LPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/lpm/modules/


MTK_LPM_DBG_COMMON_NAME=mtk-lpm-dbg-common-v1

MTK_LPM_CPC_VERSION=cpc_v3
MTK_LPM_GS_DUMP_VERSION=v1
MTK_LPM_SYSSRAM_VERSION=v1

BUILD_LPM_DBG_CPC_NAME=mtk_lpm_debug_cpc_v3

include $(MTK_LPM_MODULES_FOLDER)/debug/$(MTK_LPM_CPC_VERSION)/Makefile

LPM_MODULE_DEBUG_COMMON += $(${BUILD_LPM_DBG_CPC_NAME}_objs:%.o=../$(MTK_LPM_CPC_VERSION)/%.o)


ccflags-y += -I$(MTK_LPM_MODULES_FOLDER)/include/
ccflags-y += -I$(MTK_LPM_MODULES_FOLDER)/debug/$(MTK_LPM_CPC_VERSION)

obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${MTK_LPM_DBG_COMMON_NAME}.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_cpuidle_status.o
${MTK_LPM_DBG_COMMON_NAME}-objs += mtk_cpupm_dbg.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_cpuidle_control.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_cpuidle_fs.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_cpuidle_profile.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_cpuidle_state.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_fs_common.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_trace.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_logger.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_rc_fs.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_timer.o
${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_dbg_pm.o
${MTK_LPM_DBG_COMMON_NAME}-objs += $(LPM_MODULE_DEBUG_COMMON)


${MTK_LPM_DBG_COMMON_NAME}-objs += lpm_pwr_gs.o
${MTK_LPM_DBG_COMMON_NAME}-objs += gs/${MTK_LPM_GS_DUMP_VERSION}/lpm_pwr_gs_common.o
${MTK_LPM_DBG_COMMON_NAME}-objs += gs/${MTK_LPM_GS_DUMP_VERSION}/lpm_pwr_gs_dump_clk.o
${MTK_LPM_DBG_COMMON_NAME}-objs += gs/${MTK_LPM_GS_DUMP_VERSION}/lpm_pwr_gs_dump_pmic.o
