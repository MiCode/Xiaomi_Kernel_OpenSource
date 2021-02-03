// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pci.h>
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
	struct mhi_device *mhi_dev = mhi_cntrl->mhi_dev;
	struct pci_dev *parent = to_pci_dev(mhi_cntrl->cntrl_dev);

	if (!mhi_priv)
		return -ENOMEM;

	if (parent) {
		dev_set_name(&mhi_dev->dev, "mhi_%04x_%02u.%02u.%02u",
			     parent->device, pci_domain_nr(parent->bus),
			     parent->bus->number, PCI_SLOT(parent->devfn));
		mhi_dev->name = dev_name(&mhi_dev->dev);
	}

	mhi_priv->log_buf = ipc_log_context_create(MHI_IPC_LOG_PAGES,
						   mhi_dev->name, 0);
	mhi_priv->mhi_cntrl = mhi_cntrl;

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

	if (!mhi_priv)
		return;

	mutex_lock(&mhi_bus.lock);
	list_del(&mhi_priv->node);
	mutex_unlock(&mhi_bus.lock);

	kfree(mhi_priv);
}

void *mhi_controller_get_privdata(struct mhi_controller *mhi_cntrl)
{
	struct mhi_private *mhi_priv = dev_get_drvdata(&mhi_cntrl->mhi_dev->dev);

	if (!mhi_priv)
		return NULL;

	return mhi_priv->priv_data;
}
EXPORT_SYMBOL(mhi_controller_get_privdata);

void mhi_controller_set_privdata(struct mhi_controller *mhi_cntrl, void *priv)
{
	struct mhi_private *mhi_priv = dev_get_drvdata(&mhi_cntrl->mhi_dev->dev);

	if (!mhi_priv)
		return;

	mhi_priv->priv_data = priv;
}
EXPORT_SYMBOL(mhi_controller_set_privdata);

static struct mhi_controller *find_mhi_controller_by_name(const char *name)
{
	struct mhi_private *mhi_priv, *tmp_priv;
	struct mhi_controller *mhi_cntrl;

	list_for_each_entry_safe(mhi_priv, tmp_priv, &mhi_bus.controller_list,
				 node) {
		mhi_cntrl = mhi_priv->mhi_cntrl;
		if (mhi_cntrl->mhi_dev->name && (!strcmp(name, mhi_cntrl->mhi_dev->name)))
			return mhi_cntrl;
	}

	return NULL;
}

struct mhi_controller *mhi_bdf_to_controller(u32 domain,
					     u32 bus,
					     u32 slot,
					     u32 dev_id)
{
	char name[32];

	snprintf(name, sizeof(name), "mhi_%04x_%02u.%02u.%02u", dev_id, domain,
		 bus, slot);

	return find_mhi_controller_by_name(name);
}
EXPORT_SYMBOL(mhi_bdf_to_controller);

static int mhi_notify_fatal_cb(struct device *dev, void *data)
{
	mhi_notify(to_mhi_device(dev), MHI_CB_FATAL_ERROR);

	return 0;
}

int mhi_report_error(struct mhi_controller *mhi_cntrl)
{
	enum mhi_pm_state cur_state;

	if (!mhi_cntrl)
		return -EINVAL;

	write_lock_irq(&mhi_cntrl->pm_lock);

	cur_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_SYS_ERR_DETECT);
	if (cur_state != MHI_PM_SYS_ERR_DETECT) {
		dev_err(mhi_cntrl->cntrl_dev,
			"Failed to move to state: %s from: %s\n",
			to_mhi_pm_state_str(MHI_PM_SYS_ERR_DETECT),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		return -EPERM;
	}

	/* force inactive/error state */
	mhi_cntrl->dev_state = MHI_STATE_SYS_ERR;
	wake_up_all(&mhi_cntrl->state_event);
	write_unlock_irq(&mhi_cntrl->pm_lock);

	/* Notify fatal error to all client drivers to halt processing */
	device_for_each_child(&mhi_cntrl->mhi_dev->dev, NULL,
			      mhi_notify_fatal_cb);

	return 0;
}
EXPORT_SYMBOL(mhi_report_error);

