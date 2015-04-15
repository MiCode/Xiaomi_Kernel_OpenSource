/*
 * PCI glue for HECI provider device (ISS) driver
 *
 * Copyright (c) 2014-2015, Intel Corporation.
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
#include "client.h"
#include "heci_dev.h"
#include "hw-ish.h"
#include "hbm.h"
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

static const struct pci_device_id ish_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x22D8)},
	{0, }
};

MODULE_DEVICE_TABLE(pci, ish_pci_tbl);

static DEFINE_MUTEX(heci_mutex);
struct workqueue_struct *workqueue_for_init;

/*global variables for suspend*/
int	suspend_flag = 0;
wait_queue_head_t	suspend_wait;

#ifdef TIMER_POLLING
/*
 * DD -- ISS timer-polling workaround for H-FPGA
 * (and other platforms that fail to deliver interrupts)
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
	/*ISH_DBG_PRINT(KERN_ALERT "%s(): ish_irq_handler() returned %08X\n",
		__func__, rv);*/

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

static ssize_t ishdbg_read(struct file *file, char __user *ubuf, size_t length,
	loff_t *offset)
{
	int rv;
	int copy_len;

	if (resp_buf_read)
		return	0;	/* EOF */
	copy_len = (length > strlen(dbg_resp_buf)) ?
		strlen(dbg_resp_buf) : length;
	rv = copy_to_user(ubuf, dbg_resp_buf, copy_len);
	if (rv)
		return  -EINVAL;
	resp_buf_read = 1;
	return  copy_len;
}

static ssize_t ishdbg_write(struct file *file, const char __user *ubuf,
	size_t length, loff_t *offset)
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
			printk(KERN_ERR "[ish-dbg] sscanf failed, sscanf_match = %d\n",
				sscanf_match);
			return  -EINVAL;
		}
		if (addr < 0 /*|| addr > MAX_RANGE*/ ||
				count < 0 /*|| count > MAX_RANGE*/)
			return -EINVAL;
		if (addr % 4) {
			printk(KERN_ERR "[ish-dbg] address isn't aligned to 4 bytes\n");
			return -EINVAL;
		}
		cur_index = 0;
		for (i = 0; i < count; i++) {
			reg_data = (volatile uint32_t *)
				((char *)hw_dbg->mem_addr + addr + i*4);
			cur_index += sprintf(dbg_resp_buf + cur_index, "%08X ",
				*reg_data);
		}
		cur_index += sprintf(dbg_resp_buf + cur_index, "\n");
		resp_buf_read = 0;
	} else if (!strcmp(cmd, "e")) {
		/* Enter values e <addr> <value> */
		if (sscanf_match != 2) {
			printk(KERN_ERR "[ish-dbg] sscanf failed, sscanfMatch = %d\n",
				sscanf_match);
			return  -EINVAL;
		}
		if (addr % 4) {
			printk(KERN_ERR "[ish-dbg] address isn't aligned to 4 bytes\n");
			return -EINVAL;
		}
		reg_data = (volatile uint32_t *)((char *)hw_dbg->mem_addr
			+ addr);
		*reg_data = count;
		sprintf(dbg_resp_buf, "OK\n");
		resp_buf_read = 0;
	}

	return  length;
}

