// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include "internal.h"

struct mhi_bus mhi_bus;

void mhi_misc_init(void)
{
	mutex_init(&mhi_bus.lock);
	INIT_LIST_HEAD(&mhi_bus.controller_list);
}

void mhi_misc_exit(void)
{
	mutex_destroy(&mhi_bus.lock);
}

int mhi_misc_register_controller(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = kzalloc(sizeof(*mhi_priv), GFP_KERNEL);

	if (!mhi_priv)
		return -ENOMEM;

	mhi_priv->log_buf = ipc_log_context_create(MHI_IPC_LOG_PAGES,
					dev_name(mhi_cntrl->cntrl_dev), 0);

	/* adding it to this list only for debug purpose */
	mutex_lock(&mhi_bus.lock);
	list_add_tail(&mhi_priv->node, &mhi_bus.controller_list);
	mutex_unlock(&mhi_bus.lock);

	dev_set_drvdata(dev, mhi_priv);

	return 0;
}

void mhi_misc_unregister_controller(struct mhi_controller *mhi_cntrl)
{
	struct mhi_private *mhi_priv = dev_get_drvdata(&mhi_cntrl->mhi_dev->dev);

	mutex_lock(&mhi_bus.lock);
	list_del(&mhi_priv->node);
	mutex_unlock(&mhi_bus.lock);

	kfree(mhi_priv);
}

void mhi_set_m2_timeout_ms(struct mhi_controller *mhi_cntrl, u32 timeout)
{
	struct mhi_private *mhi_priv = dev_get_drvdata(&mhi_cntrl->mhi_dev->dev);

	mhi_priv->m2_timeout_ms = timeout;
}
EXPORT_SYMBOL(mhi_set_m2_timeout_ms);

int mhi_pm_fast_resume(struct mhi_controller *mhi_cntrl, bool notify_clients)
{
	struct mhi_chan *itr, *tmp;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);

	dev_dbg(dev, "Entered with PM state: %s, MHI state: %s notify: %s\n",
		 to_mhi_pm_state_str(mhi_cntrl->pm_state),
		 TO_MHI_STATE_STR(mhi_cntrl->dev_state),
		 notify_clients ? "true" : "false");

	if (mhi_cntrl->pm_state == MHI_PM_DISABLE)
		return 0;

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state))
		return -EIO;

	read_lock_bh(&mhi_cntrl->pm_lock);
	WARN_ON(mhi_cntrl->pm_state != MHI_PM_M3);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	/* Notify clients about exiting LPM */
	if (notify_clients) {
		list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans,
					 node) {
			mutex_lock(&itr->mutex);
			if (itr->mhi_dev)
				mhi_notify(itr->mhi_dev, MHI_CB_LPM_EXIT);
			mutex_unlock(&itr->mutex);
		}
	}

	/* disable primary event ring processing to prevent interference */
	tasklet_disable(&mhi_cntrl->mhi_event->task);

	write_lock_irq(&mhi_cntrl->pm_lock);

	/* re-check to make sure no error has occurred before proceeding */
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		write_unlock_irq(&mhi_cntrl->pm_lock);
		tasklet_enable(&mhi_cntrl->mhi_event->task);
		return -EIO;
	}

	/* restore the states */
	mhi_cntrl->pm_state = mhi_priv->saved_pm_state;
	mhi_cntrl->dev_state = mhi_priv->saved_dev_state;

	write_unlock_irq(&mhi_cntrl->pm_lock);

	switch (mhi_cntrl->pm_state) {
	case MHI_PM_M0:
		mhi_pm_m0_transition(mhi_cntrl);
		break;
	case MHI_PM_M2:
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_get(mhi_cntrl, true);
		mhi_cntrl->wake_put(mhi_cntrl, true);
		read_unlock_bh(&mhi_cntrl->pm_lock);
		break;
	default:
		dev_err(dev, "Unexpected PM state:%s after restore\n",
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
	}

	/* enable primary event ring processing and check for events */
	tasklet_enable(&mhi_cntrl->mhi_event->task);
	mhi_irq_handler(0, mhi_cntrl->mhi_event);

	return 0;
}
EXPORT_SYMBOL(mhi_pm_fast_resume);

