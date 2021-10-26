/*
 * Copyright (C) 2015 Google, Inc.
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
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/log2.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/page.h>
#include "gz-log.h"

#define TRUSTY_LOG_SIZE (PAGE_SIZE * 32)
#define TRUSTY_LINE_BUFFER_SIZE 256

static wait_queue_head_t gz_log_wq;
static atomic_t gz_log_event_count;
static int poll_event;
static struct page *trusty_log_pages;
static struct trusty_log_state *tls;

struct trusty_log_state {
	struct device *dev;
	struct device *trusty_dev;

	/*
	 * This lock is here to ensure only one consumer will read
	 * from the log ring buffer at a time.
	 */
	spinlock_t lock;
	struct log_rb *log;
	uint32_t get;

	struct page *log_pages;

	struct notifier_block call_notifier;
	struct notifier_block panic_notifier;
	char line_buffer[TRUSTY_LINE_BUFFER_SIZE];
};

static int log_read_line(struct trusty_log_state *s, int put, int get)
{
	struct log_rb *log = s->log;
	int i;
	char c = '\0';
	size_t max_to_read =
		min((size_t)(put - get), sizeof(s->line_buffer) - 1);
	size_t mask = log->sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = log->data[get++ & mask];
	s->line_buffer[i] = '\0';

	return i;
}

static int trusty_log_call_notify(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct trusty_log_state *s;

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	s = container_of(nb, struct trusty_log_state, call_notifier);
	atomic_inc(&gz_log_event_count);
	wake_up_interruptible(&gz_log_wq);
	return NOTIFY_OK;
}

static int trusty_log_panic_notify(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct trusty_log_state *s;

	/*
	 * Don't grab the spin lock to hold up the panic notifier, even
	 * though this is racy.
	 */
	s = container_of(nb, struct trusty_log_state, panic_notifier);
	pr_info("trusty-log panic notifier - trusty version %s",
		trusty_version_str_get(s->trusty_dev));
	atomic_inc(&gz_log_event_count);
	wake_up_interruptible(&gz_log_wq);
	return NOTIFY_OK;
}

static bool trusty_supports_logging(struct device *device)
{
	int result;

	result = trusty_std_call32(device, SMC_SC_SHARED_LOG_VERSION,
				   TRUSTY_LOG_API_VERSION, 0, 0);
	if (result == SM_ERR_UNDEFINED_SMC) {
		pr_info("trusty-log not supported on secure side.\n");
		return false;
	} else if (result < 0) {
		pr_info("trusty std call (SHARED_LOG_VERSION) failed: %d\n",
			result);
		return false;
	}

	if (result == TRUSTY_LOG_API_VERSION)
		return true;

	pr_info("trusty-log unsupported api version: %d, supported: %d\n",
		result, TRUSTY_LOG_API_VERSION);
	return false;
}

static int do_gz_log_read(struct file *file, char __user *buf, size_t size)
{
	struct log_rb *log = tls->log;
	uint32_t get, put, alloc;
	int read_chars = 0, copy_chars = 0, tbuf_size = 0, offset = 0;
	char *psrc = NULL;

	WARN_ON(!is_power_of_2(log->sz));

	/*
	 * For this ring buffer, at any given point, alloc >= put >= get.
	 * The producer side of the buffer is not locked, so the put and alloc
	 * pointers must be read in a defined order (put before alloc) so
	 * that the above condition is maintained. A read barrier is needed
	 * to make sure the hardware and compiler keep the reads ordered.
	 */
	get = tls->get;
	put = log->put;
	/* make sure the hardware and compiler keep the reads ordered */
	rmb();
	alloc = log->alloc;
	if (alloc - tls->get > log->sz) {
		pr_notice("trusty: log overflow.");
		get = alloc - log->sz;
	}

	if (get > put)
		return -EFAULT;

	if (get == put)
		return 0;

	tbuf_size = ((put - get) / TRUSTY_LINE_BUFFER_SIZE + 1)
		    * TRUSTY_LINE_BUFFER_SIZE;

	psrc = kzalloc(tbuf_size, GFP_KERNEL);

	if (!psrc)
		return -ENOMEM;

	while (get != put) {
		read_chars = log_read_line(tls, put, get);
		memcpy(psrc + offset, tls->line_buffer, read_chars);
		get += read_chars;
		offset += read_chars;
	}

	copy_chars = size < offset ? size : offset;
	if (copy_to_user(buf, psrc, copy_chars))
		return -EFAULT;
	kfree(psrc);

	tls->get += copy_chars;

	return copy_chars;
}

