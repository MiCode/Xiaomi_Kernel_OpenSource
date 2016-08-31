/*
 * arch/arm/mach-tegra/tegra_ptm.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/sysrq.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <linux/kallsyms.h>
#include <linux/clk.h>
#include <asm/sections.h>
#include <linux/cpu.h>
#include "pm.h"
#include "tegra_ptm.h"

/*
 * inside ETB trace buffer, each instruction can be identified by the CPU. For
 * the LP cluster, we assign it to a different ID in order to differentiate it
 * from CPU core 0.
 */
#define LP_CLUSTER_CPU_ID 0x9

enum range_type {
	NOT_USED = 0,
	RANGE_EXCLUDE,
	RANGE_INCLUDE,
};

/* PTM tracer state */
struct tracectx {
	void __iomem	*tpiu_regs;
	void __iomem	*funnel_regs;
	void __iomem	*etb_regs;
	int		*last_etb;
	unsigned int	etb_depth;
	void __iomem	*ptm_regs[8];
	int		ptm_regs_count;
	unsigned long	flags;
	int		ncmppairs;
	int		ptm_contextid_size;
	u32		etb_fc;
	unsigned long	range_start[8];
	unsigned long	range_end[8];
	enum range_type	range_type[8];
	bool		dump_initial_etb;
	struct device	*dev;
	struct mutex	mutex;
	unsigned int	timestamp_interval;
	struct clk	*coresight_clk;
	struct clk	*orig_parent_clk;
	int		orig_clk_rate;
	struct clk	*pll_p;
};

static struct tracectx tracer = {
	.range_start[0] = (unsigned long)_stext,
	.range_end[0]	= (unsigned long)_etext,
	.range_type[0]	= RANGE_INCLUDE,
	.flags		= TRACER_BRANCHOUTPUT,
	.etb_fc		= ETB_FF_CTRL_ENFTC,
	.ptm_contextid_size	= 2,
	.timestamp_interval = 0x8000,
};

static inline bool trace_isrunning(struct tracectx *t)
{
	return !!(t->flags & TRACER_RUNNING);
}

static int ptm_set_power(struct tracectx *t, int id, bool on)
{
	u32 v;

	v = ptm_readl(t, id, PTM_CTRL);
	if (on)
		v &= ~PTM_CTRL_POWERDOWN;
	else
		v |= PTM_CTRL_POWERDOWN;
	ptm_writel(t, id, v, PTM_CTRL);

	return 0;
}

static int ptm_set_programming_bit(struct tracectx *t, int id, bool on)
{
	u32 v;
	unsigned long timeout = TRACER_TIMEOUT;

	v = ptm_readl(t, id, PTM_CTRL);
	if (on)
		v |= PTM_CTRL_PROGRAM;
	else
		v &= ~PTM_CTRL_PROGRAM;
	ptm_writel(t, id, v, PTM_CTRL);

	while (--timeout) {
		if (on && ptm_progbit(t, id))
			break;
		if (!on && !ptm_progbit(t, id))
			break;
	}
	if (0 == timeout) {
		dev_err(t->dev, "PTM%d: %s progbit failed\n",
				id, on ? "set" : "clear");
		return -EFAULT;
	}
	return 0;
}

static void trace_set_cpu_funnel_port(struct tracectx *t, int id, bool on)
{
	int cpu_funnel_mask[] = { FUNNEL_CTRL_CPU0, FUNNEL_CTRL_CPU1,
				  FUNNEL_CTRL_CPU2, FUNNEL_CTRL_CPU3 };
	int funnel_ports;

	etb_regs_unlock(t);

	funnel_ports = funnel_readl(t, FUNNEL_CTRL);
	if (on)
		funnel_ports |= cpu_funnel_mask[id];
	else
		funnel_ports &= ~cpu_funnel_mask[id];

	funnel_writel(t, funnel_ports, FUNNEL_CTRL);

	etb_regs_lock(t);
}


static int ptm_setup_address_range(struct tracectx *t, int ptm_id, int cmp_id,
			unsigned long start, unsigned long end)
{
	u32 flags;

	/*
	 * We need know:
	 * 1. PFT 1.0 or PFT 1.1, and
	 * 2. Security Extension is implemented or not, and
	 * 3. privilege mode or user mode tracing required, and
	 * 4. security or non-security state tracing
	 * in order to set correct matching mode and state for this register.
	 *
	 * However using PTM_ACC_TYPE_PFT10_IGN_SECURITY will enable matching
	 * all modes and states under PFT 1.0 and 1.1
	 */
	flags = PTM_ACC_TYPE_IGN_CONTEXTID | PTM_ACC_TYPE_PFT10_IGN_SECURITY;

	/* PTM only supports instruction execute */
	flags |= PTM_ACC_TYPE_INSTR_ONLY;

	if (cmp_id < 0 || cmp_id >= t->ncmppairs)
		return -EINVAL;

	/* first comparator for the range */
	ptm_writel(t, ptm_id, flags, PTM_COMP_ACC_TYPE(cmp_id * 2));
	ptm_writel(t, ptm_id, start, PTM_COMP_VAL(cmp_id * 2));

	/* second comparator is right next to it */
	ptm_writel(t, ptm_id, flags, PTM_COMP_ACC_TYPE(cmp_id * 2 + 1));
	ptm_writel(t, ptm_id, end, PTM_COMP_VAL(cmp_id * 2 + 1));

	return 0;
}

