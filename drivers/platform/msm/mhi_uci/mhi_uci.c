/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/dma-mapping.h>

#define MHI_DEV_NODE_NAME_LEN 13
#define MHI_MAX_NR_OF_CLIENTS 23
#define MHI_SOFTWARE_CLIENT_START 0
#define MHI_SOFTWARE_CLIENT_LIMIT 23
#define MHI_MAX_SOFTWARE_CHANNELS 46
#define TRB_MAX_DATA_SIZE 0x1000
#define MHI_UCI_IPC_LOG_PAGES (100)

#define MAX_NR_TRBS_PER_CHAN 10
#define DEVICE_NAME "mhi"
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
enum UCI_DBG_LEVEL mhi_uci_ipc_log_lvl = UCI_DBG_INFO;
void *mhi_uci_ipc_log;

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
};

struct uci_client {
	u32 client_index;
	u32 out_chan;
	u32 in_chan;
	struct mhi_client_handle *out_handle;
	struct mhi_client_handle *in_handle;
	size_t pending_data;
	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	atomic_t avail_pkts;
	struct device *dev;
	u32 dev_node_enabled;
	u8 local_tiocm;
	atomic_t ref_count;
	int mhi_status;
	void *pkt_loc;
	size_t pkt_size;
	dma_addr_t *in_buf_list;
	atomic_t out_pkt_pend_ack;
	atomic_t mhi_chans_open;
	struct mhi_uci_ctxt_t *uci_ctxt;
	struct mutex in_chan_lock;
	struct mutex out_chan_lock;
};

