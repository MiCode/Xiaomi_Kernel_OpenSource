/*
 * Copyright 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 *
 * tabstop = 8
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/circ_buf.h>
#include <linux/fs.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include "bbd.h"

#ifdef CONFIG_SENSORS_SSP
#include <linux/spi/spi.h>


extern struct spi_driver *pssp_driver;
extern bool ssp_dbg;
extern bool ssp_pkt_dbg;

static struct spi_device dummy_spi = {
	.dev = {
		.init_name = "dummy",
	},
};
#endif

#ifdef CONFIG_BCM_GPS_SPI_DRIVER
extern bool ssi_dbg;
extern bool ssi_dbg_pzc;
extern bool ssi_dbg_rng;
#endif

void bbd_log_hex(const char*, const unsigned char*, unsigned long);






#define BBD_BUFF_SIZE (PAGE_SIZE*2)
struct bbd_cdev_priv {
	const char *name;
	struct cdev dev;			/* char device */
	bool busy;
	struct circ_buf read_buf;	    	/* LHD reads from BBD */
	struct mutex lock;			/* Lock for read_buf */
	char _read_buf[BBD_BUFF_SIZE];		/* LHD reads from BBD */
	char write_buf[BBD_BUFF_SIZE];		/* LHD writes into BBD */
	wait_queue_head_t poll_wait;		/* for poll */
};

struct bbd_device {
	struct kobject *kobj;			/* for sysfs register */
	struct class *class;			/* for device_create */

	struct bbd_cdev_priv priv[BBD_DEVICE_INDEX];/* individual structures */
	bool db;				/* debug flag */

	void *ssp_priv;				/* private data pointer */
	bbd_callbacks *ssp_cb;			/* callbacks for SSP */

	bool legacy_patch;	/* check for using legacy_bbd_patch */
};

/*
 * Character device names of BBD
 */
static const char *bbd_dev_name[BBD_DEVICE_INDEX] = {
	"bbd_shmd",
	"bbd_sensor",
	"bbd_control",
	"bbd_patch",

};






/*
 * The global BBD device which has all necessary information.
 * It's not beautiful but useful when we debug by Trace32.
 */
static struct bbd_device bbd;
/*
 * Embedded patch file provided as /dev/bbd_patch
 */
static unsigned char bbd_patch[] = {
#ifdef CONFIG_POP

#else

#endif
};

#ifdef CONFIG_SENSORS_BBD_LEGACY_PATCH
static unsigned char legacy_bbd_patch[] = {
#include "legacy_bbd_patch_file.h"
};
#else
static unsigned char legacy_bbd_patch[] = {
		"dummy",
};
#endif

/* Function to push read data into any bbd device's read buf */
ssize_t bbd_on_read(unsigned int minor,
					const unsigned char *buf, size_t size);







/**
 * bbd_register -Interface function called from SHMD
 * to register itself to BBD
 *
 * @priv: SHMD's private data provided back to
 * SHMD as callback argument
 * @cb: SHMD's functions to be called
 */
void bbd_register(void *priv, bbd_callbacks *cb)
{
	bbd.ssp_priv = priv;
	bbd.ssp_cb = cb;
}
EXPORT_SYMBOL(bbd_register);

/**
 * bbd_send_packet - Interface function called
 * from SHMD to send sensor packet.
 *
 *     The sensor packet is pushed into /dev/bbd_sensor
 *     to be read by gpsd/lhd.
 *     gpsd/lhd wrap the packet into RPC and sends to chip directly.
 *
 * @buf: buffer containing sensor packet
 * @size: size of sensor packet
 * @return: pushed data length = success
 */
struct sensor_pkt {
	unsigned short size;
	unsigned char buf[1022];	/*We assume max SSP packet less than 1KB */
} __attribute__((__packed__)) ss_pkt;

ssize_t bbd_send_packet(unsigned char *buf, size_t size)
{
	memset(&ss_pkt, 0, sizeof(ss_pkt));
	ss_pkt.size = (unsigned short)size;
	memcpy(ss_pkt.buf, buf, size);

	return bbd_on_read(BBD_MINOR_SENSOR,
					(unsigned char *)&ss_pkt, size+2); /* +2 for pkt.size */
}
EXPORT_SYMBOL(bbd_send_packet);


