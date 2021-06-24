// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/mailbox_controller.h>
#include <linux/sched/clock.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/clk.h>

#include <spi_slave.h>
#include <mmprofile.h>

#include "cmdq-bdg.h"
#if IS_ENABLED(CONFIG_MTK_CMDQ_V3)
#include "cmdq_helper_ext.h"
#endif

#define SYSREG_IRQ_CTRL2	0x1c
#define SYSREG_IRQ_MSK_CLR	0x74

#define SYSBUF_BASE		0xA000
#define SYSBUF_SIZE		0x2000

#define CMDQ_SYSBUF_COPY_SIZE	0x80 /* 16 inst */
#define CMDQ_SYSBUF_SIZE	(CMDQ_SYSBUF_COPY_SIZE + CMDQ_INST_SIZE)

#define GCE_BASE		0x10000
#define GCE_DBG_CTL		0x3000
#define GCE_DBG0		0x3004
#define GCE_DBG2		0x300c

#define CMDQ_THR_SLOT_CYCLES	0x30

#define CMDQ_THR_BASE		0x100
#define CMDQ_THR_SIZE		0x80

#define CMDQ_THR_WARM_RESET	0x0
#define CMDQ_THR_ENABLE		0x4
#define CMDQ_THR_SUSPEND	0x8
#define CMDQ_THR_STATUS		0xc
#define CMDQ_THR_IRQ_FLAG	0x10
#define CMDQ_THR_IRQ_FLAG_EN	0x14
#define CMDQ_THR_PC		0x20
#define CMDQ_THR_END_ADDR	0x24
#define CMDQ_THR_QOS		0x40
#define CMDQ_THR_SPR0		0x60

#define CMDQ_THR_SLOT_CYCLES_UNIT	64
#define CMDQ_THR_SLOT_CYCLES_COUNT	128

#define CMDQ_THR_IRQ_DONE	0x1
#define CMDQ_THR_IRQ_ERROR	0x12

#define CMDQ_PKT_PRI_AGE	5
#define CMDQ_OP_JUMP_OFFSET	0x10000000
#define CMDQ_OP_JUMP_PA		0x10000001

#define ceil(x, y)		(((x) / (y)) + ((x) % (y) ? 1 : 0))
#define CMDQ_SET_ADDR(addr)	(((addr) >> 3) | BIT(24))
#define CMDQ_GET_ADDR(addr)	(((addr) & ~BIT(24)) << 3)

struct cmdq_mmp {
	mmp_event		cmdq;
	mmp_event		thread_enable;
	mmp_event		thread_suspend;
	mmp_event		task_send;
	mmp_event		task_wait;
	mmp_event		task_done;
};

struct cmdq_sysbuf {
	struct list_head	list_entry;
	u32			index;
	phys_addr_t		base;
};

struct cmdq {
	void __iomem		*base;
	phys_addr_t		base_pa;
	u32			irq;
	struct clk		*clock;
	struct clk		*clock_timer;
	atomic_t		usage;
	struct mbox_controller	mbox;
	struct cmdq_thread	thread[CMDQ_THR_MAX_COUNT];
	struct workqueue_struct	*timeout_wq;
	struct list_head	sysbuf;
	atomic_t		buf_count;
	struct cmdq_mmp		mmp;
};

struct cmdq *g_cmdq;

struct cmdq_task {
	struct list_head	list_entry;
	struct cmdq		*cmdq;
	struct cmdq_thread	*thread;
	struct cmdq_pkt		*pkt;
	struct list_head	sysbuf;
	phys_addr_t		cmd_base;
	size_t			cmd_size;
};

inline u32 spi_read_reg(const u32 addr)
{
	u32 val = UINT_MAX;

	spislv_read_register(addr, &val);
	return val;
}

inline s32 spi_write_reg(const u32 addr, const u32 val)
{
	return spislv_write_register(addr, val);
}

inline s32 spi_read_mem(const u32 addr, void *val, const s32 len)
{
	return spislv_read(addr, val, len);
}

inline s32 spi_write_mem(const u32 addr, void *val, const s32 len)
{
	return spislv_write(addr, val, len);
}

static inline u32 cmdq_bdg_thread_get_reg(struct cmdq_thread *thread,
	const u32 addr)
{
	return spi_read_reg((u32)thread->base + addr);
}

static inline void cmdq_bdg_thread_set_reg(struct cmdq_thread *thread,
	const u32 addr, const u32 val)
{
	spi_write_reg((u32)thread->base + addr, val);
}

static s32 cmdq_bdg_thread_warm_reset(struct cmdq_thread *thread)
{
	u32 val;

	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_WARM_RESET, BIT(0));

	// readl_poll_timeout_atomic
	val = cmdq_bdg_thread_get_reg(thread, CMDQ_THR_WARM_RESET);
	if (!(val & BIT(0)))
		return 0;

	cmdq_err("thread:%u warm reset failed:%#x:%#x",
		thread->idx, CMDQ_THR_WARM_RESET, val);
	return -EFAULT;
}

static inline void cmdq_bdg_thread_enable(struct cmdq_thread *thread)
{
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);

	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_ENABLE, BIT(0));

	mmprofile_log_ex(cmdq->mmp.thread_enable, MMPROFILE_FLAG_PULSE,
		(BIT(0) << 16) | thread->idx,
		cmdq_bdg_thread_get_reg(thread, CMDQ_THR_STATUS));
}

