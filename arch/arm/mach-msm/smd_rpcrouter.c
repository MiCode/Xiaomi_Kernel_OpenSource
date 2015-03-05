/* arch/arm/mach-msm/smd_rpcrouter.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2013, The Linux Foundation. All rights reserved.
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

/* TODO: handle cases where smd_write() will tempfail due to full fifo */
/* TODO: thread priority? schedule a work to bump it? */
/* TODO: maybe make server_list_lock a mutex */
/* TODO: pool fragments to avoid kmalloc/kfree churn */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/reboot.h>

#include <asm/byteorder.h>

#include <mach/msm_smd.h>
#include <mach/smem_log.h>
#include <mach/subsystem_notif.h>

#include "smd_rpcrouter.h"
#include "modem_notifier.h"
#include "smd_rpc_sym.h"
#include "smd_private.h"

enum {
	SMEM_LOG = 1U << 0,
	RTR_DBG = 1U << 1,
	R2R_MSG = 1U << 2,
	R2R_RAW = 1U << 3,
	RPC_MSG = 1U << 4,
	NTFY_MSG = 1U << 5,
	RAW_PMR = 1U << 6,
	RAW_PMW = 1U << 7,
	R2R_RAW_HDR = 1U << 8,
};
static int msm_rpc_connect_timeout_ms;
module_param_named(connect_timeout, msm_rpc_connect_timeout_ms,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

static int smd_rpcrouter_debug_mask;
module_param_named(debug_mask, smd_rpcrouter_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define DIAG(x...) printk(KERN_ERR "[RR] ERROR " x)

#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
#define D(x...) do { \
if (smd_rpcrouter_debug_mask & RTR_DBG) \
	printk(KERN_ERR x); \
} while (0)

#define RR(x...) do { \
if (smd_rpcrouter_debug_mask & R2R_MSG) \
	printk(KERN_ERR "[RR] "x); \
} while (0)

#define RAW(x...) do { \
if (smd_rpcrouter_debug_mask & R2R_RAW) \
	printk(KERN_ERR "[RAW] "x); \
} while (0)

#define RAW_HDR(x...) do { \
if (smd_rpcrouter_debug_mask & R2R_RAW_HDR) \
	printk(KERN_ERR "[HDR] "x); \
} while (0)

#define RAW_PMR(x...) do { \
if (smd_rpcrouter_debug_mask & RAW_PMR) \
	printk(KERN_ERR "[PMR] "x); \
} while (0)

#define RAW_PMR_NOMASK(x...) do { \
	printk(KERN_ERR "[PMR] "x); \
} while (0)

#define RAW_PMW(x...) do { \
if (smd_rpcrouter_debug_mask & RAW_PMW) \
	printk(KERN_ERR "[PMW] "x); \
} while (0)

#define RAW_PMW_NOMASK(x...) do { \
	printk(KERN_ERR "[PMW] "x); \
} while (0)

#define IO(x...) do { \
if (smd_rpcrouter_debug_mask & RPC_MSG) \
	printk(KERN_ERR "[RPC] "x); \
} while (0)

#define NTFY(x...) do { \
if (smd_rpcrouter_debug_mask & NTFY_MSG) \
	printk(KERN_ERR "[NOTIFY] "x); \
} while (0)
#else
#define D(x...) do { } while (0)
#define RR(x...) do { } while (0)
#define RAW(x...) do { } while (0)
#define RAW_HDR(x...) do { } while (0)
#define RAW_PMR(x...) do { } while (0)
#define RAW_PMR_NO_MASK(x...) do { } while (0)
#define RAW_PMW(x...) do { } while (0)
#define RAW_PMW_NO_MASK(x...) do { } while (0)
#define IO(x...) do { } while (0)
#define NTFY(x...) do { } while (0)
#endif


static LIST_HEAD(local_endpoints);
static LIST_HEAD(remote_endpoints);

static LIST_HEAD(server_list);

static wait_queue_head_t newserver_wait;
static wait_queue_head_t subsystem_restart_wait;

static DEFINE_SPINLOCK(local_endpoints_lock);
static DEFINE_SPINLOCK(remote_endpoints_lock);
static DEFINE_SPINLOCK(server_list_lock);

static LIST_HEAD(rpc_board_dev_list);
static DEFINE_SPINLOCK(rpc_board_dev_list_lock);

static struct workqueue_struct *rpcrouter_workqueue;

static atomic_t next_xid = ATOMIC_INIT(1);
static atomic_t pm_mid = ATOMIC_INIT(1);

static void do_read_data(struct work_struct *work);
static void do_create_pdevs(struct work_struct *work);
static void do_create_rpcrouter_pdev(struct work_struct *work);
static int msm_rpcrouter_close(void);

static DECLARE_WORK(work_create_pdevs, do_create_pdevs);
static DECLARE_WORK(work_create_rpcrouter_pdev, do_create_rpcrouter_pdev);

#define RR_STATE_IDLE    0
#define RR_STATE_HEADER  1
#define RR_STATE_BODY    2
#define RR_STATE_ERROR   3

/* State for remote ep following restart */
#define RESTART_QUOTA_ABORT  1

struct rr_context {
	struct rr_packet *pkt;
	uint8_t *ptr;
	uint32_t state; /* current assembly state */
	uint32_t count; /* bytes needed in this state */
};

struct rr_context the_rr_context;

struct rpc_board_dev_info {
	struct list_head list;

	struct rpc_board_dev *dev;
};

static struct platform_device rpcrouter_pdev = {
	.name		= "oncrpc_router",
	.id		= -1,
};

struct rpcrouter_xprt_info {
	struct list_head list;

	struct rpcrouter_xprt *xprt;

	int remote_pid;
	uint32_t initialized;
	wait_queue_head_t read_wait;
	struct wake_lock wakelock;
	spinlock_t lock;
	uint32_t need_len;
	struct work_struct read_data;
	struct workqueue_struct *workqueue;
	int abort_data_read;
	unsigned char r2r_buf[RPCROUTER_MSGSIZE_MAX];
};

static LIST_HEAD(xprt_info_list);
static DEFINE_MUTEX(xprt_info_list_lock);

DECLARE_COMPLETION(rpc_remote_router_up);
static atomic_t pending_close_count = ATOMIC_INIT(0);

static int msm_rpc_reboot_call(struct notifier_block *this,
			unsigned long code, void *_cmd)
{
	 switch (code) {
	 case SYS_RESTART:
	 case SYS_HALT:
	 case SYS_POWER_OFF:
		msm_rpcrouter_close();
		break;
	 }
	 return NOTIFY_DONE;
}

static struct notifier_block msm_rpc_reboot_notifier = {
	.notifier_call = msm_rpc_reboot_call,
	.priority = 100
};

/*
 * Search for transport (xprt) that matches the provided PID.
 *
 * Note: The calling function must ensure that the mutex
 *       xprt_info_list_lock is locked when this function
 *       is called.
 *
 * @remote_pid	Remote PID for the transport
 *
 * @returns Pointer to transport or NULL if not found
 */
static struct rpcrouter_xprt_info *rpcrouter_get_xprt_info(uint32_t remote_pid)
{
	struct rpcrouter_xprt_info *xprt_info;

	list_for_each_entry(xprt_info, &xprt_info_list, list) {
		if (xprt_info->remote_pid == remote_pid)
			return xprt_info;
	}
	return NULL;
}

static int rpcrouter_send_control_msg(struct rpcrouter_xprt_info *xprt_info,
				      union rr_control_msg *msg)
{
	struct rr_header hdr;
	unsigned long flags = 0;
	int need;

	if (xprt_info->remote_pid == RPCROUTER_PID_LOCAL)
		return 0;

	if (!(msg->cmd == RPCROUTER_CTRL_CMD_HELLO) &&
	    !xprt_info->initialized) {
		printk(KERN_ERR "rpcrouter_send_control_msg(): Warning, "
		       "router not initialized\n");
		return -EINVAL;
	}

	hdr.version = RPCROUTER_VERSION;
	hdr.type = msg->cmd;
	hdr.src_pid = RPCROUTER_PID_LOCAL;
	hdr.src_cid = RPCROUTER_ROUTER_ADDRESS;
	hdr.confirm_rx = 0;
	hdr.size = sizeof(*msg);
	hdr.dst_pid = xprt_info->remote_pid;
	hdr.dst_cid = RPCROUTER_ROUTER_ADDRESS;

	/* TODO: what if channel is full? */

	need = sizeof(hdr) + hdr.size;
	spin_lock_irqsave(&xprt_info->lock, flags);
	while (xprt_info->xprt->write_avail() < need) {
		spin_unlock_irqrestore(&xprt_info->lock, flags);
		msleep(250);
		spin_lock_irqsave(&xprt_info->lock, flags);
	}
	xprt_info->xprt->write(&hdr, sizeof(hdr), HEADER);
	xprt_info->xprt->write(msg, hdr.size, PAYLOAD);
	spin_unlock_irqrestore(&xprt_info->lock, flags);

	return 0;
}

static void modem_reset_cleanup(struct rpcrouter_xprt_info *xprt_info)
{
	struct msm_rpc_endpoint *ept;
	struct rr_remote_endpoint *r_ept;
	struct rr_packet *pkt, *tmp_pkt;
	struct rr_fragment *frag, *next;
	struct msm_rpc_reply *reply, *reply_tmp;
	unsigned long flags;

	if (!xprt_info) {
		pr_err("%s: Invalid xprt_info\n", __func__);
		return;
	}
	spin_lock_irqsave(&local_endpoints_lock, flags);
	/* remove all partial packets received */
	list_for_each_entry(ept, &local_endpoints, list) {
		RR("%s EPT DST PID %x, remote_pid:%d\n", __func__,
			ept->dst_pid, xprt_info->remote_pid);

		if (xprt_info->remote_pid != ept->dst_pid)
			continue;

		D("calling teardown cb %p\n", ept->cb_restart_teardown);
		if (ept->cb_restart_teardown)
			ept->cb_restart_teardown(ept->client_data);
		ept->do_setup_notif = 1;

		/* remove replies */
		spin_lock(&ept->reply_q_lock);
		list_for_each_entry_safe(reply, reply_tmp,
					 &ept->reply_pend_q, list) {
			list_del(&reply->list);
			kfree(reply);
		}
		list_for_each_entry_safe(reply, reply_tmp,
					 &ept->reply_avail_q, list) {
			list_del(&reply->list);
			kfree(reply);
		}
		ept->reply_cnt = 0;
		spin_unlock(&ept->reply_q_lock);

		/* Set restart state for local ep */
		RR("EPT:0x%p, State %d  RESTART_PEND_NTFY_SVR "
			"PROG:0x%08x VERS:0x%08x\n",
			ept, ept->restart_state,
			be32_to_cpu(ept->dst_prog),
			be32_to_cpu(ept->dst_vers));
		spin_lock(&ept->restart_lock);
		ept->restart_state = RESTART_PEND_NTFY_SVR;

		/* remove incomplete packets */
		spin_lock(&ept->incomplete_lock);
		list_for_each_entry_safe(pkt, tmp_pkt,
					 &ept->incomplete, list) {
			list_del(&pkt->list);
			frag = pkt->first;
			while (frag != NULL) {
				next = frag->next;
				kfree(frag);
				frag = next;
			}
			kfree(pkt);
		}
		spin_unlock(&ept->incomplete_lock);

		/* remove all completed packets waiting to be read */
		spin_lock(&ept->read_q_lock);
		list_for_each_entry_safe(pkt, tmp_pkt, &ept->read_q,
					 list) {
			list_del(&pkt->list);
			frag = pkt->first;
			while (frag != NULL) {
				next = frag->next;
				kfree(frag);
				frag = next;
			}
			kfree(pkt);
		}
		spin_unlock(&ept->read_q_lock);

		spin_unlock(&ept->restart_lock);
		wake_up(&ept->wait_q);
	}

	spin_unlock_irqrestore(&local_endpoints_lock, flags);

    /* Unblock endpoints waiting for quota ack*/
	spin_lock_irqsave(&remote_endpoints_lock, flags);
	list_for_each_entry(r_ept, &remote_endpoints, list) {
		spin_lock(&r_ept->quota_lock);
		r_ept->quota_restart_state = RESTART_QUOTA_ABORT;
		RR("Set STATE_PENDING PID:0x%08x CID:0x%08x \n", r_ept->pid,
		   r_ept->cid);
		spin_unlock(&r_ept->quota_lock);
		wake_up(&r_ept->quota_wait);
	}
	spin_unlock_irqrestore(&remote_endpoints_lock, flags);
}

