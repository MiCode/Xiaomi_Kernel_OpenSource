/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s " fmt, KBUILD_MODNAME

#include <linux/atomic.h>
#include <linux/bitmap.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ipc_logging.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mailbox_client.h> /* For dev_err */
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <asm/arch_timer.h>
#include <asm-generic/io.h>

#include <soc/qcom/tcs.h>

#include <dt-bindings/soc/qcom,tcs-mbox.h>

#include "mailbox.h"

#define CREATE_TRACE_POINTS
#include <trace/events/rpmh.h>

#define RSC_DRV_IPC_LOG_SIZE		2

#define MAX_CMDS_PER_TCS		16
#define MAX_TCS_PER_TYPE		3
#define MAX_TCS_SLOTS			(MAX_CMDS_PER_TCS * MAX_TCS_PER_TYPE)

#define RSC_DRV_TCS_OFFSET		672
#define RSC_DRV_CMD_OFFSET		20

/* DRV Configuration Information Register */
#define DRV_PRNT_CHLD_CONFIG		0x0C
#define DRV_NUM_TCS_MASK		0x3F
#define DRV_NUM_TCS_SHIFT		6
#define DRV_NCPT_MASK			0x1F
#define DRV_NCPT_SHIFT			27

/* Register offsets */
#define RSC_DRV_IRQ_ENABLE		0x00
#define RSC_DRV_IRQ_STATUS		0x04
#define RSC_DRV_IRQ_CLEAR		0x08
#define RSC_DRV_CMD_WAIT_FOR_CMPL	0x10
#define RSC_DRV_CONTROL			0x14
#define RSC_DRV_STATUS			0x18
#define RSC_DRV_CMD_ENABLE		0x1C
#define RSC_DRV_CMD_MSGID		0x30
#define RSC_DRV_CMD_ADDR		0x34
#define RSC_DRV_CMD_DATA		0x38
#define RSC_DRV_CMD_STATUS		0x3C
#define RSC_DRV_CMD_RESP_DATA		0x40

#define TCS_AMC_MODE_ENABLE		BIT(16)
#define TCS_AMC_MODE_TRIGGER		BIT(24)

/* TCS CMD register bit mask */
#define CMD_MSGID_LEN			8
#define CMD_MSGID_RESP_REQ		BIT(8)
#define CMD_MSGID_WRITE			BIT(16)
#define CMD_STATUS_ISSUED		BIT(8)
#define CMD_STATUS_COMPL		BIT(16)

/* Control/Hidden TCS */
#define TCS_HIDDEN_MAX_SLOTS		2
#define TCS_HIDDEN_CMD0_DRV_DATA	0x38
#define TCS_HIDDEN_CMD_SHIFT		0x08

#define TCS_TYPE_NR			4
#define MAX_POOL_SIZE			(MAX_TCS_PER_TYPE * TCS_TYPE_NR)
#define TCS_M_INIT			0xFFFF

struct rsc_drv;

struct tcs_response {
	struct rsc_drv *drv;
	struct mbox_chan *chan;
	struct tcs_mbox_msg *msg;
	u32 m; /* m-th TCS */
	int err;
	int idx;
	bool in_use;
	struct list_head list;
};

struct tcs_response_pool {
	struct tcs_response resp[MAX_POOL_SIZE];
	spinlock_t lock;
	DECLARE_BITMAP(avail, MAX_POOL_SIZE);
};

/* One per TCS type of a controller */
struct tcs_mbox {
	struct rsc_drv *drv;
	u32 *cmd_addr;
	int type;
	u32 tcs_mask;
	u32 tcs_offset;
	int num_tcs;
	int ncpt; /* num cmds per tcs */
	DECLARE_BITMAP(slots, MAX_TCS_SLOTS);
	spinlock_t tcs_lock; /* TCS type lock */
};

/* One per MBOX controller */
struct rsc_drv {
	struct mbox_controller mbox;
	const char *name;
	unsigned long addr;
	void __iomem *base; /* start address of the RSC's registers */
	void __iomem *reg_base; /* start address for DRV specific register */
	int irq;
	int drv_id;
	struct platform_device *pdev;
	struct tcs_mbox tcs[TCS_TYPE_NR];
	int num_assigned;
	int num_tcs;
	struct tasklet_struct tasklet;
	struct list_head response_pending;
	spinlock_t drv_lock;
	struct tcs_response_pool *resp_pool;
	atomic_t tcs_in_use[MAX_POOL_SIZE];
	/* Debug info */
	u64 tcs_last_sent_ts[MAX_POOL_SIZE];
	u64 tcs_last_recv_ts[MAX_POOL_SIZE];
	atomic_t tcs_send_count[MAX_POOL_SIZE];
	atomic_t tcs_irq_count[MAX_POOL_SIZE];
	void *ipc_log_ctx;
};

/* Log to IPC and Ftrace */
#define log_send_msg(drv, m, n, i, a, d, c, t) do {			\
	trace_rpmh_send_msg(drv->name, drv->addr, m, n, i, a, d, c, t);	\
	ipc_log_string(drv->ipc_log_ctx,				\
		"send msg: m=%d n=%d msgid=0x%x addr=0x%x data=0x%x cmpl=%d trigger=%d", \
		m, n, i, a, d, c, t);					\
	} while (0)

#define log_rpmh_notify_irq(drv, m, a, e) do {				\
	trace_rpmh_notify_irq(drv->name, m, a, e);			\
	ipc_log_string(drv->ipc_log_ctx,				\
		"irq response: m=%d addr=0x%x err=%d", m, a, e);	\
	} while (0)

#define log_rpmh_control_msg(drv, d) do {				\
	trace_rpmh_control_msg(drv->name, d);				\
	ipc_log_string(drv->ipc_log_ctx, "ctrlr msg: data=0x%x", d);	\
	} while (0)

