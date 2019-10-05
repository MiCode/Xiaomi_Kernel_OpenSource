/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mnoc_drv.h"
#include "mnoc_hw.h"
#include "mnoc_qos.h"

static unsigned int mnoc_addr_phy;


static int mnoc_reg_rw_proc_show(struct seq_file *m, void *v)
{
	void *addr = 0;

	if (mnoc_addr_phy < APU_NOC_TOP_ADDR ||
		mnoc_addr_phy >= (APU_NOC_TOP_ADDR + APU_NOC_TOP_RANGE)) {
		LOG_DEBUG("Reg[%08X] not in mnoc driver reg map\n",
			mnoc_addr_phy);
	} else {
		addr = (void *) ((uintptr_t) mnoc_base +
					(mnoc_addr_phy - APU_NOC_TOP_ADDR));
		seq_printf(m, "Reg[%08X] = 0x%08X\n",
			mnoc_addr_phy, mnoc_read(addr));
	}

	return 0;
}

static ssize_t mnoc_reg_rw_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	void *addr = 0;
	unsigned long flags;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int mnoc_value = 0;
	unsigned char mnoc_rw[5] = { 0, 0, 0, 0, 0};

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%1s %x %x", mnoc_rw, &mnoc_addr_phy,
		&mnoc_value) == 3) {
		if (mnoc_addr_phy < APU_NOC_TOP_ADDR ||
			mnoc_addr_phy >=
			(APU_NOC_TOP_ADDR + APU_NOC_TOP_RANGE)) {
			LOG_DEBUG("Reg[%08X] not in mnoc driver reg map\n",
				mnoc_addr_phy);
		} else {
			addr = (void *) ((uintptr_t) mnoc_base +
					(mnoc_addr_phy - APU_NOC_TOP_ADDR));
			spin_lock_irqsave(&mnoc_spinlock, flags);
			/* w format or 'w', addr, value */
			mnoc_write(addr, mnoc_value);
			spin_unlock_irqrestore(&mnoc_spinlock, flags);
			LOG_DEBUG("Read back, Reg[%08X] = 0x%08X\n",
					mnoc_addr_phy, mnoc_read(addr));
		}
	} else if (sscanf(buf, "%1s %x", mnoc_rw, &mnoc_addr_phy) == 2) {
		if (mnoc_addr_phy < APU_NOC_TOP_ADDR ||
			mnoc_addr_phy >=
			(APU_NOC_TOP_ADDR + APU_NOC_TOP_RANGE)) {
			LOG_DEBUG("Reg[%08X] not in mnoc driver reg map\n",
				mnoc_addr_phy);
		} else {
			addr = (void *) ((uintptr_t) mnoc_base +
					(mnoc_addr_phy - APU_NOC_TOP_ADDR));
			LOG_DEBUG("Read back, aReg[%08X] = 0x%08X\n",
					mnoc_addr_phy, mnoc_read(addr));
		}
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static ssize_t mnoc_cmd_qos_start_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	uint64_t cmd_id, sub_cmd_id;
	unsigned int core;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%d %d %d", &cmd_id, &sub_cmd_id, &core) == 3)
		apu_cmd_qos_start(cmd_id, sub_cmd_id, core);

out:
	free_page((unsigned long)buf);
	return count;
}

static ssize_t mnoc_cmd_qos_end_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	uint64_t cmd_id, sub_cmd_id;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%d %d %d", &cmd_id, &sub_cmd_id) == 2)
		apu_cmd_qos_end(cmd_id, sub_cmd_id);

out:
	free_page((unsigned long)buf);
	return count;
}

static ssize_t mnoc_cmd_qos_suspend_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	uint64_t cmd_id, sub_cmd_id;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%d %d %d", &cmd_id, &sub_cmd_id) == 2)
		apu_cmd_qos_suspend(cmd_id, sub_cmd_id);

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_cmd_qos_dump_proc_show(struct seq_file *m, void *v)
{

	seq_puts(m, "Print cmd_qos list to APUART\n");
	print_cmd_qos_list(m);

	return 0;
}

static int mnoc_cmd_qos_start_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int mnoc_cmd_qos_end_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int mnoc_cmd_qos_suspend_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(mnoc_reg_rw);
PROC_FOPS_RW(mnoc_cmd_qos_start);
PROC_FOPS_RW(mnoc_cmd_qos_end);
PROC_FOPS_RW(mnoc_cmd_qos_suspend);
PROC_FOPS_RO(mnoc_cmd_qos_dump);

struct pentry {
	const char *name;
	const struct file_operations *fops;
};

struct pentry mnoc_entries[] = {
	PROC_ENTRY(mnoc_reg_rw),
	PROC_ENTRY(mnoc_cmd_qos_start),
	PROC_ENTRY(mnoc_cmd_qos_end),
	PROC_ENTRY(mnoc_cmd_qos_suspend),
	PROC_ENTRY(mnoc_cmd_qos_dump),
};

struct proc_dir_entry *mnoc_dir;

int create_procfs(void)
{
	int i;

	LOG_DEBUG("+\n");

	mnoc_dir = proc_mkdir(APUSYS_MNOC_DEV_NAME, NULL);
	if (!mnoc_dir) {
		LOG_ERR("mkdir /proc/%s failed\n", APUSYS_MNOC_DEV_NAME);
		return -1;
	}

	/* S_IRUGO | S_IWUSR | S_IWGRP = 0x1B4 */
	for (i = 0; i < ARRAY_SIZE(mnoc_entries); i++) {
		if (!proc_create(mnoc_entries[i].name,
			0x1B4,
			mnoc_dir,
			mnoc_entries[i].fops)) {
			LOG_ERR("create /proc/%s/%s failed\n",
				APUSYS_MNOC_DEV_NAME, mnoc_entries[i].name);
			return -3;
		}
	}

	LOG_DEBUG("-\n");

	return 0;
}

void remove_procfs(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mnoc_entries); i++)
		remove_proc_entry(mnoc_entries[i].name, mnoc_dir);
	remove_proc_entry(APUSYS_MNOC_DEV_NAME, NULL);

	LOG_DEBUG("proc removed\n");
}

