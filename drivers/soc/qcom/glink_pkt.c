/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/ipc_logging.h>
#include <linux/refcount.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/rpmsg.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/idr.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/termios.h>

/* Define IPC Logging Macros */
#define GLINK_PKT_IPC_LOG_PAGE_CNT 2
static void *glink_pkt_ilctxt;

static int glink_pkt_debug_mask;
module_param_named(debug_mask, glink_pkt_debug_mask, int, 0664);

enum {
	GLINK_PKT_INFO = 1U << 0,
};

#define GLINK_PKT_INFO(x, ...)						\
do {									\
	if (glink_pkt_debug_mask & GLINK_PKT_INFO) {			\
		ipc_log_string(glink_pkt_ilctxt,			\
			"[%s]: "x, __func__, ##__VA_ARGS__);		\
	}								\
} while (0)

#define GLINK_PKT_ERR(x, ...)						      \
do {									      \
	pr_err_ratelimited("[%s]: "x, __func__, ##__VA_ARGS__);		      \
	ipc_log_string(glink_pkt_ilctxt, "[%s]: "x, __func__, ##__VA_ARGS__); \
} while (0)

#define SMD_DTR_SIG BIT(31)
#define SMD_CTS_SIG BIT(30)
#define SMD_CD_SIG BIT(29)
#define SMD_RI_SIG BIT(28)

#define to_smd_signal(sigs) \
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

#define from_smd_signal(sigs) \
do { \
	if (sigs & SMD_DTR_SIG) \
		sigs |= TIOCM_DSR; \
	if (sigs & SMD_CTS_SIG) \
		sigs |= TIOCM_CTS; \
	if (sigs & SMD_CD_SIG) \
		sigs |= TIOCM_CD; \
	if (sigs & SMD_RI_SIG) \
		sigs |= TIOCM_RI; \
	sigs &= 0x0fff; \
} while (0)

#define GLINK_PKT_IOCTL_MAGIC (0xC3)

#define GLINK_PKT_IOCTL_QUEUE_RX_INTENT \
	_IOW(GLINK_PKT_IOCTL_MAGIC, 0, unsigned int)

#define MODULE_NAME "glink_pkt"
static dev_t glink_pkt_major;
static struct class *glink_pkt_class;
static int num_glink_pkt_devs;

static DEFINE_IDA(glink_pkt_minor_ida);

/**
 * struct glink_pkt - driver context, relates rpdev to cdev
 * @dev:	glink pkt device
 * @cdev:	cdev for the glink pkt device
 * @drv:	rpmsg driver for registering to rpmsg bus
 * @lock:	synchronization of @rpdev and @open_tout modifications
 * @ch_open:	wait object for opening the glink channel
 * @refcount:	count how many userspace clients have handles
 * @rpdev:	underlaying rpmsg device
 * @queue_lock:	synchronization of @queue operations
 * @queue:	incoming message queue
 * @readq:	wait object for incoming queue
 * @sig_change:	flag to indicate serial signal change
 * @dev_name:	/dev/@dev_name for glink_pkt device
 * @ch_name:	glink channel to match to
 * @edge:	glink edge to match to
 * @open_tout:	timeout for open syscall, configurable in sysfs
 */
struct glink_pkt_device {
	struct device dev;
	struct cdev cdev;
	struct rpmsg_driver drv;

	struct mutex lock;
	struct completion ch_open;
	refcount_t refcount;
	struct rpmsg_device *rpdev;

	spinlock_t queue_lock;
	struct sk_buff_head queue;
	wait_queue_head_t readq;
	int sig_change;

	const char *dev_name;
	const char *ch_name;
	const char *edge;
	int open_tout;
};

#define dev_to_gpdev(_dev) container_of(_dev, struct glink_pkt_device, dev)
#define cdev_to_gpdev(_cdev) container_of(_cdev, struct glink_pkt_device, cdev)
#define drv_to_rpdrv(_drv) container_of(_drv, struct rpmsg_driver, drv)
#define rpdrv_to_gpdev(_rdrv) container_of(_rdrv, struct glink_pkt_device, drv)

static ssize_t open_timeout_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t n)
{
	struct glink_pkt_device *gpdev = dev_to_gpdev(dev);
	long tmp;

	mutex_lock(&gpdev->lock);
	if (kstrtol(buf, 0, &tmp)) {
		mutex_unlock(&gpdev->lock);
		GLINK_PKT_ERR("unable to convert:%s to an int for /dev/%s\n",
			      buf, gpdev->dev_name);
		return -EINVAL;
	}
	gpdev->open_tout = tmp;
	mutex_unlock(&gpdev->lock);

	return n;
}

static ssize_t open_timeout_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct glink_pkt_device *gpdev = dev_to_gpdev(dev);
	ssize_t ret;

	mutex_lock(&gpdev->lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", gpdev->open_tout);
	mutex_unlock(&gpdev->lock);

	return ret;
}

static DEVICE_ATTR(open_timeout, 0664, open_timeout_show, open_timeout_store);

static int glink_pkt_rpdev_probe(struct rpmsg_device *rpdev)
{
	struct device_driver *drv = rpdev->dev.driver;
	struct rpmsg_driver *rpdrv = drv_to_rpdrv(drv);
	struct glink_pkt_device *gpdev = rpdrv_to_gpdev(rpdrv);

	mutex_lock(&gpdev->lock);
	gpdev->rpdev = rpdev;
	mutex_unlock(&gpdev->lock);

	dev_set_drvdata(&rpdev->dev, gpdev);
	complete_all(&gpdev->ch_open);

	return 0;
}

static int glink_pkt_rpdev_cb(struct rpmsg_device *rpdev, void *buf, int len,
			      void *priv, u32 addr)
{
	struct glink_pkt_device *gpdev = dev_get_drvdata(&rpdev->dev);
	unsigned long flags;
	struct sk_buff *skb;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	skb_put_data(skb, buf, len);

	spin_lock_irqsave(&gpdev->queue_lock, flags);
	skb_queue_tail(&gpdev->queue, skb);
	spin_unlock_irqrestore(&gpdev->queue_lock, flags);

	/* wake up any blocking processes, waiting for new data */
	wake_up_interruptible(&gpdev->readq);

	return 0;
}

static int glink_pkt_rpdev_sigs(struct rpmsg_device *rpdev, u32 old, u32 new)
{
	struct device_driver *drv = rpdev->dev.driver;
	struct rpmsg_driver *rpdrv = drv_to_rpdrv(drv);
	struct glink_pkt_device *gpdev = rpdrv_to_gpdev(rpdrv);
	unsigned long flags;

	spin_lock_irqsave(&gpdev->queue_lock, flags);
	gpdev->sig_change = true;
	spin_unlock_irqrestore(&gpdev->queue_lock, flags);

	/* wake up any blocking processes, waiting for new data */
	wake_up_interruptible(&gpdev->readq);

	return 0;
}

static void glink_pkt_rpdev_remove(struct rpmsg_device *rpdev)
{
	struct device_driver *drv = rpdev->dev.driver;
	struct rpmsg_driver *rpdrv = drv_to_rpdrv(drv);
	struct glink_pkt_device *gpdev = rpdrv_to_gpdev(rpdrv);

	mutex_lock(&gpdev->lock);
	gpdev->rpdev = NULL;
	mutex_unlock(&gpdev->lock);

	dev_set_drvdata(&rpdev->dev, NULL);

	/* wake up any blocked readers */
	reinit_completion(&gpdev->ch_open);
	wake_up_interruptible(&gpdev->readq);
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
	struct glink_pkt_device *gpdev = cdev_to_gpdev(inode->i_cdev);
	int tout = msecs_to_jiffies(gpdev->open_tout * 1000);
	struct device *dev = &gpdev->dev;
	int ret;

	refcount_inc(&gpdev->refcount);
	get_device(dev);

	GLINK_PKT_INFO("begin for %s by %s:%ld ref_cnt[%d]\n",
		       gpdev->ch_name, current->comm,
		       task_pid_nr(current), refcount_read(&gpdev->refcount));

	ret = wait_for_completion_interruptible_timeout(&gpdev->ch_open, tout);
	if (ret <= 0) {
		refcount_dec(&gpdev->refcount);
		put_device(dev);
		GLINK_PKT_INFO("timeout for %s by %s:%ld\n", gpdev->ch_name,
			       current->comm, task_pid_nr(current));
		return -ETIMEDOUT;
	}
	file->private_data = gpdev;

	GLINK_PKT_INFO("end for %s by %s:%ld ref_cnt[%d]\n",
		       gpdev->ch_name, current->comm,
		       task_pid_nr(current), refcount_read(&gpdev->refcount));

	return 0;
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
	struct glink_pkt_device *gpdev = cdev_to_gpdev(inode->i_cdev);
	struct device *dev = &gpdev->dev;
	struct sk_buff *skb;
	unsigned long flags;

	GLINK_PKT_INFO("for %s by %s:%ld ref_cnt[%d]\n",
		       gpdev->ch_name, current->comm,
		       task_pid_nr(current), refcount_read(&gpdev->refcount));

	refcount_dec(&gpdev->refcount);
	if (refcount_read(&gpdev->refcount) == 1) {
		spin_lock_irqsave(&gpdev->queue_lock, flags);

		/* Discard all SKBs */
		while (!skb_queue_empty(&gpdev->queue)) {
			skb = skb_dequeue(&gpdev->queue);
			kfree_skb(skb);
		}
		wake_up_interruptible(&gpdev->readq);
		gpdev->sig_change = false;
		spin_unlock_irqrestore(&gpdev->queue_lock, flags);
	}

	put_device(dev);

	return 0;
}

/**
 * glink_pkt_read() - read() syscall for the glink_pkt device
 * file:	Pointer to the file structure.
 * buf:		Pointer to the userspace buffer.
 * count:	Number bytes to read from the file.
 * ppos:	Pointer to the position into the file.
 *
 * This function is used to Read the data from glink pkt device when
 * userspace client do a read() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
ssize_t glink_pkt_read(struct file *file, char __user *buf,
		       size_t count, loff_t *ppos)
{
	struct glink_pkt_device *gpdev = file->private_data;
	unsigned long flags;
	struct sk_buff *skb;
	int use;

	if (!gpdev || refcount_read(&gpdev->refcount) == 1) {
		GLINK_PKT_ERR("invalid device handle\n", __func__);
		return -EINVAL;
	}

	if (!completion_done(&gpdev->ch_open)) {
		GLINK_PKT_ERR("%s channel in reset\n", gpdev->ch_name);
		return -ENETRESET;
	}

	GLINK_PKT_INFO("begin for %s by %s:%ld ref_cnt[%d]\n",
		       gpdev->ch_name, current->comm,
		       task_pid_nr(current), refcount_read(&gpdev->refcount));

	spin_lock_irqsave(&gpdev->queue_lock, flags);
	/* Wait for data in the queue */
	if (skb_queue_empty(&gpdev->queue)) {
		spin_unlock_irqrestore(&gpdev->queue_lock, flags);

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* Wait until we get data or the endpoint goes away */
		if (wait_event_interruptible(gpdev->readq,
					     !skb_queue_empty(&gpdev->queue) ||
					     !completion_done(&gpdev->ch_open)))
			return -ERESTARTSYS;

		/* We lost the endpoint while waiting */
		if (!completion_done(&gpdev->ch_open))
			return -ENETRESET;

		spin_lock_irqsave(&gpdev->queue_lock, flags);
	}

	skb = skb_dequeue(&gpdev->queue);
	spin_unlock_irqrestore(&gpdev->queue_lock, flags);
	if (!skb)
		return -EFAULT;

	use = min_t(size_t, count, skb->len);
	if (copy_to_user(buf, skb->data, use))
		use = -EFAULT;

	kfree_skb(skb);

	GLINK_PKT_INFO("end for %s by %s:%ld ret[%d]\n", gpdev->ch_name,
		       current->comm, task_pid_nr(current), use);

	return use;
}

/**
 * glink_pkt_write() - write() syscall for the glink_pkt device
 * file:	Pointer to the file structure.
 * buf:		Pointer to the userspace buffer.
 * count:	Number bytes to read from the file.
 * ppos:	Pointer to the position into the file.
 *
 * This function is used to write the data to glink pkt device when
 * userspace client do a write() system call. All input arguments are
 * validated by the virtual file system before calling this function.
 */
ssize_t glink_pkt_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	struct glink_pkt_device *gpdev = file->private_data;
	void *kbuf;
	int ret;

	gpdev = file->private_data;
	if (!gpdev || refcount_read(&gpdev->refcount) == 1) {
		GLINK_PKT_ERR("invalid device handle\n", __func__);
		return -EINVAL;
	}

	GLINK_PKT_INFO("begin to %s buffer_size %zu\n", gpdev->ch_name, count);
	kbuf = memdup_user(buf, count);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	if (mutex_lock_interruptible(&gpdev->lock)) {
		ret = -ERESTARTSYS;
		goto free_kbuf;
	}
	if (!completion_done(&gpdev->ch_open) || !gpdev->rpdev) {
		GLINK_PKT_ERR("%s channel in reset\n", gpdev->ch_name);
		ret = -ENETRESET;
		goto unlock_ch;
	}

	if (file->f_flags & O_NONBLOCK)
		ret = rpmsg_trysend(gpdev->rpdev->ept, kbuf, count);
	else
		ret = rpmsg_send(gpdev->rpdev->ept, kbuf, count);

unlock_ch:
	mutex_unlock(&gpdev->lock);

free_kbuf:
	kfree(kbuf);
	GLINK_PKT_INFO("finish to %s ret %d\n", gpdev->ch_name, ret);
	return ret < 0 ? ret : count;
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
	struct glink_pkt_device *gpdev = file->private_data;
	unsigned int mask = 0;
	unsigned long flags;

	gpdev = file->private_data;
	if (!gpdev || refcount_read(&gpdev->refcount) == 1) {
		GLINK_PKT_ERR("invalid device handle\n", __func__);
		return POLLERR;
	}
	if (!completion_done(&gpdev->ch_open)) {
		GLINK_PKT_ERR("%s channel in reset\n", gpdev->ch_name);
		return POLLHUP;
	}

	poll_wait(file, &gpdev->readq, wait);

	mutex_lock(&gpdev->lock);

	if (!completion_done(&gpdev->ch_open) || !gpdev->rpdev) {
		GLINK_PKT_ERR("%s channel reset after wait\n", gpdev->ch_name);
		mutex_unlock(&gpdev->lock);
		return POLLHUP;
	}

	spin_lock_irqsave(&gpdev->queue_lock, flags);
	if (!skb_queue_empty(&gpdev->queue))
		mask |= POLLIN | POLLRDNORM;

	if (gpdev->sig_change)
		mask |= POLLPRI;
	spin_unlock_irqrestore(&gpdev->queue_lock, flags);

	mask |= rpmsg_poll(gpdev->rpdev->ept, file, wait);

	mutex_unlock(&gpdev->lock);

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
static int glink_pkt_tiocmset(struct glink_pkt_device *gpdev, unsigned int cmd,
			      unsigned long arg)
{
	u32 lsigs, rsigs, val;
	int ret;

	ret = get_user(val, (u32 *)arg);
	if (ret)
		return ret;

	to_smd_signal(val);
	ret = rpmsg_get_sigs(gpdev->rpdev->ept, &lsigs, &rsigs);
	if (ret < 0) {
		GLINK_PKT_ERR("%s: Get signals failed[%d]\n", __func__, ret);
		return ret;
	}
	switch (cmd) {
	case TIOCMBIS:
		lsigs |= val;
		break;
	case TIOCMBIC:
		lsigs &= ~val;
		break;
	case TIOCMSET:
		lsigs = val;
		break;
	}
	ret = rpmsg_set_sigs(gpdev->rpdev->ept, lsigs);
	GLINK_PKT_INFO("sigs[0x%x] ret[%d]\n", lsigs, ret);
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
	struct glink_pkt_device *gpdev;
	unsigned long flags;
	u32 lsigs, rsigs;
	int ret;

	gpdev = file->private_data;
	if (!gpdev || refcount_read(&gpdev->refcount) == 1) {
		GLINK_PKT_ERR("invalid device handle\n", __func__);
		return -EINVAL;
	}
	if (mutex_lock_interruptible(&gpdev->lock))
		return -ERESTARTSYS;

	if (!completion_done(&gpdev->ch_open)) {
		GLINK_PKT_ERR("%s channel in reset\n", gpdev->ch_name);
		mutex_unlock(&gpdev->lock);
		return -ENETRESET;
	}

	switch (cmd) {
	case TIOCMGET:
		spin_lock_irqsave(&gpdev->queue_lock, flags);
		gpdev->sig_change = false;
		spin_unlock_irqrestore(&gpdev->queue_lock, flags);

		ret = rpmsg_get_sigs(gpdev->rpdev->ept, &lsigs, &rsigs);
		from_smd_signal(rsigs);
		if (!ret)
			ret = put_user(rsigs, (uint32_t *)arg);
		break;
	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
		ret = glink_pkt_tiocmset(gpdev, cmd, arg);
		break;
	case GLINK_PKT_IOCTL_QUEUE_RX_INTENT:
		/* Return success to not break userspace client logic */
		ret = 0;
		break;
	default:
		GLINK_PKT_ERR("unrecognized ioctl command 0x%x\n", cmd);
		ret = -ENOIOCTLCMD;
	}

	mutex_unlock(&gpdev->lock);

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

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct glink_pkt_device *gpdev = dev_to_gpdev(dev);

	return snprintf(buf, RPMSG_NAME_SIZE, "%s\n", gpdev->ch_name);
}
static DEVICE_ATTR_RO(name);

static struct attribute *glink_pkt_device_attrs[] = {
	&dev_attr_name.attr,
	NULL
};
ATTRIBUTE_GROUPS(glink_pkt_device);

/**
 * parse_glinkpkt_devicetree() - parse device tree binding for a subnode
 *
 * np:		pointer to a device tree node
 * gpdev:	pointer to GLINK PACKET device
 *
 * Return:	0 on success, standard Linux error codes on error.
 */
static int glink_pkt_parse_devicetree(struct device_node *np,
				      struct glink_pkt_device *gpdev)
{
	char *key;
	int ret;

	key = "qcom,glinkpkt-edge";
	ret = of_property_read_string(np, key, &gpdev->edge);
	if (ret < 0)
		goto error;

	key = "qcom,glinkpkt-ch-name";
	ret = of_property_read_string(np, key, &gpdev->ch_name);
	if (ret < 0)
		goto error;

	key = "qcom,glinkpkt-dev-name";
	ret = of_property_read_string(np, key, &gpdev->dev_name);
	if (ret < 0)
		goto error;

	GLINK_PKT_INFO("Parsed %s:%s /dev/%s\n", gpdev->edge, gpdev->ch_name,
		       gpdev->dev_name);
	return 0;

error:
	GLINK_PKT_ERR("%s: missing key: %s\n", __func__, key);
	return ret;
}

static void glink_pkt_release_device(struct device *dev)
{
	struct glink_pkt_device *gpdev = dev_to_gpdev(dev);

	ida_simple_remove(&glink_pkt_minor_ida, MINOR(gpdev->dev.devt));
	cdev_del(&gpdev->cdev);
	kfree(gpdev);
}

static int glink_pkt_init_rpmsg(struct glink_pkt_device *gpdev)
{

	struct rpmsg_driver *rpdrv = &gpdev->drv;
	struct device *dev = &gpdev->dev;
	struct rpmsg_device_id *match;
	char *drv_name;

	/* zalloc array of two to NULL terminate the match list */
	match = devm_kzalloc(dev, 2 * sizeof(*match), GFP_KERNEL);
	if (!match)
		return -ENOMEM;
	snprintf(match->name, RPMSG_NAME_SIZE, "%s", gpdev->ch_name);

	drv_name = devm_kasprintf(dev, GFP_KERNEL,
				   "%s_%s", "glink_pkt", gpdev->dev_name);
	if (!drv_name)
		return -ENOMEM;

	rpdrv->probe = glink_pkt_rpdev_probe;
	rpdrv->remove = glink_pkt_rpdev_remove;
	rpdrv->callback = glink_pkt_rpdev_cb;
	rpdrv->signals = glink_pkt_rpdev_sigs;
	rpdrv->id_table = match;
	rpdrv->drv.name = drv_name;

	register_rpmsg_driver(rpdrv);

	return 0;
}

/**
 * glink_pkt_add_device() - Create glink packet device and add cdev
 * parent:	pointer to the parent device of this glink packet device
 * np:		pointer to device node this glink packet device represents
 *
 * return:	0 for success, Standard Linux errors
 */
static int glink_pkt_create_device(struct device *parent,
				   struct device_node *np)
{
	struct glink_pkt_device *gpdev;
	struct device *dev;
	int ret;

	gpdev = devm_kzalloc(parent, sizeof(*gpdev), GFP_KERNEL);
	if (!gpdev)
		return -ENOMEM;
	ret = glink_pkt_parse_devicetree(np, gpdev);
	if (ret < 0) {
		GLINK_PKT_ERR("failed to parse dt ret:%d\n", ret);
		goto free_gpdev;
	}

	dev = &gpdev->dev;
	mutex_init(&gpdev->lock);
	refcount_set(&gpdev->refcount, 1);
	init_completion(&gpdev->ch_open);

	/* Default open timeout for open is 120 sec */
	gpdev->open_tout = 120;
	gpdev->sig_change = false;

	spin_lock_init(&gpdev->queue_lock);
	skb_queue_head_init(&gpdev->queue);
	init_waitqueue_head(&gpdev->readq);

	device_initialize(dev);
	dev->class = glink_pkt_class;
	dev->parent = parent;
	dev->groups = glink_pkt_device_groups;
	dev_set_drvdata(dev, gpdev);

	cdev_init(&gpdev->cdev, &glink_pkt_fops);
	gpdev->cdev.owner = THIS_MODULE;

	ret = ida_simple_get(&glink_pkt_minor_ida, 0, num_glink_pkt_devs,
			     GFP_KERNEL);
	if (ret < 0)
		goto free_dev;

	dev->devt = MKDEV(MAJOR(glink_pkt_major), ret);
	dev_set_name(dev, gpdev->dev_name, ret);

	ret = cdev_add(&gpdev->cdev, dev->devt, 1);
	if (ret) {
		GLINK_PKT_ERR("cdev_add failed for %s ret:%d\n",
			      gpdev->dev_name, ret);
		goto free_minor_ida;
	}

	dev->release = glink_pkt_release_device;
	ret = device_add(dev);
	if (ret) {
		GLINK_PKT_ERR("device_create failed for %s ret:%d\n",
			      gpdev->dev_name, ret);
		goto free_minor_ida;
	}

	if (device_create_file(dev, &dev_attr_open_timeout))
		GLINK_PKT_ERR("device_create_file failed for %s\n",
			      gpdev->dev_name);

	if (glink_pkt_init_rpmsg(gpdev))
		goto free_minor_ida;

	return 0;

free_minor_ida:
	ida_simple_remove(&glink_pkt_minor_ida, MINOR(dev->devt));
free_dev:
	put_device(dev);
free_gpdev:
	kfree(gpdev);

	return ret;
}

/**
 * glink_pkt_deinit() - De-initialize this module
 *
 * This function frees all the memory and unregisters the char device region.
 */
static void glink_pkt_deinit(void)
{
	class_destroy(glink_pkt_class);
	unregister_chrdev_region(MAJOR(glink_pkt_major), num_glink_pkt_devs);
}

/**
 * glink_pkt_probe() - Probe a GLINK packet device
 *
 * pdev:	Pointer to platform device.
 *
 * return:	0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to a G-Link packet device.
 */
static int glink_pkt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *cn;
	int ret;

	num_glink_pkt_devs = of_get_child_count(dev->of_node);
	ret = alloc_chrdev_region(&glink_pkt_major, 0, num_glink_pkt_devs,
				  "glinkpkt");
	if (ret < 0) {
		GLINK_PKT_ERR("alloc_chrdev_region failed ret:%d\n", ret);
		return ret;
	}
	glink_pkt_class = class_create(THIS_MODULE, "glinkpkt");
	if (IS_ERR(glink_pkt_class)) {
		GLINK_PKT_ERR("class_create failed ret:%d\n",
			      PTR_ERR(glink_pkt_class));
		goto error_deinit;
	}

	for_each_child_of_node(dev->of_node, cn) {
		glink_pkt_create_device(dev, cn);
	}

	GLINK_PKT_INFO("G-Link Packet Port Driver Initialized\n");
	return 0;

error_deinit:
	glink_pkt_deinit();
	return ret;
}

static const struct of_device_id glink_pkt_match_table[] = {
	{ .compatible = "qcom,glinkpkt" },
	{},
};

static struct platform_driver glink_pkt_driver = {
	.probe = glink_pkt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = glink_pkt_match_table,
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

	ret = platform_driver_register(&glink_pkt_driver);
	if (ret) {
		GLINK_PKT_ERR("%s: glink_pkt register failed %d\n", ret);
		return ret;
	}
	glink_pkt_ilctxt = ipc_log_context_create(GLINK_PKT_IPC_LOG_PAGE_CNT,
						  "glink_pkt", 0);
	return 0;
}

/**
 * glink_pkt_exit() - Exit function for this module
 *
 * This function is used to cleanup the module during the exit.
 */
static void __exit glink_pkt_exit(void)
{
	glink_pkt_deinit();
}

module_init(glink_pkt_init);
module_exit(glink_pkt_exit);

MODULE_DESCRIPTION("MSM G-Link Packet Port");
MODULE_LICENSE("GPL v2");
