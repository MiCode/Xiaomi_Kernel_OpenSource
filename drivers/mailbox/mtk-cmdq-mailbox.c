// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
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
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#include "cmdq-util.h"
#endif
#if IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
#include <linux/irqchip/mtk-gic-extend.h>
#endif

/* ddp main/sub, mdp path 0/1/2/3, general(misc) */
#define CMDQ_OP_CODE_MASK		(0xff << CMDQ_OP_CODE_SHIFT)

#define CMDQ_CURR_IRQ_STATUS		0x10
#define CMDQ_CURR_LOADED_THR		0x18
#define CMDQ_THR_SLOT_CYCLES		0x30
#define CMDQ_THR_EXEC_CYCLES		0x34
#define CMDQ_THR_TIMEOUT_TIMER		0x38
#define CMDQ_SYNC_TOKEN_ID		0x60
#define CMDQ_SYNC_TOKEN_VAL		0x64
#define CMDQ_SYNC_TOKEN_UPD		0x68
#define CMDQ_PREFETCH_GSIZE		0xC0
#define CMDQ_TPR_MASK			0xD0
#define CMDQ_TPR_TIMEOUT_EN		0xDC

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

#define GCE_GCTL_VALUE			0x48
#define GCE_GPR_R0_START		0x80
#define GCE_DEBUG_START_ADDR		0x1104
#define GCE_DEBUG_END_ADDR		0x1108

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
#define CMDQ_TPR_EN			BIT(31)


#define CMDQ_JUMP_BY_OFFSET		0x10000000
#define CMDQ_JUMP_BY_PA			0x10000001

#define CMDQ_MIN_AGE_VALUE              (5)	/* currently disable age */

#define CMDQ_DRIVER_NAME		"mtk_cmdq_mbox"

/* pc and end shift bit for gce, should be config in probe */
int gce_shift_bit;
EXPORT_SYMBOL(gce_shift_bit);

/* CMDQ log flag */
int mtk_cmdq_log;
EXPORT_SYMBOL(mtk_cmdq_log);

int mtk_cmdq_msg;
EXPORT_SYMBOL(mtk_cmdq_msg);

int mtk_cmdq_err = 1;
EXPORT_SYMBOL(mtk_cmdq_err);
module_param(mtk_cmdq_log, int, 0644);

int cmdq_trace;
EXPORT_SYMBOL(cmdq_trace);
module_param(cmdq_trace, int, 0644);

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
	u8			hwid;
	u32			irq;
	u32			thread_nr;
	u32			irq_mask;
	struct workqueue_struct	*buf_dump_wq;
	struct cmdq_thread	thread[CMDQ_THR_MAX_COUNT];
	u32			prefetch;
	struct clk		*clock;
	struct clk		*clock_timer;
	bool			suspended;
	atomic_t		usage;
	struct workqueue_struct *timeout_wq;
	struct wakeup_source	wake_lock;
	bool			wake_locked;
	spinlock_t		lock;
	u32			token_cnt;
	u16			*tokens;
};

struct gce_plat {
	u32 thread_nr;
	u8 shift;
};

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
#include "../misc/mediatek/mmp/mmprofile.h"

struct cmdq_mmp_event {
	mmp_event cmdq;
	mmp_event cmdq_irq;
	mmp_event loop_irq;
	mmp_event thread_en;
	mmp_event thread_suspend;
	mmp_event submit;
	mmp_event wait;
	mmp_event warning;
};

struct cmdq_mmp_event	cmdq_mmp;

#define MMP_THD(t, c)	((t)->idx | ((c)->hwid << 5))
#endif

static void cmdq_init(struct cmdq *cmdq)
{
	int i;

	cmdq_trace_ex_begin("%s", __func__);

	writel(CMDQ_THR_ACTIVE_SLOT_CYCLES, cmdq->base + CMDQ_THR_SLOT_CYCLES);
	for (i = 0; i <= CMDQ_EVENT_MAX; i++)
		writel(i, cmdq->base + CMDQ_SYNC_TOKEN_UPD);

	/* some of events need default 1 */
	for (i = 0; i < cmdq->token_cnt; i++)
		writel(cmdq->tokens[i] | BIT(16),
			cmdq->base + CMDQ_SYNC_TOKEN_UPD);

	cmdq_trace_ex_end();
}

