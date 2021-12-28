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
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/atomic.h>
#include <linux/sched/clock.h>
#include <linux/suspend.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

#include <iommu_debug.h>

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#include "cmdq-util.h"
struct cmdq_util_controller_fp *cmdq_util_controller;
#endif

#ifndef cmdq_util_msg
#define cmdq_util_msg(f, args...) cmdq_msg(f, ##args)
#endif

#ifndef cmdq_util_err
#define cmdq_util_err(f, args...) cmdq_dump(f, ##args)
#endif

cmdq_mminfra_power mminfra_power_cb;
cmdq_mminfra_gce_cg mminfra_gce_cg;

/* ddp main/sub, mdp path 0/1/2/3, general(misc) */
#define CMDQ_OP_CODE_MASK		(0xff << CMDQ_OP_CODE_SHIFT)
#define CMDQ_IRQ_MASK			GENMASK(CMDQ_THR_MAX_COUNT - 1, 0)

#define CMDQ_CORE_REST			0x0
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
#define CMDQ_ULTRA_EN			BIT(0)
#define CMDQ_PREULTRA_EN		BIT(1)

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

#define GCE_BUS_GCTL			0x40
#define GCE_GCTL_VALUE			0x48
#define GCE_OUTPIN_EVENT		0x4c
#define GCE_GPR_R0_START		0x80
#define GCE_DEBUG_START_ADDR		0x1104
#define GCE_DEBUG_END_ADDR		0x1108

#define CMDQ_THR_ENABLED		0x1
#define CMDQ_THR_DISABLED		0x0
#define CMDQ_THR_SUSPEND		0x1
#define CMDQ_THR_RESUME			0x0
#define CMDQ_THR_STATUS_SUSPENDED	BIT(1)
#define CMDQ_THR_DO_WARM_RESET		BIT(0)
#define CMDQ_THR_DO_HARD_RESET		BIT(16)
#define CMDQ_THR_ACTIVE_SLOT_CYCLES	0x3200
#define CMDQ_INST_CYCLE_TIMEOUT		0x0
#define CMDQ_THR_IRQ_DONE		0x1
#define CMDQ_THR_IRQ_ERROR		0x12
#define CMDQ_THR_IRQ_EN			(CMDQ_THR_IRQ_ERROR | CMDQ_THR_IRQ_DONE)
#define CMDQ_THR_IS_WAITING		BIT(31)
#define CMDQ_THR_PRIORITY		0x7
#define CMDQ_TPR_EN			BIT(31)

#define GCE_DBG_CTL			0x3000
#define GCE_DBG0			0x3004
#define GCE_DBG2			0x300C
#define GCE_DBG3			0x3010

#define CMDQ_JUMP_BY_OFFSET		0x10000000
#define CMDQ_JUMP_BY_PA			0x10000001

#define CMDQ_MIN_AGE_VALUE              (5)	/* currently disable age */
#define CMDQ_INIT_BUF_SIZE		8484

#define CMDQ_DRIVER_NAME		"mtk_cmdq_mbox"

/* pc and end shift bit for gce, should be config in probe */
int gce_shift_bit;
EXPORT_SYMBOL(gce_shift_bit);

int gce_mminfra;
EXPORT_SYMBOL(gce_mminfra);

bool skip_poll_sleep;
EXPORT_SYMBOL(skip_poll_sleep);

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
	u64			end_time;
};

struct cmdq_buf_dump {
	struct cmdq		*cmdq;
	struct work_struct	dump_work;
	bool			timeout; /* 0: error, 1: timeout */
	void			*cmd_buf;
	size_t			cmd_buf_size;
	dma_addr_t		pa_offset; /* pa_curr - pa_base */
};

#define mmp_event unsigned int

struct cmdq_mmp_event {
	mmp_event cmdq;
	mmp_event cmdq_irq;
	mmp_event loop_irq;
	mmp_event thread_en;
	mmp_event thread_suspend;
	mmp_event submit;
	mmp_event wait;
	mmp_event wait_done;
	mmp_event warning;
};
struct cmdq_mmp_event	cmdq_mmp;

struct cmdq {
	struct mbox_controller	mbox;
	void __iomem		*base;
	phys_addr_t		base_pa;
	u8			hwid;
	u32			irq;
	struct list_head	irq_removes;
	spinlock_t		irq_removes_lock;
	struct wait_queue_head	err_irq_wq;
	unsigned long		err_irq_idx;
	struct workqueue_struct	*buf_dump_wq;
	struct cmdq_thread	thread[CMDQ_THR_MAX_COUNT];
	u32			prefetch;
	struct clk		*clock;
	struct clk		*clock_timer;
	bool			suspended;
	atomic_t		usage;
	atomic_t		mbox_usage;
	struct mutex		mbox_mutex;
	struct workqueue_struct *timeout_wq;
	struct wakeup_source	*wake_lock;
	bool			wake_locked;
	spinlock_t		lock;
	u32			token_cnt;
	u16			*tokens;
#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
	struct cmdq_mmp_event	mmp;
#endif
	void			*init_cmds_base;
	dma_addr_t		init_cmds;
	bool			sw_ddr_en;
	bool			outpin_en;
	bool			prebuilt_enable;
	bool			unprepare_in_idle;
	struct cmdq_client	*prebuilt_clt;
};

struct gce_plat {
	u32 thread_nr;
	u8 shift;
	u32 mminfra;
};

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
#include "../misc/mediatek/mmp/mmprofile.h"

#define MMP_THD(t, c)	((t)->idx | ((c)->hwid << 5))
#endif

void cmdq_get_mminfra_cb(cmdq_mminfra_power cb)
{
	mminfra_power_cb = cb;
}
EXPORT_SYMBOL(cmdq_get_mminfra_cb);

void cmdq_get_mminfra_gce_cg_cb(cmdq_mminfra_gce_cg cb)
{
	mminfra_gce_cg = cb;
}
EXPORT_SYMBOL(cmdq_get_mminfra_gce_cg_cb);

static struct cmdq *g_cmdq[2];

void cmdq_dump_usage(void)
{
	s32 i;

	for (i = 0; i < 2; i++)
		cmdq_msg(
			"%s: hwid:%d suspend:%d usage:%d mbox_usage:%d wake_lock:%d",
			__func__, g_cmdq[i]->hwid, g_cmdq[i]->suspended,
			atomic_read(&g_cmdq[i]->usage),
			atomic_read(&g_cmdq[i]->mbox_usage),
			g_cmdq[i]->wake_locked);
}
EXPORT_SYMBOL(cmdq_dump_usage);

static void cmdq_init_cpu(struct cmdq *cmdq)
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

