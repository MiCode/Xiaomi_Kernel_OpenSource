// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#define KMSG_COMPONENT "QDSS diag bridge"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/mhi.h>
#include <linux/usb/usb_qdss.h>
#include <linux/of.h>
#include <linux/delay.h>
#include "qdss_bridge.h"

#define MODULE_NAME "qdss_bridge"
#define INIT_STATUS -1
#define DEFAULT_CHANNEL "QDSS"

static struct class *mhi_class;
static enum mhi_state dev_state = INIT_STATUS;
static enum mhi_ch curr_chan;
static struct qdss_bridge_drvdata *bridge_drvdata;

static const char * const str_mhi_curr_chan[] = {
		[QDSS]			= "QDSS",
		[QDSS_HW]		= "IP_HW_QDSS",
		[EMPTY]			= "EMPTY",
};

static const char * const str_mhi_transfer_mode[] = {
		[MHI_TRANSFER_TYPE_USB]			= "usb",
		[MHI_TRANSFER_TYPE_UCI]			= "uci",
};

static int qdss_destroy_mhi_buf_tbl(struct qdss_bridge_drvdata *drvdata)
{
	struct list_head *start, *temp;
	struct qdss_mhi_buf_tbl_t *entry = NULL;

	spin_lock_bh(&drvdata->lock);
	list_for_each_safe(start, temp, &drvdata->mhi_buf_tbl) {
		entry = list_entry(start, struct qdss_mhi_buf_tbl_t, link);
		list_del(&entry->link);
		kfree(entry->buf);
		kfree(entry);
	}
	spin_unlock_bh(&drvdata->lock);

	return 0;
}

static int qdss_destroy_buf_tbl(struct qdss_bridge_drvdata *drvdata)
{
	struct list_head *start, *temp;
	struct qdss_buf_tbl_lst *entry = NULL;

	spin_lock_bh(&drvdata->lock);
	list_for_each_safe(start, temp, &drvdata->buf_tbl) {
		entry = list_entry(start, struct qdss_buf_tbl_lst, link);
		list_del(&entry->link);
		kfree(entry->buf);
		kfree(entry->usb_req);
		kfree(entry);
	}
	spin_unlock_bh(&drvdata->lock);

	return 0;
}

static int qdss_destroy_read_done_list(struct qdss_bridge_drvdata *drvdata)
{
	struct list_head *start, *temp;
	struct qdss_mhi_buf_tbl_t *entry = NULL;

	spin_lock_bh(&drvdata->lock);
	list_for_each_safe(start, temp, &drvdata->read_done_list) {
		entry = list_entry(start, struct qdss_mhi_buf_tbl_t, link);
		list_del(&entry->link);
		kfree(entry);
	}
	spin_unlock_bh(&drvdata->lock);

	return 0;
}

static int qdss_create_buf_tbl(struct qdss_bridge_drvdata *drvdata)
{
	struct qdss_buf_tbl_lst *entry;
	void *buf;
	struct qdss_request *usb_req;
	int i;
	struct mhi_device *mhi_dev = drvdata->mhi_dev;

	drvdata->nr_trbs = mhi_get_free_desc_count(mhi_dev,
							DMA_FROM_DEVICE);

	for (i = 0; i < drvdata->nr_trbs; i++) {
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry)
			goto err;

		buf = kzalloc(drvdata->mtu, GFP_KERNEL);
		usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);

		entry->buf = buf;
		entry->usb_req = usb_req;
		atomic_set(&entry->available, 1);
		list_add_tail(&entry->link, &drvdata->buf_tbl);

		if (!buf || !usb_req)
			goto err;
	}

	return 0;
err:
	qdss_destroy_buf_tbl(drvdata);
	return -ENOMEM;
}