static inline void cmdq_bdg_thread_disable(struct cmdq_thread *thread)
{
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);

	WARN_ON(cmdq_bdg_thread_warm_reset(thread));
	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_ENABLE, 0x0);

	mmprofile_log_ex(cmdq->mmp.thread_enable, MMPROFILE_FLAG_PULSE,
		thread->idx, cmdq_bdg_thread_get_reg(thread, CMDQ_THR_STATUS));
}

static s32 cmdq_bdg_thread_suspend(struct cmdq_thread *thread)
{
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);
	u32 val;

	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_SUSPEND, BIT(0));

	mmprofile_log_ex(cmdq->mmp.thread_suspend, MMPROFILE_FLAG_PULSE,
		(BIT(0) << 16) | thread->idx,
		cmdq_bdg_thread_get_reg(thread, CMDQ_THR_STATUS));

	val = cmdq_bdg_thread_get_reg(thread, CMDQ_THR_ENABLE);
	if (!(val & BIT(0)))
		return 0;

	// readl_poll_timeout_atomic
	val = cmdq_bdg_thread_get_reg(thread, CMDQ_THR_STATUS);
	if (val & BIT(1))
		return 0;

	cmdq_err("thread:%u suspend failed:%#x:%#x",
		thread->idx, CMDQ_THR_STATUS, val);
	return -EFAULT;
}

static inline void cmdq_bdg_thread_resume(struct cmdq_thread *thread)
{
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);

	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_SUSPEND, 0x0);

	mmprofile_log_ex(cmdq->mmp.thread_suspend, MMPROFILE_FLAG_PULSE,
		thread->idx, cmdq_bdg_thread_get_reg(thread, CMDQ_THR_STATUS));
}

static inline phys_addr_t cmdq_bdg_thread_get_pc(struct cmdq_thread *thread)
{
	return CMDQ_GET_ADDR(cmdq_bdg_thread_get_reg(thread, CMDQ_THR_PC));
}

static inline void cmdq_bdg_thread_set_pc(struct cmdq_thread *thread,
	const phys_addr_t val)
{
	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_PC, CMDQ_SET_ADDR(val));
}

static inline phys_addr_t cmdq_bdg_thread_get_end(struct cmdq_thread *thread)
{
	return CMDQ_GET_ADDR(
		cmdq_bdg_thread_get_reg(thread, CMDQ_THR_END_ADDR));
}

static inline void cmdq_bdg_thread_set_end(struct cmdq_thread *thread,
	const phys_addr_t val)
{
	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_END_ADDR, CMDQ_SET_ADDR(val));
}

static s32 cmdq_bdg_clk_enable(struct cmdq_thread *thread)
{
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);
	s32 ret = atomic_read(&cmdq->usage);

	if (ret & BIT(thread->idx)) {
		cmdq_err("cmdq:%pa thread:%u usage:%#x have enabled",
			&cmdq->base_pa, thread->idx, ret);
		return ret;
	}

	atomic_add_return(BIT(thread->idx), &cmdq->usage);
	cmdq_msg("%s: cmdq:%pa thread:%u usage:%#x", __func__,
		&cmdq->base_pa, thread->idx, atomic_read(&cmdq->usage));
	return 0;
}

static void cmdq_bdg_clk_disable(struct cmdq_thread *thread)
{
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);
	s32 ret = atomic_read(&cmdq->usage);

	if (!(ret | BIT(thread->idx))) {
		cmdq_err("cmdq:%pa thread:%u usage:%#x have disabled",
			&cmdq->base_pa, thread->idx, ret);
		return;
	}

	atomic_sub_return(BIT(thread->idx), &cmdq->usage);
	cmdq_msg("%s: cmdq:%pa thread:%u usage:%#x", __func__,
		&cmdq->base_pa, thread->idx, atomic_read(&cmdq->usage));
}

static void cmdq_bdg_dump_sysbuf(const phys_addr_t base, const size_t size)
{
	u64 *command;
	s32 i;

	command = kzalloc(size, GFP_KERNEL);
	if (!command)
		return;

	cmdq_msg("%s: base:%pa size:%ld", __func__, &base, size);
	spi_read_mem(base, command, size);
	for (i = 0; i < size / CMDQ_INST_SIZE; i++)
		cmdq_msg("%s: inst[%d]:%#llx", __func__, i, *(command + i));

	kfree(command);
}

static inline phys_addr_t cmdq_bdg_task_get_end(struct cmdq_task *task)
{
	struct cmdq_sysbuf *buf =
		list_last_entry(&task->sysbuf, typeof(*buf), list_entry);

	return buf->base + (task->cmd_size % CMDQ_SYSBUF_SIZE);
}

static bool cmdq_bdg_task_running(struct cmdq_task *task, const phys_addr_t pa)
{
	struct cmdq_sysbuf *buf, *temp;
	phys_addr_t end;

	list_for_each_entry_safe(buf, temp, &task->sysbuf, list_entry) {
		if (list_is_last(&buf->list_entry, &task->sysbuf))
			end = buf->base + (task->cmd_size % CMDQ_SYSBUF_SIZE);
		else
			end = buf->base + CMDQ_SYSBUF_SIZE;

		if (pa >= buf->base && pa < end)
			return true;
	}
	return false;
}

