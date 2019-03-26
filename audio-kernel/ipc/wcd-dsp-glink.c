/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
#include <linux/rpmsg.h>
#include "sound/wcd-dsp-glink.h"

#define WDSP_GLINK_DRIVER_NAME "wcd-dsp-glink"
#define WDSP_MAX_WRITE_SIZE (256 * 1024)
#define WDSP_MAX_READ_SIZE (4 * 1024)
#define WDSP_WRITE_PKT_SIZE (sizeof(struct wdsp_write_pkt))
#define WDSP_CMD_PKT_SIZE (sizeof(struct wdsp_cmd_pkt))

#define MINOR_NUMBER_COUNT 1
#define RESP_QUEUE_SIZE 3
#define TIMEOUT_MS 2000

enum wdsp_ch_state {
	WDSP_CH_DISCONNECTED,
	WDSP_CH_CONNECTED,
};

struct wdsp_glink_dev {
	struct class *cls;
	struct device *dev;
	struct cdev cdev;
	dev_t dev_num;
};

struct wdsp_rsp_que {
	/* Size of valid data in buffer */
	u32 buf_size;

	/* Response buffer */
	u8 buf[WDSP_MAX_READ_SIZE];
};

struct wdsp_ch {
	struct wdsp_glink_priv *wpriv;
	/* rpmsg handle */
	void *handle;
	/* Channel states like connect, disconnect */
	int ch_state;
	char ch_name[RPMSG_NAME_SIZE];
	spinlock_t ch_lock;
};

struct wdsp_tx_buf {
	struct work_struct tx_work;

	/* Glink channel information */
	struct wdsp_ch *ch;

	/* Tx buffer to send to glink */
	u8 buf[0];
};

struct wdsp_glink_priv {
	/* Respone buffer related */
	u8 rsp_cnt;
	struct wdsp_rsp_que rsp[RESP_QUEUE_SIZE];
	u8 write_idx;
	u8 read_idx;
	struct completion rsp_complete;
	spinlock_t rsp_lock;

	/* Glink channel related */
	int no_of_channels;
	struct wdsp_ch **ch;
	struct workqueue_struct *work_queue;
	/* Wait for all channels state before sending any command */
	wait_queue_head_t ch_state_wait;

	struct wdsp_glink_dev *wdev;
	struct device *dev;
};
static struct wdsp_glink_priv *wpriv;

static struct wdsp_ch *wdsp_get_ch(char *ch_name)
{
	int i;

	for (i = 0; i < wpriv->no_of_channels; i++) {
		if (!strcmp(ch_name, wpriv->ch[i]->ch_name))
			return wpriv->ch[i];
	}
	return NULL;
}

/*
 * wdsp_rpmsg_callback - Rpmsg callback for responses
 * rpdev:     Rpmsg device structure
 * data:      Pointer to the Rx data
 * len:       Size of the Rx data
 * priv:      Private pointer to the channel
 * addr:      Address variable
 * Returns 0 on success and an appropriate error value on failure
 */
static int wdsp_rpmsg_callback(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 addr__unused)
{
	struct wdsp_ch *ch = dev_get_drvdata(&rpdev->dev);
	struct wdsp_glink_priv *wpriv;
	unsigned long flags;
	u8 *rx_buf;
	u8 rsp_cnt = 0;

	if (!ch || !data) {
		pr_err("%s: Invalid ch or data\n", __func__);
		return -EINVAL;
	}

	wpriv = ch->wpriv;
	rx_buf = (u8 *)data;
	if (len > WDSP_MAX_READ_SIZE) {
		dev_info_ratelimited(wpriv->dev, "%s: Size %d is greater than allowed %d\n",
			__func__, len, WDSP_MAX_READ_SIZE);
		len = WDSP_MAX_READ_SIZE;
	}
	dev_dbg_ratelimited(wpriv->dev, "%s: copy into buffer %d\n", __func__,
			    wpriv->rsp_cnt);

	if (wpriv->rsp_cnt >= RESP_QUEUE_SIZE) {
		dev_info_ratelimited(wpriv->dev, "%s: Resp Queue is Full. Ignore new one.\n",
				      __func__);
		return -EINVAL;
	}
	spin_lock_irqsave(&wpriv->rsp_lock, flags);
	rsp_cnt = wpriv->rsp_cnt;
	memcpy(wpriv->rsp[wpriv->write_idx].buf, rx_buf, len);
	wpriv->rsp[wpriv->write_idx].buf_size = len;
	wpriv->write_idx = (wpriv->write_idx + 1) % RESP_QUEUE_SIZE;
	wpriv->rsp_cnt = ++rsp_cnt;
	spin_unlock_irqrestore(&wpriv->rsp_lock, flags);

	complete(&wpriv->rsp_complete);

	return 0;
}

