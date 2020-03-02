/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>

#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/atomic.h>
#include <linux/sched/clock.h>

/* ddp main/sub, mdp path 0/1/2/3, general(misc) */
#define CMDQ_OP_CODE_MASK		(0xff << CMDQ_OP_CODE_SHIFT)
#define CMDQ_IRQ_MASK			GENMASK(CMDQ_THR_MAX_COUNT - 1, 0)

#define CMDQ_CURR_IRQ_STATUS		0x10
#define CMDQ_THR_SLOT_CYCLES		0x30
#define CMDQ_SYNC_TOKEN_ID		0x60
#define CMDQ_SYNC_TOKEN_VAL		0x64
#define CMDQ_SYNC_TOKEN_UPD		0x68
#define CMDQ_PREFETCH_GSIZE		0xC0
#define CMDQ_TPR_MASK			0xD0

#define CMDQ_THR_BASE			0x100
#define CMDQ_THR_SIZE			0x80
#define CMDQ_THR_WARM_RESET		0x00
#define CMDQ_THR_ENABLE_TASK		0x04
#define CMDQ_THR_SUSPEND_TASK		0x08
#define CMDQ_THR_CURR_STATUS		0x0c
#define CMDQ_THR_IRQ_STATUS		0x10
#define CMDQ_THR_IRQ_ENABLE		0x14
#define CMDQ_THR_CURR_ADDR		0x20
#define CMDQ_THR_END_ADDR		0x24
#define CMDQ_THR_CNT			0x28
#define CMDQ_THR_WAIT_TOKEN		0x30
#define CMDQ_THR_CFG			0x40
#define CMDQ_THR_PREFETCH		0x44
#define CMDQ_THR_INST_CYCLES		0x50
#define CMDQ_THR_INST_THRESX		0x54
#define CMDQ_THR_SPR			0x60

#define CMDQ_THR_ENABLED		0x1
#define CMDQ_THR_DISABLED		0x0
#define CMDQ_THR_SUSPEND		0x1
#define CMDQ_THR_RESUME			0x0
#define CMDQ_THR_STATUS_SUSPENDED	BIT(1)
#define CMDQ_THR_DO_WARM_RESET		BIT(0)
#define CMDQ_THR_ACTIVE_SLOT_CYCLES	0x3200
#define CMDQ_INST_CYCLE_TIMEOUT		0x0
#define CMDQ_THR_IRQ_DONE		0x1
#define CMDQ_THR_IRQ_ERROR		0x12
#define CMDQ_THR_IRQ_EN			(CMDQ_THR_IRQ_ERROR | CMDQ_THR_IRQ_DONE)
#define CMDQ_THR_IS_WAITING		BIT(31)
#define CMDQ_THR_PRIORITY		0x7


#define CMDQ_JUMP_BY_OFFSET		0x10000000
#define CMDQ_JUMP_BY_PA			0x10000001

#define CMDQ_MIN_AGE_VALUE              (5)	/* currently disable age */

#define CMDQ_DRIVER_NAME		"mtk_cmdq_mbox"

/* CMDQ log flag */
int mtk_cmdq_log;
EXPORT_SYMBOL(mtk_cmdq_log);

int mtk_cmdq_msg;
EXPORT_SYMBOL(mtk_cmdq_msg);

int mtk_cmdq_err = 1;
EXPORT_SYMBOL(mtk_cmdq_err);
module_param(mtk_cmdq_log, int, 0644);

struct cmdq_task {
	struct cmdq		*cmdq;
	struct list_head	list_entry;
	dma_addr_t		pa_base;
	struct cmdq_thread	*thread;
	struct cmdq_pkt		*pkt; /* the packet sent from mailbox client */
	u64			exec_time;
};

struct cmdq_buf_dump {
	struct cmdq		*cmdq;
	struct work_struct	dump_work;
	bool			timeout; /* 0: error, 1: timeout */
	void			*cmd_buf;
	size_t			cmd_buf_size;
	u32			pa_offset; /* pa_curr - pa_base */
};

struct cmdq {
	struct mbox_controller	mbox;
	void __iomem		*base;
	phys_addr_t		base_pa;
	u32			irq;
	struct workqueue_struct	*buf_dump_wq;
	struct cmdq_thread	thread[CMDQ_THR_MAX_COUNT];
	u32			prefetch;
	struct clk		*clock;
	struct clk		*clock_timer;
	bool			suspended;
	atomic_t		usage;
	struct workqueue_struct *timeout_wq;
	struct wakeup_source	wake_lock;
};

#if IS_ENABLED(CONFIG_MMPROFILE)
#include "../misc/mediatek/mmp/mmprofile.h"

struct cmdq_mmp_event {
	mmp_event	cmdq;
	mmp_event	gce_irq;
	mmp_event	thrd_enable;
	mmp_event	thrd_suspend;
	mmp_event	timeout;
};

struct cmdq_mmp_event	cmdq_mmp;
#endif

static inline void cmdq_mmp_init(void)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_enable(1);
	if (cmdq_mmp.cmdq) {
		mmprofile_start(1);
		return;
	}

	cmdq_mmp.cmdq = mmprofile_register_event(MMP_ROOT_EVENT, "CMDQ");
	cmdq_mmp.gce_irq = mmprofile_register_event(cmdq_mmp.cmdq, "gce_irq");
	cmdq_mmp.thrd_enable =
		mmprofile_register_event(cmdq_mmp.cmdq, "thrd_enable");
	cmdq_mmp.thrd_suspend =
		mmprofile_register_event(cmdq_mmp.cmdq, "thrd_suspend");
	cmdq_mmp.timeout = mmprofile_register_event(cmdq_mmp.cmdq, "timeout");
	mmprofile_start(1);
#endif
}

static void cmdq_lock_wake_lock(struct cmdq *cmdq, bool lock)
{
	static bool is_locked;

	if (lock) {
		if (!is_locked) {
			__pm_stay_awake(&cmdq->wake_lock);
			is_locked = true;
		} else  {
			/* should not reach here */
			cmdq_err("try lock twice");
		}
	} else {
		if (is_locked) {
			__pm_relax(&cmdq->wake_lock);
			is_locked = false;
		} else {
			/* should not reach here */
			cmdq_err("try unlock twice");
		}
	}
}

static s32 cmdq_clk_enable(struct cmdq *cmdq)
{
	s32 usage, err, err_timer;

	usage = atomic_inc_return(&cmdq->usage);

	err = clk_enable(cmdq->clock);
	if (usage <= 0 || err < 0)
		cmdq_err("ref count error after inc:%d err:%d", usage, err);
	else if (usage == 1) {
		cmdq_log("cmdq begin mbox");
		if (cmdq->prefetch)
			writel(cmdq->prefetch,
				cmdq->base + CMDQ_PREFETCH_GSIZE);
		/* make sure pm not suspend */
		cmdq_lock_wake_lock(cmdq, true);
	}

	err_timer = clk_enable(cmdq->clock_timer);
	if (err_timer < 0)
		cmdq_err("timer clk fail:%d", err_timer);

	return err;
}

