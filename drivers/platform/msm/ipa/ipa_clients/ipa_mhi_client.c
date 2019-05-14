/* Copyright (c) 2015, 2017-2019 The Linux Foundation. All rights reserved.
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
#include <linux/msm_gsi.h>
#include <linux/ipa_qmi_service_v01.h>
#include <linux/ipa_mhi.h>
#include "../ipa_common_i.h"
#include "../ipa_v3/ipa_pm.h"

#define IPA_MHI_DRV_NAME "ipa_mhi_client"

#define IPA_MHI_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_MHI_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_MHI_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_MHI_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IPA_MHI_ERR(fmt, args...) \
	do { \
		pr_err(IPA_MHI_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_MHI_FUNC_ENTRY() \
	IPA_MHI_DBG("ENTRY\n")
#define IPA_MHI_FUNC_EXIT() \
	IPA_MHI_DBG("EXIT\n")

#define IPA_MHI_RM_TIMEOUT_MSEC 10000
#define IPA_MHI_CH_EMPTY_TIMEOUT_MSEC 10

#define IPA_MHI_SUSPEND_SLEEP_MIN 900
#define IPA_MHI_SUSPEND_SLEEP_MAX 1100

#define IPA_MHI_MAX_UL_CHANNELS 1
#define IPA_MHI_MAX_DL_CHANNELS 2

/* bit #40 in address should be asserted for MHI transfers over pcie */
#define IPA_MHI_CLIENT_HOST_ADDR_COND(addr) \
	((ipa_mhi_client_ctx->assert_bit40)?(IPA_MHI_HOST_ADDR(addr)):(addr))

enum ipa_mhi_rm_state {
	IPA_MHI_RM_STATE_RELEASED,
	IPA_MHI_RM_STATE_REQUESTED,
	IPA_MHI_RM_STATE_GRANTED,
	IPA_MHI_RM_STATE_MAX
};

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

enum ipa_mhi_dma_dir {
	IPA_MHI_DMA_TO_HOST,
	IPA_MHI_DMA_FROM_HOST,
};

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
	u8 index;
	enum ipa_client_type client;
	enum ipa_hw_mhi_channel_states state;
	bool stop_in_proc;
	struct gsi_chan_info ch_info;
	u64 channel_context_addr;
	struct ipa_mhi_ch_ctx ch_ctx_host;
	u64 event_context_addr;
	struct ipa_mhi_ev_ctx ev_ctx_host;
	bool brstmode_enabled;
	union __packed gsi_channel_scratch ch_scratch;
	unsigned long cached_gsi_evt_ring_hdl;
};

struct ipa_mhi_client_ctx {
	enum ipa_mhi_state state;
	spinlock_t state_lock;
	mhi_client_cb cb_notify;
	void *cb_priv;
	struct completion rm_prod_granted_comp;
	enum ipa_mhi_rm_state rm_cons_state;
	struct completion rm_cons_comp;
	bool trigger_wakeup;
	bool wakeup_notified;
	struct workqueue_struct *wq;
	struct ipa_mhi_channel_ctx ul_channels[IPA_MHI_MAX_UL_CHANNELS];
	struct ipa_mhi_channel_ctx dl_channels[IPA_MHI_MAX_DL_CHANNELS];
	u32 total_channels;
	struct ipa_mhi_msi_info msi;
	u32 mmio_addr;
	u32 first_ch_idx;
	u32 first_er_idx;
	u32 host_ctrl_addr;
	u32 host_data_addr;
	u64 channel_context_array_addr;
	u64 event_context_array_addr;
	u32 qmi_req_id;
	u32 use_ipadma;
	bool assert_bit40;
	bool test_mode;
	u32 pm_hdl;
	u32 modem_pm_hdl;
};

static struct ipa_mhi_client_ctx *ipa_mhi_client_ctx;
static DEFINE_MUTEX(mhi_client_general_mutex);

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

static int ipa_mhi_set_lock_unlock(bool is_lock)
{
	IPA_MHI_DBG("entry\n");
	if (is_lock)
		mutex_lock(&mhi_client_general_mutex);
	else
		mutex_unlock(&mhi_client_general_mutex);
	IPA_MHI_DBG("exit\n");

	return 0;
}

static int ipa_mhi_read_write_host(enum ipa_mhi_dma_dir dir, void *dev_addr,
	u64 host_addr, int size)
{
	struct ipa_mem_buffer mem;
	int res;
	struct device *pdev;

	IPA_MHI_FUNC_ENTRY();

	if (ipa_mhi_client_ctx->use_ipadma) {
		pdev = ipa_get_dma_dev();
		host_addr = IPA_MHI_CLIENT_HOST_ADDR_COND(host_addr);

		mem.size = size;
		mem.base = dma_alloc_coherent(pdev, mem.size,
			&mem.phys_base, GFP_KERNEL);
		if (!mem.base) {
			IPA_MHI_ERR(
				"dma_alloc_coherent failed, DMA buff size %d\n"
					, mem.size);
			return -ENOMEM;
		}

		res = ipa_dma_enable();
		if (res) {
			IPA_MHI_ERR("failed to enable IPA DMA rc=%d\n", res);
			goto fail_dma_enable;
		}

		if (dir == IPA_MHI_DMA_FROM_HOST) {
			res = ipa_dma_sync_memcpy(mem.phys_base, host_addr,
				size);
			if (res) {
				IPA_MHI_ERR(
					"ipa_dma_sync_memcpy from host fail%d\n"
					, res);
				goto fail_memcopy;
			}
			memcpy(dev_addr, mem.base, size);
		} else {
			memcpy(mem.base, dev_addr, size);
			res = ipa_dma_sync_memcpy(host_addr, mem.phys_base,
				size);
			if (res) {
				IPA_MHI_ERR(
					"ipa_dma_sync_memcpy to host fail %d\n"
					, res);
				goto fail_memcopy;
			}
		}
		goto dma_succeed;
	} else {
		void *host_ptr;

		if (!ipa_mhi_client_ctx->test_mode)
			host_ptr = ioremap(host_addr, size);
		else
			host_ptr = phys_to_virt(host_addr);
		if (!host_ptr) {
			IPA_MHI_ERR("ioremap failed for 0x%llx\n", host_addr);
			return -EFAULT;
		}
		if (dir == IPA_MHI_DMA_FROM_HOST)
			memcpy(dev_addr, host_ptr, size);
		else
			memcpy(host_ptr, dev_addr, size);
		if (!ipa_mhi_client_ctx->test_mode)
			iounmap(host_ptr);
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

dma_succeed:
	IPA_MHI_FUNC_EXIT();
	res = 0;
fail_memcopy:
	if (ipa_dma_disable())
		IPA_MHI_ERR("failed to disable IPA DMA\n");
fail_dma_enable:
	dma_free_coherent(pdev, mem.size, mem.base, mem.phys_base);
	return res;
}

static int ipa_mhi_print_channel_info(struct ipa_mhi_channel_ctx *channel,
	char *buff, int len)
{
	int nbytes = 0;

	if (channel->valid) {
		nbytes += scnprintf(&buff[nbytes],
			len - nbytes,
			"channel idx=%d ch_id=%d client=%d state=%s\n",
			channel->index, channel->id, channel->client,
			MHI_CH_STATE_STR(channel->state));

		nbytes += scnprintf(&buff[nbytes],
			len - nbytes,
			"	ch_ctx=%llx\n",
			channel->channel_context_addr);

		nbytes += scnprintf(&buff[nbytes],
			len - nbytes,
			"	gsi_evt_ring_hdl=%ld ev_ctx=%llx\n",
			channel->cached_gsi_evt_ring_hdl,
			channel->event_context_addr);
	}
	return nbytes;
}

static int ipa_mhi_print_host_channel_ctx_info(
		struct ipa_mhi_channel_ctx *channel, char *buff, int len)
{
	int res, nbytes = 0;
	struct ipa_mhi_ch_ctx ch_ctx_host;

	memset(&ch_ctx_host, 0, sizeof(ch_ctx_host));

	/* reading ch context from host */
	res = ipa_mhi_read_write_host(IPA_MHI_DMA_FROM_HOST,
		&ch_ctx_host, channel->channel_context_addr,
		sizeof(ch_ctx_host));
	if (res) {
		nbytes += scnprintf(&buff[nbytes], len - nbytes,
			"Failed to read from host %d\n", res);
		return nbytes;
	}

	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"ch_id: %d\n", channel->id);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"chstate: 0x%x\n", ch_ctx_host.chstate);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"brstmode: 0x%x\n", ch_ctx_host.brstmode);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"chtype: 0x%x\n", ch_ctx_host.chtype);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"erindex: 0x%x\n", ch_ctx_host.erindex);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"rbase: 0x%llx\n", ch_ctx_host.rbase);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"rlen: 0x%llx\n", ch_ctx_host.rlen);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"rp: 0x%llx\n", ch_ctx_host.rp);
	nbytes += scnprintf(&buff[nbytes], len - nbytes,
		"wp: 0x%llx\n", ch_ctx_host.wp);

	return nbytes;
}

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
		"IPA MHI state: %s\n",
		MHI_STATE_STR(ipa_mhi_client_ctx->state));

	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->ul_channels[i];
		nbytes += ipa_mhi_print_channel_info(channel,
			&dbg_buff[nbytes], IPA_MHI_MAX_MSG_LEN - nbytes);
	}

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->dl_channels[i];
		nbytes += ipa_mhi_print_channel_info(channel,
			&dbg_buff[nbytes], IPA_MHI_MAX_MSG_LEN - nbytes);
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

