/*
 * Copyright (C) 2015 Google, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/tee_drv.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include "optee_private.h"
#include "optee_smc.h"
#include "mitee_memlog.h"
#define SET_KEY_FILE "/data/vendor/mitee/key.log"
static bool read_b_buf = false;

static int log_read_line(struct mitee_memlog_state *s, int put, int get)
{
	struct log_rb *log = s->log;
	int i;
	char c = '\0';
	size_t max_to_read = min((size_t)(put - get),
				 sizeof(s->line_buffer) - 1);
	size_t mask = log->sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = log->data[get++ & mask];
	s->line_buffer[i] = '\0';

	return i;
}

static int log_read_b_line(struct mitee_memlog_state *s, int put, int get)
{
	struct log_rb *log = s->log;
	int i;
	char c = '\0';
	size_t max_to_read = min((size_t)(put - get),
				 sizeof(s->line_buffer) - 1);
	size_t mask = log->b_sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = log->data[log->sz+(get++ & mask)];
	s->line_buffer[i] = '\0';
	return i;
}
#if 0
static void mitee_dump_logs(struct mitee_memlog_state *s)
{
	struct log_rb *log = s->log;
	uint32_t get, put, alloc;
	int read_chars;

	BUG_ON(!is_power_of_2(log->sz));

	/*
	 * For this ring buffer, at any given point, alloc >= put >= get.
	 * The producer side of the buffer is not locked, so the put and alloc
	 * pointers must be read in a defined order (put before alloc) so
	 * that the above condition is maintained. A read barrier is needed
	 * to make sure the hardware and compiler keep the reads ordered.
	 */
	get = s->get;
	while ((put = log->put) != get) {
		/* Make sure that the read of put occurs before the read of log data */
		rmb();

		/* Read a line from the log */
		read_chars = log_read_line(s, put, get);

		/* Force the loads from log_read_line to complete. */
		rmb();
		alloc = log->alloc;

		/*
		 * Discard the line that was just read if the data could
		 * have been corrupted by the producer.
		 */
		if (alloc - get > log->sz) {
			pr_err("mitee: log overflow.");
			get = alloc - log->sz;
			continue;
		}
		pr_info("mitee: %s", s->line_buffer);
		get += read_chars;
	}
	s->get = get;
}
#endif

static int mitee_memlog_call_notify(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct mitee_memlog_state *s;
	// unsigned long flags;

	if (action != MITEE_CALL_RETURNED)
		return NOTIFY_DONE;

	s = container_of(nb, struct mitee_memlog_state, call_notifier);
	// pr_err("ststest: start dump log\n");
	// spin_lock_irqsave(&s->lock, flags);
	// mitee_dump_logs(s);
	// spin_unlock_irqrestore(&s->lock, flags);
	atomic_inc(&s->mitee_log_event_count);
	wake_up_interruptible(&s->mitee_log_wq);
	// pr_err("ststest: end dump log\n");
	return NOTIFY_OK;
}

static int mitee_memlog_panic_notify(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct mitee_memlog_state *s;

	/*
	 * Don't grab the spin lock to hold up the panic notifier, even
	 * though this is racy.
	 */
	s = container_of(nb, struct mitee_memlog_state, panic_notifier);
	pr_info("mitee-log panic notifier\n");
	// mitee_dump_logs(s);
	atomic_inc(&s->mitee_log_event_count);
	wake_up_interruptible(&s->mitee_log_wq);
	return NOTIFY_OK;
}

static int is_buf_empty(struct mitee_memlog_state *s)
{
	struct log_rb *log = s->log;
	uint32_t get, put;

	get = s->get;
	put = log->put;
	return (get == put);
}

static int is_b_buf_empty(struct mitee_memlog_state *s)
{
	struct log_rb *log = s->log;
	uint32_t get, put;

	get = s->b_get;
	put = log->b_put;
	return (get == put);
}