static inline void cmdq_mmp_init(void)
{
#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_enable(1);
	if (cmdq_mmp.cmdq) {
		mmprofile_start(1);
		return;
	}

	cmdq_mmp.cmdq = mmprofile_register_event(MMP_ROOT_EVENT, "CMDQ");
	cmdq_mmp.cmdq_irq = mmprofile_register_event(cmdq_mmp.cmdq, "cmdq_irq");
	cmdq_mmp.loop_irq = mmprofile_register_event(cmdq_mmp.cmdq, "loop_irq");
	cmdq_mmp.thread_en =
		mmprofile_register_event(cmdq_mmp.cmdq, "thread_en");
	cmdq_mmp.thread_suspend =
		mmprofile_register_event(cmdq_mmp.cmdq, "thread_suspend");
	cmdq_mmp.submit = mmprofile_register_event(cmdq_mmp.cmdq, "submit");
	cmdq_mmp.wait = mmprofile_register_event(cmdq_mmp.cmdq, "wait");
	cmdq_mmp.warning = mmprofile_register_event(cmdq_mmp.cmdq, "warning");
	mmprofile_start(1);
#endif
}

static void cmdq_lock_wake_lock(struct cmdq *cmdq, bool lock)
{
	cmdq_trace_ex_begin("%s", __func__);

	if (lock) {
		if (!cmdq->wake_locked) {
			__pm_stay_awake(&cmdq->wake_lock);
			cmdq->wake_locked = true;
		} else  {
			/* should not reach here */
			cmdq_err("try lock twice cmdq:%lx",
				(unsigned long)cmdq);
			dump_stack();
		}
	} else {
		if (cmdq->wake_locked) {
			__pm_relax(&cmdq->wake_lock);
			cmdq->wake_locked = false;
		} else {
			/* should not reach here */
			cmdq_err("try unlock twice cmdq:%lx",
				(unsigned long)cmdq);
			dump_stack();
		}

	}

	cmdq_trace_ex_end();
}

static s32 cmdq_clk_enable(struct cmdq *cmdq)
{
	s32 usage, err, err_timer;
	unsigned long flags;

	cmdq_trace_ex_begin("%s", __func__);

	spin_lock_irqsave(&cmdq->lock, flags);

	usage = atomic_inc_return(&cmdq->usage);

	err = clk_enable(cmdq->clock);
	if (usage <= 0 || err < 0)
		cmdq_err("ref count error after inc:%d err:%d suspend:%s",
			usage, err, cmdq->suspended ? "true" : "false");
	else if (usage == 1) {
		cmdq_log("cmdq begin mbox");
		if (cmdq->prefetch)
			writel(cmdq->prefetch,
				cmdq->base + CMDQ_PREFETCH_GSIZE);
		writel(CMDQ_TPR_EN, cmdq->base + CMDQ_TPR_MASK);
#if IS_ENABLED(CONFIG_MACH_MT6873) || IS_ENABLED(CONFIG_MACH_MT6853)
		writel((0x7 << 16) + 0x7, cmdq->base + GCE_GCTL_VALUE);
		writel(0, cmdq->base + GCE_DEBUG_START_ADDR);
#endif
		/* make sure pm not suspend */
		cmdq_lock_wake_lock(cmdq, true);
		cmdq_init(cmdq);
	}

	err_timer = clk_enable(cmdq->clock_timer);
	if (err_timer < 0)
		cmdq_err("timer clk fail:%d", err_timer);

	spin_unlock_irqrestore(&cmdq->lock, flags);

	cmdq_trace_ex_end();

	return err;
}

static void cmdq_clk_disable(struct cmdq *cmdq)
{
	s32 usage;
	unsigned long flags;

	cmdq_trace_ex_begin("%s", __func__);

	spin_lock_irqsave(&cmdq->lock, flags);

	usage = atomic_dec_return(&cmdq->usage);

	if (usage < 0) {
		/* print error but still try close */
		cmdq_err("ref count error after dec:%d suspend:%s",
			usage, cmdq->suspended ? "true" : "false");
	} else if (usage == 0) {
		cmdq_log("cmdq shutdown mbox");
		/* clear tpr mask */
		writel(0, cmdq->base + CMDQ_TPR_MASK);
#if IS_ENABLED(CONFIG_MACH_MT6873) || IS_ENABLED(CONFIG_MACH_MT6853)
		writel(0x7, cmdq->base + GCE_GCTL_VALUE);
#endif
		/* now allow pm suspend */
		cmdq_lock_wake_lock(cmdq, false);
	}

	clk_disable(cmdq->clock_timer);
	clk_disable(cmdq->clock);

	spin_unlock_irqrestore(&cmdq->lock, flags);

	cmdq_trace_ex_end();
}

