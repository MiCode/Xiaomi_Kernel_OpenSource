/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>

#include "mdss.h"
#include "mdss_mdp.h"
#include "mdss_debug.h"

#ifdef CONFIG_FB_MSM_MDSS_XLOG_DEBUG
#define XLOG_DEFAULT_ENABLE 1
#else
#define XLOG_DEFAULT_ENABLE 0
#endif

#define XLOG_DEFAULT_PANIC 1
#define XLOG_DEFAULT_REGDUMP 0x2 /* dump in RAM */
#define XLOG_DEFAULT_DBGBUSDUMP 0x3 /* dump in LOG & RAM */

#define MDSS_XLOG_ENTRY	256
#define MDSS_XLOG_MAX_DATA 6
#define MDSS_XLOG_BUF_MAX 512
#define MDSS_XLOG_BUF_ALIGN 32

DEFINE_SPINLOCK(xlock);

struct tlog {
	s64 time;
	const char *name;
	int line;
	u32 data[MDSS_XLOG_MAX_DATA];
	u32 data_cnt;
	int pid;
};

struct mdss_dbg_xlog {
	struct tlog logs[MDSS_XLOG_ENTRY];
	u32 first;
	u32 last;
	struct dentry *xlog;
	u32 xlog_enable;
	u32 panic_on_err;
	u32 enable_reg_dump;
	u32 enable_dbgbus_dump;
	struct work_struct xlog_dump_work;
	struct mdss_debug_base *blk_arr[MDSS_DEBUG_BASE_MAX];
	bool work_panic;
	bool work_dbgbus;
	u32 *dbgbus_dump; /* address for the debug bus dump */
} mdss_dbg_xlog;

static inline bool mdss_xlog_is_enabled(u32 flag)
{
	return (flag & mdss_dbg_xlog.xlog_enable) ||
		(flag == MDSS_XLOG_ALL && mdss_dbg_xlog.xlog_enable);
}

void mdss_xlog(const char *name, int line, int flag, ...)
{
	unsigned long flags;
	int i, val = 0;
	va_list args;
	struct tlog *log;


	if (!mdss_xlog_is_enabled(flag))
		return;

	spin_lock_irqsave(&xlock, flags);

	log = &mdss_dbg_xlog.logs[mdss_dbg_xlog.last % MDSS_XLOG_ENTRY];
	log->time = ktime_to_us(ktime_get());
	log->name = name;
	log->line = line;
	log->data_cnt = 0;
	log->pid = current->pid;

	va_start(args, flag);
	for (i = 0; i < MDSS_XLOG_MAX_DATA; i++) {

		val = va_arg(args, int);
		if (val == DATA_LIMITER)
			break;

		log->data[i] = val;
	}
	va_end(args);
	log->data_cnt = i;

	mdss_dbg_xlog.last++;

	spin_unlock_irqrestore(&xlock, flags);
}

/* always dump the last entries which are not dumped yet */
static bool __mdss_xlog_dump_calc_range(void)
{
	static u32 next;
	bool need_dump = true;
	unsigned long flags;
	struct mdss_dbg_xlog *xlog = &mdss_dbg_xlog;

	spin_lock_irqsave(&xlock, flags);

	xlog->first = next;

	if (xlog->last == xlog->first) {
		need_dump = false;
		goto dump_exit;
	}

	if (xlog->last < xlog->first) {
		xlog->first %= MDSS_XLOG_ENTRY;
		if (xlog->last < xlog->first)
			xlog->last += MDSS_XLOG_ENTRY;
	}

	if ((xlog->last - xlog->first) > MDSS_XLOG_ENTRY) {
		pr_warn("xlog buffer overflow before dump: %d\n",
			xlog->last - xlog->first);
		xlog->first = xlog->last - MDSS_XLOG_ENTRY;
	}
	need_dump = true;
	next = xlog->first + 1;

dump_exit:
	spin_unlock_irqrestore(&xlock, flags);

	return need_dump;
}

