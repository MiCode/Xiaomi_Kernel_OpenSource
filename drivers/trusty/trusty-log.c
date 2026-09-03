/*
 * Copyright (C) 2015 Unisoc, Inc.
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

#define pr_fmt(fmt) "trusty-log: " fmt

#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/log2.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/seq_file.h>
#include <asm/page.h>
#include <linux/trusty/smcall.h>
#include <linux/vmalloc.h>
#include "trusty-log.h"
#include "trusty.h"

#define SMC_SC_SYSCTL_SET_CONSOLE	SMC_STDCALL_NR(SMC_ENTITY_SYSCTL, 0)
#define SMC_SC_SYSCTL_GET_CONSOLE	SMC_STDCALL_NR(SMC_ENTITY_SYSCTL, 1)
#define SMC_SC_SYSCTL_SET_LOGLEVEL	SMC_STDCALL_NR(SMC_ENTITY_SYSCTL, 2)
#define SMC_SC_SYSCTL_GET_LOGLEVEL	SMC_STDCALL_NR(SMC_ENTITY_SYSCTL, 3)

#define TRUSTY_LOG_DEFAULT_SIZE (PAGE_SIZE * 32)
#define TRUSTY_LOG_MAX_SIZE (PAGE_SIZE * 32)
#define TRUSTY_LINE_BUFFER_SIZE 256
extern struct atomic_notifier_head panic_notifier_list;

/*
 * The "log_to_dmesg" parameter can have three values: "never", "always",
 * and "until_first_reader". "never" indicates Trusty logs will never be
 * copied to the linux kernel log, while "always" indicates they will always
 * be copied to the linux kernel log. In both cases Trusty logs can still
 * be read from the /dev/trusty-logX virtual file. In the case of "always"
 * that may mean logs show up duplicated in logcat.
 * The third option, "until_first_reader", copies Trusty logs to the linux
 * kernel log, but only until /dev/trusty-logX is first opened. After that
 * Trusty logs will no longer be copied to the kernel log and are only
 * available from /dev/trusty-logX.
 */

enum log_to_dmesg_options {
	NEVER,
	ALWAYS,
	UNTIL_FIRST_READER
};

static const char * const log_to_dmesg_opt_names[] = {
	"never", "always", "until_first_reader"
};

static int log_to_dmesg_param = NEVER;

static int trusty_log_mode_set(const char *val, const struct kernel_param *kp)
{
	int i = sysfs_match_string(log_to_dmesg_opt_names, val);

	if (i < 0)
		return i;

	log_to_dmesg_param = i;
	return 0;
}

static int trusty_log_mode_get(char *buffer, const struct kernel_param *kp)
{
	int i;

	/*
	 * Output buffer is PAGE_SIZE, of which we'll use only 35 bytes,
	 * so bounds checks are not necessary in the following code.
	 */
	*buffer = 0;
	for (i = 0; i < ARRAY_SIZE(log_to_dmesg_opt_names); i++) {
		if (log_to_dmesg_param == i)
			strcat(buffer, "[");
		strcat(buffer, log_to_dmesg_opt_names[i]);
		if (log_to_dmesg_param == i)
			strcat(buffer, "]");
		strcat(buffer, " ");
	}
	strcat(buffer, "\n");
	return strlen(buffer);
}

module_param_call(log_to_dmesg, trusty_log_mode_set, trusty_log_mode_get, NULL, 0644);

/**
 * struct trusty_log_sfile - trusty log misc device state
 *
 * @misc:          misc device created for the trusty log virtual file
 * @device_name:   misc device name following the convention
 *                 "trusty-<name><id>"
 */
struct trusty_log_sfile {
	struct miscdevice misc;
	char device_name[64];
};

/**
 * struct trusty_log_sink_state - trusty log sink state
 *
 * @get:              current read unwrapped index
 * @last_successful_next:
 *                    index for the next line after the last successful get
.* @trusty_panicked:  trusty panic status at the start of the sink interation
 *                    (only used for kernel log sink)
 * @sfile:            seq_file used for sinking to a virtual file (misc device);
 *                    set to NULL for the kernel log sink.
 * @ignore_overflow:  ignore_overflow used to coalesce overflow messages and
 *                    avoid reporting an overflow when sinking the oldest
 *                    line to the virtual file (only used for virtual file sink)
 *
 * A sink state structure is used for both the kernel log sink
 * and the virtual device sink.
 * An instance of the sink state structure is dynamically created
 * for each read iteration of the trusty log virtual file (misc device).
 *
 */
