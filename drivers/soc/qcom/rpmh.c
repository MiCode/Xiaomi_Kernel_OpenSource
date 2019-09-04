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

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <soc/qcom/rpmh.h>
#include <soc/qcom/tcs.h>
#include <soc/qcom/cmd-db.h>

#define RPMH_MAX_MBOXES			2
#define RPMH_MAX_FAST_RES		32
#define RPMH_MAX_REQ_IN_BATCH		10
#define RPMH_TIMEOUT			msecs_to_jiffies(10000)

#define DEFINE_RPMH_MSG_ONSTACK(rc, s, q, c, name)	\
	struct rpmh_msg name = {			\
		.msg = {				\
			.state = s,			\
			.payload = name.cmd,		\
			.num_payload = 0,		\
			.is_read = false,		\
			.is_control = false,		\
			.is_complete = true,		\
			.invalidate = false,		\
		},					\
		.cmd = { { 0 } },			\
		.completion = q,			\
		.wait_count = c,			\
		.rc = rc,				\
		.bit = -1,				\
	}

struct rpmh_req {
	u32 addr;
	u32 sleep_val;
	u32 wake_val;
	struct list_head list;
};

struct rpmh_msg {
	struct tcs_mbox_msg msg;
	struct tcs_cmd cmd[MAX_RPMH_PAYLOAD];
	struct completion *completion;
	atomic_t *wait_count;
	struct rpmh_client *rc;
	int bit;
	int err; /* relay error from mbox for sync calls */
};

struct rpmh_mbox {
	struct device_node *mbox_dn;
	struct list_head resources;
	spinlock_t lock;
	struct rpmh_msg *msg_pool;
	DECLARE_BITMAP(fast_req, RPMH_MAX_FAST_RES);
	bool dirty;
	bool in_solver_mode;
	/* Cache sleep and wake requests sent as passthru */
	struct rpmh_msg *passthru_cache[2 * RPMH_MAX_REQ_IN_BATCH];
};

struct rpmh_client {
	struct device *dev;
	struct mbox_client client;
	struct mbox_chan *chan;
	struct rpmh_mbox *rpmh;
};

static struct rpmh_mbox mbox_ctrlr[RPMH_MAX_MBOXES];
DEFINE_MUTEX(rpmh_mbox_mutex);
bool rpmh_standalone;

static struct rpmh_msg *get_msg_from_pool(struct rpmh_client *rc)
{
	struct rpmh_mbox *rpm = rc->rpmh;
	struct rpmh_msg *msg = NULL;
	int pos;
	unsigned long flags;

	spin_lock_irqsave(&rpm->lock, flags);
	pos = find_first_zero_bit(rpm->fast_req, RPMH_MAX_FAST_RES);
	if (pos != RPMH_MAX_FAST_RES) {
		bitmap_set(rpm->fast_req, pos, 1);
		msg = &rpm->msg_pool[pos];
		memset(msg, 0, sizeof(*msg));
		msg->bit = pos;
		msg->rc = rc;
	}
	spin_unlock_irqrestore(&rpm->lock, flags);

	return msg;
}

static void __free_msg_to_pool(struct rpmh_msg *rpm_msg)
{
	struct rpmh_mbox *rpm = rpm_msg->rc->rpmh;

	/* If we allocated the pool, set it as available */
	if (rpm_msg->bit >= 0 && rpm_msg->bit != RPMH_MAX_FAST_RES)
		bitmap_clear(rpm->fast_req, rpm_msg->bit, 1);
}

static void free_msg_to_pool(struct rpmh_msg *rpm_msg)
{
	struct rpmh_mbox *rpm = rpm_msg->rc->rpmh;
	unsigned long flags;

	spin_lock_irqsave(&rpm->lock, flags);
	__free_msg_to_pool(rpm_msg);
	spin_unlock_irqrestore(&rpm->lock, flags);
}

static void rpmh_rx_cb(struct mbox_client *cl, void *msg)
{
	struct rpmh_msg *rpm_msg = container_of(msg, struct rpmh_msg, msg);

	atomic_dec(rpm_msg->wait_count);
}