/*
 * wdsp_rpmsg_probe - Rpmsg channel probe function
 * rpdev:     Rpmsg device structure
 * Returns 0 on success and an appropriate error value on failure
 */
static int wdsp_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct wdsp_ch *ch;

	ch = wdsp_get_ch(rpdev->id.name);
	if (!ch) {
		dev_err(&rpdev->dev, "%s, Invalid Channel [%s]\n",
			__func__, rpdev->id.name);
		return -EINVAL;
	}

	dev_dbg(&rpdev->dev, "%s: Channel[%s] state[Up]\n",
		 __func__, rpdev->id.name);

	spin_lock(&ch->ch_lock);
	ch->handle = rpdev;
	ch->ch_state = WDSP_CH_CONNECTED;
	spin_unlock(&ch->ch_lock);
	dev_set_drvdata(&rpdev->dev, ch);
	wake_up(&wpriv->ch_state_wait);

	return 0;
}

/*
 * wdsp_rpmsg_remove - Rpmsg channel remove function
 * rpdev:     Rpmsg device structure
 */
static void wdsp_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct wdsp_ch *ch = dev_get_drvdata(&rpdev->dev);

	if (!ch) {
		dev_err(&rpdev->dev, "%s: Invalid ch\n", __func__);
		return;
	}

	dev_dbg(&rpdev->dev, "%s: Channel[%s] state[Down]\n",
		 __func__, rpdev->id.name);
	spin_lock(&ch->ch_lock);
	ch->handle = NULL;
	ch->ch_state = WDSP_CH_DISCONNECTED;
	spin_unlock(&ch->ch_lock);
	dev_set_drvdata(&rpdev->dev, NULL);
}

static bool wdsp_is_ch_connected(struct wdsp_glink_priv *wpriv)
{
	int i;

	for (i = 0; i < wpriv->no_of_channels; i++) {
		spin_lock(&wpriv->ch[i]->ch_lock);
		if (wpriv->ch[i]->ch_state != WDSP_CH_CONNECTED) {
			spin_unlock(&wpriv->ch[i]->ch_lock);
			return false;
		}
		spin_unlock(&wpriv->ch[i]->ch_lock);
	}
	return true;
}

static int wdsp_wait_for_all_ch_connect(struct wdsp_glink_priv *wpriv)
{
	int ret;

	ret = wait_event_timeout(wpriv->ch_state_wait,
				wdsp_is_ch_connected(wpriv),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		dev_err_ratelimited(wpriv->dev, "%s: All channels are not connected\n",
				    __func__);
		ret = -ETIMEDOUT;
		goto err;
	}
	ret = 0;

err:
	return ret;
}

/*
 * wdsp_tx_buf_work - Work queue function to send tx buffer to glink
 * work:     Work structure
 */
static void wdsp_tx_buf_work(struct work_struct *work)
{
	struct wdsp_glink_priv *wpriv;
	struct wdsp_ch *ch;
	struct wdsp_tx_buf *tx_buf;
	struct wdsp_write_pkt *wpkt;
	struct wdsp_cmd_pkt *cpkt;
	int ret = 0;
	struct rpmsg_device *rpdev = NULL;

	tx_buf = container_of(work, struct wdsp_tx_buf,
			      tx_work);
	ch = tx_buf->ch;
	wpriv = ch->wpriv;
	wpkt = (struct wdsp_write_pkt *)tx_buf->buf;
	cpkt = (struct wdsp_cmd_pkt *)wpkt->payload;

	dev_dbg(wpriv->dev, "%s: ch name = %s, payload size = %d\n",
		__func__, cpkt->ch_name, cpkt->payload_size);

	spin_lock(&ch->ch_lock);
	rpdev = ch->handle;
	if (rpdev && ch->ch_state == WDSP_CH_CONNECTED) {
		spin_unlock(&ch->ch_lock);
		ret = rpmsg_send(rpdev->ept, cpkt->payload,
				 cpkt->payload_size);
		if (ret < 0)
			dev_err(wpriv->dev, "%s: rpmsg send failed, ret = %d\n",
				__func__, ret);
	} else {
		spin_unlock(&ch->ch_lock);
		if (rpdev)
			dev_err(wpriv->dev, "%s: channel %s is not in connected state\n",
				__func__, ch->ch_name);
		else
			dev_err(wpriv->dev, "%s: rpdev is NULL\n", __func__);
	}
	vfree(tx_buf);
}

