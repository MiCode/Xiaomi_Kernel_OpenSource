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
#include <linux/mutex.h>

#include "mnoc_drv.h"
#include "mnoc_hw.h"
#include "mnoc_qos.h"
#include "mnoc_api.h"
#include "mnoc_pmu.h"
#include "mnoc_option.h"

static unsigned int mnoc_addr_phy;

static int mnoc_log_level_show(struct seq_file *m, void *v)
{
	seq_printf(m, "mnoc_log_level = %d\n", mnoc_log_level);

#if MNOC_TIME_PROFILE
	/* dump cmd qos profile info */
	seq_printf(m, "sum_start = %lu, cnt_start = %d, avg = %lu\n",
		sum_start, cnt_start, sum_start/cnt_start);
	seq_printf(m, "sum_end = %lu, cnt_end = %d, avg = %lu\n",
		sum_end, cnt_end, sum_end/cnt_end);
	seq_printf(m, "sum_work_func = %lu, cnt_work_func = %d, avg = %lu\n",
		sum_work_func, cnt_work_func, sum_work_func/cnt_work_func);
#endif

	return 0;
}

static ssize_t mnoc_log_level_write(struct file *file,
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

	if (kstrtoint(buf, 10, &val) == 0)
		if (val >= 0 && val <= 2)
			mnoc_log_level = val;

out:
	free_page((unsigned long)buf);
	return count;
}

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
	unsigned char mnoc_rw = 0;
	unsigned int addr_phy;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%c %x %x", &mnoc_rw, &addr_phy,
		&mnoc_value) == 3) {
		if (mnoc_rw != 'w' && mnoc_rw != 'W')
			goto out;
		if (addr_phy < APU_NOC_TOP_ADDR ||
			addr_phy >=
			(APU_NOC_TOP_ADDR + APU_NOC_TOP_RANGE)) {
			LOG_DEBUG("Reg[%08X] not in mnoc driver reg map\n",
				addr_phy);
		} else {
#if MNOC_DBG_ENABLE
			addr = (void *) ((uintptr_t) mnoc_base +
					(addr_phy - APU_NOC_TOP_ADDR));
			spin_lock_irqsave(&mnoc_spinlock, flags);
			mnoc_addr_phy = addr_phy;
			if (mnoc_reg_valid)
				mnoc_write(addr, mnoc_value);
			spin_unlock_irqrestore(&mnoc_spinlock, flags);
#endif
		}
	} else if (sscanf(buf, "%c %x", &mnoc_rw, &addr_phy) == 2) {
		if (mnoc_rw != 'r' && mnoc_rw != 'R')
			goto out;
		if (addr_phy < APU_NOC_TOP_ADDR ||
			addr_phy >=
			(APU_NOC_TOP_ADDR + APU_NOC_TOP_RANGE)) {
			LOG_DEBUG("Reg[%08X] not in mnoc driver reg map\n",
				addr_phy);
		} else {
			addr = (void *) ((uintptr_t) mnoc_base +
					(addr_phy - APU_NOC_TOP_ADDR));
			spin_lock_irqsave(&mnoc_spinlock, flags);
			mnoc_addr_phy = addr_phy;
			if (mnoc_reg_valid)
				val = mnoc_read(addr);
			spin_unlock_irqrestore(&mnoc_spinlock, flags);
			LOG_DEBUG("Read back, Reg[%08X] = 0x%08X\n",
					addr_phy, val);
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

/* usage:
 * pmu reg setting: echo w phys_addr > mnoc_pmu_reg
 * clear pmu reg list: echo 0 > mnoc_pmu_reg
 * clear pmu counter to zero: echo c grp_num > mnoc_pmu_reg
 */
static ssize_t mnoc_pmu_reg_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	void *addr = 0;
	unsigned long flags;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int mnoc_value = 0;
	unsigned char mnoc_op = 0;
	unsigned int pmu_addr_phy = 0;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%c %x %x", &mnoc_op, &pmu_addr_phy,
		&mnoc_value) == 3) {
		if (mnoc_op != 'w' && mnoc_op != 'W')
			goto out;
		if (!mnoc_pmu_reg_in_range(pmu_addr_phy)) {
			LOG_DEBUG("Reg[%08X] not in pmu reg map\n",
				pmu_addr_phy);
		} else {
			addr = (void *) ((uintptr_t) mnoc_base +
					(pmu_addr_phy - APU_NOC_TOP_ADDR));
			spin_lock_irqsave(&mnoc_spinlock, flags);
			if (mnoc_reg_valid)
				mnoc_write(addr, mnoc_value);
			spin_unlock_irqrestore(&mnoc_spinlock, flags);
			enque_pmu_reg(pmu_addr_phy, mnoc_value);
		}
	} else if (sscanf(buf, "%c %d", &mnoc_op,
		&mnoc_value) == 2) {
		if (mnoc_op != 'c' && mnoc_op != 'C')
			goto out;
		if (mnoc_value < NR_GROUP)
			mnoc_clear_pmu_counter(mnoc_value);
	} else if (kstrtoint(buf, 10, &mnoc_value) == 0) {
		if (mnoc_value == 0)
			clear_pmu_reg_list();
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_pmu_timer_en_show(struct seq_file *m, void *v)
{
	seq_printf(m, "mnoc_cfg_timer_en = %d\n", mnoc_cfg_timer_en);

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
			mnoc_cfg_timer_en = false;
		else if (val == 1) {
			mnoc_cfg_timer_en = true;
			mnoc_pmu_timer_start();
		}
	}

out:
	free_page((unsigned long)buf);
	return count;
}