static ssize_t ipa_mhi_debugfs_dump_host_ch_ctx_arr(struct file *file,
	char __user *ubuf,
	size_t count,
	loff_t *ppos)
{
	int i, nbytes = 0;
	struct ipa_mhi_channel_ctx *channel;

	if (ipa_mhi_client_ctx->state == IPA_MHI_STATE_INITIALIZED ||
	    ipa_mhi_client_ctx->state == IPA_MHI_STATE_READY) {
		nbytes += scnprintf(&dbg_buff[nbytes],
		IPA_MHI_MAX_MSG_LEN - nbytes,
			"Cannot dump host channel context ");
		nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_MHI_MAX_MSG_LEN - nbytes,
				"before IPA MHI was STARTED\n");
		return simple_read_from_buffer(ubuf, count, ppos,
			dbg_buff, nbytes);
	}
	if (ipa_mhi_client_ctx->state == IPA_MHI_STATE_SUSPENDED) {
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			"IPA MHI is suspended, cannot dump channel ctx array");
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			" from host -PCIe can be in D3 state\n");
		return simple_read_from_buffer(ubuf, count, ppos,
			dbg_buff, nbytes);
	}

	nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			"channel contex array - dump from host\n");
	nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			"***** UL channels *******\n");

	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->ul_channels[i];
		if (!channel->valid)
			continue;
		nbytes += ipa_mhi_print_host_channel_ctx_info(channel,
			&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes);
	}

	nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			"\n***** DL channels *******\n");

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->dl_channels[i];
		if (!channel->valid)
			continue;
		nbytes += ipa_mhi_print_host_channel_ctx_info(channel,
			&dbg_buff[nbytes], IPA_MHI_MAX_MSG_LEN - nbytes);
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

const struct file_operations ipa_mhi_stats_ops = {
	.read = ipa_mhi_debugfs_stats,
};

const struct file_operations ipa_mhi_uc_stats_ops = {
	.read = ipa_mhi_debugfs_uc_stats,
};

const struct file_operations ipa_mhi_dump_host_ch_ctx_ops = {
	.read = ipa_mhi_debugfs_dump_host_ch_ctx_arr,
};


static void ipa_mhi_debugfs_init(void)
{
	const mode_t read_only_mode = 0444;
	const mode_t read_write_mode = 0664;
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
		IPA_MHI_ERR("fail to create file uc_stats\n");
		goto fail;
	}

	file = debugfs_create_u32("use_ipadma", read_write_mode, dent,
		&ipa_mhi_client_ctx->use_ipadma);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file use_ipadma\n");
		goto fail;
	}

	file = debugfs_create_file("dump_host_channel_ctx_array",
		read_only_mode, dent, 0, &ipa_mhi_dump_host_ch_ctx_ops);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file dump_host_channel_ctx_arr\n");
		goto fail;
	}

	IPA_MHI_FUNC_EXIT();
	return;
fail:
	debugfs_remove_recursive(dent);
}

#else
static void ipa_mhi_debugfs_init(void) {}
static void ipa_mhi_debugfs_destroy(void) {}
#endif /* CONFIG_DEBUG_FS */

static union IpaHwMhiDlUlSyncCmdData_t ipa_cached_dl_ul_sync_info;

static void ipa_mhi_wq_notify_wakeup(struct work_struct *work);
static DECLARE_WORK(ipa_mhi_notify_wakeup_work, ipa_mhi_wq_notify_wakeup);

static void ipa_mhi_wq_notify_ready(struct work_struct *work);
static DECLARE_WORK(ipa_mhi_notify_ready_work, ipa_mhi_wq_notify_ready);

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
	if (ipa_mhi_client_ctx->wakeup_notified) {
		IPA_MHI_DBG("wakeup already called\n");
		return;
	}
	queue_work(ipa_mhi_client_ctx->wq, &ipa_mhi_notify_wakeup_work);
	ipa_mhi_client_ctx->wakeup_notified = true;
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

	IPA_MHI_DBG("%s\n", MHI_STATE_STR(ipa_mhi_client_ctx->state));
	spin_lock_irqsave(&ipa_mhi_client_ctx->state_lock, flags);
	ipa_mhi_client_ctx->rm_cons_state = IPA_MHI_RM_STATE_REQUESTED;
	if (ipa_mhi_client_ctx->state == IPA_MHI_STATE_STARTED) {
		ipa_mhi_client_ctx->rm_cons_state = IPA_MHI_RM_STATE_GRANTED;
		res = 0;
	} else if (ipa_mhi_client_ctx->state == IPA_MHI_STATE_SUSPENDED) {
		ipa_mhi_notify_wakeup();
		res = -EINPROGRESS;
	} else if (ipa_mhi_client_ctx->state ==
			IPA_MHI_STATE_SUSPEND_IN_PROGRESS) {
		/* wakeup event will be trigger after suspend finishes */
		ipa_mhi_client_ctx->trigger_wakeup = true;
		res = -EINPROGRESS;
	} else {
		res = -EINPROGRESS;
	}

	spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);
	IPA_MHI_DBG("EXIT with %d\n", res);
	return res;
}

