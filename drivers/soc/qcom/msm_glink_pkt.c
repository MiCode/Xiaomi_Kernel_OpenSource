/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * G-link Packet Driver -- Provides a binary G-link non-muxed packet port
 *                       interface.
 */

#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <asm/ioctls.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/ipc_logging.h>
#include <linux/termios.h>

#include <soc/qcom/glink.h>

#define MODULE_NAME "msm_glinkpkt"
#define DEVICE_NAME "glinkpkt"
#define WAKEUPSOURCE_TIMEOUT (2000) /* two seconds */
#define CLOSE_WAIT_TIMEOUT 1000 /* one seconds */

#define GLINK_PKT_IOCTL_MAGIC (0xC3)

#define GLINK_PKT_IOCTL_QUEUE_RX_INTENT \
	_IOW(GLINK_PKT_IOCTL_MAGIC, 0, unsigned int)

#define SMD_DTR_SIG BIT(31)
#define SMD_CTS_SIG BIT(30)
#define SMD_CD_SIG BIT(29)
#define SMD_RI_SIG BIT(28)

#define map_to_smd_trans_signal(sigs) \
	do { \
		sigs &= 0x0fff; \
		if (sigs & TIOCM_DTR) \
			sigs |= SMD_DTR_SIG; \
		if (sigs & TIOCM_RTS) \
			sigs |= SMD_CTS_SIG; \
		if (sigs & TIOCM_CD) \
			sigs |= SMD_CD_SIG; \
		if (sigs & TIOCM_RI) \
			sigs |= SMD_RI_SIG; \
	} while (0)

#define map_from_smd_trans_signal(sigs) \
	do { \
		if (sigs & SMD_DTR_SIG) \
			sigs |= TIOCM_DTR; \
		if (sigs & SMD_CTS_SIG) \
			sigs |= TIOCM_RTS; \
		if (sigs & SMD_CD_SIG) \
			sigs |= TIOCM_CD; \
		if (sigs & SMD_RI_SIG) \
			sigs |= TIOCM_RI; \
		sigs &= 0x0fff; \
	} while (0)

/**
 * glink_pkt_dev - G-Link packet device structure
 * dev_list:	G-Link packets device list.
 * open_cfg:	Transport configuration used to open Logical channel.
 * dev_name:	Device node name used by the clients.
 * handle:	Opaque Channel handle returned by G-Link.
 * ch_lock:	Per channel lock for synchronization.
 * ch_satet:	flag used to check the channel state.
 * cdev:	structure to the internal character device.
 * devicep:	Pointer to the G-Link pkt class device structure.
 * i:		Index to this character device.
 * ref_cnt:	number of references to this device.
 * poll_mode:	flag to check polling mode.
 * ch_read_wait_queue:	reader thread wait queue.
 * ch_opened_wait_queue: open thread wait queue.
 * ch_closed_wait_queue: close thread wait queue.
 * pkt_list:	The pending Rx packets list.
 * pkt_list_lock: Lock to protect @pkt_list.
 * pa_ws:	Packet arrival Wakeup source.
 * packet_arrival_work:	Hold the wakeup source worker info.
 * pa_spinlock:	Packet arrival spinlock.
 * ws_locked:	flag to check wakeup source state.
 * sigs_updated: flag to check signal update.
 * open_time_wait: wait time for channel to fully open.
 * in_reset:	flag to check SSR state.
 * link_info:	structure to hold link information.
 * link_state_handle: handle to get link state events.
 * link_up:	flag to check link is up or not.
 */
struct glink_pkt_dev {
	struct list_head dev_list;
	struct glink_open_config open_cfg;
	const char *dev_name;
	void *handle;
	struct mutex ch_lock;
	unsigned ch_state;

	struct cdev cdev;
	struct device *devicep;

	int i;
	int ref_cnt;
	int poll_mode;

	wait_queue_head_t ch_read_wait_queue;
	wait_queue_head_t ch_opened_wait_queue;
	wait_queue_head_t ch_closed_wait_queue;
	struct list_head pkt_list;
	spinlock_t pkt_list_lock;

	struct wakeup_source pa_ws;	/* Packet Arrival Wakeup Source */
	struct work_struct packet_arrival_work;
	spinlock_t pa_spinlock;
	int ws_locked;
	int sigs_updated;
	int open_time_wait;
	int in_reset;

	struct glink_link_info link_info;
	void *link_state_handle;
	bool link_up;
};

/**
 * glink_rx_pkt - Pointer to Rx packet
 * list:	Chain the Rx packets into list.
 * data:	pointer to the Rx data.
 * pkt_ptiv:	private pointer to the Rx packet.
 * size:	The size of received data.
 */
struct glink_rx_pkt {
	struct list_head list;
	const void *data;
	const void *pkt_priv;
	size_t size;
};

/**
 * queue_rx_intent_work - Work item to Queue Rx intent.
 * size:	The size of intent to be queued.
 * devp:	Pointer to the device structure.
 * work:	Hold the worker function information.
 */
struct queue_rx_intent_work {
	size_t intent_size;
	struct glink_pkt_dev *devp;
	struct work_struct work;
};

/**
 * notify_state_work - Work item to notify channel state.
 * state:	Channel new state.
 * devp:	Pointer to the device structure.
 * work:	Hold the worker function information.
 */
struct notify_state_work {
	unsigned state;
	struct glink_pkt_dev *devp;
	void *handle;
	struct work_struct work;
};

static DEFINE_MUTEX(glink_pkt_dev_lock_lha1);
static LIST_HEAD(glink_pkt_dev_list);
static DEFINE_MUTEX(glink_pkt_driver_lock_lha1);
static LIST_HEAD(glink_pkt_driver_list);

struct class *glink_pkt_classp;
static dev_t glink_pkt_number;
struct workqueue_struct *glink_pkt_wq;

static int num_glink_pkt_ports;

#define GLINK_PKT_IPC_LOG_PAGE_CNT 2
static void *glink_pkt_ilctxt;

enum {
	GLINK_PKT_STATUS = 1U << 0,
};

static int msm_glink_pkt_debug_mask;
module_param_named(debug_mask, msm_glink_pkt_debug_mask,
		int, S_IRUGO | S_IWUSR | S_IWGRP);

static void glink_pkt_queue_rx_intent_worker(struct work_struct *work);
static void glink_pkt_notify_state_worker(struct work_struct *work);
static bool glink_pkt_read_avail(struct glink_pkt_dev *devp);

#define DEBUG