static long ishdbg_ioctl(struct file *file, unsigned int cmd,
	unsigned long data)
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
	/* set log_tail to point at the last char to be deleted */
	dev->log_tail = (dev->log_tail + min_chars - 1) % PRINT_BUFFER_SIZE;
	for (i = dev->log_tail; dev->log_buffer[i] != '\n';
			i = (i+1) % PRINT_BUFFER_SIZE)
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
	struct timeval tv1, tv2, tv_diff;

	do_gettimeofday(&tv1);
	/* Fix for power-off path */
	if (!heci_pci_device)
		return;

	do_gettimeofday(&tv);
	i = sprintf(tmp_buf, "[%ld.%06ld] ", tv.tv_sec, tv.tv_usec);

	va_start(args, format);
	length = vsnprintf(tmp_buf + i, sizeof(tmp_buf)-i, format, args);
	va_end(args);

	length = length + i;
	/* if the msg does not end with \n, add it */
	if (tmp_buf[length-1] != '\n') {
		tmp_buf[length] = '\n';
		length++;
	}

	spin_lock_irqsave(&dev->log_spinlock, flags);

	full_space = dev->log_head - dev->log_tail;
	if (full_space < 0)
		full_space = PRINT_BUFFER_SIZE + full_space;
	free_space = PRINT_BUFFER_SIZE - full_space;

	if (free_space <= length)
		/*
		 * not enougth space.
		 * needed at least 1 empty char to recognize
		 * whether buffer is full or empty
		 */
		delete_from_log(dev, (length - free_space) + 1);

	if (dev->log_head + length <= PRINT_BUFFER_SIZE) {
		memcpy(dev->log_buffer + dev->log_head, tmp_buf, length);
	} else {
		memcpy(dev->log_buffer + dev->log_head, tmp_buf,
			PRINT_BUFFER_SIZE - dev->log_head);
		memcpy(dev->log_buffer,
			tmp_buf + PRINT_BUFFER_SIZE - dev->log_head,
			length - (PRINT_BUFFER_SIZE - dev->log_head));
	}
	dev->log_head = (dev->log_head + length) % PRINT_BUFFER_SIZE;

	spin_unlock_irqrestore(&dev->log_spinlock, flags);

	do_gettimeofday(&tv2);
	tv_diff.tv_sec = tv2.tv_sec - tv1.tv_sec;
	tv_diff.tv_usec = tv2.tv_usec - tv1.tv_usec;
	if (tv1.tv_usec > tv2.tv_usec) {
		tv_diff.tv_usec += 1000000UL;
		--tv_diff.tv_sec;
	}
	if (tv_diff.tv_sec > dev->max_log_sec ||
			tv_diff.tv_sec == dev->max_log_sec &&
			tv_diff.tv_usec > dev->max_log_usec) {
		dev->max_log_sec = tv_diff.tv_sec;
		dev->max_log_usec = tv_diff.tv_usec;
	}
}


void	g_ish_print_log(char *fmt, ...)
{
	char tmp_buf[1024];
	va_list args;
	struct heci_device	*dev;

	/* Fix for power-off path */
	if (!heci_pci_device)
		return;

	dev = pci_get_drvdata(heci_pci_device);
	va_start(args, fmt);
	vsnprintf(tmp_buf, sizeof(tmp_buf), fmt, args);
	va_end(args);
	ish_print_log(dev, tmp_buf);
}
EXPORT_SYMBOL(g_ish_print_log);


static ssize_t ish_read_log(struct heci_device *dev, char *buf, size_t size)
{
	int i, full_space, ret_val;

	if (dev->log_head == dev->log_tail)/* log is empty */
		return 0;

	/* read size the minimum between full_space and the buffer size */
	full_space = dev->log_head - dev->log_tail;
	if (full_space < 0)
		full_space = PRINT_BUFFER_SIZE + full_space;

	if (full_space < size)
		i = (dev->log_tail + full_space) % PRINT_BUFFER_SIZE;
		/* log has less than 'size' bytes, i = dev->log_head */
	else
		i = (dev->log_tail + size) % PRINT_BUFFER_SIZE;
	/* i is the last character to be readen */
	i = (i-1) % PRINT_BUFFER_SIZE;

	/* read from tail to last '\n' before i */
	for (; dev->log_buffer[i] != '\n'; i = (i-1) % PRINT_BUFFER_SIZE)
		;

	if (dev->log_tail < i) {
		memcpy(buf, dev->log_buffer + dev->log_tail,
			i - dev->log_tail + 1);
		ret_val = i - dev->log_tail + 1;
	} else {
		memcpy(buf, dev->log_buffer + dev->log_tail,
			PRINT_BUFFER_SIZE - dev->log_tail);
		memcpy(buf + PRINT_BUFFER_SIZE - dev->log_tail,
			dev->log_buffer, i + 1);
		ret_val = PRINT_BUFFER_SIZE - dev->log_tail + i + 1;
	}
	return ret_val;
}

static ssize_t ish_read_flush_log(struct heci_device *dev, char *buf,
	size_t size)
{
	int ret;

	ret = ish_read_log(dev, buf, size);
	delete_from_log(dev, ret);
	return ret;
}

/* show & store functions for both read and flush char devices*/
ssize_t show_read(struct device *dev, struct device_attribute *dev_attr,
	char *buf)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;
	ssize_t retval;
	unsigned long	flags;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);
	spin_lock_irqsave(&heci_dev->log_spinlock, flags);
	retval = ish_read_log(heci_dev, buf, PAGE_SIZE);
	spin_unlock_irqrestore(&heci_dev->log_spinlock, flags);

	return retval;
}

