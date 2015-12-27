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
#include <linux/msm_gsi.h>
#include "ipa_i.h"
#include "ipa_qmi_service.h"

#define IPA_MHI_DRV_NAME "ipa_mhi"
#define IPA_MHI_DBG(fmt, args...) \
	pr_debug(IPA_MHI_DRV_NAME " %s:%d " fmt, \
		 __func__, __LINE__, ## args)
#define IPA_MHI_ERR(fmt, args...) \
	pr_err(IPA_MHI_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define IPA_MHI_FUNC_ENTRY() \
	IPA_MHI_DBG("ENTRY\n")
#define IPA_MHI_FUNC_EXIT() \
	IPA_MHI_DBG("EXIT\n")

#define IPA_MHI_GSI_ER_START 10
#define IPA_MHI_GSI_ER_END 16

#define IPA_MHI_RM_TIMEOUT_MSEC 10000

#define IPA_MHI_CH_EMPTY_TIMEOUT_MSEC 5

#define IPA_MHI_MAX_UL_CHANNELS 1
#define IPA_MHI_MAX_DL_CHANNELS 1

#if (IPA_MHI_MAX_UL_CHANNELS + IPA_MHI_MAX_DL_CHANNELS) > \
	(IPA_MHI_GSI_ER_END - IPA_MHI_GSI_ER_START)
#error not enought event rings for MHI
#endif

#define IPA_MHI_SUSPEND_SLEEP_MIN 900
#define IPA_MHI_SUSPEND_SLEEP_MAX 1100

/* bit #40 in address should be asserted for MHI transfers over pcie */
#define IPA_MHI_HOST_ADDR_COND(addr) \
		((ipa3_mhi_ctx->assert_bit40)?(IPA_MHI_HOST_ADDR(addr)):(addr))

enum ipa3_mhi_state {
	IPA_MHI_STATE_INITIALIZED,
	IPA_MHI_STATE_READY,
	IPA_MHI_STATE_STARTED,
	IPA_MHI_STATE_SUSPEND_IN_PROGRESS,
	IPA_MHI_STATE_SUSPENDED,
	IPA_MHI_STATE_RESUME_IN_PROGRESS,
	IPA_MHI_STATE_MAX
};

static char *ipa3_mhi_state_str[] = {
	__stringify(IPA_MHI_STATE_INITIALIZED),
	__stringify(IPA_MHI_STATE_READY),
	__stringify(IPA_MHI_STATE_STARTED),
	__stringify(IPA_MHI_STATE_SUSPEND_IN_PROGRESS),
	__stringify(IPA_MHI_STATE_SUSPENDED),
	__stringify(IPA_MHI_STATE_RESUME_IN_PROGRESS),
};

#define MHI_STATE_STR(state) \
	(((state) >= 0 && (state) < IPA_MHI_STATE_MAX) ? \
		ipa3_mhi_state_str[(state)] : \
		"INVALID")

enum ipa_mhi_dma_dir {
	IPA_MHI_DMA_TO_HOST,
	IPA_MHI_DMA_FROM_HOST,
};


struct ipa3_mhi_ch_ctx {
	u32 chstate;
	u32 chtype;
	u32 erindex;
	u64 rbase;
	u64 rlen;
	u64 rp;
	u64 wp;
} __packed;

struct ipa3_mhi_ev_ctx {
	u32 intmodc:16;
	u32 intmodt:16;
	u32 ertype;
	u32 msivec;
	u64 rbase;
	u64 rlen;
	u64 rp;
	u64 wp;
} __packed;

/**
 * struct ipa3_mhi_channel_ctx - MHI Channel context
 * @valid: entry is valid
 * @id: MHI channel ID
 * @index: channel handle for uC
 * @ep: IPA endpoint context
 * @state: Channel state
 * @stop_in_proc: flag to indicate if channel was stopped completely
 * @ch_info: information about channel occupancy
 * @channel_context_addr : the channel context address in host address space
 * @ch_ctx_host: MHI Channel context
 * @event_context_addr: the event context address in host address space
 * @ev_ctx_host: MHI event context
 * @cached_gsi_evt_ring_hdl: GSI channel event ring handle
 */
struct ipa3_mhi_channel_ctx {
	bool valid;
	u8 id;
	u8 index;
	struct ipa3_ep_context *ep;
	enum ipa3_hw_mhi_channel_states state;
	bool stop_in_proc;
	struct gsi_chan_info ch_info;
	u64 channel_context_addr;
	struct ipa3_mhi_ch_ctx ch_ctx_host;
	u64 event_context_addr;
	struct ipa3_mhi_ev_ctx ev_ctx_host;
	unsigned long cached_gsi_evt_ring_hdl;
};

enum ipa3_mhi_rm_state {
	IPA_MHI_RM_STATE_RELEASED,
	IPA_MHI_RM_STATE_REQUESTED,
	IPA_MHI_RM_STATE_GRANTED,
	IPA_MHI_RM_STATE_MAX
};

/**
 * struct ipa3_mhi_ctx - IPA MHI context
 * @state: IPA MHI state
 * @state_lock: lock for state read/write operations
 * @msi: Message Signaled Interrupts parameters
 * @mmio_addr: MHI MMIO physical address
 * @first_ch_idx: First channel ID for hardware accelerated channels.
 * @first_er_idx: First event ring ID for hardware accelerated channels.
 * @host_ctrl_addr: Base address of MHI control data structures
 * @host_data_addr: Base address of MHI data buffers
 * @channel_context_addr: channel context array address in host address space
 * @event_context_addr: event context array address in host address space
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
 * @use_ipadma: use IPADMA to access host space
 * @assert_bit40: should assert bit 40 in order to access hots space.
 *	if PCIe iATU is configured then not need to assert bit40
 * @ test_mode: flag to indicate if IPA MHI is in unit test mode
 */
struct ipa3_mhi_ctx {
	enum ipa3_mhi_state state;
	spinlock_t state_lock;
	struct ipa_mhi_msi_info msi;
	u32 mmio_addr;
	u32 first_ch_idx;
	u32 first_er_idx;
	u32 host_ctrl_addr;
	u32 host_data_addr;
	u64 channel_context_array_addr;
	u64 event_context_array_addr;
	mhi_client_cb cb_notify;
	void *cb_priv;
	struct ipa3_mhi_channel_ctx ul_channels[IPA_MHI_MAX_UL_CHANNELS];
	struct ipa3_mhi_channel_ctx dl_channels[IPA_MHI_MAX_DL_CHANNELS];
	u32 total_channels;
	struct completion rm_prod_granted_comp;
	enum ipa3_mhi_rm_state rm_cons_state;
	struct completion rm_cons_comp;
	bool trigger_wakeup;
	bool wakeup_notified;
	struct workqueue_struct *wq;
	u32 qmi_req_id;
	u32 use_ipadma;
	bool assert_bit40;
	bool test_mode;
};

static struct ipa3_mhi_ctx *ipa3_mhi_ctx;

static void ipa3_mhi_wq_notify_wakeup(struct work_struct *work);
static DECLARE_WORK(ipa_mhi_notify_wakeup_work, ipa3_mhi_wq_notify_wakeup);

static void ipa3_mhi_wq_notify_ready(struct work_struct *work);
static DECLARE_WORK(ipa_mhi_notify_ready_work, ipa3_mhi_wq_notify_ready);

static union IpaHwMhiDlUlSyncCmdData_t ipa3_cached_dl_ul_sync_info;

#ifdef CONFIG_DEBUG_FS
#define IPA_MHI_MAX_MSG_LEN 512
static char dbg_buff[IPA_MHI_MAX_MSG_LEN];
static struct dentry *dent;

static char *ipa3_mhi_channel_state_str[] = {
	__stringify(IPA_HW_MHI_CHANNEL_STATE_DISABLE),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_ENABLE),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_RUN),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_SUSPEND),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_STOP),
	__stringify(IPA_HW_MHI_CHANNEL_STATE_ERROR),
};

#define MHI_CH_STATE_STR(state) \
	(((state) >= 0 && (state) <= IPA_HW_MHI_CHANNEL_STATE_ERROR) ? \
	ipa3_mhi_channel_state_str[(state)] : \
	"INVALID")

static int ipa_mhi_read_write_host(enum ipa_mhi_dma_dir dir, void *dev_addr,
	u64 host_addr, int size)
{
	struct ipa3_mem_buffer mem;
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (ipa3_mhi_ctx->use_ipadma) {
		host_addr = IPA_MHI_HOST_ADDR_COND(host_addr);

		mem.size = size;
		mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size,
			&mem.phys_base, GFP_KERNEL);
		if (!mem.base) {
			IPAERR("dma_alloc_coherent failed, DMA buff size %d\n",
				mem.size);
			return -ENOMEM;
		}

		if (dir == IPA_MHI_DMA_FROM_HOST) {
			res = ipa_dma_sync_memcpy(mem.phys_base, host_addr,
				size);
			if (res) {
				IPAERR("ipa_dma_sync_memcpy from host fail%d\n",
					res);
				goto fail_memcopy;
			}
			memcpy(dev_addr, mem.base, size);
		} else {
			memcpy(mem.base, dev_addr, size);
			res = ipa_dma_sync_memcpy(host_addr, mem.phys_base,
				size);
			if (res) {
				IPAERR("ipa_dma_sync_memcpy to host fail %d\n",
					res);
				goto fail_memcopy;
			}
		}
		dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base,
			mem.phys_base);
	} else {
		void *host_ptr;

		if (!ipa3_mhi_ctx->test_mode)
			host_ptr = ioremap(host_addr, size);
		else
			host_ptr = phys_to_virt(host_addr);
		if (!host_ptr) {
			IPAERR("ioremap failed for 0x%llx\n", host_addr);
			return -EFAULT;
		}
		if (dir == IPA_MHI_DMA_FROM_HOST)
			memcpy(dev_addr, host_ptr, size);
		else
			memcpy(host_ptr, dev_addr, size);
		if (!ipa3_mhi_ctx->test_mode)
			iounmap(host_ptr);
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_memcopy:
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base,
			mem.phys_base);
	return res;
}

static int ipa3_mhi_print_channel_info(struct ipa3_mhi_channel_ctx *channel,
	char *buff, int len)
{
	int nbytes = 0;

	if (channel->valid) {
		nbytes += scnprintf(&buff[nbytes],
			len - nbytes,
			"channel idx=%d ch_id=%d client=%d state=%s\n",
			channel->index, channel->id, channel->ep->client,
			MHI_CH_STATE_STR(channel->state));

		nbytes += scnprintf(&buff[nbytes],
			len - nbytes,
			"	stop_in_proc=%d gsi_chan_hdl=%ld\n",
			channel->stop_in_proc, channel->ep->gsi_chan_hdl);

		nbytes += scnprintf(&buff[nbytes],
			len - nbytes,
			"	ch_ctx=%llx\n",
			channel->channel_context_addr);

		nbytes += scnprintf(&buff[nbytes],
			len - nbytes,
			"	gsi_evt_ring_hdl=%ld ev_ctx=%llx\n",
			channel->ep->gsi_evt_ring_hdl,
			channel->event_context_addr);
	}
	return nbytes;
}

