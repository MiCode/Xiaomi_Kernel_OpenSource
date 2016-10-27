/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/bitmap.h>
#include <linux/interrupt.h>
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
#include <linux/workqueue.h>

#include <asm-generic/io.h>

#include <soc/qcom/tcs.h>

#include <dt-bindings/soc/qcom,tcs-mbox.h>

#include "mailbox.h"

#define MAX_CMDS_PER_TCS		16
#define MAX_TCS_PER_TYPE		3
#define MAX_TCS_SLOTS			(MAX_CMDS_PER_TCS * MAX_TCS_PER_TYPE)

#define TCS_DRV_TCS_OFFSET		672
#define TCS_DRV_CMD_OFFSET		20

/* DRV Configuration Information Register */
#define DRV_PRNT_CHLD_CONFIG		0x0C
#define DRV_NUM_TCS_MASK		0x3F
#define DRV_NUM_TCS_SHIFT		6
#define DRV_NCPT_MASK			0x1F
#define DRV_NCPT_SHIFT			27

/* Register offsets */
#define TCS_DRV_IRQ_ENABLE		0x00
#define TCS_DRV_IRQ_STATUS		0x04
#define TCS_DRV_IRQ_CLEAR		0x08
#define TCS_DRV_CMD_WAIT_FOR_CMPL	0x10
#define TCS_DRV_CONTROL			0x14
#define TCS_DRV_STATUS			0x18
#define TCS_DRV_CMD_ENABLE		0x1C
#define TCS_DRV_CMD_MSGID		0x30
#define TCS_DRV_CMD_ADDR		0x34
#define TCS_DRV_CMD_DATA		0x38
#define TCS_DRV_CMD_STATUS		0x3C
#define TCS_DRV_CMD_RESP_DATA		0x40

#define TCS_AMC_MODE_ENABLE		BIT(16)
#define TCS_AMC_MODE_TRIGGER		BIT(24)

/* TCS CMD register bit mask */
#define CMD_MSGID_LEN			8
#define CMD_MSGID_RESP_REQ		BIT(8)
#define CMD_MSGID_WRITE			BIT(16)
#define CMD_STATUS_ISSUED		BIT(8)
#define CMD_STATUS_COMPL		BIT(16)

/* Control/Hidden TCS */
#define TCS_HIDDEN_MAX_SLOTS		3
#define TCS_HIDDEN_CMD0_DRV_ADDR	0x34
#define TCS_HIDDEN_CMD0_DRV_DATA	0x38
#define TCS_HIDDEN_CMD_SHIFT		0x08

#define TCS_TYPE_NR			4
#define TCS_MBOX_TOUT_MS		2000
#define MAX_POOL_SIZE			(MAX_TCS_PER_TYPE * TCS_TYPE_NR)

struct tcs_drv;

struct tcs_response {
	struct tcs_drv *drv;
	struct mbox_chan *chan;
	struct tcs_mbox_msg *msg;
	u32 m; /* m-th TCS */
	struct tasklet_struct tasklet;
	struct delayed_work dwork;
	int err;
};

struct tcs_response_pool {
	struct tcs_response *resp;
	spinlock_t lock;
	DECLARE_BITMAP(avail, MAX_POOL_SIZE);
};

/* One per TCS type of a controller */
struct tcs_mbox {
	struct tcs_drv *drv;
	u32 *cmd_addr;
	int type;
	u32 tcs_mask;
	u32 tcs_offset;
	int num_tcs;
	int ncpt; /* num cmds per tcs */
	DECLARE_BITMAP(slots, MAX_TCS_SLOTS);
	spinlock_t tcs_lock; /* TCS type lock */
	spinlock_t tcs_m_lock[MAX_TCS_PER_TYPE];
	struct tcs_response *resp[MAX_TCS_PER_TYPE];
};

/* One per MBOX controller */
struct tcs_drv {
	void *base; /* start address of the RSC's registers */
	void *reg_base; /* start address for DRV specific register */
	int drv_id;
	struct platform_device *pdev;
	struct mbox_controller mbox;
	struct tcs_mbox tcs[TCS_TYPE_NR];
	int num_assigned;
	int num_tcs;
	struct workqueue_struct *wq;
	struct tcs_response_pool *resp_pool;
};

