/* Copyright (c) 2009-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>

#include "sde_dbg.h"

#define SDE_DBG_BASE_MAX		10

#define DEFAULT_PANIC		1
#define DEFAULT_REGDUMP		SDE_DBG_DUMP_IN_MEM
#define DEFAULT_BASE_REG_CNT	0x100
#define GROUP_BYTES		4
#define ROW_BYTES		16
#define RANGE_NAME_LEN		40
#define REG_BASE_NAME_LEN	80

/**
 * struct sde_dbg_reg_offset - tracking for start and end of region
 * @start: start offset
 * @start: end offset
 */
struct sde_dbg_reg_offset {
	u32 start;
	u32 end;
};

/**
 * struct sde_dbg_reg_range - register dumping named sub-range
 * @head: head of this node
 * @reg_dump: address for the mem dump
 * @range_name: name of this range
 * @offset: offsets for range to dump
 * @xin_id: client xin id
 */
struct sde_dbg_reg_range {
	struct list_head head;
	u32 *reg_dump;
	char range_name[RANGE_NAME_LEN];
	struct sde_dbg_reg_offset offset;
	uint32_t xin_id;
};

/**
 * struct sde_dbg_reg_base - register region base.
 *	may sub-ranges: sub-ranges are used for dumping
 *	or may not have sub-ranges: dumping is base -> max_offset
 * @reg_base_head: head of this node
 * @sub_range_list: head to the list with dump ranges
 * @name: register base name
 * @base: base pointer
 * @off: cached offset of region for manual register dumping
 * @cnt: cached range of region for manual register dumping
 * @max_offset: length of region
 * @buf: buffer used for manual register dumping
 * @buf_len:  buffer length used for manual register dumping
 * @reg_dump: address for the mem dump if no ranges used
 */
struct sde_dbg_reg_base {
	struct list_head reg_base_head;
	struct list_head sub_range_list;
	char name[REG_BASE_NAME_LEN];
	void __iomem *base;
	size_t off;
	size_t cnt;
	size_t max_offset;
	char *buf;
	size_t buf_len;
	u32 *reg_dump;
};

/**
 * struct sde_dbg_base - global sde debug base structure
 * @evtlog: event log instance
 * @reg_base_list: list of register dumping regions
 * @root: base debugfs root
 * @dev: device pointer
 * @power_ctrl: callback structure for enabling power for reading hw registers
 * @req_dump_blks: list of blocks requested for dumping
 * @panic_on_err: whether to kernel panic after triggering dump via debugfs
 * @dump_work: work struct for deferring register dump work to separate thread
 * @work_panic: panic after dump if internal user passed "panic" special region
 * @enable_reg_dump: whether to dump registers into memory, kernel log, or both
 */
static struct sde_dbg_base {
	struct sde_dbg_evtlog *evtlog;
	struct list_head reg_base_list;
	struct dentry *root;
	struct device *dev;
	struct sde_dbg_power_ctrl power_ctrl;

	struct sde_dbg_reg_base *req_dump_blks[SDE_DBG_BASE_MAX];

	u32 panic_on_err;
	struct work_struct dump_work;
	bool work_panic;
	u32 enable_reg_dump;
} sde_dbg_base;

/* sde_dbg_base_evtlog - global pointer to main sde event log for macro use */
struct sde_dbg_evtlog *sde_dbg_base_evtlog;

/**
 * _sde_dbg_enable_power - use callback to turn power on for hw register access
 * @enable: whether to turn power on or off
 */
static inline void _sde_dbg_enable_power(int enable)
{
	if (!sde_dbg_base.power_ctrl.enable_fn)
		return;
	sde_dbg_base.power_ctrl.enable_fn(
			sde_dbg_base.power_ctrl.handle,
			sde_dbg_base.power_ctrl.client,
			enable);
}

/**
 * _sde_dump_reg - helper function for dumping rotator register set content
 * @dump_name: register set name
 * @reg_dump_flag: dumping flag controlling in-log/memory dump location
 * @addr: starting address offset for dumping
 * @len_bytes: range of the register set
 * @dump_mem: output buffer for memory dump location option
 * @from_isr: whether being called from isr context
 */