struct qdss_buf_tbl_lst *qdss_get_buf_tbl_entry(
					struct qdss_bridge_drvdata *drvdata,
					void *buf)
{
	struct qdss_buf_tbl_lst *entry;

	spin_lock_bh(&drvdata->lock);
	list_for_each_entry(entry, &drvdata->buf_tbl, link) {
		if (atomic_read(&entry->available))
			continue;
		if (entry->buf == buf) {
			spin_unlock_bh(&drvdata->lock);
			return entry;
		}
	}

	spin_unlock_bh(&drvdata->lock);
	return NULL;
}

static int qdss_check_entry(struct qdss_bridge_drvdata *drvdata)
{
	struct qdss_buf_tbl_lst *entry;
	int ret = 0;

	list_for_each_entry(entry, &drvdata->buf_tbl, link) {
		if (atomic_read(&entry->available) == 0
			&& atomic_read(&entry->used) == 1) {
			ret = 1;
			return ret;
		}
	}

	return ret;
}

static void qdss_del_buf_tbl_entry(struct qdss_bridge_drvdata *drvdata,
				void *buf)
{
	struct qdss_mhi_buf_tbl_t *entry, *tmp;

	spin_lock_bh(&drvdata->lock);
	list_for_each_entry_safe(entry, tmp, &drvdata->mhi_buf_tbl, link) {
		if (entry->buf == buf) {
			list_del(&entry->link);
			kfree(entry->buf);
			kfree(entry);
			spin_unlock_bh(&drvdata->lock);
			return;
		}
	}
	spin_unlock_bh(&drvdata->lock);
}

struct qdss_buf_tbl_lst *qdss_get_entry(struct qdss_bridge_drvdata *drvdata)
{
	struct qdss_buf_tbl_lst *item;

	list_for_each_entry(item, &drvdata->buf_tbl, link)
		if (atomic_cmpxchg(&item->available, 1, 0) == 1)
			return item;

	return NULL;
}

static void qdss_buf_tbl_remove(struct qdss_bridge_drvdata *drvdata,
				void *buf)
{
	struct qdss_buf_tbl_lst *entry = NULL;

	spin_lock_bh(&drvdata->lock);
	list_for_each_entry(entry, &drvdata->buf_tbl, link) {
		if (entry->buf != buf)
			continue;
		atomic_set(&entry->available, 1);
		atomic_set(&entry->used, 0);
		spin_unlock_bh(&drvdata->lock);
		return;
	}
	spin_unlock_bh(&drvdata->lock);
	pr_err_ratelimited("Failed to find buffer for removal\n");
}

static void mhi_ch_close(struct qdss_bridge_drvdata *drvdata)
{
	if (drvdata->mode == MHI_TRANSFER_TYPE_USB) {
		qdss_destroy_buf_tbl(drvdata);
		qdss_destroy_read_done_list(drvdata);
	} else if (drvdata->mode == MHI_TRANSFER_TYPE_UCI) {
		qdss_destroy_mhi_buf_tbl(drvdata);
		drvdata->cur_buf = NULL;
		qdss_destroy_read_done_list(drvdata);
	}
}

static ssize_t mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qdss_bridge_drvdata *drvdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			str_mhi_transfer_mode[drvdata->mode]);
}

static ssize_t curr_chan_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (curr_chan < QDSS || curr_chan > EMPTY)
		return -EINVAL;
	return scnprintf(buf, PAGE_SIZE, "%s\n", str_mhi_curr_chan[curr_chan]);
}