/**
 * bbd_pull_packet - Interface function called
 * from SHMD to read sensor packet.
 *
 *     Read packet consists of sensor packet from gpsd/lhd and from BBD.
 *
 * @buf: buffer to receive packet
 * @size:
 * @timeout_ms: if specified, this function waits for sensor
 * packet during given time
 *
 * @return: popped data length = success
 */
ssize_t bbd_pull_packet(unsigned char *buf, size_t size,
						unsigned int timeout_ms)
{
	struct circ_buf *circ = &bbd.priv[BBD_MINOR_SHMD].read_buf;
	size_t rd_size = 0;

	WARN_ON(!buf);
	WARN_ON(!size);

	if (timeout_ms) {
		int ret = wait_event_interruptible_timeout(
					bbd.priv[BBD_MINOR_SHMD].poll_wait,
					circ->head != circ->tail,
					msecs_to_jiffies(timeout_ms));
		if (!ret)
			return -ETIMEDOUT;
	}

	mutex_lock(&bbd.priv[BBD_MINOR_SHMD].lock);

	/* Copy from circ buffer to linear buffer
	 * Because SHMD's buffer is linear, we may require 2
	 * copies from [tail..end] and [start..head]
	 */
	do {
		size_t cnt_to_end = CIRC_CNT_TO_END(circ->head,
											circ->tail, BBD_BUFF_SIZE);
		size_t copied = min(cnt_to_end, size);

		memcpy(buf + rd_size, (void *) circ->buf + circ->tail, copied);
		size -= copied;
		rd_size += copied;
		circ->tail = (circ->tail + copied) & (BBD_BUFF_SIZE - 1);

	} while (size > 0 && CIRC_CNT(circ->head, circ->tail, BBD_BUFF_SIZE));

	mutex_unlock(&bbd.priv[BBD_MINOR_SHMD].lock);

	return rd_size;
}
EXPORT_SYMBOL(bbd_pull_packet);

/**
 * bbd_mcu_reset - Interface function called from SHMD to reset chip
 *
 *      BBD pushes reset request into /dev/bbd_control
 *      and actual reset is done by gpsd/lhd when it reads the request
 *
 * @return: 0 = success, -1 = failure
 */
int bbd_mcu_reset(void)
{
	pr_info("reset request from sensor hub\n");
	return bbd_on_read(BBD_MINOR_CONTROL,
					   BBD_CTRL_RESET_REQ, strlen(BBD_CTRL_RESET_REQ)+1);
}
EXPORT_SYMBOL(bbd_mcu_reset);









/**
 * bbd_control - Handles command string from lhd
 *
 *
 */