static int ipa_mhi_rm_cons_release(void)
{
	unsigned long flags;

	IPA_MHI_FUNC_ENTRY();

	spin_lock_irqsave(&ipa_mhi_client_ctx->state_lock, flags);
	ipa_mhi_client_ctx->rm_cons_state = IPA_MHI_RM_STATE_RELEASED;
	complete_all(&ipa_mhi_client_ctx->rm_cons_comp);
	spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static void ipa_mhi_rm_prod_notify(void *user_data, enum ipa_rm_event event,
	unsigned long data)
{
	IPA_MHI_FUNC_ENTRY();

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		IPA_MHI_DBG("IPA_RM_RESOURCE_GRANTED\n");
		complete_all(&ipa_mhi_client_ctx->rm_prod_granted_comp);
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

/**
 * ipa_mhi_wq_notify_wakeup() - Notify MHI client on data available
 *
 * This function is called from IPA MHI workqueue to notify
 * MHI client driver on data available event.
 */
static void ipa_mhi_wq_notify_wakeup(struct work_struct *work)
{
	IPA_MHI_FUNC_ENTRY();
	ipa_mhi_client_ctx->cb_notify(ipa_mhi_client_ctx->cb_priv,
		IPA_MHI_EVENT_DATA_AVAILABLE, 0);
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
	ipa_mhi_client_ctx->cb_notify(ipa_mhi_client_ctx->cb_priv,
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
	queue_work(ipa_mhi_client_ctx->wq, &ipa_mhi_notify_ready_work);
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

	spin_lock_irqsave(&ipa_mhi_client_ctx->state_lock, flags);
	IPA_MHI_DBG("Current state: %s\n",
			MHI_STATE_STR(ipa_mhi_client_ctx->state));

	switch (ipa_mhi_client_ctx->state) {
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
			if (ipa_mhi_client_ctx->trigger_wakeup) {
				ipa_mhi_client_ctx->trigger_wakeup = false;
				ipa_mhi_notify_wakeup();
			}
			res = 0;
		} else if (new_state == IPA_MHI_STATE_STARTED) {
			ipa_mhi_client_ctx->wakeup_notified = false;
			ipa_mhi_client_ctx->trigger_wakeup = false;
			if (ipa_mhi_client_ctx->rm_cons_state ==
				IPA_MHI_RM_STATE_REQUESTED) {
				ipa_rm_notify_completion(
					IPA_RM_RESOURCE_GRANTED,
					IPA_RM_RESOURCE_MHI_CONS);
				ipa_mhi_client_ctx->rm_cons_state =
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
			if (ipa_mhi_client_ctx->trigger_wakeup) {
				ipa_mhi_client_ctx->trigger_wakeup = false;
				ipa_mhi_notify_wakeup();
			}
			res = 0;
		} else if (new_state == IPA_MHI_STATE_STARTED) {
			ipa_mhi_client_ctx->trigger_wakeup = false;
			ipa_mhi_client_ctx->wakeup_notified = false;
			if (ipa_mhi_client_ctx->rm_cons_state ==
				IPA_MHI_RM_STATE_REQUESTED) {
				ipa_rm_notify_completion(
					IPA_RM_RESOURCE_GRANTED,
					IPA_RM_RESOURCE_MHI_CONS);
				ipa_mhi_client_ctx->rm_cons_state =
					IPA_MHI_RM_STATE_GRANTED;
			}
			res = 0;
		}
		break;

	default:
		IPA_MHI_ERR("Invalid state %d\n", ipa_mhi_client_ctx->state);
		WARN_ON(1);
	}

	if (res)
		IPA_MHI_ERR("Invalid state change to %s\n",
						MHI_STATE_STR(new_state));
	else {
		IPA_MHI_DBG("New state change to %s\n",
						MHI_STATE_STR(new_state));
		ipa_mhi_client_ctx->state = new_state;
	}
	spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);
	return res;
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
	IPA_MHI_DBG("MHI state: %s\n",
			MHI_STATE_STR(ipa_mhi_client_ctx->state));
	spin_lock_irqsave(&ipa_mhi_client_ctx->state_lock, flags);
	if (ipa_mhi_client_ctx->state == IPA_MHI_STATE_SUSPENDED)
		ipa_mhi_notify_wakeup();
	else if (ipa_mhi_client_ctx->state ==
			IPA_MHI_STATE_SUSPEND_IN_PROGRESS)
		/* wakeup event will be triggered after suspend finishes */
		ipa_mhi_client_ctx->trigger_wakeup = true;

	spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);
	IPA_MHI_FUNC_EXIT();
}

static int ipa_mhi_request_prod(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	reinit_completion(&ipa_mhi_client_ctx->rm_prod_granted_comp);
	IPA_MHI_DBG("requesting mhi prod\n");
	res = ipa_rm_request_resource(IPA_RM_RESOURCE_MHI_PROD);
	if (res) {
		if (res != -EINPROGRESS) {
			IPA_MHI_ERR("failed to request mhi prod %d\n", res);
			return res;
		}
		res = wait_for_completion_timeout(
			&ipa_mhi_client_ctx->rm_prod_granted_comp,
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
 * ipa_mhi_start() - Start IPA MHI engine
 * @params: pcie addresses for MHI
 *
 * This function is called by MHI client driver on MHI engine start for
 * handling MHI accelerated channels. This function is called after
 * ipa_mhi_init() was called and can be called after MHI reset to restart MHI
 * engine. When this function returns device can move to M0 state.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_start(struct ipa_mhi_start_params *params)
{
	int res;
	struct ipa_mhi_init_engine init_params;

	IPA_MHI_FUNC_ENTRY();

	if (!params) {
		IPA_MHI_ERR("null args\n");
		return -EINVAL;
	}

	if (!ipa_mhi_client_ctx) {
		IPA_MHI_ERR("not initialized\n");
		return -EPERM;
	}

	res = ipa_mhi_set_state(IPA_MHI_STATE_STARTED);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state %d\n", res);
		return res;
	}

	ipa_mhi_client_ctx->host_ctrl_addr = params->host_ctrl_addr;
	ipa_mhi_client_ctx->host_data_addr = params->host_data_addr;
	ipa_mhi_client_ctx->channel_context_array_addr =
		params->channel_context_array_addr;
	ipa_mhi_client_ctx->event_context_array_addr =
		params->event_context_array_addr;
	IPA_MHI_DBG("host_ctrl_addr 0x%x\n",
			ipa_mhi_client_ctx->host_ctrl_addr);
	IPA_MHI_DBG("host_data_addr 0x%x\n",
			ipa_mhi_client_ctx->host_data_addr);
	IPA_MHI_DBG("channel_context_array_addr 0x%llx\n",
		ipa_mhi_client_ctx->channel_context_array_addr);
	IPA_MHI_DBG("event_context_array_addr 0x%llx\n",
		ipa_mhi_client_ctx->event_context_array_addr);

	if (ipa_pm_is_used()) {
		res = ipa_pm_activate_sync(ipa_mhi_client_ctx->pm_hdl);
		if (res) {
			IPA_MHI_ERR("failed activate client %d\n", res);
			goto fail_pm_activate;
		}
		res = ipa_pm_activate_sync(ipa_mhi_client_ctx->modem_pm_hdl);
		if (res) {
			IPA_MHI_ERR("failed activate modem client %d\n", res);
			goto fail_pm_activate_modem;
		}
	} else {
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
	}

	/* gsi params */
	init_params.gsi.first_ch_idx =
			ipa_mhi_client_ctx->first_ch_idx;
	/* uC params */
	init_params.uC.first_ch_idx =
			ipa_mhi_client_ctx->first_ch_idx;
	init_params.uC.first_er_idx =
			ipa_mhi_client_ctx->first_er_idx;
	init_params.uC.host_ctrl_addr = params->host_ctrl_addr;
	init_params.uC.host_data_addr = params->host_data_addr;
	init_params.uC.mmio_addr = ipa_mhi_client_ctx->mmio_addr;
	init_params.uC.msi = &ipa_mhi_client_ctx->msi;
	init_params.uC.ipa_cached_dl_ul_sync_info =
			&ipa_cached_dl_ul_sync_info;

	res = ipa_mhi_init_engine(&init_params);
	if (res) {
		IPA_MHI_ERR("IPA core failed to start MHI %d\n", res);
		goto fail_init_engine;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_init_engine:
	if (!ipa_pm_is_used())
		ipa_mhi_release_prod();
fail_request_prod:
	if (!ipa_pm_is_used())
		ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
			IPA_RM_RESOURCE_MHI_CONS);
fail_add_q6_mhi_dep:
	if (!ipa_pm_is_used())
		ipa_rm_delete_dependency(IPA_RM_RESOURCE_MHI_PROD,
			IPA_RM_RESOURCE_Q6_CONS);
fail_add_mhi_q6_dep:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(ipa_mhi_client_ctx->modem_pm_hdl);
fail_pm_activate_modem:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(ipa_mhi_client_ctx->pm_hdl);
fail_pm_activate:
	ipa_mhi_set_state(IPA_MHI_STATE_INITIALIZED);
	return res;
}

/**
 * ipa_mhi_get_channel_context() - Get corresponding channel context
 * @ep: IPA ep
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
		channels = ipa_mhi_client_ctx->ul_channels;
		max_channels = IPA_MHI_MAX_UL_CHANNELS;
	} else {
		channels = ipa_mhi_client_ctx->dl_channels;
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
	channels[ch_idx].index = ipa_mhi_client_ctx->total_channels++;
	channels[ch_idx].client = client;
	channels[ch_idx].state = IPA_HW_MHI_CHANNEL_STATE_INVALID;

	return &channels[ch_idx];
}

/**
 * ipa_mhi_get_channel_context_by_clnt_hdl() - Get corresponding channel
 * context
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
		if (ipa_mhi_client_ctx->ul_channels[ch_idx].valid &&
		ipa_get_ep_mapping(
			ipa_mhi_client_ctx->ul_channels[ch_idx].client)
				== clnt_hdl)
			return &ipa_mhi_client_ctx->ul_channels[ch_idx];
	}

	for (ch_idx = 0; ch_idx < IPA_MHI_MAX_DL_CHANNELS; ch_idx++) {
		if (ipa_mhi_client_ctx->dl_channels[ch_idx].valid &&
		ipa_get_ep_mapping(
			ipa_mhi_client_ctx->dl_channels[ch_idx].client)
				== clnt_hdl)
			return &ipa_mhi_client_ctx->dl_channels[ch_idx];
	}

	return NULL;
}

static void ipa_mhi_dump_ch_ctx(struct ipa_mhi_channel_ctx *channel)
{
	IPA_MHI_DBG("ch_id %d\n", channel->id);
	IPA_MHI_DBG("chstate 0x%x\n", channel->ch_ctx_host.chstate);
	IPA_MHI_DBG("brstmode 0x%x\n", channel->ch_ctx_host.brstmode);
	IPA_MHI_DBG("pollcfg 0x%x\n", channel->ch_ctx_host.pollcfg);
	IPA_MHI_DBG("chtype 0x%x\n", channel->ch_ctx_host.chtype);
	IPA_MHI_DBG("erindex 0x%x\n", channel->ch_ctx_host.erindex);
	IPA_MHI_DBG("rbase 0x%llx\n", channel->ch_ctx_host.rbase);
	IPA_MHI_DBG("rlen 0x%llx\n", channel->ch_ctx_host.rlen);
	IPA_MHI_DBG("rp 0x%llx\n", channel->ch_ctx_host.rp);
	IPA_MHI_DBG("wp 0x%llx\n", channel->ch_ctx_host.wp);
}

static void ipa_mhi_dump_ev_ctx(struct ipa_mhi_channel_ctx *channel)
{
	IPA_MHI_DBG("ch_id %d event id %d\n", channel->id,
		channel->ch_ctx_host.erindex);

	IPA_MHI_DBG("intmodc 0x%x\n", channel->ev_ctx_host.intmodc);
	IPA_MHI_DBG("intmodt 0x%x\n", channel->ev_ctx_host.intmodt);
	IPA_MHI_DBG("ertype 0x%x\n", channel->ev_ctx_host.ertype);
	IPA_MHI_DBG("msivec 0x%x\n", channel->ev_ctx_host.msivec);
	IPA_MHI_DBG("rbase 0x%llx\n", channel->ev_ctx_host.rbase);
	IPA_MHI_DBG("rlen 0x%llx\n", channel->ev_ctx_host.rlen);
	IPA_MHI_DBG("rp 0x%llx\n", channel->ev_ctx_host.rp);
	IPA_MHI_DBG("wp 0x%llx\n", channel->ev_ctx_host.wp);
}

static int ipa_mhi_read_ch_ctx(struct ipa_mhi_channel_ctx *channel)
{
	int res;

	res = ipa_mhi_read_write_host(IPA_MHI_DMA_FROM_HOST,
		&channel->ch_ctx_host, channel->channel_context_addr,
		sizeof(channel->ch_ctx_host));
	if (res) {
		IPA_MHI_ERR("ipa_mhi_read_write_host failed %d\n", res);
		return res;

	}
	ipa_mhi_dump_ch_ctx(channel);

	channel->event_context_addr =
		ipa_mhi_client_ctx->event_context_array_addr +
		channel->ch_ctx_host.erindex * sizeof(struct ipa_mhi_ev_ctx);
	IPA_MHI_DBG("ch %d event_context_addr 0x%llx\n", channel->id,
		channel->event_context_addr);

	res = ipa_mhi_read_write_host(IPA_MHI_DMA_FROM_HOST,
		&channel->ev_ctx_host, channel->event_context_addr,
		sizeof(channel->ev_ctx_host));
	if (res) {
		IPA_MHI_ERR("ipa_mhi_read_write_host failed %d\n", res);
		return res;

	}
	ipa_mhi_dump_ev_ctx(channel);

	return 0;
}

static void ipa_mhi_gsi_ev_err_cb(struct gsi_evt_err_notify *notify)
{
	struct ipa_mhi_channel_ctx *channel = notify->user_data;

	IPA_MHI_ERR("channel id=%d client=%d state=%d\n",
		channel->id, channel->client, channel->state);
	switch (notify->evt_id) {
	case GSI_EVT_OUT_OF_BUFFERS_ERR:
		IPA_MHI_ERR("Received GSI_EVT_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_EVT_OUT_OF_RESOURCES_ERR:
		IPA_MHI_ERR("Received GSI_EVT_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR:
		IPA_MHI_ERR("Received GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_EVT_EVT_RING_EMPTY_ERR:
		IPA_MHI_ERR("Received GSI_EVT_EVT_RING_EMPTY_ERR\n");
		break;
	default:
		IPA_MHI_ERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	IPA_MHI_ERR("err_desc=0x%x\n", notify->err_desc);
	ipa_assert();
}

static void ipa_mhi_gsi_ch_err_cb(struct gsi_chan_err_notify *notify)
{
	struct ipa_mhi_channel_ctx *channel = notify->chan_user_data;

	IPA_MHI_ERR("channel id=%d client=%d state=%d\n",
		channel->id, channel->client, channel->state);
	switch (notify->evt_id) {
	case GSI_CHAN_INVALID_TRE_ERR:
		IPA_MHI_ERR("Received GSI_CHAN_INVALID_TRE_ERR\n");
		break;
	case GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR:
		IPA_MHI_ERR("Received GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_BUFFERS_ERR:
		IPA_MHI_ERR("Received GSI_CHAN_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_RESOURCES_ERR:
		IPA_MHI_ERR("Received GSI_CHAN_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR:
		IPA_MHI_ERR("Received GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_CHAN_HWO_1_ERR:
		IPA_MHI_ERR("Received GSI_CHAN_HWO_1_ERR\n");
		break;
	default:
		IPA_MHI_ERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	IPA_MHI_ERR("err_desc=0x%x\n", notify->err_desc);
	ipa_assert();
}


static bool ipa_mhi_gsi_channel_empty(struct ipa_mhi_channel_ctx *channel)
{
	IPA_MHI_FUNC_ENTRY();

	if (!channel->stop_in_proc) {
		IPA_MHI_DBG("Channel is not in STOP_IN_PROC\n");
		return true;
	}

	if (ipa_mhi_stop_gsi_channel(channel->client) == true) {
		channel->stop_in_proc = false;
		return true;
	}

	return false;
}

/**
 * ipa_mhi_wait_for_ul_empty_timeout() - wait for pending packets in uplink
 * @msecs: timeout to wait
 *
 * This function will poll until there are no packets pending in uplink channels
 * or timeout occurred.
 *
 * Return code: true - no pending packets in uplink channels
 *		false - timeout occurred
 */
static bool ipa_mhi_wait_for_ul_empty_timeout(unsigned int msecs)
{
	unsigned long jiffies_timeout = msecs_to_jiffies(msecs);
	unsigned long jiffies_start = jiffies;
	bool empty = false;
	int i;

	IPA_MHI_FUNC_ENTRY();
	while (!empty) {
		empty = true;
		for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
			if (!ipa_mhi_client_ctx->ul_channels[i].valid)
				continue;
			if (ipa_get_transport_type() ==
			    IPA_TRANSPORT_TYPE_GSI)
				empty &= ipa_mhi_gsi_channel_empty(
					&ipa_mhi_client_ctx->ul_channels[i]);
			else
				empty &= ipa_mhi_sps_channel_empty(
				ipa_mhi_client_ctx->ul_channels[i].client);
		}

		if (time_after(jiffies, jiffies_start + jiffies_timeout)) {
			IPA_MHI_DBG("finished waiting for UL empty\n");
			break;
		}

		if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI &&
		    IPA_MHI_MAX_UL_CHANNELS == 1)
			usleep_range(IPA_GSI_CHANNEL_STOP_SLEEP_MIN_USEC,
			IPA_GSI_CHANNEL_STOP_SLEEP_MAX_USEC);
	}

	IPA_MHI_DBG("IPA UL is %s\n", (empty) ? "empty" : "not empty");

	IPA_MHI_FUNC_EXIT();
	return empty;
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
		if (!ipa_mhi_client_ctx->ul_channels[i].valid)
			continue;
		req.source_pipe_bitmask |= 1 << ipa_get_ep_mapping(
				ipa_mhi_client_ctx->ul_channels[i].client);
	}
	if (throttle_source) {
		req.throttle_source_valid = 1;
		req.throttle_source = 1;
	}
	IPA_MHI_DBG("req_id=0x%x src_pipe_btmk=0x%x throt_src=%d\n",
		req.request_id, req.source_pipe_bitmask,
		req.throttle_source);
	res = ipa_qmi_enable_force_clear_datapath_send(&req);
	if (res) {
		IPA_MHI_ERR(
			"ipa_qmi_enable_force_clear_datapath_send failed %d\n"
				, res);
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
	res = ipa_qmi_disable_force_clear_datapath_send(&req);
	if (res) {
		IPA_MHI_ERR(
			"ipa_qmi_disable_force_clear_datapath_send failed %d\n"
				, res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static void ipa_mhi_set_holb_on_dl_channels(bool enable,
	struct ipa_ep_cfg_holb old_holb[])
{
	int i;
	struct ipa_ep_cfg_holb ep_holb;
	int ep_idx;
	int res;

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa_mhi_client_ctx->dl_channels[i].valid)
			continue;
		if (ipa_mhi_client_ctx->dl_channels[i].state ==
			IPA_HW_MHI_CHANNEL_STATE_INVALID)
			continue;
		ep_idx = ipa_get_ep_mapping(
			ipa_mhi_client_ctx->dl_channels[i].client);
		if (-1 == ep_idx) {
			IPA_MHI_ERR("Client %u is not mapped\n",
				ipa_mhi_client_ctx->dl_channels[i].client);
			ipa_assert();
			return;
		}
		memset(&ep_holb, 0, sizeof(ep_holb));
		if (enable) {
			ipa_get_holb(ep_idx, &old_holb[i]);
			ep_holb.en = 1;
			ep_holb.tmr_val = 0;
		} else {
			ep_holb = old_holb[i];
		}
		res = ipa_cfg_ep_holb(ep_idx, &ep_holb);
		if (res) {
			IPA_MHI_ERR("ipa_cfg_ep_holb failed %d\n", res);
			ipa_assert();
			return;
		}
	}
}

static int ipa_mhi_suspend_gsi_channel(struct ipa_mhi_channel_ctx *channel)
{
	int clnt_hdl;
	int res;

	IPA_MHI_FUNC_ENTRY();
	clnt_hdl = ipa_get_ep_mapping(channel->client);
	if (clnt_hdl < 0)
		return -EFAULT;

	res = ipa_stop_gsi_channel(clnt_hdl);
	if (res != 0 && res != -GSI_STATUS_AGAIN &&
	    res != -GSI_STATUS_TIMED_OUT) {
		IPA_MHI_ERR("GSI stop channel failed %d\n", res);
		return -EFAULT;
	}

	/* check if channel was stopped completely */
	if (res)
		channel->stop_in_proc = true;

	IPA_MHI_DBG("GSI channel is %s\n", (channel->stop_in_proc) ?
		"STOP_IN_PROC" : "STOP");

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_reset_ul_channel(struct ipa_mhi_channel_ctx *channel)
{
	int res;
	bool empty;
	struct ipa_ep_cfg_holb old_ep_holb[IPA_MHI_MAX_DL_CHANNELS];

	IPA_MHI_FUNC_ENTRY();
	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa_mhi_suspend_gsi_channel(channel);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_suspend_gsi_channel failed %d\n",
				 res);
			return res;
		}
	} else {
		res = ipa_uc_mhi_reset_channel(channel->index);
		if (res) {
			IPA_MHI_ERR("ipa_uc_mhi_reset_channel failed %d\n",
				res);
			return res;
		}
	}

	empty = ipa_mhi_wait_for_ul_empty_timeout(
			IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);
	if (!empty) {
		IPA_MHI_DBG("%s not empty\n",
			(ipa_get_transport_type() ==
				IPA_TRANSPORT_TYPE_GSI) ? "GSI" : "BAM");
		res = ipa_mhi_enable_force_clear(
				ipa_mhi_client_ctx->qmi_req_id, false);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_enable_force_clear failed %d\n",
				res);
			ipa_assert();
			return res;
		}

		if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
			empty = ipa_mhi_wait_for_ul_empty_timeout(
				IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);

			IPA_MHI_DBG("empty=%d\n", empty);
		} else {
			/* enable packet drop on all DL channels */
			ipa_mhi_set_holb_on_dl_channels(true, old_ep_holb);
			ipa_generate_tag_process();
			/* disable packet drop on all DL channels */
			ipa_mhi_set_holb_on_dl_channels(false, old_ep_holb);

			res = ipa_disable_sps_pipe(channel->client);
			if (res) {
				IPA_MHI_ERR("sps_pipe_disable fail %d\n", res);
				ipa_assert();
				return res;
			}
		}

		res =
		ipa_mhi_disable_force_clear(ipa_mhi_client_ctx->qmi_req_id);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_disable_force_clear failed %d\n",
				res);
			ipa_assert();
			return res;
		}
		ipa_mhi_client_ctx->qmi_req_id++;
	}

	res = ipa_mhi_reset_channel_internal(channel->client);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_reset_ul_channel_internal failed %d\n"
				, res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();

	return 0;
}

