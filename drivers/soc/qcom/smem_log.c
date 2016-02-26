/* Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
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
/*
 * Shared memory logging implementation.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/remote_spinlock.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>

#include <soc/qcom/smem.h>
#include <soc/qcom/smem_log.h>

#include <asm/arch_timer.h>

#include "smem_private.h"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define D_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	int i; \
	printk(KERN_ERR "%s", prestr); \
	for (i = 0; i < cnt; i++) \
		printk(KERN_ERR "%.2x", buf[i]); \
	printk(KERN_ERR "\n"); \
} while (0)
#else
#define D_DUMP_BUFFER(prestr, cnt, buf)
#endif

#ifdef DEBUG
#define D(x...) printk(x)
#else
#define D(x...) do {} while (0)
#endif

struct smem_log_item {
	uint32_t identifier;
	uint32_t timetick;
	uint32_t data1;
	uint32_t data2;
	uint32_t data3;
};

#define SMEM_LOG_NUM_ENTRIES 2000
#define SMEM_LOG_EVENTS_SIZE (sizeof(struct smem_log_item) * \
			      SMEM_LOG_NUM_ENTRIES)

#define SMEM_SPINLOCK_SMEM_LOG		"S:2"

static remote_spinlock_t remote_spinlock;
static uint32_t smem_log_enable;
static int smem_log_initialized;

module_param_named(log_enable, smem_log_enable, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);


struct smem_log_inst {
	int which_log;
	struct smem_log_item __iomem *events;
	uint32_t __iomem *idx;
	uint32_t num;
	uint32_t read_idx;
	uint32_t last_read_avail;
	wait_queue_head_t read_wait;
	remote_spinlock_t *remote_spinlock;
};

enum smem_logs {
	GEN = 0,
	NUM
};

static struct smem_log_inst inst[NUM];

#if defined(CONFIG_DEBUG_FS)

#define HSIZE 13

struct sym {
	uint32_t val;
	char *str;
	struct hlist_node node;
};

struct sym id_syms[] = {
	{ SMEM_LOG_PROC_ID_MODEM, "MODM" },
	{ SMEM_LOG_PROC_ID_Q6, "QDSP" },
	{ SMEM_LOG_PROC_ID_APPS, "APPS" },
	{ SMEM_LOG_PROC_ID_WCNSS, "WCNSS" },
};

struct sym base_syms[] = {
	{ SMEM_LOG_SMEM_EVENT_BASE, "SMEM" },
	{ SMEM_LOG_ERROR_EVENT_BASE, "ERROR" },
	{ SMEM_LOG_QMI_CCI_EVENT_BASE, "QCCI" },
	{ SMEM_LOG_QMI_CSI_EVENT_BASE, "QCSI" },
};

struct sym event_syms[] = {
	{ ERR_ERROR_FATAL, "ERR_ERROR_FATAL" },
	{ ERR_ERROR_FATAL_TASK, "ERR_ERROR_FATAL_TASK" },
	{ SMEM_LOG_EVENT_CB, "CB" },
	{ SMEM_LOG_EVENT_START, "START" },
	{ SMEM_LOG_EVENT_INIT, "INIT" },
	{ SMEM_LOG_EVENT_RUNNING, "RUNNING" },
	{ SMEM_LOG_EVENT_STOP, "STOP" },
	{ SMEM_LOG_EVENT_RESTART, "RESTART" },
	{ SMEM_LOG_EVENT_SS, "SS" },
	{ SMEM_LOG_EVENT_READ, "READ" },
	{ SMEM_LOG_EVENT_WRITE, "WRITE" },
	{ SMEM_LOG_EVENT_SIGS1, "SIGS1" },
	{ SMEM_LOG_EVENT_SIGS2, "SIGS2" },
	{ SMEM_LOG_EVENT_WRITE_DM, "WRITE_DM" },
	{ SMEM_LOG_EVENT_READ_DM, "READ_DM" },
	{ SMEM_LOG_EVENT_SKIP_DM, "SKIP_DM" },
	{ SMEM_LOG_EVENT_STOP_DM, "STOP_DM" },
	{ SMEM_LOG_EVENT_ISR, "ISR" },
	{ SMEM_LOG_EVENT_TASK, "TASK" },
	{ SMEM_LOG_EVENT_RS, "RS" },
};

struct sym smsm_syms[] = {
	{ 0x80000000, "UN" },
	{ 0x7F000000, "ERR" },
	{ 0x00800000, "SMLP" },
	{ 0x00400000, "ADWN" },
	{ 0x00200000, "PWRS" },
	{ 0x00100000, "DWLD" },
	{ 0x00080000, "SRBT" },
	{ 0x00040000, "SDWN" },
	{ 0x00020000, "ARBT" },
	{ 0x00010000, "REL" },
	{ 0x00008000, "SLE" },
	{ 0x00004000, "SLP" },
	{ 0x00002000, "WFPI" },
	{ 0x00001000, "EEX" },
	{ 0x00000800, "TIN" },
	{ 0x00000400, "TWT" },
	{ 0x00000200, "PWRC" },
	{ 0x00000100, "RUN" },
	{ 0x00000080, "SA" },
	{ 0x00000040, "RES" },
	{ 0x00000020, "RIN" },
	{ 0x00000010, "RWT" },
	{ 0x00000008, "SIN" },
	{ 0x00000004, "SWT" },
	{ 0x00000002, "OE" },
	{ 0x00000001, "I" },
};

struct sym smsm_entry_type_syms[] = {
	{ 0, "SMSM_APPS_STATE" },
	{ 1, "SMSM_MODEM_STATE" },
	{ 2, "SMSM_Q6_STATE" },
	{ 3, "SMSM_APPS_DEM" },
	{ 4, "SMSM_MODEM_DEM" },
	{ 5, "SMSM_Q6_DEM" },
	{ 6, "SMSM_POWER_MASTER_DEM" },
	{ 7, "SMSM_TIME_MASTER_DEM" },
};

struct sym smsm_state_syms[] = {
	{ 0x00000001, "INIT" },
	{ 0x00000002, "OSENTERED" },
	{ 0x00000004, "SMDWAIT" },
	{ 0x00000008, "SMDINIT" },
	{ 0x00000010, "RPCWAIT" },
	{ 0x00000020, "RPCINIT" },
	{ 0x00000040, "RESET" },
	{ 0x00000080, "RSA" },
	{ 0x00000100, "RUN" },
	{ 0x00000200, "PWRC" },
	{ 0x00000400, "TIMEWAIT" },
	{ 0x00000800, "TIMEINIT" },
	{ 0x00001000, "PWRC_EARLY_EXIT" },
	{ 0x00002000, "WFPI" },
	{ 0x00004000, "SLEEP" },
	{ 0x00008000, "SLEEPEXIT" },
	{ 0x00010000, "OEMSBL_RELEASE" },
	{ 0x00020000, "APPS_REBOOT" },
	{ 0x00040000, "SYSTEM_POWER_DOWN" },
	{ 0x00080000, "SYSTEM_REBOOT" },
	{ 0x00100000, "SYSTEM_DOWNLOAD" },
	{ 0x00200000, "PWRC_SUSPEND" },
	{ 0x00400000, "APPS_SHUTDOWN" },
	{ 0x00800000, "SMD_LOOPBACK" },
	{ 0x01000000, "RUN_QUIET" },
	{ 0x02000000, "MODEM_WAIT" },
	{ 0x04000000, "MODEM_BREAK" },
	{ 0x08000000, "MODEM_CONTINUE" },
	{ 0x80000000, "UNKNOWN" },
};

enum sym_tables {
	ID_SYM,
	BASE_SYM,
	EVENT_SYM,
	SMSM_SYM,
	SMSM_ENTRY_TYPE_SYM,
	SMSM_STATE_SYM,
};

static struct sym_tbl {
	struct sym *data;
	int size;
	struct hlist_head hlist[HSIZE];
} tbl[] = {
	{ id_syms, ARRAY_SIZE(id_syms) },
	{ base_syms, ARRAY_SIZE(base_syms) },
	{ event_syms, ARRAY_SIZE(event_syms) },
	{ smsm_syms, ARRAY_SIZE(smsm_syms) },
	{ smsm_entry_type_syms, ARRAY_SIZE(smsm_entry_type_syms) },
	{ smsm_state_syms, ARRAY_SIZE(smsm_state_syms) },
};

#define hash(val) (val % HSIZE)

static void init_syms(void)
{
	int i;
	int j;

	for (i = 0; i < ARRAY_SIZE(tbl); ++i)
		for (j = 0; j < HSIZE; ++j)
			INIT_HLIST_HEAD(&tbl[i].hlist[j]);

	for (i = 0; i < ARRAY_SIZE(tbl); ++i)
		for (j = 0; j < tbl[i].size; ++j) {
			INIT_HLIST_NODE(&tbl[i].data[j].node);
			hlist_add_head(&tbl[i].data[j].node,
				       &tbl[i].hlist[hash(tbl[i].data[j].val)]);
		}
}

static char *find_sym(uint32_t id, uint32_t val)
{
	struct hlist_node *n;
	struct sym *s;

	hlist_for_each(n, &tbl[id].hlist[hash(val)]) {
		s = hlist_entry(n, struct sym, node);
		if (s->val == val)
			return s->str;
	}

	return 0;
}

#else
static void init_syms(void) {}
#endif

union fifo_mem {
	uint64_t u64;
	uint8_t u8;
};

/**
 * memcpy_to_log() - copy to SMEM log FIFO
 * @dest: Destination address
 * @src: Source address
 * @num_bytes: Number of bytes to copy
 *
 * @return: Address of destination
 *
 * This function copies num_bytes from src to dest maintaining natural alignment
 * for accesses to dest as required for Device memory.
 */
