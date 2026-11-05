// SPDX-License-Identifier: GPL-2.0+
/*
 * Drivers for printk enhance.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define pr_fmt(fmt) "printk_enhance: "fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/pid_namespace.h>
#include <trace/hooks/printk.h>

/*
 * [31:30]: ctx
 * [29:24]: cpu
 * [23:0]: tid
 */
#define PRINTK_CALLER_CTX_SHIFT 30
#define PRINTK_CALLER_CPU_SHIFT 24
#define PRINTK_CALLER_TID_SHIFT 0

#define PRINTK_CALLER_CTX_MASK 0xc0000000
#define PRINTK_CALLER_CPU_MASK 0x3f000000
#define PRINTK_CALLER_TID_MASK 0x00ffffff

enum printk_caller_ctx {
	printk_ctx_task,
	printk_ctx_softirq,
	printk_ctx_hardirq,
	printk_ctx_nmi,
};

char printk_caller_ctx_str[] = {'T', 'S', 'H', 'N'};
static bool enabled = false;
static struct proc_dir_entry *pentry;

static int printk_enhance_enable_show(struct seq_file *seq_filp, void *data)
{
	seq_printf(seq_filp, "%d\n", enabled);
	return 0;
}

static ssize_t printk_enhance_enable_write(struct file *file,
                       const char __user *buf, size_t len, loff_t *ppos)
{
#define DATA_LEN    32
	char buffer[DATA_LEN] = {0};
	int ret = 0;
	int value = 0;

	len = (len > DATA_LEN) ? DATA_LEN : len;
	if (copy_from_user(buffer, buf, len)) {
		return -EFAULT;
	}
	buffer[len] = '\0';

	ret = kstrtoint(strstrip(buffer), len, &value);
	if (ret) {
		return ret;
	}
	WRITE_ONCE(enabled, value);

	pr_err("enabled has been set to %d\n", enabled);

	*ppos += len;

	return len;
}

int printk_enhance_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, printk_enhance_enable_show, file);
}

static const struct proc_ops printk_enhance_enable_proc_fops = {
	.proc_open      = printk_enhance_enable_open,
	.proc_read      = seq_read,
	.proc_write     = printk_enhance_enable_write,
	.proc_lseek     = seq_lseek,
	.proc_release   = single_release,
};

static int printk_enhance_create_procfs(void)
{
	struct proc_dir_entry *entry;

	pentry = proc_mkdir("printk_enhance", NULL);
	if (!pentry)
		return -ENOMEM;

	entry = proc_create("enabled", S_IWUGO | S_IRUGO, pentry, &printk_enhance_enable_proc_fops);
	if (!entry) {
		remove_proc_entry("printk_enhance", NULL);
		return -ENOMEM;
	}

	return 0;
}

static void printk_enhance_destory_procfs(void)
{
	remove_proc_entry("enabled", pentry);
	remove_proc_entry("printk_enhance", NULL);
}

static enum printk_caller_ctx get_printk_ctx(void)
{
	if (in_nmi())
		return printk_ctx_nmi;

	if (in_irq())
		return printk_ctx_hardirq;

	if (in_softirq())
		return printk_ctx_softirq;

	return printk_ctx_task;
}

static void enhance_caller_id(void *unused, u32 *caller_id)
{
	if (likely(!enabled))
		return;

	*caller_id = ((get_printk_ctx() << PRINTK_CALLER_CTX_SHIFT) & PRINTK_CALLER_CTX_MASK) + \
			((smp_processor_id() << PRINTK_CALLER_CPU_SHIFT) & PRINTK_CALLER_CPU_MASK) + \
			((task_pid_nr(current) << PRINTK_CALLER_TID_SHIFT) & PRINTK_CALLER_TID_MASK);
}

static void enhance_caller(void *unused, char *caller, size_t size, u32 id, int *ret)
{
	if (likely(!enabled))
		return;

	if (snprintf(caller, size, "%c,T%u,C%u",
		printk_caller_ctx_str[(id & PRINTK_CALLER_CTX_MASK) >> PRINTK_CALLER_CTX_SHIFT],
		(id & PRINTK_CALLER_TID_MASK) >> PRINTK_CALLER_TID_SHIFT,
		(id & PRINTK_CALLER_CPU_MASK) >> PRINTK_CALLER_CPU_SHIFT) > 0)
		*ret = 1;
}

static int hook_printk_enhance_events(void)
{
	int ret;

	ret = register_trace_android_vh_printk_caller_id(enhance_caller_id, NULL);
	if (ret) {
		pr_err("%s failed\n", __func__);
		return ret;
	}

	ret = register_trace_android_vh_printk_caller(enhance_caller, NULL);
	if (ret) {
		pr_err("%s failed\n", __func__);
		unregister_trace_android_vh_printk_caller_id(enhance_caller_id, NULL);
		return ret;
	}

	return 0;
}

static void unhook_printk_enhance_events(void)
{
	unregister_trace_android_vh_printk_caller(enhance_caller, NULL);
	unregister_trace_android_vh_printk_caller_id(enhance_caller_id, NULL);
}

static int __init printk_enhance_init(void)
{
	int ret;

	ret = printk_enhance_create_procfs();
	if (ret < 0)
		return ret;

	ret = hook_printk_enhance_events();
	if (ret < 0)
		goto hook_events_fail;

	pr_err("%s succeed\n", __func__);
	return 0;

hook_events_fail:
	printk_enhance_destory_procfs();

	return ret;
}

static void __exit printk_enhance_exit(void)
{
	unhook_printk_enhance_events();
	printk_enhance_destory_procfs();

	pr_err("%s\n", __func__);
}

module_init(printk_enhance_init);
module_exit(printk_enhance_exit);
MODULE_AUTHOR("gaoxiang17 <gaoxiang17@xiaomi.com>");
MODULE_DESCRIPTION("Register Printk Enhance driver");
MODULE_LICENSE("GPL v2");