struct trusty_log_sink_state {
	u32 get;
	u32 last_successful_next;
	bool trusty_panicked;
	bool trusty_wait_panicked;

	/* virtual file sink specific attributes */
	struct seq_file *sfile;
	bool ignore_overflow;
};

struct trusty_log_state {
	struct device *dev;
	struct device *trusty_dev;
	struct trusty_log_sfile log_sfile;

	/*
	 * This lock is here to ensure only one consumer will read
	 * from the log ring buffer at a time.
	 */
	spinlock_t lock;
	struct log_rb *log;
	struct trusty_log_sink_state klog_sink;
	uint32_t get;

	uint64_t *pfn_list;
	struct page **pages;
	int num_pages;

	int dumpable;

	struct notifier_block call_notifier;
	struct notifier_block panic_notifier;
	char line_buffer[TRUSTY_LINE_BUFFER_SIZE];
	wait_queue_head_t poll_waiters;
	/* this lock protects access to wake_put */
	spinlock_t wake_up_lock;
	u32 last_wake_put;
	bool have_first_reader;
};

static inline u32 u32_add_overflow(u32 a, u32 b)
{
	u32 d;

	if (check_add_overflow(a, b, &d)) {
		/*
		 * silence the overflow,
		 * what matters in the log buffer context
		 * is the casted addition
		 */
	}
	return d;
}

static inline u32 u32_sub_overflow(u32 a, u32 b)
{
	u32 d;

	if (check_sub_overflow(a, b, &d)) {
		/*
		 * silence the overflow,
		 * what matters in the log buffer context
		 * is the casted substraction
		 */
	}
	return d;
}

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

/**
 * trusty_log_has_data() - returns true when more data is available to sink
 * @s:         Current log state.
 * @sink:      trusty_log_sink_state holding the get index on a given sink
 *
 * Return: true if data is available.
 */
static bool trusty_log_has_data(struct trusty_log_state *s,
				struct trusty_log_sink_state *sink)
{
	struct log_rb *log = s->log;

	return (log->put != sink->get);
}

/**
 * trusty_log_start() - initialize the sink iteration either to kernel log
 * or to secondary log_sfile
 * @s:         Current log state.
 * @sink:      trusty_log_sink_state holding the get index on a given sink
 * @index:     Unwrapped ring buffer index from where iteration shall start
 *
 * Return: 0 if successful, negative error code otherwise
 */
static int trusty_log_start(struct trusty_log_state *s,
			    struct trusty_log_sink_state *sink,
			    u32 index)
{
	struct log_rb *log;

	if (WARN_ON(!s))
		return -EINVAL;

	log = s->log;
	if (WARN_ON(!is_power_of_2(log->sz)))
		return -EINVAL;

	sink->get = index;
	return 0;
}

/**
 * trusty_log_show() - sink log entry at current iteration
 * @s:         Current log state.
 * @sink:      trusty_log_sink_state holding the get index on a given sink
 */
static void trusty_log_show(struct trusty_log_state *s,
			    struct trusty_log_sink_state *sink)
{
	struct log_rb *log = s->log;
	u32 alloc, put, get;
	int read_chars;

	if (sink->sfile && sink == &s->klog_sink)
		dev_warn(s->dev, "klog_sink has seq_file\n");

