/* Copyright (c) 2015,2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/ipc_logging.h>
#include <linux/dma-mapping.h>
#include <linux/msm_ipa.h>
#include <linux/ipa.h>
#include <uapi/linux/mhi.h>
#include "mhi.h"

#define MHI_SOFTWARE_CLIENT_START	0
#define MHI_SOFTWARE_CLIENT_LIMIT	(MHI_MAX_SOFTWARE_CHANNELS/2)
#define MHI_UCI_IPC_LOG_PAGES		(100)

/* Max number of MHI write request structures (used in async writes) */
#define MAX_UCI_WR_REQ			10
#define MAX_NR_TRBS_PER_CHAN		9
#define MHI_QTI_IFACE_ID		4
#define DEVICE_NAME			"mhi"
#define MAX_DEVICE_NAME_SIZE		80

#define MHI_UCI_ASYNC_READ_TIMEOUT	msecs_to_jiffies(100)

enum uci_dbg_level {
	UCI_DBG_VERBOSE = 0x0,
	UCI_DBG_INFO = 0x1,
	UCI_DBG_DBG = 0x2,
	UCI_DBG_WARNING = 0x3,
	UCI_DBG_ERROR = 0x4,
	UCI_DBG_CRITICAL = 0x5,
	UCI_DBG_reserved = 0x80000000
};

static enum uci_dbg_level mhi_uci_msg_lvl = UCI_DBG_CRITICAL;
static enum uci_dbg_level mhi_uci_ipc_log_lvl = UCI_DBG_INFO;
static void *mhi_uci_ipc_log;


enum mhi_chan_dir {
	MHI_DIR_INVALID = 0x0,
	MHI_DIR_OUT = 0x1,
	MHI_DIR_IN = 0x2,
	MHI_DIR__reserved = 0x80000000
};

struct chan_attr {
	/* SW maintained channel id */
	enum mhi_client_channel chan_id;
	/* maximum buffer size for this channel */
	size_t max_packet_size;
	/* number of buffers supported in this channel */
	u32 nr_trbs;
	/* direction of the channel, see enum mhi_chan_dir */
	enum mhi_chan_dir dir;
	/* need to register mhi channel state change callback */
	bool register_cb;
	/* Name of char device */
	char *device_name;
};

/* UCI channel attributes table */
static const struct chan_attr uci_chan_attr_table[] = {
	{
		MHI_CLIENT_LOOPBACK_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_LOOPBACK_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_SAHARA_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_SAHARA_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_EFS_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_EFS_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_MBIM_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_MBIM_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_QMI_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_QMI_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_IP_CTRL_0_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_IP_CTRL_0_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_IP_CTRL_1_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_IP_CTRL_1_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_DUN_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		false,
		NULL
	},
	{
		MHI_CLIENT_DUN_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		false,
		NULL
	},
	{
		MHI_CLIENT_ADB_OUT,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_OUT,
		true,
		NULL
	},
	{
		MHI_CLIENT_ADB_IN,
		TRB_MAX_DATA_SIZE,
		MAX_NR_TRBS_PER_CHAN,
		MHI_DIR_IN,
		true,
		"android_adb"
	},
};

struct uci_ctrl {
	wait_queue_head_t	ctrl_wq;
	struct mhi_uci_ctxt_t	*uci_ctxt;
	atomic_t		ctrl_data_update;
};

struct uci_client {
	u32 client_index;
	/* write channel - always odd*/
	u32 out_chan;
	/* read channel - always even */
	u32 in_chan;
	struct mhi_dev_client *out_handle;
	struct mhi_dev_client *in_handle;
	const struct chan_attr *in_chan_attr;
	const struct chan_attr *out_chan_attr;
	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	atomic_t read_data_ready;
	struct device *dev;
	atomic_t ref_count;
	int mhi_status;
	void *pkt_loc;
	size_t pkt_size;
	struct mhi_dev_iov *in_buf_list;
	atomic_t write_data_ready;
	atomic_t mhi_chans_open;
	struct mhi_uci_ctxt_t *uci_ctxt;
	struct mutex in_chan_lock;
	struct mutex out_chan_lock;
	spinlock_t wr_req_lock;
	unsigned int f_flags;
	struct mhi_req *wreqs;
	struct list_head wr_req_list;
	struct completion read_done;
	int (*send)(struct uci_client*, void*, u32);
	int (*read)(struct uci_client*, struct mhi_req*, int*);
};

struct mhi_uci_ctxt_t {
	struct uci_client client_handles[MHI_SOFTWARE_CLIENT_LIMIT];
	struct uci_ctrl ctrl_handle;
	void (*event_notifier)(struct mhi_dev_client_cb_reason *cb);
	dev_t start_ctrl_nr;
	struct cdev cdev[MHI_MAX_SOFTWARE_CHANNELS];
	dev_t ctrl_nr;
	struct cdev *cdev_ctrl;
	struct device *dev;
	struct class *mhi_uci_class;
	atomic_t mhi_disabled;
	atomic_t mhi_enable_notif_wq_active;
};

#define CHAN_TO_CLIENT(_CHAN_NR) (_CHAN_NR / 2)
#define CLIENT_TO_CHAN(_CLIENT_NR) (_CLIENT_NR * 2)

#define uci_log(_msg_lvl, _msg, ...) do { \
	if (_msg_lvl >= mhi_uci_msg_lvl) { \
		pr_err("[%s] "_msg, __func__, ##__VA_ARGS__); \
	} \
	if (mhi_uci_ipc_log && (_msg_lvl >= mhi_uci_ipc_log_lvl)) { \
		ipc_log_string(mhi_uci_ipc_log,                     \
			"[%s] " _msg, __func__, ##__VA_ARGS__);     \
	} \
} while (0)


module_param(mhi_uci_msg_lvl, uint, 0644);
MODULE_PARM_DESC(mhi_uci_msg_lvl, "uci dbg lvl");

module_param(mhi_uci_ipc_log_lvl, uint, 0644);
MODULE_PARM_DESC(mhi_uci_ipc_log_lvl, "ipc dbg lvl");

static ssize_t mhi_uci_client_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp);
static ssize_t mhi_uci_ctrl_client_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp);
static ssize_t mhi_uci_client_write(struct file *file,
		const char __user *buf, size_t count, loff_t *offp);
