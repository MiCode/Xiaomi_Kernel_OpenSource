# SPDX-License-Identifier: GPL-2.0-only

KCOV_INSTRUMENT := n
KCSAN_SANITIZE := n

obj-$(CONFIG_UNISOC_SCHED) += unisoc-sched.o
unisoc-sched-y += core.o sysctl.o fair.o rt.o cpuload.o
unisoc-sched-$(CONFIG_SCHED_WALT) += walt.o walt_freq.o
unisoc-sched-$(CONFIG_UNISOC_GROUP_CTL) += group_ctl.o
unisoc-sched-$(CONFIG_UNISOC_HUNG_TASK_ENH) += hung_task_enh.o
unisoc-sched-$(CONFIG_UNISOC_SCHEDUTIL_GOV) += unisoc_schedutil.o
unisoc-sched-$(CONFIG_UNISOC_SCHED_PAUSE_CPU) += core_pause.o core_ctl.o
unisoc-sched-$(CONFIG_UNISOC_CPU_NETLINK) += cpu_netlink.o
unisoc-sched-$(CONFIG_UNISOC_INPUT_BOOST) += input_boost.o powerkey_boost.o