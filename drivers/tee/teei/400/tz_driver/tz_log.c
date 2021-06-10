// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * Copyright (C) 2015 Google, Inc.
 *
 */

#define IMSG_TAG "[tz_log]"
#include <imsg_log.h>

#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/log2.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <asm/page.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <teei_client_main.h>
#ifdef CONFIG_MTK_TEE_SANITY
#include <tee_sanity.h>
#endif
#include "tz_log.h"
#include "nt_smc_call.h"
#include "notify_queue.h"
#include "switch_queue.h"
#include "utdriver_macro.h"
#include "log_perf.h"

#define TEE_LOG_TYPE	0x10

struct tz_log_state *g_tz_log_state;
static struct completion teei_log_comp;

int init_tlog_comp_fn(void)
{
	init_completion(&teei_log_comp);

	return 0;
}

void teei_notify_log_fn(void)
{
	if (!completion_done(&teei_log_comp))
		complete(&teei_log_comp);
}

static int __tz_driver_read_logs(struct tz_log_state *s, char *buffer,
				uint32_t get, unsigned int cnt)
{
	struct log_rb *log = s->log;
	int i = 0;
	size_t mask = log->sz - 1;

	for (i = 0; i < cnt;)
		buffer[i++] = log->data[get++ & mask];

	return i;
}

int tz_driver_read_logs(char *buffer, unsigned long count)
{
	struct tz_log_state *local_s = NULL;
	uint32_t get = 0;
	uint32_t put = 0;
	uint32_t alloc = 0;
	int read_chars = 0;
	struct log_rb *log = NULL;
	unsigned int real_cnt = 0;

	local_s = g_tz_log_state;
	log = local_s->log;

	get = local_s->read_get;
	put = log->put;

	if (put != get) {

		alloc = log->alloc;

		if ((alloc - get) > log->sz) {
			IMSG_INFO("log overflow.\n");
			get = alloc - log->sz;
		}

		if ((put - get) > (unsigned int)count)
			real_cnt = (unsigned int)count;
		else
			real_cnt = put - get;

		read_chars = __tz_driver_read_logs(local_s,
					buffer, get, real_cnt);

		get += read_chars;
	}

	local_s->read_get = get;
	return read_chars;

}

static int log_read_line(struct tz_log_state *s, int put, int get)
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

static void tz_driver_dump_logs(struct tz_log_state *s)
{
	struct log_rb *log = s->log;
	struct boot_log_rb *boot_log = s->boot_log;
	uint32_t get, put, alloc;
	int read_chars;
	static DEFINE_RATELIMIT_STATE(_rs,
				TZ_LOG_RATELIMIT_INTERVAL,
				TZ_LOG_RATELIMIT_BURST);

	if (unlikely(log->put == 0)) {
		IMSG_DEBUG("TEE log buffer not ready yet\n");
		return;
	}

	WARN_ON(!is_power_of_2(log->sz));

	/*
	 * For this ring buffer, at any given point, alloc >= put >= get.
	 * The producer side of the buffer is not locked, so the put and alloc
	 * pointers must be read in a defined order (put before alloc) so
	 * that the above condition is maintained. A read barrier is needed
	 * to make sure the hardware and compiler keep the reads ordered.
	 */
	get = s->get;
	while ((put = log->put) != get) {
		/* Make sure that the read of put occurs */
		/* before the read of log data */
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
			IMSG_ERROR("log overflow.");
			get = alloc - log->sz;
			continue;
		}

		/*
		 * Due to UART speed is slow, printing large number of
		 * messages to UART will cause system reset by watchdog.
		 * It can prevent from printing message to uart by using
		 * KERN_DEBUG log level if with default print setting.
		 * (default setting is print message to uart
		 * if log level >= KERN_INFO)
		 */

		IMSG_PRINTK("[TZ_LOG] %s", s->line_buffer);

		/*
		 * Dump early log to boot log buffer
		 * until boot log buffer is full
		 */
		if (boot_log->put + read_chars < boot_log->sz) {
			char *log_buf = &boot_log->data[boot_log->put];

			memcpy(log_buf, s->line_buffer, read_chars);
			boot_log->put += read_chars;
		}

		/* Print warning message */
		/* if log output frequency is over rate limit */
		__ratelimit(&_rs);

		get += read_chars;

#if ENABLE_LOG_PERF == 1
		measure_log_perf(IMSG_TAG, read_chars,
					*(uint32_t *)(s->line_buffer));
#endif
	}
	s->get = get;
}