#define log_rpmh_notify(drv, m, a, e) do {				\
	trace_rpmh_notify(drv->name, m, a, e);				\
	ipc_log_string(drv->ipc_log_ctx,				\
		"tx done: m=%d addr=0x%x err=%d", m, a, e);		\
	} while (0)


static int tcs_response_pool_init(struct rsc_drv *drv)
{
	struct tcs_response_pool *pool;
	int i;

	pool = devm_kzalloc(&drv->pdev->dev, sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return -ENOMEM;

	for (i = 0; i < MAX_POOL_SIZE; i++) {
		pool->resp[i].drv = drv;
		pool->resp[i].idx = i;
		pool->resp[i].m = TCS_M_INIT;
		INIT_LIST_HEAD(&pool->resp[i].list);
	}

	spin_lock_init(&pool->lock);
	drv->resp_pool = pool;

	return 0;
}

static struct tcs_response *setup_response(struct rsc_drv *drv,
		struct tcs_mbox_msg *msg, struct mbox_chan *chan,
		u32 m, int err)
{
	struct tcs_response_pool *pool = drv->resp_pool;
	struct tcs_response *resp = ERR_PTR(-ENOMEM);
	int pos;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	pos = find_first_zero_bit(pool->avail, MAX_POOL_SIZE);
	if (pos != MAX_POOL_SIZE) {
		bitmap_set(pool->avail, pos, 1);
		resp = &pool->resp[pos];
		resp->chan = chan;
		resp->msg = msg;
		resp->m = m;
		resp->err = err;
		resp->in_use = false;
	}
	spin_unlock_irqrestore(&pool->lock, flags);

	if (pos == MAX_POOL_SIZE)
		pr_err("response pool is full\n");

	return resp;
}

static void free_response(struct tcs_response *resp)
{
	struct tcs_response_pool *pool = resp->drv->resp_pool;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	resp->err = -EINVAL;
	bitmap_clear(pool->avail, resp->idx, 1);
	spin_unlock_irqrestore(&pool->lock, flags);
}

static inline struct tcs_response *get_response(struct rsc_drv *drv, u32 m,
					bool for_use)
{
	struct tcs_response_pool *pool = drv->resp_pool;
	struct tcs_response *resp = NULL;
	int pos = 0;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	do {
		pos = find_next_bit(pool->avail, MAX_POOL_SIZE, pos);
		if (pos == MAX_POOL_SIZE)
			break;

		resp = &pool->resp[pos];
		if (resp->m == m && !resp->in_use) {
			resp->in_use = for_use;
			break;
		}
		pos++;
	} while (1);
	spin_unlock_irqrestore(&pool->lock, flags);

	return resp;
}

static void print_response(struct rsc_drv *drv, int m)
{
	struct tcs_response *resp;
	struct tcs_mbox_msg *msg;
	int i;

	resp = get_response(drv, m, false);
	if (!resp)
		return;

	msg = resp->msg;
	pr_warn("Response object [idx=%d for-tcs=%d in-use=%d]\n",
			resp->idx, resp->m, resp->in_use);
	pr_warn("Msg: state=%d\n", msg->state);
	for (i = 0; i < msg->num_payload; i++)
		pr_warn("addr=0x%x data=0x%x complete=0x%x\n",
				msg->payload[i].addr,
				msg->payload[i].data,
				msg->payload[i].complete);
}

static inline u32 read_drv_config(void __iomem *base)
{
	return le32_to_cpu(readl_relaxed(base + DRV_PRNT_CHLD_CONFIG));
}

static inline u32 read_tcs_reg(void __iomem *base, int reg, int m, int n)
{
	return le32_to_cpu(readl_relaxed(base + reg +
			RSC_DRV_TCS_OFFSET * m + RSC_DRV_CMD_OFFSET * n));
}

static inline void write_tcs_reg(void __iomem *base, int reg, int m, int n,
				u32 data)
{
	writel_relaxed(cpu_to_le32(data), base + reg +
			RSC_DRV_TCS_OFFSET * m + RSC_DRV_CMD_OFFSET * n);
}

static inline void write_tcs_reg_sync(void __iomem *base, int reg, int m, int n,
				u32 data)
{
	do {
		write_tcs_reg(base, reg, m, n, data);
		if (data == read_tcs_reg(base, reg, m, n))
			break;
		udelay(1);
	} while (1);
}

static inline bool tcs_is_free(struct rsc_drv *drv, int m)
{
	void __iomem *base = drv->reg_base;

	return read_tcs_reg(base, RSC_DRV_STATUS, m, 0) &&
			!atomic_read(&drv->tcs_in_use[m]);
}

static inline struct tcs_mbox *get_tcs_from_index(struct rsc_drv *drv, int m)
{
	struct tcs_mbox *tcs = NULL;
	int i;

	for (i = 0; i < drv->num_tcs; i++) {
		tcs = &drv->tcs[i];
		if (tcs->tcs_mask & (u32)BIT(m))
			break;
	}

	if (i == drv->num_tcs) {
		WARN(1, "Incorrect TCS index %d", m);
		tcs = NULL;
	}

	return tcs;
}

static inline struct tcs_mbox *get_tcs_of_type(struct rsc_drv *drv, int type)
{
	int i;
	struct tcs_mbox *tcs;

	for (i = 0; i < TCS_TYPE_NR; i++)
		if (type == drv->tcs[i].type)
			break;

	if (i == TCS_TYPE_NR)
		return ERR_PTR(-EINVAL);

	tcs = &drv->tcs[i];
	if (!tcs->num_tcs)
		return ERR_PTR(-EINVAL);

	return tcs;
}

static inline struct tcs_mbox *get_tcs_for_msg(struct rsc_drv *drv,
						struct tcs_mbox_msg *msg)
{
	int type = -1;

	/* Which box are we dropping this in and do we trigger the TCS */
	switch (msg->state) {
	case RPMH_SLEEP_STATE:
		type = SLEEP_TCS;
		break;
	case RPMH_WAKE_ONLY_STATE:
		type = WAKE_TCS;
		break;
	case RPMH_ACTIVE_ONLY_STATE:
		type = ACTIVE_TCS;
		break;
	case RPMH_AWAKE_STATE:
		/*
		 * Awake state is only used when the DRV has no separate
		 * TCS for ACTIVE requests. Switch to WAKE TCS to send
		 * active votes. Otherwise, the caller should be explicit
		 * about the state.
		 */
		if (IS_ERR(get_tcs_of_type(drv, ACTIVE_TCS)))
			type = WAKE_TCS;
		break;
	}

	if (msg->is_read)
		type = ACTIVE_TCS;

	if (type < 0)
		return ERR_PTR(-EINVAL);

	return get_tcs_of_type(drv, type);
}

static inline void send_tcs_response(struct tcs_response *resp)
{
	struct rsc_drv *drv = resp->drv;
	unsigned long flags;

	spin_lock_irqsave(&drv->drv_lock, flags);
	INIT_LIST_HEAD(&resp->list);
	list_add_tail(&resp->list, &drv->response_pending);
	spin_unlock_irqrestore(&drv->drv_lock, flags);

	tasklet_hi_schedule(&drv->tasklet);
}

static inline void enable_tcs_irq(struct rsc_drv *drv, int m, bool enable)
{
	void __iomem *base = drv->reg_base;
	u32 data;

	/* Enable interrupts for non-ACTIVE TCS */
	data = read_tcs_reg(base, RSC_DRV_IRQ_ENABLE, 0, 0);
	if (enable)
		data |= BIT(m);
	else
		data &= ~BIT(m);
	write_tcs_reg(base, RSC_DRV_IRQ_ENABLE, 0, 0, data);
}

/**
 * tcs_irq_handler: TX Done / Recv data handler
 */
static irqreturn_t tcs_irq_handler(int irq, void *p)
{
	struct rsc_drv *drv = p;
	void __iomem *base = drv->reg_base;
	int m, i;
	u32 irq_status, sts;
	struct tcs_mbox *tcs;
	struct tcs_response *resp;
	struct tcs_cmd *cmd;
	u32 data;

	/* Know which TCSes were triggered */
	irq_status = read_tcs_reg(base, RSC_DRV_IRQ_STATUS, 0, 0);

	for (m = 0; m < drv->num_tcs; m++) {
		if (!(irq_status & (u32)BIT(m)))
			continue;
		atomic_inc(&drv->tcs_irq_count[m]);

		resp = get_response(drv, m, true);
		if (!resp) {
			pr_err("No resp request for TCS-%d\n", m);
			goto no_resp;
		}

		/* Check if all commands were completed */
		resp->err = 0;
		for (i = 0; i < resp->msg->num_payload; i++) {
			cmd = &resp->msg->payload[i];
			sts = read_tcs_reg(base, RSC_DRV_CMD_STATUS, m, i);
			if ((!(sts & CMD_STATUS_ISSUED)) ||
				((resp->msg->is_complete || cmd->complete) &&
				(!(sts & CMD_STATUS_COMPL)))) {
				resp->err = -EIO;
				break;
			}
		}

		/* Check for response if this was a read request */
		if (resp->msg->is_read) {
			/* Respond the data back in the same req data */
			data = read_tcs_reg(base, RSC_DRV_CMD_RESP_DATA, m, 0);
			resp->msg->payload[0].data = data;
			mbox_chan_received_data(resp->chan, resp->msg);
		}

		log_rpmh_notify_irq(drv, m, resp->msg->payload[0].addr,
						resp->err);

		/* Clear the AMC mode for non-ACTIVE TCSes */
		tcs = get_tcs_from_index(drv, m);
		if (tcs && tcs->type != ACTIVE_TCS) {
			data = read_tcs_reg(base, RSC_DRV_CONTROL, m, 0);
			data &= ~TCS_AMC_MODE_TRIGGER;
			write_tcs_reg_sync(base, RSC_DRV_CONTROL, m, 0, data);
			data &= ~TCS_AMC_MODE_ENABLE;
			write_tcs_reg(base, RSC_DRV_CONTROL, m, 0, data);
			/*
			 * Disable interrupt for this TCS to avoid being
			 * spammed with interrupts coming when the solver
			 * sends its wake votes.
			 */
			enable_tcs_irq(drv, m, false);
		} else {
			/* Clear the enable bit for the commands */
			write_tcs_reg(base, RSC_DRV_CMD_ENABLE, m, 0, 0);
			write_tcs_reg(base, RSC_DRV_CMD_WAIT_FOR_CMPL, m, 0, 0);
		}

no_resp:
		/* Record the recvd time stamp */
		drv->tcs_last_recv_ts[m] = arch_counter_get_cntvct();

		/* Clear the TCS IRQ status */
		write_tcs_reg(base, RSC_DRV_IRQ_CLEAR, 0, 0, BIT(m));

		/* Notify the client that this request is completed. */
		atomic_set(&drv->tcs_in_use[m], 0);

		/* Clean up response object and notify mbox in tasklet */
		if (resp)
			send_tcs_response(resp);
	}

	return IRQ_HANDLED;
}

static inline void mbox_notify_tx_done(struct mbox_chan *chan,
				struct tcs_mbox_msg *msg, int m, int err)
{
	struct rsc_drv *drv = container_of(chan->mbox, struct rsc_drv, mbox);

	log_rpmh_notify(drv, m, msg->payload[0].addr, err);
	mbox_chan_txdone(chan, err);
}

static void respond_tx_done(struct tcs_response *resp)
{
	struct mbox_chan *chan = resp->chan;
	struct tcs_mbox_msg *msg = resp->msg;
	int err = resp->err;
	int m = resp->m;

	free_response(resp);
	mbox_notify_tx_done(chan, msg, m, err);
}

/**
 * tcs_notify_tx_done: TX Done for requests that do not trigger TCS
 */
static void tcs_notify_tx_done(unsigned long data)
{
	struct rsc_drv *drv = (struct rsc_drv *)data;
	struct tcs_response *resp;
	unsigned long flags;

	do {
		spin_lock_irqsave(&drv->drv_lock, flags);
		if (list_empty(&drv->response_pending)) {
			spin_unlock_irqrestore(&drv->drv_lock, flags);
			break;
		}
		resp = list_first_entry(&drv->response_pending,
					struct tcs_response, list);
		list_del(&resp->list);
		spin_unlock_irqrestore(&drv->drv_lock, flags);
		respond_tx_done(resp);
	} while (1);
}

static void __tcs_buffer_write(struct rsc_drv *drv, int d, int m, int n,
			struct tcs_mbox_msg *msg, bool trigger)
{
	u32 msgid, cmd_msgid = 0;
	u32 cmd_enable = 0;
	u32 cmd_complete;
	u32 enable;
	struct tcs_cmd *cmd;
	int i;
	void __iomem *base = drv->reg_base;

	/* We have homologous command set i.e pure read or write, not a mix */
	cmd_msgid = CMD_MSGID_LEN;
	cmd_msgid |= (msg->is_complete) ? CMD_MSGID_RESP_REQ : 0;
	cmd_msgid |= (!msg->is_read) ? CMD_MSGID_WRITE : 0;

	/* Read the send-after-prev complete flag for those already in TCS */
	cmd_complete = read_tcs_reg(base, RSC_DRV_CMD_WAIT_FOR_CMPL, m, 0);

	for (i = 0; i < msg->num_payload; i++) {
		cmd = &msg->payload[i];
		cmd_enable |= BIT(n + i);
		cmd_complete |= cmd->complete << (n + i);
		msgid = cmd_msgid;
		msgid |= (cmd->complete) ? CMD_MSGID_RESP_REQ : 0;
		write_tcs_reg(base, RSC_DRV_CMD_MSGID, m, n + i, msgid);
		write_tcs_reg(base, RSC_DRV_CMD_ADDR, m, n + i, cmd->addr);
		write_tcs_reg(base, RSC_DRV_CMD_DATA, m, n + i, cmd->data);
		log_send_msg(drv, m, n + i, msgid, cmd->addr,
					cmd->data, cmd->complete, trigger);
	}

	/* Write the send-after-prev completion bits for the batch */
	write_tcs_reg(base, RSC_DRV_CMD_WAIT_FOR_CMPL, m, 0, cmd_complete);

	/* Enable the new commands in TCS */
	cmd_enable |= read_tcs_reg(base, RSC_DRV_CMD_ENABLE, m, 0);
	write_tcs_reg(base, RSC_DRV_CMD_ENABLE, m, 0, cmd_enable);

	if (trigger) {
		/*
		 * HW req: Clear the DRV_CONTROL and enable TCS again
		 * While clearing ensure that the AMC mode trigger is cleared
		 * and then the mode enable is cleared.
		 */
		enable = read_tcs_reg(base, RSC_DRV_CONTROL, m, 0);
		enable &= ~TCS_AMC_MODE_TRIGGER;
		write_tcs_reg_sync(base, RSC_DRV_CONTROL, m, 0, enable);
		enable &= ~TCS_AMC_MODE_ENABLE;
		write_tcs_reg_sync(base, RSC_DRV_CONTROL, m, 0, enable);

		/* Enable the AMC mode on the TCS and then trigger the TCS */
		enable = TCS_AMC_MODE_ENABLE;
		write_tcs_reg_sync(base, RSC_DRV_CONTROL, m, 0, enable);
		enable |= TCS_AMC_MODE_TRIGGER;
		write_tcs_reg(base, RSC_DRV_CONTROL, m, 0, enable);
	}
}

/**
 * rsc_drv_is_idle: Check if any of the AMCs are busy.
 *
 * @mbox: The mailbox controller.
 *
 * Returns true if the AMCs are not engaged or absent.
 */
static bool rsc_drv_is_idle(struct mbox_controller *mbox)
{
	int m;
	struct rsc_drv *drv = container_of(mbox, struct rsc_drv, mbox);
	struct tcs_mbox *tcs = get_tcs_of_type(drv, ACTIVE_TCS);

	/* Check for WAKE TCS if there are no ACTIVE TCS */
	if (IS_ERR(tcs))
		tcs = get_tcs_of_type(drv, WAKE_TCS);

	for (m = tcs->tcs_offset; m < tcs->tcs_offset + tcs->num_tcs; m++)
		if (!tcs_is_free(drv, m))
			return false;

	return true;
}

static int check_for_req_inflight(struct rsc_drv *drv, struct tcs_mbox *tcs,
						struct tcs_mbox_msg *msg)
{
	u32 curr_enabled, addr;
	int i, j, k;
	void __iomem *base = drv->reg_base;
	int m = tcs->tcs_offset;

	for (i = 0; i < tcs->num_tcs; i++, m++) {
		if (tcs_is_free(drv, m))
			continue;

		curr_enabled = read_tcs_reg(base, RSC_DRV_CMD_ENABLE, m, 0);

		for (j = 0; j < MAX_CMDS_PER_TCS; j++) {
			if (!(curr_enabled & (u32)BIT(j)))
				continue;

			addr = read_tcs_reg(base, RSC_DRV_CMD_ADDR, m, j);
			for (k = 0; k < msg->num_payload; k++) {
				if (addr == msg->payload[k].addr)
					return -EBUSY;
			}
		}
	}

	return 0;
}

static int find_free_tcs(struct tcs_mbox *tcs)
{
	int slot = -EBUSY;
	int m = 0;

	/* Loop until we find a free AMC */
	for (m = 0; m < tcs->num_tcs; m++) {
		if (tcs_is_free(tcs->drv, tcs->tcs_offset + m)) {
			slot = m * tcs->ncpt;
			break;
		}
	}

	return slot;
}

static int find_match(struct tcs_mbox *tcs, struct tcs_cmd *cmd, int len)
{
	bool found = false;
	int i = 0, j;

	/* Check for already cached commands */
	while ((i = find_next_bit(tcs->slots, MAX_TCS_SLOTS, i)) <
			MAX_TCS_SLOTS) {
		if (tcs->cmd_addr[i] != cmd[0].addr) {
			i++;
			continue;
		}
		/* sanity check to ensure the seq is same */
		for (j = 1; j < len; j++) {
			if (tcs->cmd_addr[i + j] != cmd[j].addr) {
				pr_debug("Message does not match previous sequence.\n");
				return -EINVAL;
			}
		}
		found = true;
		break;
	}

	return found ? i : -1;
}

static int find_slots(struct tcs_mbox *tcs, struct tcs_mbox_msg *msg)
{
	int slot;
	int n = 0;

	/* For active requests find the first free AMC. */
	if (msg->state == RPMH_ACTIVE_ONLY_STATE ||
			msg->state == RPMH_AWAKE_STATE)
		return find_free_tcs(tcs);

	/* Find if we already have the msg in our TCS */
	slot = find_match(tcs, msg->payload, msg->num_payload);
	if (slot >= 0)
		return slot;

	/* Do over, until we can fit the full payload in a TCS */
	do {
		slot = bitmap_find_next_zero_area(tcs->slots, MAX_TCS_SLOTS,
						n, msg->num_payload, 0);
		if (slot >= MAX_TCS_SLOTS)
			break;
		n += tcs->ncpt;
	} while (slot + msg->num_payload - 1 >= n);

	return (slot < MAX_TCS_SLOTS) ? slot : -ENOMEM;
}

static int tcs_mbox_write(struct mbox_chan *chan, struct tcs_mbox_msg *msg,
				bool trigger)
{
	struct rsc_drv *drv = container_of(chan->mbox, struct rsc_drv, mbox);
	int d = drv->drv_id;
	struct tcs_mbox *tcs;
	int i, slot, offset, m, n, ret;
	struct tcs_response *resp = NULL;
	unsigned long flags;

	tcs = get_tcs_for_msg(drv, msg);
	if (IS_ERR(tcs))
		return PTR_ERR(tcs);

	if (trigger) {
		resp = setup_response(drv, msg, chan, TCS_M_INIT, 0);
		if (IS_ERR_OR_NULL(resp))
			return -EBUSY;
	}

	/* Identify the sequential slots that we can write to */
	spin_lock_irqsave(&tcs->tcs_lock, flags);
	slot = find_slots(tcs, msg);
	if (slot < 0) {
		spin_unlock_irqrestore(&tcs->tcs_lock, flags);
		if (resp)
			free_response(resp);
		return slot;
	}

	/* Figure out the TCS-m and CMD-n to write to */
	offset = slot / tcs->ncpt;
	m = offset + tcs->tcs_offset;
	n = slot % tcs->ncpt;

	if (trigger) {
		/* Block, if we have an address from the msg in flight */
		ret = check_for_req_inflight(drv, tcs, msg);
		if (ret) {
			spin_unlock_irqrestore(&tcs->tcs_lock, flags);
			if (resp)
				free_response(resp);
			return ret;
		}

		resp->m = m;
		/* Mark the TCS as busy */
		atomic_set(&drv->tcs_in_use[m], 1);
		atomic_inc(&drv->tcs_send_count[m]);
		/* Enable interrupt for active votes through wake TCS */
		if (tcs->type != ACTIVE_TCS)
			enable_tcs_irq(drv, m, true);
		drv->tcs_last_sent_ts[m] = arch_counter_get_cntvct();
	} else {
		/* Mark the slots as in-use, before we unlock */
		if (tcs->type == SLEEP_TCS || tcs->type == WAKE_TCS)
			bitmap_set(tcs->slots, slot, msg->num_payload);

		/* Copy the addresses of the resources over to the slots */
		for (i = 0; tcs->cmd_addr && i < msg->num_payload; i++)
			tcs->cmd_addr[slot + i] = msg->payload[i].addr;
	}

	/* Write to the TCS or AMC */
	__tcs_buffer_write(drv, d, m, n, msg, trigger);

	spin_unlock_irqrestore(&tcs->tcs_lock, flags);

	return 0;
}

static void __tcs_buffer_invalidate(void __iomem *base, int m)
{
	write_tcs_reg(base, RSC_DRV_CMD_ENABLE, m, 0, 0);
	write_tcs_reg(base, RSC_DRV_CMD_WAIT_FOR_CMPL, m, 0, 0);
}

static int tcs_mbox_invalidate(struct mbox_chan *chan)
{
	struct rsc_drv *drv = container_of(chan->mbox, struct rsc_drv, mbox);
	struct tcs_mbox *tcs;
	int m, i;
	int inv_types[] = { WAKE_TCS, SLEEP_TCS };
	int type = 0;
	unsigned long flags;

	do {
		tcs = get_tcs_of_type(drv, inv_types[type]);
		if (IS_ERR(tcs))
			return PTR_ERR(tcs);

		spin_lock_irqsave(&tcs->tcs_lock, flags);
		for (i = 0; i < tcs->num_tcs; i++) {
			m = i + tcs->tcs_offset;
			if (!tcs_is_free(drv, m)) {
				spin_unlock_irqrestore(&tcs->tcs_lock, flags);
				return -EBUSY;
			}
			__tcs_buffer_invalidate(drv->reg_base, m);
		}
		/* Mark the TCS as free */
		bitmap_zero(tcs->slots, MAX_TCS_SLOTS);
		spin_unlock_irqrestore(&tcs->tcs_lock, flags);
	} while (++type < ARRAY_SIZE(inv_types));

	return 0;
}

static void print_tcs_regs(struct rsc_drv *drv, int m)
{
	int n;
	struct tcs_mbox *tcs = get_tcs_from_index(drv, m);
	void __iomem *base = drv->reg_base;
	u32 enable, addr, data, msgid, sts, irq_sts;

	if (!tcs || tcs_is_free(drv, m))
		return;

	enable = read_tcs_reg(base, RSC_DRV_CMD_ENABLE, m, 0);
	if (!enable)
		return;

	pr_warn("RSC:%s\n", drv->name);

	sts = read_tcs_reg(base, RSC_DRV_STATUS, m, 0);
	data = read_tcs_reg(base, RSC_DRV_CONTROL, m, 0);
	irq_sts = read_tcs_reg(base, RSC_DRV_IRQ_STATUS, 0, 0);
	pr_warn("TCS=%d [ctrlr-sts:%s amc-mode:0x%x irq-sts:%s]\n",
			m, sts ? "IDLE" : "BUSY", data,
			(irq_sts & BIT(m)) ? "COMPLETED" : "PENDING");

	for (n = 0; n < tcs->ncpt; n++) {
		if (!(enable & BIT(n)))
			continue;
		addr = read_tcs_reg(base, RSC_DRV_CMD_ADDR, m, n);
		data = read_tcs_reg(base, RSC_DRV_CMD_DATA, m, n);
		msgid = read_tcs_reg(base, RSC_DRV_CMD_MSGID, m, n);
		sts = read_tcs_reg(base, RSC_DRV_CMD_STATUS, m, n);
		pr_warn("\tCMD=%d [addr=0x%x data=0x%x hdr=0x%x sts=0x%x]\n",
						n, addr, data, msgid, sts);
	}
}

static void dump_tcs_stats(struct rsc_drv *drv)
{
	int i;
	unsigned long long curr = arch_counter_get_cntvct();
	struct irq_data *rsc_irq_data = irq_get_irq_data(drv->irq);
	bool irq_sts;

	for (i = 0; i < drv->num_tcs; i++) {
		if (!atomic_read(&drv->tcs_in_use[i]))
			continue;
		pr_warn("Time: %llu: TCS-%d:\n\tReq Sent:%d Last Sent:%llu\n\tResp Recv:%d Last Recvd:%llu\n",
				curr, i,
				atomic_read(&drv->tcs_send_count[i]),
				drv->tcs_last_sent_ts[i],
				atomic_read(&drv->tcs_irq_count[i]),
				drv->tcs_last_recv_ts[i]);
		print_tcs_regs(drv, i);
		print_response(drv, i);
	}

	if (rsc_irq_data) {
		irq_get_irqchip_state(drv->irq, IRQCHIP_STATE_PENDING,
								&irq_sts);
		pr_warn("HW IRQ %lu is %s at GIC\n", rsc_irq_data->hwirq,
					irq_sts ? "PENDING" : "NOT PENDING");
	}

	if (test_bit(TASKLET_STATE_SCHED, &drv->tasklet.state))
		pr_warn("Tasklet is scheduled for execution\n");
	else if (test_bit(TASKLET_STATE_RUN, &drv->tasklet.state))
		pr_warn("Tasklet is running\n");
	else
		pr_warn("Tasklet is not active\n");
}

static void chan_debug(struct mbox_chan *chan)
{
	struct rsc_drv *drv = container_of(chan->mbox, struct rsc_drv, mbox);

	dump_tcs_stats(drv);
}

/**
 * chan_tcs_write: Validate the incoming message and write to the
 * appropriate TCS block.
 *
 * @chan: the MBOX channel
 * @data: the tcs_mbox_msg*
 *
 * Returns a negative error for invalid message structure and invalid
 * message combination, -EBUSY if there is an other active request for
 * the channel in process, otherwise bubbles up internal error.
 */
static int chan_tcs_write(struct mbox_chan *chan, void *data)
{
	struct rsc_drv *drv = container_of(chan->mbox, struct rsc_drv, mbox);
	struct tcs_mbox_msg *msg = data;
	const struct device *dev = chan->cl->dev;
	int ret = 0;

	if (!msg) {
		dev_err(dev, "Payload error\n");
		ret = -EINVAL;
		goto tx_fail;
	}

	if (!msg->payload || !msg->num_payload ||
			msg->num_payload > MAX_RPMH_PAYLOAD) {
		dev_err(dev, "Payload error\n");
		ret = -EINVAL;
		goto tx_fail;
	}

	if (msg->invalidate || msg->is_control) {
		dev_err(dev, "Incorrect API\n");
		ret = -EINVAL;
		goto tx_fail;
	}

	if (msg->state != RPMH_ACTIVE_ONLY_STATE &&
			msg->state != RPMH_AWAKE_STATE) {
		dev_err(dev, "Incorrect API\n");
		ret = -EINVAL;
		goto tx_fail;
	}

	/* Read requests should always be single */
	if (msg->is_read && msg->num_payload > 1) {
		dev_err(dev, "Incorrect read request\n");
		ret = -EINVAL;
		goto tx_fail;
	}

	/*
	 * Since we are re-purposing the wake TCS, invalidate previous
	 * contents to avoid confusion.
	 */
	if (msg->state == RPMH_AWAKE_STATE) {
		ret = tcs_mbox_invalidate(chan);
		if (ret)
			goto tx_fail;
	}

	/* Post the message to the TCS and trigger */
	ret = tcs_mbox_write(chan, msg, true);

tx_fail:
	/* If there was an error in the request, schedule a response */
	if (ret < 0 && ret != -EBUSY) {
		struct tcs_response *resp = setup_response(
				drv, msg, chan, TCS_M_INIT, ret);

		dev_err(dev, "Error sending RPMH message %d\n", ret);
		if (!IS_ERR(resp))
			send_tcs_response(resp);
		else
			dev_err(dev, "No response object %ld\n", PTR_ERR(resp));
		ret = 0;
	}

	/* If we were just busy waiting for TCS, dump the state and return */
	if (ret == -EBUSY) {
		dev_err_ratelimited(chan->cl->dev,
				"TCS Busy, retrying RPMH message send\n");
		ret = -EAGAIN;
	}

	return ret;
}

static void __tcs_write_hidden(struct rsc_drv *drv, int d,
					struct tcs_mbox_msg *msg)
{
	int i;
	void __iomem *addr = drv->base + TCS_HIDDEN_CMD0_DRV_DATA;

	for (i = 0; i < msg->num_payload; i++) {
		/* Only data is write capable */
		writel_relaxed(cpu_to_le32(msg->payload[i].data), addr);
		log_rpmh_control_msg(drv, msg->payload[i].data);
		addr += TCS_HIDDEN_CMD_SHIFT;
	}
}

static int tcs_control_write(struct mbox_chan *chan, struct tcs_mbox_msg *msg)
{
	const struct device *dev = chan->cl->dev;
	struct rsc_drv *drv = container_of(chan->mbox, struct rsc_drv, mbox);
	struct tcs_mbox *tcs;
	unsigned long flags;

	tcs = get_tcs_of_type(drv, CONTROL_TCS);
	if (IS_ERR(tcs))
		return PTR_ERR(tcs);

	if (msg->num_payload != tcs->ncpt) {
		dev_err(dev, "Request must fit the control TCS size\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&tcs->tcs_lock, flags);
	__tcs_write_hidden(tcs->drv, drv->drv_id, msg);
	spin_unlock_irqrestore(&tcs->tcs_lock, flags);

	return 0;
}

/**
 * chan_tcs_ctrl_write: Write message to the controller, no ACK sent.
 *
 * @chan: the MBOX channel
 * @data: the tcs_mbox_msg*
 */
static int chan_tcs_ctrl_write(struct mbox_chan *chan, void *data)
{
	struct tcs_mbox_msg *msg = data;
	const struct device *dev = chan->cl->dev;
	int ret = -EINVAL;

	if (!msg) {
		dev_err(dev, "Payload error\n");
		goto tx_done;
	}

	if (!msg->payload || (!msg->num_payload && !msg->invalidate) ||
			msg->num_payload > MAX_RPMH_PAYLOAD) {
		dev_err(dev, "Payload error\n");
		goto tx_done;
	}

	/* Invalidate sleep/wake TCS */
	if (msg->invalidate) {
		ret = tcs_mbox_invalidate(chan);
		goto tx_done;
	}

	/* Control slots are unique. They carry specific data. */
	if (msg->is_control) {
		ret = tcs_control_write(chan, msg);
		goto tx_done;
	}

	/* Post the message to the TCS without trigger */
	ret = tcs_mbox_write(chan, msg, false);

tx_done:
	return ret;
}

static int chan_init(struct mbox_chan *chan)
{
	return 0;
}

static void chan_shutdown(struct mbox_chan *chan)
{ }

static const struct mbox_chan_ops mbox_ops = {
	.send_data = chan_tcs_write,
	.write_controller_data = chan_tcs_ctrl_write,
	.startup = chan_init,
	.shutdown = chan_shutdown,
};

static struct mbox_chan *of_tcs_mbox_xlate(struct mbox_controller *mbox,
				const struct of_phandle_args *sp)
{
	struct rsc_drv *drv = container_of(mbox, struct rsc_drv, mbox);
	struct mbox_chan *chan;

	if (drv->num_assigned >= mbox->num_chans) {
		pr_err("TCS-Mbox out of channel memory\n");
		return ERR_PTR(-ENOMEM);
	}

	chan = &mbox->chans[drv->num_assigned++];
	chan->con_priv = drv;

	return chan;
}

static int rsc_drv_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *np;
	struct rsc_drv *drv;
	struct mbox_chan *chans;
	struct tcs_mbox *tcs;
	struct of_phandle_args p;
	int irq;
	u32 val[8] = { 0 };
	int num_chans = 0;
	int st = 0;
	int i, j, ret, nelem;
	u32 config, max_tcs, ncpt;
	int tcs_type_count[TCS_TYPE_NR] = { 0 };
	struct resource *res;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	ret = of_property_read_u32(dn, "qcom,drv-id", &drv->drv_id);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	drv->addr = res->start;
	drv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(drv->base))
		return PTR_ERR(drv->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;
	drv->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(drv->reg_base))
		return PTR_ERR(drv->reg_base);

	config = read_drv_config(drv->base);
	max_tcs = config & (DRV_NUM_TCS_MASK <<
				(DRV_NUM_TCS_SHIFT * drv->drv_id));
	max_tcs = max_tcs >> (DRV_NUM_TCS_SHIFT * drv->drv_id);
	ncpt = config & (DRV_NCPT_MASK << DRV_NCPT_SHIFT);
	ncpt = ncpt >> DRV_NCPT_SHIFT;

	nelem = of_property_count_elems_of_size(dn, "qcom,tcs-config",
						sizeof(u32));
	if (!nelem || (nelem % 2) || (nelem > 2 * TCS_TYPE_NR))
		return -EINVAL;

	ret = of_property_read_u32_array(dn, "qcom,tcs-config", val, nelem);
	if (ret)
		return ret;

	/* Ensure we have exactly not more than one of each type in DT */
	for (i = 0; i < (nelem / 2); i++) {
		if (val[2 * i] >= TCS_TYPE_NR)
			return -EINVAL;
		tcs_type_count[val[2 * i]]++;
		if (tcs_type_count[val[2 * i]] > 1)
			return -EINVAL;
	}

	/* Ensure we have each type specified in DT */
	for (i = 0; i < ARRAY_SIZE(tcs_type_count); i++)
		if (!tcs_type_count[i])
			return -EINVAL;

	for (i = 0; i < (nelem / 2); i++) {
		tcs = &drv->tcs[val[2 * i]];
		tcs->drv = drv;
		tcs->type = val[2 * i];
		tcs->num_tcs = val[2 * i + 1];
		tcs->ncpt = (tcs->type == CONTROL_TCS) ? TCS_HIDDEN_MAX_SLOTS
							: ncpt;
		spin_lock_init(&tcs->tcs_lock);

		if (tcs->num_tcs <= 0 || tcs->type == CONTROL_TCS)
			continue;

		if (tcs->num_tcs > MAX_TCS_PER_TYPE ||
			st + tcs->num_tcs > max_tcs ||
			st + tcs->num_tcs >=
				BITS_PER_BYTE * sizeof(tcs->tcs_mask))
			return -EINVAL;

		tcs->tcs_mask = ((1 << tcs->num_tcs) - 1) << st;
		tcs->tcs_offset = st;
		st += tcs->num_tcs;

		tcs->cmd_addr = devm_kzalloc(&pdev->dev, sizeof(u32) *
					tcs->num_tcs * tcs->ncpt, GFP_KERNEL);
		if (!tcs->cmd_addr)
			return -ENOMEM;

	}

	/* Allocate only that many channels specified in DT for our MBOX */
	for_each_node_with_property(np, "mboxes") {
		if (!of_device_is_available(np))
			continue;
		i = of_count_phandle_with_args(np, "mboxes", "#mbox-cells");
		for (j = 0; j < i; j++) {
			ret = of_parse_phandle_with_args(np, "mboxes",
							"#mbox-cells", j, &p);
			of_node_put(p.np);
			if (!ret && p.np == pdev->dev.of_node) {
				num_chans++;
				break;
			}
		}
	}

	if (!num_chans) {
		pr_err("%s: No clients for controller (%s)\n", __func__,
							dn->full_name);
		return -ENODEV;
	}

	chans = devm_kzalloc(&pdev->dev, num_chans * sizeof(*chans),
				GFP_KERNEL);
	if (!chans)
		return -ENOMEM;

	for (i = 0; i < num_chans; i++) {
		chans[i].mbox = &drv->mbox;
		chans[i].txdone_method = TXDONE_BY_IRQ;
	}

	drv->mbox.dev = &pdev->dev;
	drv->mbox.ops = &mbox_ops;
	drv->mbox.chans = chans;
	drv->mbox.num_chans = num_chans;
	drv->mbox.txdone_irq = true;
	drv->mbox.of_xlate = of_tcs_mbox_xlate;
	drv->mbox.is_idle = rsc_drv_is_idle;
	drv->mbox.debug = chan_debug;
	drv->num_tcs = st;
	drv->pdev = pdev;
	INIT_LIST_HEAD(&drv->response_pending);
	spin_lock_init(&drv->drv_lock);
	tasklet_init(&drv->tasklet, tcs_notify_tx_done, (unsigned long)drv);

	drv->name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!drv->name)
		drv->name = dev_name(&pdev->dev);

	ret = tcs_response_pool_init(drv);
	if (ret)
		return ret;

	irq = of_irq_get(dn, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, tcs_irq_handler,
			IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
			drv->name, drv);
	if (ret)
		return ret;

	drv->irq = irq;

	/* Enable interrupts for AMC TCS */
	write_tcs_reg(drv->reg_base, RSC_DRV_IRQ_ENABLE, 0, 0,
					drv->tcs[ACTIVE_TCS].tcs_mask);

	for (i = 0; i < ARRAY_SIZE(drv->tcs_in_use); i++)
		atomic_set(&drv->tcs_in_use[i], 0);

	drv->ipc_log_ctx = ipc_log_context_create(RSC_DRV_IPC_LOG_SIZE,
						drv->name, 0);

	ret = mbox_controller_register(&drv->mbox);
	if (ret)
		return ret;

	pr_debug("Mailbox controller (%s, drv=%d) registered\n",
					dn->full_name, drv->drv_id);

	return 0;
}

static const struct of_device_id rsc_drv_match[] = {
	{ .compatible = "qcom,tcs-drv", },
	{ }
};

static struct platform_driver rpmh_mbox_driver = {
	.probe = rsc_drv_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = rsc_drv_match,
		.suppress_bind_attrs = true,
	},
};

static int __init rpmh_mbox_driver_init(void)
{
	return platform_driver_register(&rpmh_mbox_driver);
}
arch_initcall(rpmh_mbox_driver_init);