static void _sde_dump_reg(const char *dump_name, u32 reg_dump_flag,
		char __iomem *addr, size_t len_bytes, u32 **dump_mem,
		bool from_isr)
{
	u32 in_log, in_mem, len_align_16, len_bytes_aligned;
	u32 *dump_addr = NULL;
	char __iomem *end_addr;
	int i;

	in_log = (reg_dump_flag & SDE_DBG_DUMP_IN_LOG);
	in_mem = (reg_dump_flag & SDE_DBG_DUMP_IN_MEM);

	pr_debug("reg_dump_flag=%d in_log=%d in_mem=%d\n",
		reg_dump_flag, in_log, in_mem);

	len_align_16 = (len_bytes + 15) / 16;
	len_bytes_aligned = len_align_16 * 16;
	end_addr = addr + len_bytes;

	if (in_mem) {
		if (dump_mem && !(*dump_mem)) {
			phys_addr_t phys = 0;
			*dump_mem = dma_alloc_coherent(sde_dbg_base.dev,
					len_bytes_aligned, &phys, GFP_KERNEL);
		}

		if (dump_mem && *dump_mem) {
			dump_addr = *dump_mem;
			pr_info("%s: start_addr:0x%pK end_addr:0x%pK reg_addr=0x%pK\n",
				dump_name, dump_addr,
				dump_addr + len_bytes_aligned, addr);
		} else {
			in_mem = 0;
			pr_err("dump_mem: kzalloc fails!\n");
		}
	}

	if (!from_isr)
		_sde_dbg_enable_power(true);

	for (i = 0; i < len_align_16; i++) {
		u32 x0, x4, x8, xc;

		x0 = (addr < end_addr) ? readl_relaxed(addr + 0x0) : 0;
		x4 = (addr + 0x4 < end_addr) ? readl_relaxed(addr + 0x4) : 0;
		x8 = (addr + 0x8 < end_addr) ? readl_relaxed(addr + 0x8) : 0;
		xc = (addr + 0xc < end_addr) ? readl_relaxed(addr + 0xc) : 0;

		if (in_log)
			pr_info("%pK : %08x %08x %08x %08x\n", addr, x0, x4, x8,
				xc);

		if (dump_addr) {
			dump_addr[i * 4] = x0;
			dump_addr[i * 4 + 1] = x4;
			dump_addr[i * 4 + 2] = x8;
			dump_addr[i * 4 + 3] = xc;
		}

		addr += 16;
	}

	if (!from_isr)
		_sde_dbg_enable_power(false);
}

/**
 * _sde_dbg_get_dump_range - helper to retrieve dump length for a range node
 * @range_node: range node to dump
 * @max_offset: max offset of the register base
 * @Return: length
 */
static u32 _sde_dbg_get_dump_range(struct sde_dbg_reg_offset *range_node,
		size_t max_offset)
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

/**
 * _sde_dump_reg_by_ranges - dump ranges or full range of the register blk base
 * @dbg: register blk base structure
 * @reg_dump_flag: dump target, memory, kernel log, or both
 */
static void _sde_dump_reg_by_ranges(struct sde_dbg_reg_base *dbg,
	u32 reg_dump_flag)
{
	char __iomem *addr;
	size_t len;
	struct sde_dbg_reg_range *range_node;

	if (!dbg || !dbg->base) {
		pr_err("dbg base is null!\n");
		return;
	}

	pr_info("%s:=========%s DUMP=========\n", __func__, dbg->name);

	/* If there is a list to dump the registers by ranges, use the ranges */
	if (!list_empty(&dbg->sub_range_list)) {
		list_for_each_entry(range_node, &dbg->sub_range_list, head) {
			len = _sde_dbg_get_dump_range(&range_node->offset,
				dbg->max_offset);
			addr = dbg->base + range_node->offset.start;
			pr_debug("%s: range_base=0x%pK start=0x%x end=0x%x\n",
				range_node->range_name,
				addr, range_node->offset.start,
				range_node->offset.end);

			_sde_dump_reg((const char *)range_node->range_name,
				reg_dump_flag, addr, len, &range_node->reg_dump,
				false);
		}
	} else {
		/* If there is no list to dump ranges, dump all registers */
		pr_info("Ranges not found, will dump full registers");
		pr_info("base:0x%pK len:0x%zx\n", dbg->base, dbg->max_offset);
		addr = dbg->base;
		len = dbg->max_offset;
		_sde_dump_reg((const char *)dbg->name, reg_dump_flag, addr,
			len, &dbg->reg_dump, false);
	}
}