int teei_log_fn(void *work)
{
	int retVal = 0;
#ifdef CONFIG_MICROTRUST_TZ_LOG
	struct tz_log_state *s;
	unsigned long flags;

	s = g_tz_log_state;
#endif

	while (1) {
		if (switch_input_index == switch_output_index) {
			retVal = wait_for_completion_interruptible(
							&teei_log_comp);
			if (retVal != 0)
				continue;
		}

#ifdef CONFIG_MICROTRUST_TZ_LOG
		msleep(20);
		tz_driver_dump_logs(s);
#endif

	}

	return NOTIFY_OK;
}

static int tz_log_panic_notify(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct tz_log_state *s;

	/*
	 * Don't grab the spin lock to hold up the panic notifier, even
	 * though this is racy.
	 */
	s = container_of(nb, struct tz_log_state, panic_notifier);
	IMSG_INFO("tz log panic notifier\n");
	tz_driver_dump_logs(s);
	return NOTIFY_OK;
}

#ifdef ENABLED_TEEI_BOOT_LOG
static struct tz_log_state *get_tz_log_state(void)
{
	struct tz_driver_state *drv_state = get_tz_drv_state();
	struct platform_device *pdev = drv_state->tz_log_pdev;

	return pdev->dev.platform_data;
}

static void *boot_log_seq_start(struct seq_file *f, loff_t *pos)
{
	struct tz_log_state *s = get_tz_log_state();
	struct boot_log_rb *log = s->boot_log;

	if (*pos >= log->put)
		return NULL;

	return (void *)log;
}

static void *boot_log_seq_next(struct seq_file *f, void *v, loff_t *pos)
{
	struct boot_log_rb *log = v;

	if (*pos >= log->put)
		return NULL;

	*pos = log->get;
	return v;
}

static void boot_log_seq_stop(struct seq_file *f, void *v)
{
}

static int boot_log_read_line(struct boot_log_rb *log, char *out)
{
	int put = log->put;
	int get = log->get;
	int i;
	char c = '\0';
	size_t max_to_read = min((size_t)(put - get),
				(size_t)TZ_LINE_BUFFER_SIZE - 1);

	size_t mask = log->sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		out[i++] = c = log->data[get++ & mask];
	out[i] = '\0';

	return i;
}

static int boot_log_seq_show(struct seq_file *f, void *v)
{
	struct boot_log_rb *log = v;
	char line_buffer[TZ_LINE_BUFFER_SIZE];
	uint32_t read_chars;

	read_chars = boot_log_read_line(log, line_buffer);

	if (read_chars) {
		seq_printf(f, "%s", line_buffer);
		log->get += read_chars;
	}

	return 0;
}

static const struct seq_operations boot_log_seq_ops = {
	.start = boot_log_seq_start,
	.next = boot_log_seq_next,
	.stop = boot_log_seq_stop,
	.show = boot_log_seq_show
};

static int boot_log_open(struct inode *inode, struct file *file)
{
	struct tz_log_state *s = get_tz_log_state();
	struct boot_log_rb *log = s->boot_log;

	log->get = 0;

	return seq_open(file, &boot_log_seq_ops);
};