static ssize_t mode_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct qdss_bridge_drvdata *drvdata = dev_get_drvdata(dev);
	char str[10] = "";
	int ret;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%3s", str) != 1)
		return -EINVAL;

	spin_lock_bh(&drvdata->lock);
	if (!strcmp(str, str_mhi_transfer_mode[MHI_TRANSFER_TYPE_UCI])) {
		if (drvdata->mode == MHI_TRANSFER_TYPE_USB) {
			if (drvdata->opened == ENABLE) {
				drvdata->opened = DISABLE;
				spin_unlock_bh(&drvdata->lock);
				usb_qdss_close(drvdata->usb_ch);
				mhi_unprepare_from_transfer(drvdata->mhi_dev);
				flush_workqueue(drvdata->mhi_wq);
				mhi_ch_close(drvdata);
				drvdata->mode = MHI_TRANSFER_TYPE_UCI;
			} else if (drvdata->opened == DISABLE) {
				drvdata->mode = MHI_TRANSFER_TYPE_UCI;
				spin_unlock_bh(&drvdata->lock);
			} else {
				ret = -ERESTARTSYS;
				goto out;
			}
		} else
			spin_unlock_bh(&drvdata->lock);

	} else if (!strcmp(str, str_mhi_transfer_mode[MHI_TRANSFER_TYPE_USB])) {
		if (drvdata->mode == MHI_TRANSFER_TYPE_UCI) {
			if (drvdata->opened == ENABLE) {
				drvdata->opened = DISABLE;
				spin_unlock_bh(&drvdata->lock);
				wake_up(&drvdata->uci_wq);
				mhi_unprepare_from_transfer(drvdata->mhi_dev);
				flush_workqueue(drvdata->mhi_wq);
				mhi_ch_close(drvdata);
				drvdata->mode = MHI_TRANSFER_TYPE_USB;
				queue_work(drvdata->mhi_wq,
						&drvdata->open_work);
			} else if (drvdata->opened == DISABLE) {
				drvdata->mode = MHI_TRANSFER_TYPE_USB;
				spin_unlock_bh(&drvdata->lock);
				queue_work(drvdata->mhi_wq,
						&drvdata->open_work);
			} else {
				ret = -ERESTARTSYS;
				goto out;
			}
		} else
			spin_unlock_bh(&drvdata->lock);

	} else {
		ret = -EINVAL;
		goto out;
	}

	ret = size;
	return ret;
out:
	spin_unlock_bh(&drvdata->lock);
	return ret;
}

static DEVICE_ATTR_RW(mode);
static DEVICE_ATTR_RO(curr_chan);

static struct attribute *qdss_bridge_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_curr_chan.attr,
	NULL,
};

static const struct attribute_group qdss_bridge_group = {
	.attrs = qdss_bridge_attrs,
};

static const struct attribute_group *qdss_bridge_groups[] = {
	&qdss_bridge_group,
	NULL,
};

static void mhi_read_work_fn(struct work_struct *work)
{
	int err = 0;
	enum mhi_flags mhi_flags = MHI_EOT;
	struct qdss_buf_tbl_lst *entry;

	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     read_work);

	do {
		spin_lock_bh(&drvdata->lock);
		if (drvdata->opened != ENABLE) {
			spin_unlock_bh(&drvdata->lock);
			break;
		}
		entry = qdss_get_entry(drvdata);
		if (!entry) {
			spin_unlock_bh(&drvdata->lock);
			break;
		}

		err = mhi_queue_buf(drvdata->mhi_dev, DMA_FROM_DEVICE,
					entry->buf, drvdata->mtu, mhi_flags);
		if (err) {
			pr_err_ratelimited("Unable to read from MHI buffer err:%d",
					   err);
			goto fail;
		}
		spin_unlock_bh(&drvdata->lock);

	} while (entry);

	return;
fail:
	spin_unlock_bh(&drvdata->lock);
	qdss_buf_tbl_remove(drvdata, entry->buf);
	queue_work(drvdata->mhi_wq, &drvdata->read_work);
}

static int mhi_queue_read(struct qdss_bridge_drvdata *drvdata)
{
	queue_work(drvdata->mhi_wq, &(drvdata->read_work));
	return 0;
}

static int usb_write(struct qdss_bridge_drvdata *drvdata,
			     unsigned char *buf, size_t len)
{
	int ret = 0;
	struct qdss_buf_tbl_lst *entry;

	entry = qdss_get_buf_tbl_entry(drvdata, buf);
	if (!entry)
		return -EINVAL;

	entry->usb_req->buf = buf;
	entry->usb_req->length = len;
	atomic_set(&entry->used, 1);
	ret = usb_qdss_write(drvdata->usb_ch, entry->usb_req);

	return ret;
}

