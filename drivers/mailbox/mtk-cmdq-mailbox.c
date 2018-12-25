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

/* ddp main/sub, mdp path 0/1/2/3, general(misc) */
#define CMDQ_THR_MAX_COUNT		24
#define CMDQ_OP_CODE_MASK		(0xff << CMDQ_OP_CODE_SHIFT)
#define CMDQ_TIMEOUT_MS			1000
#define CMDQ_IRQ_MASK			0xffff
#define CMDQ_NUM_CMD(t)			(t->cmd_buf_size / CMDQ_INST_SIZE)

#define CMDQ_CURR_IRQ_STATUS		0x10
#define CMDQ_THR_SLOT_CYCLES		0x30

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
#define CMDQ_THR_WAIT_TOKEN		0x30
#define CMDQ_THR_CFG			0x40
#define CMDQ_THR_CNT			0x128
#define CMDQ_THR_SPR			0x160

#define CMDQ_THR_ENABLED		0x1
#define CMDQ_THR_DISABLED		0x0
#define CMDQ_THR_SUSPEND		0x1
#define CMDQ_THR_RESUME			0x0
#define CMDQ_THR_STATUS_SUSPENDED	BIT(1)
#define CMDQ_THR_DO_WARM_RESET		BIT(0)
#define CMDQ_THR_ACTIVE_SLOT_CYCLES	0x3200
#define CMDQ_THR_IRQ_DONE		0x1
#define CMDQ_THR_IRQ_ERROR		0x12
#define CMDQ_THR_IRQ_EN			(CMDQ_THR_IRQ_ERROR | CMDQ_THR_IRQ_DONE)
#define CMDQ_THR_IS_WAITING		BIT(31)
#define CMDQ_THR_PRIORITY		0x7


#define CMDQ_JUMP_BY_OFFSET		0x10000000
#define CMDQ_JUMP_BY_PA			0x10000001

#define CMDQ_MIN_AGE_VALUE		(5)

#define CMDQ_DRIVER_NAME		"mtk_cmdq_mbox"

/* CMDQ log flag */
int mtk_cmdq_log;
EXPORT_SYMBOL(mtk_cmdq_log);

int mtk_cmdq_msg;
EXPORT_SYMBOL(mtk_cmdq_msg);

int mtk_cmdq_err = 1;
EXPORT_SYMBOL(mtk_cmdq_err);
module_param(mtk_cmdq_log, int, 0644);

struct cmdq_thread {
	struct mbox_chan	*chan;
	void __iomem		*base;
	struct list_head	task_busy_list;
	struct timer_list	timeout;
	bool			atomic_exec;
	u32			idx;
	struct work_struct	timeout_work;
	bool			dirty;
};

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
	u32			irq;
	struct workqueue_struct	*buf_dump_wq;
	struct cmdq_thread	thread[CMDQ_THR_MAX_COUNT];
	struct clk		*clock;
	bool			suspended;
	atomic_t		usage;
	struct workqueue_struct *timeout_wq;
};

static s32 cmdq_clk_enable(struct cmdq *cmdq)
{
	s32 usage, err;

	usage = atomic_inc_return(&cmdq->usage);
	err = clk_enable(cmdq->clock);
	if (usage <= 0 || err < 0)
		cmdq_err("ref count error after inc:%d err:%d", usage, err);
	else if (usage == 1)
		cmdq_log("cmdq begin mbox");
	return err;
}

static void cmdq_clk_disable(struct cmdq *cmdq)
{
	s32 usage;

	usage = atomic_dec_return(&cmdq->usage);
	clk_disable(cmdq->clock);
	if (usage < 0)
		cmdq_err("ref count error after dec:%d", usage);
	else if (usage == 0)
		cmdq_log("cmdq shutdown mbox");
}

static int cmdq_thread_suspend(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 status;

	writel(CMDQ_THR_SUSPEND, thread->base + CMDQ_THR_SUSPEND_TASK);

	/* If already disabled, treat as suspended successful. */
	if (!(readl(thread->base + CMDQ_THR_ENABLE_TASK) & CMDQ_THR_ENABLED))
		return 0;

	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_CURR_STATUS,
			status, status & CMDQ_THR_STATUS_SUSPENDED, 0, 10)) {
		cmdq_err("suspend GCE thread 0x%x failed",
			(u32)(thread->base - cmdq->base));
		return -EFAULT;
	}

	return 0;
}

static void cmdq_thread_resume(struct cmdq_thread *thread)
{
	writel(CMDQ_THR_RESUME, thread->base + CMDQ_THR_SUSPEND_TASK);
}