void cmdq_bdg_client_get_irq(struct cmdq_client *cl, u32 *irq)
{
	*irq = cmdq_bdg_thread_get_reg(cl->chan->con_priv, CMDQ_THR_IRQ_FLAG);
}
EXPORT_SYMBOL(cmdq_bdg_client_get_irq);

static inline void cmdq_bdg_dump_gce(void)
{
	const u32 irq = spi_read_reg(GCE_BASE + CMDQ_THR_IRQ_FLAG);
#if 0
	u32 dbg0[3], dbg2[6], i;

	for (i = 0; i < 6; i++) {
		if (i < 3) {
			spi_write_reg(GCE_BASE + GCE_DBG_CTL, (i << 8) | i);
			dbg0[i] = spi_read_reg(GCE_BASE + GCE_DBG0);
		} else
			spi_write_reg(GCE_BASE + GCE_DBG_CTL, (i << 8));
		dbg2[i] = spi_read_reg(GCE_BASE + GCE_DBG2);
	}

	cmdq_msg(
		"%s: gce:%#x irq:%#x dbg0:%#x %#x %#x dbg2:%#x %#x %#x %#x %#x %#x",
		__func__, GCE_BASE, irq, dbg0[0], dbg0[1], dbg0[2],
		dbg2[0], dbg2[1], dbg2[2], dbg2[3], dbg2[4], dbg2[5]);
#endif
	cmdq_msg("%s: gce:%#x irq:%#x", __func__, GCE_BASE, irq);
}

static inline u32 cmdq_bdg_dump_thread(struct cmdq_thread *thread)
{
	u32 val[10], pc, end, spr[4];
	u64 inst;

	spi_read_mem(
		(u32)thread->base + CMDQ_THR_STATUS, val, sizeof(u32) * 10);
	pc = cmdq_bdg_thread_get_pc(thread);
	end = cmdq_bdg_thread_get_end(thread);
	spi_read_mem((u32)thread->base + CMDQ_THR_SPR0, spr, sizeof(u32) * 4);
	spi_read_mem(pc, &inst, CMDQ_INST_SIZE);

	cmdq_msg(
		"%s:%u priority:%d status:%#x irq:%#x pc:%#x inst:%#llx end:%#x cookie:%#x wait:%#x spr:%#x %#x %#x %#x",
		__func__, thread->idx, thread->priority,
		val[0], val[1], pc, inst, end, val[7], val[9],
		spr[0], spr[1], spr[2], spr[3]);

	return pc;
}

#if IS_ENABLED(CONFIG_MTK_CMDQ_V3)
void cmdq_bdg_dump_handle(struct cmdqRecStruct *rec, const char *tag)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_V3)
	struct cmdq_client *client = (struct cmdq_client *)rec->pkt->cl;
	u32 pc, base;

	cmdq_msg("%s: rec:%p pkt:%p thread:%d scenario:%d engine:%#llx",
		__func__,
		rec, rec->pkt, rec->thread, rec->scenario, rec->engineFlag);
	cmdq_bdg_dump_gce();
	pc = cmdq_bdg_dump_thread(client->chan->con_priv);
	if (!strncmp(tag, "ERR", 3)) {
		base = SYSBUF_BASE + CMDQ_SYSBUF_SIZE *
			((pc - SYSBUF_BASE) / CMDQ_SYSBUF_SIZE);
		cmdq_bdg_dump_sysbuf(base, pc - base + CMDQ_INST_SIZE);
	}
#else
	cmdq_msg("%s: rec:%p CMDQ_V3 not support", __func__, ptr);
#endif
}
EXPORT_SYMBOL(cmdq_bdg_dump_handle);
#endif

static void cmdq_bdg_task_callback(struct cmdq_pkt *pkt, const s32 err)
{
	struct cmdq_cb_data data;

	if (pkt && pkt->cb.cb) {
		data.err = err;
		data.data = pkt->cb.data;
		pkt->cb.cb(data);
	}
}

static void cmdq_bdg_task_error_callback(struct cmdq_pkt *pkt, const s32 err)
{
	struct cmdq_cb_data data;

	if (pkt && pkt->err_cb.cb) {
		data.err = err;
		data.data = pkt->err_cb.data;
		pkt->err_cb.cb(data);
	}
}

static void cmdq_bdg_task_done(struct cmdq_task *task, const s32 err)
{
	struct cmdq_sysbuf *buf, *temp;
	s32 count = 0;

	cmdq_bdg_task_callback(task->pkt, err);
	list_del_init(&task->list_entry);
	mmprofile_log_ex(task->cmdq->mmp.task_done, MMPROFILE_FLAG_PULSE,
		((s16)err << 16) | task->thread->idx, (unsigned long)task->pkt);

	if (task->pkt->reuse) {
		cmdq_msg("%s: thread:%u pkt:%p task:%p err:%d reuse",
			__func__, task->thread->idx, task->pkt, task, err);
		return;
	}

	list_for_each_entry_safe(buf, temp, &task->sysbuf, list_entry) {
		list_move_tail(&buf->list_entry, &task->cmdq->sysbuf);
		count += 1;
	}
	atomic_add_return(count, &task->cmdq->buf_count);
	kfree(task);
}

