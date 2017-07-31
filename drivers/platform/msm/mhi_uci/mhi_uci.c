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

#include <linux/msm_mhi.h>
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
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/of_device.h>

#define MHI_DEV_NODE_NAME_LEN 13
#define MHI_SOFTWARE_CLIENT_LIMIT 23
#define MHI_UCI_IPC_LOG_PAGES (25)

#define DEVICE_NAME "mhi"
#define MHI_UCI_DRIVER_NAME "mhi_uci"
#define CTRL_MAGIC 0x4C525443

enum UCI_DBG_LEVEL {
	UCI_DBG_VERBOSE = 0x0,
	UCI_DBG_INFO = 0x1,
	UCI_DBG_DBG = 0x2,
	UCI_DBG_WARNING = 0x3,
	UCI_DBG_ERROR = 0x4,
	UCI_DBG_CRITICAL = 0x5,
	UCI_DBG_reserved = 0x80000000
};
enum UCI_DBG_LEVEL mhi_uci_msg_lvl = UCI_DBG_CRITICAL;

#ifdef CONFIG_MSM_MHI_DEBUG
enum UCI_DBG_LEVEL mhi_uci_ipc_log_lvl = UCI_DBG_VERBOSE;
#else
enum UCI_DBG_LEVEL mhi_uci_ipc_log_lvl = UCI_DBG_ERROR;
#endif

struct __packed rs232_ctrl_msg {
	u32 preamble;
	u32 msg_id;
	u32 dest_id;
	u32 size;
	u32 msg;
};

enum MHI_SERIAL_STATE {
	MHI_SERIAL_STATE_DCD = 0x1,
	MHI_SERIAL_STATE_DSR = 0x2,
	MHI_SERIAL_STATE_RI = 0x3,
	MHI_SERIAL_STATE_reserved = 0x80000000,
};

enum MHI_CTRL_LINE_STATE {
	MHI_CTRL_LINE_STATE_DTR = 0x1,
	MHI_CTRL_LINE_STATE_RTS = 0x2,
	MHI_CTRL_LINE_STATE_reserved = 0x80000000,
};

enum MHI_MSG_ID {
	MHI_CTRL_LINE_STATE_ID = 0x10,
	MHI_SERIAL_STATE_ID = 0x11,
	MHI_CTRL_MSG_ID = 0x12,
};

enum MHI_CHAN_DIR {
	MHI_DIR_INVALID = 0x0,
	MHI_DIR_OUT = 0x1,
	MHI_DIR_IN = 0x2,
	MHI_DIR__reserved = 0x80000000
};

struct chan_attr {
	enum MHI_CLIENT_CHANNEL chan_id;
	size_t max_packet_size;
	u32 nr_trbs;
	enum MHI_CHAN_DIR dir;
	u32 uci_ownership;
	bool enabled;
	struct mhi_client_handle *mhi_handle;
	wait_queue_head_t wq;
	struct list_head buf_head;
	struct mutex chan_lock;
	atomic_t avail_pkts; /* no. avail tre to read or space avail for tx */
	u64 pkt_count;
};

struct uci_buf {
	void *data;
	u64 pkt_id;
	struct list_head node;
};

struct uci_client {
	struct chan_attr in_attr;
	struct chan_attr out_attr;
	struct device *dev;
	u8 local_tiocm;
	struct mutex client_lock; /* sync open and close */
	int ref_count;
	struct uci_buf *cur_buf; /* current buffer read processing */
	size_t pkt_size;
	struct work_struct outbound_worker; /* clean up outbound pkts */
	atomic_t out_pkt_pend_ack;
	atomic_t completion_ack;
	struct mhi_uci_ctxt_t *uci_ctxt;
	struct cdev cdev;
	bool enabled;
	void *uci_ipc_log;
};

struct mhi_uci_ctxt_t {
	struct list_head node;
	struct platform_device *pdev;
	struct uci_client client_handles[MHI_SOFTWARE_CLIENT_LIMIT];
	dev_t dev_t;
	struct mutex ctrl_mutex;
	struct uci_client *ctrl_client;
};

struct mhi_uci_drv_ctxt {
	struct list_head head;
	struct mutex list_lock;
	struct class *mhi_uci_class;
	void *mhi_uci_ipc_log;
};

#define CHAN_TO_CLIENT(_CHAN_NR) (_CHAN_NR / 2)