static void cmdq_clk_disable(struct cmdq *cmdq)
{
	s32 usage;

	usage = atomic_dec_return(&cmdq->usage);

	if (usage < 0) {
		/* print error but still try close */
		cmdq_err("ref count error after dec:%d", usage);
	} else if (usage == 0) {
		cmdq_log("cmdq shutdown mbox");
		/* clear tpr mask */
		writel(0, cmdq->base + CMDQ_TPR_MASK);
	}

	clk_disable(cmdq->clock_timer);
	clk_disable(cmdq->clock);

	if (usage == 0) {
		/* now allow pm suspend */
		cmdq_lock_wake_lock(cmdq, false);
	}
}

static dma_addr_t cmdq_thread_get_pc(struct cmdq_thread *thread)
{
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);

	if (atomic_read(&cmdq->usage) <= 0) {
		cmdq_err("dump pc with cmdq off");
		dump_stack();
		return 0;
	}

	return CMDQ_REG_REVERT_ADDR(readl(thread->base + CMDQ_THR_CURR_ADDR));
}

static dma_addr_t cmdq_thread_get_end(struct cmdq_thread *thread)
{
	dma_addr_t end = readl(thread->base + CMDQ_THR_END_ADDR);

	return CMDQ_REG_REVERT_ADDR(end);
}

static void cmdq_thread_set_pc(struct cmdq_thread *thread, dma_addr_t pc)
{
	writel(CMDQ_REG_SHIFT_ADDR(pc), thread->base + CMDQ_THR_CURR_ADDR);
}

static void cmdq_thread_set_end(struct cmdq_thread *thread, dma_addr_t end)
{
	writel(CMDQ_REG_SHIFT_ADDR(end), thread->base + CMDQ_THR_END_ADDR);
}

static int cmdq_thread_suspend(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 status;

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq_mmp.thrd_suspend, MMPROFILE_FLAG_PULSE,
		thread->idx, CMDQ_THR_SUSPEND);
#endif
	writel(CMDQ_THR_SUSPEND, thread->base + CMDQ_THR_SUSPEND_TASK);

	/* If already disabled, treat as suspended successful. */
	if (!(readl(thread->base + CMDQ_THR_ENABLE_TASK) & CMDQ_THR_ENABLED))
		return 0;

	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_CURR_STATUS,
			status, status & CMDQ_THR_STATUS_SUSPENDED, 0, 100)) {
		cmdq_err("suspend GCE thread 0x%x failed",
			(u32)(thread->base - cmdq->base));
		return -EFAULT;
	}

	return 0;
}

static void cmdq_thread_resume(struct cmdq_thread *thread)
{
	writel(CMDQ_THR_RESUME, thread->base + CMDQ_THR_SUSPEND_TASK);
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq_mmp.thrd_suspend, MMPROFILE_FLAG_PULSE,
		thread->idx, CMDQ_THR_RESUME);
#endif
}

static int cmdq_thread_reset(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 warm_reset;

	writel(CMDQ_THR_DO_WARM_RESET, thread->base + CMDQ_THR_WARM_RESET);
	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_WARM_RESET,
			warm_reset, !(warm_reset & CMDQ_THR_DO_WARM_RESET),
			0, 10)) {
		cmdq_err("reset GCE thread %u failed", thread->idx);
		return -EFAULT;
	}
	writel(CMDQ_THR_ACTIVE_SLOT_CYCLES, cmdq->base + CMDQ_THR_SLOT_CYCLES);
	return 0;
}

static void cmdq_thread_err_reset(struct cmdq *cmdq, struct cmdq_thread *thread,
	dma_addr_t pc, u32 thd_pri)
{
	u32 i, spr[4], cookie;
	dma_addr_t end;

	for (i = 0; i < 4; i++)
		spr[i] = readl(thread->base + CMDQ_THR_SPR + i * 4);
	end = cmdq_thread_get_end(thread);
	cookie = readl(thread->base + CMDQ_THR_CNT);

	cmdq_msg(
		"reset backup pc:%pa end:%pa cookie:0x%08x spr:0x%x 0x%x 0x%x 0x%x",
		&pc, &end, cookie, spr[0], spr[1], spr[2], spr[3]);
	WARN_ON(cmdq_thread_reset(cmdq, thread) < 0);

	for (i = 0; i < 4; i++)
		writel(spr[i], thread->base + CMDQ_THR_SPR + i * 4);
	writel(CMDQ_INST_CYCLE_TIMEOUT, thread->base + CMDQ_THR_INST_CYCLES);
	cmdq_thread_set_end(thread, end);
	cmdq_thread_set_pc(thread, pc);
	writel(cookie, thread->base + CMDQ_THR_CNT);
	writel(thd_pri, thread->base + CMDQ_THR_CFG);
	writel(CMDQ_THR_IRQ_EN, thread->base + CMDQ_THR_IRQ_ENABLE);
	writel(CMDQ_THR_ENABLED, thread->base + CMDQ_THR_ENABLE_TASK);
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq_mmp.thrd_enable, MMPROFILE_FLAG_PULSE,
		thread->idx, CMDQ_THR_ENABLED);
#endif
}

static void cmdq_thread_disable(struct cmdq *cmdq, struct cmdq_thread *thread)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq_mmp.thrd_enable, MMPROFILE_FLAG_PULSE,
		thread->idx, CMDQ_THR_DISABLED);
#endif
	cmdq_thread_reset(cmdq, thread);
	writel(CMDQ_THR_DISABLED, thread->base + CMDQ_THR_ENABLE_TASK);
}

/* notify GCE to re-fetch commands by setting GCE thread PC */
static void cmdq_thread_invalidate_fetched_data(struct cmdq_thread *thread)
{
	cmdq_thread_set_pc(thread, cmdq_thread_get_pc(thread));
}

static void cmdq_task_connect_buffer(struct cmdq_task *task,
	struct cmdq_task *next_task)
{
	u64 *task_base;
	struct cmdq_pkt_buffer *buf;
	u64 inst;

	/* let previous task jump to this task */
	buf = list_last_entry(&task->pkt->buf, typeof(*buf), list_entry);
	task_base = (u64 *)(buf->va_base + CMDQ_CMD_BUFFER_SIZE -
		task->pkt->avail_buf_size - CMDQ_INST_SIZE);
	inst = *task_base;
	*task_base = (u64)CMDQ_JUMP_BY_PA << 32 |
		CMDQ_REG_SHIFT_ADDR(next_task->pa_base);
	cmdq_log("change last inst 0x%016llx to 0x%016llx connect 0x%p -> 0x%p",
		inst, *task_base, task->pkt, next_task->pkt);
}

