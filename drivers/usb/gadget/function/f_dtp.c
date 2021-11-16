/*
 * Gadget Function Driver for DTP
 *
 * Copyright (C) 2020 xiaomi, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Author: Deng yong jian <dengyongjian@xiaomi.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/ipc_logging.h>

#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/ch9.h>
#include <linux/usb/f_dtp.h>
#include <linux/configfs.h>
#include <linux/usb/composite.h>

#include "configfs.h"

#define DRIVER_NAME "dtp"

#define log_dbg(fmt, ...) do {\
	pr_debug("%s: {%s} " fmt, DRIVER_NAME, __func__, ##__VA_ARGS__);\
}while (0)

#define log_info(fmt, ...) do {\
	pr_info("%s: {%s} " fmt, DRIVER_NAME, __func__, ##__VA_ARGS__);\
}while (0)

#define log_err(fmt, ...) do {\
	pr_err("%s: {%s} " fmt, DRIVER_NAME, __func__, ##__VA_ARGS__);\
}while (0)

/*usb protocol buffer size*/
#define RX_BUFFER_SIZE    1048576
#define TX_BUFFER_SIZE    1048576
#define BULK_BUFFER_SIZE  16384
#define INTR_BUFFER_SIZE  28

/*number of tx and rx requests to allocate*/
#define TX_REQ_MAX 	8
#define RX_REQ_MAX 	2
#define INTR_REQ_MAX 	5

/*max interface num*/
#define MAX_INTERFACE_NUM 64

/*instance name len*/
#define MAX_NAME_LEN 40

/*max packet size for single receiving data, prevent deadlock in the process of receiving data*/
#define MAX_TRANSMISSION_SIZE (48*1024*1024) /*48M*/

/*String Idx*/
#define INTERFACE_STRING_INDEX 0


/*class request */
#define REQUEST_SET_DEVICE_STATUS	0x13
#define REQUEST_QUERY_DEVICE_STATUS	0x14

/*transfer history for debug*/
#define MAX_HISTORY_NUM 100

/*enum value for device state */
enum {
	e_offline = 0,
	e_ready,
	e_busy,
	e_cancel,
	e_error,
};

/*module param for debug*/
unsigned int rx_req_len = RX_BUFFER_SIZE;
module_param(rx_req_len, uint, 0644);

unsigned int tx_req_len = TX_BUFFER_SIZE;
module_param(tx_req_len, uint, 0644);

unsigned int tx_reqs = TX_REQ_MAX;
module_param(tx_reqs, uint, 0644);


struct dtp_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;

	/*usb endpoint*/
	struct usb_ep *ep_in;
	struct usb_ep *ep_out;
	struct usb_ep *ep_intr;

	int state;/*Represent the device state,such as offline,ready,busy ..*/
	spinlock_t lock;/*used to protect state/tx_idle/intr_idle*/

	atomic_t refcnt;/*for open/release*/
	atomic_t ioctl_refcnt;/*for ioctl*/

	struct list_head tx_idle;/*the idle tx list of usb_request*/
	struct list_head intr_idle;/*the idle intr list of usb_request*/

	wait_queue_head_t read_wq;/*the wait queue of wait for reading success*/
	wait_queue_head_t write_wq;/*the wait queue of wait for  writing success*/
	wait_queue_head_t intr_wq;/*the wait queue of wait for intr success*/

	struct usb_request *rx_req[RX_REQ_MAX];/*the usb_request for receive data from usb endpoint*/

	int rx_done;/*Represent receive done*/

	struct workqueue_struct *wq;/*work queue for send_file_work/receive_file_work*/
	struct work_struct send_file_work;/*send file work*/
	struct work_struct receive_file_work;/*receive file work*/

	struct mutex read_mutex;

	struct file *xfer_file;/*The file which to be send*/
	loff_t xfer_file_offset;/*file offset*/
	int64_t xfer_file_length;/*file length*/
	uint16_t xfer_command;
	uint32_t xfer_transaction_id;
	int xfer_result;/*transmission result*/

	/*history records*/
	struct {
		unsigned long vfs_rbytes;
		unsigned long vfs_wbytes;
		unsigned int vfs_rtime;
		unsigned int vfs_wtime;
	}history[MAX_HISTORY_NUM];
	unsigned int dbg_read_index;
	unsigned int dbg_write_index;
};

struct dtp_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct dtp_dev *dev;
	char ext_compat_id[16];
	struct usb_os_desc os_desc;
};

static struct usb_interface_descriptor usb_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = MAX_INTERFACE_NUM,
	.bNumEndpoints          = 3,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = USB_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol     = 0,
};


static struct usb_endpoint_descriptor ss_ep_desc_in = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_ep_comp_desc_in = {
	.bLength                = sizeof(ss_ep_comp_desc_in),
	.bDescriptorType        = USB_DT_SS_ENDPOINT_COMP,
	/* .bMaxBurst           = DYNAMIC, */
};

static struct usb_endpoint_descriptor ss_ep_desc_out = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_ep_comp_desc_out = {
	.bLength                = sizeof(ss_ep_comp_desc_out),
	.bDescriptorType        = USB_DT_SS_ENDPOINT_COMP,
	/* .bMaxBurst           = DYNAMIC, */
};

static struct usb_endpoint_descriptor highspeed_ep_desc_in = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor highspeed_ep_desc_out = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor fullspeed_ep_desc_in = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fullspeed_ep_desc_out = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor intr_ep_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize         = __constant_cpu_to_le16(INTR_BUFFER_SIZE),
	.bInterval              = 6,
};

static struct usb_ss_ep_comp_descriptor intr_ss_ep_comp_desc = {
	.bLength                = sizeof(intr_ss_ep_comp_desc),
	.bDescriptorType        = USB_DT_SS_ENDPOINT_COMP,
	.wBytesPerInterval      = cpu_to_le16(INTR_BUFFER_SIZE),
};

static struct usb_descriptor_header *fs_descs[] = {
	(struct usb_descriptor_header *) &usb_interface_desc,
	(struct usb_descriptor_header *) &fullspeed_ep_desc_in,
	(struct usb_descriptor_header *) &fullspeed_ep_desc_out,
	(struct usb_descriptor_header *) &intr_ep_desc,
	NULL,
};

static struct usb_descriptor_header *hs_descs[] = {
	(struct usb_descriptor_header *) &usb_interface_desc,
	(struct usb_descriptor_header *) &highspeed_ep_desc_in,
	(struct usb_descriptor_header *) &highspeed_ep_desc_out,
	(struct usb_descriptor_header *) &intr_ep_desc,
	NULL,
};