#define CTRL_MSG_ID
#define MHI_CTRL_MSG_ID__MASK (0xFFFFFF)
#define MHI_CTRL_MSG_ID__SHIFT (0)
#define MHI_SET_CTRL_MSG_ID(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = ((_PKT)->msg_id); \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->msg_id = new_val; \
};
#define MHI_GET_CTRL_MSG_ID(_FIELD, _PKT, _VAL) \
{ \
	(_VAL) = ((_PKT)->msg_id); \
	(_VAL) >>= (MHI_##_FIELD ## __SHIFT);\
	(_VAL) &= (MHI_##_FIELD ## __MASK); \
};

#define CTRL_DEST_ID
#define MHI_CTRL_DEST_ID__MASK (0xFFFFFF)
#define MHI_CTRL_DEST_ID__SHIFT (0)
#define MHI_SET_CTRL_DEST_ID(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = ((_PKT)->dest_id); \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->dest_id = new_val; \
};
#define MHI_GET_CTRL_DEST_ID(_FIELD, _PKT, _VAL) \
{ \
	(_VAL) = ((_PKT)->dest_id); \
	(_VAL) >>= (MHI_##_FIELD ## __SHIFT);\
	(_VAL) &= (MHI_##_FIELD ## __MASK); \
};

#define CTRL_MSG_DTR
#define MHI_CTRL_MSG_DTR__MASK (0xFFFFFFFE)
#define MHI_CTRL_MSG_DTR__SHIFT (0)

#define CTRL_MSG_RTS
#define MHI_CTRL_MSG_RTS__MASK (0xFFFFFFFD)
#define MHI_CTRL_MSG_RTS__SHIFT (1)

#define STATE_MSG_DCD
#define MHI_STATE_MSG_DCD__MASK (0xFFFFFFFE)
#define MHI_STATE_MSG_DCD__SHIFT (0)

#define STATE_MSG_DSR
#define MHI_STATE_MSG_DSR__MASK (0xFFFFFFFD)
#define MHI_STATE_MSG_DSR__SHIFT (1)

#define STATE_MSG_RI
#define MHI_STATE_MSG_RI__MASK (0xFFFFFFFB)
#define MHI_STATE_MSG_RI__SHIFT (3)
#define MHI_SET_CTRL_MSG(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = (_PKT->msg); \
	new_val &= (~((MHI_##_FIELD ## __MASK)));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->msg = new_val; \
};
#define MHI_GET_STATE_MSG(_FIELD, _PKT) \
	(((_PKT)->msg & (~(MHI_##_FIELD ## __MASK))) \
		>> MHI_##_FIELD ## __SHIFT)

#define CTRL_MSG_SIZE
#define MHI_CTRL_MSG_SIZE__MASK (0xFFFFFF)
#define MHI_CTRL_MSG_SIZE__SHIFT (0)
#define MHI_SET_CTRL_MSG_SIZE(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = (_PKT->size); \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT)->size = new_val; \
};

#define uci_log(uci_ipc_log, _msg_lvl, _msg, ...) do { \
	if (_msg_lvl >= mhi_uci_msg_lvl) { \
		pr_err("[%s] "_msg, __func__, ##__VA_ARGS__); \
	} \
	if (uci_ipc_log && (_msg_lvl >= mhi_uci_ipc_log_lvl)) { \
		ipc_log_string(uci_ipc_log, \
			"[%s] " _msg, __func__, ##__VA_ARGS__); \
	} \
} while (0)

module_param(mhi_uci_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_uci_msg_lvl, "uci dbg lvl");

module_param(mhi_uci_ipc_log_lvl, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_uci_ipc_log_lvl, "ipc dbg lvl");

static ssize_t mhi_uci_client_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp);
static ssize_t mhi_uci_client_write(struct file *file,
		const char __user *buf, size_t count, loff_t *offp);
static int mhi_uci_client_open(struct inode *mhi_inode, struct file*);
static int mhi_uci_client_release(struct inode *mhi_inode,
		struct file *file_handle);
static unsigned int mhi_uci_client_poll(struct file *file, poll_table *wait);
static long mhi_uci_ctl_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg);
static int mhi_uci_create_device(struct uci_client *uci_client);

static struct mhi_uci_drv_ctxt mhi_uci_drv_ctxt;

static void mhi_uci_clean_acked_tre(struct work_struct *work)
{
	struct uci_client *uci_client;
	int i = 0;

	uci_client = container_of(work, struct uci_client, outbound_worker);
	while (atomic_read(&uci_client->completion_ack)) {
		struct uci_buf *uci_buf;

		/* acquire lock per tre so we won't block other uci threads */
		mutex_lock(&uci_client->out_attr.chan_lock);
		uci_buf = list_first_entry_or_null(
						&uci_client->out_attr.buf_head,
						struct uci_buf, node);
		if (unlikely(!uci_buf)) {
			mutex_unlock(&uci_client->out_attr.chan_lock);
			break;
		}
		list_del(&uci_buf->node);
		kfree(uci_buf->data);
		atomic_dec(&uci_client->completion_ack);
		mutex_unlock(&uci_client->out_attr.chan_lock);
		i++;
	}
	uci_log(uci_client->uci_ipc_log, UCI_DBG_VERBOSE,
		"freed %d tres for chan %d\n",
		i, uci_client->out_attr.chan_id);
}

static int mhi_init_inbound(struct uci_client *client_handle)
{
	int ret_val = 0;
	u32 i = 0;
	struct chan_attr *chan_attributes = &client_handle->in_attr;
	void *data_loc = NULL;
	size_t buf_size = chan_attributes->max_packet_size;
	struct uci_buf *uci_buf;

	chan_attributes->nr_trbs =
			mhi_get_free_desc(client_handle->in_attr.mhi_handle);

	uci_log(client_handle->uci_ipc_log,
		UCI_DBG_INFO, "Channel %d supports %d desc\n",
		chan_attributes->chan_id,
		chan_attributes->nr_trbs);
	for (i = 0; i < chan_attributes->nr_trbs; ++i) {
		data_loc = kmalloc(buf_size + sizeof(*uci_buf), GFP_KERNEL);

		/*
		 * previously allocated memory will be freed after
		 * channel close
		 */
		if (data_loc == NULL)
			return -ENOMEM;
		uci_buf = data_loc + buf_size;
		uci_buf->data = data_loc;
		uci_buf->pkt_id = chan_attributes->pkt_count++;
		uci_log(client_handle->uci_ipc_log, UCI_DBG_INFO,
			"Allocated buffer %llu size %ld for chan:%d\n",
			uci_buf->pkt_id, buf_size, chan_attributes->chan_id);
		ret_val = mhi_queue_xfer(client_handle->in_attr.mhi_handle,
					 data_loc, buf_size, MHI_EOT);
		if (0 != ret_val) {
			kfree(data_loc);
			uci_log(client_handle->uci_ipc_log,
				UCI_DBG_ERROR,
				"Failed insertion for chan %d, ret %d\n",
				chan_attributes->chan_id,
				ret_val);
			break;
		}
		list_add_tail(&uci_buf->node, &client_handle->in_attr.buf_head);
	}
	return ret_val;
}

static int mhi_uci_send_status_cmd(struct uci_client *client)
{
	void *buf = NULL;
	struct rs232_ctrl_msg *rs232_pkt = NULL;
	struct uci_buf *uci_buf = NULL;
	struct uci_client *uci_ctrl_handle;
	struct mhi_uci_ctxt_t *uci_ctxt = client->uci_ctxt;
	int ret_val = 0;

	if (!uci_ctxt->ctrl_client) {
		uci_log(client->uci_ipc_log, UCI_DBG_INFO,
			"Control channel is not defined\n");
		return -EIO;
	}

	uci_ctrl_handle = uci_ctxt->ctrl_client;
	mutex_lock(&uci_ctrl_handle->out_attr.chan_lock);

	if (!uci_ctrl_handle->enabled) {
		uci_log(uci_ctrl_handle->uci_ipc_log, UCI_DBG_INFO,
			"Opening outbound control channel %d for chan:%d\n",
			uci_ctrl_handle->out_attr.chan_id,
			client->out_attr.chan_id);
		if (!uci_ctrl_handle->out_attr.enabled) {
			uci_log(uci_ctrl_handle->uci_ipc_log, UCI_DBG_CRITICAL,
				"Channel %d is not enable\n",
				uci_ctrl_handle->out_attr.chan_id);
			ret_val = -EIO;
			goto error_open;
		}
		ret_val = mhi_open_channel(uci_ctrl_handle->
					   out_attr.mhi_handle);
		if (ret_val) {
			uci_log(uci_ctrl_handle->uci_ipc_log, UCI_DBG_CRITICAL,
				"Could not open chan %d, for sideband ctrl\n",
				uci_ctrl_handle->out_attr.chan_id);
			ret_val = -EIO;
			goto error_open;
		}
		uci_ctrl_handle->enabled = true;
	}

	if (mhi_get_free_desc(uci_ctrl_handle->out_attr.mhi_handle) <= 0) {
		ret_val = -EIO;
		goto error_open;
	}

	buf = kzalloc(sizeof(*rs232_pkt) + sizeof(*uci_buf), GFP_KERNEL);
	if (!buf) {
		ret_val = -ENOMEM;
		goto error_open;
	}


	uci_log(uci_ctrl_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Received request to send msg for chan %d\n",
		client->out_attr.chan_id);
	uci_buf = buf + sizeof(*rs232_pkt);
	uci_buf->data = buf;
	rs232_pkt = (struct rs232_ctrl_msg *)uci_buf->data;
	rs232_pkt->preamble = CTRL_MAGIC;
	if (client->local_tiocm & TIOCM_DTR)
		MHI_SET_CTRL_MSG(CTRL_MSG_DTR, rs232_pkt, 1);
	if (client->local_tiocm & TIOCM_RTS)
		MHI_SET_CTRL_MSG(CTRL_MSG_RTS, rs232_pkt, 1);

	MHI_SET_CTRL_MSG_ID(CTRL_MSG_ID, rs232_pkt, MHI_CTRL_LINE_STATE_ID);
	MHI_SET_CTRL_MSG_SIZE(CTRL_MSG_SIZE, rs232_pkt, sizeof(u32));
	MHI_SET_CTRL_DEST_ID(CTRL_DEST_ID, rs232_pkt, client->out_attr.chan_id);

	ret_val = mhi_queue_xfer(uci_ctrl_handle->out_attr.mhi_handle,
				 uci_buf->data, sizeof(*rs232_pkt), MHI_EOT);
	if (ret_val) {
		uci_log(uci_ctrl_handle->uci_ipc_log, UCI_DBG_INFO,
			"Failed to send signal for chan %d, ret : %d\n",
			client->out_attr.chan_id, ret_val);
		goto error_queue;
	}
	list_add_tail(&uci_buf->node, &uci_ctrl_handle->out_attr.buf_head);

	mutex_unlock(&uci_ctrl_handle->out_attr.chan_lock);
	return 0;

	mutex_unlock(&uci_ctrl_handle->out_attr.chan_lock);
	return ret_val;
error_queue:
	kfree(buf);
error_open:
	mutex_unlock(&uci_ctrl_handle->out_attr.chan_lock);
	return ret_val;
}

static int mhi_uci_tiocm_set(struct uci_client *client_ctxt, u32 set, u32 clear)
{
	u8 status_set = 0;
	u8 status_clear = 0;
	u8 old_status = 0;
	int ret = 0;

	mutex_lock(&client_ctxt->uci_ctxt->ctrl_mutex);

	status_set |= (set & TIOCM_DTR) ? TIOCM_DTR : 0;
	status_clear |= (clear & TIOCM_DTR) ? TIOCM_DTR : 0;
	old_status = client_ctxt->local_tiocm;
	client_ctxt->local_tiocm |= status_set;
	client_ctxt->local_tiocm &= ~status_clear;

	uci_log(client_ctxt->uci_ipc_log, UCI_DBG_VERBOSE,
		"Old TIOCM0x%x for chan %d, Current TIOCM 0x%x\n",
		old_status, client_ctxt->out_attr.chan_id,
		client_ctxt->local_tiocm);

	if (client_ctxt->local_tiocm != old_status) {
		uci_log(client_ctxt->uci_ipc_log, UCI_DBG_VERBOSE,
			"Setting TIOCM to 0x%x for chan %d\n",
			client_ctxt->local_tiocm,
			client_ctxt->out_attr.chan_id);
		ret = mhi_uci_send_status_cmd(client_ctxt);
	}

	mutex_unlock(&client_ctxt->uci_ctxt->ctrl_mutex);
	return ret;
}

static long mhi_uci_ctl_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int ret_val = 0;
	u32 set_val;
	struct uci_client *uci_handle = NULL;
	uci_handle = file->private_data;

	if (uci_handle == NULL) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_VERBOSE,
			"Invalid handle for client\n");
		return -ENODEV;
	}
	uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Attempting to dtr cmd 0x%x arg 0x%lx for chan %d\n",
		cmd, arg, uci_handle->out_attr.chan_id);

	switch (cmd) {
	case TIOCMGET:
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
			"Returning 0x%x mask\n", uci_handle->local_tiocm);
		ret_val = uci_handle->local_tiocm;
		break;
	case TIOCMSET:
		if (0 != copy_from_user(&set_val, (void *)arg, sizeof(set_val)))
			return -ENOMEM;
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
			"Attempting to set cmd 0x%x arg 0x%x for chan %d\n",
			cmd, set_val, uci_handle->out_attr.chan_id);
		ret_val = mhi_uci_tiocm_set(uci_handle, set_val, ~set_val);
		break;
	default:
		ret_val = -EINVAL;
		break;
	}
	return ret_val;
}

static unsigned int mhi_uci_client_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct uci_client *uci_handle = file->private_data;
	struct mhi_uci_ctxt_t *uci_ctxt;

	if (uci_handle == NULL)
		return -ENODEV;

	uci_ctxt = uci_handle->uci_ctxt;
	poll_wait(file, &uci_handle->in_attr.wq, wait);
	poll_wait(file, &uci_handle->out_attr.wq, wait);
	mutex_lock(&uci_handle->in_attr.chan_lock);
	if (!uci_handle->in_attr.enabled || !uci_handle->enabled)
		mask = POLLERR;
	else if (atomic_read(&uci_handle->in_attr.avail_pkts) ||
		 uci_handle->cur_buf) {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
			"Client can read chan %d\n",
			uci_handle->in_attr.chan_id);
		mask |= POLLIN | POLLRDNORM;
	}
	mutex_unlock(&uci_handle->in_attr.chan_lock);
	mutex_lock(&uci_handle->out_attr.chan_lock);
	if (!uci_handle->out_attr.enabled || !uci_handle->enabled)
		mask |= POLLERR;
	else if (atomic_read(&uci_handle->out_attr.avail_pkts) > 0) {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
			"Client can write chan %d\n",
			uci_handle->out_attr.chan_id);
		mask |= POLLOUT | POLLWRNORM;
	}
	mutex_unlock(&uci_handle->out_attr.chan_lock);
	uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Client attempted to poll chan %d, returning mask 0x%x\n",
		uci_handle->in_attr.chan_id, mask);
	return mask;
}

