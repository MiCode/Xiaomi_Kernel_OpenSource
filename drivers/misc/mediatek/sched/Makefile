obj-$(CONFIG_MTK_SCHEDULER) += scheduler.o
scheduler-y += eas/sched_main.o
scheduler-y += eas/sched_sys_common.o
scheduler-y += eas/eas_plus.o
scheduler-y += eas/topology.o
scheduler-y += common.o
scheduler-y += fair.o
scheduler-$(CONFIG_MTK_CORE_PAUSE)+= eas/core_pause.o
scheduler-$(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)+= eas/rotate.o

obj-$(CONFIG_MTK_CPUFREQ_SUGOV_EXT) += cpufreq_sugov_ext.o
cpufreq_sugov_ext-y += sugov/cpufreq_sugov_main.o
ifeq ($(CONFIG_MTK_GEARLESS_SUPPORT),y)
	cpufreq_sugov_ext-y += mtk_energy_model/v2/static_power.o
else
	cpufreq_sugov_ext-y += mtk_energy_model/v1/static_power.o
	ifeq ($(CONFIG_MTK_LEAKAGE_HIT),y)
		subdir-ccflags-y += -DCFG_LKG_HIT
	endif
endif
cpufreq_sugov_ext-y += sugov/nonlinear_opp_cap.o
cpufreq_sugov_ext-y += common.o

subdir-ccflags-y += -I$(srctree)/kernel/
subdir-ccflags-y += -I$(srctree)/drivers/thermal/mediatek
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/sched/

obj-$(CONFIG_MTK_CORE_CTL) += mtk_core_ctl.o
mtk_core_ctl-y += core_ctl/sched_avg.o
mtk_core_ctl-y += core_ctl/core_ctl.o
