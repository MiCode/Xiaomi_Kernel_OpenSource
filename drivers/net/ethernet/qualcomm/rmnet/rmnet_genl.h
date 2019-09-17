/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * RMNET Data Generic Netlink
 *
 */

#ifndef _RMNET_GENL_H_
#define _RMNET_GENL_H_

#include <net/genetlink.h>

#define RMNET_CORE_DEBUG 0

#define rm_err(fmt, ...)  \
	do { if (RMNET_CORE_DEBUG) pr_err(fmt, __VA_ARGS__); } while (0)

/* Generic Netlink Definitions */
#define RMNET_CORE_GENL_VERSION 1
#define RMNET_CORE_GENL_FAMILY_NAME "RMNET_CORE"

#define RMNET_CORE_GENL_MAX_PIDS 32

#define RMNET_GENL_SUCCESS (0)
#define RMNET_GENL_FAILURE (-1)

extern int rmnet_core_userspace_connected;

enum {
	RMNET_CORE_GENL_CMD_UNSPEC,
	RMNET_CORE_GENL_CMD_PID_BPS_REQ,
	RMNET_CORE_GENL_CMD_PID_BOOST_REQ,
	__RMNET_CORE_GENL_CMD_MAX,
};

enum {
	RMNET_CORE_GENL_ATTR_UNSPEC,
	RMNET_CORE_GENL_ATTR_STR,
	RMNET_CORE_GENL_ATTR_INT,
	RMNET_CORE_GENL_ATTR_PID_BPS,
	RMNET_CORE_GENL_ATTR_PID_BOOST,
	__RMNET_CORE_GENL_ATTR_MAX,
};

#define RMNET_CORE_GENL_ATTR_MAX (__RMNET_CORE_GENL_ATTR_MAX - 1)

struct rmnet_core_pid_bps_info {
	u64 tx_bps;
	u32 pid;
	u32 boost_remaining_ms;
};

struct rmnet_core_pid_boost_info {
	u32 boost_enabled;
	/* Boost period in ms */
	u32 boost_period;
	u32 pid;
};

struct rmnet_core_pid_bps_req {
	struct rmnet_core_pid_bps_info list[RMNET_CORE_GENL_MAX_PIDS];
	u64 timestamp;
	u16 list_len;
	u8 valid;
};

struct rmnet_core_pid_bps_resp {
	struct rmnet_core_pid_bps_info list[RMNET_CORE_GENL_MAX_PIDS];
	u64 timestamp;
	u16 list_len;
	u8 valid;
};

struct rmnet_core_pid_boost_req {
	struct rmnet_core_pid_boost_info list[RMNET_CORE_GENL_MAX_PIDS];
	u64 timestamp;
	u16 list_len;
	u8 valid;
};

/* Function Prototypes */
int rmnet_core_genl_pid_bps_req_hdlr(struct sk_buff *skb_2,
				     struct genl_info *info);

int rmnet_core_genl_pid_boost_req_hdlr(struct sk_buff *skb_2,
				       struct genl_info *info);

/* Called by vnd select queue */
void rmnet_update_pid_and_check_boost(pid_t pid, unsigned int len,
				      int *boost_enable, u64 *boost_period);

int rmnet_core_genl_init(void);

int rmnet_core_genl_deinit(void);

#endif /*_RMNET_CORE_GENL_H_*/