static int open_client_mhi_channels(struct uci_client *uci_client)
{
	int ret_val = 0;
	int r = 0;
	struct uci_buf *itr, *tmp;

	uci_log(uci_client->uci_ipc_log, UCI_DBG_INFO,
		"Starting channels %d %d\n",
		uci_client->out_attr.chan_id,
		uci_client->in_attr.chan_id);

	ret_val = mhi_open_channel(uci_client->out_attr.mhi_handle);
	if (ret_val != 0) {
		if (ret_val == -ENOTCONN)
			return -EAGAIN;
		else
			return -EIO;
	}
	ret_val = mhi_get_free_desc(uci_client->out_attr.mhi_handle);
	if (ret_val >= 0)
		atomic_set(&uci_client->out_attr.avail_pkts, ret_val);

	ret_val = mhi_open_channel(uci_client->in_attr.mhi_handle);
	if (ret_val != 0) {
		uci_log(uci_client->uci_ipc_log, UCI_DBG_ERROR,
			"Failed to open chan %d, ret 0x%x\n",
			uci_client->out_attr.chan_id, ret_val);
		goto error_inbound_open;
	}
	uci_log(uci_client->uci_ipc_log, UCI_DBG_INFO,
		"Initializing inbound chan %d\n",
		uci_client->in_attr.chan_id);

	ret_val = mhi_init_inbound(uci_client);
	if (0 != ret_val) {
		uci_log(uci_client->uci_ipc_log, UCI_DBG_ERROR,
			"Failed to init inbound 0x%x, ret 0x%x\n",
			uci_client->in_attr.chan_id, ret_val);
		goto error_init_inbound;

	}
	atomic_set(&uci_client->completion_ack, 0);
	uci_client->enabled = true;
	return 0;

error_init_inbound:
	mhi_close_channel(uci_client->in_attr.mhi_handle);
	list_for_each_entry_safe(itr, tmp, &uci_client->in_attr.buf_head,
				 node) {
		list_del(&itr->node);
		kfree(itr->data);
	}
	INIT_LIST_HEAD(&uci_client->in_attr.buf_head);

error_inbound_open:
	mhi_close_channel(uci_client->out_attr.mhi_handle);
	return r;
}