static int cmdq_thread_reset(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 warm_reset;

	writel(CMDQ_THR_DO_WARM_RESET, thread->base + CMDQ_THR_WARM_RESET);
	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_WARM_RESET,
			warm_reset, !(warm_reset & CMDQ_THR_DO_WARM_RESET),
			0, 10)) {
		cmdq_err("reset GCE thread 0x%x failed",
			(u32)(thread->base - cmdq->base));
		return -EFAULT;
	}
	writel(CMDQ_THR_ACTIVE_SLOT_CYCLES, cmdq->base + CMDQ_THR_SLOT_CYCLES);
	return 0;
}

static void cmdq_thread_err_reset(struct cmdq *cmdq, struct cmdq_thread *thread,
	u32 pc, u32 thd_pri)
{
	u32 i, end, spr[4], cookie;

	for (i = 0; i < 4; i++)
		spr[i] = readl(thread->base + CMDQ_THR_SPR + i * 4);
	end = readl(thread->base + CMDQ_THR_END_ADDR);
	cookie = readl(thread->base + CMDQ_THR_CNT);

	cmdq_msg(
		"reset backup pc:0x%08x end:0x%08x cookie:0x%08x spr:0x%x 0x%x 0x%x 0x%x",
		pc, end, cookie, spr[0], spr[1], spr[2], spr[3]);
	WARN_ON(cmdq_thread_reset(cmdq, thread) < 0);

	for (i = 0; i < 4; i++)
		writel(spr[i], thread->base + CMDQ_THR_SPR + i * 4);
	writel(pc, thread->base + CMDQ_THR_CURR_ADDR);
	writel(end, thread->base + CMDQ_THR_END_ADDR);
	writel(cookie, thread->base + CMDQ_THR_CNT);
	writel(thd_pri, thread->base + CMDQ_THR_CFG);
	writel(CMDQ_THR_IRQ_EN, thread->base + CMDQ_THR_IRQ_ENABLE);
	writel(CMDQ_THR_ENABLED, thread->base + CMDQ_THR_ENABLE_TASK);
}

static void cmdq_thread_disable(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	cmdq_thread_reset(cmdq, thread);
	writel(CMDQ_THR_DISABLED, thread->base + CMDQ_THR_ENABLE_TASK);
}

/* notify GCE to re-fetch commands by setting GCE thread PC */
static void cmdq_thread_invalidate_fetched_data(struct cmdq_thread *thread)
{
	writel(readl(thread->base + CMDQ_THR_CURR_ADDR),
	       thread->base + CMDQ_THR_CURR_ADDR);
}

static void cmdq_task_connect_buffer(struct cmdq_task *task,
	struct cmdq_task *next_task)
{
	u64 *task_base;
#ifdef CMDQ_MEMORY_JUMP
	struct cmdq_pkt_buffer *buf;
	u64 inst;

	/* let previous task jump to this task */
	buf = list_last_entry(&task->pkt->buf, typeof(*buf), list_entry);
	task_base = (u64 *)(buf->va_base + CMDQ_CMD_BUFFER_SIZE -
		task->pkt->avail_buf_size - CMDQ_INST_SIZE);
	inst = *task_base;
	*task_base = (u64)CMDQ_JUMP_BY_PA << 32 | next_task->pa_base;
	cmdq_log("change last inst 0x%016llx to 0x%016llx connect 0x%p -> 0x%p",
		inst, *task_base, task->pkt, next_task->pkt);
#else
	struct device *dev = task->cmdq->mbox.dev;

	/* let previous task jump to this task */
	dma_sync_single_for_cpu(dev, task->pa_base,
		task->pkt->cmd_buf_size, DMA_TO_DEVICE);
	task_base = task->pkt->va_base;
	task_base[CMDQ_NUM_CMD(task->pkt) - 1] =
		(u64)CMDQ_JUMP_BY_PA << 32 | next_task->pa_base;
	dma_sync_single_for_device(dev, task->pa_base,
		task->pkt->cmd_buf_size, DMA_TO_DEVICE);
#endif
}

#ifdef CMDQ_MEMORY_JUMP
static bool cmdq_task_is_current_run(unsigned long pa, struct cmdq_pkt *pkt)
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
			return true;
		}
	}

	return false;
}
#endif

static void cmdq_task_insert_into_thread(unsigned long curr_pa,
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
#ifdef CMDQ_MEMORY_JUMP
		if (cmdq_task_is_current_run(curr_pa, prev_task->pkt))
			break;
#else
		if (curr_pa >= prev_task->pa_base &&
			curr_pa < prev_task->pa_base +
			prev_task->pkt->cmd_buf_size)
			break;
#endif
		/* stop if new task priority lower than this one */
		if (prev_task->pkt->priority >= task->pkt->priority)
			break;
		next_task = prev_task;
	}

	*insert_pos = &prev_task->list_entry;
	cmdq_task_connect_buffer(prev_task, task);
	if (next_task && next_task != prev_task) {
		cmdq_msg("reorder pkt:0x%p(%u) next pkt:0x%p(%u) pc:0x%08lx",
			task->pkt, task->pkt->priority,
			next_task->pkt, next_task->pkt->priority, curr_pa);
		cmdq_task_connect_buffer(task, next_task);
	}

	cmdq_thread_invalidate_fetched_data(thread);
}

