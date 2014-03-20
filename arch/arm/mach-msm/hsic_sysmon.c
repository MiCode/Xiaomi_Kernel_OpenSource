/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

/* add additional information to our printk's */
#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/debugfs.h>
#include <soc/qcom/hsic_sysmon.h>
#include <soc/qcom/sysmon.h>

#define DRIVER_DESC	"HSIC System monitor driver"

enum hsic_sysmon_op {
	HSIC_SYSMON_OP_READ = 0,
	HSIC_SYSMON_OP_WRITE,
	NUM_OPS
};

struct hsic_sysmon {
	struct usb_device	*udev;
	struct usb_interface	*ifc;
	__u8			in_epaddr;
	__u8			out_epaddr;
	unsigned int		pipe[NUM_OPS];
	struct kref		kref;
	struct platform_device	pdev;
	int			id;

	/* debugging counters */
	atomic_t		dbg_bytecnt[NUM_OPS];
	atomic_t		dbg_pending[NUM_OPS];
};
static struct hsic_sysmon *hsic_sysmon_devices[NUM_HSIC_SYSMON_DEVS];

static void hsic_sysmon_delete(struct kref *kref)
{
	struct hsic_sysmon *hs = container_of(kref, struct hsic_sysmon, kref);

	usb_put_dev(hs->udev);
	hsic_sysmon_devices[hs->id] = NULL;
	kfree(hs);
}

/**
 * hsic_sysmon_open() - Opens the system monitor bridge.
 * @id: the HSIC system monitor device to open
 *
 * This should only be called after the platform_device "sys_mon" with id
 * SYSMON_SS_EXT_MODEM has been added. The simplest way to do that is to
 * register a platform_driver and its probe will be called when the HSIC
 * device is ready.
 */
int hsic_sysmon_open(enum hsic_sysmon_device_id id)
{
	struct hsic_sysmon	*hs;

	if (id >= NUM_HSIC_SYSMON_DEVS) {
		pr_err("invalid dev id(%d)", id);
		return -ENODEV;
	}

	hs = hsic_sysmon_devices[id];
	if (!hs) {
		pr_err("dev is null");
		return -ENODEV;
	}

	kref_get(&hs->kref);

	return 0;
}
EXPORT_SYMBOL(hsic_sysmon_open);

/**
 * hsic_sysmon_close() - Closes the system monitor bridge.
 * @id: the HSIC system monitor device to close
 */
void hsic_sysmon_close(enum hsic_sysmon_device_id id)
{
	struct hsic_sysmon	*hs;

	if (id >= NUM_HSIC_SYSMON_DEVS) {
		pr_err("invalid dev id(%d)", id);
		return;
	}

	hs = hsic_sysmon_devices[id];
	kref_put(&hs->kref, hsic_sysmon_delete);
}
EXPORT_SYMBOL(hsic_sysmon_close);

/**
 * hsic_sysmon_readwrite() - Common function to send read/write over HSIC
 */
static int hsic_sysmon_readwrite(enum hsic_sysmon_device_id id, void *data,
				 size_t len, size_t *actual_len, int timeout,
				 enum hsic_sysmon_op op)
{
	struct hsic_sysmon	*hs;
	int			ret;
	const char		*opstr = (op == HSIC_SYSMON_OP_READ) ?
						"read" : "write";

	pr_debug("%s: id:%d, data len:%d, timeout:%d", opstr, id, len, timeout);

	if (id >= NUM_HSIC_SYSMON_DEVS) {
		pr_err("invalid dev id(%d)", id);
		return -ENODEV;
	}

	if (!len) {
		pr_err("length(%d) must be greater than 0", len);
		return -EINVAL;
	}

	hs = hsic_sysmon_devices[id];
	if (!hs) {
		pr_err("device was not opened");
		return -ENODEV;
	}

	if (!hs->ifc) {
		pr_err("can't %s, device disconnected", opstr);
		return -ENODEV;
	}

	ret = usb_autopm_get_interface(hs->ifc);
	if (ret < 0) {
		dev_err(&hs->ifc->dev, "can't %s, autopm_get failed:%d\n",
			opstr, ret);
		return ret;
	}

	atomic_inc(&hs->dbg_pending[op]);

	ret = usb_bulk_msg(hs->udev, hs->pipe[op], data, len, actual_len,
				timeout);

	atomic_dec(&hs->dbg_pending[op]);

	if (ret)
		dev_err(&hs->ifc->dev,
			"can't %s, usb_bulk_msg failed, err:%d\n", opstr, ret);
	else
		atomic_add(*actual_len, &hs->dbg_bytecnt[op]);

	usb_autopm_put_interface(hs->ifc);
	return ret;
}