/**
 * _sde_dump_reg_by_blk - dump a named register base region
 * @blk_name: register blk name
 */
static void _sde_dump_reg_by_blk(const char *blk_name)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	if (!dbg_base)
		return;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head) {
		if (strlen(blk_base->name) &&
			!strcmp(blk_base->name, blk_name)) {
			_sde_dump_reg_by_ranges(blk_base,
				dbg_base->enable_reg_dump);
			break;
		}
	}
}

/**
 * _sde_dump_reg_all - dump all register regions
 */
static void _sde_dump_reg_all(void)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	if (!dbg_base)
		return;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head)
		if (strlen(blk_base->name))
			_sde_dump_reg_by_blk(blk_base->name);
}

/**
 * _sde_dump_get_blk_addr - retrieve register block address by name
 * @blk_name: register blk name
 * @Return: register blk base, or NULL
 */
static struct sde_dbg_reg_base *_sde_dump_get_blk_addr(const char *blk_name)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *blk_base;

	list_for_each_entry(blk_base, &dbg_base->reg_base_list, reg_base_head)
		if (strlen(blk_base->name) && !strcmp(blk_base->name, blk_name))
			return blk_base;

	return NULL;
}

/**
 * _sde_dump_array - dump array of register bases
 * @blk_arr: array of register base pointers
 * @len: length of blk_arr
 * @dead: whether to trigger a panic after dumping
 * @name: string indicating origin of dump
 */
static void _sde_dump_array(struct sde_dbg_reg_base *blk_arr[],
	u32 len, bool dead, const char *name)
{
	int i;

	for (i = 0; i < len; i++) {
		if (blk_arr[i] != NULL)
			_sde_dump_reg_by_ranges(blk_arr[i],
				sde_dbg_base.enable_reg_dump);
	}

	sde_evtlog_dump_all(sde_dbg_base.evtlog);

	if (dead && sde_dbg_base.panic_on_err)
		panic(name);
}

/**
 * _sde_dump_work - deferred dump work function
 * @work: work structure
 */
static void _sde_dump_work(struct work_struct *work)
{
	_sde_dump_array(sde_dbg_base.req_dump_blks,
		ARRAY_SIZE(sde_dbg_base.req_dump_blks),
		sde_dbg_base.work_panic, "evtlog_workitem");
}

void sde_dbg_dump(bool queue_work, const char *name, ...)
{
	int i, index = 0;
	bool dead = false;
	va_list args;
	char *blk_name = NULL;
	struct sde_dbg_reg_base *blk_base = NULL;
	struct sde_dbg_reg_base **blk_arr;
	u32 blk_len;

	if (!sde_evtlog_is_enabled(sde_dbg_base.evtlog, SDE_EVTLOG_DEFAULT))
		return;

	if (queue_work && work_pending(&sde_dbg_base.dump_work))
		return;

	blk_arr = &sde_dbg_base.req_dump_blks[0];
	blk_len = ARRAY_SIZE(sde_dbg_base.req_dump_blks);

	memset(sde_dbg_base.req_dump_blks, 0,
			sizeof(sde_dbg_base.req_dump_blks));

	va_start(args, name);
	for (i = 0; i < SDE_EVTLOG_MAX_DATA; i++) {
		blk_name = va_arg(args, char*);
		if (IS_ERR_OR_NULL(blk_name))
			break;

		blk_base = _sde_dump_get_blk_addr(blk_name);
		if (blk_base) {
			if (index < blk_len) {
				blk_arr[index] = blk_base;
				index++;
			} else {
				pr_err("insufficient space to to dump %s\n",
						blk_name);
			}
		}

		if (!strcmp(blk_name, "panic"))
			dead = true;
	}
	blk_name = va_arg(args, char*);
	if (!IS_ERR_OR_NULL(blk_name))
		pr_err("could not parse all dump arguments\n");
	va_end(args);

	if (queue_work) {
		/* schedule work to dump later */
		sde_dbg_base.work_panic = dead;
		schedule_work(&sde_dbg_base.dump_work);
	} else {
		_sde_dump_array(blk_arr, blk_len, dead, name);
	}
}