static void *memcpy_to_log(void *dest, const void *src, size_t num_bytes)
{
	union fifo_mem *temp_dst = (union fifo_mem *)dest;
	union fifo_mem *temp_src = (union fifo_mem *)src;
	uintptr_t mask = sizeof(union fifo_mem) - 1;

	/* Do byte copies until we hit 8-byte (double word) alignment */
	while ((uintptr_t)temp_dst & mask && num_bytes) {
		__raw_writeb_no_log(temp_src->u8, temp_dst);
		temp_src = (union fifo_mem *)((uintptr_t)temp_src + 1);
		temp_dst = (union fifo_mem *)((uintptr_t)temp_dst + 1);
		num_bytes--;
	}

	/* Do double word copies */
	while (num_bytes >= sizeof(union fifo_mem)) {
		__raw_writeq_no_log(temp_src->u64, temp_dst);
		temp_dst++;
		temp_src++;
		num_bytes -= sizeof(union fifo_mem);
	}

	/* Copy remaining bytes */
	while (num_bytes--) {
		__raw_writeb_no_log(temp_src->u8, temp_dst);
		temp_src = (union fifo_mem *)((uintptr_t)temp_src + 1);
		temp_dst = (union fifo_mem *)((uintptr_t)temp_dst + 1);
	}

	return dest;
}


