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
//#include <linux/module.h>
#include <linux/moduleparam.h>

#include "mt-plat/mtk_printk_ctrl.h"


/* //////////////////////////////////////////////////////// */
/* --------------------------------------------------- */
/* Real work */
/* --------------------------------------------------- */
/*                     Define Proc entry               */
/* --------------------------------------------------- */
extern unsigned long long sched_clock(void);

#ifdef CONFIG_PRINTK_MTK_UART_CONSOLE
/*
 * 0: uart printk enable
 * 1: uart printk disable
 * 2: uart printk always enable
 * 2 only set in lk phase by cmline
 */
int printk_disable_uart;

module_param_named(disable_uart, printk_disable_uart, int, 0644);

bool mt_get_uartlog_status(void)
{
	if (printk_disable_uart == 1)
		return false;
	else if ((printk_disable_uart == 0) || (printk_disable_uart == 2))
		return true;
	return true;
}

void set_uartlog_status(bool value)
{
#ifdef CONFIG_MTK_ENG_BUILD
	printk_disable_uart = value ? 0 : 1;
	pr_info("set uart log status %d.\n", value);
#endif
}

void mt_disable_uart(void)
{
	/* uart print not always enable */
	if (printk_disable_uart != 2)
		printk_disable_uart = 1;
}
void mt_enable_uart(void)
{
	printk_disable_uart = 0;
}
#endif

static int mt_printk_ctrl_show(struct seq_file *m, void *v)
{
	seq_puts(m, "=== mt printk controller ===\n");
#ifdef CONFIG_PRINTK_MTK_UART_CONSOLE
	seq_puts(m, "0:   printk uart disable\n");
	seq_puts(m, "1:   printk uart enable\n");
#endif

	seq_puts(m, "=== mt printk controller ===\n\n");
	seq_printf(m, "kernel log buffer len: %dKB\n", log_buf_len_get()/1024);
#ifdef CONFIG_PRINTK_MTK_UART_CONSOLE
	seq_printf(m, "printk uart enable: %d\n", mt_get_uartlog_status());
#endif

	return 0;
}



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
#ifdef CONFIG_PRINTK_MTK_UART_CONSOLE
	case 0:
		mt_disable_uart();
		break;
	case 1:
		mt_enable_uart();
		break;
#endif

	default:
		break;
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