static void cmdq_bdg_thread_irq_handler(struct cmdq_thread *thread)
{
	struct cmdq_task *task, *temp, *curr = NULL, *next = NULL;
	phys_addr_t pc, end;
	u32 irq;

	if (!(cmdq_bdg_thread_get_reg(thread, CMDQ_THR_ENABLE) & BIT(0))) {
		cmdq_msg("%s: thread:%u disable", __func__, thread->idx);
		return;
	}

	pc = cmdq_bdg_thread_get_pc(thread);
	end = cmdq_bdg_thread_get_end(thread);

	irq = cmdq_bdg_thread_get_reg(thread, CMDQ_THR_IRQ_FLAG);
	cmdq_bdg_thread_set_reg(thread, CMDQ_THR_IRQ_FLAG, ~irq);
	cmdq_msg("%s: thread:%u irq:%#x pc:%pa end:%pa",
		__func__, thread->idx, irq, &pc, &end);

	irq &= (CMDQ_THR_IRQ_DONE | CMDQ_THR_IRQ_ERROR);
	if (!irq)
		return;

	task = list_first_entry_or_null(
		&thread->task_busy_list, typeof(*task), list_entry);
	if (task && task->pkt->loop) {
		cmdq_bdg_task_callback(task->pkt, irq);
		cmdq_msg("%s: thread:%u loop:%d",
			__func__, thread->idx, task->pkt->loop);
		return;
	}

	list_for_each_entry_safe(
		task, temp, &thread->task_busy_list, list_entry) {
		if (cmdq_bdg_task_running(task, pc))
			curr = task;

		if (!curr ||
			pc == cmdq_bdg_task_get_end(task) - CMDQ_INST_SIZE) {
			cmdq_bdg_task_done(task, 0);
		} else if (irq & CMDQ_THR_IRQ_ERROR) {
			u64 inst;

			spi_read_mem(pc, &inst, CMDQ_INST_SIZE);
			cmdq_err("thread:%u irq:%#x pkt:%p pc:%pa inst:%#llx",
				thread->idx, irq, task->pkt, &pc, inst);
			cmdq_bdg_task_done(task, -EINTR);

			next = list_first_entry_or_null(&thread->task_busy_list,
				typeof(*next), list_entry);
			if (next)
				cmdq_bdg_thread_set_pc(thread, next->cmd_base);
		}

		if (curr)
			break;
	}

	if (!list_empty(&thread->task_busy_list)) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms));
		thread->timer_mod = sched_clock();
		cmdq_msg("%s: thread:%u timeout_ms:%u reset timer",
			__func__, thread->idx, thread->timeout_ms);
	} else
		cmdq_msg("%s: thread:%u without task", __func__, thread->idx);
}

s32 cmdq_bdg_irq_handler(void)
{
	struct cmdq *cmdq = g_cmdq;
	struct cmdq_thread *thread;
	s32 bit, ret = atomic_read(&cmdq->usage);
	unsigned long irq;

	if (!ret)
		cmdq_err("cmdq:%pa usage:%#x have disabled",
			&cmdq->base_pa, ret);

	irq = spi_read_reg(cmdq->base_pa + CMDQ_THR_IRQ_FLAG);
	cmdq_msg("%s: cmdq:%pa usage:%#x irq:%#x:%#x",
		__func__, &cmdq->base_pa, ret, CMDQ_THR_IRQ_FLAG, irq);

	if (irq == UINT_MAX)
		return IRQ_NONE;

	ret = IRQ_HANDLED;
	for_each_clear_bit(bit, &irq, 32) {
		thread = &cmdq->thread[bit];
		cmdq_msg("%s: bit:%d thread:%u occupied:%d",
			__func__, bit, thread->idx, thread->occupied);

		if (!thread->occupied) {
			ret = IRQ_NONE;
			continue;
		}
		cmdq_bdg_thread_irq_handler(thread);
	}

	spi_write_reg(cmdq->base_pa + CMDQ_THR_IRQ_FLAG, UINT_MAX);
	cmdq_msg("%s: cmdq:%pa usage:%#x irq:%#x:%#x",
		__func__, &cmdq->base_pa, ret, CMDQ_THR_IRQ_FLAG,
		spi_read_reg(cmdq->base_pa + CMDQ_THR_IRQ_FLAG));

	return ret;
}
EXPORT_SYMBOL(cmdq_bdg_irq_handler);