static int ipa3_mhi_print_host_channel_ctx_info(
		struct ipa3_mhi_channel_ctx *channel, char *buff, int len)
{
	int res, nbytes = 0;
	struct ipa3_mhi_ch_ctx ch_ctx_host;

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

static ssize_t ipa3_mhi_debugfs_stats(struct file *file,
	char __user *ubuf,
	size_t count,
	loff_t *ppos)
{
	int nbytes = 0;
	int i;
	struct ipa3_mhi_channel_ctx *channel;

	nbytes += scnprintf(&dbg_buff[nbytes],
		IPA_MHI_MAX_MSG_LEN - nbytes,
		"IPA MHI state: %s\n", MHI_STATE_STR(ipa3_mhi_ctx->state));

	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa3_mhi_ctx->ul_channels[i];
		nbytes += ipa3_mhi_print_channel_info(channel,
			&dbg_buff[nbytes], IPA_MHI_MAX_MSG_LEN - nbytes);
	}

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa3_mhi_ctx->dl_channels[i];
		nbytes += ipa3_mhi_print_channel_info(channel,
			&dbg_buff[nbytes], IPA_MHI_MAX_MSG_LEN - nbytes);
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa3_mhi_debugfs_uc_stats(struct file *file,
	char __user *ubuf,
	size_t count,
	loff_t *ppos)
{
	int nbytes = 0;

	nbytes += ipa3_uc_mhi_print_stats(dbg_buff, IPA_MHI_MAX_MSG_LEN);
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa3_mhi_debugfs_dump_host_ch_ctx_arr(struct file *file,
	char __user *ubuf,
	size_t count,
	loff_t *ppos)
{
	int i, nbytes = 0;
	struct ipa3_mhi_channel_ctx *channel;

	if (ipa3_mhi_ctx->state == IPA_MHI_STATE_INITIALIZED ||
	    ipa3_mhi_ctx->state == IPA_MHI_STATE_READY) {
		nbytes += scnprintf(&dbg_buff[nbytes],
		IPA_MHI_MAX_MSG_LEN - nbytes,
			"Cannot dump host channel context ");
		nbytes += scnprintf(&dbg_buff[nbytes],
				IPA_MHI_MAX_MSG_LEN - nbytes,
				"before IPA MHI was STARTED\n");
		return simple_read_from_buffer(ubuf, count, ppos,
			dbg_buff, nbytes);
	}
	if (ipa3_mhi_ctx->state == IPA_MHI_STATE_SUSPENDED) {
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
		channel = &ipa3_mhi_ctx->ul_channels[i];
		if (!channel->valid)
			continue;
		nbytes += ipa3_mhi_print_host_channel_ctx_info(channel,
			&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes);
	}

	nbytes += scnprintf(&dbg_buff[nbytes],
			IPA_MHI_MAX_MSG_LEN - nbytes,
			"\n***** DL channels *******\n");

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa3_mhi_ctx->dl_channels[i];
		if (!channel->valid)
			continue;
		nbytes += ipa3_mhi_print_host_channel_ctx_info(channel,
			&dbg_buff[nbytes], IPA_MHI_MAX_MSG_LEN - nbytes);
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

const struct file_operations ipa3_mhi_stats_ops = {
	.read = ipa3_mhi_debugfs_stats,
};

const struct file_operations ipa3_mhi_uc_stats_ops = {
	.read = ipa3_mhi_debugfs_uc_stats,
};

const struct file_operations ipa3_mhi_dump_host_ch_ctx_ops = {
	.read = ipa3_mhi_debugfs_dump_host_ch_ctx_arr,
};


static void ipa3_mhi_debugfs_init(void)
{
	const mode_t read_only_mode = S_IRUSR | S_IRGRP | S_IROTH;
	const mode_t read_write_mode = S_IRUSR | S_IRGRP | S_IROTH |
		S_IWUSR | S_IWGRP;
	struct dentry *file;

	IPA_MHI_FUNC_ENTRY();

	dent = debugfs_create_dir("ipa_mhi", 0);
	if (IS_ERR(dent)) {
		IPA_MHI_ERR("fail to create folder ipa_mhi\n");
		return;
	}

	file = debugfs_create_file("stats", read_only_mode, dent,
		0, &ipa3_mhi_stats_ops);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file stats\n");
		goto fail;
	}

	file = debugfs_create_file("uc_stats", read_only_mode, dent,
		0, &ipa3_mhi_uc_stats_ops);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file uc_stats\n");
		goto fail;
	}

	file = debugfs_create_u32("use_ipadma", read_write_mode, dent,
		&ipa3_mhi_ctx->use_ipadma);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file use_ipadma\n");
		goto fail;
	}

	file = debugfs_create_file("dump_host_channel_ctx_array",
		read_only_mode, dent, 0, &ipa3_mhi_dump_host_ch_ctx_ops);
	if (!file || IS_ERR(file)) {
		IPA_MHI_ERR("fail to create file dump_host_channel_ctx_arr\n");
		goto fail;
	}

	IPA_MHI_FUNC_EXIT();
	return;
fail:
	debugfs_remove_recursive(dent);
}

static void ipa3_mhi_debugfs_destroy(void)
{
	debugfs_remove_recursive(dent);
}

#else
static void ipa3_mhi_debugfs_init(void) {}
static void ipa3_mhi_debugfs_destroy(void) {}
#endif /* CONFIG_DEBUG_FS */


static void ipa3_mhi_cache_dl_ul_sync_info(
	struct ipa_config_req_msg_v01 *config_req)
{
	ipa3_cached_dl_ul_sync_info.params.isDlUlSyncEnabled = true;
	ipa3_cached_dl_ul_sync_info.params.UlAccmVal =
		(config_req->ul_accumulation_time_limit_valid) ?
		config_req->ul_accumulation_time_limit : 0;
	ipa3_cached_dl_ul_sync_info.params.ulMsiEventThreshold =
		(config_req->ul_msi_event_threshold_valid) ?
		config_req->ul_msi_event_threshold : 0;
	ipa3_cached_dl_ul_sync_info.params.dlMsiEventThreshold =
		(config_req->dl_msi_event_threshold_valid) ?
		config_req->dl_msi_event_threshold : 0;
}

/**
 * ipa3_mhi_wq_notify_wakeup() - Notify MHI client on data available
 *
 * This function is called from IPA MHI workqueue to notify
 * MHI client driver on data available event.
 */