static int mhi_uci_client_open(struct inode *inode,
			       struct file *file_handle)
{
	struct uci_client *uci_handle = NULL;
	struct mhi_uci_ctxt_t *uci_ctxt = NULL, *itr;
	const long timeout = msecs_to_jiffies(1000);
	int r = 0;
	int client_id = iminor(inode);
	int major = imajor(inode);

	/* Find the uci ctxt from major */
	mutex_lock(&mhi_uci_drv_ctxt.list_lock);
	list_for_each_entry(itr, &mhi_uci_drv_ctxt.head, node) {
		if (MAJOR(itr->dev_t) == major) {
			uci_ctxt = itr;
			break;
		}
	}
	mutex_unlock(&mhi_uci_drv_ctxt.list_lock);

	if (!uci_ctxt || client_id >= MHI_SOFTWARE_CLIENT_LIMIT)
		return -EINVAL;

	uci_handle = &uci_ctxt->client_handles[client_id];
	r = wait_event_interruptible_timeout(uci_handle->out_attr.wq,
					     uci_handle->out_attr.enabled,
					     timeout);
	if (r < 0)
		return -EAGAIN;
	r = wait_event_interruptible_timeout(uci_handle->in_attr.wq,
					     uci_handle->in_attr.enabled,
					     timeout);
	if (r < 0)
		return -EAGAIN;
	r = 0;
	mutex_lock(&uci_handle->client_lock);
	mutex_lock(&uci_handle->out_attr.chan_lock);
	mutex_lock(&uci_handle->in_attr.chan_lock);
	if (!uci_handle->out_attr.enabled || !uci_handle->in_attr.enabled) {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"MHI channel still disable for, client %d\n",
			client_id);
		mutex_unlock(&uci_handle->in_attr.chan_lock);
		mutex_unlock(&uci_handle->out_attr.chan_lock);
		mutex_unlock(&uci_handle->client_lock);
		return -EAGAIN;
	}

	uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
		"Client opened device node 0x%x, ref count 0x%x\n",
		client_id, uci_handle->ref_count);

	if (++uci_handle->ref_count == 1) {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"Opening channels client %d for first time\n",
			client_id);
		r = open_client_mhi_channels(uci_handle);
		if (r) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
				"Failed to open channels ret %d\n", r);
			uci_handle->ref_count--;
		}
	}
	mutex_unlock(&uci_handle->in_attr.chan_lock);
	mutex_unlock(&uci_handle->out_attr.chan_lock);
	mutex_unlock(&uci_handle->client_lock);
	file_handle->private_data = uci_handle;
	return r;
}

static int mhi_uci_client_release(struct inode *mhi_inode,
				  struct file *file_handle)
{
	struct uci_client *uci_handle = file_handle->private_data;
	u32 nr_in_bufs = 0;
	int in_chan = 0;
	u32 buf_size = 0;

	mutex_lock(&uci_handle->client_lock);
	in_chan = uci_handle->in_attr.chan_id;
	nr_in_bufs = uci_handle->in_attr.nr_trbs;
	buf_size = uci_handle->in_attr.max_packet_size;
	uci_handle->ref_count--;
	if (!uci_handle->ref_count) {
		struct uci_buf *itr, *tmp;

		uci_log(uci_handle->uci_ipc_log, UCI_DBG_ERROR,
			"Last client left, closing channel 0x%x\n",
			in_chan);

		mutex_lock(&uci_handle->in_attr.chan_lock);
		mutex_lock(&uci_handle->out_attr.chan_lock);

		if (atomic_read(&uci_handle->out_pkt_pend_ack))
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
				"Still waiting on %d acks!, chan %d\n",
				atomic_read(&uci_handle->out_pkt_pend_ack),
				uci_handle->out_attr.chan_id);

		atomic_set(&uci_handle->in_attr.avail_pkts, 0);
		if (uci_handle->in_attr.enabled)
			mhi_close_channel(uci_handle->in_attr.mhi_handle);
		list_for_each_entry_safe(itr, tmp,
					 &uci_handle->in_attr.buf_head, node) {
			list_del(&itr->node);
			kfree(itr->data);
		}
		if (uci_handle->cur_buf)
			kfree(uci_handle->cur_buf->data);
		uci_handle->cur_buf = NULL;
		INIT_LIST_HEAD(&uci_handle->in_attr.buf_head);
		atomic_set(&uci_handle->out_attr.avail_pkts, 0);
		atomic_set(&uci_handle->out_pkt_pend_ack, 0);
		if (uci_handle->out_attr.enabled)
			mhi_close_channel(uci_handle->out_attr.mhi_handle);
		list_for_each_entry_safe(itr, tmp,
					 &uci_handle->out_attr.buf_head, node) {
			list_del(&itr->node);
			kfree(itr->data);
		}
		INIT_LIST_HEAD(&uci_handle->out_attr.buf_head);
		uci_handle->enabled = false;
		mutex_unlock(&uci_handle->out_attr.chan_lock);
		flush_work(&uci_handle->outbound_worker);
		atomic_set(&uci_handle->completion_ack, 0);
		mutex_unlock(&uci_handle->in_attr.chan_lock);
	} else {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"Client close chan %d, ref count 0x%x\n",
			iminor(mhi_inode),
			uci_handle->ref_count);
	}
	mutex_unlock(&uci_handle->client_lock);

	return 0;
}