/*
 * sde_dbg_debugfs_open - debugfs open handler for evtlog dump
 * @inode: debugfs inode
 * @file: file handle
 */
static int sde_dbg_debugfs_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

/**
 * sde_evtlog_dump_read - debugfs read handler for evtlog dump
 * @file: file handler
 * @buff: user buffer content for debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_evtlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char evtlog_buf[SDE_EVTLOG_BUF_MAX];

	len = sde_evtlog_dump_to_buffer(sde_dbg_base.evtlog, evtlog_buf,
			SDE_EVTLOG_BUF_MAX);
	if (copy_to_user(buff, evtlog_buf, len))
		return -EFAULT;
	*ppos += len;

	return len;
}

/**
 * sde_evtlog_dump_write - debugfs write handler for evtlog dump
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_evtlog_dump_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	_sde_dump_reg_all();

	sde_evtlog_dump_all(sde_dbg_base.evtlog);

	if (sde_dbg_base.panic_on_err)
		panic("sde");

	return count;
}

static const struct file_operations sde_evtlog_fops = {
	.open = sde_dbg_debugfs_open,
	.read = sde_evtlog_dump_read,
	.write = sde_evtlog_dump_write,
};

int sde_dbg_init(struct dentry *debugfs_root, struct device *dev,
		struct sde_dbg_power_ctrl *power_ctrl)
{
	int i;

	INIT_LIST_HEAD(&sde_dbg_base.reg_base_list);
	sde_dbg_base.dev = dev;
	sde_dbg_base.power_ctrl = *power_ctrl;


	sde_dbg_base.evtlog = sde_evtlog_init();
	if (IS_ERR_OR_NULL(sde_dbg_base.evtlog))
		return PTR_ERR(sde_dbg_base.evtlog);

	sde_dbg_base_evtlog = sde_dbg_base.evtlog;

	sde_dbg_base.root = debugfs_create_dir("evt_dbg", debugfs_root);
	if (IS_ERR_OR_NULL(sde_dbg_base.root)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(sde_dbg_base.root));
		sde_dbg_base.root = NULL;
		return -ENODEV;
	}

	INIT_WORK(&sde_dbg_base.dump_work, _sde_dump_work);
	sde_dbg_base.work_panic = false;

	for (i = 0; i < SDE_EVTLOG_ENTRY; i++)
		sde_dbg_base.evtlog->logs[i].counter = i;

	debugfs_create_file("dump", 0600, sde_dbg_base.root, NULL,
						&sde_evtlog_fops);
	debugfs_create_u32("enable", 0600, sde_dbg_base.root,
			    &(sde_dbg_base.evtlog->enable));
	debugfs_create_u32("panic", 0600, sde_dbg_base.root,
			    &sde_dbg_base.panic_on_err);
	debugfs_create_u32("reg_dump", 0600, sde_dbg_base.root,
			    &sde_dbg_base.enable_reg_dump);

	sde_dbg_base.panic_on_err = DEFAULT_PANIC;
	sde_dbg_base.enable_reg_dump = DEFAULT_REGDUMP;

	pr_info("evtlog_status: enable:%d, panic:%d, dump:%d\n",
		sde_dbg_base.evtlog->enable, sde_dbg_base.panic_on_err,
		sde_dbg_base.enable_reg_dump);

	return 0;
}

/**
 * sde_dbg_destroy - destroy sde debug facilities
 */
void sde_dbg_destroy(void)
{
	debugfs_remove_recursive(sde_dbg_base.root);
	sde_dbg_base.root = NULL;

	sde_dbg_base_evtlog = NULL;
	sde_evtlog_destroy(sde_dbg_base.evtlog);
	sde_dbg_base.evtlog = NULL;
}