#ifdef DEBUG

#define GLINK_PKT_LOG_STRING(x...) \
do { \
	if (glink_pkt_ilctxt) \
		ipc_log_string(glink_pkt_ilctxt, "<GLINK_PKT>: "x); \
} while (0)

#define GLINK_PKT_INFO(x...) \
do { \
	if (msm_glink_pkt_debug_mask & GLINK_PKT_STATUS) \
		pr_info("Status: "x); \
	GLINK_PKT_LOG_STRING(x); \
} while (0)

#define GLINK_PKT_ERR(x...) \
do { \
	pr_err_ratelimited("<GLINK_PKT> err: "x); \
	GLINK_PKT_LOG_STRING(x); \
} while (0)

#else
#define GLINK_PKT_INFO(x...) do {} while (0)
#define GLINK_PKT_ERR(x...) do {} while (0)
#endif

static ssize_t open_timeout_store(struct device *d,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	struct glink_pkt_dev *devp;
	long tmp;

	mutex_lock(&glink_pkt_dev_lock_lha1);
	list_for_each_entry(devp, &glink_pkt_dev_list, dev_list) {
		if (devp->devicep == d) {
			if (!kstrtol(buf, 0, &tmp)) {
				devp->open_time_wait = tmp;
				mutex_unlock(&glink_pkt_dev_lock_lha1);
				return n;
			} else {
				mutex_unlock(&glink_pkt_dev_lock_lha1);
				pr_err("%s: unable to convert: %s to an int\n",
						__func__, buf);
				return -EINVAL;
			}
		}
	}
	mutex_unlock(&glink_pkt_dev_lock_lha1);
	GLINK_PKT_ERR("%s: unable to match device to valid port\n", __func__);
	return -EINVAL;
}

static ssize_t open_timeout_show(struct device *d,
				 struct device_attribute *attr,
				 char *buf)
{
	struct glink_pkt_dev *devp;

	mutex_lock(&glink_pkt_dev_lock_lha1);
	list_for_each_entry(devp, &glink_pkt_dev_list, dev_list) {
		if (devp->devicep == d) {
			mutex_unlock(&glink_pkt_dev_lock_lha1);
			return snprintf(buf, PAGE_SIZE, "%d\n",
					devp->open_time_wait);
		}
	}
	mutex_unlock(&glink_pkt_dev_lock_lha1);
	GLINK_PKT_ERR("%s: unable to match device to valid port\n", __func__);
	return -EINVAL;

}

static DEVICE_ATTR(open_timeout, 0664, open_timeout_show, open_timeout_store);

/**
 * packet_arrival_worker() - wakeup source timeout worker fn
 * work:	Work struct queued
 *
 * This function used to keep the system awake to allow
 * userspace client to read the received packet.
 */
static void packet_arrival_worker(struct work_struct *work)
{
	struct glink_pkt_dev *devp;
	unsigned long flags;

	devp = container_of(work, struct glink_pkt_dev,
				    packet_arrival_work);
	mutex_lock(&devp->ch_lock);
	spin_lock_irqsave(&devp->pa_spinlock, flags);
	if (devp->ws_locked) {
		GLINK_PKT_INFO("%s locking glink_pkt_dev id:%d wakeup source\n",
			__func__, devp->i);
		/*
		 * Keep system awake long enough to allow userspace client
		 * to process the packet.
		 */
		__pm_wakeup_event(&devp->pa_ws, WAKEUPSOURCE_TIMEOUT);
	}
	spin_unlock_irqrestore(&devp->pa_spinlock, flags);
	mutex_unlock(&devp->ch_lock);
}

/**
 * glink_pkt_link_state_cb() - Callback to receive link state updates
 * @cb_info: Information containing link & its state.
 * @priv: Private data passed during the link state registration.
 *
 * This function is called by the GLINK core to notify the Glink Pkt drivers
 * regarding the link state updates. This function is registered with the
 * GLINK core by Glink pkt drivers with glink_register_link_state_cb().
 */
static void glink_pkt_link_state_cb(struct glink_link_state_cb_info *cb_info,
				      void *priv)
{
	struct glink_pkt_dev *devp = (struct glink_pkt_dev *)priv;

	if (!cb_info)
		return;
	if (!devp)
		return;

	if (cb_info->link_state == GLINK_LINK_STATE_UP) {
		devp->link_up = true;
		wake_up_interruptible(&devp->ch_opened_wait_queue);
	} else if (cb_info->link_state == GLINK_LINK_STATE_DOWN) {
		devp->link_up = false;
	}
}

/**
 * glink_pkt_notify_rx() - Rx data Callback from G-Link core layer
 * handle:	Opaque Channel handle returned by GLink.
 * priv:	private pointer to the channel.
 * pkt_priv:	private pointer to the packet.
 * ptr:	Pointer to the Rx data.
 * size:	Size of the Rx data.
 *
 * This callback function is notified on receiving the data from
 * remote channel.
 */
void glink_pkt_notify_rx(void *handle, const void *priv,
				const void *pkt_priv,
				const void *ptr, size_t size)
{
	struct glink_rx_pkt *pkt = NULL;
	struct glink_pkt_dev *devp = (struct glink_pkt_dev *)priv;
	unsigned long flags;

	GLINK_PKT_INFO("%s(): priv[%p] data[%p] size[%zu]\n",
		   __func__, pkt_priv, (char *)ptr, size);

	pkt = kzalloc(sizeof(*pkt), GFP_ATOMIC);
	if (!pkt) {
		GLINK_PKT_ERR("%s: memory allocation failed\n", __func__);
		return;
	}

	pkt->data = ptr;
	pkt->pkt_priv = pkt_priv;
	pkt->size = size;
	spin_lock_irqsave(&devp->pkt_list_lock, flags);
	list_add_tail(&pkt->list, &devp->pkt_list);
	spin_unlock_irqrestore(&devp->pkt_list_lock, flags);

	spin_lock_irqsave(&devp->pa_spinlock, flags);
	__pm_stay_awake(&devp->pa_ws);
	devp->ws_locked = 1;
	spin_unlock_irqrestore(&devp->pa_spinlock, flags);
	wake_up(&devp->ch_read_wait_queue);
	schedule_work(&devp->packet_arrival_work);
	return;
}

/**
 * glink_pkt_notify_tx_done() - Tx done callback function
 * handle:	Opaque Channel handle returned by GLink.
 * priv:	private pointer to the channel.
 * pkt_priv:	private pointer to the packet.
 * ptr:	Pointer to the Tx data.
 *
 * This  callback function is notified when the remote core
 * signals the Rx done to the local core.
 */