static void rpmh_tx_done(struct mbox_client *cl, void *msg, int r)
{
	struct rpmh_msg *rpm_msg = container_of(msg, struct rpmh_msg, msg);
	atomic_t *wc = rpm_msg->wait_count;
	struct completion *compl = rpm_msg->completion;

	rpm_msg->err = r;

	if (r) {
		dev_err(rpm_msg->rc->dev,
			"RPMH TX fail in msg addr 0x%x, err=%d\n",
			rpm_msg->msg.payload[0].addr, r);
		/*
		 * If we fail TX for a read, call then we won't get
		 * a rx_callback. Force a rx_cb.
		 */
		if (rpm_msg->msg.is_read)
			rpmh_rx_cb(cl, msg);
	}

	/*
	 * Copy the child object pointers before freeing up the parent,
	 * This way even if the parent (rpm_msg) object gets reused, we
	 * can free up the child objects (wq/wc) parallely.
	 * If you free up the children before the parent, then we run
	 * into an issue that the stack allocated parent object may be
	 * invalid before we can check the ->bit value.
	 */
	free_msg_to_pool(rpm_msg);

	/* Signal the blocking thread we are done */
	if (wc && atomic_dec_and_test(wc))
		if (compl)
			complete(compl);
}

/**
 * wait_for_tx_done: Wait forever until the response is received.
 *
 * @rc: The RPMH client
 * @compl: The completion object
 * @addr: An addr that we sent in that request
 * @data: The data for the address in that request
 *
 */
static inline void wait_for_tx_done(struct rpmh_client *rc,
		struct completion *compl, u32 addr, u32 data)
{
	int ret;
	int count = 4;
	int skip = 0;

	do {
		ret = wait_for_completion_timeout(compl, RPMH_TIMEOUT);
		if (ret) {
			if (count != 4)
				dev_notice(rc->dev,
				"RPMH response received addr=0x%x data=0x%x\n",
				addr, data);
			return;
		}
		if (!count) {
			if (skip++ % 100)
				continue;
			dev_err(rc->dev,
				"RPMH waiting for interrupt from AOSS\n");
			mbox_chan_debug(rc->chan);
			BUG();
		} else {
			dev_err(rc->dev,
			"RPMH response timeout (%d) addr=0x%x,data=0x%x\n",
			count, addr, data);
			count--;
		}
	} while (true);
}

static struct rpmh_req *__find_req(struct rpmh_client *rc, u32 addr)
{
	struct rpmh_req *p, *req = NULL;

	list_for_each_entry(p, &rc->rpmh->resources, list) {
		if (p->addr == addr) {
			req = p;
			break;
		}
	}

	return req;
}

static struct rpmh_req *cache_rpm_request(struct rpmh_client *rc,
			enum rpmh_state state, struct tcs_cmd *cmd)
{
	struct rpmh_req *req;
	struct rpmh_mbox *rpm = rc->rpmh;
	unsigned long flags;

	spin_lock_irqsave(&rpm->lock, flags);
	req = __find_req(rc, cmd->addr);
	if (req)
		goto existing;

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req) {
		req = ERR_PTR(-ENOMEM);
		goto unlock;
	}

	req->addr = cmd->addr;
	req->sleep_val = req->wake_val = UINT_MAX;
	INIT_LIST_HEAD(&req->list);
	list_add_tail(&req->list, &rpm->resources);

existing:
	switch (state) {
	case RPMH_ACTIVE_ONLY_STATE:
	case RPMH_AWAKE_STATE:
		if (req->sleep_val != UINT_MAX) {
			req->wake_val = cmd->data;
			rpm->dirty = true;
		}
		break;
	case RPMH_WAKE_ONLY_STATE:
		if (req->wake_val != cmd->data) {
			req->wake_val = cmd->data;
			rpm->dirty = true;
		}
		break;
	case RPMH_SLEEP_STATE:
		if (req->sleep_val != cmd->data) {
			req->sleep_val = cmd->data;
			rpm->dirty = true;
		}
		break;
	default:
		break;
	};

unlock:
	spin_unlock_irqrestore(&rpm->lock, flags);

	return req;
}

static int check_ctrlr_state(struct rpmh_client *rc, enum rpmh_state state)
{
	struct rpmh_mbox *rpm = rc->rpmh;
	unsigned long flags;
	int ret = 0;

	/* Do not allow setting active votes when in solver mode */
	spin_lock_irqsave(&rpm->lock, flags);
	if (rpm->in_solver_mode &&
	    (state == RPMH_AWAKE_STATE || state == RPMH_ACTIVE_ONLY_STATE))
		ret = -EBUSY;
	spin_unlock_irqrestore(&rpm->lock, flags);

	return ret;
}

