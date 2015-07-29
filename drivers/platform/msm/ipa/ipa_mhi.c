/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/ipa.h>
#include "ipa_i.h"
#include "ipa_qmi_service.h"

#define IPA_MHI_DRV_NAME
#define IPA_MHI_DBG(fmt, args...) \
	pr_debug(IPA_MHI_DRV_NAME " %s:%d " fmt, \
		 __func__, __LINE__, ## args)
#define IPA_MHI_ERR(fmt, args...) \
	pr_err(IPA_MHI_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define IPA_MHI_FUNC_ENTRY() \
	IPA_MHI_DBG("ENTRY\n")
#define IPA_MHI_FUNC_EXIT() \
	IPA_MHI_DBG("EXIT\n")

#define IPA_MHI_RM_TIMEOUT_MSEC 10000

#define IPA_MHI_BAM_EMPTY_TIMEOUT_MSEC 5

#define IPA_MHI_MAX_UL_CHANNELS 1
#define IPA_MHI_MAX_DL_CHANNELS 1

#define IPA_MHI_SUSPEND_SLEEP_MIN 900
#define IPA_MHI_SUSPEND_SLEEP_MAX 1100

enum ipa_mhi_state {
	IPA_MHI_STATE_INITIALIZED,
	IPA_MHI_STATE_READY,
	IPA_MHI_STATE_STARTED,
	IPA_MHI_STATE_SUSPEND_IN_PROGRESS,
	IPA_MHI_STATE_SUSPENDED,
	IPA_MHI_STATE_RESUME_IN_PROGRESS,
	IPA_MHI_STATE_MAX
};

static char *ipa_mhi_state_str[] = {
	__stringify(IPA_MHI_STATE_INITIALIZED),
	__stringify(IPA_MHI_STATE_READY),
	__stringify(IPA_MHI_STATE_STARTED),
	__stringify(IPA_MHI_STATE_SUSPEND_IN_PROGRESS),
	__stringify(IPA_MHI_STATE_SUSPENDED),
	__stringify(IPA_MHI_STATE_RESUME_IN_PROGRESS),
};

#define MHI_STATE_STR(state) \
	(((state) >= 0 && (state) < IPA_MHI_STATE_MAX) ? \
		ipa_mhi_state_str[(state)] : \
		"INVALID")

/**
 * struct ipa_mhi_channel_ctx - MHI Channel context
 * @valid: entry is valid
 * @id: MHI channel ID
 * @hdl: channel handle for uC
 * @client: IPA Client
 * @state: Channel state
 */
struct ipa_mhi_channel_ctx {
	bool valid;
	u8 id;
	u8 hdl;
	enum ipa_client_type client;
	enum ipa_hw_mhi_channel_states state;
};

enum ipa_mhi_rm_state {
	IPA_MHI_RM_STATE_RELEASED,
	IPA_MHI_RM_STATE_REQUESTED,
	IPA_MHI_RM_STATE_GRANTED,
	IPA_MHI_RM_STATE_MAX
};

/**
 * struct ipa_mhi_ctx - IPA MHI context
 * @state: IPA MHI state
 * @state_lock: lock for state read/write operations
 * @msi: Message Signaled Interrupts parameters
 * @mmio_addr: MHI MMIO physical address
 * @first_ch_idx: First channel ID for hardware accelerated channels.
 * @first_er_idx: First event ring ID for hardware accelerated channels.
 * @host_ctrl_addr: Base address of MHI control data structures
 * @host_data_addr: Base address of MHI data buffers
 * @cb_notify: client callback
 * @cb_priv: client private data to be provided in client callback
 * @ul_channels: IPA MHI uplink channel contexts
 * @dl_channels: IPA MHI downlink channel contexts
 * @total_channels: Total number of channels ever connected to IPA MHI
 * @rm_prod_granted_comp: Completion object for MHI producer resource in IPA RM
 * @rm_cons_state: MHI consumer resource state in IPA RM
 * @rm_cons_comp: Completion object for MHI consumer resource in IPA RM
 * @trigger_wakeup: trigger wakeup callback ?
 * @wakeup_notified: MHI Client wakeup function was called
 * @wq: workqueue for wakeup event
 * @qmi_req_id: QMI request unique id
 */
struct ipa_mhi_ctx {
	enum ipa_mhi_state state;
	spinlock_t state_lock;
	struct ipa_mhi_msi_info msi;
	u32 mmio_addr;
	u32 first_ch_idx;
	u32 first_er_idx;
	u32 host_ctrl_addr;
	u32 host_data_addr;
	mhi_client_cb cb_notify;
	void *cb_priv;
	struct ipa_mhi_channel_ctx ul_channels[IPA_MHI_MAX_UL_CHANNELS];
	struct ipa_mhi_channel_ctx dl_channels[IPA_MHI_MAX_DL_CHANNELS];
	u32 total_channels;
	struct completion rm_prod_granted_comp;
	enum ipa_mhi_rm_state rm_cons_state;
	struct completion rm_cons_comp;
	bool trigger_wakeup;
	bool wakeup_notified;
	struct workqueue_struct *wq;
	u32 qmi_req_id;
};

static struct ipa_mhi_ctx *ipa_mhi_ctx;

static void ipa_mhi_wq_notify_wakeup(struct work_struct *work);
static DECLARE_WORK(ipa_mhi_notify_wakeup_work, ipa_mhi_wq_notify_wakeup);

static void ipa_mhi_wq_notify_ready(struct work_struct *work);
static DECLARE_WORK(ipa_mhi_notify_ready_work, ipa_mhi_wq_notify_ready);

static union IpaHwMhiDlUlSyncCmdData_t cached_dl_ul_sync_info;

#ifdef CONFIG_DEBUG_FS
#define IPA_MHI_MAX_MSG_LEN 512
static char dbg_buff[IPA_MHI_MAX_MSG_LEN];
static struct dentry *dent;

static char *ipa_mhi_channel_state_str[] = {
	__stringify(IPA_HW_MHI_CHANNEL_STATE_DISABLE),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_ENABLE),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_RUN),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_SUSPEND),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_STOP),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_ERROR),
};

#define MHI_CH_STATE_STR(state) \
	(((state) >= 0 && (state) <= IPA_HW_MHI_CHANNEL_STATE_ERROR) ? \
	ipa_mhi_channel_state_str[(state)] : \
	"INVALID")