static void modem_reset_startup(struct rpcrouter_xprt_info *xprt_info)
{
	struct msm_rpc_endpoint *ept;
	unsigned long flags;

	spin_lock_irqsave(&local_endpoints_lock, flags);

	/* notify all endpoints that we are coming back up */
	list_for_each_entry(ept, &local_endpoints, list) {
		RR("%s EPT DST PID %x, remote_pid:%d\n", __func__,
			ept->dst_pid, xprt_info->remote_pid);

		if (xprt_info->remote_pid != ept->dst_pid)
			continue;

		D("calling setup cb %d:%p\n", ept->do_setup_notif,
					ept->cb_restart_setup);
		if (ept->do_setup_notif && ept->cb_restart_setup)
			ept->cb_restart_setup(ept->client_data);
		ept->do_setup_notif = 0;
	}

	spin_unlock_irqrestore(&local_endpoints_lock, flags);
}

/*
 * Blocks and waits for endpoint if a reset is in progress.
 *
 * @returns
 *    ENETRESET     Reset is in progress and a notification needed
 *    ERESTARTSYS   Signal occurred
 *    0             Reset is not in progress
 */
static int wait_for_restart_and_notify(struct msm_rpc_endpoint *ept)
{
	unsigned long flags;
	int ret = 0;
	DEFINE_WAIT(__wait);

	for (;;) {
		prepare_to_wait(&ept->restart_wait, &__wait,
				TASK_INTERRUPTIBLE);

		spin_lock_irqsave(&ept->restart_lock, flags);
		if (ept->restart_state == RESTART_NORMAL) {
			spin_unlock_irqrestore(&ept->restart_lock, flags);
			break;
		} else if (ept->restart_state & RESTART_PEND_NTFY) {
			ept->restart_state &= ~RESTART_PEND_NTFY;
			spin_unlock_irqrestore(&ept->restart_lock, flags);
			ret = -ENETRESET;
			break;
		}
		if (signal_pending(current) &&
		   ((!(ept->flags & MSM_RPC_UNINTERRUPTIBLE)))) {
			spin_unlock_irqrestore(&ept->restart_lock, flags);
			ret = -ERESTARTSYS;
			break;
		}
		spin_unlock_irqrestore(&ept->restart_lock, flags);
		schedule();
	}
	finish_wait(&ept->restart_wait, &__wait);
	return ret;
}

static struct rr_server *rpcrouter_create_server(uint32_t pid,
							uint32_t cid,
							uint32_t prog,
							uint32_t ver)
{
	struct rr_server *server;
	unsigned long flags;
	int rc;

	server = kmalloc(sizeof(struct rr_server), GFP_KERNEL);
	if (!server)
		return ERR_PTR(-ENOMEM);

	memset(server, 0, sizeof(struct rr_server));
	server->pid = pid;
	server->cid = cid;
	server->prog = prog;
	server->vers = ver;

	spin_lock_irqsave(&server_list_lock, flags);
	list_add_tail(&server->list, &server_list);
	spin_unlock_irqrestore(&server_list_lock, flags);

	rc = msm_rpcrouter_create_server_cdev(server);
	if (rc < 0)
		goto out_fail;

	return server;
out_fail:
	spin_lock_irqsave(&server_list_lock, flags);
	list_del(&server->list);
	spin_unlock_irqrestore(&server_list_lock, flags);
	kfree(server);
	return ERR_PTR(rc);
}

static void rpcrouter_destroy_server(struct rr_server *server)
{
	unsigned long flags;

	spin_lock_irqsave(&server_list_lock, flags);
	list_del(&server->list);
	spin_unlock_irqrestore(&server_list_lock, flags);
	device_destroy(msm_rpcrouter_class, server->device_number);
	kfree(server);
}

int msm_rpc_add_board_dev(struct rpc_board_dev *devices, int num)
{
	unsigned long flags;
	struct rpc_board_dev_info *board_info;
	int i;

	for (i = 0; i < num; i++) {
		board_info = kzalloc(sizeof(struct rpc_board_dev_info),
				     GFP_KERNEL);
		if (!board_info)
			return -ENOMEM;

		board_info->dev = &devices[i];
		D("%s: adding program %x\n", __func__, board_info->dev->prog);
		spin_lock_irqsave(&rpc_board_dev_list_lock, flags);
		list_add_tail(&board_info->list, &rpc_board_dev_list);
		spin_unlock_irqrestore(&rpc_board_dev_list_lock, flags);
	}

	return 0;
}
EXPORT_SYMBOL(msm_rpc_add_board_dev);

static void rpcrouter_register_board_dev(struct rr_server *server)
{
	struct rpc_board_dev_info *board_info;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&rpc_board_dev_list_lock, flags);
	list_for_each_entry(board_info, &rpc_board_dev_list, list) {
		if (server->prog == board_info->dev->prog) {
			D("%s: registering device %x\n",
			  __func__, board_info->dev->prog);
			list_del(&board_info->list);
			spin_unlock_irqrestore(&rpc_board_dev_list_lock, flags);
			rc = platform_device_register(&board_info->dev->pdev);
			if (rc)
				pr_err("%s: board dev register failed %d\n",
				       __func__, rc);
			kfree(board_info);
			return;
		}
	}
	spin_unlock_irqrestore(&rpc_board_dev_list_lock, flags);
}

static struct rr_server *rpcrouter_lookup_server(uint32_t prog, uint32_t ver)
{
	struct rr_server *server;
	unsigned long flags;

	spin_lock_irqsave(&server_list_lock, flags);
	list_for_each_entry(server, &server_list, list) {
		if (server->prog == prog
		 && server->vers == ver) {
			spin_unlock_irqrestore(&server_list_lock, flags);
			return server;
		}
	}
	spin_unlock_irqrestore(&server_list_lock, flags);
	return NULL;
}

static struct rr_server *rpcrouter_lookup_server_by_dev(dev_t dev)
{
	struct rr_server *server;
	unsigned long flags;

	spin_lock_irqsave(&server_list_lock, flags);
	list_for_each_entry(server, &server_list, list) {
		if (server->device_number == dev) {
			spin_unlock_irqrestore(&server_list_lock, flags);
			return server;
		}
	}
	spin_unlock_irqrestore(&server_list_lock, flags);
	return NULL;
}

struct msm_rpc_endpoint *msm_rpcrouter_create_local_endpoint(dev_t dev)
{
	struct msm_rpc_endpoint *ept;
	unsigned long flags;

	ept = kmalloc(sizeof(struct msm_rpc_endpoint), GFP_KERNEL);
	if (!ept)
		return NULL;
	memset(ept, 0, sizeof(struct msm_rpc_endpoint));
	ept->cid = (uint32_t) ept;
	ept->pid = RPCROUTER_PID_LOCAL;
	ept->dev = dev;

	if ((dev != msm_rpcrouter_devno) && (dev != MKDEV(0, 0))) {
		struct rr_server *srv;
		/*
		 * This is a userspace client which opened
		 * a program/ver devicenode. Bind the client
		 * to that destination
		 */
		srv = rpcrouter_lookup_server_by_dev(dev);
		/* TODO: bug? really? */
		BUG_ON(!srv);

		ept->dst_pid = srv->pid;
		ept->dst_cid = srv->cid;
		ept->dst_prog = cpu_to_be32(srv->prog);
		ept->dst_vers = cpu_to_be32(srv->vers);
	} else {
		/* mark not connected */
		ept->dst_pid = 0xffffffff;
	}

	init_waitqueue_head(&ept->wait_q);
	INIT_LIST_HEAD(&ept->read_q);
	spin_lock_init(&ept->read_q_lock);
	INIT_LIST_HEAD(&ept->reply_avail_q);
	INIT_LIST_HEAD(&ept->reply_pend_q);
	spin_lock_init(&ept->reply_q_lock);
	spin_lock_init(&ept->restart_lock);
	init_waitqueue_head(&ept->restart_wait);
	ept->restart_state = RESTART_NORMAL;
	wake_lock_init(&ept->read_q_wake_lock, WAKE_LOCK_SUSPEND, "rpc_read");
	wake_lock_init(&ept->reply_q_wake_lock, WAKE_LOCK_SUSPEND, "rpc_reply");
	INIT_LIST_HEAD(&ept->incomplete);
	spin_lock_init(&ept->incomplete_lock);

	spin_lock_irqsave(&local_endpoints_lock, flags);
	list_add_tail(&ept->list, &local_endpoints);
	spin_unlock_irqrestore(&local_endpoints_lock, flags);
	return ept;
}