static int do_mitee_memlog_read(struct mitee_memlog_state *s,
			  char __user *buf, size_t size)
{
	struct log_rb* log = s->log;
	uint32_t get, put, alloc;
	int read_chars = 0, copy_chars = 0, tbuf_size, outbuf_size;
	char* psrc = NULL;

	WARN_ON(!is_power_of_2(log->sz));

	if (!read_b_buf) {
		/*
		* For this ring buffer, at any given point, alloc >= put >= get.
		* The producer side of the buffer is not locked, so the put and alloc
		* pointers must be read in a defined order (put before alloc) so
		* that the above condition is maintained. A read barrier is needed
		* to make sure the hardware and compiler keep the reads ordered.
		*/
		get = s->b_get;
		put = log->b_put;
		/* make sure the hardware and compiler reads the correct put & alloc*/
		rmb();
		alloc = log->b_alloc;

		if (alloc - get > log->b_sz) {
			pr_notice("mitee: log overflow, lose some msg.");
			get = alloc - log->b_sz;
		}

		if (get > put)
			return -EFAULT;

		if (is_b_buf_empty(s)) {
			read_b_buf = true;
			return 0;
		}

		outbuf_size = min((int)(put - get), (int)size);

		tbuf_size = (outbuf_size / MITEE_LINE_BUFFER_SIZE + 1)
			* MITEE_LINE_BUFFER_SIZE;

		/* tbuf_size >= outbuf_size >= size */
		psrc = kzalloc(tbuf_size, GFP_KERNEL);

		if (!psrc)
			return -ENOMEM;

		while (get != put) {
			read_chars = log_read_b_line(s, put, get);
			/* Force the loads from log_read_line to complete. */
			rmb();
			if (copy_chars + read_chars > outbuf_size)
				break;
			memcpy(psrc + copy_chars, s->line_buffer, read_chars);
			get += read_chars;
			copy_chars += read_chars;
		}

		if (copy_to_user(buf, psrc, copy_chars)) {
			kfree(psrc);
			return -EFAULT;
		}
		kfree(psrc);

		s->b_get = get;

		return copy_chars;
	} else {
		/*
		* For this ring buffer, at any given point, alloc >= put >= get.
		* The producer side of the buffer is not locked, so the put and alloc
		* pointers must be read in a defined order (put before alloc) so
		* that the above condition is maintained. A read barrier is needed
		* to make sure the hardware and compiler keep the reads ordered.
		*/
		get = s->get;
		put = log->put;
		/* make sure the hardware and compiler reads the correct put & alloc*/
		rmb();
		alloc = log->alloc;

		if (alloc - get > log->sz) {
			pr_notice("mitee: log overflow, lose some msg.");
			get = alloc - log->sz;
		}

		if (get > put)
			return -EFAULT;

		if (is_buf_empty(s))
			return 0;

		outbuf_size = min((int)(put - get), (int)size);

		tbuf_size = (outbuf_size / MITEE_LINE_BUFFER_SIZE + 1)
			* MITEE_LINE_BUFFER_SIZE;

		/* tbuf_size >= outbuf_size >= size */
		psrc = kzalloc(tbuf_size, GFP_KERNEL);

		if (!psrc)
			return -ENOMEM;

		while (get != put) {
			read_chars = log_read_line(s, put, get);
			/* Force the loads from log_read_line to complete. */
			rmb();
			if (copy_chars + read_chars > outbuf_size)
				break;
			memcpy(psrc + copy_chars, s->line_buffer, read_chars);
			get += read_chars;
			copy_chars += read_chars;
		}

		if (copy_to_user(buf, psrc, copy_chars)) {
			kfree(psrc);
			return -EFAULT;
		}
		kfree(psrc);

		s->get = get;

		return copy_chars;
	}
}

static ssize_t mitee_memlog_read(struct file *file, char __user *buf, size_t size,
			   loff_t *ppos)
{
	struct mitee_memlog_state *s = PDE_DATA(file_inode(file));
	int ret = 0;

	if (atomic_xchg(&s->readable, 0)) {
		ret = do_mitee_memlog_read(s, buf, size);
		s->poll_event = atomic_read(&s->mitee_log_event_count);
		atomic_set(&s->readable, 1);
	}
	return ret;
}