static void cmdq_bdg_thread_timeout_work(struct work_struct *work)
{
	struct cmdq_thread *thread =
		container_of(work, typeof(*thread), timeout_work);
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);
	struct cmdq_task *task, *temp, *next;
	s32 ret = atomic_read(&cmdq->usage);
	u64 duration;
	phys_addr_t pc;

	if (!(ret | BIT(thread->idx)))
		cmdq_err("cmdq:%pa thread:%u usage:%#x have disabled",
			&cmdq->base_pa, thread->idx, ret);

	if (list_empty(&thread->task_busy_list)) {
		cmdq_msg("%s: thread:%u without task", __func__, thread->idx);
		cmdq_bdg_thread_disable(thread);
		cmdq_bdg_clk_disable(thread);
		return;
	}

	WARN_ON(cmdq_bdg_thread_suspend(thread));
	cmdq_bdg_thread_irq_handler(thread);

	pc = cmdq_bdg_thread_get_pc(thread);
	list_for_each_entry_safe(
		task, temp, &thread->task_busy_list, list_entry) {
		if (cmdq_bdg_task_running(task, pc)) {
			u64 inst;

			spi_read_mem(pc, &inst, CMDQ_INST_SIZE);
			cmdq_err("thread:%u pkt:%p pc:%pa inst:%#llx",
				thread->idx, task->pkt, &pc, inst);

			cmdq_bdg_task_error_callback(task->pkt, -ETIMEDOUT);
			cmdq_bdg_task_done(task, -ETIMEDOUT);

			next = list_first_entry_or_null(&thread->task_busy_list,
				typeof(*next), list_entry);
			if (next)
				cmdq_bdg_thread_set_pc(thread, next->cmd_base);
			break;
		}
		cmdq_bdg_task_done(task, 0);
	}

	if (list_empty(&thread->task_busy_list)) {
		cmdq_msg("%s: thread:%u without task after irq handler",
			__func__, thread->idx);
		cmdq_bdg_thread_disable(thread);
		cmdq_bdg_clk_disable(thread);
		return;
	}

	// cmdq_bdg_thread_timeout_excceed
	duration = div_s64(sched_clock() - thread->timer_mod, 1000000);
	if (duration < thread->timeout_ms)
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms - duration));
	else
		mod_timer(&thread->timeout,
			jiffies + msecs_to_jiffies(thread->timeout_ms));

	thread->timer_mod = sched_clock();
	cmdq_msg("%s: thread:%u timeout_ms:%u duration:%llu",
		__func__, thread->idx, thread->timeout_ms, duration);
	cmdq_bdg_thread_resume(thread);
}

static void cmdq_bdg_thread_timeout(struct timer_list *timer)
{
	struct cmdq_thread *thread = from_timer(thread, timer, timeout);
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);

	if (list_empty(&thread->task_busy_list)) {
		cmdq_msg("%s: thread:%u without task", __func__, thread->idx);
		return;
	}

	if (!work_pending(&thread->timeout_work)) {
		cmdq_msg("%s: thread:%u queue work", __func__, thread->idx);
		queue_work(cmdq->timeout_wq, &thread->timeout_work);
	} else
		cmdq_msg("%s: thread:%u ignore timeout", __func__, thread->idx);
}

static void cmdq_bdg_thread_shutdown(struct cmdq_thread *thread)
{
	struct cmdq *cmdq =
		container_of(thread->chan->mbox, typeof(*cmdq), mbox);
	struct cmdq_task *task, *temp;

	if (list_empty(&thread->task_busy_list)) {
		cmdq_msg("%s: thread:%u without task", __func__, thread->idx);
		return;
	}

	WARN_ON(cmdq_bdg_thread_suspend(thread));
	cmdq_bdg_thread_irq_handler(thread);

	list_for_each_entry_safe(
		task, temp, &thread->task_busy_list, list_entry) {
		cmdq_bdg_task_done(task, -ECONNABORTED);
	}

	cmdq_bdg_thread_disable(thread);
	cmdq_bdg_clk_disable(thread);
}

void cmdq_bdg_client_shutdown(struct cmdq_client *cl)
{
	cmdq_bdg_thread_shutdown(cl->chan->con_priv);
}
EXPORT_SYMBOL(cmdq_bdg_client_shutdown);

static inline void cmdq_bdg_mbox_write_sysbuf(struct cmdq_task *task)
{
	struct cmdq_pkt *pkt = task->pkt;
	struct cmdq_pkt_buffer *pkt_buf, *pkt_temp;
	struct cmdq_sysbuf *buf, *temp;
	s32 count, remain;

	remain = CMDQ_CMD_BUFFER_SIZE / CMDQ_SYSBUF_COPY_SIZE;
	buf = list_first_entry(&task->sysbuf, typeof(*buf), list_entry);
	list_for_each_entry_safe(pkt_buf, pkt_temp, &pkt->buf, list_entry) {
		if (list_is_last(&pkt_buf->list_entry, &pkt->buf))
			remain = ceil(pkt->cmd_buf_size % CMDQ_CMD_BUFFER_SIZE,
				CMDQ_SYSBUF_COPY_SIZE);
		cmdq_msg("%s: pkt_buf va:%p pa:%pa remain:%d",
			__func__, pkt_buf->va_base, &pkt_buf->pa_base, remain);

		count = 0;
		list_for_each_entry_safe_from(
			buf, temp, &task->sysbuf, list_entry) {
			u64 inst = ((u64)CMDQ_OP_JUMP_PA << 32) |
				CMDQ_SET_ADDR(temp->base);

			cmdq_msg("%s: count:%d remain:%d buf(%d):%pa", __func__,
				count, remain, buf->index, &buf->base);

			if (list_is_last(&buf->list_entry, &task->sysbuf)) {
				spi_write_mem(buf->base, pkt_buf->va_base +
					CMDQ_SYSBUF_COPY_SIZE * count,
					task->cmd_size % CMDQ_SYSBUF_SIZE);
				break;
			}

			spi_write_mem(buf->base, pkt_buf->va_base +
				CMDQ_SYSBUF_COPY_SIZE * count,
				CMDQ_SYSBUF_COPY_SIZE);

			if (++count == remain) { // replace jump
				cmdq_msg("%s:jump pc:%pa:%#x inst:%#llx",
					__func__, &buf->base,
					CMDQ_SYSBUF_COPY_SIZE - CMDQ_INST_SIZE,
					inst);
				spi_write_mem(buf->base +
					CMDQ_SYSBUF_COPY_SIZE - CMDQ_INST_SIZE,
					&inst, CMDQ_INST_SIZE);
				break;
			}

			// insert jump
			cmdq_msg("%s:jump pc:%pa:%#x inst:%#llx",
				__func__, &buf->base,
				CMDQ_SYSBUF_COPY_SIZE, inst);
			spi_write_mem(buf->base +
				CMDQ_SYSBUF_COPY_SIZE, &inst, CMDQ_INST_SIZE);
		}
	}

	if (pkt->loop) {
		u64 inst = ((u64)CMDQ_OP_JUMP_PA << 32) |
			CMDQ_SET_ADDR(task->cmd_base);
		phys_addr_t end = cmdq_bdg_task_get_end(task) - CMDQ_INST_SIZE;

		spi_write_mem(end, &inst, CMDQ_INST_SIZE);
	}
	cmdq_bdg_dump_sysbuf(buf->base, task->cmd_size % CMDQ_SYSBUF_SIZE);
}