static bool cmdq_command_is_wfe(u64 cmd)
{
	u64 wfe_option = CMDQ_WFE_UPDATE | CMDQ_WFE_WAIT | CMDQ_WFE_WAIT_VALUE;
	u64 wfe_op = (u64)(CMDQ_CODE_WFE << CMDQ_OP_CODE_SHIFT) << 32;
	u64 wfe_mask = (u64)CMDQ_OP_CODE_MASK << 32 | 0xffffffff;

	return ((cmd & wfe_mask) == (wfe_op | wfe_option));
}

/* we assume tasks in the same display GCE thread are waiting the same event. */
static void cmdq_task_remove_wfe(struct cmdq_task *task)
{
	struct device *dev = task->cmdq->mbox.dev;
#ifdef CMDQ_MEMORY_JUMP
	u64 *base;
	int i;
	struct cmdq_pkt_buffer *buf;
	u32 cmd_cnt;

	list_for_each_entry(buf, &task->pkt->buf, list_entry) {
		base = buf->va_base;
		if (list_is_last(&buf->list_entry, &task->pkt->buf))
			cmd_cnt = (CMDQ_CMD_BUFFER_SIZE -
				task->pkt->avail_buf_size) / CMDQ_INST_SIZE;
		else
			cmd_cnt = CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE;
		dma_sync_single_for_cpu(dev, buf->pa_base,
			CMDQ_CMD_BUFFER_SIZE, DMA_TO_DEVICE);
		for (i = 0; i < cmd_cnt; i++)
			if (cmdq_command_is_wfe(base[i]))
				base[i] = (u64)CMDQ_JUMP_BY_OFFSET << 32 |
					CMDQ_JUMP_PASS;
		dma_sync_single_for_device(dev, buf->pa_base,
			CMDQ_CMD_BUFFER_SIZE, DMA_TO_DEVICE);
	}
#else
	u64 *base = task->pkt->va_base;
	int i;

	dma_sync_single_for_cpu(dev, task->pa_base, task->pkt->cmd_buf_size,
				DMA_TO_DEVICE);
	for (i = 0; i < CMDQ_NUM_CMD(task->pkt); i++)
		if (cmdq_command_is_wfe(base[i]))
			base[i] = (u64)CMDQ_JUMP_BY_OFFSET << 32 |
				  CMDQ_JUMP_PASS;
	dma_sync_single_for_device(dev, task->pa_base, task->pkt->cmd_buf_size,
				   DMA_TO_DEVICE);
#endif
}

static bool cmdq_thread_is_in_wfe(struct cmdq_thread *thread)
{
	return readl(thread->base + CMDQ_THR_WAIT_TOKEN) & CMDQ_THR_IS_WAITING;
}

static void cmdq_thread_wait_end(struct cmdq_thread *thread,
				 unsigned long end_pa)
{
	unsigned long curr_pa;

	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_CURR_ADDR,
			curr_pa, curr_pa == end_pa, 1, 20))
		cmdq_err("GCE thread cannot run to end.");
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
		cmdq_cb_data.data = pkt->cb.data;
		pkt->err_cb.cb(cmdq_cb_data);
	}
}

#ifdef CMDQ_MEMORY_JUMP
void cmdq_task_unmap_dma(struct device *dev, struct cmdq_pkt *pkt)
{
	/* we use memory pool thus no need to umnap */
}

dma_addr_t cmdq_task_map_dma(struct device *dev, struct cmdq_pkt *pkt)
{
	dma_addr_t dma_handle;
	struct cmdq_pkt_buffer *buf, *prev_buf = NULL;
	u64 *va;

	if (list_empty(&pkt->buf))
		return 0;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (!buf->pa_base) {
			dma_handle = dma_map_single(dev, buf->va_base,
				CMDQ_CMD_BUFFER_SIZE, DMA_TO_DEVICE);
			if (dma_mapping_error(dev, dma_handle)) {
				cmdq_task_unmap_dma(dev, pkt);
				return 0;
			}
			buf->pa_base = dma_handle;
		}
		if (prev_buf) {
			va = (u64 *)(prev_buf->va_base + CMDQ_CMD_BUFFER_SIZE);
			va[-1] = (u64)((CMDQ_CODE_JUMP << CMDQ_OP_CODE_SHIFT) |
				1) << 32 | (u32)buf->pa_base;
		}
		prev_buf = buf;
	}

	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	return buf->pa_base;
}
#else
dma_addr_t cmdq_task_map_dma(struct device *dev, struct cmdq_pkt *pkt)
{
	dma_addr_t dma_handle;

	dma_handle = dma_map_single(dev, pkt->va_base, pkt->cmd_buf_size,
		DMA_TO_DEVICE);
	if (dma_mapping_error(dev, dma_handle))
		return 0;

	return dma_handle;
}
#endif
EXPORT_SYMBOL(cmdq_task_map_dma);