struct mhi_uci_ctxt_t {
	struct chan_attr chan_attrib[MHI_MAX_SOFTWARE_CHANNELS];
	struct uci_client client_handles[MHI_SOFTWARE_CLIENT_LIMIT];
	struct mhi_client_info_t client_info;
	dev_t start_ctrl_nr;
	struct mhi_client_handle *ctrl_handle;
	struct mutex ctrl_mutex;
	struct cdev cdev[MHI_MAX_SOFTWARE_CHANNELS];
	struct class *mhi_uci_class;
	u32 ctrl_chan_id;
	atomic_t mhi_disabled;
	atomic_t mhi_enable_notif_wq_active;
	struct work_struct mhi_enabled_work;
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

#define uci_log(_msg_lvl, _msg, ...) do { \
	if (_msg_lvl >= mhi_uci_msg_lvl) { \
		pr_err("[%s] "_msg, __func__, ##__VA_ARGS__); \
	} \
	if (mhi_uci_ipc_log && (_msg_lvl >= mhi_uci_ipc_log_lvl)) { \
		ipc_log_string(mhi_uci_ipc_log,                     \
			"[%s] " _msg, __func__, ##__VA_ARGS__);     \
	} \
} while (0)

module_param(mhi_uci_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_uci_msg_lvl, "uci dbg lvl");

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

static struct mhi_uci_ctxt_t uci_ctxt;

static enum MHI_STATUS mhi_init_inbound(struct uci_client *client_handle,
		enum MHI_CLIENT_CHANNEL chan)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	u32 i = 0;
	dma_addr_t dma_addr = 0;
	struct chan_attr *chan_attributes =
		&uci_ctxt.chan_attrib[chan];
	void *data_loc = NULL;
	size_t buf_size = chan_attributes->max_packet_size;

	if (client_handle == NULL) {
		uci_log(UCI_DBG_ERROR, "Bad Input data, quitting\n");
		return MHI_STATUS_ERROR;
	}
	for (i = 0; i < (chan_attributes->nr_trbs); ++i) {
		data_loc = kmalloc(buf_size, GFP_KERNEL);
		if (data_loc == NULL)
			return -ENOMEM;
		dma_addr = dma_map_single(NULL, data_loc,
					buf_size, DMA_BIDIRECTIONAL);
		if (dma_mapping_error(NULL, dma_addr)) {
			uci_log(UCI_DBG_ERROR, "Failed to Map DMA\n");
			return -ENOMEM;
		}
		client_handle->in_buf_list[i] = dma_addr;
		ret_val = mhi_queue_xfer(client_handle->in_handle,
					  dma_addr, buf_size, MHI_EOT);
		if (MHI_STATUS_SUCCESS != ret_val) {
			dma_unmap_single(NULL, dma_addr,
					 buf_size, DMA_BIDIRECTIONAL);
			kfree(data_loc);
			goto error_insert;
		}
	}
	return ret_val;
error_insert:
	uci_log(UCI_DBG_ERROR,
			"Failed insertion for chan %d\n", chan);
	return MHI_STATUS_ERROR;
}

static int mhi_uci_send_packet(struct mhi_client_handle **client_handle,
		void *buf, u32 size, u32 is_uspace_buf)
{
	u32 nr_avail_trbs = 0;
	u32 i = 0;
	void *data_loc = NULL;
	uintptr_t memcpy_result = 0;
	int data_left_to_insert = 0;
	size_t data_to_insert_now = 0;
	u32 data_inserted_so_far = 0;
	dma_addr_t dma_addr = 0;
	int ret_val = 0;
	enum MHI_FLAGS flags;
	struct uci_client *uci_handle;
	uci_handle = container_of(client_handle, struct uci_client,
					out_handle);

	if (client_handle == NULL || buf == NULL ||
		!size || uci_handle == NULL)
		return MHI_STATUS_ERROR;

	nr_avail_trbs = mhi_get_free_desc(*client_handle);

	data_left_to_insert = size;

	if (0 == nr_avail_trbs)
		return 0;

	for (i = 0; i < nr_avail_trbs; ++i) {
		data_to_insert_now = min(data_left_to_insert,
				TRB_MAX_DATA_SIZE);

		if (is_uspace_buf) {
			data_loc = kmalloc(data_to_insert_now, GFP_KERNEL);
			if (NULL == data_loc) {
				uci_log(UCI_DBG_ERROR,
					"Failed to allocate memory 0x%x\n",
					data_to_insert_now);
				return -ENOMEM;
			}
			memcpy_result = copy_from_user(data_loc,
					buf + data_inserted_so_far,
					data_to_insert_now);

			if (0 != memcpy_result)
				goto error_memcpy;
		} else {
			data_loc = buf;
		}

		dma_addr = dma_map_single(NULL, data_loc,
					data_to_insert_now, DMA_TO_DEVICE);
		if (dma_mapping_error(NULL, dma_addr)) {
			uci_log(UCI_DBG_ERROR,
					"Failed to Map DMA 0x%x\n", size);
			data_inserted_so_far = -ENOMEM;
			goto error_memcpy;
		}

		flags = MHI_EOT;
		if (data_left_to_insert - data_to_insert_now > 0)
			flags |= MHI_CHAIN | MHI_EOB;
		uci_log(UCI_DBG_VERBOSE,
			    "At trb i = %d/%d, chain = %d, eob = %d, addr 0x%lx chan %d\n",
				i, nr_avail_trbs,
				flags & MHI_CHAIN,
				flags & MHI_EOB,
				(uintptr_t)dma_addr,
				uci_handle->out_chan);
		ret_val = mhi_queue_xfer(*client_handle, dma_addr,
					data_to_insert_now, flags);
		if (0 != ret_val) {
			goto error_queue;
		} else {
			data_left_to_insert -= data_to_insert_now;
			data_inserted_so_far += data_to_insert_now;
			atomic_inc(&uci_handle->out_pkt_pend_ack);
		}
		if (0 == data_left_to_insert)
			break;
	}
	return data_inserted_so_far;

error_queue:
	dma_unmap_single(NULL,
		(dma_addr_t)dma_addr,
		data_to_insert_now,
		DMA_TO_DEVICE);
error_memcpy:
	kfree(data_loc);
	return data_inserted_so_far;
}

static int mhi_uci_send_status_cmd(struct uci_client *client)
{
	struct rs232_ctrl_msg *rs232_pkt = NULL;
	struct uci_client *uci_handle;
	u32 ctrl_chan_id = uci_ctxt.ctrl_chan_id;

	int ret_val = 0;
	size_t pkt_size = sizeof(struct rs232_ctrl_msg);
	u32 amount_sent;
	rs232_pkt = kzalloc(sizeof(struct rs232_ctrl_msg), GFP_KERNEL);
	if (rs232_pkt == NULL)
		return -ENOMEM;
	uci_log(UCI_DBG_VERBOSE,
		"Received request to send msg for chan %d\n",
		client->out_chan);
	rs232_pkt->preamble = CTRL_MAGIC;
	if (client->local_tiocm & TIOCM_DTR)
		MHI_SET_CTRL_MSG(CTRL_MSG_DTR, rs232_pkt, 1);
	if (client->local_tiocm & TIOCM_RTS)
		MHI_SET_CTRL_MSG(CTRL_MSG_RTS, rs232_pkt, 1);

	MHI_SET_CTRL_MSG_ID(CTRL_MSG_ID, rs232_pkt, MHI_CTRL_LINE_STATE_ID);
	MHI_SET_CTRL_MSG_SIZE(CTRL_MSG_SIZE, rs232_pkt, sizeof(u32));
	MHI_SET_CTRL_DEST_ID(CTRL_DEST_ID, rs232_pkt, client->out_chan);

	if (MHI_STATUS_SUCCESS != ret_val) {
		uci_log(UCI_DBG_CRITICAL,
			"Could not open chan %d, for sideband ctrl\n",
			client->out_chan);
		goto error;
	}

	uci_handle = &uci_ctxt.client_handles[ctrl_chan_id/2];

	amount_sent = mhi_uci_send_packet(&uci_handle->out_handle,
						rs232_pkt,
						pkt_size, 0);
	if (pkt_size != amount_sent) {
		uci_log(UCI_DBG_INFO,
			"Failed to send signal on chan %d, ret : %d n",
			client->out_chan, ret_val);
		goto error;
	}
	return ret_val;
error:
	kfree(rs232_pkt);
	return ret_val;
}

static int mhi_uci_tiocm_set(struct uci_client *client_ctxt, u32 set, u32 clear)
{
	u8 status_set = 0;
	u8 status_clear = 0;
	u8 old_status = 0;

	mutex_lock(&client_ctxt->uci_ctxt->ctrl_mutex);

	status_set |= (set & TIOCM_DTR) ? TIOCM_DTR : 0;
	status_clear |= (clear & TIOCM_DTR) ? TIOCM_DTR : 0;
	old_status = client_ctxt->local_tiocm;
	client_ctxt->local_tiocm |= status_set;
	client_ctxt->local_tiocm &= ~status_clear;

	uci_log(UCI_DBG_VERBOSE,
		"Old TIOCM0x%x for chan %d, Current TIOCM 0x%x\n",
		old_status, client_ctxt->out_chan, client_ctxt->local_tiocm);
	mutex_unlock(&client_ctxt->uci_ctxt->ctrl_mutex);

	if (client_ctxt->local_tiocm != old_status) {
		uci_log(UCI_DBG_VERBOSE,
			"Setting TIOCM to 0x%x for chan %d\n",
			client_ctxt->local_tiocm, client_ctxt->out_chan);
		return mhi_uci_send_status_cmd(client_ctxt);
	}
	return 0;
}

static long mhi_uci_ctl_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int ret_val = 0;
	u32 set_val;
	struct uci_client *uci_handle = NULL;
	uci_handle = file->private_data;