static inline void cmdq_bdg_task_connect(struct cmdq_task *task,
	struct cmdq_task *next)
{
	phys_addr_t end;
	u64 val = ((u64)CMDQ_OP_JUMP_PA << 32) | CMDQ_SET_ADDR(next->cmd_base);
	u64 inst[2];

	end = cmdq_bdg_task_get_end(task) - CMDQ_INST_SIZE;
	spi_read_mem(end, &inst[0], CMDQ_INST_SIZE);
	spi_write_mem(end, &val, CMDQ_INST_SIZE);
	spi_read_mem(end, &inst[1], CMDQ_INST_SIZE);

	cmdq_msg("%s: end:%pa inst:%#llx val:%#llx inst:%#llx",
		__func__, &end, inst[0], val, inst[1]);
}

static inline void cmdq_bdg_task_insert_thread(struct cmdq_task *task,
	const phys_addr_t pa, struct list_head **pos)
{
	struct cmdq_task *prev = NULL, *curr, *next = NULL;

	list_for_each_entry_reverse(
		curr, &task->thread->task_busy_list, list_entry) {
		prev = curr;
		if (next)
			next->pkt->priority += CMDQ_PKT_PRI_AGE;
		if (cmdq_bdg_task_running(curr, pa))
			break;
		if (curr->pkt->priority >= task->pkt->priority)
			break;
		next = curr;
	}
	*pos = &prev->list_entry;

	cmdq_bdg_task_connect(prev, task);

	if (next)
		cmdq_bdg_task_connect(task, next);
}