static void mhi_read_done_work_fn(struct work_struct *work)
{
	unsigned char *buf = NULL;
	int err = 0;
	size_t len = 0;
	struct qdss_mhi_buf_tbl_t *tp, *_tp;
	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     read_done_work);
	LIST_HEAD(head);

	do {
		spin_lock_bh(&drvdata->lock);
		if (drvdata->opened != ENABLE
		    || drvdata->mode != MHI_TRANSFER_TYPE_USB) {
			spin_unlock_bh(&drvdata->lock);
			break;
		}

		if (list_empty(&drvdata->read_done_list)) {
			spin_unlock_bh(&drvdata->lock);
			break;
		}
		list_splice_tail_init(&drvdata->read_done_list, &head);
		spin_unlock_bh(&drvdata->lock);

		list_for_each_entry_safe(tp, _tp, &head, link) {
			list_del(&tp->link);
			buf = tp->buf;
			len = tp->len;
			kfree(tp);
			if (!buf)
				break;
			pr_debug("Read from mhi buf %pK len:%zd\n", buf, len);
			/*
			 * The read buffers can come after the MHI channels are
			 * closed. If the channels are closed at the time of
			 * read, discard the buffers here and do not forward
			 * them to the mux layer.
			 */
			spin_lock_bh(&drvdata->lock);
			if (drvdata->mode == MHI_TRANSFER_TYPE_USB) {
				if (drvdata->opened == ENABLE) {
					spin_unlock_bh(&drvdata->lock);
					err = usb_write(drvdata, buf, len);
					if (err)
						qdss_buf_tbl_remove(drvdata,
								    buf);
				} else if (drvdata->opened == DISABLE) {
					spin_unlock_bh(&drvdata->lock);
					qdss_buf_tbl_remove(drvdata, buf);
				} else {
					spin_unlock_bh(&drvdata->lock);
				}
			} else {
				spin_unlock_bh(&drvdata->lock);
			}
		}
		list_del_init(&head);
	} while (buf);
}

static void usb_write_done(struct qdss_bridge_drvdata *drvdata,
				   struct qdss_request *d_req)
{
	if (d_req->status)
		pr_err_ratelimited("USB write failed err:%d\n", d_req->status);

	qdss_buf_tbl_remove(drvdata, d_req->buf);
	mhi_queue_read(drvdata);
}

static void usb_notifier(void *priv, unsigned int event,
			struct qdss_request *d_req, struct usb_qdss_ch *ch)
{
	struct qdss_bridge_drvdata *drvdata = priv;

	if (!drvdata || drvdata->mode != MHI_TRANSFER_TYPE_USB) {
		pr_err_ratelimited("%s can't be called in invalid status.\n",
				__func__);
		return;
	}

	switch (event) {
	case USB_QDSS_CONNECT:
		if (drvdata->opened == ENABLE) {
			usb_qdss_alloc_req(ch, drvdata->nr_trbs);
			mhi_queue_read(drvdata);
		}
		break;

	case USB_QDSS_DISCONNECT:
		if (drvdata->opened == ENABLE)
			usb_qdss_free_req(drvdata->usb_ch);
		break;

	case USB_QDSS_DATA_WRITE_DONE:
		usb_write_done(drvdata, d_req);
		break;

	default:
		break;
	}
}

static int mhi_ch_open(struct qdss_bridge_drvdata *drvdata)
{
	int ret;

	spin_lock_bh(&drvdata->lock);
	if (drvdata->opened == ENABLE) {
		spin_unlock_bh(&drvdata->lock);
		return 0;
	}
	if (drvdata->opened == SSR) {
		spin_unlock_bh(&drvdata->lock);
		return -ERESTARTSYS;
	}
	spin_unlock_bh(&drvdata->lock);

	ret = mhi_prepare_for_transfer(drvdata->mhi_dev);
	if (ret) {
		pr_err("Unable to open MHI channel\n");
		return ret;
	}

	return 0;
}