static ssize_t ipa_mhi_debugfs_stats(struct file *file,
	char __user *ubuf,
	size_t count,
	loff_t *ppos)
{
	int nbytes = 0;
	int i;
	struct ipa_mhi_channel_ctx *channel;

	nbytes += scnprintf(&dbg_buff[nbytes],
		IPA_MHI_MAX_MSG_LEN - nbytes,
		"IPA MHI state: %s\n", MHI_STATE_STR(ipa_mhi_ctx->state));

	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa_mhi_ctx->ul_channels[i];
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			"channel %d: ", i);
		if (channel->valid) {
			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_MHI_MAX_MSG_LEN - nbytes,
				"ch_id=%d client=%d state=%s",
				channel->id, channel->client,
				MHI_CH_STATE_STR(channel->state));
		} else {
			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_MHI_MAX_MSG_LEN - nbytes,
				"never connected");
		}

		nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes, "\n");
	}

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa_mhi_ctx->dl_channels[i];
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			"channel %d: ", i);
		if (channel->valid) {
			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_MHI_MAX_MSG_LEN - nbytes,
				"ch_id=%d client=%d state=%s",
				channel->id, channel->client,
				MHI_CH_STATE_STR(channel->state));
		} else {
			nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_MHI_MAX_MSG_LEN - nbytes,
				"never connected");
		}

		nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes, "\n");
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_mhi_debugfs_uc_stats(struct file *file,
	char __user *ubuf,
	size_t count,
	loff_t *ppos)
{
	int nbytes = 0;
	nbytes += ipa_uc_mhi_print_stats(dbg_buff, IPA_MHI_MAX_MSG_LEN);
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

const struct file_operations ipa_mhi_stats_ops = {
	.read = ipa_mhi_debugfs_stats,
};

const struct file_operations ipa_mhi_uc_stats_ops = {
	.read = ipa_mhi_debugfs_uc_stats,
};

static void ipa_mhi_debugfs_init(void)
{
	const mode_t read_only_mode = S_IRUSR | S_IRGRP | S_IROTH;
	struct dentry *file;

	IPA_MHI_FUNC_ENTRY();

	dent = debugfs_create_dir("ipa_mhi", 0);
	if (IS_ERR(dent)) {
		IPA_MHI_ERR("fail to create folder ipa_mhi\n");
		return;
	}

	file = debugfs_create_file("stats", read_only_mode, dent,
		0, &ipa_mhi_stats_ops);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file stats\n");
		goto fail;
	}

	file = debugfs_create_file("uc_stats", read_only_mode, dent,
		0, &ipa_mhi_uc_stats_ops);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file stats\n");
		goto fail;
	}

	IPA_MHI_FUNC_EXIT();
	return;
fail:
	debugfs_remove_recursive(dent);
}

static void ipa_mhi_debugfs_destroy(void)
{
	debugfs_remove_recursive(dent);
}

#else
static void ipa_mhi_debugfs_init(void) {}
static void ipa_mhi_debugfs_destroy(void) {}
#endif /* CONFIG_DEBUG_FS */


static void ipa_mhi_cache_dl_ul_sync_info(
	struct ipa_config_req_msg_v01 *config_req)
{
	cached_dl_ul_sync_info.params.isDlUlSyncEnabled = true;
	cached_dl_ul_sync_info.params.UlAccmVal =
		(config_req->ul_accumulation_time_limit_valid) ?
		config_req->ul_accumulation_time_limit : 0;
	cached_dl_ul_sync_info.params.ulMsiEventThreshold =
		(config_req->ul_msi_event_threshold_valid) ?
		config_req->ul_msi_event_threshold : 0;
	cached_dl_ul_sync_info.params.dlMsiEventThreshold =
		(config_req->dl_msi_event_threshold_valid) ?
		config_req->dl_msi_event_threshold : 0;
}

/**
 * ipa_mhi_wq_notify_wakeup() - Notify MHI client on data available
 *
 * This function is called from IPA MHI workqueue to notify
 * MHI client driver on data available event.
 */