static void ipa3_mhi_wq_notify_wakeup(struct work_struct *work)
{
	IPA_MHI_FUNC_ENTRY();
	ipa3_mhi_ctx->cb_notify(ipa3_mhi_ctx->cb_priv,
		IPA_MHI_EVENT_DATA_AVAILABLE, 0);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa3_mhi_notify_wakeup() - Schedule work to notify data available
 *
 * This function will schedule a work to notify data available event.
 * In case this function is called more than once, only one notification will
 * be sent to MHI client driver. No further notifications will be sent until
 * IPA MHI state will become STARTED.
 */
static void ipa3_mhi_notify_wakeup(void)
{
	IPA_MHI_FUNC_ENTRY();
	if (ipa3_mhi_ctx->wakeup_notified) {
		IPADBG("wakeup already called\n");
		return;
	}
	queue_work(ipa3_mhi_ctx->wq, &ipa_mhi_notify_wakeup_work);
	ipa3_mhi_ctx->wakeup_notified = true;
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa3_mhi_wq_notify_ready() - Notify MHI client on ready
 *
 * This function is called from IPA MHI workqueue to notify
 * MHI client driver on ready event when IPA uC is loaded
 */
static void ipa3_mhi_wq_notify_ready(struct work_struct *work)
{
	IPA_MHI_FUNC_ENTRY();
	ipa3_mhi_ctx->cb_notify(ipa3_mhi_ctx->cb_priv,
		IPA_MHI_EVENT_READY, 0);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa3_mhi_notify_ready() - Schedule work to notify ready
 *
 * This function will schedule a work to notify ready event.
 */
static void ipa3_mhi_notify_ready(void)
{
	IPA_MHI_FUNC_ENTRY();
	queue_work(ipa3_mhi_ctx->wq, &ipa_mhi_notify_ready_work);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa3_mhi_set_state() - Set new state to IPA MHI
 * @state: new state
 *
 * Sets a new state to IPA MHI if possible according to IPA MHI state machine.
 * In some state transitions a wakeup request will be triggered.
 *
 * Returns: 0 on success, -1 otherwise
 */
static int ipa3_mhi_set_state(enum ipa3_mhi_state new_state)
{
	unsigned long flags;
	int res = -EPERM;

	spin_lock_irqsave(&ipa3_mhi_ctx->state_lock, flags);
	IPA_MHI_DBG("Current state: %s\n", MHI_STATE_STR(ipa3_mhi_ctx->state));

	switch (ipa3_mhi_ctx->state) {
	case IPA_MHI_STATE_INITIALIZED:
		if (new_state == IPA_MHI_STATE_READY) {
			ipa3_mhi_notify_ready();
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
			if (ipa3_mhi_ctx->trigger_wakeup) {
				ipa3_mhi_ctx->trigger_wakeup = false;
				ipa3_mhi_notify_wakeup();
			}
			res = 0;
		} else if (new_state == IPA_MHI_STATE_STARTED) {
			ipa3_mhi_ctx->wakeup_notified = false;
			ipa3_mhi_ctx->trigger_wakeup = false;
			if (ipa3_mhi_ctx->rm_cons_state ==
				IPA_MHI_RM_STATE_REQUESTED) {
				ipa3_rm_notify_completion(
					IPA_RM_RESOURCE_GRANTED,
					IPA_RM_RESOURCE_MHI_CONS);
				ipa3_mhi_ctx->rm_cons_state =
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
			if (ipa3_mhi_ctx->trigger_wakeup) {
				ipa3_mhi_ctx->trigger_wakeup = false;
				ipa3_mhi_notify_wakeup();
			}
			res = 0;
		} else if (new_state == IPA_MHI_STATE_STARTED) {
			ipa3_mhi_ctx->trigger_wakeup = false;
			ipa3_mhi_ctx->wakeup_notified = false;
			if (ipa3_mhi_ctx->rm_cons_state ==
				IPA_MHI_RM_STATE_REQUESTED) {
				ipa3_rm_notify_completion(
					IPA_RM_RESOURCE_GRANTED,
					IPA_RM_RESOURCE_MHI_CONS);
				ipa3_mhi_ctx->rm_cons_state =
					IPA_MHI_RM_STATE_GRANTED;
			}
			res = 0;
		}
		break;

	default:
		IPA_MHI_ERR("Invalid state %d\n", ipa3_mhi_ctx->state);
		WARN_ON(1);
	}

	if (res)
		IPA_MHI_ERR("Invalid state change to %s\n",
						MHI_STATE_STR(new_state));
	else {
		IPA_MHI_DBG("New state change to %s\n",
						MHI_STATE_STR(new_state));
		ipa3_mhi_ctx->state = new_state;
	}
	spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);
	return res;
}

static void ipa3_mhi_rm_prod_notify(void *user_data, enum ipa_rm_event event,
	unsigned long data)
{
	IPA_MHI_FUNC_ENTRY();

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		IPA_MHI_DBG("IPA_RM_RESOURCE_GRANTED\n");
		complete_all(&ipa3_mhi_ctx->rm_prod_granted_comp);
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

static void ipa3_mhi_uc_ready_cb(void)
{
	IPA_MHI_FUNC_ENTRY();
	ipa3_mhi_set_state(IPA_MHI_STATE_READY);
	IPA_MHI_FUNC_EXIT();
}

static void ipa3_mhi_uc_wakeup_request_cb(void)
{
	unsigned long flags;

	IPA_MHI_FUNC_ENTRY();
	IPA_MHI_DBG("MHI state: %s\n", MHI_STATE_STR(ipa3_mhi_ctx->state));
	spin_lock_irqsave(&ipa3_mhi_ctx->state_lock, flags);
	if (ipa3_mhi_ctx->state == IPA_MHI_STATE_SUSPENDED)
		ipa3_mhi_notify_wakeup();
	else if (ipa3_mhi_ctx->state == IPA_MHI_STATE_SUSPEND_IN_PROGRESS)
		/* wakeup event will be triggered after suspend finishes */
		ipa3_mhi_ctx->trigger_wakeup = true;

	spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);
	IPA_MHI_FUNC_EXIT();
}

/**
 * ipa3_mhi_rm_cons_request() - callback function for IPA RM request resource
 *
 * In case IPA MHI is not suspended, MHI CONS will be granted immediately.
 * In case IPA MHI is suspended, MHI CONS will be granted after resume.
 */
static int ipa3_mhi_rm_cons_request(void)
{
	unsigned long flags;
	int res;

	IPA_MHI_FUNC_ENTRY();

	IPA_MHI_DBG("%s\n", MHI_STATE_STR(ipa3_mhi_ctx->state));
	spin_lock_irqsave(&ipa3_mhi_ctx->state_lock, flags);
	ipa3_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_REQUESTED;
	if (ipa3_mhi_ctx->state == IPA_MHI_STATE_STARTED) {
		ipa3_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_GRANTED;
		res = 0;
	} else if (ipa3_mhi_ctx->state == IPA_MHI_STATE_SUSPENDED) {
		ipa3_mhi_notify_wakeup();
		res = -EINPROGRESS;
	} else if (ipa3_mhi_ctx->state == IPA_MHI_STATE_SUSPEND_IN_PROGRESS) {
		/* wakeup event will be trigger after suspend finishes */
		ipa3_mhi_ctx->trigger_wakeup = true;
		res = -EINPROGRESS;
	} else {
		res = -EINPROGRESS;
	}

	spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);
	IPA_MHI_DBG("EXIT with %d\n", res);
	return res;
}

static int ipa3_mhi_rm_cons_release(void)
{
	unsigned long flags;

	IPA_MHI_FUNC_ENTRY();

	spin_lock_irqsave(&ipa3_mhi_ctx->state_lock, flags);
	ipa3_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_RELEASED;
	complete_all(&ipa3_mhi_ctx->rm_cons_comp);
	spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);
	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_wait_for_cons_release(void)
{
	unsigned long flags;
	int res;

	IPA_MHI_FUNC_ENTRY();
	reinit_completion(&ipa3_mhi_ctx->rm_cons_comp);
	spin_lock_irqsave(&ipa3_mhi_ctx->state_lock, flags);
	if (ipa3_mhi_ctx->rm_cons_state != IPA_MHI_RM_STATE_GRANTED) {
		spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);

	res = wait_for_completion_timeout(
		&ipa3_mhi_ctx->rm_cons_comp,
		msecs_to_jiffies(IPA_MHI_RM_TIMEOUT_MSEC));
	if (res == 0) {
		IPA_MHI_ERR("timeout release mhi cons\n");
		return -ETIME;
	}
	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_request_prod(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	reinit_completion(&ipa3_mhi_ctx->rm_prod_granted_comp);
	IPA_MHI_DBG("requesting mhi prod\n");
	res = ipa3_rm_request_resource(IPA_RM_RESOURCE_MHI_PROD);
	if (res) {
		if (res != -EINPROGRESS) {
			IPA_MHI_ERR("failed to request mhi prod %d\n", res);
			return res;
		}
		res = wait_for_completion_timeout(
			&ipa3_mhi_ctx->rm_prod_granted_comp,
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

static int ipa3_mhi_release_prod(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	res = ipa3_rm_release_resource(IPA_RM_RESOURCE_MHI_PROD);

	IPA_MHI_FUNC_EXIT();
	return res;

}

/**
 * ipa3_mhi_get_channel_context() - Get corresponding channel context
 * @ep: IPA ep
 * @channel_id: Channel ID
 *
 * This function will return the corresponding channel context or allocate new
 * one in case channel context for channel does not exist.
 */
static struct ipa3_mhi_channel_ctx *ipa3_mhi_get_channel_context(
	struct ipa3_ep_context *ep, u8 channel_id)
{
	int ch_idx;
	struct ipa3_mhi_channel_ctx *channels;
	int max_channels;

	if (IPA_CLIENT_IS_PROD(ep->client)) {
		channels = ipa3_mhi_ctx->ul_channels;
		max_channels = IPA_MHI_MAX_UL_CHANNELS;
	} else {
		channels = ipa3_mhi_ctx->dl_channels;
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
	channels[ch_idx].index = ipa3_mhi_ctx->total_channels++;
	channels[ch_idx].ep = ep;
	channels[ch_idx].state = IPA_HW_MHI_CHANNEL_STATE_INVALID;

	return &channels[ch_idx];
}

/**
 * ipa3_mhi_get_channel_context_by_clnt_hdl() - Get corresponding channel
 * context
 * @clnt_hdl: client handle as provided in ipa3_mhi_connect_pipe()
 *
 * This function will return the corresponding channel context or NULL in case
 * that channel does not exist.
 */
static struct ipa3_mhi_channel_ctx *ipa3_mhi_get_channel_context_by_clnt_hdl(
	u32 clnt_hdl)
{
	int ch_idx;

	for (ch_idx = 0; ch_idx < IPA_MHI_MAX_UL_CHANNELS; ch_idx++) {
		if (ipa3_mhi_ctx->ul_channels[ch_idx].valid &&
		    ipa3_get_ep_mapping(
		    ipa3_mhi_ctx->ul_channels[ch_idx].ep->client) == clnt_hdl)
			return &ipa3_mhi_ctx->ul_channels[ch_idx];
	}

	for (ch_idx = 0; ch_idx < IPA_MHI_MAX_DL_CHANNELS; ch_idx++) {
		if (ipa3_mhi_ctx->dl_channels[ch_idx].valid &&
		    ipa3_get_ep_mapping(
		    ipa3_mhi_ctx->dl_channels[ch_idx].ep->client) == clnt_hdl)
			return &ipa3_mhi_ctx->dl_channels[ch_idx];
	}

	return NULL;
}

static int ipa3_mhi_enable_force_clear(u32 request_id, bool throttle_source)
{
	struct ipa_enable_force_clear_datapath_req_msg_v01 req;
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	req.source_pipe_bitmask = 0;
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->ul_channels[i].valid)
			continue;
		req.source_pipe_bitmask |= 1 << ipa3_get_ep_mapping(
				ipa3_mhi_ctx->ul_channels[i].ep->client);
	}
	if (throttle_source) {
		req.throttle_source_valid = 1;
		req.throttle_source = 1;
	}
	IPA_MHI_DBG("req_id=0x%x src_pipe_btmk=0x%x throt_src=%d\n",
		req.request_id, req.source_pipe_bitmask,
		req.throttle_source);
	res = ipa3_qmi_enable_force_clear_datapath_send(&req);
	if (res) {
		IPA_MHI_ERR(
			"ipa3_qmi_enable_force_clear_datapath_send failed %d\n",
			res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_disable_force_clear(u32 request_id)
{
	struct ipa_disable_force_clear_datapath_req_msg_v01 req;
	int res;

	IPA_MHI_FUNC_ENTRY();
	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	IPA_MHI_DBG("req_id=0x%x\n", req.request_id);
	res = ipa3_qmi_disable_force_clear_datapath_send(&req);
	if (res) {
		IPA_MHI_ERR(
			"ipa3_qmi_disable_force_clear_datapath_send failed %d\n",
			res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static bool ipa3_mhi_sps_channel_empty(struct ipa3_mhi_channel_ctx *channel)
{
	u32 pipe_idx;
	bool pending;

	pipe_idx = ipa3_get_ep_mapping(channel->ep->client);
	if (sps_pipe_pending_desc(ipa3_ctx->bam_handle,
		pipe_idx, &pending)) {
		IPA_MHI_ERR("sps_pipe_pending_desc failed\n");
		WARN_ON(1);
		return false;
	}

	return !pending;
}

static bool ipa3_mhi_gsi_channel_empty(struct ipa3_mhi_channel_ctx *channel)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (!channel->stop_in_proc) {
		IPA_MHI_DBG("Channel is not in STOP_IN_PROC\n");
		return true;
	}

	IPA_MHI_DBG("Stopping GSI channel %ld\n", channel->ep->gsi_chan_hdl);
	res = gsi_stop_channel(channel->ep->gsi_chan_hdl);
	if (res != 0 &&
		res != -GSI_STATUS_AGAIN &&
		res != -GSI_STATUS_TIMED_OUT) {
		IPA_MHI_ERR("GSI stop channel failed %d\n",
			res);
		WARN_ON(1);
		return false;
	}

	if (res == 0) {
		IPA_MHI_DBG("GSI channel %ld STOP\n",
			channel->ep->gsi_chan_hdl);
		channel->stop_in_proc = false;
		return true;
	}

	return false;
}

/**
 * ipa3_mhi_wait_for_ul_empty_timeout() - wait for pending packets in uplink
 * @msecs: timeout to wait
 *
 * This function will poll until there are no packets pending in uplink channels
 * or timeout occurred.
 *
 * Return code: true - no pending packets in uplink channels
 *		false - timeout occurred
 */
static bool ipa3_mhi_wait_for_ul_empty_timeout(unsigned int msecs)
{
	unsigned long jiffies_timeout = msecs_to_jiffies(msecs);
	unsigned long jiffies_start = jiffies;
	bool empty = false;
	int i;

	IPA_MHI_FUNC_ENTRY();
	while (!empty) {
		empty = true;
		for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
			if (!ipa3_mhi_ctx->ul_channels[i].valid)
				continue;
			if (ipa3_ctx->transport_prototype ==
			    IPA_TRANSPORT_TYPE_GSI)
				empty &= ipa3_mhi_gsi_channel_empty(
					&ipa3_mhi_ctx->ul_channels[i]);
			else
				empty &= ipa3_mhi_sps_channel_empty(
					&ipa3_mhi_ctx->ul_channels[i]);
		}

		if (time_after(jiffies, jiffies_start + jiffies_timeout)) {
			IPA_MHI_DBG("timeout waiting for UL empty\n");
			break;
		}
	}
	IPA_MHI_DBG("IPA UL is %s\n", (empty) ? "empty" : "not empty");

	IPA_MHI_FUNC_EXIT();
	return empty;
}

static void ipa3_mhi_set_holb_on_dl_channels(bool enable,
	struct ipa_ep_cfg_holb old_holb[])
{
	int i;
	struct ipa_ep_cfg_holb ep_holb;
	int ep_idx;
	int res;

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->dl_channels[i].valid)
			continue;
		if (ipa3_mhi_ctx->dl_channels[i].state ==
			IPA_HW_MHI_CHANNEL_STATE_INVALID)
			continue;
		ep_idx = ipa3_get_ep_mapping(
			ipa3_mhi_ctx->dl_channels[i].ep->client);
		if (-1 == ep_idx) {
			IPA_MHI_ERR("Client %u is not mapped\n",
				ipa3_mhi_ctx->dl_channels[i].ep->client);
			BUG();
			return;
		}
		memset(&ep_holb, 0, sizeof(ep_holb));
		if (enable) {
			ep_holb.en = 1;
			ep_holb.tmr_val = 0;
			old_holb[i] = ipa3_ctx->ep[ep_idx].holb;
		} else {
			ep_holb = old_holb[i];
		}
		res = ipa3_cfg_ep_holb(ep_idx, &ep_holb);
		if (res) {
			IPA_MHI_ERR("ipa3_cfg_ep_holb failed %d\n", res);
			BUG();
			return;
		}
	}
}

static int ipa3_mhi_suspend_gsi_channel(struct ipa3_mhi_channel_ctx *channel)
{
	int res;
	u32 clnt_hdl;

	IPA_MHI_FUNC_ENTRY();

	clnt_hdl = ipa3_get_ep_mapping(channel->ep->client);
	if (clnt_hdl == -1)
		return -EFAULT;

	res = ipa3_stop_gsi_channel(clnt_hdl);
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

static int ipa3_mhi_reset_gsi_channel(struct ipa3_mhi_channel_ctx *channel)
{
	int res;
	u32 clnt_hdl;

	IPA_MHI_FUNC_ENTRY();

	clnt_hdl = ipa3_get_ep_mapping(channel->ep->client);
	if (clnt_hdl == -1)
		return -EFAULT;

	res = ipa3_reset_gsi_channel(clnt_hdl);
	if (res) {
		IPA_MHI_ERR("ipa3_reset_gsi_channel failed %d\n", res);
		return -EFAULT;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_reset_ul_channel(struct ipa3_mhi_channel_ctx *channel)
{
	int res;
	struct ipa_ep_cfg_holb old_ep_holb[IPA_MHI_MAX_DL_CHANNELS];
	bool empty;

	IPA_MHI_FUNC_ENTRY();
	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa3_mhi_suspend_gsi_channel(channel);
		if (res) {
			IPA_MHI_ERR("ipa3_mhi_suspend_gsi_channel failed %d\n",
				 res);
			return res;
		}
	} else {
		res = ipa3_uc_mhi_reset_channel(channel->index);
		if (res) {
			IPA_MHI_ERR("ipa3_uc_mhi_reset_channel failed %d\n",
				res);
			return res;
		}
	}

	empty = ipa3_mhi_wait_for_ul_empty_timeout(
			IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);
	if (!empty) {
		IPA_MHI_DBG("%s not empty\n",
			(ipa3_ctx->transport_prototype ==
				IPA_TRANSPORT_TYPE_GSI) ? "GSI" : "BAM");
		res = ipa3_mhi_enable_force_clear(ipa3_mhi_ctx->qmi_req_id,
			false);
		if (res) {
			IPA_MHI_ERR("ipa3_mhi_enable_force_clear failed %d\n",
				res);
			BUG();
			return res;
		}

		if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
			empty = ipa3_mhi_wait_for_ul_empty_timeout(
				IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);

			IPADBG("empty=%d\n", empty);
		} else {
			/* enable packet drop on all DL channels */
			ipa3_mhi_set_holb_on_dl_channels(true, old_ep_holb);
			res = ipa3_tag_process(NULL, 0, HZ);
			if (res)
				IPAERR("TAG process failed\n");

			/* disable packet drop on all DL channels */
			ipa3_mhi_set_holb_on_dl_channels(false, old_ep_holb);
			res = sps_pipe_disable(ipa3_ctx->bam_handle,
				ipa3_get_ep_mapping(channel->ep->client));
			if (res) {
				IPA_MHI_ERR("sps_pipe_disable fail %d\n", res);
				BUG();
				return res;
			}
		}

		res = ipa3_mhi_disable_force_clear(ipa3_mhi_ctx->qmi_req_id);
		if (res) {
			IPA_MHI_ERR("ipa3_mhi_disable_force_clear failed %d\n",
				res);
			BUG();
			return res;
		}
		ipa3_mhi_ctx->qmi_req_id++;
	}

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa3_mhi_reset_gsi_channel(channel);
		if (res) {
			IPAERR("ipa3_mhi_reset_gsi_channel failed\n");
			BUG();
			return res;
		}
	}

	res = ipa3_disable_data_path(ipa3_get_ep_mapping(channel->ep->client));
	if (res) {
		IPA_MHI_ERR("ipa3_disable_data_path failed %d\n", res);
		return res;
	}
	IPA_MHI_FUNC_EXIT();

	return 0;
}

static int ipa3_mhi_reset_dl_channel(struct ipa3_mhi_channel_ctx *channel)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa3_mhi_suspend_gsi_channel(channel);
		if (res) {
			IPAERR("ipa3_mhi_suspend_gsi_channel failed %d\n", res);
			return res;
		}

		res = ipa3_mhi_reset_gsi_channel(channel);
		if (res) {
			IPAERR("ipa3_mhi_reset_gsi_channel failed\n");
			return res;
		}

		res = ipa3_disable_data_path(
			ipa3_get_ep_mapping(channel->ep->client));
		if (res) {
			IPA_MHI_ERR("ipa3_disable_data_path failed\n");
			return res;
		}
	} else {
		res = ipa3_disable_data_path(
			ipa3_get_ep_mapping(channel->ep->client));
		if (res) {
			IPA_MHI_ERR("ipa3_disable_data_path failed %d\n", res);
			return res;
		}

		res = ipa3_uc_mhi_reset_channel(channel->index);
		if (res) {
			IPA_MHI_ERR("ipa3_uc_mhi_reset_channel failed %d\n",
				res);
			ipa3_enable_data_path(
				ipa3_get_ep_mapping(channel->ep->client));
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_reset_channel(struct ipa3_mhi_channel_ctx *channel)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	if (IPA_CLIENT_IS_PROD(channel->ep->client))
		res = ipa3_mhi_reset_ul_channel(channel);
	else
		res = ipa3_mhi_reset_dl_channel(channel);
	if (res) {
		IPA_MHI_ERR("failed to reset channel error %d\n", res);
		return res;
	}

	channel->state = IPA_HW_MHI_CHANNEL_STATE_DISABLE;

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
				offsetof(struct ipa3_mhi_ch_ctx, chstate),
				sizeof(channel->state));
		if (res) {
			IPAERR("ipa_mhi_read_write_host failed %d\n", res);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa_mhi_start_uc_channel(struct ipa3_mhi_channel_ctx *channel,
	int ipa_ep_idx)
{
	int res;
	struct ipa3_ep_context *ep;

	IPA_MHI_FUNC_ENTRY();

	ep = channel->ep;
	if (channel->state == IPA_HW_MHI_CHANNEL_STATE_INVALID) {
		IPA_MHI_DBG("Initializing channel\n");
		res = ipa3_uc_mhi_init_channel(ipa_ep_idx, channel->index,
			channel->id, (IPA_CLIENT_IS_PROD(ep->client) ? 1 : 2));
		if (res) {
			IPA_MHI_ERR("init_channel failed %d\n", res);
			return res;
		}
	} else if (channel->state == IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
		if (channel->ep != ep) {
			IPA_MHI_ERR("previous channel client was %d\n",
				ep->client);
			return res;
		}
		IPA_MHI_DBG("Starting channel\n");
		res = ipa3_uc_mhi_resume_channel(channel->index, false);
		if (res) {
			IPA_MHI_ERR("init_channel failed %d\n", res);
			return res;
		}
	} else {
		IPA_MHI_ERR("Invalid channel state %d\n", channel->state);
		return -EFAULT;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static void ipa_mhi_dump_ch_ctx(struct ipa3_mhi_channel_ctx *channel)
{
	IPA_MHI_DBG("ch_id %d\n", channel->id);
	IPA_MHI_DBG("chstate 0x%x\n", channel->ch_ctx_host.chstate);
	IPA_MHI_DBG("chtype 0x%x\n", channel->ch_ctx_host.chtype);
	IPA_MHI_DBG("erindex 0x%x\n", channel->ch_ctx_host.erindex);
	IPA_MHI_DBG("rbase 0x%llx\n", channel->ch_ctx_host.rbase);
	IPA_MHI_DBG("rlen 0x%llx\n", channel->ch_ctx_host.rlen);
	IPA_MHI_DBG("rp 0x%llx\n", channel->ch_ctx_host.rp);
	IPA_MHI_DBG("wp 0x%llx\n", channel->ch_ctx_host.wp);
}

static void ipa_mhi_dump_ev_ctx(struct ipa3_mhi_channel_ctx *channel)
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

static int ipa_mhi_read_ch_ctx(struct ipa3_mhi_channel_ctx *channel)
{
	int res;

	res = ipa_mhi_read_write_host(IPA_MHI_DMA_FROM_HOST,
		&channel->ch_ctx_host, channel->channel_context_addr,
		sizeof(channel->ch_ctx_host));
	if (res) {
		IPAERR("ipa_mhi_read_write_host failed %d\n", res);
		return res;

	}
	ipa_mhi_dump_ch_ctx(channel);

	channel->event_context_addr = ipa3_mhi_ctx->event_context_array_addr +
		channel->ch_ctx_host.erindex * sizeof(struct ipa3_mhi_ev_ctx);
	IPA_MHI_DBG("ch %d event_context_addr 0x%llx\n", channel->id,
		channel->event_context_addr);

	res = ipa_mhi_read_write_host(IPA_MHI_DMA_FROM_HOST,
		&channel->ev_ctx_host, channel->event_context_addr,
		sizeof(channel->ev_ctx_host));
	if (res) {
		IPAERR("ipa_mhi_read_write_host failed %d\n", res);
		return res;

	}
	ipa_mhi_dump_ev_ctx(channel);

	return 0;
}

static void ipa_mhi_gsi_ev_err_cb(struct gsi_evt_err_notify *notify)
{
	struct ipa3_mhi_channel_ctx *channel = notify->user_data;

	IPAERR("channel id=%d client=%d state=%d\n",
		channel->id, channel->ep->client, channel->state);
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
}

static void ipa_mhi_gsi_ch_err_cb(struct gsi_chan_err_notify *notify)
{
	struct ipa3_mhi_channel_ctx *channel = notify->chan_user_data;

	IPAERR("channel id=%d client=%d state=%d\n",
		channel->id, channel->ep->client, channel->state);
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
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	IPA_MHI_ERR("err_desc=0x%x\n", notify->err_desc);
}

static int ipa_mhi_start_gsi_channel(struct ipa3_mhi_channel_ctx *channel,
	int ipa_ep_idx)
{
	int res;
	struct ipa3_ep_context *ep;
	struct gsi_evt_ring_props ev_props;
	struct ipa_mhi_msi_info msi;
	union __packed gsi_evt_scratch ev_scratch;
	struct gsi_chan_props ch_props;
	union __packed gsi_channel_scratch ch_scratch;

	IPA_MHI_FUNC_ENTRY();

	if (channel->state != IPA_HW_MHI_CHANNEL_STATE_INVALID &&
	    channel->state != IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
		IPA_MHI_ERR("Invalid channel state %d\n", channel->state);
		return -EFAULT;
	}

	msi = ipa3_mhi_ctx->msi;
	ep = channel->ep;

	IPA_MHI_DBG("reading ch/ev context from host\n");
	res = ipa_mhi_read_ch_ctx(channel);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_read_ch_ctx failed %d\n", res);
		return res;
	}

	/* allocate event ring only for the first time pipe is connected */
	if (channel->state == IPA_HW_MHI_CHANNEL_STATE_INVALID) {
		IPA_MHI_DBG("allocating event ring\n");
		memset(&ev_props, 0, sizeof(ev_props));
		ev_props.intf = GSI_EVT_CHTYPE_MHI_EV;
		ev_props.intr = GSI_INTR_MSI;
		ev_props.re_size = GSI_EVT_RING_RE_SIZE_16B;
		ev_props.ring_len = channel->ev_ctx_host.rlen;
		ev_props.ring_base_addr = IPA_MHI_HOST_ADDR_COND(
				channel->ev_ctx_host.rbase);
		ev_props.int_modt = channel->ev_ctx_host.intmodt *
				IPA_SLEEP_CLK_RATE_KHZ;
		ev_props.int_modc = channel->ev_ctx_host.intmodc;
		ev_props.intvec = ((msi.data & ~msi.mask) |
				(channel->ev_ctx_host.msivec & msi.mask));
		ev_props.msi_addr = IPA_MHI_HOST_ADDR_COND(
				(((u64)msi.addr_hi << 32) | msi.addr_low));
		ev_props.rp_update_addr = IPA_MHI_HOST_ADDR_COND(
				channel->event_context_addr +
				offsetof(struct ipa3_mhi_ev_ctx, rp));
		ev_props.exclusive = true;
		ev_props.err_cb = ipa_mhi_gsi_ev_err_cb;
		ev_props.user_data = channel;
		ev_props.evchid_valid = true;
		ev_props.evchid = channel->index + IPA_MHI_GSI_ER_START;
		res = gsi_alloc_evt_ring(&ev_props, ipa3_ctx->gsi_dev_hdl,
			&channel->ep->gsi_evt_ring_hdl);
		if (res) {
			IPA_MHI_ERR("gsi_alloc_evt_ring failed %d\n", res);
			goto fail_alloc_evt;
			return res;
		}

		channel->cached_gsi_evt_ring_hdl =
			channel->ep->gsi_evt_ring_hdl;

		memset(&ev_scratch, 0, sizeof(ev_scratch));
		ev_scratch.mhi.ul_dl_sync_en =
			ipa3_cached_dl_ul_sync_info.params.isDlUlSyncEnabled;
		res = gsi_write_evt_ring_scratch(channel->ep->gsi_evt_ring_hdl,
					ev_scratch);
		if (res) {
			IPA_MHI_ERR("gsi_write_evt_ring_scratch failed %d\n",
				res);
			goto fail_evt_scratch;
		}
	}

	memset(&ch_props, 0, sizeof(ch_props));
	ch_props.prot = GSI_CHAN_PROT_MHI;
	ch_props.dir = IPA_CLIENT_IS_PROD(ep->client) ?
		GSI_CHAN_DIR_TO_GSI : GSI_CHAN_DIR_FROM_GSI;
	ch_props.ch_id =
		ipa_get_gsi_ep_info(ipa_ep_idx)->ipa_gsi_chan_num;
	ch_props.evt_ring_hdl = channel->cached_gsi_evt_ring_hdl;
	ch_props.re_size = GSI_CHAN_RE_SIZE_16B;
	ch_props.ring_len = channel->ch_ctx_host.rlen;
	ch_props.ring_base_addr = IPA_MHI_HOST_ADDR_COND(
			channel->ch_ctx_host.rbase);
	ch_props.use_db_eng = GSI_CHAN_DIRECT_MODE;
	ch_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	ch_props.low_weight = 1;
	ch_props.err_cb = ipa_mhi_gsi_ch_err_cb;
	ch_props.chan_user_data = channel;
	res = gsi_alloc_channel(&ch_props, ipa3_ctx->gsi_dev_hdl,
		&channel->ep->gsi_chan_hdl);
	if (res) {
		IPA_MHI_ERR("gsi_alloc_channel failed %d\n",
			res);
		goto fail_alloc_ch;
	}

	memset(&ch_scratch, 0, sizeof(ch_scratch));
	ch_scratch.mhi.mhi_host_wp_addr = IPA_MHI_HOST_ADDR_COND(
			channel->channel_context_addr +
			offsetof(struct ipa3_mhi_ch_ctx, wp));
	ch_scratch.mhi.ul_dl_sync_en = ipa3_cached_dl_ul_sync_info.
			params.isDlUlSyncEnabled;
	ch_scratch.mhi.assert_bit40 = ipa3_mhi_ctx->assert_bit40;
	ch_scratch.mhi.max_outstanding_tre =
		ipa_get_gsi_ep_info(ipa_ep_idx)->ipa_if_aos *
			GSI_CHAN_RE_SIZE_16B;
	ch_scratch.mhi.outstanding_threshold =
		4 * GSI_CHAN_RE_SIZE_16B;
	res = gsi_write_channel_scratch(channel->ep->gsi_chan_hdl,
		ch_scratch);
	if (res) {
		IPA_MHI_ERR("gsi_write_channel_scratch failed %d\n",
			res);
		goto fail_ch_scratch;
	}

	IPA_MHI_DBG("Starting channel\n");
	res = gsi_start_channel(channel->ep->gsi_chan_hdl);
	if (res) {
		IPA_MHI_ERR("gsi_start_channel failed %d\n", res);
		goto fail_ch_start;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_ch_start:
fail_ch_scratch:
	gsi_dealloc_channel(channel->ep->gsi_chan_hdl);
fail_alloc_ch:
fail_evt_scratch:
	gsi_dealloc_evt_ring(channel->ep->gsi_evt_ring_hdl);
	channel->ep->gsi_evt_ring_hdl = ~0;
fail_alloc_evt:
	return res;
}

/**
 * ipa3_mhi_init() - Initialize IPA MHI driver
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
int ipa3_mhi_init(struct ipa_mhi_init_params *params)
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

	if (ipa3_mhi_ctx) {
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
	IPA_MHI_DBG("assert_bit40=%d\n", params->assert_bit40);
	IPA_MHI_DBG("test_mode=%d\n", params->test_mode);

	/* Initialize context */
	ipa3_mhi_ctx = kzalloc(sizeof(*ipa3_mhi_ctx), GFP_KERNEL);
	if (!ipa3_mhi_ctx) {
		IPA_MHI_ERR("no memory\n");
		res = -EFAULT;
		goto fail_alloc_ctx;
	}

	ipa3_mhi_ctx->state = IPA_MHI_STATE_INITIALIZED;
	ipa3_mhi_ctx->msi = params->msi;
	ipa3_mhi_ctx->mmio_addr = params->mmio_addr;
	ipa3_mhi_ctx->first_ch_idx = params->first_ch_idx;
	ipa3_mhi_ctx->first_er_idx = params->first_er_idx;
	ipa3_mhi_ctx->cb_notify = params->notify;
	ipa3_mhi_ctx->cb_priv = params->priv;
	ipa3_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_RELEASED;
	ipa3_mhi_ctx->qmi_req_id = 0;
	ipa3_mhi_ctx->use_ipadma = 1;
	ipa3_mhi_ctx->assert_bit40 = !!params->assert_bit40;
	ipa3_mhi_ctx->test_mode = params->test_mode;
	init_completion(&ipa3_mhi_ctx->rm_prod_granted_comp);
	spin_lock_init(&ipa3_mhi_ctx->state_lock);
	init_completion(&ipa3_mhi_ctx->rm_cons_comp);

	ipa3_mhi_ctx->wq = create_singlethread_workqueue("ipa_mhi_wq");
	if (!ipa3_mhi_ctx->wq) {
		IPA_MHI_ERR("failed to create workqueue\n");
		res = -EFAULT;
		goto fail_create_wq;
	}

	/* Create PROD in IPA RM */
	memset(&mhi_prod_params, 0, sizeof(mhi_prod_params));
	mhi_prod_params.name = IPA_RM_RESOURCE_MHI_PROD;
	mhi_prod_params.floor_voltage = IPA_VOLTAGE_SVS;
	mhi_prod_params.reg_params.notify_cb = ipa3_mhi_rm_prod_notify;
	res = ipa3_rm_create_resource(&mhi_prod_params);
	if (res) {
		IPA_MHI_ERR("fail to create IPA_RM_RESOURCE_MHI_PROD\n");
		goto fail_create_rm_prod;
	}

	/* Create CONS in IPA RM */
	memset(&mhi_cons_params, 0, sizeof(mhi_cons_params));
	mhi_cons_params.name = IPA_RM_RESOURCE_MHI_CONS;
	mhi_cons_params.floor_voltage = IPA_VOLTAGE_SVS;
	mhi_cons_params.request_resource = ipa3_mhi_rm_cons_request;
	mhi_cons_params.release_resource = ipa3_mhi_rm_cons_release;
	res = ipa3_rm_create_resource(&mhi_cons_params);
	if (res) {
		IPA_MHI_ERR("fail to create IPA_RM_RESOURCE_MHI_CONS\n");
		goto fail_create_rm_cons;
	}

	/* (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
	 * we need to move to READY state only after
	 * HPS/DPS/GSI firmware are loaded.
	 */

	/* Initialize uC interface */
	ipa3_uc_mhi_init(ipa3_mhi_uc_ready_cb,
		ipa3_mhi_uc_wakeup_request_cb);
	if (ipa3_uc_state_check() == 0)
		ipa3_mhi_set_state(IPA_MHI_STATE_READY);

	/* Initialize debugfs */
	ipa3_mhi_debugfs_init();

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_create_rm_cons:
	ipa3_rm_delete_resource(IPA_RM_RESOURCE_MHI_PROD);
fail_create_rm_prod:
	destroy_workqueue(ipa3_mhi_ctx->wq);
fail_create_wq:
	kfree(ipa3_mhi_ctx);
	ipa3_mhi_ctx = NULL;
fail_alloc_ctx:
	return res;
}

/**
 * ipa3_mhi_start() - Start IPA MHI engine
 * @params: pcie addresses for MHI
 *
 * This function is called by MHI client driver on MHI engine start for
 * handling MHI accelerated channels. This function is called after
 * ipa3_mhi_init() was called and can be called after MHI reset to restart MHI
 * engine. When this function returns device can move to M0 state.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa3_mhi_start(struct ipa_mhi_start_params *params)
{
	int res;
	struct gsi_device_scratch gsi_scratch;
	struct ipa_gsi_ep_config *gsi_ep_info;

	IPA_MHI_FUNC_ENTRY();

	if (!params) {
		IPA_MHI_ERR("null args\n");
		return -EINVAL;
	}

	if (!ipa3_mhi_ctx) {
		IPA_MHI_ERR("not initialized\n");
		return -EPERM;
	}

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_SPS &&
	    ipa3_uc_state_check()) {
		IPA_MHI_ERR("IPA uc is not loaded\n");
		return -EAGAIN;
	}

	res = ipa3_mhi_set_state(IPA_MHI_STATE_STARTED);
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_set_state %d\n", res);
		return res;
	}

	ipa3_mhi_ctx->host_ctrl_addr = params->host_ctrl_addr;
	ipa3_mhi_ctx->host_data_addr = params->host_data_addr;
	ipa3_mhi_ctx->channel_context_array_addr =
		params->channel_context_array_addr;
	ipa3_mhi_ctx->event_context_array_addr =
		params->event_context_array_addr;
	IPADBG("host_ctrl_addr 0x%x\n", ipa3_mhi_ctx->host_ctrl_addr);
	IPADBG("host_data_addr 0x%x\n", ipa3_mhi_ctx->host_data_addr);
	IPADBG("channel_context_array_addr 0x%llx\n",
		ipa3_mhi_ctx->channel_context_array_addr);
	IPADBG("event_context_array_addr 0x%llx\n",
		ipa3_mhi_ctx->event_context_array_addr);

	/* Add MHI <-> Q6 dependencies to IPA RM */
	res = ipa3_rm_add_dependency(IPA_RM_RESOURCE_MHI_PROD,
		IPA_RM_RESOURCE_Q6_CONS);
	if (res && res != -EINPROGRESS) {
		IPA_MHI_ERR("failed to add dependency %d\n", res);
		goto fail_add_mhi_q6_dep;
	}

	res = ipa3_rm_add_dependency(IPA_RM_RESOURCE_Q6_PROD,
		IPA_RM_RESOURCE_MHI_CONS);
	if (res && res != -EINPROGRESS) {
		IPA_MHI_ERR("failed to add dependency %d\n", res);
		goto fail_add_q6_mhi_dep;
	}

	res = ipa3_mhi_request_prod();
	if (res) {
		IPA_MHI_ERR("failed request prod %d\n", res);
		goto fail_request_prod;
	}

	/* Initialize IPA MHI engine */
	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		gsi_ep_info = ipa_get_gsi_ep_info(
			ipa_get_ep_mapping(IPA_CLIENT_MHI_PROD));
		if (!gsi_ep_info) {
			IPAERR("MHI PROD has no ep allocated\n");
			BUG();
		}
		memset(&gsi_scratch, 0, sizeof(gsi_scratch));
		gsi_scratch.mhi_base_chan_idx_valid = true;
		gsi_scratch.mhi_base_chan_idx = gsi_ep_info->ipa_gsi_chan_num +
			ipa3_mhi_ctx->first_ch_idx;
		res = gsi_write_device_scratch(ipa3_ctx->gsi_dev_hdl,
			&gsi_scratch);
		if (res) {
			IPA_MHI_ERR("failed to write device scratch %d\n", res);
			goto fail_init_engine;
		}
	} else {
		res = ipa3_uc_mhi_init_engine(&ipa3_mhi_ctx->msi,
			ipa3_mhi_ctx->mmio_addr,
			ipa3_mhi_ctx->host_ctrl_addr,
			ipa3_mhi_ctx->host_data_addr,
			ipa3_mhi_ctx->first_ch_idx,
			ipa3_mhi_ctx->first_er_idx);
		if (res) {
			IPA_MHI_ERR("failed to start MHI engine %d\n", res);
			goto fail_init_engine;
		}

		/* Update UL/DL sync if valid */
		res = ipa3_uc_mhi_send_dl_ul_sync_info(
			ipa3_cached_dl_ul_sync_info);
		if (res) {
			IPA_MHI_ERR("failed to update ul/dl sync %d\n", res);
			goto fail_init_engine;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_init_engine:
	ipa3_mhi_release_prod();
fail_request_prod:
	ipa3_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
		IPA_RM_RESOURCE_MHI_CONS);
fail_add_q6_mhi_dep:
	ipa3_rm_delete_dependency(IPA_RM_RESOURCE_MHI_PROD,
		IPA_RM_RESOURCE_Q6_CONS);
fail_add_mhi_q6_dep:
	ipa3_mhi_set_state(IPA_MHI_STATE_INITIALIZED);
	return res;
}

/**
 * ipa3_mhi_connect_pipe() - Connect pipe to IPA and start corresponding
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
int ipa3_mhi_connect_pipe(struct ipa_mhi_connect_params *in, u32 *clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;
	int res;
	struct ipa3_mhi_channel_ctx *channel = NULL;
	unsigned long flags;

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
		IPA_MHI_ERR("Invalid MHI client, client: %d\n", in->sys.client);
		return -EINVAL;
	}

	IPA_MHI_DBG("channel=%d\n", in->channel_id);

	spin_lock_irqsave(&ipa3_mhi_ctx->state_lock, flags);
	if (!ipa3_mhi_ctx || ipa3_mhi_ctx->state != IPA_MHI_STATE_STARTED) {
		IPA_MHI_ERR("IPA MHI was not started\n");
		spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&ipa3_mhi_ctx->state_lock, flags);

	ipa_ep_idx = ipa3_get_ep_mapping(in->sys.client);
	if (ipa_ep_idx == -1) {
		IPA_MHI_ERR("Invalid client.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->valid == 1) {
		IPA_MHI_ERR("EP already allocated.\n");
		return -EPERM;
	}

	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));
	ep->valid = 1;
	ep->skip_ep_cfg = in->sys.skip_ep_cfg;
	ep->client = in->sys.client;
	ep->client_notify = in->sys.notify;
	ep->priv = in->sys.priv;
	ep->keep_ipa_awake = in->sys.keep_ipa_awake;

	channel = ipa3_mhi_get_channel_context(ep,
		in->channel_id);
	if (!channel) {
		IPA_MHI_ERR("ipa3_mhi_get_channel_context failed\n");
		res = -EINVAL;
		goto fail_init_channel;
	}

	channel->channel_context_addr =
		ipa3_mhi_ctx->channel_context_array_addr +
			channel->id * sizeof(struct ipa3_mhi_ch_ctx);

	/* for event context address index needs to read from host */

	IPA_MHI_DBG("client %d channelHandle %d channelIndex %d\n",
		channel->ep->client, channel->index, channel->id);
	IPA_MHI_DBG("channel_context_addr 0x%llx\n",
		channel->channel_context_addr);

	IPA_ACTIVE_CLIENTS_INC_EP(in->sys.client);

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		res = ipa_mhi_start_gsi_channel(channel, ipa_ep_idx);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_start_gsi_channel failed %d\n",
				res);
			goto fail_start_channel;
		}
		channel->state = IPA_HW_MHI_CHANNEL_STATE_RUN;

		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
				offsetof(struct ipa3_mhi_ch_ctx, chstate),
				sizeof(channel->state));
		if (res) {
			IPAERR("ipa_mhi_read_write_host failed\n");
			return res;

		}
	} else {
		res = ipa_mhi_start_uc_channel(channel, ipa_ep_idx);
		if (res) {
			IPA_MHI_ERR("ipa_mhi_start_uc_channel failed %d\n",
				res);
			goto fail_start_channel;
		}
		channel->state = IPA_HW_MHI_CHANNEL_STATE_RUN;
	}

	res = ipa3_enable_data_path(ipa_ep_idx);
	if (res) {
		IPA_MHI_ERR("enable data path failed res=%d clnt=%d.\n", res,
			ipa_ep_idx);
		goto fail_enable_dp;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &in->sys.ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_ep_cfg;
		}
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_ep_cfg;
		}
		IPA_MHI_DBG("ep configuration successful\n");
	} else {
		IPA_MHI_DBG("skipping ep configuration\n");
	}

	*clnt_hdl = ipa_ep_idx;

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->sys.client))
		ipa3_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	IPA_MHI_DBG("client %d (ep: %d) connected\n", in->sys.client,
		ipa_ep_idx);

	IPA_MHI_FUNC_EXIT();

	return 0;

fail_ep_cfg:
	ipa3_disable_data_path(ipa_ep_idx);
fail_enable_dp:
	ipa3_mhi_reset_channel(channel);
fail_start_channel:
	IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);