static void qdss_bridge_open_work_fn(struct work_struct *work)
{
	struct qdss_bridge_drvdata *drvdata =
				container_of(work,
					     struct qdss_bridge_drvdata,
					     open_work);
	int ret;

	ret = mhi_ch_open(drvdata);
	if (ret)
		goto err_open;

	ret = qdss_create_buf_tbl(drvdata);
	if (ret)
		goto err;

	drvdata->usb_ch = usb_qdss_open("qdss_mdm", drvdata, usb_notifier);
	if (IS_ERR_OR_NULL(drvdata->usb_ch)) {
		ret = PTR_ERR(drvdata->usb_ch);
		goto err;
	}

	drvdata->opened = ENABLE;
	return;
err:
	mhi_unprepare_from_transfer(drvdata->mhi_dev);
	mhi_ch_close(drvdata);
err_open:
	pr_err("Open work failed with err:%d\n", ret);
}

static void qdss_mhi_write_cb(struct mhi_device *mhi_dev,
				struct mhi_result *result)
{
}

static void qdss_mhi_read_cb(struct mhi_device *mhi_dev,
				struct mhi_result *result)
{
	struct qdss_bridge_drvdata *drvdata = NULL;
	struct qdss_mhi_buf_tbl_t *tp;
	void *buf = NULL;

	drvdata = bridge_drvdata;
	if (!drvdata)
		return;
	buf = result->buf_addr;

	spin_lock_bh(&drvdata->lock);
	if (drvdata->opened == ENABLE &&
	    result->transaction_status != -ENOTCONN) {
		spin_unlock_bh(&drvdata->lock);
		tp = kmalloc(sizeof(*tp), GFP_ATOMIC);
		if (!tp)
			return;
		tp->buf = buf;
		tp->len = result->bytes_xferd;
		spin_lock_bh(&drvdata->lock);
		list_add_tail(&tp->link, &drvdata->read_done_list);
		spin_unlock_bh(&drvdata->lock);
		if (drvdata->mode == MHI_TRANSFER_TYPE_USB)
			queue_work(drvdata->mhi_wq, &drvdata->read_done_work);
		else
			wake_up(&drvdata->uci_wq);
	} else {
		if (drvdata->mode == MHI_TRANSFER_TYPE_USB) {
			spin_unlock_bh(&drvdata->lock);
			qdss_buf_tbl_remove(drvdata, buf);
		} else {
			spin_unlock_bh(&drvdata->lock);
			return;
		}
	}

}

static int mhi_uci_release(struct inode *inode, struct file *file)
{
	struct qdss_bridge_drvdata *drvdata = file->private_data;

	spin_lock_bh(&drvdata->lock);
	if (drvdata->mode == MHI_TRANSFER_TYPE_UCI) {
		if (drvdata->opened == ENABLE) {
			drvdata->opened = DISABLE;
			spin_unlock_bh(&drvdata->lock);
			wake_up(&drvdata->uci_wq);
			mhi_unprepare_from_transfer(drvdata->mhi_dev);
			flush_workqueue(drvdata->mhi_wq);
			mhi_ch_close(drvdata);
		} else if (drvdata->opened == SSR) {
			spin_unlock_bh(&drvdata->lock);
			complete(&drvdata->completion);
		} else
			spin_unlock_bh(&drvdata->lock);
	} else
		spin_unlock_bh(&drvdata->lock);

	return 0;
}

