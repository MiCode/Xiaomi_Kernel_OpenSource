/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include "internal.h"

/* //////////////////////////////////////////////////////// */
/* --------------------------------------------------- */
/* Real work */
/* --------------------------------------------------- */
/*                     Define Proc entry               */
/* --------------------------------------------------- */
MT_DEBUG_ENTRY(printk_ctrl);
/* always enable uart printk */
int mt_need_uart_console;

static int mt_printk_ctrl_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "=== mt printk controller ===\n");
	SEQ_printf(m, "0:   printk uart disable\n");
	SEQ_printf(m, "1:   printk uart enable\n");
	SEQ_printf(m, "2:   printk too much disable\n");
	SEQ_printf(m, "3:   printk too much enable\n");
	SEQ_printf(m,
"xxx: printk too much detect count(xxx represents for a integer > 100)\n");
	SEQ_printf(m, "=== mt printk controller ===\n\n");
	SEQ_printf(m, "kernel log buffer len: %dKB\n", log_buf_len_get()/1024);
	SEQ_printf(m, "printk uart enable: %d\n", mt_get_uartlog_status());
	SEQ_printf(m, "printk too much enable: %d\n", get_logtoomuch_enable());
	SEQ_printf(m, "printk too much detect count: %d\n", get_detect_count());
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
	case 0:
		mt_need_uart_console = 0;
		mt_disable_uart();
		break;
	case 1:
		mt_need_uart_console = 1;
		mt_enable_uart();
		break;
	case 2:
		set_logtoomuch_enable(0);
		break;
	case 3:
		set_logtoomuch_enable(1);
		break;
	default:
		if (val > 100)
			set_detect_count(val);
		break;
	}
	return cnt;
}

static int __init init_mt_printk_ctrl(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("mtprintk", 0664, NULL, &mt_printk_ctrl_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}

device_initcall(init_mt_printk_ctrl);