static int mhi_uci_client_open(struct inode *mhi_inode, struct file*);
static int mhi_uci_ctrl_open(struct inode *mhi_inode, struct file*);
static int mhi_uci_client_release(struct inode *mhi_inode,
		struct file *file_handle);
static unsigned int mhi_uci_client_poll(struct file *file, poll_table *wait);
static unsigned int mhi_uci_ctrl_poll(struct file *file, poll_table *wait);
static struct mhi_uci_ctxt_t uci_ctxt;

static int mhi_init_read_chan(struct uci_client *client_handle,
		enum mhi_client_channel chan)
{
	int rc = 0;
	u32 i, j;
	const struct chan_attr *in_chan_attr;
	size_t buf_size;
	void *data_loc;

	if (client_handle == NULL) {
		uci_log(UCI_DBG_ERROR, "Bad Input data, quitting\n");
		return -EINVAL;
	}
	if (chan >= MHI_MAX_SOFTWARE_CHANNELS) {
		uci_log(UCI_DBG_ERROR, "Incorrect channel number %d\n", chan);
		return -EINVAL;
	}

	in_chan_attr = client_handle->in_chan_attr;
	if (!in_chan_attr) {
		uci_log(UCI_DBG_ERROR, "Null channel attributes for chan %d\n",
				client_handle->in_chan);
		return -EINVAL;
	}

	/* Init the completion event for read */
	init_completion(&client_handle->read_done);

	buf_size = in_chan_attr->max_packet_size;
	for (i = 0; i < (in_chan_attr->nr_trbs); i++) {
		data_loc = kmalloc(buf_size, GFP_KERNEL);
		if (!data_loc) {
			rc = -ENOMEM;
			goto free_memory;
		}
		client_handle->in_buf_list[i].addr = data_loc;
		client_handle->in_buf_list[i].buf_size = buf_size;
	}

	return rc;

free_memory:
	for (j = 0; j < i; j++)
		kfree(client_handle->in_buf_list[j].addr);

	return rc;
}

static void mhi_uci_write_completion_cb(void *req)
{
	struct mhi_req *ureq = req;
	struct uci_client *uci_handle;
	unsigned long flags;

	uci_handle = (struct uci_client *)ureq->context;
	kfree(ureq->buf);
	ureq->buf = NULL;

	spin_lock_irqsave(&uci_handle->wr_req_lock, flags);
	list_add_tail(&ureq->list, &uci_handle->wr_req_list);
	spin_unlock_irqrestore(&uci_handle->wr_req_lock, flags);
}

static void mhi_uci_read_completion_cb(void *req)
{
	struct mhi_req *ureq = req;
	struct uci_client *uci_handle;

	uci_handle = (struct uci_client *)ureq->context;
	complete(&uci_handle->read_done);
}

static int mhi_uci_send_sync(struct uci_client *uci_handle,
			void *data_loc, u32 size)
{
	struct mhi_req ureq;
	int ret_val;

	ureq.client = uci_handle->out_handle;
	ureq.buf = data_loc;
	ureq.len = size;
	ureq.chan = uci_handle->out_chan;
	ureq.mode = IPA_DMA_SYNC;

	ret_val = mhi_dev_write_channel(&ureq);

	kfree(data_loc);
	return ret_val;
}

static int mhi_uci_send_async(struct uci_client *uci_handle,
			void *data_loc, u32 size)
{
	int bytes_to_write;
	struct mhi_req *ureq;

	uci_log(UCI_DBG_VERBOSE,
		"Got async write for ch %d of size %d\n",
		uci_handle->out_chan, size);

	spin_lock_irq(&uci_handle->wr_req_lock);
	if (list_empty(&uci_handle->wr_req_list)) {
		uci_log(UCI_DBG_ERROR, "Write request pool empty\n");
		spin_unlock_irq(&uci_handle->wr_req_lock);
		return -ENOMEM;
	}
	ureq = container_of(uci_handle->wr_req_list.next,
						struct mhi_req, list);
	list_del_init(&ureq->list);
	spin_unlock_irq(&uci_handle->wr_req_lock);

	ureq->client = uci_handle->out_handle;
	ureq->context = uci_handle;
	ureq->buf = data_loc;
	ureq->len = size;
	ureq->chan = uci_handle->out_chan;
	ureq->mode = IPA_DMA_ASYNC;
	ureq->client_cb = mhi_uci_write_completion_cb;
	ureq->snd_cmpl = 1;

	bytes_to_write = mhi_dev_write_channel(ureq);
	if (bytes_to_write != size)
		goto error_async_transfer;

	return bytes_to_write;

error_async_transfer:
	kfree(data_loc);
	ureq->buf = NULL;
	spin_lock_irq(&uci_handle->wr_req_lock);
	list_add_tail(&ureq->list, &uci_handle->wr_req_list);
	spin_unlock_irq(&uci_handle->wr_req_lock);

	return bytes_to_write;
}

static int mhi_uci_send_packet(struct mhi_dev_client **client_handle,
		const char __user *buf, u32 size)
{
	void *data_loc;
	unsigned long memcpy_result;
	struct uci_client *uci_handle;

	if (!client_handle || !buf || !size)
		return -EINVAL;

	if (size > TRB_MAX_DATA_SIZE) {
		uci_log(UCI_DBG_ERROR,
			"Too big write size: %d, max supported size is %d\n",
			size, TRB_MAX_DATA_SIZE);
		return -EFBIG;
	}

	uci_handle = container_of(client_handle, struct uci_client,
					out_handle);
	data_loc = kmalloc(size, GFP_KERNEL);
	if (!data_loc) {
		uci_log(UCI_DBG_ERROR,
		"Failed to allocate kernel buf for user requested size 0x%x\n",
			size);
		return -ENOMEM;
	}
	memcpy_result = copy_from_user(data_loc, buf, size);
	if (memcpy_result)
		goto error_memcpy;

	return uci_handle->send(uci_handle, data_loc, size);

error_memcpy:
	kfree(data_loc);
	return -EFAULT;
}