fail_init_channel:
	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));
	return -EPERM;
}

/**
 * ipa3_mhi_disconnect_pipe() - Disconnect pipe from IPA and reset corresponding
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
int ipa3_mhi_disconnect_pipe(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	static struct ipa3_mhi_channel_ctx *channel;
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes) {
		IPAERR("invalid handle %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("pipe was not connected %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (!ipa3_mhi_ctx) {
		IPA_MHI_ERR("IPA MHI was not initialized\n");
		return -EINVAL;
	}

	if (!IPA_CLIENT_IS_MHI(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("invalid IPA MHI client, client: %d\n",
			ipa3_ctx->ep[clnt_hdl].client);
		return -EINVAL;
	}

	channel = ipa3_mhi_get_channel_context_by_clnt_hdl(clnt_hdl);
	if (!channel) {
		IPAERR("invalid clnt index\n");
		return -EINVAL;
	}
	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	res = ipa3_mhi_reset_channel(channel);
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_reset_channel failed %d\n", res);
		goto fail_reset_channel;
	}

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		res = gsi_dealloc_channel(channel->ep->gsi_chan_hdl);
		if (res) {
			IPAERR("gsi_dealloc_channel failed %d\n", res);
			goto fail_reset_channel;
		}
	}

	ep->valid = 0;
	ipa3_delete_dflt_flt_rules(clnt_hdl);
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPA_MHI_DBG("client (ep: %d) disconnected\n", clnt_hdl);
	IPA_MHI_FUNC_EXIT();
	return 0;

fail_reset_channel:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return res;
}


static int ipa3_mhi_suspend_ul_channels(void)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->ul_channels[i].valid)
			continue;
		if (ipa3_mhi_ctx->ul_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_RUN)
			continue;
		IPA_MHI_DBG("suspending channel %d\n",
			ipa3_mhi_ctx->ul_channels[i].id);

		if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
			res = ipa3_mhi_suspend_gsi_channel(
				&ipa3_mhi_ctx->ul_channels[i]);
		else
			res = ipa3_uc_mhi_suspend_channel(
				ipa3_mhi_ctx->ul_channels[i].index);

		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		ipa3_mhi_ctx->ul_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_SUSPEND;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_resume_ul_channels(bool LPTransitionRejected)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->ul_channels[i].valid)
			continue;
		if (ipa3_mhi_ctx->ul_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("resuming channel %d\n",
			ipa3_mhi_ctx->ul_channels[i].id);

		if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
			res = gsi_start_channel(
				ipa3_mhi_ctx->ul_channels[i].ep->gsi_chan_hdl);
		else
			res = ipa3_uc_mhi_resume_channel(
				ipa3_mhi_ctx->ul_channels[i].index,
				LPTransitionRejected);

		if (res) {
			IPA_MHI_ERR("failed to resume channel %d error %d\n",
				i, res);
			return res;
		}

		ipa3_mhi_ctx->ul_channels[i].stop_in_proc = false;
		ipa3_mhi_ctx->ul_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_RUN;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_stop_event_update_ul_channels(void)
{
	int i;
	int res;

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
		return 0;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->ul_channels[i].valid)
			continue;
		if (ipa3_mhi_ctx->ul_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("stop update event channel %d\n",
			ipa3_mhi_ctx->ul_channels[i].id);
		res = ipa3_uc_mhi_stop_event_update_channel(
			ipa3_mhi_ctx->ul_channels[i].index);
		if (res) {
			IPA_MHI_ERR("failed stop event channel %d error %d\n",
				i, res);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_suspend_dl_channels(void)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->dl_channels[i].valid)
			continue;
		if (ipa3_mhi_ctx->dl_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_RUN)
			continue;
		IPA_MHI_DBG("suspending channel %d\n",
			ipa3_mhi_ctx->dl_channels[i].id);
		if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
			res = ipa3_mhi_suspend_gsi_channel(
				&ipa3_mhi_ctx->dl_channels[i]);
		else
			res = ipa3_uc_mhi_suspend_channel(
				ipa3_mhi_ctx->dl_channels[i].index);
		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		ipa3_mhi_ctx->dl_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_SUSPEND;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_resume_dl_channels(bool LPTransitionRejected)
{
	int i;
	int res;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->dl_channels[i].valid)
			continue;
		if (ipa3_mhi_ctx->dl_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("resuming channel %d\n",
			ipa3_mhi_ctx->dl_channels[i].id);
		if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
			res = gsi_start_channel(
				ipa3_mhi_ctx->dl_channels[i].ep->gsi_chan_hdl);
		else
			res = ipa3_uc_mhi_resume_channel(
				ipa3_mhi_ctx->dl_channels[i].index,
				LPTransitionRejected);
		if (res) {
			IPA_MHI_ERR("failed to suspend channel %d error %d\n",
				i, res);
			return res;
		}
		ipa3_mhi_ctx->dl_channels[i].stop_in_proc = false;
		ipa3_mhi_ctx->dl_channels[i].state =
			IPA_HW_MHI_CHANNEL_STATE_RUN;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static int ipa3_mhi_stop_event_update_dl_channels(void)
{
	int i;
	int res;

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
		return 0;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		if (!ipa3_mhi_ctx->dl_channels[i].valid)
			continue;
		if (ipa3_mhi_ctx->dl_channels[i].state !=
		    IPA_HW_MHI_CHANNEL_STATE_SUSPEND)
			continue;
		IPA_MHI_DBG("stop update event channel %d\n",
			ipa3_mhi_ctx->dl_channels[i].id);
		res = ipa3_uc_mhi_stop_event_update_channel(
			ipa3_mhi_ctx->dl_channels[i].index);
		if (res) {
			IPA_MHI_ERR("failed stop event channel %d error %d\n",
				i, res);
			return res;
		}
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

static bool ipa3_mhi_check_pending_packets_from_host(void)
{
	int i;
	int res;
	struct ipa3_mhi_channel_ctx *channel;

	IPA_MHI_FUNC_ENTRY();
	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa3_mhi_ctx->ul_channels[i];
		if (!channel->valid)
			continue;

		res = gsi_query_channel_info(channel->ep->gsi_chan_hdl,
			&channel->ch_info);
		if (res) {
			IPAERR("gsi_query_channel_info failed\n");
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

static void ipa3_mhi_update_host_ch_state(bool update_rp)
{
	int i;
	int res;
	struct ipa3_mhi_channel_ctx *channel;

	for (i = 0; i < IPA_MHI_MAX_UL_CHANNELS; i++) {
		channel = &ipa3_mhi_ctx->ul_channels[i];
		if (!channel->valid)
			continue;

		if (update_rp) {
			res = gsi_query_channel_info(channel->ep->gsi_chan_hdl,
				&channel->ch_info);
			if (res) {
				IPAERR("gsi_query_channel_info failed\n");
				BUG();
				return;
			}

			res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
				&channel->ch_info.rp,
				channel->channel_context_addr +
					offsetof(struct ipa3_mhi_ch_ctx, rp),
				sizeof(channel->ch_info.rp));
			if (res) {
				IPAERR("ipa_mhi_read_write_host failed\n");
				BUG();
				return;
			}
		}

		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
				offsetof(struct ipa3_mhi_ch_ctx, chstate),
			sizeof(channel->state));
		if (res) {
			IPAERR("ipa_mhi_read_write_host failed\n");
			BUG();
			return;
		}
	}

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa3_mhi_ctx->dl_channels[i];
		if (!channel->valid)
			continue;

		if (update_rp) {
			res = gsi_query_channel_info(channel->ep->gsi_chan_hdl,
				&channel->ch_info);
			if (res) {
				IPAERR("gsi_query_channel_info failed\n");
				BUG();
				return;
			}

			res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
				&channel->ch_info.rp,
				channel->channel_context_addr +
					offsetof(struct ipa3_mhi_ch_ctx, rp),
				sizeof(channel->ch_info.rp));
			if (res) {
				IPAERR("ipa_mhi_read_write_host failed\n");
				BUG();
				return;
			}
		}

		res = ipa_mhi_read_write_host(IPA_MHI_DMA_TO_HOST,
			&channel->state, channel->channel_context_addr +
			offsetof(struct ipa3_mhi_ch_ctx, chstate),
			sizeof(channel->state));
		if (res) {
			IPAERR("ipa_mhi_read_write_host failed\n");
			BUG();
		}
	}
}

static bool ipa3_mhi_has_open_aggr_frame(void)
{
	struct ipa3_mhi_channel_ctx *channel;
	u32 aggr_state_active;
	int i;
	int ipa_ep_idx;

	aggr_state_active = ipa_read_reg(ipa3_ctx->mmio,
		IPA_STATE_AGGR_ACTIVE_OFST);
	IPA_MHI_DBG("IPA_STATE_AGGR_ACTIVE_OFST 0x%x\n", aggr_state_active);

	for (i = 0; i < IPA_MHI_MAX_DL_CHANNELS; i++) {
		channel = &ipa3_mhi_ctx->dl_channels[i];

		if (!channel->valid)
			continue;

		ipa_ep_idx = ipa_get_ep_mapping(channel->ep->client);
		if (ipa_ep_idx == -1) {
			BUG();
			return false;
		}

		if ((1 << ipa_ep_idx) & aggr_state_active)
			return true;
	}

	return false;
}


/**
 * ipa3_mhi_suspend() - Suspend MHI accelerated channels
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
int ipa3_mhi_suspend(bool force)
{
	int res;
	bool empty;
	bool force_clear = false;

	IPA_MHI_FUNC_ENTRY();

	res = ipa3_mhi_set_state(IPA_MHI_STATE_SUSPEND_IN_PROGRESS);
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_set_state failed %d\n", res);
		return res;
	}

	res = ipa3_mhi_suspend_ul_channels();
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_suspend_ul_channels failed %d\n", res);
		goto fail_suspend_ul_channel;
	}

	empty = ipa3_mhi_wait_for_ul_empty_timeout(
			IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);

	if (!empty) {
		if (force) {
			res = ipa3_mhi_enable_force_clear(
				ipa3_mhi_ctx->qmi_req_id, false);
			if (res) {
				IPA_MHI_ERR("failed to enable force clear\n");
				BUG();
				return res;
			}
			force_clear = true;
			IPA_MHI_DBG("force clear datapath enabled\n");

			empty = ipa3_mhi_wait_for_ul_empty_timeout(
				IPA_MHI_CH_EMPTY_TIMEOUT_MSEC);
			IPADBG("empty=%d\n", empty);
		} else {
			IPA_MHI_DBG("IPA not empty\n");
			res = -EAGAIN;
			goto fail_suspend_ul_channel;
		}
	}

	if (force_clear) {
		res = ipa3_mhi_disable_force_clear(ipa3_mhi_ctx->qmi_req_id);
		if (res) {
			IPA_MHI_ERR("failed to disable force clear\n");
			BUG();
			return res;
		}
		IPA_MHI_DBG("force clear datapath disabled\n");
		ipa3_mhi_ctx->qmi_req_id++;
	}

	if (!force && ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		if (ipa3_mhi_check_pending_packets_from_host()) {
			res = -EAGAIN;
			goto fail_suspend_ul_channel;
		}
	}

	res = ipa3_mhi_stop_event_update_ul_channels();
	if (res) {
		IPA_MHI_ERR(
			"ipa3_mhi_stop_event_update_ul_channels failed %d\n",
			res);
		goto fail_suspend_ul_channel;
	}

	/*
	 * hold IPA clocks and release them after all
	 * IPA RM resource are released to make sure tag process will not start
	 */
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	IPA_MHI_DBG("release prod\n");
	res = ipa3_mhi_release_prod();
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_release_prod failed %d\n", res);
		goto fail_release_prod;
	}

	IPA_MHI_DBG("wait for cons release\n");
	res = ipa3_mhi_wait_for_cons_release();
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_wait_for_cons_release failed %d\n", res);
		goto fail_release_cons;
	}

	usleep_range(IPA_MHI_SUSPEND_SLEEP_MIN, IPA_MHI_SUSPEND_SLEEP_MAX);

	res = ipa3_mhi_suspend_dl_channels();
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_suspend_dl_channels failed %d\n", res);
		goto fail_suspend_dl_channel;
	}

	res = ipa3_mhi_stop_event_update_dl_channels();
	if (res) {
		IPA_MHI_ERR("failed to stop event update on DL %d\n", res);
		goto fail_stop_event_update_dl_channel;
	}

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		if (ipa3_mhi_has_open_aggr_frame()) {
			IPA_MHI_DBG("There is an open aggr frame\n");
			if (force) {
				ipa3_mhi_ctx->trigger_wakeup = true;
			} else {
				res = -EAGAIN;
				goto fail_stop_event_update_dl_channel;
			}
		}
	}

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
		ipa3_mhi_update_host_ch_state(true);

	if (!empty)
		ipa3_ctx->tag_process_before_gating = false;

	res = ipa3_mhi_set_state(IPA_MHI_STATE_SUSPENDED);
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_set_state failed %d\n", res);
		goto fail_release_cons;
	}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPA_MHI_FUNC_EXIT();
	return 0;

