/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Unisoc (shanghai) Technologies Co., Ltd
 */

#ifndef _CPU_NETLINK_H
#define _CPU_NETLINK_H

#define CPU_GENL_FAMILY_NAME		"uni-cpu"
#define CPU_GENL_VERSION		0x01
#define CPU_GENL_SAMPLING_GROUP_NAME	"sampling"
#define CPU_GENL_EVENT_GROUP_NAME	"event"

/* Attributes of cpu_genl_family */
enum cpu_genl_attr {
	CPU_GENL_ATTR_UNSPEC,
	CPU_GENL_ATTR_CPU_ID,
	CPU_GENL_ATTR_HIGH_LOAD,
	CPU_GENL_ATTR_VIP_COMM,
	CPU_GENL_ATTR_VIP_PID,
	CPU_GENL_ATTR_VIP_FORK_PID,
	CPU_GENL_ATTR_CPU_LOADING,

	__CPU_GENL_ATTR_MAX,
};
#define CPU_GENL_ATTR_MAX (__CPU_GENL_ATTR_MAX - 1)

/* Events of cpu_genl_family */
enum cpu_genl_event {
	CPU_GENL_EVENT_UNSPEC,
	CPU_GENL_EVENT_VIP_FORK,
	CPU_GENL_EVENT_HIGH_LOAD,
	CPU_GENL_EVENT_CPU_LOAD,
	__CPU_GENL_EVENT_MAX,
};
#define CPU_GENL_EVENT_MAX (__CPU_GENL_EVENT_MAX - 1)

/* Commands supported by the cpu_genl_family */
enum cpu_genl_cmd {
	CPU_GENL_CMD_UNSPEC,
	CPU_GENL_CMD_GET_CPU_LOADING,	/* List of cpu loding */
	__CPU_GENL_CMD_MAX,
};
#define CPU_GENL_CMD_MAX (__CPU_GENL_CMD_MAX - 1)

/* Netlink notification function */
#ifdef CONFIG_UNISOC_CPU_NETLINK
int cpu_netlink_init(void);
int cpu_notify_vip_fork_task(int tgid, int pid, const char *name);
#else
static inline int cpu_netlink_init(void)
{
	return 0;
}
static inline int cpu_notify_vip_fork_task(int tgid, int pid, const char *name)
{
	return 0;
}
#endif

#endif /* _CPU_NETLINK_H */