static int ipa_mhi_reset_dl_channel(struct ipa_mhi_channel_ctx *channel)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa_mhi_suspend_gsi_channel(channel);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_suspend_gsi_channel failed %d\n"
					, res);
			return res;
		}

		res = ipa_mhi_reset_channel_internal(channel->client);
		if (res) {
			IPA_MHI_ERR(
				"ipa_mhi_reset_ul_channel_internal failed %d\n"
				, res);
			return res;
		}
	} else {
		res = ipa_mhi_reset_channel_internal(channel->client);
		if (res) {
			IPA_MHI_ERR(
				"ipa_mhi_reset_ul_channel_internal failed %d\n"
				, res);
			return res;
		}

		res = ipa_uc_mhi_reset_channel(channel->index);
		if (res) {
			IPA_MHI_ERR("ipa_uc_mhi_reset_channel failed %d\n",
				res);
			ipa_mhi_start_channel_internal(channel->client);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
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

	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
				offsetof(struct ipa_mhi_ch_ctx, chstate),
				sizeof(((struct ipa_mhi_ch_ctx *)0)->chstate));
		if (res) {
			IPA_MHI_ERR("ipa_mhi_read_write_host failed %d\n", res);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

/**
 * ipa_mhi_connect_pipe() - Connect pipe to IPA and start corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in, u32 *clnt_hdl)
{
	int res;
	unsigned long flags;
	struct ipa_mhi_channel_ctx *channel = NULL;

	IPA_MHI_FUNC_ENTRY();

	if (!in || !clnt_hdl) {
		IPA_MHI_ERR("NULL args\n");
		return -EINVAL;
	}

	if (in->sys.client >= IPA_CLIENT_MAX) {
		IPA_MHI_ERR("bad param client:%d\n", in->sys.client);
		return -EINVAL;
	}

	if (!IPA_CLIENT_IS_MHI(in->sys.client)) {
		IPA_MHI_ERR(
			"Invalid MHI client, client: %d\n", in->sys.client);
		return -EINVAL;
	}

	IPA_MHI_DBG("channel=%d\n", in->channel_id);

	spin_lock_irqsave(&ipa_mhi_client_ctx->state_lock, flags);
	if (!ipa_mhi_client_ctx ||
			ipa_mhi_client_ctx->state != IPA_MHI_STATE_STARTED) {
		IPA_MHI_ERR("IPA MHI was not started\n");
		spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);

	channel = ipa_mhi_get_channel_context(in->sys.client, in->channel_id);
	if (!channel) {
		IPA_MHI_ERR("ipa_mhi_get_channel_context failed\n");
		return -EINVAL;
	}

	if (channel->state != IPA_HW_MHI_CHANNEL_STATE_INVALID &&
	    channel->state != IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
		IPA_MHI_ERR("Invalid channel state %d\n", channel->state);
		return -EFAULT;
	}

	channel->channel_context_addr =
		ipa_mhi_client_ctx->channel_context_array_addr +
			channel->id * sizeof(struct ipa_mhi_ch_ctx);

	/* for event context address index needs to read from host */

	IPA_MHI_DBG("client %d channelIndex %d channelID %d, state %d\n",
		channel->client, channel->index, channel->id, channel->state);
	IPA_MHI_DBG("channel_context_addr 0x%llx cached_gsi_evt_ring_hdl %lu\n",
		channel->channel_context_addr,
		channel->cached_gsi_evt_ring_hdl);

	IPA_ACTIVE_CLIENTS_INC_EP(in->sys.client);

	mutex_lock(&mhi_client_general_mutex);
	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		struct ipa_mhi_connect_params_internal internal;

		IPA_MHI_DBG("reading ch/ev context from host\n");
		res = ipa_mhi_read_ch_ctx(channel);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_read_ch_ctx failed %d\n", res);
			goto fail_start_channel;
		}

		internal.channel_id = in->channel_id;
		internal.sys = &in->sys;
		internal.start.gsi.state = channel->state;
		internal.start.gsi.msi = &ipa_mhi_client_ctx->msi;
		internal.start.gsi.ev_ctx_host = &channel->ev_ctx_host;
		internal.start.gsi.event_context_addr =
				channel->event_context_addr;
		internal.start.gsi.ch_ctx_host = &channel->ch_ctx_host;
		internal.start.gsi.channel_context_addr =
				channel->channel_context_addr;
		internal.start.gsi.ch_err_cb = ipa_mhi_gsi_ch_err_cb;
		internal.start.gsi.channel = (void *)channel;
		internal.start.gsi.ev_err_cb = ipa_mhi_gsi_ev_err_cb;
		internal.start.gsi.assert_bit40 =
				ipa_mhi_client_ctx->assert_bit40;
		internal.start.gsi.mhi = &channel->ch_scratch.mhi;
		internal.start.gsi.cached_gsi_evt_ring_hdl =
				&channel->cached_gsi_evt_ring_hdl;
		internal.start.gsi.evchid = channel->index;

		res = ipa_connect_mhi_pipe(&internal, clnt_hdl);
		if (res) {
			IPA_MHI_ERR("ipa_connect_mhi_pipe failed %d\n", res);
			goto fail_connect_pipe;
		}
		channel->state = IPA_HW_MHI_CHANNEL_STATE_RUN;
		channel->brstmode_enabled =
				channel->ch_scratch.mhi.burst_mode_enabled;

		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
				offsetof(struct ipa_mhi_ch_ctx, chstate),
				sizeof(((struct ipa_mhi_ch_ctx *)0)->chstate));
		if (res) {
			IPA_MHI_ERR("ipa_mhi_read_write_host failed\n");
			mutex_unlock(&mhi_client_general_mutex);
			IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);
			return res;

		}
	} else {
		struct ipa_mhi_connect_params_internal internal;

		internal.channel_id = in->channel_id;
		internal.sys = &in->sys;
		internal.start.uC.index = channel->index;
		internal.start.uC.id = channel->id;
		internal.start.uC.state = channel->state;
		res = ipa_connect_mhi_pipe(&internal, clnt_hdl);
		if (res) {
			IPA_MHI_ERR("ipa_connect_mhi_pipe failed %d\n", res);
			goto fail_connect_pipe;
		}
		channel->state = IPA_HW_MHI_CHANNEL_STATE_RUN;
	}
	mutex_unlock(&mhi_client_general_mutex);

	if (!in->sys.keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);

	IPA_MHI_FUNC_EXIT();

	return 0;