/**
 * __rpmh_write: Cache and send the RPMH request
 *
 * @rc: The RPMH client
 * @state: Active/Sleep request type
 * @rpm_msg: The data that needs to be sent (payload).
 *
 * Cache the RPMH request and send if the state is ACTIVE_ONLY.
 * SLEEP/WAKE_ONLY requests are not sent to the controller at
 * this time. Use rpmh_flush() to send them to the controller.
 */
int __rpmh_write(struct rpmh_client *rc, enum rpmh_state state,
			struct rpmh_msg *rpm_msg)
{
	struct rpmh_req *req;
	int ret = 0;
	int i;

	/* Cache the request in our store and link the payload */
	for (i = 0; i < rpm_msg->msg.num_payload; i++) {
		req = cache_rpm_request(rc, state, &rpm_msg->msg.payload[i]);
		if (IS_ERR(req))
			return PTR_ERR(req);
	}

	rpm_msg->msg.state = state;

	/* Send to mailbox only if active or awake */
	if (state == RPMH_ACTIVE_ONLY_STATE || state == RPMH_AWAKE_STATE) {
		ret = mbox_send_message(rc->chan, &rpm_msg->msg);
		if (ret > 0)
			ret = 0;
	} else {
		/* Clean up our call by spoofing tx_done */
		rpmh_tx_done(&rc->client, &rpm_msg->msg, ret);
	}

	return ret;
}

/**
 * rpmh_write_single_async: Write a single RPMH command
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 * @state: Active/sleep set
 * @addr: The ePCB address
 * @data: The data
 *
 * Write a single value in fast-path. Fire and forget.
 * May be called from atomic contexts.
 */
int rpmh_write_single_async(struct rpmh_client *rc, enum rpmh_state state,
			u32 addr, u32 data)
{
	struct rpmh_msg *rpm_msg;
	int ret;

	if (IS_ERR_OR_NULL(rc))
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(rc, state);
	if (ret)
		return ret;

	rpm_msg = get_msg_from_pool(rc);
	if (!rpm_msg)
		return -ENOMEM;

	rpm_msg->cmd[0].addr = addr;
	rpm_msg->cmd[0].data = data;

	rpm_msg->msg.payload = rpm_msg->cmd;
	rpm_msg->msg.num_payload = 1;

	return __rpmh_write(rc, state, rpm_msg);
}
EXPORT_SYMBOL(rpmh_write_single_async);

/**
 * rpmh_write_single: Write a single RPMH command and
 * wait for completion of the command.
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 * @state: Active/sleep set
 * @addr: The ePCB address
 * @offset: Offset of the resource
 * @data: The data
 *
 * Write a single value in slow-path and wait for the request to be
 * complete. Blocks until the request is completed on the accelerator.
 * Do not call from atomic contexts.
 */
int rpmh_write_single(struct rpmh_client *rc, enum rpmh_state state,
			u32 addr, u32 data)
{
	DECLARE_COMPLETION_ONSTACK(compl);
	atomic_t wait_count = ATOMIC_INIT(1);
	DEFINE_RPMH_MSG_ONSTACK(rc, state, &compl, &wait_count, rpm_msg);
	int ret;

	if (IS_ERR_OR_NULL(rc))
		return -EINVAL;

	might_sleep();

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(rc, state);
	if (ret)
		return ret;

	rpm_msg.cmd[0].addr = addr;
	rpm_msg.cmd[0].data = data;
	rpm_msg.msg.num_payload = 1;

	ret = __rpmh_write(rc, state, &rpm_msg);
	if (ret < 0)
		return ret;

	wait_for_tx_done(rc, &compl, addr, data);

	return rpm_msg.err;
}
EXPORT_SYMBOL(rpmh_write_single);

struct rpmh_msg *__get_rpmh_msg_async(struct rpmh_client *rc,
		enum rpmh_state state, struct tcs_cmd *cmd, int n)
{
	struct rpmh_msg *rpm_msg;

	if (IS_ERR_OR_NULL(rc) || !cmd || n <= 0 || n > MAX_RPMH_PAYLOAD)
		return ERR_PTR(-EINVAL);

	rpm_msg = get_msg_from_pool(rc);
	if (!rpm_msg)
		return ERR_PTR(-ENOMEM);

