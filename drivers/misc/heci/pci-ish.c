/*
 * PCI glue for HECI provider device (ISH) driver
 *
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/aio.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/uuid.h>
#include <linux/compat.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include "heci_dev.h"
#include "hw-ish.h"
#include "utils.h"
#include <linux/miscdevice.h>

#ifdef dev_dbg
#undef dev_dbg
#endif
static void no_dev_dbg(void *v, char *s, ...)
{
}
#define dev_dbg no_dev_dbg

/*#define dev_dbg dev_err*/

/*
 *  heci driver strings
 */
static bool nomsi;
module_param_named(nomsi, nomsi, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nomsi, "don't use msi (default = false)");

/* Currently this driver works as long as there is only a single AMT device. */
static struct pci_dev *heci_pci_device;

static DEFINE_PCI_DEVICE_TABLE(ish_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x22D8)},
	{0, }
};

MODULE_DEVICE_TABLE(pci, ish_pci_tbl);

static DEFINE_MUTEX(heci_mutex);

#ifdef TIMER_POLLING
/*
 * DD -- ISH timer-polling workaround for H-FPGA (and other platforms that fail to deliver interrupts)
 * NOTE: currently this will break (crash) if driver is unloaded
 */

#include <linux/timer.h>

struct timer_list	ish_poll_timer;
void	*timer_data;
struct work_struct	ish_poll_work;

void	ish_poll_work_fn(void *prm)
{
}

void	ish_poll_timer_fn(unsigned long unused)
{
	irqreturn_t	rv;

	rv = ish_irq_handler(0, timer_data);
	/*ISH_DBG_PRINT(KERN_ALERT "%s(): ish_irq_handler() returned %08X\n", __func__, rv);*/

	/* Reschedule timer */
	ish_poll_timer.expires += 2;
	add_timer(&ish_poll_timer);
}

#endif	/* TIMER_POLLING */


#if ISH_DEBUGGER

struct ish_hw *hw_dbg;

static int ishdbg_open(struct inode *inode, struct file *file)
{
	return	0;
}

static int ishdbg_release(struct inode *inode, struct file *file)
{
	return	0;
}

static char	dbg_resp_buf[2048];
static int	resp_buf_read;

static ssize_t ishdbg_read(struct file *file, char __user *ubuf, size_t length, loff_t *offset)
{
	int rv;
	int copy_len;

	if (resp_buf_read)
		return	0;	/* EOF */
	copy_len = (length > strlen(dbg_resp_buf)) ? strlen(dbg_resp_buf) : length;
	rv = copy_to_user(ubuf, dbg_resp_buf, copy_len);
	if (rv)
		return  -EINVAL;
	resp_buf_read = 1;
	return  copy_len;
}

static ssize_t ishdbg_write(struct file *file, const char __user *ubuf, size_t length, loff_t *offset)
{
	char    dbg_req_buf[768];
	char    cmd[768];
	int     rv;
	int     addr, count, sscanf_match, i, cur_index;
	volatile uint32_t *reg_data;

       	if (length > sizeof(dbg_req_buf))
		length = sizeof(dbg_req_buf);
	rv = copy_from_user(dbg_req_buf, ubuf, length);
	if (rv)
		return  -EINVAL;
	if (sscanf(dbg_req_buf, "%s ", cmd) != 1) {
		printk(KERN_ERR "[ish-dbg]) sscanf failed\n");
		return  -EINVAL;
	}
	sscanf_match = sscanf(dbg_req_buf + 2, "%x %d", &addr, &count);
	if (!strcmp(cmd, "d")) {
		/* Dump values: d <addr> [count] */
		if (sscanf_match == 1)
			count = 1;
		else if (sscanf_match != 2) {
			printk(KERN_ERR "[ish-dbg] sscanf failed, sscanf_match = %d\n", sscanf_match);
			return  -EINVAL;
		}
		if (addr % 4) {
			printk(KERN_ERR "[ish-dbg] address isn't aligned to 4 bytes\n");
			return -EINVAL;
		}
		cur_index = 0;
		for (i = 0; i < count; i++) {
			reg_data = (volatile uint32_t *)((char *)hw_dbg->mem_addr + addr + i*4);
			cur_index += sprintf(dbg_resp_buf + cur_index, "%08X ", *reg_data);
		}
		cur_index += sprintf(dbg_resp_buf + cur_index, "\n");
		resp_buf_read = 0;
	} else if (!strcmp(cmd, "e")) {
		/* Enter values e <addr> <value> */
		if (sscanf_match != 2) {
			printk(KERN_ERR "[ish-dbg] sscanf failed, sscanfMatch = %d\n", sscanf_match);
			return  -EINVAL;
		}
		if (addr % 4) {
			printk(KERN_ERR "[ish-dbg] address isn't aligned to 4 bytes\n");
			return -EINVAL;
		}
		reg_data = (volatile uint32_t *)((char *)hw_dbg->mem_addr + addr);
		*reg_data = count;
		sprintf(dbg_resp_buf, "OK\n");
		resp_buf_read = 0;
	}

	return  length;
}

