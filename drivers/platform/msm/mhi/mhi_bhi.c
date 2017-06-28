/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include "mhi_sys.h"
#include "mhi.h"
#include "mhi_macros.h"
#include "mhi_hwio.h"
#include "mhi_bhi.h"

static int bhi_open(struct inode *mhi_inode, struct file *file_handle)
{
	struct mhi_device_ctxt *mhi_dev_ctxt;

	mhi_dev_ctxt = container_of(mhi_inode->i_cdev,
				    struct mhi_device_ctxt,
				    bhi_ctxt.cdev);
	file_handle->private_data = mhi_dev_ctxt;
	return 0;
}

static int bhi_alloc_bhie_xfer(struct mhi_device_ctxt *mhi_dev_ctxt,
			       size_t size,
			       struct bhie_vec_table *vec_table)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	struct device *dev = &mhi_dev_ctxt->plat_dev->dev;
	const phys_addr_t align = bhi_ctxt->alignment - 1;
	size_t seg_size = bhi_ctxt->firmware_info.segment_size;
	/* We need one additional entry for Vector Table */
	int segments = DIV_ROUND_UP(size, seg_size) + 1;
	int i;
	struct scatterlist *sg_list;
	struct bhie_mem_info *bhie_mem_info, *info;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Total size:%lu total_seg:%d seg_size:%lu\n",
		size, segments, seg_size);

	sg_list = kcalloc(segments, sizeof(*sg_list), GFP_KERNEL);
	if (!sg_list)
		return -ENOMEM;

	bhie_mem_info = kcalloc(segments, sizeof(*bhie_mem_info), GFP_KERNEL);
	if (!bhie_mem_info)
		goto alloc_bhi_mem_info_error;

	/* Allocate buffers for bhi/e vector table */
	for (i = 0; i < segments; i++) {
		size_t size = seg_size;

		/* Last entry if for vector table */
		if (i == segments - 1)
			size = sizeof(struct bhi_vec_entry) * i;
		info = &bhie_mem_info[i];
		info->size = size;
		info->alloc_size = info->size + align;
		info->pre_aligned =
			dma_alloc_coherent(dev, info->alloc_size,
					   &info->dma_handle, GFP_KERNEL);
		if (!info->pre_aligned)
			goto alloc_dma_error;

		info->phys_addr = (info->dma_handle + align) & ~align;
		info->aligned = info->pre_aligned +
			(info->phys_addr - info->dma_handle);
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Seg:%d unaligned Img: 0x%llx aligned:0x%llx\n",
			i, info->dma_handle, info->phys_addr);
	}

	sg_init_table(sg_list, segments);
	sg_set_buf(sg_list, info->aligned, info->size);
	sg_dma_address(sg_list) = info->phys_addr;
	sg_dma_len(sg_list) = info->size;
	vec_table->sg_list = sg_list;
	vec_table->bhie_mem_info = bhie_mem_info;
	vec_table->bhi_vec_entry = info->aligned;
	vec_table->segment_count = segments;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"BHI/E table successfully allocated\n");
	return 0;

alloc_dma_error:
	for (i = i - 1; i >= 0; i--)
		dma_free_coherent(dev,
				  bhie_mem_info[i].alloc_size,
				  bhie_mem_info[i].pre_aligned,
				  bhie_mem_info[i].dma_handle);
	kfree(bhie_mem_info);
alloc_bhi_mem_info_error:
	kfree(sg_list);
	return -ENOMEM;
}

