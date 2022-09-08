// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/div64.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/moduleparam.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/kmsg_dump.h>
#include <uapi/linux/sched/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <trace/hooks/logbuf.h>
#include <printk/printk_ringbuffer.h>

#include "aee.h"

/*
 * printk_ctrl:
 * 0: uart printk disable
 * 1: uart printk enable
 * 2: uart printk always enable
 */

static struct proc_dir_entry *entry;

#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
static int printk_ctrl_disable;
module_param_named(disable_uart, printk_ctrl_disable, int, 0644);

bool mt_get_uartlog_status(void)
{
	if (printk_ctrl_disable == 1)
		return false;
	else if ((printk_ctrl_disable == 0) || (printk_ctrl_disable == 2))
		return true;
	return true;
}
EXPORT_SYMBOL_GPL(mt_get_uartlog_status);

/* 0:disable uart, 1:enable uart */
void update_uartlog_status(bool new_value, int value)
{
	struct console *bcon = NULL;

	if (new_value == false || printk_ctrl_disable == 2) {
		pr_info("use default valut %d to set uart status.\n", printk_ctrl_disable == 1 ? 0 : 1);
	} else if (value == 0) { /* disable uart log */
		printk_ctrl_disable = 1;
	} else if (value == 1) { /* enable uart log */
		printk_ctrl_disable = 0;
	} else {
		pr_info("invalid value %d, use default value %d.\n",
			value, printk_ctrl_disable == 1 ? 0 : 1);
	}

#if IS_ENABLED(CONFIG_MTK_PRINTK_DEBUG)
	set_printk_uart_status(!printk_ctrl_disable);
#endif

	if (printk_ctrl_disable == 1) {
		for_each_console(bcon) {
			pr_info("console name: %s, status 0x%x.\n", bcon->name, bcon->flags);
			if (!strncmp(bcon->name, "ttyS", 4)) {
				bcon->flags &= ~CON_ENABLED;
				return;
			}
		}
	} else {
		for_each_console(bcon) {
			pr_info("console name: %s. status 0x%x.\n", bcon->name, bcon->flags);
			if (!strncmp(bcon->name, "ttyS", 4)) {
				bcon->flags |= CON_ENABLED;
				return;
			}
		}
	}
}
EXPORT_SYMBOL_GPL(update_uartlog_status);

#else
bool mt_get_uartlog_status(void)
{
	return false;
}
EXPORT_SYMBOL_GPL(mt_get_uartlog_status);

void update_uartlog_status(bool new_value, int value)
{

}
EXPORT_SYMBOL_GPL(update_uartlog_status);
#endif


#ifdef CONFIG_LOG_TOO_MUCH_WARNING
static int detect_count = CONFIG_LOG_TOO_MUCH_DETECT_COUNT;
static bool logmuch_enable;
static int logmuch_exit;
static void *log_much;
static int log_much_len = 1 << (CONFIG_LOG_BUF_SHIFT + 1);
struct proc_dir_entry *logmuch_entry;
static u32 log_count;
static bool detect_count_after_effect_flag;
static int detect_count_after;
DECLARE_WAIT_QUEUE_HEAD(logmuch_thread_exit);
void mt_print_much_log(void)
{
	unsigned long long t1 = 0;
	unsigned long long t2 = 0;
	unsigned long print_num = 0;
	unsigned long long result = 0;

	t1 = sched_clock();
	pr_info("printk debug log: start time: %lld.\n", t1);

	for (;;) {
		t2 = sched_clock();
		result = t2 - t1;
		do_div(result, 1000000);
		if (result > 10 * 1000)
			break;
		pr_info("printk debug log: the %ld line, time: %lld.\n",
			print_num++, t2);
		__delay(5);
	}
	pr_info("mt log total write %ld line in 10 second.\n", print_num);
}

void set_logtoomuch_enable(void)
{
	logmuch_enable = true;
}
EXPORT_SYMBOL_GPL(set_logtoomuch_enable);

void set_logtoomuch_disable(void)
{
	logmuch_enable = false;
}
EXPORT_SYMBOL_GPL(set_logtoomuch_disable);

bool get_logtoomuch_status(void)
{
	return logmuch_enable;
}
EXPORT_SYMBOL_GPL(get_logtoomuch_status);

void set_detect_count(int val)
{
	pr_info("set log_much detect value %d.\n", val);
	if (val > 0) {
		if (val >= detect_count) {
			detect_count_after_effect_flag = false;
			detect_count = val;
		} else {
			detect_count_after_effect_flag = true;
			detect_count_after = val;
		}

	}
}
EXPORT_SYMBOL_GPL(set_detect_count);