static void ipa_mhi_wq_notify_wakeup(struct work_struct *work)
{
	IPA_MHI_FUNC_ENTRY();
	ipa_mhi_ctx->cb_notify(ipa_mhi_ctx->cb_priv,
		IPA_MHI_EVENT_DATA_AVAILABLE, 0);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa_mhi_notify_wakeup() - Schedule work to notify data available
 *
 * This function will schedule a work to notify data available event.
 * In case this function is called more than once, only one notification will
 * be sent to MHI client driver. No further notifications will be sent until
 * IPA MHI state will become STARTED.
 */
static void ipa_mhi_notify_wakeup(void)
{
	IPA_MHI_FUNC_ENTRY();
	if (ipa_mhi_ctx->wakeup_notified) {
		IPADBG("wakeup already called\n");
		return;
	}
	queue_work(ipa_mhi_ctx->wq, &ipa_mhi_notify_wakeup_work);
	ipa_mhi_ctx->wakeup_notified = true;
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa_mhi_wq_notify_ready() - Notify MHI client on ready
 *
 * This function is called from IPA MHI workqueue to notify
 * MHI client driver on ready event when IPA uC is loaded
 */
static void ipa_mhi_wq_notify_ready(struct work_struct *work)
{
	IPA_MHI_FUNC_ENTRY();
	ipa_mhi_ctx->cb_notify(ipa_mhi_ctx->cb_priv,
		IPA_MHI_EVENT_READY, 0);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa_mhi_notify_ready() - Schedule work to notify ready
 *
 * This function will schedule a work to notify ready event.
 */
static void ipa_mhi_notify_ready(void)
{
	IPA_MHI_FUNC_ENTRY();
	queue_work(ipa_mhi_ctx->wq, &ipa_mhi_notify_ready_work);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa_mhi_set_state() - Set new state to IPA MHI
 * @state: new state
 *
 * Sets a new state to IPA MHI if possible according to IPA MHI state machine.
 * In some state transitions a wakeup request will be triggered.
 *
 * Returns: 0 on success, -1 otherwise
 */
static int ipa_mhi_set_state(enum ipa_mhi_state new_state)
{
	unsigned long flags;
	int res = -EPERM;

	spin_lock_irqsave(&ipa_mhi_ctx->state_lock, flags);
	IPA_MHI_DBG("Current state: %s\n", MHI_STATE_STR(ipa_mhi_ctx->state));

	switch (ipa_mhi_ctx->state) {
	case IPA_MHI_STATE_INITIALIZED:
		if (new_state == IPA_MHI_STATE_READY) {
			ipa_mhi_notify_ready();
			res = 0;
		}
		break;

	case IPA_MHI_STATE_READY:
		if (new_state == IPA_MHI_STATE_READY)
			res = 0;
		if (new_state == IPA_MHI_STATE_STARTED)
			res = 0;
		break;

	case IPA_MHI_STATE_STARTED:
		if (new_state == IPA_MHI_STATE_INITIALIZED)
			res = 0;
		else if (new_state == IPA_MHI_STATE_SUSPEND_IN_PROGRESS)
			res = 0;
		break;

	case IPA_MHI_STATE_SUSPEND_IN_PROGRESS:
		if (new_state == IPA_MHI_STATE_SUSPENDED) {
			if (ipa_mhi_ctx->trigger_wakeup) {
				ipa_mhi_ctx->trigger_wakeup = false;
				ipa_mhi_notify_wakeup();
			}
			res = 0;
		} else if (new_state == IPA_MHI_STATE_STARTED) {
			ipa_mhi_ctx->wakeup_notified = false;
			if (ipa_mhi_ctx->rm_cons_state ==
				IPA_MHI_RM_STATE_REQUESTED) {
				ipa_rm_notify_completion(
					IPA_RM_RESOURCE_GRANTED,
					IPA_RM_RESOURCE_MHI_CONS);
				ipa_mhi_ctx->rm_cons_state =
					IPA_MHI_RM_STATE_GRANTED;
			}
			res = 0;
		}
		break;

	case IPA_MHI_STATE_SUSPENDED:
		if (new_state == IPA_MHI_STATE_RESUME_IN_PROGRESS)
			res = 0;
		break;

	case IPA_MHI_STATE_RESUME_IN_PROGRESS:
		if (new_state == IPA_MHI_STATE_SUSPENDED) {
			if (ipa_mhi_ctx->trigger_wakeup) {
				ipa_mhi_ctx->trigger_wakeup = false;
				ipa_mhi_notify_wakeup();
			}
			res = 0;
		} else if (new_state == IPA_MHI_STATE_STARTED) {
			ipa_mhi_ctx->wakeup_notified = false;
			if (ipa_mhi_ctx->rm_cons_state ==
				IPA_MHI_RM_STATE_REQUESTED) {
				ipa_rm_notify_completion(
					IPA_RM_RESOURCE_GRANTED,
					IPA_RM_RESOURCE_MHI_CONS);
				ipa_mhi_ctx->rm_cons_state =
					IPA_MHI_RM_STATE_GRANTED;
			}
			res = 0;
		}
		break;

	default:
		IPA_MHI_ERR("invalied state %d\n", ipa_mhi_ctx->state);
		WARN_ON(1);
	}

	if (res)
		IPA_MHI_ERR("Invalid state change to %s\n",
						MHI_STATE_STR(new_state));
	else {
		IPA_MHI_DBG("New state change to %s\n",
						MHI_STATE_STR(new_state));
		ipa_mhi_ctx->state = new_state;
	}
	spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);
	return res;
}

static void ipa_mhi_rm_prod_notify(void *user_data, enum ipa_rm_event event,
	unsigned long data)
{
	IPA_MHI_FUNC_ENTRY();

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		IPA_MHI_DBG("IPA_RM_RESOURCE_GRANTED\n");
		complete_all(&ipa_mhi_ctx->rm_prod_granted_comp);
		break;

	case IPA_RM_RESOURCE_RELEASED:
		IPA_MHI_DBG("IPA_RM_RESOURCE_RELEASED\n");
		break;

	default:
		IPA_MHI_ERR("unexpected event %d\n", event);
		WARN_ON(1);
		break;
	}

	IPA_MHI_FUNC_EXIT();
}

static void ipa_mhi_uc_ready_cb(void)
{
	IPA_MHI_FUNC_ENTRY();
	ipa_mhi_set_state(IPA_MHI_STATE_READY);
	IPA_MHI_FUNC_EXIT();
}

static void ipa_mhi_uc_wakeup_request_cb(void)
{
	unsigned long flags;

	IPA_MHI_FUNC_ENTRY();
	IPA_MHI_DBG("MHI state: %s\n", MHI_STATE_STR(ipa_mhi_ctx->state));
	spin_lock_irqsave(&ipa_mhi_ctx->state_lock, flags);
	if (ipa_mhi_ctx->state == IPA_MHI_STATE_SUSPENDED) {
		ipa_mhi_notify_wakeup();
	} else if (ipa_mhi_ctx->state == IPA_MHI_STATE_SUSPEND_IN_PROGRESS) {
		/* wakeup event will be triggered after suspend finishes */
		ipa_mhi_ctx->trigger_wakeup = true;
	}
	spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa_mhi_rm_cons_request() - callback function for IPA RM request resource
 *
 * In case IPA MHI is not suspended, MHI CONS will be granted immediately.
 * In case IPA MHI is suspended, MHI CONS will be granted after resume.
 */
static int ipa_mhi_rm_cons_request(void)
{
	unsigned long flags;
	int res;

	IPA_MHI_FUNC_ENTRY();

	IPA_MHI_DBG("%s\n", MHI_STATE_STR(ipa_mhi_ctx->state));
	spin_lock_irqsave(&ipa_mhi_ctx->state_lock, flags);
	ipa_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_REQUESTED;
	if (ipa_mhi_ctx->state == IPA_MHI_STATE_STARTED) {
		ipa_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_GRANTED;
		res = 0;
	} else if (ipa_mhi_ctx->state == IPA_MHI_STATE_SUSPENDED) {
		ipa_mhi_notify_wakeup();
		res = -EINPROGRESS;
	} else if (ipa_mhi_ctx->state == IPA_MHI_STATE_SUSPEND_IN_PROGRESS) {
		/* wakeup event will be trigger after suspend finishes */
		ipa_mhi_ctx->trigger_wakeup = true;
		res = -EINPROGRESS;
	} else {
		res = -EINPROGRESS;
	}

	spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);
	IPA_MHI_DBG("EXIT with %d\n", res);
	return res;
}

static int ipa_mhi_rm_cons_release(void)
{
	unsigned long flags;

	IPA_MHI_FUNC_ENTRY();

	spin_lock_irqsave(&ipa_mhi_ctx->state_lock, flags);
	ipa_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_RELEASED;
	complete_all(&ipa_mhi_ctx->rm_cons_comp);
	spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);
	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_wait_for_cons_release(void)
{
	unsigned long flags;
	int res;

	IPA_MHI_FUNC_ENTRY();
	INIT_COMPLETION(ipa_mhi_ctx->rm_cons_comp);
	spin_lock_irqsave(&ipa_mhi_ctx->state_lock, flags);
	if (ipa_mhi_ctx->rm_cons_state != IPA_MHI_RM_STATE_GRANTED) {
		spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);

	res = wait_for_completion_timeout(
		&ipa_mhi_ctx->rm_cons_comp,
		msecs_to_jiffies(IPA_MHI_RM_TIMEOUT_MSEC));
	if (res == 0) {
		IPA_MHI_ERR("timeout release mhi cons\n");
		return -ETIME;
	}
	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_request_prod(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	INIT_COMPLETION(ipa_mhi_ctx->rm_prod_granted_comp);
	IPA_MHI_DBG("requesting mhi prod\n");
	res = ipa_rm_request_resource(IPA_RM_RESOURCE_MHI_PROD);
	if (res) {
		if (res != -EINPROGRESS) {
			IPA_MHI_ERR("failed to request mhi prod %d\n", res);
			return res;
		}
		res = wait_for_completion_timeout(
			&ipa_mhi_ctx->rm_prod_granted_comp,
			msecs_to_jiffies(IPA_MHI_RM_TIMEOUT_MSEC));
		if (res == 0) {
			IPA_MHI_ERR("timeout request mhi prod\n");
			return -ETIME;
		}
	}

	IPA_MHI_DBG("mhi prod granted\n");
	IPA_MHI_FUNC_EXIT();
	return 0;

}

static int ipa_mhi_release_prod(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	res = ipa_rm_release_resource(IPA_RM_RESOURCE_MHI_PROD);

	IPA_MHI_FUNC_EXIT();
	return res;

}

/**
 * ipa_mhi_get_channel_context() - Get corresponding channel context
 * @client: IPA client
 * @channel_id: Channel ID
 *
 * This function will return the corresponding channel context or allocate new
 * one in case channel context for channel does not exist.
 */
static struct ipa_mhi_channel_ctx *ipa_mhi_get_channel_context(
	enum ipa_client_type client, u8 channel_id)
{
	int ch_idx;
	struct ipa_mhi_channel_ctx *channels;
	int max_channels;

	if (IPA_CLIENT_IS_PROD(client)) {
		channels = ipa_mhi_ctx->ul_channels;
		max_channels = IPA_MHI_MAX_UL_CHANNELS;
	} else {
		channels = ipa_mhi_ctx->dl_channels;
		max_channels = IPA_MHI_MAX_DL_CHANNELS;
	}

	/* find the channel context according to channel id */
	for (ch_idx = 0; ch_idx < max_channels; ch_idx++) {
		if (channels[ch_idx].valid &&
		    channels[ch_idx].id == channel_id)
			return &channels[ch_idx];
	}

	/* channel context does not exists, allocate a new one */
	for (ch_idx = 0; ch_idx < max_channels; ch_idx++) {
		if (!channels[ch_idx].valid)
			break;
	}

	if (ch_idx == max_channels) {
		IPA_MHI_ERR("no more channels available\n");
		return NULL;
	}

	channels[ch_idx].valid = true;
	channels[ch_idx].id = channel_id;
	channels[ch_idx].hdl = ipa_mhi_ctx->total_channels++;
	channels[ch_idx].client = client;
	channels[ch_idx].state = IPA_HW_MHI_CHANNEL_STATE_INVALID;

	return &channels[ch_idx];
}

/**
 * ipa_mhi_get_channel_context_by_clnt_hdl() - Get corresponding channel context
 * @clnt_hdl: client handle as provided in ipa_mhi_connect_pipe()
 *
 * This function will return the corresponding channel context or NULL in case
 * that channel does not exist.
 */
static struct ipa_mhi_channel_ctx *ipa_mhi_get_channel_context_by_clnt_hdl(
	u32 clnt_hdl)
{
	int ch_idx;

	for (ch_idx = 0; ch_idx < IPA_MHI_MAX_UL_CHANNELS; ch_idx++) {
		if (ipa_mhi_ctx->ul_channels[ch_idx].valid &&
		    ipa_get_ep_mapping(
		    ipa_mhi_ctx->ul_channels[ch_idx].client) == clnt_hdl)
			return &ipa_mhi_ctx->ul_channels[ch_idx];
	}

	for (ch_idx = 0; ch_idx < IPA_MHI_MAX_DL_CHANNELS; ch_idx++) {
		if (ipa_mhi_ctx->dl_channels[ch_idx].valid &&
		    ipa_get_ep_mapping(
		    ipa_mhi_ctx->dl_channels[ch_idx].client) == clnt_hdl)
			return &ipa_mhi_ctx->dl_channels[ch_idx];
	}

	return NULL;
}

static int ipa_mhi_enable_force_clear(u32 request_id, bool throttle_source)
{
	struct ipa_enable_force_clear_datapath_req_msg_v01 req;
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	req.source_pipe_bitmask = 0;
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa_mhi_ctx->ul_channels[i].valid)
			continue;
		req.source_pipe_bitmask |= 1 << ipa_get_ep_mapping(
					ipa_mhi_ctx->ul_channels[i].client);
	}
	if (throttle_source) {
		req.throttle_source_valid = 1;
		req.throttle_source = 1;
	}
	IPA_MHI_DBG("req_id=0x%x src_pipe_btmk=0x%x throt_src=%d\n",
		req.request_id, req.source_pipe_bitmask,
		req.throttle_source);
	res = qmi_enable_force_clear_datapath_send(&req);
	if (res) {
		IPA_MHI_ERR("qmi_enable_force_clear_datapath_send failed %d\n",
			res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_disable_force_clear(u32 request_id)
{
	struct ipa_disable_force_clear_datapath_req_msg_v01 req;
	int res;

	IPA_MHI_FUNC_ENTRY();
	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	IPA_MHI_DBG("req_id=0x%x\n", req.request_id);
	res = qmi_disable_force_clear_datapath_send(&req);
	if (res) {
		IPA_MHI_ERR("qmi_disable_force_clear_datapath_send failed %d\n",
			res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

/**
 * ipa_mhi_wait_for_bam_empty_timeout() - wait for pending packets in uplink
 * @msecs: timeout to wait
 *
 * This function will poll until there are no packets pending in uplink channels
 * or timeout occured.
 *
 * Return code: true - no pending packets in uplink channels
 *		false - timeout occurred
 */
static bool ipa_mhi_wait_for_bam_empty_timeout(unsigned int msecs)
{
	unsigned long jiffies_timeout = msecs_to_jiffies(msecs);
	unsigned long jiffies_start = jiffies;
	bool empty = false;
	bool pending;
	int i;
	u32 pipe_idx;

	IPA_MHI_FUNC_ENTRY();
	while (!empty) {
		empty = true;
		for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
			if (!ipa_mhi_ctx->ul_channels[i].valid)
				continue;
			pipe_idx = ipa_get_ep_mapping(
				ipa_mhi_ctx->ul_channels[i].client);
			if (sps_pipe_pending_desc(ipa_ctx->bam_handle,
						pipe_idx, &pending)) {
				IPA_MHI_ERR("sps_pipe_pending_desc failed\n");
				WARN_ON(1);
				return false;
			}
			empty &= !pending;
		}

		if (time_after(jiffies, jiffies_start + jiffies_timeout)) {
			IPA_MHI_DBG("timeout waiting for BAM empty\n");
			break;
		}
	}
	IPA_MHI_DBG("Bam is %s\n", (empty) ? "empty" : "not empty");
	IPA_MHI_FUNC_EXIT();
	return empty;
}

static int ipa_mhi_reset_ul_channel(struct ipa_mhi_channel_ctx *channel)
{
	int res;
	int i;
	int ep_idx;
	struct ipa_ep_cfg_holb ep_holb;
	struct ipa_ep_cfg_holb old_ep_holb[IPA_MHI_MAX_DL_CHANNELS];
	bool empty;

	IPA_MHI_FUNC_ENTRY();
	res = ipa_uc_mhi_reset_channel(channel->hdl);
	if (res) {
		IPA_MHI_ERR("ipa_uc_mhi_reset_channel failed %d\n", res);
		return res;
	}
	empty = ipa_mhi_wait_for_bam_empty_timeout(
		IPA_MHI_BAM_EMPTY_TIMEOUT_MSEC);
	if (!empty) {
		IPA_MHI_DBG("BAM not empty\n");
		res = ipa_mhi_enable_force_clear(ipa_mhi_ctx->qmi_req_id,
			true);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_enable_force_clear failed %d\n",
				res);
			BUG();
			return res;
		}

		/* enable packet drop on all DL channels */
		for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
			if (!ipa_mhi_ctx->dl_channels[i].valid)
				continue;
			if (ipa_mhi_ctx->dl_channels[i].state ==
			    IPA_HW_MHI_CHANNEL_STATE_INVALID)
				continue;
			ep_idx = ipa_get_ep_mapping(
				ipa_mhi_ctx->dl_channels[i].client);
			if (-1 == ep_idx) {
				IPA_MHI_ERR("Client %u is not mapped\n",
					ipa_mhi_ctx->dl_channels[i].client);
				BUG();
				return -EFAULT;
			}
			memset(&ep_holb, 0, sizeof(ep_holb));
			ep_holb.en = 1;
			ep_holb.tmr_val = 0;
			old_ep_holb[i] = ipa_ctx->ep[ep_idx].holb;
			res = ipa_cfg_ep_holb(ep_idx, &ep_holb);
			if (res) {
				IPA_MHI_ERR("ipa_cfg_ep_holb failed %d\n", res);
				BUG();
				return res;
			}
		}

		res = ipa_tag_process(NULL, 0, HZ);
		if (res)
			IPAERR("TAG process failed\n");

		/* disable packet drop on all DL channels */
		for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
			if (!ipa_mhi_ctx->dl_channels[i].valid)
				continue;
			if (ipa_mhi_ctx->dl_channels[i].state ==
				IPA_HW_MHI_CHANNEL_STATE_INVALID)
				continue;
			ep_idx = ipa_get_ep_mapping(
				ipa_mhi_ctx->dl_channels[i].client);
			res = ipa_cfg_ep_holb(ep_idx, &old_ep_holb[i]);
			if (res) {
				IPA_MHI_ERR("ipa_cfg_ep_holb failed %d\n", res);
				BUG();
				return res;
			}
		}

		res = sps_pipe_disable(ipa_ctx->bam_handle,
			ipa_get_ep_mapping(channel->client));
		if (res) {
			IPA_MHI_ERR("sps_pipe_disable failed %d\n", res);
			BUG();
			return res;
		}

		res = ipa_mhi_disable_force_clear(ipa_mhi_ctx->qmi_req_id);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_disable_force_clear failed %d\n",
				res);
			BUG();
			return res;
		}
		ipa_mhi_ctx->qmi_req_id++;
	}

	res = ipa_disable_data_path(ipa_get_ep_mapping(channel->client));
	if (res) {
		IPA_MHI_ERR("ipa_disable_data_path failed %d\n", res);
		return res;
	}
	IPA_MHI_FUNC_EXIT();

	return 0;
}

