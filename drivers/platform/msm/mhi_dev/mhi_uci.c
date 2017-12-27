/* Copyright (c) 2015,2017, The Linux Foundation. All rights reserved.
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

#define MHI_DEV_NODE_NAME_LEN		13
#define MHI_MAX_NR_OF_CLIENTS		23
#define MHI_SOFTWARE_CLIENT_START	0
#define MHI_SOFTWARE_CLIENT_LIMIT	(MHI_MAX_SOFTWARE_CHANNELS/2)
#define MHI_UCI_IPC_LOG_PAGES		(100)

#define MAX_NR_TRBS_PER_CHAN		1
#define MHI_QTI_IFACE_ID		4
#define DEVICE_NAME "mhi"

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
	u32 uci_ownership;
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
};

struct mhi_uci_ctxt_t {
	struct chan_attr chan_attrib[MHI_MAX_SOFTWARE_CHANNELS];
	struct uci_client client_handles[MHI_SOFTWARE_CLIENT_LIMIT];
	struct uci_ctrl ctrl_handle;
	void (*event_notifier)(struct mhi_dev_client_cb_reason *cb);
	dev_t start_ctrl_nr;
	struct cdev cdev[MHI_MAX_SOFTWARE_CHANNELS];
	dev_t ctrl_nr;
	struct cdev cdev_ctrl;
	struct device *dev;
	struct class *mhi_uci_class;
	atomic_t mhi_disabled;
	atomic_t mhi_enable_notif_wq_active;
};

#define CHAN_TO_CLIENT(_CHAN_NR) (_CHAN_NR / 2)

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

module_param(mhi_uci_ipc_log_lvl, uint, S_IRUGO | S_IWUSR);
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
	u32 i , j;
	struct chan_attr *chan_attributes;
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

	chan_attributes = &uci_ctxt.chan_attrib[chan];
	buf_size = chan_attributes->max_packet_size;

	for (i = 0; i < (chan_attributes->nr_trbs); i++) {
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

static int mhi_uci_send_packet(struct mhi_dev_client **client_handle, void *buf,
		u32 size, u32 is_uspace_buf)
{
	void *data_loc = NULL;
	uintptr_t memcpy_result = 0;
	u32 data_inserted_so_far = 0;
	struct uci_client *uci_handle;
	struct mhi_req ureq;


	uci_handle = container_of(client_handle, struct uci_client,
					out_handle);

	if (!client_handle || !buf ||
		!size || !uci_handle)
		return -EINVAL;

	if (is_uspace_buf) {
		data_loc = kmalloc(size, GFP_KERNEL);
		if (!data_loc) {
			uci_log(UCI_DBG_ERROR,
				"Failed to allocate memory 0x%x\n",
				size);
			return -ENOMEM;
		}
		memcpy_result = copy_from_user(data_loc, buf, size);
		if (memcpy_result)
			goto error_memcpy;
	} else {
		data_loc = buf;
	}
	ureq.client = *client_handle;
	ureq.buf = data_loc;
	ureq.len = size;
	ureq.chan = uci_handle->out_chan;
	ureq.mode = IPA_DMA_SYNC;

	data_inserted_so_far = mhi_dev_write_channel(&ureq);

error_memcpy:
	kfree(data_loc);
	return data_inserted_so_far;
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

static int open_client_mhi_channels(struct uci_client *uci_client)
{
	int rc = 0;

	uci_log(UCI_DBG_DBG,
			"Starting channels %d %d.\n",
			uci_client->out_chan,
			uci_client->in_chan);
	mutex_lock(&uci_client->out_chan_lock);
	mutex_lock(&uci_client->in_chan_lock);
	uci_log(UCI_DBG_DBG,
			"Initializing inbound chan %d.\n",
			uci_client->in_chan);

	rc = mhi_init_read_chan(uci_client, uci_client->in_chan);
	if (rc < 0) {
		uci_log(UCI_DBG_ERROR,
			"Failed to init inbound 0x%x, ret 0x%x\n",
			uci_client->in_chan, rc);
	}

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

	uci_handle =
		&uci_ctxt.client_handles[iminor(mhi_inode)];

	uci_log(UCI_DBG_DBG,
		"Client opened struct device node 0x%x, ref count 0x%x\n",
		iminor(mhi_inode), atomic_read(&uci_handle->ref_count));
	if (atomic_add_return(1, &uci_handle->ref_count) == 1) {
		if (!uci_handle) {
			atomic_dec(&uci_handle->ref_count);
			return -ENOMEM;
		}
		uci_handle->uci_ctxt = &uci_ctxt;
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
	struct mhi_uci_ctxt_t *uci_ctxt;
	u32 nr_in_bufs = 0;
	int rc = 0;
	int in_chan = 0;
	u32 buf_size = 0;

	if (!uci_handle)
		return -EINVAL;

	uci_ctxt = uci_handle->uci_ctxt;
	in_chan = iminor(mhi_inode) + 1;
	nr_in_bufs = uci_ctxt->chan_attrib[in_chan].nr_trbs;
	buf_size = uci_ctxt->chan_attrib[in_chan].max_packet_size;

	if (atomic_sub_return(1, &uci_handle->ref_count) == 0) {
		uci_log(UCI_DBG_DBG,
				"Last client left, closing channel 0x%x\n",
				iminor(mhi_inode));
		if (atomic_read(&uci_handle->mhi_chans_open)) {
			atomic_set(&uci_handle->mhi_chans_open, 0);

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
	rc = mhi_ctrl_state_info(&info);
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
	void *local_buf = NULL;
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
	ureq.mode = IPA_DMA_SYNC;

	uci_log(UCI_DBG_VERBOSE, "Client attempted read on chan %d\n",
			ureq.chan);
	do {
		if (!uci_handle->pkt_loc &&
				!atomic_read(&uci_ctxt.mhi_disabled)) {

			bytes_avail = mhi_dev_read_channel(&ureq);

			uci_log(UCI_DBG_VERBOSE,
				"reading from mhi_core local_buf = %p",
				local_buf);
			uci_log(UCI_DBG_VERBOSE,
					"buf_size = 0x%x bytes_read = 0x%x\n",
					 ureq.len, bytes_avail);

			if (bytes_avail < 0) {
				uci_log(UCI_DBG_ERROR,
				"Failed to read channel ret %d\n",
					bytes_avail);
				ret_val =  -EIO;
				goto error;
			}

			if (bytes_avail > 0) {
				uci_handle->pkt_loc = (void *) ureq.buf;
				uci_handle->pkt_size = ureq.actual_len;

				*bytes_pending = (loff_t)uci_handle->pkt_size;
				uci_log(UCI_DBG_VERBOSE,
					"Got pkt of sz 0x%x at adr %p, ch %d\n",
					uci_handle->pkt_size,
					ureq.buf, ureq.chan);
			} else {
				uci_handle->pkt_loc = 0;
				uci_handle->pkt_size = 0;
			}
		}
		if (bytes_avail == 0) {

			/* If nothing was copied yet, wait for data */
			uci_log(UCI_DBG_VERBOSE,
				"No data read_data_ready %d, chan %d\n",
				atomic_read(&uci_handle->read_data_ready),
				ureq.chan);

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
				"Got packet: avail pkts %d phy_adr %p, ch %d\n",
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
				"chan %d err: avail pkts %d phy_adr %p",
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
				"All data consumed. Pkt loc %p ,chan %d\n",
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
				(void *)buf, count, 1);
		if (ret_val < 0) {
			uci_log(UCI_DBG_ERROR,
				"Error while writing data to MHI, chan %d, buf %p, size %d\n",
				chan, (void *)buf, count);
			ret_val = -EIO;
			break;
		}
		if (!ret_val) {
			uci_log(UCI_DBG_VERBOSE,
				"No descriptors available, did we poll, chan %d?\n",
				chan);
			mutex_unlock(&uci_handle->out_chan_lock);
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

static int uci_init_client_attributes(struct mhi_uci_ctxt_t *uci_ctxt)
{
	u32 i = 0;
	u32 data_size = TRB_MAX_DATA_SIZE;
	u32 index = 0;
	struct uci_client *client;
	struct chan_attr *chan_attrib = NULL;

	for (i = 0; i < ARRAY_SIZE(uci_ctxt->chan_attrib); i++) {
		chan_attrib = &uci_ctxt->chan_attrib[i];
		switch (i) {
		case MHI_CLIENT_LOOPBACK_OUT:
		case MHI_CLIENT_LOOPBACK_IN:
		case MHI_CLIENT_SAHARA_OUT:
		case MHI_CLIENT_SAHARA_IN:
		case MHI_CLIENT_EFS_OUT:
		case MHI_CLIENT_EFS_IN:
		case MHI_CLIENT_MBIM_OUT:
		case MHI_CLIENT_MBIM_IN:
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
			      kmalloc(sizeof(struct mhi_dev_iov) *
					      chan_attrib->nr_trbs,
					GFP_KERNEL);
			if (NULL == client->in_buf_list)
				return -ENOMEM;
		}
		if (i % 2 == 0)
			chan_attrib->dir = MHI_DIR_OUT;
		else
			chan_attrib->dir = MHI_DIR_IN;
	}
	return 0;
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
	mhi_client->out_chan = index * 2 + 1;
	mhi_client->in_chan = index * 2;
	mhi_client->client_index = index;

	mutex_init(&mhi_client->in_chan_lock);
	mutex_init(&mhi_client->out_chan_lock);

	uci_log(UCI_DBG_DBG, "Registering chan %d.\n", mhi_client->out_chan);
	return 0;
}

static long mhi_uci_client_ioctl(struct file *file, unsigned cmd,
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

int mhi_uci_init(void)
{
	u32 i = 0;
	int ret_val = 0;
	struct uci_client *mhi_client = NULL;
	s32 r = 0;

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

	/* Control node */
	cdev_init(&uci_ctxt.cdev_ctrl, &mhi_uci_ctrl_client_fops);
	uci_ctxt.cdev_ctrl.owner = THIS_MODULE;
	r = cdev_add(&uci_ctxt.cdev_ctrl, uci_ctxt.ctrl_nr , 1);
	if (IS_ERR_VALUE(r)) {
		uci_log(UCI_DBG_ERROR,
		"Failed to add ctrl cdev %d, ret 0x%x\n", i, r);
		goto failed_char_add;
	}

	uci_ctxt.dev =
		device_create(uci_ctxt.mhi_uci_class, NULL,
				uci_ctxt.ctrl_nr,
				NULL, DEVICE_NAME "_ctrl");
	if (IS_ERR(uci_ctxt.dev)) {
		uci_log(UCI_DBG_ERROR,
				"Failed to add ctrl cdev %d\n", i);
		cdev_del(&uci_ctxt.cdev_ctrl);
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