static ssize_t mhi_uci_client_read(struct file *file,
				   char __user *buf,
				   size_t uspace_buf_size,
				   loff_t *bytes_pending)
{
	struct uci_client *uci_handle = NULL;
	struct mhi_client_handle *client_handle = NULL;
	int ret_val = 0;
	size_t buf_size = 0;
	struct mutex *chan_lock;
	u32 chan = 0;
	size_t bytes_copied = 0;
	u32 addr_offset = 0;
	struct mhi_result result;

	if (file == NULL || buf == NULL ||
	    uspace_buf_size == 0 || file->private_data == NULL)
		return -EINVAL;

	uci_handle = file->private_data;
	client_handle = uci_handle->in_attr.mhi_handle;
	chan_lock = &uci_handle->in_attr.chan_lock;
	chan = uci_handle->in_attr.chan_id;
	buf_size = uci_handle->in_attr.max_packet_size;
	result.buf_addr = NULL;

	uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Client attempted read on chan %d\n", chan);

	mutex_lock(chan_lock);

	/* confirm channel is active */
	if (!uci_handle->in_attr.enabled || !uci_handle->enabled) {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"chan:%d is disabled\n", chan);
		ret_val = -ERESTARTSYS;
		goto read_error;
	}

	/* No data available to read, wait */
	if (!uci_handle->cur_buf &&
	    !atomic_read(&uci_handle->in_attr.avail_pkts)) {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"No data available to read for chan:%d waiting\n",
			chan);
		mutex_unlock(chan_lock);
		ret_val = wait_event_interruptible(uci_handle->in_attr.wq,
				(atomic_read(&uci_handle->in_attr.avail_pkts) ||
				 !uci_handle->in_attr.enabled));
		mutex_lock(chan_lock);
		if (ret_val == -ERESTARTSYS) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
				"Exit signal caught for chan:%d\n", chan);
			goto read_error;

		}
		if (!uci_handle->in_attr.enabled || !uci_handle->enabled) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
				"chan:%d is disabled\n", chan);
			ret_val = -ERESTARTSYS;
			goto read_error;
		}
	}

	/* new read, get the data from MHI */
	if (!uci_handle->cur_buf) {
		struct uci_buf *cur_buf;

		ret_val = mhi_poll_inbound(client_handle, &result);
		if (unlikely(ret_val || !result.buf_addr)) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_ERROR,
				"Failed to poll inbound ret %d avail_pkt %d\n",
				ret_val,
				atomic_read(&uci_handle->in_attr.avail_pkts));
			goto read_error;
		}
		cur_buf = list_first_entry_or_null(
					&uci_handle->in_attr.buf_head,
					struct uci_buf, node);
		if (unlikely(!cur_buf)) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_ERROR,
				"Received completion cb but no packets queued, avail_pkt:%d\n",
				atomic_read(&uci_handle->in_attr.avail_pkts));
			ret_val = -EIO;
			goto read_error;
		}

		if (unlikely(cur_buf->data != result.buf_addr)) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_ERROR,
				"Receive out of order packet id:%llu\n",
				cur_buf->pkt_id);
			ret_val = -EIO;
			goto read_error;
		}

		list_del(&cur_buf->node);
		uci_handle->cur_buf = cur_buf;
		*bytes_pending = result.bytes_xferd;
		uci_handle->pkt_size = result.bytes_xferd;
		atomic_dec(&uci_handle->in_attr.avail_pkts);
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
			"Got pkt @ %llu size:%llu for chan:%d\n",
			uci_handle->cur_buf->pkt_id, *bytes_pending, chan);
	}

	/* Copy the buffer to user space */
	bytes_copied = min_t(size_t, uspace_buf_size, *bytes_pending);
	addr_offset = uci_handle->pkt_size - *bytes_pending;
	ret_val = copy_to_user(buf, uci_handle->cur_buf->data + addr_offset,
			       bytes_copied);
	if (ret_val != 0)
		goto read_error;
	uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Copied %lu of %llu bytes for chan:%d\n",
		bytes_copied, *bytes_pending, chan);
	*bytes_pending -= bytes_copied;

	/* We finished with this buffer, map it back */
	if (*bytes_pending == 0) {
		struct uci_buf *uci_buf = uci_handle->cur_buf;

		uci_handle->cur_buf = NULL;
		uci_buf->pkt_id = uci_handle->in_attr.pkt_count++;
		memset(uci_buf->data, 0xdeadbeef, buf_size);
		ret_val = mhi_queue_xfer(client_handle, uci_buf->data,
					 buf_size, MHI_EOT);
		if (0 != ret_val) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_ERROR,
				"Failed to recycle element\n");
			kfree(uci_buf->data);
			goto read_error;
		}
		list_add_tail(&uci_buf->node, &uci_handle->in_attr.buf_head);
	}
	uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Returning %lu bytes, %llu bytes left\n",
		bytes_copied, *bytes_pending);
	mutex_unlock(chan_lock);
	return bytes_copied;

read_error:
	mutex_unlock(chan_lock);
	return ret_val;
}

static ssize_t mhi_uci_client_write(struct file *file,
				    const char __user *buf,
				    size_t count,
				    loff_t *offp)
{
	struct uci_client *uci_handle = NULL;
	struct chan_attr *chan_attr;
	size_t bytes_transferrd = 0;
	int ret_val = 0;
	u32 chan = 0xFFFFFFFF;

	if (file == NULL || buf == NULL || !count ||
	    file->private_data == NULL)
		return -EINVAL;
	else
		uci_handle = file->private_data;

	chan_attr = &uci_handle->out_attr;
	chan = chan_attr->chan_id;
	mutex_lock(&chan_attr->chan_lock);

	if (!chan_attr->enabled || !uci_handle->enabled) {
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"Link is disabled\n");
		ret_val = -ERESTARTSYS;
		goto sys_interrupt;
	}

	while (count) {
		size_t xfer_size;
		void *data_loc = NULL;
		struct uci_buf *uci_buf;

		mutex_unlock(&chan_attr->chan_lock);
		ret_val = wait_event_interruptible(chan_attr->wq,
				(atomic_read(&chan_attr->avail_pkts) ||
				 !chan_attr->enabled));
		mutex_lock(&chan_attr->chan_lock);
		if (-ERESTARTSYS == ret_val) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
				"Waitqueue cancelled by system\n");
			goto sys_interrupt;
		}
		if (!chan_attr->enabled || !uci_handle->enabled) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
				"Link is disabled\n");
			ret_val = -ERESTARTSYS;
			goto sys_interrupt;
		}

		xfer_size = min_t(size_t, count, chan_attr->max_packet_size);
		data_loc = kmalloc(xfer_size + sizeof(*uci_buf), GFP_KERNEL);
		if (!data_loc) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
				"Failed to allocate memory %lu\n", xfer_size);
			ret_val = -ENOMEM;
			goto sys_interrupt;
		}

		uci_buf = data_loc + xfer_size;
		uci_buf->data = data_loc;
		uci_buf->pkt_id = uci_handle->out_attr.pkt_count++;
		ret_val = copy_from_user(uci_buf->data, buf, xfer_size);
		if (unlikely(ret_val)) {
			kfree(uci_buf->data);
			goto sys_interrupt;
		}
		ret_val = mhi_queue_xfer(chan_attr->mhi_handle, uci_buf->data,
					 xfer_size, MHI_EOT);
		if (unlikely(ret_val)) {
			kfree(uci_buf->data);
			goto sys_interrupt;
		}

		bytes_transferrd += xfer_size;
		count -= xfer_size;
		buf += xfer_size;
		atomic_inc(&uci_handle->out_pkt_pend_ack);
		atomic_dec(&uci_handle->out_attr.avail_pkts);
		list_add_tail(&uci_buf->node, &uci_handle->out_attr.buf_head);
	}

	mutex_unlock(&chan_attr->chan_lock);
	return bytes_transferrd;