static int trace_config_periodic_timestamp(struct tracectx *t, int id)
{
	if (0 == (t->flags & TRACER_TIMESTAMP))
		return 0;

	/* if the counter down value is 0, we disable periodic timestamp */
	if (0 == t->timestamp_interval)
		return 0;

	/* config counter0 to counter down: */

	/* set all the load value and reload vlaue */
	ptm_writel(t, id, t->timestamp_interval, PTMCNTVR(0));
	ptm_writel(t, id, t->timestamp_interval, PTMCNTRLDVR(0));
	/* reload the counter0 value if counter0 reach 0 */
	ptm_writel(t, id, DEF_PTM_EVENT(LOGIC_A, 0, COUNTER0),
			PTMCNTRLDEVR(0));
	/* config the timestamp trigger event if counter0 is 0 */
	ptm_writel(t, id, DEF_PTM_EVENT(LOGIC_A, 0, COUNTER0),
			PTMTSEVR);
	/* start the counter0 now */
	ptm_writel(t, id, DEF_PTM_EVENT(LOGIC_A, 0, ALWAYS_TRUE),
			PTMCNTENR(0));
	return 0;
}

static int trace_program_ptm(struct tracectx *t, int id)
{
	u32 v;
	int i;
	int excl_flags = PTM_TRACE_ENABLE_EXCL_CTRL;
	int incl_flags = PTM_TRACE_ENABLE_INCL_CTRL;

	/* we must maintain programming bit here */
	v = PTM_CTRL_PROGRAM;
	v |= PTM_CTRL_CONTEXTIDSIZE(t->ptm_contextid_size);
	if (t->flags & TRACER_CYCLE_ACC)
		v |= PTM_CTRL_CYCLEACCURATE;
	if (t->flags & TRACER_BRANCHOUTPUT)
		v |= PTM_CTRL_BRANCH_OUTPUT;
	if (t->flags & TRACER_TIMESTAMP)
		v |= PTM_CTRL_TIMESTAMP_EN;
	if (t->flags & TRACER_RETURN_STACK)
		v |= PTM_CTRL_RETURN_STACK_EN;
	ptm_writel(t, id, v, PTM_CTRL);

	for (i = 0; i < ARRAY_SIZE(t->range_start); i++)
		if (t->range_type[i] != NOT_USED)
			ptm_setup_address_range(t, id, i,
					t->range_start[i],
					t->range_end[i]);

	/*
	 * after the range is set up, we enable the comparators based on
	 * inc/exc flags
	 */
	for (i = 0; i < ARRAY_SIZE(t->range_type); i++) {
		if (t->range_type[i] == RANGE_EXCLUDE) {
			excl_flags |= 1 << i;
			ptm_writel(t, id, excl_flags, PTM_TRACE_ENABLE_CTRL1);
		}
		if (t->range_type[i] == RANGE_INCLUDE) {
			incl_flags |= 1 << i;
			ptm_writel(t, id, incl_flags, PTM_TRACE_ENABLE_CTRL1);
		}
	}

	/* trace all permitted processor execution... */
	ptm_writel(t, id, DEF_PTM_EVENT(LOGIC_A, 0, ALWAYS_TRUE),
			PTM_TRACE_ENABLE_EVENT);

	/* assigning ATID for low power CPU */
	if (is_lp_cluster())
		ptm_writel(t, 0, LP_CLUSTER_CPU_ID, PTM_TRACEIDR);
	else
		ptm_writel(t, id, id, PTM_TRACEIDR);

	/* programming the Isync packet frequency */
	ptm_writel(t, id, 100, PTM_SYNC_FREQ);

	trace_config_periodic_timestamp(t, id);

	return 0;
}

static void trace_start_ptm(struct tracectx *t, int id)
{
	int ret;

	trace_set_cpu_funnel_port(t, id, true);

	ptm_regs_unlock(t, id);

	ptm_os_unlock(t, id);

	ptm_set_power(t, id, true);

	ptm_set_programming_bit(t, id, true);
	ret = trace_program_ptm(t, id);
	if (ret)
		dev_err(t->dev, "enable PTM%d failed\n", id);
	ptm_set_programming_bit(t, id, false);

	ptm_regs_lock(t, id);
}