fail_connect_pipe:
	mutex_unlock(&mhi_client_general_mutex);
	ipa_mhi_reset_channel(channel);
fail_start_channel:
	IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);
	return -EPERM;
}

/**
 * ipa_mhi_disconnect_pipe() - Disconnect pipe from IPA and reset corresponding
 * MHI channel
 * @clnt_hdl: client handle for this pipe
 *
 * This function is called by MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to uC/GSI to reset corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_disconnect_pipe(u32 clnt_hdl)
{
	int res;
	enum ipa_client_type client;
	static struct ipa_mhi_channel_ctx *channel;

	IPA_MHI_FUNC_ENTRY();

	if (!ipa_mhi_client_ctx) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	client = ipa_get_client_mapping(clnt_hdl);

	if (!IPA_CLIENT_IS_MHI(client)) {
		IPA_MHI_ERR("invalid IPA MHI client, client: %d\n", client);
		return -EINVAL;
	}

	channel = ipa_mhi_get_channel_context_by_clnt_hdl(clnt_hdl);
	if (!channel) {
		IPA_MHI_ERR("invalid clnt index\n");
		return -EINVAL;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(ipa_get_client_mapping(clnt_hdl));

	res = ipa_mhi_reset_channel(channel);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_reset_channel failed %d\n", res);
		goto fail_reset_channel;
	}

	mutex_lock(&mhi_client_general_mutex);
	res = ipa_disconnect_mhi_pipe(clnt_hdl);
	if (res) {
		IPA_MHI_ERR(
			"IPA core driver failed to disconnect the pipe hdl %d, res %d"
				, clnt_hdl, res);
		goto fail_disconnect_pipe;
	}
	mutex_unlock(&mhi_client_general_mutex);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa_get_client_mapping(clnt_hdl));

	IPA_MHI_DBG("client (ep: %d) disconnected\n", clnt_hdl);
	IPA_MHI_FUNC_EXIT();
	return 0;
fail_disconnect_pipe:
	mutex_unlock(&mhi_client_general_mutex);
fail_reset_channel:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa_get_client_mapping(clnt_hdl));
	return res;
}

static int ipa_mhi_wait_for_cons_release(void)
{
	unsigned long flags;
	int res;

	IPA_MHI_FUNC_ENTRY();
	reinit_completion(&ipa_mhi_client_ctx->rm_cons_comp);
	spin_lock_irqsave(&ipa_mhi_client_ctx->state_lock, flags);
	if (ipa_mhi_client_ctx->rm_cons_state != IPA_MHI_RM_STATE_GRANTED) {
		spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);

	res = wait_for_completion_timeout(
		&ipa_mhi_client_ctx->rm_cons_comp,
		msecs_to_jiffies(IPA_MHI_RM_TIMEOUT_MSEC));
	if (res == 0) {
		IPA_MHI_ERR("timeout release mhi cons\n");
		return -ETIME;
	}
	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_suspend_channels(struct ipa_mhi_channel_ctx *channels,
	int max_channels)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < max_channels; i++) {
		if (!channels[i].valid)
			continue;
		if (channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_RUN)
			continue;
		IPA_MHI_DBG("suspending channel %d\n",
			channels[i].id);

		if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI)
			res = ipa_mhi_suspend_gsi_channel(
				&channels[i]);
		else
			res = ipa_uc_mhi_suspend_channel(
				channels[i].index);

		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_SUSPEND;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_stop_event_update_channels(
		struct ipa_mhi_channel_ctx *channels, int max_channels)
{
	int i;
	int res;

	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI)
		return 0;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < max_channels; i++) {
		if (!channels[i].valid)
			continue;
		if (channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("stop update event channel %d\n",
			channels[i].id);
		res = ipa_uc_mhi_stop_event_update_channel(
			channels[i].index);
		if (res) {
			IPA_MHI_ERR("failed stop event channel %d error %d\n",
				i, res);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static bool ipa_mhi_check_pending_packets_from_host(void)
{
	int i;
	int res;
	struct ipa_mhi_channel_ctx *channel;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->ul_channels[i];
		if (!channel->valid)
			continue;

		res = ipa_mhi_query_ch_info(channel->client,
				&channel->ch_info);
		if (res) {
			IPA_MHI_ERR("gsi_query_channel_info failed\n");
			return true;
		}
		res = ipa_mhi_read_ch_ctx(channel);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_read_ch_ctx failed %d\n", res);
			return true;
		}

		if (channel->ch_info.rp != channel->ch_ctx_host.wp) {
			IPA_MHI_DBG("There are pending packets from host\n");
			IPA_MHI_DBG("device rp 0x%llx host 0x%llx\n",
				channel->ch_info.rp, channel->ch_ctx_host.wp);

			return true;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return false;
}

static int ipa_mhi_resume_channels(bool LPTransitionRejected,
		struct ipa_mhi_channel_ctx *channels, int max_channels)
{
	int i;
	int res;
	struct ipa_mhi_channel_ctx *channel;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < max_channels; i++) {
		if (!channels[i].valid)
			continue;
		if (channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		channel = &channels[i];
		IPA_MHI_DBG("resuming channel %d\n", channel->id);

		res = ipa_mhi_resume_channels_internal(channel->client,
			LPTransitionRejected, channel->brstmode_enabled,
			channel->ch_scratch, channel->index);

		if (res) {
			IPA_MHI_ERR("failed to resume channel %d error %d\n",
				i, res);
			return res;
		}

		channel->stop_in_proc = false;
		channel->state = IPA_HW_MHI_CHANNEL_STATE_RUN;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

/**
 * ipa_mhi_suspend_ul() - Suspend MHI accelerated up link channels
 * @force:
 *	false: in case of data pending in IPA, MHI channels will not be
 *		suspended and function will fail.
 *	true:  in case of data pending in IPA, make sure no further access from
 *		IPA to PCIe is possible. In this case suspend cannot fail.
 *
 *
 * This function is called by MHI client driver on MHI suspend.
 * This function is called after MHI channel was started.
 * When this function returns device can move to M1/M2/M3/D3cold state.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
static int ipa_mhi_suspend_ul(bool force, bool *empty, bool *force_clear)
{
	int res;

	*force_clear = false;

	res = ipa_mhi_suspend_channels(ipa_mhi_client_ctx->ul_channels,
		IPA_MHI_MAX_UL_CHANNELS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_suspend_ul_channels failed %d\n", res);
		goto fail_suspend_ul_channel;
	}

	*empty = ipa_mhi_wait_for_ul_empty_timeout(
			IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);

	if (!*empty) {
		if (force) {
			res = ipa_mhi_enable_force_clear(
				ipa_mhi_client_ctx->qmi_req_id, false);
			if (res) {
				IPA_MHI_ERR("failed to enable force clear\n");
				ipa_assert();
				return res;
			}
			*force_clear = true;
			IPA_MHI_DBG("force clear datapath enabled\n");

			*empty = ipa_mhi_wait_for_ul_empty_timeout(
				IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);
			IPA_MHI_DBG("empty=%d\n", *empty);
			if (!*empty && ipa_get_transport_type()
				== IPA_TRANSPORT_TYPE_GSI) {
				IPA_MHI_ERR("Failed to suspend UL channels\n");
				if (ipa_mhi_client_ctx->test_mode) {
					res = -EAGAIN;
					goto fail_suspend_ul_channel;
				}

				ipa_assert();
			}
		} else {
			IPA_MHI_DBG("IPA not empty\n");
			res = -EAGAIN;
			goto fail_suspend_ul_channel;
		}
	}

	if (*force_clear) {
		res =
		ipa_mhi_disable_force_clear(ipa_mhi_client_ctx->qmi_req_id);
		if (res) {
			IPA_MHI_ERR("failed to disable force clear\n");
			ipa_assert();
			return res;
		}
		IPA_MHI_DBG("force clear datapath disabled\n");
		ipa_mhi_client_ctx->qmi_req_id++;
	}

	if (!force && ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		if (ipa_mhi_check_pending_packets_from_host()) {
			res = -EAGAIN;
			goto fail_suspend_ul_channel;
		}
	}

	res = ipa_mhi_stop_event_update_channels(
		ipa_mhi_client_ctx->ul_channels, IPA_MHI_MAX_UL_CHANNELS);
	if (res) {
		IPA_MHI_ERR(
			"ipa_mhi_stop_event_update_ul_channels failed %d\n",
			res);
		goto fail_suspend_ul_channel;
	}

	return 0;

fail_suspend_ul_channel:
	return res;
}

static bool ipa_mhi_has_open_aggr_frame(void)
{
	struct ipa_mhi_channel_ctx *channel;
	int i;

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->dl_channels[i];

		if (!channel->valid)
			continue;

		if (ipa_has_open_aggr_frame(channel->client))
			return true;
	}

	return false;
}

static void ipa_mhi_update_host_ch_state(bool update_rp)
{
	int i;
	int res;
	struct ipa_mhi_channel_ctx *channel;

	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->ul_channels[i];
		if (!channel->valid)
			continue;

		if (update_rp) {
			res = ipa_mhi_query_ch_info(channel->client,
				&channel->ch_info);
			if (res) {
				IPA_MHI_ERR("gsi_query_channel_info failed\n");
				ipa_assert();
				return;
			}

			res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
				&channel->ch_info.rp,
				channel->channel_context_addr +
					offsetof(struct ipa_mhi_ch_ctx, rp),
				sizeof(channel->ch_info.rp));
			if (res) {
				IPA_MHI_ERR("ipa_mhi_read_write_host failed\n");
				ipa_assert();
				return;
			}
		}

		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
				offsetof(struct ipa_mhi_ch_ctx, chstate),
			sizeof(((struct ipa_mhi_ch_ctx *)0)->chstate));
		if (res) {
			IPA_MHI_ERR("ipa_mhi_read_write_host failed\n");
			ipa_assert();
			return;
		}
		IPA_MHI_DBG("Updated UL CH=%d state to %s on host\n",
			i, MHI_CH_STATE_STR(channel->state));
	}

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa_mhi_client_ctx->dl_channels[i];
		if (!channel->valid)
			continue;

		if (update_rp) {
			res = ipa_mhi_query_ch_info(channel->client,
				&channel->ch_info);
			if (res) {
				IPA_MHI_ERR("gsi_query_channel_info failed\n");
				ipa_assert();
				return;
			}

			res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
				&channel->ch_info.rp,
				channel->channel_context_addr +
					offsetof(struct ipa_mhi_ch_ctx, rp),
				sizeof(channel->ch_info.rp));
			if (res) {
				IPA_MHI_ERR("ipa_mhi_read_write_host failed\n");
				ipa_assert();
				return;
			}
		}

		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
			offsetof(struct ipa_mhi_ch_ctx, chstate),
			sizeof(((struct ipa_mhi_ch_ctx *)0)->chstate));
		if (res) {
			IPA_MHI_ERR("ipa_mhi_read_write_host failed\n");
			ipa_assert();
			return;
		}
		IPA_MHI_DBG("Updated DL CH=%d state to %s on host\n",
			i, MHI_CH_STATE_STR(channel->state));
	}
}

static int ipa_mhi_suspend_dl(bool force)
{
	int res;

	res = ipa_mhi_suspend_channels(ipa_mhi_client_ctx->dl_channels,
		IPA_MHI_MAX_DL_CHANNELS);
	if (res) {
		IPA_MHI_ERR(
			"ipa_mhi_suspend_channels for dl failed %d\n", res);
		goto fail_suspend_dl_channel;
	}

	res = ipa_mhi_stop_event_update_channels
			(ipa_mhi_client_ctx->dl_channels,
			IPA_MHI_MAX_DL_CHANNELS);
	if (res) {
		IPA_MHI_ERR("failed to stop event update on DL %d\n", res);
		goto fail_stop_event_update_dl_channel;
	}

	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		if (ipa_mhi_has_open_aggr_frame()) {
			IPA_MHI_DBG("There is an open aggr frame\n");
			if (force) {
				ipa_mhi_client_ctx->trigger_wakeup = true;
			} else {
				res = -EAGAIN;
				goto fail_stop_event_update_dl_channel;
			}
		}
	}

	return 0;

fail_stop_event_update_dl_channel:
		ipa_mhi_resume_channels(true,
				ipa_mhi_client_ctx->dl_channels,
				IPA_MHI_MAX_DL_CHANNELS);
fail_suspend_dl_channel:
		return res;
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
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_suspend(bool force)
{
	int res;
	bool empty;
	bool force_clear;

	IPA_MHI_FUNC_ENTRY();

	res = ipa_mhi_set_state(IPA_MHI_STATE_SUSPEND_IN_PROGRESS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		return res;
	}

	res = ipa_mhi_suspend_dl(force);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_suspend_dl failed %d\n", res);
		goto fail_suspend_dl_channel;
	}

	usleep_range(IPA_MHI_SUSPEND_SLEEP_MIN, IPA_MHI_SUSPEND_SLEEP_MAX);

	res = ipa_mhi_suspend_ul(force, &empty, &force_clear);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_suspend_ul failed %d\n", res);
		goto fail_suspend_ul_channel;
	}

	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI)
		ipa_mhi_update_host_ch_state(true);

	/*
	 * hold IPA clocks and release them after all
	 * IPA RM resource are released to make sure tag process will not start
	 */
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	if (ipa_pm_is_used()) {
		res = ipa_pm_deactivate_sync(ipa_mhi_client_ctx->pm_hdl);
		if (res) {
			IPA_MHI_ERR("fail to deactivate client %d\n", res);
			goto fail_deactivate_pm;
		}
		res = ipa_pm_deactivate_sync(ipa_mhi_client_ctx->modem_pm_hdl);
		if (res) {
			IPA_MHI_ERR("fail to deactivate client %d\n", res);
			goto fail_deactivate_modem_pm;
		}
	} else {
		IPA_MHI_DBG("release prod\n");
		res = ipa_mhi_release_prod();
		if (res) {
			IPA_MHI_ERR("ipa_mhi_release_prod failed %d\n", res);
			goto fail_release_prod;
		}

		IPA_MHI_DBG("wait for cons release\n");
		res = ipa_mhi_wait_for_cons_release();
		if (res) {
			IPA_MHI_ERR("ipa_mhi_wait_for_cons_release failed\n");
			goto fail_release_cons;
		}
	}
	usleep_range(IPA_MHI_SUSPEND_SLEEP_MIN, IPA_MHI_SUSPEND_SLEEP_MAX);

	if (!empty)
		ipa_set_tag_process_before_gating(false);

	res = ipa_mhi_set_state(IPA_MHI_STATE_SUSPENDED);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		goto fail_release_cons;
	}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPA_MHI_FUNC_EXIT();
	return 0;