int mhi_device_configure(struct mhi_device *mhi_dev,
			 enum dma_data_direction dir,
			 struct mhi_buf *cfg_tbl,
			 int elements)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_chan *mhi_chan;
	struct mhi_event_ctxt *er_ctxt;
	struct mhi_chan_ctxt *ch_ctxt;
	int er_index, chan;

	switch (dir) {
	case DMA_TO_DEVICE:
		mhi_chan = mhi_dev->ul_chan;
		break;
	case DMA_BIDIRECTIONAL:
	case DMA_FROM_DEVICE:
	case DMA_NONE:
		mhi_chan = mhi_dev->dl_chan;
		break;
	default:
		return -EINVAL;
	}

	er_index = mhi_chan->er_index;
	chan = mhi_chan->chan;

	for (; elements > 0; elements--, cfg_tbl++) {
		/* update event context array */
		if (!strcmp(cfg_tbl->name, "ECA")) {
			er_ctxt = &mhi_cntrl->mhi_ctxt->er_ctxt[er_index];
			if (sizeof(*er_ctxt) != cfg_tbl->len) {
				MHI_ERR(
					"Invalid ECA size, expected:%zu actual%zu\n",
					sizeof(*er_ctxt), cfg_tbl->len);
				return -EINVAL;
			}
			memcpy((void *)er_ctxt, cfg_tbl->buf, sizeof(*er_ctxt));
			continue;
		}

		/* update channel context array */
		if (!strcmp(cfg_tbl->name, "CCA")) {
			ch_ctxt = &mhi_cntrl->mhi_ctxt->chan_ctxt[chan];
			if (cfg_tbl->len != sizeof(*ch_ctxt)) {
				MHI_ERR(
					"Invalid CCA size, expected:%zu actual:%zu\n",
					sizeof(*ch_ctxt), cfg_tbl->len);
				return -EINVAL;
			}
			memcpy((void *)ch_ctxt, cfg_tbl->buf, sizeof(*ch_ctxt));
			continue;
		}

		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mhi_device_configure);

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

	MHI_VERB("Entered with PM state: %s, MHI state: %s notify: %s\n",
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
		MHI_ERR("Unexpected PM state:%s after restore\n",
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

	MHI_VERB("Allowing Fast M3 transition with notify: %s\n",
		notify_clients ? "true" : "false");

	/* save the current states */
	mhi_priv->saved_pm_state = mhi_cntrl->pm_state;
	mhi_priv->saved_dev_state = mhi_cntrl->dev_state;

	/* move from M2 to M0 as device can allow the transition but not host */
	if (mhi_cntrl->pm_state == MHI_PM_M2) {
		new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M0);
		if (new_state != MHI_PM_M0) {
			MHI_ERR("Error setting to PM state: %s from: %s\n",
				to_mhi_pm_state_str(MHI_PM_M0),
				to_mhi_pm_state_str(mhi_cntrl->pm_state));
			ret = -EIO;
			goto error_suspend;
		}
	}

	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3_ENTER);
	if (new_state != MHI_PM_M3_ENTER) {
		MHI_ERR("Error setting to PM state: %s from: %s\n",
			to_mhi_pm_state_str(MHI_PM_M3_ENTER),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		ret = -EIO;
		goto error_suspend;
	}

	/* set dev_state to M3_FAST and host pm_state to M3 */
	new_state = mhi_tryset_pm_state(mhi_cntrl, MHI_PM_M3);
	if (new_state != MHI_PM_M3) {
		MHI_ERR("Error setting to PM state: %s from: %s\n",
			to_mhi_pm_state_str(MHI_PM_M3),
			to_mhi_pm_state_str(mhi_cntrl->pm_state));
		ret = -EIO;
		goto error_suspend;
	}

	mhi_cntrl->dev_state = MHI_STATE_M3_FAST;
	mhi_cntrl->M3_fast++;

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
			MHI_ERR("invalid size for SFR file\n");
			goto err;
		}
	}
	sfr_buf[info->file_size] = '\0';

	/* force sfr string to log in kernel msg */
	MHI_ERR("%s\n", sfr_buf);
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
			MHI_ERR("invalid size for file %s\n",
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
		MHI_ERR("invalid reported header size %u\n",
			rddm_header->header_size);
		return;
	}

	table_size = (rddm_header->header_size - 8) / sizeof(*table_info);
	if (!table_size) {
		MHI_ERR("invalid rddm table size %u\n", table_size);
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

	MHI_VERB("Checking BHI debug register for 0x%x\n", cookie);

	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		return false;

	ret = mhi_read_reg(mhi_cntrl, mhi_cntrl->bhi, BHI_ERRDBG2, &val);
	if (ret)
		return false;

	MHI_VERB("BHI_ERRDBG2 value:0x%x\n", val);
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

	MHI_ERR("host pm_state:%s dev_state:%s ee:%s\n",
		to_mhi_pm_state_str(mhi_cntrl->pm_state),
		TO_MHI_STATE_STR(mhi_cntrl->dev_state),
		TO_MHI_EXEC_STR(mhi_cntrl->ee));

	state = mhi_get_mhi_state(mhi_cntrl);
	ee = mhi_get_exec_env(mhi_cntrl);

	MHI_ERR("device ee: %s dev_state: %s\n", TO_MHI_EXEC_STR(ee),
		TO_MHI_STATE_STR(state));

	for (i = 0; debug_reg[i].name; i++) {
		if (!debug_reg[i].base)
			continue;
		ret = mhi_read_reg(mhi_cntrl, debug_reg[i].base,
				   debug_reg[i].offset, &val);
		MHI_ERR("reg: %s val: 0x%x, ret: %d\n", debug_reg[i].name,
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
			MHI_ERR("Return without waiting for M0\n");

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
		MHI_ERR("Did not enter M0, cur_state: %s pm_state: %s\n",
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

static int mhi_get_capability_offset(struct mhi_controller *mhi_cntrl,
				     u32 capability, u32 *offset)
{
	u32 cur_cap, next_offset;
	int ret;

	/* get the 1st supported capability offset */
	ret = mhi_read_reg_field(mhi_cntrl, mhi_cntrl->regs, MISC_OFFSET,
				 MISC_CAP_MASK, MISC_CAP_SHIFT, offset);
	if (ret)
		return ret;
	do {
		ret = mhi_read_reg_field(mhi_cntrl, mhi_cntrl->regs, *offset,
					 CAP_CAPID_MASK, CAP_CAPID_SHIFT,
					 &cur_cap);
		if (ret)
			return ret;

		if (cur_cap == capability)
			return 0;

		ret = mhi_read_reg_field(mhi_cntrl, mhi_cntrl->regs, *offset,
					 CAP_NEXT_CAP_MASK, CAP_NEXT_CAP_SHIFT,
					 &next_offset);
		if (ret)
			return ret;

		*offset = next_offset;
		if (*offset >= MHI_REG_SIZE)
			return -ENXIO;
	} while (next_offset);

	return -ENXIO;
}

/* to be used only if a single event ring with the type is present */
static int mhi_get_er_index(struct mhi_controller *mhi_cntrl,
			    enum mhi_er_data_type type)
{
	int i;
	struct mhi_event *mhi_event = mhi_cntrl->mhi_event;

	/* find event ring for requested type */
	for (i = 0; i < mhi_cntrl->total_ev_rings; i++, mhi_event++) {
		if (mhi_event->data_type == type)
			return mhi_event->er_index;
	}

	return -ENOENT;
}

static int mhi_init_bw_scale(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);
	int ret, er_index;
	u32 bw_cfg_offset;

	/* controller doesn't support dynamic bw switch */
	if (!mhi_priv->bw_scale)
		return -ENODEV;

	ret = mhi_get_capability_offset(mhi_cntrl, BW_SCALE_CAP_ID,
					&bw_cfg_offset);
	if (ret)
		return ret;

	/* No ER configured to support BW scale */
	er_index = mhi_get_er_index(mhi_cntrl, MHI_ER_BW_SCALE);
	if (er_index < 0)
		return er_index;

	bw_cfg_offset += BW_SCALE_CFG_OFFSET;

	/* advertise host support */
	mhi_write_reg(mhi_cntrl, mhi_cntrl->regs, bw_cfg_offset,
		      MHI_BW_SCALE_SETUP(er_index));

	MHI_VERB("Bandwidth scaling setup complete. Event ring:%d\n",
		er_index);

	return 0;
}

int mhi_misc_init_mmio(struct mhi_controller *mhi_cntrl)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);
	void __iomem *base = mhi_cntrl->regs;
	u32 chdb_off;
	int ret;

	/* Read channel db offset */
	ret = mhi_read_reg_field(mhi_cntrl, base, CHDBOFF, CHDBOFF_CHDBOFF_MASK,
				 CHDBOFF_CHDBOFF_SHIFT, &chdb_off);
	if (ret) {
		MHI_ERR("Unable to read CHDBOFF register\n");
		return -EIO;
	}

	mhi_priv->bw_scale_db = base + chdb_off + (8 * MHI_BW_SCALE_CHAN_DB);

	ret = mhi_init_bw_scale(mhi_cntrl);
	if (ret)
		return ret;

	return 0;
}

/* Recycle by fast forwarding WP to the last posted event */
static void mhi_recycle_fwd_ev_ring_element
		(struct mhi_controller *mhi_cntrl, struct mhi_ring *ring)
{
	dma_addr_t ctxt_wp;

	/* update the WP */
	ring->wp += ring->el_size;
	if (ring->wp >= (ring->base + ring->len))
		ring->wp = ring->base;

	/* update the context WP based on the RP to support fast forwarding */
	ctxt_wp = ring->iommu_base + (ring->wp - ring->base);
	*ring->ctxt_wp = ctxt_wp;

	/* update the RP */
	ring->rp += ring->el_size;
	if (ring->rp >= (ring->base + ring->len))
		ring->rp = ring->base;

	/* visible to other cores */
	smp_wmb();
}

/* dedicated bw scale event ring processing */
int mhi_process_misc_bw_ev_ring(struct mhi_controller *mhi_cntrl,
				struct mhi_event *mhi_event,
				u32 event_quota)
{
	struct mhi_tre *dev_rp;
	struct mhi_ring *ev_ring = &mhi_event->ring;
	struct mhi_event_ctxt *er_ctxt =
		&mhi_cntrl->mhi_ctxt->er_ctxt[mhi_event->er_index];
	struct mhi_link_info link_info, *cur_info = &mhi_cntrl->mhi_link_info;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);
	int result, ret = -EINVAL;

	if (!MHI_IN_MISSION_MODE(mhi_cntrl->ee))
		goto exit_bw_scale_process;

	spin_lock_bh(&mhi_event->lock);
	dev_rp = mhi_to_virtual(ev_ring, er_ctxt->rp);

	/* if rp points to base, we need to wrap it around */
	if (dev_rp == ev_ring->base)
		dev_rp = ev_ring->base + ev_ring->len;
	dev_rp--;

	/* fast forward to currently processed element and recycle er */
	ev_ring->rp = dev_rp;
	ev_ring->wp = dev_rp - 1;
	if (ev_ring->wp < ev_ring->base)
		ev_ring->wp = ev_ring->base + ev_ring->len - ev_ring->el_size;
	mhi_recycle_fwd_ev_ring_element(mhi_cntrl, ev_ring);

	if (WARN_ON(MHI_TRE_GET_EV_TYPE(dev_rp) != MHI_PKT_TYPE_BW_REQ_EVENT)) {
		MHI_ERR("!BW SCALE REQ event\n");
		spin_unlock_bh(&mhi_event->lock);
		goto exit_bw_scale_process;
	}

	link_info.target_link_speed = MHI_TRE_GET_EV_LINKSPEED(dev_rp);
	link_info.target_link_width = MHI_TRE_GET_EV_LINKWIDTH(dev_rp);
	link_info.sequence_num = MHI_TRE_GET_EV_BW_REQ_SEQ(dev_rp);

	MHI_VERB("Received BW_REQ with seq:%d link speed:0x%x width:0x%x\n",
		link_info.sequence_num,
		link_info.target_link_speed,
		link_info.target_link_width);

	read_lock_bh(&mhi_cntrl->pm_lock);
	if (likely(MHI_DB_ACCESS_VALID(mhi_cntrl)))
		mhi_ring_er_db(mhi_event);
	read_unlock_bh(&mhi_cntrl->pm_lock);
	spin_unlock_bh(&mhi_event->lock);

	ret = mhi_device_get_sync(mhi_cntrl->mhi_dev);
	if (ret)
		goto exit_bw_scale_process;
	mhi_cntrl->runtime_get(mhi_cntrl);

	mutex_lock(&mhi_cntrl->pm_mutex);

	ret = mhi_priv->bw_scale(mhi_cntrl, &link_info);
	if (!ret)
		*cur_info = link_info;

	result = ret ? MHI_BW_SCALE_NACK : 0;

	read_lock_bh(&mhi_cntrl->pm_lock);
	if (likely(MHI_DB_ACCESS_VALID(mhi_cntrl)))
		mhi_write_reg(mhi_cntrl, mhi_priv->bw_scale_db, 0,
			      MHI_BW_SCALE_RESULT(result,
						  link_info.sequence_num));
	read_unlock_bh(&mhi_cntrl->pm_lock);

	mhi_cntrl->runtime_put(mhi_cntrl);
	mhi_device_put(mhi_cntrl->mhi_dev);

	mutex_unlock(&mhi_cntrl->pm_mutex);

exit_bw_scale_process:
	MHI_VERB("exit er_index:%u ret:%d\n", mhi_event->er_index, ret);

	return ret;
}