static int trace_start(struct tracectx *t)
{
	int id;
	u32 etb_fc = t->etb_fc;
	int ret;

	t->orig_clk_rate = clk_get_rate(t->coresight_clk);
	t->orig_parent_clk = clk_get_parent(t->coresight_clk);

	ret = clk_set_parent(t->coresight_clk, t->pll_p);
	if (ret < 0)
		return ret;
	ret = clk_set_rate(t->coresight_clk, 144000000);
	if (ret < 0)
		return ret;

	etb_regs_unlock(t);

	etb_writel(t, 0, ETB_WRITEADDR);
	etb_writel(t, etb_fc, ETB_FF_CTRL);
	etb_writel(t, TRACE_CAPATURE_ENABLE, ETB_CTRL);

	t->dump_initial_etb = false;

	etb_regs_lock(t);

	/* configure ptm(s) */
	for_each_online_cpu(id)
		trace_start_ptm(t, id);

	t->flags |= TRACER_RUNNING;

	return 0;
}

static int trace_stop_ptm(struct tracectx *t, int id)
{
	ptm_regs_unlock(t, id);

	/*
	 * when the programming bit is 1:
	 *  1. trace generation is stopped.
	 *  2. PTM FIFO is emptied
	 *  3. counter, sequencer and start/stop are held in current state.
	 *  4. external outputs are forced low.
	 */
	ptm_set_programming_bit(t, id, true);

	ptm_set_power(t, id, false);

	ptm_os_lock(t, id);

	ptm_regs_lock(t, id);

	/*
	 * Per ARM errata 780121 requires to disable the funnel port after PTM
	 * is disabled
	 */
	trace_set_cpu_funnel_port(t, id, false);

	return 0;
}

static int trace_stop(struct tracectx *t)
{
	int id;

	if (!trace_isrunning(t))
		return 0;

	for_each_online_cpu(id)
		trace_stop_ptm(t, id);

	etb_regs_unlock(t);

	etb_writel(t, TRACE_CAPATURE_DISABLE, ETB_CTRL);

	etb_regs_lock(t);

	clk_set_parent(t->coresight_clk, t->orig_parent_clk);

	clk_set_rate(t->coresight_clk, t->orig_clk_rate);

	t->flags &= ~TRACER_RUNNING;

	return 0;
}

static int etb_getdatalen(struct tracectx *t)
{
	u32 v;
	int wp;

	v = etb_readl(t, ETB_STATUS);

	if (v & ETB_STATUS_FULL)
		return t->etb_depth;

	wp = etb_readl(t, ETB_WRITEADDR);
	return wp;
}

/* sysrq+v will always stop the running trace and leave it at that */
static void ptm_dump(void)
{
	struct tracectx *t = &tracer;
	u32 first = 0;
	int length;

	if (!t->etb_regs) {
		pr_info("No tracing hardware found\n");
		return;
	}

	if (trace_isrunning(t))
		trace_stop(t);

	etb_regs_unlock(t);

	length = etb_getdatalen(t);

	if (length == t->etb_depth)
		first = etb_readl(t, ETB_WRITEADDR);

	etb_writel(t, first, ETB_READADDR);

	pr_info("Trace buffer contents length: %d\n", length);
	pr_info("--- ETB buffer begin ---\n");
	for (; length; length--)
		pr_cont("%08x", cpu_to_be32(etb_readl(t, ETB_READMEM)));
	pr_info("\n--- ETB buffer end ---\n");

	etb_regs_lock(t);
}

static int etb_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct tracectx *t = dev_get_drvdata(miscdev->parent);

	if (NULL == t->etb_regs)
		return -ENODEV;

	file->private_data = t;

	return nonseekable_open(inode, file);
}

static ssize_t etb_read(struct file *file, char __user *data,
		size_t len, loff_t *ppos)
{
	int total, i;
	long length;
	struct tracectx *t = file->private_data;
	u32 first = 0;
	u32 *buf;
	int wpos;
	int skip;
	long wlength;
	loff_t pos = *ppos;

	mutex_lock(&t->mutex);

	if (trace_isrunning(t)) {
		length = 0;
		goto out;
	}

	etb_regs_unlock(t);

	total = etb_getdatalen(t);
	if (total == 0 && t->dump_initial_etb)
		total = t->etb_depth;
	if (total == t->etb_depth)
		first = etb_readl(t, ETB_WRITEADDR);

	if (pos > total * 4) {
		skip = 0;
		wpos = total;
	} else {
		skip = (int)pos % 4;
		wpos = (int)pos / 4;
	}
	total -= wpos;
	first = (first + wpos) % t->etb_depth;

	etb_writel(t, first, ETB_READADDR);

	wlength = min(total, DIV_ROUND_UP(skip + (int)len, 4));
	length = min(total * 4 - skip, (int)len);
	if (wlength == 0) {
		etb_regs_lock(t);
		mutex_unlock(&t->mutex);
		return 0;
	}

	buf = vmalloc(wlength * 4);

	dev_dbg(t->dev, "ETB read %ld bytes to %lld from %ld words at %d\n",
			length, pos, wlength, first);
	dev_dbg(t->dev, "ETB buffer length: %d\n", total + wpos);
	dev_dbg(t->dev, "ETB status reg: %x\n", etb_readl(t, ETB_STATUS));
	for (i = 0; i < wlength; i++)
		buf[i] = etb_readl(t, ETB_READMEM);

	etb_regs_lock(t);

	length -= copy_to_user(data, (u8 *)buf + skip, length);
	vfree(buf);
	*ppos = pos + length;
out:
	mutex_unlock(&t->mutex);
	return length;
}