ssize_t bbd_control(const char *buf, ssize_t len)
{
	printk("%s : %s \n", __func__, buf);

	if (strnstr(buf, ESW_CTRL_READY, strlen(buf))) {

		if (bbd.ssp_cb && bbd.ssp_cb->on_mcu_ready)
			bbd.ssp_cb->on_mcu_ready(bbd.ssp_priv, true);
#ifdef CONFIG_BCM_GPS_SPI_DRIVER
		bcm477x_debug_info(ESW_CTRL_READY);
#endif
	} else if (strnstr(buf, ESW_CTRL_NOTREADY, strlen(buf))) {
		struct circ_buf *circ = &bbd.priv[BBD_MINOR_SENSOR].read_buf;
		circ->head = circ->tail = 0;
		if (bbd.ssp_cb && bbd.ssp_cb->on_mcu_ready)
			bbd.ssp_cb->on_mcu_ready(bbd.ssp_priv, false);
#ifdef CONFIG_BCM_GPS_SPI_DRIVER
		bcm477x_debug_info(ESW_CTRL_NOTREADY);
#endif
	} else if (strnstr(buf, ESW_CTRL_CRASHED, strlen(buf))) {
		struct circ_buf *circ = &bbd.priv[BBD_MINOR_SENSOR].read_buf;
		circ->head = circ->tail = 0;

		if (bbd.ssp_cb && bbd.ssp_cb->on_mcu_ready)
			bbd.ssp_cb->on_mcu_ready(bbd.ssp_priv, false);

		if (bbd.ssp_cb && bbd.ssp_cb->on_control)
			bbd.ssp_cb->on_control(bbd.ssp_priv, buf);
#ifdef CONFIG_BCM_GPS_SPI_DRIVER
		bcm477x_debug_info(ESW_CTRL_CRASHED);
#endif
#if 0
	} else if (strnstr(buf, BBD_CTRL_DEBUG_ON, strlen(buf))) {
		bbd.db = true;
#endif
	} else if (strnstr(buf, BBD_CTRL_DEBUG_OFF, strlen(buf))) {
		bbd.db = false;
#ifdef CONFIG_SENSORS_SSP
	} else if (strnstr(buf, SSP_DEBUG_ON, strlen(buf))) {
		ssp_dbg = true;
		ssp_pkt_dbg = true;
	} else if (strnstr(buf, SSP_DEBUG_OFF, strlen(buf))) {
		ssp_dbg = false;
		ssp_pkt_dbg = false;
#endif
#ifdef CONFIG_BCM_GPS_SPI_DRIVER
	} else if (strnstr(buf, SSI_DEBUG_ON, strlen(buf))) {
		ssi_dbg = true;
	} else if (strnstr(buf, SSI_DEBUG_OFF, strlen(buf))) {
		ssi_dbg = false;
	} else if (strnstr(buf, PZC_DEBUG_ON, strlen(buf))) {
		ssi_dbg_pzc = true;
	} else if (strnstr(buf, PZC_DEBUG_OFF, strlen(buf))) {
		ssi_dbg_pzc = false;
	} else if (strnstr(buf, RNG_DEBUG_ON, strlen(buf))) {
		ssi_dbg_rng = true;
	} else if (strnstr(buf, RNG_DEBUG_OFF, strlen(buf))) {
		ssi_dbg_rng = false;
#endif
	} else if (bbd.ssp_cb && bbd.ssp_cb->on_control) {
		/* Tell SHMD about the unknown control string */
		bbd.ssp_cb->on_control(bbd.ssp_priv, buf);
	}

	return len;
}









/**
 * bbd_common_open - Common open function for BBD devices
 *
 */
int bbd_common_open(struct inode *inode, struct file *filp)
{
	unsigned int minor = iminor(inode);
	struct circ_buf *circ = &bbd.priv[minor].read_buf;

	pr_info("%s++\n", __func__);

	if (minor >= BBD_DEVICE_INDEX)
		return -ENODEV;

	pr_info("%s", bbd.priv[minor].name);

	if (bbd.priv[minor].busy && minor != BBD_MINOR_CONTROL)
		return -EBUSY;

	bbd.priv[minor].busy = true;

	/* Reset circ buffer */
	circ->head = circ->tail = 0;

	filp->private_data = &bbd;

	pr_info("%s--\n", __func__);
	return 0;
}

/**
 * bbd_common_release - Common release function for BBD devices
 */
static int bbd_common_release(struct inode *inode, struct file *filp)
{
	unsigned int minor = iminor(inode);


	pr_info("%s++\n", __func__);

	BUG_ON(minor >= BBD_DEVICE_INDEX);
	pr_info("%s", bbd.priv[minor].name);

	bbd.priv[minor].busy = false;

	pr_info("%s--\n", __func__);
	return 0;
}

/**
 * bbd_common_read - Common read function for BBD devices
 *
 * lhd reads from BBD devices via this function
 *
 */
static ssize_t bbd_common_read(struct file *filp,
							   char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int minor = iminor(filp->f_path.dentry->d_inode);

	struct circ_buf *circ = &bbd.priv[minor].read_buf;
	size_t rd_size = 0;

	pr_info("%s++\n", __func__);
	BUG_ON(minor >= BBD_DEVICE_INDEX);

	mutex_lock(&bbd.priv[minor].lock);

	/* Copy from circ buffer to lhd
	 * Because lhd's buffer is linear,
	 * we may require 2 copies from [tail..end] and [end..head]
	 */
	do {
		size_t cnt_to_end = CIRC_CNT_TO_END(circ->head,
											circ->tail, BBD_BUFF_SIZE);
		size_t copied = min(cnt_to_end, size);

		WARN_ON(copy_to_user(buf + rd_size,
							(void *) circ->buf + circ->tail, copied));
		size -= copied;
		rd_size += copied;
		circ->tail = (circ->tail + copied) & (BBD_BUFF_SIZE - 1);

	} while (size > 0 && CIRC_CNT(circ->head, circ->tail, BBD_BUFF_SIZE));

	mutex_unlock(&bbd.priv[minor].lock);

	bbd_log_hex(bbd_dev_name[minor], buf, rd_size);

	pr_info("%s--\n", __func__);
	return rd_size;
}