static ssize_t mdss_xlog_dump_entry(char *xlog_buf, ssize_t xlog_buf_size)
{
	int i;
	ssize_t off = 0;
	struct tlog *log, *prev_log;
	unsigned long flags;

	spin_lock_irqsave(&xlock, flags);

	log = &mdss_dbg_xlog.logs[mdss_dbg_xlog.first %
		MDSS_XLOG_ENTRY];

	prev_log = &mdss_dbg_xlog.logs[(mdss_dbg_xlog.first - 1) %
		MDSS_XLOG_ENTRY];

	off = snprintf((xlog_buf + off), (xlog_buf_size - off), "%s:%-4d",
		log->name, log->line);

	if (off < MDSS_XLOG_BUF_ALIGN) {
		memset((xlog_buf + off), 0x20, (MDSS_XLOG_BUF_ALIGN - off));
		off = MDSS_XLOG_BUF_ALIGN;
	}

	off += snprintf((xlog_buf + off), (xlog_buf_size - off),
		"=>[%-8d:%-11llu:%9llu][%-4d]:", mdss_dbg_xlog.first,
		log->time, (log->time - prev_log->time), log->pid);

	for (i = 0; i < log->data_cnt; i++)
		off += snprintf((xlog_buf + off), (xlog_buf_size - off),
			"%x ", log->data[i]);

	off += snprintf((xlog_buf + off), (xlog_buf_size - off), "\n");

	spin_unlock_irqrestore(&xlock, flags);

	return off;
}

static void mdss_xlog_dump_all(void)
{
	char xlog_buf[MDSS_XLOG_BUF_MAX];

	while (__mdss_xlog_dump_calc_range()) {
		mdss_xlog_dump_entry(xlog_buf, MDSS_XLOG_BUF_MAX);
		pr_info("%s", xlog_buf);
	}
}

u32 get_dump_range(struct dump_offset *range_node, size_t max_offset)
{
	u32 length = 0;

	if ((range_node->start > range_node->end) ||
		(range_node->end > max_offset) || (range_node->start == 0
		&& range_node->end == 0)) {
		length = max_offset;
	} else {
		length = range_node->end - range_node->start;
	}

	return length;
}

static void mdss_dump_debug_bus(u32 bus_dump_flag,
	u32 **dump_mem)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	u32 status = 0;
	struct debug_bus *head;
	phys_addr_t phys = 0;
	int list_size = mdata->dbg_bus_size;
	int i;

	if (!(mdata->dbg_bus && list_size))
		return;

	/* will keep in memory 4 entries of 4 bytes each */
	list_size = (list_size * 4 * 4);

	in_log = (bus_dump_flag & MDSS_DBG_DUMP_IN_LOG);
	in_mem = (bus_dump_flag & MDSS_DBG_DUMP_IN_MEM);

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(&mdata->pdev->dev,
				list_size, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			pr_info("bus dump_addr:%pK size:%d\n",
				dump_addr, list_size);
		} else {
			in_mem = false;
			pr_err("dump_mem: allocation fails\n");
		}
	}

	pr_info("======== Debug bus DUMP =========\n");

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	for (i = 0; i < mdata->dbg_bus_size; i++) {
		head = mdata->dbg_bus + i;
		writel_relaxed(TEST_MASK(head->block_id, head->test_id),
				mdss_res->mdp_base + head->wr_addr);
		wmb(); /* make sure test bits were written */
		status = readl_relaxed(mdss_res->mdp_base +
			head->wr_addr + 0x4);

		if (in_log)
			pr_err("waddr=0x%x blk=%d tst=%d val=0x%x\n",
				head->wr_addr, head->block_id, head->test_id,
				status);

		if (dump_addr && in_mem) {
			dump_addr[i*4]     = head->wr_addr;
			dump_addr[i*4 + 1] = head->block_id;
			dump_addr[i*4 + 2] = head->test_id;
			dump_addr[i*4 + 3] = status;
		}

	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	pr_info("========End Debug bus=========\n");

}