static ssize_t mhi_uci_read(struct file *file,
			char __user *buf,
			size_t count,
			loff_t *ppos)
{
	struct qdss_bridge_drvdata *drvdata = file->private_data;
	struct mhi_device *mhi_dev = drvdata->mhi_dev;
	struct qdss_mhi_buf_tbl_t *uci_buf;
	char *ptr;
	size_t to_copy;
	int ret = 0;

	if (!buf)
		return -EINVAL;

	pr_debug("Client provided buf len:%lu\n", count);

	/* confirm channel is active */
	spin_lock_bh(&drvdata->lock);
	if (drvdata->opened != ENABLE ||
	    drvdata->mode != MHI_TRANSFER_TYPE_UCI) {
		spin_unlock_bh(&drvdata->lock);
		return -ERESTARTSYS;
	}

	/* No data available to read, wait */
	if (!drvdata->cur_buf && list_empty(&drvdata->read_done_list)) {
		spin_unlock_bh(&drvdata->lock);

		pr_debug("No data available to read waiting\n");
		ret = wait_event_interruptible(drvdata->uci_wq,
				((drvdata->opened != ENABLE
				 || !list_empty(&drvdata->read_done_list))));
		if (ret == -ERESTARTSYS) {
			pr_debug("Exit signal caught for node\n");
			return -ERESTARTSYS;
		}

		spin_lock_bh(&drvdata->lock);
		if (drvdata->opened != ENABLE) {
			spin_unlock_bh(&drvdata->lock);
			pr_debug("node was disabled or SSR occurred.\n");
			ret = -ERESTARTSYS;
			return ret;
		}
	}

	/* new read, get the next descriptor from the list */
	if (!drvdata->cur_buf) {
		uci_buf = list_first_entry_or_null(&drvdata->read_done_list,
					struct qdss_mhi_buf_tbl_t, link);
		if (unlikely(!uci_buf)) {
			ret = -EIO;
			goto read_error;
		}

		list_del(&uci_buf->link);
		drvdata->cur_buf = uci_buf;
		drvdata->rx_size = uci_buf->len;
		pr_debug("Got pkt of size:%zu\n", drvdata->rx_size);
	}

	uci_buf = drvdata->cur_buf;
	spin_unlock_bh(&drvdata->lock);

	/* Copy the buffer to user space */
	to_copy = min_t(size_t, count, drvdata->rx_size);
	ptr = uci_buf->buf + (uci_buf->len - drvdata->rx_size);
	ret = copy_to_user(buf, ptr, to_copy);
	if (ret)
		return ret;

	pr_debug("Copied %lu of %lu bytes\n", to_copy, drvdata->rx_size);
	drvdata->rx_size -= to_copy;

	/* we finished with this buffer, queue it back to hardware */
	if (!drvdata->rx_size) {
		spin_lock_bh(&drvdata->lock);
		drvdata->cur_buf = NULL;

		if (drvdata->opened == ENABLE)
			ret = mhi_queue_buf(mhi_dev, DMA_FROM_DEVICE,
						 uci_buf->buf, drvdata->mtu,
						 MHI_EOT);
		else
			ret = -ERESTARTSYS;

		spin_unlock_bh(&drvdata->lock);

		if (ret) {
			pr_err("Failed to recycle element, ret: %d\n", ret);
			qdss_del_buf_tbl_entry(drvdata, uci_buf->buf);
			uci_buf->buf = NULL;
			kfree(uci_buf);
			return ret;
		}
		kfree(uci_buf);
	}

	pr_debug("Returning %lu bytes\n", to_copy);
	return to_copy;

read_error:
	spin_unlock_bh(&drvdata->lock);
	return ret;
}

static int mhi_queue_inbound(struct qdss_bridge_drvdata *drvdata)
{
	struct mhi_device *mhi_dev = drvdata->mhi_dev;
	void *buf;
	struct qdss_mhi_buf_tbl_t *entry;
	int ret = -EIO, i;

	for (i = 0; i < drvdata->nr_trbs; i++) {
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry)
			goto err;

		buf = kzalloc(drvdata->mtu, GFP_KERNEL);
		if (!buf) {
			kfree(entry);
			goto err;
		}

		entry->buf = buf;

		ret = mhi_queue_buf(mhi_dev, DMA_FROM_DEVICE, buf,
					drvdata->mtu,
					MHI_EOT);
		if (ret) {
			kfree(buf);
			kfree(entry);
			pr_err("Failed to queue buffer %d\n", i);
			return ret;
		}
		list_add_tail(&entry->link, &drvdata->mhi_buf_tbl);
	}

	return ret;
err:
	return -ENOMEM;

}

