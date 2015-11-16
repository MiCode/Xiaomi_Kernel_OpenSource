/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

#include "mhi_sys.h"

enum MHI_DEBUG_LEVEL mhi_msg_lvl = MHI_MSG_ERROR;

#ifdef CONFIG_MSM_MHI_DEBUG
	enum MHI_DEBUG_LEVEL mhi_ipc_log_lvl = MHI_MSG_VERBOSE;
#else
	enum MHI_DEBUG_LEVEL mhi_ipc_log_lvl = MHI_MSG_ERROR;
#endif

unsigned int mhi_log_override;

module_param(mhi_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_msg_lvl, "dbg lvl");

module_param(mhi_ipc_log_lvl, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_ipc_log_lvl, "dbg lvl");

static ssize_t mhi_dbgfs_chan_read(struct file *fp, char __user *buf,
				size_t count, loff_t *offp)
{
	int amnt_copied = 0;
	struct mhi_chan_ctxt *chan_ctxt;
	struct mhi_device_ctxt *mhi_dev_ctxt =
		&mhi_devices.device_list[0].mhi_ctxt;
	uintptr_t v_wp_index;
	uintptr_t v_rp_index;
	int valid_chan = 0;
	struct mhi_chan_ctxt *cc_list;
	struct mhi_client_handle *client_handle;

	if (NULL == mhi_dev_ctxt)
		return -EIO;
	cc_list = mhi_dev_ctxt->dev_space.ring_ctxt.cc_list;
	*offp = (u32)(*offp) % MHI_MAX_CHANNELS;

	while (!valid_chan) {
		client_handle = mhi_dev_ctxt->client_handle_list[*offp];
		if (*offp == (MHI_MAX_CHANNELS - 1))
			msleep(1000);
		if (!VALID_CHAN_NR(*offp) ||
		    !cc_list[*offp].mhi_trb_ring_base_addr ||
		    !client_handle) {
			*offp += 1;
			*offp = (u32)(*offp) % MHI_MAX_CHANNELS;
			continue;
		}
		valid_chan = 1;
	}

	chan_ctxt = &cc_list[*offp];
	get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[*offp],
			mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].rp,
			&v_rp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_chan_ctxt[*offp],
			mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].wp,
			&v_wp_index);

	amnt_copied =
	scnprintf(mhi_dev_ctxt->chan_info,
		MHI_LOG_SIZE,
		"%s0x%x %s %d %s 0x%x %s 0x%llx %s %p %s %p %s %lu %s %p %s %lu %s %d %s %d %s %u\n",
		"chan:",
		(unsigned int)*offp,
		"pkts from dev:",
		mhi_dev_ctxt->counters.chan_pkts_xferd[*offp],
		"state:",
		chan_ctxt->mhi_chan_state,
		"p_base:",
		chan_ctxt->mhi_trb_ring_base_addr,
		"v_base:",
		mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].base,
		"v_wp:",
		mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].wp,
		"index:",
		v_wp_index,
		"v_rp:",
		mhi_dev_ctxt->mhi_local_chan_ctxt[*offp].rp,
		"index:",
		v_rp_index,
		"pkts_queued",
		get_nr_avail_ring_elements(
		&mhi_dev_ctxt->mhi_local_chan_ctxt[*offp]),
		"/",
		client_handle->chan_info.max_desc,
		"bb_used:",
		mhi_dev_ctxt->counters.bb_used[*offp]);

	*offp += 1;

	if (amnt_copied < count)
		return amnt_copied -
			copy_to_user(buf, mhi_dev_ctxt->chan_info, amnt_copied);
	else
		return -ENOMEM;
}

static const struct file_operations mhi_dbgfs_chan_fops = {
	.read = mhi_dbgfs_chan_read,
	.write = NULL,
};