	memcpy(rpm_msg->cmd, cmd, n * sizeof(*cmd));

	rpm_msg->msg.state = state;
	rpm_msg->msg.payload = rpm_msg->cmd;
	rpm_msg->msg.num_payload = n;

	return rpm_msg;
}

/**
 * rpmh_write_async: Write a batch of RPMH commands
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 * @state: Active/sleep set
 * @cmd: The payload data
 * @n: The number of elements in payload
 *
 * Write a batch of RPMH commands, the order of commands is maintained
 * and will be sent as a single shot. By default the entire set of commands
 * are considered active only (i.e, will not be cached in wake set, unless
 * all of them have their corresponding sleep requests).
 */
int rpmh_write_async(struct rpmh_client *rc, enum rpmh_state state,
			struct tcs_cmd *cmd, int n)
{
	struct rpmh_msg *rpm_msg;
	int ret;

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(rc, state);
	if (ret)
		return ret;

	rpm_msg = __get_rpmh_msg_async(rc, state, cmd, n);
	if (IS_ERR(rpm_msg))
		return PTR_ERR(rpm_msg);

	return __rpmh_write(rc, state, rpm_msg);
}
EXPORT_SYMBOL(rpmh_write_async);

/**
 * rpmh_write: Write a batch of RPMH commands
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 * @state: Active/sleep set
 * @cmd: The payload data
 * @n: The number of elements in payload
 *
 * Write a batch of RPMH commands, the order of commands is maintained
 * and will be sent as a single shot. By default the entire set of commands
 * are considered active only (i.e, will not be cached in wake set, unless
 * all of them have their corresponding sleep requests). All requests are
 * sent as slow path requests.
 *
 * May sleep. Do not call from atomic contexts.
 */
int rpmh_write(struct rpmh_client *rc, enum rpmh_state state,
			struct tcs_cmd *cmd, int n)
{
	DECLARE_COMPLETION_ONSTACK(compl);
	atomic_t wait_count = ATOMIC_INIT(1);
	DEFINE_RPMH_MSG_ONSTACK(rc, state, &compl, &wait_count, rpm_msg);
	int ret;

	if (IS_ERR_OR_NULL(rc) || !cmd || n <= 0 || n > MAX_RPMH_PAYLOAD)
		return -EINVAL;

	might_sleep();

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(rc, state);
	if (ret)
		return ret;

	memcpy(rpm_msg.cmd, cmd, n * sizeof(*cmd));
	rpm_msg.msg.num_payload = n;

	ret = __rpmh_write(rc, state, &rpm_msg);
	if (ret)
		return ret;

	wait_for_tx_done(rc, &compl, cmd[0].addr, cmd[0].data);

	return rpm_msg.err;
}
EXPORT_SYMBOL(rpmh_write);

static int cache_passthru(struct rpmh_client *rc, struct rpmh_msg **rpm_msg,
					int count)
{
	struct rpmh_mbox *rpm = rc->rpmh;
	unsigned long flags;
	int ret = 0;
	int index = 0;
	int i;

	spin_lock_irqsave(&rpm->lock, flags);
	while (rpm->passthru_cache[index])
		index++;
	if (index + count >=  2 * RPMH_MAX_REQ_IN_BATCH) {
		ret = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < count; i++)
		rpm->passthru_cache[index + i] = rpm_msg[i];
fail:
	spin_unlock_irqrestore(&rpm->lock, flags);

	return ret;
}

static int flush_passthru(struct rpmh_client *rc)
{
	struct rpmh_mbox *rpm = rc->rpmh;
	struct rpmh_msg *rpm_msg;
	unsigned long flags;
	int ret = 0;
	int i;

	/* Send Sleep/Wake requests to the controller, expect no response */
	spin_lock_irqsave(&rpm->lock, flags);
	for (i = 0; rpm->passthru_cache[i]; i++) {
		rpm_msg = rpm->passthru_cache[i];
		ret = mbox_send_controller_data(rc->chan, &rpm_msg->msg);
		if (ret)
			goto fail;
	}
fail:
	spin_unlock_irqrestore(&rpm->lock, flags);

	return ret;
}

