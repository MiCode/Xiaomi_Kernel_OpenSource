// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
#include <linux/time.h>

#include "coresight-byte-cntr.h"
#include "coresight-priv.h"
#include "coresight-tmc.h"

#define USB_BLK_SIZE 65536
#define USB_SG_NUM (USB_BLK_SIZE / PAGE_SIZE)
#define USB_BUF_NUM 255
#define USB_TIME_OUT (5 * HZ)

static struct tmc_drvdata *tmcdrvdata;

static void tmc_etr_read_bytes(struct byte_cntr *byte_cntr_data, loff_t *ppos,
			       size_t bytes, size_t *len, char **bufp)
{
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;
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
	dma_addr_t paddr = tmcdrvdata->sysfs_buf->hwaddr;

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
	long offset;

	if (!byte_cntr_data)
		return -ENOMEM;

	mutex_lock(&byte_cntr_data->usb_bypass_lock);

	if (!tmcdrvdata->enable) {
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return -EINVAL;
	}

	atomic_set(&byte_cntr_data->usb_free_buf, USB_BUF_NUM);

	offset = tmc_sg_get_rwp_offset(tmcdrvdata);
	if (offset < 0) {
		dev_err(&tmcdrvdata->csdev->dev,
			"%s: invalid rwp offset value\n", __func__);
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return offset;
	}
	byte_cntr_data->offset = offset;

	byte_cntr_data->read_active = true;
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
	if (byte_cntr_data->read_active)
		byte_cntr_data->read_active = false;
	else {
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return;
	}
	wake_up(&byte_cntr_data->usb_wait_wq);
	pr_info("coresight: stop usb bypass\n");
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, 0);
	mutex_unlock(&byte_cntr_data->usb_bypass_lock);

}
EXPORT_SYMBOL(usb_bypass_stop);

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

static int usb_transfer_small_packet(struct qdss_request *usb_req,
			struct byte_cntr *drvdata, size_t *small_size)
{
	int ret = 0;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;
	size_t req_size, actual;
	long w_offset;

	w_offset = tmc_sg_get_rwp_offset(tmcdrvdata);
	if (w_offset < 0) {
		ret = w_offset;
		dev_err_ratelimited(&tmcdrvdata->csdev->dev,
			"%s: RWP offset is invalid\n", __func__);
		goto out;
	}

	req_size = ((w_offset < drvdata->offset) ? etr_buf->size : 0) +
				w_offset - drvdata->offset;
	req_size = ((req_size + *small_size) < USB_BLK_SIZE) ? req_size :
		(USB_BLK_SIZE - *small_size);

	while (req_size > 0) {

		usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);
		if (!usb_req) {
			ret = -EFAULT;
			goto out;
		}

		actual = tmc_etr_buf_get_data(etr_buf, drvdata->offset,
					req_size, &usb_req->buf);