/**
 * bbd_common_write - Common write function for BBD devices *
 * lhd writes to BBD devices via this function
 *
 */
static ssize_t bbd_common_write(struct file *filp,
								const char __user *buf, size_t size,
								loff_t *ppos)
{
	unsigned int minor = iminor(filp->f_path.dentry->d_inode);


	BUG_ON(size >= BBD_BUFF_SIZE);

	WARN_ON(copy_from_user(bbd.priv[minor].write_buf, buf, size));

	return size;
}

/**
 * bbd_common_poll - Common poll function for BBD devices
 *
 */
static unsigned int bbd_common_poll(struct file *filp, poll_table *wait)
{
	unsigned int minor = iminor(filp->f_path.dentry->d_inode);

	struct circ_buf *circ = &bbd.priv[minor].read_buf;
	unsigned int mask = 0;

	BUG_ON(minor >= BBD_DEVICE_INDEX);

	poll_wait(filp, &bbd.priv[minor].poll_wait, wait);

	if (CIRC_CNT(circ->head, circ->tail, BBD_BUFF_SIZE))
		mask |= POLLIN;

	return mask;
}










/**
 * bbd_sensor_write - BBD's RPC calls this function to send sensor packet
 *
 * @buf: contains sensor packet coming from gpsd/lhd
 *
 */
ssize_t bbd_sensor_write(const char *buf, size_t size)
{
	/* Copies into /dev/bbd_shmd.
	*  If SHMD was sleeping in poll_wait, bbd_on_read() wakes it up also */
	bbd_on_read(BBD_MINOR_SHMD, buf, size);

	/* OK. Now call pre-registered SHMD callbacks */
	if (bbd.ssp_cb->on_packet)
		bbd.ssp_cb->on_packet(bbd.ssp_priv,
							  bbd.priv[BBD_MINOR_SHMD].write_buf, size);
	else if (bbd.ssp_cb->on_packet_alarm)
		bbd.ssp_cb->on_packet_alarm(bbd.ssp_priv);
	else
		pr_err("%s no SSP on_packet callback registered. "
				"Dropped %u bytes\n", __func__, (unsigned int)size);

	return size;
}

/**
 * bbd_control_write - Write function for BBD control (/dev/bbd_control)
 *
 *  Receives control string from lhd and handles it
 *
 */
ssize_t bbd_control_write(struct file *filp, const char __user *buf,
						  size_t size, loff_t *ppos)
{
	unsigned int minor = iminor(filp->f_path.dentry->d_inode);


	/* get command string first */
	ssize_t len = bbd_common_write(filp, buf, size, ppos);
	if (len <= 0)
		return len;

	/* Process received command string */
	return bbd_control(bbd.priv[minor].write_buf, len);
}

ssize_t bbd_patch_read(struct file *filp, char __user *buf,
						size_t size, loff_t *ppos)
{
	ssize_t rd_size = size;
	size_t  offset = filp->f_pos;
	struct bbd_device *bbd = filp->private_data;
	unsigned char *curr_bbd_patch;
	size_t bbd_patch_sz;

	if (bbd->legacy_patch) {
		curr_bbd_patch = legacy_bbd_patch;
		bbd_patch_sz = sizeof(legacy_bbd_patch);
	} else {
		curr_bbd_patch = bbd_patch;
		bbd_patch_sz = sizeof(bbd_patch);
	}

	if (offset >= bbd_patch_sz) {       /* signal EOF */
		*ppos = 0;
		return 0;
	}
	if (offset+size > bbd_patch_sz)
		rd_size = bbd_patch_sz - offset;
	if (copy_to_user(buf, curr_bbd_patch + offset, rd_size))
		rd_size = -EFAULT;
	else
		*ppos = filp->f_pos + rd_size;

	return rd_size;
}