#ifdef CMDQ_MEMORY_JUMP
static dma_addr_t cmdq_task_get_end_pa(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;

	/* let previous task jump to this task */
	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	return buf->pa_base + CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
}
#endif

static void cmdq_task_exec(struct cmdq_pkt *pkt, struct cmdq_thread *thread)
{
	struct cmdq *cmdq;
	struct cmdq_task *task, *last_task;
	unsigned long curr_pa, end_pa;
	dma_addr_t dma_handle;
	struct list_head *insert_pos;

	cmdq = dev_get_drvdata(thread->chan->mbox->dev);

	/* Client should not flush new tasks if suspended. */
	WARN_ON(cmdq->suspended);

#ifdef CMDQ_MEMORY_JUMP
	dma_handle = cmdq_task_map_dma(cmdq->mbox.dev, pkt);
#else
	if (pkt->pa_base) {
		/* dma map before send directly */
		dma_handle = pkt->pa_base;
	} else {
		dma_handle = cmdq_task_map_dma(cmdq->mbox.dev, pkt);
		if (!dma_handle) {
			cmdq_err("dma map failed");
			cmdq_task_callback(pkt, -EINVAL);
			return;
		}
	}
#endif

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
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

		writel(task->pa_base, thread->base + CMDQ_THR_CURR_ADDR);
#ifdef CMDQ_MEMORY_JUMP
		writel(cmdq_task_get_end_pa(pkt),
			thread->base + CMDQ_THR_END_ADDR);
		cmdq_log("set pc:0x%08x end:0x%08x pkt:0x%p",
			(u32)task->pa_base,
			(u32)cmdq_task_get_end_pa(pkt),
			pkt);
#else
		writel(task->pa_base + pkt->cmd_buf_size,
		       thread->base + CMDQ_THR_END_ADDR);
#endif
		writel(pkt->hw_priority & CMDQ_THR_PRIORITY,
			thread->base + CMDQ_THR_CFG);
		writel(CMDQ_THR_IRQ_EN, thread->base + CMDQ_THR_IRQ_ENABLE);
		writel(CMDQ_THR_ENABLED, thread->base + CMDQ_THR_ENABLE_TASK);

		if (pkt->timeout)
			mod_timer(&thread->timeout,
				jiffies + msecs_to_jiffies(pkt->timeout));
		list_move_tail(&task->list_entry, &thread->task_busy_list);
	} else {
		WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);
		curr_pa = readl(thread->base + CMDQ_THR_CURR_ADDR);
		end_pa = readl(thread->base + CMDQ_THR_END_ADDR);

		cmdq_log("curr task %p~%p, thread->base=%p thread:%u",
			(void *)curr_pa, (void *)end_pa, thread->base,
			thread->idx);

		/*
		 * Atomic execution should remove the following wfe, i.e. only
		 * wait event at first task, and prevent to pause when running.
		 */
		if (thread->atomic_exec) {
			/* GCE is executing if command is not WFE */
			if (!cmdq_thread_is_in_wfe(thread)) {
				cmdq_thread_resume(thread);
				cmdq_thread_wait_end(thread, end_pa);
				WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);
				/* set to this task directly */
				writel(task->pa_base,
				       thread->base + CMDQ_THR_CURR_ADDR);
				insert_pos = &thread->task_busy_list;
			} else {
				cmdq_task_insert_into_thread(curr_pa, task,
					&insert_pos);
				cmdq_task_remove_wfe(task);
				smp_mb(); /* modify jump before enable thread */
			}
		} else {
			/* check boundary */
			if (curr_pa == end_pa - CMDQ_INST_SIZE ||
			    curr_pa == end_pa) {
				/* set to this task directly */
				writel(task->pa_base,
				       thread->base + CMDQ_THR_CURR_ADDR);
				last_task = list_last_entry(
					&thread->task_busy_list,
					typeof(*task), list_entry);
				insert_pos = &last_task->list_entry;
				cmdq_log("set pc:%pa pkt:0x%p",
					&task->pa_base, task->pkt);
			} else {
				cmdq_task_insert_into_thread(curr_pa, task,
					&insert_pos);
				smp_mb(); /* modify jump before enable thread */
			}
		}
		list_add(&task->list_entry, insert_pos);
		last_task = list_last_entry(&thread->task_busy_list,
			typeof(*task), list_entry);