dma_addr_t cmdq_thread_get_pc(struct cmdq_thread *thread)
{
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);

	if (atomic_read(&cmdq->usage) <= 0)
		return 0;

	return CMDQ_REG_REVERT_ADDR(readl(thread->base + CMDQ_THR_CURR_ADDR));
}

dma_addr_t cmdq_thread_get_end(struct cmdq_thread *thread)
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

void cmdq_thread_set_spr(struct mbox_chan *chan, u8 id, u32 val)
{
	struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;

	writel(val, thread->base + CMDQ_THR_SPR + id * 4);
}

static int cmdq_thread_suspend(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 status;

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_log_ex(cmdq_mmp.thread_suspend, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq), CMDQ_THR_SUSPEND);
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
#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	struct cmdq *cmdq = container_of(
		thread->chan->mbox, typeof(*cmdq), mbox);
#endif

	writel(CMDQ_THR_RESUME, thread->base + CMDQ_THR_SUSPEND_TASK);
#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_log_ex(cmdq_mmp.thread_suspend, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq), CMDQ_THR_RESUME);
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
#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_log_ex(cmdq_mmp.thread_en, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq), CMDQ_THR_ENABLED);
#endif
}

static void cmdq_thread_disable(struct cmdq *cmdq, struct cmdq_thread *thread)
{
#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_log_ex(cmdq_mmp.thread_en, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq), CMDQ_THR_DISABLED);
#endif
	cmdq_thread_reset(cmdq, thread);
	writel(CMDQ_THR_DISABLED, thread->base + CMDQ_THR_ENABLE_TASK);
#if IS_ENABLED(CONFIG_MACH_MT6873) || IS_ENABLED(CONFIG_MACH_MT6853)
	if (cmdq_thread_ddr_user_check(thread->idx)) {
		unsigned long flags;

		spin_lock_irqsave(&cmdq->lock, flags);
		writel(readl(cmdq->base + GCE_DEBUG_START_ADDR) - 1,
			cmdq->base + GCE_DEBUG_START_ADDR);
		spin_unlock_irqrestore(&cmdq->lock, flags);
	}
#endif
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
		if (pa >= buf->pa_base && pa < end)
			return buf->va_base + (pa - buf->pa_base);
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

static void *cmdq_task_get_end_va(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;

	/* let previous task jump to this task */
	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	return buf->va_base + CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
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

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_log_ex(cmdq_mmp.submit, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq), (unsigned long)pkt);
#endif

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

#if IS_ENABLED(CONFIG_MACH_MT6873) || IS_ENABLED(CONFIG_MACH_MT6853)
		if (cmdq_thread_ddr_user_check(thread->idx)) {
			unsigned long flags;

			spin_lock_irqsave(&cmdq->lock, flags);
			writel(readl(cmdq->base + GCE_GCTL_VALUE) | (1 << 16),
				cmdq->base + GCE_GCTL_VALUE);
			writel(readl(cmdq->base + GCE_DEBUG_START_ADDR) + 1,
				cmdq->base + GCE_DEBUG_START_ADDR);
			spin_unlock_irqrestore(&cmdq->lock, flags);
		}
#endif
		writel(CMDQ_INST_CYCLE_TIMEOUT,
			thread->base + CMDQ_THR_INST_CYCLES);
		writel(thread->priority & CMDQ_THR_PRIORITY,
			thread->base + CMDQ_THR_CFG);
		cmdq_thread_set_end(thread, cmdq_task_get_end_pa(pkt));
		cmdq_thread_set_pc(thread, task->pa_base);
		writel(CMDQ_THR_IRQ_EN, thread->base + CMDQ_THR_IRQ_ENABLE);
		writel(CMDQ_THR_ENABLED, thread->base + CMDQ_THR_ENABLE_TASK);
#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
		mmprofile_log_ex(cmdq_mmp.thread_en, MMPROFILE_FLAG_PULSE,
			MMP_THD(thread, cmdq), CMDQ_THR_ENABLED);
#endif

		cmdq_log("set pc:0x%08x end:0x%08x pkt:0x%p",
			(u32)task->pa_base,
			(u32)cmdq_task_get_end_pa(pkt),
			pkt);

		if (thread->timeout_ms != CMDQ_NO_TIMEOUT) {
			mod_timer(&thread->timeout, jiffies +
				msecs_to_jiffies(thread->timeout_ms));
			thread->timer_mod = sched_clock();
		}
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

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	pkt->rec_trigger = sched_clock();
#endif
}