static ssize_t store_sysfs_bbd_control(struct device *dev,
									   struct device_attribute *attr,
									   const char *buf, size_t len)
{
	bbd_control(buf, strlen(buf)+1);
	return len;
}

ssize_t bbd_request_mcu(bool on);
static ssize_t show_sysfs_bbd_pl(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(bbd, 0220, NULL, store_sysfs_bbd_control);
static DEVICE_ATTR(pl, 0440, show_sysfs_bbd_pl, NULL);

static struct attribute *bbd_attributes[] = {
	&dev_attr_bbd.attr,
	&dev_attr_pl.attr,
	NULL
};

static const struct attribute_group bbd_group = {
	.attrs = bbd_attributes,
};







void bbd_log_hex(const char *pIntroduction,
		const unsigned char *pData,
		unsigned long        ulDataLen)
{
	const unsigned char *pDataEnd = pData + ulDataLen;

	if (likely(!bbd.db))
		return;
	if (!pIntroduction)
		pIntroduction = "...unknown...";

	while (pData < pDataEnd) {
		char buf[128];
		size_t bufsize = sizeof(buf) - 3;
		size_t lineLen = pDataEnd - pData;
		size_t perLineCount = lineLen;
		if (lineLen > 32) {
			lineLen = 32;
			perLineCount = lineLen;
		}

		snprintf(buf, bufsize, "%s [%u] { ", pIntroduction,
					(unsigned int)lineLen);

		for (; perLineCount > 0; ++pData, --perLineCount) {
			size_t len = strlen(buf);
			snprintf(buf+len, bufsize - len, "%02X ", *pData);
		}
		printk(KERN_INFO"%s}\n", buf);
	}
}

/**
 *
 * bbd_on_read - Push data into read buffer of specified char device.
 *   if minor is bbd_sensor
 *
 * @buf: linear buffer
 */
ssize_t bbd_on_read(unsigned int minor, const unsigned char *buf,
					size_t size)
{
	struct circ_buf *circ = &bbd.priv[minor].read_buf;
	size_t wr_size = 0;

	bbd_log_hex(bbd_dev_name[minor], buf, size);

	mutex_lock(&bbd.priv[minor].lock);

	/* If there's not enough speace, drop it but try waking up reader */
	if (CIRC_SPACE(circ->head, circ->tail, BBD_BUFF_SIZE) < size) {
		pr_err("%s read buffer full. Dropping %u bytes\n",
				bbd_dev_name[minor], (unsigned int)size);
		goto skip;
	}

	/* Copy into circ buffer from linear buffer
	 * We may require 2 copies from [head..end] and [start..head]
	 */
	do {
		size_t space_to_end = CIRC_SPACE_TO_END(circ->head,
												circ->tail, BBD_BUFF_SIZE);
		size_t copied = min(space_to_end, size);

		memcpy((void *) circ->buf + circ->head, buf + wr_size, copied);
		size -= copied;
		wr_size += copied;
		circ->head = (circ->head + copied) & (BBD_BUFF_SIZE - 1);

	} while (size > 0 && CIRC_SPACE(circ->head, circ->tail, BBD_BUFF_SIZE));
skip:
	mutex_unlock(&bbd.priv[minor].lock);

	/* Wake up reader */
	wake_up(&bbd.priv[minor].poll_wait);


	return wr_size;
}

ssize_t bbd_request_mcu(bool on)
{
	printk("%s(%s) called", __func__, (on)?"On":"Off");
	if (on)
		return bbd_on_read(BBD_MINOR_CONTROL,
						   GPSD_SENSOR_ON, strlen(GPSD_SENSOR_ON)+1);
	else {
		bbd.ssp_cb->on_mcu_ready(bbd.ssp_priv, false);
		return bbd_on_read(BBD_MINOR_CONTROL,
						   GPSD_SENSOR_OFF, strlen(GPSD_SENSOR_OFF)+1);
	}
}
EXPORT_SYMBOL(bbd_request_mcu);






static int bbd_suspend(pm_message_t state)
{
	pr_info("[SSPBBD]: %s ++ \n", __func__);

#ifdef CONFIG_SENSORS_SSP
	/* Call SSP suspend */
	if (pssp_driver->driver.pm && pssp_driver->driver.pm->suspend)
		pssp_driver->driver.pm->suspend(&dummy_spi.dev);
#endif
	mdelay(20);

	pr_info("[SSPBBD]: %s -- \n", __func__);
	return 0;
}

static int bbd_resume(void)
{
#ifdef CONFIG_SENSORS_SSP
	/* Call SSP resume */
	if (pssp_driver->driver.pm && pssp_driver->driver.pm->suspend)
		pssp_driver->driver.pm->resume(&dummy_spi.dev);
#endif

	return 0;
}

static int bbd_notifier(struct notifier_block *nb,
						unsigned long event, void *data)
{
	pm_message_t state = {0};
		switch (event) {
		case PM_SUSPEND_PREPARE:
			printk("%s going to sleep", __func__);
			state.event = event;
			bbd_suspend(state);
			break;
		case PM_POST_SUSPEND:
			printk("%s waking up", __func__);
			bbd_resume();
			break;
	}
	return NOTIFY_OK;
}

static struct notifier_block bbd_notifier_block = {
				.notifier_call = bbd_notifier,
};








static const struct file_operations bbd_fops[BBD_DEVICE_INDEX] = {
	/* bbd shmd file operations */
	{
			.owner          =  THIS_MODULE,
	},
	/* bbd sensor file operations */
		{
				.owner          =  THIS_MODULE,
				.open           =  bbd_common_open,
				.release        =  bbd_common_release,
				.read           =  bbd_common_read,
				.write          =  NULL,
				.poll           =  bbd_common_poll,
	},
	/* bbd control file operations */
	{
		.owner		=  THIS_MODULE,
		.open		=  bbd_common_open,
		.release	=  bbd_common_release,
		.read		=  bbd_common_read,
		.write		=  bbd_control_write,
		.poll		=  bbd_common_poll,
	},
	/* bbd patch file operations */
	{
		.owner		=  THIS_MODULE,
		.open		=  bbd_common_open,
		.release	=  bbd_common_release,
		.read		=  bbd_patch_read,
		.write		=  NULL, /* /dev/bbd_patch is read-only */
		.poll		=  NULL,
	},
	/* bbd ssi spi debug operations
	{
		.owner          =  THIS_MODULE,
		.open		=  bbd_common_open,
		.release	=  bbd_common_release,
		.read           =  NULL,
		.write          =  bbd_ssi_spi_debug_write,
		.poll           =  NULL,
	}
	*/
};


int bbd_init(struct device *dev, bool legacy_patch)
{
	int minor, ret = -ENOMEM;
	struct timespec ts1;
	unsigned long start, elapsed;

	ts1 = ktime_to_timespec(ktime_get_boottime());
	start = ts1.tv_sec * 1000000000ULL + ts1.tv_nsec;


	/* Initialize BBD device */
	memset(&bbd, 0, sizeof(bbd));

	bbd.legacy_patch = legacy_patch;

	/* Create class which is required for device_create() */
	bbd.class = class_create(THIS_MODULE, "bbd");
	if (IS_ERR(bbd.class)) {
		WARN("BBD:%s() failed to create class \"bbd\"", __func__);
		goto exit;
	}

	/* Create BBD char devices */
	for (minor = 0; minor < BBD_DEVICE_INDEX; minor++) {
		dev_t devno = MKDEV(BBD_DEVICE_MAJOR, minor);
		struct cdev *cdev = &bbd.priv[minor].dev;
		const char *name = bbd_dev_name[minor];
		struct device *dev;

		/* Init buf, waitqueue, mutex, etc. */
		bbd.priv[minor].name = bbd_dev_name[minor];
		bbd.priv[minor].read_buf.buf = bbd.priv[minor]._read_buf;

		init_waitqueue_head(&bbd.priv[minor].poll_wait);
		mutex_init(&bbd.priv[minor].lock);

		/* Don't register /dev/bbd_shmd */
		if (minor == BBD_MINOR_SHMD)
			continue;
		/* Reserve char device number (a.k.a, major, minor)
		 * for this BBD device */
		ret = register_chrdev_region(devno, 1, name);
		if (ret) {
			pr_err("BBD:%s() failed to register_chrdev_region() "
					"\"%s\", ret=%d", __func__, name, ret);
			goto free_class;
		}

		/* Register cdev which relates above device
		 * number with this BBD device */
		cdev_init(cdev, &bbd_fops[minor]);
		cdev->owner = THIS_MODULE;
		cdev->ops = &bbd_fops[minor];
		ret = cdev_add(cdev, devno, 1);
		if (ret) {
			pr_err("BBD:%s()) failed to cdev_add() \"%s\", ret=%d",
						__func__, name, ret);
			unregister_chrdev_region(devno, 1);
			goto free_class;
		}

		/* Let it show in FS */
		dev = device_create(bbd.class, NULL, devno, NULL, "%s", name);
		if (IS_ERR_OR_NULL(dev)) {
			pr_err("BBD:%s() failed to device_create() "
				"\"%s\", ret=%d", __func__, name, ret);
			unregister_chrdev_region(devno, 1);
			cdev_del(&bbd.priv[minor].dev);
			goto free_class;
		}

		/* Done. Put success log and init BBD specific fields */
		pr_info("BBD:%s(%d,%d) registered /dev/%s\n",
			      __func__, BBD_DEVICE_MAJOR, minor, name);

	}

	/* Register sysfs entry */
	bbd.kobj = kobject_create_and_add("bbd", NULL);
	BUG_ON(!bbd.kobj);
	ret = sysfs_create_group(bbd.kobj, &bbd_group);
	if (ret < 0) {
		pr_err("%s failed to sysfs_create_group \"bbd\", ret = %d",
							__func__, ret);
		goto free_kobj;
	}


	/* Register PM */
	ret = register_pm_notifier(&bbd_notifier_block);
	BUG_ON(ret);

#ifdef CONFIG_SENSORS_SSP
	/* Now, we can initialize SSP */
	BUG_ON(device_register(&dummy_spi.dev));
	{
		struct spi_device *spi = to_spi_device(dev);
		void *org_priv, *new_priv;

		org_priv = spi_get_drvdata(spi);
		pssp_driver->probe(spi);
		new_priv = spi_get_drvdata(spi);
		spi_set_drvdata(spi, org_priv);
		spi_set_drvdata(&dummy_spi, new_priv);

	}
#endif
	ts1 = ktime_to_timespec(ktime_get_boottime());
	elapsed = (ts1.tv_sec * 1000000000ULL + ts1.tv_nsec) - start;
	pr_info("BBD:%s %lu nsec elapsed\n", __func__, elapsed);

		return 0;

free_kobj:
	kobject_put(bbd.kobj);
free_class:
	while (--minor > BBD_MINOR_SHMD) {
		dev_t devno = MKDEV(BBD_DEVICE_MAJOR, minor);
		struct cdev *cdev = &bbd.priv[minor].dev;

		device_destroy(bbd.class, devno);
		cdev_del(cdev);
		unregister_chrdev_region(devno, 1);
	}
	class_destroy(bbd.class);
exit:
	return ret;
}

static void __exit bbd_exit(void)
{
	int minor;

	pr_info("%s ++\n", __func__);

#ifdef CONFIG_SENSORS_SSP
	/* Shutdown SSP first*/
	pssp_driver->shutdown(&dummy_spi);
#endif

	/* Remove sysfs entry */
	sysfs_remove_group(bbd.kobj, &bbd_group);

	/* Remove BBD char devices */
	for (minor = BBD_MINOR_SENSOR; minor < BBD_DEVICE_INDEX; minor++) {
		dev_t devno = MKDEV(BBD_DEVICE_MAJOR, minor);
		struct cdev *cdev = &bbd.priv[minor].dev;
		const char *name = bbd_dev_name[minor];

		device_destroy(bbd.class, devno);
		cdev_del(cdev);
		unregister_chrdev_region(devno, 1);

		pr_info("%s(%d,%d) unregistered /dev/%s\n",
			__func__, BBD_DEVICE_MAJOR, minor, name);
	}

	/* Remove class */
	class_destroy(bbd.class);
	/* Done. Put success log */
	pr_info("%s --\n", __func__);
}

MODULE_AUTHOR("Broadcom");
MODULE_LICENSE("Dual BSD/GPL");