int msm_rpcrouter_destroy_local_endpoint(struct msm_rpc_endpoint *ept)
{
	int rc;
	union rr_control_msg msg = { 0 };
	struct msm_rpc_reply *reply, *reply_tmp;
	unsigned long flags;
	struct rpcrouter_xprt_info *xprt_info;

	/* Endpoint with dst_pid = 0xffffffff corresponds to that of
	** router port. So don't send a REMOVE CLIENT message while
	** destroying it.*/
	spin_lock_irqsave(&local_endpoints_lock, flags);
	list_del(&ept->list);
	spin_unlock_irqrestore(&local_endpoints_lock, flags);
	if (ept->dst_pid != 0xffffffff) {
		msg.cmd = RPCROUTER_CTRL_CMD_REMOVE_CLIENT;
		msg.cli.pid = ept->pid;
		msg.cli.cid = ept->cid;

		RR("x REMOVE_CLIENT id=%d:%08x\n", ept->pid, ept->cid);
		mutex_lock(&xprt_info_list_lock);
		list_for_each_entry(xprt_info, &xprt_info_list, list) {
			rc = rpcrouter_send_control_msg(xprt_info, &msg);
			if (rc < 0) {
				mutex_unlock(&xprt_info_list_lock);
				return rc;
			}
		}
		mutex_unlock(&xprt_info_list_lock);
	}

	/* Free replies */
	spin_lock_irqsave(&ept->reply_q_lock, flags);
	list_for_each_entry_safe(reply, reply_tmp, &ept->reply_pend_q, list) {
		list_del(&reply->list);
		kfree(reply);
	}
	list_for_each_entry_safe(reply, reply_tmp, &ept->reply_avail_q, list) {
		list_del(&reply->list);
		kfree(reply);
	}
	spin_unlock_irqrestore(&ept->reply_q_lock, flags);

	wake_lock_destroy(&ept->read_q_wake_lock);
	wake_lock_destroy(&ept->reply_q_wake_lock);
	kfree(ept);
	return 0;
}

static int rpcrouter_create_remote_endpoint(uint32_t pid, uint32_t cid)
{
	struct rr_remote_endpoint *new_c;
	unsigned long flags;

	new_c = kmalloc(sizeof(struct rr_remote_endpoint), GFP_KERNEL);
	if (!new_c)
		return -ENOMEM;
	memset(new_c, 0, sizeof(struct rr_remote_endpoint));

	new_c->cid = cid;
	new_c->pid = pid;
	init_waitqueue_head(&new_c->quota_wait);
	spin_lock_init(&new_c->quota_lock);

	spin_lock_irqsave(&remote_endpoints_lock, flags);
	list_add_tail(&new_c->list, &remote_endpoints);
	new_c->quota_restart_state = RESTART_NORMAL;
	spin_unlock_irqrestore(&remote_endpoints_lock, flags);
	return 0;
}

static struct msm_rpc_endpoint *rpcrouter_lookup_local_endpoint(uint32_t cid)
{
	struct msm_rpc_endpoint *ept;

	list_for_each_entry(ept, &local_endpoints, list) {
		if (ept->cid == cid)
			return ept;
	}
	return NULL;
}

static struct rr_remote_endpoint *rpcrouter_lookup_remote_endpoint(uint32_t pid,
								   uint32_t cid)
{
	struct rr_remote_endpoint *ept;
	unsigned long flags;

	spin_lock_irqsave(&remote_endpoints_lock, flags);
	list_for_each_entry(ept, &remote_endpoints, list) {
		if ((ept->pid == pid) && (ept->cid == cid)) {
			spin_unlock_irqrestore(&remote_endpoints_lock, flags);
			return ept;
		}
	}
	spin_unlock_irqrestore(&remote_endpoints_lock, flags);
	return NULL;
}

static void handle_server_restart(struct rr_server *server,
				  uint32_t pid, uint32_t cid,
				  uint32_t prog, uint32_t vers)
{
	struct rr_remote_endpoint *r_ept;
	struct msm_rpc_endpoint *ept;
	unsigned long flags;
	r_ept = rpcrouter_lookup_remote_endpoint(pid, cid);
	if (r_ept && (r_ept->quota_restart_state !=
		      RESTART_NORMAL)) {
		spin_lock_irqsave(&r_ept->quota_lock, flags);
		r_ept->tx_quota_cntr = 0;
		r_ept->quota_restart_state =
		RESTART_NORMAL;
		spin_unlock_irqrestore(&r_ept->quota_lock, flags);
		D(KERN_INFO "rpcrouter: Remote EPT Reset %0x\n",
			   (unsigned int)r_ept);
		wake_up(&r_ept->quota_wait);
	}
	spin_lock_irqsave(&local_endpoints_lock, flags);
	list_for_each_entry(ept, &local_endpoints, list) {
		if ((be32_to_cpu(ept->dst_prog) == prog) &&
		    (be32_to_cpu(ept->dst_vers) == vers) &&
		    (ept->restart_state & RESTART_PEND_SVR)) {
			spin_lock(&ept->restart_lock);
			ept->restart_state &= ~RESTART_PEND_SVR;
			spin_unlock(&ept->restart_lock);
			D("rpcrouter: Local EPT Reset %08x:%08x \n",
			  prog, vers);
			wake_up(&ept->restart_wait);
			wake_up(&ept->wait_q);
		}
	}
	spin_unlock_irqrestore(&local_endpoints_lock, flags);
}

static int process_control_msg(struct rpcrouter_xprt_info *xprt_info,
			       union rr_control_msg *msg, int len)
{
	union rr_control_msg ctl = { 0 };
	struct rr_server *server;
	struct rr_remote_endpoint *r_ept;
	int rc = 0;
	unsigned long flags;
	static int first = 1;

	if (len != sizeof(*msg)) {
		RR(KERN_ERR "rpcrouter: r2r msg size %d != %d\n",
		       len, sizeof(*msg));
		return -EINVAL;
	}

	switch (msg->cmd) {
	case RPCROUTER_CTRL_CMD_HELLO:
		RR("o HELLO PID %d\n", xprt_info->remote_pid);
		memset(&ctl, 0, sizeof(ctl));
		ctl.cmd = RPCROUTER_CTRL_CMD_HELLO;
		rpcrouter_send_control_msg(xprt_info, &ctl);

		xprt_info->initialized = 1;

		/* Send list of servers one at a time */
		ctl.cmd = RPCROUTER_CTRL_CMD_NEW_SERVER;

		/* TODO: long time to hold a spinlock... */
		spin_lock_irqsave(&server_list_lock, flags);
		list_for_each_entry(server, &server_list, list) {
			if (server->pid != RPCROUTER_PID_LOCAL)
				continue;
			ctl.srv.pid = server->pid;
			ctl.srv.cid = server->cid;
			ctl.srv.prog = server->prog;
			ctl.srv.vers = server->vers;

			RR("x NEW_SERVER id=%d:%08x prog=%08x:%08x\n",
			   server->pid, server->cid,
			   server->prog, server->vers);

			rpcrouter_send_control_msg(xprt_info, &ctl);
		}
		spin_unlock_irqrestore(&server_list_lock, flags);

		if (first) {
			first = 0;
			queue_work(rpcrouter_workqueue,
				   &work_create_rpcrouter_pdev);
		}
		break;

	case RPCROUTER_CTRL_CMD_RESUME_TX:
		RR("o RESUME_TX id=%d:%08x\n", msg->cli.pid, msg->cli.cid);

		r_ept = rpcrouter_lookup_remote_endpoint(msg->cli.pid,
							 msg->cli.cid);
		if (!r_ept) {
			printk(KERN_ERR
			       "rpcrouter: Unable to resume client\n");
			break;
		}
		spin_lock_irqsave(&r_ept->quota_lock, flags);
		r_ept->tx_quota_cntr = 0;
		spin_unlock_irqrestore(&r_ept->quota_lock, flags);
		wake_up(&r_ept->quota_wait);
		break;

	case RPCROUTER_CTRL_CMD_NEW_SERVER:
		if (msg->srv.vers == 0) {
			pr_err(
			"rpcrouter: Server create rejected, version = 0, "
			"program = %08x\n", msg->srv.prog);
			break;
		}

		RR("o NEW_SERVER id=%d:%08x prog=%08x:%08x\n",
		   msg->srv.pid, msg->srv.cid, msg->srv.prog, msg->srv.vers);

		server = rpcrouter_lookup_server(msg->srv.prog, msg->srv.vers);

		if (!server) {
			server = rpcrouter_create_server(
				msg->srv.pid, msg->srv.cid,
				msg->srv.prog, msg->srv.vers);
			if (!server)
				return -ENOMEM;
			/*
			 * XXX: Verify that its okay to add the
			 * client to our remote client list
			 * if we get a NEW_SERVER notification
			 */
			if (!rpcrouter_lookup_remote_endpoint(msg->srv.pid,
							      msg->srv.cid)) {
				rc = rpcrouter_create_remote_endpoint(
					msg->srv.pid, msg->srv.cid);
				if (rc < 0)
					printk(KERN_ERR
						"rpcrouter:Client create"
						"error (%d)\n", rc);
			}
			rpcrouter_register_board_dev(server);
			schedule_work(&work_create_pdevs);
			wake_up(&newserver_wait);
		} else {
			if ((server->pid == msg->srv.pid) &&
			    (server->cid == msg->srv.cid)) {
				handle_server_restart(server,
						      msg->srv.pid,
						      msg->srv.cid,
						      msg->srv.prog,
						      msg->srv.vers);
			} else {
				server->pid = msg->srv.pid;
				server->cid = msg->srv.cid;
			}
		}
		break;

	case RPCROUTER_CTRL_CMD_REMOVE_SERVER:
		RR("o REMOVE_SERVER prog=%08x:%d\n",
		   msg->srv.prog, msg->srv.vers);
		server = rpcrouter_lookup_server(msg->srv.prog, msg->srv.vers);
		if (server)
			rpcrouter_destroy_server(server);
		break;

	case RPCROUTER_CTRL_CMD_REMOVE_CLIENT:
		RR("o REMOVE_CLIENT id=%d:%08x\n", msg->cli.pid, msg->cli.cid);
		if (msg->cli.pid == RPCROUTER_PID_LOCAL) {
			printk(KERN_ERR
			       "rpcrouter: Denying remote removal of "
			       "local client\n");
			break;
		}
		r_ept = rpcrouter_lookup_remote_endpoint(msg->cli.pid,
							 msg->cli.cid);
		if (r_ept) {
			spin_lock_irqsave(&remote_endpoints_lock, flags);
			list_del(&r_ept->list);
			spin_unlock_irqrestore(&remote_endpoints_lock, flags);
			kfree(r_ept);
		}

		/* Notify local clients of this event */
		printk(KERN_ERR "rpcrouter: LOCAL NOTIFICATION NOT IMP\n");
		rc = -ENOSYS;

		break;
	case RPCROUTER_CTRL_CMD_PING:
		/* No action needed for ping messages received */
		RR("o PING\n");
		break;
	default:
		RR("o UNKNOWN(%08x)\n", msg->cmd);
		rc = -ENOSYS;
	}

	return rc;
}

static void do_create_rpcrouter_pdev(struct work_struct *work)
{
	D("%s: modem rpc router up\n", __func__);
	platform_device_register(&rpcrouter_pdev);
	complete_all(&rpc_remote_router_up);
}

static void do_create_pdevs(struct work_struct *work)
{
	unsigned long flags;
	struct rr_server *server;

	/* TODO: race if destroyed while being registered */
	spin_lock_irqsave(&server_list_lock, flags);
	list_for_each_entry(server, &server_list, list) {
		if (server->pid != RPCROUTER_PID_LOCAL) {
			if (server->pdev_name[0] == 0) {
				sprintf(server->pdev_name, "rs%.8x",
					server->prog);
				spin_unlock_irqrestore(&server_list_lock,
						       flags);
				msm_rpcrouter_create_server_pdev(server);
				schedule_work(&work_create_pdevs);
				return;
			}
		}
	}
	spin_unlock_irqrestore(&server_list_lock, flags);
}