fail_release_cons:
	if (!ipa_pm_is_used())
		ipa_mhi_request_prod();
fail_release_prod:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(ipa_mhi_client_ctx->modem_pm_hdl);
fail_deactivate_modem_pm:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(ipa_mhi_client_ctx->pm_hdl);
fail_deactivate_pm:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
fail_suspend_ul_channel:
	ipa_mhi_resume_channels(true, ipa_mhi_client_ctx->ul_channels,
		IPA_MHI_MAX_UL_CHANNELS);
	if (force_clear) {
		if (
		ipa_mhi_disable_force_clear(ipa_mhi_client_ctx->qmi_req_id)) {
			IPA_MHI_ERR("failed to disable force clear\n");
			ipa_assert();
		}
		IPA_MHI_DBG("force clear datapath disabled\n");
		ipa_mhi_client_ctx->qmi_req_id++;
	}
fail_suspend_dl_channel:
	ipa_mhi_resume_channels(true, ipa_mhi_client_ctx->dl_channels,
		IPA_MHI_MAX_DL_CHANNELS);
	ipa_mhi_set_state(IPA_MHI_STATE_STARTED);
	return res;
}

/**
 * ipa_mhi_resume() - Resume MHI accelerated channels
 *
 * This function is called by MHI client driver on MHI resume.
 * This function is called after MHI channel was suspended.
 * When this function returns device can move to M0 state.
 * This function is doing the following:
 *	- Send command to uC/GSI to resume corresponding MHI channel
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

	res = ipa_mhi_set_state(IPA_MHI_STATE_RESUME_IN_PROGRESS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		return res;
	}

	if (ipa_mhi_client_ctx->rm_cons_state == IPA_MHI_RM_STATE_REQUESTED) {
		/* resume all DL channels */
		res = ipa_mhi_resume_channels(false,
				ipa_mhi_client_ctx->dl_channels,
				IPA_MHI_MAX_DL_CHANNELS);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_resume_dl_channels failed %d\n",
				res);
			goto fail_resume_dl_channels;
		}
		dl_channel_resumed = true;

		ipa_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
			IPA_RM_RESOURCE_MHI_CONS);
		ipa_mhi_client_ctx->rm_cons_state = IPA_MHI_RM_STATE_GRANTED;
	}

	if (ipa_pm_is_used()) {
		res = ipa_pm_activate_sync(ipa_mhi_client_ctx->pm_hdl);
		if (res) {
			IPA_MHI_ERR("fail to activate client %d\n", res);
			goto fail_pm_activate;
		}
		ipa_pm_activate_sync(ipa_mhi_client_ctx->modem_pm_hdl);
		if (res) {
			IPA_MHI_ERR("fail to activate client %d\n", res);
			goto fail_pm_activate_modem;
		}
	} else {
		res = ipa_mhi_request_prod();
		if (res) {
			IPA_MHI_ERR("ipa_mhi_request_prod failed %d\n", res);
			goto fail_request_prod;
		}
	}

	/* resume all UL channels */
	res = ipa_mhi_resume_channels(false,
					ipa_mhi_client_ctx->ul_channels,
					IPA_MHI_MAX_UL_CHANNELS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_resume_ul_channels failed %d\n", res);
		goto fail_resume_ul_channels;
	}

	if (!dl_channel_resumed) {
		res = ipa_mhi_resume_channels(false,
					ipa_mhi_client_ctx->dl_channels,
					IPA_MHI_MAX_DL_CHANNELS);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_resume_dl_channels failed %d\n",
				res);
			goto fail_resume_dl_channels2;
		}
	}

	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI)
		ipa_mhi_update_host_ch_state(false);

	res = ipa_mhi_set_state(IPA_MHI_STATE_STARTED);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_set_state failed %d\n", res);
		goto fail_set_state;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_set_state:
	ipa_mhi_suspend_channels(ipa_mhi_client_ctx->dl_channels,
		IPA_MHI_MAX_DL_CHANNELS);