void glink_pkt_notify_tx_done(void *handle, const void *priv,
				const void *pkt_priv, const void *ptr)
{
	GLINK_PKT_INFO("%s(): priv[%p] pkt_priv[%p] ptr[%p]\n",
					__func__, priv, pkt_priv, ptr);
/* Free Tx buffer allocated in glink_pkt_write */
	kfree(ptr);
}

/**
 * glink_pkt_notify_state() - state notification callback function
 * handle:	Opaque Channel handle returned by GLink.
 * priv:	private pointer to the channel.
 * event:	channel state
 *
 * This callback function is notified when the remote channel alters
 * the channel state and send the event to local G-Link core.
 */
void glink_pkt_notify_state(void *handle, const void *priv, unsigned event)
{
	struct glink_pkt_dev *devp = (struct glink_pkt_dev *)priv;
	struct notify_state_work *work_item;

	if ((devp->handle != NULL) && (devp->handle != handle)) {
		GLINK_PKT_ERR("%s() event[%d] on incorrect channel [%s]\n",
				__func__, event, devp->open_cfg.name);
		return;
	}
	GLINK_PKT_INFO("%s(): event[%d] on [%s]\n", __func__, event,
						devp->open_cfg.name);

	work_item = kzalloc(sizeof(*work_item), GFP_ATOMIC);
	if (!work_item) {
		GLINK_PKT_ERR("%s() failed allocate work_item\n", __func__);
		return;
	}

	work_item->state = event;
	work_item->devp = devp;
	work_item->handle = handle;
	INIT_WORK(&work_item->work, glink_pkt_notify_state_worker);
	queue_work(glink_pkt_wq, &work_item->work);
}

/**
 * glink_pkt_rmt_rx_intent_req_cb() - Remote Rx intent request callback
 * handle:	Opaque Channel handle returned by GLink.
 * priv:	private pointer to the channel.
 * sz:	the size of the requested Rx intent
 *
 * This callback function is notified when remote client
 * request the intent from local client.
 */
bool glink_pkt_rmt_rx_intent_req_cb(void *handle, const void *priv, size_t sz)
{
	struct queue_rx_intent_work *work_item;
	GLINK_PKT_INFO("%s(): QUEUE RX INTENT to receive size[%zu]\n",
		   __func__, sz);

	work_item = kzalloc(sizeof(*work_item), GFP_ATOMIC);
	if (!work_item) {
		GLINK_PKT_ERR("%s failed allocate work_item\n", __func__);
		return false;
	}

	work_item->intent_size = sz;
	work_item->devp = (struct glink_pkt_dev *)priv;
	INIT_WORK(&work_item->work, glink_pkt_queue_rx_intent_worker);
	queue_work(glink_pkt_wq, &work_item->work);

	return true;
}

/**
 * glink_pkt_notify_rx_sigs() - signals callback
 * handle:      Opaque Channel handle returned by GLink.
 * priv:        private pointer to the channel.
 * old_sigs:    signal before modification
 * new_sigs:    signal after modification
 *
 * This callback function is notified when remote client
 * updated the signal.
 */
void glink_pkt_notify_rx_sigs(void *handle, const void *priv,
			uint32_t old_sigs, uint32_t new_sigs)
{
	struct glink_pkt_dev *devp = (struct glink_pkt_dev *)priv;
	GLINK_PKT_INFO("%s(): sigs old[%x] new[%x]\n",
				__func__, old_sigs, new_sigs);
	mutex_lock(&devp->ch_lock);
	devp->sigs_updated = true;
	mutex_unlock(&devp->ch_lock);
	wake_up(&devp->ch_read_wait_queue);
}

/**
 * glink_pkt_queue_rx_intent_worker() - Queue Rx worker function
 *
 * work:	Pointer to the work struct
 *
 * This function is used to queue the RX intent which
 * can sleep during allocation of larger buffers.
 */
static void glink_pkt_queue_rx_intent_worker(struct work_struct *work)
{
	int ret;
	struct queue_rx_intent_work *work_item =
				container_of(work,
				struct queue_rx_intent_work, work);
	struct glink_pkt_dev *devp = work_item->devp;

	if (!devp) {
		GLINK_PKT_ERR("%s: Invalid device\n", __func__);
		kfree(work_item);
		return;
	}
	mutex_lock(&devp->ch_lock);
	if (!devp->handle) {
		GLINK_PKT_ERR("%s: Invalid device Handle\n", __func__);
		mutex_unlock(&devp->ch_lock);
		kfree(work_item);
		return;
	}

	ret = glink_queue_rx_intent(devp->handle, devp, work_item->intent_size);
	mutex_unlock(&devp->ch_lock);
	GLINK_PKT_INFO("%s: Triggered with size[%zu] ret[%d]\n",
				__func__, work_item->intent_size, ret);
	if (ret)
		GLINK_PKT_ERR("%s queue_rx_intent failed\n", __func__);
	kfree(work_item);
}

/**
 * glink_pkt_notify_state_worker() - Notify state worker function
 *
 * work:	Pointer to the work struct
 *
 * This function is used to notify the channel state and update the
 * internal data structure.
 */
static void glink_pkt_notify_state_worker(struct work_struct *work)
{
	struct notify_state_work *work_item =
				container_of(work,
				struct notify_state_work, work);
	struct glink_pkt_dev *devp = work_item->devp;
	unsigned event = work_item->state;
	void *handle = work_item->handle;

	if (!devp) {
		GLINK_PKT_ERR("%s: Invalid device Handle\n", __func__);
		kfree(work_item);
		return;
	}

	GLINK_PKT_INFO("%s(): event[%d] on [%s]\n", __func__,
				event, devp->open_cfg.name);
	mutex_lock(&devp->ch_lock);
	devp->ch_state = event;
	if (event == GLINK_CONNECTED) {
		if (!devp->handle)
			devp->handle = handle;
		devp->in_reset = 0;
		wake_up_interruptible(&devp->ch_opened_wait_queue);
	} else if (event == GLINK_REMOTE_DISCONNECTED) {
		devp->in_reset = 1;
		wake_up(&devp->ch_read_wait_queue);
		wake_up_interruptible(&devp->ch_opened_wait_queue);
	} else if (event == GLINK_LOCAL_DISCONNECTED) {
		if (devp->handle == handle)
			devp->handle = NULL;
		wake_up_interruptible(&devp->ch_closed_wait_queue);
	}
	mutex_unlock(&devp->ch_lock);
	kfree(work_item);
}