static void *rr_malloc(unsigned sz)
{
	void *ptr = kmalloc(sz, GFP_KERNEL);
	if (ptr)
		return ptr;

	printk(KERN_ERR "rpcrouter: kmalloc of %d failed, retrying...\n", sz);
	do {
		ptr = kmalloc(sz, GFP_KERNEL);
	} while (!ptr);

	return ptr;
}

static int rr_read(struct rpcrouter_xprt_info *xprt_info,
		   void *data, uint32_t len)
{
	int rc;
	unsigned long flags;

	while (!xprt_info->abort_data_read) {
		spin_lock_irqsave(&xprt_info->lock, flags);
		if (xprt_info->xprt->read_avail() >= len) {
			rc = xprt_info->xprt->read(data, len);
			spin_unlock_irqrestore(&xprt_info->lock, flags);
			if (rc == len && !xprt_info->abort_data_read)
				return 0;
			else
				return -EIO;
		}
		xprt_info->need_len = len;
		wake_unlock(&xprt_info->wakelock);
		spin_unlock_irqrestore(&xprt_info->lock, flags);

		wait_event(xprt_info->read_wait,
			xprt_info->xprt->read_avail() >= len
			|| xprt_info->abort_data_read);
	}
	return -EIO;
}

#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
static char *type_to_str(int i)
{
	switch (i) {
	case RPCROUTER_CTRL_CMD_DATA:
		return "data    ";
	case RPCROUTER_CTRL_CMD_HELLO:
		return "hello   ";
	case RPCROUTER_CTRL_CMD_BYE:
		return "bye     ";
	case RPCROUTER_CTRL_CMD_NEW_SERVER:
		return "new_srvr";
	case RPCROUTER_CTRL_CMD_REMOVE_SERVER:
		return "rmv_srvr";
	case RPCROUTER_CTRL_CMD_REMOVE_CLIENT:
		return "rmv_clnt";
	case RPCROUTER_CTRL_CMD_RESUME_TX:
		return "resum_tx";
	case RPCROUTER_CTRL_CMD_EXIT:
		return "cmd_exit";
	default:
		return "invalid";
	}
}
#endif

static void do_read_data(struct work_struct *work)
{
	struct rr_header hdr;
	struct rr_packet *pkt;
	struct rr_fragment *frag;
	struct msm_rpc_endpoint *ept;
#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
	struct rpc_request_hdr *rq;
#endif
	uint32_t pm, mid;
	unsigned long flags;

	struct rpcrouter_xprt_info *xprt_info =
		container_of(work,
			     struct rpcrouter_xprt_info,
			     read_data);

	if (rr_read(xprt_info, &hdr, sizeof(hdr)))
		goto fail_io;

	RR("- ver=%d type=%d src=%d:%08x crx=%d siz=%d dst=%d:%08x\n",
	   hdr.version, hdr.type, hdr.src_pid, hdr.src_cid,
	   hdr.confirm_rx, hdr.size, hdr.dst_pid, hdr.dst_cid);
	RAW_HDR("[r rr_h] "
	    "ver=%i,type=%s,src_pid=%08x,src_cid=%08x,"
	    "confirm_rx=%i,size=%3i,dst_pid=%08x,dst_cid=%08x\n",
	    hdr.version, type_to_str(hdr.type), hdr.src_pid, hdr.src_cid,
	    hdr.confirm_rx, hdr.size, hdr.dst_pid, hdr.dst_cid);

	if (hdr.version != RPCROUTER_VERSION) {
		DIAG("version %d != %d\n", hdr.version, RPCROUTER_VERSION);
		goto fail_data;
	}
	if (hdr.size > RPCROUTER_MSGSIZE_MAX) {
		DIAG("msg size %d > max %d\n", hdr.size, RPCROUTER_MSGSIZE_MAX);
		goto fail_data;
	}

	if (hdr.dst_cid == RPCROUTER_ROUTER_ADDRESS) {
		if (xprt_info->remote_pid == -1) {
			xprt_info->remote_pid = hdr.src_pid;

			/* do restart notification */
			modem_reset_startup(xprt_info);
		}

		if (rr_read(xprt_info, xprt_info->r2r_buf, hdr.size))
			goto fail_io;
		process_control_msg(xprt_info,
				    (void *) xprt_info->r2r_buf, hdr.size);
		goto done;
	}

	if (hdr.size < sizeof(pm)) {
		DIAG("runt packet (no pacmark)\n");
		goto fail_data;
	}
	if (rr_read(xprt_info, &pm, sizeof(pm)))
		goto fail_io;

	hdr.size -= sizeof(pm);

	frag = rr_malloc(sizeof(*frag));
	frag->next = NULL;
	frag->length = hdr.size;
	if (rr_read(xprt_info, frag->data, hdr.size)) {
		kfree(frag);
		goto fail_io;
	}

#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
	if ((smd_rpcrouter_debug_mask & RAW_PMR) &&
	    ((pm >> 30 & 0x1) || (pm >> 31 & 0x1))) {
		uint32_t xid = 0;
		if (pm >> 30 & 0x1) {
			rq = (struct rpc_request_hdr *) frag->data;
			xid = ntohl(rq->xid);
		}
		if ((pm >> 31 & 0x1) || (pm >> 30 & 0x1))
			RAW_PMR_NOMASK("xid:0x%03x first=%i,last=%i,mid=%3i,"
				       "len=%3i,dst_cid=%08x\n",
				       xid,
				       pm >> 30 & 0x1,
				       pm >> 31 & 0x1,
				       pm >> 16 & 0xFF,
				       pm & 0xFFFF, hdr.dst_cid);
	}

	if (smd_rpcrouter_debug_mask & SMEM_LOG) {
		rq = (struct rpc_request_hdr *) frag->data;
		if (rq->xid == 0)
			smem_log_event(SMEM_LOG_PROC_ID_APPS |
				       RPC_ROUTER_LOG_EVENT_MID_READ,
				       PACMARK_MID(pm),
				       hdr.dst_cid,
				       hdr.src_cid);
		else
			smem_log_event(SMEM_LOG_PROC_ID_APPS |
				       RPC_ROUTER_LOG_EVENT_MSG_READ,
				       ntohl(rq->xid),
				       hdr.dst_cid,
				       hdr.src_cid);
	}
#endif

	spin_lock_irqsave(&local_endpoints_lock, flags);
	ept = rpcrouter_lookup_local_endpoint(hdr.dst_cid);
	if (!ept) {
		spin_unlock_irqrestore(&local_endpoints_lock, flags);
		DIAG("no local ept for cid %08x\n", hdr.dst_cid);
		kfree(frag);
		goto done;
	}

	/* See if there is already a partial packet that matches our mid
	 * and if so, append this fragment to that packet.
	 */
	mid = PACMARK_MID(pm);
	spin_lock(&ept->incomplete_lock);
	list_for_each_entry(pkt, &ept->incomplete, list) {
		if (pkt->mid == mid) {
			pkt->last->next = frag;
			pkt->last = frag;
			pkt->length += frag->length;
			if (PACMARK_LAST(pm)) {
				list_del(&pkt->list);
				spin_unlock(&ept->incomplete_lock);
				goto packet_complete;
			}
			spin_unlock(&ept->incomplete_lock);
			spin_unlock_irqrestore(&local_endpoints_lock, flags);
			goto done;
		}
	}
	spin_unlock(&ept->incomplete_lock);
	spin_unlock_irqrestore(&local_endpoints_lock, flags);
	/* This mid is new -- create a packet for it, and put it on
	 * the incomplete list if this fragment is not a last fragment,
	 * otherwise put it on the read queue.
	 */
	pkt = rr_malloc(sizeof(struct rr_packet));
	pkt->first = frag;
	pkt->last = frag;
	memcpy(&pkt->hdr, &hdr, sizeof(hdr));
	pkt->mid = mid;
	pkt->length = frag->length;

	spin_lock_irqsave(&local_endpoints_lock, flags);
	ept = rpcrouter_lookup_local_endpoint(hdr.dst_cid);
	if (!ept) {
		spin_unlock_irqrestore(&local_endpoints_lock, flags);
		DIAG("no local ept for cid %08x\n", hdr.dst_cid);
		kfree(frag);
		kfree(pkt);
		goto done;
	}
	if (!PACMARK_LAST(pm)) {
		spin_lock(&ept->incomplete_lock);
		list_add_tail(&pkt->list, &ept->incomplete);
		spin_unlock(&ept->incomplete_lock);
		spin_unlock_irqrestore(&local_endpoints_lock, flags);
		goto done;
	}

packet_complete:
	spin_lock(&ept->read_q_lock);
	D("%s: take read lock on ept %p\n", __func__, ept);
	wake_lock(&ept->read_q_wake_lock);
	list_add_tail(&pkt->list, &ept->read_q);
	wake_up(&ept->wait_q);
	spin_unlock(&ept->read_q_lock);
	spin_unlock_irqrestore(&local_endpoints_lock, flags);
done:

	if (hdr.confirm_rx) {
		union rr_control_msg msg = { 0 };

		msg.cmd = RPCROUTER_CTRL_CMD_RESUME_TX;
		msg.cli.pid = hdr.dst_pid;
		msg.cli.cid = hdr.dst_cid;

		RR("x RESUME_TX id=%d:%08x\n", msg.cli.pid, msg.cli.cid);
		rpcrouter_send_control_msg(xprt_info, &msg);

#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
		if (smd_rpcrouter_debug_mask & SMEM_LOG)
			smem_log_event(SMEM_LOG_PROC_ID_APPS |
				       RPC_ROUTER_LOG_EVENT_MSG_CFM_SNT,
				       RPCROUTER_PID_LOCAL,
				       hdr.dst_cid,
				       hdr.src_cid);
#endif

	}

	/* don't requeue if we should be shutting down */
	if (!xprt_info->abort_data_read) {
		queue_work(xprt_info->workqueue, &xprt_info->read_data);
		return;
	}

	D("rpc_router terminating for '%s'\n",
		xprt_info->xprt->name);

fail_io:
fail_data:
	D(KERN_ERR "rpc_router has died for '%s'\n",
			xprt_info->xprt->name);
}

void msm_rpc_setup_req(struct rpc_request_hdr *hdr, uint32_t prog,
		       uint32_t vers, uint32_t proc)
{
	memset(hdr, 0, sizeof(struct rpc_request_hdr));
	hdr->xid = cpu_to_be32(atomic_add_return(1, &next_xid));
	hdr->rpc_vers = cpu_to_be32(2);
	hdr->prog = cpu_to_be32(prog);
	hdr->vers = cpu_to_be32(vers);
	hdr->procedure = cpu_to_be32(proc);
}
EXPORT_SYMBOL(msm_rpc_setup_req);

