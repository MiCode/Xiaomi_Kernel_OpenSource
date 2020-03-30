// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include "sde_rotator_debug.h"
#include "sde_rotator_base.h"
#include "sde_rotator_core.h"
#include "sde_rotator_dev.h"
#include "sde_rotator_trace.h"

#ifdef CONFIG_MSM_SDE_ROTATOR_EVTLOG_DEBUG
#define SDE_EVTLOG_DEFAULT_ENABLE 1
#else
#define SDE_EVTLOG_DEFAULT_ENABLE 0
#endif
#define SDE_EVTLOG_DEFAULT_PANIC 1
#define SDE_EVTLOG_DEFAULT_REGDUMP SDE_ROT_DBG_DUMP_IN_MEM
#define SDE_EVTLOG_DEFAULT_VBIF_DBGBUSDUMP SDE_ROT_DBG_DUMP_IN_MEM
#define SDE_EVTLOG_DEFAULT_ROT_DBGBUSDUMP SDE_ROT_DBG_DUMP_IN_MEM

/*
 * evtlog will print this number of entries when it is called through
 * sysfs node or panic. This prevents kernel log from evtlog message
 * flood.
 */
#define SDE_ROT_EVTLOG_PRINT_ENTRY	256

/*
 * evtlog keeps this number of entries in memory for debug purpose. This
 * number must be greater than print entry to prevent out of bound evtlog
 * entry array access.
 */
#define SDE_ROT_EVTLOG_ENTRY	(SDE_ROT_EVTLOG_PRINT_ENTRY * 4)
#define SDE_ROT_EVTLOG_MAX_DATA 15
#define SDE_ROT_EVTLOG_BUF_MAX 512
#define SDE_ROT_EVTLOG_BUF_ALIGN 32
#define SDE_ROT_DEBUG_BASE_MAX 10

#define SDE_ROT_DEFAULT_BASE_REG_CNT 0x100
#define GROUP_BYTES 4
#define ROW_BYTES 16

#define SDE_ROT_TEST_MASK(id, tp)	((id << 4) | (tp << 1) | BIT(0))

static DEFINE_SPINLOCK(sde_rot_xlock);

/*
 * tlog - EVTLOG entry structure
 * @counter - EVTLOG entriy counter
 * @time - timestamp of EVTLOG entry
 * @name - function name of EVTLOG entry
 * @line - line number of EVTLOG entry
 * @data - EVTLOG data contents
 * @data_cnt - number of data contents
 * @pid - pid of current calling thread
 */
struct tlog {
	u32 counter;
	s64 time;
	const char *name;
	int line;
	u32 data[SDE_ROT_EVTLOG_MAX_DATA];
	u32 data_cnt;
	int pid;
};

/*
 * sde_rot_dbg_evtlog - EVTLOG debug data structure
 * @logs - EVTLOG entries
 * @first - first entry index in the EVTLOG
 * @last - last entry index in the EVTLOG
 * @curr - curr entry index in the EVTLOG
 * @evtlog - EVTLOG debugfs handle
 * @evtlog_enable - boolean indicates EVTLOG enable/disable
 * @panic_on_err - boolean indicates issue panic after EVTLOG dump
 * @enable_reg_dump - control in-log/memory dump for rotator registers
 * @enable_vbif_dbgbus_dump - control in-log/memory dump for VBIF debug bus
 * @enable_rot_dbgbus_dump - control in-log/memroy dump for rotator debug bus
 * @evtlog_dump_work - schedule work strucutre for timeout handler
 * @work_dump_reg - storage for register dump control in schedule work
 * @work_panic - storage for panic control in schedule work
 * @work_vbif_dbgbus - storage for VBIF debug bus control in schedule work
 * @work_rot_dbgbus - storage for rotator debug bus control in schedule work
 * @nrt_vbif_dbgbus_dump - memory buffer for VBIF debug bus dumping
 * @rot_dbgbus_dump - memory buffer for rotator debug bus dumping
 * @reg_dump_array - memory buffer for rotator registers dumping
 */
struct sde_rot_dbg_evtlog {
	struct tlog logs[SDE_ROT_EVTLOG_ENTRY];
	u32 first;
	u32 last;
	u32 curr;
	struct dentry *evtlog;
	u32 evtlog_enable;
	u32 panic_on_err;
	u32 enable_reg_dump;
	u32 enable_vbif_dbgbus_dump;
	u32 enable_rot_dbgbus_dump;
	struct work_struct evtlog_dump_work;
	bool work_dump_reg;
	bool work_panic;
	bool work_vbif_dbgbus;
	bool work_rot_dbgbus;
	u32 *nrt_vbif_dbgbus_dump; /* address for the nrt vbif debug bus dump */
	u32 *rot_dbgbus_dump;
	u32 *reg_dump_array[SDE_ROT_DEBUG_BASE_MAX];
} sde_rot_dbg_evtlog;