static inline unsigned int read_timestamp(void)
{
	return (unsigned int)(arch_counter_get_cntvct());
}

static void smem_log_event_from_user(struct smem_log_inst *inst,
				     const char *buf, int size, int num)
{
	uint32_t idx;
	uint32_t next_idx;
	unsigned long flags;
	uint32_t identifier = 0;
	uint32_t timetick = 0;
	int first = 1;

	if (!inst->idx) {
		pr_err("%s: invalid write index\n", __func__);
		return;
	}

	remote_spin_lock_irqsave(inst->remote_spinlock, flags);

	while (num--) {
		idx = *inst->idx;

		if (idx < inst->num) {
			memcpy_to_log(&inst->events[idx], buf, size);

			if (first) {
				identifier =
					inst->events[idx].
					identifier;
				timetick = read_timestamp();
				first = 0;
			} else {
				identifier |= SMEM_LOG_CONT;
			}
			inst->events[idx].identifier =
				identifier;
			inst->events[idx].timetick =
				timetick;
		}

		next_idx = idx + 1;
		if (next_idx >= inst->num)
			next_idx = 0;
		*inst->idx = next_idx;
		buf += sizeof(struct smem_log_item);
	}

	wmb();
	remote_spin_unlock_irqrestore(inst->remote_spinlock, flags);
}

static void _smem_log_event(
	struct smem_log_item __iomem *events,
	uint32_t __iomem *_idx,
	remote_spinlock_t *lock,
	int num,
	uint32_t id, uint32_t data1, uint32_t data2,
	uint32_t data3)
{
	struct smem_log_item item;
	uint32_t idx;
	uint32_t next_idx;
	unsigned long flags;

	item.timetick = read_timestamp();
	item.identifier = id;
	item.data1 = data1;
	item.data2 = data2;
	item.data3 = data3;

	remote_spin_lock_irqsave(lock, flags);

	idx = *_idx;

	if (idx < num)
		memcpy_to_log(&events[idx], &item, sizeof(item));

	next_idx = idx + 1;
	if (next_idx >= num)
		next_idx = 0;
	*_idx = next_idx;
	wmb();

	remote_spin_unlock_irqrestore(lock, flags);
}

