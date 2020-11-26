lpm_modules_debug_cflags += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
lpm_modules_debug_cflags += -I$(LPM_MODULES_FOLDER)/include/mt6873/

lpm_modules_debug_objs += mt6873_dbg_init.o
lpm_modules_debug_objs += mt6873_dbg_fs.o
lpm_modules_debug_objs += mt6873_dbg_spm_fs.o
lpm_modules_debug_objs += mt6873_lpm_logger.o
lpm_modules_debug_objs += mt6873_lpm_trace_event.o

ifeq (${CONFIG_MTK_LPM_GS_DUMP_SUPPORT},y)
lpm_modules_debug_objs += mt6873_dbg_power_gs.o
lpm_modules_debug_objs += mtk_power_gs_6359_array.o
lpm_modules_debug_objs += mtk_power_gs_6315_1_array.o
lpm_modules_debug_objs += mtk_power_gs_6315_2_array.o
lpm_modules_debug_objs += mtk_power_gs_6315_3_array.o
lpm_modules_debug_objs += mtk_dcm_array.o
lpm_modules_debug_objs += mtk_cg_array.o
endif