#ifdef CMDQ_MEMORY_JUMP
		writel(cmdq_task_get_end_pa(last_task->pkt),
			thread->base + CMDQ_THR_END_ADDR);
		cmdq_log("set end:0x%08x pkt:0x%p",
			(u32)cmdq_task_get_end_pa(last_task->pkt),
			last_task->pkt);
#else
		writel(last_task->pa_base + last_task->pkt->cmd_buf_size,
		       thread->base + CMDQ_THR_END_ADDR);
#endif

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
	struct device *dev = task->cmdq->mbox.dev;

	cmdq_task_callback(task->pkt, err);
#ifdef CMDQ_MEMORY_JUMP
	cmdq_task_unmap_dma(dev, task->pkt);
#else
	dma_unmap_single(dev, task->pa_base, task->pkt->cmd_buf_size,
		DMA_TO_DEVICE);
#endif

	cmdq_log("pkt:0x%p done err:%d", task->pkt, err);
	list_del(&task->list_entry);
}

#ifdef CMDQ_MEMORY_JUMP
#else
static void cmdq_buf_print_write(struct device *dev, u32 offset, u64 cmd)
{
	u32 subsys, addr_base, addr, value;

	subsys = ((u32)(cmd >> (32 + CMDQ_SUBSYS_SHIFT)) & 0x1f);
	addr_base = cmdq_subsys_id_to_base(subsys);
	addr = ((u32)(cmd >> 32) & 0xffff) | (addr_base << CMDQ_SUBSYS_SHIFT);
	value = (u32)(cmd & 0xffffffff);
	cmdq_err("0x%08x 0x%016llx write register 0x%08x with value 0x%08x",
		offset, cmd, addr, value);
}

static void cmdq_buf_print_wfe(struct device *dev, u32 offset, u64 cmd)
{
	u32 event = (u32)(cmd >> 32) & 0x1ff;
	char *event_str;

	/* Display start of frame(SOF) events */
	if (event == cmdq_event_value[CMDQ_EVENT_DISP_OVL0_SOF])
		event_str = "CMDQ_EVENT_DISP_OVL0_SOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_OVL1_SOF])
		event_str = "CMDQ_EVENT_DISP_OVL1_SOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA0_SOF])
		event_str = "CMDQ_EVENT_DISP_RDMA0_SOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA1_SOF])
		event_str = "CMDQ_EVENT_DISP_RDMA1_SOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA2_SOF])
		event_str = "CMDQ_EVENT_DISP_RDMA2_SOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_WDMA0_SOF])
		event_str = "CMDQ_EVENT_DISP_WDMA0_SOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_WDMA1_SOF])
		event_str = "CMDQ_EVENT_DISP_WDMA1_SOF";
	/* Display end of frame(EOF) events */
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_OVL0_EOF])
		event_str = "CMDQ_EVENT_DISP_OVL0_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_OVL1_EOF])
		event_str = "CMDQ_EVENT_DISP_OVL1_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA0_EOF])
		event_str = "CMDQ_EVENT_DISP_RDMA0_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA1_EOF])
		event_str = "CMDQ_EVENT_DISP_RDMA1_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA2_EOF])
		event_str = "CMDQ_EVENT_DISP_RDMA2_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_WDMA0_EOF])
		event_str = "CMDQ_EVENT_DISP_WDMA0_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_WDMA1_EOF])
		event_str = "CMDQ_EVENT_DISP_WDMA1_EOF";
	/* Mutex end of frame(EOF) events */
	else if (event == cmdq_event_value[CMDQ_EVENT_MUTEX0_STREAM_EOF])
		event_str = "CMDQ_EVENT_MUTEX0_STREAM_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MUTEX1_STREAM_EOF])
		event_str = "CMDQ_EVENT_MUTEX1_STREAM_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MUTEX2_STREAM_EOF])
		event_str = "CMDQ_EVENT_MUTEX2_STREAM_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MUTEX3_STREAM_EOF])
		event_str = "CMDQ_EVENT_MUTEX3_STREAM_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MUTEX4_STREAM_EOF])
		event_str = "CMDQ_EVENT_MUTEX4_STREAM_EOF";
	/* Display underrun events */
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA0_UNDERRUN])
		event_str = "CMDQ_EVENT_DISP_RDMA0_UNDERRUN";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA1_UNDERRUN])
		event_str = "CMDQ_EVENT_DISP_RDMA1_UNDERRUN";
	else if (event == cmdq_event_value[CMDQ_EVENT_DISP_RDMA2_UNDERRUN])
		event_str = "CMDQ_EVENT_DISP_RDMA2_UNDERRUN";
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_RDMA0_EOF])
		event_str = "CMDQ_EVENT_MDP_RDMA0_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_RDMA1_EOF])
		event_str = "CMDQ_EVENT_MDP_RDMA1_EOF";