static int bhi_alloc_pbl_xfer(struct mhi_device_ctxt *mhi_dev_ctxt,
			      struct bhie_mem_info *const mem_info,
			      size_t size)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	const phys_addr_t align_len = bhi_ctxt->alignment;
	struct device *dev = &mhi_dev_ctxt->plat_dev->dev;

	mem_info->size = size;
	mem_info->alloc_size = size + (align_len - 1);
	mem_info->pre_aligned =
		dma_alloc_coherent(dev, mem_info->alloc_size,
				   &mem_info->dma_handle, GFP_KERNEL);
	if (mem_info->pre_aligned == NULL)
		return -ENOMEM;

	mem_info->phys_addr = (mem_info->dma_handle + (align_len - 1)) &
		~(align_len - 1);
	mem_info->aligned = mem_info->pre_aligned + (mem_info->phys_addr -
						     mem_info->dma_handle);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"alloc_size:%lu image_size:%lu unal_addr:0x%llx0x al_addr:0x%llx\n",
		mem_info->alloc_size, mem_info->size,
		mem_info->dma_handle, mem_info->phys_addr);

	return 0;
}

/* transfer firmware or ramdump via bhie protocol */
static int bhi_bhie_transfer(struct mhi_device_ctxt *mhi_dev_ctxt,
			     struct bhie_vec_table *vec_table,
			     bool tx_vec_table)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	/* last element is the vector table */
	const struct bhie_mem_info *bhie_mem_info =
		&vec_table->bhie_mem_info[vec_table->segment_count - 1];
	u32 val;
	const u32 tx_sequence = vec_table->sequence++;
	unsigned long timeout;
	rwlock_t *pm_xfer_lock = &mhi_dev_ctxt->pm_xfer_lock;
	unsigned bhie_vecaddr_high_offs, bhie_vecaddr_low_offs,
		bhie_vecsize_offs, bhie_vecdb_offs,
		bhie_vecstatus_offs;

	if (tx_vec_table) {
		bhie_vecaddr_high_offs = BHIE_TXVECADDR_HIGH_OFFS;
		bhie_vecaddr_low_offs = BHIE_TXVECADDR_LOW_OFFS;
		bhie_vecsize_offs = BHIE_TXVECSIZE_OFFS;
		bhie_vecdb_offs = BHIE_TXVECDB_OFFS;
		bhie_vecstatus_offs = BHIE_TXVECSTATUS_OFFS;
	} else {
		bhie_vecaddr_high_offs = BHIE_RXVECADDR_HIGH_OFFS;
		bhie_vecaddr_low_offs = BHIE_RXVECADDR_LOW_OFFS;
		bhie_vecsize_offs = BHIE_RXVECSIZE_OFFS;
		bhie_vecdb_offs = BHIE_RXVECDB_OFFS;
		bhie_vecstatus_offs = BHIE_RXVECSTATUS_OFFS;
	}

	/* Program TX/RX Vector table */
	read_lock_bh(pm_xfer_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
		read_unlock_bh(pm_xfer_lock);
		return -EIO;
	}

	val = HIGH_WORD(bhie_mem_info->phys_addr);
	mhi_reg_write(mhi_dev_ctxt, bhi_ctxt->bhi_base,
		      bhie_vecaddr_high_offs, val);
	val = LOW_WORD(bhie_mem_info->phys_addr);
	mhi_reg_write(mhi_dev_ctxt, bhi_ctxt->bhi_base,
		      bhie_vecaddr_low_offs, val);
	val = (u32)bhie_mem_info->size;
	mhi_reg_write(mhi_dev_ctxt, bhi_ctxt->bhi_base, bhie_vecsize_offs, val);

	/* Ring DB to begin Xfer */
	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base, bhie_vecdb_offs,
			    BHIE_TXVECDB_SEQNUM_BMSK, BHIE_TXVECDB_SEQNUM_SHFT,
			    tx_sequence);
	read_unlock_bh(pm_xfer_lock);

	timeout = jiffies + msecs_to_jiffies(bhi_ctxt->poll_timeout);
	while (time_before(jiffies, timeout)) {
		u32 current_seq, status;

		read_lock_bh(pm_xfer_lock);
		if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
			read_unlock_bh(pm_xfer_lock);
			return -EIO;
		}
		val = mhi_reg_read(bhi_ctxt->bhi_base, bhie_vecstatus_offs);
		read_unlock_bh(pm_xfer_lock);
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"%sVEC_STATUS:0x%x\n", tx_vec_table ? "TX" : "RX", val);
		current_seq = (val & BHIE_TXVECSTATUS_SEQNUM_BMSK) >>
			BHIE_TXVECSTATUS_SEQNUM_SHFT;
		status = (val & BHIE_TXVECSTATUS_STATUS_BMSK) >>
			BHIE_TXVECSTATUS_STATUS_SHFT;
		if ((status == BHIE_TXVECSTATUS_STATUS_XFER_COMPL) &&
		    (current_seq == tx_sequence)) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"%s transfer complete\n",
				tx_vec_table ? "image" : "rddm");
			return 0;
		}
		msleep(BHI_POLL_SLEEP_TIME_MS);
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
		"Error xfer %s via BHIE\n", tx_vec_table ? "image" : "rddm");
	return -EIO;
}