struct msm_rpc_endpoint *msm_rpc_open(void)
{
	struct msm_rpc_endpoint *ept;

	ept = msm_rpcrouter_create_local_endpoint(MKDEV(0, 0));
	if (ept == NULL)
		return ERR_PTR(-ENOMEM);

	return ept;
}

void msm_rpc_read_wakeup(struct msm_rpc_endpoint *ept)
{
	ept->forced_wakeup = 1;
	wake_up(&ept->wait_q);
}

int msm_rpc_close(struct msm_rpc_endpoint *ept)
{
	if (!ept)
		return -EINVAL;
	return msm_rpcrouter_destroy_local_endpoint(ept);
}
EXPORT_SYMBOL(msm_rpc_close);

static int msm_rpc_write_pkt(
	struct rr_header *hdr,
	struct msm_rpc_endpoint *ept,
	struct rr_remote_endpoint *r_ept,
	void *buffer,
	int count,
	int first,
	int last,
	uint32_t mid
	)
{
#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
	struct rpc_request_hdr *rq = buffer;
	uint32_t event_id;
#endif
	uint32_t pacmark;
	unsigned long flags = 0;
	int rc;
	struct rpcrouter_xprt_info *xprt_info;
	int needed;

	DEFINE_WAIT(__wait);

	/* Create routing header */
	hdr->type = RPCROUTER_CTRL_CMD_DATA;
	hdr->version = RPCROUTER_VERSION;
	hdr->src_pid = ept->pid;
	hdr->src_cid = ept->cid;
	hdr->confirm_rx = 0;
	hdr->size = count + sizeof(uint32_t);

	rc = wait_for_restart_and_notify(ept);
	if (rc)
		return rc;

	if (r_ept) {
		for (;;) {
			prepare_to_wait(&r_ept->quota_wait, &__wait,
					TASK_INTERRUPTIBLE);
			spin_lock_irqsave(&r_ept->quota_lock, flags);
			if ((r_ept->tx_quota_cntr <
			     RPCROUTER_DEFAULT_RX_QUOTA) ||
			    (r_ept->quota_restart_state != RESTART_NORMAL))
				break;
			if (signal_pending(current) &&
			    (!(ept->flags & MSM_RPC_UNINTERRUPTIBLE)))
				break;
			spin_unlock_irqrestore(&r_ept->quota_lock, flags);
			schedule();
		}
		finish_wait(&r_ept->quota_wait, &__wait);

		if (r_ept->quota_restart_state != RESTART_NORMAL) {
			spin_lock(&ept->restart_lock);
			ept->restart_state &= ~RESTART_PEND_NTFY;
			spin_unlock(&ept->restart_lock);
			spin_unlock_irqrestore(&r_ept->quota_lock, flags);
			return -ENETRESET;
		}

		if (signal_pending(current) &&
		    (!(ept->flags & MSM_RPC_UNINTERRUPTIBLE))) {
			spin_unlock_irqrestore(&r_ept->quota_lock, flags);
			return -ERESTARTSYS;
		}
		r_ept->tx_quota_cntr++;
		if (r_ept->tx_quota_cntr == RPCROUTER_DEFAULT_RX_QUOTA) {
			hdr->confirm_rx = 1;

#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
			if (smd_rpcrouter_debug_mask & SMEM_LOG) {
				event_id = (rq->xid == 0) ?
					RPC_ROUTER_LOG_EVENT_MID_CFM_REQ :
					RPC_ROUTER_LOG_EVENT_MSG_CFM_REQ;

				smem_log_event(SMEM_LOG_PROC_ID_APPS | event_id,
					       hdr->dst_pid,
					       hdr->dst_cid,
					       hdr->src_cid);
			}
#endif

		}
	}
	pacmark = PACMARK(count, mid, first, last);

	if (r_ept)
		spin_unlock_irqrestore(&r_ept->quota_lock, flags);

	mutex_lock(&xprt_info_list_lock);
	xprt_info = rpcrouter_get_xprt_info(hdr->dst_pid);
	if (!xprt_info) {
		mutex_unlock(&xprt_info_list_lock);
		return -ENETRESET;
	}
	spin_lock_irqsave(&xprt_info->lock, flags);
	mutex_unlock(&xprt_info_list_lock);
	spin_lock(&ept->restart_lock);
	if (ept->restart_state != RESTART_NORMAL) {
		ept->restart_state &= ~RESTART_PEND_NTFY;
		spin_unlock(&ept->restart_lock);
		spin_unlock_irqrestore(&xprt_info->lock, flags);
		return -ENETRESET;
	}

	needed = sizeof(*hdr) + hdr->size;
	while ((ept->restart_state == RESTART_NORMAL) &&
	       (xprt_info->xprt->write_avail() < needed)) {
		spin_unlock(&ept->restart_lock);
		spin_unlock_irqrestore(&xprt_info->lock, flags);
		msleep(250);

		/* refresh xprt pointer to ensure that it hasn't
		 * been deleted since our last retrieval */
		mutex_lock(&xprt_info_list_lock);
		xprt_info = rpcrouter_get_xprt_info(hdr->dst_pid);
		if (!xprt_info) {
			mutex_unlock(&xprt_info_list_lock);
			return -ENETRESET;
		}
		spin_lock_irqsave(&xprt_info->lock, flags);
		mutex_unlock(&xprt_info_list_lock);
		spin_lock(&ept->restart_lock);
	}
	if (ept->restart_state != RESTART_NORMAL) {
		ept->restart_state &= ~RESTART_PEND_NTFY;
		spin_unlock(&ept->restart_lock);
		spin_unlock_irqrestore(&xprt_info->lock, flags);
		return -ENETRESET;
	}

	/* TODO: deal with full fifo */
	xprt_info->xprt->write(hdr, sizeof(*hdr), HEADER);
	RAW_HDR("[w rr_h] "
		    "ver=%i,type=%s,src_pid=%08x,src_cid=%08x,"
		"confirm_rx=%i,size=%3i,dst_pid=%08x,dst_cid=%08x\n",
		hdr->version, type_to_str(hdr->type),
		hdr->src_pid, hdr->src_cid,
		hdr->confirm_rx, hdr->size, hdr->dst_pid, hdr->dst_cid);
	xprt_info->xprt->write(&pacmark, sizeof(pacmark), PACKMARK);

#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
	if ((smd_rpcrouter_debug_mask & RAW_PMW) &&
	    ((pacmark >> 30 & 0x1) || (pacmark >> 31 & 0x1))) {
		uint32_t xid = 0;
		if (pacmark >> 30 & 0x1)
			xid = ntohl(rq->xid);
		if ((pacmark >> 31 & 0x1) || (pacmark >> 30 & 0x1))
			RAW_PMW_NOMASK("xid:0x%03x first=%i,last=%i,mid=%3i,"
				       "len=%3i,src_cid=%x\n",
				       xid,
				       pacmark >> 30 & 0x1,
				       pacmark >> 31 & 0x1,
				       pacmark >> 16 & 0xFF,
				       pacmark & 0xFFFF, hdr->src_cid);
	}
#endif

	xprt_info->xprt->write(buffer, count, PAYLOAD);
	spin_unlock(&ept->restart_lock);
	spin_unlock_irqrestore(&xprt_info->lock, flags);

#if defined(CONFIG_MSM_ONCRPCROUTER_DEBUG)
	if (smd_rpcrouter_debug_mask & SMEM_LOG) {
		if (rq->xid == 0)
			smem_log_event(SMEM_LOG_PROC_ID_APPS |
				       RPC_ROUTER_LOG_EVENT_MID_WRITTEN,
				       PACMARK_MID(pacmark),
				       hdr->dst_cid,
				       hdr->src_cid);
		else
			smem_log_event(SMEM_LOG_PROC_ID_APPS |
				       RPC_ROUTER_LOG_EVENT_MSG_WRITTEN,
				       ntohl(rq->xid),
				       hdr->dst_cid,
				       hdr->src_cid);
	}
#endif

	return needed;
}

static struct msm_rpc_reply *get_pend_reply(struct msm_rpc_endpoint *ept,
					    uint32_t xid)
{
	unsigned long flags;
	struct msm_rpc_reply *reply;
	spin_lock_irqsave(&ept->reply_q_lock, flags);
	list_for_each_entry(reply, &ept->reply_pend_q, list) {
		if (reply->xid == xid) {
			list_del(&reply->list);
			spin_unlock_irqrestore(&ept->reply_q_lock, flags);
			return reply;
		}
	}
	spin_unlock_irqrestore(&ept->reply_q_lock, flags);
	return NULL;
}

void get_requesting_client(struct msm_rpc_endpoint *ept, uint32_t xid,
			   struct msm_rpc_client_info *clnt_info)
{
	unsigned long flags;
	struct msm_rpc_reply *reply;

	if (!clnt_info)
		return;

