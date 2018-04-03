/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/of_irq.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>

#include "coresight-byte-cntr.h"
#include "coresight-priv.h"
#include "coresight-tmc.h"

static struct tmc_drvdata *tmcdrvdata;

static void tmc_etr_read_bytes(struct byte_cntr *byte_cntr_data, loff_t *ppos,
			       size_t bytes, size_t *len, char **bufp)
{

	if (*bufp >= (char *)(tmcdrvdata->vaddr + tmcdrvdata->size))
		*bufp = tmcdrvdata->vaddr;

	if (*len >= bytes)
		*len = bytes;
	else if (((uint32_t)*ppos % bytes) + *len > bytes)
		*len = bytes - ((uint32_t)*ppos % bytes);

	if ((*bufp + *len) > (char *)(tmcdrvdata->vaddr +
		tmcdrvdata->size))
		*len = (char *)(tmcdrvdata->vaddr + tmcdrvdata->size) -
			*bufp;
	if (*len == bytes || (*len + (uint32_t)*ppos) % bytes == 0)
		atomic_dec(&byte_cntr_data->irq_cnt);
}

static void tmc_etr_sg_read_pos(loff_t *ppos,
				size_t bytes, bool noirq, size_t *len,
				char **bufpp)
{
	uint32_t rwp, i = 0;
	uint32_t blk_num, sg_tbl_num, blk_num_loc, read_off;
	uint32_t *virt_pte, *virt_st_tbl;
	void *virt_blk;
	phys_addr_t phys_pte;
	int total_ents = DIV_ROUND_UP(tmcdrvdata->size, PAGE_SIZE);
	int ents_per_pg = PAGE_SIZE/sizeof(uint32_t);

	if (*len == 0)
		return;

	blk_num = *ppos / PAGE_SIZE;
	read_off = *ppos % PAGE_SIZE;

	virt_st_tbl = (uint32_t *)tmcdrvdata->vaddr;

	/* Compute table index and block entry index within that table */
	if (blk_num && (blk_num == (total_ents - 1)) &&
	    !(blk_num % (ents_per_pg - 1))) {
		sg_tbl_num = blk_num / ents_per_pg;
		blk_num_loc = ents_per_pg - 1;
	} else {
		sg_tbl_num = blk_num / (ents_per_pg - 1);
		blk_num_loc = blk_num % (ents_per_pg - 1);
	}

	for (i = 0; i < sg_tbl_num; i++) {
		virt_pte = virt_st_tbl + (ents_per_pg - 1);
		phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);
		virt_st_tbl = (uint32_t *)phys_to_virt(phys_pte);
	}

	virt_pte = virt_st_tbl + blk_num_loc;
	phys_pte = TMC_ETR_SG_ENT_TO_BLK(*virt_pte);
	virt_blk = phys_to_virt(phys_pte);

	*bufpp = (char *)(virt_blk + read_off);

	if (noirq) {
		rwp = readl_relaxed(tmcdrvdata->base + TMC_RWP);
		tmc_etr_sg_rwp_pos(tmcdrvdata, rwp);
		if (tmcdrvdata->sg_blk_num == blk_num &&
		    rwp >= (phys_pte + read_off))
			*len = rwp - phys_pte - read_off;
		else if (tmcdrvdata->sg_blk_num > blk_num)
			*len = PAGE_SIZE - read_off;
		else
			*len = bytes;
	} else {

		if (*len > (PAGE_SIZE - read_off))
			*len = PAGE_SIZE - read_off;

		if (*len >= (bytes - ((uint32_t)*ppos % bytes)))
			*len = bytes - ((uint32_t)*ppos % bytes);

		if (*len == bytes || (*len + (uint32_t)*ppos) % bytes == 0)
			atomic_dec(&tmcdrvdata->byte_cntr->irq_cnt);
	}

	/*
	 * Invalidate cache range before reading. This will make sure that CPU
	 * reads latest contents from DDR
	 */
	dmac_inv_range((void *)(*bufpp), (void *)(*bufpp) + *len);
}