static void tcs_notify_tx_done(unsigned long data);
static void tcs_notify_timeout(struct work_struct *work);

static int tcs_response_pool_init(struct tcs_drv *drv)
{
	struct tcs_response_pool *pool;
	int i;

	pool = devm_kzalloc(&drv->pdev->dev, sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return -ENOMEM;

	pool->resp = devm_kzalloc(&drv->pdev->dev, sizeof(*pool->resp) *
				MAX_POOL_SIZE, GFP_KERNEL);
	if (!pool->resp)
		return -ENOMEM;

	for (i = 0; i < MAX_POOL_SIZE; i++) {
		tasklet_init(&pool->resp[i].tasklet, tcs_notify_tx_done,
						(unsigned long) &pool->resp[i]);
		INIT_DELAYED_WORK(&pool->resp[i].dwork,
						tcs_notify_timeout);
	}

	spin_lock_init(&pool->lock);
	drv->resp_pool = pool;

	return 0;
}

static struct tcs_response *get_response_from_pool(struct tcs_drv *drv)
{
	struct tcs_response_pool *pool = drv->resp_pool;
	struct tcs_response *resp = ERR_PTR(-ENOMEM);
	unsigned long flags;
	int pos;

	spin_lock_irqsave(&pool->lock, flags);
	pos = find_first_zero_bit(pool->avail, MAX_POOL_SIZE);
	if (pos != MAX_POOL_SIZE) {
		bitmap_set(pool->avail, pos, 1);
		resp = &pool->resp[pos];
		memset(resp, 0, sizeof(*resp));
		tasklet_init(&resp->tasklet, tcs_notify_tx_done,
						(unsigned long) resp);
		INIT_DELAYED_WORK(&resp->dwork, tcs_notify_timeout);
		resp->drv = drv;
	}
	spin_unlock_irqrestore(&pool->lock, flags);

	return resp;
}

static void free_response_to_pool(struct tcs_response *resp)
{
	struct tcs_response_pool *pool = resp->drv->resp_pool;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&pool->lock, flags);
	i = resp - pool->resp;
	bitmap_clear(pool->avail, i, 1);
	spin_unlock_irqrestore(&pool->lock, flags);
}

static inline u32 read_drv_config(void __iomem *base)
{
	return le32_to_cpu(readl_relaxed(base + DRV_PRNT_CHLD_CONFIG));
}

static inline u32 read_tcs_reg(void __iomem *base, int reg, int m, int n)
{
	return le32_to_cpu(readl_relaxed(base + reg +
			TCS_DRV_TCS_OFFSET * m + TCS_DRV_CMD_OFFSET * n));
}

static inline void write_tcs_reg(void __iomem *base, int reg, int m, int n,
				u32 data)
{
	writel_relaxed(cpu_to_le32(data), base + reg +
			TCS_DRV_TCS_OFFSET * m + TCS_DRV_CMD_OFFSET * n);
}

static inline void write_tcs_reg_sync(void __iomem *base, int reg, int m, int n,
				u32 data)
{
	do {
		write_tcs_reg(base, reg, m, n, data);
		if (data == read_tcs_reg(base, reg, m, n))
			break;
		cpu_relax();
	} while (1);
}

static inline bool tcs_is_free(void __iomem *base, int m)
{
	return read_tcs_reg(base, TCS_DRV_STATUS, m, 0);
}

static inline struct tcs_mbox *get_tcs_from_index(struct tcs_drv *drv, int m)
{
	struct tcs_mbox *tcs;
	int i;

	for (i = 0; i < TCS_TYPE_NR; i++) {
		tcs = &drv->tcs[i];
		if (tcs->tcs_mask & BIT(m))
			break;
	}

	if (i == TCS_TYPE_NR)
		tcs = NULL;

	return tcs;
}

static inline struct tcs_mbox *get_tcs_of_type(struct tcs_drv *drv, int type)
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

static inline struct tcs_mbox *get_tcs_for_msg(struct tcs_drv *drv,
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
	}

	if (msg->is_read)
		type = ACTIVE_TCS;

	if (type < 0)
		return ERR_PTR(-EINVAL);

	return get_tcs_of_type(drv, type);
}