static int bhi_rddm_graceful(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret;
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	struct bhie_vec_table *rddm_table = &bhi_ctxt->rddm_table;
	enum MHI_EXEC_ENV exec_env = mhi_dev_ctxt->dev_exec_env;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Entered with pm_state:0x%x exec_env:0x%x mhi_state:%s\n",
		mhi_dev_ctxt->mhi_pm_state, exec_env,
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state));

	if (exec_env != MHI_EXEC_ENV_RDDM) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Not in RDDM exec env, exec_env:0x%x\n", exec_env);
		return -EIO;
	}

	ret = bhi_bhie_transfer(mhi_dev_ctxt, rddm_table, false);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "rddm transfer status:%d\n", ret);
	return ret;
}

/* collect ramdump from device using bhie protocol */
int bhi_rddm(struct mhi_device_ctxt *mhi_dev_ctxt, bool in_panic)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	struct bhie_vec_table *rddm_table = &bhi_ctxt->rddm_table;
	struct bhie_mem_info *bhie_mem_info;
	u32 rx_sequence, val, current_seq;
	u32 timeout = (bhi_ctxt->poll_timeout * 1000) / BHIE_RDDM_DELAY_TIME_US;
	int i;
	u32 cur_exec, prev_exec = 0;
	u32 state, prev_state = 0;
	u32 rx_status, prev_status = 0;

	if (!rddm_table->bhie_mem_info) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "RDDM table == NULL\n");
		return -ENOMEM;
	}

	if (!in_panic)
		return bhi_rddm_graceful(mhi_dev_ctxt);

	/*
	 * Below code should only be executed during kernel panic,
	 * we expect other cores to be shutting down while we're
	 * executing rddm transfer. After returning from this function,
	 * we expect device to reset.
	 */

	/* Trigger device into RDDM */
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "pm_state:0x%x mhi_state:%s\n",
		mhi_dev_ctxt->mhi_pm_state,
		TO_MHI_STATE_STR(mhi_dev_ctxt->mhi_state));
	if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Register access not allowed\n");
		return -EIO;
	}

	/*
	 * Normally we only set mhi_pm_state after grabbing pm_xfer_lock as a
	 * write, by function mhi_tryset_pm_state. Since we're in a kernel
	 * panic, we will set pm state w/o grabbing xfer lock. We're setting
	 * pm_state to LD as a safety precautions. If another core in middle
	 * of register access this should deter it. However, there is no
	 * no gurantee change will take effect.
	 */
	mhi_dev_ctxt->mhi_pm_state = MHI_PM_LD_ERR_FATAL_DETECT;
	/* change should take effect immediately */
	smp_wmb();

	bhie_mem_info = &rddm_table->
		bhie_mem_info[rddm_table->segment_count - 1];
	rx_sequence = rddm_table->sequence++;

	/* program the vector table */
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Programming RXVEC table\n");
	val = HIGH_WORD(bhie_mem_info->phys_addr);
	mhi_reg_write(mhi_dev_ctxt, bhi_ctxt->bhi_base,
		      BHIE_RXVECADDR_HIGH_OFFS, val);
	val = LOW_WORD(bhie_mem_info->phys_addr);
	mhi_reg_write(mhi_dev_ctxt, bhi_ctxt->bhi_base, BHIE_RXVECADDR_LOW_OFFS,
		      val);
	val = (u32)bhie_mem_info->size;
	mhi_reg_write(mhi_dev_ctxt, bhi_ctxt->bhi_base, BHIE_RXVECSIZE_OFFS,
		      val);
	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base, BHIE_RXVECDB_OFFS,
			    BHIE_TXVECDB_SEQNUM_BMSK, BHIE_TXVECDB_SEQNUM_SHFT,
			    rx_sequence);

	/* trigger device into rddm */
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Triggering Device into RDDM mode\n");
	mhi_set_m_state(mhi_dev_ctxt, MHI_STATE_SYS_ERR);
	i = 0;

	while (timeout--) {
		cur_exec = mhi_reg_read(bhi_ctxt->bhi_base, BHI_EXECENV);
		state = mhi_get_m_state(mhi_dev_ctxt);
		rx_status = mhi_reg_read(bhi_ctxt->bhi_base,
					 BHIE_RXVECSTATUS_OFFS);
		/* if reg. values changed or each sec (udelay(1000)) log it */
		if (cur_exec != prev_exec || state != prev_state ||
		    rx_status != prev_status || !(i & (SZ_1K - 1))) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"EXECENV:0x%x MHISTATE:0x%x RXSTATUS:0x%x\n",
				cur_exec, state, rx_status);
			prev_exec = cur_exec;
			prev_state = state;
			prev_status = rx_status;
		};
		current_seq = (rx_status & BHIE_TXVECSTATUS_SEQNUM_BMSK) >>
			BHIE_TXVECSTATUS_SEQNUM_SHFT;
		rx_status = (rx_status & BHIE_TXVECSTATUS_STATUS_BMSK) >>
			BHIE_TXVECSTATUS_STATUS_SHFT;

		if ((rx_status == BHIE_TXVECSTATUS_STATUS_XFER_COMPL) &&
		    (current_seq == rx_sequence)) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"rddm transfer completed\n");
			return 0;
		}
		udelay(BHIE_RDDM_DELAY_TIME_US);
		i++;
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR, "rddm transfer timeout\n");

	return -EIO;
}