ssize_t store_read(struct device *dev, struct device_attribute *dev_attr,
	const char *buf, size_t count)
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

ssize_t show_flush(struct device *dev, struct device_attribute *dev_attr,
	char *buf)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;
	unsigned long	flags;
	ssize_t retval;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);
	spin_lock_irqsave(&heci_dev->log_spinlock, flags);
	retval = ish_read_flush_log(heci_dev, buf, PAGE_SIZE);
	spin_unlock_irqrestore(&heci_dev->log_spinlock, flags);

	return retval;
}

ssize_t store_flush(struct device *dev, struct device_attribute *dev_attr,
	const char *buf, size_t count)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;
	unsigned long   flags;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);

	if (!strncmp(buf, "empty", 5)) {
		spin_lock_irqsave(&heci_dev->log_spinlock, flags);
		heci_dev->log_tail = heci_dev->log_head;
		spin_unlock_irqrestore(&heci_dev->log_spinlock, flags);
	}
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
#else

static void ish_print_log_nolog(struct heci_device *dev, char *format, ...)
{
}

void	g_ish_print_log(char *fmt, ...)
{
}
EXPORT_SYMBOL(g_ish_print_log);

#endif /* ISH_LOG */

ssize_t show_heci_dev_props(struct device *dev,
	struct device_attribute *dev_attr, char *buf)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;
	ssize_t	ret = -ENOENT;
	unsigned	count;
	unsigned long   flags, flags2, tx_flags, tx_free_flags;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);

	if (!strcmp(dev_attr->attr.name, "heci_dev_state")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)heci_dev->dev_state);
		ret = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name, "hbm_state")) {
		scnprintf(buf, PAGE_SIZE, "%u\n",
			(unsigned)heci_dev->hbm_state);
		ret = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name, "fw_status")) {
		scnprintf(buf, PAGE_SIZE, "%08X\n",
			heci_dev->ops->get_fw_status(heci_dev));
		ret = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name, "ipc_buf")) {
		struct wr_msg_ctl_info *ipc_link, *ipc_link_next;

		count = 0;
		spin_lock_irqsave(&heci_dev->wr_processing_spinlock, flags);
		list_for_each_entry_safe(ipc_link, ipc_link_next,
			&heci_dev->wr_processing_list_head.link, link)
			++count;
		spin_unlock_irqrestore(&heci_dev->wr_processing_spinlock,
			flags);
		scnprintf(buf, PAGE_SIZE, "outstanding %u messages\n", count);
		ret = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name, "host_clients")) {
		struct heci_cl *cl, *next;
		static const char * const cl_states[] = {"initializing",
			"connecting", "connected", "disconnecting",
			"disconnected"};
		struct heci_cl_rb	*rb, *next_rb;
		struct heci_cl_tx_ring	*tx_rb, *next_tx_rb;

		scnprintf(buf, PAGE_SIZE, "Host clients:\n"
				"------------\n");
		spin_lock_irqsave(&heci_dev->device_lock, flags);
		list_for_each_entry_safe(cl, next, &heci_dev->cl_list, link) {
			sprintf(buf + strlen(buf), "id: %d\n",
				cl->host_client_id);
			scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"state: %s\n", cl->state < 0 || cl->state >
					HECI_CL_DISCONNECTED ?
					"unknown" : cl_states[cl->state]);
			if (cl->state == HECI_CL_CONNECTED) {
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"FW client id: %d\n", cl->me_client_id);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"RX ring size: %u\n", cl->rx_ring_size);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"TX ring size: %u\n", cl->tx_ring_size);

				count = 0;
				spin_lock_irqsave(&cl->in_process_spinlock,
					flags2);
				list_for_each_entry_safe(rb, next_rb,
						&cl->in_process_list.list, list)
					++count;
				spin_unlock_irqrestore(&cl->in_process_spinlock,
					flags2);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"RX in work: %u\n", count);

				count = 0;
				spin_lock_irqsave(&cl->in_process_spinlock,
					flags2);
				list_for_each_entry_safe(rb, next_rb,
						&cl->free_rb_list.list, list)
					++count;
				spin_unlock_irqrestore(&cl->in_process_spinlock,
					flags2);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"RX free: %u\n", count);

				count = 0;

				spin_lock_irqsave(&cl->tx_list_spinlock,
					tx_flags);
				list_for_each_entry_safe(tx_rb, next_tx_rb,
						&cl->tx_list.list, list)
					++count;
				spin_unlock_irqrestore(&cl->tx_list_spinlock,
					tx_flags);

				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"TX pending: %u\n", count);
				count = 0;
				spin_lock_irqsave(
					&cl->tx_free_list_spinlock,
					tx_free_flags);
				list_for_each_entry_safe(tx_rb, next_tx_rb,
						&cl->tx_free_list.list, list)
					++count;
				spin_unlock_irqrestore(
					&cl->tx_free_list_spinlock,
					tx_free_flags);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"TX free: %u\n", count);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"FC: %u\n",
					(unsigned)cl->heci_flow_ctrl_creds);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					 "out FC: %u\n",
					(unsigned)cl->out_flow_ctrl_creds);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"Err snd msg: %u\n",
					(unsigned)cl->err_send_msg);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"Err snd FC: %u\n",
					(unsigned)cl->err_send_fc);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"Tx count: %u\n",
					(unsigned)cl->send_msg_cnt);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"Rx count: %u\n",
					(unsigned)cl->recv_msg_cnt);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"FC count: %u\n",
					(unsigned)cl->heci_flow_ctrl_cnt);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"out FC cnt: %u\n",
					(unsigned)cl->out_flow_ctrl_cnt);
				scnprintf(buf + strlen(buf),
					PAGE_SIZE - strlen(buf),
					"Max FC delay: %lu.%06lu\n",
					cl->max_fc_delay_sec,
					cl->max_fc_delay_usec);
			}
		}
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC HID out FC: %u\n",
			(unsigned)heci_dev->ipc_hid_out_fc);
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC HID out FC count: %u\n",
			(unsigned)heci_dev->ipc_hid_out_fc_cnt);
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC HID in msg: %u\n",
			(unsigned)heci_dev->ipc_hid_in_msg);
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC HID in FC: %u\n",
			(unsigned)heci_dev->ipc_hid_in_fc);
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC HID in FC count: %u\n",
			(unsigned)heci_dev->ipc_hid_in_fc_cnt);
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC HID out msg: %u\n",
			(unsigned)heci_dev->ipc_hid_out_msg);
		spin_unlock_irqrestore(&heci_dev->device_lock, flags);
		ret = strlen(buf);
	} else if (!strcmp(dev_attr->attr.name, "stats")) {
		scnprintf(buf, PAGE_SIZE, "Max. log time: %lu.%06lu\n",
			heci_dev->max_log_sec, heci_dev->max_log_usec);
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC Rx frames: %u; bytes: %llu\n",
			heci_dev->ipc_rx_cnt, heci_dev->ipc_rx_bytes_cnt);
		scnprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
			"IPC Tx frames: %u; bytes: %llu\n",
			heci_dev->ipc_tx_cnt, heci_dev->ipc_tx_bytes_cnt);
		ret = strlen(buf);
	}

	return	ret;
}

