// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/preempt.h>
#include <linux/sched/clock.h>

#include <apusys_device.h>

#include <common/mdla_device.h>
#include <common/mdla_scheduler.h>

#include <utilities/mdla_util.h>
#include <utilities/mdla_debug.h>

#include "mdla_hw_reg_v2_0.h"


struct command_batch {
	struct list_head node;
	u32 index;
	u32 size;
};

static inline u32 mdla_get_swcmd(void *base_kva, u32 offset)
{
	return (*(u32 *)(base_kva + offset));
}

static inline void mdla_set_swcmd(void *base_kva, u32 offset, u32 val)
{
	(*(u32 *)(base_kva + offset)) = val;
}

//cid, which cmd id layer end bit do you want to get
static inline bool mdla_is_layer_end(void *base_kva, u32 cid)
{
	return (mdla_get_swcmd(base_kva + (cid - 1) * MREG_CMD_SIZE,
			       MREG_CMD_GENERAL_CTRL_0)
			       & MREG_CMD_LAYER_END);
}

//cid, which cmd id wait bit do you want to clear
static inline void mdla_clear_swcmd_wait_bit(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1)
			   & ~MREG_CMD_WAIT_SWCMD_DONE);
}

//cid, which cmd id issue bit do you want to clear
static inline void mdla_clear_swcmd_int_bit(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1)
			   & ~MREG_CMD_INT_SWCMD_DONE);
}

static inline void mdla_set_swcmd_done_int(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT)
		       | MREG_CMD_SWCMD_FINISH_INT_EN);
}


/*
 * Enqueue one CE and start scheduler
 *
 */
static void mdla_sched_enqueue_ce(u32 core_id,
					struct command_entry *ce)
{
	struct mdla_scheduler *sched = mdla_get_device(core_id)->sched;
	unsigned long flags;

	if (!ce)
		return;

	spin_lock_irqsave(&sched->lock, flags);

	list_add_tail(&ce->node, &sched->active_ce_queue);
	ce->state |= BIT(CE_QUEUE);

	/* there are CEs under processing */
	if (sched->pro_ce != NULL) {
		spin_unlock_irqrestore(&sched->lock, flags);
		return;
	}

	/* there is no CE under processing: dequeue and trigger engine */
	sched->dequeue_ce(core_id);
	sched->issue_ce(core_id);

	spin_unlock_irqrestore(&sched->lock, flags);
}

/*
 * Consume the processing_ce:
 * 1. get the finished command id and tile id from HW RGs
 * 2. check whether this batch completed
 *
 * Return value:
 * 1. return CE_DONE if all the batches in the CE completed.
 * 2. return CE_RUN if a batch completed, and we can go on to issue the next.
 * 3. return CE_NONE if the batch is still under processing.
 *
 * NOTE: sched->lock should be acquired by caller
 */