/*
 * wdsp_glink_read - Read API to send the data to userspace
 * file:    Pointer to the file structure
 * buf:     Pointer to the userspace buffer
 * count:   Number bytes to read from the file
 * ppos:    Pointer to the position into the file
 * Returns 0 on success and an appropriate error value on failure
 */
static ssize_t wdsp_glink_read(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos)
{
	int ret = 0, ret1 = 0;
	struct wdsp_rsp_que *read_rsp = NULL;
	struct wdsp_glink_priv *wpriv;
	unsigned long flags;

	wpriv = (struct wdsp_glink_priv *)file->private_data;
	if (!wpriv) {
		pr_err("%s: Invalid private data\n", __func__);
		return -EINVAL;
	}

	if (count > WDSP_MAX_READ_SIZE) {
		dev_info_ratelimited(wpriv->dev, "%s: count = %zd is more than WDSP_MAX_READ_SIZE\n",
			__func__, count);
		count = WDSP_MAX_READ_SIZE;
	}
	/*
	 * Complete signal has given from gwdsp_rpmsg_callback()
	 * or from flush API. Also use interruptible wait_for_completion API
	 * to allow the system to go in suspend.
	 */
	ret = wait_for_completion_interruptible(&wpriv->rsp_complete);
	if (ret < 0)
		return ret;

	read_rsp = kzalloc(sizeof(struct wdsp_rsp_que), GFP_KERNEL);
	if (!read_rsp)
		return -ENOMEM;

	spin_lock_irqsave(&wpriv->rsp_lock, flags);
	if (wpriv->rsp_cnt) {
		wpriv->rsp_cnt--;
		dev_dbg(wpriv->dev, "%s: rsp_cnt=%d read from buffer %d\n",
			__func__, wpriv->rsp_cnt, wpriv->read_idx);

		memcpy(read_rsp, &wpriv->rsp[wpriv->read_idx],
			sizeof(struct wdsp_rsp_que));
		wpriv->read_idx = (wpriv->read_idx + 1) % RESP_QUEUE_SIZE;
		spin_unlock_irqrestore(&wpriv->rsp_lock, flags);

		if (count < read_rsp->buf_size) {
			ret1 = copy_to_user(buf, read_rsp->buf, count);
			/* Return the number of bytes copied */
			ret = count;
		} else {
			ret1 = copy_to_user(buf, read_rsp->buf,
					    read_rsp->buf_size);
			/* Return the number of bytes copied */
			ret = read_rsp->buf_size;
		}

		if (ret1) {
			dev_err_ratelimited(wpriv->dev, "%s: copy_to_user failed %d\n",
					    __func__, ret);
			ret = -EFAULT;
			goto done;
		}
	} else {
		/*
		 * This will execute only if flush API is called or
		 * something wrong with ref_cnt
		 */
		dev_dbg(wpriv->dev, "%s: resp count = %d\n", __func__,
			wpriv->rsp_cnt);
		spin_unlock_irqrestore(&wpriv->rsp_lock, flags);
		ret = -EINVAL;
	}

done:
	kfree(read_rsp);
	return ret;
}

/*
 * wdsp_glink_write - Write API to receive the data from userspace
 * file:    Pointer to the file structure
 * buf:     Pointer to the userspace buffer
 * count:   Number bytes to read from the file
 * ppos:    Pointer to the position into the file
 * Returns 0 on success and an appropriate error value on failure
 */