static void cmdq_init(struct cmdq *cmdq)
{
	if (cmdq->init_cmds_base)
		cmdq_init_cmds(cmdq);
	else
		cmdq_init_cpu(cmdq);
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
	mmprofile_enable_event_recursive(cmdq_mmp.cmdq, 1);
	mmprofile_start(1);
#endif
}

static void cmdq_lock_wake_lock(struct cmdq *cmdq, bool lock)
{
	cmdq_trace_ex_begin("%s", __func__);

	if (lock) {
		if (!cmdq->wake_locked) {
			__pm_stay_awake(cmdq->wake_lock);
			cmdq->wake_locked = true;
		} else  {
			/* should not reach here */
			cmdq_err("try lock twice cmdq:%lx",
				(unsigned long)cmdq);
			dump_stack();
		}
	} else {
		if (cmdq->wake_locked) {
			__pm_relax(cmdq->wake_lock);
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

static int cmdq_ultra_en(struct cmdq *cmdq)
{
	cmdq_log("%s hwid:%d", __func__, cmdq->hwid);

	writel(CMDQ_ULTRA_EN, cmdq->base + GCE_BUS_GCTL);
	return 0;
}

static s32 cmdq_clk_enable(struct cmdq *cmdq)
{
	s32 usage, err, err_timer;
	unsigned long flags;
	u32 id;

	cmdq_trace_ex_begin("%s", __func__);

	spin_lock_irqsave(&cmdq->lock, flags);

	usage = atomic_inc_return(&cmdq->usage);
	err = clk_enable(cmdq->clock);
	if (usage <= 0 || err < 0)
		cmdq_err("ref count error after inc:%d err:%d suspend:%s",
			usage, err, cmdq->suspended ? "true" : "false");
	else if (usage == 1) {
		cmdq_log("cmdq begin mbox");
		id = cmdq_util_get_hw_id((u32)cmdq->base_pa);

		if (mminfra_gce_cg && !mminfra_gce_cg(id)) {
			cmdq_err("gce cg is off,cmdq:%pa id:%u usage:%d",
			&cmdq->base_pa, cmdq->hwid, atomic_read(&cmdq->usage));
			dump_stack();
		}

		if (cmdq->prefetch)
			writel(cmdq->prefetch,
				cmdq->base + CMDQ_PREFETCH_GSIZE);
		writel(CMDQ_TPR_EN, cmdq->base + CMDQ_TPR_MASK);
		if (cmdq->sw_ddr_en) {
			writel((0x7 << 16) + 0x7, cmdq->base + GCE_GCTL_VALUE);
			writel(0, cmdq->base + GCE_DEBUG_START_ADDR);
		}
		writel(cmdq->outpin_en ? 0x3 : 0x0,
			cmdq->base + GCE_OUTPIN_EVENT);
		/* make sure pm not suspend */
		cmdq_lock_wake_lock(cmdq, true);
		if (gce_mminfra)
			cmdq_ultra_en(cmdq);
		if (cmdq->prebuilt_enable) {
			cmdq_init_cpu(cmdq);
			cmdq_util_prebuilt_enable(cmdq->hwid);
		} else
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

	if (usage == -1)
		cmdq_util_aee("CMDQ", "%s cmdq:%pa suspend:%d usage:%d",
			__func__, &cmdq->base_pa, cmdq->suspended, usage);
	else if (usage < 0) {
		/* print error but still try close */
		cmdq_err("ref count error after dec:%d suspend:%s",
			usage, cmdq->suspended ? "true" : "false");
	} else if (usage == 0) {
		cmdq_log("cmdq shutdown mbox");
		/* clear tpr mask */
		writel(0, cmdq->base + CMDQ_TPR_MASK);
		if (cmdq->sw_ddr_en)
			writel(0x7, cmdq->base + GCE_GCTL_VALUE);
		if (cmdq->prebuilt_enable)
			cmdq_util_prebuilt_disable(cmdq->hwid);
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

	return CMDQ_REG_REVERT_ADDR((dma_addr_t)readl(thread->base + CMDQ_THR_CURR_ADDR));
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
	if (id >= CMDQ_GPR_CNT_ID) {
		struct cmdq *cmdq = container_of(chan->mbox, typeof(*cmdq), mbox);

		writel(val, cmdq->base + GCE_GPR_R0_START + (id - CMDQ_GPR_CNT_ID) * 4);
	} else {
		struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;

		writel(val, thread->base + CMDQ_THR_SPR + id * 4);
	}
}
EXPORT_SYMBOL(cmdq_thread_set_spr);

static int cmdq_core_reset(struct cmdq *cmdq)
{
	cmdq_msg("%s hwid:%d", __func__, cmdq->hwid);
	writel(CMDQ_THR_DO_HARD_RESET, cmdq->base + CMDQ_CORE_REST);
	writel(0, cmdq->base + CMDQ_CORE_REST);
	return 0;
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
		cmdq_err("suspend GCE %hu thread %u failed",
			cmdq->hwid, thread->idx);
		if (thread->chan)
			cmdq_chan_dump_dbg(thread->chan);
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

int cmdq_thread_reset(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 warm_reset;

	writel(CMDQ_THR_DO_WARM_RESET, thread->base + CMDQ_THR_WARM_RESET);
	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_WARM_RESET,
			warm_reset, !(warm_reset & CMDQ_THR_DO_WARM_RESET),
			0, 10)) {
		cmdq_err("reset GCE thread %u failed", thread->idx);
		if (thread->chan)
			cmdq_chan_dump_dbg(thread->chan);
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
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	if (cmdq->sw_ddr_en && cmdq_util_controller->thread_ddr_module(thread->idx)) {
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

static void *cmdq_task_current_va(dma_addr_t pa, struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;
	dma_addr_t end;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (list_is_last(&buf->list_entry, &pkt->buf))
			end = CMDQ_BUF_ADDR(buf) + CMDQ_CMD_BUFFER_SIZE -
				pkt->avail_buf_size;
		else
			end = CMDQ_BUF_ADDR(buf) + CMDQ_CMD_BUFFER_SIZE;
		if (pa >= CMDQ_BUF_ADDR(buf) && pa < end)
			return buf->va_base + (pa - CMDQ_BUF_ADDR(buf));
	}

	return NULL;
}

size_t cmdq_task_current_offset(dma_addr_t pa, struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;
	dma_addr_t end;
	size_t offset = 0;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (list_is_last(&buf->list_entry, &pkt->buf))
			end = CMDQ_BUF_ADDR(buf) + CMDQ_CMD_BUFFER_SIZE -
				pkt->avail_buf_size;
		else
			end = CMDQ_BUF_ADDR(buf) + CMDQ_CMD_BUFFER_SIZE;
		if (pa >= CMDQ_BUF_ADDR(buf) && pa < end)
			return offset + (pa - CMDQ_BUF_ADDR(buf));
		offset += CMDQ_CMD_BUFFER_SIZE;
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_task_current_offset);

static bool cmdq_task_is_current_run(dma_addr_t pa, struct cmdq_pkt *pkt)
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
	return CMDQ_BUF_ADDR(buf) + CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
}

static void *cmdq_task_get_end_va(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;

	/* let previous task jump to this task */
	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	return buf->va_base + CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
}

void cmdq_init_cmds(void *dev_cmdq)
{
	struct cmdq *cmdq = dev_cmdq;
	struct cmdq_thread *thread = &cmdq->thread[0];
	dma_addr_t pc, end;
	u32 status;

	cmdq_trace_ex_begin("%s", __func__);

	pc = cmdq->init_cmds;
	end = cmdq->init_cmds + CMDQ_EVENT_MAX * CMDQ_INST_SIZE;

	cmdq_thread_reset(cmdq, thread);
	cmdq_thread_set_end(thread, end);
	cmdq_thread_set_pc(thread, pc);
	writel(CMDQ_THR_ENABLED, thread->base + CMDQ_THR_ENABLE_TASK);

	end = CMDQ_REG_SHIFT_ADDR(end);
	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_CURR_ADDR,
		pc, pc == end, 0, 100)) {
		cmdq_err("clear event instructions timeout pc:%#lx end:%#lx",
			(unsigned long)pc,
			(unsigned long)end);
		writel(CMDQ_THR_SUSPEND, thread->base + CMDQ_THR_SUSPEND_TASK);
		if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_CURR_STATUS,
				status, status & CMDQ_THR_STATUS_SUSPENDED, 0, 1000)) {
			cmdq_err("suspend GCE thread 0x%x failed",
				(u32)(thread->base - cmdq->base));
		}
		cmdq_core_reset(cmdq);
		cmdq_thread_reset(cmdq, thread);
		cmdq_init_cpu(cmdq);
	}
	writel(CMDQ_THR_DISABLED, thread->base + CMDQ_THR_ENABLE_TASK);

	cmdq_trace_ex_end();
}

static void cmdq_task_exec(struct cmdq_pkt *pkt, struct cmdq_thread *thread)
{
	struct cmdq *cmdq;
	struct cmdq_task *task, *last_task;
	dma_addr_t curr_pa, pkt_end_pa, end_pa, dma_handle;
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
	dma_handle = CMDQ_BUF_ADDR(buf);

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (!task) {
		cmdq_task_callback(pkt, -ENOMEM);
		return;
	}
	pkt->task_alloc = true;

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
		if (mminfra_power_cb && !mminfra_power_cb()) {
			cmdq_err("task running when mminfra power off,cmdq:%pa id:%u usage:%d",
			&cmdq->base_pa, cmdq->hwid, atomic_read(&cmdq->usage));
			dump_stack();
		}

		WARN_ON(cmdq_clk_enable(cmdq) < 0);
		WARN_ON(cmdq_thread_reset(cmdq, thread) < 0);

		cmdq_log("task pa:%pa iova:%pa size:%zu thread->base=0x%p thread:%u",
			&buf->pa_base, &buf->iova_base, pkt->cmd_buf_size, thread->base,
			thread->idx);

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
		if (cmdq->sw_ddr_en &&
			cmdq_util_controller->thread_ddr_module(thread->idx)) {
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
		pkt_end_pa = cmdq_task_get_end_pa(pkt);
		cmdq_log("set pc:%pa end:%pa pkt:0x%p",
			&task->pa_base,
			&end_pa,
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
		pkt_end_pa = cmdq_task_get_end_pa(last_task->pkt);
		cmdq_log("set end:%pa pkt:0x%p",
			&pkt_end_pa,
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
	task->end_time = sched_clock();
	list_del_init(&task->list_entry);
}

static void cmdq_buf_dump_schedule(struct cmdq_task *task, bool timeout,
				   dma_addr_t pa_curr)
{
	struct cmdq_pkt_buffer *buf;
	u64 *inst = NULL;

	list_for_each_entry(buf, &task->pkt->buf, list_entry) {
		if (!(pa_curr >= CMDQ_BUF_ADDR(buf) &&
			pa_curr < CMDQ_BUF_ADDR(buf) + CMDQ_CMD_BUFFER_SIZE)) {
			continue;
		}
		inst = (u64 *)(buf->va_base + (pa_curr - CMDQ_BUF_ADDR(buf)));
		break;
	}

	cmdq_util_user_err(task->thread->chan,
		"task:0x%p timeout:%s pkt:0x%p size:%zu pc:%pa inst:0x%016llx",
		task, timeout ? "true" : "false", task->pkt,
		task->pkt->cmd_buf_size, &pa_curr,
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

static void cmdq_thread_dump_pkt_by_pc(struct cmdq_thread *thread, const u64 pc,
	const bool prev)
{
	struct cmdq_task *task;
	struct cmdq_pkt_buffer *buf;

	list_for_each_entry(task, &thread->task_busy_list, list_entry) {
		if (prev)
			cmdq_dump_pkt(task->pkt, pc, true);
		list_for_each_entry(buf, &task->pkt->buf, list_entry) {
			if ((pc >= (CMDQ_BUF_ADDR(buf) & UINT_MAX)) &&
				(pc < ((CMDQ_BUF_ADDR(buf) +
				CMDQ_CMD_BUFFER_SIZE) & UINT_MAX))) {
				if (!prev)
					cmdq_dump_pkt(task->pkt, pc, true);
				return;
			}
		}
	}
}

static void cmdq_thread_irq_handler(struct cmdq *cmdq,
	struct cmdq_thread *thread, struct list_head *removes)
{
	struct cmdq_task *task, *tmp, *curr_task = NULL;
	u32 irq_flag;
	dma_addr_t curr_pa, task_end_pa;
	s32 err = 0;
	unsigned long flags;

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

	if (err < 0) {
		struct iommu_domain *domain;
		phys_addr_t pa;

		cmdq_err("pc:%pa end:%pa err:%d gce base:%lx thread:%u",
			&curr_pa, &task_end_pa, err,
			(unsigned long)cmdq->base_pa, thread->idx);

		cmdq_util_prebuilt_dump(
			cmdq->hwid, CMDQ_TOKEN_PREBUILT_DISP_WAIT); // set iova

		domain = iommu_get_domain_for_dev(cmdq->mbox.dev);
		if (domain) {
			pa = iommu_iova_to_phys(domain, curr_pa);
			cmdq_err("iova:%pa iommu pa:%pa", &curr_pa, &pa);
		} else
			cmdq_err("cannot get dev:%p domain", cmdq->mbox.dev);

		set_bit(thread->idx, &cmdq->err_irq_idx);
		wake_up_interruptible(&cmdq->err_irq_wq);
	}

	cmdq_log("task status %pa~%pa err:%d",
		&curr_pa, &task_end_pa, err);

	task = list_first_entry_or_null(&thread->task_busy_list,
		struct cmdq_task, list_entry);
	if (task && task->pkt->loop) {
		cmdq_log("task loop %p", &task->pkt);
		if (err)
			cmdq_err("irq flag:%#x hwid:%hu idx:%u pkt:%p loop",
				irq_flag, cmdq->hwid, thread->idx, task->pkt);

		cmdq_task_callback(task->pkt, err);

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
		task->pkt->rec_irq = sched_clock();
#endif

#if IS_ENABLED(CMDQ_MMPROFILE_SUPPORT)
		mmprofile_log_ex(cmdq_mmp.loop_irq, MMPROFILE_FLAG_PULSE,
			MMP_THD(thread, cmdq), (unsigned long)task->pkt);
#endif

		if (!err)
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
		thread->irq_task += 1;

		if (task->end_time)
			cmdq_util_err("thd:%d pkt:%p is done,start:%llu end:%llu",
				thread->idx, task->pkt, task->exec_time, task->end_time);
		task_end_pa = cmdq_task_get_end_pa(task->pkt);
		if (cmdq_task_is_current_run(curr_pa, task->pkt)) {
			curr_task = task;
		/* for some self trigger loop, notify it is still working */
			if (curr_task->pkt->self_loop)
				cmdq_task_err_callback(curr_task->pkt, -EBUSY);
		}

		if (!curr_task || curr_pa == task_end_pa - CMDQ_INST_SIZE) {
			if (curr_task && (curr_pa != task_end_pa)) {
				cmdq_log(
					"remove task that not ending pkt:0x%p %pa to %pa",
					curr_task->pkt, &curr_pa, &task_end_pa);
			}
			cmdq_task_exec_done(task, 0);
			spin_lock_irqsave(&cmdq->irq_removes_lock, flags);
			list_add_tail(&task->list_entry, &cmdq->irq_removes);
			spin_unlock_irqrestore(&cmdq->irq_removes_lock, flags);
		} else if (err) {
			cmdq_err("pkt:0x%p thread:%u err:%d",
				curr_task->pkt, thread->idx, err);
			cmdq_buf_dump_schedule(task, false, curr_pa);
			cmdq_task_exec_done(task, err);
			cmdq_task_handle_error(curr_task);
			spin_lock_irqsave(&cmdq->irq_removes_lock, flags);
			list_add_tail(&task->list_entry, &cmdq->irq_removes);
			spin_unlock_irqrestore(&cmdq->irq_removes_lock, flags);
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
	int bit, i;
	bool secure_irq = false;
	u64 start = sched_clock(), end[4];
	u32 end_cnt = 0, thd_cnt = 0;

	if (atomic_read(&cmdq->usage) == -1)
		cmdq_util_aee("CMDQ", "%s irq:%d cmdq:%pa suspend:%d usage:%d",
			__func__, irq, &cmdq->base_pa, cmdq->suspended,
			atomic_read(&cmdq->usage));

	if (atomic_read(&cmdq->usage) <= 0) {
		if (cmdq->suspended)
			return IRQ_HANDLED;

		cmdq_clk_enable(cmdq);
		cmdq_thread_dump_all(cmdq, false, false, false);

		for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++)
			if (cmdq->thread[i].chan) {
				cmdq_dump_core(cmdq->thread[i].chan);
				break;
			}
		cmdq_clk_disable(cmdq);
		return IRQ_HANDLED;
	}

	end[end_cnt++] = sched_clock();

	irq_status = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS) & CMDQ_IRQ_MASK;
	cmdq_log("gce:%lx irq: %#x, %#x",
		(unsigned long)cmdq->base_pa, (u32)irq_status,
		(u32)(irq_status ^ CMDQ_IRQ_MASK));
	if (!(irq_status ^ CMDQ_IRQ_MASK)) {
		cmdq_msg("not handle for empty status:0x%x",
			(u32)irq_status);
		return IRQ_NONE;
	}

	end[end_cnt++] = sched_clock();

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		cmdq->thread[i].irq_time = 0;
		cmdq->thread[i].irq_task = 0;
	}

	for_each_clear_bit(bit, &irq_status, fls(CMDQ_IRQ_MASK)) {
		struct cmdq_thread *thread = &cmdq->thread[bit];
		u64 irq_time;

		cmdq_log("bit=%d, thread->base=%p", bit, thread->base);
		if (!thread->occupied) {
			secure_irq = true;
			continue;
		}

		irq_time = sched_clock();
		spin_lock_irqsave(&thread->chan->lock, flags);
		cmdq_thread_irq_handler(cmdq, thread, &cmdq->irq_removes);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		thread->irq_time = sched_clock() - irq_time;
		thd_cnt += 1;
	}

	end[end_cnt++] = sched_clock();

	set_bit(CMDQ_THR_MAX_COUNT, &cmdq->err_irq_idx);
	wake_up_interruptible(&cmdq->err_irq_wq);

	end[end_cnt] = sched_clock();
	if (end[end_cnt] - start >= 1000000) { /* 1ms */
		cmdq_util_err(
			"IRQ_LONG:%llu atomic:%llu readl:%llu bit:%llu wakeup:%llu",
			end[end_cnt] - start, end[0] - start,
			end[1] - end[0], end[2] - end[1], end[3] - end[2]);
		for (i = 0; i < ARRAY_SIZE(cmdq->thread); i += 8) {
			struct cmdq_thread *thread = &cmdq->thread[i];

			cmdq_util_err(
				" hwid:%hu thread:%u:%d %llu:%u %llu:%u %llu:%u %llu:%u %llu:%u %llu:%u %llu:%u %llu:%u",
				cmdq->hwid, thd_cnt, i,
				thread->irq_time, thread->irq_task,
				(thread + 1)->irq_time, (thread + 1)->irq_task,
				(thread + 2)->irq_time, (thread + 2)->irq_task,
				(thread + 3)->irq_time, (thread + 3)->irq_task,
				(thread + 4)->irq_time, (thread + 4)->irq_task,
				(thread + 5)->irq_time, (thread + 5)->irq_task,
				(thread + 6)->irq_time, (thread + 6)->irq_task,
				(thread + 7)->irq_time, (thread + 7)->irq_task);
		}
	}

	return secure_irq ? IRQ_NONE : IRQ_HANDLED;
}

static int cmdq_irq_handler_thread(void *data)
{
	struct cmdq *cmdq = data;
	unsigned long irq, flags;
	u32 bit;

	while (!kthread_should_stop()) {
		wait_event_interruptible(cmdq->err_irq_wq, cmdq->err_irq_idx);
		irq = cmdq->err_irq_idx;

		if (irq & BIT(CMDQ_THR_MAX_COUNT)) {
			struct cmdq_task *task, *tmp;

			spin_lock_irqsave(&cmdq->irq_removes_lock, flags);
			list_for_each_entry_safe(task, tmp, &cmdq->irq_removes,
				list_entry) {
				list_del(&task->list_entry);

				spin_unlock_irqrestore(
					&cmdq->irq_removes_lock, flags);
				kfree(task);
				spin_lock_irqsave(
					&cmdq->irq_removes_lock, flags);
			}
			spin_unlock_irqrestore(&cmdq->irq_removes_lock, flags);

			clear_bit(CMDQ_THR_MAX_COUNT, &cmdq->err_irq_idx);
			if (irq == BIT(CMDQ_THR_MAX_COUNT))
				continue;
		}

		for_each_set_bit(bit, &cmdq->err_irq_idx, fls(CMDQ_IRQ_MASK)) {
			struct cmdq_thread *thread = &cmdq->thread[bit];
			dma_addr_t pc = cmdq_thread_get_pc(thread);

			cmdq_err("%s: hwid:%hu irq:%#lx idx:%u pc:%pa",
				__func__, cmdq->hwid, cmdq->err_irq_idx,
				thread->idx, &pc);

			spin_lock_irqsave(&thread->chan->lock, flags);
			cmdq_thread_dump_pkt_by_pc(thread, pc, false);
			spin_unlock_irqrestore(&thread->chan->lock, flags);
			clear_bit(bit, &cmdq->err_irq_idx);
		}
		cmdq_util_dump_smi();
		cmdq_util_aee("CMDQ", "%s: hwid:%hu irq:%#lx",
			__func__, cmdq->hwid, irq);
	}

	return 0;
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
	dma_addr_t pa_curr;
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

	cmdq_util_user_err(thread->chan,
		"timeout for thread:0x%p idx:%u usage:%d",
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

		cmdq_msg("ending not curr in timeout pkt:0x%p curr_pa:%pa",
			task->pkt, &pa_curr);
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
	u32 irq, loaded, cycle, thd_timer, tpr_mask, tpr_en, bus_gctl;

	irq = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS);
	loaded = readl(cmdq->base + CMDQ_CURR_LOADED_THR);
	cycle = readl(cmdq->base + CMDQ_THR_EXEC_CYCLES);
	thd_timer = readl(cmdq->base + CMDQ_THR_TIMEOUT_TIMER);
	tpr_mask = readl(cmdq->base + CMDQ_TPR_MASK);
	tpr_en = readl(cmdq->base + CMDQ_TPR_TIMEOUT_EN);
	bus_gctl = readl(cmdq->base + GCE_BUS_GCTL);


	cmdq_util_user_msg(chan,
		"irq:%#x loaded:%#x cycle:%#x thd timer:%#x mask:%#x en:%#x",
		irq, loaded, cycle, thd_timer, tpr_mask, tpr_en);
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	cmdq_chan_dump_dbg(chan);
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

	cmdq_util_user_msg(thread->chan,
		"thrd:%u spr:%#x %#x %#x %#x dbg:%#x %#x",
		thread->idx, spr[0], spr[1], spr[2], spr[3], dbg[0], dbg[1]);

	for (i = 0; i < 16; i++)
		gpr[i] = readl(cmdq->base + GCE_GPR_R0_START + i * 4);

	cmdq_util_user_msg(thread->chan,
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
	u32 warn_rst, en, suspend, status, irq, irq_en, cnt,
		wait_token, cfg, prefetch, pri = 0;
	size_t size = 0;
	u64 *end_va, *curr_va = NULL, inst = 0, last_inst = 0;
	void *va_base = NULL;
	dma_addr_t curr_pa, end_pa, pa_base;
	bool empty = true;

	/* lock channel and get info */
	spin_lock_irqsave(&chan->lock, flags);

	if (atomic_read(&cmdq->usage) <= 0) {
		cmdq_err("%s gce off cmdq:%p thread:%u",
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
		pa_base = CMDQ_BUF_ADDR(buf);

		buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
		end_va = (u64 *)(buf->va_base + CMDQ_CMD_BUFFER_SIZE -
			pkt->avail_buf_size - CMDQ_INST_SIZE);
		last_inst = *end_va;
		break;
	}
	spin_unlock_irqrestore(&chan->lock, flags);

	cmdq_util_user_msg(chan,
		"thd:%u pc:%pa(%p) inst:%#018llx end:%pa cnt:%#x token:%#010x",
		thread->idx, &curr_pa, curr_va, inst, &end_pa, cnt, wait_token);
	cmdq_util_user_msg(chan,
		"rst:%#x en:%#x suspend:%#x status:%#x irq:%x en:%#x cfg:%#x",
		warn_rst, en, suspend, status, irq, irq_en, cfg);
	cmdq_thread_dump_spr(thread);

	if (pkt) {
		cmdq_util_user_msg(chan,
			"cur pkt:0x%p size:%zu va:0x%p pa:%pa priority:%u",
			pkt, size, va_base, &pa_base, pri);
		cmdq_util_user_msg(chan, "last inst %#018llx", last_inst);

		if (cl_pkt && cl_pkt != pkt) {
			buf = list_first_entry(&cl_pkt->buf, typeof(*buf),
				list_entry);
			cmdq_util_user_msg(chan,
				"expect pkt:0x%p size:%zu va:0x%p pa:%pa iova:%pa priority:%u",
				cl_pkt, cl_pkt->cmd_buf_size, buf->va_base,
				&buf->pa_base, &buf->iova_base, cl_pkt->priority);

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
		cmdq_util_msg("gic dump not support irq id:%u\n",
			cmdq->irq);

	if (inst_out)
		*inst_out = curr_va;
	if (pc_out)
		*pc_out = curr_pa;
}
EXPORT_SYMBOL(cmdq_thread_dump);

void cmdq_mbox_dump_dbg(void *mbox_cmdq, void *chan, const bool lock)
{
	struct cmdq *cmdq = mbox_cmdq;
	void *base = cmdq->base;
	u32 dbg0[3], dbg2[6], dbg3, i;
	u32 id;
	unsigned long flags;

	if (!base) {
		cmdq_util_msg("no cmdq dbg since no base");
		return;
	}

	if (lock)
		spin_lock_irqsave(&cmdq->lock, flags);
	if (atomic_read(&cmdq->usage) <= 0) {
		cmdq_util_msg("no cmdq dbg since mbox disable");
		if (lock)
			spin_unlock_irqrestore(&cmdq->lock, flags);
		return;
	}

	id = cmdq_util_get_hw_id((u32)cmdq->base_pa);
	cmdq_util_enable_dbg(id);

	/* debug select */
	for (i = 0; i < 6; i++) {
		if (i < 3) {
			writel((i << 8) | i, base + GCE_DBG_CTL);
			dbg0[i] = readl(base + GCE_DBG0);
		} else {
			/* only other part */
			writel(i << 8, base + GCE_DBG_CTL);
		}
		dbg2[i] = readl(base + GCE_DBG2);
	}

	dbg3 = readl(base + GCE_DBG3);
	if (lock)
		spin_unlock_irqrestore(&cmdq->lock, flags);

	if (chan)
		cmdq_util_user_msg(chan,
		"id:%u dbg0:%#x %#x %#x dbg2:%#x %#x %#x %#x %#x %#x dbg3:%#x",
		id,
		dbg0[0], dbg0[1], dbg0[2],
		dbg2[0], dbg2[1], dbg2[2], dbg2[3], dbg2[4], dbg2[5],
		dbg3);
	else
		cmdq_util_msg(
			"id:%u dbg0:%#x %#x %#x dbg2:%#x %#x %#x %#x %#x %#x dbg3:%#x",
			id,
			dbg0[0], dbg0[1], dbg0[2],
			dbg2[0], dbg2[1], dbg2[2], dbg2[3], dbg2[4], dbg2[5],
			dbg3);
}
EXPORT_SYMBOL(cmdq_mbox_dump_dbg);

void cmdq_chan_dump_dbg(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	cmdq_mbox_dump_dbg(cmdq, chan, false);
}
EXPORT_SYMBOL(cmdq_chan_dump_dbg);

void cmdq_thread_dump_all(void *mbox_cmdq, const bool lock, const bool dump_pkt,
	const bool dump_prev)
{
	struct cmdq *cmdq = mbox_cmdq;
	s32 usage = atomic_read(&cmdq->usage), i;

	cmdq_util_msg("%s: cmdq:%pa hwid:%hu usage:%d",
		__func__, &cmdq->base_pa, cmdq->hwid, usage);

	if (usage <= 0)
		return;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		struct cmdq_thread *thread = &cmdq->thread[i];
		unsigned long flags = 0L;
		dma_addr_t curr_pa, end_pa;

		if (!thread->occupied || !thread->chan)
			continue;

		if (lock)
			spin_lock_irqsave(&thread->chan->lock, flags);

		if (list_empty(&thread->task_busy_list) ||
			!readl(thread->base + CMDQ_THR_ENABLE_TASK)) {
			if (lock)
				spin_unlock_irqrestore(
					&thread->chan->lock, flags);
			continue;
		}

		curr_pa = cmdq_thread_get_pc(thread);
		end_pa = cmdq_thread_get_end(thread);

		cmdq_util_msg("thd idx:%u pc:%pa end:%pa",
			thread->idx, &curr_pa, &end_pa);
		cmdq_thread_dump_spr(thread);

		if (dump_pkt)
			cmdq_thread_dump_pkt_by_pc(thread, curr_pa, dump_prev);

		if (lock)
			spin_unlock_irqrestore(&thread->chan->lock, flags);
	}
}
EXPORT_SYMBOL(cmdq_thread_dump_all);

void cmdq_thread_dump_all_seq(void *mbox_cmdq, struct seq_file *seq)
{
	struct cmdq *cmdq = mbox_cmdq;
	u32 i;
	u32 en;
	dma_addr_t curr_pa, end_pa;
	s32 usage = atomic_read(&cmdq->usage);

	seq_printf(seq, "[cmdq] cmdq:%#x usage:%d\n",
		(u32)cmdq->base_pa, usage);
	if (usage <= 0)
		return;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		struct cmdq_thread *thread = &cmdq->thread[i];

		if (!thread->occupied || list_empty(&thread->task_busy_list))
			continue;

		en = readl(thread->base + CMDQ_THR_ENABLE_TASK);
		if (!en)
			continue;

		curr_pa = cmdq_thread_get_pc(thread);
		end_pa = cmdq_thread_get_end(thread);

		seq_printf(seq, "[cmdq] thd idx:%u pc:%pa end:%pa\n",
			thread->idx, &curr_pa, &end_pa);
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
	dma_addr_t pa_curr;
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
	struct cmdq_task *timeout_task = NULL;

	INIT_LIST_HEAD(&removes);

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list)) {
		cmdq_log("stop empty thread:%u", thread->idx);
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
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	/* find timeout task */
	if (thread->dirty) {
		dma_addr_t pa_curr = cmdq_thread_get_pc(thread);

		list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
			list_entry) {
			if (cmdq_task_is_current_run(pa_curr, task->pkt)) {
				timeout_task = task;
				break;
			}
		}
	}

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
		list_entry) {
		/* ignore timeout task */
		if (timeout_task && (timeout_task == task))
			continue;

		cmdq_task_exec_done(task, -ECONNABORTED);
		kfree(task);
	}

	if (list_empty(&thread->task_busy_list)) {
		cmdq_thread_disable(cmdq, thread);
		cmdq_clk_disable(cmdq);
	}
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

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		thread = &cmdq->thread[i];
		if (!list_empty(&thread->task_busy_list)) {
			cmdq_mbox_thread_stop(thread);
			task_running = true;
			cmdq_err("thread %d running", i);
		}
	}

	if (task_running) {
		dev_notice(dev, "exist running task(s) in suspend\n");
		schedule();
	}
	if (!cmdq->unprepare_in_idle) {
		clk_unprepare(cmdq->clock);
		clk_unprepare(cmdq->clock_timer);
	}
	return 0;
}

static int cmdq_resume(struct device *dev)
{
	struct cmdq *cmdq = dev_get_drvdata(dev);
	if (!cmdq->unprepare_in_idle) {
		WARN_ON(clk_prepare(cmdq->clock) < 0);
		WARN_ON(clk_prepare(cmdq->clock_timer) < 0);
	}
	cmdq->suspended = false;
	return 0;
}

static int cmdq_remove(struct platform_device *pdev)
{
	struct cmdq *cmdq = platform_get_drvdata(pdev);

	wakeup_source_unregister(cmdq->wake_lock);
	destroy_workqueue(cmdq->buf_dump_wq);
	mbox_controller_unregister(&cmdq->mbox);
	if (!cmdq->unprepare_in_idle) {
		clk_unprepare(cmdq->clock);
		clk_unprepare(cmdq->clock_timer);
	}

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

static void cmdq_config_init_buf(struct device *dev, struct cmdq *cmdq)
{
	u32 i, *va;

	cmdq->init_cmds_base = dma_alloc_coherent(dev, CMDQ_INIT_BUF_SIZE,
		&cmdq->init_cmds, GFP_KERNEL);
	cmdq_msg("init cmd buffer:%#lx (%#lx)",
		(unsigned long)cmdq->init_cmds_base,
		(unsigned long)cmdq->init_cmds);

	if (!cmdq->init_cmds_base) {
		cmdq_err("fail to alloc init cmd buffer");
		return;
	}

	va = (u32 *)cmdq->init_cmds_base;
	for (i = 0; i < CMDQ_EVENT_MAX; i++) {
		va[i * 2] = 0x80000000;
		va[i * 2 + 1] = 0x20000000 | i;
	}

	/* some token default set to 1, config in dts */
	for (i = 0; i < cmdq->token_cnt; i++)
		va[cmdq->tokens[i] * 2] = 0x80010000;
}

int cmdq_iommu_fault_callback(int port, dma_addr_t mva, void *cb_data)
{
	struct cmdq *cmdq = (struct cmdq *)cb_data;
	struct iommu_domain *domain = iommu_get_domain_for_dev(cmdq->mbox.dev);
	phys_addr_t pa = domain ? iommu_iova_to_phys(domain, mva) : 0;
	s32 i;

	cmdq_msg("%s: port:%d mva:%pa cmdq hwid:%hu iommu domain:%p pa:%pa",
		__func__, port, &mva, cmdq->hwid, domain, &pa);

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		if (!cmdq->thread[i].occupied || !cmdq->thread[i].chan)
			continue;

		cmdq_dump_core(cmdq->thread[i].chan);
		break;
	}

	cmdq_thread_dump_all(cmdq, true, true, true);
	return 0;
}

static int cmdq_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct platform_device *smi;
	struct device_link *link;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct of_phandle_args args;
	struct cmdq *cmdq;
	struct task_struct *kthr;
	int err, i;
	struct gce_plat *plat_data;
	static u8 hwid;

	cmdq = devm_kzalloc(dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		cmdq_err("failed to get resource");
		return -EINVAL;
	}

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

	INIT_LIST_HEAD(&cmdq->irq_removes);
	spin_lock_init(&cmdq->irq_removes_lock);

	init_waitqueue_head(&cmdq->err_irq_wq);
	kthr = kthread_run(cmdq_irq_handler_thread, cmdq, "cmdq_irq_thread");

	plat_data = (struct gce_plat *)of_device_get_match_data(dev);
	if (!plat_data) {
		cmdq_err("failed to get match data\n");
		return -EINVAL;
	}

	gce_shift_bit = plat_data->shift;
	gce_mminfra = plat_data->mminfra;
	if (!of_property_read_bool(dev->of_node, "skip-poll-sleep"))
		skip_poll_sleep = true;

	dev_notice(dev,
		"cmdq thread:%u shift:%u mminfra:%#x base:0x%lx pa:0x%lx\n",
		plat_data->thread_nr, plat_data->shift, plat_data->mminfra,
		(unsigned long)cmdq->base,
		(unsigned long)cmdq->base_pa);

	cmdq_msg("cmdq thread:%u shift:%u base:0x%lx pa:0x%lx\n",
		plat_data->thread_nr, plat_data->shift,
		(unsigned long)cmdq->base,
		(unsigned long)cmdq->base_pa);

	cmdq_config_prefetch(dev->of_node, cmdq);
	cmdq_config_default_token(dev, cmdq);
	cmdq_config_dma_mask(dev);
	cmdq_config_init_buf(dev, cmdq);

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

	node = of_parse_phandle(dev->of_node, "mediatek,smi", 0);
	if (!node)
		cmdq_msg("failed to get mediatek,smi");

	smi = of_find_device_by_node(node);
	if (!smi)
		cmdq_msg("failed to find smi node");
	else {
		link = device_link_add(dev, &smi->dev,
			DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			cmdq_msg("failed to create device link with smi");
	}

	g_cmdq[hwid] = cmdq;
	cmdq->hwid = hwid++;
	cmdq->prebuilt_enable =
		of_property_read_bool(dev->of_node, "prebuilt-enable");
	cmdq->unprepare_in_idle =
		of_property_read_bool(dev->of_node, "unprepare_in_idle");

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
	mutex_init(&cmdq->mbox_mutex);

	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
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

	pm_runtime_enable(dev);
	platform_set_drvdata(pdev, cmdq);
	if (!cmdq->unprepare_in_idle) {
		WARN_ON(clk_prepare(cmdq->clock) < 0);
		WARN_ON(clk_prepare(cmdq->clock_timer) < 0);
	}

	cmdq->wake_lock = wakeup_source_register(dev, "cmdq_pm_lock");

	spin_lock_init(&cmdq->lock);

	cmdq_mmp_init();

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	cmdq_util_controller->track_ctrl(cmdq, cmdq->base_pa, false);
#endif
	cmdq->prebuilt_clt = cmdq_mbox_create(&pdev->dev, 0);

	if (!of_parse_phandle_with_args(
		dev->of_node, "iommus", "#iommu-cells", 0, &args)) {
		mtk_iommu_register_fault_callback(
			args.args[0], cmdq_iommu_fault_callback, cmdq, false);
	}
	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
};

static const struct gce_plat gce_plat_v2 = {.thread_nr = 16};
static const struct gce_plat gce_plat_v4 = {.thread_nr = 24, .shift = 3};
static const struct gce_plat gce_plat_v5 = {
	.thread_nr = 32, .shift = 3, .mminfra = BIT(30)};

static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mt8173-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt8168-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6739-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6761-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6765-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6768-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6771-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt6779-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6785-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6833-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6853-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6873-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6877-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6879-gce", .data = (void *)&gce_plat_v5},
	{.compatible = "mediatek,mt6885-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6893-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt6895-gce", .data = (void *)&gce_plat_v5},
	{.compatible = "mediatek,mt6983-gce", .data = (void *)&gce_plat_v5},
	{.compatible = "mediatek,mt6855-gce", .data = (void *)&gce_plat_v5},
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

	cmdq_util_init();
	err = platform_driver_register(&cmdq_drv);
	if (err) {
		cmdq_err("platform driver register failed:%d", err);
		return err;
	}
	cmdq_helper_init();

	return 0;
}

void cmdq_mbox_enable(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);
	s32 mbox_usage;

	WARN_ON(cmdq->suspended);
	if (cmdq->suspended) {
		cmdq_err("cmdq:%pa id:%u suspend:%d cannot enable usage:%d",
			&cmdq->base_pa, cmdq->hwid, cmdq->suspended,
			atomic_read(&cmdq->usage));
		return;
	}
	pm_runtime_get_sync(cmdq->mbox.dev);
	mutex_lock(&cmdq->mbox_mutex);
	mbox_usage = atomic_inc_return(&cmdq->mbox_usage);
	if (cmdq->unprepare_in_idle) {
		if (mbox_usage == 1) {
			WARN_ON(clk_prepare(cmdq->clock) < 0);
			WARN_ON(clk_prepare(cmdq->clock_timer) < 0);
		}
	}
	mutex_unlock(&cmdq->mbox_mutex);

	cmdq_clk_enable(cmdq);
}
EXPORT_SYMBOL(cmdq_mbox_enable);

void cmdq_mbox_disable(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);
	s32 mbox_usage;

	WARN_ON(cmdq->suspended);
	if (cmdq->suspended) {
		cmdq_err("cmdq:%pa id:%u suspend:%d cannot disable usage:%d",
			&cmdq->base_pa, cmdq->hwid, cmdq->suspended,
			atomic_read(&cmdq->usage));
		return;
	}
	cmdq_clk_disable(cmdq);

	mutex_lock(&cmdq->mbox_mutex);
	mbox_usage = atomic_dec_return(&cmdq->mbox_usage);
	if (cmdq->unprepare_in_idle) {
		if (mbox_usage == 0) {
			clk_unprepare(cmdq->clock_timer);
			clk_unprepare(cmdq->clock);
		} else if (mbox_usage < 0) {
			cmdq_err("mbox_usage:%d", mbox_usage);
			dump_stack();
		}
	}
	mutex_unlock(&cmdq->mbox_mutex);
	if ((mbox_usage == 0) && (atomic_read(&cmdq->usage) > 0))
		cmdq_err("mbox_disable when task running,cmdq:%pa id:%u usage:%d",
		&cmdq->base_pa, cmdq->hwid, atomic_read(&cmdq->usage));
	if (mminfra_power_cb && !mminfra_power_cb()) {
		cmdq_err("mminfra power off when task running,cmdq:%pa id:%u usage:%d",
		&cmdq->base_pa, cmdq->hwid, atomic_read(&cmdq->usage));
		dump_stack();
	}
	pm_runtime_put_sync(cmdq->mbox.dev);
}
EXPORT_SYMBOL(cmdq_mbox_disable);

s32 cmdq_mbox_get_usage(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return atomic_read(&cmdq->usage);
}
EXPORT_SYMBOL(cmdq_mbox_get_usage);

void *cmdq_mbox_get_base(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return (void *)cmdq->base;
}
EXPORT_SYMBOL(cmdq_mbox_get_base);

phys_addr_t cmdq_mbox_get_base_pa(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return cmdq->base_pa;
}
EXPORT_SYMBOL(cmdq_mbox_get_base_pa);

phys_addr_t cmdq_dev_get_base_pa(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		cmdq_err("failed to get resource from dev:%p", dev);
		return -EINVAL;
	}
	cmdq_log("%s: res:%p start:%pa", __func__, res, &res->start);

	return res->start;
}
EXPORT_SYMBOL(cmdq_dev_get_base_pa);