fail_stop_event_update_dl_channel:
	ipa3_mhi_resume_dl_channels(true);
fail_suspend_dl_channel:
fail_release_cons:
	ipa3_mhi_request_prod();
fail_release_prod:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
fail_suspend_ul_channel:
	ipa3_mhi_resume_ul_channels(true);
	ipa3_mhi_set_state(IPA_MHI_STATE_STARTED);
	return res;
}

/**
 * ipa3_mhi_resume() - Resume MHI accelerated channels
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
int ipa3_mhi_resume(void)
{
	int res;
	bool dl_channel_resumed = false;

	IPA_MHI_FUNC_ENTRY();

	res = ipa3_mhi_set_state(IPA_MHI_STATE_RESUME_IN_PROGRESS);
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_set_state failed %d\n", res);
		return res;
	}

	if (ipa3_mhi_ctx->rm_cons_state == IPA_MHI_RM_STATE_REQUESTED) {
		/* resume all DL channels */
		res = ipa3_mhi_resume_dl_channels(false);
		if (res) {
			IPA_MHI_ERR("ipa3_mhi_resume_dl_channels failed %d\n",
				res);
			goto fail_resume_dl_channels;
		}
		dl_channel_resumed = true;

		ipa3_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
			IPA_RM_RESOURCE_MHI_CONS);
		ipa3_mhi_ctx->rm_cons_state = IPA_MHI_RM_STATE_GRANTED;
	}

	res = ipa3_mhi_request_prod();
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_request_prod failed %d\n", res);
		goto fail_request_prod;
	}

	/* resume all UL channels */
	res = ipa3_mhi_resume_ul_channels(false);
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_resume_ul_channels failed %d\n", res);
		goto fail_resume_ul_channels;
	}

	if (!dl_channel_resumed) {
		res = ipa3_mhi_resume_dl_channels(true);
		if (res) {
			IPA_MHI_ERR("ipa3_mhi_resume_dl_channels failed %d\n",
				res);
			goto fail_resume_dl_channels2;
		}
	}

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
		ipa3_mhi_update_host_ch_state(false);

	res = ipa3_mhi_set_state(IPA_MHI_STATE_STARTED);
	if (res) {
		IPA_MHI_ERR("ipa3_mhi_set_state failed %d\n", res);
		goto fail_set_state;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_set_state:
	ipa3_mhi_suspend_dl_channels();
fail_resume_dl_channels2:
	ipa3_mhi_suspend_ul_channels();
fail_resume_ul_channels:
	ipa3_mhi_release_prod();
fail_request_prod:
	ipa3_mhi_suspend_dl_channels();
fail_resume_dl_channels:
	ipa3_mhi_set_state(IPA_MHI_STATE_SUSPENDED);
	return res;
}

