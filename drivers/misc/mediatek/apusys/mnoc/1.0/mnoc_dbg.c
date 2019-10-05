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
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mnoc_drv.h"
#include "mnoc_hw.h"
#include "mnoc_qos.h"
#include "mnoc_api.h"
#include "mnoc_pmu.h"

static unsigned int mnoc_addr_phy;

static int mnoc_reg_rw_show(struct seq_file *m, void *v)
{
	void *addr = 0;
	unsigned int val = 0;
	unsigned long flags;

	if (mnoc_addr_phy < APU_NOC_TOP_ADDR ||
		mnoc_addr_phy >= (APU_NOC_TOP_ADDR + APU_NOC_TOP_RANGE)) {
		LOG_DEBUG("Reg[%08X] not in mnoc driver reg map\n",
			mnoc_addr_phy);
	} else {
		addr = (void *) ((uintptr_t) mnoc_base +
					(mnoc_addr_phy - APU_NOC_TOP_ADDR));
		spin_lock_irqsave(&mnoc_spinlock, flags);
		if (mnoc_reg_valid)
			val = mnoc_read(addr);
		spin_unlock_irqrestore(&mnoc_spinlock, flags);
		seq_printf(m, "Reg[%08X] = 0x%08X\n",
			mnoc_addr_phy, val);
	}

	return 0;
}

static ssize_t mnoc_reg_rw_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	void *addr = 0;
	unsigned long flags;
	unsigned int val = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int mnoc_value = 0;
	unsigned char mnoc_rw[5] = {0, 0, 0, 0, 0};

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
			if (mnoc_reg_valid)
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
			spin_lock_irqsave(&mnoc_spinlock, flags);
			if (mnoc_reg_valid)
				val = mnoc_read(addr);
			spin_unlock_irqrestore(&mnoc_spinlock, flags);
			LOG_DEBUG("Read back, Reg[%08X] = 0x%08X\n",
					mnoc_addr_phy, val);
		}
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_pmu_reg_show(struct seq_file *m, void *v)
{
	seq_puts(m, "Print pmu_reg list\n");
	print_pmu_reg_list(m);

	return 0;
}

static ssize_t mnoc_pmu_reg_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	void *addr = 0;
	unsigned long flags;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int mnoc_value = 0, mnoc_op = 0;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%x %x", &mnoc_addr_phy,
		&mnoc_value) == 2) {
		if (mnoc_addr_phy < APU_NOC_TOP_ADDR ||
			mnoc_addr_phy >=
			(APU_NOC_TOP_ADDR + APU_NOC_TOP_RANGE)) {
			LOG_DEBUG("Reg[%08X] not in mnoc reg map\n",
				mnoc_addr_phy);
		} else {
			addr = (void *) ((uintptr_t) mnoc_base +
					(mnoc_addr_phy - APU_NOC_TOP_ADDR));
			spin_lock_irqsave(&mnoc_spinlock, flags);
			if (mnoc_reg_valid)
				mnoc_write(addr, mnoc_value);
			spin_unlock_irqrestore(&mnoc_spinlock, flags);
			enque_pmu_reg(mnoc_addr_phy, mnoc_value);
			LOG_DEBUG("Read back, Reg[%08X] = 0x%08X\n",
					mnoc_addr_phy, mnoc_read(addr));
		}
	} else if (kstrtoint(buf, 10, &mnoc_op) == 0) {
		if (mnoc_op == 0)
			clear_pmu_reg_list();
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_pmu_timer_en_show(struct seq_file *m, void *v)
{
	seq_printf(m, "cfg_timer_en = %d\n", cfg_timer_en);

	return 0;
}

static ssize_t mnoc_pmu_timer_en_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int val;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &val) == 0) {
		if (val == 0)
			cfg_timer_en = false;
		else if (val == 1) {
			cfg_timer_en = true;
			mnoc_pmu_timer_start();
		}
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_cmd_qos_start_show(struct seq_file *m, void *v)
{
#if MNOC_TIME_PROFILE
	seq_printf(m, "sum_start = %lu, cnt_start = %d, avg = %d\n",
		sum_start, cnt_start, sum_start/cnt_start);
#endif
	return 0;
}

static ssize_t mnoc_cmd_qos_start_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%d %d %d %d", &cmd_id, &sub_cmd_id,
		&dev_type, &devcore) == 4)
		apu_cmd_qos_start((uint64_t) cmd_id,
			(uint64_t) sub_cmd_id, dev_type, devcore);

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_cmd_qos_suspend_show(struct seq_file *m, void *v)
{
#if MNOC_TIME_PROFILE
	seq_printf(m, "sum_suspend = %lu, cnt_suspend = %d, avg = %d\n",
		sum_suspend, cnt_suspend, sum_suspend/cnt_suspend);
#endif
	return 0;
}

