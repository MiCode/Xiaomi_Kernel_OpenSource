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
	file_handle->private_data = &mhi_devices.device_list[0];
	return 0;
}

static ssize_t bhi_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *offp)
{
	int ret_val = 0;
	u32 pcie_word_val = 0;
	u32 i = 0;
	struct bhi_ctxt_t *bhi_ctxt =
		&(((struct mhi_pcie_dev_info *)file->private_data)->bhi_ctxt);
	struct mhi_device_ctxt *mhi_dev_ctxt =
		&((struct mhi_pcie_dev_info *)file->private_data)->mhi_ctxt;
	size_t amount_copied = 0;
	uintptr_t align_len = 0x1000;
	u32 tx_db_val = 0;

	if (buf == NULL || 0 == count)
		return -EIO;

	if (count > BHI_MAX_IMAGE_SIZE)
		return -ENOMEM;

	wait_event_interruptible(*mhi_dev_ctxt->mhi_ev_wq.bhi_event,
			mhi_dev_ctxt->mhi_state == MHI_STATE_BHI);

	mhi_log(MHI_MSG_INFO, "Entered. User Image size 0x%zx\n", count);

	bhi_ctxt->unaligned_image_loc = kmalloc(count + (align_len - 1),
						GFP_KERNEL);
	if (bhi_ctxt->unaligned_image_loc == NULL)
		return -ENOMEM;

	mhi_log(MHI_MSG_INFO, "Unaligned Img Loc: %p\n",
			bhi_ctxt->unaligned_image_loc);
	bhi_ctxt->image_loc =
			(void *)((uintptr_t)bhi_ctxt->unaligned_image_loc +
		 (align_len - (((uintptr_t)bhi_ctxt->unaligned_image_loc) %
			       align_len)));

	mhi_log(MHI_MSG_INFO, "Aligned Img Loc: %p\n", bhi_ctxt->image_loc);

	bhi_ctxt->image_size = count;

	if (0 != copy_from_user(bhi_ctxt->image_loc, buf, count)) {
		ret_val = -ENOMEM;
		goto bhi_copy_error;
	}
	amount_copied = count;
	/* Flush the writes, in anticipation for a device read */
	wmb();
	mhi_log(MHI_MSG_INFO,
		"Copied image from user at addr: %p\n", bhi_ctxt->image_loc);
	bhi_ctxt->phy_image_loc = dma_map_single(
			&mhi_dev_ctxt->dev_info->plat_dev->dev,
			bhi_ctxt->image_loc,
			bhi_ctxt->image_size,
			DMA_TO_DEVICE);

	if (dma_mapping_error(NULL, bhi_ctxt->phy_image_loc)) {
		ret_val = -EIO;
		goto bhi_copy_error;
	}
	mhi_log(MHI_MSG_INFO,
		"Mapped image to DMA addr 0x%lx:\n",
		(uintptr_t)bhi_ctxt->phy_image_loc);

	bhi_ctxt->image_size = count;

	/* Write the image size */
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

	mhi_reg_write(mhi_dev_ctxt, bhi_ctxt->bhi_base, BHI_INTVEC, 0);

	for (i = 0; i < BHI_POLL_NR_RETRIES; ++i) {
		u32 err = 0, errdbg1 = 0, errdbg2 = 0, errdbg3 = 0;

		err = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRCODE);
		errdbg1 = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRDBG1);
		errdbg2 = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRDBG2);
		errdbg3 = mhi_reg_read(bhi_ctxt->bhi_base, BHI_ERRDBG3);
		tx_db_val = mhi_reg_read_field(bhi_ctxt->bhi_base,
						BHI_STATUS,
						BHI_STATUS_MASK,
						BHI_STATUS_SHIFT);
		mhi_log(MHI_MSG_CRITICAL,
		"BHI STATUS 0x%x, err:0x%x errdbg1:0x%x errdbg2:0x%x errdbg3:0x%x\n",
			tx_db_val, err, errdbg1, errdbg2, errdbg3);
		if (BHI_STATUS_SUCCESS != tx_db_val)
			mhi_log(MHI_MSG_CRITICAL,
				"Incorrect BHI status: %d retry: %d\n",
				tx_db_val, i);
		else
			break;
		usleep_range(20000, 25000);
	}
	dma_unmap_single(&mhi_dev_ctxt->dev_info->plat_dev->dev,
			bhi_ctxt->phy_image_loc,
			bhi_ctxt->image_size, DMA_TO_DEVICE);

	kfree(bhi_ctxt->unaligned_image_loc);

	ret_val = mhi_init_state_transition(mhi_dev_ctxt,
					STATE_TRANSITION_RESET);
	if (ret_val) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to start state change event\n");
	}
	return amount_copied;

bhi_copy_error:
	kfree(bhi_ctxt->unaligned_image_loc);
	return amount_copied;
}

static const struct file_operations bhi_fops = {
	.write = bhi_write,
	.open = bhi_open,
};

int bhi_probe(struct mhi_pcie_dev_info *mhi_pcie_device)
{
	struct bhi_ctxt_t *bhi_ctxt = &mhi_pcie_device->bhi_ctxt;
	int ret_val = 0;
	int r;

	if (NULL == mhi_pcie_device || 0 == mhi_pcie_device->core.bar0_base
	    || 0 == mhi_pcie_device->core.bar0_end)
		return -EIO;

	mhi_log(MHI_MSG_INFO,
		"Successfully registered char dev. bhi base is: 0x%p.\n",
		bhi_ctxt->bhi_base);
	ret_val = alloc_chrdev_region(&bhi_ctxt->bhi_dev, 0, 1, "bhi");
	if (IS_ERR_VALUE(ret_val)) {
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to alloc char device %d\n",
			ret_val);
		return -EIO;
	}
	bhi_ctxt->bhi_class = class_create(THIS_MODULE, "bhi");
	if (IS_ERR(bhi_ctxt->bhi_class)) {
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to instantiate class %d\n",
			ret_val);
		r = PTR_RET(bhi_ctxt->bhi_class);
		goto err_class_create;
	}
	cdev_init(&bhi_ctxt->cdev, &bhi_fops);
	bhi_ctxt->cdev.owner = THIS_MODULE;
	ret_val = cdev_add(&bhi_ctxt->cdev, bhi_ctxt->bhi_dev, 1);
	bhi_ctxt->dev = device_create(bhi_ctxt->bhi_class, NULL,
					bhi_ctxt->bhi_dev, NULL,
					"bhi");
	if (IS_ERR(bhi_ctxt->dev)) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to add bhi cdev\n");
		r = PTR_RET(bhi_ctxt->dev);
		goto err_dev_create;
	}
	return 0;
err_dev_create:
	cdev_del(&bhi_ctxt->cdev);
	class_destroy(bhi_ctxt->bhi_class);
err_class_create:
	unregister_chrdev_region(MAJOR(bhi_ctxt->bhi_dev), 1);
	return r;
}