static int bhi_load_firmware(struct mhi_device_ctxt *mhi_dev_ctxt,
			     const struct bhie_mem_info  *const mem_info)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	u32 pcie_word_val = 0;
	u32 tx_db_val = 0;
	unsigned long timeout;
	rwlock_t *pm_xfer_lock = &mhi_dev_ctxt->pm_xfer_lock;

	/* Write the image size */
	read_lock_bh(pm_xfer_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
		read_unlock_bh(pm_xfer_lock);
		return -EIO;
	}
	pcie_word_val = HIGH_WORD(mem_info->phys_addr);
	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base, BHI_IMGADDR_HIGH,
			    0xFFFFFFFF, 0, pcie_word_val);

	pcie_word_val = LOW_WORD(mem_info->phys_addr);
	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base, BHI_IMGADDR_LOW,
			    0xFFFFFFFF, 0, pcie_word_val);

	pcie_word_val = mem_info->size;
	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base, BHI_IMGSIZE,
			    0xFFFFFFFF, 0, pcie_word_val);

	pcie_word_val = mhi_reg_read(bhi_ctxt->bhi_base, BHI_IMGTXDB);
	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base,
			    BHI_IMGTXDB, 0xFFFFFFFF, 0, ++pcie_word_val);
	read_unlock_bh(pm_xfer_lock);
	timeout = jiffies + msecs_to_jiffies(bhi_ctxt->poll_timeout);
	while (time_before(jiffies, timeout)) {
		u32 err = 0, errdbg1 = 0, errdbg2 = 0, errdbg3 = 0;

		read_lock_bh(pm_xfer_lock);
		if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
			read_unlock_bh(pm_xfer_lock);
			return -EIO;
		}
		err = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRCODE);
		errdbg1 = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRDBG1);
		errdbg2 = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRDBG2);
		errdbg3 = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRDBG3);
		tx_db_val = mhi_reg_read_field(bhi_ctxt->bhi_base,
						BHI_STATUS,
						BHI_STATUS_MASK,
						BHI_STATUS_SHIFT);
		read_unlock_bh(pm_xfer_lock);
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"%s 0x%x %s:0x%x %s:0x%x %s:0x%x %s:0x%x\n",
			"BHI STATUS", tx_db_val,
			"err", err,
			"errdbg1", errdbg1,
			"errdbg2", errdbg2,
			"errdbg3", errdbg3);
		if (tx_db_val == BHI_STATUS_SUCCESS)
			break;
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "retrying...\n");
		msleep(BHI_POLL_SLEEP_TIME_MS);
	}

	return (tx_db_val == BHI_STATUS_SUCCESS) ? 0 : -EIO;
}