sys_interrupt:
	mutex_unlock(&chan_attr->chan_lock);
	return ret_val;
}

static int uci_init_client_attributes(struct mhi_uci_ctxt_t *uci_ctxt,
				      struct device_node *of_node)
{
	int num_rows, ret_val = 0;
	int i, dir;
	u32 ctrl_chan = -1;
	u32 *chan_info, *itr;
	const char *prop_name = "qcom,mhi-uci-channels";

	ret_val = of_property_read_u32(of_node, "qcom,mhi-uci-ctrlchan",
				       &ctrl_chan);
	if (ret_val) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log,
			UCI_DBG_INFO,
			"Could not find property 'qcom,mhi-uci-ctrlchan'\n");
	}

	num_rows = of_property_count_elems_of_size(of_node, prop_name,
						   sizeof(u32) * 4);
	/* At least one pair of channels should exist */
	if (num_rows < 1) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log,
			UCI_DBG_CRITICAL,
			"Missing or invalid property 'qcom,mhi-uci-channels'\n");
		return -ENODEV;
	}

	if (num_rows > MHI_SOFTWARE_CLIENT_LIMIT)
		num_rows = MHI_SOFTWARE_CLIENT_LIMIT;

	chan_info = kmalloc_array(num_rows, 4 * sizeof(*chan_info), GFP_KERNEL);
	if (!chan_info)
		return -ENOMEM;

	ret_val = of_property_read_u32_array(of_node, prop_name, chan_info,
					     num_rows * 4);
	if (ret_val)
		goto error_dts;

	for (i = 0, itr = chan_info; i < num_rows; i++) {
		struct uci_client *client = &uci_ctxt->client_handles[i];
		struct chan_attr *chan_attrib;

		for (dir = 0; dir < 2; dir++) {
			chan_attrib = (dir) ?
				&client->in_attr : &client->out_attr;
			chan_attrib->uci_ownership = 1;
			chan_attrib->chan_id = *itr++;
			chan_attrib->max_packet_size = *itr++;
			if (dir == 0)
				chan_attrib->dir = MHI_DIR_OUT;
			else
				chan_attrib->dir = MHI_DIR_IN;

			if (chan_attrib->chan_id == ctrl_chan)
				uci_ctxt->ctrl_client = client;

			INIT_LIST_HEAD(&chan_attrib->buf_head);
			mutex_init(&chan_attrib->chan_lock);
			atomic_set(&chan_attrib->avail_pkts, 0);
		}
		INIT_WORK(&client->outbound_worker, mhi_uci_clean_acked_tre);
	}

error_dts:
	kfree(chan_info);
	return ret_val;
}

static void process_rs232_state(struct uci_client *ctrl_client,
				struct mhi_result *result)
{
	struct rs232_ctrl_msg *rs232_pkt = result->buf_addr;
	struct uci_client *client = NULL;
	struct mhi_uci_ctxt_t *uci_ctxt = ctrl_client->uci_ctxt;
	u32 msg_id;
	int ret_val, i;
	u32 chan;

	mutex_lock(&uci_ctxt->ctrl_mutex);
	if (result->transaction_status != 0) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_ERROR,
			"Non successful transfer code 0x%x\n",
			result->transaction_status);
		goto error_bad_xfer;
	}
	if (result->bytes_xferd != sizeof(struct rs232_ctrl_msg)) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_ERROR,
			"Buffer is of wrong size is: 0x%zx: expected 0x%zx\n",
			result->bytes_xferd,
			sizeof(struct rs232_ctrl_msg));
		goto error_size;
	}
	MHI_GET_CTRL_DEST_ID(CTRL_DEST_ID, rs232_pkt, chan);
	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; i++)
		if (chan == uci_ctxt->client_handles[i].out_attr.chan_id ||
		    chan == uci_ctxt->client_handles[i].in_attr.chan_id) {
			client = &uci_ctxt->client_handles[i];
			break;
		}

	/* No valid channel found */
	if (!client)
		goto error_bad_xfer;

	MHI_GET_CTRL_MSG_ID(CTRL_MSG_ID, rs232_pkt, msg_id);
	client->local_tiocm = 0;
	if (MHI_SERIAL_STATE_ID == msg_id) {
		client->local_tiocm |=
			MHI_GET_STATE_MSG(STATE_MSG_DCD, rs232_pkt) ?
			TIOCM_CD : 0;
		client->local_tiocm |=
			MHI_GET_STATE_MSG(STATE_MSG_DSR, rs232_pkt) ?
			TIOCM_DSR : 0;
		client->local_tiocm |=
			MHI_GET_STATE_MSG(STATE_MSG_RI, rs232_pkt) ?
			TIOCM_RI : 0;
	}
error_bad_xfer:
error_size:
	memset(rs232_pkt, 0, sizeof(struct rs232_ctrl_msg));
	ret_val = mhi_queue_xfer(ctrl_client->in_attr.mhi_handle,
				 result->buf_addr,
				 result->bytes_xferd,
				 result->flags);
	if (0 != ret_val) {
		uci_log(ctrl_client->uci_ipc_log, UCI_DBG_ERROR,
			"Failed to recycle ctrl msg buffer\n");
	}
	mutex_unlock(&uci_ctxt->ctrl_mutex);
}

static void parse_inbound_ack(struct uci_client *uci_handle,
			struct mhi_result *result)
{
	atomic_inc(&uci_handle->in_attr.avail_pkts);
	uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Received cb on chan %d, avail pkts: 0x%x\n",
		uci_handle->in_attr.chan_id,
		atomic_read(&uci_handle->in_attr.avail_pkts));
	wake_up(&uci_handle->in_attr.wq);
	if (uci_handle == uci_handle->uci_ctxt->ctrl_client)
		process_rs232_state(uci_handle, result);
}

static void parse_outbound_ack(struct uci_client *uci_handle,
			struct mhi_result *result)
{
	uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
		"Received ack on chan %d, pending acks: 0x%x\n",
		uci_handle->out_attr.chan_id,
		atomic_read(&uci_handle->out_pkt_pend_ack));
	atomic_dec(&uci_handle->out_pkt_pend_ack);
	atomic_inc(&uci_handle->out_attr.avail_pkts);
	atomic_inc(&uci_handle->completion_ack);
	wake_up(&uci_handle->out_attr.wq);
	schedule_work(&uci_handle->outbound_worker);
}