/**
 * glink_pkt_read_avail() - check any pending packets to read
 * devp:	pointer to G-Link packet device.
 *
 * This function is used to check any pending data packets are
 * available to read or not.
 */
static bool glink_pkt_read_avail(struct glink_pkt_dev *devp)
{
	bool list_is_empty;
	unsigned long flags;

	spin_lock_irqsave(&devp->pkt_list_lock, flags);
	list_is_empty = list_empty(&devp->pkt_list);
	spin_unlock_irqrestore(&devp->pkt_list_lock, flags);
	return !list_is_empty;
}

/**
 * glink_pkt_read() - read() syscall for the glink_pkt device
 * file:	Pointer to the file structure.
 * buf:	Pointer to the userspace buffer.
 * count:	Number bytes to read from the file.
 * ppos:	Pointer to the position into the file.
 *
 * This function is used to Read the data from glink pkt device when
 * userspace client do a read() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
ssize_t glink_pkt_read(struct file *file,
		       char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int ret = 0;
	struct glink_pkt_dev *devp;
	struct glink_rx_pkt *pkt = NULL;
	unsigned long flags;

	devp = file->private_data;

	if (!devp) {
		GLINK_PKT_ERR("%s on NULL glink_pkt_dev\n", __func__);
		return -EINVAL;
	}
	if (!devp->handle) {
		GLINK_PKT_ERR("%s on a closed glink_pkt_dev id:%d\n",
			__func__, devp->i);
		return -EINVAL;
	}
	if (devp->in_reset) {
		GLINK_PKT_ERR("%s: notifying reset for glink_pkt_dev id:%d\n",
			__func__, devp->i);
		return -ENETRESET;
	}

	mutex_lock(&devp->ch_lock);
	if (!glink_rx_intent_exists(devp->handle, count)) {
		ret  = glink_queue_rx_intent(devp->handle, devp, count);
		if (ret) {
			GLINK_PKT_ERR("%s: failed to queue_rx_intent ret[%d]\n",
					__func__, ret);
			mutex_unlock(&devp->ch_lock);
			return ret;
		}
	}
	mutex_unlock(&devp->ch_lock);

	GLINK_PKT_INFO("Begin %s on glink_pkt_dev id:%d buffer_size %zu\n",
		__func__, devp->i, count);

	ret = wait_event_interruptible(devp->ch_read_wait_queue,
				     !devp->handle || devp->in_reset ||
				     glink_pkt_read_avail(devp));
	if (devp->in_reset) {
		GLINK_PKT_ERR("%s: notifying reset for glink_pkt_dev id:%d\n",
			__func__, devp->i);
		return -ENETRESET;
	}
	if (!devp->handle) {
		GLINK_PKT_ERR("%s on a closed glink_pkt_dev id:%d\n",
			__func__, devp->i);
		return -EINVAL;
	}
	if (ret < 0) {
		/* qualify error message */
		if (ret != -ERESTARTSYS) {
			/* we get this anytime a signal comes in */
			GLINK_PKT_ERR("%s: wait on dev id:%d ret %i\n",
					__func__, devp->i, ret);
		}
		return ret;
	}

	spin_lock_irqsave(&devp->pkt_list_lock, flags);
	pkt = list_first_entry(&devp->pkt_list, struct glink_rx_pkt, list);
	if (pkt->size > count) {
		GLINK_PKT_ERR("%s: Small Buff on dev Id:%d-[%zu > %zu]\n",
				__func__, devp->i, pkt->size, count);
		spin_unlock_irqrestore(&devp->pkt_list_lock, flags);
		return -ETOOSMALL;
	}
	list_del(&pkt->list);
	spin_unlock_irqrestore(&devp->pkt_list_lock, flags);

	ret = copy_to_user(buf, pkt->data, pkt->size);
	 if (ret) {
		GLINK_PKT_ERR(
		"%s copy_to_user failed ret[%d] on dev id:%d size %zu\n",
		 __func__, ret, devp->i, pkt->size);
		spin_lock_irqsave(&devp->pkt_list_lock, flags);
		list_add_tail(&pkt->list, &devp->pkt_list);
		spin_unlock_irqrestore(&devp->pkt_list_lock, flags);
		return -EFAULT;
	}


	ret = pkt->size;
	glink_rx_done(devp->handle, pkt->data, false);
	kfree(pkt);

	mutex_lock(&devp->ch_lock);
	spin_lock_irqsave(&devp->pa_spinlock, flags);
	if (devp->poll_mode && !glink_pkt_read_avail(devp)) {
		__pm_relax(&devp->pa_ws);
		devp->ws_locked = 0;
		devp->poll_mode = 0;
		GLINK_PKT_INFO("%s unlocked pkt_dev id:%d wakeup_source\n",
			__func__, devp->i);
	}
	spin_unlock_irqrestore(&devp->pa_spinlock, flags);
	mutex_unlock(&devp->ch_lock);

	GLINK_PKT_INFO("End %s on glink_pkt_dev id:%d ret[%d]\n",
				__func__, devp->i, ret);
	return ret;
}