static void sde_rot_dump_debug_bus(u32 bus_dump_flag, u32 **dump_mem)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	u32 status = 0;
	struct sde_rot_debug_bus *head;
	phys_addr_t phys = 0;
	int i;
	u32 offset;
	void __iomem *base;

	in_log = (bus_dump_flag & SDE_ROT_DBG_DUMP_IN_LOG);
	in_mem = (bus_dump_flag & SDE_ROT_DBG_DUMP_IN_MEM);
	base = mdata->sde_io.base;

	if (!base || !mdata->rot_dbg_bus || !mdata->rot_dbg_bus_size)
		return;

	pr_info("======== SDE Rotator Debug bus DUMP =========\n");

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(&mdata->pdev->dev,
				mdata->rot_dbg_bus_size * 4 * sizeof(u32),
				&phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			pr_info("%s: start_addr:0x%pK end_addr:0x%pK\n",
				__func__, dump_addr,
				dump_addr + (u32)mdata->rot_dbg_bus_size * 16);
		} else {
			in_mem = false;
			pr_err("dump_mem: allocation fails\n");
		}
	}

	sde_smmu_ctrl(1);

	for (i = 0; i < mdata->rot_dbg_bus_size; i++) {
		head = mdata->rot_dbg_bus + i;
		writel_relaxed(SDE_ROT_TEST_MASK(head->block_id, head->test_id),
				base + head->wr_addr);
		wmb(); /* make sure test bits were written */

		offset = head->wr_addr + 0x4;

		status = readl_relaxed(base + offset);

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
		writel_relaxed(0, base + head->wr_addr);
	}

	sde_smmu_ctrl(0);

	pr_info("========End Debug bus=========\n");
}

/*
 * sde_rot_evtlog_is_enabled - helper function for checking EVTLOG
 *                             enable/disable
 * @flag - EVTLOG option flag
 */
static inline bool sde_rot_evtlog_is_enabled(u32 flag)
{
	return (flag & sde_rot_dbg_evtlog.evtlog_enable) ||
		(flag == SDE_ROT_EVTLOG_ALL &&
		 sde_rot_dbg_evtlog.evtlog_enable);
}

/*
 * __vbif_debug_bus - helper function for VBIF debug bus dump
 * @head - VBIF debug bus data structure
 * @vbif_base - VBIF IO mapped address
 * @dump_addr - output buffer for memory dump option
 * @in_log - boolean indicates in-log dump option
 */
static void __vbif_debug_bus(struct sde_rot_vbif_debug_bus *head,
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
		for (j = 0; j < head->test_pnt_cnt; j++) {
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

/*
 * sde_rot_dump_vbif_debug_bus - VBIF debug bus dump
 * @bus_dump_flag - dump flag controlling in-log/memory dump option
 * @dump_mem - output buffer for memory dump location
 */
static void sde_rot_dump_vbif_debug_bus(u32 bus_dump_flag,
	u32 **dump_mem)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	u32 value;
	struct sde_rot_vbif_debug_bus *head;
	phys_addr_t phys = 0;
	int i, list_size = 0;
	void __iomem *vbif_base;
	struct sde_rot_vbif_debug_bus *dbg_bus;
	u32 bus_size;

	pr_info("======== NRT VBIF Debug bus DUMP =========\n");
	vbif_base = mdata->vbif_nrt_io.base;
	dbg_bus = mdata->nrt_vbif_dbg_bus;
	bus_size = mdata->nrt_vbif_dbg_bus_size;

	if (!vbif_base || !dbg_bus || !bus_size)
		return;

	/* allocate memory for each test point */
	for (i = 0; i < bus_size; i++) {
		head = dbg_bus + i;
		list_size += (head->block_cnt * head->test_pnt_cnt);
	}

	/* 4 bytes * 4 entries for each test point*/
	list_size *= 16;

	in_log = (bus_dump_flag & SDE_ROT_DBG_DUMP_IN_LOG);
	in_mem = (bus_dump_flag & SDE_ROT_DBG_DUMP_IN_MEM);

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

	sde_smmu_ctrl(1);

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

	sde_smmu_ctrl(0);

	pr_info("========End VBIF Debug bus=========\n");
}

/*
 * sde_rot_dump_reg - helper function for dumping rotator register set content
 * @dump_name - register set name
 * @reg_dump_flag - dumping flag controlling in-log/memory dump location
 * @access - access type, sde registers or vbif registers
 * @addr - starting address offset for dumping
 * @len - range of the register set
 * @dump_mem - output buffer for memory dump location option
 */
void sde_rot_dump_reg(const char *dump_name, u32 reg_dump_flag,
	enum sde_rot_regdump_access access, u32 addr,
	int len, u32 **dump_mem)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	phys_addr_t phys = 0;
	int i;
	void __iomem *base;

	in_log = (reg_dump_flag & SDE_ROT_DBG_DUMP_IN_LOG);
	in_mem = (reg_dump_flag & SDE_ROT_DBG_DUMP_IN_MEM);

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
			pr_info("%s: start_addr:0x%pK end_addr:0x%pK reg_addr=0x%X\n",
				dump_name, dump_addr, dump_addr + (u32)len * 16,
				addr);
		} else {
			in_mem = false;
			pr_err("dump_mem: kzalloc fails!\n");
		}
	}

	base = mdata->sde_io.base;
	/*
	 * VBIF NRT base handling
	 */
	if (access == SDE_ROT_REGDUMP_VBIF)
		base = mdata->vbif_nrt_io.base;

	for (i = 0; i < len; i++) {
		u32 x0, x4, x8, xc;

		x0 = readl_relaxed(base + addr+0x0);
		x4 = readl_relaxed(base + addr+0x4);
		x8 = readl_relaxed(base + addr+0x8);
		xc = readl_relaxed(base + addr+0xc);

		if (in_log)
			pr_info("0x%08X : %08x %08x %08x %08x\n",
					addr, x0, x4, x8, xc);

		if (dump_addr && in_mem) {
			dump_addr[i*4] = x0;
			dump_addr[i*4 + 1] = x4;
			dump_addr[i*4 + 2] = x8;
			dump_addr[i*4 + 3] = xc;
		}

		addr += 16;
	}
}