	/*
	 * For this ring buffer, at any given point, alloc >= put >= get.
	 * The producer side of the buffer is not locked, so the put and alloc
	 * pointers must be read in a defined order (put before alloc) so
	 * that the above condition is maintained. A read barrier is needed
	 * to make sure the hardware and compiler keep the reads ordered.
	 */
	get = sink->get;
	put = log->put;

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
	if (u32_sub_overflow(alloc, get) > log->sz) {
		/*
		 * this condition is acceptable in the case of the sfile sink
		 * when attempting to read the oldest entry (at alloc-log->sz)
		 * which may be overrun by a new one when ring buffer write
		 * index wraps around.
		 * So the overrun is not reported in case the oldest line
		 * was being read.
		 */
		if (sink->sfile) {
			if (!sink->ignore_overflow)
				seq_puts(sink->sfile, "log overflow.\n");
			/* coalesce subsequent contiguous overflows. */
			sink->ignore_overflow = true;
		} else {
			dev_err(s->dev, "log overflow.\n");
		}
		sink->get = u32_sub_overflow(alloc, log->sz);
		return;
	}
	/* compute next line index */
	sink->get = u32_add_overflow(get, read_chars);
	/* once a line is valid, ignore_overflow must be disabled */
	sink->ignore_overflow = false;
	if (sink->sfile) {
		seq_printf(sink->sfile, "%s", s->line_buffer);
	} else {
		if (sink->trusty_panicked) {
			/* next line after last successful get */
			sink->last_successful_next = sink->get;
		}
	}
}

static void *trusty_log_seq_start(struct seq_file *sfile, loff_t *pos)
{
	struct trusty_log_sfile *lb;
	struct trusty_log_state *s;
	struct log_rb *log;
	struct trusty_log_sink_state *log_sfile_sink;
	u32 index;
	int rc;

	if (WARN_ON(!pos))
		return ERR_PTR(-EINVAL);

	lb = sfile->private;
	if (WARN_ON(!lb))
		return ERR_PTR(-EINVAL);

	log_sfile_sink = kzalloc(sizeof(*log_sfile_sink), GFP_KERNEL);
	if (!log_sfile_sink)
		return ERR_PTR(-ENOMEM);

	s = container_of(lb, struct trusty_log_state, log_sfile);
	s->klog_sink.sfile = sfile;

	log_sfile_sink->sfile = sfile;
	log = s->log;
	if (*pos == 0) {
		/* start at the oldest line */
		index = 0;
		if (log->alloc > log->sz)
			index = u32_sub_overflow(log->alloc, log->sz);
	} else {
		/*
		 * '*pos>0': pos hold the 32bits unwrapped index from where
		 * to start iterating
		 */
		index = (u32)*pos;
	}
	pr_debug("%s start=%u\n", __func__, index);

	log_sfile_sink->ignore_overflow = true;
	rc = trusty_log_start(s, log_sfile_sink, index);
	if (rc < 0)
		goto free_sink;

	if (!trusty_log_has_data(s, log_sfile_sink))
		goto free_sink;

	return log_sfile_sink;

free_sink:
	pr_debug("%s kfree\n", __func__);
	kfree(log_sfile_sink);
	return rc < 0 ? ERR_PTR(rc) : NULL;
}

static void *trusty_log_seq_next(struct seq_file *sfile, void *v, loff_t *pos)
{
	struct trusty_log_sfile *lb;
	struct trusty_log_state *s;
	struct trusty_log_sink_state *log_sfile_sink = v;
	int rc = 0;

	if (WARN_ON(!log_sfile_sink))
		return ERR_PTR(-EINVAL);

	lb = sfile->private;
	if (WARN_ON(!lb)) {
		rc = -EINVAL;
		goto end_of_iter;
	}
	s = container_of(lb, struct trusty_log_state, log_sfile);

	if (WARN_ON(!pos)) {
		rc = -EINVAL;
		goto end_of_iter;
	}
	/*
	 * When starting a virtual file sink, the start function is invoked
	 * with a pos argument which value is set to zero.
	 * Subsequent starts are invoked with pos being set to
	 * the unwrapped read index (get).
	 * Upon u32 wraparound, the get index could be reset to zero.
	 * Thus a msb is used to distinguish the `get` zero value
	 * from the `start of file` zero value.
	 */
	*pos = (1ULL << 32) + log_sfile_sink->get;
	if (!trusty_log_has_data(s, log_sfile_sink))
		goto end_of_iter;


	return log_sfile_sink;

end_of_iter:
	pr_debug("%s kfree\n", __func__);
	kfree(log_sfile_sink);
	return rc < 0 ? ERR_PTR(rc) : NULL;
}