static inline struct tcs_response *get_tcs_response(struct tcs_drv *drv, int m)
{
	struct tcs_mbox *tcs = get_tcs_from_index(drv, m);

	return tcs ? tcs->resp[m - tcs->tcs_offset] : NULL;
}

static inline void send_tcs_response(struct tcs_response *resp)
{
	tasklet_schedule(&resp->tasklet);
}

static inline void schedule_tcs_err_response(struct tcs_response *resp)
{
	schedule_delayed_work(&resp->dwork, msecs_to_jiffies(TCS_MBOX_TOUT_MS));
}

/**
 * tcs_irq_handler: TX Done / Recv data handler
 */
static irqreturn_t tcs_irq_handler(int irq, void *p)
{
	struct tcs_drv *drv = p;
	void __iomem *base = drv->reg_base;
	int m, i;
	u32 irq_status, sts;
	struct tcs_response *resp;
	u32 irq_clear = 0;
	u32 data;

	/* Know which TCSes were triggered */
	irq_status = read_tcs_reg(base, TCS_DRV_IRQ_STATUS, 0, 0);

	for (m = 0; irq_status >= BIT(m); m++) {
		if (!(irq_status & BIT(m)))
			continue;

		/* Find the TCS that triggered */
		resp = get_tcs_response(drv, m);
		if (!resp) {
			pr_err("No resp request for TCS-%d\n", m);
			continue;
		}

		cancel_delayed_work(&resp->dwork);

		/* Clear the enable bit for the commands */
		write_tcs_reg(base, TCS_DRV_CMD_ENABLE, m, 0, 0);

		/* Check if all commands were completed */
		resp->err = 0;
		for (i = 0; i < resp->msg->num_payload; i++) {
			sts = read_tcs_reg(base, TCS_DRV_CMD_STATUS, m, i);
			if (!(sts & CMD_STATUS_ISSUED) ||
				(resp->msg->is_complete &&
					!(sts & CMD_STATUS_COMPL)))
				resp->err = -EIO;
		}

		/* Check for response if this was a read request */
		if (resp->msg->is_read) {
			/* Respond the data back in the same req data */
			data = read_tcs_reg(base, TCS_DRV_CMD_RESP_DATA, m, 0);
			resp->msg->payload[0].data = data;
			mbox_chan_received_data(resp->chan, resp->msg);
		}

		/* Notify the client that this request is completed. */
		send_tcs_response(resp);
		irq_clear |= BIT(m);
	}

	/* Clear the TCS IRQ status */
	write_tcs_reg(base, TCS_DRV_IRQ_CLEAR, 0, 0, irq_clear);

	return IRQ_HANDLED;
}

static inline void mbox_notify_tx_done(struct mbox_chan *chan,
				struct tcs_mbox_msg *msg, int m, int err)
{
	mbox_chan_txdone(chan, err);
}

/**
 * tcs_notify_tx_done: TX Done for requests that do not trigger TCS
 */
static void tcs_notify_tx_done(unsigned long data)
{
	struct tcs_response *resp = (struct tcs_response *) data;
	struct mbox_chan *chan = resp->chan;
	struct tcs_mbox_msg *msg = resp->msg;
	int err = resp->err;
	int m = resp->m;

	free_response_to_pool(resp);
	mbox_notify_tx_done(chan, msg, m, err);
}

/**
 * tcs_notify_timeout: TX Done for requests that do trigger TCS, but
 * we do not get a response IRQ back.
 */
static void tcs_notify_timeout(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct tcs_response *resp = container_of(dwork,
					struct tcs_response, dwork);
	struct mbox_chan *chan = resp->chan;
	struct tcs_mbox_msg *msg = resp->msg;
	struct tcs_drv *drv = resp->drv;
	int m = resp->m;
	int err = -EIO;

	/*
	 * In case the RPMH resource fails to respond to the completion
	 * request, the TCS would be blocked forever waiting on the response.
	 * There is no way to recover from this case.
	 */
	if (!tcs_is_free(drv->reg_base, m)) {
		bool pending = false;
		struct tcs_cmd *cmd;
		int i;
		u32 addr;

		for (i = 0; i < msg->num_payload; i++) {
			cmd = &msg->payload[i];
			addr = read_tcs_reg(drv->reg_base, TCS_DRV_CMD_ADDR,
						m, i);
			pending = (cmd->addr == addr);
		}
		if (pending) {
			pr_err("TCS-%d blocked waiting for RPMH to respond.\n",
				m);
			for (i = 0; i < msg->num_payload; i++)
				pr_err("Addr: 0x%x Data: 0x%x\n",
						msg->payload[i].addr,
						msg->payload[i].data);
			BUG();
		}
	}

	free_response_to_pool(resp);
	mbox_notify_tx_done(chan, msg, -1, err);
}