/**
 * hsic_sysmon_read() - Read data from the HSIC sysmon interface.
 * @id: the HSIC system monitor device to open
 * @data: pointer to caller-allocated buffer to fill in
 * @len: length in bytes of the buffer
 * @actual_len: pointer to a location to put the actual length read
 *	in bytes
 * @timeout: time in msecs to wait for the message to complete before
 *	timing out (if 0 the wait is forever)
 *
 * Context: !in_interrupt ()
 *
 * Synchronously reads data from the HSIC interface. The call will return
 * after the read has completed, encountered an error, or timed out. Upon
 * successful return actual_len will reflect the number of bytes read.
 *
 * If successful, it returns 0, otherwise a negative error number.  The number
 * of actual bytes transferred will be stored in the actual_len paramater.
 */
int hsic_sysmon_read(enum hsic_sysmon_device_id id, char *data, size_t len,
		     size_t *actual_len, int timeout)
{
	return hsic_sysmon_readwrite(id, data, len, actual_len,
					timeout, HSIC_SYSMON_OP_READ);
}
EXPORT_SYMBOL(hsic_sysmon_read);

/**
 * hsic_sysmon_write() - Write data to the HSIC sysmon interface.
 * @id: the HSIC system monitor device to open
 * @data: pointer to caller-allocated buffer to write
 * @len: length in bytes of the data in buffer to write
 * @actual_len: pointer to a location to put the actual length written
 *	in bytes
 * @timeout: time in msecs to wait for the message to complete before
 *	timing out (if 0 the wait is forever)
 *
 * Context: !in_interrupt ()
 *
 * Synchronously writes data to the HSIC interface. The call will return
 * after the write has completed, encountered an error, or timed out. Upon
 * successful return actual_len will reflect the number of bytes written.
 *
 * If successful, it returns 0, otherwise a negative error number.  The number
 * of actual bytes transferred will be stored in the actual_len paramater.
 */
int hsic_sysmon_write(enum hsic_sysmon_device_id id, const char *data,
		      size_t len, int timeout)
{
	size_t actual_len;
	return hsic_sysmon_readwrite(id, (void *)data, len, &actual_len,
					timeout, HSIC_SYSMON_OP_WRITE);
}
EXPORT_SYMBOL(hsic_sysmon_write);

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	512
static ssize_t sysmon_debug_read_stats(struct file *file, char __user *ubuf,
					size_t count, loff_t *ppos)
{
	char	*buf;
	int	i, ret = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < NUM_HSIC_SYSMON_DEVS; i++) {
		struct hsic_sysmon *hs = hsic_sysmon_devices[i];
		if (!hs)
			continue;

		ret += scnprintf(buf + ret, DEBUG_BUF_SIZE - ret,
				"---HSIC Sysmon #%d---\n"
				"epin:%d, epout:%d\n"
				"bytes to host: %d\n"
				"bytes to mdm: %d\n"
				"pending reads: %d\n"
				"pending writes: %d\n",
				i, hs->in_epaddr & ~0x80, hs->out_epaddr,
				atomic_read(
				    &hs->dbg_bytecnt[HSIC_SYSMON_OP_READ]),
				atomic_read(
				    &hs->dbg_bytecnt[HSIC_SYSMON_OP_WRITE]),
				atomic_read(
				    &hs->dbg_pending[HSIC_SYSMON_OP_READ]),
				atomic_read(
				    &hs->dbg_pending[HSIC_SYSMON_OP_WRITE])
				);
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, ret);
	kfree(buf);
	return ret;
}

static ssize_t sysmon_debug_reset_stats(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos)
{
	int	i;

	for (i = 0; i < NUM_HSIC_SYSMON_DEVS; i++) {
		struct hsic_sysmon *hs = hsic_sysmon_devices[i];
		if (hs) {
			atomic_set(&hs->dbg_bytecnt[HSIC_SYSMON_OP_READ], 0);
			atomic_set(&hs->dbg_bytecnt[HSIC_SYSMON_OP_WRITE], 0);
			atomic_set(&hs->dbg_pending[HSIC_SYSMON_OP_READ], 0);
			atomic_set(&hs->dbg_pending[HSIC_SYSMON_OP_WRITE], 0);
		}
	}

	return count;
}

const struct file_operations sysmon_stats_ops = {
	.read = sysmon_debug_read_stats,
	.write = sysmon_debug_reset_stats,
};

static struct dentry *dent;

static void hsic_sysmon_debugfs_init(void)
{
	struct dentry *dfile;

	dent = debugfs_create_dir("hsic_sysmon", 0);
	if (IS_ERR(dent))
		return;

	dfile = debugfs_create_file("status", 0444, dent, 0, &sysmon_stats_ops);
	if (!dfile || IS_ERR(dfile))
		debugfs_remove(dent);
}