	if (uci_handle == NULL) {
		uci_log(UCI_DBG_VERBOSE,
			"Invalid handle for client.\n");
		return -ENODEV;
	}
	uci_log(UCI_DBG_VERBOSE,
			"Attempting to dtr cmd 0x%x arg 0x%lx for chan %d\n",
			cmd, arg, uci_handle->out_chan);

	switch (cmd) {
	case TIOCMGET:
		uci_log(UCI_DBG_VERBOSE,
			"Returning 0x%x mask\n", uci_handle->local_tiocm);
		ret_val = uci_handle->local_tiocm;
		break;
	case TIOCMSET:
		if (0 != copy_from_user(&set_val, (void *)arg, sizeof(set_val)))
			return -ENOMEM;
		uci_log(UCI_DBG_VERBOSE,
			"Attempting to set cmd 0x%x arg 0x%x for chan %d\n",
			cmd, set_val, uci_handle->out_chan);
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
	struct uci_client *uci_handle = NULL;
	uci_handle = file->private_data;

	if (uci_handle == NULL)
		return -ENODEV;
	poll_wait(file, &uci_handle->read_wq, wait);
	poll_wait(file, &uci_handle->write_wq, wait);
	if (atomic_read(&uci_handle->avail_pkts) > 0) {
		uci_log(UCI_DBG_VERBOSE,
		"Client can read chan %d\n", uci_handle->in_chan);
		mask |= POLLIN | POLLRDNORM;
	}
	if (!atomic_read(&uci_ctxt.mhi_disabled) &&
	    (mhi_get_free_desc(uci_handle->out_handle) > 0)) {
		uci_log(UCI_DBG_VERBOSE,
		"Client can write chan %d\n", uci_handle->out_chan);
		mask |= POLLOUT | POLLWRNORM;
	}

	uci_log(UCI_DBG_VERBOSE,
		"Client attempted to poll chan %d, returning mask 0x%x\n",
		uci_handle->in_chan, mask);
	return mask;
}

static int open_client_mhi_channels(struct uci_client *uci_client)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	int r = 0;
	uci_log(UCI_DBG_INFO,
			"Starting channels %d %d.\n",
			uci_client->out_chan,
			uci_client->in_chan);
	mutex_lock(&uci_client->out_chan_lock);
	mutex_lock(&uci_client->in_chan_lock);
	if (atomic_read(&uci_client->mhi_chans_open))
		goto handle_not_rdy_err;
	ret_val = mhi_open_channel(uci_client->out_handle);
	if (ret_val != MHI_STATUS_SUCCESS) {
		if (ret_val == MHI_STATUS_DEVICE_NOT_READY)
			r = -EAGAIN;
		else
			r = -EIO;
		goto handle_not_rdy_err;
	}

	ret_val = mhi_open_channel(uci_client->in_handle);
	if (ret_val != MHI_STATUS_SUCCESS) {
		uci_log(UCI_DBG_ERROR,
				 "Failed to open chan %d, ret 0x%x\n",
		   uci_client->out_chan, ret_val);
		goto handle_in_err;
	}
	uci_log(UCI_DBG_INFO,
			"Initializing inbound chan %d.\n",
			uci_client->in_chan);

	ret_val = mhi_init_inbound(uci_client, uci_client->in_chan);
	if (MHI_STATUS_SUCCESS != ret_val) {
		uci_log(UCI_DBG_ERROR,
			   "Failed to init inbound 0x%x, ret 0x%x\n",
			   uci_client->in_chan, ret_val);
	}

	atomic_set(&uci_client->mhi_chans_open, 1);
	mutex_unlock(&uci_client->in_chan_lock);
	mutex_unlock(&uci_client->out_chan_lock);
	return 0;

handle_in_err:
	mhi_close_channel(uci_client->out_handle);
handle_not_rdy_err:
	mutex_unlock(&uci_client->in_chan_lock);
	mutex_unlock(&uci_client->out_chan_lock);
	return r;
}

static int mhi_uci_client_open(struct inode *mhi_inode,
				struct file *file_handle)
{
	struct uci_client *uci_handle = NULL;
	int r = 0;
	uci_handle =
		&uci_ctxt.client_handles[iminor(mhi_inode)];