int get_detect_count(void)
{
	pr_info("get log_much detect value %d,get log_much detect after value %d.\n",
		detect_count, detect_count_after);

	if (detect_count_after_effect_flag)
		return detect_count_after;
	else
		return detect_count;
}
EXPORT_SYMBOL_GPL(get_detect_count);

static int logmuch_dump_thread(void *arg)
{
	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};
	struct kmsg_dumper dumper = { .active = true };
	size_t len = 0;
	char aee_str[63] = {0};
	int add_len;
	u64 last_seq;
	unsigned long long old = 0;
	unsigned long long now = 0;
	unsigned long long period = 0;
	unsigned long long mod = 0;
#if IS_ENABLED(CONFIG_MTK_PRINTK_DEBUG)
	unsigned long long printk_irq_t0 = 0;
	unsigned long long printk_irq_t1 = 0;
	int wake_up_type = 0;
#endif
	sched_setscheduler(current, SCHED_FIFO, &param);
	log_much = kmalloc(log_much_len, GFP_KERNEL);
	if (log_much == NULL) {
		proc_remove(logmuch_entry);
		pr_notice("printk: failed to create 0x%x log_much memory.\n", log_much_len);
		return 1;
	}

	/* don't detect boot up phase*/
	wait_event_interruptible_timeout(logmuch_thread_exit, logmuch_exit == 1, 60 * HZ);

	while (1) {
		if (log_much == NULL)
			break;
		old = sched_clock();
		kmsg_dump_rewind(&dumper);
		last_seq = dumper.next_seq;
		if ((detect_count_after_effect_flag == true) && (detect_count_after > 0)) {
			detect_count_after_effect_flag = false;
			detect_count = detect_count_after;
		}
		wait_event_interruptible_timeout(logmuch_thread_exit, logmuch_exit == 1, 5 * HZ);
		if (logmuch_enable == false || detect_count <= 0)
			continue;

		now = sched_clock();
		kmsg_dump_rewind(&dumper);
		log_count = dumper.next_seq - last_seq;
		period = now - old;
		do_div(period, 1000000);
		do_div(period, 100);
		if (period * detect_count < log_count * 10) {
			pr_info("log_much detect.\n");
			if (log_much == NULL)
				break;
			memset((char *)log_much, 0, log_much_len);
			kmsg_dump_rewind(&dumper);
			dumper.cur_seq = last_seq;
			kmsg_dump_get_buffer(&dumper, true, (char *)log_much,
								 log_much_len, &len);
			memset(aee_str, 0, 63);
			mod = do_div(period, 10);
			add_len = scnprintf(aee_str, 63,
					"Printk too much: >%d L/s, L: %llu, S: %llu.%03lu\n",
					detect_count, log_count, period, mod);
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_PRINTK_TOO_MUCH | DB_OPT_DUMMY_DUMP,
					aee_str, "Need to shrink kernel log");
			pr_info("log_much detect %d log.\n", len);
			wait_event_interruptible_timeout(logmuch_thread_exit,
				logmuch_exit == 1, 60 * CONFIG_LOG_TOO_MUCH_DETECT_GAP * HZ);
		}
#if IS_ENABLED(CONFIG_MTK_PRINTK_DEBUG)
		wake_up_type = get_printk_wake_up_time(&printk_irq_t0, &printk_irq_t1);
		if (printk_irq_t0 != 0 || printk_irq_t1 != 0)
			pr_info("printk irq %d, %llu, %llu.\n", wake_up_type,
				printk_irq_t0, printk_irq_t1);
#endif
	}
	pr_notice("[log_much] logmuch_Detect dump thread exit.\n");
	logmuch_exit = 0;
	return 0;
}

static int log_much_show(struct seq_file *m, void *v)
{
	if (log_much == NULL) {
		seq_puts(m, "log_much buff is null.\n");
		return 0;
	}
	seq_write(m, (char *)log_much, log_much_len);
	return 0;
}

static int log_much_open(struct inode *inode, struct file *file)
{
	return single_open(file, log_much_show, inode->i_private);
}

