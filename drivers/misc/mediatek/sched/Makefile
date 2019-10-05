LINUXINCLUDE += -include $(srctree)/kernel/sched/sched.h

# For RT monitor
obj-$(CONFIG_MTK_RT_THROTTLE_MON) += rt_monitor.o

# RQ stats for TLP estimation
obj-$(CONFIG_MTK_SCHED_RQAVG_US) += rq_stats.o
obj-$(CONFIG_MTK_SCHED_RQAVG_KS) += sched_avg.o

# For CPU topology to user space
obj-y += sched_ctl.o

# sysfs interface for user to get EAS related info
obj-y += eas_plus.o

ccflags-y += -DMET_USER_EVENT_SUPPORT