static irqreturn_t etr_handler(int irq, void *data)
{
	struct byte_cntr *byte_cntr_data = data;

	atomic_inc(&byte_cntr_data->irq_cnt);

	wake_up(&byte_cntr_data->wq);

	return IRQ_HANDLED;
}

static void tmc_etr_flush_bytes(loff_t *ppos, size_t bytes, size_t *len)
{
	uint32_t rwp = 0;

	rwp = readl_relaxed(tmcdrvdata->base + TMC_RWP);

	if (rwp >= (tmcdrvdata->paddr + *ppos)) {
		if (bytes > (rwp - tmcdrvdata->paddr - *ppos))
			*len = rwp - tmcdrvdata->paddr - *ppos;
	}
}

static ssize_t tmc_etr_byte_cntr_read(struct file *fp, char __user *data,
			       size_t len, loff_t *ppos)
{
	struct byte_cntr *byte_cntr_data = fp->private_data;
	char *bufp;

	if (!data)
		return -EINVAL;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	if (!byte_cntr_data->read_active)
		goto err0;

	bufp = (char *)(tmcdrvdata->buf + *ppos);

	if (byte_cntr_data->enable) {
		if (!atomic_read(&byte_cntr_data->irq_cnt)) {
			mutex_unlock(&byte_cntr_data->byte_cntr_lock);
			if (wait_event_interruptible(byte_cntr_data->wq,
				atomic_read(&byte_cntr_data->irq_cnt) > 0))
				return -ERESTARTSYS;
			mutex_lock(&byte_cntr_data->byte_cntr_lock);
			if (!byte_cntr_data->read_active)
				goto err0;
		}

		if (tmcdrvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG)
			tmc_etr_read_bytes(byte_cntr_data, ppos,
					   byte_cntr_data->block_size, &len,
					   &bufp);
		else
			tmc_etr_sg_read_pos(ppos, byte_cntr_data->block_size, 0,
					    &len, &bufp);

	} else {
		if (!atomic_read(&byte_cntr_data->irq_cnt)) {
			if (tmcdrvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG)
				tmc_etr_flush_bytes(ppos,
						    byte_cntr_data->block_size,
						    &len);
			else
				tmc_etr_sg_read_pos(ppos,
						    byte_cntr_data->block_size,
						    1,
						    &len, &bufp);
			if (!len)
				goto err0;
		} else {
			if (tmcdrvdata->memtype == TMC_ETR_MEM_TYPE_CONTIG)
				tmc_etr_read_bytes(byte_cntr_data, ppos,
						   byte_cntr_data->block_size,
						   &len, &bufp);
			else
				tmc_etr_sg_read_pos(ppos,
						    byte_cntr_data->block_size,
						    1,
						    &len, &bufp);
		}
	}

	if (copy_to_user(data, bufp, len)) {
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
		dev_dbg(tmcdrvdata->dev, "%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	if (*ppos + len >= tmcdrvdata->size)
		*ppos = 0;
	else
		*ppos += len;
err0:
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

	return len;
}

void tmc_etr_byte_cntr_start(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);

	if (byte_cntr_data->block_size == 0) {
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
		return;
	}

	atomic_set(&byte_cntr_data->irq_cnt, 0);
	byte_cntr_data->enable = true;
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
}
EXPORT_SYMBOL(tmc_etr_byte_cntr_start);

void tmc_etr_byte_cntr_stop(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	byte_cntr_data->enable = false;
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, 0);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

}
EXPORT_SYMBOL(tmc_etr_byte_cntr_stop);


static int tmc_etr_byte_cntr_release(struct inode *in, struct file *fp)
{
	struct byte_cntr *byte_cntr_data = fp->private_data;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	byte_cntr_data->read_active = false;

	coresight_csr_set_byte_cntr(byte_cntr_data->csr, 0);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

	return 0;
}