/*
 * this function will be called right after the PTM driver is initialized, it
 * will save the ETB contents from up to last reset.
 */
static ssize_t etb_save_last(struct tracectx *t)
{
	u32 first = 0;
	int i;
	int total;

	BUG_ON(!t->dump_initial_etb);


	etb_regs_unlock(t);
	/*
	 * not all ETB can maintain the ETB buffer write pointer after WDT
	 * reset, and we just read 0 for the write pointer; but we can still
	 * read/parse the ETB partially in this case.
	 */
	total = etb_getdatalen(t);
	if (total == 0 && t->dump_initial_etb)
		total = t->etb_depth;
	first = 0;
	if (total == t->etb_depth)
		first = etb_readl(t, ETB_WRITEADDR);

	etb_writel(t, first, ETB_READADDR);
	for (i = 0; i < t->etb_depth; i++)
		t->last_etb[i] = etb_readl(t, ETB_READMEM);

	etb_regs_lock(t);

	return 0;
}

static ssize_t last_etb_read(struct file *file, char __user *data,
		size_t len, loff_t *ppos)
{
	struct tracectx *t = file->private_data;
	size_t last_etb_size;
	size_t ret;

	mutex_lock(&t->mutex);

	ret = 0;
	last_etb_size = t->etb_depth * sizeof(*t->last_etb);
	if (*ppos >= last_etb_size)
		goto out;
	if (*ppos + len > last_etb_size)
		len = last_etb_size - *ppos;
	if (copy_to_user(data, (char *) t->last_etb + *ppos, len)) {
		ret = -EFAULT;
		goto out;
	}
	*ppos += len;
	ret = len;
out:
	mutex_unlock(&t->mutex);
	return ret;
}

static int etb_release(struct inode *inode, struct file *file)
{
	/* there's nothing to do here, actually */
	return 0;
}

/* use a sysfs file "trace_running" to start/stop tracing */
static ssize_t trace_running_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%x\n", trace_isrunning(&tracer));
}

static ssize_t trace_running_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned int value;
	int ret;

	if (sscanf(buf, "%u", &value) != 1)
		return -EINVAL;

	if (!tracer.etb_regs)
		return -ENODEV;

	mutex_lock(&tracer.mutex);
	if (value != 0)
		ret = trace_start(&tracer);
	else
		ret = trace_stop(&tracer);
	mutex_unlock(&tracer.mutex);

	return ret ? : n;
}

static ssize_t trace_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	u32 etb_wa, etb_ra, etb_st, etb_fc, ptm_ctrl, ptm_st;
	int datalen;
	int id;
	int ret;

	mutex_lock(&tracer.mutex);
	if (tracer.etb_regs) {
		etb_regs_unlock(&tracer);
		datalen = etb_getdatalen(&tracer);
		etb_wa = etb_readl(&tracer, ETB_WRITEADDR);
		etb_ra = etb_readl(&tracer, ETB_READADDR);
		etb_st = etb_readl(&tracer, ETB_STATUS);
		etb_fc = etb_readl(&tracer, ETB_FF_CTRL);
		etb_regs_lock(&tracer);
	} else {
		etb_wa = etb_ra = etb_st = etb_fc = ~0;
		datalen = -1;
	}

	ret = sprintf(buf, "Trace buffer len: %d\nComparator pairs: %d\n"
			   "ETB_WRITEADDR:\t%08x\nETB_READADDR:\t%08x\n"
			   "ETB_STATUS:\t%08x\nETB_FF_CTRL:\t%08x\n",
			   datalen, tracer.ncmppairs,
			   etb_wa, etb_ra,
			   etb_st, etb_fc);

	for (id = 0; id < tracer.ptm_regs_count; id++) {
		if (!cpu_online(id)) {
			ret += sprintf(buf + ret, "PTM_CTRL%d:\tOFFLINE\n"
				"PTM_STATUS%d:\tOFFLINE\n", id, id);
			continue;
		}
		ptm_regs_unlock(&tracer, id);
		ptm_ctrl = ptm_readl(&tracer, id, PTM_CTRL);
		ptm_st = ptm_readl(&tracer, id, PTM_STATUS);
		ptm_regs_lock(&tracer, id);
		ret += sprintf(buf + ret, "PTM_CTRL%d:\t%08x\n"
			"PTM_STATUS%d:\t%08x\n", id, ptm_ctrl, id, ptm_st);
	}
	mutex_unlock(&tracer.mutex);

	return ret;
}

static ssize_t trace_cycle_accurate_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", !!(tracer.flags & TRACER_CYCLE_ACC));
}