static unsigned int mhi_uci_ctrl_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct uci_ctrl *uci_ctrl_handle;

	uci_ctrl_handle = file->private_data;

	if (!uci_ctrl_handle)
		return -ENODEV;

	poll_wait(file, &uci_ctrl_handle->ctrl_wq, wait);
	if (!atomic_read(&uci_ctxt.mhi_disabled) &&
		atomic_read(&uci_ctrl_handle->ctrl_data_update)) {
		uci_log(UCI_DBG_VERBOSE, "Client can read ctrl_state");
		mask |= POLLIN | POLLRDNORM;
	}

	uci_log(UCI_DBG_VERBOSE,
		"Client attempted to poll ctrl returning mask 0x%x\n",
		mask);

	return mask;
}

static unsigned int mhi_uci_client_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct uci_client *uci_handle;

	uci_handle = file->private_data;

	if (!uci_handle)
		return -ENODEV;

	poll_wait(file, &uci_handle->read_wq, wait);
	poll_wait(file, &uci_handle->write_wq, wait);
	if (!atomic_read(&uci_ctxt.mhi_disabled) &&
		!mhi_dev_channel_isempty(uci_handle->in_handle)) {
		uci_log(UCI_DBG_VERBOSE,
		"Client can read chan %d\n", uci_handle->in_chan);
		mask |= POLLIN | POLLRDNORM;
	}
	if (!atomic_read(&uci_ctxt.mhi_disabled) &&
		!mhi_dev_channel_isempty(uci_handle->out_handle)) {
		uci_log(UCI_DBG_VERBOSE,
		"Client can write chan %d\n", uci_handle->out_chan);
		mask |= POLLOUT | POLLWRNORM;
	}

	uci_log(UCI_DBG_VERBOSE,
		"Client attempted to poll chan %d, returning mask 0x%x\n",
		uci_handle->in_chan, mask);
	return mask;
}