static int ipa_mhi_reset_dl_channel(struct ipa_mhi_channel_ctx *channel)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	res = ipa_disable_data_path(ipa_get_ep_mapping(channel->client));
	if (res) {
		IPA_MHI_ERR("ipa_disable_data_path failed %d\n", res);
		return res;
	}

	res = ipa_uc_mhi_reset_channel(channel->hdl);
	if (res) {
		IPA_MHI_ERR("ipa_uc_mhi_reset_channel failed %d\n", res);
		goto fail_reset_channel;
	}
	IPA_MHI_FUNC_EXIT();

	return 0;

fail_reset_channel:
	ipa_enable_data_path(ipa_get_ep_mapping(channel->client));
	return res;
}

static int ipa_mhi_reset_channel(struct ipa_mhi_channel_ctx *channel)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	if (IPA_CLIENT_IS_PROD(channel->client))
		res = ipa_mhi_reset_ul_channel(channel);
	else
		res = ipa_mhi_reset_dl_channel(channel);
	if (res) {
		IPA_MHI_ERR("failed to reset channel error %d\n", res);
		return res;
	}

	channel->state = IPA_HW_MHI_CHANNEL_STATE_DISABLE;
	IPA_MHI_FUNC_EXIT();
	return 0;
}

/**
 * ipa_mhi_init() - Initialize IPA MHI driver
 * @params: initialization params
 *
 * This function is called by MHI client driver on boot to initialize IPA MHI
 * Driver. When this function returns device can move to READY state.
 * This function is doing the following:
 *	- Initialize MHI IPA internal data structures
 *	- Create IPA RM resources
 *	- Initialize debugfs
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_init(struct ipa_mhi_init_params *params)
{
	int res;
	struct ipa_rm_create_params mhi_prod_params;
	struct ipa_rm_create_params mhi_cons_params;

	IPA_MHI_FUNC_ENTRY();

	if (!params) {
		IPA_MHI_ERR("null args\n");
		return -EINVAL;
	}

	if (!params->notify) {
		IPA_MHI_ERR("null notify function\n");
		return -EINVAL;
	}

	if (ipa_mhi_ctx) {
		IPA_MHI_ERR("already initialized\n");
		return -EPERM;
	}

	IPA_MHI_DBG("msi: addr_lo = 0x%x addr_hi = 0x%x\n",
		params->msi.addr_low, params->msi.addr_hi);
	IPA_MHI_DBG("msi: data = 0x%x mask = 0x%x\n",
		params->msi.data, params->msi.mask);
	IPA_MHI_DBG("mmio_addr = 0x%x\n", params->mmio_addr);
	IPA_MHI_DBG("first_ch_idx = 0x%x\n", params->first_ch_idx);
	IPA_MHI_DBG("first_er_idx = 0x%x\n", params->first_er_idx);
	IPA_MHI_DBG("notify = %pF priv = %p\n", params->notify, params->priv);

	/* Initialize context */
	ipa_mhi_ctx = kzalloc(sizeof(*ipa_mhi_ctx), GFP_KERNEL);
	if (!ipa_mhi_ctx) {
		IPA_MHI_ERR("no memory\n");
		res = -EFAULT;
		goto fail_alloc_ctx;
	}

	ipa_mhi_ctx->state = IPA_MHI_STATE_INITIALIZED;
	ipa_mhi_ctx->msi = params->msi;
	ipa_mhi_ctx->mmio_addr = params->mmio_addr;
	ipa_mhi_ctx->first_ch_idx = params->first_ch_idx;
	ipa_mhi_ctx->first_er_idx = params->first_er_idx;
	ipa_mhi_ctx->cb_notify = params->notify;
	ipa_mhi_ctx->cb_priv = params->priv;
	ipa_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_RELEASED;
	ipa_mhi_ctx->qmi_req_id = 0;
	init_completion(&ipa_mhi_ctx->rm_prod_granted_comp);
	spin_lock_init(&ipa_mhi_ctx->state_lock);
	init_completion(&ipa_mhi_ctx->rm_cons_comp);

	ipa_mhi_ctx->wq = create_singlethread_workqueue("ipa_mhi_wq");
	if (!ipa_mhi_ctx->wq) {
		IPA_MHI_ERR("failed to create workqueue\n");
		res = -EFAULT;
		goto fail_create_wq;
	}

	/* Initialize debugfs */
	ipa_mhi_debugfs_init();

	/* Create PROD in IPA RM */
	memset(&mhi_prod_params, 0, sizeof(mhi_prod_params));
	mhi_prod_params.name = IPA_RM_RESOURCE_MHI_PROD;
	mhi_prod_params.floor_voltage = IPA_VOLTAGE_SVS;
	mhi_prod_params.reg_params.notify_cb = ipa_mhi_rm_prod_notify;
	res = ipa_rm_create_resource(&mhi_prod_params);
	if (res) {
		IPA_MHI_ERR("fail to create IPA_RM_RESOURCE_MHI_PROD\n");
		goto fail_create_rm_prod;
	}

	/* Create CONS in IPA RM */
	memset(&mhi_cons_params, 0, sizeof(mhi_cons_params));
	mhi_cons_params.name = IPA_RM_RESOURCE_MHI_CONS;
	mhi_cons_params.floor_voltage = IPA_VOLTAGE_SVS;
	mhi_cons_params.request_resource = ipa_mhi_rm_cons_request;
	mhi_cons_params.release_resource = ipa_mhi_rm_cons_release;
	res = ipa_rm_create_resource(&mhi_cons_params);
	if (res) {
		IPA_MHI_ERR("fail to create IPA_RM_RESOURCE_MHI_CONS\n");
		goto fail_create_rm_cons;
	}

	/* Initialize uC interface */
	ipa_uc_mhi_init(ipa_mhi_uc_ready_cb, ipa_mhi_uc_wakeup_request_cb);
	if (ipa_uc_state_check() == 0)
		ipa_mhi_set_state(IPA_MHI_STATE_READY);

	IPA_MHI_FUNC_EXIT();

	return 0;