static struct usb_descriptor_header *ss_descs[] = {
	(struct usb_descriptor_header *) &usb_interface_desc,
	(struct usb_descriptor_header *) &ss_ep_desc_in,
	(struct usb_descriptor_header *) &ss_ep_comp_desc_in,
	(struct usb_descriptor_header *) &ss_ep_desc_out,
	(struct usb_descriptor_header *) &ss_ep_comp_desc_out,
	(struct usb_descriptor_header *) &intr_ep_desc,
	(struct usb_descriptor_header *) &intr_ss_ep_comp_desc,
	NULL,
};


static struct usb_string usb_string_defs[] = {
	/* Naming interface "DTP" so libdtp will recognize us */
	[INTERFACE_STRING_INDEX].s	= "DTP",
	{  },	/* end of list */
};

static struct usb_gadget_strings usb_string_table = {
	.language		= 0x0409,	/* en-US */
	.strings		= usb_string_defs,
};

static struct usb_gadget_strings *usb_strings[] = {
	&usb_string_table,
	NULL,
};


static struct dtp_dev *_dtp_dev = NULL;
static struct dentry *root_dentry = NULL;
static const char device_name[] = DRIVER_NAME "_usb";


static inline struct dtp_dev *get_dtp_dev(void)
{
	return _dtp_dev;
}

static inline struct dtp_instance *ci_to_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct dtp_instance, func_inst.group);
}

static inline struct dtp_dev *to_dtp_dev(struct usb_function *f)
{
	return container_of(f, struct dtp_dev, function);
}

static inline struct dtp_instance *to_instance(const struct usb_function_instance *fi)
{
	return container_of(fi, struct dtp_instance, func_inst);
}

/*
 * Function: atomic_try_down - inc refcnt, if the refcnt is 1, return success,otherwise return failed
 * */

static inline int atomic_try_down(atomic_t *refcnt)
{
	if (atomic_inc_return(refcnt) == 1) {
		return 0;
	} else {
		atomic_dec(refcnt);
		return -1;
	}
}

/*
 * Function: atomic_up - dec refcnt
 * */
static inline void atomic_up(atomic_t *refcnt)
{
	atomic_dec(refcnt);
}

/*
 * Function: usb_request_add - add a usb_request to the idl list
 * */
static void usb_request_add(struct dtp_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}
/*
 * Function: usb_request_get - get a usb_requset from the idle list
 * */
static struct usb_request *usb_request_get(struct dtp_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req = NULL;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = NULL;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	return req;
}

/*
 * Function: usb_request_new - alloc a new usb_request and transfer buffer
 * */
static struct usb_request *usb_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);

	if (!req)
		return NULL;

	/*allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

/*
 * Function: usb_request_free - free a usb_request and transfer buffer
 * */
static void usb_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

/*
 * Function: usb_request_complete_in - be called after tx transfer done
 * */
static void usb_request_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct dtp_dev *dev = get_dtp_dev();
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (req->status != 0 && dev->state != e_offline)
		dev->state = e_error;
	spin_unlock_irqrestore(&dev->lock, flags);

	usb_request_add(dev, &dev->tx_idle, req);

	/*wake up usb_write or send_file_work*/
	wake_up(&dev->write_wq);
}

/*
 * Function: usb_request_complete_out - be called after rx transfer done
 * */
static void usb_request_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct dtp_dev *dev = get_dtp_dev();
	unsigned long flags;

	dev->rx_done = 1;/*rx done*/
	spin_lock_irqsave(&dev->lock, flags);
	if (req->status != 0 && dev->state != e_offline)
		dev->state = e_error;
	spin_unlock_irqrestore(&dev->lock, flags);

	/*wake up usb_read or receive_file_work*/
	wake_up(&dev->read_wq);
}

/*
 * Function: usb_request_complete_intr - be called after intr transfer done
 * */
static void usb_request_complete_intr(struct usb_ep *ep, struct usb_request *req)
{
	struct dtp_dev *dev = get_dtp_dev();
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (req->status != 0 && dev->state != e_offline)
		dev->state = e_error;
	spin_unlock_irqrestore(&dev->lock, flags);

	usb_request_add(dev, &dev->intr_idle, req);

	/*wake up usb_send_event*/
	wake_up(&dev->intr_wq);
}


static int debug_read_history(struct seq_file *s, void *unused)
{
	struct dtp_dev *dev = get_dtp_dev();
	unsigned int iteration = 0;
	unsigned int min = 0;
	unsigned int max = 0;
	unsigned int sum = 0;
	unsigned long flags;
	int i = 0;

	seq_puts(s, "\n=======================\n");
	seq_puts(s, "History Write:\n");
	seq_puts(s, "\n=======================\n");

	spin_lock_irqsave(&dev->lock, flags);
	min = dev->history[0].vfs_wtime;

	for (i = 0; i < MAX_HISTORY_NUM; i++) {
		seq_printf(s, "vfs write: bytes:%ld\t\t time:%d\n",
				dev->history[i].vfs_wbytes,
				dev->history[i].vfs_wtime);

		if (dev->history[i].vfs_wbytes == rx_req_len) {
			sum += dev->history[i].vfs_wtime;
			if (min > dev->history[i].vfs_wtime)
				min = dev->history[i].vfs_wtime;
			if (max < dev->history[i].vfs_wtime)
				max = dev->history[i].vfs_wtime;
			iteration++;
		}
	}

	seq_printf(s, "vfs_write(time in usec) min:%d\t max:%d\t avg:%d\n",
						min, max, sum / iteration);
	min = max = sum = iteration = 0;

	seq_puts(s, "\n=======================\n");
	seq_puts(s, "History Read:\n");
	seq_puts(s, "\n=======================\n");

	min = dev->history[0].vfs_rtime;

	for (i = 0; i < MAX_HISTORY_NUM; i++) {
		seq_printf(s, "vfs read: bytes:%ld\t\t time:%d\n",
				dev->history[i].vfs_rbytes,
				dev->history[i].vfs_rtime);
		if (dev->history[i].vfs_rbytes == tx_req_len) {
			sum += dev->history[i].vfs_rtime;
			if (min > dev->history[i].vfs_rtime)
				min = dev->history[i].vfs_rtime;
			if (max < dev->history[i].vfs_rtime)
				max = dev->history[i].vfs_rtime;
			iteration++;
		}
	}

	seq_printf(s, "vfs_read(time in usec) min:%d\t max:%d\t avg:%d\n",
						min, max, sum / iteration);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}