static ssize_t mhi_dbgfs_ev_read(struct file *fp, char __user *buf,
				size_t count, loff_t *offp)
{
	int amnt_copied = 0;
	int event_ring_index = 0;
	struct mhi_event_ctxt *ev_ctxt;
	uintptr_t v_wp_index;
	uintptr_t v_rp_index;
	uintptr_t device_p_rp_index;

	struct mhi_device_ctxt *mhi_dev_ctxt =
		&mhi_devices.device_list[0].mhi_ctxt;
	if (NULL == mhi_dev_ctxt)
		return -EIO;
	*offp = (u32)(*offp) % mhi_dev_ctxt->mmio_info.nr_event_rings;
	event_ring_index = *offp;
	ev_ctxt = &mhi_dev_ctxt->dev_space.ring_ctxt.ec_list[event_ring_index];
	if (*offp == (mhi_dev_ctxt->mmio_info.nr_event_rings - 1))
		msleep(1000);

	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].rp,
			&v_rp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].wp,
			&v_wp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].wp,
			&v_wp_index);
	get_element_index(&mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index],
			(void *)mhi_p2v_addr(mhi_dev_ctxt,
					MHI_RING_TYPE_EVENT_RING,
					event_ring_index,
					ev_ctxt->mhi_event_read_ptr),
					&device_p_rp_index);

	amnt_copied =
	scnprintf(mhi_dev_ctxt->chan_info,
		MHI_LOG_SIZE,
		"%s 0x%d %s %02x %s 0x%08x %s 0x%08x %s 0x%llx %s %llx %s %lu %s %p %s %p %s %lu %s %p %s %lu\n",
		"Event Context ",
		(unsigned int)event_ring_index,
		"Intmod_T",
		MHI_GET_EV_CTXT(EVENT_CTXT_INTMODT, ev_ctxt),
		"MSI Vector",
		ev_ctxt->mhi_msi_vector,
		"MSI RX Count",
		mhi_dev_ctxt->counters.msi_counter[*offp],
		"p_base:",
		ev_ctxt->mhi_event_ring_base_addr,
		"p_rp:",
		ev_ctxt->mhi_event_read_ptr,
		"index:",
		device_p_rp_index,
		"v_base:",
		mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].base,
		"v_wp:",
		mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].wp,
		"index:",
		v_wp_index,
		"v_rp:",
		mhi_dev_ctxt->mhi_local_event_ctxt[event_ring_index].rp,
		"index:",
		v_rp_index);

	*offp += 1;
	if (amnt_copied < count)
		return amnt_copied -
			copy_to_user(buf, mhi_dev_ctxt->chan_info, amnt_copied);
	else
		return -ENOMEM;
}

static const struct file_operations mhi_dbgfs_ev_fops = {
	.read = mhi_dbgfs_ev_read,
	.write = NULL,
};

static ssize_t mhi_dbgfs_trigger_msi(struct file *fp, const char __user *buf,
				size_t count, loff_t *offp)
{
	u32 msi_nr = 0;
	void *irq_ctxt = &((mhi_devices.device_list[0]).pcie_device->dev);

	if (copy_from_user(&msi_nr, buf, sizeof(msi_nr)))
		return -ENOMEM;
	mhi_msi_handlr(msi_nr, irq_ctxt);
	return 0;
}

static const struct file_operations mhi_dbgfs_trigger_msi_fops = {
	.read = NULL,
	.write = mhi_dbgfs_trigger_msi,
};

static ssize_t mhi_dbgfs_state_read(struct file *fp, char __user *buf,
				size_t count, loff_t *offp)
{
	int amnt_copied = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt =
		&mhi_devices.device_list[0].mhi_ctxt;
	if (NULL == mhi_dev_ctxt)
		return -EIO;
	msleep(100);
	amnt_copied =
	scnprintf(mhi_dev_ctxt->chan_info,
			MHI_LOG_SIZE,
			"%s %u %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d, %s, %d, %s %d\n",
			"Our State:",
			mhi_dev_ctxt->mhi_state,
			"M0->M1:",
			mhi_dev_ctxt->counters.m0_m1,
			"M0<-M1:",
			mhi_dev_ctxt->counters.m1_m0,
			"M1->M2:",
			mhi_dev_ctxt->counters.m1_m2,
			"M0<-M2:",
			mhi_dev_ctxt->counters.m2_m0,
			"M0->M3:",
			mhi_dev_ctxt->counters.m0_m3,
			"M0<-M3:",
			mhi_dev_ctxt->counters.m3_m0,
			"M3_ev_TO:",
			mhi_dev_ctxt->counters.m3_event_timeouts,
			"M0_ev_TO:",
			mhi_dev_ctxt->counters.m0_event_timeouts,
			"MSI_d:",
			mhi_dev_ctxt->counters.msi_disable_cntr,
			"MSI_e:",
			mhi_dev_ctxt->counters.msi_enable_cntr,
			"outstanding_acks:",
			atomic_read(&mhi_dev_ctxt->counters.outbound_acks),
			"LPM:",
			mhi_dev_ctxt->enable_lpm);
	if (amnt_copied < count)
		return amnt_copied - copy_to_user(buf,
				mhi_dev_ctxt->chan_info, amnt_copied);
	else
		return -ENOMEM;
}

static const struct file_operations mhi_dbgfs_state_fops = {
	.read = mhi_dbgfs_state_read,
	.write = NULL,
};