static ssize_t wdsp_glink_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret = 0, i, tx_buf_size;
	struct wdsp_write_pkt *wpkt;
	struct wdsp_cmd_pkt *cpkt;
	struct wdsp_tx_buf *tx_buf;
	struct wdsp_glink_priv *wpriv;
	size_t pkt_max_size;

	wpriv = (struct wdsp_glink_priv *)file->private_data;
	if (!wpriv) {
		pr_err("%s: Invalid private data\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if ((count < WDSP_WRITE_PKT_SIZE) ||
	    (count > WDSP_MAX_WRITE_SIZE)) {
		dev_err_ratelimited(wpriv->dev, "%s: Invalid count = %zd\n",
			__func__, count);
		ret = -EINVAL;
		goto done;
	}

	dev_dbg(wpriv->dev, "%s: count = %zd\n", __func__, count);

	tx_buf_size = count + sizeof(struct wdsp_tx_buf);
	tx_buf = vzalloc(tx_buf_size);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto done;
	}

	ret = copy_from_user(tx_buf->buf, buf, count);
	if (ret) {
		dev_err_ratelimited(wpriv->dev, "%s: copy_from_user failed %d\n",
			__func__, ret);
		ret = -EFAULT;
		goto free_buf;
	}

	wpkt = (struct wdsp_write_pkt *)tx_buf->buf;
	switch (wpkt->pkt_type) {
	case WDSP_REG_PKT:
		/* Keep this case to support backward compatibility */
		vfree(tx_buf);
		break;
	case WDSP_READY_PKT:
		ret = wdsp_wait_for_all_ch_connect(wpriv);
		if (ret < 0)
			dev_err_ratelimited(wpriv->dev, "%s: Channels not in connected state\n",
					    __func__);
		vfree(tx_buf);
		break;
	case WDSP_CMD_PKT:
		if (count <= (WDSP_WRITE_PKT_SIZE + WDSP_CMD_PKT_SIZE)) {
			dev_err_ratelimited(wpriv->dev, "%s: Invalid cmd pkt size = %zd\n",
				__func__, count);
			ret = -EINVAL;
			goto free_buf;
		}
		cpkt = (struct wdsp_cmd_pkt *)wpkt->payload;
		pkt_max_size =  sizeof(struct wdsp_write_pkt) +
					sizeof(struct wdsp_cmd_pkt) +
					cpkt->payload_size;
		if (count < pkt_max_size) {
			dev_err_ratelimited(wpriv->dev, "%s: Invalid cmd pkt count = %zd, pkt_size = %zd\n",
				__func__, count, pkt_max_size);
			ret = -EINVAL;
			goto free_buf;
		}
		for (i = 0; i < wpriv->no_of_channels; i++) {
			if (!strcmp(cpkt->ch_name, wpriv->ch[i]->ch_name)) {
				tx_buf->ch = wpriv->ch[i];
				break;
			}
		}
		if (!tx_buf->ch) {
			dev_err_ratelimited(wpriv->dev, "%s: Failed to get channel\n",
				__func__);
			ret = -EINVAL;
			goto free_buf;
		}
		dev_dbg(wpriv->dev, "%s: requested ch_name: %s, pkt_size: %zd\n",
			__func__, cpkt->ch_name, pkt_max_size);

		spin_lock(&tx_buf->ch->ch_lock);
		if (tx_buf->ch->ch_state != WDSP_CH_CONNECTED) {
			spin_unlock(&tx_buf->ch->ch_lock);
			ret = -ENETRESET;
			dev_err_ratelimited(wpriv->dev, "%s: Channels are not in connected state\n",
						__func__);
			goto free_buf;
		}
		spin_unlock(&tx_buf->ch->ch_lock);

		INIT_WORK(&tx_buf->tx_work, wdsp_tx_buf_work);
		queue_work(wpriv->work_queue, &tx_buf->tx_work);
		break;
	default:
		dev_err_ratelimited(wpriv->dev, "%s: Invalid packet type\n",
				    __func__);
		ret = -EINVAL;
		vfree(tx_buf);
		break;
	}
	goto done;

free_buf:
	vfree(tx_buf);

done:
	return ret;
}

/*
 * wdsp_glink_open - Open API to initialize private data
 * inode:   Pointer to the inode structure
 * file:    Pointer to the file structure
 * Returns 0 on success and an appropriate error value on failure
 */
static int wdsp_glink_open(struct inode *inode, struct file *file)
{

	pr_debug("%s: wpriv = %pK\n", __func__, wpriv);
	file->private_data = wpriv;

	return 0;
}

/*
 * wdsp_glink_flush - Flush API to unblock read.
 * file:    Pointer to the file structure
 * id:      Lock owner ID
 * Returns 0 on success and an appropriate error value on failure
 */