	spin_lock_irqsave(&ept->reply_q_lock, flags);
	list_for_each_entry(reply, &ept->reply_pend_q, list) {
		if (reply->xid == xid) {
			clnt_info->pid = reply->pid;
			clnt_info->cid = reply->cid;
			clnt_info->prog = reply->prog;
			clnt_info->vers = reply->vers;
			spin_unlock_irqrestore(&ept->reply_q_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&ept->reply_q_lock, flags);
	return;
}

static void set_avail_reply(struct msm_rpc_endpoint *ept,
			    struct msm_rpc_reply *reply)
{
	unsigned long flags;
	spin_lock_irqsave(&ept->reply_q_lock, flags);
	list_add_tail(&reply->list, &ept->reply_avail_q);
	spin_unlock_irqrestore(&ept->reply_q_lock, flags);
}

static struct msm_rpc_reply *get_avail_reply(struct msm_rpc_endpoint *ept)
{
	struct msm_rpc_reply *reply;
	unsigned long flags;
	if (list_empty(&ept->reply_avail_q)) {
		if (ept->reply_cnt >= RPCROUTER_PEND_REPLIES_MAX) {
			printk(KERN_ERR
			       "exceeding max replies of %d \n",
			       RPCROUTER_PEND_REPLIES_MAX);
			return 0;
		}
		reply = kmalloc(sizeof(struct msm_rpc_reply), GFP_KERNEL);
		if (!reply)
			return 0;
		D("Adding reply 0x%08x \n", (unsigned int)reply);
		memset(reply, 0, sizeof(struct msm_rpc_reply));
		spin_lock_irqsave(&ept->reply_q_lock, flags);
		ept->reply_cnt++;
		spin_unlock_irqrestore(&ept->reply_q_lock, flags);
	} else {
		spin_lock_irqsave(&ept->reply_q_lock, flags);
		reply = list_first_entry(&ept->reply_avail_q,
					 struct msm_rpc_reply,
					 list);
		list_del(&reply->list);
		spin_unlock_irqrestore(&ept->reply_q_lock, flags);
	}
	return reply;
}

static void set_pend_reply(struct msm_rpc_endpoint *ept,
			   struct msm_rpc_reply *reply)
{
		unsigned long flags;
		spin_lock_irqsave(&ept->reply_q_lock, flags);
		D("%s: take reply lock on ept %p\n", __func__, ept);
		wake_lock(&ept->reply_q_wake_lock);
		list_add_tail(&reply->list, &ept->reply_pend_q);
		spin_unlock_irqrestore(&ept->reply_q_lock, flags);
}

int msm_rpc_write(struct msm_rpc_endpoint *ept, void *buffer, int count)
{
	struct rr_header hdr;
	struct rpc_request_hdr *rq = buffer;
	struct rr_remote_endpoint *r_ept;
	struct msm_rpc_reply *reply = NULL;
	int max_tx;
	int tx_cnt;
	char *tx_buf;
	int rc;
	int first_pkt = 1;
	uint32_t mid;
	unsigned long flags;

	/* snoop the RPC packet and enforce permissions */

	/* has to have at least the xid and type fields */
	if (count < (sizeof(uint32_t) * 2)) {
		printk(KERN_ERR "rr_write: rejecting runt packet\n");
		return -EINVAL;
	}

	if (rq->type == 0) {
		/* RPC CALL */
		if (count < (sizeof(uint32_t) * 6)) {
			printk(KERN_ERR
			       "rr_write: rejecting runt call packet\n");
			return -EINVAL;
		}
		if (ept->dst_pid == 0xffffffff) {
			printk(KERN_ERR "rr_write: not connected\n");
			return -ENOTCONN;
		}
		if ((ept->dst_prog != rq->prog) ||
		    ((be32_to_cpu(ept->dst_vers) & 0x0fff0000) !=
		     (be32_to_cpu(rq->vers) & 0x0fff0000))) {
			printk(KERN_ERR
			       "rr_write: cannot write to %08x:%08x "
			       "(bound to %08x:%08x)\n",
			       be32_to_cpu(rq->prog), be32_to_cpu(rq->vers),
			       be32_to_cpu(ept->dst_prog),
			       be32_to_cpu(ept->dst_vers));
			return -EINVAL;
		}
		hdr.dst_pid = ept->dst_pid;
		hdr.dst_cid = ept->dst_cid;
		IO("CALL to %08x:%d @ %d:%08x (%d bytes)\n",
		   be32_to_cpu(rq->prog), be32_to_cpu(rq->vers),
		   ept->dst_pid, ept->dst_cid, count);
	} else {
		/* RPC REPLY */
		reply = get_pend_reply(ept, rq->xid);
		if (!reply) {
			printk(KERN_ERR
			       "rr_write: rejecting, reply not found \n");
			return -EINVAL;
		}
		hdr.dst_pid = reply->pid;
		hdr.dst_cid = reply->cid;
		IO("REPLY to xid=%d @ %d:%08x (%d bytes)\n",
		   be32_to_cpu(rq->xid), hdr.dst_pid, hdr.dst_cid, count);
	}

	r_ept = rpcrouter_lookup_remote_endpoint(hdr.dst_pid, hdr.dst_cid);

	if ((!r_ept) && (hdr.dst_pid != RPCROUTER_PID_LOCAL)) {
		printk(KERN_ERR
			"msm_rpc_write(): No route to ept "
			"[PID %x CID %x]\n", hdr.dst_pid, hdr.dst_cid);
		count = -EHOSTUNREACH;
		goto write_release_lock;
	}

	tx_cnt = count;
	tx_buf = buffer;
	mid = atomic_add_return(1, &pm_mid) & 0xFF;
	/* The modem's router can only take 500 bytes of data. The
	   first 8 bytes it uses on the modem side for addressing,
	   the next 4 bytes are for the pacmark header. */
	max_tx = RPCROUTER_MSGSIZE_MAX - 8 - sizeof(uint32_t);
	IO("Writing %d bytes, max pkt size is %d\n",
	   tx_cnt, max_tx);
	while (tx_cnt > 0) {
		if (tx_cnt > max_tx) {
			rc = msm_rpc_write_pkt(&hdr, ept, r_ept,
					       tx_buf, max_tx,
					       first_pkt, 0, mid);
			if (rc < 0) {
				count = rc;
				goto write_release_lock;
			}
			IO("Wrote %d bytes First %d, Last 0 mid %d\n",
			   rc, first_pkt, mid);
			tx_cnt -= max_tx;
			tx_buf += max_tx;
		} else {
			rc = msm_rpc_write_pkt(&hdr, ept, r_ept,
					       tx_buf, tx_cnt,
					       first_pkt, 1, mid);
			if (rc < 0) {
				count = rc;
				goto write_release_lock;
			}
			IO("Wrote %d bytes First %d Last 1 mid %d\n",
			   rc, first_pkt, mid);
			break;
		}
		first_pkt = 0;
	}

 write_release_lock:
	/* if reply, release wakelock after writing to the transport */
	if (rq->type != 0) {
		/* Upon failure, add reply tag to the pending list.
		** Else add reply tag to the avail/free list. */
		if (count < 0)
			set_pend_reply(ept, reply);
		else
			set_avail_reply(ept, reply);

		spin_lock_irqsave(&ept->reply_q_lock, flags);
		if (list_empty(&ept->reply_pend_q)) {
			D("%s: release reply lock on ept %p\n", __func__, ept);
			wake_unlock(&ept->reply_q_wake_lock);
		}
		spin_unlock_irqrestore(&ept->reply_q_lock, flags);
	}

	return count;
}
EXPORT_SYMBOL(msm_rpc_write);

/*
 * NOTE: It is the responsibility of the caller to kfree buffer
 */
int msm_rpc_read(struct msm_rpc_endpoint *ept, void **buffer,
		 unsigned user_len, long timeout)
{
	struct rr_fragment *frag, *next;
	char *buf;
	int rc;

	rc = __msm_rpc_read(ept, &frag, user_len, timeout);
	if (rc <= 0)
		return rc;

	/* single-fragment messages conveniently can be
	 * returned as-is (the buffer is at the front)
	 */
	if (frag->next == 0) {
		*buffer = (void*) frag;
		return rc;
	}

	/* multi-fragment messages, we have to do it the
	 * hard way, which is rather disgusting right now
	 */
	buf = rr_malloc(rc);
	*buffer = buf;

	while (frag != NULL) {
		memcpy(buf, frag->data, frag->length);
		next = frag->next;
		buf += frag->length;
		kfree(frag);
		frag = next;
	}

	return rc;
}
EXPORT_SYMBOL(msm_rpc_read);

int msm_rpc_call(struct msm_rpc_endpoint *ept, uint32_t proc,
		 void *_request, int request_size,
		 long timeout)
{
	return msm_rpc_call_reply(ept, proc,
				  _request, request_size,
				  NULL, 0, timeout);
}
EXPORT_SYMBOL(msm_rpc_call);

int msm_rpc_call_reply(struct msm_rpc_endpoint *ept, uint32_t proc,
		       void *_request, int request_size,
		       void *_reply, int reply_size,
		       long timeout)
{
	struct rpc_request_hdr *req = _request;
	struct rpc_reply_hdr *reply;
	int rc;

	if (request_size < sizeof(*req))
		return -ETOOSMALL;

	if (ept->dst_pid == 0xffffffff)
		return -ENOTCONN;

	memset(req, 0, sizeof(*req));
	req->xid = cpu_to_be32(atomic_add_return(1, &next_xid));
	req->rpc_vers = cpu_to_be32(2);
	req->prog = ept->dst_prog;
	req->vers = ept->dst_vers;
	req->procedure = cpu_to_be32(proc);

	rc = msm_rpc_write(ept, req, request_size);
	if (rc < 0)
		return rc;

	for (;;) {
		rc = msm_rpc_read(ept, (void*) &reply, -1, timeout);
		if (rc < 0)
			return rc;
		if (rc < (3 * sizeof(uint32_t))) {
			rc = -EIO;
			break;
		}
		/* we should not get CALL packets -- ignore them */
		if (reply->type == 0) {
			kfree(reply);
			continue;
		}
		/* If an earlier call timed out, we could get the (no
		 * longer wanted) reply for it.	 Ignore replies that
		 * we don't expect
		 */
		if (reply->xid != req->xid) {
			kfree(reply);
			continue;
		}
		if (reply->reply_stat != 0) {
			rc = -EPERM;
			break;
		}
		if (reply->data.acc_hdr.accept_stat != 0) {
			rc = -EINVAL;
			break;
		}
		if (_reply == NULL) {
			rc = 0;
			break;
		}
		if (rc > reply_size) {
			rc = -ENOMEM;
		} else {
			memcpy(_reply, reply, rc);
		}
		break;
	}
	kfree(reply);
	return rc;
}
EXPORT_SYMBOL(msm_rpc_call_reply);


static inline int ept_packet_available(struct msm_rpc_endpoint *ept)
{
	unsigned long flags;
	int ret;
	spin_lock_irqsave(&ept->read_q_lock, flags);
	ret = !list_empty(&ept->read_q);
	spin_unlock_irqrestore(&ept->read_q_lock, flags);
	return ret;
}

int __msm_rpc_read(struct msm_rpc_endpoint *ept,
		   struct rr_fragment **frag_ret,
		   unsigned len, long timeout)
{
	struct rr_packet *pkt;
	struct rpc_request_hdr *rq;
	struct msm_rpc_reply *reply;
	unsigned long flags;
	int rc;

	rc = wait_for_restart_and_notify(ept);
	if (rc)
		return rc;

	IO("READ on ept %p\n", ept);
	if (ept->flags & MSM_RPC_UNINTERRUPTIBLE) {
		if (timeout < 0) {
			wait_event(ept->wait_q, (ept_packet_available(ept) ||
						   ept->forced_wakeup ||
						   ept->restart_state));
			if (!msm_rpc_clear_netreset(ept))
				return -ENETRESET;
		} else {
			rc = wait_event_timeout(
				ept->wait_q,
				(ept_packet_available(ept) ||
				 ept->forced_wakeup ||
				 ept->restart_state),
				timeout);
			if (!msm_rpc_clear_netreset(ept))
				return -ENETRESET;
			if (rc == 0)
				return -ETIMEDOUT;
		}
	} else {
		if (timeout < 0) {
			rc = wait_event_interruptible(
				ept->wait_q, (ept_packet_available(ept) ||
					ept->forced_wakeup ||
					ept->restart_state));
			if (!msm_rpc_clear_netreset(ept))
				return -ENETRESET;
			if (rc < 0)
				return rc;
		} else {
			rc = wait_event_interruptible_timeout(
				ept->wait_q,
				(ept_packet_available(ept) ||
				 ept->forced_wakeup ||
				 ept->restart_state),
				timeout);
			if (!msm_rpc_clear_netreset(ept))
				return -ENETRESET;
			if (rc == 0)
				return -ETIMEDOUT;
		}
	}

	if (ept->forced_wakeup) {
		ept->forced_wakeup = 0;
		return 0;
	}

	spin_lock_irqsave(&ept->read_q_lock, flags);
	if (list_empty(&ept->read_q)) {
		spin_unlock_irqrestore(&ept->read_q_lock, flags);
		return -EAGAIN;
	}
	pkt = list_first_entry(&ept->read_q, struct rr_packet, list);
	if (pkt->length > len) {
		spin_unlock_irqrestore(&ept->read_q_lock, flags);
		return -ETOOSMALL;
	}
	list_del(&pkt->list);
	spin_unlock_irqrestore(&ept->read_q_lock, flags);

	rc = pkt->length;

	*frag_ret = pkt->first;
	rq = (void*) pkt->first->data;
	if ((rc >= (sizeof(uint32_t) * 3)) && (rq->type == 0)) {
		/* RPC CALL */
		reply = get_avail_reply(ept);
		if (!reply) {
			rc = -ENOMEM;
			goto read_release_lock;
		}
		reply->cid = pkt->hdr.src_cid;
		reply->pid = pkt->hdr.src_pid;
		reply->xid = rq->xid;
		reply->prog = rq->prog;
		reply->vers = rq->vers;
		set_pend_reply(ept, reply);
	}

	kfree(pkt);

	IO("READ on ept %p (%d bytes)\n", ept, rc);

 read_release_lock:

	/* release read wakelock after taking reply wakelock */
	spin_lock_irqsave(&ept->read_q_lock, flags);
	if (list_empty(&ept->read_q)) {
		D("%s: release read lock on ept %p\n", __func__, ept);
		wake_unlock(&ept->read_q_wake_lock);
	}
	spin_unlock_irqrestore(&ept->read_q_lock, flags);

	return rc;
}

int msm_rpc_is_compatible_version(uint32_t server_version,
				  uint32_t client_version)
{

	if ((server_version & RPC_VERSION_MODE_MASK) !=
	    (client_version & RPC_VERSION_MODE_MASK))
		return 0;

	if (server_version & RPC_VERSION_MODE_MASK)
		return server_version == client_version;

	return ((server_version & RPC_VERSION_MAJOR_MASK) ==
		 (client_version & RPC_VERSION_MAJOR_MASK)) &&
		((server_version & RPC_VERSION_MINOR_MASK) >=
		 (client_version & RPC_VERSION_MINOR_MASK));
}
EXPORT_SYMBOL(msm_rpc_is_compatible_version);

static struct rr_server *msm_rpc_get_server(uint32_t prog, uint32_t vers,
					    uint32_t accept_compatible,
					    uint32_t *found_prog)
{
	struct rr_server *server;
	unsigned long     flags;

	if (found_prog == NULL)
		return NULL;

	*found_prog = 0;
	spin_lock_irqsave(&server_list_lock, flags);
	list_for_each_entry(server, &server_list, list) {
		if (server->prog == prog) {
			*found_prog = 1;
			spin_unlock_irqrestore(&server_list_lock, flags);
			if (accept_compatible) {
				if (msm_rpc_is_compatible_version(server->vers,
								  vers)) {
					return server;
				} else {
					return NULL;
				}
			} else if (server->vers == vers) {
				return server;
			} else
				return NULL;
		}
	}
	spin_unlock_irqrestore(&server_list_lock, flags);
	return NULL;
}

static struct msm_rpc_endpoint *__msm_rpc_connect(uint32_t prog, uint32_t vers,
						  uint32_t accept_compatible,
						  unsigned flags)
{
	struct msm_rpc_endpoint *ept;
	struct rr_server *server;
	uint32_t found_prog;
	int rc = 0;

	DEFINE_WAIT(__wait);

	for (;;) {
		prepare_to_wait(&newserver_wait, &__wait,
				TASK_INTERRUPTIBLE);

		server = msm_rpc_get_server(prog, vers, accept_compatible,
					    &found_prog);
		if (server)
			break;

		if (found_prog) {
			pr_info("%s: server not found %x:%x\n",
				__func__, prog, vers);
			rc = -EHOSTUNREACH;
			break;
		}

		if (msm_rpc_connect_timeout_ms == 0) {
			rc = -EHOSTUNREACH;
			break;
		}

		if (signal_pending(current)) {
			rc = -ERESTARTSYS;
			break;
		}

		rc = schedule_timeout(
			msecs_to_jiffies(msm_rpc_connect_timeout_ms));
		if (!rc) {
			rc = -ETIMEDOUT;
			break;
		}
	}
	finish_wait(&newserver_wait, &__wait);

	if (!server)
		return ERR_PTR(rc);

	if (accept_compatible && (server->vers != vers)) {
		D("RPC Using new version 0x%08x(0x%08x) prog 0x%08x",
			vers, server->vers, prog);
		D(" ... Continuing\n");
	}

	ept = msm_rpc_open();
	if (IS_ERR(ept))
		return ept;

	ept->flags = flags;
	ept->dst_pid = server->pid;
	ept->dst_cid = server->cid;
	ept->dst_prog = cpu_to_be32(prog);
	ept->dst_vers = cpu_to_be32(server->vers);

	return ept;
}

struct msm_rpc_endpoint *msm_rpc_connect_compatible(uint32_t prog,
			uint32_t vers, unsigned flags)
{
	return __msm_rpc_connect(prog, vers, 1, flags);
}
EXPORT_SYMBOL(msm_rpc_connect_compatible);

struct msm_rpc_endpoint *msm_rpc_connect(uint32_t prog,
			 uint32_t vers, unsigned flags)
{
	return __msm_rpc_connect(prog, vers, 0, flags);
}
EXPORT_SYMBOL(msm_rpc_connect);

/* TODO: permission check? */
int msm_rpc_register_server(struct msm_rpc_endpoint *ept,
			    uint32_t prog, uint32_t vers)
{
	int rc;
	union rr_control_msg msg = { 0 };
	struct rr_server *server;
	struct rpcrouter_xprt_info *xprt_info;

	server = rpcrouter_create_server(ept->pid, ept->cid,
					 prog, vers);
	if (!server)
		return -ENODEV;

	msg.srv.cmd = RPCROUTER_CTRL_CMD_NEW_SERVER;
	msg.srv.pid = ept->pid;
	msg.srv.cid = ept->cid;
	msg.srv.prog = prog;
	msg.srv.vers = vers;

	RR("x NEW_SERVER id=%d:%08x prog=%08x:%08x\n",
	   ept->pid, ept->cid, prog, vers);

	mutex_lock(&xprt_info_list_lock);
	list_for_each_entry(xprt_info, &xprt_info_list, list) {
		rc = rpcrouter_send_control_msg(xprt_info, &msg);
		if (rc < 0) {
			mutex_unlock(&xprt_info_list_lock);
			return rc;
		}
	}
	mutex_unlock(&xprt_info_list_lock);
	return 0;
}

int msm_rpc_clear_netreset(struct msm_rpc_endpoint *ept)
{
	unsigned long flags;
	int rc = 1;
	spin_lock_irqsave(&ept->restart_lock, flags);
	if (ept->restart_state !=  RESTART_NORMAL) {
		ept->restart_state &= ~RESTART_PEND_NTFY;
		rc = 0;
	}
	spin_unlock_irqrestore(&ept->restart_lock, flags);
	return rc;
}

/* TODO: permission check -- disallow unreg of somebody else's server */
int msm_rpc_unregister_server(struct msm_rpc_endpoint *ept,
			      uint32_t prog, uint32_t vers)
{
	struct rr_server *server;
	server = rpcrouter_lookup_server(prog, vers);

	if (!server)
		return -ENOENT;
	rpcrouter_destroy_server(server);
	return 0;
}

int msm_rpc_get_curr_pkt_size(struct msm_rpc_endpoint *ept)
{
	unsigned long flags;
	struct rr_packet *pkt;
	int rc = 0;

	if (!ept)
		return -EINVAL;

	if (!msm_rpc_clear_netreset(ept))
		return -ENETRESET;

	spin_lock_irqsave(&ept->read_q_lock, flags);
	if (!list_empty(&ept->read_q)) {
		pkt = list_first_entry(&ept->read_q, struct rr_packet, list);
		rc = pkt->length;
	}
	spin_unlock_irqrestore(&ept->read_q_lock, flags);

	return rc;
}

static int msm_rpcrouter_close(void)
{
	struct rpcrouter_xprt_info *xprt_info;
	union rr_control_msg ctl;

	ctl.cmd = RPCROUTER_CTRL_CMD_BYE;
	mutex_lock(&xprt_info_list_lock);
	while (!list_empty(&xprt_info_list)) {
		xprt_info = list_first_entry(&xprt_info_list,
					struct rpcrouter_xprt_info, list);
		modem_reset_cleanup(xprt_info);
		xprt_info->abort_data_read = 1;
		wake_up(&xprt_info->read_wait);
		rpcrouter_send_control_msg(xprt_info, &ctl);
		xprt_info->xprt->close();
		list_del(&xprt_info->list);
		mutex_unlock(&xprt_info_list_lock);

		flush_workqueue(xprt_info->workqueue);
		destroy_workqueue(xprt_info->workqueue);
		wake_lock_destroy(&xprt_info->wakelock);
		/*free memory*/
		xprt_info->xprt->priv = 0;
		kfree(xprt_info);

		mutex_lock(&xprt_info_list_lock);
	}
	mutex_unlock(&xprt_info_list_lock);
	return 0;
}

#if defined(CONFIG_DEBUG_FS)
static int dump_servers(char *buf, int max)
{
	int i = 0;
	unsigned long flags;
	struct rr_server *svr;
	const char *sym;

	spin_lock_irqsave(&server_list_lock, flags);
	list_for_each_entry(svr, &server_list, list) {
		i += scnprintf(buf + i, max - i, "pdev_name: %s\n",
			       svr->pdev_name);
		i += scnprintf(buf + i, max - i, "pid: 0x%08x\n", svr->pid);
		i += scnprintf(buf + i, max - i, "cid: 0x%08x\n", svr->cid);
		i += scnprintf(buf + i, max - i, "prog: 0x%08x", svr->prog);
		sym = smd_rpc_get_sym(svr->prog);
		if (sym)
			i += scnprintf(buf + i, max - i, " (%s)\n", sym);
		else
			i += scnprintf(buf + i, max - i, "\n");
		i += scnprintf(buf + i, max - i, "vers: 0x%08x\n", svr->vers);
		i += scnprintf(buf + i, max - i, "\n");
	}
	spin_unlock_irqrestore(&server_list_lock, flags);

	return i;
}

static int dump_remote_endpoints(char *buf, int max)
{
	int i = 0;
	unsigned long flags;
	struct rr_remote_endpoint *ept;

	spin_lock_irqsave(&remote_endpoints_lock, flags);
	list_for_each_entry(ept, &remote_endpoints, list) {
		i += scnprintf(buf + i, max - i, "pid: 0x%08x\n", ept->pid);
		i += scnprintf(buf + i, max - i, "cid: 0x%08x\n", ept->cid);
		i += scnprintf(buf + i, max - i, "tx_quota_cntr: %i\n",
			       ept->tx_quota_cntr);
		i += scnprintf(buf + i, max - i, "quota_restart_state: %i\n",
			       ept->quota_restart_state);
		i += scnprintf(buf + i, max - i, "\n");
	}
	spin_unlock_irqrestore(&remote_endpoints_lock, flags);

	return i;
}

static int dump_msm_rpc_endpoint(char *buf, int max)
{
	int i = 0;
	unsigned long flags;
	struct msm_rpc_reply *reply;
	struct msm_rpc_endpoint *ept;
	struct rr_packet *pkt;
	const char *sym;

	spin_lock_irqsave(&local_endpoints_lock, flags);
	list_for_each_entry(ept, &local_endpoints, list) {
		i += scnprintf(buf + i, max - i, "pid: 0x%08x\n", ept->pid);
		i += scnprintf(buf + i, max - i, "cid: 0x%08x\n", ept->cid);
		i += scnprintf(buf + i, max - i, "dst_pid: 0x%08x\n",
			       ept->dst_pid);
		i += scnprintf(buf + i, max - i, "dst_cid: 0x%08x\n",
			       ept->dst_cid);
		i += scnprintf(buf + i, max - i, "dst_prog: 0x%08x",
			       be32_to_cpu(ept->dst_prog));
		sym = smd_rpc_get_sym(be32_to_cpu(ept->dst_prog));
		if (sym)
			i += scnprintf(buf + i, max - i, " (%s)\n", sym);
		else
			i += scnprintf(buf + i, max - i, "\n");
		i += scnprintf(buf + i, max - i, "dst_vers: 0x%08x\n",
			       be32_to_cpu(ept->dst_vers));
		i += scnprintf(buf + i, max - i, "reply_cnt: %i\n",
			       ept->reply_cnt);
		i += scnprintf(buf + i, max - i, "restart_state: %i\n",
			       ept->restart_state);

		i += scnprintf(buf + i, max - i, "outstanding xids:\n");
		spin_lock(&ept->reply_q_lock);
		list_for_each_entry(reply, &ept->reply_pend_q, list)
			i += scnprintf(buf + i, max - i, "    xid = %u\n",
				       ntohl(reply->xid));
		spin_unlock(&ept->reply_q_lock);

		i += scnprintf(buf + i, max - i, "complete unread packets:\n");
		spin_lock(&ept->read_q_lock);
		list_for_each_entry(pkt, &ept->read_q, list) {
			i += scnprintf(buf + i, max - i, "    mid = %i\n",
				       pkt->mid);
			i += scnprintf(buf + i, max - i, "    length = %i\n",
				       pkt->length);
		}
		spin_unlock(&ept->read_q_lock);
		i += scnprintf(buf + i, max - i, "\n");
	}
	spin_unlock_irqrestore(&local_endpoints_lock, flags);

	return i;
}

#define DEBUG_BUFMAX 4096
static char debug_buffer[DEBUG_BUFMAX];

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int (*fill)(char *buf, int max) = file->private_data;
	int bsize = fill(debug_buffer, DEBUG_BUFMAX);
	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
			 struct dentry *dent,
			 int (*fill)(char *buf, int max))
{
	debugfs_create_file(name, mode, dent, fill, &debug_ops);
}

static void debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smd_rpcrouter", 0);
	if (IS_ERR(dent))
		return;

