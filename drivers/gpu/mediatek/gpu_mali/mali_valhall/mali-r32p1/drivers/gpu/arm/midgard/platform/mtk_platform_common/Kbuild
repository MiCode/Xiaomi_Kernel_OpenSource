include $(src)/Kbuild-mtk-custom-env

curr := platform/mtk_platform_common

mali_kbase-y += $(curr)/mtk_platform_common.o
mali_kbase-y += $(curr)/mtk_gpu_dvfs.o
mali_kbase-y += $(curr)/mtk_gpu_devfreq_governor.o
mali_kbase-y += $(curr)/mtk_gpu_devfreq_thermal.o
mali_kbase-y += $(curr)/mtk_mfg_counter.o
mali_kbase-y += $(curr)/mtk_gpu_power_sspm_ipi.o

ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/perf_common/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/tinysys_scmi/