static ssize_t trace_cycle_accurate_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	unsigned int cycacc;

	if (sscanf(buf, "%u", &cycacc) != 1)
		return -EINVAL;

	mutex_lock(&tracer.mutex);

	if (cycacc)
		tracer.flags |= TRACER_CYCLE_ACC;
	else
		tracer.flags &= ~TRACER_CYCLE_ACC;

	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_contextid_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	/* 0: No context id tracing, 1: One byte, 2: Two bytes, 3: Four bytes */
	return sprintf(buf, "%d\n", (1 << tracer.ptm_contextid_size) >> 1);
}

static ssize_t trace_contextid_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned int contextid_size;


	if (sscanf(buf, "%u", &contextid_size) != 1)
		return -EINVAL;

	if (contextid_size == 3 || contextid_size > 4)
		return -EINVAL;

	mutex_lock(&tracer.mutex);
	tracer.ptm_contextid_size = fls(contextid_size);
	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_branch_output_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", !!(tracer.flags & TRACER_BRANCHOUTPUT));
}

static ssize_t trace_branch_output_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned int branch_output;

	if (sscanf(buf, "%u", &branch_output) != 1)
		return -EINVAL;

	mutex_lock(&tracer.mutex);
	if (branch_output) {
		tracer.flags |= TRACER_BRANCHOUTPUT;
		/* Branch broadcasting is incompatible with the return stack */
		if (tracer.flags == TRACER_RETURN_STACK)
			dev_err(tracer.dev, "Need turn off return stack too\n");
		tracer.flags &= ~TRACER_RETURN_STACK;
	} else {
		tracer.flags &= ~TRACER_BRANCHOUTPUT;
	}
	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_return_stack_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", !!(tracer.flags & TRACER_RETURN_STACK));
}

static ssize_t trace_return_stack_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned int return_stack;

	if (sscanf(buf, "%u", &return_stack) != 1)
		return -EINVAL;

	mutex_lock(&tracer.mutex);
	if (return_stack) {
		tracer.flags |= TRACER_RETURN_STACK;
		/* Return stack is incompatible with branch broadcasting */
		tracer.flags &= ~TRACER_BRANCHOUTPUT;
	} else {
		tracer.flags &= ~TRACER_RETURN_STACK;
	}
	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_timestamp_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", !!(tracer.flags & TRACER_TIMESTAMP));
}

static ssize_t trace_timestamp_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned int timestamp;

	if (sscanf(buf, "%d", &timestamp) < 1)
		return -EINVAL;

	mutex_lock(&tracer.mutex);
	if (timestamp)
		tracer.flags |= TRACER_TIMESTAMP;
	else
		tracer.flags &= ~TRACER_TIMESTAMP;
	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_timestamp_interval_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tracer.timestamp_interval);
}

static ssize_t trace_timestamp_interval_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	if (sscanf(buf, "%d", &tracer.timestamp_interval) != 1)
		return -EINVAL;

	return n;
}

static ssize_t trace_range_addr_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int id;

	if (sscanf(attr->attr.name, "trace_range%d", &id) != 1)
		return -EINVAL;
	if (id >= tracer.ncmppairs)
		return sprintf(buf, "invalid trace range comparator\n");

	return sprintf(buf, "%08lx %08lx\n",
			tracer.range_start[id], tracer.range_end[id]);
}

static ssize_t trace_range_addr_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	unsigned long range_start, range_end;
	int id;

	/* get the trace range ID */
	if (sscanf(attr->attr.name, "trace_range%d", &id) != 1)
		return -EINVAL;

	if (sscanf(buf, "%lx %lx", &range_start, &range_end) != 2)
		return -EINVAL;

	if (id >= tracer.ncmppairs)
		return -EINVAL;

	mutex_lock(&tracer.mutex);
	tracer.range_start[id] = range_start;
	tracer.range_end[id] = range_end;
	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_range_type_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int id;
	char *str;

	if (sscanf(attr->attr.name, "trace_range_type%d", &id) != 1)
		return -EINVAL;

	if (id >= tracer.ncmppairs)
		return sprintf(buf, "invalid trace range comparator\n");

	if (tracer.range_type[id] == NOT_USED)
		str = "disable";
	if (tracer.range_type[id] == RANGE_EXCLUDE)
		str = "exclude";
	if (tracer.range_type[id] == RANGE_INCLUDE)
		str = "include";
	return sprintf(buf, "%s\n", str);
}