static void uci_xfer_cb(struct mhi_cb_info *cb_info)
{
	struct uci_client *uci_handle = NULL;
	struct mhi_result *result;
	struct chan_attr *chan_attr;

	if (!cb_info || !cb_info->result) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_CRITICAL,
			"Bad CB info from MHI\n");
		return;
	}

	uci_handle = cb_info->result->user_data;
	switch (cb_info->cb_reason) {
	case MHI_CB_MHI_PROBED:
		/* If it's outbound channel create the node */
		mutex_lock(&uci_handle->client_lock);
		if (!uci_handle->dev &&
		    cb_info->chan == uci_handle->out_attr.chan_id)
			mhi_uci_create_device(uci_handle);
		mutex_unlock(&uci_handle->client_lock);
		break;
	case MHI_CB_MHI_ENABLED:
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"MHI enabled CB received for chan %d\n",
			cb_info->chan);
		chan_attr = (cb_info->chan % 2) ? &uci_handle->in_attr :
			&uci_handle->out_attr;
		mutex_lock(&chan_attr->chan_lock);
		chan_attr->enabled = true;
		mutex_unlock(&chan_attr->chan_lock);
		wake_up(&chan_attr->wq);
		break;
	case MHI_CB_SYS_ERROR:
	case MHI_CB_MHI_SHUTDOWN:
	case MHI_CB_MHI_DISABLED:
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_INFO,
			"MHI disabled CB received 0x%x for chan:%d\n",
			cb_info->cb_reason, cb_info->chan);

		chan_attr = (cb_info->chan % 2) ? &uci_handle->in_attr :
			&uci_handle->out_attr;
		mutex_lock(&chan_attr->chan_lock);
		chan_attr->enabled = false;
		/* we disable entire handler by grabbing only one lock */
		uci_handle->enabled = false;
		mutex_unlock(&chan_attr->chan_lock);
		wake_up(&chan_attr->wq);

		/*
		 * if it's ctrl channel clear the resource now
		 * otherwise during file close we will release the
		 * resources
		 */
		if (uci_handle == uci_handle->uci_ctxt->ctrl_client &&
		    chan_attr == &uci_handle->out_attr) {
			struct uci_buf *itr, *tmp;

			mutex_lock(&chan_attr->chan_lock);
			atomic_set(&uci_handle->out_attr.avail_pkts, 0);
			atomic_set(&uci_handle->out_pkt_pend_ack, 0);
			list_for_each_entry_safe(itr, tmp, &chan_attr->buf_head,
						 node) {
				list_del(&itr->node);
				kfree(itr->data);
			}
			atomic_set(&uci_handle->completion_ack, 0);
			INIT_LIST_HEAD(&uci_handle->out_attr.buf_head);
			mutex_unlock(&chan_attr->chan_lock);
		}
		break;
	case MHI_CB_XFER:
		if (!cb_info->result) {
			uci_log(uci_handle->uci_ipc_log, UCI_DBG_CRITICAL,
				"Failed to obtain mhi result from CB\n");
				return;
		}
		result = cb_info->result;
		if (cb_info->chan % 2)
			parse_inbound_ack(uci_handle, result);
		else
			parse_outbound_ack(uci_handle, result);
		break;
	default:
		uci_log(uci_handle->uci_ipc_log, UCI_DBG_VERBOSE,
			"Cannot handle cb reason 0x%x\n",
			cb_info->cb_reason);
	}
}

static int mhi_register_client(struct uci_client *mhi_client,
			       struct device *dev)
{
	int ret_val = 0;
	struct mhi_client_info_t client_info;

	uci_log(mhi_client->uci_ipc_log, UCI_DBG_INFO,
		"Setting up workqueues\n");
	init_waitqueue_head(&mhi_client->in_attr.wq);
	init_waitqueue_head(&mhi_client->out_attr.wq);

	uci_log(mhi_client->uci_ipc_log, UCI_DBG_INFO,
		"Registering chan %d\n",
		mhi_client->out_attr.chan_id);
	client_info.dev = dev;
	client_info.node_name = "qcom,mhi";
	client_info.user_data = mhi_client;
	client_info.mhi_client_cb = uci_xfer_cb;
	client_info.chan = mhi_client->out_attr.chan_id;
	client_info.max_payload = mhi_client->out_attr.max_packet_size;
	ret_val = mhi_register_channel(&mhi_client->out_attr.mhi_handle,
				       &client_info);
	if (0 != ret_val)
		uci_log(mhi_client->uci_ipc_log,
			UCI_DBG_ERROR,
			"Failed to init outbound chan 0x%x, ret 0x%x\n",
			mhi_client->out_attr.chan_id,
			ret_val);

	uci_log(mhi_client->uci_ipc_log, UCI_DBG_INFO,
		"Registering chan %d\n",
		mhi_client->in_attr.chan_id);
	client_info.max_payload = mhi_client->in_attr.max_packet_size;
	client_info.chan = mhi_client->in_attr.chan_id;
	ret_val = mhi_register_channel(&mhi_client->in_attr.mhi_handle,
				       &client_info);
	if (0 != ret_val)
		uci_log(mhi_client->uci_ipc_log, UCI_DBG_ERROR,
			"Failed to init inbound chan 0x%x, ret 0x%x\n",
			mhi_client->in_attr.chan_id,
			ret_val);
	return 0;
}

static const struct file_operations mhi_uci_client_fops = {
	.read = mhi_uci_client_read,
	.write = mhi_uci_client_write,
	.open = mhi_uci_client_open,
	.release = mhi_uci_client_release,
	.poll = mhi_uci_client_poll,
	.unlocked_ioctl = mhi_uci_ctl_ioctl,
};