static void cmdq_task_exec_done(struct cmdq_task *task, s32 err)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	task->pkt->rec_irq = sched_clock();
#endif
	cmdq_task_callback(task->pkt, err);
	cmdq_log("pkt:0x%p done err:%d", task->pkt, err);
	list_del_init(&task->list_entry);
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

static void cmdq_task_handle_error(struct cmdq_task *task)
{
	struct cmdq_thread *thread = task->thread;
	struct cmdq_task *next_task;

	cmdq_err("task 0x%p pkt 0x%p error", task, task->pkt);
	WARN_ON(cmdq_thread_suspend(task->cmdq, thread) < 0);
	next_task = list_first_entry_or_null(&thread->task_busy_list,
			struct cmdq_task, list_entry);
	if (next_task)
		cmdq_thread_set_pc(thread, next_task->pa_base);
	cmdq_thread_resume(thread);
}

static void cmdq_thread_irq_handler(struct cmdq *cmdq,
	struct cmdq_thread *thread, struct list_head *removes)
{
	struct cmdq_task *task, *tmp, *curr_task = NULL;
	u32 irq_flag;
	dma_addr_t curr_pa, task_end_pa;
	s32 err = 0;

	if (atomic_read(&cmdq->usage) <= 0) {
		cmdq_log("irq handling during gce off gce:%lx thread:%u",
			(unsigned long)cmdq->base_pa, thread->idx);
		return;
	}

	irq_flag = readl(thread->base + CMDQ_THR_IRQ_STATUS);
	writel(~irq_flag, thread->base + CMDQ_THR_IRQ_STATUS);

	cmdq_log("irq flag:%#x gce:%lx idx:%u",
		irq_flag, (unsigned long)cmdq->base_pa, thread->idx);

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
		cmdq_err("pc:%pa end:%pa err:%d gce base:%lx",
			&curr_pa, &task_end_pa, err,
			(unsigned long)cmdq->base_pa);

	cmdq_log("task status %pa~%pa err:%d",
		&curr_pa, &task_end_pa, err);

	task = list_first_entry_or_null(&thread->task_busy_list,
		struct cmdq_task, list_entry);
	if (task && task->pkt->loop) {
		cmdq_log("task loop %p", &task->pkt);
		cmdq_task_callback(task->pkt, err);

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
		task->pkt->rec_irq = sched_clock();
#endif

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
		mmprofile_log_ex(cmdq_mmp.loop_irq, MMPROFILE_FLAG_PULSE,
			MMP_THD(thread, cmdq), (unsigned long)task->pkt);
#endif

		return;
	}

	if (thread->dirty) {
		cmdq_log("task in error dump thread:%u pkt:0x%p",
			thread->idx, task ? task->pkt : NULL);
		return;
	}

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_log_ex(cmdq_mmp.cmdq_irq, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq), task ? (unsigned long)task->pkt : 0);
#endif

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
			list_add_tail(&task->list_entry, removes);
		} else if (err) {
			cmdq_err("pkt:0x%p thread:%u err:%d",
				curr_task->pkt, thread->idx, err);
			cmdq_buf_dump_schedule(task, false, curr_pa);
			cmdq_task_exec_done(task, err);
			cmdq_task_handle_error(curr_task);
			list_add_tail(&task->list_entry, removes);
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
		thread->timer_mod = sched_clock();
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
	struct cmdq_task *task, *tmp;
	struct list_head removes;

	if (atomic_read(&cmdq->usage) <= 0) {
		cmdq_msg("%s cmdq:%#lx suspend:%s",
			__func__, (unsigned long)cmdq->base_pa,
			cmdq->suspended ? "true" : "false");
		return IRQ_HANDLED;
	}

	irq_status = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS) & cmdq->irq_mask;
	cmdq_log("gce:%lx irq: %#x, %#x",
		(unsigned long)cmdq->base_pa, (u32)irq_status,
		(u32)(irq_status ^ cmdq->irq_mask));
	if (!(irq_status ^ cmdq->irq_mask)) {
		cmdq_msg("not handle for empty status:0x%x",
			(u32)irq_status);
		return IRQ_NONE;
	}

	INIT_LIST_HEAD(&removes);
	for_each_clear_bit(bit, &irq_status, fls(cmdq->irq_mask)) {
		struct cmdq_thread *thread = &cmdq->thread[bit];

		cmdq_log("bit=%d, thread->base=%p", bit, thread->base);
		if (!thread->occupied) {
			secure_irq = true;
			continue;
		}

		spin_lock_irqsave(&thread->chan->lock, flags);
		cmdq_thread_irq_handler(cmdq, thread, &removes);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
	}

	list_for_each_entry_safe(task, tmp, &removes, list_entry) {
		list_del(&task->list_entry);
		kfree(task);
	}

	return secure_irq ? IRQ_NONE : IRQ_HANDLED;
}