static void mdss_dump_reg(u32 reg_dump_flag,
	char *addr, int len, u32 **dump_mem)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	phys_addr_t phys = 0;
	int i;

	in_log = (reg_dump_flag & MDSS_DBG_DUMP_IN_LOG);
	in_mem = (reg_dump_flag & MDSS_DBG_DUMP_IN_MEM);

	pr_info("reg_dump_flag=%d in_log=%d in_mem=%d\n", reg_dump_flag, in_log,
		in_mem);

	if (len % 16)
		len += 16;
	len /= 16;

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(&mdata->pdev->dev,
				len * 16, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			pr_info("start_addr:%pK end_addr:%pK reg_addr=%pK\n",
				dump_addr, dump_addr + (u32)len * 16,
				addr);
		} else {
			in_mem = false;
			pr_err("dump_mem: kzalloc fails!\n");
		}
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	for (i = 0; i < len; i++) {
		u32 x0, x4, x8, xc;

		x0 = readl_relaxed(addr+0x0);
		x4 = readl_relaxed(addr+0x4);
		x8 = readl_relaxed(addr+0x8);
		xc = readl_relaxed(addr+0xc);

		if (in_log)
			pr_info("%pK : %08x %08x %08x %08x\n", addr, x0, x4, x8,
				xc);

		if (dump_addr && in_mem) {
			dump_addr[i*4] = x0;
			dump_addr[i*4 + 1] = x4;
			dump_addr[i*4 + 2] = x8;
			dump_addr[i*4 + 3] = xc;
		}

		addr += 16;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

static void mdss_dump_reg_by_ranges(struct mdss_debug_base *dbg,
	u32 reg_dump_flag)
{
	char *addr;
	int len;
	struct range_dump_node *xlog_node, *xlog_tmp;

	if (!dbg || !dbg->base) {
		pr_err("dbg base is null!\n");
		return;
	}

	pr_info("%s:=========%s DUMP=========\n", __func__, dbg->name);

	/* If there is a list to dump the registers by ranges, use the ranges */
	if (!list_empty(&dbg->dump_list)) {
		list_for_each_entry_safe(xlog_node, xlog_tmp,
			&dbg->dump_list, head) {
			len = get_dump_range(&xlog_node->offset,
				dbg->max_offset);
			addr = dbg->base + xlog_node->offset.start;
			pr_info("%s: range_base=0x%pK start=0x%x end=0x%x\n",
				xlog_node->range_name,
				addr, xlog_node->offset.start,
				xlog_node->offset.end);
			mdss_dump_reg(reg_dump_flag, addr, len,
				&xlog_node->reg_dump);
		}
	} else {
		/* If there is no list to dump ranges, dump all registers */
		pr_info("Ranges not found, will dump full registers");
		pr_info("base:0x%pK len:0x%zu\n", dbg->base, dbg->max_offset);
		addr = dbg->base;
		len = dbg->max_offset;
		mdss_dump_reg(reg_dump_flag, addr, len, &dbg->reg_dump);
	}
}

static void mdss_dump_reg_by_blk(const char *blk_name)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	struct mdss_debug_base *blk_base, *tmp;

	if (!mdd)
		return;

	list_for_each_entry_safe(blk_base, tmp, &mdd->base_list, head) {
		if (blk_base->name &&
			!strcmp(blk_base->name, blk_name)) {
			mdss_dump_reg_by_ranges(blk_base,
				mdss_dbg_xlog.enable_reg_dump);
			break;
		}
	}
}

static void mdss_dump_reg_all(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	struct mdss_debug_base *blk_base, *tmp;

	if (!mdd)
		return;

	list_for_each_entry_safe(blk_base, tmp, &mdd->base_list, head) {
		if (blk_base->name)
			mdss_dump_reg_by_blk(blk_base->name);
	}
}

static void clear_dump_blk_arr(struct mdss_debug_base *blk_arr[],
	u32 blk_len)
{
	int i;

	for (i = 0; i < blk_len; i++)
		blk_arr[i] = NULL;
}

struct mdss_debug_base *get_dump_blk_addr(const char *blk_name)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	struct mdss_debug_base *blk_base, *tmp;

	if (!mdd)
		return NULL;

	list_for_each_entry_safe(blk_base, tmp, &mdd->base_list, head) {
		if (blk_base->name &&
			!strcmp(blk_base->name, blk_name))
				return blk_base;
	}

	return NULL;
}

static void mdss_xlog_dump_array(struct mdss_debug_base *blk_arr[],
	u32 len, bool dead, const char *name, bool dump_dbgbus)
{
	int i;

	for (i = 0; i < len; i++) {
		if (blk_arr[i] != NULL)
			mdss_dump_reg_by_ranges(blk_arr[i],
				mdss_dbg_xlog.enable_reg_dump);
	}

	if (mdss_xlog_is_enabled(MDSS_XLOG_DEFAULT))
		mdss_xlog_dump_all();

	if (dump_dbgbus)
		mdss_dump_debug_bus(mdss_dbg_xlog.enable_dbgbus_dump,
			&mdss_dbg_xlog.dbgbus_dump);

	if (dead && mdss_dbg_xlog.panic_on_err)
		panic(name);
}

static void xlog_debug_work(struct work_struct *work)
{

	mdss_xlog_dump_array(mdss_dbg_xlog.blk_arr,
		ARRAY_SIZE(mdss_dbg_xlog.blk_arr),
		mdss_dbg_xlog.work_panic, "xlog_workitem",
		mdss_dbg_xlog.work_dbgbus);
}