fail_create_rm_cons:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_MHI_PROD);
fail_create_rm_prod:
	destroy_workqueue(ipa_mhi_ctx->wq);
fail_create_wq:
	kfree(ipa_mhi_ctx);
	ipa_mhi_ctx = NULL;
fail_alloc_ctx:
	return res;
}
EXPORT_SYMBOL(ipa_mhi_init);

/**
 * ipa_mhi_start() - Start IPA MHI engine
 * @params: pcie addresses for MHI
 *
 * This function is called by MHI client driver on MHI engine start for
 * handling MHI accelerated channels. This function is called after
 * ipa_mhi_init() was called and can be called after MHI reset to restart MHI
 * engine. When this function returns device can move to M0 state.
 * This function is doing the following:
 *	- Send command to uC for initialization of MHI engine
 *	- Add dependencies to IPA RM
 *	- Request MHI_PROD in IPA RM
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_start(struct ipa_mhi_start_params *params)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (!params) {
		IPA_MHI_ERR("null args\n");
		return -EINVAL;
	}

	if (unlikely(!ipa_mhi_ctx)) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	if (ipa_uc_state_check()) {
		IPA_MHI_ERR("IPA uc is not loaded\n");
		return -EAGAIN;
	}

	res = ipa_mhi_set_state(IPA_MHI_STATE_STARTED);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state %d\n", res);
		return res;
	}

	ipa_mhi_ctx->host_ctrl_addr = params->host_ctrl_addr;
	ipa_mhi_ctx->host_data_addr = params->host_data_addr;

	/* Add MHI <-> Q6 dependencies to IPA RM */
	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_MHI_PROD,
		IPA_RM_RESOURCE_Q6_CONS);
	if (res && res != -EINPROGRESS) {
		IPA_MHI_ERR("failed to add dependency %d\n", res);
		goto fail_add_mhi_q6_dep;
	}

	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_Q6_PROD,
		IPA_RM_RESOURCE_MHI_CONS);
	if (res && res != -EINPROGRESS) {
		IPA_MHI_ERR("failed to add dependency %d\n", res);
		goto fail_add_q6_mhi_dep;
	}

	res = ipa_mhi_request_prod();
	if (res) {
		IPA_MHI_ERR("failed request prod %d\n", res);
		goto fail_request_prod;
	}

	/* Initialize IPA MHI engine */
	res = ipa_uc_mhi_init_engine(&ipa_mhi_ctx->msi, ipa_mhi_ctx->mmio_addr,
		ipa_mhi_ctx->host_ctrl_addr, ipa_mhi_ctx->host_data_addr,
		ipa_mhi_ctx->first_ch_idx, ipa_mhi_ctx->first_er_idx);
	if (res) {
		IPA_MHI_ERR("failed to start MHI engine %d\n", res);
		goto fail_init_engine;
	}

	/* Update UL/DL sync if valid */
	res = ipa_uc_mhi_send_dl_ul_sync_info(cached_dl_ul_sync_info);
	if (res) {
		IPA_MHI_ERR("failed to update ul/dl sync %d\n", res);
		goto fail_init_engine;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_init_engine:
	ipa_mhi_release_prod();
fail_request_prod:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
		IPA_RM_RESOURCE_MHI_CONS);
