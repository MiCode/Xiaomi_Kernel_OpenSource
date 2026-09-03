/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright 2023 Unisoc(Shanghai) Technologies Co.Ltd
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation; version 2.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef _BINDER_NETLINK_H
#define _BINDER_NETLINK_H

#define BINDER_GENL_FAMILY_NAME "unibinder"
#define BINDER_GENL_VERSION 1
#define BINDER_GENL_MC_GRP_NAME "unibinder-event"

/* Binder generic netlink message attributes */
enum binder_genl_attrs {
	BINDER_GENL_ATTR_UNSPEC,
	BINDRE_GENL_ATTR_PID,
	BINDRE_GENL_ATTR_SKIP_RESTORE,
	BINDRE_GENL_ATTR_INHERIT_RT,

	__BINDER_GENL_ATTR_MAX
};
#define BINDER_GENL_ATTR_MAX (__BINDER_GENL_ATTR_MAX - 1)

/* Binder generic netlink message commands */
enum binder_genl_cmds {
	BINDER_GENL_CMD_UNSPEC,
	BINDER_GENL_CMD_SKIP_RESTORE,
	BINDER_GENL_CMD_INHERIT_RT,
	BINDER_GENL_CMD_RM_SPAMMING_TRANS,

	__BINDER_GENL_CMD_MAX,
};
#define BINDER_GENL_CMD_MAX (__BINDER_GENL_CMD_MAX - 1)

/* Binder netlink functions */
#ifdef CONFIG_UNISOC_BINDER_NETLINK
int binder_netlink_init(void);
void binder_netlink_exit(void);
#else
static inline int binder_netlink_init(void)
{
	return 0;
}
static inline void binder_netlink_exit(void)
{
}
#endif

#endif /* _BINDER_NETLINK_H */