static void *cmdq_task_current_va(unsigned long pa, struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;
	u32 end;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (list_is_last(&buf->list_entry, &pkt->buf))
			end = buf->pa_base + CMDQ_CMD_BUFFER_SIZE -
				pkt->avail_buf_size;
		else
			end = buf->pa_base + CMDQ_CMD_BUFFER_SIZE;
		if (pa >= buf->pa_base && pa < end) {
			cmdq_log("current pkt:0x%p pc:0x%lx pa:%pa 0x%016llx",
				pkt, pa, &buf->pa_base,
				*((u64 *)(buf->va_base + (pa - buf->pa_base))));
			return buf->va_base + (pa - buf->pa_base);
		}
	}

	return NULL;
}

static bool cmdq_task_is_current_run(unsigned long pa, struct cmdq_pkt *pkt)
{
	if (cmdq_task_current_va(pa, pkt))
		return true;
	return false;
}


static void cmdq_task_insert_into_thread(dma_addr_t curr_pa,
	struct cmdq_task *task, struct list_head **insert_pos)
{
	struct cmdq_thread *thread = task->thread;
	struct cmdq_task *prev_task = NULL, *next_task = NULL, *cursor_task;

	list_for_each_entry_reverse(cursor_task, &thread->task_busy_list,
		list_entry) {
		prev_task = cursor_task;
		if (next_task)
			next_task->pkt->priority += CMDQ_MIN_AGE_VALUE;
		/* stop if we found current running task */
		if (cmdq_task_is_current_run(curr_pa, prev_task->pkt))
			break;
		/* stop if new task priority lower than this one */
		if (prev_task->pkt->priority >= task->pkt->priority)
			break;
		next_task = prev_task;
	}

	*insert_pos = &prev_task->list_entry;
	cmdq_task_connect_buffer(prev_task, task);
	if (next_task && next_task != prev_task) {
		cmdq_msg("reorder pkt:0x%p(%u) next pkt:0x%p(%u) pc:%pa",
			task->pkt, task->pkt->priority,
			next_task->pkt, next_task->pkt->priority, &curr_pa);
		cmdq_task_connect_buffer(task, next_task);
	}

	cmdq_thread_invalidate_fetched_data(thread);
}

static void cmdq_task_callback(struct cmdq_pkt *pkt, s32 err)
{
	struct cmdq_cb_data cmdq_cb_data;

	if (pkt->cb.cb) {
		cmdq_cb_data.err = err;
		cmdq_cb_data.data = pkt->cb.data;
		pkt->cb.cb(cmdq_cb_data);
	}
}

static void cmdq_task_err_callback(struct cmdq_pkt *pkt, s32 err)
{
	struct cmdq_cb_data cmdq_cb_data;

	if (pkt->err_cb.cb) {
		cmdq_cb_data.err = err;
		cmdq_cb_data.data = pkt->err_cb.data;
		pkt->err_cb.cb(cmdq_cb_data);
	}
}

static dma_addr_t cmdq_task_get_end_pa(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;

	/* let previous task jump to this task */
	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	return buf->pa_base + CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
}

static void cmdq_task_exec(struct cmdq_pkt *pkt, struct cmdq_thread *thread)
{
	struct cmdq *cmdq;
	struct cmdq_task *task, *last_task;
	dma_addr_t curr_pa, end_pa, dma_handle;
	struct list_head *insert_pos;
	struct cmdq_pkt_buffer *buf;

	cmdq = dev_get_drvdata(thread->chan->mbox->dev);

	/* Client should not flush new tasks if suspended. */
	WARN_ON(cmdq->suspended);

	buf = list_first_entry_or_null(&pkt->buf, typeof(*buf),
			list_entry);
	if (!buf) {
		cmdq_err("no command to execute");
		return;
	}
	dma_handle = buf->pa_base;

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (!task)
		return;
	task->cmdq = cmdq;
	INIT_LIST_HEAD(&task->list_entry);
	task->pa_base = dma_handle;
	task->thread = thread;
	task->pkt = pkt;
	task->exec_time = sched_clock();

	if (list_empty(&thread->task_busy_list)) {
		WARN_ON(cmdq_clk_enable(cmdq) < 0);
		WARN_ON(cmdq_thread_reset(cmdq, thread) < 0);

		cmdq_log("task %pa size:%zu thread->base=0x%p thread:%u",
			&task->pa_base, pkt->cmd_buf_size, thread->base,
			thread->idx);

		writel(CMDQ_INST_CYCLE_TIMEOUT,
			thread->base + CMDQ_THR_INST_CYCLES);
		writel(thread->priority & CMDQ_THR_PRIORITY,
			thread->base + CMDQ_THR_CFG);
		cmdq_thread_set_end(thread, cmdq_task_get_end_pa(pkt));
		cmdq_thread_set_pc(thread, task->pa_base);
		writel(CMDQ_THR_IRQ_EN, thread->base + CMDQ_THR_IRQ_ENABLE);
		writel(CMDQ_THR_ENABLED, thread->base + CMDQ_THR_ENABLE_TASK);
#if IS_ENABLED(CONFIG_MMPROFILE)
		mmprofile_log_ex(cmdq_mmp.thrd_enable, MMPROFILE_FLAG_PULSE,
			thread->idx, CMDQ_THR_ENABLED);
#endif

		cmdq_log("set pc:0x%08x end:0x%08x pkt:0x%p",
			(u32)task->pa_base,
			(u32)cmdq_task_get_end_pa(pkt),
			pkt);

		if (thread->timeout_ms != CMDQ_NO_TIMEOUT)
			mod_timer(&thread->timeout, jiffies +
				msecs_to_jiffies(thread->timeout_ms));
		list_move_tail(&task->list_entry, &thread->task_busy_list);
	} else {
		/* no warn on here to prevent slow down cpu */
		cmdq_thread_suspend(cmdq, thread);
		curr_pa = cmdq_thread_get_pc(thread);
		end_pa = cmdq_thread_get_end(thread);

		cmdq_log("curr task %pa~%pa thread->base:0x%p thread:%u",
			&curr_pa, &end_pa, thread->base, thread->idx);

		/* check boundary */
		if (curr_pa == end_pa - CMDQ_INST_SIZE || curr_pa == end_pa) {
			/* set to this task directly */
			cmdq_thread_set_pc(thread, task->pa_base);
			last_task = list_last_entry(&thread->task_busy_list,
				typeof(*task), list_entry);
			insert_pos = &last_task->list_entry;
			cmdq_log("set pc:%pa pkt:0x%p",
				&task->pa_base, task->pkt);
		} else {
			cmdq_task_insert_into_thread(curr_pa, task,
				&insert_pos);
			smp_mb(); /* modify jump before enable thread */
		}
		list_add(&task->list_entry, insert_pos);
		last_task = list_last_entry(&thread->task_busy_list,
			typeof(*task), list_entry);
		cmdq_thread_set_end(thread,
			cmdq_task_get_end_pa(last_task->pkt));
		cmdq_log("set end:0x%08x pkt:0x%p",
			(u32)cmdq_task_get_end_pa(last_task->pkt),
			last_task->pkt);

		if (thread->dirty) {
			cmdq_err("new task during error on thread:%u",
				thread->idx);
		} else {
			/* safe to go */
			cmdq_thread_resume(thread);
		}
	}
}

