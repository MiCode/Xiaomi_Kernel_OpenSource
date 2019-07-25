// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight Trace Memory Controller driver
 */
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/of_irq.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/usb/usb_qdss.h>

#include "coresight-byte-cntr.h"
#include "coresight-priv.h"
#include "coresight-tmc.h"

#define USB_BLK_SIZE 65536
#define USB_BUF_NUM 255

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

	if (tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_USB
	    && byte_cntr_data->sw_usb) {
		atomic_inc(&byte_cntr_data->irq_cnt);
		wake_up(&byte_cntr_data->usb_wait_wq);
	} else if (tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
		atomic_inc(&byte_cntr_data->irq_cnt);
		wake_up(&byte_cntr_data->wq);
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
			if (!len) {
				ret = -EINVAL;
				goto err0;
			}
		} else {
			tmc_etr_read_bytes(byte_cntr_data, ppos,
						   byte_cntr_data->block_size,
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


static int tmc_etr_byte_cntr_release(struct inode *in, struct file *fp)
{
	struct byte_cntr *byte_cntr_data = fp->private_data;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	byte_cntr_data->read_active = false;

	coresight_csr_set_byte_cntr(byte_cntr_data->csr, 0);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

	return 0;
}

int usb_bypass_start(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return -ENOMEM;

	mutex_lock(&byte_cntr_data->usb_bypass_lock);

	if (!tmcdrvdata->enable) {
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return -EINVAL;
	}

	atomic_set(&byte_cntr_data->usb_free_buf, USB_BUF_NUM);
	byte_cntr_data->offset = tmcdrvdata->etr_buf->offset;
	/*
	 * IRQ is a '8- byte' counter and to observe interrupt at
	 * 'block_size' bytes of data
	 */
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, USB_BLK_SIZE / 8);

	atomic_set(&byte_cntr_data->irq_cnt, 0);
	mutex_unlock(&byte_cntr_data->usb_bypass_lock);

	return 0;
}

void usb_bypass_stop(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->usb_bypass_lock);
	wake_up(&byte_cntr_data->usb_wait_wq);
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, 0);
	mutex_unlock(&byte_cntr_data->usb_bypass_lock);

}
EXPORT_SYMBOL(usb_bypass_stop);

static int tmc_etr_byte_cntr_open(struct inode *in, struct file *fp)
{
	struct byte_cntr *byte_cntr_data =
			container_of(in->i_cdev, struct byte_cntr, dev);

	mutex_lock(&byte_cntr_data->byte_cntr_lock);

	if (!byte_cntr_data->enable || !byte_cntr_data->block_size) {
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

static void usb_read_work_fn(struct work_struct *work)
{
	int ret, seq = 0;
	struct qdss_request *usb_req = NULL;
	struct etr_buf *etr_buf = tmcdrvdata->etr_buf;
	size_t actual, req_size;
	struct byte_cntr *drvdata =
		container_of(work, struct byte_cntr, read_work);

	while (tmcdrvdata->enable
		&& tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		if (!atomic_read(&drvdata->irq_cnt)) {
			ret = wait_event_interruptible(drvdata->usb_wait_wq,
				atomic_read(&drvdata->irq_cnt) > 0
				|| !tmcdrvdata->enable || tmcdrvdata->out_mode
				!= TMC_ETR_OUT_MODE_USB);
			if (ret == -ERESTARTSYS || !tmcdrvdata->enable
			|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_USB)
				break;
		}

		req_size = USB_BLK_SIZE;
		while (req_size > 0) {
			seq++;
			usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);
			if (!usb_req)
				return;
			actual = tmc_etr_buf_get_data(etr_buf, drvdata->offset,
					req_size, &usb_req->buf);
			if (actual <= 0) {
				kfree(usb_req);
				usb_req = NULL;
				dev_err(tmcdrvdata->dev, "No data in ETR\n");
				break;
			}
			usb_req->length = actual;
			drvdata->usb_req = usb_req;
			req_size -= actual;
			if ((drvdata->offset + usb_req->length)
					>= tmcdrvdata->size)
				drvdata->offset = 0;
			else
				drvdata->offset += usb_req->length;
			if (atomic_read(&drvdata->usb_free_buf) > 0) {
				ret = usb_qdss_write(tmcdrvdata->usbch,
						drvdata->usb_req);
				if (ret) {
					kfree(usb_req);
					usb_req = NULL;
					drvdata->usb_req = NULL;
					dev_err(tmcdrvdata->dev,
						"Write data failed\n");
					continue;
				}
				atomic_dec(&drvdata->usb_free_buf);

			} else {
				dev_dbg(tmcdrvdata->dev,
				"Drop data, offset = %d, seq = %d, irq = %d\n",
					drvdata->offset, seq,
					atomic_read(&drvdata->irq_cnt));
				kfree(usb_req);
				drvdata->usb_req = NULL;
			}
		}
		if (atomic_read(&drvdata->irq_cnt) > 0)
			atomic_dec(&drvdata->irq_cnt);
	}
	dev_err(tmcdrvdata->dev, "TMC has been stopped.\n");
}

static void usb_write_done(struct byte_cntr *drvdata,
				   struct qdss_request *d_req)
{
	atomic_inc(&drvdata->usb_free_buf);
	if (d_req->status)
		pr_err_ratelimited("USB write failed err:%d\n", d_req->status);
	kfree(d_req);
}

void usb_bypass_notifier(void *priv, unsigned int event,
			struct qdss_request *d_req, struct usb_qdss_ch *ch)
{
	struct byte_cntr *drvdata = priv;

	if (!drvdata)
		return;

	switch (event) {
	case USB_QDSS_CONNECT:
		usb_qdss_alloc_req(ch, USB_BUF_NUM, 0);
		usb_bypass_start(drvdata);
		queue_work(drvdata->usb_wq, &(drvdata->read_work));
		break;

	case USB_QDSS_DISCONNECT:
		usb_bypass_stop(drvdata);
		break;

	case USB_QDSS_DATA_WRITE_DONE:
		usb_write_done(drvdata, d_req);
		break;

	default:
		break;
	}
}
EXPORT_SYMBOL(usb_bypass_notifier);


static int usb_bypass_init(struct byte_cntr *byte_cntr_data)
{
	byte_cntr_data->usb_wq = create_singlethread_workqueue("byte-cntr");
	if (!byte_cntr_data->usb_wq)
		return -ENOMEM;

	byte_cntr_data->offset = 0;
	mutex_init(&byte_cntr_data->usb_bypass_lock);
	init_waitqueue_head(&byte_cntr_data->usb_wait_wq);
	atomic_set(&byte_cntr_data->usb_free_buf, USB_BUF_NUM);
	INIT_WORK(&(byte_cntr_data->read_work), usb_read_work_fn);

	return 0;
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

	byte_cntr_data->sw_usb = of_property_read_bool(np, "qcom,sw-usb");
	if (byte_cntr_data->sw_usb) {
		ret = usb_bypass_init(byte_cntr_data);
		if (ret)
			return NULL;
	}
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