/*
 * sde_rot_dump_reg_all - dumping all SDE rotator registers
 */
static void sde_rot_dump_reg_all(void)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_rot_regdump *head, *regdump;
	u32 regdump_size;
	int i;

	regdump = mdata->regdump;
	regdump_size = mdata->regdump_size;

	if (!regdump || !regdump_size)
		return;

	/* Enable clock to rotator if not yet enabled */
	sde_smmu_ctrl(1);

	for (i = 0; (i < regdump_size) && (i < SDE_ROT_DEBUG_BASE_MAX); i++) {
		head = &regdump[i];

		if (head->access == SDE_ROT_REGDUMP_WRITE) {
			if (head->len != 1) {
				SDEROT_ERR("invalid write len %u\n", head->len);
				continue;
			}
			writel_relaxed(head->value,
					mdata->sde_io.base + head->offset);
			/* Make sure write go through */
			wmb();
		} else {
			sde_rot_dump_reg(head->name,
					sde_rot_dbg_evtlog.enable_reg_dump,
					head->access,
					head->offset, head->len,
					&sde_rot_dbg_evtlog.reg_dump_array[i]);
		}
	}

	/* Disable rotator clock */
	sde_smmu_ctrl(0);
}

/*
 * __sde_rot_evtlog_dump_calc_range - calculate dump range for EVTLOG
 */
static bool __sde_rot_evtlog_dump_calc_range(void)
{
	static u32 next;
	bool need_dump = true;
	unsigned long flags;
	struct sde_rot_dbg_evtlog *evtlog = &sde_rot_dbg_evtlog;

	spin_lock_irqsave(&sde_rot_xlock, flags);

	evtlog->first = next;

	if (evtlog->last == evtlog->first) {
		need_dump = false;
		goto dump_exit;
	}

	if (evtlog->last < evtlog->first) {
		evtlog->first %= SDE_ROT_EVTLOG_ENTRY;
		if (evtlog->last < evtlog->first)
			evtlog->last += SDE_ROT_EVTLOG_ENTRY;
	}

	if ((evtlog->last - evtlog->first) > SDE_ROT_EVTLOG_PRINT_ENTRY) {
		pr_warn("evtlog buffer overflow before dump: %d\n",
			evtlog->last - evtlog->first);
		evtlog->first = evtlog->last - SDE_ROT_EVTLOG_PRINT_ENTRY;
	}
	next = evtlog->first + 1;

dump_exit:
	spin_unlock_irqrestore(&sde_rot_xlock, flags);

	return need_dump;
}

/*
 * sde_rot_evtlog_dump_entry - helper function for EVTLOG content dumping
 * @evtlog_buf: EVTLOG dump output buffer
 * @evtlog_buf_size: EVTLOG output buffer size
 */
static ssize_t sde_rot_evtlog_dump_entry(char *evtlog_buf,
		ssize_t evtlog_buf_size)
{
	int i;
	ssize_t off = 0;
	struct tlog *log, *prev_log;
	unsigned long flags;

	spin_lock_irqsave(&sde_rot_xlock, flags);

	log = &sde_rot_dbg_evtlog.logs[sde_rot_dbg_evtlog.first %
		SDE_ROT_EVTLOG_ENTRY];

	prev_log = &sde_rot_dbg_evtlog.logs[(sde_rot_dbg_evtlog.first - 1) %
		SDE_ROT_EVTLOG_ENTRY];

	off = snprintf((evtlog_buf + off), (evtlog_buf_size - off), "%s:%-4d",
		log->name, log->line);

	if (off < SDE_ROT_EVTLOG_BUF_ALIGN) {
		memset((evtlog_buf + off), 0x20,
				(SDE_ROT_EVTLOG_BUF_ALIGN - off));
		off = SDE_ROT_EVTLOG_BUF_ALIGN;
	}

	off += snprintf((evtlog_buf + off), (evtlog_buf_size - off),
		"=>[%-8d:%-11llu:%9llu][%-4d]:", sde_rot_dbg_evtlog.first,
		log->time, (log->time - prev_log->time), log->pid);

	for (i = 0; i < log->data_cnt; i++)
		off += snprintf((evtlog_buf + off), (evtlog_buf_size - off),
			"%x ", log->data[i]);

	off += snprintf((evtlog_buf + off), (evtlog_buf_size - off), "\n");

	spin_unlock_irqrestore(&sde_rot_xlock, flags);

	return off;
}

/*
 * sde_rot_evtlog_dump_all - Dumping all content in EVTLOG buffer
 */
static void sde_rot_evtlog_dump_all(void)
{
	char evtlog_buf[SDE_ROT_EVTLOG_BUF_MAX];

	while (__sde_rot_evtlog_dump_calc_range()) {
		sde_rot_evtlog_dump_entry(evtlog_buf, SDE_ROT_EVTLOG_BUF_MAX);
		pr_info("%s\n", evtlog_buf);
	}
}