static int wdsp_glink_flush(struct file *file, fl_owner_t id)
{
	struct wdsp_glink_priv *wpriv;

	wpriv = (struct wdsp_glink_priv *)file->private_data;
	if (!wpriv) {
		pr_err("%s: Invalid private data\n", __func__);
		return -EINVAL;
	}

	complete(&wpriv->rsp_complete);

	return 0;
}

/*
 * wdsp_glink_release - Release API to clean up resources.
 * Whenever a file structure is shared across multiple threads,
 * release won't be invoked until all copies are closed
 * (file->f_count.counter should be 0). If we need to flush pending
 * data when any copy is closed, you should implement the flush method.
 *
 * inode:   Pointer to the inode structure
 * file:    Pointer to the file structure
 * Returns 0 on success and an appropriate error value on failure
 */
static int wdsp_glink_release(struct inode *inode, struct file *file)
{
	pr_debug("%s: file->private_data = %pK\n", __func__,
		 file->private_data);
	file->private_data = NULL;

	return 0;
}

static struct rpmsg_driver wdsp_rpmsg_driver = {
	.probe = wdsp_rpmsg_probe,
	.remove = wdsp_rpmsg_remove,
	.callback = wdsp_rpmsg_callback,
	/* Update this dynamically before register_rpmsg() */
	.id_table = NULL,
	.drv = {
		.name = "wdsp_rpmsg",
	},
};

static int wdsp_register_rpmsg(struct platform_device *pdev,
				struct wdsp_glink_dev *wdev)
{
	int ret = 0;
	int i, no_of_channels;
	struct rpmsg_device_id *wdsp_rpmsg_id_table, *id_table;
	const char *ch_name = NULL;

	wpriv = devm_kzalloc(&pdev->dev,
			     sizeof(struct wdsp_glink_priv), GFP_KERNEL);
	if (!wpriv)
		return -ENOMEM;

	no_of_channels = of_property_count_strings(pdev->dev.of_node,
						   "qcom,wdsp-channels");
	if (no_of_channels < 0) {
		dev_err(&pdev->dev, "%s: channel name parse error %d\n",
			__func__, no_of_channels);
		return -EINVAL;
	}

	wpriv->ch = devm_kzalloc(&pdev->dev,
			(sizeof(struct wdsp_glink_priv *) * no_of_channels),
			GFP_KERNEL);
	if (!wpriv->ch)
		return -ENOMEM;

	for (i = 0; i < no_of_channels; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
						   "qcom,wdsp-channels", i,
						   &ch_name);
		if (ret) {
			dev_err(&pdev->dev, "%s: channel name parse error %d\n",
				__func__, ret);
			return -EINVAL;
		}
		wpriv->ch[i] = devm_kzalloc(&pdev->dev,
					    sizeof(struct wdsp_glink_priv),
					    GFP_KERNEL);
		if (!wpriv->ch[i])
			return -ENOMEM;

		strlcpy(wpriv->ch[i]->ch_name, ch_name, RPMSG_NAME_SIZE);
		wpriv->ch[i]->wpriv = wpriv;
		spin_lock_init(&wpriv->ch[i]->ch_lock);
	}
	init_waitqueue_head(&wpriv->ch_state_wait);
	init_completion(&wpriv->rsp_complete);
	spin_lock_init(&wpriv->rsp_lock);

	wpriv->wdev = wdev;
	wpriv->dev = wdev->dev;
	wpriv->work_queue = create_singlethread_workqueue("wdsp_glink_wq");
	if (!wpriv->work_queue) {
		dev_err(&pdev->dev, "%s: Error creating wdsp_glink_wq\n",
			__func__);
		return -EINVAL;
	}

	wdsp_rpmsg_id_table = devm_kzalloc(&pdev->dev,
					   (sizeof(struct rpmsg_device_id) *
							(no_of_channels + 1)),
					   GFP_KERNEL);
	if (!wdsp_rpmsg_id_table) {
		ret = -ENOMEM;
		goto err;
	}

	wpriv->no_of_channels = no_of_channels;
	id_table = wdsp_rpmsg_id_table;
	for (i = 0; i < no_of_channels; i++) {
		strlcpy(id_table->name, wpriv->ch[i]->ch_name,
			RPMSG_NAME_SIZE);
		id_table++;
	}
	wdsp_rpmsg_driver.id_table = wdsp_rpmsg_id_table;
	ret = register_rpmsg_driver(&wdsp_rpmsg_driver);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Rpmsg driver register failed, err = %d\n",
			__func__, ret);
		goto err;
	}

	return 0;