	uci_log(UCI_DBG_INFO,
		"Client opened struct device node 0x%x, ref count 0x%x\n",
		iminor(mhi_inode), atomic_read(&uci_handle->ref_count));
	if (atomic_add_return(1, &uci_handle->ref_count) == 1) {
		if (!uci_handle->dev_node_enabled) {
			r = -EPERM;
			goto handle_alloc_err;
		}
		if (uci_handle == NULL) {
			r = -ENOMEM;
			goto handle_alloc_err;
		}
		uci_handle->uci_ctxt = &uci_ctxt;
		if (!atomic_read(&uci_handle->mhi_chans_open)) {
			uci_log(UCI_DBG_INFO,
				"Opening channels client %d\n",
				iminor(mhi_inode));
			r = open_client_mhi_channels(uci_handle);
			if (r) {
				uci_log(UCI_DBG_INFO,
					"Failed to open channels ret %d\n", r);
			}
			while (!atomic_read(&uci_handle->mhi_chans_open)) {
				msleep(50);
				uci_log(UCI_DBG_INFO,
				"Waiting for channels to open client %d\n",
				iminor(mhi_inode));
			}
		}
	}
	file_handle->private_data = uci_handle;
	return r;

handle_alloc_err:
	msleep(50);
	atomic_dec(&uci_handle->ref_count);
	return r;
}

static int mhi_uci_client_release(struct inode *mhi_inode,
		struct file *file_handle)
{
	struct uci_client *uci_handle = file_handle->private_data;
	struct mhi_uci_ctxt_t *uci_ctxt = uci_handle->uci_ctxt;
	u32 retry_cnt = 100;
	u32 nr_in_bufs = 0;
	int in_chan = 0;
	int i = 0;
	u32 buf_size = 0;
	in_chan = iminor(mhi_inode) + 1;
	nr_in_bufs = uci_ctxt->chan_attrib[in_chan].nr_trbs;
	buf_size = uci_ctxt->chan_attrib[in_chan].max_packet_size;

	if (uci_handle == NULL)
		return -EINVAL;
	if (atomic_sub_return(1, &uci_handle->ref_count) == 0) {
		uci_log(UCI_DBG_ERROR,
				"Last client left, closing channel 0x%x\n",
				iminor(mhi_inode));
		while ((--retry_cnt > 0) &&
			(0 != atomic_read(&uci_handle->out_pkt_pend_ack))) {
			uci_log(UCI_DBG_CRITICAL,
				"Still waiting on %d acks!, chan %d\n",
				atomic_read(&uci_handle->out_pkt_pend_ack),
				iminor(mhi_inode));
			usleep_range(10000, 15000);
		}
		if (atomic_read(&uci_handle->mhi_chans_open)) {
			atomic_set(&uci_handle->mhi_chans_open, 0);
			mhi_close_channel(uci_handle->out_handle);
			mhi_close_channel(uci_handle->in_handle);
			for (i = 0; i < nr_in_bufs; ++i) {
				dma_unmap_single(NULL,
						uci_handle->in_buf_list[i],
						buf_size,
						DMA_BIDIRECTIONAL);
				kfree(dma_to_virt(NULL,
						uci_handle->in_buf_list[i]));
			}
		}
		atomic_set(&uci_handle->avail_pkts, 0);
	} else {
		uci_log(UCI_DBG_ERROR,
			"Client close chan %d, ref count 0x%x\n",
			iminor(mhi_inode),
			atomic_read(&uci_handle->ref_count));
	}
	return 0;
}

