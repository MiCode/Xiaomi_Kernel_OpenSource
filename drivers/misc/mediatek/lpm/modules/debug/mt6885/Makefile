ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/power_gs_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_fs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6885/

obj-y += mt6885_dbg_fs.o
obj-y += mt6885_dbg_lpm_fs.o
obj-y += mt6885_dbg_lpm_rc_fs.o
obj-y += mt6885_dbg_spm_fs.o
obj-y += mt6885_logger.o
obj-y += mt6885_lpm_trace_event.o

obj-y += mt6885_cpupm_dbg.o
mt6885_cpupm_dbg-y += mtk_cpupm_dbg.o
mt6885_cpupm_dbg-y += mtk_cpuidle_status.o
mt6885_cpupm_dbg-y += mtk_cpuidle_cpc.o
mt6885_cpupm_dbg-y += mtk_idle_procfs.o
mt6885_cpupm_dbg-y += mtk_idle_procfs_state.o
mt6885_cpupm_dbg-y += mtk_idle_procfs_cpc.o
mt6885_cpupm_dbg-y += mtk_idle_procfs_profile.o
mt6885_cpupm_dbg-y += mtk_idle_procfs_control.o
