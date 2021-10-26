ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/power_gs_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_fs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6877/

obj-y += mt6877_dbg_fs.o
obj-y += mt6877_dbg_lpm_fs.o
obj-y += mt6877_dbg_lpm_rc_fs.o
obj-y += mt6877_dbg_spm_fs.o
obj-y += mt6877_logger.o
obj-y += mt6877_lpm_trace_event.o

obj-y += mt6877_cpupm_dbg.o
mt6877_cpupm_dbg-y += mtk_cpupm_dbg.o
mt6877_cpupm_dbg-y += mtk_cpuidle_status.o
mt6877_cpupm_dbg-y += mtk_cpuidle_cpc.o
mt6877_cpupm_dbg-y += mtk_idle_procfs.o
mt6877_cpupm_dbg-y += mtk_idle_procfs_state.o
mt6877_cpupm_dbg-y += mtk_idle_procfs_cpc.o
mt6877_cpupm_dbg-y += mtk_idle_procfs_profile.o
mt6877_cpupm_dbg-y += mtk_idle_procfs_control.o

obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += mt6877_power_gs_compare.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += mtk_power_gs_6315_1_array.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += mtk_power_gs_6315_2_array.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += mtk_power_gs_6365_array.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += mtk_dcm_array.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += mtk_cg_array.o