/*
 * sde_rot_evtlog_dump_open - debugfs open handler for evtlog dump
 * @inode: debugfs inode
 * @file: file handler
 */
static int sde_rot_evtlog_dump_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

/*
 * sde_rot_evtlog_dump_read - debugfs read handler for evtlog dump
 * @file: file handler
 * @buff: user buffer content for debugfs
 * @count: size of user buffer
 * @ppos: position offset of user buffer
 */
static ssize_t sde_rot_evtlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char evtlog_buf[SDE_ROT_EVTLOG_BUF_MAX];

	if (__sde_rot_evtlog_dump_calc_range()) {
		len = sde_rot_evtlog_dump_entry(evtlog_buf,
				SDE_ROT_EVTLOG_BUF_MAX);
		if (len < 0 || len > count) {
			pr_err("len is more than the user buffer size\n");
			return 0;
		}

		if (copy_to_user(buff, evtlog_buf, len))
			return -EFAULT;
		*ppos += len;
	}

	return len;
}

/*
 * sde_rot_evtlog_dump_helper - helper function for evtlog dump
 * @dead: boolean indicates panic after dump
 * @panic_name: Panic signature name show up in log
 * @dump_rot: boolean indicates rotator register dump
 * @dump_vbif_debug_bus: boolean indicates VBIF debug bus dump
 */
static void sde_rot_evtlog_dump_helper(bool dead, const char *panic_name,
	bool dump_rot, bool dump_vbif_debug_bus, bool dump_rot_debug_bus)
{
	sde_rot_evtlog_dump_all();

	if (dump_rot_debug_bus)
		sde_rot_dump_debug_bus(
				sde_rot_dbg_evtlog.enable_rot_dbgbus_dump,
				&sde_rot_dbg_evtlog.rot_dbgbus_dump);

	if (dump_vbif_debug_bus)
		sde_rot_dump_vbif_debug_bus(
				sde_rot_dbg_evtlog.enable_vbif_dbgbus_dump,
				&sde_rot_dbg_evtlog.nrt_vbif_dbgbus_dump);

	/*
	 * Rotator registers always dump last
	 */
	if (dump_rot)
		sde_rot_dump_reg_all();

	if (dead)
		panic(panic_name);
}

/*
 * sde_rot_evtlog_debug_work - schedule work function for evtlog dump
 * @work: schedule work structure
 */
static void sde_rot_evtlog_debug_work(struct work_struct *work)
{
	sde_rot_evtlog_dump_helper(
		sde_rot_dbg_evtlog.work_panic,
		"evtlog_workitem",
		sde_rot_dbg_evtlog.work_dump_reg,
		sde_rot_dbg_evtlog.work_vbif_dbgbus,
		sde_rot_dbg_evtlog.work_rot_dbgbus);
}

/*
 * sde_rot_evtlog_tout_handler - log dump timeout handler
 * @queue: boolean indicate putting log dump into queue
 * @name: function name having timeout
 */
void sde_rot_evtlog_tout_handler(bool queue, const char *name, ...)
{
	int i;
	bool dead = false;
	bool dump_rot = false;
	bool dump_vbif_dbgbus = false;
	bool dump_rot_dbgbus = false;
	char *blk_name = NULL;
	va_list args;

	if (!sde_rot_evtlog_is_enabled(SDE_ROT_EVTLOG_DEFAULT))
		return;

	if (queue && work_pending(&sde_rot_dbg_evtlog.evtlog_dump_work))
		return;

	va_start(args, name);
	for (i = 0; i < SDE_ROT_EVTLOG_MAX_DATA; i++) {
		blk_name = va_arg(args, char*);
		if (IS_ERR_OR_NULL(blk_name))
			break;

		if (!strcmp(blk_name, "rot"))
			dump_rot = true;

		if (!strcmp(blk_name, "vbif_dbg_bus"))
			dump_vbif_dbgbus = true;

		if (!strcmp(blk_name, "rot_dbg_bus"))
			dump_rot_dbgbus = true;

		if (!strcmp(blk_name, "panic"))
			dead = true;
	}
	va_end(args);

	if (queue) {
		/* schedule work to dump later */
		sde_rot_dbg_evtlog.work_panic = dead;
		sde_rot_dbg_evtlog.work_dump_reg = dump_rot;
		sde_rot_dbg_evtlog.work_vbif_dbgbus = dump_vbif_dbgbus;
		sde_rot_dbg_evtlog.work_rot_dbgbus = dump_rot_dbgbus;
		schedule_work(&sde_rot_dbg_evtlog.evtlog_dump_work);
	} else {
		sde_rot_evtlog_dump_helper(dead, name, dump_rot,
			dump_vbif_dbgbus, dump_rot_dbgbus);
	}
}

/*
 * sde_rot_evtlog - log contents into memory for dump analysis
 * @name: Name of function calling evtlog
 * @line: line number of calling function
 * @flag: Log control flag
 */