static int mhi_uci_alloc_write_reqs(struct uci_client *client)
{
	int i;

	client->wreqs = kcalloc(MAX_UCI_WR_REQ,
				sizeof(struct mhi_req),
				GFP_KERNEL);
	if (!client->wreqs) {
		uci_log(UCI_DBG_ERROR, "Write reqs alloc failed\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&client->wr_req_list);
	for (i = 0; i < MAX_UCI_WR_REQ; ++i)
		list_add_tail(&client->wreqs[i].list, &client->wr_req_list);

	uci_log(UCI_DBG_INFO,
		"UCI write reqs allocation successful\n");
	return 0;
}

static int mhi_uci_read_async(struct uci_client *uci_handle,
			struct mhi_req *ureq, int *bytes_avail)
{
	int ret_val = 0;
	unsigned long compl_ret;

	uci_log(UCI_DBG_ERROR,
		"Async read for ch %d\n", uci_handle->in_chan);

	ureq->mode = IPA_DMA_ASYNC;
	ureq->client_cb = mhi_uci_read_completion_cb;
	ureq->snd_cmpl = 1;
	ureq->context = uci_handle;

	reinit_completion(&uci_handle->read_done);

	*bytes_avail = mhi_dev_read_channel(ureq);
	uci_log(UCI_DBG_VERBOSE, "buf_size = 0x%x bytes_read = 0x%x\n",
		ureq->len, *bytes_avail);
	if (*bytes_avail < 0) {
		uci_log(UCI_DBG_ERROR, "Failed to read channel ret %d\n",
			*bytes_avail);
		return -EIO;
	}

	if (*bytes_avail > 0) {
		uci_log(UCI_DBG_VERBOSE,
			"Waiting for async read completion!\n");
		compl_ret =
			wait_for_completion_interruptible_timeout(
			&uci_handle->read_done,
			MHI_UCI_ASYNC_READ_TIMEOUT);

		if (compl_ret == -ERESTARTSYS) {
			uci_log(UCI_DBG_ERROR, "Exit signal caught\n");
			return compl_ret;
		} else if (compl_ret == 0) {
			uci_log(UCI_DBG_ERROR, "Read timed out for ch %d\n",
				uci_handle->in_chan);
			return -EIO;
		}
		uci_log(UCI_DBG_VERBOSE,
			"wk up Read completed on ch %d\n", ureq->chan);

		uci_handle->pkt_loc = (void *)ureq->buf;
		uci_handle->pkt_size = ureq->actual_len;

		uci_log(UCI_DBG_VERBOSE,
			"Got pkt of sz 0x%x at adr %pK, ch %d\n",
			uci_handle->pkt_size,
			ureq->buf, ureq->chan);
	} else {
		uci_handle->pkt_loc = NULL;
		uci_handle->pkt_size = 0;
	}

	return ret_val;
}

static int mhi_uci_read_sync(struct uci_client *uci_handle,
			struct mhi_req *ureq, int *bytes_avail)
{
	int ret_val = 0;

	ureq->mode = IPA_DMA_SYNC;
	*bytes_avail = mhi_dev_read_channel(ureq);

	uci_log(UCI_DBG_VERBOSE, "buf_size = 0x%x bytes_read = 0x%x\n",
		ureq->len, *bytes_avail);

	if (*bytes_avail < 0) {
		uci_log(UCI_DBG_ERROR, "Failed to read channel ret %d\n",
			*bytes_avail);
		return -EIO;
	}

	if (*bytes_avail > 0) {
		uci_handle->pkt_loc = (void *)ureq->buf;
		uci_handle->pkt_size = ureq->actual_len;

		uci_log(UCI_DBG_VERBOSE,
			"Got pkt of sz 0x%x at adr %pK, ch %d\n",
			uci_handle->pkt_size,
			ureq->buf, ureq->chan);
	} else {
		uci_handle->pkt_loc = NULL;
		uci_handle->pkt_size = 0;
	}

	return ret_val;
}

static int open_client_mhi_channels(struct uci_client *uci_client)
{
	int rc = 0;

	uci_log(UCI_DBG_DBG,
			"Starting channels %d %d.\n",
			uci_client->out_chan,
			uci_client->in_chan);
	mutex_lock(&uci_client->out_chan_lock);
	mutex_lock(&uci_client->in_chan_lock);

	/* Allocate write requests for async operations */
	if (!(uci_client->f_flags & O_SYNC)) {
		rc = mhi_uci_alloc_write_reqs(uci_client);
		if (rc)
			goto handle_not_rdy_err;
		uci_client->send = mhi_uci_send_async;
		uci_client->read = mhi_uci_read_async;
	} else {
		uci_client->send = mhi_uci_send_sync;
		uci_client->read = mhi_uci_read_sync;
	}

	uci_log(UCI_DBG_DBG,
			"Initializing inbound chan %d.\n",
			uci_client->in_chan);
	rc = mhi_init_read_chan(uci_client, uci_client->in_chan);
	if (rc < 0)
		uci_log(UCI_DBG_ERROR,
			"Failed to init inbound 0x%x, ret 0x%x\n",
			uci_client->in_chan, rc);

	rc = mhi_dev_open_channel(uci_client->out_chan,
			&uci_client->out_handle,
			uci_ctxt.event_notifier);
	if (rc < 0)
		goto handle_not_rdy_err;

	rc = mhi_dev_open_channel(uci_client->in_chan,
			&uci_client->in_handle,
			uci_ctxt.event_notifier);
	if (rc < 0) {
		uci_log(UCI_DBG_ERROR,
			"Failed to open chan %d, ret 0x%x\n",
			uci_client->out_chan, rc);
		goto handle_in_err;
	}
	atomic_set(&uci_client->mhi_chans_open, 1);
	mutex_unlock(&uci_client->in_chan_lock);
	mutex_unlock(&uci_client->out_chan_lock);

	return 0;

handle_in_err:
	mhi_dev_close_channel(uci_client->out_handle);
handle_not_rdy_err:
	mutex_unlock(&uci_client->in_chan_lock);
	mutex_unlock(&uci_client->out_chan_lock);
	return rc;
}

static int mhi_uci_ctrl_open(struct inode *inode,
			struct file *file_handle)
{
	struct uci_ctrl *uci_ctrl_handle;

	uci_log(UCI_DBG_DBG, "Client opened ctrl file device node\n");

	uci_ctrl_handle = &uci_ctxt.ctrl_handle;
	if (!uci_ctrl_handle)
		return -EINVAL;

	file_handle->private_data = uci_ctrl_handle;

	return 0;
}

static int mhi_uci_client_open(struct inode *mhi_inode,
				struct file *file_handle)
{
	struct uci_client *uci_handle;
	int rc = 0;

	rc = iminor(mhi_inode);
	if (rc < MHI_SOFTWARE_CLIENT_LIMIT) {
		uci_handle =
			&uci_ctxt.client_handles[iminor(mhi_inode)];
	} else {
		uci_log(UCI_DBG_DBG,
		"Cannot open struct device node 0x%x\n", iminor(mhi_inode));
		return -EINVAL;
	}

	uci_log(UCI_DBG_DBG,
		"Client opened struct device node 0x%x, ref count 0x%x\n",
		iminor(mhi_inode), atomic_read(&uci_handle->ref_count));
	if (atomic_add_return(1, &uci_handle->ref_count) == 1) {
		if (!uci_handle) {
			atomic_dec(&uci_handle->ref_count);
			return -ENOMEM;
		}
		uci_handle->uci_ctxt = &uci_ctxt;
		uci_handle->f_flags = file_handle->f_flags;
		if (!atomic_read(&uci_handle->mhi_chans_open)) {
			uci_log(UCI_DBG_INFO,
				"Opening channels client %d\n",
				iminor(mhi_inode));
			rc = open_client_mhi_channels(uci_handle);
			if (rc) {
				uci_log(UCI_DBG_INFO,
					"Failed to open channels ret %d\n", rc);
				return rc;
			}
		}
	}
	file_handle->private_data = uci_handle;

	return 0;

}

static int mhi_uci_client_release(struct inode *mhi_inode,
		struct file *file_handle)
{
	struct uci_client *uci_handle = file_handle->private_data;
	int rc = 0;

	if (!uci_handle)
		return -EINVAL;

	if (atomic_sub_return(1, &uci_handle->ref_count) == 0) {
		uci_log(UCI_DBG_DBG,
				"Last client left, closing channel 0x%x\n",
				iminor(mhi_inode));
		if (atomic_read(&uci_handle->mhi_chans_open)) {
			atomic_set(&uci_handle->mhi_chans_open, 0);

			if (!(uci_handle->f_flags & O_SYNC))
				kfree(uci_handle->wreqs);
			mutex_lock(&uci_handle->out_chan_lock);
			rc = mhi_dev_close_channel(uci_handle->out_handle);
			wake_up(&uci_handle->write_wq);
			mutex_unlock(&uci_handle->out_chan_lock);

			mutex_lock(&uci_handle->in_chan_lock);
			rc = mhi_dev_close_channel(uci_handle->in_handle);
			wake_up(&uci_handle->read_wq);
			mutex_unlock(&uci_handle->in_chan_lock);

		}
		atomic_set(&uci_handle->read_data_ready, 0);
		atomic_set(&uci_handle->write_data_ready, 0);
		file_handle->private_data = NULL;
	} else {
		uci_log(UCI_DBG_DBG,
			"Client close chan %d, ref count 0x%x\n",
			iminor(mhi_inode),
			atomic_read(&uci_handle->ref_count));
	}
	return rc;
}

static void  mhi_parse_state(char *buf, int *nbytes, uint32_t info)
{
	switch (info) {
	case MHI_STATE_CONNECTED:
		*nbytes = scnprintf(buf, MHI_CTRL_STATE,
			"CONNECTED");
		break;
	case MHI_STATE_DISCONNECTED:
		*nbytes = scnprintf(buf, MHI_CTRL_STATE,
			"DISCONNECTED");
		break;
	case MHI_STATE_CONFIGURED:
	default:
		*nbytes = scnprintf(buf, MHI_CTRL_STATE,
			"CONFIGURED");
		break;
	}
}

static int mhi_state_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	int rc, nbytes = 0;
	uint32_t info = 0;
	char buf[MHI_CTRL_STATE];

	rc = mhi_ctrl_state_info(MHI_DEV_UEVENT_CTRL, &info);
	if (rc) {
		pr_err("Failed to obtain MHI_STATE\n");
		return -EINVAL;
	}

	mhi_parse_state(buf, &nbytes, info);
	add_uevent_var(env, "MHI_STATE=%s", buf);

	rc = mhi_ctrl_state_info(MHI_CLIENT_QMI_OUT, &info);
	if (rc) {
		pr_err("Failed to obtain channel 14 state\n");
		return -EINVAL;
	}
	nbytes = 0;
	mhi_parse_state(buf, &nbytes, info);
	add_uevent_var(env, "MHI_CHANNEL_STATE_14=%s", buf);

	rc = mhi_ctrl_state_info(MHI_CLIENT_MBIM_OUT, &info);
	if (rc) {
		pr_err("Failed to obtain channel 12 state\n");
		return -EINVAL;
	}
	nbytes = 0;
	mhi_parse_state(buf, &nbytes, info);
	add_uevent_var(env, "MHI_CHANNEL_STATE_12=%s", buf);

	return 0;
}

static ssize_t mhi_uci_ctrl_client_read(struct file *file,
		char __user *user_buf,
		size_t count, loff_t *offp)
{
	uint32_t rc = 0, info;
	int nbytes, size;
	char buf[MHI_CTRL_STATE];
	struct uci_ctrl *uci_ctrl_handle = NULL;

	if (!file || !user_buf || !count ||
		(count < MHI_CTRL_STATE) || !file->private_data)
		return -EINVAL;

	uci_ctrl_handle = file->private_data;
	rc = mhi_ctrl_state_info(MHI_CLIENT_QMI_OUT, &info);
	if (rc)
		return -EINVAL;

	switch (info) {
	case MHI_STATE_CONFIGURED:
		nbytes = scnprintf(buf, sizeof(buf),
			"MHI_STATE=CONFIGURED");
		break;
	case MHI_STATE_CONNECTED:
		nbytes = scnprintf(buf, sizeof(buf),
			"MHI_STATE=CONNECTED");
		break;
	case MHI_STATE_DISCONNECTED:
		nbytes = scnprintf(buf, sizeof(buf),
			"MHI_STATE=DISCONNECTED");
		break;
	default:
		pr_err("invalid info:%d\n", info);
		return -EINVAL;
	}


	size = simple_read_from_buffer(user_buf, count, offp, buf, nbytes);

	atomic_set(&uci_ctrl_handle->ctrl_data_update, 0);

	if (size == 0)
		*offp = 0;

	return size;
}

static ssize_t mhi_uci_client_read(struct file *file, char __user *ubuf,
		size_t uspace_buf_size, loff_t *bytes_pending)
{
	struct uci_client *uci_handle = NULL;
	struct mhi_dev_client *client_handle = NULL;
	int bytes_avail = 0;
	int ret_val = 0;
	struct mutex *mutex;
	ssize_t bytes_copied = 0;
	u32 addr_offset = 0;
	struct mhi_req ureq;

	if (!file || !ubuf || !uspace_buf_size ||
			!file->private_data)
		return -EINVAL;

	uci_handle = file->private_data;
	client_handle = uci_handle->in_handle;
	mutex = &uci_handle->in_chan_lock;
	ureq.chan = uci_handle->in_chan;

	mutex_lock(mutex);
	ureq.client = client_handle;
	ureq.buf = uci_handle->in_buf_list[0].addr;
	ureq.len = uci_handle->in_buf_list[0].buf_size;


	uci_log(UCI_DBG_VERBOSE, "Client attempted read on chan %d\n",
			ureq.chan);
	do {
		if (!uci_handle->pkt_loc &&
			!atomic_read(&uci_ctxt.mhi_disabled)) {
			ret_val = uci_handle->read(uci_handle, &ureq,
							&bytes_avail);
			if (ret_val)
				goto error;
			if (bytes_avail > 0)
				*bytes_pending = (loff_t)uci_handle->pkt_size;
		}
		if (bytes_avail == 0) {

			/* If nothing was copied yet, wait for data */
			uci_log(UCI_DBG_VERBOSE,
				"No data read_data_ready %d, chan %d\n",
				atomic_read(&uci_handle->read_data_ready),
				ureq.chan);
			if (uci_handle->f_flags & (O_NONBLOCK | O_NDELAY)) {
				ret_val = -EAGAIN;
				goto error;
			}
			ret_val = wait_event_interruptible(uci_handle->read_wq,
				(!mhi_dev_channel_isempty(client_handle)));

			if (ret_val == -ERESTARTSYS) {
				uci_log(UCI_DBG_ERROR, "Exit signal caught\n");
				goto error;
			}
			uci_log(UCI_DBG_VERBOSE,
				"wk up Got data on ch %d read_data_ready %d\n",
				ureq.chan,
				atomic_read(&uci_handle->read_data_ready));

			/* A valid packet was returned from MHI */
		} else if (bytes_avail > 0) {
			uci_log(UCI_DBG_VERBOSE,
				"Got packet: avail pkts %d phy_adr %pK, ch %d\n",
				atomic_read(&uci_handle->read_data_ready),
				ureq.buf,
				ureq.chan);
			break;
			/*
			 * MHI did not return a valid packet, but we have one
			 * which we did not finish returning to user
			 */
		} else {
			uci_log(UCI_DBG_CRITICAL,
				"chan %d err: avail pkts %d phy_adr %pK",
				ureq.chan,
				atomic_read(&uci_handle->read_data_ready),
				ureq.buf);
			return -EIO;
		}
	} while (!uci_handle->pkt_loc);

	if (uspace_buf_size >= *bytes_pending) {
		addr_offset = uci_handle->pkt_size - *bytes_pending;
		if (copy_to_user(ubuf, uci_handle->pkt_loc + addr_offset,
							*bytes_pending)) {
			ret_val = -EIO;
			goto error;
		}

		bytes_copied = *bytes_pending;
		*bytes_pending = 0;
		uci_log(UCI_DBG_VERBOSE, "Copied 0x%x of 0x%x, chan %d\n",
				bytes_copied, (u32)*bytes_pending, ureq.chan);
	} else {
		addr_offset = uci_handle->pkt_size - *bytes_pending;
		if (copy_to_user(ubuf, (void *) (uintptr_t)uci_handle->pkt_loc +
					addr_offset, uspace_buf_size)) {
			ret_val = -EIO;
			goto error;
		}
		bytes_copied = uspace_buf_size;
		*bytes_pending -= uspace_buf_size;
		uci_log(UCI_DBG_VERBOSE, "Copied 0x%x of 0x%x,chan %d\n",
				bytes_copied,
				(u32)*bytes_pending,
				ureq.chan);
	}
	/* We finished with this buffer, map it back */
	if (*bytes_pending == 0) {
		uci_log(UCI_DBG_VERBOSE,
				"All data consumed. Pkt loc %pK ,chan %d\n",
				uci_handle->pkt_loc, ureq.chan);
		uci_handle->pkt_loc = 0;
		uci_handle->pkt_size = 0;
	}
	uci_log(UCI_DBG_VERBOSE,
			"Returning 0x%x bytes, 0x%x bytes left\n",
			bytes_copied, (u32)*bytes_pending);
	mutex_unlock(mutex);
	return bytes_copied;
error:
	mutex_unlock(mutex);
	uci_log(UCI_DBG_ERROR, "Returning %d\n", ret_val);
	return ret_val;
}

static ssize_t mhi_uci_client_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *offp)
{
	struct uci_client *uci_handle = NULL;
	int ret_val = 0;
	u32 chan = 0xFFFFFFFF;

	if (file == NULL || buf == NULL ||
			!count || file->private_data == NULL)
		return -EINVAL;

	uci_handle = file->private_data;

	if (atomic_read(&uci_ctxt.mhi_disabled)) {
		uci_log(UCI_DBG_ERROR,
			"Client %d attempted to write while MHI is disabled\n",
			uci_handle->out_chan);
		return -EIO;
	}
	chan = uci_handle->out_chan;
	mutex_lock(&uci_handle->out_chan_lock);
	while (!ret_val) {
		ret_val = mhi_uci_send_packet(&uci_handle->out_handle,
						buf, count);
		if (ret_val < 0) {
			uci_log(UCI_DBG_ERROR,
				"Error while writing data to MHI, chan %d, buf %pK, size %d\n",
				chan, (void *)buf, count);
			ret_val = -EIO;
			break;
		}
		if (!ret_val) {
			uci_log(UCI_DBG_VERBOSE,
				"No descriptors available, did we poll, chan %d?\n",
				chan);
			mutex_unlock(&uci_handle->out_chan_lock);
			if (uci_handle->f_flags & (O_NONBLOCK | O_NDELAY))
				return -EAGAIN;
			ret_val = wait_event_interruptible(uci_handle->write_wq,
				!mhi_dev_channel_isempty(
					uci_handle->out_handle));

			mutex_lock(&uci_handle->out_chan_lock);
			if (-ERESTARTSYS == ret_val) {
				uci_log(UCI_DBG_WARNING,
					    "Waitqueue cancelled by system\n");
				break;
			}
		}
	}
	mutex_unlock(&uci_handle->out_chan_lock);
	return ret_val;
}