#if 0
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_RDMA2_EOF])
		event_str = "CMDQ_EVENT_MDP_RDMA2_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_RDMA3_EOF])
		event_str = "CMDQ_EVENT_MDP_RDMA3_EOF";
#endif
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_WDMA_EOF])
		event_str = "CMDQ_EVENT_MDP_WDMA_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_WROT0_W_EOF])
		event_str = "CMDQ_EVENT_MDP_WROT0_W_EOF";
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_WROT1_W_EOF])
		event_str = "CMDQ_EVENT_MDP_WROT1_W_EOF";
#if 0
	else if (event == cmdq_event_value[CMDQ_EVENT_MDP_WROT2_W_EOF])
		event_str = "CMDQ_EVENT_MDP_WROT2_W_EOF";
#endif
	else
		event_str = "UNKNOWN";

	cmdq_err("0x%08x 0x%016llx %s event %d:%s",
		offset, cmd, cmdq_command_is_wfe(cmd) ?
		"wait for" : "clear", event, event_str);
}

static void cmdq_buf_print_mask(struct device *dev, u32 offset, u64 cmd)
{
	u32 arg_b = (u32)(cmd & 0xffffffff);

	cmdq_err("0x%08x 0x%016llx mask 0x%08x",
		offset, cmd, ~arg_b);
}

static void cmdq_buf_print_misc(struct device *dev, u32 offset, u64 cmd)
{
	enum cmdq_code op = (enum cmdq_code)(cmd >> (32 + CMDQ_OP_CODE_SHIFT));
	char *cmd_str;

	switch (op) {
	case CMDQ_CODE_JUMP:
		cmd_str = "jump";
		break;
	case CMDQ_CODE_EOC:
		cmd_str = "eoc";
		break;
	default:
		cmd_str = "unknown";
		break;
	}

	cmdq_err("0x%08x 0x%016llx %s", offset, cmd, cmd_str);
}

static void cmdq_buf_dump_work(struct work_struct *work_item)
{
	struct cmdq_buf_dump *buf_dump = container_of(work_item,
			struct cmdq_buf_dump, dump_work);
	struct device *dev = buf_dump->cmdq->mbox.dev;
	u64 *buf = buf_dump->cmd_buf;
	enum cmdq_code op;
	u32 i, offset = 0;

	cmdq_err("dump %s task start ----------",
		buf_dump->timeout ? "timeout" : "error");
	cmdq_err("pa_curr - pa_base = 0x%x", buf_dump->pa_offset);
	for (i = 0; i < CMDQ_NUM_CMD(buf_dump); i++) {
		op = (enum cmdq_code)(buf[i] >> (32 + CMDQ_OP_CODE_SHIFT));
		switch (op) {
		case CMDQ_CODE_WRITE:
			cmdq_buf_print_write(dev, offset, buf[i]);
			break;
		case CMDQ_CODE_WFE:
			cmdq_buf_print_wfe(dev, offset, buf[i]);
			break;
		case CMDQ_CODE_MASK:
			cmdq_buf_print_mask(dev, offset, buf[i]);
			break;
		default:
			cmdq_buf_print_misc(dev, offset, buf[i]);
			break;
		}
		offset += CMDQ_INST_SIZE;
	}
	cmdq_err("dump %s task end   ----------",
		buf_dump->timeout ? "timeout" : "error");

	kfree(buf_dump->cmd_buf);
	kfree(buf_dump);
}
#endif

static void cmdq_buf_dump_schedule(struct cmdq_task *task, bool timeout,
				   u32 pa_curr)
{
#ifdef CMDQ_MEMORY_JUMP
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
#else
	struct device *dev = task->cmdq->mbox.dev;
	struct cmdq_buf_dump *buf_dump;
	u64 *inst = (u64 *)(task->pkt->va_base + (pa_curr - task->pa_base));

	cmdq_err("task:0x%p timeout:%s pkt:0x%p size:%zu pc:0x%08x inst:0x%016llx",
		task, timeout ? "true" : "false", task->pkt,
		task->pkt->cmd_buf_size, pa_curr,
		*inst);

	return;

	buf_dump = kmalloc(sizeof(*buf_dump), GFP_ATOMIC);
	buf_dump->cmdq = task->cmdq;
	buf_dump->timeout = timeout;
	buf_dump->cmd_buf = kmalloc(task->pkt->cmd_buf_size, GFP_ATOMIC);
	buf_dump->cmd_buf_size = task->pkt->cmd_buf_size;
	buf_dump->pa_offset = pa_curr - task->pa_base;
	dma_sync_single_for_cpu(dev, task->pa_base,
				task->pkt->cmd_buf_size, DMA_TO_DEVICE);
	memcpy(buf_dump->cmd_buf, task->pkt->va_base, task->pkt->cmd_buf_size);
	dma_sync_single_for_device(dev, task->pa_base,
				   task->pkt->cmd_buf_size, DMA_TO_DEVICE);
	INIT_WORK(&buf_dump->dump_work, cmdq_buf_dump_work);
	queue_work(task->cmdq->buf_dump_wq, &buf_dump->dump_work);
#endif
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
		writel(next_task->pa_base, thread->base + CMDQ_THR_CURR_ADDR);
	cmdq_thread_resume(thread);
}