static void trusty_log_seq_stop(struct seq_file *sfile, void *v)
{
	/*
	 * When iteration completes or on error, the next callback frees
	 * the sink structure and returns NULL/error-code.
	 * In that case stop (being invoked with void* v set to the last next
	 * return value) would be invoked with v == NULL or error code.
	 * When user space stops the iteration earlier than the end
	 * (in case of user-space memory allocation limit for example)
	 * then the stop function receives a non NULL get pointer
	 * and is in charge or freeing the sink structure.
	 */
	struct trusty_log_sink_state *log_sfile_sink = v;

	/* nothing to do - sink structure already freed */
	if (IS_ERR_OR_NULL(log_sfile_sink))
		return;
	kfree(log_sfile_sink);

	pr_debug("%s kfree\n", __func__);
}

static int trusty_log_seq_show(struct seq_file *sfile, void *v)
{
	struct trusty_log_sfile *lb;
	struct trusty_log_state *s;
	struct trusty_log_sink_state *log_sfile_sink = v;

	if (WARN_ON(!log_sfile_sink))
		return -EINVAL;

	lb = sfile->private;
	if (WARN_ON(!lb))
		return -EINVAL;

	s = container_of(lb, struct trusty_log_state, log_sfile);

	trusty_log_show(s, log_sfile_sink);
	return 0;
}

static void trusty_dump_logs(struct trusty_log_state *s, unsigned long action)
{
	struct log_rb *log = s->log;
	uint32_t get, put, alloc;
	int read_chars;

	if (!s->dumpable) {
		pr_err_once("Trusty log currently is not dumpable\n");
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
		/*
		 * Make sure that the read of put occurs before the read
		 * of log data
		 */
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
			pr_err("trusty: log overflow(sz %d)\n", log->sz);
			get = alloc - log->sz;
			continue;
		}
		if (action == TRUSTY_CALL_PANIC)
			pr_emerg("trusty: %s", s->line_buffer);
		else
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
	u32 cur_put;

	if (action == TRUSTY_CALL_PREPARE)
		return NOTIFY_DONE;

	s = container_of(nb, struct trusty_log_state, call_notifier);
	spin_lock_irqsave(&s->wake_up_lock, flags);
	cur_put = s->log->put;
	if (cur_put != s->last_wake_put) {
		s->last_wake_put = cur_put;
		wake_up_all(&s->poll_waiters);
	}
	spin_unlock_irqrestore(&s->wake_up_lock, flags);
	if (log_to_dmesg_param == ALWAYS || (log_to_dmesg_param == UNTIL_FIRST_READER &&
	     !s->have_first_reader)) {
		spin_lock_irqsave(&s->lock, flags);
		trusty_dump_logs(s, action);
		spin_unlock_irqrestore(&s->lock, flags);
	}
	return NOTIFY_OK;
}

static void trusty_panic_status(struct trusty_log_state *s)
{
	u32 start;
	int rc;
	/*
	 * note: klopg_sink.get and last_successful_next
	 * initialized to zero by kzalloc
	 */
	s->klog_sink.trusty_panicked = trusty_get_panic_status(s->trusty_dev);

	/* output to kernel log ONLY when the dmesg param is ALWAYS */
	if (s->klog_sink.trusty_panicked && log_to_dmesg_param == ALWAYS)
		trusty_dump_logs(s, TRUSTY_CALL_PANIC);

	// start = s->klog_sink.trusty_panicked ?
	// 		s->klog_sink.last_successful_next :
	// 		s->klog_sink.get;
	start = s->klog_sink.last_successful_next;
	rc = trusty_log_start(s, &s->klog_sink, start);
	if (rc < 0)
		return;

	while (trusty_log_has_data(s, &s->klog_sink))
		trusty_log_show(s, &s->klog_sink);
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

	trusty_panic_status(s);

	return NOTIFY_OK;
}

const struct seq_operations trusty_log_seq_ops = {
	.start = trusty_log_seq_start,
	.stop = trusty_log_seq_stop,
	.next = trusty_log_seq_next,
	.show = trusty_log_seq_show,
};