static void cmdq_task_exec_done(struct cmdq_task *task, s32 err)
{
	cmdq_task_callback(task->pkt, err);
	cmdq_log("pkt:0x%p done err:%d", task->pkt, err);
	list_del(&task->list_entry);
}

static void cmdq_buf_dump_schedule(struct cmdq_task *task, bool timeout,
				   u32 pa_curr)
{
	struct cmdq_pkt_buffer *buf;
	u64 *inst = NULL;

	list_for_each_entry(buf, &task->pkt->buf, list_entry) {
		if (!(pa_curr >= buf->pa_base &&
			pa_curr < buf->pa_base + CMDQ_CMD_BUFFER_SIZE)) {
			continue;
		}
		inst = (u64 *)(buf->va_base + (pa_curr - buf->pa_base));
		break;
	}

	cmdq_err("task:0x%p timeout:%s pkt:0x%p size:%zu pc:0x%08x inst:0x%016llx",
		task, timeout ? "true" : "false", task->pkt,
		task->pkt->cmd_buf_size, pa_curr,
		inst ? *inst : -1);
}

static void cmdq_task_handle_error(struct cmdq_task *task, u32 pa_curr)
{
	struct cmdq_thread *thread = task->thread;
	struct cmdq_task *next_task;

	cmdq_err("task 0x%p pkt 0x%p error", task, task->pkt);
	cmdq_buf_dump_schedule(task, false, pa_curr);
	WARN_ON(cmdq_thread_suspend(task->cmdq, thread) < 0);
	next_task = list_first_entry_or_null(&thread->task_busy_list,
			struct cmdq_task, list_entry);
	if (next_task)
		cmdq_thread_set_pc(thread, next_task->pa_base);
	cmdq_thread_resume(thread);
}

static void cmdq_thread_irq_handler(struct cmdq *cmdq,
				    struct cmdq_thread *thread)
{
	struct cmdq_task *task, *tmp, *curr_task = NULL;
	u32 irq_flag;
	dma_addr_t curr_pa, task_end_pa;
	s32 err = 0;

	irq_flag = readl(thread->base + CMDQ_THR_IRQ_STATUS);
	writel(~irq_flag, thread->base + CMDQ_THR_IRQ_STATUS);

	cmdq_log("CMDQ_THR_IRQ_STATUS:%u thread chan:0x%p idx:%u",
		irq_flag, thread->chan, thread->idx);

	/*
	 * When ISR call this function, another CPU core could run
	 * "release task" right before we acquire the spin lock, and thus
	 * reset / disable this GCE thread, so we need to check the enable
	 * bit of this GCE thread.
	 */
	if (!(readl(thread->base + CMDQ_THR_ENABLE_TASK) & CMDQ_THR_ENABLED))
		return;

	if (irq_flag & CMDQ_THR_IRQ_ERROR)
		err = -EINVAL;
	else if (irq_flag & CMDQ_THR_IRQ_DONE)
		err = 0;
	else
		return;

	if (list_empty(&thread->task_busy_list))
		cmdq_err("empty! may we hang later?");

	curr_pa = cmdq_thread_get_pc(thread);
	task_end_pa = cmdq_thread_get_end(thread);

	if (err < 0)
		cmdq_err("pc:%pa end:%pa err:%d",
			&curr_pa, &task_end_pa, err);

	cmdq_log("task status %pa~%pa err:%d",
		&curr_pa, &task_end_pa, err);

	task = list_first_entry_or_null(&thread->task_busy_list,
		struct cmdq_task, list_entry);
	if (task && task->pkt->loop) {
		cmdq_log("task loop %p", &task->pkt);
		cmdq_task_callback(task->pkt, err);
		return;
	}

	if (thread->dirty) {
		cmdq_log("task in error dump thread:%u pkt:0x%p",
			thread->idx, task ? task->pkt : NULL);
		return;
	}

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
				 list_entry) {
		task_end_pa = cmdq_task_get_end_pa(task->pkt);
		if (cmdq_task_is_current_run(curr_pa, task->pkt))
			curr_task = task;

		if (!curr_task || curr_pa == task_end_pa - CMDQ_INST_SIZE) {
			if (curr_task && (curr_pa != task_end_pa)) {
				cmdq_log(
					"remove task that not ending pkt:0x%p %pa to %pa",
					curr_task->pkt, &curr_pa, &task_end_pa);
			}
			cmdq_task_exec_done(task, 0);
			kfree(task);
		} else if (err) {
			cmdq_err("pkt:0x%p thread:%u err:%d",
				curr_task->pkt, thread->idx, err);
			cmdq_task_exec_done(task, err);
			cmdq_task_handle_error(curr_task, curr_pa);
			kfree(task);
		}

		if (curr_task)
			break;
	}

	task = list_first_entry_or_null(&thread->task_busy_list,
		struct cmdq_task, list_entry);
	if (!task) {
		cmdq_thread_disable(cmdq, thread);
		cmdq_clk_disable(cmdq);

		cmdq_log("empty task thread:%u", thread->idx);
	} else {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms));
		cmdq_log("mod_timer pkt:0x%p timeout:%u thread:%u",
			task->pkt, thread->timeout_ms, thread->idx);
	}
}

static irqreturn_t cmdq_irq_handler(int irq, void *dev)
{
	struct cmdq *cmdq = dev;
	unsigned long irq_status, flags = 0L;
	int bit;
	bool secure_irq = false;

	if (atomic_read(&cmdq->usage) <= 0) {
#if 0
		clk_enable(cmdq->clock);
		irq_status = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS) &
			CMDQ_IRQ_MASK;
		cmdq_msg("cmdq not enable status:0x%x", irq_status);
		clk_disable(cmdq->clock);
		if (!(irq_status ^ CMDQ_IRQ_MASK))
			return IRQ_HANDLED;
#endif
		return IRQ_NONE;
	}

	irq_status = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS) & CMDQ_IRQ_MASK;
	cmdq_log("CMDQ_CURR_IRQ_STATUS: %x, %x",
		(u32)irq_status, (u32)(irq_status ^ CMDQ_IRQ_MASK));
	if (!(irq_status ^ CMDQ_IRQ_MASK)) {
		cmdq_msg("not handle for empty status:0x%x",
			(u32)irq_status);
		return IRQ_NONE;
	}

	for_each_clear_bit(bit, &irq_status, fls(CMDQ_IRQ_MASK)) {
		struct cmdq_thread *thread = &cmdq->thread[bit];

		cmdq_log("bit=%d, thread->base=%p", bit, thread->base);
		if (!thread->occupied) {
			secure_irq = true;
			continue;
		}

		spin_lock_irqsave(&thread->chan->lock, flags);
		cmdq_thread_irq_handler(cmdq, thread);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
	}

	return secure_irq ? IRQ_NONE : IRQ_HANDLED;
}