static ssize_t mnoc_cmd_qos_suspend_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int cmd_id, sub_cmd_id;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%d %d", &cmd_id, &sub_cmd_id) == 2)
		apu_cmd_qos_suspend((uint64_t) cmd_id, (uint64_t) sub_cmd_id);

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_cmd_qos_end_show(struct seq_file *m, void *v)
{
#if MNOC_TIME_PROFILE
	seq_printf(m, "sum_end = %lu, cnt_end = %d, avg = %d\n",
		sum_end, cnt_end, sum_end/cnt_end);
#endif
	return 0;
}

static ssize_t mnoc_cmd_qos_end_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int cmd_id, sub_cmd_id;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%d %d", &cmd_id, &sub_cmd_id) == 2)
		apu_cmd_qos_end((uint64_t) cmd_id, (uint64_t) sub_cmd_id);

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_cmd_qos_dump_show(struct seq_file *m, void *v)
{

	seq_puts(m, "Print cmd_qos list\n");
	print_cmd_qos_list(m);

	return 0;
}

#define DBG_FOPS_RW(name)						\
	struct dentry *dentry_ ## name;				\
	static int name ## _open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _show,		\
			inode->i_private);				\
	}								\
	static const struct file_operations name ## _fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _write,			\
	}

#define DBG_FOPS_RO(name)						\
	struct dentry *dentry_ ## name;				\
	static int name ## _open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _show,		\
			inode->i_private);				\
	}								\
	static const struct file_operations name ## _fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define CREATE_DBGFS(name)                         \
	{                                                           \
		dentry_ ## name = debugfs_create_file(#name, 0644, \
				mnoc_dbg_root,         \
				NULL, &name ## _fops);       \
		if (IS_ERR_OR_NULL(dentry_ ## name))                          \
			LOG_ERR("failed to create debug file[" #name "]\n"); \
	}


DBG_FOPS_RW(mnoc_reg_rw);
DBG_FOPS_RW(mnoc_pmu_reg);
DBG_FOPS_RW(mnoc_pmu_timer_en);
DBG_FOPS_RW(mnoc_cmd_qos_start);
DBG_FOPS_RW(mnoc_cmd_qos_suspend);
DBG_FOPS_RW(mnoc_cmd_qos_end);
DBG_FOPS_RO(mnoc_cmd_qos_dump);

struct dentry *mnoc_dbg_root;

int create_debugfs(void)
{
	int ret = 0;

	LOG_DEBUG("+\n");

	mnoc_dbg_root = debugfs_create_dir(APUSYS_MNOC_DEV_NAME, NULL);
	ret = IS_ERR_OR_NULL(mnoc_dbg_root);
	if (ret) {
		LOG_ERR("failed to create debugfs dir\n");
		goto out;
	}

	CREATE_DBGFS(mnoc_reg_rw);
	CREATE_DBGFS(mnoc_pmu_reg);
	CREATE_DBGFS(mnoc_pmu_timer_en);
	CREATE_DBGFS(mnoc_cmd_qos_start);
	CREATE_DBGFS(mnoc_cmd_qos_suspend);
	CREATE_DBGFS(mnoc_cmd_qos_end);
	CREATE_DBGFS(mnoc_cmd_qos_dump);

	LOG_DEBUG("-\n");

out:
	return ret;
}

void remove_debugfs(void)
{
	debugfs_remove_recursive(mnoc_dbg_root);

	LOG_DEBUG("debugfs removed\n");
}