phys_addr_t cmdq_mbox_get_dummy_reg(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return cmdq->thread[22].gce_pa
		+ CMDQ_THR_BASE + CMDQ_THR_SIZE * 22
		+ CMDQ_THR_SPR + 4*CMDQ_THR_SPR_IDX3;
}
EXPORT_SYMBOL(cmdq_mbox_get_dummy_reg);

phys_addr_t cmdq_mbox_get_spr_pa(void *chan, u8 spr)
{
	struct cmdq_thread *thread =
		(struct cmdq_thread *)((struct mbox_chan *)chan)->con_priv;

	return thread->gce_pa + CMDQ_THR_BASE + CMDQ_THR_SIZE * thread->idx +
		CMDQ_THR_SPR + 4 * spr;
}
EXPORT_SYMBOL(cmdq_mbox_get_spr_pa);

struct device *cmdq_mbox_get_dev(void *chan)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return cmdq->mbox.dev;
}

s32 cmdq_mbox_set_hw_id(void *cmdq_mbox)
{
	struct cmdq *cmdq = cmdq_mbox;

	if (!cmdq)
		return -EINVAL;
	cmdq->hwid = (u8)cmdq_util_get_hw_id(cmdq->base_pa);
	cmdq_util_prebuilt_set_client(cmdq->hwid, cmdq->prebuilt_clt);
	return 0;
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

void cmdq_mbox_check_buffer(struct mbox_chan *chan,
	struct cmdq_pkt_buffer *buffer)
{
	struct cmdq *cmdq = container_of(chan->mbox, typeof(*cmdq), mbox);
	bool err = false;
	s32 i;

	for (i = 0; i < ARRAY_SIZE(cmdq->thread) && !err; i++) {
		struct cmdq_thread *thread = &cmdq->thread[i];
		struct cmdq_task *task;
		struct cmdq_pkt_buffer *buf;
		unsigned long flags;

		if (!thread->occupied && !thread->chan)
			continue;

		spin_lock_irqsave(&thread->chan->lock, flags);
		list_for_each_entry(task, &thread->task_busy_list, list_entry) {
			list_for_each_entry(buf, &task->pkt->buf, list_entry) {
				if (CMDQ_BUF_ADDR(buffer) ==
					CMDQ_BUF_ADDR(buf)) {
					cmdq_util_err(
						"hwid:%hu thread:%u cur:%p va:%p iova:%pa pa:%pa alloc_time:%llu",
						cmdq->hwid, thread->idx, buffer,
						buffer->va_base,
						&buffer->iova_base,
						&buffer->pa_base,
						buffer->alloc_time);
					cmdq_util_err(
						"hwid:%hu thread:%u buf:%p va:%p iova:%pa pa:%pa alloc_time:%llu allocated",
						cmdq->hwid, thread->idx, buf,
						buf->va_base, &buf->iova_base,
						&buf->pa_base, buf->alloc_time);
					err = true;
					break;
				}
			}
			if (err)
				break;
		}
		spin_unlock_irqrestore(&thread->chan->lock, flags);
	}

}
EXPORT_SYMBOL(cmdq_mbox_check_buffer);

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
EXPORT_SYMBOL(cmdq_task_get_thread_pc);

s32 cmdq_task_get_thread_irq(struct mbox_chan *chan, u32 *irq_out)
{
	struct cmdq_thread *thread;

	if (!irq_out || !chan)
		return -EINVAL;

	thread = chan->con_priv;
	*irq_out = readl(thread->base + CMDQ_THR_IRQ_STATUS);

	return 0;
}
EXPORT_SYMBOL(cmdq_task_get_thread_irq);

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
EXPORT_SYMBOL(cmdq_task_get_pkt_from_thread);

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

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
void cmdq_controller_set_fp(struct cmdq_util_controller_fp *cust_cmdq_util)
{
	cmdq_util_controller = cust_cmdq_util;
}
EXPORT_SYMBOL(cmdq_controller_set_fp);
#endif

void cmdq_set_outpin_event(struct cmdq_client *cl, bool ena)
{
	struct cmdq *cmdq = container_of(cl->chan->mbox, struct cmdq, mbox);

	cmdq->outpin_en = ena;
}
EXPORT_SYMBOL(cmdq_set_outpin_event);

module_init(cmdq_drv_init);

MODULE_LICENSE("GPL v2");