static bool cmdq_thread_timeout_excceed(struct cmdq_thread *thread)
{
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	struct cmdq_task *task;
	u64 duration;

	/* If first time exec time stamp smaller than timeout value,
	 * it is last round timeout. Skip it.
	 */
	task = list_first_entry(&thread->task_busy_list,
		typeof(*task), list_entry);
	duration = div_s64(sched_clock() - task->exec_time, 1000000);
	if (duration < thread->timeout_ms) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms - duration));
		cmdq_msg(
			"thread:%u usage:%d exec:%llu dur:%llu timeout not excceed\n",
			thread->idx, atomic_read(&cmdq->usage),
			task->exec_time, duration);
		return false;
	}

	return true;
}

static void cmdq_thread_handle_timeout_work(struct work_struct *work_item)
{
	struct cmdq_thread *thread = container_of(work_item,
		struct cmdq_thread, timeout_work);
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	struct cmdq_task *task, *tmp, *timeout_task = NULL;
	unsigned long flags;
	bool first_task = true;
	u32 pa_curr;

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list)) {
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	/* Check before suspend thread to prevent hurt performance. */
	if (!cmdq_thread_timeout_excceed(thread)) {
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);

	/*
	 * Although IRQ is disabled, GCE continues to execute.
	 * It may have pending IRQ before GCE thread is suspended,
	 * so check this condition again.
	 */
	cmdq_thread_irq_handler(cmdq, thread);

	if (list_empty(&thread->task_busy_list)) {
		cmdq_thread_resume(thread);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		cmdq_err("thread:%u empty after irq handle in timeout",
			thread->idx);
		return;
	}

	/* After IRQ, first task may change. */
	if (!cmdq_thread_timeout_excceed(thread)) {
		cmdq_thread_resume(thread);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	cmdq_err("timeout for thread:0x%p idx:%u usage:%d",
		thread->base, thread->idx, atomic_read(&cmdq->usage));

	pa_curr = cmdq_thread_get_pc(thread);

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
		list_entry) {
		bool curr_task = cmdq_task_is_current_run(pa_curr, task->pkt);

		if (first_task) {
			cmdq_buf_dump_schedule(task, true, pa_curr);
			first_task = false;
		}

		if (curr_task) {
			timeout_task = task;
			break;
		}

		cmdq_msg("ending not curr in timeout pkt:0x%p curr_pa:0x%08x",
			task->pkt, pa_curr);
		cmdq_task_exec_done(task, 0);
		kfree(task);
	}

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(cmdq_mmp.timeout, MMPROFILE_FLAG_PULSE,
		thread->idx, timeout_task ? (unsigned long)timeout_task : 0);
#endif

	if (timeout_task) {
		thread->dirty = true;
		spin_unlock_irqrestore(&thread->chan->lock, flags);

		cmdq_task_err_callback(timeout_task->pkt, -ETIMEDOUT);

		spin_lock_irqsave(&thread->chan->lock, flags);
		thread->dirty = false;

		task = list_first_entry_or_null(&thread->task_busy_list,
			struct cmdq_task, list_entry);
		if (timeout_task == task) {
			cmdq_task_exec_done(task, -ETIMEDOUT);
			kfree(task);
		} else {
			cmdq_err("task list changed");
		}
	}

	task = list_first_entry_or_null(&thread->task_busy_list,
		struct cmdq_task, list_entry);
	if (task) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms));
		cmdq_thread_err_reset(cmdq, thread,
			task->pa_base, thread->priority);
		cmdq_thread_resume(thread);
	} else {
		cmdq_thread_resume(thread);
		cmdq_thread_disable(cmdq, thread);
		cmdq_clk_disable(cmdq);
	}
	spin_unlock_irqrestore(&thread->chan->lock, flags);

}
static void cmdq_thread_handle_timeout(unsigned long data)
{
	struct cmdq_thread *thread = (struct cmdq_thread *)data;
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	unsigned long flags;
	bool empty;

	spin_lock_irqsave(&thread->chan->lock, flags);
	empty = list_empty(&thread->task_busy_list);
	spin_unlock_irqrestore(&thread->chan->lock, flags);
	if (empty)
		return;

	if (!work_pending(&thread->timeout_work)) {
		cmdq_log("queue cmdq timeout thread:%u", thread->idx);
		queue_work(cmdq->timeout_wq, &thread->timeout_work);
	} else {
		cmdq_msg("ignore cmdq timeout thread:%u", thread->idx);
	}
}

void cmdq_thread_dump_err(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;
	unsigned long flags;
	struct cmdq_task *task;
	struct cmdq_pkt_buffer *buf;

	struct cmdq_pkt *pkt = NULL;
	u32 warn_rst, en, suspend, status, irq, irq_en, curr_pa, end_pa, cnt,
		wait_token, cfg, prefetch, cycles, thresx, pri = 0;
	size_t size = 0;
	u64 *end_va, *curr_va = NULL, inst = 0, last_inst[2] = {0};
	void *va_base = NULL;
	dma_addr_t pa_base;

	/* lock channel and get info */
	spin_lock_irqsave(&chan->lock, flags);

	warn_rst = readl(thread->base + CMDQ_THR_WARM_RESET);
	en = readl(thread->base + CMDQ_THR_ENABLE_TASK);
	suspend = readl(thread->base + CMDQ_THR_SUSPEND_TASK);
	status = readl(thread->base + CMDQ_THR_CURR_STATUS);
	irq = readl(thread->base + CMDQ_THR_IRQ_STATUS);
	irq_en = readl(thread->base + CMDQ_THR_IRQ_ENABLE);
	curr_pa = cmdq_thread_get_pc(thread);
	end_pa = cmdq_thread_get_end(thread);
	cnt = readl(thread->base + CMDQ_THR_CNT);
	wait_token = readl(thread->base + CMDQ_THR_WAIT_TOKEN);
	cfg = readl(thread->base + CMDQ_THR_CFG);
	prefetch = readl(thread->base + CMDQ_THR_PREFETCH);
	cycles = readl(thread->base + CMDQ_THR_INST_CYCLES);
	thresx = readl(thread->base + CMDQ_THR_INST_THRESX);

	list_for_each_entry(task, &thread->task_busy_list, list_entry) {
		curr_va = (u64 *)cmdq_task_current_va(curr_pa, task->pkt);
		if (!curr_va)
			continue;
		inst = *curr_va;
		pkt = task->pkt;
		size = pkt->cmd_buf_size;
		pri = pkt->priority;

		buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
		va_base = buf->va_base;
		pa_base = buf->pa_base;

		buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
		end_va = (u64 *)(buf->va_base + CMDQ_CMD_BUFFER_SIZE -
			pkt->avail_buf_size - CMDQ_INST_SIZE * 2);
		last_inst[0] = *end_va;
		last_inst[1] = *++end_va;
		break;
	}
	spin_unlock_irqrestore(&chan->lock, flags);

	cmdq_dump("error thread:%u pc:0x%08x (0x%p) instruction:%016llx",
		thread->idx, curr_pa, curr_va, inst);
	cmdq_dump("end:0x%08x wait token:0x%08x irq:%x irq en:%x",
		end_pa, wait_token, irq, irq_en);
	cmdq_dump("warn rst:0x%x suspend:0x%x status:0x%x cnt:0x%x cfg:0x%x",
		warn_rst, suspend, status, cnt, cfg);
	cmdq_dump("prefetch:0x%x timeout cycle:%u thresx:0x%x",
		prefetch, cycles, thresx);
	if (pkt) {
		cmdq_dump("pkt:0x%p size:%zu base va:0x%p pa:%pa priority:%u",
			pkt, size, va_base, &pa_base, pri);
		cmdq_dump("last inst 0x%016llx 0x%016llx",
			last_inst[0], last_inst[1]);
	}
}
EXPORT_SYMBOL(cmdq_thread_dump_err);