static int tmc_etr_byte_cntr_open(struct inode *in, struct file *fp)
{
	struct byte_cntr *byte_cntr_data =
			container_of(in->i_cdev, struct byte_cntr, dev);

	mutex_lock(&byte_cntr_data->byte_cntr_lock);

	if (!tmcdrvdata->enable || !byte_cntr_data->block_size) {
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
		return -EINVAL;
	}

	/* IRQ is a '8- byte' counter and to observe interrupt at
	 * 'block_size' bytes of data
	 */
	coresight_csr_set_byte_cntr(byte_cntr_data->csr,
				(byte_cntr_data->block_size) / 8);

	fp->private_data = byte_cntr_data;
	nonseekable_open(in, fp);
	byte_cntr_data->enable = true;
	byte_cntr_data->read_active = true;
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

	return 0;
}

static const struct file_operations byte_cntr_fops = {
	.owner		= THIS_MODULE,
	.open		= tmc_etr_byte_cntr_open,
	.read		= tmc_etr_byte_cntr_read,
	.release	= tmc_etr_byte_cntr_release,
	.llseek		= no_llseek,
};

static int byte_cntr_register_chardev(struct byte_cntr *byte_cntr_data)
{
	int ret;
	unsigned int baseminor = 0;
	unsigned int count = 1;
	struct device *device;
	dev_t dev;

	ret = alloc_chrdev_region(&dev, baseminor, count, "byte-cntr");
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed %d\n", ret);
		return ret;
	}
	cdev_init(&byte_cntr_data->dev, &byte_cntr_fops);

	byte_cntr_data->dev.owner = THIS_MODULE;
	byte_cntr_data->dev.ops = &byte_cntr_fops;

	ret = cdev_add(&byte_cntr_data->dev, dev, 1);
	if (ret)
		goto exit_unreg_chrdev_region;

	byte_cntr_data->driver_class = class_create(THIS_MODULE,
						   "coresight-tmc-etr-stream");
	if (IS_ERR(byte_cntr_data->driver_class)) {
		ret = -ENOMEM;
		pr_err("class_create failed %d\n", ret);
		goto exit_unreg_chrdev_region;
	}

	device = device_create(byte_cntr_data->driver_class, NULL,
			       byte_cntr_data->dev.dev, byte_cntr_data,
			       "byte-cntr");

	if (IS_ERR(device)) {
		pr_err("class_device_create failed %d\n", ret);
		ret = -ENOMEM;
		goto exit_destroy_class;
	}

	return 0;

exit_destroy_class:
	class_destroy(byte_cntr_data->driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(byte_cntr_data->dev.dev, 1);
	return ret;
}

struct byte_cntr *byte_cntr_init(struct amba_device *adev,
				 struct tmc_drvdata *drvdata)
{
	struct device *dev = &adev->dev;
	struct device_node *np = adev->dev.of_node;
	int byte_cntr_irq;
	int ret;
	struct byte_cntr *byte_cntr_data;

	byte_cntr_irq = of_irq_get_byname(np, "byte-cntr-irq");
	if (byte_cntr_irq < 0)
		return NULL;

	byte_cntr_data = devm_kzalloc(dev, sizeof(*byte_cntr_data), GFP_KERNEL);
	if (!byte_cntr_data)
		return NULL;

	ret = devm_request_irq(dev, byte_cntr_irq, etr_handler,
			       IRQF_TRIGGER_RISING | IRQF_SHARED,
			       "tmc-etr", byte_cntr_data);
	if (ret) {
		devm_kfree(dev, byte_cntr_data);
		dev_err(dev, "Byte_cntr interrupt registration failed\n");
		return NULL;
	}

	ret = byte_cntr_register_chardev(byte_cntr_data);
	if (ret) {
		devm_free_irq(dev, byte_cntr_irq, byte_cntr_data);
		devm_kfree(dev, byte_cntr_data);
		dev_err(dev, "Byte_cntr char dev registration failed\n");
		return NULL;
	}

	tmcdrvdata = drvdata;
	byte_cntr_data->byte_cntr_irq = byte_cntr_irq;
	byte_cntr_data->csr = drvdata->csr;
	atomic_set(&byte_cntr_data->irq_cnt, 0);
	init_waitqueue_head(&byte_cntr_data->wq);
	mutex_init(&byte_cntr_data->byte_cntr_lock);

	return byte_cntr_data;
}
EXPORT_SYMBOL(byte_cntr_init);
