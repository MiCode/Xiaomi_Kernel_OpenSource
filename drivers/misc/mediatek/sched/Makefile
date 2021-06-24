obj-$(CONFIG_MTK_SCHEDULER) += scheduler.o
scheduler-y += sched_main.o sched_sys_common.o eas_plus.o fair.o core.o static_power.o
scheduler-$(CONFIG_MTK_CORE_PAUSE)+= core_pause.o
scheduler-$(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)+= rotate.o

obj-$(CONFIG_MTK_CPUFREQ_SUGOV_EXT) += cpufreq_sugov_ext.o
cpufreq_sugov_ext-y += cpufreq_sugov_main.o

ccflags-y += -I$(srctree)/kernel/sched/
subdir-ccflags-y += -I$(srctree)/drivers/thermal/mediatek

obj-$(CONFIG_MTK_CORE_CTL) += mtk_core_ctl.o
mtk_core_ctl-y += sched_avg.o
mtk_core_ctl-y += core_ctl.o
