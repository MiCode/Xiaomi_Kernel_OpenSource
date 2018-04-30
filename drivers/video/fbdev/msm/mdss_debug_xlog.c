/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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
#define XLOG_DEFAULT_DBGBUSDUMP 0x2 /* dump in RAM */
#define XLOG_DEFAULT_VBIF_DBGBUSDUMP 0x2 /* dump in RAM */
#define XLOG_DEFAULT_DSI_DBGBUSDUMP 0x2 /* dump in RAM */

/*
 * xlog will print this number of entries when it is called through
 * sysfs node or panic. This prevents kernel log from xlog message
 * flood.
 */
#define MDSS_XLOG_PRINT_ENTRY	256

/*
 * xlog keeps this number of entries in memory for debug purpose. This
 * number must be greater than print entry to prevent out of bound xlog
 * entry array access.
 */
#define MDSS_XLOG_ENTRY	(MDSS_XLOG_PRINT_ENTRY * 4)
#define MDSS_XLOG_MAX_DATA 15
#define MDSS_XLOG_BUF_MAX 512
#define MDSS_XLOG_BUF_ALIGN 32

DEFINE_SPINLOCK(xlock);

struct tlog {
	u32 counter;
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
	u32 curr;
	struct dentry *xlog;
	u32 xlog_enable;
	u32 panic_on_err;
	u32 enable_reg_dump;
	u32 enable_dbgbus_dump;
	u32 enable_vbif_dbgbus_dump;
	u32 enable_dsi_dbgbus_dump;
	struct work_struct xlog_dump_work;
	struct mdss_debug_base *blk_arr[MDSS_DEBUG_BASE_MAX];
	bool work_panic;
	bool work_dbgbus;
	bool work_vbif_dbgbus;
	bool work_dsi_dbgbus;
	u32 *dbgbus_dump; /* address for the debug bus dump */
	u32 *vbif_dbgbus_dump; /* address for the vbif debug bus dump */
	u32 *nrt_vbif_dbgbus_dump; /* address for the nrt vbif debug bus dump */
	u32 *dsi_dbgbus_dump; /* address for the dsi debug bus dump */
} mdss_dbg_xlog;

static inline bool mdss_xlog_is_enabled(u32 flag)
{
	return (flag & mdss_dbg_xlog.xlog_enable) ||
		(flag == MDSS_XLOG_ALL && mdss_dbg_xlog.xlog_enable);
}

static void __halt_vbif_xin(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	pr_err("Halting VBIF-XIN\n");
	MDSS_VBIF_WRITE(mdata, MMSS_VBIF_XIN_HALT_CTRL0, 0xFFFFFFFF, false);
}

static void __halt_vbif_axi(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	pr_err("Halting VBIF-AXI\n");
	MDSS_VBIF_WRITE(mdata, MMSS_VBIF_AXI_HALT_CTRL0, 0xFFFFFFFF, false);
}