/**
 * glink_pkt_write() - write() syscall for the glink_pkt device
 * file:	Pointer to the file structure.
 * buf:	Pointer to the userspace buffer.
 * count:	Number bytes to read from the file.
 * ppos:	Pointer to the position into the file.
 *
 * This function is used to write the data to glink pkt device when
 * userspace client do a write() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
ssize_t glink_pkt_write(struct file *file,
		       const char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int ret = 0;
	struct glink_pkt_dev *devp;
	void *data;

	devp = file->private_data;

	if (!count) {
		GLINK_PKT_ERR("%s: data count is zero\n", __func__);
		return -EINVAL;
	}

	if (!devp) {
		GLINK_PKT_ERR("%s on NULL glink_pkt_dev\n", __func__);
		return -EINVAL;
	}
	if (!devp->handle) {
		GLINK_PKT_ERR("%s on a closed glink_pkt_dev id:%d\n",
			__func__, devp->i);
		return -EINVAL;
	}
	if (devp->in_reset) {
		GLINK_PKT_ERR("%s: notifying reset for glink_pkt_dev id:%d\n",
			__func__, devp->i);
		return -ENETRESET;
	};

	GLINK_PKT_INFO("Begin %s on glink_pkt_dev id:%d buffer_size %zu\n",
		__func__, devp->i, count);
	data = kzalloc(count, GFP_KERNEL);
	if (!data) {
		GLINK_PKT_ERR("%s buffer allocation failed\n", __func__);
		return -ENOMEM;
	}

	ret = copy_from_user(data, buf, count);
	if (ret) {
		GLINK_PKT_ERR(
		"%s copy_from_user failed ret[%d] on dev id:%d size %zu\n",
		 __func__, ret, devp->i, count);
		kfree(data);
		return -EFAULT;
	}

	ret = glink_tx(devp->handle, data, data, count, GLINK_TX_REQ_INTENT);
	if (ret) {
		GLINK_PKT_ERR("%s glink_tx failed ret[%d]\n", __func__, ret);
		kfree(data);
		return ret;
	}

	GLINK_PKT_INFO("Finished %s on glink_pkt_dev id:%d buffer_size %zu\n",
		__func__, devp->i, count);

	return count;
}

/**
 * glink_pkt_poll() - poll() syscall for the glink_pkt device
 * file:	Pointer to the file structure.
 * wait:	pointer to Poll table.
 *
 * This function is used to poll on the glink pkt device when
 * userspace client do a poll() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
static unsigned int glink_pkt_poll(struct file *file, poll_table *wait)
{
	struct glink_pkt_dev *devp;
	unsigned int mask = 0;

	devp = file->private_data;
	if (!devp || !devp->handle) {
		GLINK_PKT_ERR("%s: Invalid device handle\n", __func__);
		return POLLERR;
	}
	if (devp->in_reset)
		return POLLHUP;

	devp->poll_mode = 1;
	poll_wait(file, &devp->ch_read_wait_queue, wait);
	mutex_lock(&devp->ch_lock);
	if (!devp->handle) {
		mutex_unlock(&devp->ch_lock);
		return POLLERR;
	}
	if (devp->in_reset) {
		mutex_unlock(&devp->ch_lock);
		return POLLHUP;
	}

	if (glink_pkt_read_avail(devp)) {
		mask |= POLLIN | POLLRDNORM;
		GLINK_PKT_INFO("%s sets POLLIN for glink_pkt_dev id: %d\n",
			__func__, devp->i);
	}

	if (devp->sigs_updated) {
		mask |= POLLPRI;
		GLINK_PKT_INFO("%s sets POLLPRI for glink_pkt_dev id: %d\n",
			__func__, devp->i);
	}
	mutex_unlock(&devp->ch_lock);

	return mask;
}

/**
 * glink_pkt_tiocmset() - set the signals for glink_pkt device
 * devp:	Pointer to the glink_pkt device structure.
 * cmd:		IOCTL command.
 * arg:		Arguments to the ioctl call.
 *
 * This function is used to set the signals on the glink pkt device
 * when userspace client do a ioctl() system call with TIOCMBIS,
 * TIOCMBIC and TICOMSET.
 */
static int glink_pkt_tiocmset(struct glink_pkt_dev *devp, unsigned int cmd,
							unsigned long arg)
{
	int ret;
	uint32_t sigs;
	uint32_t val;

	ret = get_user(val, (uint32_t *)arg);
	if (ret)
		return ret;
	map_to_smd_trans_signal(val);
	ret = glink_sigs_local_get(devp->handle, &sigs);
	if (ret < 0) {
		GLINK_PKT_ERR("%s: Get signals failed[%d]\n", __func__, ret);
		return ret;
	}
	switch (cmd) {
	case TIOCMBIS:
		sigs |= val;
		break;
	case TIOCMBIC:
		sigs &= ~val;
		break;
	case TIOCMSET:
		sigs = val;
		break;
	}
	ret = glink_sigs_set(devp->handle, sigs);
	GLINK_PKT_INFO("%s: sigs[0x%x] ret[%d]\n", __func__, sigs, ret);
	return ret;
}