static int write_key(char* key, size_t len)
{
	struct file* fp;
	mm_segment_t fs;
	loff_t pos;

	fp = filp_open(SET_KEY_FILE, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk(KERN_ERR "write key file error,return!\n");
		return -1;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	pos = 0;
	vfs_write(fp, key, len, &pos);
	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(fs);
	return 0;
}

static int mitee_memlog_open(struct inode *inode, struct file *file)
{
	struct mitee_memlog_state *s = PDE_DATA(inode);
	int ret;

	ret = nonseekable_open(inode, file);
	if (unlikely(ret))
		return ret;
	s->poll_event = atomic_read(&s->mitee_log_event_count);
	write_key((char*)s->log->aeskey, KEY_LENGTH);
	return 0;
}

static int mitee_memlog_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned int mitee_memlog_poll(struct file *file, poll_table *wait)
{
	struct mitee_memlog_state *s = PDE_DATA(file_inode(file));
	int mask = 0;

	if (!is_buf_empty(s))
		return POLLIN | POLLRDNORM;

	poll_wait(file, &s->mitee_log_wq, wait);

	if (s->poll_event != atomic_read(&s->mitee_log_event_count))
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static const struct file_operations mitee_memlog_fops = {
	.owner = THIS_MODULE,
	.open = mitee_memlog_open,
	.read = mitee_memlog_read,
	.release = mitee_memlog_release,
	.poll = mitee_memlog_poll,
};

int mitee_memlog_probe(struct platform_device *pdev)
{
	struct mitee_memlog_state *s;
	int result;
	phys_addr_t pa;
	phys_addr_t begin;
	phys_addr_t end;
	size_t size;
	void *va;
	struct optee *optee;

	union {
		struct arm_smccc_res smccc;
		struct optee_smc_get_shm_config_result result;
	} res;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		result = -ENOMEM;
		goto error_alloc_state;
	}

	spin_lock_init(&s->lock);
	s->dev = &pdev->dev;
	s->mitee_dev = s->dev->parent;
	s->get = 0;
	s->b_get = 0;
	read_b_buf = false;

	optee = get_optee_drv_state();
	optee->invoke_fn(OPTEE_SMC_GET_MITEE_LOG_BUFFER, 0, 0, 0, 0, 0, 0, 0, &res.smccc);
	if (res.result.status != OPTEE_SMC_RETURN_OK) {
		pr_err("miteelog service not available\n");
		result = -EIO;
		goto error_alloc_state;
	}
	// pr_err("miteelog start=%x, size=%zu, setting=%x\n", (uint64_t)res.result.start, res.result.size,res.result.settings);
	begin = roundup(res.result.start, PAGE_SIZE);
	end = rounddown(res.result.start + res.result.size, PAGE_SIZE);
	pa = begin;
	size = end - begin;
	// pr_err("miteelog paddr=%x, size=%zu\n", (uint64_t)pa, size);
	va = memremap(pa, size, MEMREMAP_WB);
	if (!va) {
		pr_err("miteelog ioremap failed\n");
		result = -EFAULT;
		goto error_alloc_state;
	}
	s->log = va;

	s->call_notifier.notifier_call = mitee_memlog_call_notify;
	result = mitee_call_notifier_register(&s->call_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register mitee call notifier\n");
		goto error_call_notifier;
	}

	s->panic_notifier.notifier_call = mitee_memlog_panic_notify;
	result = atomic_notifier_chain_register(&panic_notifier_list,
						&s->panic_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register mitee panic notifier\n");
		goto error_panic_notifier;
	}
	init_waitqueue_head(&s->mitee_log_wq);
	atomic_set(&s->mitee_log_event_count, 0);
	atomic_set(&s->readable, 1);
	platform_set_drvdata(pdev, s);

	/* create /proc/mitee_log */
	s->proc = proc_create_data("mitee_log", 0444, NULL, &mitee_memlog_fops,
				     s);
	if (!s->proc) {
		pr_info("mitee_log proc_create failed!\n");
		return -ENOMEM;
	}

	return 0;

error_panic_notifier:
	mitee_call_notifier_unregister(&s->call_notifier);
error_call_notifier:
//TODO notify tee we failed register notifier
	// trusty_std_call32(s->trusty_dev, SMC_SC_SHARED_LOG_RM,
	// (u32)pa, (u32)((u64)pa >> 32), 0);
// error_std_call:
	// __free_pages(s->log_pages, get_order(TRUSTY_LOG_SIZE));
// error_alloc_log:
// 	kfree(s);
error_alloc_state:
	return result;
}

int mitee_memlog_remove(struct platform_device *pdev)
{
	// int result;
	struct mitee_memlog_state *s = platform_get_drvdata(pdev);
	// phys_addr_t pa = page_to_phys(s->log_pages);

	dev_dbg(&pdev->dev, "%s\n", __func__);
	proc_remove(s->proc);
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &s->panic_notifier);
	mitee_call_notifier_unregister(&s->call_notifier);
//TODO notify tee we failed register notifier
	// result = trusty_std_call32(s->trusty_dev, SMC_SC_SHARED_LOG_RM,
	// (u32)pa, (u32)((u64)pa >> 32), 0);
	// if (result) {
	// 	pr_err("trusty std call (SMC_SC_SHARED_LOG_RM) failed: %d\n", result);
	// }
	// __free_pages(s->log_pages, get_order(TRUSTY_LOG_SIZE));
	kfree(s);

	return 0;
}
