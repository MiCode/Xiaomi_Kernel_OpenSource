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
			      size_t size)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	const phys_addr_t align_len = bhi_ctxt->alignment;
	size_t alloc_size = size + (align_len - 1);
	struct device *dev = &mhi_dev_ctxt->plat_dev->dev;

	bhi_ctxt->unaligned_image_loc =
		dma_alloc_coherent(dev, alloc_size, &bhi_ctxt->dma_handle,
				   GFP_KERNEL);
	if (bhi_ctxt->unaligned_image_loc == NULL)
		return -ENOMEM;

	bhi_ctxt->alloc_size = alloc_size;
	bhi_ctxt->phy_image_loc = (bhi_ctxt->dma_handle + (align_len - 1)) &
		~(align_len - 1);
	bhi_ctxt->image_loc = bhi_ctxt->unaligned_image_loc +
		(bhi_ctxt->phy_image_loc - bhi_ctxt->dma_handle);
	bhi_ctxt->image_size = size;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"alloc_size:%lu image_size:%lu unal_addr:0x%llx0x al_addr:0x%llx\n",
		bhi_ctxt->alloc_size, bhi_ctxt->image_size,
		bhi_ctxt->dma_handle, bhi_ctxt->phy_image_loc);

	return 0;
}

/* Load firmware via bhie protocol */
static int bhi_load_bhie_firmware(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
	struct bhie_vec_table *fw_table = &bhi_ctxt->fw_table;
	const struct bhie_mem_info *bhie_mem_info =
		&fw_table->bhie_mem_info[fw_table->segment_count - 1];
	u32 val;
	const u32 tx_sequence = fw_table->sequence++;
	unsigned long timeout;
	rwlock_t *pm_xfer_lock = &mhi_dev_ctxt->pm_xfer_lock;

	/* Program TX/RX Vector table */
	read_lock_bh(pm_xfer_lock);
	if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
		read_unlock_bh(pm_xfer_lock);
		return -EIO;
	}

	val = HIGH_WORD(bhie_mem_info->phys_addr);
	mhi_reg_write(mhi_dev_ctxt,
		      bhi_ctxt->bhi_base,
		      BHIE_TXVECADDR_HIGH_OFFS,
		      val);
	val = LOW_WORD(bhie_mem_info->phys_addr);
	mhi_reg_write(mhi_dev_ctxt,
		      bhi_ctxt->bhi_base,
		      BHIE_TXVECADDR_LOW_OFFS,
		      val);
	val = (u32)bhie_mem_info->size;
	mhi_reg_write(mhi_dev_ctxt,
		      bhi_ctxt->bhi_base,
		      BHIE_TXVECSIZE_OFFS,
		      val);

	/* Ring DB to begin Xfer */
	mhi_reg_write_field(mhi_dev_ctxt,
			    bhi_ctxt->bhi_base,
			    BHIE_TXVECDB_OFFS,
			    BHIE_TXVECDB_SEQNUM_BMSK,
			    BHIE_TXVECDB_SEQNUM_SHFT,
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
		val = mhi_reg_read(bhi_ctxt->bhi_base, BHIE_TXVECSTATUS_OFFS);
		read_unlock_bh(pm_xfer_lock);
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"TXVEC_STATUS:0x%x\n", val);
		current_seq = (val & BHIE_TXVECSTATUS_SEQNUM_BMSK) >>
			BHIE_TXVECSTATUS_SEQNUM_SHFT;
		status = (val & BHIE_TXVECSTATUS_STATUS_BMSK) >>
			BHIE_TXVECSTATUS_STATUS_SHFT;
		if ((status == BHIE_TXVECSTATUS_STATUS_XFER_COMPL) &&
		    (current_seq == tx_sequence)) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
				"Image transfer complete\n");
			return 0;
		}
		msleep(BHI_POLL_SLEEP_TIME_MS);
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
		 "Error xfering image via BHIE\n");
	return -EIO;
}

static int bhi_load_firmware(struct mhi_device_ctxt *mhi_dev_ctxt)
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
	pcie_word_val = HIGH_WORD(bhi_ctxt->phy_image_loc);
	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base,
				BHI_IMGADDR_HIGH,
				0xFFFFFFFF,
				0,
				pcie_word_val);

	pcie_word_val = LOW_WORD(bhi_ctxt->phy_image_loc);

	mhi_reg_write_field(mhi_dev_ctxt, bhi_ctxt->bhi_base,
				BHI_IMGADDR_LOW,
				0xFFFFFFFF,
				0,
				pcie_word_val);

	pcie_word_val = bhi_ctxt->image_size;
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
	long timeout;

	if (buf == NULL || 0 == count)
		return -EIO;

	if (count > BHI_MAX_IMAGE_SIZE)
		return -ENOMEM;

	ret_val = bhi_alloc_pbl_xfer(mhi_dev_ctxt, count);
	if (ret_val)
		return -ENOMEM;

	if (copy_from_user(bhi_ctxt->image_loc, buf, count)) {
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

	ret_val = bhi_load_firmware(mhi_dev_ctxt);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to load bhi image\n");
	}
	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev,
			  bhi_ctxt->alloc_size,
			  bhi_ctxt->unaligned_image_loc,
			  bhi_ctxt->dma_handle);

	/* Regardless of failure set to RESET state */
	ret_val = mhi_init_state_transition(mhi_dev_ctxt,
					STATE_TRANSITION_RESET);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to start state change event\n");
	}
	return count;

bhi_copy_error:
	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev,
			  bhi_ctxt->alloc_size,
			  bhi_ctxt->unaligned_image_loc,
			  bhi_ctxt->dma_handle);

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
	int ret;
	long timeout;

	mhi_dev_ctxt = container_of(work, struct mhi_device_ctxt,
				    bhi_ctxt.fw_load_work);
	bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Enter\n");

	wait_event_interruptible(*mhi_dev_ctxt->mhi_ev_wq.bhi_event,
			mhi_dev_ctxt->mhi_state == MHI_STATE_BHI);

	ret = bhi_load_firmware(mhi_dev_ctxt);
	if (ret) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to Load sbl firmware\n");
		return;
	}
	mhi_init_state_transition(mhi_dev_ctxt,
				  STATE_TRANSITION_RESET);

	timeout = wait_event_timeout(*mhi_dev_ctxt->mhi_ev_wq.bhi_event,
				mhi_dev_ctxt->dev_exec_env == MHI_EXEC_ENV_BHIE,
				msecs_to_jiffies(bhi_ctxt->poll_timeout));
	if (!timeout) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to Enter EXEC_ENV_BHIE\n");
		return;
	}

	ret = bhi_load_bhie_firmware(mhi_dev_ctxt);
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

	/*
	 * Re-use BHI/E pointer for BHI since we guranteed BHI/E segment
	 * is >= to SBL image.
	 */
	bhi_ctxt->phy_image_loc = sg_dma_address(&fw_table->sg_list[1]);
	bhi_ctxt->image_size = fw_info->max_sbl_len;

	fw_table->sequence++;
	release_firmware(firmware);

	/* Schedule a worker thread and wait for BHI Event */
	schedule_work(&bhi_ctxt->fw_load_work);
	return 0;
}