int mhi_init_debugfs(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	struct dentry *mhi_chan_stats;
	struct dentry *mhi_state_stats;
	struct dentry *mhi_msi_trigger;
	struct dentry *mhi_ev_stats;

	mhi_dev_ctxt->mhi_parent_folder =
					debugfs_create_dir("mhi", NULL);
	if (mhi_dev_ctxt->mhi_parent_folder == NULL) {
		mhi_log(MHI_MSG_INFO, "Failed to create debugfs parent dir.\n");
		return -EIO;
	}
	mhi_chan_stats = debugfs_create_file("mhi_chan_stats",
					0444,
					mhi_dev_ctxt->mhi_parent_folder,
					mhi_dev_ctxt,
					&mhi_dbgfs_chan_fops);
	if (mhi_chan_stats == NULL)
		return -ENOMEM;
	mhi_ev_stats = debugfs_create_file("mhi_ev_stats",
					0444,
					mhi_dev_ctxt->mhi_parent_folder,
					mhi_dev_ctxt,
					&mhi_dbgfs_ev_fops);
	if (mhi_ev_stats == NULL)
		goto clean_chan;
	mhi_state_stats = debugfs_create_file("mhi_state_stats",
					0444,
					mhi_dev_ctxt->mhi_parent_folder,
					mhi_dev_ctxt,
					&mhi_dbgfs_state_fops);
	if (mhi_state_stats == NULL)
		goto clean_ev_stats;
	mhi_msi_trigger = debugfs_create_file("mhi_msi_trigger",
					0444,
					mhi_dev_ctxt->mhi_parent_folder,
					mhi_dev_ctxt,
					&mhi_dbgfs_trigger_msi_fops);
	if (mhi_msi_trigger == NULL)
		goto clean_state;

	mhi_dev_ctxt->chan_info = kmalloc(MHI_LOG_SIZE, GFP_KERNEL);
	if (mhi_dev_ctxt->chan_info == NULL)
		goto clean_all;
	return 0;
clean_all:
	debugfs_remove(mhi_msi_trigger);
clean_state:
	debugfs_remove(mhi_state_stats);
clean_ev_stats:
	debugfs_remove(mhi_ev_stats);
clean_chan:
	debugfs_remove(mhi_chan_stats);
	debugfs_remove(mhi_dev_ctxt->mhi_parent_folder);
	return -ENOMEM;
}

uintptr_t mhi_p2v_addr(struct mhi_device_ctxt *mhi_dev_ctxt,
			enum MHI_RING_TYPE type,
			u32 chan, uintptr_t phy_ptr)
{
	uintptr_t virtual_ptr;
	struct mhi_ring_ctxt *cs = &mhi_dev_ctxt->dev_space.ring_ctxt;

	switch (type) {
	case MHI_RING_TYPE_EVENT_RING:
		 virtual_ptr = (uintptr_t)((phy_ptr -
		(uintptr_t)cs->ec_list[chan].mhi_event_ring_base_addr)
			+ mhi_dev_ctxt->mhi_local_event_ctxt[chan].base);
		break;
	case MHI_RING_TYPE_XFER_RING:
		virtual_ptr = (uintptr_t)((phy_ptr -
		(uintptr_t)cs->cc_list[chan].mhi_trb_ring_base_addr)
				+ mhi_dev_ctxt->mhi_local_chan_ctxt[chan].base);
		 break;
	case MHI_RING_TYPE_CMD_RING:
		virtual_ptr = (uintptr_t)((phy_ptr -
		(uintptr_t)cs->cmd_ctxt[chan].mhi_cmd_ring_base_addr)
				+ mhi_dev_ctxt->mhi_local_cmd_ctxt[chan].base);
		break;
	default:
		break;
		}
	return virtual_ptr;
}

dma_addr_t mhi_v2p_addr(struct mhi_device_ctxt *mhi_dev_ctxt,
			enum MHI_RING_TYPE type,
			 u32 chan, uintptr_t va_ptr)
{
	dma_addr_t phy_ptr;
	struct mhi_ring_ctxt *cs = &mhi_dev_ctxt->dev_space.ring_ctxt;

	switch (type) {
	case MHI_RING_TYPE_EVENT_RING:
		phy_ptr = (dma_addr_t)((va_ptr -
		(uintptr_t)mhi_dev_ctxt->mhi_local_event_ctxt[chan].base) +
		(uintptr_t)cs->ec_list[chan].mhi_event_ring_base_addr);
		break;
	case MHI_RING_TYPE_XFER_RING:
		phy_ptr = (dma_addr_t)((va_ptr -
		(uintptr_t)mhi_dev_ctxt->mhi_local_chan_ctxt[chan].base) +
		((uintptr_t)cs->cc_list[chan].mhi_trb_ring_base_addr));
		break;
	case MHI_RING_TYPE_CMD_RING:
		phy_ptr = (dma_addr_t)((va_ptr -
	(uintptr_t)mhi_dev_ctxt->mhi_local_cmd_ctxt[chan].base) +
	((uintptr_t)cs->cmd_ctxt[chan].mhi_cmd_ring_base_addr));
		break;
	default:
		break;
		}
		return phy_ptr;
}