void uci_ctrl_update(struct mhi_dev_client_cb_reason *reason)
{
	struct uci_ctrl *uci_ctrl_handle = NULL;

	if (reason->reason == MHI_DEV_CTRL_UPDATE) {
		uci_ctrl_handle = &uci_ctxt.ctrl_handle;
		if (!uci_ctrl_handle) {
			pr_err("Invalid uci ctrl handle\n");
			return;
		}

		uci_log(UCI_DBG_DBG, "received state change update\n");
		wake_up(&uci_ctrl_handle->ctrl_wq);
		atomic_set(&uci_ctrl_handle->ctrl_data_update, 1);
	}
}
EXPORT_SYMBOL(uci_ctrl_update);

static void uci_event_notifier(struct mhi_dev_client_cb_reason *reason)
{
	int client_index = 0;
	struct uci_client *uci_handle = NULL;

	if (reason->reason == MHI_DEV_TRE_AVAILABLE) {
		client_index = reason->ch_id / 2;
		uci_handle = &uci_ctxt.client_handles[client_index];
		uci_log(UCI_DBG_DBG,
			"recived TRE available event for chan %d\n",
					uci_handle->in_chan);

		if (reason->ch_id % 2) {
			atomic_set(&uci_handle->write_data_ready, 1);
			wake_up(&uci_handle->write_wq);
		} else {
			atomic_set(&uci_handle->read_data_ready, 1);
			wake_up(&uci_handle->read_wq);
		}
	}
}