static void _smem_log_event6(
	struct smem_log_item __iomem *events,
	uint32_t __iomem *_idx,
	remote_spinlock_t *lock,
	int num,
	uint32_t id, uint32_t data1, uint32_t data2,
	uint32_t data3, uint32_t data4, uint32_t data5,
	uint32_t data6)
{
	struct smem_log_item item[2];
	uint32_t idx;
	uint32_t next_idx;
	unsigned long flags;

	item[0].timetick = read_timestamp();
	item[0].identifier = id;
	item[0].data1 = data1;
	item[0].data2 = data2;
	item[0].data3 = data3;
	item[1].identifier = item[0].identifier;
	item[1].timetick = item[0].timetick;
	item[1].data1 = data4;
	item[1].data2 = data5;
	item[1].data3 = data6;

	remote_spin_lock_irqsave(lock, flags);

	idx = *_idx;

	/* FIXME: Wrap around */
	if (idx < (num-1))
		memcpy_to_log(&events[idx], &item, sizeof(item));

	next_idx = idx + 2;
	if (next_idx >= num)
		next_idx = 0;
	*_idx = next_idx;

	wmb();
	remote_spin_unlock_irqrestore(lock, flags);
}

void smem_log_event(uint32_t id, uint32_t data1, uint32_t data2,
		    uint32_t data3)
{
	if (smem_log_enable)
		_smem_log_event(inst[GEN].events, inst[GEN].idx,
				inst[GEN].remote_spinlock,
				SMEM_LOG_NUM_ENTRIES, id,
				data1, data2, data3);
}

void smem_log_event6(uint32_t id, uint32_t data1, uint32_t data2,
		     uint32_t data3, uint32_t data4, uint32_t data5,
		     uint32_t data6)
{
	if (smem_log_enable)
		_smem_log_event6(inst[GEN].events, inst[GEN].idx,
				 inst[GEN].remote_spinlock,
				 SMEM_LOG_NUM_ENTRIES, id,
				 data1, data2, data3, data4, data5, data6);
}

static int _smem_log_init(void)
{
	int ret;

	inst[GEN].which_log = GEN;
	inst[GEN].events =
		(struct smem_log_item *)smem_alloc(SMEM_SMEM_LOG_EVENTS,
						  SMEM_LOG_EVENTS_SIZE,
						  0,
						  SMEM_ANY_HOST_FLAG);
	inst[GEN].idx = (uint32_t *)smem_alloc(SMEM_SMEM_LOG_IDX,
					     sizeof(uint32_t),
					     0,
					     SMEM_ANY_HOST_FLAG);
	if (IS_ERR_OR_NULL(inst[GEN].events) || IS_ERR_OR_NULL(inst[GEN].idx)) {
		pr_err("%s: no log or log_idx allocated\n", __func__);
		return -ENODEV;
	}

	inst[GEN].num = SMEM_LOG_NUM_ENTRIES;
	inst[GEN].read_idx = 0;
	inst[GEN].last_read_avail = SMEM_LOG_NUM_ENTRIES;
	init_waitqueue_head(&inst[GEN].read_wait);
	inst[GEN].remote_spinlock = &remote_spinlock;

	ret = remote_spin_lock_init(&remote_spinlock,
			      SMEM_SPINLOCK_SMEM_LOG);
	if (ret) {
		mb();
		return ret;
	}

	init_syms();
	mb();

	return 0;
}

static ssize_t smem_log_write_bin(struct file *fp, const char __user *_buf,
			 size_t count, loff_t *pos)
{
	void *buf;
	int r;

	if (count < sizeof(struct smem_log_item))
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	r = copy_from_user(buf, _buf, count);
	if (r) {
		kfree(buf);
		return -EFAULT;
	}

	if (smem_log_enable)
		smem_log_event_from_user(fp->private_data, buf,
					sizeof(struct smem_log_item),
					count / sizeof(struct smem_log_item));
	kfree(buf);
	return count;
}

static int smem_log_open(struct inode *ip, struct file *fp)
{
	fp->private_data = &inst[GEN];

	return 0;
}