err:
	destroy_workqueue(wpriv->work_queue);
	return ret;
}

static const struct file_operations wdsp_glink_fops = {
	.owner =                THIS_MODULE,
	.open =                 wdsp_glink_open,
	.read =                 wdsp_glink_read,
	.write =                wdsp_glink_write,
	.flush =                wdsp_glink_flush,
	.release =              wdsp_glink_release,
};

static int wdsp_glink_probe(struct platform_device *pdev)
{
	int ret;
	struct wdsp_glink_dev *wdev;

	wdev = devm_kzalloc(&pdev->dev, sizeof(*wdev), GFP_KERNEL);
	if (!wdev) {
		ret = -ENOMEM;
		goto done;
	}

	ret = alloc_chrdev_region(&wdev->dev_num, 0, MINOR_NUMBER_COUNT,
				  WDSP_GLINK_DRIVER_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Failed to alloc char dev, err = %d\n",
			__func__, ret);
		goto err_chrdev;
	}

	wdev->cls = class_create(THIS_MODULE, WDSP_GLINK_DRIVER_NAME);
	if (IS_ERR(wdev->cls)) {
		ret = PTR_ERR(wdev->cls);
		dev_err(&pdev->dev, "%s: Failed to create class, err = %d\n",
			__func__, ret);
		goto err_class;
	}

	wdev->dev = device_create(wdev->cls, NULL, wdev->dev_num,
				  NULL, WDSP_GLINK_DRIVER_NAME);
	if (IS_ERR(wdev->dev)) {
		ret = PTR_ERR(wdev->dev);
		dev_err(&pdev->dev, "%s: Failed to create device, err = %d\n",
			__func__, ret);
		goto err_dev_create;
	}

	cdev_init(&wdev->cdev, &wdsp_glink_fops);
	ret = cdev_add(&wdev->cdev, wdev->dev_num, MINOR_NUMBER_COUNT);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Failed to register char dev, err = %d\n",
			__func__, ret);
		goto err_cdev_add;
	}

	ret = wdsp_register_rpmsg(pdev, wdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Failed to register with rpmsg, err = %d\n",
			__func__, ret);
		goto err_cdev_add;
	}
	platform_set_drvdata(pdev, wpriv);

	goto done;

err_cdev_add:
	device_destroy(wdev->cls, wdev->dev_num);

err_dev_create:
	class_destroy(wdev->cls);

err_class:
	unregister_chrdev_region(0, MINOR_NUMBER_COUNT);

err_chrdev:
done:
	return ret;
}

static int wdsp_glink_remove(struct platform_device *pdev)
{
	struct wdsp_glink_priv *wpriv = platform_get_drvdata(pdev);

	unregister_rpmsg_driver(&wdsp_rpmsg_driver);

	if (wpriv) {
		flush_workqueue(wpriv->work_queue);
		destroy_workqueue(wpriv->work_queue);
		if (wpriv->wdev) {
			cdev_del(&wpriv->wdev->cdev);
			device_destroy(wpriv->wdev->cls, wpriv->wdev->dev_num);
			class_destroy(wpriv->wdev->cls);
			unregister_chrdev_region(0, MINOR_NUMBER_COUNT);
		}
	}

	return 0;
}

static const struct of_device_id wdsp_glink_of_match[] = {
	{.compatible = "qcom,wcd-dsp-glink"},
	{ }
};
MODULE_DEVICE_TABLE(of, wdsp_glink_of_match);

static struct platform_driver wdsp_glink_driver = {
	.probe          = wdsp_glink_probe,
	.remove         = wdsp_glink_remove,
	.driver         = {
		.name   = WDSP_GLINK_DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = wdsp_glink_of_match,
	},
};

static int __init wdsp_glink_init(void)
{
	return platform_driver_register(&wdsp_glink_driver);
}

static void __exit wdsp_glink_exit(void)
{
	platform_driver_unregister(&wdsp_glink_driver);
}

module_init(wdsp_glink_init);
module_exit(wdsp_glink_exit);
MODULE_DESCRIPTION("SoC WCD_DSP GLINK Driver");
MODULE_LICENSE("GPL v2");