static int  ipa3_mhi_destroy_channels(struct ipa3_mhi_channel_ctx *channels,
	int num_of_channels)
{
	struct ipa3_mhi_channel_ctx *channel;
	int i, res;
	u32 clnt_hdl;

	for (i = 0; i < num_of_channels; i++) {
		channel = &channels[i];
		if (!channel->valid)
			continue;
		if (channel->state == IPA_HW_MHI_CHANNEL_STATE_INVALID)
			continue;
		if (channel->state != IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
			clnt_hdl = ipa3_get_ep_mapping(channel->ep->client);
			IPA_MHI_DBG("disconnect pipe (ep: %d)\n", clnt_hdl);
			res = ipa3_mhi_disconnect_pipe(clnt_hdl);
			if (res) {
				IPAERR("failed to disconnect pipe %d, err %d\n",
					clnt_hdl, res);
				goto fail;
			}
		}

		if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
			IPA_MHI_DBG("reset event ring (hdl: %lu, ep: %d)\n",
				channel->ep->gsi_evt_ring_hdl, clnt_hdl);
			res = gsi_reset_evt_ring(channel->ep->gsi_evt_ring_hdl);
			if (res) {
				IPAERR(" failed to reset evt ring %lu, err %d\n"
					, channel->ep->gsi_evt_ring_hdl, res);
				goto fail;
			}
			res = gsi_dealloc_evt_ring(
				channel->ep->gsi_evt_ring_hdl);
			if (res) {
				IPAERR("dealloc evt ring %lu failed, err %d\n"
					, channel->ep->gsi_evt_ring_hdl, res);
				goto fail;
			}
		}
	}

	return 0;