static int mhi_uci_open(struct inode *inode, struct file *filp)
{
	int ret = -EIO;
	struct qdss_mhi_buf_tbl_t *buf_itr, *tmp;
	struct qdss_bridge_drvdata *drvdata = bridge_drvdata;

	spin_lock_bh(&drvdata->lock);
	if (drvdata->opened) {
		pr_err("Node was opened or SSR occurred\n");
		spin_unlock_bh(&drvdata->lock);
		return ret;
	}
	spin_unlock_bh(&drvdata->lock);

	ret = mhi_prepare_for_transfer(drvdata->mhi_dev);
	if (ret) {
		pr_err("Error starting transfer channels\n");
		return ret;
	}

	ret = mhi_queue_inbound(drvdata);
	if (ret)
		goto error_rx_queue;

	filp->private_data = drvdata;
	drvdata->opened = ENABLE;
	return ret;

error_rx_queue:
	mhi_unprepare_from_transfer(drvdata->mhi_dev);
	list_for_each_entry_safe(buf_itr, tmp, &drvdata->read_done_list, link) {
		list_del(&buf_itr->link);
		kfree(buf_itr->buf);
		kfree(buf_itr);
	}

	return ret;
}



static const struct file_operations mhidev_fops = {
	.open = mhi_uci_open,
	.release = mhi_uci_release,
	.read = mhi_uci_read,
};

static void qdss_mhi_remove(struct mhi_device *mhi_dev)
{
	struct qdss_bridge_drvdata *drvdata = NULL;

	if (!mhi_dev)
		return;
	drvdata = bridge_drvdata;
	if (!drvdata)
		return;

	pr_debug("remove dev state: %d\n", mhi_dev->mhi_cntrl->dev_state);

	dev_state = mhi_dev->mhi_cntrl->dev_state;
	if (mhi_dev->mhi_cntrl->dev_state != MHI_STATE_RESET)
		curr_chan = EMPTY;

	spin_lock_bh(&drvdata->lock);
	if (drvdata->opened == ENABLE) {
		drvdata->opened = SSR;
		if (drvdata->mode == MHI_TRANSFER_TYPE_UCI) {
			spin_unlock_bh(&drvdata->lock);
			wake_up(&drvdata->uci_wq);
			wait_for_completion(&drvdata->completion);
		} else {
			spin_unlock_bh(&drvdata->lock);
			if (drvdata->usb_ch)
				usb_qdss_close(drvdata->usb_ch);
			do {
				msleep(20);
			} while (qdss_check_entry(drvdata));
		}
		flush_workqueue(drvdata->mhi_wq);
		mhi_ch_close(drvdata);
	} else
		spin_unlock_bh(&drvdata->lock);

	device_destroy(mhi_class, drvdata->cdev->dev);
	unregister_chrdev_region(drvdata->cdev->dev, 1);
	cdev_del(drvdata->cdev);
}

int qdss_mhi_init(struct qdss_bridge_drvdata *drvdata)
{
	drvdata->mhi_wq = create_singlethread_workqueue(MODULE_NAME);
	if (!drvdata->mhi_wq)
		return -ENOMEM;

	spin_lock_init(&drvdata->lock);
	INIT_WORK(&(drvdata->read_work), mhi_read_work_fn);
	INIT_WORK(&(drvdata->read_done_work), mhi_read_done_work_fn);
	INIT_WORK(&(drvdata->open_work), qdss_bridge_open_work_fn);
	INIT_LIST_HEAD(&drvdata->buf_tbl);
	INIT_LIST_HEAD(&drvdata->mhi_buf_tbl);
	init_waitqueue_head(&drvdata->uci_wq);
	init_completion(&drvdata->completion);
	INIT_LIST_HEAD(&drvdata->read_done_list);
	drvdata->opened = DISABLE;

	return 0;
}