static void __dump_vbif_state(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	unsigned int reg_vbif_src_err, reg_vbif_err_info,
		reg_vbif_xin_halt_ctrl0, reg_vbif_xin_halt_ctrl1,
		reg_vbif_axi_halt_ctrl0, reg_vbif_axi_halt_ctrl1;

	reg_vbif_src_err = MDSS_VBIF_READ(mdata,
					MMSS_VBIF_SRC_ERR, false);
	reg_vbif_err_info = MDSS_VBIF_READ(mdata,
					MMSS_VBIF_ERR_INFO, false);
	reg_vbif_xin_halt_ctrl0 = MDSS_VBIF_READ(mdata,
					MMSS_VBIF_XIN_HALT_CTRL0, false);
	reg_vbif_xin_halt_ctrl1 = MDSS_VBIF_READ(mdata,
					MMSS_VBIF_XIN_HALT_CTRL1, false);
	reg_vbif_axi_halt_ctrl0 = MDSS_VBIF_READ(mdata,
					MMSS_VBIF_AXI_HALT_CTRL0, false);
	reg_vbif_axi_halt_ctrl1 = MDSS_VBIF_READ(mdata,
					MMSS_VBIF_AXI_HALT_CTRL1, false);
	pr_err("VBIF SRC_ERR=%x, ERR_INFO=%x\n",
				reg_vbif_src_err, reg_vbif_err_info);
	pr_err("VBIF XIN_HALT_CTRL0=%x, XIN_HALT_CTRL1=%x, AXI_HALT_CTRL0=%x, AXI_HALT_CTRL1=%x\n"
			, reg_vbif_xin_halt_ctrl0, reg_vbif_xin_halt_ctrl1,
			reg_vbif_axi_halt_ctrl0, reg_vbif_axi_halt_ctrl1);
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
	log = &mdss_dbg_xlog.logs[mdss_dbg_xlog.curr];
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
	mdss_dbg_xlog.curr = (mdss_dbg_xlog.curr + 1) % MDSS_XLOG_ENTRY;
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

	if ((xlog->last - xlog->first) > MDSS_XLOG_PRINT_ENTRY) {
		pr_warn("xlog buffer overflow before dump: %d\n",
			xlog->last - xlog->first);
		xlog->first = xlog->last - MDSS_XLOG_PRINT_ENTRY;
	}
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
	u32 offset;

	if (!(mdata->dbg_bus && list_size))
		return;

	/* will keep in memory 4 entries of 4 bytes each */
	list_size = (list_size * 4 * 4);

	in_log = (bus_dump_flag & MDSS_DBG_DUMP_IN_LOG);
	in_mem = (bus_dump_flag & MDSS_DBG_DUMP_IN_MEM);

	pr_info("======== Debug bus DUMP =========\n");

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(&mdata->pdev->dev,
				list_size, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			pr_info("%s: start_addr:0x%pK end_addr:0x%pK\n",
				__func__, dump_addr, dump_addr + list_size);
		} else {
			in_mem = false;
			pr_err("dump_mem: allocation fails\n");
		}
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	for (i = 0; i < mdata->dbg_bus_size; i++) {
		head = mdata->dbg_bus + i;
		writel_relaxed(TEST_MASK(head->block_id, head->test_id),
				mdss_res->mdp_base + head->wr_addr);
		wmb(); /* make sure test bits were written */

		if (mdata->dbg_bus_flags & DEBUG_FLAGS_DSPP)
			offset = MDSS_MDP_DSPP_DEBUGBUS_STATUS;
		else
			offset = head->wr_addr + 0x4;

		status = readl_relaxed(mdss_res->mdp_base +
			offset);

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

		/* Disable debug bus once we are done */
		writel_relaxed(0, mdss_res->mdp_base + head->wr_addr);

	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	pr_info("========End Debug bus=========\n");
}

static void __vbif_debug_bus(struct vbif_debug_bus *head,
	void __iomem *vbif_base, u32 *dump_addr, bool in_log)
{
	int i, j;
	u32 val;

	if (!dump_addr && !in_log)
		return;

	for (i = 0; i < head->block_cnt; i++) {
		writel_relaxed(1 << (i + head->bit_offset),
				vbif_base + head->block_bus_addr);
		/* make sure that current bus blcok enable */
		wmb();
		for (j = head->test_pnt_start; j < head->test_pnt_cnt; j++) {
			writel_relaxed(j, vbif_base + head->block_bus_addr + 4);
			/* make sure that test point is enabled */
			wmb();
			val = readl_relaxed(vbif_base + MMSS_VBIF_TEST_BUS_OUT);
			if (dump_addr) {
				*dump_addr++ = head->block_bus_addr;
				*dump_addr++ = i;
				*dump_addr++ = j;
				*dump_addr++ = val;
			}
			if (in_log)
				pr_err("testpoint:%x arb/xin id=%d index=%d val=0x%x\n",
					head->block_bus_addr, i, j, val);
		}
	}
}

static void mdss_dump_vbif_debug_bus(u32 bus_dump_flag,
	u32 **dump_mem, bool real_time)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	u32 value;
	struct vbif_debug_bus *head;
	phys_addr_t phys = 0;
	int i, list_size = 0;
	void __iomem *vbif_base;
	struct vbif_debug_bus *dbg_bus;
	u32 bus_size;

	if (real_time) {
		pr_info("======== VBIF Debug bus DUMP =========\n");
		vbif_base = mdata->vbif_io.base;
		dbg_bus = mdata->vbif_dbg_bus;
		bus_size = mdata->vbif_dbg_bus_size;
	} else {
		pr_info("======== NRT VBIF Debug bus DUMP =========\n");
		vbif_base = mdata->vbif_nrt_io.base;
		dbg_bus = mdata->nrt_vbif_dbg_bus;
		bus_size = mdata->nrt_vbif_dbg_bus_size;
	}

	if (!vbif_base || !dbg_bus || !bus_size)
		return;

	/* allocate memory for each test point */
	for (i = 0; i < bus_size; i++) {
		head = dbg_bus + i;
		list_size += (head->block_cnt * head->test_pnt_cnt);
	}

	/* 4 bytes * 4 entries for each test point*/
	list_size *= 16;

	in_log = (bus_dump_flag & MDSS_DBG_DUMP_IN_LOG);
	in_mem = (bus_dump_flag & MDSS_DBG_DUMP_IN_MEM);

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(&mdata->pdev->dev,
				list_size, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			pr_info("%s: start_addr:0x%pK end_addr:0x%pK\n",
				__func__, dump_addr, dump_addr + list_size);
		} else {
			in_mem = false;
			pr_err("dump_mem: allocation fails\n");
		}
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	value = readl_relaxed(vbif_base + MMSS_VBIF_CLKON);
	writel_relaxed(value | BIT(1), vbif_base + MMSS_VBIF_CLKON);

	/* make sure that vbif core is on */
	wmb();

	for (i = 0; i < bus_size; i++) {
		head = dbg_bus + i;

		writel_relaxed(0, vbif_base + head->disable_bus_addr);
		writel_relaxed(BIT(0), vbif_base + MMSS_VBIF_TEST_BUS_OUT_CTRL);
		/* make sure that other bus is off */
		wmb();

		__vbif_debug_bus(head, vbif_base, dump_addr, in_log);
		if (dump_addr)
			dump_addr += (head->block_cnt * head->test_pnt_cnt * 4);
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	pr_info("========End VBIF Debug bus=========\n");
}

void mdss_dump_reg(const char *dump_name, u32 reg_dump_flag, char *addr,
	int len, u32 **dump_mem, bool from_isr)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	phys_addr_t phys = 0;
	int i;

	in_log = (reg_dump_flag & MDSS_DBG_DUMP_IN_LOG);
	in_mem = (reg_dump_flag & MDSS_DBG_DUMP_IN_MEM);

	pr_debug("reg_dump_flag=%d in_log=%d in_mem=%d\n",
		reg_dump_flag, in_log, in_mem);

	if (len % 16)
		len += 16;
	len /= 16;

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(&mdata->pdev->dev,
				len * 16, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			pr_info("%s: start_addr:0x%pK end_addr:0x%pK reg_addr=0x%pK\n",
				dump_name, dump_addr, dump_addr + (u32)len * 16,
				addr);
		} else {
			in_mem = false;
			pr_err("dump_mem: kzalloc fails!\n");
		}
	}

	if (!from_isr)
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

	if (!from_isr)
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
			pr_debug("%s: range_base=0x%pK start=0x%x end=0x%x\n",
				xlog_node->range_name,
				addr, xlog_node->offset.start,
				xlog_node->offset.end);
			mdss_dump_reg((const char *)xlog_node->range_name,
				reg_dump_flag, addr, len, &xlog_node->reg_dump,
				false);
		}
	} else {
		/* If there is no list to dump ranges, dump all registers */
		pr_info("Ranges not found, will dump full registers");
		pr_info("base:0x%pK len:%zu\n", dbg->base, dbg->max_offset);
		addr = dbg->base;
		len = dbg->max_offset;
		mdss_dump_reg((const char *)dbg->name, reg_dump_flag, addr,
			len, &dbg->reg_dump, false);
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
		if (strlen(blk_base->name) &&
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
		if (strlen(blk_base->name))
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
		if (strlen(blk_base->name) &&
			!strcmp(blk_base->name, blk_name))
				return blk_base;
	}

	return NULL;
}