static ssize_t bhi_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *offp)
{
	int ret_val = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = file->private_data;
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	struct bhie_mem_info mem_info;
	long timeout;

	if (buf == NULL || 0 == count)
		return -EIO;

	if (count > BHI_MAX_IMAGE_SIZE)
		return -ENOMEM;

	ret_val = bhi_alloc_pbl_xfer(mhi_dev_ctxt, &mem_info, count);
	if (ret_val)
		return -ENOMEM;

	if (copy_from_user(mem_info.aligned, buf, count)) {
		ret_val = -ENOMEM;
		goto bhi_copy_error;
	}

	timeout = wait_event_interruptible_timeout(
				*mhi_dev_ctxt->mhi_ev_wq.bhi_event,
				mhi_dev_ctxt->mhi_state == MHI_STATE_BHI,
				msecs_to_jiffies(bhi_ctxt->poll_timeout));
	if (timeout <= 0 && mhi_dev_ctxt->mhi_state != MHI_STATE_BHI) {
		ret_val = -EIO;
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Timed out waiting for BHI\n");
		goto bhi_copy_error;
	}

	ret_val = bhi_load_firmware(mhi_dev_ctxt, &mem_info);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to load bhi image\n");
	}
	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev, mem_info.alloc_size,
			  mem_info.pre_aligned, mem_info.dma_handle);

	/* Regardless of failure set to RESET state */
	ret_val = mhi_init_state_transition(mhi_dev_ctxt,
					STATE_TRANSITION_RESET);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to start state change event\n");
	}
	return count;

bhi_copy_error:
	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev, mem_info.alloc_size,
			  mem_info.pre_aligned, mem_info.dma_handle);

	return ret_val;
}

static const struct file_operations bhi_fops = {
	.write = bhi_write,
	.open = bhi_open,
};

int bhi_expose_dev_bhi(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret_val;
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	const struct pcie_core_info *core = &mhi_dev_ctxt->core;
	char node_name[32];

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Creating dev node\n");

	ret_val = alloc_chrdev_region(&bhi_ctxt->bhi_dev, 0, 1, "bhi");
	if (IS_ERR_VALUE(ret_val)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to alloc char device %d\n", ret_val);
		return -EIO;
	}
	cdev_init(&bhi_ctxt->cdev, &bhi_fops);
	bhi_ctxt->cdev.owner = THIS_MODULE;
	ret_val = cdev_add(&bhi_ctxt->cdev, bhi_ctxt->bhi_dev, 1);
	snprintf(node_name, sizeof(node_name),
		 "bhi_%04X_%02u.%02u.%02u",
		 core->dev_id, core->domain, core->bus, core->slot);
	bhi_ctxt->dev = device_create(mhi_device_drv->mhi_bhi_class,
				      NULL,
				      bhi_ctxt->bhi_dev,
				      NULL,
				      node_name);
	if (IS_ERR(bhi_ctxt->dev)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to add bhi cdev\n");
		ret_val = PTR_RET(bhi_ctxt->dev);
		goto err_dev_create;
	}
	return 0;

err_dev_create:
	cdev_del(&bhi_ctxt->cdev);
	unregister_chrdev_region(MAJOR(bhi_ctxt->bhi_dev), 1);
	return ret_val;
}