static int mdla_sched_process_ce(u32 core_id)
{
	unsigned long flags;
	int ret = CE_NONE;
	struct command_entry *ce;
	struct command_batch *cb;
	struct mdla_dev *dev = mdla_get_device(core_id);
	struct mdla_scheduler *sched = dev->sched;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	struct mdla_pmu_info *pmu;
	u16 priority;
	u32 cmda4, fin_cid, irq_status;

	spin_lock_irqsave(&dev->hw_lock, flags);

	irq_status = io->cmde.read(core_id, MREG_TOP_G_INTP0);

	/* Read current EVT_ID */
	cmda4 = io->cmde.read(core_id, MREG_TOP_G_CDMA4);

	fin_cid = io->cmde.read(core_id, MREG_TOP_G_FIN3);

	priority = sched->pro_ce->cmd_batch_en ? 1 : 0;

	pmu = mdla_util_pmu_ops_get()->get_info(core_id, priority);
	if (pmu)
		mdla_util_pmu_ops_get()->reg_counter_save(core_id, pmu);

	if (likely(irq_status & INTR_PMU_INT))
		io->cmde.write(core_id, MREG_TOP_G_INTP0,
					   INTR_PMU_INT);

	spin_unlock_irqrestore(&dev->hw_lock, flags);

	ce = sched->pro_ce;
	/* FIXME: Need to remove? */
	if (!ce)
		return ret;

	/* FIXME: bool type + 1 ? */
	if (cmda4 != (ce->cmd_batch_en + 1)) {
		ce->irq_state |= IRQ_RECORD_ERROR;
		return ret;
	}

	if (fin_cid < ce->wish_fin_cid) {
		ce->irq_state |= IRQ_TWICE;
		ce->fin_cid = fin_cid;
		return ret;
	}
	/* read after write make sure it will write back to register now */
	spin_lock_irqsave(&dev->hw_lock, flags);
	io->cmde.write(core_id, MREG_TOP_G_CDMA4, 1);
	io->cmde.read(core_id, MREG_TOP_G_CDMA4);
	spin_unlock_irqrestore(&dev->hw_lock, flags);

	ce->fin_cid = fin_cid;
	if (ce->batch_list_head != NULL) {
		if (likely(!list_empty(ce->batch_list_head))) {
			cb = list_first_entry(ce->batch_list_head,
						struct command_batch, node);
			list_del(&cb->node);
			kfree(cb);
		}
	}

	/* all command done for command-based scheduling */
	if (fin_cid >= ce->count) {
		ret = CE_DONE;
		ce->state |= (1 << CE_DONE);
	} else {
		ret = CE_SCHED;
		ce->state |= (1 << CE_SCHED);
	}
	return ret;
}

/*
 * Issue the processing_ce to HW engine
 * NOTE: sched->lock should be acquired by caller
 */
static void mdla_sched_issue_ce(u32 core_id)
{
	dma_addr_t addr;
	u32 nr_cmd_to_issue, irq_status;
	struct mdla_dev *dev = mdla_get_device(core_id);
	struct mdla_scheduler *sched = dev->sched;
	struct command_entry *ce;
	struct command_batch *cb;
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();
	unsigned long flags;

	ce = sched->pro_ce;
	if (!ce) {
		mdla_util_pmu_ops_get()->disable_counter(core_id);
		return;
	}

	if (!(ce->req_start_t))
		ce->req_start_t = sched_clock();

	addr = ce->mva + ((dma_addr_t)ce->fin_cid) * MREG_CMD_SIZE;
	nr_cmd_to_issue = ce->count - ce->fin_cid;
	if ((ce->batch_list_head != NULL)
			&& !list_empty(ce->batch_list_head)) {
		cb = list_first_entry(ce->batch_list_head,
					struct command_batch, node);
		nr_cmd_to_issue = cb->size;
	}
	ce->wish_fin_cid = ce->fin_cid + nr_cmd_to_issue;

	spin_lock_irqsave(&dev->hw_lock, flags);

	irq_status = io->cmde.read(core_id, MREG_TOP_G_INTP0);

	if (likely(irq_status & INTR_CDMA_FIFO_EMPTY)) {
		u32 cdma1 = 0;
		u32 cdma2 = 0;

		/* reset pmu and set register */
		mdla_util_pmu_ops_get()->reset_write_evt_exec(core_id,
						(u16)ce->cmd_batch_en);

		ce->state |= (1 << CE_RUN);

		if (likely(ce->context_callback != NULL))
			ce->context_callback(
				APUSYS_DEVICE_MDLA,
				core_id, ce->ctx_id);

		/* set command address */
		io->cmde.write(core_id, MREG_TOP_G_CDMA1, addr);

		if (unlikely(mdla_dbg_read_u32(FS_TIMEOUT_DBG))) {
			cdma1 = io->cmde.read(core_id, MREG_TOP_G_CDMA1);
			if (cdma1 != addr) {
				ce->state |= (1 << CE_ISSUE_ERROR1);
				//pr_info("cdma1 = %x\n", cdma1);
			}
		}

		/* set command number */
		io->cmde.write(core_id, MREG_TOP_G_CDMA2, nr_cmd_to_issue);

		if (unlikely(mdla_dbg_read_u32(FS_TIMEOUT_DBG))) {
			cdma2 = io->cmde.read(core_id, MREG_TOP_G_CDMA2);
			if (cdma2 != nr_cmd_to_issue) {
				ce->state |= (1 << CE_ISSUE_ERROR2);
				//pr_info("cdma2 = %x\n", cdma2);
			}
		}

		/* trigger engine */
		io->cmde.write(core_id, MREG_TOP_G_CDMA3,
				(ce->cmd_batch_en + 1));

	} else {
		if ((ce->irq_state & IRQ_N_EMPTY_IN_SCHED) == 0)
			ce->irq_state |= IRQ_NE_ISSUE_FIRST;
		ce->irq_state |= IRQ_N_EMPTY_IN_ISSUE;
		ce->state |= (1 << CE_SKIP);
		if (in_interrupt())
			ce->irq_state |= IRQ_IN_IRQ;
		else
			ce->irq_state |= IRQ_NOT_IN_IRQ;
	}

	spin_unlock_irqrestore(&dev->hw_lock, flags);
}