static const struct file_operations boot_log_fops = {
	.open = boot_log_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

static struct dentry *root_entry;
static int tz_log_debugfs_init(void)
{
	root_entry = debugfs_create_dir("tz_log", NULL);
	if (!root_entry) {
		IMSG_WARN("Can not create tz_log debugfs\n");
		return -1;
	}

	debugfs_create_file("boot_log", 0444, root_entry, NULL, &boot_log_fops);

	return 0;
}


#endif

int teei_change_log_status(unsigned long new_status)
{
	struct completion *wait_completion = NULL;
	int retVal = 0;

	teei_cpus_read_lock();

	wait_completion = kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (wait_completion == NULL) {
		IMSG_ERROR("TEEI: Failed to alloc completion[%s]\n", __func__);
		teei_cpus_read_unlock();
		return -ENOMEM;
	}

	init_completion(wait_completion);

	retVal = add_work_entry(SMC_CALL_TYPE, N_INVOKE_T_NQ, 0, 0, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add_work_entry[%s]\n", __func__);
		kfree(wait_completion);
		teei_cpus_read_unlock();
		return retVal;
	}

	retVal = add_nq_entry(TEEI_MODIFY_TEE_CONFIG, TEE_LOG_TYPE,
				(unsigned long long)(wait_completion),
				new_status, 0, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add one nq to n_t_buffer\n");
		kfree(wait_completion);
		teei_cpus_read_unlock();
		return retVal;
	}

	teei_notify_switch_fn();

	wait_for_completion(wait_completion);
	kfree(wait_completion);
	teei_cpus_read_unlock();

	return 0;
}

int tz_log_probe(struct platform_device *pdev)
{
	struct tz_log_state *s;
	int result;

	IMSG_DEBUG("%s\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		result = -ENOMEM;
		goto error_alloc_state;
	}

	g_tz_log_state = s;

	spin_lock_init(&s->lock);
	s->dev = &pdev->dev;
	s->get = 0;
	s->read_get = 0;
	s->log_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO | GFP_DMA,
				   get_order(TZ_LOG_SIZE));
	if (!s->log_pages) {
		result = -ENOMEM;
		goto error_alloc_log;
	}
	s->log = page_address(s->log_pages);

	s->boot_log_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO | GFP_DMA,
				   get_order(TZ_LOG_SIZE));
	if (!s->boot_log_pages) {
		result = -ENOMEM;
		goto error_alloc_boot_log;
	}
	s->boot_log = page_address(s->boot_log_pages);

	s->boot_log->put = 0;
	s->boot_log->sz = rounddown_pow_of_two(
				TZ_LOG_SIZE - sizeof(struct boot_log_rb));

	s->panic_notifier.notifier_call = tz_log_panic_notify;
	result = atomic_notifier_chain_register(&panic_notifier_list,
						&s->panic_notifier);
	if (result < 0) {
		IMSG_ERROR("failed to register panic notifier\n");
		goto error_panic_notifier;
	}
	platform_device_add_data(pdev, s, sizeof(struct tz_log_state));

#ifdef ENABLED_TEEI_BOOT_LOG
	tz_log_debugfs_init();
#endif

	return 0;

error_panic_notifier:
	__free_pages(s->boot_log_pages, get_order(TZ_LOG_SIZE));
error_alloc_boot_log:
	__free_pages(s->log_pages, get_order(TZ_LOG_SIZE));
error_alloc_log:
	kfree(s);
	g_tz_log_state = NULL;
error_alloc_state:
	return result;
}

int tz_log_remove(struct platform_device *pdev)
{
	struct tz_log_state *s = dev_get_platdata(&pdev->dev);

	IMSG_DEBUG("%s\n", __func__);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &s->panic_notifier);

	__free_pages(s->log_pages, get_order(TZ_LOG_SIZE));
	__free_pages(s->boot_log_pages, get_order(TZ_LOG_SIZE));
	kfree(s);
	g_tz_log_state = NULL;

	return 0;
}