static int trusty_log_sfile_dev_open(struct inode *inode, struct file *file)
{
	struct trusty_log_sfile *ls;
	struct trusty_log_state *s;
	struct seq_file *sfile;
	int rc;

	/*
	 * file->private_data contains a pointer to the misc_device struct
	 * passed to misc_register()
	 */
	if (WARN_ON(!file->private_data))
		return -EINVAL;

	ls = container_of(file->private_data, struct trusty_log_sfile, misc);

	/*
	 * seq_open uses file->private_data to store the seq_file associated
	 * with the struct file, but it must be NULL when seq_open is called
	 */
	file->private_data = NULL;
	rc = seq_open(file, &trusty_log_seq_ops);
	if (rc < 0)
		return rc;

	sfile = file->private_data;
	if (WARN_ON(!sfile))
		return -EINVAL;

	sfile->private = ls;
	s = container_of(ls, struct trusty_log_state, log_sfile);
	s->have_first_reader = true;
	return 0;
}

static unsigned int trusty_log_sfile_dev_poll(struct file *filp,
					      struct poll_table_struct *wait)
{
	struct seq_file *sfile;
	struct trusty_log_sfile *lb;
	struct trusty_log_state *s;
	struct log_rb *log;

	/*
	 * trusty_log_sfile_dev_open() pointed filp->private_data to a
	 * seq_file, and that seq_file->private to the trusty_log_sfile
	 * field of a trusty_log_state
	 */
	sfile = filp->private_data;
	lb = sfile->private;
	s = container_of(lb, struct trusty_log_state, log_sfile);

	poll_wait(filp, &s->poll_waiters, wait);
	log = s->log;

	/*
	 * Userspace has read up to sfile->index so far. Update klog_sink
	 * to indicate that, so that we don't end up dumping the entire
	 * Trusty log in case of panic. Only do this when not logging to
	 * klog_sink, since logging to klog_sink already updates this.
	 */
	// if (log_to_dmesg_param != ALWAYS)
	// 	s->klog_sink.last_successful_next = (u32)sfile->index;
	 s->klog_sink.last_successful_next = (u32)sfile->index;
	if (log->put != (u32)sfile->index) {
		/* data ready to read */
		return EPOLLIN | EPOLLRDNORM;
	} else if (trusty_get_panic_status(s->trusty_dev)) {
		pr_debug("poll error because of trusty panic\n");
		return EPOLLERR | EPOLLHUP;
	}

	/* no data available, go to sleep */
	return 0;
}

static int trusty_log_sfile_dev_release(struct inode *inode, struct file *filp)
{
	struct seq_file *sfile;
	struct trusty_log_sfile *ls;
	struct trusty_log_state *s;

	sfile = filp->private_data;
	ls = sfile->private;
	s = container_of(ls, struct trusty_log_state, log_sfile);

	if (trusty_get_panic_status(s->trusty_dev))
		trusty_panic_wait_do_completion(s->trusty_dev);

	return seq_release(inode, filp);
}

static const struct file_operations log_sfile_dev_operations = {
	.owner = THIS_MODULE,
	.open = trusty_log_sfile_dev_open,
	.poll = trusty_log_sfile_dev_poll,
	.read = seq_read,
	.release = trusty_log_sfile_dev_release,
};

static int trusty_log_sfile_register(struct trusty_log_state *s)
{
	int ret;
	struct trusty_log_sfile *ls = &s->log_sfile;

	if (WARN_ON(!ls))
		return -EINVAL;

	snprintf(ls->device_name, sizeof(ls->device_name),
		"trusty-log%d", s->dev->id);
	ls->misc.minor = MISC_DYNAMIC_MINOR;
	ls->misc.name = ls->device_name;
	ls->misc.fops = &log_sfile_dev_operations;

	ret = misc_register(&ls->misc);
	if (ret) {
		dev_err(s->dev,
			"log_sfile error while doing misc_register ret=%d\n",
			ret);
		return ret;
	}
	dev_info(s->dev, "/dev/%s registered\n",
		ls->device_name);
	return 0;
}

static void trusty_log_sfile_unregister(struct trusty_log_state *s)
{
	struct trusty_log_sfile *ls = &s->log_sfile;

	misc_deregister(&ls->misc);
	if (s->dev) {
		dev_info(s->dev, "/dev/%s unregistered\n",
			 ls->misc.name);
	}
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

	if (result == TRUSTY_LOG_API_VERSION)
		return true;

	pr_info("trusty-log unsupported api version: %d, supported: %d\n",
			result, TRUSTY_LOG_API_VERSION);
	return false;
}