/**
 * sde_dbg_reg_base_release - release allocated reg dump file private data
 * @inode: debugfs inode
 * @file: file handle
 * @Return: 0 on success
 */
static int sde_dbg_reg_base_release(struct inode *inode, struct file *file)
{
	struct sde_dbg_reg_base *dbg = file->private_data;

	if (dbg && dbg->buf) {
		kfree(dbg->buf);
		dbg->buf_len = 0;
		dbg->buf = NULL;
	}
	return 0;
}


/**
 * sde_dbg_reg_base_offset_write - set new offset and len to debugfs reg base
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_offset_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg = file->private_data;
	u32 off = 0;
	u32 cnt = DEFAULT_BASE_REG_CNT;
	char buf[24];

	if (!dbg)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (sscanf(buf, "%5x %x", &off, &cnt) != 2)
		return -EFAULT;

	if (off > dbg->max_offset)
		return -EINVAL;

	if (off % sizeof(u32))
		return -EINVAL;

	if (cnt > (dbg->max_offset - off))
		cnt = dbg->max_offset - off;

	if (cnt % sizeof(u32))
		return -EINVAL;

	dbg->off = off;
	dbg->cnt = cnt;

	pr_debug("offset=%x cnt=%x\n", off, cnt);

	return count;
}

/**
 * sde_dbg_reg_base_offset_read - read current offset and len of register base
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_offset_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg = file->private_data;
	int len = 0;
	char buf[24] = {'\0'};

	if (!dbg)
		return -ENODEV;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(buf, sizeof(buf), "0x%08zx %zx\n", dbg->off, dbg->cnt);
	if (len < 0 || len >= sizeof(buf))
		return 0;

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;	/* increase offset */

	return len;
}

/**
 * sde_dbg_reg_base_reg_write - write to reg base hw at offset a given value
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg = file->private_data;
	size_t off;
	u32 data, cnt;
	char buf[24];

	if (!dbg)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	cnt = sscanf(buf, "%zx %x", &off, &data);

	if (cnt < 2)
		return -EFAULT;

	if (off >= dbg->max_offset)
		return -EFAULT;

	_sde_dbg_enable_power(true);

	writel_relaxed(data, dbg->base + off);

	_sde_dbg_enable_power(false);

	pr_debug("addr=%zx data=%x\n", off, data);

	return count;
}

/**
 * sde_dbg_reg_base_reg_read - read len from reg base hw at current offset
 * @file: file handler
 * @user_buf: user buffer content from debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_dbg_reg_base_reg_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_dbg_reg_base *dbg = file->private_data;
	size_t len;

	if (!dbg) {
		pr_err("invalid handle\n");
		return -ENODEV;
	}

	if (!dbg->buf) {
		char *hwbuf, *hwbuf_cur;
		char dump_buf[64];
		char __iomem *ioptr;
		int cnt, tot;

		dbg->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(dbg->cnt, ROW_BYTES);

		if (dbg->buf_len % sizeof(u32))
			return -EINVAL;

		dbg->buf = kzalloc(dbg->buf_len, GFP_KERNEL);

		if (!dbg->buf)
			return -ENOMEM;

		hwbuf = kzalloc(dbg->buf_len, GFP_KERNEL);
		if (!hwbuf) {
			kfree(dbg->buf);
			return -ENOMEM;
		}
		hwbuf_cur = hwbuf;

		ioptr = dbg->base + dbg->off;
		tot = 0;

		_sde_dbg_enable_power(true);

		memcpy_fromio(hwbuf, ioptr, dbg->buf_len);

		_sde_dbg_enable_power(false);

		for (cnt = dbg->cnt; cnt > 0; cnt -= ROW_BYTES) {
			hex_dump_to_buffer(hwbuf_cur,
					   min(cnt, ROW_BYTES),
					   ROW_BYTES, GROUP_BYTES, dump_buf,
					   sizeof(dump_buf), false);
			len = scnprintf(dbg->buf + tot, dbg->buf_len - tot,
					"0x%08x: %s\n",
					((int) (unsigned long) hwbuf_cur) -
					((int) (unsigned long) dbg->base),
					dump_buf);

			hwbuf_cur += ROW_BYTES;
			tot += len;
			if (tot >= dbg->buf_len)
				break;
		}

		dbg->buf_len = tot;
		kfree(hwbuf);
	}

	if (*ppos >= dbg->buf_len)
		return 0; /* done reading */

	len = min(count, dbg->buf_len - (size_t) *ppos);
	if (copy_to_user(user_buf, dbg->buf + *ppos, len)) {
		pr_err("failed to copy to user\n");
		return -EFAULT;
	}

	*ppos += len; /* increase offset */

	return len;
}

