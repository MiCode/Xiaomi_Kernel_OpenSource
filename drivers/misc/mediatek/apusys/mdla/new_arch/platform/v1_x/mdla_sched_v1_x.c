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

#include <interface/mdla_cmd_data_v1_x.h>

#include "mdla_hw_reg_v1_x.h"


struct command_batch {
	struct list_head node;
	u32 index;
	u32 size;
};

struct sched_smp_ce {
	spinlock_t lock;
	u64 deadline;
};

static struct sched_smp_ce smp_ce[PRIORITY_LEVEL];
static struct lock_class_key smp_ce_lock_key[PRIORITY_LEVEL];

static struct lock_class_key sched_lock_key[MAX_CORE_NUM];

static void mdla_sched_set_smp_deadline(int priority, u64 deadline)
{
	unsigned long flags;

	if (unlikely(priority < 0 || priority >= PRIORITY_LEVEL))
		return;

	spin_lock_irqsave(&smp_ce[priority].lock, flags);
	if (deadline > smp_ce[priority].deadline)
		smp_ce[priority].deadline = deadline;
	spin_unlock_irqrestore(&smp_ce[priority].lock, flags);
}

static u64 mdla_sched_get_smp_deadline(int priority)
{
	if (unlikely(priority < 0 || priority >= PRIORITY_LEVEL))
		return 0;

	return smp_ce[priority].deadline;
}

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
			       & MSK_MREG_CMD_LAYER_END);
}

//cid, which cmd id wait bit do you want to clear
static inline void mdla_clear_swcmd_wait_bit(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1)
			   & ~MSK_MREG_CMD_SWCMD_WAIT_SWCMDDONE);
}

//cid, which cmd id issue bit do you want to clear
static inline void mdla_clear_swcmd_int_bit(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1)
			   & ~MSK_MREG_CMD_SWCMD_INT_SWCMDDONE);
}

static inline void mdla_set_swcmd_done_int(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT)
		       | MSK_MREG_CMD_SWCMD_FINISH_INT_EN);
}

/*
 * Enqueue one CE and start scheduler
 *
 */