static ssize_t debug_usb_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct dtp_dev *dev = get_dtp_dev();
	unsigned long flags;
	int val = -1;

	if (!buf) {
		log_err("buf is null\n");
		return -EINVAL;
	}

	if (kstrtoint(buf, 0, &val) || val != 0) {
		log_err("Wrong value, please enter value as 0.\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&dev->lock, flags);
	memset(&dev->history[0], 0, MAX_HISTORY_NUM * sizeof(dev->history[0]));
	dev->dbg_read_index = 0;
	dev->dbg_write_index = 0;
	spin_unlock_irqrestore(&dev->lock, flags);

	return count;
}

static int debug_usb_open(struct inode *inode, struct file *file)
{
	return single_open(file, debug_read_history, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open = debug_usb_open,
	.read = seq_read,
	.write = debug_usb_write,
};

/*
 * Function: debug_debugfs_init - debugfs init
 * */
static void debug_debugfs_init(void)
{
	struct dentry *sub_dentry;

	root_dentry = debugfs_create_dir("gadget_dtp", 0);
	if (!root_dentry || IS_ERR(root_dentry))
		return;

	sub_dentry = debugfs_create_file("status", 0644, root_dentry, 0, &debug_fops);
	if (!sub_dentry || IS_ERR(sub_dentry)) {
		debugfs_remove(root_dentry);
		root_dentry = NULL;
		return;
	}
}

/*
 * Function: usb_read - The api to read data from usb endpoint
 */
static ssize_t usb_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct dtp_dev *dev = fp->private_data;
	struct usb_request *req_read = NULL;
	struct usb_request *req_write = NULL;
	size_t readed_count = 0;
	size_t copy_count = 0;
	size_t xfer = 0;
	size_t xfer_copy = 0;
	size_t req_len = 0;
	bool read_done  = false;
	int max_loop = 0;
	int ret = 0;
	int cur = 0;
	unsigned long delta;
	unsigned long flags;
	ktime_t start_time;

	log_dbg("(%zu) state:%d\n", count, dev->state);

	/*wait for read_mutex,only single thread is supported*/
	mutex_lock(&dev->read_mutex);

	/*waiting for online */
	ret = wait_event_interruptible_timeout(dev->read_wq, dev->state != e_offline, msecs_to_jiffies(5000));
	if (ret <= 0) {
		log_err("wait for timeout\n");
		ret = -ENODEV;
		goto wait_err;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state == e_offline) {
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = -ENODEV;
		log_err("dev is offline\n");
		goto state_err;
	}

	if (dev->state == e_cancel) {
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = -ECANCELED;
		log_err("dev is canceled\n");
		goto state_err;
	}

	dev->state = e_busy;/*busy*/

	spin_unlock_irqrestore(&dev->lock, flags);

	req_len = (count < rx_req_len ) ? count : rx_req_len;
	max_loop = MAX_TRANSMISSION_SIZE/req_len;
	log_dbg("req len: %d, max loop: %d\n", req_len, max_loop);

	while (((readed_count < count) && (max_loop-- > 0)) || req_write) {
requeue_req:
		/*get a request*/
		log_dbg("read_done: %d\n", read_done);
		if (!read_done) {
			req_read = dev->rx_req[cur];
			cur = (cur + 1) % RX_REQ_MAX;
			dev->rx_done = 0;
			if ((count - readed_count) > req_len)
				req_read->length = req_len;
			else
				req_read->length = count - readed_count;

			log_dbg("read length:%d\n", req_read->length);

			ret = usb_ep_queue(dev->ep_out, req_read, GFP_KERNEL);
			if (ret < 0) {
				log_err("usb eq queue failed\n");
				ret = -EIO;
				goto done;
			} else {
				log_dbg("rx %pK queue\n", req_read);
				start_time = ktime_get();
			}
		}

		if (req_write) {
			xfer_copy = (req_write->actual < count) ? req_write->actual : count;
			if (copy_to_user(buf + copy_count, req_write->buf, xfer_copy)) {
				log_err("copy to user failed\n");
				ret = -EFAULT;
				goto done;
			}
			copy_count += xfer_copy;
			req_write = NULL;
			log_dbg("copy_count:%d,xfer_copy:%d\n", copy_count, xfer_copy);
		}

		if (req_read == NULL)
			break;

		/* wait for a request to complete */
		ret = wait_event_interruptible(dev->read_wq, dev->rx_done || dev->state != e_busy);

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->state == e_cancel || dev->state == e_offline) {
			if (dev->state == e_offline)
				ret = -EIO;
			else
				ret = -ECANCELED;
			spin_unlock_irqrestore(&dev->lock, flags);
			if (!dev->rx_done)
				usb_ep_dequeue(dev->ep_out, req_read);
			goto done;
		}
		spin_unlock_irqrestore(&dev->lock, flags);

		if (ret < 0) {
			usb_ep_dequeue(dev->ep_out, req_read);
			goto done;
		}

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->state == e_busy) {
			spin_unlock_irqrestore(&dev->lock, flags);
			/* If we got a 0-len packet, throw it back and try again. */
			if (req_read->actual == 0) {
				log_dbg("empty packet\n");
				goto requeue_req;
			}

			if (req_read->actual < req_read->length) {
				 /*short packet is used to signal EOF*/
				log_dbg("short packet\n");
				read_done = true;
			}


			delta = ktime_to_ms(ktime_sub(ktime_get(), start_time));
			log_dbg("rx %pK %d (delta: %dms)\n", req_read, req_read->actual, delta);
			xfer = (req_read->actual < count) ? req_read->actual : count;

			readed_count += xfer;
			req_write = req_read;
			req_read = NULL;

			if (readed_count == count) {
				log_dbg("read complete\n");
				read_done = true;
			}


		} else {
			spin_unlock_irqrestore(&dev->lock, flags);
			ret = -EIO;
		}
	}


done:
	if (ret >= 0)
		ret = copy_count;
wait_err:
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state != e_offline)
		dev->state = e_ready;
	spin_unlock_irqrestore(&dev->lock, flags);

state_err:
	mutex_unlock(&dev->read_mutex);

	log_dbg("returning %d state:%d\n", ret, dev->state);
	return ret;
}


/*
 * Function: usb_write - The api to write data to usb endpoint
 */