void cmdq_mbox_thread_remove_task(struct mbox_chan *chan,
	struct cmdq_pkt *pkt)
{
	struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	struct cmdq_task *task, *tmp;
	unsigned long flags;
	u32 pa_curr;
	bool curr_task = false;
	bool last_task = false;

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list)) {
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	cmdq_msg("remove task from thread idx:%u usage:%d pkt:0x%p",
		thread->idx, atomic_read(&cmdq->usage), pkt);

	WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);

	/*
	 * Although IRQ is disabled, GCE continues to execute.
	 * It may have pending IRQ before GCE thread is suspended,
	 * so check this condition again.
	 */
	cmdq_thread_irq_handler(cmdq, thread);

	if (list_empty(&thread->task_busy_list)) {
		cmdq_err("thread:%u empty after irq handle in timeout",
			thread->idx);
		cmdq_thread_resume(thread);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	pa_curr = cmdq_thread_get_pc(thread);

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
		list_entry) {
		if (task->pkt != pkt)
			continue;

		curr_task = cmdq_task_is_current_run(pa_curr, task->pkt);
		if (list_is_last(&task->list_entry, &thread->task_busy_list))
			last_task = true;

		if (task == list_first_entry(&thread->task_busy_list,
			typeof(*task), list_entry) &&
			thread->dirty) {
			/* task during error handling, skip */
			spin_unlock_irqrestore(&thread->chan->lock, flags);
			return;
		}

		cmdq_task_exec_done(task, curr_task ? -ECONNABORTED : 0);
		kfree(task);
		break;
	}

	if (list_empty(&thread->task_busy_list)) {
		cmdq_thread_resume(thread);
		cmdq_thread_disable(cmdq, thread);
		cmdq_clk_disable(cmdq);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	if (curr_task) {
		task = list_first_entry(&thread->task_busy_list,
			typeof(*task), list_entry);

		cmdq_thread_set_pc(thread, task->pa_base);
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms));
	}

	if (last_task) {
		/* reset end addr again if remove last task */
		task = list_last_entry(&thread->task_busy_list,
			typeof(*task), list_entry);
		cmdq_thread_set_end(thread, cmdq_task_get_end_pa(task->pkt));
	}

	cmdq_thread_err_reset(cmdq, thread, cmdq_thread_get_pc(thread),
		thread->priority);

	spin_unlock_irqrestore(&thread->chan->lock, flags);
}
EXPORT_SYMBOL(cmdq_mbox_thread_remove_task);

static void cmdq_mbox_thread_stop(struct cmdq_thread *thread)
{
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	struct cmdq_task *task, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list)) {
		cmdq_err("stop empty thread:%u", thread->idx);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);

	/*
	 * Although IRQ is disabled, GCE continues to execute.
	 * It may have pending IRQ before GCE thread is suspended,
	 * so check this condition again.
	 */
	cmdq_thread_irq_handler(cmdq, thread);
	if (list_empty(&thread->task_busy_list)) {
		cmdq_err("thread:%u empty after irq handle in disable thread",
			thread->idx);
		cmdq_thread_resume(thread);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
		list_entry) {
		cmdq_task_exec_done(task, -ECONNABORTED);
		kfree(task);
	}

	cmdq_thread_disable(cmdq, thread);
	cmdq_clk_disable(cmdq);
	spin_unlock_irqrestore(&thread->chan->lock, flags);
}

void cmdq_mbox_channel_stop(struct mbox_chan *chan)
{
	cmdq_mbox_thread_stop(chan->con_priv);
}
EXPORT_SYMBOL(cmdq_mbox_channel_stop);

static int cmdq_suspend(struct device *dev)
{
	struct cmdq *cmdq = dev_get_drvdata(dev);
	struct cmdq_thread *thread;
	int i;
	bool task_running = false;

	cmdq->suspended = true;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		thread = &cmdq->thread[i];
		if (!list_empty(&thread->task_busy_list)) {
			cmdq_mbox_thread_stop(thread);
			task_running = true;
			cmdq_err("thread %d running", i);
		}
	}

	if (task_running) {
		dev_warn(dev, "exist running task(s) in suspend\n");
		schedule();
	}

	clk_unprepare(cmdq->clock_timer);
	clk_unprepare(cmdq->clock);
	return 0;
}

static int cmdq_resume(struct device *dev)
{
	struct cmdq *cmdq = dev_get_drvdata(dev);

	WARN_ON(clk_prepare(cmdq->clock) < 0);
	WARN_ON(clk_prepare(cmdq->clock_timer) < 0);
	cmdq->suspended = false;
	return 0;
}

static int cmdq_remove(struct platform_device *pdev)
{
	struct cmdq *cmdq = platform_get_drvdata(pdev);

	destroy_workqueue(cmdq->buf_dump_wq);
	mbox_controller_unregister(&cmdq->mbox);
	clk_unprepare(cmdq->clock_timer);
	clk_unprepare(cmdq->clock);
	return 0;
}

static int cmdq_mbox_send_data(struct mbox_chan *chan, void *data)
{
	cmdq_task_exec(data, chan->con_priv);
	return 0;
}

static int cmdq_mbox_startup(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	thread->occupied = true;
	return 0;
}

static void cmdq_mbox_shutdown(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	cmdq_mbox_thread_stop(chan->con_priv);
	thread->occupied = false;
}

static bool cmdq_mbox_last_tx_done(struct mbox_chan *chan)
{
	return true;
}

static const struct mbox_chan_ops cmdq_mbox_chan_ops = {
	.send_data = cmdq_mbox_send_data,
	.startup = cmdq_mbox_startup,
	.shutdown = cmdq_mbox_shutdown,
	.last_tx_done = cmdq_mbox_last_tx_done,
};

static struct mbox_chan *cmdq_xlate(struct mbox_controller *mbox,
		const struct of_phandle_args *sp)
{
	int ind = sp->args[0];
	struct cmdq_thread *thread;

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	thread = mbox->chans[ind].con_priv;
	thread->timeout_ms = sp->args[1] != 0 ?
		sp->args[1] : CMDQ_TIMEOUT_DEFAULT;
	thread->priority = sp->args[2];
	thread->chan = &mbox->chans[ind];

	return &mbox->chans[ind];
}