#if MNOC_QOS_BOOST_ENABLE && MNOC_QOS_ENABLE
static int mnoc_apu_qos_boost_show(struct seq_file *m, void *v)
{
	seq_printf(m, "apu_qos_boost_flag = %d\n", apu_qos_boost_flag);

	return 0;
}

static ssize_t mnoc_apu_qos_boost_write(struct file *file,
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
		if (val == 0) {
			mutex_lock(&apu_qos_boost_mtx);
			apu_qos_boost_flag = false;
			apu_qos_boost_end();
			mutex_unlock(&apu_qos_boost_mtx);
		} else if (val == 1) {
			mutex_lock(&apu_qos_boost_mtx);
			apu_qos_boost_flag = true;
			apu_qos_boost_start();
			mutex_unlock(&apu_qos_boost_mtx);
		}
	}

out:
	free_page((unsigned long)buf);
	return count;
}
#endif

#if MNOC_DBG_ENABLE
static int mnoc_cmd_qos_start_show(struct seq_file *m, void *v)
{
#if MNOC_TIME_PROFILE
	seq_printf(m, "sum_start = %lu, cnt_start = %d, avg = %lu\n",
		sum_start, cnt_start, sum_start/cnt_start);
#endif
	return 0;
}

static ssize_t mnoc_cmd_qos_start_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore, boost;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%d %d %d %d %d", &cmd_id, &sub_cmd_id,
		&dev_type, &devcore, &boost) == 4)
		apu_cmd_qos_start((uint64_t) cmd_id,
			(uint64_t) sub_cmd_id, dev_type, devcore, boost);

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_cmd_qos_suspend_show(struct seq_file *m, void *v)
{
#if MNOC_TIME_PROFILE
	seq_printf(m, "sum_suspend = %lu, cnt_suspend = %d, avg = %lu\n",
		sum_suspend, cnt_suspend, sum_suspend/cnt_suspend);
#endif
	return 0;
}

static ssize_t mnoc_cmd_qos_suspend_write(struct file *file,
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
		apu_cmd_qos_suspend((uint64_t) cmd_id,
			(uint64_t) sub_cmd_id, dev_type, devcore);

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_cmd_qos_end_show(struct seq_file *m, void *v)
{
#if MNOC_TIME_PROFILE
	seq_printf(m, "sum_end = %lu, cnt_end = %d, avg = %lu\n",
		sum_end, cnt_end, sum_end/cnt_end);
#endif
	return 0;
}

static ssize_t mnoc_cmd_qos_end_write(struct file *file,
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
		apu_cmd_qos_end((uint64_t) cmd_id,
			(uint64_t) sub_cmd_id, dev_type, devcore);

out:
	free_page((unsigned long)buf);
	return count;
}

static int mnoc_tcm_endis_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x10001E98 = 0x%08X\n",
		mnoc_read(mnoc_slp_prot_base1 + 0xE98));
	return 0;
}

static ssize_t mnoc_tcm_endis_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int endis;
	unsigned char mnoc_tcm;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%c %d", &mnoc_tcm, &endis) == 2) {
		if (mnoc_tcm != 't' && mnoc_tcm != 'T')
			goto out;
		if (endis == 1)
			infra2apu_sram_en();
		else if (endis == 0)
			infra2apu_sram_dis();
	}

out:
	free_page((unsigned long)buf);
	return count;
}
#endif /* MNOC_DBG_ENABLE */

static int mnoc_cmd_qos_dump_show(struct seq_file *m, void *v)
{
	seq_puts(m, "Print cmd_qos list\n");
	print_cmd_qos_list(m);

	return 0;
}

static int mnoc_int_sta_dump_show(struct seq_file *m, void *v)
{
	seq_puts(m, "Print interrupt count and last snapshot\n");
	print_int_sta(m);

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


DBG_FOPS_RW(mnoc_log_level);
DBG_FOPS_RW(mnoc_reg_rw);
DBG_FOPS_RW(mnoc_pmu_reg);
DBG_FOPS_RW(mnoc_pmu_timer_en);
#if MNOC_QOS_BOOST_ENABLE && MNOC_QOS_ENABLE
DBG_FOPS_RW(mnoc_apu_qos_boost);
#endif
#if MNOC_DBG_ENABLE
DBG_FOPS_RW(mnoc_cmd_qos_start);
DBG_FOPS_RW(mnoc_cmd_qos_suspend);
DBG_FOPS_RW(mnoc_cmd_qos_end);
DBG_FOPS_RW(mnoc_tcm_endis);
#endif
DBG_FOPS_RO(mnoc_cmd_qos_dump);
DBG_FOPS_RO(mnoc_int_sta_dump);

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

	CREATE_DBGFS(mnoc_log_level);
	CREATE_DBGFS(mnoc_reg_rw);
	CREATE_DBGFS(mnoc_pmu_reg);
	CREATE_DBGFS(mnoc_pmu_timer_en);
#if MNOC_QOS_BOOST_ENABLE && MNOC_QOS_ENABLE
	CREATE_DBGFS(mnoc_apu_qos_boost);
#endif
#if MNOC_DBG_ENABLE
	CREATE_DBGFS(mnoc_cmd_qos_start);
	CREATE_DBGFS(mnoc_cmd_qos_suspend);
	CREATE_DBGFS(mnoc_cmd_qos_end);
	CREATE_DBGFS(mnoc_tcm_endis);
#endif
	CREATE_DBGFS(mnoc_cmd_qos_dump);
	CREATE_DBGFS(mnoc_int_sta_dump);

	LOG_DEBUG("-\n");

out:
	return ret;
}

void remove_debugfs(void)
{
	debugfs_remove_recursive(mnoc_dbg_root);

	LOG_DEBUG("debugfs removed\n");
}