static ssize_t usb_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	struct dtp_dev *dev = fp->private_data;
	struct usb_request *req = NULL;
	unsigned long flags;
	unsigned xfer = 0;
	ssize_t r = count;
	int sendZLP = 1;
	int max_loop = 0;
	int ret = -1;

	log_dbg("(%zu) state:%d\n", count, dev->state);

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state == e_cancel) {
		spin_unlock_irqrestore(&dev->lock, flags);
		r = -ECANCELED;
		log_err("dev is canceled\n");
		goto state_err;
	}
	if (dev->state == e_offline) {
		spin_unlock_irqrestore(&dev->lock, flags);
		r = -ENODEV;
		log_err("dev is offline\n");
		goto state_err;
	}

	dev->state = e_busy;/*busy*/
	spin_unlock_irqrestore(&dev->lock, flags);

	max_loop = MAX_TRANSMISSION_SIZE/tx_req_len;
	log_dbg("tx req len: %d, max loop: %d\n", tx_req_len, max_loop);

	while (((count > 0) && (max_loop-- > 0)) || sendZLP) {

		if (count <= 0 || max_loop <= 0) {
			sendZLP = 0;
			count = 0;
		}

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->state != e_busy) {
			spin_unlock_irqrestore(&dev->lock, flags);
			r = -EIO;
			log_err("dev is not busy,error of the dev state\n");
			break;
		}
		spin_unlock_irqrestore(&dev->lock, flags);

		/*get an idle tx request to use */
		req = NULL;
		ret = wait_event_interruptible(dev->write_wq, ((req = usb_request_get(dev, &dev->tx_idle)) || dev->state != e_busy));
		if (!req) {
			log_err("request NULL ret:%d state:%d\n", ret, dev->state);
			r = ret;
			break;
		}

		if (count > tx_req_len)
			xfer = tx_req_len;
		else
			xfer = count;
		if (xfer && copy_from_user(req->buf, buf, xfer)) {
			r = -EFAULT;
			log_err("copy from user failed\n");
			break;
		}


		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			log_err("xfer error %d\n", ret);
			r = -EIO;
			break;
		}

		buf += xfer;
		count -= xfer;

		/* zero this so we don't try to free it on error exit */
		req = NULL;
	}

	if (req)
		usb_request_add(dev, &dev->tx_idle, req);

state_err:
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state != e_offline)
		dev->state = e_ready;
	spin_unlock_irqrestore(&dev->lock, flags);

	log_dbg("returning %d state:%d\n", r, dev->state);
	return r;
}


static int usb_send_event(struct dtp_dev *dev, struct dtp_event *event)
{
	struct usb_request *req = NULL;
	unsigned long flags;
	int length = event->length;
	int ret = 0;

	log_dbg("(%zu)\n", event->length);

	if (length < 0 || length > INTR_BUFFER_SIZE)
		return -EINVAL;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state == e_offline) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return -ENODEV;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	ret = wait_event_interruptible_timeout(dev->intr_wq,
			(req = usb_request_get(dev, &dev->intr_idle)),
			msecs_to_jiffies(1000));
	if (!req)
		return -ETIME;

	if (copy_from_user(req->buf, (void __user *)event->data, length)) {
		usb_request_add(dev, &dev->intr_idle, req);
		return -EFAULT;
	}
	req->length = length;
	ret = usb_ep_queue(dev->ep_intr, req, GFP_KERNEL);
	if (ret)
		usb_request_add(dev, &dev->intr_idle, req);

	return ret;
}

static long send_receive_ioctl(struct file *fp, unsigned int code,
	struct dtp_file_desc *fdesc)
{
	struct dtp_dev *dev = fp->private_data;
	struct work_struct *work = NULL;
	struct file *filp = NULL;
	unsigned long flags;
	int ret = -EINVAL;

	if (atomic_try_down(&dev->ioctl_refcnt)) {
		log_err("dev is busy, state(%d)\n", dev->state);
		return -EBUSY;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state == e_cancel) {
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = -ECANCELED;
		goto err1;
	}
	if (dev->state == e_offline) {
		spin_unlock_irqrestore(&dev->lock, flags);
		ret = -ENODEV;
		goto err1;
	}
	dev->state = e_busy;
	spin_unlock_irqrestore(&dev->lock, flags);

	/* hold a reference to the file while we are working with it */
	filp = fget(fdesc->fd);
	if (!filp) {
		ret = -EBADF;
		goto err2;
	}

	/* write the parameters */
	dev->xfer_file = filp;
	dev->xfer_file_offset = fdesc->offset;
	dev->xfer_file_length = fdesc->length;
	/* make sure write is done before parameters are read */
	smp_wmb();

	if (code == DTP_SEND_FILE) {
		work = &dev->send_file_work;
	} else {
		work = &dev->receive_file_work;
	}

	/* We do the file transfer on a work queue so it will run
	 * in kernel context, which is necessary for vfs_read and
	 * vfs_write to use our buffers in the kernel address space.
	 */
	queue_work(dev->wq, work);
	/* wait for operation to complete */
	flush_workqueue(dev->wq);
	fput(filp);

	/* read the result */
	smp_rmb();
	ret = dev->xfer_result;

err2:
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state == e_cancel)
		ret = -ECANCELED;
	else if (dev->state != e_offline)
		dev->state = e_ready;
	spin_unlock_irqrestore(&dev->lock, flags);
err1:
	atomic_up(&dev->ioctl_refcnt);
	log_dbg("ioctl returning %d\n", ret);
	return ret;
}

static long usb_ioctl(struct file *fp, unsigned int code, unsigned long value)
{
	struct dtp_dev *dev = fp->private_data;
	struct dtp_file_desc fdesc;
	struct dtp_event event;
	int ret = -EINVAL;

	switch (code) {
	case DTP_SEND_FILE:
	case DTP_RECEIVE_FILE:
		if (copy_from_user(&fdesc, (void __user *)value, sizeof(fdesc))) {
			ret = -EFAULT;
			log_err("copy file desc failed\n");
			goto fail;
		}
		ret = send_receive_ioctl(fp, code, &fdesc);
	break;
	case DTP_SEND_EVENT:
		if (atomic_try_down(&dev->ioctl_refcnt)) {
			log_info("dev is busy\n");
			return -EBUSY;
		}
		/* return here so we don't change dev->state below,
		 * which would interfere with bulk transfer state.
		 */
		if (copy_from_user(&event, (void __user *)value, sizeof(event))) {
			ret = -EFAULT;
			log_err("copy event failed\n");
		} else
			ret = usb_send_event(dev, &event);
		atomic_up(&dev->ioctl_refcnt);
	break;
	default:
		log_dbg("unknown ioctl code: %d\n", code);
	}
fail:
	return ret;
}

/*
 * 32 bit userspace calling into 64 bit kernel. handle ioctl code
 * and userspace pointer
 */