static int smem_log_release(struct inode *ip, struct file *fp)
{
	return 0;
}

static const struct file_operations smem_log_bin_fops = {
	.owner = THIS_MODULE,
	.write = smem_log_write_bin,
	.open = smem_log_open,
	.release = smem_log_release,
};

static struct miscdevice smem_log_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smem_log",
	.fops = &smem_log_bin_fops,
};

#if defined(CONFIG_DEBUG_FS)

#define SMEM_LOG_ITEM_PRINT_SIZE 160

#define EVENTS_PRINT_SIZE \
(SMEM_LOG_ITEM_PRINT_SIZE * SMEM_LOG_NUM_ENTRIES)

static uint32_t smem_log_timeout_ms;
module_param_named(timeout_ms, smem_log_timeout_ms,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

static int smem_log_debug_mask;
module_param_named(debug_mask, smem_log_debug_mask, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

#define DBG(x...) do {\
	if (smem_log_debug_mask) \
		printk(KERN_DEBUG x);\
	} while (0)

static int update_read_avail(struct smem_log_inst *inst)
{
	int curr_read_avail;
	unsigned long flags = 0;

	if (!inst->idx)
		return 0;

	remote_spin_lock_irqsave(inst->remote_spinlock, flags);
	curr_read_avail = (*inst->idx - inst->read_idx);
	if (curr_read_avail < 0)
		curr_read_avail = inst->num - inst->read_idx + *inst->idx;

	DBG("%s: read = %d write = %d curr = %d last = %d\n", __func__,
	    inst->read_idx, *inst->idx, curr_read_avail, inst->last_read_avail);

	if (curr_read_avail < inst->last_read_avail) {
		if (inst->last_read_avail != inst->num)
			pr_info("smem_log: skipping %d log entries\n",
				inst->last_read_avail);
		inst->read_idx = *inst->idx + 1;
		inst->last_read_avail = inst->num - 1;
	} else
		inst->last_read_avail = curr_read_avail;

	remote_spin_unlock_irqrestore(inst->remote_spinlock, flags);

	DBG("%s: read = %d write = %d curr = %d last = %d\n", __func__,
	    inst->read_idx, *inst->idx, curr_read_avail, inst->last_read_avail);

	return inst->last_read_avail;
}

static int _debug_dump(int log, char *buf, int max, uint32_t cont)
{
	unsigned int idx;
	int write_idx, read_avail = 0;
	unsigned long flags;
	int i = 0;

	if (!inst[log].events)
		return 0;

	if (cont && update_read_avail(&inst[log]) == 0)
		return 0;

	remote_spin_lock_irqsave(inst[log].remote_spinlock, flags);

	if (cont) {
		idx = inst[log].read_idx;
		write_idx = (inst[log].read_idx + inst[log].last_read_avail);
		if (write_idx >= inst[log].num)
			write_idx -= inst[log].num;
	} else {
		write_idx = *inst[log].idx;
		idx = (write_idx + 1);
	}

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num - 1);

	while ((max - i) > 50) {
		if ((inst[log].num - 1) < idx)
			idx = 0;

		if (idx == write_idx)
			break;

		if (inst[log].events[idx].identifier) {

			i += scnprintf(buf + i, max - i,
				       "%08x %08x %08x %08x %08x\n",
				       inst[log].events[idx].identifier,
				       inst[log].events[idx].timetick,
				       inst[log].events[idx].data1,
				       inst[log].events[idx].data2,
				       inst[log].events[idx].data3);
		}
		idx++;
	}
	if (cont) {
		inst[log].read_idx = idx;
		read_avail = (write_idx - inst[log].read_idx);
		if (read_avail < 0)
			read_avail = inst->num - inst->read_idx + write_idx;
		inst[log].last_read_avail = read_avail;
	}

	remote_spin_unlock_irqrestore(inst[log].remote_spinlock, flags);

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num);

	return i;
}