static int cmdq_bdg_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct cmdq *cmdq = dev_get_drvdata(chan->mbox->dev);
	struct cmdq_thread *thread = chan->con_priv;
	struct cmdq_pkt *pkt = data;
	struct cmdq_task *task, *last;
	struct cmdq_sysbuf *buf, *temp;
	struct list_head *pos;
	phys_addr_t pc, end;
	unsigned long flags;
	s32 count, remain;

	if (!pkt) {
		cmdq_msg("%s: pkt:%p", __func__, pkt);
		dump_stack();
		return 0;
	}

	if (list_empty(&pkt->buf)) {
		cmdq_err("thread:%u pkt:%p without command", thread->idx, pkt);
		dump_stack();
		cmdq_bdg_task_callback(pkt, -EINVAL);
		return -EINVAL;
	}

	task = pkt->bdg_data ? (struct cmdq_task *)pkt->bdg_data : NULL;
	if (task) {
		if (!pkt->reuse) {
			cmdq_msg("%s: thread:%u pkt:%p task:%p reuse done",
				__func__, thread->idx, pkt, task);
			cmdq_bdg_task_done(task, 0);
			return 0;
		}

		if (task->list_entry.next != &task->list_entry) {
			cmdq_err("thread:%u pkt:%p task:%p in list",
				thread->idx, pkt, task);
			cmdq_bdg_task_callback(pkt, -EEXIST);
			return -EEXIST;
		}

		cmdq_msg("%s: thread:%u pkt:%p data:%p task:%p existed",
			__func__, thread->idx, pkt, pkt->bdg_data, task);
	} else {
		task = kzalloc(sizeof(*task), GFP_ATOMIC);
		if (!task) {
			cmdq_bdg_task_callback(pkt, -ENOMEM);
			return -ENOMEM;
		}
		pkt->task_alloc = true;

		task->cmdq = cmdq;
		task->thread = thread;
		task->pkt = pkt;
		INIT_LIST_HEAD(&task->sysbuf);

		// allocate sysbuf
		task->cmd_size = pkt->cmd_buf_size + CMDQ_INST_SIZE *
			(ceil(pkt->cmd_buf_size, CMDQ_SYSBUF_COPY_SIZE) - 1);
		count = ceil(task->cmd_size, CMDQ_SYSBUF_SIZE);

		remain = atomic_read(&cmdq->buf_count);
		if (remain < count) {
			cmdq_err(
				"thread:%u pkt:%p cmd_buf_size:%ld cmd_size:%ld count:%d without enough sysbuf:%d",
				thread->idx, pkt, pkt->cmd_buf_size,
				task->cmd_size, count, remain);

			cmdq_bdg_task_callback(pkt, -ENOMEM);
			kfree(task);
			return -ENOMEM;
		}

		remain = atomic_sub_return(count, &cmdq->buf_count);
		cmdq_msg(
			"%s: thread:%u pkt:%p cmd_buf_size:%ld cmd_size:%ld count:%d remain:%d",
			__func__, thread->idx, pkt, pkt->cmd_buf_size,
			task->cmd_size, count, remain);

		list_for_each_entry_safe(buf, temp, &cmdq->sysbuf, list_entry) {
			list_move_tail(&buf->list_entry, &task->sysbuf);
			if (!task->cmd_base)
				task->cmd_base = buf->base;
			if (!--count)
				break;
		}

		spin_unlock_irqrestore(&thread->chan->lock, flags);
		cmdq_bdg_mbox_write_sysbuf(task);
		spin_lock_irqsave(&thread->chan->lock, flags);
		pkt->bdg_data = (void *)task;
	}

	// insert task
	spin_unlock_irqrestore(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list)) {
		WARN_ON(cmdq_bdg_clk_enable(thread));
		WARN_ON(cmdq_bdg_thread_warm_reset(thread));

		spi_write_reg(cmdq->base_pa + CMDQ_THR_SLOT_CYCLES,
			CMDQ_THR_SLOT_CYCLES_COUNT * CMDQ_THR_SLOT_CYCLES_UNIT);

		cmdq_bdg_thread_set_reg(thread, CMDQ_THR_QOS, thread->priority);
		cmdq_bdg_thread_set_end(thread, cmdq_bdg_task_get_end(task));
		cmdq_bdg_thread_set_pc(thread, task->cmd_base);
		cmdq_bdg_thread_set_reg(thread, CMDQ_THR_IRQ_FLAG_EN,
			CMDQ_THR_IRQ_DONE | CMDQ_THR_IRQ_ERROR);

		if (thread->timeout_ms != CMDQ_NO_TIMEOUT) {
			mod_timer(&thread->timeout,
				jiffies + msecs_to_jiffies(thread->timeout_ms));
			thread->timer_mod = sched_clock();
		}
		list_add_tail(&task->list_entry, &thread->task_busy_list);

		pc = cmdq_bdg_thread_get_pc(thread);
		end = cmdq_bdg_thread_get_end(thread);

		// irq clear
		spislv_write_register_mask(
			SYSREG_IRQ_MSK_CLR, BIT(10), BIT(10));
		cmdq_msg("%s:single pkt:%p thread:%u pc:%pa end:%pa irq:%#x",
			__func__, pkt, thread->idx, &pc, &end,
			spi_read_reg(SYSREG_IRQ_CTRL2));
		cmdq_bdg_thread_enable(thread);
	} else {
		WARN_ON(cmdq_bdg_thread_suspend(thread));

		pc = cmdq_bdg_thread_get_pc(thread);
		end = cmdq_bdg_thread_get_end(thread);
		if (pc == end - CMDQ_INST_SIZE || pc == end) {
			cmdq_bdg_thread_set_pc(thread, task->cmd_base);
			list_add_tail(
				&task->list_entry, &thread->task_busy_list);
		} else {
			cmdq_bdg_task_insert_thread(task, pc, &pos);
			cmdq_bdg_thread_set_pc(task->thread,
				cmdq_bdg_thread_get_pc(task->thread));
			list_add(&task->list_entry, pos);
		}
		last = list_last_entry(
			&thread->task_busy_list, typeof(*last), list_entry);
		cmdq_bdg_thread_set_end(thread, cmdq_bdg_task_get_end(last));

		pc = cmdq_bdg_thread_get_pc(thread);
		end = cmdq_bdg_thread_get_end(thread);
		cmdq_msg("%s:multiple pkt:%p thread:%u pc:%pa end:%pa",
			__func__, pkt, thread->idx, &pc, &end);
		cmdq_bdg_thread_resume(thread);
	}
	mmprofile_log_ex(cmdq->mmp.task_send, MMPROFILE_FLAG_PULSE,
		thread->idx, (unsigned long)pkt);

	spin_lock_irqsave(&thread->chan->lock, flags);
	return 0;
}

static int cmdq_bdg_mbox_startup(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	thread->occupied = true;
	return 0;
}

static void cmdq_bdg_mbox_shutdown(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	cmdq_bdg_thread_shutdown(thread);
	thread->occupied = false;
}

static bool cmdq_bdg_mbox_last_tx_done(struct mbox_chan *chan)
{
	return true;
}