static int mhi_register_client(struct uci_client *mhi_client, int index)
{
	init_waitqueue_head(&mhi_client->read_wq);
	init_waitqueue_head(&mhi_client->write_wq);
	mhi_client->client_index = index;

	mutex_init(&mhi_client->in_chan_lock);
	mutex_init(&mhi_client->out_chan_lock);
	spin_lock_init(&mhi_client->wr_req_lock);

	uci_log(UCI_DBG_DBG, "Registering chan %d.\n", mhi_client->out_chan);
	return 0;
}

static long mhi_uci_client_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct uci_client *uci_handle = NULL;
	int rc = 0;
	struct ep_info epinfo;

	if (file == NULL || file->private_data == NULL)
		return -EINVAL;

	uci_handle = file->private_data;

	uci_log(UCI_DBG_DBG, "Received command %d for client:%d\n",
		cmd, uci_handle->client_index);

	if (cmd == MHI_UCI_EP_LOOKUP) {
		uci_log(UCI_DBG_DBG, "EP_LOOKUP for client:%d\n",
						uci_handle->client_index);
		epinfo.ph_ep_info.ep_type = DATA_EP_TYPE_PCIE;
		epinfo.ph_ep_info.peripheral_iface_id = MHI_QTI_IFACE_ID;
		epinfo.ipa_ep_pair.cons_pipe_num =
			ipa_get_ep_mapping(IPA_CLIENT_MHI_PROD);
		epinfo.ipa_ep_pair.prod_pipe_num =
			ipa_get_ep_mapping(IPA_CLIENT_MHI_CONS);

		uci_log(UCI_DBG_DBG, "client:%d ep_type:%d intf:%d\n",
			uci_handle->client_index,
			epinfo.ph_ep_info.ep_type,
			epinfo.ph_ep_info.peripheral_iface_id);

		uci_log(UCI_DBG_DBG, "ipa_cons_idx:%d ipa_prod_idx:%d\n",
			epinfo.ipa_ep_pair.cons_pipe_num,
			epinfo.ipa_ep_pair.prod_pipe_num);

		rc = copy_to_user((void __user *)arg, &epinfo,
			sizeof(epinfo));
		if (rc)
			uci_log(UCI_DBG_ERROR, "copying to user space failed");
	} else {
		uci_log(UCI_DBG_ERROR, "wrong parameter:%d\n", cmd);
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations mhi_uci_ctrl_client_fops = {
	.open = mhi_uci_ctrl_open,
	.read = mhi_uci_ctrl_client_read,
	.poll = mhi_uci_ctrl_poll,
};

static const struct file_operations mhi_uci_client_fops = {
	.read = mhi_uci_client_read,
	.write = mhi_uci_client_write,
	.open = mhi_uci_client_open,
	.release = mhi_uci_client_release,
	.poll = mhi_uci_client_poll,
	.unlocked_ioctl = mhi_uci_client_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mhi_uci_client_ioctl,
#endif
};

static int uci_device_create(struct uci_client *client)
{
	unsigned long r;
	int n;
	ssize_t dst_size;
	unsigned int client_index;
	static char device_name[MAX_DEVICE_NAME_SIZE];

	client_index = CHAN_TO_CLIENT(client->out_chan);
	if (uci_ctxt.client_handles[client_index].dev)
		return -EEXIST;

	cdev_init(&uci_ctxt.cdev[client_index], &mhi_uci_client_fops);
	uci_ctxt.cdev[client_index].owner = THIS_MODULE;
	r = cdev_add(&uci_ctxt.cdev[client_index],
		uci_ctxt.start_ctrl_nr + client_index, 1);
	if (IS_ERR_VALUE(r)) {
		uci_log(UCI_DBG_ERROR,
			"Failed to add cdev for client %d, ret 0x%x\n",
			client_index, r);
		return r;
	}
	if (!client->in_chan_attr->device_name) {
		n = snprintf(device_name, sizeof(device_name),
			DEVICE_NAME "_pipe_%d", CLIENT_TO_CHAN(client_index));
		if (n >= sizeof(device_name)) {
			uci_log(UCI_DBG_ERROR, "Device name buf too short\n");
			r = -E2BIG;
			goto error;
		}
	} else {
		dst_size = strscpy(device_name,
				client->in_chan_attr->device_name,
				sizeof(device_name));
		if (dst_size <= 0) {
			uci_log(UCI_DBG_ERROR, "Device name buf too short\n");
			r = dst_size;
			goto error;
		}
	}

	uci_ctxt.client_handles[client_index].dev =
		device_create(uci_ctxt.mhi_uci_class, NULL,
				uci_ctxt.start_ctrl_nr + client_index,
				NULL, device_name);
	if (IS_ERR(uci_ctxt.client_handles[client_index].dev)) {
		uci_log(UCI_DBG_ERROR,
			"Failed to create device for client %d\n",
			client_index);
		r = -EIO;
		goto error;
	}

	uci_log(UCI_DBG_INFO,
		"Created device with class 0x%pK and ctrl number %d\n",
		uci_ctxt.mhi_uci_class,
		uci_ctxt.start_ctrl_nr + client_index);

	return 0;

error:
	cdev_del(&uci_ctxt.cdev[client_index]);
	return r;
}

static void mhi_uci_client_cb(struct mhi_dev_client_cb_data *cb_data)
{
	struct uci_client *client = cb_data->user_data;

	uci_log(UCI_DBG_VERBOSE, " Rcvd MHI cb for channel %d, state %d\n",
		cb_data->channel, cb_data->ctrl_info);

	if (cb_data->ctrl_info == MHI_STATE_CONNECTED)
		uci_device_create(client);
}

static int uci_init_client_attributes(struct mhi_uci_ctxt_t *uci_ctxt)
{
	u32 i;
	u32 index;
	struct uci_client *client;
	const struct chan_attr *chan_attrib;

	for (i = 0; i < ARRAY_SIZE(uci_chan_attr_table); i += 2) {
		chan_attrib = &uci_chan_attr_table[i];
		index = CHAN_TO_CLIENT(chan_attrib->chan_id);
		client = &uci_ctxt->client_handles[index];
		client->out_chan_attr = chan_attrib;
		client->in_chan_attr = ++chan_attrib;
		client->in_chan = index * 2;
		client->out_chan = index * 2 + 1;
		client->in_buf_list =
			kcalloc(chan_attrib->nr_trbs,
			sizeof(struct mhi_dev_iov),
			GFP_KERNEL);
		if (!client->in_buf_list)
			return -ENOMEM;
		/* Register callback with MHI if requested */
		if (client->out_chan_attr->register_cb)
			mhi_register_state_cb(mhi_uci_client_cb, client,
						client->out_chan);
	}
	return 0;
}

int mhi_uci_init(void)
{
	u32 i = 0;
	int ret_val = 0;
	struct uci_client *mhi_client = NULL;
	unsigned long r = 0;

	mhi_uci_ipc_log = ipc_log_context_create(MHI_UCI_IPC_LOG_PAGES,
						"mhi-uci", 0);
	if (mhi_uci_ipc_log == NULL) {
		uci_log(UCI_DBG_WARNING,
				"Failed to create IPC logging context\n");
	}
	uci_ctxt.event_notifier = uci_event_notifier;

	uci_log(UCI_DBG_DBG, "Setting up channel attributes.\n");

	ret_val = uci_init_client_attributes(&uci_ctxt);
	if (ret_val < 0) {
		uci_log(UCI_DBG_ERROR,
				"Failed to init client attributes\n");
		return -EIO;
	}

	uci_log(UCI_DBG_DBG, "Initializing clients\n");
	uci_log(UCI_DBG_INFO, "Registering for MHI events.\n");

	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; i++) {
		mhi_client = &uci_ctxt.client_handles[i];
		if (!mhi_client->in_chan_attr)
			continue;
		r = mhi_register_client(mhi_client, i);
		if (r) {
			uci_log(UCI_DBG_CRITICAL,
				"Failed to reg client %d ret %d\n",
				r, i);
		}
	}

	init_waitqueue_head(&uci_ctxt.ctrl_handle.ctrl_wq);
	uci_log(UCI_DBG_INFO, "Allocating char devices.\n");
	r = alloc_chrdev_region(&uci_ctxt.start_ctrl_nr,
			0, MHI_MAX_SOFTWARE_CHANNELS,
			DEVICE_NAME);
	if (IS_ERR_VALUE(r)) {
		uci_log(UCI_DBG_ERROR,
				"Failed to alloc char devs, ret 0x%x\n", r);
		goto failed_char_alloc;
	}

	r = alloc_chrdev_region(&uci_ctxt.ctrl_nr, 0, 1, DEVICE_NAME);
	if (IS_ERR_VALUE(r)) {
		uci_log(UCI_DBG_ERROR,
				"Failed to alloc char ctrl devs, 0x%x\n", r);
		goto failed_char_alloc;
	}

	uci_log(UCI_DBG_INFO, "Creating class\n");
	uci_ctxt.mhi_uci_class = class_create(THIS_MODULE,
						DEVICE_NAME);
	if (IS_ERR(uci_ctxt.mhi_uci_class)) {
		uci_log(UCI_DBG_ERROR,
			"Failed to instantiate class, ret 0x%x\n", r);
		r = -ENOMEM;
		goto failed_class_add;
	}

	uci_log(UCI_DBG_INFO, "Setting up device nodes.\n");
	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; i++) {
		mhi_client = &uci_ctxt.client_handles[i];
		if (!mhi_client->in_chan_attr)
			continue;
		/*
		 * Delay device node creation until the callback for
		 * this client's channels is called by the MHI driver,
		 * if one is registered.
		 */
		if (mhi_client->in_chan_attr->register_cb)
			continue;
		ret_val = uci_device_create(mhi_client);
		if (ret_val)
			goto failed_device_create;
	}

	/* Control node */
	uci_ctxt.cdev_ctrl = cdev_alloc();
	if (uci_ctxt.cdev_ctrl == NULL) {
		pr_err("%s: ctrl cdev alloc failed\n", __func__);
		return 0;
	}

	cdev_init(uci_ctxt.cdev_ctrl, &mhi_uci_ctrl_client_fops);
	uci_ctxt.cdev_ctrl->owner = THIS_MODULE;
	r = cdev_add(uci_ctxt.cdev_ctrl, uci_ctxt.ctrl_nr, 1);
	if (IS_ERR_VALUE(r)) {
		uci_log(UCI_DBG_ERROR,
		"Failed to add ctrl cdev %d, ret 0x%x\n", i, r);
		kfree(uci_ctxt.cdev_ctrl);
		uci_ctxt.cdev_ctrl = NULL;
		return 0;
	}

	uci_ctxt.dev =
		device_create(uci_ctxt.mhi_uci_class, NULL,
				uci_ctxt.ctrl_nr,
				NULL, DEVICE_NAME "_ctrl");
	if (IS_ERR(uci_ctxt.dev)) {
		uci_log(UCI_DBG_ERROR,
				"Failed to add ctrl cdev %d\n", i);
		cdev_del(uci_ctxt.cdev_ctrl);
		kfree(uci_ctxt.cdev_ctrl);
		uci_ctxt.cdev_ctrl = NULL;
	}

	uci_ctxt.mhi_uci_class->dev_uevent = mhi_state_uevent;

	return 0;

failed_device_create:
	while (--i >= 0) {
		cdev_del(&uci_ctxt.cdev[i]);
		device_destroy(uci_ctxt.mhi_uci_class,
		MKDEV(MAJOR(uci_ctxt.start_ctrl_nr), i * 2));
	};
	class_destroy(uci_ctxt.mhi_uci_class);
failed_class_add:
	unregister_chrdev_region(MAJOR(uci_ctxt.start_ctrl_nr),
			MHI_MAX_SOFTWARE_CHANNELS);
failed_char_alloc:
	return r;
}
