MTK_LPM_GS_DUMP_VERSION = $(strip $(subst ",, $(CONFIG_MTK_LPM_GS_DUMP_VERSION)))

lpm_modules_debug_common_cflags += -I$(MTK_LPM_MODULES_FOLDER)/include/mt6853/
lpm_modules_debug_common_cflags += -I$(MTK_LPM_MODULES_FOLDER)/include/mt6873/
lpm_modules_debug_common_cflags += -I$(MTK_LPM_MODULES_FOLDER)/include/

ccflags-y += -I$(MTK_LPM_MODULES_FOLDER)/include/mt6873/

lpm_modules_debug_common_objs += lpm_dbg_cpuidle_status.o
lpm_modules_debug_common_objs += mtk_cpupm_dbg.o
lpm_modules_debug_common_objs += lpm_dbg_cpuidle_control.o
lpm_modules_debug_common_objs += lpm_dbg_cpuidle_fs.o
lpm_modules_debug_common_objs += lpm_dbg_cpuidle_profile.o
lpm_modules_debug_common_objs += lpm_dbg_cpuidle_state.o
lpm_modules_debug_common_objs += lpm_dbg_fs_common.o
lpm_modules_debug_common_objs += lpm_dbg_trace.o
lpm_modules_debug_common_objs += lpm_dbg_logger.o
lpm_modules_debug_common_objs += lpm_dbg_rc_fs.o
lpm_modules_debug_common_objs += lpm_dbg_timer.o
lpm_modules_debug_common_objs += lpm_dbg_pm.o

ifeq ($(CONFIG_MTK_LPM_GS_DUMP_SUPPORT), y)
lpm_modules_debug_common_objs += lpm_pwr_gs.o
lpm_modules_debug_common_objs += gs/${MTK_LPM_GS_DUMP_VERSION}/lpm_pwr_gs_common.o
lpm_modules_debug_common_objs += gs/${MTK_LPM_GS_DUMP_VERSION}/lpm_pwr_gs_dump_clk.o
lpm_modules_debug_common_objs += gs/${MTK_LPM_GS_DUMP_VERSION}/lpm_pwr_gs_dump_pmic.o
endif