static bool cmdq_thread_timeout_excceed(struct cmdq_thread *thread)
{
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	u64 duration;

	/* If first time exec time stamp smaller than timeout value,
	 * it is last round timeout. Skip it.
	 */
	duration = div_s64(sched_clock() - thread->timer_mod, 1000000);
	if (duration < thread->timeout_ms) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms - duration));
		thread->timer_mod = sched_clock();
		cmdq_msg(
			"thread:%u usage:%d mod time:%llu dur:%llu timeout not excceed",
			thread->idx, atomic_read(&cmdq->usage),
			thread->timer_mod, duration);
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
	struct list_head removes;

	INIT_LIST_HEAD(&removes);

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
	cmdq_thread_irq_handler(cmdq, thread, &removes);

	if (list_empty(&thread->task_busy_list)) {
		cmdq_err("thread:%u empty after irq handle in timeout",
			thread->idx);
		goto unlock_free_done;
	}

	/* After IRQ, first task may change. */
	if (!cmdq_thread_timeout_excceed(thread)) {
		cmdq_thread_resume(thread);
		goto unlock_free_done;
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

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	mmprofile_log_ex(cmdq_mmp.warning, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq),
		timeout_task ? (unsigned long)timeout_task : 0);
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
		thread->timer_mod = sched_clock();
		cmdq_thread_err_reset(cmdq, thread,
			task->pa_base, thread->priority);
		cmdq_thread_resume(thread);
	} else {
		cmdq_thread_resume(thread);
		cmdq_thread_disable(cmdq, thread);
		cmdq_clk_disable(cmdq);
	}

unlock_free_done:
	spin_unlock_irqrestore(&thread->chan->lock, flags);

	list_for_each_entry_safe(task, tmp, &removes, list_entry) {
		list_del(&task->list_entry);
		kfree(task);
	}
}
static void cmdq_thread_handle_timeout(struct timer_list *t)
{
	struct cmdq_thread *thread = from_timer(thread, t, timeout);
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

void cmdq_dump_core(struct mbox_chan *chan)
{
	struct cmdq *cmdq = dev_get_drvdata(chan->mbox->dev);
	u32 irq, loaded, cycle, thd_timer, tpr_mask, tpr_en;

	irq = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS);
	loaded = readl(cmdq->base + CMDQ_CURR_LOADED_THR);
	cycle = readl(cmdq->base + CMDQ_THR_EXEC_CYCLES);
	thd_timer = readl(cmdq->base + CMDQ_THR_TIMEOUT_TIMER);
	tpr_mask = readl(cmdq->base + CMDQ_TPR_MASK);
	tpr_en = readl(cmdq->base + CMDQ_TPR_TIMEOUT_EN);

	cmdq_util_msg(
		"irq:%#x loaded:%#x cycle:%#x thd timer:%#x mask:%#x en:%#x",
		irq, loaded, cycle, thd_timer, tpr_mask, tpr_en);
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	cmdq_util_dump_dbg_reg(chan);
#endif
}
EXPORT_SYMBOL(cmdq_dump_core);

void cmdq_thread_dump_spr(struct cmdq_thread *thread)
{
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	u32 i, spr[4] = {0}, dbg[2] = {0}, gpr[16] = {0};

	for (i = 0; i < 4; i++)
		spr[i] = readl(thread->base + CMDQ_THR_SPR + i * 4);
	dbg[0] = readl(cmdq->base + GCE_DEBUG_START_ADDR);
	dbg[1] = readl(cmdq->base + GCE_DEBUG_END_ADDR);

	cmdq_util_msg("thrd:%u spr:%#x %#x %#x %#x dbg:%#x %#x",
		thread->idx, spr[0], spr[1], spr[2], spr[3], dbg[0], dbg[1]);

	for (i = 0; i < 16; i++)
		gpr[i] = readl(cmdq->base + GCE_GPR_R0_START + i * 4);

	cmdq_util_msg(
		"cmdq:%pa gpr:%#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x %#x",
		&cmdq->base_pa, gpr[0], gpr[1], gpr[2], gpr[3], gpr[4], gpr[5],
		gpr[6], gpr[7], gpr[8], gpr[9], gpr[10], gpr[11], gpr[12],
		gpr[13], gpr[14], gpr[15]);
}
EXPORT_SYMBOL(cmdq_thread_dump_spr);