static int gz_log_open(struct inode *inode, struct file *file)
{
	int ret;

	ret = nonseekable_open(inode, file);
	if (unlikely(ret))
		return ret;
	poll_event = atomic_read(&gz_log_event_count);
	return 0;
}

static ssize_t gz_log_read(struct file *file, char __user *buf, size_t size,
			   loff_t *ppos)
{
	DEFINE_WAIT(wait);
	int ret;

	for (;;) {
		prepare_to_wait(&gz_log_wq, &wait, TASK_INTERRUPTIBLE);
		if (poll_event != atomic_read(&gz_log_event_count))
			break;
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
		schedule();
	}
	finish_wait(&gz_log_wq, &wait);
	ret = do_gz_log_read(file, buf, size);
	poll_event = atomic_read(&gz_log_event_count);
	return ret;
}

static int gz_log_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned int gz_log_poll(struct file *file, poll_table *wait)
{
	int mask = 0;

	poll_wait(file, &gz_log_wq, wait);
	if (poll_event != atomic_read(&gz_log_event_count))
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static const struct file_operations proc_gz_log_file_operations = {
	.owner = THIS_MODULE,
	.open = gz_log_open,
	.read = gz_log_read,
	.release = gz_log_release,
	.poll = gz_log_poll,
};

static int gz_log_proc_init(void)
{
	struct proc_dir_entry *gz_log_proc_file;

	init_waitqueue_head(&gz_log_wq);

	/* create /proc/gz_log */
	gz_log_proc_file = proc_create("gz_log", 0444, NULL,
				       &proc_gz_log_file_operations);
	if (gz_log_proc_file == NULL) {
		pr_info("gz_log proc_create failed!\n");
		return -ENOMEM;
	}

	return 0;
}

int trusty_call_nop_std32(uint32_t type, uint64_t value)
{
	int ret;
	uint32_t val_a = value;
	uint32_t val_b = value >> 32;

	dev_dbg(tls->trusty_dev, "%s\n", __func__);

	ret = trusty_std_call32(tls->trusty_dev, SMC_SC_NOP, type, val_a,
				val_b);
	while (ret == SM_ERR_NOP_INTERRUPTED || ret == SM_ERR_BUSY) {
		if (ret == SM_ERR_BUSY) {
			usleep_range(100, 500);
			ret = trusty_std_call32(tls->trusty_dev, SMC_SC_NOP,
						type, val_a, val_b);
		} else {
			ret = trusty_std_call32(tls->trusty_dev, SMC_SC_NOP, 0,
						0, 0);
		}
	}

	if (ret != SM_ERR_NOP_DONE)
		dev_info(tls->trusty_dev, "%s: SMC_SC_NOP failed %d", __func__,
			 ret);

	return ret;
}

/* get_gz_log_buffer was called in arch_initcall */
void get_gz_log_buffer(unsigned long *addr, unsigned long *size,
		       unsigned long *start)
{
	*addr = (unsigned long)page_address(trusty_log_pages);
	pr_info("trusty_log_pages virtual address:%lx\n", (unsigned long)*addr);
	*start = 0;
	*size = TRUSTY_LOG_SIZE;
}

int gz_log_page_init(void)
{
	trusty_log_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO | GFP_DMA,
				       get_order(TRUSTY_LOG_SIZE));
	if (!trusty_log_pages) {
		pr_info("trusty_log_pages alloc fail!\n");
		return -ENOMEM;
	}

	return 0;
}
arch_initcall(gz_log_page_init);