static long ishdbg_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	return	0;
}

/*
 * file operations structure will be used for heci char device.
 */
static const struct file_operations ishdbg_fops = {
	.owner = THIS_MODULE,
	.read = ishdbg_read,
	.unlocked_ioctl = ishdbg_ioctl,
	.open = ishdbg_open,
	.release = ishdbg_release,
	.write = ishdbg_write,
	.llseek = no_llseek
};

/*
 * Misc Device Struct
 */
static struct miscdevice  ishdbg_misc_device = {
		.name = "ishdbg",
		.fops = &ishdbg_fops,
		.minor = MISC_DYNAMIC_MINOR,
};

#endif /* ISH_DEBUGGER */

#if ISH_LOG

void delete_from_log(struct heci_device *dev, size_t min_chars)
{
	int i;

	dev->log_tail = (dev->log_tail + min_chars - 1) % PRINT_BUFFER_SIZE;    /* log_tail points now on the last char to be deleted */
	for (i = dev->log_tail; dev->log_buffer[i] != '\n'; i = (i+1) % PRINT_BUFFER_SIZE)
		;
	dev->log_tail = (i+1) % PRINT_BUFFER_SIZE;
}

static void ish_print_log(struct heci_device *dev, char *format, ...)
{
	char tmp_buf[1024];
	va_list args;
	int length, i, full_space, free_space;
	unsigned long	flags;
	struct timeval tv;

	do_gettimeofday(&tv);
	i = sprintf(tmp_buf, "[%ld.%06ld] ", tv.tv_sec, tv.tv_usec);

	va_start(args, format);
	length = vsprintf(tmp_buf + i, format, args);
	va_end(args);

	length = length + i;
	if (tmp_buf[length-1] != '\n') {        /* if the msg does not end with \n, add it*/
		tmp_buf[length] = '\n';
		length++;
	}

	spin_lock_irqsave(&dev->log_spinlock, flags);

	full_space = dev->log_head - dev->log_tail;
	if (full_space < 0)
		full_space = PRINT_BUFFER_SIZE + full_space;
	free_space = PRINT_BUFFER_SIZE - full_space;

	if (free_space <= length)
		delete_from_log(dev, (length - free_space)+1);  /* needed at least 1 empty char to recognize whether buffer is full or empty */

	if (dev->log_head + length <= PRINT_BUFFER_SIZE)
		memcpy(dev->log_buffer + dev->log_head, tmp_buf, length);
	else {
		memcpy(dev->log_buffer + dev->log_head, tmp_buf,  PRINT_BUFFER_SIZE - dev->log_head);
		memcpy(dev->log_buffer, tmp_buf + PRINT_BUFFER_SIZE - dev->log_head, length - (PRINT_BUFFER_SIZE - dev->log_head));
	}
	dev->log_head = (dev->log_head + length) % PRINT_BUFFER_SIZE;

	spin_unlock_irqrestore(&dev->log_spinlock, flags);
}


void	g_ish_print_log(char *fmt, ...)
{
	char tmp_buf[1024];
	va_list args;
	struct heci_device	*dev = pci_get_drvdata(heci_pci_device);

	va_start(args, fmt);
	vsprintf(tmp_buf, fmt, args);
	va_end(args);
	ish_print_log(dev, tmp_buf);
}
EXPORT_SYMBOL(g_ish_print_log);