fail_add_q6_mhi_dep:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_MHI_PROD,
		IPA_RM_RESOURCE_Q6_CONS);
fail_add_mhi_q6_dep:
	ipa_mhi_set_state(IPA_MHI_STATE_INITIALIZED);
	return res;
}
EXPORT_SYMBOL(ipa_mhi_start);

/**
 * ipa_mhi_connect_pipe() - Connect pipe to IPA and start corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 * This function is doing the following:
 *	- Send command to uC to start corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in, u32 *clnt_hdl)
{
	struct ipa_ep_context *ep;
	int ipa_ep_idx;
	int res;
	struct ipa_mhi_channel_ctx *channel = NULL;
	unsigned long flags;

	IPA_MHI_FUNC_ENTRY();

	if (!in || !clnt_hdl) {
		IPA_MHI_ERR("NULL args\n");
		return -EINVAL;
	}

	if (in->sys.client >= IPA_CLIENT_MAX) {
		IPA_MHI_ERR("bad parm client:%d\n", in->sys.client);
		return -EINVAL;
	}

	if (unlikely(!ipa_mhi_ctx)) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ipa_mhi_ctx->state_lock, flags);
	if (!ipa_mhi_ctx || ipa_mhi_ctx->state != IPA_MHI_STATE_STARTED) {
		IPA_MHI_ERR("IPA MHI was not started\n");
		spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&ipa_mhi_ctx->state_lock, flags);

	ipa_ep_idx = ipa_get_ep_mapping(in->sys.client);
	if (ipa_ep_idx == -1) {
		IPA_MHI_ERR("Invalid client.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	channel = ipa_mhi_get_channel_context(in->sys.client,
		in->channel_id);
	if (!channel) {
		IPA_MHI_ERR("ipa_mhi_get_channel_context failed\n");
		return -EINVAL;
	}

	IPA_MHI_DBG("client %d channelHandle %d channelIndex %d\n",
		channel->client, channel->hdl, channel->id);

	ipa_inc_client_enable_clks();

	if (ep->valid == 1) {
		IPA_MHI_ERR("EP already allocated.\n");
		goto fail_ep_exists;
	}

	memset(ep, 0, offsetof(struct ipa_ep_context, sys));
	ep->valid = 1;
	ep->skip_ep_cfg = in->sys.skip_ep_cfg;
	ep->client = in->sys.client;
	ep->client_notify = in->sys.notify;
	ep->priv = in->sys.priv;
	ep->keep_ipa_awake = in->sys.keep_ipa_awake;

	/* start channel in uC */
	if (channel->state == IPA_HW_MHI_CHANNEL_STATE_INVALID) {
		IPA_MHI_DBG("Initializing channel\n");
		res = ipa_uc_mhi_init_channel(ipa_ep_idx, channel->hdl,
			channel->id, (IPA_CLIENT_IS_PROD(ep->client) ? 1 : 2));
		if (res) {
			IPA_MHI_ERR("init_channel failed %d\n", res);
			goto fail_init_channel;
		}
	} else if (channel->state == IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
		if (channel->client != ep->client) {
			IPA_MHI_ERR("previous channel client was %d\n",
				ep->client);
			goto fail_init_channel;
		}
		IPA_MHI_DBG("Starting channel\n");
		res = ipa_uc_mhi_resume_channel(channel->hdl, false);
		if (res) {
			IPA_MHI_ERR("init_channel failed %d\n", res);
			goto fail_init_channel;
		}
	} else {
		IPA_MHI_ERR("Invalid channel state %d\n", channel->state);
		goto fail_init_channel;
	}

	channel->state = IPA_HW_MHI_CHANNEL_STATE_RUN;

	res = ipa_enable_data_path(ipa_ep_idx);
	if (res) {
		IPA_MHI_ERR("enable data path failed res=%d clnt=%d.\n", res,
			ipa_ep_idx);
		goto fail_enable_dp;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa_cfg_ep(ipa_ep_idx, &in->sys.ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_ep_cfg;
		}
		if (ipa_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_ep_cfg;
		}
		IPA_MHI_DBG("ep configuration successful\n");
	} else {
		IPA_MHI_DBG("skipping ep configuration\n");
	}

	*clnt_hdl = ipa_ep_idx;

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->sys.client))
		ipa_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		ipa_dec_client_disable_clks();

	ipa_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	IPA_MHI_DBG("client %d (ep: %d) connected\n", in->sys.client,
		ipa_ep_idx);

	IPA_MHI_FUNC_EXIT();

	return 0;