ssize_t store_heci_dev_props(struct device *dev,
	struct device_attribute *dev_attr, const char *buf, size_t count)
{
	return	-EINVAL;
}

/* Debug interface to force flow-control to HID client */
static unsigned	num_force_hid_fc;

ssize_t show_force_hid_fc(struct device *dev, struct device_attribute *dev_attr,
	char *buf)
{
	sprintf(buf, "%u\n", num_force_hid_fc);
	return	 strlen(buf);
}

ssize_t store_force_hid_fc(struct device *dev,
	struct device_attribute *dev_attr, const char *buf, size_t count)
{
	struct pci_dev *pdev;
	struct heci_device *heci_dev;
	struct heci_cl *cl, *next;
	unsigned long	tx_flags;

	pdev = container_of(dev, struct pci_dev, dev);
	heci_dev = pci_get_drvdata(pdev);

	list_for_each_entry_safe(cl, next, &heci_dev->cl_list, link) {
		if (cl->host_client_id == 3 && cl->me_client_id == 5) {
			dev_warn(dev, "HID FC %u, forced to 1\n",
				(unsigned)cl->heci_flow_ctrl_creds);
			cl->heci_flow_ctrl_creds = 1;
			++num_force_hid_fc;
			spin_lock_irqsave(&cl->tx_list_spinlock, tx_flags);
			if (!list_empty(&cl->tx_list.list)) {
				/* start sending the first msg
				 = the callback function */
				spin_unlock_irqrestore(&cl->tx_list_spinlock,
					tx_flags);
				heci_cl_send_msg(heci_dev, cl);
			} else {
				spin_unlock_irqrestore(&cl->tx_list_spinlock,
					tx_flags);
			}
			break;
		}
	}