	debug_create("dump_msm_rpc_endpoints", 0444, dent,
		     dump_msm_rpc_endpoint);
	debug_create("dump_remote_endpoints", 0444, dent,
		     dump_remote_endpoints);
	debug_create("dump_servers", 0444, dent,
		     dump_servers);

}

#else
static void debugfs_init(void) {}
#endif

static int msm_rpcrouter_add_xprt(struct rpcrouter_xprt *xprt)
{
	struct rpcrouter_xprt_info *xprt_info;

	D("Registering xprt %s to RPC Router\n", xprt->name);

	xprt_info = kmalloc(sizeof(struct rpcrouter_xprt_info), GFP_KERNEL);
	if (!xprt_info)
		return -ENOMEM;

	xprt_info->xprt = xprt;
	xprt_info->initialized = 0;
	xprt_info->remote_pid = -1;
	init_waitqueue_head(&xprt_info->read_wait);
	spin_lock_init(&xprt_info->lock);
	wake_lock_init(&xprt_info->wakelock,
		       WAKE_LOCK_SUSPEND, xprt->name);
	xprt_info->need_len = 0;
	xprt_info->abort_data_read = 0;
	INIT_WORK(&xprt_info->read_data, do_read_data);
	INIT_LIST_HEAD(&xprt_info->list);

	xprt_info->workqueue = create_singlethread_workqueue(xprt->name);
	if (!xprt_info->workqueue) {
		kfree(xprt_info);
		return -ENOMEM;
	}

	if (!strcmp(xprt->name, "rpcrouter_loopback_xprt")) {
		xprt_info->remote_pid = RPCROUTER_PID_LOCAL;
		xprt_info->initialized = 1;
	} else {
		smsm_change_state(SMSM_APPS_STATE, 0, SMSM_RPCINIT);
	}

	mutex_lock(&xprt_info_list_lock);
	list_add_tail(&xprt_info->list, &xprt_info_list);
	mutex_unlock(&xprt_info_list_lock);

	queue_work(xprt_info->workqueue, &xprt_info->read_data);

	xprt->priv = xprt_info;

	return 0;
}