static void __tcs_buffer_write(void __iomem *base, int d, int m, int n,
			struct tcs_mbox_msg *msg, bool trigger)
{
	u32 cmd_msgid = 0;
	u32 cmd_enable = 0;
	u32 cmd_complete;
	u32 enable = TCS_AMC_MODE_ENABLE;
	struct tcs_cmd *cmd;
	int i;

	/* We have homologous command set i.e pure read or write, not a mix */
	cmd_msgid = CMD_MSGID_LEN;
	cmd_msgid |= (msg->is_complete) ? CMD_MSGID_RESP_REQ : 0;
	cmd_msgid |= (!msg->is_read) ? CMD_MSGID_WRITE : 0;

	/* Read the send-after-prev complete flag for those already in TCS */
	cmd_complete = read_tcs_reg(base, TCS_DRV_CMD_WAIT_FOR_CMPL, m, 0);

	for (i = 0; i < msg->num_payload; i++) {
		cmd = &msg->payload[i];
		cmd_enable |= BIT(n + i);
		cmd_complete |= cmd->complete << (n + i);
		write_tcs_reg(base, TCS_DRV_CMD_MSGID, m, n + i, cmd_msgid);
		write_tcs_reg(base, TCS_DRV_CMD_ADDR, m, n + i, cmd->addr);
		write_tcs_reg(base, TCS_DRV_CMD_DATA, m, n + i, cmd->data);
	}

	/* Write the send-after-prev completion bits for the batch */
	write_tcs_reg(base, TCS_DRV_CMD_WAIT_FOR_CMPL, m, 0, cmd_complete);

	/* Enable the new commands in TCS */
	cmd_enable |= read_tcs_reg(base, TCS_DRV_CMD_ENABLE, m, 0);
	write_tcs_reg(base, TCS_DRV_CMD_ENABLE, m, 0, cmd_enable);

	if (trigger) {
		/* Clear pending interrupt bits for this TCS, OK to not lock */
		write_tcs_reg(base, TCS_DRV_IRQ_CLEAR, 0, 0, BIT(m));
		/* HW req: Clear the DRV_CONTROL and enable TCS again */
		write_tcs_reg_sync(base, TCS_DRV_CONTROL, m, 0, 0);
		write_tcs_reg_sync(base, TCS_DRV_CONTROL, m, 0, enable);
		/* Enable the AMC mode on the TCS */
		enable |= TCS_AMC_MODE_TRIGGER;
		write_tcs_reg_sync(base, TCS_DRV_CONTROL, m, 0, enable);
	}
}

static void wait_for_req_inflight(struct tcs_drv *drv, struct tcs_mbox *tcs,
						struct tcs_mbox_msg *msg)
{
	u32 curr_enabled;
	int i, j, k;
	bool is_free;

	do  {
		is_free = true;
		for (i = 1; i > tcs->tcs_mask; i = i << 1) {
			if (!(tcs->tcs_mask & i))
				continue;
			if (tcs_is_free(drv->reg_base, i))
				continue;
			curr_enabled = read_tcs_reg(drv->reg_base,
						TCS_DRV_CMD_ENABLE, i, 0);
			for (j = 0; j < msg->num_payload; j++) {
				for (k = 0; k < curr_enabled; k++) {
					if (!(curr_enabled & BIT(k)))
						continue;
					if (tcs->cmd_addr[k] ==
						msg->payload[j].addr) {
						is_free = false;
						goto retry;
					}
				}
			}
		}
retry:
		if (!is_free)
			cpu_relax();
	} while (!is_free);
}

