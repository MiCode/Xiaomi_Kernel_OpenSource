#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>
#include "prof_ctl.h"
#include <linux/module.h>
#include <linux/pid.h>

#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/aee.h>
#include <linux/stacktrace.h>

#include <mach/mt_storage_logger.h>
#include <linux/ftrace.h>
#include <linux/debug_locks.h>
#include <linux/printk.h>

/* Some utility macro*/
#define SEQ_printf(m, x...)	    \
 do {			    \
    if (m)		    \
	seq_printf(m, x);	\
    else		    \
	pr_err(x);	    \
 } while (0)

#define MT_DEBUG_ENTRY(name) \
static int mt_##name##_show(struct seq_file *m, void *v);\
static ssize_t mt_##name##_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data);\
static int mt_##name##_open(struct inode *inode, struct file *file) \
{ \
    return single_open(file, mt_##name##_show, inode->i_private); \
} \
\
static const struct file_operations mt_##name##_fops = { \
    .open = mt_##name##_open, \
    .write = mt_##name##_write,\
    .read = seq_read, \
    .llseek = seq_lseek, \
    .release = single_release, \
};\
void mt_##name##_switch(int on);

/*
 * Ease the printing of nsec fields:
 */
/*
static long long nsec_high(unsigned long long nsec)
{
    if ((long long)nsec < 0) {
	nsec = -nsec;
	do_div(nsec, 1000000);
	return -nsec;
    }
    do_div(nsec, 1000000);

    return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
    if ((long long)nsec < 0)
	nsec = -nsec;

    return do_div(nsec, 1000000);
}
#define SPLIT_NS(x) nsec_high(x), nsec_low(x)
*/
/*  */
/* //////////////////////////////////////////////////////// */
/* --------------------------------------------------- */
/* Real work */
/* --------------------------------------------------- */
/*                     Define Proc entry               */
/* --------------------------------------------------- */
MT_DEBUG_ENTRY(debug_config);
static int mt_debug_config_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "=== mt debug config ===\n");

	return 0;
}

static ssize_t mt_debug_config_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int val;
	int ret;
	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, (unsigned long *)&val);
	if (val == 0) {
		pr_err("MTK debug disable:\n");
		/* printk("1.ftrace\n"); */
/* mt_ftrace_enable_disable(0); */
//		pr_err("3.storage_logger\n");
	//	storage_logger_switch(0);
		pr_err("5.prove locking\n");
		debug_locks_off();
		pr_err("6.Disable console log\n");
		console_silent();
	}
	if (ret < 0)
		return ret;
	pr_err(" %d\n", val);
	return cnt;
}

static int __init init_mt_debug_config(void)
{
	struct proc_dir_entry *pe;
	pe = proc_create("mtconfig", 0664, NULL, &mt_debug_config_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}

__initcall(init_mt_debug_config);