static void invalidate_passthru(struct rpmh_client *rc)
{
	struct rpmh_mbox *rpm = rc->rpmh;
	unsigned long flags;
	int index = 0;
	int i;

	spin_lock_irqsave(&rpm->lock, flags);
	while (rpm->passthru_cache[index])
		index++;
	for (i = 0; i < index; i++) {
		__free_msg_to_pool(rpm->passthru_cache[i]);
		rpm->passthru_cache[i] = NULL;
	}
	spin_unlock_irqrestore(&rpm->lock, flags);
}

/**
 * rpmh_write_batch: Write multiple sets of RPMH commands and wait for the
 * batch to finish.
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 * @state: Active/sleep set
 * @cmd: The payload data
 * @n: The array of count of elements in each batch, 0 terminated.
 *
 * Write a request to the mailbox controller without caching. If the request
 * state is ACTIVE or AWAKE, then the requests are treated as completion request
 * and sent to the controller immediately. The function waits until all the
 * commands are complete. If the request was to SLEEP or WAKE_ONLY, then the
 * request is sent as fire-n-forget and no ack is expected.
 *
 * May sleep. Do not call from atomic contexts for ACTIVE_ONLY requests.
 */
int rpmh_write_batch(struct rpmh_client *rc, enum rpmh_state state,
			struct tcs_cmd *cmd, int *n)
{
	struct rpmh_msg *rpm_msg[RPMH_MAX_REQ_IN_BATCH] = { NULL };
	DECLARE_COMPLETION_ONSTACK(compl);
	atomic_t wait_count = ATOMIC_INIT(0); /* overwritten */
	int count = 0;
	int ret, i, j, k;
	bool complete_set;
	u32 addr, data;

	if (IS_ERR_OR_NULL(rc) || !cmd || !n)
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	ret = check_ctrlr_state(rc, state);
	if (ret)
		return ret;

	while (n[count++] > 0)
		;
	count--;
	if (!count || count > RPMH_MAX_REQ_IN_BATCH)
		return -EINVAL;

	if (state == RPMH_ACTIVE_ONLY_STATE || state == RPMH_AWAKE_STATE) {
		/*
		 * Ensure the 'complete' bit is set for atleast one command in
		 * each set for active/awake requests.
		 */
		for (i = 0, k = 0; i < count; i++, k += n[i]) {
			complete_set = false;
			for (j = 0; j < n[i]; j++) {
				if (cmd[k + j].complete) {
					complete_set = true;
					break;
				}
			}
			if (!complete_set) {
				dev_err(rc->dev, "No completion set for batch");
				return -EINVAL;
			}
		}
	}

	addr = cmd[0].addr;
	data = cmd[0].data;
	/* Create async request batches */
	for (i = 0; i < count; i++) {
		rpm_msg[i] = __get_rpmh_msg_async(rc, state, cmd, n[i]);
		if (IS_ERR_OR_NULL(rpm_msg[i])) {
			for (j = 0 ; j < i; j++)
				free_msg_to_pool(rpm_msg[j]);
			return PTR_ERR(rpm_msg[i]);
		}
		cmd += n[i];
	}

	/* Send if Active or Awake and wait for the whole set to complete */
	if (state == RPMH_ACTIVE_ONLY_STATE || state == RPMH_AWAKE_STATE) {
		might_sleep();
		atomic_set(&wait_count, count);
		for (i = 0; i < count; i++) {
			rpm_msg[i]->completion = &compl;
			rpm_msg[i]->wait_count = &wait_count;
			/* Bypass caching and write to mailbox directly */
			ret = mbox_send_message(rc->chan, &rpm_msg[i]->msg);
			if (ret < 0) {
				pr_err("Error(%d) sending RPM message addr=0x%x\n",
					ret, rpm_msg[i]->msg.payload[0].addr);
				break;
			}
		}
		/* For those unsent requests, spoof tx_done */
		for (j = i; j < count; j++)
			rpmh_tx_done(&rc->client, &rpm_msg[j]->msg, ret);
		wait_for_tx_done(rc, &compl, addr, data);
	} else {
		/*
		 * Cache sleep/wake data in store.
		 * But flush passthru first before flushing all other data.
		 */
		return cache_passthru(rc, rpm_msg, count);
	}

	return 0;
}
EXPORT_SYMBOL(rpmh_write_batch);

/**
 * rpmh_mode_solver_set: Indicate that the RSC controller hardware has
 * been configured to be in solver mode
 *
 * @rc: The RPMH handle
 * @enable: Boolean value indicating if the controller is in solver mode.
 *
 * When solver mode is enabled, passthru API will not be able to send wake
 * votes, just awake and active votes.
 */
