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
#include <asm/page.h>
#include "trusty-log.h"

#define TRUSTY_LOG_SIZE (PAGE_SIZE * 2)
#define TRUSTY_LINE_BUFFER_SIZE 256

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
	size_t max_to_read = min((size_t)(put - get),
				 sizeof(s->line_buffer) - 1);
	size_t mask = log->sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = log->data[get++ & mask];
	s->line_buffer[i] = '\0';

	return i;
}

static void trusty_dump_logs(struct trusty_log_state *s)
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
			pr_err("trusty: log overflow.");
			get = alloc - log->sz;
			continue;
		}
		pr_info("trusty: %s", s->line_buffer);
		get += read_chars;
	}
	s->get = get;
}

static int trusty_log_call_notify(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct trusty_log_state *s;
	unsigned long flags;

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	s = container_of(nb, struct trusty_log_state, call_notifier);
	spin_lock_irqsave(&s->lock, flags);
	trusty_dump_logs(s);
	spin_unlock_irqrestore(&s->lock, flags);
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
	trusty_dump_logs(s);
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
		pr_err("trusty std call (SMC_SC_SHARED_LOG_VERSION) failed: %d\n",
		       result);
		return false;
	}

	if (result == TRUSTY_LOG_API_VERSION) {
		return true;
	} else {
		pr_info("trusty-log unsupported api version: %d, supported: %d\n",
			result, TRUSTY_LOG_API_VERSION);
		return false;
	}
}

static int trusty_log_probe(struct platform_device *pdev)
{
	struct trusty_log_state *s;
	int result;
	phys_addr_t pa;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (!trusty_supports_logging(pdev->dev.parent)) {
		return -ENXIO;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		result = -ENOMEM;
		goto error_alloc_state;
	}

	spin_lock_init(&s->lock);
	s->dev = &pdev->dev;
	s->trusty_dev = s->dev->parent;
	s->get = 0;
	s->log_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO,
				   get_order(TRUSTY_LOG_SIZE));
	if (!s->log_pages) {
		result = -ENOMEM;
		goto error_alloc_log;
	}
	s->log = page_address(s->log_pages);

	pa = page_to_phys(s->log_pages);
	result = trusty_std_call32(s->trusty_dev,
				   SMC_SC_SHARED_LOG_ADD,
				   (u32)(pa), (u32)((u64)pa >> 32),
				   TRUSTY_LOG_SIZE);
	if (result < 0) {
		pr_err("trusty std call (SMC_SC_SHARED_LOG_ADD) failed: %d %pa\n",
		       result, &pa);
		goto error_std_call;
	}

	s->call_notifier.notifier_call = trusty_log_call_notify;
	result = trusty_call_notifier_register(s->trusty_dev,
					       &s->call_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register trusty call notifier\n");
		goto error_call_notifier;
	}

	s->panic_notifier.notifier_call = trusty_log_panic_notify;
	result = atomic_notifier_chain_register(&panic_notifier_list,
						&s->panic_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register panic notifier\n");
		goto error_panic_notifier;
	}
	platform_set_drvdata(pdev, s);

	return 0;

error_panic_notifier:
	trusty_call_notifier_unregister(s->trusty_dev, &s->call_notifier);
error_call_notifier:
	trusty_std_call32(s->trusty_dev, SMC_SC_SHARED_LOG_RM,
			  (u32)pa, (u32)((u64)pa >> 32), 0);
error_std_call:
	__free_pages(s->log_pages, get_order(TRUSTY_LOG_SIZE));
error_alloc_log:
	kfree(s);
error_alloc_state:
	return result;
}

static int trusty_log_remove(struct platform_device *pdev)
{
	int result;
	struct trusty_log_state *s = platform_get_drvdata(pdev);
	phys_addr_t pa = page_to_phys(s->log_pages);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &s->panic_notifier);
	trusty_call_notifier_unregister(s->trusty_dev, &s->call_notifier);

	result = trusty_std_call32(s->trusty_dev, SMC_SC_SHARED_LOG_RM,
				   (u32)pa, (u32)((u64)pa >> 32), 0);
	if (result) {
		pr_err("trusty std call (SMC_SC_SHARED_LOG_RM) failed: %d\n",
		       result);
	}
	__free_pages(s->log_pages, get_order(TRUSTY_LOG_SIZE));
	kfree(s);

	return 0;
}

static const struct of_device_id trusty_test_of_match[] = {
	{ .compatible = "android,trusty-log-v1", },
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