void cmdq_thread_dump(struct mbox_chan *chan, struct cmdq_pkt *cl_pkt,
	u64 **inst_out, dma_addr_t *pc_out)
{
	struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	unsigned long flags;
	struct cmdq_task *task;
	struct cmdq_pkt_buffer *buf;

	struct cmdq_pkt *pkt = NULL;
	u32 warn_rst, en, suspend, status, irq, irq_en, curr_pa, end_pa, cnt,
		wait_token, cfg, prefetch, pri = 0;
	size_t size = 0;
	u64 *end_va, *curr_va = NULL, inst = 0, last_inst[2] = {0};
	void *va_base = NULL;
	dma_addr_t pa_base;
	bool empty = true;

	/* lock channel and get info */
	spin_lock_irqsave(&chan->lock, flags);

	if (atomic_read(&cmdq->usage) <= 0) {
		cmdq_err("%s gce off cmdq:%lx thread:%u",
			__func__, cmdq, thread->idx);
		dump_stack();
		spin_unlock_irqrestore(&chan->lock, flags);
		return;
	}

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

	list_for_each_entry(task, &thread->task_busy_list, list_entry) {
		empty = false;

		if (curr_pa == cmdq_task_get_end_pa(task->pkt))
			curr_va = (u64 *)cmdq_task_get_end_va(task->pkt);
		else
			curr_va = (u64 *)cmdq_task_current_va(curr_pa,
				task->pkt);
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

	cmdq_util_msg(
		"thd:%u pc:%#010x(%p) inst:%#018llx end:%#010x cnt:%#x token:%#010x",
		thread->idx, curr_pa, curr_va, inst, end_pa, cnt, wait_token);
	cmdq_util_msg(
		"rst:%#x en:%#x suspend:%#x status:%#x irq:%x en:%#x cfg:%#x",
		warn_rst, en, suspend, status, irq, irq_en, cfg);
	cmdq_thread_dump_spr(thread);

	if (pkt) {
		cmdq_util_msg(
			"cur pkt:0x%p size:%zu va:0x%p pa:%pa priority:%u",
			pkt, size, va_base, &pa_base, pri);
		cmdq_util_msg("last inst %#018llx %#018llx",
			last_inst[0], last_inst[1]);

		if (cl_pkt && cl_pkt != pkt) {
			buf = list_first_entry(&cl_pkt->buf, typeof(*buf),
				list_entry);
			cmdq_util_msg(
				"expect pkt:0x%p size:%zu va:0x%p pa:%pa priority:%u",
				cl_pkt, cl_pkt->cmd_buf_size, buf->va_base,
				&buf->pa_base, cl_pkt->priority);

			curr_va = NULL;
			curr_pa = 0;
		}
	} else {
		/* empty or not found case is critical */
		cmdq_util_msg("pkt not available (%s)",
			empty ? "thread empty" : "pc not match");
	}

/* if pc match end and irq flag on, dump irq status */
	if (curr_pa == end_pa && irq)
#ifdef CONFIG_MTK_GIC_V3_EXT
		mt_irq_dump_status(cmdq->irq);
#else
		cmdq_util_msg("gic dump not support irq id:%u\n",
			cmdq->irq);
#endif

	if (inst_out)
		*inst_out = curr_va;
	if (pc_out)
		*pc_out = curr_pa;
}
EXPORT_SYMBOL(cmdq_thread_dump);

void cmdq_thread_dump_all(void *mbox_cmdq)
{
	struct cmdq *cmdq = mbox_cmdq;
	u32 i;
	u32 en, curr_pa, end_pa;
	s32 usage = atomic_read(&cmdq->usage);

	cmdq_util_msg("cmdq:%#x usage:%d", (u32)cmdq->base_pa, usage);
	if (usage <= 0)
		return;

	for (i = 0; i < cmdq->thread_nr; i++) {
		struct cmdq_thread *thread = &cmdq->thread[i];

		if (!thread->occupied || list_empty(&thread->task_busy_list))
			continue;

		en = readl(thread->base + CMDQ_THR_ENABLE_TASK);
		if (!en)
			continue;

		curr_pa = cmdq_thread_get_pc(thread);
		end_pa = cmdq_thread_get_end(thread);

		cmdq_util_msg("thd idx:%u pc:%#x end:%#x",
			thread->idx, curr_pa, end_pa);
		cmdq_thread_dump_spr(thread);
	}
}
EXPORT_SYMBOL(cmdq_thread_dump_all);

void cmdq_thread_dump_all_seq(void *mbox_cmdq, struct seq_file *seq)
{
	struct cmdq *cmdq = mbox_cmdq;
	u32 i;
	u32 en, curr_pa, end_pa;
	s32 usage = atomic_read(&cmdq->usage);

	seq_printf(seq, "[cmdq] cmdq:%#x usage:%d\n",
		(u32)cmdq->base_pa, usage);
	if (usage <= 0)
		return;

	for (i = 0; i < cmdq->thread_nr; i++) {
		struct cmdq_thread *thread = &cmdq->thread[i];

		if (!thread->occupied || list_empty(&thread->task_busy_list))
			continue;

		en = readl(thread->base + CMDQ_THR_ENABLE_TASK);
		if (!en)
			continue;

		curr_pa = cmdq_thread_get_pc(thread);
		end_pa = cmdq_thread_get_end(thread);

		seq_printf(seq, "[cmdq] thd idx:%u pc:%#x end:%#x\n",
			thread->idx, curr_pa, end_pa);
	}

}
EXPORT_SYMBOL(cmdq_thread_dump_all_seq);

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
	struct list_head removes;

	INIT_LIST_HEAD(&removes);

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
	cmdq_thread_irq_handler(cmdq, thread, &removes);

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
		thread->timer_mod = sched_clock();
	}

	if (last_task) {
		/* reset end addr again if remove last task */
		task = list_last_entry(&thread->task_busy_list,
			typeof(*task), list_entry);
		cmdq_thread_set_end(thread, cmdq_task_get_end_pa(task->pkt));
	}

	cmdq_thread_resume(thread);

	spin_unlock_irqrestore(&thread->chan->lock, flags);

	list_for_each_entry_safe(task, tmp, &removes, list_entry) {
		list_del(&task->list_entry);
		kfree(task);
	}
}
EXPORT_SYMBOL(cmdq_mbox_thread_remove_task);