static void mdla_sched_enqueue_ce(u32 core_id, struct command_entry *ce, u32 resume)
{
	struct mdla_scheduler *sched = mdla_get_device(core_id)->sched;

	if (!ce)
		return;

	if (resume) {
		list_add(&ce->node, &sched->ce_list[ce->priority]);
		ce->state |= BIT(CE_QUEUE_RESUME);
	} else {
		list_add_tail(&ce->node, &sched->ce_list[ce->priority]);
		ce->state |= BIT(CE_QUEUE);
	}

	ce_func_trace(ce, F_ENQUEUE | resume);
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
	u32 fin_cid;

	spin_lock_irqsave(&dev->hw_lock, flags);
	/* Read current EVT_ID */
	fin_cid = io->cmde.read(core_id, MREG_TOP_G_FIN3);
	spin_unlock_irqrestore(&dev->hw_lock, flags);

	ce = sched->pro_ce;

	if (!ce)
		return ret;

	ce->fin_cid = fin_cid;
	if (fin_cid < ce->wish_fin_cid) {
		ce->irq_state |= IRQ_TWICE;
		ce_func_trace(ce, F_CMDDONE_CE_FIN3ERROR);
		return ret;
	}

	/* read after write make sure it will write back to register now */
	spin_lock_irqsave(&dev->hw_lock, flags);

	pmu = mdla_util_pmu_ops_get()->get_info(core_id, (u16)ce->priority);
	if (pmu)
		mdla_util_pmu_ops_get()->reg_counter_save(core_id, pmu);

	io->cmde.write(core_id, MREG_TOP_G_CDMA4, 1);

	spin_unlock_irqrestore(&dev->hw_lock, flags);

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

	if (likely(irq_status & MDLA_IRQ_CDMA_FIFO_EMPTY)) {
		u64 deadline = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

		/* reset pmu and set register */
		mdla_util_pmu_ops_get()->reset_write_evt_exec(core_id,
						(u16)ce->cmd_batch_en);

		ce->state |= (1 << CE_RUN);

		if (likely(ce->context_callback != NULL))
			ce->context_callback(
				APUSYS_DEVICE_MDLA,
				core_id, ce->ctx_id);

		/* update smp deadline */
		if (ce->multicore_total > 1)
			mdla_sched_set_smp_deadline(ce->priority, deadline);
		else
			ce->deadline_t = deadline;

		/* set command address */
		io->cmde.write(core_id, MREG_TOP_G_CDMA1, addr);

		/* set command number */
		io->cmde.write(core_id, MREG_TOP_G_CDMA2, nr_cmd_to_issue);

		ce->req_start_t = sched_clock();

		/* trigger engine */
		io->cmde.write(core_id, MREG_TOP_G_CDMA3, ce->csn);

		ce_func_trace(ce, F_ISSUE);

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
static struct command_entry *mdla_sched_dequeue_ce(u32 core_id)
{
	struct mdla_scheduler *sched = mdla_get_device(core_id)->sched;
	struct command_entry *ce = NULL;
	int prio;

	for (prio = PRIORITY_LEVEL - 1; prio >= 0; prio--) {
		ce = list_first_entry_or_null(&sched->ce_list[prio], struct command_entry, node);
		if (ce) {
			list_del(&ce->node);
			ce_func_trace(ce, F_DEQUEUE);
			break;
		}
	}

	return ce;
}

static void mdla_sched_normal_smp_issue_ce(
	uint32_t core_id,
	uint64_t dual_cmd_id)
{
	struct mdla_scheduler *sched;
	unsigned long flags[MAX_CORE_NUM];
	struct command_entry *ce;
	int i;

	for_each_mdla_core(i) {
		sched = mdla_get_device(i)->sched;
		if (sched->pro_ce != NULL)
			goto unlock;
		spin_lock_irqsave(&sched->lock, flags[i]);
	}

	/* check list status */
	for_each_mdla_core(i) {
		ce = mdla_sched_dequeue_ce(i);

		if ((ce == NULL) || (ce->cmd_id != dual_cmd_id)
				|| (ce->priority != MDLA_LOW_PRIORITY)) {
			mdla_get_device(i)->sched->pro_ce = ce;
			break;
		}

		mdla_get_device(i)->sched->pro_ce = ce;
	}

	if (i == mdla_util_get_core_num()) {
		for_each_mdla_core(i)
			mdla_sched_issue_ce(i);
	} else {
		for (; i >= 0; i--) {
			sched = mdla_get_device(i)->sched;
			mdla_sched_enqueue_ce(i, sched->pro_ce, 2);
			sched->pro_ce = NULL;
		}
	}

	i = mdla_util_get_core_num();

unlock:
	/* free all cores spin lock */
	for (i = i - 1; i >= 0; i--)
		spin_unlock_irqrestore(&mdla_get_device(i)->sched->lock, flags[i]);
}

void mdla_preempt_ce(unsigned int core_id, struct command_entry *high_ce)
{
	struct mdla_scheduler *sched = mdla_get_device(core_id)->sched;
	struct command_entry *low_ce = sched->pro_ce;

	low_ce->state |= (1 << CE_PREEMPTED);
	low_ce->deadline_t = get_jiffies_64() + msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	sched->enqueue_ce(core_id, low_ce, 1);
	mdla_dbg_add_u32(FS_PREEMPTION_TIMES, 1);

	high_ce->state |= (1 << CE_PREEMPTING);
	sched->pro_ce = high_ce;
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

static void mdla_backup_command_batch(struct command_entry *ce)
{
	uint32_t i = 0;
	void *cmd_kva = NULL;

	if (unlikely(!ce->cmd_int_backup || !ce->cmd_ctrl_1_backup))
		return;

	apusys_mem_invalidate(ce->cmdbuf);
	/* backup cmd buffer value */
	for (i = 1; i <= ce->count; i++) {
		cmd_kva = ce->kva + (i - 1) * MREG_CMD_SIZE;

		ce->cmd_int_backup[i - 1] =
			mdla_get_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT);
		ce->cmd_ctrl_1_backup[i - 1] =
			mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1);
	}
	apusys_mem_flush(ce->cmdbuf);
}

static void mdla_restore_command_batch(struct command_entry *ce)
{
	uint32_t i = 0;
	void *cmd_kva = NULL;

	if (unlikely(ce->cmdbuf == NULL))
		return;

	if (unlikely(!ce->cmd_int_backup || !ce->cmd_ctrl_1_backup))
		return;

	apusys_mem_invalidate(ce->cmdbuf);
	for (i = 1; i <= ce->count; i++) {
		cmd_kva = ce->kva + (i - 1) * MREG_CMD_SIZE;
		mdla_set_swcmd(cmd_kva,
			MREG_CMD_TILE_CNT_INT, ce->cmd_int_backup[i - 1]);
		mdla_set_swcmd(cmd_kva,
			MREG_CMD_GENERAL_CTRL_1, ce->cmd_ctrl_1_backup[i - 1]);
	}
	apusys_mem_flush(ce->cmdbuf);
}

int mdla_v1_x_sched_init(void)
{
	struct mdla_scheduler *sched;
	struct mdla_sched_cb_func *sched_cb = mdla_sched_plat_cb();
	int i, j;

	for_each_mdla_core(i) {
		sched = kzalloc(sizeof(struct mdla_scheduler), GFP_KERNEL);
		if (!sched)
			goto err;

		mdla_get_device(i)->sched = sched;

		spin_lock_init(&sched->lock);
		lockdep_set_class(&sched->lock, &sched_lock_key[i]);

		for (j = 0; j < PRIORITY_LEVEL; j++)
			INIT_LIST_HEAD(&sched->ce_list[j]);

		sched->pro_ce           = NULL;
		sched->enqueue_ce       = mdla_sched_enqueue_ce;
		sched->dequeue_ce       = mdla_sched_dequeue_ce;
		sched->process_ce       = mdla_sched_process_ce;
		sched->issue_ce         = mdla_sched_issue_ce;
		sched->complete_ce      = mdla_sched_complete_ce;
		sched->issue_dual_lowce = mdla_sched_normal_smp_issue_ce;
		sched->preempt_ce       = mdla_preempt_ce;
		sched->get_smp_deadline	= mdla_sched_get_smp_deadline;
		sched->set_smp_deadline = mdla_sched_set_smp_deadline;

	}

	for (i = 0; i < PRIORITY_LEVEL; i++) {
		spin_lock_init(&smp_ce[i].lock);
		lockdep_set_class(&smp_ce[i].lock, &smp_ce_lock_key[i]);
	}

	/* set scheduler callback */
	sched_cb->split_alloc_cmd_batch = mdla_split_alloc_command_batch;
	sched_cb->del_free_cmd_batch    = mdla_del_free_command_batch;
	sched_cb->backup_cmd_batch      = mdla_backup_command_batch;
	sched_cb->restore_cmd_batch     = mdla_restore_command_batch;

	return 0;

err:
	for (i = i - 1; i >= 0; i--)
		kfree(mdla_get_device(i)->sched);

	return -ENOMEM;
}

void mdla_v1_x_sched_deinit(void)
{
	int i;

	for_each_mdla_core(i)
		kfree(mdla_get_device(i)->sched);
}