static int find_free_tcs(struct tcs_mbox *tcs)
{
	int slot, m = 0;

	/* Loop until we find a free AMC */
	do {
		if (tcs_is_free(tcs->drv->reg_base, tcs->tcs_offset + m)) {
			slot = m * tcs->ncpt;
			break;
		}
		if (++m > tcs->num_tcs)
			m = 0;
		cpu_relax();
	} while (1);

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
			WARN((tcs->cmd_addr[i + j] != cmd[j].addr),
				"Message does not match previous sequence.\n");
				return -EINVAL;
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
	if (tcs->type == ACTIVE_TCS)
		return find_free_tcs(tcs);

	/* Find if we already have the msg in our TCS */
	slot = find_match(tcs, msg->payload, msg->num_payload);
	if (slot >= 0)
		return slot;

	/* Do over, until we can fit the full payload in a TCS */
	do {
		slot = bitmap_find_next_zero_area(tcs->slots, MAX_TCS_SLOTS,
						n, msg->num_payload, 0);
		if (slot == MAX_TCS_SLOTS)
			break;
		n += tcs->ncpt;
	} while (slot + msg->num_payload - 1 >= n);

	return (slot != MAX_TCS_SLOTS) ? slot : -ENOMEM;
}

static struct tcs_response *setup_response(struct tcs_mbox *tcs,
		struct mbox_chan *chan, struct tcs_mbox_msg *msg, int m)
{
	struct tcs_response *resp = get_response_from_pool(tcs->drv);

	if (IS_ERR(resp))
		return resp;

	if (m < tcs->tcs_offset)
		return ERR_PTR(-EINVAL);

	tcs->resp[m - tcs->tcs_offset] = resp;
	resp->msg = msg;
	resp->chan = chan;
	resp->m = m;
	resp->err = 0;

	return resp;
}

static int tcs_mbox_write(struct mbox_chan *chan, struct tcs_mbox_msg *msg,
				bool trigger)
{
	const struct device *dev = chan->cl->dev;
	struct tcs_drv *drv = container_of(chan->mbox, struct tcs_drv, mbox);
	int d = drv->drv_id;
	struct tcs_mbox *tcs;
	int i, slot, offset, m, n;
	struct tcs_response *resp;

	tcs = get_tcs_for_msg(drv, msg);
	if (IS_ERR(tcs))
		return PTR_ERR(tcs);

	/* Identify the sequential slots that we can write to */
	spin_lock(&tcs->tcs_lock);
	slot = find_slots(tcs, msg);
	if (slot < 0) {
		dev_err(dev, "No TCS slot found.\n");
		spin_unlock(&tcs->tcs_lock);
		return slot;
	}
	/* Mark the slots as in-use, before we unlock */
	if (tcs->type == SLEEP_TCS || tcs->type == WAKE_TCS)
		bitmap_set(tcs->slots, slot, msg->num_payload);

	/* Copy the addresses of the resources over to the slots */
	for (i = 0; tcs->cmd_addr && i < msg->num_payload; i++)
		tcs->cmd_addr[slot + i] = msg->payload[i].addr;

	if (trigger)
		resp = setup_response(tcs, chan, msg,
				slot / tcs->ncpt + tcs->tcs_offset);

	spin_unlock(&tcs->tcs_lock);

	/*
	 * Find the TCS corresponding to the slot and start writing.
	 * Break down 'slot' into a 'n' position in the 'm'th TCS.
	 */
	offset = slot / tcs->ncpt;
	m = offset + tcs->tcs_offset;
	n = slot % tcs->ncpt;

	spin_lock(&tcs->tcs_m_lock[offset]);
	if (trigger) {
		/* Block, if we have an address from the msg in flight */
		wait_for_req_inflight(drv, tcs, msg);
		/* If the TCS is busy there is nothing to do but spin wait */
		while (!tcs_is_free(drv->reg_base, m))
			cpu_relax();
	}

	/* Write to the TCS or AMC */
	__tcs_buffer_write(drv->reg_base, d, m, n, msg, trigger);

	/* Schedule a timeout response, incase there is no actual response */
	if (trigger)
		schedule_tcs_err_response(resp);

	spin_unlock(&tcs->tcs_m_lock[offset]);

	return 0;
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
	struct tcs_mbox_msg *msg = data;
	const struct device *dev = chan->cl->dev;
	int ret = -EINVAL;

	if (!msg) {
		dev_err(dev, "Payload error.\n");
		goto tx_fail;
	}

	if (!msg->payload || msg->num_payload > MAX_RPMH_PAYLOAD) {
		dev_err(dev, "Payload error.\n");
		goto tx_fail;
	}

	if (msg->invalidate || msg->is_control) {
		dev_err(dev, "Incorrect API.\n");
		goto tx_fail;
	}

	if (msg->state != RPMH_ACTIVE_ONLY_STATE) {
		dev_err(dev, "Incorrect API.\n");
		goto tx_fail;
	}

	/* Read requests should always be single */
	if (msg->is_read && msg->num_payload > 1) {
		dev_err(dev, "Incorrect read request.\n");
		goto tx_fail;
	}

	/* Post the message to the TCS and trigger */
	ret = tcs_mbox_write(chan, msg, true);

tx_fail:
	if (ret) {
		struct tcs_drv *drv = container_of(chan->mbox,
							struct tcs_drv, mbox);
		struct tcs_response *resp = get_response_from_pool(drv);

		resp->chan = chan;
		resp->msg = msg;
		resp->err = ret;

		dev_err(dev, "Error sending RPMH message %d\n", ret);
		send_tcs_response(resp);
	}

	return 0;
}