static ssize_t trusty_loglevel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct trusty_log_state *log_state = NULL;
	unsigned long loglevel;
	int result;


	result = kstrtoul(buf, 0, &loglevel);
	if (result) {
		pr_info("%s is not in hex or decimal form.\n", buf);
		return -EINVAL;
	}

	log_state = platform_get_drvdata(to_platform_device(dev));
	result = trusty_std_call32(log_state->trusty_dev,
	SMC_SC_SYSCTL_SET_LOGLEVEL, loglevel, 0, 0);
	if (result == SM_ERR_UNDEFINED_SMC) {
		pr_info("SMC_SC_SYSCTL_SET_CONSOLE not supported on secure side.\n");
		return -EFAULT;
	}

	return strnlen(buf, count);
}


static ssize_t trusty_loglevel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct trusty_log_state *log_state = NULL;
	int result;

	log_state = platform_get_drvdata(to_platform_device(dev));
	result = trusty_std_call32(log_state->trusty_dev,
		SMC_SC_SYSCTL_GET_LOGLEVEL, 0, 0, 0);
	if (result == SM_ERR_UNDEFINED_SMC) {
		pr_info("SMC_SC_SYSCTL_GET_CONSOLE not supported on secure side.\n");
		return -EFAULT;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static DEVICE_ATTR(trusty_loglevel, 0660,
		   trusty_loglevel_show, trusty_loglevel_store);


static ssize_t trusty_console_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct trusty_log_state *log_state = NULL;
	int result;

	log_state = platform_get_drvdata(to_platform_device(dev));
	result = trusty_std_call32(log_state->trusty_dev,
		SMC_SC_SYSCTL_GET_CONSOLE, 0, 0, 0);
	if (result == SM_ERR_UNDEFINED_SMC) {
		pr_info("SMC_SC_SYSCTL_GET_CONSOLE not supported on secure side.\n");
		return -EFAULT;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", result);
}

static ssize_t trusty_console_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct trusty_log_state *log_state = NULL;
	unsigned long console;
	int result;

	result = kstrtoul(buf, 0, &console);
	if (result) {
		pr_info("%s is not in hex or decimal form.\n", buf);
		return -EINVAL;
	}

	log_state = platform_get_drvdata(to_platform_device(dev));
	result = trusty_std_call32(log_state->trusty_dev,
				   SMC_SC_SYSCTL_SET_CONSOLE, console, 0, 0);

	if (result == SM_ERR_UNDEFINED_SMC) {
		pr_info("SMC_SC_SYSCTL_SET_CONSOLE not supported on secure side.\n");
		return -EFAULT;
	}

	return strnlen(buf, count);

}
static DEVICE_ATTR(trusty_console, 0660,
		   trusty_console_show, trusty_console_store);


static void trusty_log_free_pages(struct page **pages, size_t num_pages,
				  void *va)
{
	size_t n = num_pages;

	if (va != NULL)
		vm_unmap_ram(va, num_pages);

	while (n--)
		__free_page(pages[n]);
	kfree(pages);
}

static struct page **trusty_log_alloc_pages(size_t size, void **va)
{
	struct page *page, **pages;
	void *addr;
	int i, n;
	int result = 0;

	if (size == 0 || size > (TRUSTY_LOG_MAX_SIZE >> PAGE_SHIFT))
		return ERR_PTR(-EINVAL);

	n = size;
	pages = kmalloc_array(n, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < n; i++) {
		page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM);
		if (!page) {
			result = -ENOMEM;
			goto err;
		}
		pages[i] = page;
	}

	//addr = vm_map_ram(pages, n, -1, PAGE_KERNEL);
	addr = vm_map_ram(pages, n, -1);
	if (!addr) {
		result = -ENOMEM;
		goto err;
	}
	*va = addr;

	return pages;

err:
	trusty_log_free_pages(pages, i, NULL);
	return ERR_PTR(result);
}

static void *trusty_log_register_buf(struct device *trusty_dev,
				     struct page **pages, size_t num_pages)
{
	uint64_t *pfn_list;
	u32 high_4bytes;
	phys_addr_t pa;
	int n = num_pages;
	int result = 0;

	if (n * sizeof(uint64_t) > PAGE_SIZE)
		return ERR_PTR(-EINVAL);