static ssize_t trace_range_type_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	int id;
	size_t size;

	if (sscanf(attr->attr.name, "trace_range_type%d", &id) != 1)
		return -EINVAL;

	if (id >= tracer.ncmppairs)
		return -EINVAL;

	mutex_lock(&tracer.mutex);
	size = n - 1;
	if (0 == strncmp("disable", buf, size) || 0 == strncmp("0", buf, size))
		tracer.range_type[id] = NOT_USED;
	else if (0 == strncmp("include", buf, size))
		tracer.range_type[id] = RANGE_INCLUDE;
	else if (0 == strncmp("exclude", buf, size))
		tracer.range_type[id] = RANGE_EXCLUDE;
	else
		n = -EINVAL;
	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_function_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	unsigned long range_start, size, offset;
	char name[KSYM_NAME_LEN];
	char mod_name[KSYM_NAME_LEN];
	int id;

	if (sscanf(attr->attr.name, "trace_range_function%d", &id) != 1)
		return -EINVAL;
	if (id >= tracer.ncmppairs)
		return -EINVAL;

	n = min(n, sizeof(name));
	strncpy(name, buf, n);
	name[n - 1] = '\0';

	if (strncmp("all", name, 3) == 0) {
		/* magic "all" for tracking the kernel */
		range_start = (unsigned long) _stext;
		size = (unsigned long) _etext - (unsigned long) _stext + 4;
	} else {

		range_start = kallsyms_lookup_name(name);
		if (range_start == 0)
			return -EINVAL;

		if (0 > lookup_symbol_attrs(range_start, &size, &offset,
						mod_name, name))
			return -EINVAL;
	}

	mutex_lock(&tracer.mutex);
	tracer.range_start[id] = range_start;
	tracer.range_end[id] = range_start + size - 4;
	mutex_unlock(&tracer.mutex);

	return n;
}

static ssize_t trace_function_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned long range_start, size, offset;
	char name[KSYM_NAME_LEN];
	char mod_name[KSYM_NAME_LEN];
	int id;
	int ret;

	if (sscanf(attr->attr.name, "trace_range_function%d", &id) != 1)
		return -EINVAL;
	if (id >= tracer.ncmppairs)
		return -EINVAL;

	range_start = tracer.range_start[id];
	ret = 0;
	while (range_start <= tracer.range_end[id]) {
		if (0 > lookup_symbol_attrs(range_start, &size,
					&offset, mod_name, name))
			return -EINVAL;
		range_start += size;
		ret += sprintf(buf + ret, "%s\n", name);
		if (ret > (PAGE_SIZE - KSYM_NAME_LEN)) {
			ret += sprintf(buf + ret, "...\nGood news, everyone!");
			ret += sprintf(buf + ret, " Too many to list\n");
			break;
		}
	}
	return ret;
}

#define A(a, b, c, d)	__ATTR(trace_##a, b, \
		trace_##c##_show, trace_##d##_store)
static const struct kobj_attribute trace_attr[] = {
	__ATTR(trace_info,	0444, trace_info_show,	NULL),
	A(running,		0644, running,		running),
	A(range0,		0644, range_addr,	range_addr),
	A(range1,		0644, range_addr,	range_addr),
	A(range2,		0644, range_addr,	range_addr),
	A(range3,		0644, range_addr,	range_addr),
	A(range_function0,	0644, function,		function),
	A(range_function1,	0644, function,		function),
	A(range_function2,	0644, function,		function),
	A(range_function3,	0644, function,		function),
	A(range_type0,		0644, range_type,	range_type),
	A(range_type1,		0644, range_type,	range_type),
	A(range_type2,		0644, range_type,	range_type),
	A(range_type3,		0644, range_type,	range_type),
	A(cycle_accurate,	0644, cycle_accurate,	cycle_accurate),
	A(contextid_size,	0644, contextid_size,	contextid_size),
	A(branch_output,	0644, branch_output,	branch_output),
	A(return_stack,		0644, return_stack,	return_stack),
	A(timestamp,		0644, timestamp,	timestamp),
	A(timestamp_interval,	0644, timestamp_interval, timestamp_interval)
};

#define clk_readl(reg)  __raw_readl(reg_clk_base + (reg))
#define clk_writel(value, reg) __raw_writel(value, reg_clk_base + (reg))
static void __iomem *reg_clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

static int funnel_and_tpiu_init(struct tracectx *t)
{
	u32 tmp;

	/* Enable the trace clk for the TPIU */
	tmp = clk_readl(CLK_TPIU_OUT_ENB_U);
	tmp |= CLK_TPIU_OUT_ENB_U_TRACKCLK_IN;
	clk_writel(tmp, CLK_TPIU_OUT_ENB_U);

	/* set trace clk of TPIU using the pll_p */
	tmp = clk_readl(CLK_TPIU_SRC_TRACECLKIN);
	tmp &= ~CLK_TPIU_SRC_TRACECLKIN_SRC_MASK;
	tmp |= CLK_TPIU_SRC_TRACECLKIN_PLLP;
	clk_writel(tmp, CLK_TPIU_SRC_TRACECLKIN);

	/* disable TPIU */
	tpiu_regs_unlock(t);
	tpiu_writel(t, TPIU_FF_CTRL_STOPFL, TPIU_FF_CTRL);
	tpiu_writel(t, TPIU_FF_CTRL_STOPFL | TPIU_FF_CTRL_MANUAL_FLUSH,
			TPIU_FF_CTRL);
	tpiu_regs_lock(t);

	/* Disable TPIU clk */
	tmp = clk_readl(CLK_TPIU_OUT_ENB_U);
	tmp &= ~CLK_TPIU_OUT_ENB_U_TRACKCLK_IN;
	clk_writel(tmp, CLK_TPIU_OUT_ENB_U);

	funnel_regs_unlock(t);
	/* enable CPU0 - 3 for funnel ports, also assign funnel hold time */
	funnel_writel(t, FUNNEL_MINIMUM_HOLD_TIME(0) |
			FUNNEL_CTRL_CPU0 | FUNNEL_CTRL_CPU1 |
			FUNNEL_CTRL_CPU2 | FUNNEL_CTRL_CPU3,
			FUNNEL_CTRL);

	/* Assign CPU0-3 funnel priority */
	funnel_writel(t, 0xFFFFFFFF, FUNNEL_PRIORITY);
	funnel_writel(t, FUNNEL_PRIORITY_CPU0(0) | FUNNEL_PRIORITY_CPU1(0) |
			 FUNNEL_PRIORITY_CPU2(0) | FUNNEL_PRIORITY_CPU3(0),
			 FUNNEL_PRIORITY);
	funnel_regs_lock(t);

	return 0;
}