/**
 * glink_pkt_ioctl() - ioctl() syscall for the glink_pkt device
 * file:	Pointer to the file structure.
 * cmd:		IOCTL command.
 * arg:		Arguments to the ioctl call.
 *
 * This function is used to ioctl on the glink pkt device when
 * userspace client do a ioctl() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
static long glink_pkt_ioctl(struct file *file, unsigned int cmd,
					     unsigned long arg)
{
	int ret;
	struct glink_pkt_dev *devp;
	uint32_t size = 0;
	uint32_t sigs = 0;

	devp = file->private_data;
	if (!devp || !devp->handle) {
		GLINK_PKT_ERR("%s: Invalid device handle\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&devp->ch_lock);
	switch (cmd) {
	case TIOCMGET:
		devp->sigs_updated = false;
		ret = glink_sigs_remote_get(devp->handle, &sigs);
		GLINK_PKT_INFO("%s: TIOCMGET ret[%d] sigs[0x%x]\n",
					__func__, ret, sigs);
		map_from_smd_trans_signal(sigs);
		if (!ret)
			ret = put_user(sigs, (uint32_t *)arg);
		break;
	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
		ret = glink_pkt_tiocmset(devp, cmd, arg);
		break;

	case GLINK_PKT_IOCTL_QUEUE_RX_INTENT:
		ret = get_user(size, (uint32_t *)arg);
		GLINK_PKT_INFO("%s: intent size[%d]\n", __func__, size);
		ret  = glink_queue_rx_intent(devp->handle, devp, size);
		if (ret) {
			GLINK_PKT_ERR("%s: failed to QUEUE_RX_INTENT ret[%d]\n",
					__func__, ret);
		}
		break;
	default:
		GLINK_PKT_ERR("%s: Unrecognized ioctl command 0x%x\n",
					__func__, cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	mutex_unlock(&devp->ch_lock);

	return ret;
}

/**
 * glink_pkt_open() - open() syscall for the glink_pkt device
 * inode:	Pointer to the inode structure.
 * file:	Pointer to the file structure.
 *
 * This function is used to open the glink pkt device when
 * userspace client do a open() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
int glink_pkt_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct glink_pkt_dev *devp = NULL;
	int wait_time_msecs;

	devp = container_of(inode->i_cdev, struct glink_pkt_dev, cdev);
	if (!devp) {
		GLINK_PKT_ERR("%s on NULL device\n", __func__);
		return -EINVAL;
	}
	GLINK_PKT_INFO("Begin %s() on dev id:%d open_time_wait[%d] by [%s]\n",
		__func__, devp->i, devp->open_time_wait, current->comm);
	file->private_data = devp;
	wait_time_msecs = devp->open_time_wait * 1000;

	mutex_lock(&devp->ch_lock);
	/* waiting for previous close to complete */
	if (devp->handle && devp->ref_cnt == 0) {
		mutex_unlock(&devp->ch_lock);
		if (wait_time_msecs < 0) {
			ret = wait_event_interruptible(
				devp->ch_opened_wait_queue,
				devp->ch_state == GLINK_LOCAL_DISCONNECTED);
		} else {
			ret = wait_event_interruptible_timeout(
				devp->ch_opened_wait_queue,
				devp->ch_state == GLINK_LOCAL_DISCONNECTED,
				msecs_to_jiffies(wait_time_msecs));
			if (ret >= 0)
				wait_time_msecs = jiffies_to_msecs(ret);
			if (ret == 0)
				ret = -ETIMEDOUT;
		}
		if (ret < 0) {
			GLINK_PKT_ERR(
			"%s:failed for prev close on dev id:%d rc:%d\n",
			__func__, devp->i, ret);
			return ret;
		}
		mutex_lock(&devp->ch_lock);
	}

	if (!devp->handle) {
		mutex_unlock(&devp->ch_lock);
		/*
		 * Wait for the link to be complete up so we know
		 * the remote is ready enough.
		 */
		if (wait_time_msecs < 0) {
			ret = wait_event_interruptible(
				devp->ch_opened_wait_queue,
				devp->link_up == true);
		} else {
			ret = wait_event_interruptible_timeout(
				devp->ch_opened_wait_queue,
				devp->link_up == true,
				msecs_to_jiffies(wait_time_msecs));
			if (ret >= 0)
				wait_time_msecs = jiffies_to_msecs(ret);
			if (ret == 0)
				ret = -ETIMEDOUT;
		}
		mutex_lock(&devp->ch_lock);
		if (ret < 0) {
			GLINK_PKT_ERR(
				"%s: Link not up edge[%s] name[%s] rc:%d\n",
				__func__, devp->open_cfg.edge,
				devp->open_cfg.name, ret);
			devp->handle = NULL;
			goto error;
		}

		devp->handle = glink_open(&devp->open_cfg);
		if (IS_ERR_OR_NULL(devp->handle)) {
			GLINK_PKT_ERR(
				"%s: open failed xprt[%s] edge[%s] name[%s]\n",
				__func__, devp->open_cfg.transport,
				devp->open_cfg.edge, devp->open_cfg.name);
			ret = -ENODEV;
			devp->handle = NULL;
			goto error;
		}

		mutex_unlock(&devp->ch_lock);
		/*
		 * Wait for the channel to be complete open state so we know
		 * the remote client is ready enough.
		 */
		if (wait_time_msecs < 0) {
			ret = wait_event_interruptible(
				devp->ch_opened_wait_queue,
				devp->ch_state == GLINK_CONNECTED);
		} else {
			ret = wait_event_interruptible_timeout(
				devp->ch_opened_wait_queue,
				devp->ch_state == GLINK_CONNECTED,
				msecs_to_jiffies(wait_time_msecs));
			if (ret == 0)
				ret = -ETIMEDOUT;
		}
		mutex_lock(&devp->ch_lock);
		if (ret < 0) {
			GLINK_PKT_ERR("%s: open failed on dev id:%d rc:%d\n",
					__func__, devp->i, ret);
			glink_close(devp->handle);
			devp->handle = NULL;
			goto error;
		}
	}
	ret = 0;
	devp->ref_cnt++;

error:
	mutex_unlock(&devp->ch_lock);
	GLINK_PKT_INFO("END %s() on dev id:%d ref_cnt[%d] ret[%d]\n",
			__func__, devp->i, devp->ref_cnt, ret);
	return ret;
}

/**
 * pop_rx_pkt() - return first pkt from rx pkt_list
 * devp:	pointer to G-Link packet device.
 *
 * This function return first item from rx pkt_list and NULL if list is empty.
 */
struct glink_rx_pkt *pop_rx_pkt(struct glink_pkt_dev *devp)
{
	unsigned long flags;
	struct glink_rx_pkt *pkt = NULL;

	spin_lock_irqsave(&devp->pkt_list_lock, flags);
	if (!list_empty(&devp->pkt_list)) {
		pkt = list_first_entry(&devp->pkt_list,
				struct glink_rx_pkt, list);
		list_del(&pkt->list);
	}
	spin_unlock_irqrestore(&devp->pkt_list_lock, flags);
	return pkt;
}

/**
 * glink_pkt_release() - release operation on glink_pkt device
 * inode:	Pointer to the inode structure.
 * file:	Pointer to the file structure.
 *
 * This function is used to release the glink pkt device when
 * userspace client do a close() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
int glink_pkt_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct glink_pkt_dev *devp = file->private_data;
	unsigned long flags;
	struct glink_rx_pkt *pkt;
	GLINK_PKT_INFO("%s() on dev id:%d by [%s] ref_cnt[%d]\n",
			__func__, devp->i, current->comm, devp->ref_cnt);

	mutex_lock(&devp->ch_lock);
	if (devp->ref_cnt > 0)
		devp->ref_cnt--;

	if (devp->handle && devp->ref_cnt == 0) {
		while ((pkt = pop_rx_pkt(devp))) {
			glink_rx_done(devp->handle, pkt->data, false);
			kfree(pkt);
		}
		wake_up(&devp->ch_read_wait_queue);
		wake_up_interruptible(&devp->ch_opened_wait_queue);
		ret = glink_close(devp->handle);
		devp->handle = NULL;
		if (ret)  {
			GLINK_PKT_ERR("%s: close failed ret[%d]\n",
						__func__, ret);
		} else {
			mutex_unlock(&devp->ch_lock);
			ret = wait_event_interruptible_timeout(
				devp->ch_closed_wait_queue,
				devp->ch_state == GLINK_LOCAL_DISCONNECTED,
				msecs_to_jiffies(CLOSE_WAIT_TIMEOUT));
			if (ret == 0)
				GLINK_PKT_ERR(
				"%s(): close TIMEOUT on dev_id[%d]\n",
				__func__, devp->i);
			mutex_lock(&devp->ch_lock);
		}
		devp->poll_mode = 0;
		spin_lock_irqsave(&devp->pa_spinlock, flags);
		if (devp->ws_locked) {
			__pm_relax(&devp->pa_ws);
			devp->ws_locked = 0;
		}
		spin_unlock_irqrestore(&devp->pa_spinlock, flags);
		devp->sigs_updated = false;
		devp->in_reset = 0;
	}
	mutex_unlock(&devp->ch_lock);

	if (flush_work(&devp->packet_arrival_work))
		GLINK_PKT_INFO("%s: Flushed work for glink_pkt_dev id:%d\n",
			__func__, devp->i);
	return ret;
}

static const struct file_operations glink_pkt_fops = {
	.owner = THIS_MODULE,
	.open = glink_pkt_open,
	.release = glink_pkt_release,
	.read = glink_pkt_read,
	.write = glink_pkt_write,
	.poll = glink_pkt_poll,
	.unlocked_ioctl = glink_pkt_ioctl,
	.compat_ioctl = glink_pkt_ioctl,
};

/**
 * glink_pkt_init_add_device() - Initialize G-Link packet device and add cdev
 * devp:	pointer to G-Link packet device.
 * i:		index of the G-Link packet device.
 *
 * return:	0 for success, Standard Linux errors
 */