fail_resume_dl_channels2:
	ipa_mhi_suspend_channels(ipa_mhi_client_ctx->ul_channels,
		IPA_MHI_MAX_UL_CHANNELS);
fail_resume_ul_channels:
	if (!ipa_pm_is_used())
		ipa_mhi_release_prod();
fail_request_prod:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(ipa_mhi_client_ctx->modem_pm_hdl);
fail_pm_activate_modem:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(ipa_mhi_client_ctx->pm_hdl);
fail_pm_activate:
	ipa_mhi_suspend_channels(ipa_mhi_client_ctx->dl_channels,
		IPA_MHI_MAX_DL_CHANNELS);
fail_resume_dl_channels:
	ipa_mhi_set_state(IPA_MHI_STATE_SUSPENDED);
	return res;
}


static int  ipa_mhi_destroy_channels(struct ipa_mhi_channel_ctx *channels,
	int num_of_channels)
{
	struct ipa_mhi_channel_ctx *channel;
	int i, res;
	u32 clnt_hdl;

	for (i = 0; i < num_of_channels; i++) {
		channel = &channels[i];
		if (!channel->valid)
			continue;
		if (channel->state == IPA_HW_MHI_CHANNEL_STATE_INVALID)
			continue;
		if (channel->state != IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
			clnt_hdl = ipa_get_ep_mapping(channel->client);
			IPA_MHI_DBG("disconnect pipe (ep: %d)\n", clnt_hdl);
			res = ipa_mhi_disconnect_pipe(clnt_hdl);
			if (res) {
				IPA_MHI_ERR(
					"failed to disconnect pipe %d, err %d\n"
					, clnt_hdl, res);
				goto fail;
			}
		}
		res = ipa_mhi_destroy_channel(channel->client);
		if (res) {
			IPA_MHI_ERR(
				"ipa_mhi_destroy_channel failed %d"
					, res);
			goto fail;
		}
	}
	return 0;
fail:
	return res;
}

/**
 * ipa_mhi_destroy_all_channels() - Destroy MHI IPA channels
 *
 * This function is called by IPA MHI client driver on MHI reset to destroy all
 * IPA MHI channels.
 */
int ipa_mhi_destroy_all_channels(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	/* reset all UL and DL acc channels and its accociated event rings */
	res = ipa_mhi_destroy_channels(ipa_mhi_client_ctx->ul_channels,
		IPA_MHI_MAX_UL_CHANNELS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_destroy_channels(ul_channels) failed %d\n",
			res);
		return -EPERM;
	}
	IPA_MHI_DBG("All UL channels are disconnected\n");

	res = ipa_mhi_destroy_channels(ipa_mhi_client_ctx->dl_channels,
		IPA_MHI_MAX_DL_CHANNELS);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_destroy_channels(dl_channels) failed %d\n",
			res);
		return -EPERM;
	}
	IPA_MHI_DBG("All DL channels are disconnected\n");

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static void ipa_mhi_debugfs_destroy(void)
{
	debugfs_remove_recursive(dent);
}

static void ipa_mhi_delete_rm_resources(void)
{
	int res;

	if (ipa_mhi_client_ctx->state != IPA_MHI_STATE_INITIALIZED  &&
		ipa_mhi_client_ctx->state != IPA_MHI_STATE_READY) {

		IPA_MHI_DBG("release prod\n");
		res = ipa_mhi_release_prod();
		if (res) {
			IPA_MHI_ERR("ipa_mhi_release_prod failed %d\n",
				res);
			goto fail;
		}
		IPA_MHI_DBG("wait for cons release\n");
		res = ipa_mhi_wait_for_cons_release();
		if (res) {
			IPA_MHI_ERR("ipa_mhi_wait_for_cons_release%d\n",
				res);
			goto fail;
		}

		usleep_range(IPA_MHI_SUSPEND_SLEEP_MIN,
			IPA_MHI_SUSPEND_SLEEP_MAX);

		IPA_MHI_DBG("deleate dependency Q6_PROD->MHI_CONS\n");
		res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
			IPA_RM_RESOURCE_MHI_CONS);
		if (res) {
			IPA_MHI_ERR(
				"Error deleting dependency %d->%d, res=%d\n",
				IPA_RM_RESOURCE_Q6_PROD,
				IPA_RM_RESOURCE_MHI_CONS,
				res);
			goto fail;
		}
		IPA_MHI_DBG("deleate dependency MHI_PROD->Q6_CONS\n");
		res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_MHI_PROD,
			IPA_RM_RESOURCE_Q6_CONS);
		if (res) {
			IPA_MHI_ERR(
				"Error deleting dependency %d->%d, res=%d\n",
				IPA_RM_RESOURCE_MHI_PROD,
				IPA_RM_RESOURCE_Q6_CONS,
				res);
			goto fail;
		}
	}

	res = ipa_rm_delete_resource(IPA_RM_RESOURCE_MHI_PROD);
	if (res) {
		IPA_MHI_ERR("Error deleting resource %d, res=%d\n",
			IPA_RM_RESOURCE_MHI_PROD, res);
		goto fail;
	}

	res = ipa_rm_delete_resource(IPA_RM_RESOURCE_MHI_CONS);
	if (res) {
		IPA_MHI_ERR("Error deleting resource %d, res=%d\n",
			IPA_RM_RESOURCE_MHI_CONS, res);
		goto fail;
	}

	return;
fail:
	ipa_assert();
}

static void ipa_mhi_deregister_pm(void)
{
	ipa_pm_deactivate_sync(ipa_mhi_client_ctx->pm_hdl);
	ipa_pm_deregister(ipa_mhi_client_ctx->pm_hdl);
	ipa_mhi_client_ctx->pm_hdl = ~0;

	ipa_pm_deactivate_sync(ipa_mhi_client_ctx->modem_pm_hdl);
	ipa_pm_deregister(ipa_mhi_client_ctx->modem_pm_hdl);
	ipa_mhi_client_ctx->modem_pm_hdl = ~0;
}

/**
 * ipa_mhi_destroy() - Destroy MHI IPA
 *
 * This function is called by MHI client driver on MHI reset to destroy all IPA
 * MHI resources.
 * When this function returns ipa_mhi can re-initialize.
 */
void ipa_mhi_destroy(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	if (!ipa_mhi_client_ctx) {
		IPA_MHI_DBG("IPA MHI was not initialized, already destroyed\n");
		return;
	}

	ipa_deregister_client_callback(IPA_CLIENT_MHI_PROD);

	/* reset all UL and DL acc channels and its accociated event rings */
	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa_mhi_destroy_all_channels();
		if (res) {
			IPA_MHI_ERR("ipa_mhi_destroy_all_channels failed %d\n",
				res);
			goto fail;
		}
	}
	IPA_MHI_DBG("All channels are disconnected\n");

	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_SPS) {
		IPA_MHI_DBG("cleanup uC MHI\n");
		ipa_uc_mhi_cleanup();
	}

	if (ipa_pm_is_used())
		ipa_mhi_deregister_pm();
	else
		ipa_mhi_delete_rm_resources();

	ipa_dma_destroy();
	ipa_mhi_debugfs_destroy();
	destroy_workqueue(ipa_mhi_client_ctx->wq);
	kfree(ipa_mhi_client_ctx);
	ipa_mhi_client_ctx = NULL;
	IPA_MHI_DBG("IPA MHI was reset, ready for re-init\n");

	IPA_MHI_FUNC_EXIT();
	return;
fail:
	ipa_assert();
}