static int _debug_dump_sym(int log, char *buf, int max, uint32_t cont)
{
	unsigned int idx;
	int write_idx, read_avail = 0;
	unsigned long flags;
	int i = 0;

	char *proc;
	char *sub;
	char *id;
	const char *sym = NULL;

	uint32_t proc_val = 0;
	uint32_t sub_val = 0;
	uint32_t id_val = 0;
	uint32_t id_only_val = 0;
	uint32_t data1 = 0;
	uint32_t data2 = 0;
	uint32_t data3 = 0;

	if (!inst[log].events)
		return 0;

	if (cont && update_read_avail(&inst[log]) == 0)
		return 0;

	remote_spin_lock_irqsave(inst[log].remote_spinlock, flags);

	if (cont) {
		idx = inst[log].read_idx;
		write_idx = (inst[log].read_idx + inst[log].last_read_avail);
		if (write_idx >= inst[log].num)
			write_idx -= inst[log].num;
	} else {
		write_idx = *inst[log].idx;
		idx = (write_idx + 1);
	}

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num - 1);

	for (; (max - i) > SMEM_LOG_ITEM_PRINT_SIZE; idx++) {
		if (idx > (inst[log].num - 1))
			idx = 0;

		if (idx == write_idx)
			break;

		if (idx < inst[log].num) {
			if (!inst[log].events[idx].identifier)
				continue;

			proc_val = PROC & inst[log].events[idx].identifier;
			sub_val = SUB & inst[log].events[idx].identifier;
			id_val = (SUB | ID) & inst[log].events[idx].identifier;
			id_only_val = ID & inst[log].events[idx].identifier;
			data1 = inst[log].events[idx].data1;
			data2 = inst[log].events[idx].data2;
			data3 = inst[log].events[idx].data3;

			if (!(proc_val & SMEM_LOG_CONT)) {
				i += scnprintf(buf + i, max - i, "\n");

				proc = find_sym(ID_SYM, proc_val);

				if (proc)
					i += scnprintf(buf + i, max - i,
						       "%4s: ", proc);
				else
					i += scnprintf(buf + i, max - i,
						       "%04x: ",
						       PROC &
						       inst[log].events[idx].
						       identifier);

				i += scnprintf(buf + i, max - i, "%10u ",
					       inst[log].events[idx].timetick);

				sub = find_sym(BASE_SYM, sub_val);

				if (sub)
					i += scnprintf(buf + i, max - i,
						       "%9s: ", sub);
				else
					i += scnprintf(buf + i, max - i,
						       "%08x: ", sub_val);

				id = find_sym(EVENT_SYM, id_val);

				if (id)
					i += scnprintf(buf + i, max - i,
						       "%11s: ", id);
				else
					i += scnprintf(buf + i, max - i,
						       "%08x: ", id_only_val);
			}

			if (proc_val & SMEM_LOG_CONT) {
				i += scnprintf(buf + i, max - i,
					       " %08x %08x %08x",
					       data1, data2, data3);
			} else if (id_val == SMEM_LOG_EVENT_CB) {
				unsigned vals[] = {data2, data3};
				unsigned j;
				unsigned mask;
				unsigned tmp;
				unsigned once;
				i += scnprintf(buf + i, max - i, "%08x ",
					       data1);
				for (j = 0; j < ARRAY_SIZE(vals); ++j) {
					i += scnprintf(buf + i, max - i, "[");
					mask = 0x80000000;
					once = 0;
					while (mask) {
						tmp = vals[j] & mask;
						mask >>= 1;
						if (!tmp)
							continue;
						sym = find_sym(SMSM_SYM, tmp);

						if (once)
							i += scnprintf(buf + i,
								       max - i,
								       " ");
						if (sym)
							i += scnprintf(buf + i,
								       max - i,
								       "%s",
								       sym);
						else
							i += scnprintf(buf + i,
								       max - i,
								       "%08x",
								       tmp);
						once = 1;
					}
					i += scnprintf(buf + i, max - i, "] ");
				}
			} else {
				i += scnprintf(buf + i, max - i,
					       "%08x %08x %08x",
					       data1, data2, data3);
			}
		}
	}
	if (cont) {
		inst[log].read_idx = idx;
		read_avail = (write_idx - inst[log].read_idx);
		if (read_avail < 0)
			read_avail = inst->num - inst->read_idx + write_idx;
		inst[log].last_read_avail = read_avail;
	}

	remote_spin_unlock_irqrestore(inst[log].remote_spinlock, flags);

	DBG("%s: read %d write %d idx %d num %d\n", __func__,
	    inst[log].read_idx, write_idx, idx, inst[log].num);

	return i;
}