	pfn_list = (uint64_t *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
	if (!pfn_list)
		return ERR_PTR(-ENOMEM);

	while (n--)
		pfn_list[n] = page_to_pfn(pages[n]);

	pa = virt_to_phys(pfn_list);
#ifdef CONFIG_PHYS_ADDR_T_64BIT
	high_4bytes = (u32)(pa >> 32);
#else
	high_4bytes = 0;
#endif
	result = trusty_std_call32(trusty_dev, SMC_SC_SHARED_LOG_ADD,
				   (u32)pa, high_4bytes, num_pages);
	if (result < 0) {
		pr_err("Trusty std call SMC_SC_SHARED_LOG_ADD failed(%d)\n",
			result);
		free_page((unsigned long)pfn_list);
		return ERR_PTR(result);
	}

	return pfn_list;
}

static int trusty_log_unregister_buf(struct device *trusty_dev, void *buf)
{
	int result;
	u32 high_4bytes;
	phys_addr_t pa = virt_to_phys(buf);

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	high_4bytes = (u32)(pa >> 32);
#else
	high_4bytes = 0;
#endif
	result = trusty_std_call32(trusty_dev, SMC_SC_SHARED_LOG_RM,
				   (u32)pa, high_4bytes, 0);
	if (result) {
		pr_err("Trusty std call SMC_SC_SHARED_LOG_RM failed(%d)\n",
		       result);
		return result;
	}

	free_page((unsigned long)buf);

	return 0;
}

#define trusty_log_dump_lock(s) ({ s->dumpable = 0; })
#define trusty_log_dump_unlock(s) ({ s->dumpable = 1; })

static ssize_t trusty_logsize_store(struct device *dev,
		struct device_attribute  *attr, const char *buf, size_t count)
{
	struct trusty_log_state *s = NULL;
	struct page **pages = NULL;
	void *log_buf, *pfn_list;
	size_t size, n;
	int result;

	result = kstrtoul(buf, 0, (unsigned long *)&size);
	if (result) {
		pr_err("Invalid value argument(%s)!\n", buf);
		return -EINVAL;
	}

	s = platform_get_drvdata(to_platform_device(dev));

	/* Disable log dumpping util log buffer resizing is done */
	trusty_log_dump_lock(s);

	n = size >> PAGE_SHIFT;
	/*
	 * Allocate and register new buffer pages. Doing this before free and
	 * unregister the old buffer is for easily rollback in case of
	 * allocation/register failure
	 */
	pages = trusty_log_alloc_pages(n, &log_buf);
	if (IS_ERR(pages)) {
		pr_err("Trusty log allocate buffer error(%ld)\n",
		       PTR_ERR(pages));
		count = PTR_ERR(pages);
		goto _exit;
	}

	pfn_list = trusty_log_register_buf(s->trusty_dev,
				pages, n);
	if (IS_ERR(pfn_list)) {
		pr_err("Trusty log register buffer failed(%ld)\n",
		       PTR_ERR(pfn_list));
		count = PTR_ERR(pfn_list);
		goto _exit1;
	}

	/* Unregister log buf and free buffer pages */
	result = trusty_log_unregister_buf(s->trusty_dev, s->pfn_list);
	if (result) {
		pr_err("Trusty log unregister buffer error(%d)\n", result);
		count = result;
		goto _exit1;
	}
	trusty_log_free_pages(s->pages, s->num_pages, s->log);

	s->pages = pages;
	s->num_pages = n;
	s->pfn_list = pfn_list;
	s->log = log_buf;
	s->get = 0;
	s->klog_sink.get = 0;
_exit:
	trusty_log_dump_unlock(s);
	return count;
_exit1:
	trusty_log_free_pages(pages, n, log_buf);
	trusty_log_dump_unlock(s);
	return count;

}

static ssize_t trusty_logsize_show(struct device *dev,
		struct device_attribute  *attr, char *buf)
{
	struct trusty_log_state *s = NULL;

