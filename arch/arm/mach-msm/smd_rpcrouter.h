/** arch/arm/mach-msm/smd_rpcrouter.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, The Linux Foundation. All rights reserved.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ARCH_ARM_MACH_MSM_SMD_RPCROUTER_H
#define _ARCH_ARM_MACH_MSM_SMD_RPCROUTER_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/msm_rpcrouter.h>
#include <linux/wakelock.h>

#include <mach/msm_smd.h>
#include <mach/msm_rpcrouter.h>

/* definitions for the R2R wire protcol */

#define RPCROUTER_VERSION			1
#define RPCROUTER_PROCESSORS_MAX		4
#define RPCROUTER_MSGSIZE_MAX			512
#define RPCROUTER_PEND_REPLIES_MAX		32

#define RPCROUTER_CLIENT_BCAST_ID		0xffffffff
#define RPCROUTER_ROUTER_ADDRESS		0xfffffffe

#define RPCROUTER_PID_LOCAL			1

#define RPCROUTER_CTRL_CMD_DATA			1
#define RPCROUTER_CTRL_CMD_HELLO		2
#define RPCROUTER_CTRL_CMD_BYE			3
#define RPCROUTER_CTRL_CMD_NEW_SERVER		4
#define RPCROUTER_CTRL_CMD_REMOVE_SERVER	5
#define RPCROUTER_CTRL_CMD_REMOVE_CLIENT	6
#define RPCROUTER_CTRL_CMD_RESUME_TX		7
#define RPCROUTER_CTRL_CMD_EXIT			8
#define RPCROUTER_CTRL_CMD_PING			9

#define RPCROUTER_DEFAULT_RX_QUOTA	5

#define RPCROUTER_XPRT_EVENT_DATA  1
#define RPCROUTER_XPRT_EVENT_OPEN  2
#define RPCROUTER_XPRT_EVENT_CLOSE 3

/* Restart states for endpoint.
 *
 * Two different bits are specified here, one for
 * the remote server notification (RESTART_PEND_SVR)
 * and one for client notification (RESTART_PEND_NTFY).
 * The client notification is used to ensure that
 * the client gets notified by an ENETRESET return
 * code at least once, even if they miss the actual
 * reset event.  The server notification is used to
 * properly handle the reset state of the endpoint.
 */
#define RESTART_NORMAL 0x0
#define RESTART_PEND_SVR 0x1
#define RESTART_PEND_NTFY 0x2
#define RESTART_PEND_NTFY_SVR (RESTART_PEND_SVR | RESTART_PEND_NTFY)

union rr_control_msg {
	uint32_t cmd;
	struct {
		uint32_t cmd;
		uint32_t prog;
		uint32_t vers;
		uint32_t pid;
		uint32_t cid;
	} srv;
	struct {
		uint32_t cmd;
		uint32_t pid;
		uint32_t cid;
	} cli;
};

struct rr_header {
	uint32_t version;
	uint32_t type;
	uint32_t src_pid;
	uint32_t src_cid;
	uint32_t confirm_rx;
	uint32_t size;
	uint32_t dst_pid;
	uint32_t dst_cid;
};

/* internals */

#define RPCROUTER_MAX_REMOTE_SERVERS		100

struct rr_fragment {
	unsigned char data[RPCROUTER_MSGSIZE_MAX];
	uint32_t length;
	struct rr_fragment *next;
};

struct rr_packet {
	struct list_head list;
	struct rr_fragment *first;
	struct rr_fragment *last;
	struct rr_header hdr;
	uint32_t mid;
	uint32_t length;
};

#define PACMARK_LAST(n) ((n) & 0x80000000)
#define PACMARK_MID(n)  (((n) >> 16) & 0xFF)
#define PACMARK_LEN(n)  ((n) & 0xFFFF)

static inline uint32_t PACMARK(uint32_t len, uint32_t mid, uint32_t first,
			       uint32_t last)
{
	return (len & 0xFFFF) |
	  ((mid & 0xFF) << 16) |
	  ((!!first) << 30) |
	  ((!!last) << 31);
}

struct rr_server {
	struct list_head list;