static int debug_dump(char *buf, int max, uint32_t cont)
{
	int r;

	if (!inst[GEN].idx || !inst[GEN].events)
		return -ENODEV;

	while (cont) {
		update_read_avail(&inst[GEN]);
		r = wait_event_interruptible_timeout(inst[GEN].read_wait,
						     inst[GEN].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: read available %d\n", __func__,
		    inst[GEN].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[GEN].last_read_avail)
			break;
	}

	return _debug_dump(GEN, buf, max, cont);
}

static int debug_dump_sym(char *buf, int max, uint32_t cont)
{
	int r;

	if (!inst[GEN].idx || !inst[GEN].events)
		return -ENODEV;

	while (cont) {
		update_read_avail(&inst[GEN]);
		r = wait_event_interruptible_timeout(inst[GEN].read_wait,
						     inst[GEN].last_read_avail,
						     smem_log_timeout_ms *
						     HZ / 1000);
		DBG("%s: readavailable %d\n", __func__,
		    inst[GEN].last_read_avail);
		if (r < 0)
			return 0;
		else if (inst[GEN].last_read_avail)
			break;
	}

	return _debug_dump_sym(GEN, buf, max, cont);
}

static char debug_buffer[EVENTS_PRINT_SIZE];

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int r;
	int bsize = 0;
	int (*fill)(char *, int, uint32_t) = file->private_data;
	if (!(*ppos)) {
		bsize = fill(debug_buffer, EVENTS_PRINT_SIZE, 0);

		if (bsize < 0)
			bsize = scnprintf(debug_buffer,
				EVENTS_PRINT_SIZE, "Log not available\n");
	}
	DBG("%s: count %zu ppos %d\n", __func__, count, (unsigned int)*ppos);
	r =  simple_read_from_buffer(buf, count, ppos, debug_buffer,
				     bsize);
	return r;
}

static ssize_t debug_read_cont(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos)
{
	int (*fill)(char *, int, uint32_t) = file->private_data;
	char *buffer = kmalloc(count, GFP_KERNEL);
	int bsize;
	if (!buffer)
		return -ENOMEM;

	bsize = fill(buffer, count, 1);
	if (bsize < 0) {
		if (*ppos == 0)
			bsize = scnprintf(buffer, count, "Log not available\n");
		else
			bsize = 0;
	}

	DBG("%s: count %zu bsize %d\n", __func__, count, bsize);
	if (copy_to_user(buf, buffer, bsize)) {
		kfree(buffer);
		return -EFAULT;
	}
	*ppos += bsize;
	kfree(buffer);
	return bsize;
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static const struct file_operations debug_ops_cont = {
	.read = debug_read_cont,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
			 struct dentry *dent,
			 int (*fill)(char *buf, int max, uint32_t cont),
			 const struct file_operations *fops)
{
	debugfs_create_file(name, mode, dent, fill, fops);
}

static void smem_log_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smem_log", 0);
	if (IS_ERR(dent))
		return;

	debug_create("dump", 0444, dent, debug_dump, &debug_ops);
	debug_create("dump_sym", 0444, dent, debug_dump_sym, &debug_ops);

	debug_create("dump_cont", 0444, dent, debug_dump, &debug_ops_cont);
	debug_create("dump_sym_cont", 0444, dent,
		     debug_dump_sym, &debug_ops_cont);

	smem_log_timeout_ms = 500;
	smem_log_debug_mask = 0;
}
#else
static void smem_log_debugfs_init(void) {}
#endif

static int smem_log_initialize(void)
{
	int ret;

	ret = _smem_log_init();
	if (ret < 0) {
		pr_err("%s: init failed %d\n", __func__, ret);
		return ret;
	}

	ret = misc_register(&smem_log_dev);
	if (ret < 0) {
		pr_err("%s: device register failed %d\n", __func__, ret);
		return ret;
	}

	smem_log_enable = 1;
	smem_log_initialized = 1;
	smem_log_debugfs_init();
	return ret;
}

static int smem_module_init_notifier(struct notifier_block *this,
				    unsigned long code,
				    void *_cmd)
{
	int ret = 0;
	if (!smem_log_initialized)
		ret = smem_log_initialize();
	return ret;
}

static struct notifier_block nb = {
	.notifier_call = smem_module_init_notifier,
};

static int __init smem_log_init(void)
{
	return smem_module_init_notifier_register(&nb);
}


module_init(smem_log_init);

MODULE_DESCRIPTION("smem log");
MODULE_LICENSE("GPL v2");