int mhi_pm_fast_suspend(struct mhi_controller *mhi_cntrl, bool notify_clients)
{
	struct mhi_chan *itr, *tmp;
	struct mhi_device *mhi_dev = mhi_cntrl->mhi_dev;
	struct device *dev = &mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);
	enum mhi_pm_state new_state;
	int ret;

	if (mhi_cntrl->pm_state == MHI_PM_DISABLE)
		return -EINVAL;

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state))
		return -EIO;

	/* check if host/clients have any bus votes or packets to be sent */
	if (atomic_read(&mhi_cntrl->pending_pkts))
		return -EBUSY;

	/* wait for the device to attempt a low power mode (M2 entry) */
	wait_event_timeout(mhi_cntrl->state_event,
			   mhi_cntrl->dev_state == MHI_STATE_M2,
			   msecs_to_jiffies(mhi_priv->m2_timeout_ms));

	/* disable primary event ring processing to prevent interference */
	tasklet_disable(&mhi_cntrl->mhi_event->task);

	write_lock_irq(&mhi_cntrl->pm_lock);

	/* re-check if host/clients have any bus votes or packets to be sent */
	if (atomic_read(&mhi_cntrl->pending_pkts)) {
		ret = -EBUSY;
		goto error_suspend;
	}

	/* re-check to make sure no error has occurred before proceeding */
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		ret = -EIO;
		goto error_suspend;
	}

	dev_dbg(dev, "Allowing Fast M3 transition with notify: %s\n",
		notify_clients ? "true" : "false");

	/* save the current states */
	mhi_priv->saved_pm_state = mhi_cntrl->pm_state;
	mhi_priv->saved_dev_state = mhi_cntrl->dev_state;

	/* move from M2 to M0 as device can allow the transition but not host */
	if (mhi_cntrl->pm_state == MHI_PM_M2) {
		new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M0);
		if (new_state != MHI_PM_M0) {
			dev_err(dev, "Error setting to PM state: %s from: %s\n",
				to_mhi_pm_state_str(MHI_PM_M0),
				to_mhi_pm_state_str(mhi_cntrl->pm_state));
			ret = -EIO;
			goto error_suspend;
		}
	}

	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3_ENTER);
	if (new_state != MHI_PM_M3_ENTER) {
		dev_err(dev, "Error setting to PM state: %s from: %s\n",
			to_mhi_pm_state_str(MHI_PM_M3_ENTER),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		ret = -EIO;
		goto error_suspend;
	}

	/* set dev_state to M3_FAST and host pm_state to M3 */
	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3);
	if (new_state != MHI_PM_M3) {
		dev_err(dev, "Error setting to PM state: %s from: %s\n",
			to_mhi_pm_state_str(MHI_PM_M3),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		ret = -EIO;
		goto error_suspend;
	}

	mhi_cntrl->dev_state = MHI_STATE_M3_FAST;

	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* enable primary event ring processing and check for events */
	tasklet_enable(&mhi_cntrl->mhi_event->task);
	mhi_irq_handler(0, mhi_cntrl->mhi_event);

	/* Notify clients about entering LPM */
	if (notify_clients) {
		list_for_each_entry_safe(itr, tmp, &mhi_cntrl->lpm_chans,
					 node) {
			mutex_lock(&itr->mutex);
			if (itr->mhi_dev)
				mhi_notify(itr->mhi_dev, MHI_CB_LPM_ENTER);
			mutex_unlock(&itr->mutex);
		}
	}

	return 0;

error_suspend:
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* enable primary event ring processing and check for events */
	tasklet_enable(&mhi_cntrl->mhi_event->task);
	mhi_irq_handler(0, mhi_cntrl->mhi_event);

	return ret;
}
EXPORT_SYMBOL(mhi_pm_fast_suspend);

static void mhi_process_sfr(struct mhi_controller *mhi_cntrl,
			    struct file_info *info)
{
	struct mhi_buf *mhi_buf = mhi_cntrl->rddm_image->mhi_buf;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	u8 *sfr_buf, *file_offset = info->file_offset;
	u32 file_size = info->file_size;
	u32 rem_seg_len = info->rem_seg_len;
	u32 seg_idx = info->seg_idx;

	sfr_buf = kzalloc(file_size + 1, GFP_KERNEL);
	if (!sfr_buf)
		return;

