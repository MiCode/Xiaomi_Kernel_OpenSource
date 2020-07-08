
mtk_lpm_modules_debug_cflags += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
mtk_lpm_modules_debug_cflags += -I$(MTK_LPM_MODULES_FOLDER)/include/mt6873/

mtk_lpm_modules_debug_objs += mt6873_dbg_init.o
mtk_lpm_modules_debug_objs += mt6873_dbg_fs.o
mtk_lpm_modules_debug_objs += mt6873_dbg_lpm_fs.o
mtk_lpm_modules_debug_objs += mt6873_dbg_lpm_rc_fs.o
mtk_lpm_modules_debug_objs += mt6873_dbg_spm_fs.o
mtk_lpm_modules_debug_objs += mt6873_logger.o
mtk_lpm_modules_debug_objs += mt6873_lpm_trace_event.o

mtk_lpm_modules_debug_objs += mtk_cpuidle_cpc.o
mtk_lpm_modules_debug_objs += mtk_cpuidle_status.o
mtk_lpm_modules_debug_objs += mtk_cpupm_dbg.o
mtk_lpm_modules_debug_objs += mtk_idle_procfs.o
mtk_lpm_modules_debug_objs += mtk_idle_procfs_state.o
mtk_lpm_modules_debug_objs += mtk_idle_procfs_cpc.o
mtk_lpm_modules_debug_objs += mtk_idle_procfs_profile.o
mtk_lpm_modules_debug_objs += mtk_idle_procfs_control.o