fail_ep_cfg:
	ipa_disable_data_path(ipa_ep_idx);
fail_enable_dp:
	ipa_uc_mhi_reset_channel(channel->hdl);
	channel->state = IPA_HW_MHI_CHANNEL_STATE_DISABLE;
fail_init_channel:
	memset(ep, 0, offsetof(struct ipa_ep_context, sys));
fail_ep_exists:
	ipa_dec_client_disable_clks();
	return -EPERM;
}
EXPORT_SYMBOL(ipa_mhi_connect_pipe);

/**
 * ipa_mhi_disconnect_pipe() - Disconnect pipe from IPA and reset corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to uC to reset corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_disconnect_pipe(u32 clnt_hdl)
{
	struct ipa_ep_context *ep;
	static struct ipa_mhi_channel_ctx *channel;
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (clnt_hdl >= ipa_ctx->ipa_num_pipes) {
		IPAERR("invalid handle %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("pipe was not connected %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (unlikely(!ipa_mhi_ctx)) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	channel = ipa_mhi_get_channel_context_by_clnt_hdl(clnt_hdl);
	if (!channel) {
		IPAERR("invalid clnt hdl\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		ipa_inc_client_enable_clks();

	res = ipa_mhi_reset_channel(channel);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_reset_channel failed %d\n", res);
		goto fail_reset_channel;
	}

	ep->valid = 0;
	ipa_delete_dflt_flt_rules(clnt_hdl);

	ipa_dec_client_disable_clks();

	IPA_MHI_DBG("client (ep: %d) disconnected\n", clnt_hdl);
	IPA_MHI_FUNC_EXIT();
	return 0;

fail_reset_channel:
	if (!ep->keep_ipa_awake)
		ipa_dec_client_disable_clks();
	return res;
}
EXPORT_SYMBOL(ipa_mhi_disconnect_pipe);

static int ipa_mhi_suspend_ul_channels(void)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa_mhi_ctx->ul_channels[i].valid)
			continue;
		if (ipa_mhi_ctx->ul_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_RUN)
			continue;
		IPA_MHI_DBG("suspending channel %d\n",
			ipa_mhi_ctx->ul_channels[i].hdl);
		res = ipa_uc_mhi_suspend_channel(
			ipa_mhi_ctx->ul_channels[i].hdl);
		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		ipa_mhi_ctx->ul_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_SUSPEND;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_resume_ul_channels(bool LPTransitionRejected)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa_mhi_ctx->ul_channels[i].valid)
			continue;
		if (ipa_mhi_ctx->ul_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("suspending channel %d\n",
			ipa_mhi_ctx->ul_channels[i].hdl);
		res = ipa_uc_mhi_resume_channel(ipa_mhi_ctx->ul_channels[i].hdl,
			LPTransitionRejected);
		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		ipa_mhi_ctx->ul_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_RUN;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_stop_event_update_ul_channels(void)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa_mhi_ctx->ul_channels[i].valid)
			continue;
		if (ipa_mhi_ctx->ul_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("stop update event channel %d\n",
			ipa_mhi_ctx->ul_channels[i].hdl);
		res = ipa_uc_mhi_stop_event_update_channel(
			ipa_mhi_ctx->ul_channels[i].hdl);
		if (res) {
			IPA_MHI_ERR("failed stop event channel %d error %d\n",
				i, res);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_suspend_dl_channels(void)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa_mhi_ctx->dl_channels[i].valid)
			continue;
		if (ipa_mhi_ctx->dl_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_RUN)
			continue;
		IPA_MHI_DBG("suspending channel %d\n",
			ipa_mhi_ctx->dl_channels[i].hdl);
		res = ipa_uc_mhi_suspend_channel(
			ipa_mhi_ctx->dl_channels[i].hdl);
		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		ipa_mhi_ctx->dl_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_SUSPEND;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_resume_dl_channels(bool LPTransitionRejected)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa_mhi_ctx->dl_channels[i].valid)
			continue;
		if (ipa_mhi_ctx->dl_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("suspending channel %d\n",
			ipa_mhi_ctx->dl_channels[i].hdl);
		res = ipa_uc_mhi_resume_channel(ipa_mhi_ctx->dl_channels[i].hdl,
			LPTransitionRejected);
		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		ipa_mhi_ctx->dl_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_RUN;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_stop_event_update_dl_channels(void)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa_mhi_ctx->dl_channels[i].valid)
			continue;
		if (ipa_mhi_ctx->dl_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("stop update event channel %d\n",
			ipa_mhi_ctx->dl_channels[i].hdl);
		res = ipa_uc_mhi_stop_event_update_channel(
			ipa_mhi_ctx->dl_channels[i].hdl);
		if (res) {
			IPA_MHI_ERR("failed stop event channel %d error %d\n",
				i, res);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

/**
 * ipa_mhi_suspend() - Suspend MHI accelerated channels
 * @force:
 *	false: in case of data pending in IPA, MHI channels will not be
 *		suspended and function will fail.
 *	true:  in case of data pending in IPA, make sure no further access from
 *		IPA to PCIe is possible. In this case suspend cannot fail.
 *
 * This function is called by MHI client driver on MHI suspend.
 * This function is called after MHI channel was started.
 * When this function returns device can move to M1/M2/M3/D3cold state.
 * This function is doing the following:
 *	- Send command to uC to suspend corresponding MHI channel
 *	- Make sure no further access is possible from IPA to PCIe
 *	- Release MHI_PROD in IPA RM
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_suspend(bool force)
{
	int res;
	bool bam_empty;
	bool force_clear = false;

	IPA_MHI_FUNC_ENTRY();

	if (unlikely(!ipa_mhi_ctx)) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	res = ipa_mhi_set_state(IPA_MHI_STATE_SUSPEND_IN_PROGRESS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		return res;
	}

	res = ipa_mhi_suspend_ul_channels();
	if (res) {
		IPA_MHI_ERR("ipa_mhi_suspend_ul_channels failed %d\n", res);
		goto fail_suspend_ul_channel;
	}

	bam_empty = ipa_mhi_wait_for_bam_empty_timeout(
		IPA_MHI_BAM_EMPTY_TIMEOUT_MSEC);
	if (!bam_empty) {
		if (force) {
			res = ipa_mhi_enable_force_clear(
				ipa_mhi_ctx->qmi_req_id, false);
			if (res) {
				IPA_MHI_ERR("failed to enable force clear\n");
				BUG();
				return res;
			}
			force_clear = true;
			IPA_MHI_DBG("force clear datapath enabled\n");

			bam_empty = ipa_mhi_wait_for_bam_empty_timeout(
				IPA_MHI_BAM_EMPTY_TIMEOUT_MSEC);
			IPADBG("bam_empty=%d\n", bam_empty);

		} else {
			IPA_MHI_DBG("BAM not empty\n");
			res = -EAGAIN;
			goto fail_suspend_ul_channel;
		}
	}

	res = ipa_mhi_stop_event_update_ul_channels();
	if (res) {
		IPA_MHI_ERR("ipa_mhi_stop_event_update_ul_channels failed %d\n",
			res);
		goto fail_suspend_ul_channel;
	}

	/*
	 * in case BAM not empty, hold IPA clocks and release them after all
	 * IPA RM resource are released to make sure tag process will not start
	 */
	if (!bam_empty)
		ipa_inc_client_enable_clks();

	IPA_MHI_DBG("release prod\n");
	res = ipa_mhi_release_prod();
	if (res) {
		IPA_MHI_ERR("ipa_mhi_release_prod failed %d\n", res);
		goto fail_release_prod;
	}

	IPA_MHI_DBG("wait for cons release\n");
	res = ipa_mhi_wait_for_cons_release();
	if (res) {
		IPA_MHI_ERR("ipa_mhi_wait_for_cons_release failed %d\n", res);
		goto fail_release_cons;
	}

	usleep_range(IPA_MHI_SUSPEND_SLEEP_MIN, IPA_MHI_SUSPEND_SLEEP_MAX);

	res = ipa_mhi_suspend_dl_channels();
	if (res) {
		IPA_MHI_ERR("ipa_mhi_suspend_dl_channels failed %d\n", res);
		goto fail_suspend_dl_channel;
	}

	res = ipa_mhi_stop_event_update_dl_channels();
	if (res) {
		IPA_MHI_ERR("failed to stop event update on DL %d\n", res);
		goto fail_stop_event_update_dl_channel;
	}

	if (force_clear) {
		res = ipa_mhi_disable_force_clear(ipa_mhi_ctx->qmi_req_id);
		if (res) {
			IPA_MHI_ERR("failed to disable force clear\n");
			BUG();
			return res;
		}
		IPA_MHI_DBG("force clear datapath disabled\n");
		ipa_mhi_ctx->qmi_req_id++;
	}

	if (!bam_empty) {
		ipa_ctx->tag_process_before_gating = false;
		ipa_dec_client_disable_clks();
	}

	res = ipa_mhi_set_state(IPA_MHI_STATE_SUSPENDED);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		goto fail_release_cons;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_stop_event_update_dl_channel:
	ipa_mhi_resume_dl_channels(true);