void mhi_controller_set_bw_scale_cb(struct mhi_controller *mhi_cntrl,
			int (*cb_func)(struct mhi_controller *mhi_cntrl,
			struct mhi_link_info *link_info))
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);

	mhi_priv->bw_scale = cb_func;
}
EXPORT_SYMBOL(mhi_controller_set_bw_scale_cb);

void mhi_controller_set_base(struct mhi_controller *mhi_cntrl, phys_addr_t base)
{
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);

	mhi_priv->base_addr = base;
}
EXPORT_SYMBOL(mhi_controller_set_base);

int mhi_get_channel_db_base(struct mhi_device *mhi_dev, phys_addr_t *value)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);
	u32 offset;
	int ret;

	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		return -EIO;

	ret = mhi_read_reg_field(mhi_cntrl, mhi_cntrl->regs, CHDBOFF,
				 CHDBOFF_CHDBOFF_MASK, CHDBOFF_CHDBOFF_SHIFT,
				 &offset);
	if (ret)
		return -EIO;

	*value = mhi_priv->base_addr + offset;

	return ret;
}
EXPORT_SYMBOL(mhi_get_channel_db_base);

int mhi_get_event_ring_db_base(struct mhi_device *mhi_dev, phys_addr_t *value)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct device *dev = &mhi_cntrl->mhi_dev->dev;
	struct mhi_private *mhi_priv = dev_get_drvdata(dev);
	u32 offset;
	int ret;

	if (!MHI_REG_ACCESS_VALID(mhi_cntrl->pm_state))
		return -EIO;

	ret = mhi_read_reg_field(mhi_cntrl, mhi_cntrl->regs, ERDBOFF,
				 ERDBOFF_ERDBOFF_MASK, ERDBOFF_ERDBOFF_SHIFT,
				 &offset);
	if (ret)
		return -EIO;

	*value = mhi_priv->base_addr + offset;

	return ret;
}
EXPORT_SYMBOL(mhi_get_event_ring_db_base);

struct mhi_device *mhi_get_device_for_channel(struct mhi_controller *mhi_cntrl,
					      u32 channel)
{
	if (channel >= mhi_cntrl->max_chan)
		return NULL;

	return mhi_cntrl->mhi_chan[channel].mhi_dev;
}
EXPORT_SYMBOL(mhi_get_device_for_channel);

#if !IS_ENABLED(CONFIG_MHI_DTR)
long mhi_device_ioctl(struct mhi_device *mhi_dev, unsigned int cmd,
		      unsigned long arg)
{
	return -EIO;
}
EXPORT_SYMBOL(mhi_device_ioctl);
#endif