static void cmdq_thread_irq_handler(struct cmdq *cmdq,
				    struct cmdq_thread *thread)
{
	struct cmdq_task *task, *tmp, *curr_task = NULL;
	u32 curr_pa, irq_flag, task_end_pa;
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

	curr_pa = readl(thread->base + CMDQ_THR_CURR_ADDR);
	task_end_pa = readl(thread->base + CMDQ_THR_END_ADDR);

	if (err < 0)
		cmdq_err("pc:0x%08x end:0x%08x err:%d",
			curr_pa, task_end_pa, err);

	cmdq_log("task status %p~%p err:%d",
		(void *)(unsigned long)curr_pa,
		(void *)(unsigned long)task_end_pa, err);

	task = list_first_entry_or_null(&thread->task_busy_list,
		struct cmdq_task, list_entry);
	if (task && !task->pkt->timeout) {
		cmdq_log("task loop %pa", &task->pa_base);
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
#ifdef CMDQ_MEMORY_JUMP
		task_end_pa = cmdq_task_get_end_pa(task->pkt);
		if (cmdq_task_is_current_run(curr_pa, task->pkt))
			curr_task = task;
#else
		task_end_pa = task->pa_base + task->pkt->cmd_buf_size;

		cmdq_log("task %pa~0x%x",
			&task->pa_base,
			task_end_pa);

		if (curr_pa >= task->pa_base && curr_pa < task_end_pa)
			curr_task = task;
#endif

		if (!curr_task || curr_pa == task_end_pa - CMDQ_INST_SIZE) {
			if (curr_task && (curr_pa != task_end_pa)) {
				cmdq_err("remove task that not ending pkt:0x%p 0x%08x:0x%08x",
					curr_task->pkt, curr_pa, task_end_pa);
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
		mod_timer(&thread->timeout,
			  jiffies + msecs_to_jiffies(task->pkt->timeout));
		cmdq_log("mod_timer pkt:0x%p timeout:%u thread:%u",
			task->pkt, task->pkt->timeout, thread->idx);
	}
}

static irqreturn_t cmdq_irq_handler(int irq, void *dev)
{
	struct cmdq *cmdq = dev;
	unsigned long irq_status, flags = 0L;
	int bit;

	if (atomic_read(&cmdq->usage) <= 0) {
		cmdq_msg("cmdq not enable");
		return IRQ_NONE;
	}

	irq_status = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS) & CMDQ_IRQ_MASK;
	cmdq_log("CMDQ_CURR_IRQ_STATUS: %x, %x",
		(u32)irq_status, (u32)(irq_status ^ CMDQ_IRQ_MASK));
	if (!(irq_status ^ CMDQ_IRQ_MASK))
		return IRQ_NONE;

	for_each_clear_bit(bit, &irq_status, fls(CMDQ_IRQ_MASK)) {
		struct cmdq_thread *thread = &cmdq->thread[bit];

		cmdq_log("bit=%d, thread->base=%p", bit, thread->base);
		spin_lock_irqsave(&thread->chan->lock, flags);
		cmdq_thread_irq_handler(cmdq, thread);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
	}
	return IRQ_HANDLED;
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
	if (duration < task->pkt->timeout) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(task->pkt->timeout - duration));
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

	pa_curr = readl(thread->base + CMDQ_THR_CURR_ADDR);

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
		list_entry) {
#ifdef CMDQ_MEMORY_JUMP
		bool curr_task = cmdq_task_is_current_run(pa_curr, task->pkt);
#else
		bool curr_task = pa_curr >= task->pa_base &&
			pa_curr < task->pa_base + task->pkt->cmd_buf_size;
#endif

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
			msecs_to_jiffies(task->pkt->timeout));
		cmdq_thread_err_reset(cmdq, thread,
			task->pa_base, task->pkt->hw_priority);
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
		cmdq_msg("queue cmdq timeout thread:%u", thread->idx);
		queue_work(cmdq->timeout_wq, &thread->timeout_work);
	} else {
		cmdq_msg("ignore cmdq timeout thread:%u", thread->idx);
	}
}