void mdss_xlog_tout_handler_default(bool enforce_dump, bool queue,
	const char *name, ...)
{
	int i, index = 0;
	bool dead = false;
	bool dump_dbgbus = false;
	va_list args;
	char *blk_name = NULL;
	struct mdss_debug_base *blk_base = NULL;
	struct mdss_debug_base **blk_arr;
	u32 blk_len;

	if (!mdss_xlog_is_enabled(MDSS_XLOG_DEFAULT) && !enforce_dump)
		return;

	if (queue && work_pending(&mdss_dbg_xlog.xlog_dump_work))
		return;

	blk_arr = &mdss_dbg_xlog.blk_arr[0];
	blk_len = ARRAY_SIZE(mdss_dbg_xlog.blk_arr);

	clear_dump_blk_arr(blk_arr, blk_len);

	va_start(args, name);
	for (i = 0; i < MDSS_XLOG_MAX_DATA; i++) {
		blk_name = va_arg(args, char*);
		if (IS_ERR_OR_NULL(blk_name))
			break;

		blk_base = get_dump_blk_addr(blk_name);
		if (blk_base && (index < blk_len)) {
			blk_arr[index] = blk_base;
			index++;
		}

		if (!strcmp(blk_name, "mdp_dbg_bus"))
			dump_dbgbus = true;

		if (!strcmp(blk_name, "panic"))
			dead = true;
	}
	va_end(args);

	if (queue) {
		/* schedule work to dump later */
		mdss_dbg_xlog.work_panic = dead;
		mdss_dbg_xlog.work_dbgbus = dump_dbgbus;
		schedule_work(&mdss_dbg_xlog.xlog_dump_work);
	} else {
		mdss_xlog_dump_array(blk_arr, blk_len, dead, name, dump_dbgbus);
	}
}

int mdss_xlog_tout_handler_iommu(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	if (!mdss_xlog_is_enabled(MDSS_XLOG_IOMMU))
		return 0;

	mdss_dump_reg_by_blk("mdp");
	mdss_dump_reg_by_blk("vbif");
	mdss_xlog_dump_all();
	panic("mdp iommu");

	return 0;
}

static int mdss_xlog_dump_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mdss_xlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char xlog_buf[MDSS_XLOG_BUF_MAX];

	if (__mdss_xlog_dump_calc_range()) {
		len = mdss_xlog_dump_entry(xlog_buf, MDSS_XLOG_BUF_MAX);
		if (copy_to_user(buff, xlog_buf, len))
			return -EFAULT;
		*ppos += len;
	}

	return len;
}

static ssize_t mdss_xlog_dump_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	mdss_dump_reg_all();

	mdss_xlog_dump_all();

	if (mdss_dbg_xlog.panic_on_err)
		panic("mdss");

	return count;
}


static const struct file_operations mdss_xlog_fops = {
	.open = mdss_xlog_dump_open,
	.read = mdss_xlog_dump_read,
	.write = mdss_xlog_dump_write,
};

int mdss_create_xlog_debug(struct mdss_debug_data *mdd)
{
	mdss_dbg_xlog.xlog = debugfs_create_dir("xlog", mdd->root);
	if (IS_ERR_OR_NULL(mdss_dbg_xlog.xlog)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(mdss_dbg_xlog.xlog));
		mdss_dbg_xlog.xlog = NULL;
		return -ENODEV;
	}

	INIT_WORK(&mdss_dbg_xlog.xlog_dump_work, xlog_debug_work);
	mdss_dbg_xlog.work_panic = false;

	debugfs_create_file("dump", 0644, mdss_dbg_xlog.xlog, NULL,
						&mdss_xlog_fops);
	debugfs_create_u32("enable", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.xlog_enable);
	debugfs_create_bool("panic", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.panic_on_err);
	debugfs_create_u32("reg_dump", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.enable_reg_dump);
	debugfs_create_u32("dbgbus_dump", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.enable_dbgbus_dump);

	mdss_dbg_xlog.xlog_enable = XLOG_DEFAULT_ENABLE;
	mdss_dbg_xlog.panic_on_err = XLOG_DEFAULT_PANIC;
	mdss_dbg_xlog.enable_reg_dump = XLOG_DEFAULT_REGDUMP;
	mdss_dbg_xlog.enable_dbgbus_dump = XLOG_DEFAULT_DBGBUSDUMP;

	pr_info("xlog_status: enable:%d, panic:%d, dump:%d\n",
		mdss_dbg_xlog.xlog_enable, mdss_dbg_xlog.panic_on_err,
		mdss_dbg_xlog.enable_reg_dump);

	return 0;
}