static int glink_pkt_init_add_device(struct glink_pkt_dev *devp, int i)
{
	int ret = 0;

	devp->open_cfg.notify_rx = glink_pkt_notify_rx;
	devp->open_cfg.notify_tx_done = glink_pkt_notify_tx_done;
	devp->open_cfg.notify_state = glink_pkt_notify_state;
	devp->open_cfg.notify_rx_intent_req = glink_pkt_rmt_rx_intent_req_cb;
	devp->open_cfg.notify_rx_sigs = glink_pkt_notify_rx_sigs;
	devp->open_cfg.options |= GLINK_OPT_INITIAL_XPORT;
	devp->open_cfg.priv = devp;

	devp->link_up = false;
	devp->link_info.edge = devp->open_cfg.edge;
	devp->link_info.transport = devp->open_cfg.transport;
	devp->link_info.glink_link_state_notif_cb =
				glink_pkt_link_state_cb;
	devp->i = i;
	devp->poll_mode = 0;
	devp->ws_locked = 0;
	devp->ch_state = GLINK_LOCAL_DISCONNECTED;
	/* Default timeout for open wait is 120sec */
	devp->open_time_wait = 120;
	mutex_init(&devp->ch_lock);
	init_waitqueue_head(&devp->ch_read_wait_queue);
	init_waitqueue_head(&devp->ch_opened_wait_queue);
	init_waitqueue_head(&devp->ch_closed_wait_queue);
	spin_lock_init(&devp->pa_spinlock);
	INIT_LIST_HEAD(&devp->pkt_list);
	spin_lock_init(&devp->pkt_list_lock);
	wakeup_source_init(&devp->pa_ws, devp->dev_name);
	INIT_WORK(&devp->packet_arrival_work, packet_arrival_worker);

	devp->link_state_handle =
		glink_register_link_state_cb(&devp->link_info, devp);
	if (IS_ERR_OR_NULL(devp->link_state_handle)) {
		GLINK_PKT_ERR(
			"%s: link state cb reg. failed edge[%s] name[%s]\n",
			__func__, devp->open_cfg.edge, devp->open_cfg.name);
		ret = PTR_ERR(devp->link_state_handle);
		return ret;
	}
	cdev_init(&devp->cdev, &glink_pkt_fops);
	devp->cdev.owner = THIS_MODULE;

	ret = cdev_add(&devp->cdev, (glink_pkt_number + i), 1);
	if (IS_ERR_VALUE(ret)) {
		GLINK_PKT_ERR("%s: cdev_add() failed for dev id:%d ret:%i\n",
			__func__, i, ret);
		wakeup_source_trash(&devp->pa_ws);
		return ret;
	}

	devp->devicep = device_create(glink_pkt_classp,
			      NULL,
			      (glink_pkt_number + i),
			      NULL,
			      devp->dev_name);

	if (IS_ERR_OR_NULL(devp->devicep)) {
		GLINK_PKT_ERR("%s: device_create() failed for dev id:%d\n",
			__func__, i);
		ret = -ENOMEM;
		cdev_del(&devp->cdev);
		wakeup_source_trash(&devp->pa_ws);
		return ret;
	}

	if (device_create_file(devp->devicep, &dev_attr_open_timeout))
		GLINK_PKT_ERR("%s: device_create_file() failed for id:%d\n",
			__func__, i);

	mutex_lock(&glink_pkt_dev_lock_lha1);
	list_add(&devp->dev_list, &glink_pkt_dev_list);
	mutex_unlock(&glink_pkt_dev_lock_lha1);

	return ret;
}

/**
 * glink_pkt_core_deinit- De-initialization for this module
 *
 * This function remove all the memory and unregister
 * the char device region.
 */
static void glink_pkt_core_deinit(void)
{
	struct glink_pkt_dev *glink_pkt_devp;
	struct glink_pkt_dev *index;

	mutex_lock(&glink_pkt_dev_lock_lha1);
	list_for_each_entry_safe(glink_pkt_devp, index, &glink_pkt_dev_list,
							dev_list) {
		if (glink_pkt_devp->link_state_handle)
			glink_unregister_link_state_cb(
				glink_pkt_devp->link_state_handle);
		cdev_del(&glink_pkt_devp->cdev);
		list_del(&glink_pkt_devp->dev_list);
		device_destroy(glink_pkt_classp,
			       MKDEV(MAJOR(glink_pkt_number),
			       glink_pkt_devp->i));
		kfree(glink_pkt_devp);
	}
	mutex_unlock(&glink_pkt_dev_lock_lha1);

	if (!IS_ERR_OR_NULL(glink_pkt_classp))
		class_destroy(glink_pkt_classp);

	unregister_chrdev_region(MAJOR(glink_pkt_number), num_glink_pkt_ports);
}

/**
 * glink_pkt_alloc_chrdev_region() - allocate the char device region
 *
 * This function allocate memory for G-Link packet character-device region and
 * create the class.
 */
static int glink_pkt_alloc_chrdev_region(void)
{
	int ret;

	ret = alloc_chrdev_region(&glink_pkt_number,
			       0,
			       num_glink_pkt_ports,
			       DEVICE_NAME);
	if (IS_ERR_VALUE(ret)) {
		GLINK_PKT_ERR("%s: alloc_chrdev_region() failed ret:%i\n",
			__func__, ret);
		return ret;
	}

	glink_pkt_classp = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(glink_pkt_classp)) {
		GLINK_PKT_ERR("%s: class_create() failed ENOMEM\n", __func__);
		ret = -ENOMEM;
		unregister_chrdev_region(MAJOR(glink_pkt_number),
						num_glink_pkt_ports);
		return ret;
	}

	return 0;
}

