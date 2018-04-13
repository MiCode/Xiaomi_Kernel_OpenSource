ifdef CONFIG_FUNCTION_TRACER
CFLAGS_REMOVE_clock.o = -pg
endif

ifneq ($(CONFIG_SCHED_OMIT_FRAME_POINTER),y)
# According to Alan Modra <alan@linuxcare.com.au>, the -fno-omit-frame-pointer is
# needed for x86 only.  Why this used to be enabled for all architectures is beyond
# me.  I suspect most platforms don't need this, but until we know that for sure
# I turn this off for IA-64 only.  Andreas Schwab says it's also needed on m68k
# to get a correct value for the wait-channel (WCHAN in ps). --davidm
CFLAGS_core.o := $(PROFILING) -fno-omit-frame-pointer
endif

ifdef CONFIG_SCHED_QHMP
obj-y += qhmp_core.o qhmp_fair.o qhmp_rt.o
else
obj-y += core.o fair.o rt.o
endif

obj-y += proc.o clock.o cputime.o
obj-y += idle_task.o deadline.o stop_task.o
obj-y += wait.o completion.o idle.o sched_avg.o
obj-$(CONFIG_SMP) += cpupri.o cpudeadline.o
obj-$(CONFIG_SCHED_AUTOGROUP) += auto_group.o
obj-$(CONFIG_SCHEDSTATS) += stats.o
obj-$(CONFIG_SCHED_DEBUG) += debug.o
obj-$(CONFIG_CGROUP_CPUACCT) += cpuacct.o
obj-$(CONFIG_SCHED_CORE_CTL) += core_ctl.o