void sde_rot_evtlog(const char *name, int line, int flag, ...)
{
	unsigned long flags;
	int i, val = 0;
	va_list args;
	struct tlog *log;

	if (!sde_rot_evtlog_is_enabled(flag))
		return;

	spin_lock_irqsave(&sde_rot_xlock, flags);
	log = &sde_rot_dbg_evtlog.logs[sde_rot_dbg_evtlog.curr];
	log->time = ktime_to_us(ktime_get());
	log->name = name;
	log->line = line;
	log->data_cnt = 0;
	log->pid = current->pid;

	va_start(args, flag);
	for (i = 0; i < SDE_ROT_EVTLOG_MAX_DATA; i++) {

		val = va_arg(args, int);
		if (val == SDE_ROT_DATA_LIMITER)
			break;

		log->data[i] = val;
	}
	va_end(args);
	log->data_cnt = i;
	sde_rot_dbg_evtlog.curr =
		(sde_rot_dbg_evtlog.curr + 1) % SDE_ROT_EVTLOG_ENTRY;
	sde_rot_dbg_evtlog.last++;

	trace_sde_rot_evtlog(name, line, log->data_cnt, log->data);

	spin_unlock_irqrestore(&sde_rot_xlock, flags);
}

/*
 * sde_rotator_stat_show - Show statistics on read to this debugfs file
 * @s: Pointer to sequence file structure
 * @data: Pointer to private data structure
 */
static int sde_rotator_stat_show(struct seq_file *s, void *data)
{
	int i, offset;
	struct sde_rotator_device *rot_dev = s->private;
	struct sde_rotator_statistics *stats = &rot_dev->stats;
	u64 count = stats->count;
	int num_events;
	s64 proc_max, proc_min, proc_avg;
	s64 swoh_max, swoh_min, swoh_avg;

	proc_max = 0;
	proc_min = S64_MAX;
	proc_avg = 0;
	swoh_max = 0;
	swoh_min = S64_MAX;
	swoh_avg = 0;

	if (count > SDE_ROTATOR_NUM_EVENTS) {
		num_events = SDE_ROTATOR_NUM_EVENTS;
		offset = count % SDE_ROTATOR_NUM_EVENTS;
	} else {
		num_events = count;
		offset = 0;
	}

	for (i = 0; i < num_events; i++) {
		int k = (offset + i) % SDE_ROTATOR_NUM_EVENTS;
		ktime_t *ts = stats->ts[k];
		ktime_t start_time =
			ktime_before(ts[SDE_ROTATOR_TS_SRCQB],
					ts[SDE_ROTATOR_TS_DSTQB]) ?
					ts[SDE_ROTATOR_TS_SRCQB] :
					ts[SDE_ROTATOR_TS_DSTQB];
		s64 proc_time =
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_RETIRE],
					start_time));
		s64 sw_overhead_time =
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_FLUSH],
					start_time));

		seq_printf(s,
			"s:%d sq:%lld dq:%lld fe:%lld q:%lld c:%lld st:%lld fl:%lld d:%lld sdq:%lld ddq:%lld t:%lld oht:%lld\n",
			i,
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_FENCE],
					ts[SDE_ROTATOR_TS_SRCQB])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_FENCE],
					ts[SDE_ROTATOR_TS_DSTQB])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_QUEUE],
					ts[SDE_ROTATOR_TS_FENCE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_COMMIT],
					ts[SDE_ROTATOR_TS_QUEUE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_START],
					ts[SDE_ROTATOR_TS_COMMIT])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_FLUSH],
					ts[SDE_ROTATOR_TS_START])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_DONE],
					ts[SDE_ROTATOR_TS_FLUSH])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_RETIRE],
					ts[SDE_ROTATOR_TS_DONE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_SRCDQB],
					ts[SDE_ROTATOR_TS_RETIRE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_DSTDQB],
					ts[SDE_ROTATOR_TS_RETIRE])),
			proc_time, sw_overhead_time);

		proc_max = max(proc_max, proc_time);
		proc_min = min(proc_min, proc_time);
		proc_avg += proc_time;

		swoh_max = max(swoh_max, sw_overhead_time);
		swoh_min = min(swoh_min, sw_overhead_time);
		swoh_avg += sw_overhead_time;
	}

	proc_avg = (num_events) ?
			DIV_ROUND_CLOSEST_ULL(proc_avg, num_events) : 0;
	swoh_avg = (num_events) ?
			DIV_ROUND_CLOSEST_ULL(swoh_avg, num_events) : 0;

	seq_printf(s, "count:%llu\n", count);
	seq_printf(s, "fai1:%llu\n", stats->fail_count);
	seq_printf(s, "t_max:%lld\n", proc_max);
	seq_printf(s, "t_min:%lld\n", proc_min);
	seq_printf(s, "t_avg:%lld\n", proc_avg);
	seq_printf(s, "swoh_max:%lld\n", swoh_max);
	seq_printf(s, "swoh_min:%lld\n", swoh_min);
	seq_printf(s, "swoh_avg:%lld\n", swoh_avg);

	return 0;
}

/*
 * sde_rotator_raw_show - Show raw statistics on read from this debugfs file
 * @s: Pointer to sequence file structure
 * @data: Pointer to private data structure
 */