#ifdef CONFIG_COMPAT
static long compat_usb_ioctl(struct file *fp, unsigned int code, unsigned long value)
{
	struct dtp_dev *dev = fp->private_data;
	struct __compat_dtp_file_desc cfdesc;
	struct __compat_dtp_event cevent;
	struct dtp_file_desc fdesc;
	struct dtp_event event;
	unsigned int cmd = 0;
	bool send_file = false;
	int ret = -EINVAL;

	switch (code) {
	case COMPAT_DTP_SEND_FILE:
		cmd = DTP_SEND_FILE;
		send_file = true;
		break;
	case COMPAT_DTP_RECEIVE_FILE:
		cmd = DTP_RECEIVE_FILE;
		send_file = true;
		break;
	case COMPAT_DTP_SEND_EVENT:
		cmd = DTP_SEND_EVENT;
		break;
	default:
		log_info("unknown compat_ioctl code: %d\n", code);
		ret = -ENOIOCTLCMD;
		goto fail;
	}

	if (send_file) {
		if (copy_from_user(&cfdesc, (void __user *)value, sizeof(cfdesc))) {
			ret = -EFAULT;
			log_err("copy file desc failed\n");
			goto fail;
		}
		fdesc.fd = cfdesc.fd;
		fdesc.offset = cfdesc.offset;
		fdesc.length = cfdesc.length;
		fdesc.command = cfdesc.command;
		fdesc.transaction_id = cfdesc.transaction_id;
		ret = send_receive_ioctl(fp, cmd, &fdesc);
	} else {
		if (atomic_try_down(&dev->ioctl_refcnt)) {
			log_info("dev is busy\n");
			return -EBUSY;
		}
		/* return here so we don't change dev->state below,
		 * which would interfere with bulk transfer state.
		 */
		if (copy_from_user(&cevent, (void __user *)value, sizeof(cevent))) {
			ret = -EFAULT;
			log_err("copy event failed\n");
			goto fail;
		}
		event.length = cevent.length;
		event.data = compat_ptr(cevent.data);
		ret = usb_send_event(dev, &event);
		atomic_up(&dev->ioctl_refcnt);
	}
fail:
	return ret;
}
#endif


static int usb_open(struct inode *ip, struct file *fp)
{
	struct dtp_dev *dev = get_dtp_dev();
	unsigned long flags;

	log_dbg("open\n");
	if (atomic_try_down(&dev->refcnt)) {
		log_info("dev is busy! please check! Is it exited abnormally or not closed?\n");
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state != e_offline)
		dev->state = e_ready;
	spin_unlock_irqrestore(&dev->lock, flags);

	fp->private_data = dev;
	return 0;
}

static int usb_release(struct inode *ip, struct file *fp)
{
	struct dtp_dev *dev = get_dtp_dev();

	log_dbg("release\n");

	atomic_up(&dev->refcnt);
	return 0;
}
static struct file_operations usb_fops = {
	.owner = THIS_MODULE,
	.read = usb_read,
	.write = usb_write,
	.unlocked_ioctl = usb_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_usb_ioctl,
#endif
	.open = usb_open,
	.release = usb_release,
};

static struct miscdevice usb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = device_name,
	.fops = &usb_fops,
};



/*
 * Function send_file_work - read from a local file and write to USB
 * */
static void send_file_work(struct work_struct *data)
{
	struct dtp_dev *dev = container_of(data, struct dtp_dev, send_file_work);
	struct usb_request *req = 0;
	struct file *filp = NULL;
	unsigned long flags;
	int64_t count = 0;
	int sendZLP = 0;
	int xfer = 0;
	int ret = 0;
	int r = 0;
	loff_t offset = 0;
	ktime_t start_time;

	/*read our parameters*/
	smp_rmb();
	filp = dev->xfer_file;
	offset = dev->xfer_file_offset;
	count = dev->xfer_file_length;

	log_dbg("(%lld %lld)\n", offset, count);

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state == e_offline) {
		r = -EIO;
		spin_unlock_irqrestore(&dev->lock, flags);
		goto fail;
	}
	spin_unlock_irqrestore(&dev->lock, flags);


	/* we need to send a zero length packet to signal the end of transfer
	 * if the transfer size is aligned to a packet boundary.
	 */
	if ((count & (dev->ep_in->maxpacket - 1)) == 0)
		sendZLP = 1;

	while (count > 0 || sendZLP) {
		/* so we exit after sending ZLP */
		if (count == 0)
			sendZLP = 0;

		/*get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq, (req = usb_request_get(dev, &dev->tx_idle)) || dev->state != e_busy);
		spin_lock_irqsave(&dev->lock, flags);
		if (dev->state == e_cancel || dev->state == e_cancel) {
			if (dev->state == e_offline)
				r = -EIO;
			else
				r = -ECANCELED;
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&dev->lock, flags);
		if (!req) {
			log_dbg("request NULL ret:%d state:%d\n", ret, dev->state);
			r = ret;
			break;
		}

		if (count > tx_req_len)
			xfer = tx_req_len;
		else
			xfer = count;

		start_time = ktime_get();
		/*ret = vfs_read(filp, req->buf, xfer, &offset);*/
		ret = -1;/*no used*/
		if (ret < 0) {
			r = ret;
			break;
		}

		xfer = ret;
		dev->history[dev->dbg_read_index].vfs_rbytes = xfer;
		dev->history[dev->dbg_read_index].vfs_rtime = ktime_to_us(ktime_sub(ktime_get(), start_time));
		dev->dbg_read_index = (dev->dbg_read_index + 1) % MAX_HISTORY_NUM;

		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			log_dbg("xfer error %d\n", ret);
			spin_lock_irqsave(&dev->lock, flags);
			if (dev->state != e_offline)
				dev->state = e_error;
			r = -EIO;
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}

		count -= xfer;

		/*zero this so we don't try to free it on error exit */
		req = 0;
	}

	if (req)
		usb_request_add(dev, &dev->tx_idle, req);

fail:
	log_dbg("returning %d state:%d\n", r, dev->state);
	dev->xfer_result = r;
	smp_wmb();
}


/*
 * Function: receive_file_work - read data from USB and write to a local file
 * */