static s32 cmdq_config_prefetch(struct device_node *np, struct cmdq *cmdq)
{
	u32 i, prefetch_cnt = 0, prefetchs[4] = {0};
	s32 err;

	cmdq->prefetch = 0;
	of_property_read_u32(np, "max_prefetch_cnt", &prefetch_cnt);
	if (!prefetch_cnt)
		return 0;

	if (prefetch_cnt > ARRAY_SIZE(prefetchs)) {
		cmdq_err("prefetch count more than expect:%u",
			prefetch_cnt);
		prefetch_cnt = ARRAY_SIZE(prefetchs);
	}

	err = of_property_read_u32_array(np, "prefetch_size",
		prefetchs, prefetch_cnt);
	if (err != 0) {
		/* print log but do notify error hw setting */
		cmdq_err("read prefetch count:%u size error:%d",
			prefetch_cnt, err);
		return -EINVAL;
	}

	if (!prefetch_cnt)
		return 0;

	for (i = 0; i < prefetch_cnt; i++)
		cmdq->prefetch |= (prefetchs[i] / 32 - 1) << (i * 4);

	cmdq_msg("prefetch size configure:0x%x", cmdq->prefetch);
	return 0;
}

static void cmdq_config_dma_mask(struct device *dev)
{
	u32 dma_mask_bit = 0;
	s32 ret;

	ret = of_property_read_u32(dev->of_node, "dma_mask_bit",
		&dma_mask_bit);
	/* if not assign from dts, give default 32bit for legacy chip */
	if (ret != 0 || !dma_mask_bit)
		dma_mask_bit = 32;
	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(dma_mask_bit));
	cmdq_msg("mbox set dma mask bit:%u result:%d\n",
		dma_mask_bit, ret);
}

static int cmdq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct cmdq *cmdq;
	int err, i;

	cmdq = devm_kzalloc(dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cmdq->base = devm_ioremap_resource(dev, res);
	cmdq->base_pa = res->start;
	if (IS_ERR(cmdq->base)) {
		cmdq_err("failed to ioremap gce");
		return PTR_ERR(cmdq->base);
	}

	cmdq->irq = platform_get_irq(pdev, 0);
	if (!cmdq->irq) {
		cmdq_err("failed to get irq");
		return -EINVAL;
	}
	err = devm_request_irq(dev, cmdq->irq, cmdq_irq_handler, IRQF_SHARED,
			       "mtk_cmdq", cmdq);
	if (err < 0) {
		cmdq_err("failed to register ISR (%d)", err);
		return err;
	}

	dev_dbg(dev, "cmdq device: addr:0x%p, va:0x%p, irq:%d\n",
		dev, cmdq->base, cmdq->irq);

	cmdq_msg("cmdq device: addr:0x%p va:0x%p irq:%d mask:%#x",
		dev, cmdq->base, cmdq->irq, (u32)CMDQ_IRQ_MASK);

	cmdq_config_prefetch(dev->of_node, cmdq);
	cmdq_config_dma_mask(dev);

	cmdq->clock = devm_clk_get(dev, "gce");
	if (IS_ERR(cmdq->clock)) {
		cmdq_err("failed to get gce clk");
		cmdq->clock = NULL;
#if 0
		return PTR_ERR(cmdq->clock);
#endif
	}

	cmdq->clock_timer = devm_clk_get(dev, "gce-timer");
	if (IS_ERR(cmdq->clock_timer)) {
		cmdq_err("failed to get gce timer clk");
		cmdq->clock_timer = NULL;
	}

	cmdq->mbox.dev = dev;
	cmdq->mbox.chans = devm_kcalloc(dev, CMDQ_THR_MAX_COUNT,
					sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans)
		return -ENOMEM;

	cmdq->mbox.num_chans = CMDQ_THR_MAX_COUNT;
	cmdq->mbox.ops = &cmdq_mbox_chan_ops;
	cmdq->mbox.of_xlate = cmdq_xlate;

	/* make use of TXDONE_BY_ACK */
	cmdq->mbox.txdone_irq = false;
	cmdq->mbox.txdone_poll = false;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		cmdq->thread[i].base = cmdq->base + CMDQ_THR_BASE +
				CMDQ_THR_SIZE * i;
		cmdq->thread[i].gce_pa = cmdq->base_pa;
		INIT_LIST_HEAD(&cmdq->thread[i].task_busy_list);
		init_timer(&cmdq->thread[i].timeout);
		cmdq->thread[i].timeout.function = cmdq_thread_handle_timeout;
		cmdq->thread[i].timeout.data = (unsigned long)&cmdq->thread[i];
		cmdq->thread[i].idx = i;
		cmdq->mbox.chans[i].con_priv = &cmdq->thread[i];
		INIT_WORK(&cmdq->thread[i].timeout_work,
			cmdq_thread_handle_timeout_work);
	}

	err = mbox_controller_register(&cmdq->mbox);
	if (err < 0) {
		cmdq_err("failed to register mailbox:%d", err);
		return err;
	}

	cmdq->buf_dump_wq = alloc_ordered_workqueue(
			"%s", WQ_MEM_RECLAIM | WQ_HIGHPRI,
			"cmdq_buf_dump");

	cmdq->timeout_wq = create_singlethread_workqueue(
		"cmdq_timeout_handler");

	platform_set_drvdata(pdev, cmdq);
	WARN_ON(clk_prepare(cmdq->clock) < 0);
	WARN_ON(clk_prepare(cmdq->clock_timer) < 0);

	wakeup_source_init(&cmdq->wake_lock, "cmdq_wakelock");

	cmdq_mmp_init();
	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
};

static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mailbox-gce"},
	{}
};

static struct platform_driver cmdq_drv = {
	.probe = cmdq_probe,
	.remove = cmdq_remove,
	.driver = {
		.name = CMDQ_DRIVER_NAME,
		.pm = &cmdq_pm_ops,
		.of_match_table = cmdq_of_ids,
	}
};

static __init int cmdq_init(void)
{
	u32 err = 0;

	cmdq_msg("%s enter", __func__);

	err = platform_driver_register(&cmdq_drv);
	if (err) {
		cmdq_err("platform driver register failed:%d", err);
		return err;
	}

	return 0;
}

struct device *cmdq_mbox_get_dev(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return cmdq->mbox.dev;
}

s32 cmdq_mbox_thread_reset(void *chan)
{
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq,
		mbox);

	return cmdq_thread_reset(cmdq, thread);
}
EXPORT_SYMBOL(cmdq_mbox_thread_reset);

s32 cmdq_mbox_thread_suspend(void *chan)
{
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq,
		mbox);

	return cmdq_thread_suspend(cmdq, thread);
}
EXPORT_SYMBOL(cmdq_mbox_thread_suspend);

void cmdq_mbox_thread_disable(void *chan)
{
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq,
		mbox);

	cmdq_thread_disable(cmdq, thread);
}
EXPORT_SYMBOL(cmdq_mbox_thread_disable);