static int sde_rotator_raw_show(struct seq_file *s, void *data)
{
	int i, j, offset;
	struct sde_rotator_device *rot_dev = s->private;
	struct sde_rotator_statistics *stats = &rot_dev->stats;
	u64 count = stats->count;
	int num_events;

	if (count > SDE_ROTATOR_NUM_EVENTS) {
		num_events = SDE_ROTATOR_NUM_EVENTS;
		offset = count % SDE_ROTATOR_NUM_EVENTS;
	} else {
		num_events = count;
		offset = 0;
	}

	for (i = 0; i < num_events; i++) {
		int k = (offset + i) % SDE_ROTATOR_NUM_EVENTS;
		ktime_t *ts = stats->ts[k];

		seq_printf(s, "%d ", i);
		for (j = 0; j < SDE_ROTATOR_NUM_TIMESTAMPS; j++)
			seq_printf(s, "%lld ", ktime_to_us(ts[j]));
		seq_puts(s, "\n");
	}

	return 0;
}

/*
 * sde_rotator_dbg_open - Processed statistics debugfs file open function
 * @inode:
 * @file:
 */
static int sde_rotator_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, sde_rotator_stat_show, inode->i_private);
}

/*
 * sde_rotator_dbg_open - Raw statistics debugfs file open function
 * @inode:
 * @file:
 */
static int sde_rotator_raw_open(struct inode *inode, struct file *file)
{
	return single_open(file, sde_rotator_raw_show, inode->i_private);
}

/*
 * sde_rotator_dbg_open - Raw statistics debugfs file open function
 * @mdata: Pointer to rotator global data
 * @debugfs_root: Pointer to parent debugfs node
 */
static int sde_rotator_base_create_debugfs(
		struct sde_rot_data_type *mdata,
		struct dentry *debugfs_root)
{
	if (!debugfs_create_u32("iommu_ref_cnt", 0444,
			debugfs_root, &mdata->iommu_ref_cnt)) {
		SDEROT_WARN("failed to create debugfs iommu ref cnt\n");
		return -EINVAL;
	}