static int mhi_uci_create_device(struct uci_client *uci_client)
{
	struct mhi_uci_ctxt_t *uci_ctxt = uci_client->uci_ctxt;
	char node_name[32];
	int index = uci_client - uci_ctxt->client_handles;
	int ret;

	uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_INFO,
		"Creating dev node %04x_%02u.%02u.%02u_pipe%d\n",
		uci_client->out_attr.mhi_handle->dev_id,
		uci_client->out_attr.mhi_handle->domain,
		uci_client->out_attr.mhi_handle->bus,
		uci_client->out_attr.mhi_handle->slot,
		uci_client->out_attr.chan_id);

	cdev_init(&uci_client->cdev, &mhi_uci_client_fops);
	uci_client->cdev.owner = THIS_MODULE;
	ret = cdev_add(&uci_client->cdev, uci_ctxt->dev_t + index, 1);
	if (ret) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_ERROR,
			"Failed to add cdev %d, ret:%d\n", index, ret);
		return ret;
	}
	uci_client->dev = device_create(mhi_uci_drv_ctxt.mhi_uci_class, NULL,
					uci_ctxt->dev_t + index, NULL,
					DEVICE_NAME "_%04x_%02u.%02u.%02u%s%d",
					uci_client->out_attr.mhi_handle->dev_id,
					uci_client->out_attr.mhi_handle->domain,
					uci_client->out_attr.mhi_handle->bus,
					uci_client->out_attr.mhi_handle->slot,
					"_pipe_",
					uci_client->out_attr.chan_id);
	if (IS_ERR(uci_client->dev)) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_ERROR,
			"Failed to add cdev %d\n", IS_ERR(uci_client->dev));
		cdev_del(&uci_client->cdev);
		return -EIO;
	}

	/* dev node created successfully, create logging buffer */
	snprintf(node_name, sizeof(node_name), "mhi_uci_%04x_%02u.%02u.%02u_%d",
		 uci_client->out_attr.mhi_handle->dev_id,
		 uci_client->out_attr.mhi_handle->domain,
		 uci_client->out_attr.mhi_handle->bus,
		 uci_client->out_attr.mhi_handle->slot,
		 uci_client->out_attr.chan_id);
	uci_client->uci_ipc_log = ipc_log_context_create(MHI_UCI_IPC_LOG_PAGES,
							 node_name, 0);

	return 0;
}

static int mhi_uci_probe(struct platform_device *pdev)
{
	struct mhi_uci_ctxt_t *uci_ctxt;
	int ret_val;
	int i;

	uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_INFO, "Entered\n");

	if (mhi_is_device_ready(&pdev->dev, "qcom,mhi") == false)
		return -EPROBE_DEFER;

	if (pdev->dev.of_node == NULL)
		return -ENODEV;

	pdev->id = of_alias_get_id(pdev->dev.of_node, "mhi_uci");
	if (pdev->id < 0)
		return -ENODEV;

	uci_ctxt = devm_kzalloc(&pdev->dev,
				sizeof(*uci_ctxt),
				GFP_KERNEL);
	if (!uci_ctxt)
		return -ENOMEM;

	uci_ctxt->pdev = pdev;
	mutex_init(&uci_ctxt->ctrl_mutex);

	uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_INFO,
		"Setting up channel attributes\n");
	ret_val = uci_init_client_attributes(uci_ctxt,
					     pdev->dev.of_node);
	if (ret_val) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_ERROR,
			"Failed to init client attributes\n");
		return -EIO;
	}

	uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_INFO,
		"Allocating char devices\n");
	ret_val = alloc_chrdev_region(&uci_ctxt->dev_t, 0,
				      MHI_SOFTWARE_CLIENT_LIMIT,
				      DEVICE_NAME);
	if (IS_ERR_VALUE(ret_val)) {
		uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_ERROR,
			"Failed to alloc char devs, ret 0x%x\n", ret_val);
		return ret_val;
	}

	uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log, UCI_DBG_INFO,
		"Registering for MHI events\n");
	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; ++i) {
		struct uci_client *uci_client = &uci_ctxt->client_handles[i];

		uci_client->uci_ctxt = uci_ctxt;
		mutex_init(&uci_client->client_lock);
		if (!uci_client->in_attr.uci_ownership)
			continue;
		ret_val = mhi_register_client(uci_client, &pdev->dev);
		if (ret_val) {
			uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log,
				UCI_DBG_CRITICAL,
				"Failed to reg client %d ret %d\n",
				ret_val, i);
				return -EIO;
		}

		mutex_lock(&uci_client->client_lock);
		/* If we have device id, create the node now */
		if (uci_client->out_attr.mhi_handle->dev_id != PCI_ANY_ID) {
			ret_val = mhi_uci_create_device(uci_client);
			if (ret_val) {
				uci_log(mhi_uci_drv_ctxt.mhi_uci_ipc_log,
					UCI_DBG_CRITICAL,
					"Failed to create device node, ret:%d\n",
					ret_val);
				mutex_unlock(&uci_client->client_lock);
				return -EIO;
			}
		}
		mutex_unlock(&uci_client->client_lock);
	}

	platform_set_drvdata(pdev, uci_ctxt);
	mutex_lock(&mhi_uci_drv_ctxt.list_lock);
	list_add_tail(&uci_ctxt->node, &mhi_uci_drv_ctxt.head);
	mutex_unlock(&mhi_uci_drv_ctxt.list_lock);

	return 0;
};

static int mhi_uci_remove(struct platform_device *pdev)
{
	struct mhi_uci_ctxt_t *uci_ctxt = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; ++i) {
		struct uci_client *uci_client = &uci_ctxt->client_handles[i];

		uci_client->uci_ctxt = uci_ctxt;
		if (uci_client->in_attr.uci_ownership) {
			mhi_deregister_channel(uci_client->out_attr.mhi_handle);
			mhi_deregister_channel(uci_client->in_attr.mhi_handle);
			cdev_del(&uci_client->cdev);
			device_destroy(mhi_uci_drv_ctxt.mhi_uci_class,
				       MKDEV(MAJOR(uci_ctxt->dev_t), i));
		}
	}

	unregister_chrdev_region(MAJOR(uci_ctxt->dev_t),
				 MHI_SOFTWARE_CLIENT_LIMIT);

	mutex_lock(&mhi_uci_drv_ctxt.list_lock);
	list_del(&uci_ctxt->node);
	mutex_unlock(&mhi_uci_drv_ctxt.list_lock);
	return 0;
};

static const struct of_device_id mhi_uci_match_table[] = {
	{.compatible = "qcom,mhi-uci"},
	{},
};

static struct platform_driver mhi_uci_driver = {
	.probe = mhi_uci_probe,
	.remove = mhi_uci_remove,
	.driver = {
		.name = MHI_UCI_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mhi_uci_match_table,
	},
};

static int mhi_uci_init(void)
{
	mhi_uci_drv_ctxt.mhi_uci_ipc_log =
		ipc_log_context_create(MHI_UCI_IPC_LOG_PAGES,
				       "mhi-uci",
				       0);
	if (mhi_uci_drv_ctxt.mhi_uci_ipc_log == NULL) {
		uci_log(NULL,
			UCI_DBG_WARNING,
			"Failed to create IPC logging context");
	}

	mhi_uci_drv_ctxt.mhi_uci_class =
		class_create(THIS_MODULE, DEVICE_NAME);

	if (IS_ERR(mhi_uci_drv_ctxt.mhi_uci_class))
		return -ENODEV;

	mutex_init(&mhi_uci_drv_ctxt.list_lock);
	INIT_LIST_HEAD(&mhi_uci_drv_ctxt.head);

	return platform_driver_register(&mhi_uci_driver);
}

static void __exit mhi_uci_exit(void)
{
	class_destroy(mhi_uci_drv_ctxt.mhi_uci_class);
	platform_driver_unregister(&mhi_uci_driver);
}

module_exit(mhi_uci_exit);
module_init(mhi_uci_init);
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("MHI_UCI");
MODULE_DESCRIPTION("MHI UCI Driver");