void bhi_firmware_download(struct work_struct *work)
{
	struct mhi_device_ctxt *mhi_dev_ctxt;
	struct bhi_ctxt_t *bhi_ctxt;
	struct bhie_mem_info mem_info;
	int ret;

	mhi_dev_ctxt = container_of(work, struct mhi_device_ctxt,
				    bhi_ctxt.fw_load_work);
	bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Enter\n");

	ret = wait_event_interruptible_timeout(
		*mhi_dev_ctxt->mhi_ev_wq.bhi_event,
		mhi_dev_ctxt->mhi_state == MHI_STATE_BHI ||
		mhi_dev_ctxt->mhi_pm_state == MHI_PM_LD_ERR_FATAL_DETECT,
		msecs_to_jiffies(MHI_MAX_STATE_TRANSITION_TIMEOUT));
	if (!ret || mhi_dev_ctxt->mhi_pm_state == MHI_PM_LD_ERR_FATAL_DETECT) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"MHI is not in valid state for firmware download\n");
		return;
	}

	/* PBL image is the first segment in firmware vector table */
	mem_info = *bhi_ctxt->fw_table.bhie_mem_info;
	mem_info.size = bhi_ctxt->firmware_info.max_sbl_len;
	ret = bhi_load_firmware(mhi_dev_ctxt, &mem_info);
	if (ret) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to Load sbl firmware\n");
		return;
	}
	mhi_init_state_transition(mhi_dev_ctxt,
				  STATE_TRANSITION_RESET);

	wait_event_timeout(*mhi_dev_ctxt->mhi_ev_wq.bhi_event,
		mhi_dev_ctxt->dev_exec_env == MHI_EXEC_ENV_BHIE ||
		mhi_dev_ctxt->mhi_pm_state == MHI_PM_LD_ERR_FATAL_DETECT,
		msecs_to_jiffies(bhi_ctxt->poll_timeout));
	if (mhi_dev_ctxt->mhi_pm_state == MHI_PM_LD_ERR_FATAL_DETECT ||
	    mhi_dev_ctxt->dev_exec_env != MHI_EXEC_ENV_BHIE) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to Enter EXEC_ENV_BHIE\n");
		return;
	}

	ret = bhi_bhie_transfer(mhi_dev_ctxt, &mhi_dev_ctxt->bhi_ctxt.fw_table,
				true);
	if (ret) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to Load amss firmware\n");
	}
}