	uint32_t pid;
	uint32_t cid;
	uint32_t prog;
	uint32_t vers;

	dev_t device_number;
	struct cdev cdev;
	struct device *device;
	struct rpcsvr_platform_device p_device;
	char pdev_name[32];
};

struct rr_remote_endpoint {
	uint32_t pid;
	uint32_t cid;

	int tx_quota_cntr;
	int quota_restart_state;
	spinlock_t quota_lock;
	wait_queue_head_t quota_wait;

	struct list_head list;
};

struct msm_rpc_reply {
	struct list_head list;
	uint32_t pid;
	uint32_t cid;
	uint32_t prog; /* be32 */
	uint32_t vers; /* be32 */
	uint32_t xid; /* be32 */
};

struct msm_rpc_endpoint {
	struct list_head list;

	/* incomplete packets waiting for assembly */
	struct list_head incomplete;
	spinlock_t incomplete_lock;

	/* complete packets waiting to be read */
	struct list_head read_q;
	spinlock_t read_q_lock;
	struct wake_lock read_q_wake_lock;
	wait_queue_head_t wait_q;
	unsigned flags;
	uint32_t forced_wakeup;

	/* restart handling */
	int restart_state;
	spinlock_t restart_lock;
	wait_queue_head_t restart_wait;

	/* modem restart notifications */
	int do_setup_notif;
	void *client_data;
	void (*cb_restart_teardown)(void *client_data);
	void (*cb_restart_setup)(void *client_data);

	/* endpoint address */
	uint32_t pid;
	uint32_t cid;

	/* bound remote address
	 * if not connected (dst_pid == 0xffffffff) RPC_CALL writes fail
	 * RPC_CALLs must be to the prog/vers below or they will fail
	 */
	uint32_t dst_pid;
	uint32_t dst_cid;
	uint32_t dst_prog; /* be32 */
	uint32_t dst_vers; /* be32 */

	/* reply queue for inbound messages */
	struct list_head reply_pend_q;
	struct list_head reply_avail_q;
	spinlock_t reply_q_lock;
	uint32_t reply_cnt;
	struct wake_lock reply_q_wake_lock;

	/* device node if this endpoint is accessed via userspace */
	dev_t dev;
};

enum write_data_type {
	HEADER = 1,
	PACKMARK,
	PAYLOAD,
};

struct rpcrouter_xprt {
	char *name;
	void *priv;

	int (*read_avail)(void);
	int (*read)(void *data, uint32_t len);
	int (*write_avail)(void);
	int (*write)(void *data, uint32_t len, enum write_data_type type);
	int (*close)(void);
};

/* shared between smd_rpcrouter*.c */
void msm_rpcrouter_xprt_notify(struct rpcrouter_xprt *xprt, unsigned event);
int __msm_rpc_read(struct msm_rpc_endpoint *ept,
		   struct rr_fragment **frag,
		   unsigned len, long timeout);

struct msm_rpc_endpoint *msm_rpcrouter_create_local_endpoint(dev_t dev);
int msm_rpcrouter_destroy_local_endpoint(struct msm_rpc_endpoint *ept);

int msm_rpcrouter_create_server_cdev(struct rr_server *server);
int msm_rpcrouter_create_server_pdev(struct rr_server *server);

int msm_rpcrouter_init_devices(void);
void msm_rpcrouter_exit_devices(void);

void get_requesting_client(struct msm_rpc_endpoint *ept, uint32_t xid,
			   struct msm_rpc_client_info *clnt_info);

extern dev_t msm_rpcrouter_devno;
extern struct completion rpc_remote_router_up;
extern struct class *msm_rpcrouter_class;

void xdr_init(struct msm_rpc_xdr *xdr);
void xdr_init_input(struct msm_rpc_xdr *xdr, void *buf, uint32_t size);
void xdr_init_output(struct msm_rpc_xdr *xdr, void *buf, uint32_t size);
void xdr_clean_input(struct msm_rpc_xdr *xdr);
void xdr_clean_output(struct msm_rpc_xdr *xdr);
uint32_t xdr_read_avail(struct msm_rpc_xdr *xdr);
#endif