/**
 * parse_glinkpkt_devicetree() - parse device tree binding
 *
 * node:	pointer to device tree node
 * glink_pkt_devp: pointer to GLINK PACKET device
 *
 * Return:	0 on success, -ENODEV on failure.
 */
static int parse_glinkpkt_devicetree(struct device_node *node,
					struct glink_pkt_dev *glink_pkt_devp)
{
	char *key;

	key = "qcom,glinkpkt-transport";
	glink_pkt_devp->open_cfg.transport = of_get_property(node, key, NULL);
	if (!glink_pkt_devp->open_cfg.transport)
		goto error;
	GLINK_PKT_INFO("%s transport = %s\n", __func__,
			glink_pkt_devp->open_cfg.transport);

	key = "qcom,glinkpkt-edge";
	glink_pkt_devp->open_cfg.edge = of_get_property(node, key, NULL);
	if (!glink_pkt_devp->open_cfg.edge)
		goto error;
	GLINK_PKT_INFO("%s edge = %s\n", __func__,
			glink_pkt_devp->open_cfg.edge);

	key = "qcom,glinkpkt-ch-name";
	glink_pkt_devp->open_cfg.name = of_get_property(node, key, NULL);
	if (!glink_pkt_devp->open_cfg.name)
		goto error;
	GLINK_PKT_INFO("%s ch_name = %s\n", __func__,
			glink_pkt_devp->open_cfg.name);

	key = "qcom,glinkpkt-dev-name";
	glink_pkt_devp->dev_name = of_get_property(node, key, NULL);
	if (!glink_pkt_devp->dev_name)
		goto error;
	GLINK_PKT_INFO("%s dev_name = %s\n", __func__,
			glink_pkt_devp->dev_name);
	return 0;

error:
	GLINK_PKT_ERR("%s: missing key: %s\n", __func__, key);
	return -ENODEV;

}

/**
 * glink_pkt_devicetree_init() - Initialize the add char device
 *
 * pdev:	Pointer to device tree data.
 *
 * return:	0 on success, -ENODEV on failure.
 */
static int glink_pkt_devicetree_init(struct platform_device *pdev)
{
	int ret;
	int i = 0;
	struct device_node *node;
	struct glink_pkt_dev *glink_pkt_devp;
	int subnode_num = 0;

	for_each_child_of_node(pdev->dev.of_node, node)
		++subnode_num;
	if (!subnode_num) {
		GLINK_PKT_ERR("%s subnode_num = %d\n", __func__, subnode_num);
		return 0;
	}

	num_glink_pkt_ports = subnode_num;

	ret = glink_pkt_alloc_chrdev_region();
	if (ret) {
		GLINK_PKT_ERR("%s: chrdev_region allocation failed ret:%i\n",
			__func__, ret);
		return ret;
	}

	for_each_child_of_node(pdev->dev.of_node, node) {
		glink_pkt_devp = kzalloc(sizeof(*glink_pkt_devp),
						GFP_KERNEL);
		if (IS_ERR_OR_NULL(glink_pkt_devp)) {
			GLINK_PKT_ERR("%s: allocation failed id:%d\n",
						__func__, i);
			ret = -ENOMEM;
			goto error_destroy;
		}

		ret = parse_glinkpkt_devicetree(node, glink_pkt_devp);
		if (ret) {
			GLINK_PKT_ERR("%s: failed to parse devicetree %d\n",
						__func__, i);
			kfree(glink_pkt_devp);
			goto error_destroy;
		}

		ret = glink_pkt_init_add_device(glink_pkt_devp, i);
		if (ret < 0) {
			GLINK_PKT_ERR("%s: add device failed idx:%d ret=%d\n",
					__func__, i, ret);
			kfree(glink_pkt_devp);
			goto error_destroy;
		}
		i++;
	}

	GLINK_PKT_INFO("G-Link Packet Port Driver Initialized.\n");
	return 0;

error_destroy:
	glink_pkt_core_deinit();
	return ret;
}

/**
 * msm_glink_pkt_probe() - Probe a G-Link packet device
 *
 * pdev:	Pointer to device tree data.
 *
 * return:	0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to a G-Link packet device.
 */
static int msm_glink_pkt_probe(struct platform_device *pdev)
{
	int ret;

	if (pdev) {
		if (pdev->dev.of_node) {
			GLINK_PKT_INFO("%s device tree implementation\n",
							__func__);
			ret = glink_pkt_devicetree_init(pdev);
			if (ret)
				GLINK_PKT_ERR("%s: device tree init failed\n",
					__func__);
		}
	}

	return 0;
}

static struct of_device_id msm_glink_pkt_match_table[] = {
	{ .compatible = "qcom,glinkpkt" },
	{},
};

static struct platform_driver msm_glink_pkt_driver = {
	.probe = msm_glink_pkt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_glink_pkt_match_table,
	 },
};

/**
 * glink_pkt_init() - Initialization function for this module
 *
 * returns:	0 on success, standard Linux error code otherwise.
 */
static int __init glink_pkt_init(void)
{
	int ret;

	INIT_LIST_HEAD(&glink_pkt_dev_list);
	INIT_LIST_HEAD(&glink_pkt_driver_list);
	ret = platform_driver_register(&msm_glink_pkt_driver);
	if (ret) {
		GLINK_PKT_ERR("%s: msm_glink_driver register failed %d\n",
			 __func__, ret);
		return ret;
	}

	glink_pkt_ilctxt = ipc_log_context_create(GLINK_PKT_IPC_LOG_PAGE_CNT,
						"glink_pkt", 0);
	glink_pkt_wq = create_singlethread_workqueue("glink_pkt_wq");
	if (!glink_pkt_wq) {
		GLINK_PKT_ERR("%s: Error creating glink_pkt_wq\n", __func__);
		return -ENOMEM;
	}
	return 0;
}

/**
 * glink_pkt_cleanup() - Exit function for this module
 *
 * This function is used to cleanup the module during the exit.
 */
static void __exit glink_pkt_cleanup(void)
{
	glink_pkt_core_deinit();
}

module_init(glink_pkt_init);
module_exit(glink_pkt_cleanup);

MODULE_DESCRIPTION("MSM G-Link Packet Port");
MODULE_LICENSE("GPL v2");