static void receive_file_work(struct work_struct *data)
{
	struct dtp_dev *dev = container_of(data, struct dtp_dev, receive_file_work);
	struct usb_request *read_req = NULL, *write_req = NULL;
	struct file *filp = NULL;
	unsigned long flags;
	int64_t count = 0;
	int cur_buf = 0;
	int ret = 0;
	int r = 0;
	loff_t offset;
	ktime_t start_time;

	/*read our parameters*/
	smp_rmb();
	filp = dev->xfer_file;
	offset = dev->xfer_file_offset;
	count = dev->xfer_file_length;

	log_dbg("(%lld)\n", count);
	if (!IS_ALIGNED(count, dev->ep_out->maxpacket))
		log_dbg("- count(%lld) not multiple of mtu(%d)\n", count, dev->ep_out->maxpacket);

	mutex_lock(&dev->read_mutex);
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->state == e_offline) {
		r = -EIO;
		spin_unlock_irqrestore(&dev->lock, flags);
		goto fail;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	while (count > 0 || write_req) {
		if (count > 0) {
			/*get a request */
			read_req = dev->rx_req[cur_buf];
			cur_buf = (cur_buf + 1) % RX_REQ_MAX;

			/*some h/w expects size to be aligned to ep's MTU */
			read_req->length = rx_req_len;

			dev->rx_done = 0;
			ret = usb_ep_queue(dev->ep_out, read_req, GFP_KERNEL);
			if (ret < 0) {
				r = -EIO;
				spin_lock_irqsave(&dev->lock, flags);
				if (dev->state != e_offline)
					dev->state = e_error;
				spin_unlock_irqrestore(&dev->lock, flags);
				break;
			}
		}

		if (write_req) {
			log_dbg("rx %pK %d\n", write_req, write_req->actual);
			start_time = ktime_get();
			/*ret = vfs_write(filp, write_req->buf, write_req->actual, &offset);*/
			ret = -1;/*no used*/
			log_dbg("vfs_write %d\n", ret);
			if (ret != write_req->actual) {
				r = -EIO;
				spin_lock_irqsave(&dev->lock, flags);
				if (dev->state != e_offline)
					dev->state = e_error;
				spin_unlock_irqrestore(&dev->lock, flags);
				if (read_req && !dev->rx_done)
					usb_ep_dequeue(dev->ep_out, read_req);
				break;
			}
			dev->history[dev->dbg_write_index].vfs_wbytes = ret;
			dev->history[dev->dbg_write_index].vfs_wtime = ktime_to_us(ktime_sub(ktime_get(), start_time));
			dev->dbg_write_index = (dev->dbg_write_index + 1) % MAX_HISTORY_NUM;
			write_req = NULL;
		}

		if (read_req) {
			/*wait for our last read to complete*/
			ret = wait_event_interruptible(dev->read_wq, dev->rx_done || dev->state != e_busy);

			spin_lock_irqsave(&dev->lock, flags);
			if (dev->state == e_cancel || dev->state == e_offline) {
				if (dev->state == e_offline)
					r = -EIO;
				else
					r = -ECANCELED;
				if (!dev->rx_done)
					usb_ep_dequeue(dev->ep_out, read_req);
				spin_unlock_irqrestore(&dev->lock, flags);
				break;
			} else  {
				spin_unlock_irqrestore(&dev->lock, flags);
			}

			if (read_req->status) {
				r = read_req->status;
				break;
			}

			/*Check if we aligned the size due to MTU constraint */
			if (count < read_req->length)
				read_req->actual = (read_req->actual > count ? count : read_req->actual);

			/* if xfer_file_length is 0xFFFFFFFF, then we read until
			 * we get a zero length packet
			 */
			if (count != 0xFFFFFFFF)
				count -= read_req->actual;
			if (read_req->actual < read_req->length) {
				/*
				 * short packet is used to signal EOF for
				 * sizes > 4 gig
				 */
				log_dbg("got short packet\n");
				count = 0;
			}

			write_req = read_req;
			read_req = NULL;
		}
	}
fail:
	mutex_unlock(&dev->read_mutex);
	log_dbg("returning %d\n", r);
	dev->xfer_result = r;
	smp_wmb();
}

/*
 * Function: init_dtp_dev - initialize lock,wait queue,work queue
 * */
static int init_dtp_dev(struct dtp_instance *inst)
{
	struct dtp_dev *dev = NULL;
	int ret = 0;

	if (!inst) {
		log_err("cant found inst\n");
		return -EINVAL;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		log_err("alloc dev failed\n");
		return -ENOMEM;
	}

	inst->dev = dev;

	spin_lock_init(&dev->lock);
	mutex_init(&dev->read_mutex);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	init_waitqueue_head(&dev->intr_wq);

	atomic_set(&dev->refcnt, 0);
	atomic_set(&dev->ioctl_refcnt, 0);

	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->intr_idle);

	dev->wq = create_singlethread_workqueue("gadget_dtp");
	if (!dev->wq) {
		ret = -ENOMEM;
		log_err("create work queue failed!\n");
		goto err1;
	}

	INIT_WORK(&dev->send_file_work, send_file_work);
	INIT_WORK(&dev->receive_file_work, receive_file_work);

	dev->state = e_offline;

	/*initialize dtp dev success*/
	_dtp_dev = dev;

	/*initialize misc device to expose the API to the upper layer, such as HIDL*/
	ret = misc_register(&usb_device);
	if (ret) {
		log_err("register misc device failed, ret(%d)\n", ret);
		goto err2;
	}

	debug_debugfs_init();/*debugfs*/

	return 0;

err2:
	destroy_workqueue(dev->wq);
err1:
	_dtp_dev = NULL;
	kfree(dev);
	return ret;

}

static void uninit_dtp_dev(struct dtp_instance *inst)
{
	struct dtp_dev *dev = inst->dev;

	if(!dev)
		return;

	if (root_dentry) {
		debugfs_remove_recursive(root_dentry);
		root_dentry = NULL;
	}
	misc_deregister(&usb_device);
	destroy_workqueue(dev->wq);
	kfree(dev);
	_dtp_dev = NULL;
}


static void configfs_item_release(struct config_item *item)
{
	struct dtp_instance *inst = ci_to_instance(item);

	usb_put_function_instance(&inst->func_inst);
}

static struct configfs_item_operations configfs_item_ops = {
	.release = configfs_item_release,
};

static struct config_item_type configfs_item_type = {
	.ct_item_ops = &configfs_item_ops,
	.ct_owner    = THIS_MODULE,
};


/*
 * Function: set_instance_name - set the usb function instance name when boot sequence
 * */
static int set_instance_name(struct usb_function_instance *fi, const char *name)
{
	struct dtp_instance *inst = to_instance(fi);
	char *ptr = NULL;
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > MAX_NAME_LEN) {
		return -ENAMETOOLONG;
	}

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr) {
		return -ENOMEM;
	}

	inst->name = ptr;

	return 0;
}

/*
 * Function: free_func_instance - uninit dtp_dev and free instance name
 * */
static void free_func_instance(struct usb_function_instance *fi)
{
	struct dtp_instance *inst = to_instance(fi);

	if (!inst)
		return;

	uninit_dtp_dev(inst);
	if (inst->name)
		kfree(inst->name);
	kfree(inst);
}


/*
 * Function: create_bulk_endpoints - create usb in/out/intr endpoint for transfer
 * create endpoints and allocate usb_request
 * */