	mdata->clk_always_on = false;
	if (!debugfs_create_bool("clk_always_on", 0644,
			debugfs_root, &mdata->clk_always_on)) {
		SDEROT_WARN("failed to create debugfs clk_always_on\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * sde_rotator_dbg_open - Raw statistics debugfs file open function
 * @mgr: Pointer to rotator manager structure
 * @debugfs_root: Pointer to parent debugfs node
 */
static int sde_rotator_core_create_debugfs(
		struct sde_rot_mgr *mgr,
		struct dentry *debugfs_root)
{
	int ret;

	if (!debugfs_create_u32("hwacquire_timeout", 0400,
			debugfs_root, &mgr->hwacquire_timeout)) {
		SDEROT_WARN("failed to create debugfs hw acquire timeout\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("ppc_numer", 0644,
			debugfs_root, &mgr->pixel_per_clk.numer)) {
		SDEROT_WARN("failed to create debugfs ppc numerator\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("ppc_denom", 0600,
			debugfs_root, &mgr->pixel_per_clk.denom)) {
		SDEROT_WARN("failed to create debugfs ppc denominator\n");
		return -EINVAL;
	}

	if (!debugfs_create_u64("enable_bw_vote", 0644,
			debugfs_root, &mgr->enable_bw_vote)) {
		SDEROT_WARN("failed to create enable_bw_vote\n");
		return -EINVAL;
	}

	if (mgr->ops_hw_create_debugfs) {
		ret = mgr->ops_hw_create_debugfs(mgr, debugfs_root);
		if (ret)
			return ret;
	}
	return 0;
}

static const struct file_operations sde_rot_evtlog_fops = {
	.open = sde_rot_evtlog_dump_open,
	.read = sde_rot_evtlog_dump_read,
};

static int sde_rotator_evtlog_create_debugfs(
		struct sde_rot_mgr *mgr,
		struct dentry *debugfs_root)
{
	int i;

	sde_rot_dbg_evtlog.evtlog = debugfs_create_dir("evtlog", debugfs_root);
	if (IS_ERR_OR_NULL(sde_rot_dbg_evtlog.evtlog)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(sde_rot_dbg_evtlog.evtlog));
		sde_rot_dbg_evtlog.evtlog = NULL;
		return -ENODEV;
	}

	INIT_WORK(&sde_rot_dbg_evtlog.evtlog_dump_work,
			sde_rot_evtlog_debug_work);
	sde_rot_dbg_evtlog.work_panic = false;

	for (i = 0; i < SDE_ROT_EVTLOG_ENTRY; i++)
		sde_rot_dbg_evtlog.logs[i].counter = i;

	debugfs_create_file("dump", 0644, sde_rot_dbg_evtlog.evtlog, NULL,
						&sde_rot_evtlog_fops);
	debugfs_create_u32("enable", 0644, sde_rot_dbg_evtlog.evtlog,
			    &sde_rot_dbg_evtlog.evtlog_enable);
	debugfs_create_u32("panic", 0644, sde_rot_dbg_evtlog.evtlog,
			    &sde_rot_dbg_evtlog.panic_on_err);
	debugfs_create_u32("reg_dump", 0644, sde_rot_dbg_evtlog.evtlog,
			    &sde_rot_dbg_evtlog.enable_reg_dump);
	debugfs_create_u32("vbif_dbgbus_dump", 0644, sde_rot_dbg_evtlog.evtlog,
			    &sde_rot_dbg_evtlog.enable_vbif_dbgbus_dump);
	debugfs_create_u32("rot_dbgbus_dump", 0644, sde_rot_dbg_evtlog.evtlog,
			    &sde_rot_dbg_evtlog.enable_rot_dbgbus_dump);

	sde_rot_dbg_evtlog.evtlog_enable = SDE_EVTLOG_DEFAULT_ENABLE;
	sde_rot_dbg_evtlog.panic_on_err = SDE_EVTLOG_DEFAULT_PANIC;
	sde_rot_dbg_evtlog.enable_reg_dump = SDE_EVTLOG_DEFAULT_REGDUMP;
	sde_rot_dbg_evtlog.enable_vbif_dbgbus_dump =
		SDE_EVTLOG_DEFAULT_VBIF_DBGBUSDUMP;
	sde_rot_dbg_evtlog.enable_rot_dbgbus_dump =
		SDE_EVTLOG_DEFAULT_ROT_DBGBUSDUMP;

	pr_info("evtlog_status: enable:%d, panic:%d, dump:%d\n",
			sde_rot_dbg_evtlog.evtlog_enable,
			sde_rot_dbg_evtlog.panic_on_err,
			sde_rot_dbg_evtlog.enable_reg_dump);

	return 0;
}

/*
 * struct sde_rotator_stat_ops - processed statistics file operations
 */
static const struct file_operations sde_rotator_stat_ops = {
	.open		= sde_rotator_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

/*
 * struct sde_rotator_raw_ops - raw statistics file operations
 */
static const struct file_operations sde_rotator_raw_ops = {
	.open		= sde_rotator_raw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int sde_rotator_debug_base_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static int sde_rotator_debug_base_release(struct inode *inode,
		struct file *file)
{
	struct sde_rotator_debug_base *dbg = file->private_data;

	if (dbg) {
		mutex_lock(&dbg->buflock);
		kfree(dbg->buf);
		dbg->buf_len = 0;
		dbg->buf = NULL;
		mutex_unlock(&dbg->buflock);
	}

	return 0;
}

static ssize_t sde_rotator_debug_base_offset_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_rotator_debug_base *dbg = file->private_data;
	u32 off = 0;
	u32 cnt = SDE_ROT_DEFAULT_BASE_REG_CNT;
	char buf[24];

	if (!dbg)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;

	if (sscanf(buf, "%5x %x", &off, &cnt) < 2)
		return -EINVAL;

	if (off % sizeof(u32))
		return -EINVAL;

	if (off > dbg->max_offset)
		return -EINVAL;

	if (cnt > (dbg->max_offset - off))
		cnt = dbg->max_offset - off;

	mutex_lock(&dbg->buflock);
	dbg->off = off;
	dbg->cnt = cnt;
	mutex_unlock(&dbg->buflock);

	SDEROT_DBG("offset=%x cnt=%x\n", off, cnt);

	return count;
}

static ssize_t sde_rotator_debug_base_offset_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct sde_rotator_debug_base *dbg = file->private_data;
	int len = 0;
	char buf[24] = {'\0'};

	if (!dbg)
		return -ENODEV;

	if (*ppos)
		return 0;	/* the end */

	mutex_lock(&dbg->buflock);
	len = snprintf(buf, sizeof(buf), "0x%08zx %zx\n", dbg->off, dbg->cnt);
	mutex_unlock(&dbg->buflock);

	if (len < 0 || len >= sizeof(buf))
		return 0;

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;	/* increase offset */

	return len;
}

static ssize_t sde_rotator_debug_base_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_rotator_debug_base *dbg = file->private_data;
	size_t off;
	u32 data, cnt;
	char buf[24];

	if (!dbg)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;

	cnt = sscanf(buf, "%zx %x", &off, &data);

	if (cnt < 2)
		return -EFAULT;

	if (off % sizeof(u32))
		return -EFAULT;

	if (off >= dbg->max_offset)
		return -EFAULT;

	mutex_lock(&dbg->buflock);

	/* Enable Clock for register access */
	sde_rot_mgr_lock(dbg->mgr);
	if (!sde_rotator_resource_ctrl_enabled(dbg->mgr)) {
		SDEROT_WARN("resource ctrl is not enabled\n");
		sde_rot_mgr_unlock(dbg->mgr);
		goto debug_write_error;
	}
	sde_rotator_clk_ctrl(dbg->mgr, true);

	writel_relaxed(data, dbg->base + off);

	/* Disable Clock after register access */
	sde_rotator_clk_ctrl(dbg->mgr, false);
	sde_rot_mgr_unlock(dbg->mgr);

	mutex_unlock(&dbg->buflock);

	SDEROT_DBG("addr=%zx data=%x\n", off, data);

	return count;

debug_write_error:
	mutex_unlock(&dbg->buflock);
	return 0;
}

static ssize_t sde_rotator_debug_base_reg_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_rotator_debug_base *dbg = file->private_data;
	size_t len;
	int rc = 0;

	if (!dbg) {
		SDEROT_ERR("invalid handle\n");
		return -ENODEV;
	}