static ssize_t ish_read_log(struct heci_device *dev, char *buf, size_t size)
{
	int i, full_space, ret_val;
	unsigned long	flags;

	spin_lock_irqsave(&dev->log_spinlock, flags);

	if (dev->log_head == dev->log_tail) {/* log is empty */
		spin_unlock_irqrestore(&dev->log_spinlock, flags);
		return 0;
	}

	/* read size the minimum between full_space and the buffer size */
	full_space = dev->log_head - dev->log_tail;
	if (full_space < 0)
		full_space = PRINT_BUFFER_SIZE + full_space;

	if (full_space < size)
		i = (dev->log_tail + full_space) % PRINT_BUFFER_SIZE; /* =dev->log_head */
	else
		i = (dev->log_tail + size) % PRINT_BUFFER_SIZE;
	/* i is the last character to be readen */
	i = (i-1) % PRINT_BUFFER_SIZE;

	/* read from tail to last '\n' before i */
	for (; dev->log_buffer[i] != '\n'; i = (i-1) % PRINT_BUFFER_SIZE)
		;

	if (dev->log_tail < i) {
		memcpy(buf, dev->log_buffer + dev->log_tail, i - dev->log_tail + 1);
		ret_val = i - dev->log_tail + 1;
	} else {
		memcpy(buf, dev->log_buffer + dev->log_tail, PRINT_BUFFER_SIZE - dev->log_tail);
		memcpy(buf + PRINT_BUFFER_SIZE - dev->log_tail, dev->log_buffer, i + 1);
		ret_val = PRINT_BUFFER_SIZE - dev->log_tail + i + 1;
	}
	spin_unlock_irqrestore(&dev->log_spinlock, flags);
	return ret_val;
}

static ssize_t ish_read_flush_log(struct heci_device *dev, char *buf, size_t size)
{
	int ret;
	unsigned long	flags;

	ret = ish_read_log(dev, buf, size);
	spin_lock_irqsave(&dev->log_spinlock, flags);
	delete_from_log(dev, ret);
	spin_unlock_irqrestore(&dev->log_spinlock, flags);
	return ret;
}

/* show & store functions for both read and flush char devices*/
ssize_t show_read(struct device *dev, struct device_attribute *dev_attr, char *buf)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);
	return ish_read_log(heci_dev, buf, PAGE_SIZE);
}

ssize_t store_read(struct device *dev, struct device_attribute *dev_attr, const char *buf, size_t count)
{
	return count;
}

static struct device_attribute read_attr = {
	.attr = {
		.name = "ish_read_log",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_read,
	.store = store_read
};

ssize_t show_flush(struct device *dev, struct device_attribute *dev_attr, char *buf)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);
	return ish_read_flush_log(heci_dev, buf, PAGE_SIZE);
}

ssize_t store_flush(struct device *dev, struct device_attribute *dev_attr, const char *buf, size_t count)
{
	return count;
}

static struct device_attribute flush_attr = {
	.attr = {
		.name = "ish_flush_log",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_flush,
	.store = store_flush
};

#endif /* ISH_LOG */

ssize_t show_heci_dev_props(struct device *dev, struct device_attribute *dev_attr, char *buf)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;
	ssize_t	ret = -ENOENT;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);

	if (!strcmp(dev_attr->attr.name, "heci_dev_state")) {
		sprintf(buf, "%u\n", (unsigned)heci_dev->dev_state);
		ret = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name, "hbm_state")) {
		sprintf(buf, "%u\n", (unsigned)heci_dev->hbm_state);
		ret = strlen(buf);
	}

	return	ret;
}

ssize_t store_heci_dev_props(struct device *dev, struct device_attribute *dev_attr, const char *buf, size_t count)
{
	return	-EINVAL;
}

