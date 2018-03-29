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

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <aee.h>
#include <linux/timer.h>
/* #include <asm/system.h> */
#include <asm-generic/irq_regs.h>
/* #include <asm/mach/map.h> */
#include <sync_write.h>
/*#include <mach/irqs.h>*/
#include <asm/cacheflush.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/fb.h>
#include <linux/debugfs.h>
#include <m4u.h>
#include <mt_smi.h>

#include "smi_common.h"
#include "smi_reg.h"

#define SMI_LOG_TAG "smi"

bool smi_clk_always_on = 0;

static char debug_buffer[4096];

static void process_dbg_opt(const char *opt)
{
	unsigned long addr = 0;
	int ret = 0;
	char *buf = debug_buffer + strlen(debug_buffer);

	if (0 == strncmp(opt, "set_reg:", 8)) {
		unsigned long val = 0;

		char *p = (char *)opt + 8;

		ret = kstrtoul(p, 16, &addr);
		p++;

		ret = kstrtoul(p, 16, &val);

		SMIMSG("set register: 0x%lx = 0x%x\n", addr, (unsigned int)val);

		COM_WriteReg32(addr, val);
	} else if (0 == strncmp(opt, "get_reg:", 8)) {
		char *p = (char *)opt + 8;

		ret = kstrtoul(p, 16, &addr);

		SMIMSG("get register: 0x%lx = 0x%x\n", addr, COM_ReadReg32(addr));
	} else if (0 == strncmp(opt, "set_pa:", 7)) {
		unsigned long val = 0;
		unsigned long reg_va;

		char *p = (char *)opt + 7;

		ret = kstrtoul(p, 16, &addr);
		p += 11;

		ret = kstrtoul(p, 16, &val);

		SMIMSG("set register: 0x%lx = 0x%x\n", addr, (unsigned int)val);
		sprintf(buf, "set register: 0x%lx = 0x%x\n", addr, (unsigned int)val);
		reg_va = (unsigned long)ioremap_nocache(addr, 0x100);
		COM_WriteReg32(reg_va, val);
		iounmap((void *)reg_va);
	} else if (0 == strncmp(opt, "get_pa:", 7)) {
		char *p = (char *)opt + 7;
		unsigned long reg_va;

		ret = kstrtoul(p, 16, &addr);
		reg_va = (unsigned long)ioremap_nocache(addr, 0x100);
		SMIMSG("get register: pa 0x%lx = 0x%x\n", addr, COM_ReadReg32(reg_va));
		sprintf(buf, "get register: pa 0x%lx = 0x%x, va is 0x%lx\n",
			addr, COM_ReadReg32(reg_va), reg_va);

		iounmap((void *)reg_va);
	} else if (0 == strncmp(opt, "always_on_clk:", 14)) {
		unsigned long val = 0;
		char *p = (char *)opt + 14;

		ret = kstrtoul(p, 16, &val);
		if (val)
			smi_clk_always_on = 1;
		else
			smi_clk_always_on = 0;
	}

}


static void process_dbg_cmd(char *cmd)
{
	char *tok;
	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);

}


/* --------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* --------------------------------------------------------------------------- */

struct dentry *smi_dbgfs = NULL;


static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, strlen(debug_buffer));
}


static ssize_t debug_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;
	memset(debug_buffer, 0, sizeof(debug_buffer));

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}


static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};


void SMI_DBG_Init(void)
{
	smi_dbgfs = debugfs_create_file("smi", S_IFREG | S_IRUGO, NULL, (void *)0, &debug_fops);
	memset(debug_buffer, 0, sizeof(debug_buffer));
}


void SMI_DBG_Deinit(void)
{
	debugfs_remove(smi_dbgfs);
}