/*
 * Set the status of completed CE as CE_FIN
 * NOTE: sched->lock should be acquired by caller
 */
static void mdla_sched_complete_ce(u32 core_id)
{
	struct mdla_scheduler *sched = mdla_get_device(core_id)->sched;

	sched->pro_ce->req_end_t = sched_clock();
	sched->pro_ce->state |= (1 << CE_FIN);

	complete(&sched->pro_ce->swcmd_done_wait);
	sched->pro_ce = NULL;
}

/*
 * Dequeue a prioritized CE from active CE queue, handle the context switch of
 * the original processing_ce, and set the prioritized CE as processing_ce.
 *
 * NOTE: sched->lock should be acquired by caller
 */
static int mdla_sched_dequeue_ce(u32 core_id)
{
	struct mdla_scheduler *sched = mdla_get_device(core_id)->sched;
	struct command_entry *prioritized_ce = NULL;
	int ret = REASON_QUEUE_NORMALEXE;

	/* get one CE from the active CE queue */
	prioritized_ce =
		list_first_entry_or_null(
			&sched->active_ce_queue,
			struct command_entry,
			node);

	/* return if we don't need to update the processing_ce */
	if (prioritized_ce == NULL)
		return REASON_QUEUE_NOCHANGE;

	/* remove prioritized CE from active CE queue */
	list_del(&prioritized_ce->node);
	prioritized_ce->state |= BIT(CE_DEQUE);
	if (sched->pro_ce != NULL) {
		sched->pro_ce->req_end_t = sched_clock();
		sched->pro_ce->state |= BIT(CE_PREEMPTED);
		mdla_dbg_add_u32(FS_PREEMPTION_TIMES, 1);
		list_add_tail(
			&sched->pro_ce->node,
			&sched->active_ce_queue);
		ret = REASON_QUEUE_PREEMPTION;
		prioritized_ce->state |= BIT(CE_PREEMPTING);
	}
	prioritized_ce->deadline_t = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));
	sched->pro_ce = prioritized_ce;
	return ret;
}

static void mdla_del_free_command_batch(struct command_entry *ce)
{
	struct command_batch *cb;
	struct list_head *tmp, *next;

	if (unlikely(ce->cmd_batch_size >= ce->count))
		return;

	list_for_each_safe(tmp, next, ce->batch_list_head) {
		cb = list_entry(tmp, struct command_batch, node);
		list_del(&cb->node);
		kfree(cb);
	}
	kfree(ce->batch_list_head);
	ce->batch_list_head = NULL;
}