static struct device_attribute heci_dev_state_attr = {
	.attr = {
		.name = "heci_dev_state",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_heci_dev_props,
	.store = store_heci_dev_props
};

static struct device_attribute hbm_state_attr = {
	.attr = {
		.name = "hbm_state",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_heci_dev_props,
	.store = store_heci_dev_props
};

/**********************************/

typedef struct {
  struct work_struct my_work;
  struct heci_device *dev;
} my_work_t;

void workqueue_init_function(struct work_struct *work)
{
	struct heci_device *dev = ((my_work_t *)work)->dev;
	int err;

	ISH_DBG_PRINT(KERN_ALERT "[pci driver] %s() in workqueue func, continue initialization process\n", __func__);

	pci_set_drvdata(dev->pdev, dev);
/*	dev_dbg(&dev->pdev->dev, "heci: after pci_set_drvdata\n");*/

	device_create_file(&dev->pdev->dev, &heci_dev_state_attr);
	device_create_file(&dev->pdev->dev, &hbm_state_attr);

#if ISH_LOG

	device_create_file(&dev->pdev->dev, &read_attr);
	device_create_file(&dev->pdev->dev, &flush_attr);

	dev->log_head = dev->log_tail = 0;
	dev->print_log = ish_print_log;

	spin_lock_init(&dev->log_spinlock);

	dev->print_log(dev, "[heci-ish]: %s():+++ [Build "BUILD_ID "]\n", __func__);
	dev->print_log(dev, "[heci-ish] %s() running on %s revision [%02X]\n", __func__,
		dev->pdev->revision == REVISION_ID_CHT_A0 || (dev->pdev->revision & REVISION_ID_SI_MASK) == REVISION_ID_CHT_A0_SI ? "CHT A0" :
		dev->pdev->revision == REVISION_ID_CHT_B0 || (dev->pdev->revision & REVISION_ID_SI_MASK) == REVISION_ID_CHT_Bx_SI ? "CHT B0" : "Unknown", dev->pdev->revision);

#endif /*ISH_LOG*/

	mutex_lock(&heci_mutex);
	if (heci_start(dev)) {
		dev_err(&dev->pdev->dev, "heci: Init hw failure.\n");
		err = -ENODEV;
		goto out_err;
	}
/*	dev_dbg(&dev->pdev->dev, "heci: after heci_start\n");*/

	err = heci_register(dev);
	if (err)
		goto out_err;
/*	dev_dbg(&dev->pdev->dev, "heci: after heci_register\n");*/


	mutex_unlock(&heci_mutex);

	ISH_DBG_PRINT(KERN_ALERT "[pci driver] %s() in workqueue func, finished initialization process\n", __func__);
	kfree((void *)work);
	return;

out_err:
	mutex_unlock(&heci_mutex);
	kfree((void *)work);
}

/**********************************/

/**
 * heci_probe - Device Initialization Routine
 *
 * @pdev: PCI device structure
 * @ent: entry in ish_pci_tbl
 *
 * returns 0 on success, <0 on failure.
 */
static int ish_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct heci_device *dev;
	struct ish_hw *hw;
	int err;
	int	rv;
	struct workqueue_struct *workqueue_for_init;
	my_work_t *work;

	ISH_INFO_PRINT(KERN_ERR "[heci-ish]: %s():+++ [Build "BUILD_ID "]\n", __func__);
	ISH_INFO_PRINT(KERN_ERR "[heci-ish] %s() running on %s revision [%02X]\n", __func__,
		pdev->revision == REVISION_ID_CHT_A0 || (pdev->revision & REVISION_ID_SI_MASK) == REVISION_ID_CHT_A0_SI ? "CHT A0" :
		pdev->revision == REVISION_ID_CHT_B0 || (pdev->revision & REVISION_ID_SI_MASK) == REVISION_ID_CHT_Bx_SI ? "CHT B0" : "Unknown", pdev->revision);
#if defined (SUPPORT_Ax_ONLY)
	pdev->revision = REVISION_ID_CHT_A0;
	ISH_DBG_PRINT(KERN_ALERT "[heci-ish] %s() revision forced to A0\n", __func__);
#elif defined (SUPPORT_Bx_ONLY)
	pdev->revision = REVISION_ID_CHT_B0;
	ISH_DBG_PRINT(KERN_ALERT "[heci-ish] %s() revision forced to B0\n", __func__);
#endif
	mutex_lock(&heci_mutex);
	if (heci_pci_device) {
		err = -EEXIST;
		goto end;
	}
	/* enable pci dev */
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "heci: Failed to enable pci device.\n");
		goto end;
	}
	/* set PCI host mastering  */
	pci_set_master(pdev);
	/* pci request regions for heci driver */
	err = pci_request_regions(pdev, KBUILD_MODNAME);
	if (err) {
		dev_err(&pdev->dev, "heci: Failed to get pci regions.\n");
		goto disable_device;
	}

	/* allocates and initializes the heci dev structure */
	dev = ish_dev_init(pdev);
	if (!dev) {
		err = -ENOMEM;
		goto release_regions;
	}
	hw = to_ish_hw(dev);

	/* mapping  IO device memory */
	hw->mem_addr = pci_iomap(pdev, 0, 0);
	if (!hw->mem_addr) {
		dev_err(&pdev->dev, "mapping I/O device memory failure.\n");
		err = -ENOMEM;
		goto free_device;
	}