u32 cmdq_mbox_set_thread_timeout(void *chan, u32 timeout)
{
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;
	unsigned long flags;
	u32 timeout_prv;

	spin_lock_irqsave(&thread->chan->lock, flags);
	timeout_prv = thread->timeout_ms;
	thread->timeout_ms = timeout;
	spin_unlock_irqrestore(&thread->chan->lock, flags);

	return timeout_prv;
}
EXPORT_SYMBOL(cmdq_mbox_set_thread_timeout);

s32 cmdq_mbox_chan_id(void *chan)
{
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;

	if (!thread || !thread->occupied)
		return -1;

	return thread->idx;
}
EXPORT_SYMBOL(cmdq_mbox_chan_id);

s32 cmdq_task_get_thread_pc(struct mbox_chan *chan, dma_addr_t *pc_out)
{
	struct cmdq_thread *thread;
	dma_addr_t pc = 0;

	if (!pc_out || !chan)
		return -EINVAL;

	thread = chan->con_priv;
	pc = cmdq_thread_get_pc(thread);

	*pc_out = pc;

	return 0;
}

s32 cmdq_task_get_thread_irq(struct mbox_chan *chan, u32 *irq_out)
{
	struct cmdq_thread *thread;

	if (!irq_out || !chan)
		return -EINVAL;

	thread = chan->con_priv;
	*irq_out = readl(thread->base + CMDQ_THR_IRQ_STATUS);

	return 0;
}

s32 cmdq_task_get_thread_irq_en(struct mbox_chan *chan, u32 *irq_en_out)
{
	struct cmdq_thread *thread;

	if (!irq_en_out || !chan)
		return -EINVAL;

	thread = chan->con_priv;
	*irq_en_out = readl(thread->base + CMDQ_THR_IRQ_ENABLE);

	return 0;
}

s32 cmdq_task_get_thread_end_addr(struct mbox_chan *chan,
	dma_addr_t *end_addr_out)
{
	struct cmdq_thread *thread;

	if (!end_addr_out || !chan)
		return -EINVAL;

	thread = chan->con_priv;
	*end_addr_out = cmdq_thread_get_end(thread);

	return 0;
}

s32 cmdq_task_get_task_info_from_thread_unlock(struct mbox_chan *chan,
	struct list_head *task_list_out, u32 *task_num_out)
{
	struct cmdq_thread *thread;
	struct cmdq_task *task;
	u32 task_num = 0;

	if (!chan || !task_list_out)
		return -EINVAL;

	thread = chan->con_priv;
	list_for_each_entry(task, &thread->task_busy_list, list_entry) {
		struct cmdq_thread_task_info *task_info;

		task_info = kzalloc(sizeof(*task_info), GFP_ATOMIC);
		if (!task_info)
			continue;

		task_info->pa_base = task->pa_base;

		/* copy pkt here to avoid released */
		task_info->pkt = kzalloc(sizeof(*task_info->pkt), GFP_ATOMIC);
		if (!task_info->pkt) {
			kfree(task_info);
			continue;
		}
		memcpy(task_info->pkt, task->pkt, sizeof(*task->pkt));

		INIT_LIST_HEAD(&task_info->list_entry);
		list_add_tail(&task_info->list_entry, task_list_out);
		task_num++;
	}

	if (task_num_out)
		*task_num_out = task_num;

	return 0;
}

s32 cmdq_task_get_pkt_from_thread(struct mbox_chan *chan,
	struct cmdq_pkt **pkt_list_out, u32 pkt_list_size, u32 *pkt_count_out)
{
	struct cmdq_thread *thread;
	struct cmdq_task *task;
	u32 pkt_num = 0;
	u32 tmp_num = 0;
	unsigned long flags;

	if (!chan || !pkt_list_out || !pkt_count_out) {
		if (chan) {
			thread = chan->con_priv;
			list_for_each_entry(task, &thread->task_busy_list,
				list_entry) {
				tmp_num++;
			}

		}

		if (pkt_count_out)
			*pkt_count_out = pkt_num;

		return -EINVAL;
	}

	thread = chan->con_priv;

	spin_lock_irqsave(&thread->chan->lock, flags);

	if (list_empty(&thread->task_busy_list)) {
		*pkt_count_out = pkt_num;
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return 0;
	}

	list_for_each_entry(task, &thread->task_busy_list, list_entry) {
		if (pkt_list_size == pkt_num)
			break;
		pkt_list_out[pkt_num] = task->pkt;
		pkt_num++;
	}

	spin_unlock_irqrestore(&thread->chan->lock, flags);

	*pkt_count_out = pkt_num;

	return 0;
}

void cmdq_set_event(void *chan, u16 event_id)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	writel((1L << 16) | event_id, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
}
EXPORT_SYMBOL(cmdq_set_event);

void cmdq_clear_event(void *chan, u16 event_id)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	writel(event_id, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
}
EXPORT_SYMBOL(cmdq_clear_event);

u32 cmdq_get_event(void *chan, u16 event_id)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	writel(0x3FF & event_id, cmdq->base + CMDQ_SYNC_TOKEN_ID);
	return readl(cmdq->base + CMDQ_SYNC_TOKEN_VAL);
}
EXPORT_SYMBOL(cmdq_get_event);

void cmdq_event_verify(void *chan, u16 event_id)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);
	/* should be CMDQ_SYNC_TOKEN_USER_0 */
	const u16 test_token = 649;
	u32 i;

	cmdq_msg("chan:%lx cmdq:%lx event:%u",
		(unsigned long)chan, (unsigned long)cmdq, event_id);

	if (event_id > 512)
		event_id = 512;

	/* check if this event can be set and clear */
	writel((1L << 16) | event_id, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
	writel(event_id, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
	if (!readl(cmdq->base + CMDQ_SYNC_TOKEN_VAL))
		cmdq_msg("event cannot be set:%u", event_id);

	writel(event_id, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
	if (readl(cmdq->base + CMDQ_SYNC_TOKEN_VAL))
		cmdq_msg("event cannot be clear:%u", event_id);

	/* check if sw token can be set and clear */
	writel((1L << 16) | test_token, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
	writel(test_token, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
	if (!readl(cmdq->base + CMDQ_SYNC_TOKEN_VAL))
		cmdq_msg("event cannot be set:%u", test_token);

	writel(test_token, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
	if (readl(cmdq->base + CMDQ_SYNC_TOKEN_VAL))
		cmdq_msg("event cannot be clear:%u", test_token);

	/* clear all event first */
	for (i = 0; i < event_id + 20; i++)
		writel(i, cmdq->base + CMDQ_SYNC_TOKEN_UPD);

	/* now see if any event unable to clear */
	for (i = 0; i < event_id + 20; i++) {
		writel(i, cmdq->base + CMDQ_SYNC_TOKEN_UPD);
		if (readl(cmdq->base + CMDQ_SYNC_TOKEN_VAL))
			cmdq_msg("event still on:%u", i);
	}

	cmdq_msg("end debug event for %u", event_id);
}
EXPORT_SYMBOL(cmdq_event_verify);

arch_initcall(cmdq_init);