	s = platform_get_drvdata(to_platform_device(dev));

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", s->num_pages << PAGE_SHIFT);
}

static DEVICE_ATTR(trusty_logsize, 0660,
		   trusty_logsize_show, trusty_logsize_store);

static struct attribute *trusty_log_attrs[] = {
	&dev_attr_trusty_logsize.attr,
	&dev_attr_trusty_loglevel.attr,
	&dev_attr_trusty_console.attr,
	NULL
};

static const struct attribute_group trusty_log_attr_group = {
	.attrs = trusty_log_attrs,
};

static int trusty_log_probe(struct platform_device *pdev)
{
	struct trusty_log_state *s;
	struct page **pages;
	void *log_buf, *pfn_list;
	size_t num_pages;
	int result;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (!trusty_supports_logging(pdev->dev.parent))
		return -ENXIO;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		result = -ENOMEM;
		goto error_alloc_state;
	}

	spin_lock_init(&s->lock);
	s->dev = &pdev->dev;
	s->trusty_dev = s->dev->parent;
	s->get = 0;
	s->klog_sink.get = 0;

	num_pages = TRUSTY_LOG_DEFAULT_SIZE >> PAGE_SHIFT;
	pages = trusty_log_alloc_pages(num_pages, &log_buf);
	if (IS_ERR(pages)) {
		result = PTR_ERR(pages);
		goto error_alloc_pages;
	}

	pfn_list = trusty_log_register_buf(s->trusty_dev, pages, num_pages);
	if (IS_ERR(pfn_list)) {
		dev_err(&pdev->dev, "register failed(%ld)\n",
			PTR_ERR(pfn_list));
		result = PTR_ERR(pfn_list);
		goto error_register_buf;
	}

	s->pages = pages;
	s->num_pages = num_pages;
	s->pfn_list = pfn_list;
	s->log = log_buf;
	s->get = 0;
	s->klog_sink.get = 0;
	s->dumpable = 1;

	init_waitqueue_head(&s->poll_waiters);
	spin_lock_init(&s->wake_up_lock);

	s->call_notifier.notifier_call = trusty_log_call_notify;
	result = trusty_call_notifier_register(s->trusty_dev,
					       &s->call_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register trusty call notifier\n");
		goto error_call_notifier;
	}

	s->panic_notifier.notifier_call = trusty_log_panic_notify;
	s->panic_notifier.priority = INT_MAX;
	result = atomic_notifier_chain_register(&panic_notifier_list,
						&s->panic_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register panic notifier\n");
		goto error_panic_notifier;
	}

	result = trusty_log_sfile_register(s);
	if (result < 0) {
		dev_err(&pdev->dev, "failed to register log_sfile\n");
		goto error_log_sfile;
	}

	platform_set_drvdata(pdev, s);

	result = sysfs_create_group(&pdev->dev.kobj, &trusty_log_attr_group);
	if (result)
		dev_err(&pdev->dev, "Failed to create debug files\n");

	return 0;

error_log_sfile:
	atomic_notifier_chain_unregister(&panic_notifier_list, &s->panic_notifier);
error_panic_notifier:
	trusty_call_notifier_unregister(s->trusty_dev, &s->call_notifier);
error_call_notifier:
	trusty_log_unregister_buf(s->trusty_dev, pfn_list);
error_register_buf:
	trusty_log_free_pages(pages, num_pages, log_buf);
error_alloc_pages:
	kfree(s);
error_alloc_state:
	return result;
}

static int trusty_log_remove(struct platform_device *pdev)
{
	struct trusty_log_state *s = platform_get_drvdata(pdev);

	trusty_log_sfile_unregister(s);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &s->panic_notifier);
	trusty_call_notifier_unregister(s->trusty_dev, &s->call_notifier);

	trusty_log_unregister_buf(s->trusty_dev, s->pfn_list);

	trusty_log_free_pages(s->pages, s->num_pages, s->log);

	kfree(s);

	sysfs_remove_group(&pdev->dev.kobj, &trusty_log_attr_group);

	return 0;
}

static const struct of_device_id trusty_test_of_match[] = {
	{ .compatible = "sprd,trusty-log-v1", },
	{},
};

struct platform_driver trusty_log_driver = {
	.probe = trusty_log_probe,
	.remove = trusty_log_remove,
	.driver = {
		.name = "sprd-trusty-log",
		.owner = THIS_MODULE,
		.of_match_table = trusty_test_of_match,
	},
};

module_platform_driver(trusty_log_driver);


MODULE_DESCRIPTION("Sprd trusty log driver");
MODULE_LICENSE("GPL v2");