static int create_bulk_endpoints(struct dtp_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc,
				struct usb_endpoint_descriptor *intr_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = NULL;
	struct usb_ep *ep = NULL;
	int i = 0;


	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		log_err("create ep in failed\n");
		return -ENODEV;
	}
	ep->driver_data = dev;
	dev->ep_in = ep;
	log_dbg("ep in: %s\n", ep->name);

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		log_err("create ep out failed\n");
		return -ENODEV;
	}
	ep->driver_data = dev;
	dev->ep_out = ep;
	log_dbg("ep out: %s\n", ep->name);

	ep = usb_ep_autoconfig(cdev->gadget, intr_desc);
	if (!ep) {
		log_err("create ep intr failed\n");
		return -ENODEV;
	}
	ep->driver_data = dev;
	dev->ep_intr = ep;
	log_dbg("ep intr: %s\n", ep->name);

	/*allocate usb requests*/
retry_tx_alloc:
	for (i = 0; i < tx_reqs; i++) {
		req = usb_request_new(dev->ep_in, tx_req_len);
		if (!req) {
			if (tx_req_len <= BULK_BUFFER_SIZE)
				goto fail;
			while ((req = usb_request_get(dev, &dev->tx_idle)))
				usb_request_free(req, dev->ep_in);
			tx_req_len = BULK_BUFFER_SIZE;
			tx_reqs = TX_REQ_MAX;
			log_err("alloc tx buffer failed\n");
			goto retry_tx_alloc;
		}
		req->complete = usb_request_complete_in;
		usb_request_add(dev, &dev->tx_idle, req);
	}

	/*
	 * The RX buffer should be aligned to EP max packet for
	 * some controllers.  At bind time, we don't know the
	 * operational speed.  Hence assuming super speed max
	 * packet size.
	 */
	if (rx_req_len % 1024)
		rx_req_len = BULK_BUFFER_SIZE;

retry_rx_alloc:
	for (i = 0; i < RX_REQ_MAX; i++) {
		req = usb_request_new(dev->ep_out, rx_req_len);
		if (!req) {
			if (rx_req_len <= BULK_BUFFER_SIZE)
				goto fail;

			for (--i; i >= 0; i--)
				usb_request_free(dev->rx_req[i], dev->ep_out);
			rx_req_len = BULK_BUFFER_SIZE;
			log_err("alloc rx buffer failed\n");
			goto retry_rx_alloc;
		}
		req->complete = usb_request_complete_out;
		dev->rx_req[i] = req;
	}

	for (i = 0; i < INTR_REQ_MAX; i++) {
		req = usb_request_new(dev->ep_intr, INTR_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = usb_request_complete_intr;
		usb_request_add(dev, &dev->intr_idle, req);
	}

	return 0;

fail:
	log_err("create bulk endpoints failed\n");
	return -1;
}

/*
 *Function: _function_setup - be called by function_setup, Respond to the usb ctl request from the host side, such as query device status
 * */
static int _function_setup(struct usb_composite_dev *cdev, const struct usb_ctrlrequest *ctrl)
{
	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 w_length = le16_to_cpu(ctrl->wLength);
	struct dtp_dev *dev = get_dtp_dev();
	unsigned long flags;
	int value = -EOPNOTSUPP;
	int rc = -1;

	log_dbg("%02x.%02x v%04x i%04x l%u\n", ctrl->bRequestType, ctrl->bRequest, w_value, w_index, w_length);

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		if (ctrl->bRequest == REQUEST_SET_DEVICE_STATUS && (ctrl->bRequestType & USB_DIR_IN)) {
			if (w_value == e_cancel) {
				spin_lock_irqsave(&dev->lock, flags);
				if (dev->state == e_busy) {
					dev->state = e_cancel;
					spin_unlock_irqrestore(&dev->lock, flags);
					wake_up(&dev->read_wq);
					wake_up(&dev->write_wq);
				} else
					spin_unlock_irqrestore(&dev->lock, flags);
				log_dbg("requset cancel\n");

			} else if(w_value == e_ready) {

				spin_lock_irqsave(&dev->lock, flags);
				dev->state = e_ready;/*ready*/
				spin_unlock_irqrestore(&dev->lock, flags);
				/*readers may be blocked waiting for us to go online, prevent waiting forever */
				wake_up(&dev->read_wq);
				log_dbg("requset goto ready\n");
			}
			value = w_length;
			memcpy(cdev->req->buf, &dev->state, value);
		}

	}

	/* respond with data */
	if (value >= 0) {

		cdev->req->zero = value < w_length;
		cdev->req->length = value;
		rc = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (rc < 0) {
			log_err("queue error\n");
		}
	}
	return value;
}

/*
 * Function: function_bind - binding the usb_function, be called when the 2st of plugging
 * create usb endpoints
 * */
static int function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct dtp_instance *inst = to_instance(f->fi);
	struct usb_composite_dev *cdev = c->cdev;
	struct dtp_dev *dev = to_dtp_dev(f);
	unsigned max_burst = 0;
	int id = 0;
	int ret = 0;


	log_dbg("dev: %pK\n", dev);

	/*initialize usb_composite_dev*/
	dev->cdev = cdev;

	/*allocate interface ID*/
	id = usb_interface_id(c, f);
	if (id < 0) {
		log_err("allocate interface id failed,id(%d)\n", id);
		return id;
	}

	//usb_interface_desc.bInterfaceNumber = id;

	if (usb_string_defs[INTERFACE_STRING_INDEX].id == 0) {
		ret = usb_string_id(c->cdev);
		if (ret < 0) {
			log_err("allocate an unused string ID failed,ret(%d)\n", ret);
			return ret;
		}
		usb_string_defs[INTERFACE_STRING_INDEX].id = ret;
		usb_interface_desc.iInterface = ret;
	}


	if (cdev->use_os_string) {
		f->os_desc_table = kzalloc(sizeof(*f->os_desc_table), GFP_KERNEL);
		if (!f->os_desc_table) {
			log_err("allocate os desc table failed\n");
			return -ENOMEM;
		}
		f->os_desc_n = 1;
		f->os_desc_table[0].os_desc = &inst->os_desc;
	}

	/*create usb endpoints */
	ret = create_bulk_endpoints(dev, &fullspeed_ep_desc_in, &fullspeed_ep_desc_out, &intr_ep_desc);
	if (ret)
		return ret;

	/*support high speed hardware*/
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		highspeed_ep_desc_in.bEndpointAddress = fullspeed_ep_desc_in.bEndpointAddress;
		highspeed_ep_desc_out.bEndpointAddress = fullspeed_ep_desc_out.bEndpointAddress;
	}
	/*support super speed hardware*/
	if (gadget_is_superspeed(c->cdev->gadget)) {

		/*Calculate bMaxBurst, we know packet size is 1024*/
		max_burst = min_t(unsigned, BULK_BUFFER_SIZE / 1024, 15);
		ss_ep_desc_in.bEndpointAddress = fullspeed_ep_desc_in.bEndpointAddress;
		ss_ep_comp_desc_in.bMaxBurst = max_burst;
		ss_ep_desc_out.bEndpointAddress = fullspeed_ep_desc_out.bEndpointAddress;
		ss_ep_comp_desc_out.bMaxBurst = max_burst;
	}

	inst->func_inst.f = &dev->function;

	log_dbg("%s speed %s: IN/%s, OUT/%s\n",
		gadget_is_superspeed(c->cdev->gadget) ? "super" :
		(gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full"),
		f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

/*
 * Function: function_unbind - be called when the 2st of unpluging
 * release all of the usb_request
 * */
static void function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct dtp_instance *inst = to_instance(f->fi);
	struct dtp_dev *dev = to_dtp_dev(f);
	struct usb_request *req = NULL;
	unsigned long flags;
	int i = 0;

	log_dbg("dev: %pK\n", dev);

	usb_string_defs[INTERFACE_STRING_INDEX].id = 0;

	mutex_lock(&dev->read_mutex);

	while ((req = usb_request_get(dev, &dev->tx_idle)))
		usb_request_free(req, dev->ep_in);

	for (i = 0; i < RX_REQ_MAX; i++)
		usb_request_free(dev->rx_req[i], dev->ep_out);

	while ((req = usb_request_get(dev, &dev->intr_idle)))
		usb_request_free(req, dev->ep_intr);

	spin_lock_irqsave(&dev->lock, flags);
	dev->state = e_offline;/*offline*/
	spin_unlock_irqrestore(&dev->lock, flags);
	dev->cdev = NULL;

	mutex_unlock(&dev->read_mutex);

	if (f->os_desc_table)
		kfree(f->os_desc_table);
	f->os_desc_n = 0;
	inst->func_inst.f = NULL;
}

/*
 * Function: function_disable - be called when the 1st of unpluging
 * disable all of the usb endpoint
 * */
static void function_disable(struct usb_function *f)
{
	struct dtp_dev *dev = to_dtp_dev(f);
	unsigned long flags;

	log_dbg("dev: %pK\n", dev);

	spin_lock_irqsave(&dev->lock, flags);
	dev->state = e_offline;/*offline*/
	spin_unlock_irqrestore(&dev->lock, flags);

	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);
	usb_ep_disable(dev->ep_intr);

	/*readers may be blocked waiting for us to go online, prevent readers waiting forever*/
	wake_up(&dev->read_wq);

}

