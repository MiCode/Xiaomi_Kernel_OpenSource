// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/moduleparam.h>

#include "mt-plat/mtk_printk_ctrl.h"


/* //////////////////////////////////////////////////////// */
/* --------------------------------------------------- */
/* Real work */
/* --------------------------------------------------- */
/*                     Define Proc entry               */
/* --------------------------------------------------- */

#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
/*
 * printk_ctrl:
 * 0: uart printk enable
 * 1: uart printk disable
 * 2: uart printk always enable
 * 2 only set in lk phase by cmline
 */

#ifdef CONFIG_MTK_ENG_BUILD
int printk_ctrl;
#else
int printk_ctrl = 1;
#endif

module_param_named(disable_uart, printk_ctrl, int, 0644);

bool mt_get_uartlog_status(void)
{
	if (printk_ctrl == 1)
		return false;
	else if ((printk_ctrl == 0) || (printk_ctrl == 2))
		return true;
	return true;
}

void mt_disable_uart(void)
{
	/* uart print not always enable */
	if (printk_ctrl != 2)
		printk_ctrl = 1;
}
EXPORT_SYMBOL_GPL(mt_disable_uart);

void mt_enable_uart(void)
{
	printk_ctrl = 0;
}
EXPORT_SYMBOL_GPL(mt_enable_uart);
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
"xxx: printk too much detect count(xxx represents for a integer > 100)\n");
#endif
	seq_puts(m, "=== mt printk controller ===\n\n");
	seq_printf(m, "kernel log buffer len: %dKB\n", log_buf_len_get()/1024);
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
	seq_printf(m, "printk uart enable: %d\n", mt_get_uartlog_status());
#endif
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	seq_printf(m, "printk too much enable: %d.\n", get_logtoomuch_enable());
	seq_printf(m, "printk too much detect count: %d\n", get_detect_count());
#endif
	return 0;
}

#ifdef CONFIG_LOG_TOO_MUCH_WARNING
void mt_print_much_log(void)
{
	unsigned long long t1 = 0;
	unsigned long long t2 = 0;
	unsigned long print_num = 0;

	t1 = sched_clock();
	pr_info("printk debug log: start time: %lld.\n", t1);

	for (;;) {
		t2 = sched_clock();
		if ((t2 - t1) / 1000000 > 10 * 1000)
			break;
		pr_info("printk debug log: the %ld line, time: %lld.\n",
			print_num++, t2);
		__delay(5);
	}
	pr_info("mt log total write %ld line in 10 second.\n", print_num);
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
		mt_disable_uart();
		break;
	case 1:
		mt_enable_uart();
		break;
#endif
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	case 2:
		set_logtoomuch_enable(0);
		break;
	case 3:
		set_logtoomuch_enable(1);
		break;
	case 4:
		mt_print_much_log();
		break;
	default:
		if (val > 100)
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

static const struct file_operations mt_printk_ctrl_fops = {
	.open = mt_printk_ctrl_open,
	.write = mt_printk_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init init_mt_printk_ctrl(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("mtprintk", 0664, NULL, &mt_printk_ctrl_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}

device_initcall(init_mt_printk_ctrl);