static const struct file_operations sde_off_fops = {
	.open = sde_dbg_debugfs_open,
	.release = sde_dbg_reg_base_release,
	.read = sde_dbg_reg_base_offset_read,
	.write = sde_dbg_reg_base_offset_write,
};

static const struct file_operations sde_reg_fops = {
	.open = sde_dbg_debugfs_open,
	.release = sde_dbg_reg_base_release,
	.read = sde_dbg_reg_base_reg_read,
	.write = sde_dbg_reg_base_reg_write,
};

int sde_dbg_reg_register_base(const char *name, void __iomem *base,
		size_t max_offset)
{
	struct sde_dbg_base *dbg_base = &sde_dbg_base;
	struct sde_dbg_reg_base *reg_base;
	struct dentry *ent_off, *ent_reg;
	char dn[80] = "";
	int prefix_len = 0;

	reg_base = kzalloc(sizeof(*reg_base), GFP_KERNEL);
	if (!reg_base)
		return -ENOMEM;

	if (name)
		strlcpy(reg_base->name, name, sizeof(reg_base->name));
	reg_base->base = base;
	reg_base->max_offset = max_offset;
	reg_base->off = 0;
	reg_base->cnt = DEFAULT_BASE_REG_CNT;
	reg_base->reg_dump = NULL;

	if (name)
		prefix_len = snprintf(dn, sizeof(dn), "%s_", name);
	strlcpy(dn + prefix_len, "off", sizeof(dn) - prefix_len);
	ent_off = debugfs_create_file(dn, 0600, dbg_base->root, reg_base,
			&sde_off_fops);
	if (IS_ERR_OR_NULL(ent_off)) {
		pr_err("debugfs_create_file: offset fail\n");
		goto off_fail;
	}

	strlcpy(dn + prefix_len, "reg", sizeof(dn) - prefix_len);
	ent_reg = debugfs_create_file(dn, 0600, dbg_base->root, reg_base,
			&sde_reg_fops);
	if (IS_ERR_OR_NULL(ent_reg)) {
		pr_err("debugfs_create_file: reg fail\n");
		goto reg_fail;
	}

	/* Initialize list to make sure check for null list will be valid */
	INIT_LIST_HEAD(&reg_base->sub_range_list);

	pr_debug("%s base: %pK max_offset 0x%zX\n", reg_base->name,
			reg_base->base, reg_base->max_offset);

	list_add(&reg_base->reg_base_head, &dbg_base->reg_base_list);

	return 0;
reg_fail:
	debugfs_remove(ent_off);
off_fail:
	kfree(reg_base);
	return -ENODEV;
}

void sde_dbg_reg_register_dump_range(const char *base_name,
		const char *range_name, u32 offset_start, u32 offset_end,
		uint32_t xin_id)
{
	struct sde_dbg_reg_base *reg_base;
	struct sde_dbg_reg_range *range;

	reg_base = _sde_dump_get_blk_addr(base_name);
	if (!reg_base) {
		pr_err("error: for range %s unable to locate base %s\n",
				range_name, base_name);
		return;
	}

	range = kzalloc(sizeof(*range), GFP_KERNEL);
	if (!range)
		return;

	strlcpy(range->range_name, range_name, sizeof(range->range_name));
	range->offset.start = offset_start;
	range->offset.end = offset_end;
	range->xin_id = xin_id;
	list_add_tail(&range->head, &reg_base->sub_range_list);

	pr_debug("%s start: 0x%X end: 0x%X\n", range->range_name,
			range->offset.start, range->offset.end);
}