void cmdq_thread_remove_task(struct mbox_chan *chan,
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

	pa_curr = readl(thread->base + CMDQ_THR_CURR_ADDR);

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

		writel(task->pa_base, thread->base + CMDQ_THR_CURR_ADDR);
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(task->pkt->timeout));
	}

	if (last_task) {
		/* reset end addr again if remove last task */
		task = list_last_entry(&thread->task_busy_list,
			typeof(*task), list_entry);
		writel(cmdq_task_get_end_pa(task->pkt),
			thread->base + CMDQ_THR_END_ADDR);
	}

	cmdq_thread_resume(thread);

	spin_unlock_irqrestore(&thread->chan->lock, flags);
}
EXPORT_SYMBOL(cmdq_thread_remove_task);

static void cmdq_thread_stop(struct cmdq_thread *thread)
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
			cmdq_thread_stop(thread);
			task_running = true;
			break;
		}
	}

	if (task_running) {
		dev_warn(dev, "exist running task(s) in suspend\n");
		schedule();
	}

	clk_unprepare(cmdq->clock);
	return 0;
}

static int cmdq_resume(struct device *dev)
{
	struct cmdq *cmdq = dev_get_drvdata(dev);

	WARN_ON(clk_prepare(cmdq->clock) < 0);
	cmdq->suspended = false;
	return 0;
}

static int cmdq_remove(struct platform_device *pdev)
{
	struct cmdq *cmdq = platform_get_drvdata(pdev);

	destroy_workqueue(cmdq->buf_dump_wq);
	mbox_controller_unregister(&cmdq->mbox);
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
	return 0;
}

static void cmdq_mbox_shutdown(struct mbox_chan *chan)
{
	cmdq_thread_stop(chan->con_priv);
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
	thread->atomic_exec = (sp->args[1] != 0);
	thread->chan = &mbox->chans[ind];

	return &mbox->chans[ind];
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

	cmdq->clock = devm_clk_get(dev, "gce");
	if (IS_ERR(cmdq->clock)) {
		cmdq_err("failed to get gce clk");
		return PTR_ERR(cmdq->clock);
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

	cmdq_event_value = of_device_get_match_data(dev);

	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
};

static const struct of_device_id cmdq_of_ids[] = {
#if 0
	{.compatible = "mediatek,mt8173-gce", .data = cmdq_event_value_8173},
	{.compatible = "mediatek,mt2712-gce", .data = cmdq_event_value_2712},
#endif
	{.compatible = "mediatek,mailbox-gce", .data = cmdq_event_value_common},
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

void cmdq_mbox_channel_stop(void *chan)
{
	cmdq_thread_stop(((struct mbox_chan *)chan)->con_priv);
}
EXPORT_SYMBOL(cmdq_mbox_channel_stop);

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


#ifdef CMDQ_MEMORY_JUMP
#else
s32 cmdq_mbox_get_task_pa(const struct cmdq_pkt *pkt, struct mbox_chan *chan,
	dma_addr_t *pa_out)
{
	struct cmdq_thread *thread;
	s32 ret;
	unsigned long flags;

	if (!pkt || !chan)
		return -EINVAL;

	thread = chan->con_priv;

	spin_lock_irqsave(&thread->chan->lock, flags);
	ret = cmdq_mbox_get_task_pa_unlock(pkt, chan, pa_out);
	spin_unlock_irqrestore(&thread->chan->lock, flags);

	return ret;
}

s32 cmdq_mbox_get_task_pa_unlock(const struct cmdq_pkt *pkt,
	struct mbox_chan *chan, dma_addr_t *pa_out)
{
	struct cmdq_thread *thread = NULL;
	struct cmdq_task *task;
	dma_addr_t temp_pa = 0;

	if (!pkt || !chan)
		return -EINVAL;

	thread = chan->con_priv;

	if (list_empty(&thread->task_busy_list))
		return -EINVAL;

	list_for_each_entry(task, &thread->task_busy_list, list_entry) {
		if (task->pkt == pkt) {
			temp_pa = task->pa_base;
			break;
		}
	}

	*pa_out = temp_pa;

	return temp_pa ? 0 : -EINVAL;
}
#endif

s32 cmdq_task_get_thread_pc(struct mbox_chan *chan, dma_addr_t *pc_out)
{
	struct cmdq_thread *thread;
	dma_addr_t pc = 0;

	if (!pc_out || !chan)
		return -EINVAL;

	thread = chan->con_priv;
	pc = readl(thread->base + CMDQ_THR_CURR_ADDR);

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
	*end_addr_out = readl(thread->base + CMDQ_THR_END_ADDR);

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

arch_initcall(cmdq_init);