static void mdss_xlog_dump_array(struct mdss_debug_base *blk_arr[],
	u32 len, bool dead, const char *name, bool dump_dbgbus,
	bool dump_vbif_dbgbus, bool dump_dsi_dbgbus)
{
	int i;

	for (i = 0; i < len; i++) {
		if (blk_arr[i] != NULL)
			mdss_dump_reg_by_ranges(blk_arr[i],
				mdss_dbg_xlog.enable_reg_dump);
	}

	mdss_xlog_dump_all();

	if (dump_dbgbus)
		mdss_dump_debug_bus(mdss_dbg_xlog.enable_dbgbus_dump,
			&mdss_dbg_xlog.dbgbus_dump);

	if (dump_vbif_dbgbus) {
		mdss_dump_vbif_debug_bus(mdss_dbg_xlog.enable_vbif_dbgbus_dump,
			&mdss_dbg_xlog.vbif_dbgbus_dump, true);

		mdss_dump_vbif_debug_bus(mdss_dbg_xlog.enable_vbif_dbgbus_dump,
			&mdss_dbg_xlog.nrt_vbif_dbgbus_dump, false);
	}

	if (dump_dsi_dbgbus)
		mdss_dump_dsi_debug_bus(mdss_dbg_xlog.enable_dsi_dbgbus_dump,
			&mdss_dbg_xlog.dsi_dbgbus_dump);

	if (dead && mdss_dbg_xlog.panic_on_err) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		__dump_vbif_state();
		__halt_vbif_xin();
		usleep_range(10000, 10010);
		__halt_vbif_axi();
		usleep_range(10000, 10010);
		__dump_vbif_state();
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		panic(name);
	}
}