static const struct proc_ops log_much_ops = {
	.proc_open = log_much_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
void set_detect_count(int val)
{

}
EXPORT_SYMBOL_GPL(set_detect_count);

int get_detect_count(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(get_detect_count);
void set_logtoomuch_enable(void)
{

}
EXPORT_SYMBOL_GPL(set_logtoomuch_enable);

void set_logtoomuch_disable(void)
{

}
EXPORT_SYMBOL_GPL(set_logtoomuch_disable);

bool get_logtoomuch_status(void)
{
	return false;
}
EXPORT_SYMBOL_GPL(get_logtoomuch_status);
#endif



static int mt_printk_ctrl_show(struct seq_file *m, void *v)
{
	seq_puts(m, "=== mt printk controller ===\n");
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
	seq_puts(m, "0:   printk uart disable\n");
	seq_puts(m, "1:   printk uart enable\n");
#endif
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	seq_puts(m, "2:   printk too much disable\n");
	seq_puts(m, "3:   printk too much enable\n");
	seq_puts(m, "4:   printk too much log in 10 seconds.\n");
	seq_puts(m,
"xxx: printk too much detect count(xxx represents for a integer > 50)\n");
	seq_printf(m, "log_much detect %d Line, %d sieze.\n", log_count, log_much_len);
#endif
	seq_puts(m, "=== mt printk controller ===\n\n");
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
	seq_printf(m, "printk uart enable: %d\n", mt_get_uartlog_status());
#endif
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	seq_printf(m, "printk too much enable: %d.\n", logmuch_enable);
	seq_printf(m, "printk too much detect count: %d\n", detect_count);
#endif
	return 0;
}

#if !IS_ENABLED(CONFIG_MTK_PRINTK_DEBUG)
/*
 * register_trace_android_vh_printk_logbuf function, don't call printk/pr_xx to
 * printk log, or will into infinite loop
 */
#define CPU_INDEX (100000)
#define UART_INDEX (1000000)
static void mt_printk_logbuf(void *data, struct printk_ringbuffer *rb,
	struct printk_record *r)
{
	if (r->info->caller_id  & 0x80000000) {
		r->info->caller_id = ((r->info->caller_id & 0xFF) * CPU_INDEX)
			| task_pid_nr(current) | 0x80000000;
	} else {
		/* max pid 0x8000 -> 32768 */
		r->info->caller_id = r->info->caller_id + (raw_smp_processor_id() * CPU_INDEX);
	}
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
	if (printk_ctrl_disable != 1)
		r->info->caller_id = r->info->caller_id + UART_INDEX;
#endif
}
#endif

static ssize_t mt_printk_ctrl_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	switch (val) {
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
	case 0:
		update_uartlog_status(true, 0);
		break;
	case 1:
		update_uartlog_status(true, 1);
		break;
	case 5:
		printk_ctrl_disable = 2;
		update_uartlog_status(false, 0);
		break;
#endif
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	case 2:
		set_logtoomuch_disable();
		break;
	case 3:
		set_logtoomuch_enable();
		break;
	case 4:
		mt_print_much_log();
		break;
	default:
		if (val > 50)
			set_detect_count(val);
		break;
#else
	default:
		break;
#endif
	}
	return cnt;
}

/*** Seq operation of mtprof ****/
static int mt_printk_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_printk_ctrl_show, inode->i_private);
}

static const struct proc_ops mt_printk_ctrl_fops = {
	.proc_open = mt_printk_ctrl_open,
	.proc_write = mt_printk_ctrl_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init mt_printk_ctrl_init(void)
{
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	static struct task_struct *hd_thread;
#endif

	entry = proc_create("mtprintk", 0664, NULL, &mt_printk_ctrl_fops);
	if (!entry)
		return -ENOMEM;
#if !IS_ENABLED(CONFIG_MTK_PRINTK_DEBUG)
	register_trace_android_vh_logbuf(mt_printk_logbuf, NULL);
#endif

#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	logmuch_entry = proc_create("log_much", 0444, NULL, &log_much_ops);
	if (!logmuch_entry) {
		pr_notice("printk: failed to create proc log_much entry\n");
		return 1;
	}

	hd_thread = kthread_create(logmuch_dump_thread, NULL, "logmuch_detect");
	if (hd_thread)
		wake_up_process(hd_thread);
#endif
	return 0;
}


#ifdef MODULE
static void __exit mt_printk_ctrl_exit(void)
{
	if (entry)
		proc_remove(entry);

#if !IS_ENABLED(CONFIG_MTK_PRINTK_DEBUG)
	unregister_trace_android_vh_logbuf(mt_printk_logbuf, NULL);
#endif

#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	if (logmuch_entry)
		proc_remove(logmuch_entry);

	if (log_much) {
		kfree(log_much);
		log_much = NULL;
		logmuch_enable = false;
		logmuch_exit = 1;
		wake_up_interruptible(&logmuch_thread_exit);
		while (logmuch_exit == 1)
			ssleep(5);
	}
#endif
pr_notice("log_much exit.");

}

module_init(mt_printk_ctrl_init);
module_exit(mt_printk_ctrl_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Printk enhance");
MODULE_AUTHOR("MediaTek Inc.");
#else
device_initcall(mt_printk_ctrl_init);
#endif