int rpmh_mode_solver_set(struct rpmh_client *rc, bool enable)
{
	struct rpmh_mbox *rpm;
	unsigned long flags;

	if (IS_ERR_OR_NULL(rc))
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	rpm = rc->rpmh;
	do {
		spin_lock_irqsave(&rpm->lock, flags);
		if (mbox_controller_is_idle(rc->chan)) {
			rpm->in_solver_mode = enable;
			spin_unlock_irqrestore(&rpm->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&rpm->lock, flags);
		udelay(10);
	} while (1);

	return 0;
}
EXPORT_SYMBOL(rpmh_mode_solver_set);

/**
 * rpmh_write_control: Write async control commands to the controller
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 * @cmd: The payload data
 * @n: The number of elements in payload
 *
 * Write control commands to the controller. The messages are always sent
 * async.
 *
 * May be called from atomic contexts.
 */
int rpmh_write_control(struct rpmh_client *rc, struct tcs_cmd *cmd, int n)
{
	DEFINE_RPMH_MSG_ONSTACK(rc, 0, NULL, NULL, rpm_msg);

	if (IS_ERR_OR_NULL(rc) || n <= 0 || n > MAX_RPMH_PAYLOAD)
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	memcpy(rpm_msg.cmd, cmd, n * sizeof(*cmd));
	rpm_msg.msg.num_payload = n;
	rpm_msg.msg.is_control = true;
	rpm_msg.msg.is_complete = false;

	return mbox_send_controller_data(rc->chan, &rpm_msg.msg);
}
EXPORT_SYMBOL(rpmh_write_control);

/**
 * rpmh_invalidate: Invalidate all sleep and active sets
 * sets.
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 *
 * Invalidate the sleep and active values in the TCS blocks.
 * Nothing to do here.
 */
int rpmh_invalidate(struct rpmh_client *rc)
{
	DEFINE_RPMH_MSG_ONSTACK(rc, 0, NULL, NULL, rpm_msg);
	struct rpmh_mbox *rpm;
	unsigned long flags;

	if (IS_ERR_OR_NULL(rc))
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	invalidate_passthru(rc);

	rpm = rc->rpmh;
	rpm_msg.msg.invalidate = true;
	rpm_msg.msg.is_complete = false;

	spin_lock_irqsave(&rpm->lock, flags);
	rpm->dirty = true;
	spin_unlock_irqrestore(&rpm->lock, flags);

	return mbox_send_controller_data(rc->chan, &rpm_msg.msg);
}
EXPORT_SYMBOL(rpmh_invalidate);

/**
 * rpmh_read: Read a resource value
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 * @addr: The ePCB address
 * @resp: The store for the response received from RPMH
 *
 * Read a resource value from RPMH.
 */
int rpmh_read(struct rpmh_client *rc, u32 addr, u32 *resp)
{
	int ret;
	DECLARE_COMPLETION_ONSTACK(compl);
	atomic_t wait_count = ATOMIC_INIT(2); /* wait for rx_cb and tx_done */
	DEFINE_RPMH_MSG_ONSTACK(rc, RPMH_ACTIVE_ONLY_STATE,
				&compl, &wait_count, rpm_msg);

	if (IS_ERR_OR_NULL(rc) || !resp)
		return -EINVAL;

	might_sleep();

	if (rpmh_standalone)
		return 0;

	rpm_msg.cmd[0].addr = addr;
	rpm_msg.cmd[0].data = 0;
	rpm_msg.msg.num_payload = 1;

	rpm_msg.msg.is_read = true;

	ret = mbox_send_message(rc->chan, &rpm_msg.msg);
	if (ret < 0)
		return ret;

	/* Wait until the response is received from RPMH */
	wait_for_tx_done(rc, &compl, addr, 0);

	/* Read the data back from the tcs_mbox_msg structrure */
	*resp = rpm_msg.cmd[0].data;

	return rpm_msg.err;
}
EXPORT_SYMBOL(rpmh_read);

/**
 * rpmh_ctrlr_idle: Check if the controller is idle
 *
 * @rc: The RPMH handle got from rpmh_get_dev_channel
 *
 * Returns if the controller is idle or not.
 */
int rpmh_ctrlr_idle(struct rpmh_client *rc)
{
	if (IS_ERR_OR_NULL(rc))
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	if (!mbox_controller_is_idle(rc->chan))
		return -EBUSY;

	return 0;
}
EXPORT_SYMBOL(rpmh_ctrlr_idle);

static inline int is_req_valid(struct rpmh_req *req)
{
	return (req->sleep_val != UINT_MAX && req->wake_val != UINT_MAX
			&& req->sleep_val != req->wake_val);
}

int send_single(struct rpmh_client *rc, enum rpmh_state state, u32 addr,
				u32 data)
{
	DEFINE_RPMH_MSG_ONSTACK(rc, state, NULL, NULL, rpm_msg);

	/* Wake sets are always complete and sleep sets are not */
	rpm_msg.msg.is_complete = (state == RPMH_WAKE_ONLY_STATE);
	rpm_msg.cmd[0].addr = addr;
	rpm_msg.cmd[0].data = data;
	rpm_msg.msg.num_payload = 1;

	return mbox_send_controller_data(rc->chan, &rpm_msg.msg);
}

/**
 * rpmh_flush: Flushes the buffered active and sleep sets to TCS
 *
 * @rc: The RPMh handle got from rpmh_get_dev_channel
 *
 * This function is generally called from the sleep code from the last CPU
 * that is powering down the entire system.
 *
 * Returns -EBUSY if the controller is busy, probably waiting on a response
 * to a RPMH request sent earlier.
 */
int rpmh_flush(struct rpmh_client *rc)
{
	DEFINE_RPMH_MSG_ONSTACK(rc, 0, NULL, NULL, rpm_msg);
	struct rpmh_req *p;
	struct rpmh_mbox *rpm = rc->rpmh;
	int ret;
	unsigned long flags;

	if (IS_ERR_OR_NULL(rc))
		return -EINVAL;

	if (rpmh_standalone)
		return 0;

	if (!mbox_controller_is_idle(rc->chan))
		return -EBUSY;

	spin_lock_irqsave(&rpm->lock, flags);
	if (!rpm->dirty) {
		pr_debug("Skipping flush, TCS has latest data.\n");
		spin_unlock_irqrestore(&rpm->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&rpm->lock, flags);

	/* Invalidate sleep and wake TCS */
	rpm_msg.msg.invalidate = true;
	rpm_msg.msg.is_complete = false;
	ret = mbox_send_controller_data(rc->chan, &rpm_msg.msg);
	if (ret)
		return ret;

	/* First flush the cached passthru's */
	ret = flush_passthru(rc);
	if (ret)
		return ret;

	/*
	 * Nobody else should be calling this function other than sleep,
	 * hence we can run without locks.
	 */
	list_for_each_entry(p, &rc->rpmh->resources, list) {
		if (!is_req_valid(p)) {
			pr_debug("%s: skipping RPMH req: a:0x%x s:0x%x w:0x%x",
				__func__, p->addr, p->sleep_val, p->wake_val);
			continue;
		}
		ret = send_single(rc, RPMH_SLEEP_STATE, p->addr, p->sleep_val);
		if (ret)
			return ret;
		ret = send_single(rc, RPMH_WAKE_ONLY_STATE, p->addr,
						p->wake_val);
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&rpm->lock, flags);
	rpm->dirty = false;
	spin_unlock_irqrestore(&rpm->lock, flags);

	return 0;
}
EXPORT_SYMBOL(rpmh_flush);

/**
 * get_mbox: Get the MBOX controller
 * @pdev: the platform device
 * @name: the MBOX name as specified in DT for the device.
 * @index: the index in the mboxes property if name is not provided.
 *
 * Get the MBOX Device node. We will use that to know which
 * MBOX controller this platform device is intending to talk
 * to.
 */
static struct rpmh_mbox *get_mbox(struct platform_device *pdev,
			const char *name, int index)
{
	int i;
	struct property *prop;
	struct of_phandle_args spec;
	const char *mbox_name;
	struct rpmh_mbox *rpmh;

	if (index < 0) {
		if (!name || !name[0])
			return ERR_PTR(-EINVAL);
		index = 0;
		of_property_for_each_string(pdev->dev.of_node,
				"mbox-names", prop, mbox_name) {
			if (!strcmp(name, mbox_name))
				break;
			index++;
		}
	}

	if (of_parse_phandle_with_args(pdev->dev.of_node, "mboxes",
					"#mbox-cells", index, &spec)) {
		dev_dbg(&pdev->dev, "%s: can't parse mboxes property\n",
					__func__);
		return ERR_PTR(-ENODEV);
	}

	for (i = 0; i < RPMH_MAX_MBOXES; i++)
		if (mbox_ctrlr[i].mbox_dn == spec.np) {
			rpmh = &mbox_ctrlr[i];
			goto found;
		}

	/* A new MBOX */
	for (i = 0; i < RPMH_MAX_MBOXES; i++)
		if (!mbox_ctrlr[i].mbox_dn)
			break;

	/* More controllers than expected - not recoverable */
	WARN_ON(i == RPMH_MAX_MBOXES);

	rpmh = &mbox_ctrlr[i];

	rpmh->msg_pool = kzalloc(sizeof(struct rpmh_msg) *
				RPMH_MAX_FAST_RES, GFP_KERNEL);
	if (!rpmh->msg_pool) {
		of_node_put(spec.np);
		return ERR_PTR(-ENOMEM);
	}

	rpmh->mbox_dn = spec.np;
	INIT_LIST_HEAD(&rpmh->resources);
	spin_lock_init(&rpmh->lock);

found:
	of_node_put(spec.np);

	return rpmh;
}

static struct rpmh_client *get_rpmh_client(struct platform_device *pdev,
				const char *name, int index)
{
	struct rpmh_client *rc;
	int ret = 0;

	ret = cmd_db_ready();
	if (ret)
		return ERR_PTR(ret);

	rc = kzalloc(sizeof(*rc), GFP_KERNEL);
	if (!rc)
		return ERR_PTR(-ENOMEM);

	rc->client.rx_callback = rpmh_rx_cb;
	rc->client.tx_prepare = NULL;
	rc->client.tx_done = rpmh_tx_done;
	rc->client.tx_block = false;
	rc->client.knows_txdone = false;
	rc->client.dev = &pdev->dev;
	rc->dev = &pdev->dev;

	rc->chan = ERR_PTR(-EINVAL);

	/* Initialize by index or name, whichever is present */
	if (index >= 0)
		rc->chan = mbox_request_channel(&rc->client, index);
	else if (name)
		rc->chan = mbox_request_channel_byname(&rc->client, name);

	if (IS_ERR_OR_NULL(rc->chan)) {
		ret = PTR_ERR(rc->chan);
		goto cleanup;
	}

	mutex_lock(&rpmh_mbox_mutex);
	rc->rpmh = get_mbox(pdev, name, index);
	rpmh_standalone = (cmd_db_is_standalone() > 0);
	mutex_unlock(&rpmh_mbox_mutex);

	if (IS_ERR(rc->rpmh)) {
		ret = PTR_ERR(rc->rpmh);
		mbox_free_channel(rc->chan);
		goto cleanup;
	}

	return rc;

cleanup:
	kfree(rc);
	return ERR_PTR(ret);
}

/**
 * rpmh_get_byname: Get the RPMh handle by mbox name
 *
 * @pdev: the platform device which needs to communicate with RPM
 * accelerators
 * @name: The mbox-name assigned to the client's mailbox handle
 *
 * May sleep.
 */
struct rpmh_client *rpmh_get_byname(struct platform_device *pdev,
				const char *name)
{
	return get_rpmh_client(pdev, name, -1);
}
EXPORT_SYMBOL(rpmh_get_byname);

/**
 * rpmh_get_byindex: Get the RPMh handle by mbox index
 *
 * @pdev: the platform device which needs to communicate with RPM
 * accelerators
 * @index : The index of the mbox tuple as specified in order in DT
 *
 * May sleep.
 */
struct rpmh_client *rpmh_get_byindex(struct platform_device *pdev,
				int index)
{
	return get_rpmh_client(pdev, NULL, index);
}
EXPORT_SYMBOL(rpmh_get_byindex);

/**
 * rpmh_release: Release the RPMH client
 *
 * @rc: The RPMh handle to be freed.
 */
void rpmh_release(struct rpmh_client *rc)
{
	if (rc && !IS_ERR_OR_NULL(rc->chan))
		mbox_free_channel(rc->chan);

	kfree(rc);
}
EXPORT_SYMBOL(rpmh_release);