static void xlog_debug_work(struct work_struct *work)
{

	mdss_xlog_dump_array(mdss_dbg_xlog.blk_arr,
		ARRAY_SIZE(mdss_dbg_xlog.blk_arr),
		mdss_dbg_xlog.work_panic, "xlog_workitem",
		mdss_dbg_xlog.work_dbgbus,
		mdss_dbg_xlog.work_vbif_dbgbus,
		mdss_dbg_xlog.work_dsi_dbgbus);
}

void mdss_xlog_tout_handler_default(bool queue, const char *name, ...)
{
	int i, index = 0;
	bool dead = false;
	bool dump_dbgbus = false, dump_vbif_dbgbus = false;
	bool dump_dsi_dbgbus = false;
	va_list args;
	char *blk_name = NULL;
	struct mdss_debug_base *blk_base = NULL;
	struct mdss_debug_base **blk_arr;
	u32 blk_len;

	if (!mdss_xlog_is_enabled(MDSS_XLOG_DEFAULT))
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

		if (!strcmp(blk_name, "dbg_bus"))
			dump_dbgbus = true;

		if (!strcmp(blk_name, "vbif_dbg_bus"))
			dump_vbif_dbgbus = true;

		if (!strcmp(blk_name, "dsi_dbg_bus"))
			dump_dsi_dbgbus = true;

		if (!strcmp(blk_name, "panic"))
			dead = true;
	}
	va_end(args);

	if (queue) {
		/* schedule work to dump later */
		mdss_dbg_xlog.work_panic = dead;
		mdss_dbg_xlog.work_dbgbus = dump_dbgbus;
		mdss_dbg_xlog.work_vbif_dbgbus = dump_vbif_dbgbus;
		mdss_dbg_xlog.work_dsi_dbgbus = dump_dsi_dbgbus;
		schedule_work(&mdss_dbg_xlog.xlog_dump_work);
	} else {
		mdss_xlog_dump_array(blk_arr, blk_len, dead, name, dump_dbgbus,
			dump_vbif_dbgbus, dump_dsi_dbgbus);
	}
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
		if (len < 0 || len > count) {
			pr_err("len is more than the size of user buffer\n");
			return 0;
		}

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
	int i;

	mdss_dbg_xlog.xlog = debugfs_create_dir("xlog", mdd->root);
	if (IS_ERR_OR_NULL(mdss_dbg_xlog.xlog)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(mdss_dbg_xlog.xlog));
		mdss_dbg_xlog.xlog = NULL;
		return -ENODEV;
	}

	INIT_WORK(&mdss_dbg_xlog.xlog_dump_work, xlog_debug_work);
	mdss_dbg_xlog.work_panic = false;

	for (i = 0; i < MDSS_XLOG_ENTRY; i++)
		mdss_dbg_xlog.logs[i].counter = i;

	debugfs_create_file("dump", 0644, mdss_dbg_xlog.xlog, NULL,
						&mdss_xlog_fops);
	debugfs_create_u32("enable", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.xlog_enable);
	debugfs_create_u32("panic", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.panic_on_err);
	debugfs_create_u32("reg_dump", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.enable_reg_dump);
	debugfs_create_u32("dbgbus_dump", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.enable_dbgbus_dump);
	debugfs_create_u32("vbif_dbgbus_dump", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.enable_vbif_dbgbus_dump);

	mdss_dbg_xlog.xlog_enable = XLOG_DEFAULT_ENABLE;
	mdss_dbg_xlog.panic_on_err = XLOG_DEFAULT_PANIC;
	mdss_dbg_xlog.enable_reg_dump = XLOG_DEFAULT_REGDUMP;
	mdss_dbg_xlog.enable_dbgbus_dump = XLOG_DEFAULT_DBGBUSDUMP;
	mdss_dbg_xlog.enable_vbif_dbgbus_dump = XLOG_DEFAULT_VBIF_DBGBUSDUMP;
	mdss_dbg_xlog.enable_dsi_dbgbus_dump = XLOG_DEFAULT_DSI_DBGBUSDUMP;

	pr_info("xlog_status: enable:%d, panic:%d, dump:%d\n",
		mdss_dbg_xlog.xlog_enable, mdss_dbg_xlog.panic_on_err,
		mdss_dbg_xlog.enable_reg_dump);

	return 0;
}