static struct mbox_chan *cmdq_bdg_xlate(struct mbox_controller *mbox,
	const struct of_phandle_args *sp)
{
	struct cmdq_thread *thread;
	s32 idx = sp->args[0];

	if (idx >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	thread = mbox->chans[idx].con_priv;
	thread->chan = &mbox->chans[idx];
	thread->timeout_ms = sp->args[1] ? sp->args[1] : CMDQ_TIMEOUT_DEFAULT;
	thread->priority = sp->args[2];
	return &mbox->chans[idx];
}

void cmdq_bdg_mmprofile_task_wait(struct mbox_chan *chan, struct cmdq_pkt *pkt)
{
	struct cmdq_thread *thread = chan->con_priv;
	struct cmdq *cmdq = container_of(chan->mbox, typeof(*cmdq), mbox);

	mmprofile_log_ex(cmdq->mmp.task_wait, MMPROFILE_FLAG_PULSE,
		thread->idx, (unsigned long)pkt);
}

static const struct mbox_chan_ops cmdq_bdg_mbox_chan_ops = {
	.send_data = cmdq_bdg_mbox_send_data,
	.startup = cmdq_bdg_mbox_startup,
	.shutdown = cmdq_bdg_mbox_shutdown,
	.last_tx_done = cmdq_bdg_mbox_last_tx_done,
};

static int cmdq_bdg_suspend(struct device *dev)
{
	return 0;
}

static int cmdq_bdg_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops cmdq_bdg_pm_ops = {
	.suspend = cmdq_bdg_suspend,
	.resume = cmdq_bdg_resume,
};

static inline void cmdq_bdg_mmp_init(struct cmdq *cmdq)
{
	mmprofile_enable(true);
	if (cmdq->mmp.cmdq) {
		mmprofile_start(true);
		return;
	}

	cmdq->mmp.cmdq = mmprofile_register_event(MMP_ROOT_EVENT, "cmdq_bdg");

	cmdq->mmp.thread_enable =
		mmprofile_register_event(cmdq->mmp.cmdq, "thread_enable");
	cmdq->mmp.thread_suspend =
		mmprofile_register_event(cmdq->mmp.cmdq, "thread_suspend");

	cmdq->mmp.task_send =
		mmprofile_register_event(cmdq->mmp.cmdq, "task_send");
	cmdq->mmp.task_wait =
		mmprofile_register_event(cmdq->mmp.cmdq, "task_wait");
	cmdq->mmp.task_done =
		mmprofile_register_event(cmdq->mmp.cmdq, "task_done");

	mmprofile_enable_event_recursive(cmdq->mmp.cmdq, true);
	mmprofile_start(true);
}

static int cmdq_bdg_probe(struct platform_device *pdev)
{
	struct cmdq_sysbuf *buf;
	struct cmdq *cmdq;
	s32 i, ret = 0;

	cmdq = devm_kzalloc(&pdev->dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	cmdq->base = (void *)GCE_BASE;
	cmdq->base_pa = GCE_BASE;

	cmdq->mbox.chans = devm_kcalloc(&pdev->dev, CMDQ_THR_MAX_COUNT,
		sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		cmdq->thread[i].base =
			cmdq->base + CMDQ_THR_BASE + CMDQ_THR_SIZE * i;
		cmdq->thread[i].gce_pa = cmdq->base_pa;
		INIT_LIST_HEAD(&cmdq->thread[i].task_busy_list);
		timer_setup(&cmdq->thread[i].timeout,
			cmdq_bdg_thread_timeout, 0);
		INIT_WORK(&cmdq->thread[i].timeout_work,
			cmdq_bdg_thread_timeout_work);
		cmdq->thread[i].idx = i;
		cmdq->mbox.chans[i].con_priv = &cmdq->thread[i];
	}

	cmdq->mbox.dev = &pdev->dev;
	cmdq->mbox.ops = &cmdq_bdg_mbox_chan_ops;
	cmdq->mbox.num_chans = CMDQ_THR_MAX_COUNT;
	cmdq->mbox.of_xlate = cmdq_bdg_xlate;

	ret = mbox_controller_register(&cmdq->mbox);
	if (ret) {
		cmdq_err("mbox_controller_register failed:%d", ret);
		return ret;
	}

	cmdq->timeout_wq = create_singlethread_workqueue("cmdq_bdg_timeout_wq");
	// cmdq->buf_dump_wq

	INIT_LIST_HEAD(&cmdq->sysbuf);
	for (i = 0; i < SYSBUF_SIZE / CMDQ_SYSBUF_SIZE; i++) {
		buf = kzalloc(sizeof(*buf), GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
		buf->index = i;
		buf->base = SYSBUF_BASE + CMDQ_SYSBUF_SIZE * i;
		list_add_tail(&buf->list_entry, &cmdq->sysbuf);
	}
	atomic_set(&cmdq->buf_count, SYSBUF_SIZE / CMDQ_SYSBUF_SIZE);

	cmdq_bdg_mmp_init(cmdq);
	platform_set_drvdata(pdev, cmdq);

	cmdq_msg("%s: pdev:%p buf_count:%d",
		__func__, pdev, atomic_read(&cmdq->buf_count));

	g_cmdq = cmdq;
	return ret;
}

static int cmdq_bdg_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id cmdq_bdg_of_ids[] = {
	{.compatible = "mediatek,mailbox-gce-bdg",},
	{}
};

static struct platform_driver cmdq_bdg_drv = {
	.probe = cmdq_bdg_probe,
	.remove = cmdq_bdg_remove,
	.driver = {
		.name = "cmdq-bdg-mailbox",
		.of_match_table = cmdq_bdg_of_ids,
		.pm = &cmdq_bdg_pm_ops,
	},
};

static __init int cmdq_bdg_init(void)
{
	s32 ret = platform_driver_register(&cmdq_bdg_drv);

	if (ret)
		cmdq_err("platform_driver_register failed:%d", ret);
	return ret;
}
arch_initcall(cmdq_bdg_init);