fail_suspend_dl_channel:
fail_release_cons:
	ipa_mhi_request_prod();
fail_release_prod:
fail_suspend_ul_channel:
	ipa_mhi_resume_ul_channels(true);
	ipa_mhi_set_state(IPA_MHI_STATE_STARTED);
	return res;
}
EXPORT_SYMBOL(ipa_mhi_suspend);

/**
 * ipa_mhi_resume() - Resume MHI accelerated channels
 *
 * This function is called by MHI client driver on MHI resume.
 * This function is called after MHI channel was suspended.
 * When this function returns device can move to M0 state.
 * This function is doing the following:
 *	- Send command to uC to resume corresponding MHI channel
 *	- Request MHI_PROD in IPA RM
 *	- Resume data to IPA
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_resume(void)
{
	int res;
	bool dl_channel_resumed = false;

	IPA_MHI_FUNC_ENTRY();

	if (unlikely(!ipa_mhi_ctx)) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	res = ipa_mhi_set_state(IPA_MHI_STATE_RESUME_IN_PROGRESS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		return res;
	}

	if (ipa_mhi_ctx->rm_cons_state == IPA_MHI_RM_STATE_REQUESTED) {
		/* resume all DL channels */
		res = ipa_mhi_resume_dl_channels(false);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_resume_dl_channels failed %d\n",
				res);
			goto fail_resume_dl_channels;
		}
		dl_channel_resumed = true;

		ipa_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
			IPA_RM_RESOURCE_MHI_CONS);
		ipa_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_GRANTED;
	}

	res = ipa_mhi_request_prod();
	if (res) {
		IPA_MHI_ERR("ipa_mhi_request_prod failed %d\n", res);
		goto fail_request_prod;
	}

	/* resume all UL channels */
	res = ipa_mhi_resume_ul_channels(false);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_resume_ul_channels failed %d\n", res);
		goto fail_resume_ul_channels;
	}

	if (!dl_channel_resumed) {
		res = ipa_mhi_resume_dl_channels(true);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_resume_dl_channels failed %d\n",
				res);
			goto fail_resume_dl_channels2;
		}
	}

	res = ipa_mhi_set_state(IPA_MHI_STATE_STARTED);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		goto fail_set_state;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_set_state:
	ipa_mhi_suspend_dl_channels();
fail_resume_dl_channels2:
	ipa_mhi_suspend_ul_channels();
fail_resume_ul_channels:
	ipa_mhi_release_prod();
fail_request_prod:
	ipa_mhi_suspend_dl_channels();
fail_resume_dl_channels:
	ipa_mhi_set_state(IPA_MHI_STATE_SUSPENDED);
	return res;
}
EXPORT_SYMBOL(ipa_mhi_resume);

/**
 * ipa_mhi_destroy() - Destroy MHI IPA
 *
 * This function is called by MHI client driver on MHI reset to destroy all IPA
 * MHI resources.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_destroy(void)
{
	IPA_MHI_FUNC_ENTRY();

	if (unlikely(!ipa_mhi_ctx)) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	IPAERR("Not implemented Yet!\n");
	ipa_mhi_debugfs_destroy();

	IPA_MHI_FUNC_EXIT();
	return -EPERM;
}
EXPORT_SYMBOL(ipa_mhi_destroy);

/**
 * ipa_mhi_handle_ipa_config_req() - hanle IPA CONFIG QMI message
 *
 * This function is called by by IPA QMI service to indicate that IPA CONFIG
 * message was sent from modem. IPA MHI will update this information to IPA uC
 * or will cache it until IPA MHI will be initialized.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_handle_ipa_config_req(struct ipa_config_req_msg_v01 *config_req)
{
	IPA_MHI_FUNC_ENTRY();
	ipa_mhi_cache_dl_ul_sync_info(config_req);

	if (ipa_mhi_ctx && ipa_mhi_ctx->state != IPA_MHI_STATE_INITIALIZED)
		ipa_uc_mhi_send_dl_ul_sync_info(cached_dl_ul_sync_info);

	IPA_MHI_FUNC_EXIT();
	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA MHI driver");