static const struct file_operations etb_fops = {
	.owner = THIS_MODULE,
	.read = etb_read,
	.open = etb_open,
	.release = etb_release,
	.llseek = no_llseek,
};

static const struct file_operations last_etb_fops = {
	.owner = THIS_MODULE,
	.read = last_etb_read,
	.open = etb_open,
	.release = etb_release,
	.llseek = no_llseek,
};

static struct miscdevice etb_miscdev = {
	.name = "etb",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &etb_fops,
};

static struct miscdevice last_etb_miscdev = {
	.name = "last_etb",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &last_etb_fops,
};

void ptm_power_idle_resume(int cpu)
{
	struct tracectx *t = &tracer;

	if (trace_isrunning(&tracer))
		trace_start_ptm(t, cpu);
}

static int etb_init(struct tracectx *t)
{
	int ret;

	t->dump_initial_etb = true;

	etb_regs_unlock(t);

	t->etb_depth = etb_readl(t, ETB_DEPTH);
	/* make sure trace capture is disabled */
	etb_writel(t, TRACE_CAPATURE_DISABLE, ETB_CTRL);
	etb_writel(t, ETB_FF_CTRL_STOPFL, ETB_FF_CTRL);

	etb_regs_lock(t);

	t->last_etb = devm_kzalloc(t->dev, t->etb_depth * sizeof(*t->last_etb),
					GFP_KERNEL);
	if (NULL == t->last_etb) {
		dev_err(t->dev, "failes to allocate memory to hold ETB\n");
		return -ENOMEM;
	}

	etb_miscdev.parent = t->dev;
	ret = misc_register(&etb_miscdev);
	if (ret) {
		dev_err(t->dev, "failes to register /dev/etb\n");
		return ret;
	}
	last_etb_miscdev.parent = t->dev;
	ret = misc_register(&last_etb_miscdev);
	if (ret) {
		dev_err(t->dev, "failes to register /dev/last_etb\n");
		return ret;
	}

	dev_info(t->dev, "ETB is initialized.\n");
	return 0;
}

static void tegra_ptm_enter_resume(void)
{
#ifdef CONFIG_PM_SLEEP
	if (trace_isrunning(&tracer)) {
		/*
		 * On Tegra, LP0 will cut off VDD_CORE, and in that case
		 * TPIU and FUNNEL need to be initialized again.
		 */
		if (!(TRACE_CAPATURE_ENABLE & etb_readl(&tracer, ETB_CTRL))) {
			funnel_and_tpiu_init(&tracer);
			trace_start(&tracer);
		}
	}
#endif
}

static struct syscore_ops tegra_ptm_enter_syscore_ops = {
	.resume = tegra_ptm_enter_resume,
};