static void cmdq_mbox_thread_stop(struct cmdq_thread *thread)
{
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	struct cmdq_task *task, *tmp;
	unsigned long flags;
	struct list_head removes;

	INIT_LIST_HEAD(&removes);

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
	cmdq_thread_irq_handler(cmdq, thread, &removes);
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

	list_for_each_entry_safe(task, tmp, &removes, list_entry) {
		list_del(&task->list_entry);
		kfree(task);
	}
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

	for (i = 0; i < cmdq->thread_nr; i++) {
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
	cmdq_trace_begin("%s", __func__);
	cmdq_task_exec(data, chan->con_priv);
	cmdq_trace_end();
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

u32 cmdq_thread_timeout_backup(struct cmdq_thread *thread, const u32 ms)
{
	u32 backup = thread->timeout_ms;

	thread->timeout_ms = ms;
	return backup;
}
EXPORT_SYMBOL(cmdq_thread_timeout_backup);

void cmdq_thread_timeout_restore(struct cmdq_thread *thread, const u32 ms)
{
	thread->timeout_ms = ms;
}
EXPORT_SYMBOL(cmdq_thread_timeout_restore);

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

static void cmdq_config_default_token(struct device *dev, struct cmdq *cmdq)
{
	int count, ret;

	count = of_property_count_u16_elems(dev->of_node, "default_tokens");
	if (count <= 0) {
		cmdq_err("no default tokens:%d", count);
		return;
	}

	cmdq->token_cnt = count;
	cmdq->tokens = devm_kcalloc(dev, count, sizeof(*cmdq->tokens),
		GFP_KERNEL);
	ret = of_property_read_u16_array(dev->of_node,
		"default_tokens", cmdq->tokens, count);
	if (ret < 0) {
		cmdq_err("of_property_read_u16_array fail err:%d", ret);
		cmdq->token_cnt = 0;
	}
}

static int cmdq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct cmdq *cmdq;
	int err, i;
	struct gce_plat *plat_data;

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

	plat_data = (struct gce_plat *)of_device_get_match_data(dev);
	if (!plat_data) {
		dev_err(dev, "failed to get match data\n");
		return -EINVAL;
	}

	cmdq->thread_nr = plat_data->thread_nr;
	gce_shift_bit = plat_data->shift;
	cmdq->irq_mask = GENMASK(cmdq->thread_nr - 1, 0);

	dev_notice(dev, "cmdq thread:%u shift:%u base:0x%lx pa:0x%lx\n",
		plat_data->thread_nr, plat_data->shift,
		(unsigned long)cmdq->base,
		(unsigned long)cmdq->base_pa);

	cmdq_msg("cmdq thread:%u shift:%u base:0x%lx pa:0x%lx\n",
		plat_data->thread_nr, plat_data->shift,
		(unsigned long)cmdq->base,
		(unsigned long)cmdq->base_pa);

	cmdq_config_prefetch(dev->of_node, cmdq);
	cmdq_config_dma_mask(dev);
	cmdq_config_default_token(dev, cmdq);

	cmdq->clock = devm_clk_get(dev, "gce");
	if (IS_ERR(cmdq->clock)) {
		cmdq_err("failed to get gce clk");
		cmdq->clock = NULL;
	}

	cmdq->clock_timer = devm_clk_get(dev, "gce-timer");
	if (IS_ERR(cmdq->clock_timer)) {
		cmdq_err("failed to get gce timer clk");
		cmdq->clock_timer = NULL;
	}

	cmdq->mbox.dev = dev;
	cmdq->mbox.chans = devm_kcalloc(dev, plat_data->thread_nr,
			sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans)
		return -ENOMEM;

	cmdq->mbox.num_chans = plat_data->thread_nr;
	cmdq->mbox.ops = &cmdq_mbox_chan_ops;
	cmdq->mbox.of_xlate = cmdq_xlate;

	/* make use of TXDONE_BY_ACK */
	cmdq->mbox.txdone_irq = false;
	cmdq->mbox.txdone_poll = false;

	for (i = 0; i < cmdq->thread_nr; i++) {
		cmdq->thread[i].base = cmdq->base + CMDQ_THR_BASE +
				CMDQ_THR_SIZE * i;
		cmdq->thread[i].gce_pa = cmdq->base_pa;
		INIT_LIST_HEAD(&cmdq->thread[i].task_busy_list);
		timer_setup(&cmdq->thread[i].timeout,
			cmdq_thread_handle_timeout, 0);
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
	dev_notice(dev, "register mailbox successfully\n");

	cmdq->buf_dump_wq = alloc_ordered_workqueue(
			"%s", WQ_MEM_RECLAIM | WQ_HIGHPRI,
			"cmdq_buf_dump");

	cmdq->timeout_wq = create_singlethread_workqueue(
		"cmdq_timeout_handler");

	platform_set_drvdata(pdev, cmdq);
	WARN_ON(clk_prepare(cmdq->clock) < 0);
	WARN_ON(clk_prepare(cmdq->clock_timer) < 0);

	wakeup_source_add(&cmdq->wake_lock);

	spin_lock_init(&cmdq->lock);
	clk_enable(cmdq->clock);
	cmdq_init(cmdq);
	clk_disable(cmdq->clock);

	cmdq_mmp_init();

#ifdef CONFIG_MTK_CMDQ_MBOX_EXT
	cmdq->hwid = cmdq_util_track_ctrl(cmdq, cmdq->base_pa, false);
#endif
	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
};

static const struct gce_plat gce_plat_v2 = {.thread_nr = 16};
static const struct gce_plat gce_plat_v4 = {.thread_nr = 24, .shift = 3};

static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mt8173-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6761-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6779-gce", .data = (void *)&gce_plat_v4},
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

static __init int cmdq_drv_init(void)
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

void cmdq_mbox_enable(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	WARN_ON(clk_prepare(cmdq->clock) < 0);
	cmdq_clk_enable(cmdq);
}

void cmdq_mbox_disable(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	cmdq_clk_disable(cmdq);
	clk_unprepare(cmdq->clock);
}

s32 cmdq_mbox_get_usage(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return atomic_read(&cmdq->usage);
}

void *cmdq_mbox_get_base(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return (void *)cmdq->base;
}

phys_addr_t cmdq_mbox_get_base_pa(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return cmdq->base_pa;
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

u32 cmdq_mbox_get_thread_timeout(void *chan)
{
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;

	return thread->timeout_ms;
}
EXPORT_SYMBOL(cmdq_mbox_get_thread_timeout);

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

unsigned long cmdq_get_tracing_mark(void)
{
	static unsigned long __read_mostly tracing_mark_write_addr;

	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");

	return tracing_mark_write_addr;
}

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
void cmdq_mmp_wait(struct mbox_chan *chan, void *pkt)
{
	struct cmdq_thread *thread = chan->con_priv;
	struct cmdq *cmdq = container_of(chan->mbox, typeof(*cmdq), mbox);

	mmprofile_log_ex(cmdq_mmp.wait, MMPROFILE_FLAG_PULSE,
		MMP_THD(thread, cmdq), (unsigned long)pkt);
}
EXPORT_SYMBOL(cmdq_mmp_wait);
#endif

arch_initcall(cmdq_drv_init);