int bhi_probe(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	struct firmware_info *fw_info = &bhi_ctxt->firmware_info;
	struct bhie_vec_table *fw_table = &bhi_ctxt->fw_table;
	struct bhie_vec_table *rddm_table = &bhi_ctxt->rddm_table;
	const struct firmware *firmware;
	struct scatterlist *itr;
	int ret, i;
	size_t remainder;
	const u8 *image;

	/* expose dev node to userspace */
	if (bhi_ctxt->manage_boot == false)
		return bhi_expose_dev_bhi(mhi_dev_ctxt);

	/* Make sure minimum  buffer we allocate for BHI/E is >= sbl image */
	while (fw_info->segment_size < fw_info->max_sbl_len)
		fw_info->segment_size <<= 1;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"max sbl image size:%lu segment size:%lu\n",
		fw_info->max_sbl_len, fw_info->segment_size);

	/* Read the fw image */
	ret = request_firmware(&firmware, fw_info->fw_image,
			       &mhi_dev_ctxt->plat_dev->dev);
	if (ret) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Error request firmware for:%s ret:%d\n",
			fw_info->fw_image, ret);
		return ret;
	}

	ret = bhi_alloc_bhie_xfer(mhi_dev_ctxt,
				  firmware->size,
				  fw_table);
	if (ret) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Error Allocating memory for firmware image\n");
		release_firmware(firmware);
		return ret;
	}

	/* Copy the fw image to vector table */
	remainder = firmware->size;
	image = firmware->data;
	for (i = 0, itr = &fw_table->sg_list[1];
	     i < fw_table->segment_count - 1; i++, itr++) {
		size_t to_copy = min(remainder, fw_info->segment_size);

		memcpy(fw_table->bhie_mem_info[i].aligned, image, to_copy);
		fw_table->bhi_vec_entry[i].phys_addr =
			fw_table->bhie_mem_info[i].phys_addr;
		fw_table->bhi_vec_entry[i].size = to_copy;
		sg_set_buf(itr, fw_table->bhie_mem_info[i].aligned, to_copy);
		sg_dma_address(itr) = fw_table->bhie_mem_info[i].phys_addr;
		sg_dma_len(itr) = to_copy;
		remainder -= to_copy;
		image += to_copy;
	}

	fw_table->sequence++;
	release_firmware(firmware);

	/* allocate memory and setup rddm table */
	if (bhi_ctxt->support_rddm) {
		ret = bhi_alloc_bhie_xfer(mhi_dev_ctxt, bhi_ctxt->rddm_size,
					  rddm_table);
		if (!ret) {
			for (i = 0, itr = &rddm_table->sg_list[1];
			     i < rddm_table->segment_count - 1; i++, itr++) {
				size_t size = rddm_table->bhie_mem_info[i].size;

				rddm_table->bhi_vec_entry[i].phys_addr =
					rddm_table->bhie_mem_info[i].phys_addr;
				rddm_table->bhi_vec_entry[i].size = size;
				sg_set_buf(itr, rddm_table->
					   bhie_mem_info[i].aligned, size);
				sg_dma_address(itr) =
					rddm_table->bhie_mem_info[i].phys_addr;
				sg_dma_len(itr) = size;
			}
			rddm_table->sequence++;
		} else {
			/* out of memory for rddm, not fatal error */
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"Could not successfully allocate mem for rddm\n");
		}
	}

	/* Schedule a worker thread and wait for BHI Event */
	schedule_work(&bhi_ctxt->fw_load_work);
	return 0;
}

void bhi_exit(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	struct bhie_vec_table *fw_table = &bhi_ctxt->fw_table;
	struct bhie_vec_table *rddm_table = &bhi_ctxt->rddm_table;
	struct device *dev = &mhi_dev_ctxt->plat_dev->dev;
	struct bhie_mem_info *bhie_mem_info;
	int i;

	if (bhi_ctxt->manage_boot == false)
		return;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"freeing firmware and rddm memory\n");

	/* free memory allocated for firmware */
	kfree(fw_table->sg_list);
	fw_table->sg_list = NULL;
	bhie_mem_info = fw_table->bhie_mem_info;
	for (i = 0; i < fw_table->segment_count; i++, bhie_mem_info++)
		dma_free_coherent(dev, bhie_mem_info->alloc_size,
				  bhie_mem_info->pre_aligned,
				  bhie_mem_info->dma_handle);
	kfree(fw_table->bhie_mem_info);
	fw_table->bhie_mem_info = NULL;
	/* vector table is the last entry in bhie_mem_info */
	fw_table->bhi_vec_entry = NULL;

	if (!rddm_table->bhie_mem_info)
		return;

	/* free memory allocated for rddm */
	kfree(rddm_table->sg_list);
	rddm_table->sg_list = NULL;
	bhie_mem_info = rddm_table->bhie_mem_info;
	for (i = 0; i < rddm_table->segment_count; i++, bhie_mem_info++)
		dma_free_coherent(dev, bhie_mem_info->alloc_size,
				  bhie_mem_info->pre_aligned,
				  bhie_mem_info->dma_handle);
	kfree(rddm_table->bhie_mem_info);
	rddm_table->bhie_mem_info = NULL;
	rddm_table->bhi_vec_entry = NULL;
}