	while (file_size) {
		/* file offset starting from seg base */
		if (!rem_seg_len) {
			file_offset = mhi_buf[seg_idx].buf;
			if (file_size > mhi_buf[seg_idx].len)
				rem_seg_len = mhi_buf[seg_idx].len;
			else
				rem_seg_len = file_size;
		}

		if (file_size <= rem_seg_len) {
			memcpy(sfr_buf, file_offset, file_size);
			break;
		}

		memcpy(sfr_buf, file_offset, rem_seg_len);
		sfr_buf += rem_seg_len;
		file_size -= rem_seg_len;
		rem_seg_len = 0;
		seg_idx++;
		if (seg_idx == mhi_cntrl->rddm_image->entries) {
			dev_err(dev, "invalid size for SFR file\n");
			goto err;
		}
	}
	sfr_buf[info->file_size] = '\0';

	/* force sfr string to log in kernel msg */
	dev_err(dev, "%s\n", sfr_buf);
err:
	kfree(sfr_buf);
}

static int mhi_find_next_file_offset(struct mhi_controller *mhi_cntrl,
				     struct file_info *info,
				     struct rddm_table_info *table_info)
{
	struct mhi_buf *mhi_buf = mhi_cntrl->rddm_image->mhi_buf;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;

	if (info->rem_seg_len >= table_info->size) {
		info->file_offset += table_info->size;
		info->rem_seg_len -= table_info->size;
		return 0;
	}

	info->file_size = table_info->size - info->rem_seg_len;
	info->rem_seg_len = 0;
	/* iterate over segments until eof is reached */
	while (info->file_size) {
		info->seg_idx++;
		if (info->seg_idx == mhi_cntrl->rddm_image->entries) {
			dev_err(dev, "invalid size for file %s\n",
				table_info->file_name);
			return -EINVAL;
		}
		if (info->file_size > mhi_buf[info->seg_idx].len) {
			info->file_size -= mhi_buf[info->seg_idx].len;
		} else {
			info->file_offset = mhi_buf[info->seg_idx].buf +
				info->file_size;
			info->rem_seg_len = mhi_buf[info->seg_idx].len -
				info->file_size;
			info->file_size = 0;
		}
	}

	return 0;
}

void mhi_dump_sfr(struct mhi_controller *mhi_cntrl)
{
	struct mhi_buf *mhi_buf = mhi_cntrl->rddm_image->mhi_buf;
	struct rddm_header *rddm_header =
		(struct rddm_header *)mhi_buf->buf;
	struct rddm_table_info *table_info;
	struct file_info info;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	u32 table_size, n;

	memset(&info, 0, sizeof(info));

	if (rddm_header->header_size > sizeof(*rddm_header) ||
			rddm_header->header_size < 8) {
		dev_err(dev, "invalid reported header size %u\n",
			rddm_header->header_size);
		return;
	}

	table_size = (rddm_header->header_size - 8) / sizeof(*table_info);
	if (!table_size) {
		dev_err(dev, "invalid rddm table size %u\n", table_size);
		return;
	}

	info.file_offset = (u8 *)rddm_header + rddm_header->header_size;
	info.rem_seg_len = mhi_buf[0].len - rddm_header->header_size;
	for (n = 0; n < table_size; n++) {
		table_info = &rddm_header->table_info[n];

		if (!strcmp(table_info->file_name, "Q6-SFR.bin")) {
			info.file_size = table_info->size;
			mhi_process_sfr(mhi_cntrl, &info);
			return;
		}

		if (mhi_find_next_file_offset(mhi_cntrl, &info, table_info))
			return;
	}
}
EXPORT_SYMBOL(mhi_dump_sfr);

bool mhi_scan_rddm_cookie(struct mhi_controller *mhi_cntrl, u32 cookie)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	int ret;
	u32 val;

	if (!mhi_cntrl->rddm_image || !cookie)
		return false;

	dev_dbg(dev, "Checking BHI debug register for 0x%x\n", cookie);

	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		return false;

	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_ERRDBG2, &val);
	if (ret)
		return false;

	dev_dbg(dev, "BHI_ERRDBG2 value:0x%x\n", val);
	if (val == cookie)
		return true;

	return false;
}
EXPORT_SYMBOL(mhi_scan_rddm_cookie);