static void ipa_mhi_pm_cb(void *p, enum ipa_pm_cb_event event)
{
	unsigned long flags;

	IPA_MHI_FUNC_ENTRY();

	if (event != IPA_PM_REQUEST_WAKEUP) {
		IPA_MHI_ERR("Unexpected event %d\n", event);
		WARN_ON(1);
		return;
	}

	IPA_MHI_DBG("%s\n", MHI_STATE_STR(ipa_mhi_client_ctx->state));
	spin_lock_irqsave(&ipa_mhi_client_ctx->state_lock, flags);
	if (ipa_mhi_client_ctx->state == IPA_MHI_STATE_SUSPENDED) {
		ipa_mhi_notify_wakeup();
	} else if (ipa_mhi_client_ctx->state ==
		IPA_MHI_STATE_SUSPEND_IN_PROGRESS) {
		/* wakeup event will be trigger after suspend finishes */
		ipa_mhi_client_ctx->trigger_wakeup = true;
	}
	spin_unlock_irqrestore(&ipa_mhi_client_ctx->state_lock, flags);
	IPA_MHI_DBG("EXIT");
}

static int ipa_mhi_register_pm(void)
{
	int res;
	struct ipa_pm_register_params params;

	memset(&params, 0, sizeof(params));
	params.name = "MHI";
	params.callback = ipa_mhi_pm_cb;
	params.group = IPA_PM_GROUP_DEFAULT;
	res = ipa_pm_register(&params, &ipa_mhi_client_ctx->pm_hdl);
	if (res) {
		IPA_MHI_ERR("fail to register with PM %d\n", res);
		return res;
	}

	res = ipa_pm_associate_ipa_cons_to_client(ipa_mhi_client_ctx->pm_hdl,
		IPA_CLIENT_MHI_CONS);
	if (res) {
		IPA_MHI_ERR("fail to associate cons with PM %d\n", res);
		goto fail_pm_cons;
	}

	res = ipa_pm_set_throughput(ipa_mhi_client_ctx->pm_hdl, 1000);
	if (res) {
		IPA_MHI_ERR("fail to set perf profile to PM %d\n", res);
		goto fail_pm_cons;
	}

	/* create a modem client for clock scaling */
	memset(&params, 0, sizeof(params));
	params.name = "MODEM (MHI)";
	params.group = IPA_PM_GROUP_MODEM;
	params.skip_clk_vote = true;
	res = ipa_pm_register(&params, &ipa_mhi_client_ctx->modem_pm_hdl);
	if (res) {
		IPA_MHI_ERR("fail to register with PM %d\n", res);
		goto fail_pm_cons;
	}

	return 0;

fail_pm_cons:
	ipa_pm_deregister(ipa_mhi_client_ctx->pm_hdl);
	ipa_mhi_client_ctx->pm_hdl = ~0;
	return res;
}

static int ipa_mhi_create_rm_resources(void)
{
	int res;
	struct ipa_rm_create_params mhi_prod_params;
	struct ipa_rm_create_params mhi_cons_params;
	struct ipa_rm_perf_profile profile;

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

	memset(&profile, 0, sizeof(profile));
	profile.max_supported_bandwidth_mbps = 1000;
	res = ipa_rm_set_perf_profile(IPA_RM_RESOURCE_MHI_PROD, &profile);
	if (res) {
		IPA_MHI_ERR("fail to set profile to MHI_PROD\n");
		goto fail_perf_rm_prod;
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

	memset(&profile, 0, sizeof(profile));
	profile.max_supported_bandwidth_mbps = 1000;
	res = ipa_rm_set_perf_profile(IPA_RM_RESOURCE_MHI_CONS, &profile);
	if (res) {
		IPA_MHI_ERR("fail to set profile to MHI_CONS\n");
		goto fail_perf_rm_cons;
	}
fail_perf_rm_cons:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_MHI_CONS);
fail_create_rm_cons:
fail_perf_rm_prod:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_MHI_PROD);
fail_create_rm_prod:
	return res;
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

	IPA_MHI_FUNC_ENTRY();

	if (!params) {
		IPA_MHI_ERR("null args\n");
		return -EINVAL;
	}

	if (!params->notify) {
		IPA_MHI_ERR("null notify function\n");
		return -EINVAL;
	}

	if (ipa_mhi_client_ctx) {
		IPA_MHI_ERR("already initialized\n");
		return -EPERM;
	}

	IPA_MHI_DBG("notify = %pF priv = %pK\n", params->notify, params->priv);
	IPA_MHI_DBG("msi: addr_lo = 0x%x addr_hi = 0x%x\n",
		params->msi.addr_low, params->msi.addr_hi);
	IPA_MHI_DBG("msi: data = 0x%x mask = 0x%x\n",
		params->msi.data, params->msi.mask);
	IPA_MHI_DBG("mmio_addr = 0x%x\n", params->mmio_addr);
	IPA_MHI_DBG("first_ch_idx = 0x%x\n", params->first_ch_idx);
	IPA_MHI_DBG("first_er_idx = 0x%x\n", params->first_er_idx);
	IPA_MHI_DBG("assert_bit40=%d\n", params->assert_bit40);
	IPA_MHI_DBG("test_mode=%d\n", params->test_mode);

	/* Initialize context */
	ipa_mhi_client_ctx = kzalloc(sizeof(*ipa_mhi_client_ctx), GFP_KERNEL);
	if (!ipa_mhi_client_ctx) {
		res = -EFAULT;
		goto fail_alloc_ctx;
	}

	ipa_mhi_client_ctx->state = IPA_MHI_STATE_INITIALIZED;
	ipa_mhi_client_ctx->cb_notify = params->notify;
	ipa_mhi_client_ctx->cb_priv = params->priv;
	ipa_mhi_client_ctx->rm_cons_state = IPA_MHI_RM_STATE_RELEASED;
	init_completion(&ipa_mhi_client_ctx->rm_prod_granted_comp);
	spin_lock_init(&ipa_mhi_client_ctx->state_lock);
	init_completion(&ipa_mhi_client_ctx->rm_cons_comp);
	ipa_mhi_client_ctx->msi = params->msi;
	ipa_mhi_client_ctx->mmio_addr = params->mmio_addr;
	ipa_mhi_client_ctx->first_ch_idx = params->first_ch_idx;
	ipa_mhi_client_ctx->first_er_idx = params->first_er_idx;
	ipa_mhi_client_ctx->qmi_req_id = 0;
	ipa_mhi_client_ctx->use_ipadma = true;
	ipa_mhi_client_ctx->assert_bit40 = !!params->assert_bit40;
	ipa_mhi_client_ctx->test_mode = params->test_mode;

	ipa_mhi_client_ctx->wq = create_singlethread_workqueue("ipa_mhi_wq");
	if (!ipa_mhi_client_ctx->wq) {
		IPA_MHI_ERR("failed to create workqueue\n");
		res = -EFAULT;
		goto fail_create_wq;
	}

	res = ipa_dma_init();
	if (res) {
		IPA_MHI_ERR("failed to init ipa dma %d\n", res);
		goto fail_dma_init;
	}

	if (ipa_pm_is_used())
		res = ipa_mhi_register_pm();
	else
		res = ipa_mhi_create_rm_resources();
	if (res) {
		IPA_MHI_ERR("failed to create RM resources\n");
		res = -EFAULT;
		goto fail_rm;
	}

	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_GSI) {
		ipa_mhi_set_state(IPA_MHI_STATE_READY);
	} else {
		/* Initialize uC interface */
		ipa_uc_mhi_init(ipa_mhi_uc_ready_cb,
			ipa_mhi_uc_wakeup_request_cb);
		if (ipa_uc_state_check() == 0)
			ipa_mhi_set_state(IPA_MHI_STATE_READY);
	}

	ipa_register_client_callback(&ipa_mhi_set_lock_unlock, NULL,
					IPA_CLIENT_MHI_PROD);

	/* Initialize debugfs */
	ipa_mhi_debugfs_init();

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_rm:
	ipa_dma_destroy();
fail_dma_init:
	destroy_workqueue(ipa_mhi_client_ctx->wq);
fail_create_wq:
	kfree(ipa_mhi_client_ctx);
	ipa_mhi_client_ctx = NULL;
fail_alloc_ctx:
	return res;
}

static void ipa_mhi_cache_dl_ul_sync_info(
	struct ipa_config_req_msg_v01 *config_req)
{
	ipa_cached_dl_ul_sync_info.params.isDlUlSyncEnabled = true;
	ipa_cached_dl_ul_sync_info.params.UlAccmVal =
		(config_req->ul_accumulation_time_limit_valid) ?
		config_req->ul_accumulation_time_limit : 0;
	ipa_cached_dl_ul_sync_info.params.ulMsiEventThreshold =
		(config_req->ul_msi_event_threshold_valid) ?
		config_req->ul_msi_event_threshold : 0;
	ipa_cached_dl_ul_sync_info.params.dlMsiEventThreshold =
		(config_req->dl_msi_event_threshold_valid) ?
		config_req->dl_msi_event_threshold : 0;
}

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

	if (ipa_get_transport_type() != IPA_TRANSPORT_TYPE_GSI) {
		ipa_mhi_cache_dl_ul_sync_info(config_req);
		if (ipa_mhi_client_ctx &&
				ipa_mhi_client_ctx->state !=
						IPA_MHI_STATE_INITIALIZED)
			ipa_uc_mhi_send_dl_ul_sync_info(
				&ipa_cached_dl_ul_sync_info);
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

int ipa_mhi_is_using_dma(bool *flag)
{
	IPA_MHI_FUNC_ENTRY();

	if (!ipa_mhi_client_ctx) {
		IPA_MHI_ERR("not initialized\n");
		return -EPERM;
	}

	*flag = ipa_mhi_client_ctx->use_ipadma ? true : false;

	IPA_MHI_FUNC_EXIT();
	return 0;
}
EXPORT_SYMBOL(ipa_mhi_is_using_dma);

const char *ipa_mhi_get_state_str(int state)
{
	return MHI_STATE_STR(state);
}
EXPORT_SYMBOL(ipa_mhi_get_state_str);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA MHI client driver");