static void __tcs_buffer_invalidate(void __iomem *base, int m)
{
	write_tcs_reg(base, TCS_DRV_CMD_ENABLE, m, 0, 0);
}

static int tcs_mbox_invalidate(struct mbox_chan *chan)
{
	struct tcs_drv *drv = container_of(chan->mbox, struct tcs_drv, mbox);
	struct tcs_mbox *tcs;
	int m, i;
	int inv_types[] = { WAKE_TCS, SLEEP_TCS };
	int type = 0;

	do {
		tcs = get_tcs_of_type(drv, inv_types[type]);
		if (IS_ERR(tcs))
			return PTR_ERR(tcs);

		spin_lock(&tcs->tcs_lock);
		for (i = 0; i < tcs->num_tcs; i++) {
			m = i + tcs->tcs_offset;
			spin_lock(&tcs->tcs_m_lock[i]);
			while (!tcs_is_free(drv->reg_base, m))
				cpu_relax();
			__tcs_buffer_invalidate(drv->reg_base, m);
			spin_unlock(&tcs->tcs_m_lock[i]);
		}
		/* Mark the TCS as free */
		bitmap_zero(tcs->slots, MAX_TCS_SLOTS);
		spin_unlock(&tcs->tcs_lock);
	} while (++type < ARRAY_SIZE(inv_types));

	return 0;
}

static void __tcs_write_hidden(void *base, int d, struct tcs_mbox_msg *msg)
{
	int i;
	void __iomem *addr;
	const u32 offset = TCS_HIDDEN_CMD0_DRV_DATA - TCS_HIDDEN_CMD0_DRV_ADDR;

	addr = base + TCS_HIDDEN_CMD0_DRV_ADDR;
	for (i = 0; i < msg->num_payload; i++) {
		/* Only data is write capable */
		writel_relaxed(cpu_to_le32(msg->payload[i].data),
							addr + offset);
		addr += TCS_HIDDEN_CMD_SHIFT;
	}
}

