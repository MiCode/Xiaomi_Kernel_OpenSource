/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "coresight-byte-cntr.h"
#include "coresight-priv.h"
#include "coresight-tmc.h"

#define PCIE_BLK_SIZE 32768

static struct tmc_drvdata *tmcdrvdata;

static void tmc_etr_read_bytes(struct byte_cntr *byte_cntr_data, loff_t *ppos,
			       size_t bytes, size_t *len, char **bufp)
{
	struct etr_buf *etr_buf = tmcdrvdata->etr_buf;
	size_t actual;

	if (*len >= bytes)
		*len = bytes;
	else if (((uint32_t)*ppos % bytes) + *len > bytes)
		*len = bytes - ((uint32_t)*ppos % bytes);

	actual = tmc_etr_buf_get_data(etr_buf, *ppos, *len, bufp);
	*len = actual;
	if (actual == bytes || (actual + (uint32_t)*ppos) % bytes == 0)
		atomic_dec(&byte_cntr_data->irq_cnt);
}

static irqreturn_t etr_handler(int irq, void *data)
{
	struct byte_cntr *byte_cntr_data = data;

	if (tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
		atomic_inc(&byte_cntr_data->irq_cnt);
		wake_up(&byte_cntr_data->wq);
	} else if (tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_PCIE) {
		atomic_inc(&byte_cntr_data->irq_cnt);
		wake_up(&byte_cntr_data->pcie_wait_wq);
	}

	return IRQ_HANDLED;
}

static void tmc_etr_flush_bytes(loff_t *ppos, size_t bytes, size_t *len)
{
	uint32_t rwp = 0;
	dma_addr_t paddr = tmcdrvdata->etr_buf->hwaddr;

	rwp = readl_relaxed(tmcdrvdata->base + TMC_RWP);

	if (rwp >= (paddr + *ppos)) {
		if (bytes > (rwp - paddr - *ppos))
			*len = rwp - paddr - *ppos;
	}
}

static ssize_t tmc_etr_byte_cntr_read(struct file *fp, char __user *data,
			       size_t len, loff_t *ppos)
{
	struct byte_cntr *byte_cntr_data = fp->private_data;
	char *bufp;
	int ret = 0;
	if (!data)
		return -EINVAL;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	if (!byte_cntr_data->read_active) {
		ret = -EINVAL;
		goto err0;
	}

	if (byte_cntr_data->enable) {
		if (!atomic_read(&byte_cntr_data->irq_cnt)) {
			mutex_unlock(&byte_cntr_data->byte_cntr_lock);
			if (wait_event_interruptible(byte_cntr_data->wq,
				atomic_read(&byte_cntr_data->irq_cnt) > 0
				|| !byte_cntr_data->enable))
				return -ERESTARTSYS;
			mutex_lock(&byte_cntr_data->byte_cntr_lock);
			if (!byte_cntr_data->read_active) {
				ret = -EINVAL;
				goto err0;
			}

		}

		tmc_etr_read_bytes(byte_cntr_data, ppos,
					byte_cntr_data->block_size, &len, &bufp);

	} else {
		if (!atomic_read(&byte_cntr_data->irq_cnt)) {
			tmc_etr_flush_bytes(ppos, byte_cntr_data->block_size,
									&len);
			if (!len)
				goto err0;
		} else {
			tmc_etr_read_bytes(byte_cntr_data, ppos,
					byte_cntr_data->block_size, &len, &bufp);
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

	goto out;

err0:
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	return ret;
out:
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	return len;
}

void tmc_etr_byte_cntr_start(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);

	if (byte_cntr_data->block_size == 0
		|| byte_cntr_data->read_active) {
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
	byte_cntr_data->read_active = false;
	wake_up(&byte_cntr_data->wq);
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, 0);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

}
EXPORT_SYMBOL(tmc_etr_byte_cntr_stop);

static void etr_pcie_close_channel(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	mhi_dev_close_channel(byte_cntr_data->out_handle);
	byte_cntr_data->pcie_chan_opened = false;
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
}

int etr_pcie_start(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return -ENOMEM;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, PCIE_BLK_SIZE / 8);
	atomic_set(&byte_cntr_data->irq_cnt, 0);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

	if (!byte_cntr_data->pcie_chan_opened)
		queue_work(byte_cntr_data->pcie_wq,
				&byte_cntr_data->pcie_open_work);

	queue_work(byte_cntr_data->pcie_wq, &byte_cntr_data->pcie_write_work);
	return 0;
}
EXPORT_SYMBOL(etr_pcie_start);

void etr_pcie_stop(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	etr_pcie_close_channel(byte_cntr_data);
	wake_up(&byte_cntr_data->pcie_wait_wq);

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, 0);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
}
EXPORT_SYMBOL(etr_pcie_stop);

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