void mhi_debug_reg_dump(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	enum mhi_state state;
	enum mhi_ee_type ee;
	int i, ret;
	u32 val;
	void __iomem *mhi_base = mhi_cntrl->regs;
	void __iomem *bhi_base = mhi_cntrl->bhi;
	void __iomem *bhie_base = mhi_cntrl->bhie;
	void __iomem *wake_db = mhi_cntrl->wake_db;
	struct {
		const char *name;
		int offset;
		void __iomem *base;
	} debug_reg[] = {
		{ "BHI_ERRDBG2", BHI_ERRDBG2, bhi_base},
		{ "BHI_ERRDBG3", BHI_ERRDBG3, bhi_base},
		{ "BHI_ERRDBG1", BHI_ERRDBG1, bhi_base},
		{ "BHI_ERRCODE", BHI_ERRCODE, bhi_base},
		{ "BHI_EXECENV", BHI_EXECENV, bhi_base},
		{ "BHI_STATUS", BHI_STATUS, bhi_base},
		{ "MHI_CNTRL", MHICTRL, mhi_base},
		{ "MHI_STATUS", MHISTATUS, mhi_base},
		{ "MHI_WAKE_DB", 0, wake_db},
		{ "BHIE_TXVEC_DB", BHIE_TXVECDB_OFFS, bhie_base},
		{ "BHIE_TXVEC_STATUS", BHIE_TXVECSTATUS_OFFS, bhie_base},
		{ "BHIE_RXVEC_DB", BHIE_RXVECDB_OFFS, bhie_base},
		{ "BHIE_RXVEC_STATUS", BHIE_RXVECSTATUS_OFFS, bhie_base},
		{ NULL },
	};

	dev_err(dev, "host pm_state:%s dev_state:%s ee:%s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		TO_MHI_STATE_STR(mhi_cntrl->dev_state),
		TO_MHI_EXEC_STR(mhi_cntrl->ee));

	state = mhi_get_mhi_state(mhi_cntrl);
	ee = mhi_get_exec_env(mhi_cntrl);

	dev_err(dev, "device ee: %s dev_state: %s\n", TO_MHI_EXEC_STR(ee),
		TO_MHI_STATE_STR(state));

	for (i = 0; debug_reg[i].name; i++) {
		if (!debug_reg[i].base)
			continue;
		ret = mhi_read_reg(mhi_cntrl, debug_reg[i].base,
				   debug_reg[i].offset, &val);
		dev_err(dev, "reg: %s val: 0x%x, ret: %d\n", debug_reg[i].name,
			val, ret);
	}
}
EXPORT_SYMBOL(mhi_debug_reg_dump);

int mhi_device_get_sync_atomic(struct mhi_device *mhi_dev, int timeout_us,
			       bool in_panic)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct device *dev = &mhi_dev->dev;

	read_lock_bh(&mhi_cntrl->pm_lock);
	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state)) {
		read_unlock_bh(&mhi_cntrl->pm_lock);
		return -EIO;
	}

	mhi_cntrl->wake_get(mhi_cntrl, true);
	read_unlock_bh(&mhi_cntrl->pm_lock);

	mhi_dev->dev_wake++;
	pm_wakeup_event(&mhi_cntrl->mhi_dev->dev, 0);
	mhi_cntrl->runtime_get(mhi_cntrl);

	/* Return if client doesn't want us to wait */
	if (!timeout_us) {
		if (mhi_cntrl->pm_state != MHI_PM_M0)
			dev_err(dev, "Return without waiting for M0\n");

		mhi_cntrl->runtime_put(mhi_cntrl);
		return 0;
	}

	if (in_panic) {
		while (mhi_get_mhi_state(mhi_cntrl) != MHI_STATE_M0 &&
		       !MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) &&
		       timeout_us > 0) {
			udelay(MHI_FORCE_WAKE_DELAY_US);
			timeout_us -= MHI_FORCE_WAKE_DELAY_US;
		}
	} else {
		while (mhi_cntrl->pm_state != MHI_PM_M0 &&
		       !MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) &&
		       timeout_us > 0) {
			udelay(MHI_FORCE_WAKE_DELAY_US);
			timeout_us -= MHI_FORCE_WAKE_DELAY_US;
		}
	}

	if (MHI_PM_IN_ERROR_STATE(mhi_cntrl->pm_state) || timeout_us <= 0) {
		dev_err(dev, "Did not enter M0, cur_state: %s pm_state: %s\n",
			TO_MHI_STATE_STR(mhi_cntrl->dev_state),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		read_lock_bh(&mhi_cntrl->pm_lock);
		mhi_cntrl->wake_put(mhi_cntrl, false);
		read_unlock_bh(&mhi_cntrl->pm_lock);
		mhi_dev->dev_wake--;
		mhi_cntrl->runtime_put(mhi_cntrl);
		return -ETIMEDOUT;
	}

	mhi_cntrl->runtime_put(mhi_cntrl);

	return 0;
}
EXPORT_SYMBOL(mhi_device_get_sync_atomic);