static void msm_rpcrouter_remove_xprt(struct rpcrouter_xprt *xprt)
{
	struct rpcrouter_xprt_info *xprt_info;
	unsigned long flags;

	if (xprt && xprt->priv) {
		xprt_info = xprt->priv;

		/* abort rr_read thread */
		xprt_info->abort_data_read = 1;
		wake_up(&xprt_info->read_wait);

		/* remove xprt from available xprts */
		mutex_lock(&xprt_info_list_lock);
		spin_lock_irqsave(&xprt_info->lock, flags);
		list_del(&xprt_info->list);

		/* unlock the spinlock last to avoid a race
		 * condition with rpcrouter_get_xprt_info
		 * in msm_rpc_write_pkt in which the
		 * xprt is returned from rpcrouter_get_xprt_info
		 * and then deleted here. */
		mutex_unlock(&xprt_info_list_lock);
		spin_unlock_irqrestore(&xprt_info->lock, flags);

		/* cleanup workqueues and wakelocks */
		flush_workqueue(xprt_info->workqueue);
		destroy_workqueue(xprt_info->workqueue);
		wake_lock_destroy(&xprt_info->wakelock);


		/* free memory */
		xprt->priv = 0;
		kfree(xprt_info);
	}
}

struct rpcrouter_xprt_work {
	struct rpcrouter_xprt *xprt;
	struct work_struct work;
};

static void xprt_open_worker(struct work_struct *work)
{
	struct rpcrouter_xprt_work *xprt_work =
		container_of(work, struct rpcrouter_xprt_work, work);

	msm_rpcrouter_add_xprt(xprt_work->xprt);

	kfree(xprt_work);
}

static void xprt_close_worker(struct work_struct *work)
{
	struct rpcrouter_xprt_work *xprt_work =
		container_of(work, struct rpcrouter_xprt_work, work);

	modem_reset_cleanup(xprt_work->xprt->priv);
	msm_rpcrouter_remove_xprt(xprt_work->xprt);

	if (atomic_dec_return(&pending_close_count) == 0)
		wake_up(&subsystem_restart_wait);

	kfree(xprt_work);
}

void msm_rpcrouter_xprt_notify(struct rpcrouter_xprt *xprt, unsigned event)
{
	struct rpcrouter_xprt_info *xprt_info;
	struct rpcrouter_xprt_work *xprt_work;
	unsigned long flags;

	/* Workqueue is created in init function which works for all existing
	 * clients.  If this fails in the future, then it will need to be
	 * created earlier. */
	BUG_ON(!rpcrouter_workqueue);

	switch (event) {
	case RPCROUTER_XPRT_EVENT_OPEN:
		D("open event for '%s'\n", xprt->name);
		xprt_work = kmalloc(sizeof(struct rpcrouter_xprt_work),
				GFP_ATOMIC);
		xprt_work->xprt = xprt;
		INIT_WORK(&xprt_work->work, xprt_open_worker);
		queue_work(rpcrouter_workqueue, &xprt_work->work);
		break;

	case RPCROUTER_XPRT_EVENT_CLOSE:
		D("close event for '%s'\n", xprt->name);

		atomic_inc(&pending_close_count);

		xprt_work = kmalloc(sizeof(struct rpcrouter_xprt_work),
				GFP_ATOMIC);
		xprt_work->xprt = xprt;
		INIT_WORK(&xprt_work->work, xprt_close_worker);
		queue_work(rpcrouter_workqueue, &xprt_work->work);
		break;
	}

	xprt_info = xprt->priv;
	if (xprt_info) {
		spin_lock_irqsave(&xprt_info->lock, flags);
		/* Check read_avail even for OPEN event to handle missed
		   DATA events while processing the OPEN event*/
		if (xprt->read_avail() >= xprt_info->need_len)
			wake_lock(&xprt_info->wakelock);
		wake_up(&xprt_info->read_wait);
		spin_unlock_irqrestore(&xprt_info->lock, flags);
	}
}

static int modem_restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data);
static struct notifier_block nb = {
	.notifier_call = modem_restart_notifier_cb,
};

static int modem_restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data)
{
	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		D("%s: SUBSYS_BEFORE_SHUTDOWN\n", __func__);
		break;

	case SUBSYS_BEFORE_POWERUP:
		D("%s: waiting for RPC restart to complete\n", __func__);
		wait_event(subsystem_restart_wait,
			atomic_read(&pending_close_count) == 0);
		D("%s: finished restart wait\n", __func__);
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static void *restart_notifier_handle;
static __init int modem_restart_late_init(void)
{
	restart_notifier_handle = subsys_notif_register_notifier("modem", &nb);
	return 0;
}
late_initcall(modem_restart_late_init);

static int __init rpcrouter_init(void)
{
	int ret;

	msm_rpc_connect_timeout_ms = 0;
	smd_rpcrouter_debug_mask |= SMEM_LOG;
	debugfs_init();
	ret = register_reboot_notifier(&msm_rpc_reboot_notifier);
	if (ret)
		pr_err("%s: Failed to register reboot notifier", __func__);

	/* Initialize what we need to start processing */
	rpcrouter_workqueue =
		create_singlethread_workqueue("rpcrouter");
	if (!rpcrouter_workqueue) {
		msm_rpcrouter_exit_devices();
		return -ENOMEM;
	}

	init_waitqueue_head(&newserver_wait);
	init_waitqueue_head(&subsystem_restart_wait);

	ret = msm_rpcrouter_init_devices();
	if (ret < 0)
		return ret;

	return ret;
}

module_init(rpcrouter_init);
MODULE_DESCRIPTION("MSM RPC Router");
MODULE_AUTHOR("San Mehat <san@android.com>");
MODULE_LICENSE("GPL");