static void mdla_split_alloc_command_batch(struct command_entry *ce)
{
	size_t i, j;
	u32 cur_batch_len = 0;
	u32 batch_tail_id = 0;
	u32 batch_size = ce->cmd_batch_size;
	u32 cmd_count = ce->count;
	struct command_batch *cb;
	struct list_head *tmp, *next;

	if (unlikely(ce->cmdbuf == NULL))
		return;

	ce->batch_list_head =
		kzalloc(sizeof(struct list_head), GFP_KERNEL);

	if (!ce->batch_list_head)
		return;

	INIT_LIST_HEAD(ce->batch_list_head);

	apusys_mem_invalidate(ce->cmdbuf);

	// TODO: add default policy when batch_size is zero
	for (i = 1; i <= cmd_count; i += cur_batch_len) {

		cur_batch_len = 0;
		for (j = i; j <= cmd_count; ++j) {
			cur_batch_len++;
			if (cur_batch_len >= batch_size &&
			    mdla_is_layer_end(ce->kva, j))
				break;
		}

		// allocate one command batch
		cb = kzalloc(sizeof(*cb), GFP_KERNEL);
		if (!cb) {
			mdla_del_free_command_batch(ce);
			return;
		}

		cb->index = i;// first cmd id
		cb->size = cur_batch_len;
		batch_tail_id = i + cur_batch_len - 1;// Last cmd id
		list_add_tail(&cb->node, ce->batch_list_head);
		// encode SWCMD DONE
		mdla_set_swcmd_done_int(ce->kva, batch_tail_id);

		// handle first cmd wait bit and check fuse cmd
		mdla_clear_swcmd_wait_bit(ce->kva, cb->index);
		if (batch_tail_id > cb->index &&
			!mdla_is_layer_end(ce->kva, cb->index)) {
			mdla_clear_swcmd_wait_bit(ce->kva, cb->index + 1);
		}
		// encode the previous layer if that is fused with tail
		mdla_clear_swcmd_int_bit(ce->kva, batch_tail_id);
		if (batch_tail_id > 1 &&
			!mdla_is_layer_end(ce->kva, batch_tail_id - 1)) {
			mdla_set_swcmd_done_int(ce->kva, batch_tail_id - 1);
			mdla_clear_swcmd_int_bit(ce->kva, batch_tail_id - 1);
		}
	}

	// buffer sync here
	if (likely(ce->cmdbuf != NULL))
		apusys_mem_flush(ce->cmdbuf);

	if (mdla_dbg_read_u32(FS_PREEMPTION_DBG))
		list_for_each_safe(tmp, next, ce->batch_list_head) {
			cb = list_entry(tmp, struct command_batch, node);
			mdla_cmd_debug("%s: id = %d, size = %d, tid = %d\n",
				__func__, cb->index,
				cb->size, cb->index + cb->size - 1);
		}
}

int mdla_v2_0_sched_init(void)
{
	struct mdla_scheduler *sched;
	struct mdla_sched_cb_func *sched_cb = mdla_sched_plat_cb();
	int i;

	for_each_mdla_core(i) {
		sched = kzalloc(sizeof(struct mdla_scheduler),
							 GFP_KERNEL);

		if (!sched)
			goto err;

		mdla_get_device(i)->sched = sched;

		spin_lock_init(&sched->lock);
		INIT_LIST_HEAD(&sched->active_ce_queue);

		sched->pro_ce        = NULL;
		sched->enqueue_ce    = mdla_sched_enqueue_ce;
		sched->dequeue_ce    = mdla_sched_dequeue_ce;
		sched->process_ce    = mdla_sched_process_ce;
		sched->issue_ce      = mdla_sched_issue_ce;
		sched->complete_ce   = mdla_sched_complete_ce;
		sched->pro_ce_normal = NULL;
		sched->pro_ce_high   = NULL;
	}

	/* set scheduler callback */
	sched_cb->split_alloc_cmd_batch = mdla_split_alloc_command_batch;
	sched_cb->del_free_cmd_batch    = mdla_del_free_command_batch;

	return 0;

err:
	for (i = i - 1; i >= 0; i--)
		kfree(mdla_get_device(i)->sched);

	return -ENOMEM;
}

void mdla_v2_0_sched_deinit(void)
{
	int i;

	for_each_mdla_core(i)
		kfree(mdla_get_device(i)->sched);
}