static ssize_t mhi_uci_client_read(struct file *file, char __user *buf,
		size_t uspace_buf_size, loff_t *bytes_pending)
{
	struct uci_client *uci_handle = NULL;
	uintptr_t phy_buf = 0;
	struct mhi_client_handle *client_handle = NULL;
	int ret_val = 0;
	size_t buf_size = 0;
	struct mutex *mutex;
	u32 chan = 0;
	ssize_t bytes_copied = 0;
	u32 addr_offset = 0;
	struct mhi_result result;

	if (file == NULL || buf == NULL ||
	    uspace_buf_size == 0 || file->private_data == NULL)
		return -EINVAL;

	uci_handle = file->private_data;
	client_handle = uci_handle->in_handle;
	mutex = &uci_handle->in_chan_lock;
	chan = uci_handle->in_chan;
	mutex_lock(mutex);
	buf_size = uci_ctxt.chan_attrib[chan].max_packet_size;

	uci_log(UCI_DBG_VERBOSE,
		"Client attempted read on chan %d\n", chan);
	do {
		if (!uci_handle->pkt_loc &&
				!atomic_read(&uci_ctxt.mhi_disabled)) {
			ret_val = mhi_poll_inbound(client_handle, &result);
			if (ret_val) {
				uci_log(UCI_DBG_ERROR,
				"Failed to poll inbound ret %d avail pkt %d\n",
				ret_val, atomic_read(&uci_handle->avail_pkts));
			}
			phy_buf = result.payload_buf;
			if (phy_buf != 0)
				uci_handle->pkt_loc = dma_to_virt(NULL,
								phy_buf);
			else
				uci_handle->pkt_loc = 0;
			uci_handle->pkt_size = result.bytes_xferd;
			*bytes_pending = uci_handle->pkt_size;
			uci_log(UCI_DBG_VERBOSE,
				"Got pkt of size 0x%x at addr 0x%lx, chan %d\n",
				uci_handle->pkt_size, (uintptr_t)phy_buf, chan);
			dma_unmap_single(NULL, (dma_addr_t)phy_buf,
					 uci_handle->pkt_size,
					 DMA_BIDIRECTIONAL);
		}
		if ((*bytes_pending == 0 || uci_handle->pkt_loc == 0) &&
				(atomic_read(&uci_handle->avail_pkts) <= 0)) {
			/* If nothing was copied yet, wait for data */
			uci_log(UCI_DBG_VERBOSE,
					"No data avail_pkts %d, chan %d\n",
					atomic_read(&uci_handle->avail_pkts),
					chan);
			ret_val = wait_event_interruptible(
				uci_handle->read_wq,
				(atomic_read(&uci_handle->avail_pkts) > 0));
			if (ret_val == -ERESTARTSYS) {
				uci_log(UCI_DBG_ERROR,
					"Exit signal caught\n");
				goto error;
			}
		/* A pending reset exists */
		} else if ((atomic_read(&uci_handle->avail_pkts) > 0) &&
			    0 == uci_handle->pkt_size &&
			    0 == uci_handle->pkt_loc &&
			    uci_handle->mhi_status == -ENETRESET) {
			uci_log(UCI_DBG_ERROR,
				"Detected pending reset, reporting chan %d\n",
				chan);
			atomic_dec(&uci_handle->avail_pkts);
			uci_handle->mhi_status = 0;
			mutex_unlock(mutex);
			return -ENETRESET;
			/* A valid packet was returned from MHI */
		} else if (atomic_read(&uci_handle->avail_pkts) &&
			   uci_handle->pkt_size != 0 &&
			   uci_handle->pkt_loc != 0) {
			uci_log(UCI_DBG_VERBOSE,
			"Got packet: avail pkts %d phy_adr 0x%lx, chan %d\n",
					atomic_read(&uci_handle->avail_pkts),
					phy_buf,
					chan);
			break;
			/*
			 * MHI did not return a valid packet, but we have one
			 * which we did not finish returning to user
			 */
		} else {
			uci_log(UCI_DBG_CRITICAL,
			"chan %d err: avail pkts %d phy_adr 0x%lx mhi_stat%d\n",
					chan,
					atomic_read(&uci_handle->avail_pkts),
					phy_buf,
					uci_handle->mhi_status);
			return -EIO;
		}
	} while (!uci_handle->pkt_loc);

	if (uspace_buf_size >= *bytes_pending) {
		addr_offset = uci_handle->pkt_size - *bytes_pending;
		if (0 != copy_to_user(buf,
				     uci_handle->pkt_loc + addr_offset,
				     *bytes_pending)) {
			ret_val = -EIO;
			goto error;
		}

		bytes_copied = *bytes_pending;
		*bytes_pending = 0;
		uci_log(UCI_DBG_VERBOSE,
				"Copied 0x%x of 0x%x, chan %d\n",
				bytes_copied, (u32)*bytes_pending, chan);
	} else {
		addr_offset = uci_handle->pkt_size - *bytes_pending;
		if (copy_to_user(buf,
				     (void *)(uintptr_t)uci_handle->pkt_loc +
				     addr_offset,
				     uspace_buf_size) != 0) {
			ret_val = -EIO;
			goto error;
		}
		bytes_copied = uspace_buf_size;
		*bytes_pending -= uspace_buf_size;
		uci_log(UCI_DBG_VERBOSE,
				"Copied 0x%x of 0x%x,chan %d\n",
				bytes_copied,
				(u32)*bytes_pending,
				chan);
	}
	/* We finished with this buffer, map it back */
	if (*bytes_pending == 0) {
		uci_log(UCI_DBG_VERBOSE, "Pkt loc %p ,chan %d\n",
					uci_handle->pkt_loc, chan);
		memset(uci_handle->pkt_loc, 0, buf_size);
		phy_buf = dma_map_single(NULL, uci_handle->pkt_loc,
				buf_size, DMA_BIDIRECTIONAL);
		atomic_dec(&uci_handle->avail_pkts);
		uci_log(UCI_DBG_VERBOSE,
				"Decremented avail pkts avail 0x%x\n",
				atomic_read(&uci_handle->avail_pkts));

		ret_val = mhi_queue_xfer(client_handle, phy_buf,
					 buf_size, MHI_EOT);

		if (MHI_STATUS_SUCCESS != ret_val) {
			uci_log(UCI_DBG_ERROR,
					"Failed to recycle element\n");
			ret_val = -EIO;
			goto error;
		}
		uci_handle->pkt_loc = 0;
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
	else
		uci_handle = file->private_data;
	if (atomic_read(&uci_ctxt.mhi_disabled) &&
		uci_handle->out_chan != 10 && uci_handle->out_chan != 2) {
		uci_log(UCI_DBG_ERROR,
			"Client %d attempted to write while MHI is disabled\n",
			uci_handle->out_chan);
		return -EIO;
	}
	chan = uci_handle->out_chan;
	mutex_lock(&uci_handle->out_chan_lock);
	while (ret_val == 0) {
		ret_val = mhi_uci_send_packet(&uci_handle->out_handle,
				(void *)buf, count, 1);
		if (!ret_val) {
			uci_log(UCI_DBG_VERBOSE,
			"No descriptors available, did we poll, chan %d?\n",
			chan);
			mutex_unlock(&uci_handle->out_chan_lock);
			ret_val =
				wait_event_interruptible(
						uci_handle->write_wq,
			mhi_get_free_desc(uci_handle->out_handle) > 0);
			mutex_lock(&uci_handle->out_chan_lock);
			if (-ERESTARTSYS == ret_val) {
				goto sys_interrupt;
				uci_log(UCI_DBG_WARNING,
					    "Waitqueue cancelled by system\n");
			}
		}
	}
sys_interrupt:
	mutex_unlock(&uci_handle->out_chan_lock);
	return ret_val;
}

static enum MHI_STATUS uci_init_client_attributes(struct mhi_uci_ctxt_t
								*uci_ctxt)
{
	u32 i = 0;
	u32 data_size = TRB_MAX_DATA_SIZE;
	u32 index = 0;
	struct uci_client *client;
	struct chan_attr *chan_attrib = NULL;

	for (i = 0; i < MHI_MAX_SOFTWARE_CHANNELS; ++i) {
		chan_attrib = &uci_ctxt->chan_attrib[i];
		switch (i) {
		case MHI_CLIENT_LOOPBACK_OUT:
		case MHI_CLIENT_LOOPBACK_IN:
		case MHI_CLIENT_SAHARA_OUT:
		case MHI_CLIENT_SAHARA_IN:
		case MHI_CLIENT_EFS_OUT:
		case MHI_CLIENT_EFS_IN:
		case MHI_CLIENT_QMI_OUT:
		case MHI_CLIENT_QMI_IN:
		case MHI_CLIENT_IP_CTRL_0_OUT:
		case MHI_CLIENT_IP_CTRL_0_IN:
		case MHI_CLIENT_IP_CTRL_1_OUT:
		case MHI_CLIENT_IP_CTRL_1_IN:
		case MHI_CLIENT_DUN_OUT:
		case MHI_CLIENT_DUN_IN:
			chan_attrib->uci_ownership = 1;
			break;
		default:
			chan_attrib->uci_ownership = 0;
			break;
		}
		if (chan_attrib->uci_ownership) {
			chan_attrib->chan_id = i;
			chan_attrib->max_packet_size = data_size;
			index = CHAN_TO_CLIENT(i);
			client = &uci_ctxt->client_handles[index];
			chan_attrib->nr_trbs = 9;
			client->in_buf_list =
			      kmalloc(sizeof(dma_addr_t) * chan_attrib->nr_trbs,
					GFP_KERNEL);
			if (NULL == client->in_buf_list)
				return MHI_STATUS_ERROR;
		}
		if (i % 2 == 0)
			chan_attrib->dir = MHI_DIR_OUT;
		else
			chan_attrib->dir = MHI_DIR_IN;
	}
	return MHI_STATUS_SUCCESS;
}

static void process_mhi_enabled_notif(struct work_struct *work)
{
	int i = 0;
	int r = 0;
	struct uci_client *uci_handle = NULL;

	uci_log(UCI_DBG_VERBOSE, "Entered.\n");
	for (i = 0; i < MHI_MAX_NR_OF_CLIENTS; ++i) {
		if (uci_ctxt.chan_attrib[i*2].uci_ownership) {
			uci_handle =
				&uci_ctxt.client_handles[i];
			if (!atomic_read(&uci_handle->mhi_chans_open) &&
			     atomic_read(&uci_handle->ref_count)) {
				r = open_client_mhi_channels(uci_handle);
				if (r) {
					uci_log(UCI_DBG_INFO,
					"Error: open chans client %d ret %d.\n",
					i, r);
				}
			}
		}
	}
	atomic_set(&uci_ctxt.mhi_disabled, 0);
	uci_log(UCI_DBG_VERBOSE, "Exited.\n");
	atomic_set(&uci_ctxt.mhi_enable_notif_wq_active, 0);
}

static int process_mhi_disabled_notif_sync(struct uci_client *uci_handle)
{
	int i = 0;

	uci_log(UCI_DBG_INFO, "Entered.\n");
	if (uci_handle->mhi_status != -ENETRESET) {
		uci_log(UCI_DBG_CRITICAL,
		"Setting reset for chan %d.\n",
		i * 2);
		uci_handle->pkt_size = 0;
		uci_handle->pkt_loc = NULL;
		uci_handle->mhi_status = -ENETRESET;
		atomic_set(&uci_handle->avail_pkts, 1);
		atomic_set(&uci_handle->mhi_chans_open, 0);
		mhi_close_channel(uci_handle->out_handle);
		mhi_close_channel(uci_handle->in_handle);
		wake_up(&uci_handle->read_wq);
	} else {
		uci_log(UCI_DBG_CRITICAL,
			"Chan %d state already reset.\n",
			i*2);
	}
	uci_log(UCI_DBG_INFO, "Exited.\n");
	return 0;
}

static void process_rs232_state(struct mhi_result *result)
{
	struct rs232_ctrl_msg *rs232_pkt;
	struct uci_client *client;
	u32 msg_id;
	enum MHI_STATUS ret_val;
	u32 chan;

	mutex_lock(&uci_ctxt.ctrl_mutex);
	if (result->transaction_status != MHI_STATUS_SUCCESS) {
		uci_log(UCI_DBG_ERROR,
			"Non successful transfer code 0x%x\n",
			 result->transaction_status);
		goto error_bad_xfer;
	}
	if (result->bytes_xferd != sizeof(struct rs232_ctrl_msg)) {
		uci_log(UCI_DBG_ERROR,
		"Buffer is of wrong size is: 0x%x: expected 0x%x\n",
		result->bytes_xferd, sizeof(struct rs232_ctrl_msg));
		goto error_size;
	}
	dma_unmap_single(NULL, result->payload_buf,
			result->bytes_xferd, DMA_BIDIRECTIONAL);
	rs232_pkt = dma_to_virt(NULL, result->payload_buf);
	MHI_GET_CTRL_DEST_ID(CTRL_DEST_ID, rs232_pkt, chan);
	client = &uci_ctxt.client_handles[chan / 2];


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
	dma_map_single(NULL, rs232_pkt,
			sizeof(struct rs232_ctrl_msg),
			DMA_BIDIRECTIONAL);
	ret_val = mhi_queue_xfer(client->in_handle,
			result->payload_buf,
			result->bytes_xferd,
			result->flags);
	if (MHI_STATUS_SUCCESS != ret_val) {
		uci_log(UCI_DBG_ERROR,
		"Failed to recycle ctrl msg buffer\n");
	}
	mutex_unlock(&uci_ctxt.ctrl_mutex);
}

static void parse_inbound_ack(struct uci_client *uci_handle,
			struct mhi_result *result)
{
	atomic_inc(&uci_handle->avail_pkts);
	uci_log(UCI_DBG_VERBOSE,
		"Received cb on chan %d, avail pkts: 0x%x\n",
		uci_handle->in_chan,
		atomic_read(&uci_handle->avail_pkts));
	wake_up(&uci_handle->read_wq);
	if (uci_handle->in_chan == MHI_CLIENT_IP_CTRL_1_IN)
		process_rs232_state(result);
}

static void parse_outbound_ack(struct uci_client *uci_handle,
			struct mhi_result *result)
{
	dma_unmap_single(NULL,
			result->payload_buf,
			result->bytes_xferd,
			DMA_TO_DEVICE);
	kfree(dma_to_virt(NULL,
			result->payload_buf));
	uci_log(UCI_DBG_VERBOSE,
		"Received ack on chan %d, pending acks: 0x%x\n",
		uci_handle->out_chan,
		atomic_read(&uci_handle->out_pkt_pend_ack));
	atomic_dec(&uci_handle->out_pkt_pend_ack);
	if (mhi_get_free_desc(uci_handle->out_handle))
		wake_up(&uci_handle->write_wq);
}

static void uci_xfer_cb(struct mhi_cb_info *cb_info)
{
	u32 chan_nr;
	struct uci_client *uci_handle = NULL;
	u32 client_index;
	struct mhi_result *result;
	int r = 0;
	if (NULL == cb_info)
		uci_log(UCI_DBG_CRITICAL, "Bad CB info from MHI.\n");
	if (NULL != cb_info->result) {
		chan_nr = (u32)cb_info->result->user_data;
		client_index = CHAN_TO_CLIENT(chan_nr);
		uci_handle =
			&uci_ctxt.client_handles[client_index];
	}
	switch (cb_info->cb_reason) {
	case MHI_CB_MHI_ENABLED:
		if (atomic_read(&uci_ctxt.mhi_enable_notif_wq_active)) {
			return;
		} else {
			uci_log(UCI_DBG_INFO,
				"MHI enabled CB received spawning wq.\n");
		}
		r = schedule_work(&uci_ctxt.mhi_enabled_work);
		if (r) {
			uci_log(UCI_DBG_INFO,
				"Failed to spawn wq chan %d, ret %d.\n",
				chan_nr, r);
		} else {
			atomic_set(&uci_ctxt.mhi_enable_notif_wq_active, 1);
		}
		break;
	case MHI_CB_MHI_DISABLED:
		atomic_set(&uci_ctxt.mhi_disabled, 1);
		uci_log(UCI_DBG_INFO, "MHI disabled CB received.\n");
		process_mhi_disabled_notif_sync(uci_handle);
		break;
	case MHI_CB_XFER:
		if (cb_info->result == NULL) {
			uci_log(UCI_DBG_CRITICAL,
				"Failed to obtain mhi result from CB.\n");
				return;
		}
		result = cb_info->result;
		chan_nr = (u32)result->user_data;
		client_index = chan_nr / 2;
			uci_handle =
				&uci_ctxt.client_handles[client_index];
		if (chan_nr % 2)
			parse_inbound_ack(uci_handle, result);
		else
			parse_outbound_ack(uci_handle, result);
		break;
	default:
		uci_log(UCI_DBG_VERBOSE,
			"Cannot handle cb reason 0x%x\n",
			cb_info->cb_reason);
	}
}

static int mhi_register_client(struct uci_client *mhi_client, int index)
{
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	int i = 0;

	uci_log(UCI_DBG_INFO, "Setting up workqueues.\n");
	init_waitqueue_head(&mhi_client->read_wq);
	init_waitqueue_head(&mhi_client->write_wq);
	mhi_client->out_chan = index * 2;
	mhi_client->in_chan = index * 2 + 1;
	mhi_client->client_index = index;

	mutex_init(&mhi_client->in_chan_lock);
	mutex_init(&mhi_client->out_chan_lock);

	uci_log(UCI_DBG_INFO, "Registering chan %d.\n", mhi_client->out_chan);
	ret_val = mhi_register_channel(&mhi_client->out_handle,
			mhi_client->out_chan,
			0,
			&uci_ctxt.client_info,
			(void *)(mhi_client->out_chan));
	if (MHI_STATUS_SUCCESS != ret_val)
			uci_log(UCI_DBG_ERROR,
			"Failed to init outbound chan 0x%x, ret 0x%x\n",
			mhi_client->out_chan, ret_val);

	uci_log(UCI_DBG_INFO, "Registering chan %d.\n", mhi_client->in_chan);
	ret_val = mhi_register_channel(&mhi_client->in_handle,
			mhi_client->in_chan,
			0,
			&uci_ctxt.client_info,
			(void *)(mhi_client->in_chan));
	if (MHI_STATUS_SUCCESS != ret_val)
		uci_log(UCI_DBG_ERROR,
			"Failed to init inbound chan 0x%x, ret 0x%x\n",
			mhi_client->in_chan, ret_val);
	if (i != (uci_ctxt.ctrl_chan_id / 2))
		mhi_client->dev_node_enabled = 1;
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

static int mhi_uci_init(void)
{
	u32 i = 0;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	struct uci_client *mhi_client = NULL;
	s32 r = 0;
	mhi_uci_ipc_log = ipc_log_context_create(MHI_UCI_IPC_LOG_PAGES,
						"mhi-uci", 0);
	if (mhi_uci_ipc_log == NULL) {
		uci_log(UCI_DBG_WARNING,
				"Failed to create IPC logging context\n");
	}
	uci_log(UCI_DBG_INFO, "Setting up work queues.\n");
	INIT_WORK(&uci_ctxt.mhi_enabled_work, process_mhi_enabled_notif);
	uci_ctxt.client_info.mhi_client_cb = uci_xfer_cb;

	mutex_init(&uci_ctxt.ctrl_mutex);

	uci_log(UCI_DBG_INFO, "Setting up channel attributes.\n");
	ret_val = uci_init_client_attributes(&uci_ctxt);
	if (MHI_STATUS_SUCCESS != ret_val) {
		uci_log(UCI_DBG_ERROR,
				"Failed to init client attributes\n");
		return -EIO;
	}
	uci_ctxt.ctrl_chan_id = MHI_CLIENT_IP_CTRL_1_OUT;
	uci_log(UCI_DBG_VERBOSE, "Initializing clients\n");

	uci_log(UCI_DBG_INFO, "Registering for MHI events.\n");
	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; ++i) {
		if (uci_ctxt.chan_attrib[i * 2].uci_ownership) {
			mhi_client = &uci_ctxt.client_handles[i];
			r = mhi_register_client(mhi_client, i);
			if (r) {
				uci_log(UCI_DBG_CRITICAL,
					"Failed to reg client %d ret %d\n",
					r, i);
			}
		}
	}
	uci_log(UCI_DBG_INFO, "Allocating char devices.\n");
	r = alloc_chrdev_region(&uci_ctxt.start_ctrl_nr,
			0, MHI_MAX_SOFTWARE_CHANNELS,
			DEVICE_NAME);

	if (IS_ERR_VALUE(r)) {
		uci_log(UCI_DBG_ERROR,
				"Failed to alloc char devs, ret 0x%x\n", r);
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
	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; ++i) {
		if (uci_ctxt.chan_attrib[i*2].uci_ownership) {
			cdev_init(&uci_ctxt.cdev[i], &mhi_uci_client_fops);
			uci_ctxt.cdev[i].owner = THIS_MODULE;
			r = cdev_add(&uci_ctxt.cdev[i],
					uci_ctxt.start_ctrl_nr + i , 1);
			if (IS_ERR_VALUE(r)) {
				uci_log(UCI_DBG_ERROR,
					"Failed to add cdev %d, ret 0x%x\n",
					i, r);
				goto failed_char_add;
			}
			uci_ctxt.client_handles[i].dev =
				device_create(uci_ctxt.mhi_uci_class, NULL,
						uci_ctxt.start_ctrl_nr + i,
						NULL, DEVICE_NAME "_pipe_%d",
						i * 2);

			if (IS_ERR(uci_ctxt.client_handles[i].dev)) {
				uci_log(UCI_DBG_ERROR,
						"Failed to add cdev %d\n", i);
				cdev_del(&uci_ctxt.cdev[i]);
				goto failed_device_create;
			}
		}
	}
	return 0;

failed_char_add:
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

static void __exit mhi_uci_exit(void)
{
	int i;
	for (i = 0; i < MHI_SOFTWARE_CLIENT_LIMIT; ++i) {
		cdev_del(&uci_ctxt.cdev[i]);
		device_destroy(uci_ctxt.mhi_uci_class,
			MKDEV(MAJOR(uci_ctxt.start_ctrl_nr), i * 2));
	}
	class_destroy(uci_ctxt.mhi_uci_class);
	unregister_chrdev_region(MAJOR(uci_ctxt.start_ctrl_nr),
			MHI_MAX_SOFTWARE_CHANNELS);
}

module_exit(mhi_uci_exit);
module_init(mhi_uci_init);
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("MHI_UCI");
MODULE_DESCRIPTION("MHI UCI Driver");
