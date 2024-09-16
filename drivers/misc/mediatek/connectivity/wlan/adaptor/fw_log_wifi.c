/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/string.h>

#include "wmt_exp.h"
#include "stp_exp.h"
#include "connsys_debug_utility.h"

#if (CFG_ANDORID_CONNINFRA_SUPPORT == 1)
#include "fw_log_wifi.h"
#include "conninfra.h"
#endif

MODULE_LICENSE("Dual BSD/GPL");

#define PFX                         "[WIFI-FW] "
#define WIFI_FW_LOG_DBG             3
#define WIFI_FW_LOG_INFO            2
#define WIFI_FW_LOG_WARN            1
#define WIFI_FW_LOG_ERR             0

uint32_t fwDbgLevel = WIFI_FW_LOG_DBG;

#define WIFI_DBG_FUNC(fmt, arg...)	\
	do { \
		if (fwDbgLevel >= WIFI_FW_LOG_DBG) \
			pr_info(PFX "%s[D]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_INFO_FUNC(fmt, arg...)	\
	do { \
		if (fwDbgLevel >= WIFI_FW_LOG_INFO) \
			pr_info(PFX "%s[I]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_INFO_FUNC_LIMITED(fmt, arg...)	\
	do { \
		if (fwDbgLevel >= WIFI_FW_LOG_INFO) \
			pr_info_ratelimited(PFX "%s[L]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_WARN_FUNC(fmt, arg...)	\
	do { \
		if (fwDbgLevel >= WIFI_FW_LOG_WARN) \
			pr_info(PFX "%s[W]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_ERR_FUNC(fmt, arg...)	\
	do { \
		if (fwDbgLevel >= WIFI_FW_LOG_ERR) \
			pr_info(PFX "%s[E]: " fmt, __func__, ##arg); \
	} while (0)


#define WIFI_FW_LOG_IOC_MAGIC        (0xfc)
#define WIFI_FW_LOG_IOCTL_ON_OFF     _IOW(WIFI_FW_LOG_IOC_MAGIC, 0, int)
#define WIFI_FW_LOG_IOCTL_SET_LEVEL  _IOW(WIFI_FW_LOG_IOC_MAGIC, 1, int)

#define WIFI_FW_LOG_CMD_ON_OFF        0
#define WIFI_FW_LOG_CMD_SET_LEVEL     1

#if (CFG_ANDORID_CONNINFRA_SUPPORT == 1)
#define CONNLOG_TYPE_WIFI 0 /* CONN_DEBUG_TYPE_WIFI */
#endif

typedef void (*wifi_fwlog_event_func_cb)(int, int);
wifi_fwlog_event_func_cb pfFwEventFuncCB;
static wait_queue_head_t wq;

#if (CFG_ANDORID_CONNINFRA_SUPPORT == 1)
typedef int (*wifi_fwlog_chkbushang_func_cb)(void *, uint8_t);
wifi_fwlog_chkbushang_func_cb gpfn_check_bus_hang;
#endif

static struct semaphore ioctl_mtx;

#if (CFG_ANDORID_CONNINFRA_SUPPORT == 1)
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
struct connsys_dump_ctx *g_wifi_coredump_handler;
struct coredump_event_cb g_wifi_coredump_cb = {
	.reg_readable = fw_log_reg_readable,
	.poll_cpupcr = NULL,
};
#endif /* CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT */
#endif /* CFG_ANDORID_CONNINFRA_SUPPORT */

void wifi_fwlog_event_func_register(wifi_fwlog_event_func_cb func)
{
	WIFI_INFO_FUNC("wifi_fwlog_event_func_register %p\n", func);
	pfFwEventFuncCB = func;
}
EXPORT_SYMBOL(wifi_fwlog_event_func_register);

int wifi_fwlog_onoff_status(void)
{
	int ret = 88;
	return ret;
}
EXPORT_SYMBOL(wifi_fwlog_onoff_status);

static int fw_log_wifi_open(struct inode *inode, struct file *file)
{
	WIFI_INFO_FUNC("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	return 0;
}

static int fw_log_wifi_release(struct inode *inode, struct file *file)
{
	WIFI_INFO_FUNC("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	return 0;
}

static ssize_t fw_log_wifi_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	size_t ret = 0;

	WIFI_INFO_FUNC_LIMITED("fw_log_wifi_read len --> %d\n", (uint32_t) len);

	ret = connsys_log_read_to_user(CONNLOG_TYPE_WIFI, buf, len);

	return ret;
}

static unsigned int fw_log_wifi_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &wq, wait);

	if (connsys_log_get_buf_size(CONNLOG_TYPE_WIFI) > 0)
		return POLLIN|POLLRDNORM;
	return 0;
}


static long fw_log_wifi_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	down(&ioctl_mtx);
	switch (cmd) {
	case WIFI_FW_LOG_IOCTL_ON_OFF:{
		unsigned int log_on_off = (unsigned int) arg;

		WIFI_INFO_FUNC("fw_log_wifi_unlocked_ioctl WIFI_FW_LOG_IOCTL_ON_OFF start\n");

		if (pfFwEventFuncCB) {
			WIFI_INFO_FUNC("WIFI_FW_LOG_IOCTL_ON_OFF invoke:%d\n", (int)log_on_off);
			pfFwEventFuncCB(WIFI_FW_LOG_CMD_ON_OFF, log_on_off);
		} else
			WIFI_ERR_FUNC("WIFI_FW_LOG_IOCTL_ON_OFF invoke failed\n");

		WIFI_INFO_FUNC("fw_log_wifi_unlocked_ioctl WIFI_FW_LOG_IOCTL_ON_OFF end\n");
		break;
	}
	case WIFI_FW_LOG_IOCTL_SET_LEVEL:{
		unsigned int log_level = (unsigned int) arg;

		WIFI_INFO_FUNC("fw_log_wifi_unlocked_ioctl WIFI_FW_LOG_IOCTL_SET_LEVEL start\n");

		if (pfFwEventFuncCB) {
			WIFI_INFO_FUNC("WIFI_FW_LOG_IOCTL_SET_LEVEL invoke:%d\n", (int)log_level);
			pfFwEventFuncCB(WIFI_FW_LOG_CMD_SET_LEVEL, log_level);
		} else
			WIFI_ERR_FUNC("WIFI_FW_LOG_IOCTL_ON_OFF invoke failed\n");

		WIFI_INFO_FUNC("fw_log_wifi_unlocked_ioctl WIFI_FW_LOG_IOCTL_SET_LEVEL end\n");
		break;
	}
	default:
		ret = -EPERM;
	}
	WIFI_INFO_FUNC("fw_log_wifi_unlocked_ioctl cmd --> %d, ret=%d\n", cmd, ret);
	up(&ioctl_mtx);
	return ret;
}


#if (CFG_ANDORID_CONNINFRA_SUPPORT == 1)
int fw_log_wifi_irq_handler(void)
{
	return connsys_log_irq_handler(CONN_DEBUG_TYPE_WIFI);
}
EXPORT_SYMBOL(fw_log_wifi_irq_handler);

#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)

void fw_log_bug_hang_register(void *func)
{
	WIFI_INFO_FUNC("fw_log_bug_hang_register: %p\n", func);
	gpfn_check_bus_hang = (wifi_fwlog_chkbushang_func_cb)func;
}
EXPORT_SYMBOL(fw_log_bug_hang_register);

int fw_log_reg_readable(void)
{
	int ret = 1;

	if (conninfra_reg_readable() == 0) {
		WIFI_INFO_FUNC("conninfra_reg_readable: 0\n");
		ret = 0;
	}

	if (gpfn_check_bus_hang) {
		if (gpfn_check_bus_hang(NULL, 0) != 0) {
			WIFI_INFO_FUNC("gpfn_check_bus_hang: 1\n");
			ret = 0;
		}
	}

	WIFI_INFO_FUNC("fw_log_reg_readable: %d\n", ret);

	return ret;
}

void fw_log_connsys_coredump_init(void)
{
	g_wifi_coredump_handler = connsys_coredump_init(CONN_DEBUG_TYPE_WIFI, &g_wifi_coredump_cb);
	if (g_wifi_coredump_handler == NULL)
		WIFI_INFO_FUNC("connsys_coredump_init init fail!\n");
}
EXPORT_SYMBOL(fw_log_connsys_coredump_init);

void fw_log_connsys_coredump_deinit(void)
{
	connsys_coredump_deinit(g_wifi_coredump_handler);
}
EXPORT_SYMBOL(fw_log_connsys_coredump_deinit);

void fw_log_connsys_coredump_start(unsigned int drv, char *reason)
{
	connsys_coredump_start(g_wifi_coredump_handler, 0, (enum consys_drv_type)drv, reason);
	connsys_coredump_clean(g_wifi_coredump_handler);
}
EXPORT_SYMBOL(fw_log_connsys_coredump_start);
#endif /* CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT */
#endif /* CFG_ANDORID_CONNINFRA_SUPPORT */

#ifdef CONFIG_COMPAT
static long fw_log_wifi_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int32_t wait_cnt = 0;

	WIFI_INFO_FUNC("COMPAT fw_log_wifi_compact_ioctl cmd --> %d\n", cmd);

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	while (wait_cnt < 2000) {
		if (pfFwEventFuncCB)
			break;
		if (wait_cnt % 20 == 0)
			WIFI_ERR_FUNC("Wi-Fi driver is not ready for 2s\n");
		msleep(100);
		wait_cnt++;
	}
	fw_log_wifi_unlocked_ioctl(filp, cmd, arg);

	return ret;
}
#endif

const struct file_operations fw_log_wifi_fops = {
	.open = fw_log_wifi_open,
	.release = fw_log_wifi_release,
	.read = fw_log_wifi_read,
	.poll = fw_log_wifi_poll,
	.unlocked_ioctl = fw_log_wifi_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fw_log_wifi_compat_ioctl,
#endif
};

struct fw_log_wifi_device {
	struct cdev cdev;
	dev_t devno;
	struct class *driver_class;
	struct device *class_dev;
};

#define FW_LOG_WIFI_DRIVER_NAME "fw_log_wifi"
static struct fw_log_wifi_device *fw_log_wifi_dev;
static int fw_log_wifi_major;

static void fw_log_wifi_event_cb(void)
{
	wake_up_interruptible(&wq);
}

int fw_log_wifi_init(void)
{
	int result = 0;
	int err = 0;

	fw_log_wifi_dev = kmalloc(sizeof(struct fw_log_wifi_device), GFP_KERNEL);

	if (fw_log_wifi_dev == NULL) {
		result = -ENOMEM;
		goto return_fn;
	}

	fw_log_wifi_dev->devno = MKDEV(fw_log_wifi_major, 0);

	result = alloc_chrdev_region(&fw_log_wifi_dev->devno, 0, 1, FW_LOG_WIFI_DRIVER_NAME);
	fw_log_wifi_major = MAJOR(fw_log_wifi_dev->devno);
	WIFI_INFO_FUNC("alloc_chrdev_region result %d, major %d\n", result, fw_log_wifi_major);

	if (result < 0)
		return result;

	fw_log_wifi_dev->driver_class = class_create(THIS_MODULE, FW_LOG_WIFI_DRIVER_NAME);

	if (IS_ERR(fw_log_wifi_dev->driver_class)) {
		result = -ENOMEM;
		WIFI_ERR_FUNC("class_create failed %d.\n", result);
		goto unregister_chrdev_region;
	}

	fw_log_wifi_dev->class_dev = device_create(fw_log_wifi_dev->driver_class,
		NULL, fw_log_wifi_dev->devno, NULL, FW_LOG_WIFI_DRIVER_NAME);

	if (!fw_log_wifi_dev->class_dev) {
		result = -ENOMEM;
		WIFI_ERR_FUNC("class_device_create failed %d.\n", result);
		goto class_destroy;
	}

	sema_init(&ioctl_mtx, 1);

	cdev_init(&fw_log_wifi_dev->cdev, &fw_log_wifi_fops);

	fw_log_wifi_dev->cdev.owner = THIS_MODULE;
	fw_log_wifi_dev->cdev.ops = &fw_log_wifi_fops;

	err = cdev_add(&fw_log_wifi_dev->cdev, fw_log_wifi_dev->devno, 1);
	if (err) {
		result = -ENOMEM;
		WIFI_ERR_FUNC("Error %d adding fw_log_wifi dev.\n", err);
		goto cdev_del;
	}

	/* integrated with common debug utility */
	init_waitqueue_head(&wq);
	connsys_log_init(CONNLOG_TYPE_WIFI);
	connsys_log_register_event_cb(CONNLOG_TYPE_WIFI, fw_log_wifi_event_cb);
	pfFwEventFuncCB = NULL;
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
	gpfn_check_bus_hang = NULL;
#endif
	goto return_fn;

cdev_del:
	cdev_del(&fw_log_wifi_dev->cdev);
class_destroy:
	class_destroy(fw_log_wifi_dev->driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(fw_log_wifi_dev->devno, 1);
	kfree(fw_log_wifi_dev);
return_fn:
	return result;
}
EXPORT_SYMBOL(fw_log_wifi_init);

int fw_log_wifi_deinit(void)
{
	device_destroy(fw_log_wifi_dev->driver_class, fw_log_wifi_dev->devno);
	class_destroy(fw_log_wifi_dev->driver_class);
	cdev_del(&fw_log_wifi_dev->cdev);
	kfree(fw_log_wifi_dev);
	unregister_chrdev_region(MKDEV(fw_log_wifi_major, 0), 1);
	WIFI_INFO_FUNC("unregister_chrdev_region major %d\n", fw_log_wifi_major);

	/* integrated with common debug utility */
	connsys_log_deinit(CONNLOG_TYPE_WIFI);
	return 0;
}
EXPORT_SYMBOL(fw_log_wifi_deinit);

#endif