fail:
	return res;
}

/**
 * ipa3_mhi_destroy() - Destroy MHI IPA
 *
 * This function is called by MHI client driver on MHI reset to destroy all IPA
 * MHI resources.
 * When this function returns ipa_mhi can re-initialize.
 */
void ipa3_mhi_destroy(void)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	if (!ipa3_mhi_ctx) {
		IPA_MHI_DBG("IPA MHI was not initialized, already destroyed\n");
		return;
	}
	/* reset all UL and DL acc channels and its accociated event rings */
	res = ipa3_mhi_destroy_channels(ipa3_mhi_ctx->ul_channels,
		IPA_MHI_MAX_UL_CHANNELS);
	if (res) {
		IPAERR("ipa3_mhi_destroy_channels(ul_channels) failed %d\n",
			res);
		goto fail;
	}
	IPA_MHI_DBG("All UL channels are disconnected\n");

	res = ipa3_mhi_destroy_channels(ipa3_mhi_ctx->dl_channels,
		IPA_MHI_MAX_DL_CHANNELS);
	if (res) {
		IPAERR("ipa3_mhi_destroy_channels(dl_channels) failed %d\n",
			res);
		goto fail;
	}
	IPA_MHI_DBG("All DL channels are disconnected\n");

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_SPS) {
		IPA_MHI_DBG("cleanup uC MHI\n");
		ipa3_uc_mhi_cleanup();
	}

	if (ipa3_mhi_ctx->state != IPA_MHI_STATE_INITIALIZED  &&
	    ipa3_mhi_ctx->state != IPA_MHI_STATE_READY) {
		IPA_MHI_DBG("release prod\n");
		res = ipa3_mhi_release_prod();
		if (res) {
			IPA_MHI_ERR("ipa3_mhi_release_prod failed %d\n", res);
			goto fail;
		}
		IPA_MHI_DBG("wait for cons release\n");
		res = ipa3_mhi_wait_for_cons_release();
		if (res) {
			IPAERR("ipa3_mhi_wait_for_cons_release failed %d\n",
				res);
			goto fail;
		}
		usleep_range(IPA_MHI_SUSPEND_SLEEP_MIN,
				IPA_MHI_SUSPEND_SLEEP_MAX);

		IPA_MHI_DBG("deleate dependency Q6_PROD->MHI_CONS\n");
		res = ipa3_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
			IPA_RM_RESOURCE_MHI_CONS);
		if (res) {
			IPAERR("Error deleting dependency %d->%d, res=%d\n",
			IPA_RM_RESOURCE_Q6_PROD, IPA_RM_RESOURCE_MHI_CONS, res);
			goto fail;
		}
		IPA_MHI_DBG("deleate dependency MHI_PROD->Q6_CONS\n");
		res = ipa3_rm_delete_dependency(IPA_RM_RESOURCE_MHI_PROD,
			IPA_RM_RESOURCE_Q6_CONS);
		if (res) {
			IPAERR("Error deleting dependency %d->%d, res=%d\n",
			IPA_RM_RESOURCE_MHI_PROD, IPA_RM_RESOURCE_Q6_CONS, res);
			goto fail;
		}
	}

	res = ipa3_rm_delete_resource(IPA_RM_RESOURCE_MHI_PROD);
	if (res) {
		IPAERR("Error deleting resource %d, res=%d\n",
			IPA_RM_RESOURCE_MHI_PROD, res);
		goto fail;
	}

	res = ipa3_rm_delete_resource(IPA_RM_RESOURCE_MHI_CONS);
	if (res) {
		IPAERR("Error deleting resource %d, res=%d\n",
			IPA_RM_RESOURCE_MHI_CONS, res);
		goto fail;
	}

	ipa3_mhi_debugfs_destroy();
	destroy_workqueue(ipa3_mhi_ctx->wq);
	kfree(ipa3_mhi_ctx);
	ipa3_mhi_ctx = NULL;
	IPA_MHI_DBG("IPA MHI was reset, ready for re-init\n");

	IPA_MHI_FUNC_EXIT();
	return;
fail:
	BUG();
	return;
}

/**
 * ipa3_mhi_handle_ipa_config_req() - hanle IPA CONFIG QMI message
 *
 * This function is called by by IPA QMI service to indicate that IPA CONFIG
 * message was sent from modem. IPA MHI will update this information to IPA uC
 * or will cache it until IPA MHI will be initialized.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa3_mhi_handle_ipa_config_req(struct ipa_config_req_msg_v01 *config_req)
{
	IPA_MHI_FUNC_ENTRY();

	if (ipa3_ctx->transport_prototype != IPA_TRANSPORT_TYPE_GSI) {
		ipa3_mhi_cache_dl_ul_sync_info(config_req);
		if (ipa3_mhi_ctx &&
		    ipa3_mhi_ctx->state != IPA_MHI_STATE_INITIALIZED)
			ipa3_uc_mhi_send_dl_ul_sync_info(
				ipa3_cached_dl_ul_sync_info);
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA MHI driver");