	mutex_lock(&dbg->buflock);
	if (!dbg->buf) {
		char dump_buf[64];
		char *ptr;
		int cnt, tot;

		dbg->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(dbg->cnt, ROW_BYTES);
		dbg->buf = kzalloc(dbg->buf_len, GFP_KERNEL);

		if (!dbg->buf) {
			SDEROT_ERR("not enough memory to hold reg dump\n");
			rc = -ENOMEM;
			goto debug_read_error;
		}

		if (dbg->off % sizeof(u32)) {
			rc = -EFAULT;
			goto debug_read_error;
		}

		ptr = dbg->base + dbg->off;
		tot = 0;

		/* Enable clock for register access */
		sde_rot_mgr_lock(dbg->mgr);
		if (!sde_rotator_resource_ctrl_enabled(dbg->mgr)) {
			SDEROT_WARN("resource ctrl is not enabled\n");
			sde_rot_mgr_unlock(dbg->mgr);
			goto debug_read_error;
		}
		sde_rotator_clk_ctrl(dbg->mgr, true);

		for (cnt = dbg->cnt; cnt > 0; cnt -= ROW_BYTES) {
			hex_dump_to_buffer(ptr, min(cnt, ROW_BYTES),
					   ROW_BYTES, GROUP_BYTES, dump_buf,
					   sizeof(dump_buf), false);
			len = scnprintf(dbg->buf + tot, dbg->buf_len - tot,
					"0x%08x: %s\n",
					((int) (unsigned long) ptr) -
					((int) (unsigned long) dbg->base),
					dump_buf);

			ptr += ROW_BYTES;
			tot += len;
			if (tot >= dbg->buf_len)
				break;
		}
		/* Disable clock after register access */
		sde_rotator_clk_ctrl(dbg->mgr, false);
		sde_rot_mgr_unlock(dbg->mgr);

		dbg->buf_len = tot;
	}

	if (*ppos >= dbg->buf_len) {
		rc = 0; /* done reading */
		goto debug_read_error;
	}

	len = min(count, dbg->buf_len - (size_t) *ppos);
	if (copy_to_user(user_buf, dbg->buf + *ppos, len)) {
		SDEROT_ERR("failed to copy to user\n");
		rc = -EFAULT;
		goto debug_read_error;
	}

	*ppos += len; /* increase offset */

	mutex_unlock(&dbg->buflock);
	return len;

debug_read_error:
	mutex_unlock(&dbg->buflock);
	return rc;
}

static const struct file_operations sde_rotator_off_fops = {
	.open = sde_rotator_debug_base_open,
	.release = sde_rotator_debug_base_release,
	.read = sde_rotator_debug_base_offset_read,
	.write = sde_rotator_debug_base_offset_write,
};

static const struct file_operations sde_rotator_reg_fops = {
	.open = sde_rotator_debug_base_open,
	.release = sde_rotator_debug_base_release,
	.read = sde_rotator_debug_base_reg_read,
	.write = sde_rotator_debug_base_reg_write,
};

/*
 * sde_rotator_create_debugfs - Setup rotator debugfs directory structure.
 * @rot_dev: Pointer to rotator device
 */
struct dentry *sde_rotator_create_debugfs(
		struct sde_rotator_device *rot_dev)
{
	struct dentry *debugfs_root;
	char dirname[32] = {0};

	snprintf(dirname, sizeof(dirname), "%s%d",
			SDE_ROTATOR_DRV_NAME, rot_dev->dev->id);
	debugfs_root = debugfs_create_dir(dirname, NULL);
	if (!debugfs_root) {
		SDEROT_ERR("fail create debugfs root\n");
		return NULL;
	}

	if (!debugfs_create_file("stats", 0400,
		debugfs_root, rot_dev, &sde_rotator_stat_ops)) {
		SDEROT_ERR("fail create debugfs stats\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_file("raw", 0400,
		debugfs_root, rot_dev, &sde_rotator_raw_ops)) {
		SDEROT_ERR("fail create debugfs raw\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("fence_timeout", 0400,
			debugfs_root, &rot_dev->fence_timeout)) {
		SDEROT_ERR("fail create fence_timeout\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("open_timeout", 0400,
			debugfs_root, &rot_dev->open_timeout)) {
		SDEROT_ERR("fail create open_timeout\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("disable_syscache", 0400,
			debugfs_root, &rot_dev->disable_syscache)) {
		SDEROT_ERR("fail create disable_syscache\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("streamoff_timeout", 0400,
			debugfs_root, &rot_dev->streamoff_timeout)) {
		SDEROT_ERR("fail create streamoff_timeout\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("early_submit", 0400,
			debugfs_root, &rot_dev->early_submit)) {
		SDEROT_ERR("fail create early_submit\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (sde_rotator_base_create_debugfs(rot_dev->mdata, debugfs_root)) {
		SDEROT_ERR("fail create base debugfs\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (sde_rotator_core_create_debugfs(rot_dev->mgr, debugfs_root)) {
		SDEROT_ERR("fail create core debugfs\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (sde_rotator_evtlog_create_debugfs(rot_dev->mgr, debugfs_root)) {
		SDEROT_ERR("fail create evtlog debugfs\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	return debugfs_root;
}

/*
 * sde_rotator_destroy_debugfs - Destroy rotator debugfs directory structure.
 * @rot_dev: Pointer to rotator debugfs
 */
void sde_rotator_destroy_debugfs(struct dentry *debugfs)
{
	debugfs_remove_recursive(debugfs);
}