		if (actual <= 0 || actual > req_size) {
			kfree(usb_req);
			usb_req = NULL;
			dev_err_ratelimited(&tmcdrvdata->csdev->dev,
				"%s: Invalid data in ETR\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		usb_req->length = actual;
		drvdata->usb_req = usb_req;
		req_size -= actual;

		if ((drvdata->offset + actual) >=
				tmcdrvdata->sysfs_buf->size)
			drvdata->offset = 0;
		else
			drvdata->offset += actual;

		*small_size += actual;

		if (atomic_read(&drvdata->usb_free_buf) > 0) {
			ret = usb_qdss_write(tmcdrvdata->usbch, usb_req);

			if (ret) {
				kfree(usb_req);
				usb_req = NULL;
				drvdata->usb_req = NULL;
				dev_err_ratelimited(tmcdrvdata->dev,
					"Write data failed:%d\n", ret);
				goto out;
			}

			atomic_dec(&drvdata->usb_free_buf);
		} else {
			dev_dbg(tmcdrvdata->dev,
			"Drop data, offset = %d, len = %d\n",
				drvdata->offset, req_size);
			kfree(usb_req);
			drvdata->usb_req = NULL;
		}
	}

out:
	return ret;
}

static void usb_read_work_fn(struct work_struct *work)
{
	int ret, i, seq = 0;
	struct qdss_request *usb_req = NULL;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;
	size_t actual, req_size, req_sg_num, small_size = 0;
	size_t actual_total = 0;
	char *buf;
	struct byte_cntr *drvdata =
		container_of(work, struct byte_cntr, read_work);


	while (tmcdrvdata->enable
		&& tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		if (!atomic_read(&drvdata->irq_cnt)) {
			ret =  wait_event_interruptible_timeout(
				drvdata->usb_wait_wq,
				atomic_read(&drvdata->irq_cnt) > 0
				|| !tmcdrvdata->enable || tmcdrvdata->out_mode
				!= TMC_ETR_OUT_MODE_USB
				|| !drvdata->read_active, USB_TIME_OUT);
			if (ret == -ERESTARTSYS || !tmcdrvdata->enable
			|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_USB
			|| !drvdata->read_active)
				break;

			if (ret == 0) {
				ret = usb_transfer_small_packet(usb_req,
						drvdata, &small_size);
				if (ret && ret != -EAGAIN)
					return;
				continue;
			}
		}

		req_size = USB_BLK_SIZE - small_size;
		small_size = 0;
		actual_total = 0;

		if (req_size > 0) {
			seq++;
			req_sg_num = (req_size - 1) / PAGE_SIZE + 1;
			usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);
			if (!usb_req)
				return;
			usb_req->sg = kcalloc(req_sg_num,
				sizeof(*(usb_req->sg)), GFP_KERNEL);
			if (!usb_req->sg) {
				kfree(usb_req);
				usb_req = NULL;
				return;
			}

			for (i = 0; i < req_sg_num; i++) {
				actual = tmc_etr_buf_get_data(etr_buf,
							drvdata->offset,
							PAGE_SIZE, &buf);

				if (actual <= 0 || actual > PAGE_SIZE) {
					kfree(usb_req->sg);
					kfree(usb_req);
					usb_req = NULL;
					dev_err_ratelimited(
						&tmcdrvdata->csdev->dev,
						"Invalid data in ETR\n");
					return;
				}

				sg_set_buf(&usb_req->sg[i], buf, actual);

				if (i == 0)
					usb_req->buf = buf;
				if (i == req_sg_num - 1)
					sg_mark_end(&usb_req->sg[i]);

				if ((drvdata->offset + actual) >=
					tmcdrvdata->sysfs_buf->size)
					drvdata->offset = 0;
				else
					drvdata->offset += actual;
				actual_total += actual;
			}

			usb_req->length = actual_total;
			drvdata->usb_req = usb_req;
			usb_req->num_sgs = i;

			if (atomic_read(&drvdata->usb_free_buf) > 0) {
				ret = usb_qdss_write(tmcdrvdata->usbch,
						drvdata->usb_req);
				if (ret) {
					kfree(usb_req->sg);
					kfree(usb_req);
					usb_req = NULL;
					drvdata->usb_req = NULL;
					dev_err_ratelimited(tmcdrvdata->dev,
						"Write data failed:%d\n", ret);
					if (ret == -EAGAIN)
						continue;
					return;
				}
				atomic_dec(&drvdata->usb_free_buf);

			} else {
				dev_dbg(tmcdrvdata->dev,
				"Drop data, offset = %d, seq = %d, irq = %d\n",
					drvdata->offset, seq,
					atomic_read(&drvdata->irq_cnt));
				kfree(usb_req->sg);
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
	kfree(d_req->sg);
	kfree(d_req);
}

void usb_bypass_notifier(void *priv, unsigned int event,
			struct qdss_request *d_req, struct usb_qdss_ch *ch)
{
	struct byte_cntr *drvdata = priv;
	int ret;

	if (!drvdata)
		return;

	if (tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_USB
				|| tmcdrvdata->mode == CS_MODE_DISABLED) {
		dev_err(&tmcdrvdata->csdev->dev,
		"%s: ETR is not USB mode, or ETR is disabled.\n", __func__);
		return;
	}

	switch (event) {
	case USB_QDSS_CONNECT:
		ret = usb_bypass_start(drvdata);
		if (ret < 0)
			return;

		usb_qdss_alloc_req(ch, USB_BUF_NUM);
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