static int qdss_mhi_probe(struct mhi_device *mhi_dev,
				const struct mhi_device_id *id)
{
	int ret;
	unsigned int baseminor = 0;
	unsigned int count = 1;
	struct qdss_bridge_drvdata *drvdata;
	dev_t dev;

	pr_debug("probe dev state: %d chan: %s curr_chan: %d\n",
		  mhi_dev->mhi_cntrl->dev_state,
		  id->chan,
		  curr_chan);

	if (dev_state == INIT_STATUS) {
		if (strcmp(mhi_dev->name, DEFAULT_CHANNEL))
			return -EINVAL;
		if (!strcmp(id->chan, "QDSS"))
			curr_chan = QDSS;
		if (!strcmp(id->chan, "IP_HW_QDSS"))
			curr_chan = QDSS_HW;
	} else if (dev_state == MHI_STATE_RESET) {
		if (strcmp(id->chan, str_mhi_curr_chan[curr_chan]))
			return -EINVAL;
	} else {
		if (curr_chan != EMPTY) {
			pr_err("Need unbind another channel before bind.\n");
			return -EINVAL;
		}
		if (!strcmp(id->chan, "QDSS"))
			curr_chan = QDSS;
		if (!strcmp(id->chan, "IP_HW_QDSS"))
			curr_chan = QDSS_HW;
	}

	drvdata = devm_kzalloc(&mhi_dev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		ret = -ENOMEM;
		return ret;
	}
	drvdata->cdev = cdev_alloc();
	if (!drvdata->cdev) {
		ret = -ENOMEM;
		return ret;
	}

	ret = alloc_chrdev_region(&dev, baseminor, count, "mhi_qdss");
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed %d\n", ret);
		return ret;
	}

	drvdata->cdev->owner = THIS_MODULE;
	drvdata->cdev->ops = &mhidev_fops;

	ret = cdev_add(drvdata->cdev, dev, 1);
	if (ret)
		goto exit_unreg_chrdev_region;

	drvdata->dev = device_create(mhi_class, NULL,
			       drvdata->cdev->dev, drvdata,
			       "mhi_qdss");
	if (IS_ERR(drvdata->dev)) {
		pr_err("class_device_create failed %d\n", ret);
		ret = -ENOMEM;
		goto exit_cdev_add;
	}

	drvdata->mode = MHI_TRANSFER_TYPE_USB;
	drvdata->mtu = min_t(size_t, id->driver_data, MHI_MAX_MTU);
	drvdata->mhi_dev = mhi_dev;
	bridge_drvdata = drvdata;
	dev_set_drvdata(drvdata->dev, drvdata);

	ret = qdss_mhi_init(drvdata);
	if (ret) {
		pr_err("Device probe failed err:%d\n", ret);
		goto exit_destroy_device;
	}
	queue_work(drvdata->mhi_wq, &drvdata->open_work);
	return 0;

exit_destroy_device:
	device_destroy(mhi_class, drvdata->cdev->dev);
exit_cdev_add:
	cdev_del(drvdata->cdev);
exit_unreg_chrdev_region:
	unregister_chrdev_region(drvdata->cdev->dev, 1);
	return ret;

}

static const struct mhi_device_id qdss_mhi_match_table[] = {
	{ .chan = "QDSS", .driver_data = 0x8000 },
	{ .chan = "IP_HW_QDSS", .driver_data = 0x8000 },
	{},
};

static struct mhi_driver qdss_mhi_driver = {
	.id_table = qdss_mhi_match_table,
	.probe = qdss_mhi_probe,
	.remove = qdss_mhi_remove,
	.dl_xfer_cb = qdss_mhi_read_cb,
	.ul_xfer_cb = qdss_mhi_write_cb,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	}
};

static int __init qdss_bridge_init(void)
{
	int ret;

	mhi_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(mhi_class))
		return -ENODEV;

	mhi_class->dev_groups = qdss_bridge_groups;

	ret = mhi_driver_register(&qdss_mhi_driver);
	if (ret)
		class_destroy(mhi_class);

	return ret;
}

static void __exit qdss_bridge_exit(void)
{
	mhi_driver_unregister(&qdss_mhi_driver);
}

module_init(qdss_bridge_init);
module_exit(qdss_bridge_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QDSS Bridge driver");