#if ISH_DEBUGGER
	ishdbg_misc_device.parent = &pdev->dev;
	rv = misc_register(&ishdbg_misc_device);
	if (rv)
		dev_err(&pdev->dev, "error starting ISH debugger (misc_register failed): %d\n", rv);
	hw_dbg = hw;
#endif

	heci_pci_device = pdev;

	/* request and enable interrupt   */
#ifndef TIMER_POLLING
	err = request_irq(pdev->irq, ish_irq_handler, IRQF_SHARED, KBUILD_MODNAME, dev);
	if (err) {
		dev_err(&pdev->dev, "heci: request_irq failure. irq = %d\n", pdev->irq);
		goto free_device;
	}
	printk(KERN_ALERT "[heci-ish]: uses IRQ %d\n", pdev->irq);

	/* Diagnostic output */
	do {
		uint32_t	msg_addr;
		uint32_t	msg_data;

		pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_ADDRESS_LO, &msg_addr);
		pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_DATA_32, &msg_data);
		ISH_DBG_PRINT(KERN_ALERT "[heci-ish] %s(): assigned IRQ = %d, [PCI_MSI_ADDRESS_LO] = %08X [PCI_MSI_DATA_32] = %08X\n", __func__, pdev->irq, msg_addr, msg_data);
	} while (0);
	/*********************/
#else
	/* Init & prepare workqueue */
	INIT_WORK(&ish_poll_work, ish_poll_work_fn);

	/* Create and schedule ISH polling timer */
	init_timer(&ish_poll_timer);
	ish_poll_timer.data = 0;
	ish_poll_timer.function = ish_poll_timer_fn;
	ish_poll_timer.expires = jiffies + 2;
	timer_data = dev;
	add_timer(&ish_poll_timer);

	/* Init ISH polling timers workqueue */
#endif

	/* PCI quirk: prevent from being put into D3 state */
	pdev->dev_flags |= PCI_DEV_FLAGS_NO_D3;


	/* 7/7/2014: in order to not stick Android boot, from here & below needs to run in work queue and here we should return success */
	/****************************************************************/
	work = (my_work_t *)kmalloc(sizeof(my_work_t), GFP_KERNEL);

	work->dev = dev;
	workqueue_for_init = create_workqueue("workqueue_for_init");
	INIT_WORK(&work->my_work, workqueue_init_function);
	queue_work(workqueue_for_init, &work->my_work);

	ISH_DBG_PRINT("[pci driver] %s() enqueue init_work function\n", __func__);

	mutex_unlock(&heci_mutex);
	return 0;
	/****************************************************************/

free_device:
	pci_iounmap(pdev, hw->mem_addr);
	kfree(dev);
release_regions:
	pci_release_regions(pdev);
disable_device:
	pci_disable_device(pdev);
end:
	mutex_unlock(&heci_mutex);
	dev_err(&pdev->dev, "heci: Driver initialization failed.\n");
	return err;
}

/**
 * heci_remove - Device Removal Routine
 *
 * @pdev: PCI device structure
 *
 * heci_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.
 */
static void ish_remove(struct pci_dev *pdev)
{
	struct heci_device *dev;
	struct ish_hw *hw;

	/*** If this case of removal is viable, also go through HECI clients removal ***/

	if (heci_pci_device != pdev) {
		dev_err(&pdev->dev, "heci: heci_pci_device != pdev\n");
		return;
	}

	dev = pci_get_drvdata(pdev);
	if (!dev) {
		dev_err(&pdev->dev, "heci: dev =NULL\n");
		return;
	}

	hw = to_ish_hw(dev);

	/* disable interrupts */
	ish_intr_disable(dev);
	free_irq(pdev->irq, dev);
	pci_disable_msi(pdev);
	pci_iounmap(pdev, hw->mem_addr);
	heci_pci_device = NULL;
	flush_scheduled_work();
	pci_set_drvdata(pdev, NULL);
	heci_deregister(dev);
	kfree(dev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

#define HECI_ISH_PM_OPS	NULL

/*
 *  PCI driver structure
 */
static struct pci_driver ish_driver = {
	.name = KBUILD_MODNAME,
	.id_table = ish_pci_tbl,
	.probe = ish_probe,
	.remove = ish_remove,
	.shutdown = ish_remove,
	.driver.pm = HECI_ISH_PM_OPS,
};

module_pci_driver(ish_driver);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Integrated Sensor Hub IPC");
MODULE_LICENSE("GPL v2");