static void hsic_sysmon_debugfs_cleanup(void)
{
	if (dent) {
		debugfs_remove_recursive(dent);
		dent = NULL;
	}
}
#else
static inline void hsic_sysmon_debugfs_init(void) { }
static inline void hsic_sysmon_debugfs_cleanup(void) { }
#endif

static void hsic_sysmon_pdev_release(struct device *dev) { }

static int
hsic_sysmon_probe(struct usb_interface *ifc, const struct usb_device_id *id)
{
	struct hsic_sysmon		*hs;
	struct usb_host_interface	*ifc_desc;
	struct usb_endpoint_descriptor	*ep_desc;
	int				i;
	int				ret = -ENOMEM;

	hs = kzalloc(sizeof(*hs), GFP_KERNEL);
	if (!hs) {
		pr_err("unable to allocate hsic_sysmon");
		return -ENOMEM;
	}

	hs->udev = usb_get_dev(interface_to_usbdev(ifc));
	hs->ifc = ifc;
	kref_init(&hs->kref);

	ifc_desc = ifc->cur_altsetting;
	for (i = 0; i < ifc_desc->desc.bNumEndpoints; i++) {
		ep_desc = &ifc_desc->endpoint[i].desc;

		if (!hs->in_epaddr && usb_endpoint_is_bulk_in(ep_desc)) {
			hs->in_epaddr = ep_desc->bEndpointAddress;
			hs->pipe[HSIC_SYSMON_OP_READ] =
				usb_rcvbulkpipe(hs->udev, hs->in_epaddr);
		}

		if (!hs->out_epaddr && usb_endpoint_is_bulk_out(ep_desc)) {
			hs->out_epaddr = ep_desc->bEndpointAddress;
			hs->pipe[HSIC_SYSMON_OP_WRITE] =
				usb_sndbulkpipe(hs->udev, hs->out_epaddr);
		}
	}

	if (!(hs->in_epaddr && hs->out_epaddr)) {
		pr_err("could not find bulk in and bulk out endpoints");
		ret = -ENODEV;
		goto error;
	}

	hs->id = HSIC_SYSMON_DEV_EXT_MODEM + id->driver_info;
	if (hs->id >= NUM_HSIC_SYSMON_DEVS) {
		pr_warn("invalid dev id(%d)", hs->id);
		hs->id = 0;
	}

	hsic_sysmon_devices[hs->id] = hs;
	usb_set_intfdata(ifc, hs);

	hs->pdev.name = "sys_mon";
	hs->pdev.id = SYSMON_SS_EXT_MODEM + hs->id;
	hs->pdev.dev.release = hsic_sysmon_pdev_release;
	platform_device_register(&hs->pdev);

	pr_debug("complete");

	return 0;

error:
	if (hs)
		kref_put(&hs->kref, hsic_sysmon_delete);

	return ret;
}

static void hsic_sysmon_disconnect(struct usb_interface *ifc)
{
	struct hsic_sysmon	*hs = usb_get_intfdata(ifc);

	platform_device_unregister(&hs->pdev);
	kref_put(&hs->kref, hsic_sysmon_delete);
	usb_set_intfdata(ifc, NULL);
}

static int hsic_sysmon_suspend(struct usb_interface *ifc, pm_message_t message)
{
	return 0;
}

static int hsic_sysmon_resume(struct usb_interface *ifc)
{
	return 0;
}

/* driver_info is the instance number when multiple devices are present */
static const struct usb_device_id hsic_sysmon_ids[] = {
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9048, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x904C, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9075, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9079, 1), .driver_info = 1, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908A, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909C, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909D, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909E, 2), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909F, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A0, 1), .driver_info = 0, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A4, 2), .driver_info = 0, },

	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, hsic_sysmon_ids);

static struct usb_driver hsic_sysmon_driver = {
	.name =		"hsic_sysmon",
	.probe =	hsic_sysmon_probe,
	.disconnect =	hsic_sysmon_disconnect,
	.suspend =	hsic_sysmon_suspend,
	.resume =	hsic_sysmon_resume,
	.reset_resume =	hsic_sysmon_resume,
	.id_table =	hsic_sysmon_ids,
	.supports_autosuspend = 1,
};

static int __init hsic_sysmon_init(void)
{
	int ret;

	ret = usb_register(&hsic_sysmon_driver);
	if (ret) {
		pr_err("unable to register " DRIVER_DESC);
		return ret;
	}

	hsic_sysmon_debugfs_init();
	return 0;
}

static void __exit hsic_sysmon_exit(void)
{
	hsic_sysmon_debugfs_cleanup();
	usb_deregister(&hsic_sysmon_driver);
}

module_init(hsic_sysmon_init);
module_exit(hsic_sysmon_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