static void etr_pcie_client_cb(struct mhi_dev_client_cb_data *cb_data)
{
	struct byte_cntr *byte_cntr_data = NULL;

	if (!cb_data)
		return;

	byte_cntr_data = cb_data->user_data;
	if (!byte_cntr_data)
		return;

	switch (cb_data->ctrl_info) {
	case  MHI_STATE_CONNECTED:
		if (cb_data->channel == byte_cntr_data->pcie_out_chan) {
			dev_dbg(tmcdrvdata->dev, "PCIE out channel connected.\n");
			queue_work(byte_cntr_data->pcie_wq,
					&byte_cntr_data->pcie_open_work);
		}

		break;
	case MHI_STATE_DISCONNECTED:
		if (cb_data->channel == byte_cntr_data->pcie_out_chan) {
			dev_dbg(tmcdrvdata->dev, "PCIE out channel disconnected.\n");
			etr_pcie_close_channel(byte_cntr_data);
		}
		break;
	default:
		break;
	}
}

static void etr_pcie_write_complete_cb(void *req)
{
	struct mhi_req *mreq = req;

	if (!mreq)
		return;
	kfree(req);
}

static void etr_pcie_open_work_fn(struct work_struct *work)
{
	int ret = 0;
	struct byte_cntr *byte_cntr_data = container_of(work,
					      struct byte_cntr,
					      pcie_open_work);

	if (!byte_cntr_data)
		return;

	/* Open write channel*/
	ret = mhi_dev_open_channel(byte_cntr_data->pcie_out_chan,
			&byte_cntr_data->out_handle,
			NULL);
	if (ret < 0) {
		dev_err(tmcdrvdata->dev, "%s: open pcie out channel fail %d\n",
						__func__, ret);
	} else {
		dev_dbg(tmcdrvdata->dev,
				"Open pcie out channel successfully\n");
		mutex_lock(&byte_cntr_data->byte_cntr_lock);
		byte_cntr_data->pcie_chan_opened = true;
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	}

}

static void etr_pcie_write_work_fn(struct work_struct *work)
{
	int ret = 0;
	struct mhi_req *req;
	size_t actual;
	int bytes_to_write;
	char *buf;

	struct byte_cntr *byte_cntr_data = container_of(work,
						struct byte_cntr,
						pcie_write_work);

	while (tmcdrvdata->enable
		&& tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_PCIE) {
		if (!atomic_read(&byte_cntr_data->irq_cnt)) {
			ret =  wait_event_interruptible(
				byte_cntr_data->pcie_wait_wq,
				atomic_read(&byte_cntr_data->irq_cnt) > 0
				|| !tmcdrvdata->enable
				|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_PCIE
				|| !byte_cntr_data->pcie_chan_opened);
			if (ret == -ERESTARTSYS || !tmcdrvdata->enable
			|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_PCIE
			|| !byte_cntr_data->pcie_chan_opened)
				break;
		}

		actual = PCIE_BLK_SIZE;
		buf = (char *)(tmcdrvdata->buf + byte_cntr_data->offset);
		req = kzalloc(sizeof(*req), GFP_KERNEL);
		if (!req)
			break;

		tmc_etr_read_bytes(byte_cntr_data, &byte_cntr_data->offset,
					PCIE_BLK_SIZE, &actual, &buf);

		if (actual <= 0) {
			kfree(req);
			req = NULL;
			break;
		}

		req->buf = buf;
		req->client = byte_cntr_data->out_handle;
		req->context = byte_cntr_data;
		req->len = actual;
		req->chan = byte_cntr_data->pcie_out_chan;
		req->mode = DMA_ASYNC;
		req->client_cb = etr_pcie_write_complete_cb;
		req->snd_cmpl = 1;

		bytes_to_write = mhi_dev_write_channel(req);
		if (bytes_to_write != PCIE_BLK_SIZE) {
			dev_err(tmcdrvdata->dev, "Write error %d\n",
							bytes_to_write);

			kfree(req);
			req = NULL;
			break;
		}

		mutex_lock(&byte_cntr_data->byte_cntr_lock);
		if (byte_cntr_data->offset + actual >= tmcdrvdata->size)
			byte_cntr_data->offset = 0;
		else
			byte_cntr_data->offset += actual;
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	}
}

int etr_register_pcie_channel(struct byte_cntr *byte_cntr_data)
{
	return mhi_register_state_cb(etr_pcie_client_cb, byte_cntr_data,
					byte_cntr_data->pcie_out_chan);
}

static int etr_pcie_init(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return -EIO;

	byte_cntr_data->pcie_out_chan = MHI_CLIENT_QDSS_IN;
	byte_cntr_data->offset = 0;
	byte_cntr_data->pcie_chan_opened = false;
	INIT_WORK(&byte_cntr_data->pcie_open_work, etr_pcie_open_work_fn);
	INIT_WORK(&byte_cntr_data->pcie_write_work, etr_pcie_write_work_fn);
	init_waitqueue_head(&byte_cntr_data->pcie_wait_wq);
	byte_cntr_data->pcie_wq = create_singlethread_workqueue("etr_pcie");
	if (!byte_cntr_data->pcie_wq)
		return -ENOMEM;

	return etr_register_pcie_channel(byte_cntr_data);
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

	etr_pcie_init(byte_cntr_data);
	return byte_cntr_data;
}
EXPORT_SYMBOL(byte_cntr_init);