/*
 * Function function_set_alt - be called when the 4st of plugging
 * config and enable all of the usb endpoint
 * */
static int function_set_alt(struct usb_function *f,	unsigned intf, unsigned alt)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct dtp_dev	*dev = to_dtp_dev(f);
	int ret = -1;

	log_dbg("%d alt: %d\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_intr);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_intr);
	if (ret) {
		usb_ep_disable(dev->ep_out);
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	return 0;
}

/*
 *Function: function_setup - be called when the 3st of plugging
 * */
static int function_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	return _function_setup(f->config->cdev, ctrl);
}

static void function_free(struct usb_function *f)
{
	struct dtp_dev	*dev = to_dtp_dev(f);

	log_dbg("dev: %pK\n", dev);
}


/*
 * Function: alloc_usb_func_instance - be called when the 1st of booting up
 * */
static struct usb_function_instance *alloc_usb_func_instance(void)
{
	struct dtp_instance *inst = NULL;
	struct usb_os_desc *descs[1];
	char *names[1];
	int ret = 0;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		log_err("alloc inst failed\n");
		return ERR_PTR(-ENOMEM);
	}

	/*initialize struct usb_function_instance*/
	inst->func_inst.set_inst_name = set_instance_name;
	inst->func_inst.free_func_inst = free_func_instance;

	inst->os_desc.ext_compat_id = inst->ext_compat_id;
	INIT_LIST_HEAD(&inst->os_desc.ext_prop);
	descs[0] = &inst->os_desc;
	names[0] = "DTP";

	/*initialize struct dtp_dev*/
	ret = init_dtp_dev(inst);
	if (ret) {
		kfree(inst);
		log_err("init dev failed, ret(%d)\n", ret);
		return ERR_PTR(ret);
	}

	/*initialize configfs*/
	config_group_init_type_name(&inst->func_inst.group, "", &configfs_item_type);
	usb_os_desc_prepare_interf_dir(&inst->func_inst.group, 1, descs, names, THIS_MODULE);


	log_dbg("alloc usb func instance success\n");

	return &inst->func_inst;
}

/*
 * Function: alloc_usb_func - register usb function, be called when the 1st of plugging
 * */
static struct usb_function *alloc_usb_func(struct usb_function_instance *fi)
{
	struct dtp_instance *inst = to_instance(fi);
	struct dtp_dev *dev = inst->dev;

	if (dev == NULL) {
		log_err("cant found dev\n");
		return ERR_PTR(-EINVAL);
	}


	dev->function.name = DRIVER_NAME;
	dev->function.strings = usb_strings;
	/*initialize usb interface descriptor, usb endpoint descriptor*/
	dev->function.fs_descriptors = fs_descs;
	dev->function.hs_descriptors = hs_descs;
	dev->function.ss_descriptors = ss_descs;
	dev->function.ssp_descriptors = ss_descs;

	/*initialize the struct usb_function, the bind,unbind func would be called when plugging/unpluging USB*/
	dev->function.bind = function_bind;
	dev->function.unbind = function_unbind;
	dev->function.set_alt = function_set_alt;
	dev->function.disable = function_disable;
	dev->function.setup = function_setup;
	dev->function.free_func = function_free;
	fi->f = &dev->function;

	log_dbg("alloc usb func success\n");
	return &dev->function;
}

/*
 * Boot up sequence
 * 1st: alloc_usb_func_instance
 * */

/*
 * Bind sequence         (plugging usb)
 * 1st: alloc_usb_func
 * 2st: function_bind    (create usb endpoint and usb requests list)
 * 3st: function_setup   (response the ctl requests from the host side)
 * 4st: function_set_alt (enable usb endpoint and mark device is ready)
 * 5st: function_setup   (response the ctl requests from the host side aggin)
 */

/*
 * Unbind sequence       (unpluging usb)
 * 1st: function_disable (disable usb endpoint)
 * 2st: function_unbind  (release usb requests list)
 * */

DECLARE_USB_FUNCTION(dtp, alloc_usb_func_instance, alloc_usb_func);

static int gadget_dtp_init(void)
{
	return usb_function_register(&dtpusb_func);
}
module_init(gadget_dtp_init);

static void __exit gadget_dtp_exit(void)
{
	usb_function_unregister(&dtpusb_func);
}
module_exit(gadget_dtp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deng yongjian <dengyongjian@xiaomi.com>");
MODULE_DESCRIPTION("Gadget DTP function driver");