	return	 strlen(buf);
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

static struct device_attribute fw_status_attr = {
	.attr = {
		.name = "fw_status",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_heci_dev_props,
	.store = store_heci_dev_props
};

static struct device_attribute host_clients_attr = {
	.attr = {
		.name = "host_clients",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_heci_dev_props,
	.store = store_heci_dev_props
};

static struct device_attribute ipc_buf_attr = {
	.attr = {
		.name = "ipc_buf",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_heci_dev_props,
	.store = store_heci_dev_props
};

static struct device_attribute stats_attr = {
	.attr = {
		.name = "stats",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_heci_dev_props,
	.store = store_heci_dev_props
};

static struct device_attribute force_hid_fc_attr = {
	.attr = {
		.name = "force_hid_fc",
		.mode = (S_IWUSR | S_IRUGO)
	},
	.show = show_force_hid_fc,
	.store = store_force_hid_fc
};
/**********************************/

struct my_work_t {
	struct work_struct my_work;
	struct heci_device *dev;
};

void workqueue_init_function(struct work_struct *work)
{
	struct heci_device *dev = ((struct my_work_t *)work)->dev;
	int err;

	ISH_DBG_PRINT(KERN_ALERT
		"[pci driver] %s() in workqueue func, continue initialization process\n",
		__func__);

	pci_set_drvdata(dev->pdev, dev);
/*	dev_dbg(&dev->pdev->dev, "heci: after pci_set_drvdata\n");*/

	device_create_file(&dev->pdev->dev, &heci_dev_state_attr);
	device_create_file(&dev->pdev->dev, &hbm_state_attr);
	device_create_file(&dev->pdev->dev, &fw_status_attr);
	device_create_file(&dev->pdev->dev, &host_clients_attr);
	device_create_file(&dev->pdev->dev, &ipc_buf_attr);
	device_create_file(&dev->pdev->dev, &stats_attr);
	device_create_file(&dev->pdev->dev, &force_hid_fc_attr);

#if ISH_LOG

	device_create_file(&dev->pdev->dev, &read_attr);
	device_create_file(&dev->pdev->dev, &flush_attr);

	dev->log_head = dev->log_tail = 0;
	dev->print_log = ish_print_log;

	spin_lock_init(&dev->log_spinlock);

	dev->print_log(dev, "[heci-ish]: %s():+++ [Build "BUILD_ID "]\n",
		__func__);
	dev->print_log(dev, "[heci-ish] %s() running on %s revision [%02X]\n",
		__func__,
		dev->pdev->revision == REVISION_ID_CHT_A0 ||
		(dev->pdev->revision & REVISION_ID_SI_MASK) ==
		REVISION_ID_CHT_A0_SI ? "CHT Ax" :
		dev->pdev->revision == REVISION_ID_CHT_B0 ||
		(dev->pdev->revision & REVISION_ID_SI_MASK) ==
		REVISION_ID_CHT_Bx_SI ? "CHT Bx" :
		(dev->pdev->revision & REVISION_ID_SI_MASK) ==
		REVISION_ID_CHT_Kx_SI ? "CHT Kx/Cx" : "Unknown",
		dev->pdev->revision);
#else
	dev->print_log = ish_print_log_nolog;
#endif /*ISH_LOG*/

	init_waitqueue_head(&suspend_wait);

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

	ISH_DBG_PRINT(KERN_ALERT
		"[pci driver] %s() in workqueue func, finished initialization process\n",
		__func__);
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
	struct my_work_t *work;

	ISH_INFO_PRINT(KERN_ERR "[heci-ish]: %s():+++ [Build "BUILD_ID "]\n",
		__func__);
	ISH_INFO_PRINT(KERN_ERR
		"[heci-ish] %s() running on %s revision [%02X]\n", __func__,
		pdev->revision == REVISION_ID_CHT_A0 ||
		(pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_A0_SI ? "CHT A0" :
		pdev->revision == REVISION_ID_CHT_B0 ||
		(pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Bx_SI ? "CHT B0" :
		(pdev->revision & REVISION_ID_SI_MASK) ==
			REVISION_ID_CHT_Kx_SI ? "CHT Kx/Cx" : "Unknown",
		pdev->revision);
#if defined(SUPPORT_Ax_ONLY)
	pdev->revision = REVISION_ID_CHT_A0;
	ISH_DBG_PRINT(KERN_ALERT "[heci-ish] %s() revision forced to A0\n",
		__func__);
#elif defined(SUPPORT_Bx_ONLY)
	pdev->revision = REVISION_ID_CHT_B0;
	ISH_DBG_PRINT(KERN_ALERT "[heci-ish] %s() revision forced to B0\n",
		__func__);
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
		dev_err(&pdev->dev,
			"error starting ISS debugger (misc_register): %d\n",
			rv);
	hw_dbg = hw;
#endif /*ISH_DEBUGGER*/

	heci_pci_device = pdev;

	/* request and enable interrupt   */
#ifndef TIMER_POLLING
	err = request_irq(pdev->irq, ish_irq_handler, IRQF_SHARED,
		KBUILD_MODNAME, dev);
	if (err) {
		dev_err(&pdev->dev, "heci: request_irq failure. irq = %d\n",
			pdev->irq);
		goto free_device;
	}
	dev_alert(&pdev->dev, "[heci-ish]: uses IRQ %d\n", pdev->irq);

	/* Diagnostic output */
	do {
		uint32_t	msg_addr;
		uint32_t	msg_data;

		pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_ADDRESS_LO,
			&msg_addr);
		pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_DATA_32,
			&msg_data);
		ISH_DBG_PRINT(KERN_ALERT
			"[heci-ish] %s(): assigned IRQ = %d, [PCI_MSI_ADDRESS_LO] = %08X [PCI_MSI_DATA_32] = %08X\n",
			__func__, pdev->irq, msg_addr, msg_data);
	} while (0);
	/*********************/
#else
	/* Init & prepare workqueue */
	INIT_WORK(&ish_poll_work, ish_poll_work_fn);

	/* Create and schedule ISS polling timer */
	init_timer(&ish_poll_timer);
	ish_poll_timer.data = 0;
	ish_poll_timer.function = ish_poll_timer_fn;
	ish_poll_timer.expires = jiffies + 2;
	timer_data = dev;
	add_timer(&ish_poll_timer);

	/* Init ISS polling timers workqueue */
#endif

	/* PCI quirk: prevent from being put into D3 state */
	pdev->dev_flags |= PCI_DEV_FLAGS_NO_D3;


	/*
	 * 7/7/2014: in order to not stick Android boot,
	 * from here & below needs to run in work queue
	 * and here we should return success
	 */
	/****************************************************************/
	work = kmalloc(sizeof(struct my_work_t), GFP_KERNEL);
	if (!work)
		return -ENOMEM;
	work->dev = dev;
	workqueue_for_init = create_workqueue("workqueue_for_init");
	if (!workqueue_for_init)
		return -ENOMEM;
	INIT_WORK(&work->my_work, workqueue_init_function);
	queue_work(workqueue_for_init, &work->my_work);

	ISH_DBG_PRINT("[pci driver] %s() enqueue init_work function\n",
		__func__);

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

	/*
	 *** If this case of removal is viable,
	 * also go through HECI clients removal ***
	 */

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
	if (workqueue_for_init) {
		flush_workqueue(workqueue_for_init);
		destroy_workqueue(workqueue_for_init);
		workqueue_for_init = NULL;
	}
	pci_set_drvdata(pdev, NULL);
	heci_deregister(dev);
	kfree(dev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

int ish_suspend(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct heci_device *dev = pci_get_drvdata(pdev);

	/* If previous suspend hasn't been asnwered then ISH is likely dead,
	don't attempt nested notification */
	if (suspend_flag)
		return	0;

	suspend_flag = 1;
	send_suspend(dev);

	/* 250 ms should be likely enough for live ISH to flush all IPC buf */
	if (suspend_flag)
		wait_event_timeout(suspend_wait, !suspend_flag, HZ / 4);
	return 0;
}

int ish_resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct heci_device *dev = pci_get_drvdata(pdev);
	send_resume(dev);
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops ish_pm_ops = {
	.suspend = ish_suspend,
	.resume = ish_resume,
};

#define HECI_ISH_PM_OPS	(&ish_pm_ops)
#else
#define HECI_ISH_PM_OPS	NULL
#endif /* CONFIG_PM */


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
MODULE_DESCRIPTION("Intel(R) Integrated Sensor Hub PCI Device Driver");
MODULE_LICENSE("GPL v2");