static int trusty_log_probe(struct platform_device *pdev)
{
	int result;
	phys_addr_t pa;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (!trusty_supports_logging(pdev->dev.parent))
		return -ENXIO;

	tls = kzalloc(sizeof(*tls), GFP_KERNEL);
	if (!tls) {
		result = -ENOMEM;
		goto error_alloc_state;
	}

	spin_lock_init(&tls->lock);
	tls->dev = &pdev->dev;
	tls->trusty_dev = tls->dev->parent;
	tls->get = 0;
	tls->log_pages = trusty_log_pages;
	if (!tls->log_pages) {
		result = -ENOMEM;
		goto error_alloc_log;
	}
	tls->log = page_address(tls->log_pages);
	pr_info("tls->log virtual address:%p\n", tls->log);

	pa = page_to_phys(tls->log_pages);
	pr_info("tls->log physical address:%x\n", (unsigned int)pa);
	result = trusty_std_call32(tls->trusty_dev, SMC_SC_SHARED_LOG_ADD,
				   (u32)(pa), (u32)((u64)pa >> 32),
				   TRUSTY_LOG_SIZE);
	if (result < 0) {
		pr_info("trusty std call (SHARED_LOG_ADD) failed: %d %pa\n",
			result, &pa);
		goto error_std_call;
	}

	tls->call_notifier.notifier_call = trusty_log_call_notify;
	result = trusty_call_notifier_register(tls->trusty_dev,
					       &tls->call_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register trusty call notifier\n");
		goto error_call_notifier;
	}

	tls->panic_notifier.notifier_call = trusty_log_panic_notify;
	result = atomic_notifier_chain_register(&panic_notifier_list,
						&tls->panic_notifier);
	if (result < 0) {
		dev_err(&pdev->dev, "failed to register panic notifier\n");
		goto error_panic_notifier;
	}
	platform_set_drvdata(pdev, tls);

	gz_log_proc_init();

	return 0;

error_panic_notifier:
	trusty_call_notifier_unregister(tls->trusty_dev, &tls->call_notifier);
error_call_notifier:
	trusty_std_call32(tls->trusty_dev, SMC_SC_SHARED_LOG_RM, (u32)pa,
			  (u32)((u64)pa >> 32), 0);
error_std_call:
	__free_pages(tls->log_pages, get_order(TRUSTY_LOG_SIZE));
error_alloc_log:
	kfree(tls);
error_alloc_state:
	return result;
}

static int trusty_log_remove(struct platform_device *pdev)
{
	int result;
	phys_addr_t pa = page_to_phys(tls->log_pages);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &tls->panic_notifier);
	trusty_call_notifier_unregister(tls->trusty_dev, &tls->call_notifier);

	result = trusty_std_call32(tls->trusty_dev, SMC_SC_SHARED_LOG_RM,
				   (u32)pa, (u32)((u64)pa >> 32), 0);
	if (result) {
		pr_info("trusty std call (SMC_SC_SHARED_LOG_RM) failed: %d\n",
			result);
	}
	__free_pages(tls->log_pages, get_order(TRUSTY_LOG_SIZE));
	kfree(tls);

	return 0;
}

static const struct of_device_id trusty_test_of_match[] = {
	{
		.compatible = "android,trusty-log-v1",
	},
	{},
};

static struct platform_driver trusty_log_driver = {
	.probe = trusty_log_probe,
	.remove = trusty_log_remove,
	.driver = {
			.name = "trusty-log",
			.owner = THIS_MODULE,
			.of_match_table = trusty_test_of_match,
		},
};

module_platform_driver(trusty_log_driver);