static int tcs_control_write(struct mbox_chan *chan, struct tcs_mbox_msg *msg)
{
	const struct device *dev = chan->cl->dev;
	struct tcs_drv *drv = container_of(chan->mbox, struct tcs_drv, mbox);
	struct tcs_mbox *tcs;

	tcs = get_tcs_of_type(drv, CONTROL_TCS);
	if (IS_ERR(tcs))
		return PTR_ERR(tcs);

	if (msg->num_payload != tcs->ncpt) {
		dev_err(dev, "Request must fit the control TCS size.\n");
		return -EINVAL;
	}

	spin_lock(&tcs->tcs_lock);
	__tcs_write_hidden(tcs->drv->base, drv->drv_id, msg);
	spin_unlock(&tcs->tcs_lock);

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
		dev_err(dev, "Payload error.\n");
		goto tx_done;
	}

	if (msg->num_payload > MAX_RPMH_PAYLOAD) {
		dev_err(dev, "Payload error.\n");
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

	if (msg->is_complete) {
		dev_err(dev, "Incorrect ctrl request.\n");
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
	.send_controller_data = chan_tcs_ctrl_write,
	.startup = chan_init,
	.shutdown = chan_shutdown,
};

static struct mbox_chan *of_tcs_mbox_xlate(struct mbox_controller *mbox,
				const struct of_phandle_args *sp)
{
	struct tcs_drv *drv = container_of(mbox, struct tcs_drv, mbox);
	struct mbox_chan *chan;

	if (drv->num_assigned >= mbox->num_chans) {
		pr_err("TCS-Mbox out of channel memory\n");
		return ERR_PTR(-ENOMEM);
	}

	chan = &mbox->chans[drv->num_assigned++];

	return chan;
}

static int tcs_drv_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *np;
	struct tcs_drv *drv;
	struct mbox_chan *chans;
	struct tcs_mbox *tcs;
	struct of_phandle_args p;
	int irq;
	u32 val[8] = { 0 };
	int num_chans = 0;
	int st = 0;
	int i, j, ret, nelem;
	u32 config, max_tcs, ncpt;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	of_property_read_u32(dn, "qcom,drv-id", &drv->drv_id);

	drv->base = of_iomap(dn, 0);
	if (IS_ERR(drv->base))
		return PTR_ERR(drv->base);

	drv->reg_base = of_iomap(dn, 1);
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

	for (i = 0; i < (nelem / 2); i++) {
		tcs = &drv->tcs[i];
		tcs->drv = drv;
		tcs->type = val[2 * i];
		tcs->num_tcs = val[2 * i + 1];
		tcs->ncpt = (tcs->type == CONTROL_TCS) ? TCS_HIDDEN_MAX_SLOTS
							: ncpt;
		spin_lock_init(&tcs->tcs_lock);

		if (tcs->num_tcs <= 0 || tcs->type == CONTROL_TCS)
			continue;

		if (tcs->num_tcs > MAX_TCS_PER_TYPE)
			return -EINVAL;

		if (st > max_tcs)
			return -EINVAL;

		tcs->tcs_mask = ((1 << tcs->num_tcs) - 1) << st;
		tcs->tcs_offset = st;
		st += tcs->num_tcs;

		tcs->cmd_addr = devm_kzalloc(&pdev->dev, sizeof(u32) *
					tcs->num_tcs * tcs->ncpt, GFP_KERNEL);
		if (!tcs->cmd_addr)
			return -ENOMEM;

		for (j = 0; j < tcs->num_tcs; j++)
			spin_lock_init(&tcs->tcs_m_lock[j]);
	}

	/* Allocate only that many channels specified in DT for our MBOX */
	for_each_node_with_property(np, "mboxes") {
		if (!of_device_is_available(np))
			continue;
		i = of_count_phandle_with_args(np, "mboxes", "#mbox-cells");
		for (j = 0; j < i; j++) {
			ret = of_parse_phandle_with_args(np, "mboxes",
							"#mbox-cells", j, &p);
			if (!ret && p.np == pdev->dev.of_node)
				break;
		}
		num_chans++;
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
	drv->num_tcs = st;
	drv->pdev = pdev;

	ret = tcs_response_pool_init(drv);
	if (ret)
		return ret;

	irq = of_irq_get(dn, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
			tcs_irq_handler,
			IRQF_ONESHOT | IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
			"tcs_irq", drv);
	if (ret)
		return ret;

	/* Enable interrupts for AMC TCS */
	write_tcs_reg(drv->reg_base, TCS_DRV_IRQ_ENABLE, 0, 0,
					drv->tcs[ACTIVE_TCS].tcs_mask);

	ret = mbox_controller_register(&drv->mbox);
	if (ret)
		return ret;

	pr_debug("Mailbox controller (%s, drv=%d) registered\n",
					dn->full_name, drv->drv_id);

	return 0;
}

static const struct of_device_id tcs_drv_match[] = {
	{ .compatible = "qcom,tcs-drv", },
	{ }
};

static struct platform_driver tcs_mbox_driver = {
	.probe = tcs_drv_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = tcs_drv_match,
	},
};

static int __init tcs_mbox_driver_init(void)
{
	return platform_driver_register(&tcs_mbox_driver);
}
arch_initcall(tcs_mbox_driver_init);
