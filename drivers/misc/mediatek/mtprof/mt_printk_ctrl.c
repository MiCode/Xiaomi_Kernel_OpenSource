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
static ssize_t mt_##name##_write(struct file *filp, const char *ubuf, ssize_t cnt, loff_t *data);\
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
MT_DEBUG_ENTRY(printk_ctrl);
int mt_need_uart_console = 0;
extern void mt_enable_uart(void);	/* printk.c */
extern void mt_disable_uart(void);	/* printk.c */
extern bool printk_disable_uart;
static int mt_printk_ctrl_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "=== mt printk controller ===\n");
	SEQ_printf(m, "mt_need_uart_console:%d, printk_disable_uart:%d\n", mt_need_uart_console,
		   printk_disable_uart);

	return 0;
}

static ssize_t mt_printk_ctrl_write(struct file *filp, const char *ubuf, ssize_t cnt, loff_t *data)
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
		mt_disable_uart();
	} else if (val == 1) {
		mt_need_uart_console = 1;
		mt_enable_uart();
		pr_err("need uart log\n");
	}
	if (ret < 0)
		return ret;
	pr_err(" %d\n", val);
	return cnt;
}

static int __init init_mt_printk_ctrl(void)
{
	struct proc_dir_entry *pe;
	mt_need_uart_console = 0;	/* defualt, no uart */
	pe = proc_create("mtprintk", 0664, NULL, &mt_printk_ctrl_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}

__initcall(init_mt_printk_ctrl);