static int tegra_ptm_cpu_notify(struct notifier_block *self,
				    unsigned long action, void *cpu)
{
	struct tracectx *t = &tracer;

	/* re-initialize the PTM if the PTM's CPU back on online */
	switch (action) {
	case CPU_STARTING:
		if (trace_isrunning(t))
			trace_start_ptm(t, (int)cpu);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block tegra_ptm_cpu_nb = {
	.notifier_call = tegra_ptm_cpu_notify,
};

static int ptm_probe(struct platform_device *dev)
{
	struct tracectx *t = &tracer;
	int ret = 0;
	int i;

	mutex_lock(&t->mutex);

	t->dev = &dev->dev;
	platform_set_drvdata(dev, t);

	/* PLL_P can be used for CoreSight parent clock at high freq */
	t->pll_p = clk_get_sys(NULL, "pll_p");
	if (IS_ERR(t->pll_p)) {
		dev_err(&dev->dev, "Could not get pll_p clock\n");
		goto out;
	}

	/* eanble the CoreSight(csite) clock for PTM/FUNNEL/ETB/TPIU */
	t->coresight_clk = clk_get_sys("csite", NULL);
	if (IS_ERR(t->coresight_clk)) {
		dev_err(&dev->dev, "Could not get csite clock\n");
		goto out;
	}
	clk_enable(t->coresight_clk);

	/* get all PTM resrouces */
	t->ptm_regs_count = 0;
	ret = -ENOMEM;
	for (i = 0; i < dev->num_resources; i++) {
		struct resource *res;
		void __iomem *addr;

		res = platform_get_resource(dev, IORESOURCE_MEM, i);
		if (NULL == res)
			goto out;
		addr = devm_ioremap_nocache(&dev->dev, res->start,
				resource_size(res));
		if (NULL == addr)
			goto out;

		if (0 == strncmp("ptm", res->name, 3)) {
			t->ptm_regs[t->ptm_regs_count] = addr;
			t->ptm_regs_count++;
		}
		if (0 == strncmp("etb", res->name, 3))
			t->etb_regs = addr;
		if (0 == strncmp("tpiu", res->name, 4))
			t->tpiu_regs = addr;
		if (0 == strncmp("funnel", res->name, 6))
			t->funnel_regs = addr;
	}
	/* at least one PTM is required */
	if (t->ptm_regs[0] == NULL || t->etb_regs == NULL ||
	    t->tpiu_regs == NULL   || t->funnel_regs == NULL) {
		dev_err(&dev->dev, "Could not get PTM resources\n");
		goto out;
	}

	if (0x13 != (0xff & ptm_readl(t, 0, CORESIGHT_DEVTYPE))) {
		dev_err(&dev->dev, "Did not find correct PTM device type\n");
		goto out;
	}

	t->ncmppairs = 0xf & ptm_readl(t, 0, PTM_CONFCODE);

	/* initialize ETB, TPIU, FUNNEL and PTM */
	ret = etb_init(t);
	if (ret)
		goto out;

	ret = funnel_and_tpiu_init(t);
	if (ret)
		goto out;

	for (i = 0; i < t->ptm_regs_count; i++)
		trace_stop_ptm(t, i);

	/* create sysfs */
	for (i = 0; i < ARRAY_SIZE(trace_attr); i++) {
		ret = sysfs_create_file(&dev->dev.kobj, &trace_attr[i].attr);
		if (ret)
			dev_err(&dev->dev, "failed to create %s\n",
					trace_attr[i].attr.name);
	}

	/* register CPU PM/hotplug related callback */
	register_syscore_ops(&tegra_ptm_enter_syscore_ops);
	register_cpu_notifier(&tegra_ptm_cpu_nb);

	dev_info(&dev->dev, "PTM driver initialized.\n");

	etb_save_last(t);

	/* start the PTM and ETB now */
	trace_start(t);
out:
	dev_err(&dev->dev, "Failed to start the PTM device\n");
	mutex_unlock(&t->mutex);
	return ret;
}

static int ptm_remove(struct platform_device *dev)
{
	struct tracectx *t = &tracer;
	int i;

	unregister_cpu_notifier(&tegra_ptm_cpu_nb);
	unregister_syscore_ops(&tegra_ptm_enter_syscore_ops);
	for (i = 0; i < ARRAY_SIZE(trace_attr); i++)
		sysfs_remove_file(&dev->dev.kobj, &trace_attr[i].attr);

	mutex_lock(&t->mutex);

	devm_iounmap(&dev->dev, t->ptm_regs);
	devm_iounmap(&dev->dev, t->tpiu_regs);
	devm_iounmap(&dev->dev, t->funnel_regs);
	devm_iounmap(&dev->dev, t->etb_regs);
	t->etb_regs = NULL;
	clk_disable(t->coresight_clk);
	clk_put(t->coresight_clk);

	mutex_unlock(&t->mutex);

	return 0;
}

static struct platform_driver ptm_driver = {
	.probe          = ptm_probe,
	.remove         = ptm_remove,
	.driver         = {
		.name	= "ptm",
		.owner	= THIS_MODULE,
	},
};

static void sysrq_ptm_dump(int key)
{
	if (!mutex_trylock(&tracer.mutex)) {
		pr_info("Tracing hardware busy\n");
		return;
	}
	dev_dbg(tracer.dev, "Dumping ETB buffer\n");
	ptm_dump();
	mutex_unlock(&tracer.mutex);
}

static struct sysrq_key_op sysrq_ptm_op = {
	.handler = sysrq_ptm_dump,
	.help_msg = "PTM buffer dump(V)",
	.action_msg = "ptm",
};

static int __init tegra_ptm_driver_init(void)
{
	int retval;

	mutex_init(&tracer.mutex);

	retval = platform_driver_register(&ptm_driver);
	if (retval) {
		pr_err("Failed to probe ptm\n");
		return retval;
	}

	/* not being able to install this handler is not fatal */
	(void)register_sysrq_key('v', &sysrq_ptm_op);

	return 0;
}
device_initcall(tegra_ptm_driver_init);
