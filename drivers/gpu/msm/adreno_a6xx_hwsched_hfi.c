// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/iommu.h>
#include <linux/sched/clock.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_hwsched.h"
#include "adreno_hfi.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"
#include "kgsl_device.h"
#include "kgsl_eventlog.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_trace.h"

#define HFI_QUEUE_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_MAX_CNT)

#define DEFINE_QHDR(gmuaddr, id, prio) \
	{\
		.status = 1, \
		.start_addr = GMU_QUEUE_START_ADDR(gmuaddr, id), \
		.type = QUEUE_HDR_TYPE(id, prio, 0, 0), \
		.queue_size = SZ_4K >> 2, \
		.msg_size = 0, \
		.unused0 = 0, \
		.unused1 = 0, \
		.unused2 = 0, \
		.unused3 = 0, \
		.unused4 = 0, \
		.read_index = 0, \
		.write_index = 0, \
}

static struct dq_info {
	/** @max_dq: Maximum number of dispatch queues per RB level */
	u32 max_dq;
	/** @base_dq_id: Base dqid for level */
	u32 base_dq_id;
	/** @offset: Next dqid to use for roundrobin context assignment */
	u32 offset;
} a6xx_hfi_dqs[KGSL_PRIORITY_MAX_RB_LEVELS] = {
	{ 4, 0, }, /* RB0 */
	{ 4, 4, }, /* RB1 */
	{ 3, 8, }, /* RB2 */
	{ 3, 11, }, /* RB3 */
};

struct a6xx_hwsched_hfi *to_a6xx_hwsched_hfi(
	struct adreno_device *adreno_dev)
{
	struct a6xx_device *a6xx_dev = container_of(adreno_dev,
					struct a6xx_device, adreno_dev);
	struct a6xx_hwsched_device *a6xx_hwsched = container_of(a6xx_dev,
					struct a6xx_hwsched_device, a6xx_dev);

	return &a6xx_hwsched->hwsched_hfi;
}

static void add_waiter(struct a6xx_hwsched_hfi *hfi, u32 hdr,
	struct pending_cmd *ack)
{
	memset(ack, 0x0, sizeof(*ack));

	init_completion(&ack->complete);
	write_lock_irq(&hfi->msglock);
	list_add_tail(&ack->node, &hfi->msglist);
	write_unlock_irq(&hfi->msglock);

	ack->sent_hdr = hdr;
}

static void del_waiter(struct a6xx_hwsched_hfi *hfi, struct pending_cmd *ack)
{
	write_lock_irq(&hfi->msglock);
	list_del(&ack->node);
	write_unlock_irq(&hfi->msglock);
}

static void a6xx_receive_ack_async(struct adreno_device *adreno_dev, void *rcvd)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct pending_cmd *cmd = NULL;
	u32 waiters[64], num_waiters = 0, i;
	u32 *ack = rcvd;
	u32 hdr = ack[0];
	u32 req_hdr = ack[1];
	u32 size_bytes = MSG_HDR_GET_SIZE(hdr) << 2;

	if (size_bytes > sizeof(cmd->results))
		dev_err_ratelimited(&gmu->pdev->dev,
			"Ack result too big: %d Truncating to: %ld\n",
			size_bytes, sizeof(cmd->results));

	read_lock(&hfi->msglock);

	list_for_each_entry(cmd, &hfi->msglist, node) {
		if (HDR_CMP_SEQNUM(cmd->sent_hdr, req_hdr)) {
			memcpy(cmd->results, ack,
				min_t(u32, size_bytes,
					sizeof(cmd->results)));
			complete(&cmd->complete);
			read_unlock(&hfi->msglock);
			return;
		}

		if (num_waiters < ARRAY_SIZE(waiters))
			waiters[num_waiters++] = cmd->sent_hdr;
	}

	read_unlock(&hfi->msglock);

	/* Didn't find the sender, list the waiter */
	dev_err_ratelimited(&gmu->pdev->dev,
		"Unexpectedly got id %d seqnum %d. Total waiters: %d Top %d Waiters:\n",
		MSG_HDR_GET_ID(req_hdr), MSG_HDR_GET_SEQNUM(req_hdr),
		num_waiters, min_t(u32, num_waiters, 5));

	for (i = 0; i < num_waiters && i < 5; i++)
		dev_err_ratelimited(&gmu->pdev->dev,
			" id %d seqnum %d\n",
			MSG_HDR_GET_ID(waiters[i]),
			MSG_HDR_GET_SEQNUM(waiters[i]));
}

static void log_profiling_info(struct adreno_device *adreno_dev, u32 *rcvd)
{
	struct hfi_ts_retire_cmd *cmd = (struct hfi_ts_retire_cmd *)rcvd;
	struct kgsl_context *context;
	struct retire_info info = {0};

	context = kgsl_context_get(KGSL_DEVICE(adreno_dev), cmd->ctxt_id);
	if (context == NULL)
		return;

	info.timestamp = cmd->ts;
	info.rb_id = adreno_get_level(context->priority);
	info.gmu_dispatch_queue = context->gmu_dispatch_queue;
	info.submitted_to_rb = cmd->submitted_to_rb;
	info.sop = cmd->sop;
	info.eop = cmd->eop;
	info.retired_on_gmu = cmd->retired_on_gmu;

	trace_adreno_cmdbatch_retired(context, &info, 0, 0, 0);

	log_kgsl_cmdbatch_retired_event(context->id, cmd->ts, context->priority,
		0, cmd->sop, cmd->eop);

	kgsl_context_put(context);
}

u32 a6xx_hwsched_parse_payload(struct payload_section *payload, u32 key)
{
	u32 i;

	/* Each key-value pair is 2 dwords */
	for (i = 0; i < payload->dwords; i += 2) {
		if (payload->data[i] == key)
			return payload->data[i + 1];
	}

	return 0;
}

/* Look up a particular key's value for a given type of payload */
static u32 a6xx_hwsched_lookup_key_value(struct adreno_device *adreno_dev,
	u32 type, u32 key)
{
	struct hfi_context_bad_cmd *cmd = adreno_dev->hwsched.ctxt_bad;
	u32 i = 0, payload_bytes;
	void *start;

	if (!cmd->hdr)
		return 0;

	payload_bytes = (MSG_HDR_GET_SIZE(cmd->hdr) << 2) -
			offsetof(struct hfi_context_bad_cmd, payload);

	start = &cmd->payload[0];

	while (i < payload_bytes) {
		struct payload_section *payload = start + i;

		if (payload->type == type)
			return a6xx_hwsched_parse_payload(payload, key);

		i += struct_size(payload, data, payload->dwords);
	}

	return 0;
}

static u32 get_payload_rb_key(struct adreno_device *adreno_dev,
	u32 rb_id, u32 key)
{
	struct hfi_context_bad_cmd *cmd = adreno_dev->hwsched.ctxt_bad;
	u32 i = 0, payload_bytes;
	void *start;

	if (!cmd->hdr)
		return 0;

	payload_bytes = (MSG_HDR_GET_SIZE(cmd->hdr) << 2) -
			offsetof(struct hfi_context_bad_cmd, payload);

	start = &cmd->payload[0];

	while (i < payload_bytes) {
		struct payload_section *payload = start + i;

		if (payload->type == PAYLOAD_RB) {
			u32 id = a6xx_hwsched_parse_payload(payload, KEY_RB_ID);

			if (id == rb_id)
				return a6xx_hwsched_parse_payload(payload, key);
		}

		i += struct_size(payload, data, payload->dwords);
	}

	return 0;
}

static void log_gpu_fault(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct device *dev = &gmu->pdev->dev;
	struct hfi_context_bad_cmd *cmd = adreno_dev->hwsched.ctxt_bad;

	switch (cmd->error) {
	case GMU_GPU_HW_HANG:
		dev_crit_ratelimited(dev, "MISC: GPU hang detected\n");
		break;
	case GMU_GPU_SW_HANG:
		dev_crit_ratelimited(dev, "gpu timeout ctx %d ts %d\n",
			cmd->ctxt_id, cmd->ts);
		break;
	case GMU_CP_OPCODE_ERROR:
		dev_crit_ratelimited(dev,
			"CP opcode error interrupt | opcode=0x%8.8x\n",
			a6xx_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
			KEY_CP_OPCODE_ERROR));
		break;
	case GMU_CP_PROTECTED_ERROR: {
		u32 status = a6xx_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_PROTECTED_ERROR);

		dev_crit_ratelimited(dev,
			"CP | Protected mode error | %s | addr=0x%5.5x | status=0x%8.8x\n",
			status & (1 << 20) ? "READ" : "WRITE",
			status & 0x3FFFF, status);
		}
		break;
	case GMU_CP_ILLEGAL_INST_ERROR:
		dev_crit_ratelimited(dev, "CP Illegal instruction error\n");
		break;
	case GMU_CP_UCODE_ERROR:
		dev_crit_ratelimited(dev, "CP ucode error interrupt\n");
		break;
	case GMU_CP_HW_FAULT_ERROR:
		dev_crit_ratelimited(dev,
			"CP | Ringbuffer HW fault | status=0x%8.8x\n",
			a6xx_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_HW_FAULT));
		break;
	case GMU_GPU_PREEMPT_TIMEOUT: {
		u32 cur, next, cur_rptr, cur_wptr, next_rptr, next_wptr;

		cur = a6xx_hwsched_lookup_key_value(adreno_dev,
			PAYLOAD_PREEMPT_TIMEOUT, KEY_PREEMPT_TIMEOUT_CUR_RB_ID);
		next = a6xx_hwsched_lookup_key_value(adreno_dev,
			PAYLOAD_PREEMPT_TIMEOUT,
			KEY_PREEMPT_TIMEOUT_NEXT_RB_ID);
		cur_rptr = get_payload_rb_key(adreno_dev, cur, KEY_RB_RPTR);
		cur_wptr = get_payload_rb_key(adreno_dev, cur, KEY_RB_WPTR);
		next_rptr = get_payload_rb_key(adreno_dev, next, KEY_RB_RPTR);
		next_wptr = get_payload_rb_key(adreno_dev, next, KEY_RB_WPTR);

		dev_crit_ratelimited(dev,
			"Preemption Fault: cur=%d R/W=0x%x/0x%x, next=%d R/W=0x%x/0x%x\n",
			cur, cur_rptr, cur_wptr, next, next_rptr, next_wptr);
		}
		break;
	default:
		dev_crit_ratelimited(dev, "Unknown GPU fault: %u\n",
			cmd->error);
		break;
	}
}

static void process_ctx_bad(struct adreno_device *adreno_dev)
{
	log_gpu_fault(adreno_dev);

	adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
}

static u32 peek_next_header(struct a6xx_gmu_device *gmu, uint32_t queue_idx)
{
	struct kgsl_memdesc *mem_addr = gmu->hfi.hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	u32 *queue;

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return 0;

	if (hdr->read_index == hdr->write_index)
		return 0;

	queue = HOST_QUEUE_START_ADDR(mem_addr, queue_idx);

	return queue[hdr->read_index];
}

static void process_msgq_irq(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 rcvd[MAX_RCVD_SIZE], next_hdr;

	for (;;) {
		next_hdr = peek_next_header(gmu, HFI_MSG_ID);

		if (!next_hdr)
			return;

		if (MSG_HDR_GET_ID(next_hdr) == F2H_MSG_CONTEXT_BAD) {
			a6xx_hfi_queue_read(gmu, HFI_MSG_ID,
				(u32 *)adreno_dev->hwsched.ctxt_bad,
				HFI_MAX_MSG_SIZE);
			process_ctx_bad(adreno_dev);
			continue;
		}

		a6xx_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd));

		/*
		 * We are assuming that there is only one outstanding ack
		 * because hfi sending thread waits for completion while
		 * holding the device mutex
		 */
		if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
			a6xx_receive_ack_async(adreno_dev, rcvd);
		} else if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_TS_RETIRE) {
			adreno_hwsched_trigger(adreno_dev);
			log_profiling_info(adreno_dev, rcvd);
		}
	}
}

static void process_log_block(struct adreno_device *adreno_dev, void *data)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_log_block *cmd = data;
	u32 *log_event = gmu->gmu_log->hostptr;
	u32 start, end;

	start = cmd->start_index;
	end = cmd->stop_index;

	log_event += start * 4;
	while (start != end) {
		trace_gmu_event(log_event);
		log_event += 4;
		start++;
	}
}

static void process_dbgq_irq(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 rcvd[MAX_RCVD_SIZE];
	bool recovery = false;

	while (a6xx_hfi_queue_read(gmu, HFI_DBG_ID, rcvd, sizeof(rcvd)) > 0) {

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_ERR) {
			adreno_a6xx_receive_err_req(gmu, rcvd);
			recovery = true;
			break;
		}

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_DEBUG)
			adreno_a6xx_receive_debug_req(gmu, rcvd);

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_LOG_BLOCK)
			process_log_block(adreno_dev, rcvd);
	}

	if (!recovery)
		return;

	adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
}

/* HFI interrupt handler */
static irqreturn_t a6xx_hwsched_hfi_handler(int irq, void *data)
{
	struct adreno_device *adreno_dev = data;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status = 0;

	gmu_core_regread(device, A6XX_GMU_GMU2HOST_INTR_INFO, &status);
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, hfi->irq_mask);

	/*
	 * If interrupts are not enabled on the HFI message queue,
	 * the inline message processing loop will process it,
	 * else, process it here.
	 */
	if (!(hfi->irq_mask & HFI_IRQ_MSGQ_MASK))
		status &= ~HFI_IRQ_MSGQ_MASK;

	if (status & (HFI_IRQ_MSGQ_MASK | HFI_IRQ_DBGQ_MASK)) {
		wake_up_interruptible(&hfi->f2h_wq);
		adreno_hwsched_trigger(adreno_dev);
	}
	if (status & HFI_IRQ_CM3_FAULT_MASK) {
		atomic_set(&gmu->cm3_fault, 1);

		/* make sure other CPUs see the update */
		smp_wmb();

		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU CM3 fault interrupt received\n");

		adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
	}

	/* Ignore OOB bits */
	status &= GENMASK(31 - (oob_max - 1), 0);

	if (status & ~hfi->irq_mask)
		dev_err_ratelimited(&gmu->pdev->dev,
			"Unhandled HFI interrupts 0x%x\n",
			status & ~hfi->irq_mask);

	return IRQ_HANDLED;
}

#define HFI_IRQ_MSGQ_MASK BIT(0)
#define HFI_RSP_TIMEOUT   100 /* msec */

static int wait_ack_completion(struct adreno_device *adreno_dev,
		struct pending_cmd *ack)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int rc;

	rc = wait_for_completion_timeout(&ack->complete,
		HFI_RSP_TIMEOUT);
	if (!rc) {
		dev_err(&gmu->pdev->dev,
			"Ack timeout for id:%d sequence=%d\n",
			MSG_HDR_GET_ID(ack->sent_hdr),
			MSG_HDR_GET_SEQNUM(ack->sent_hdr));
		gmu_core_fault_snapshot(KGSL_DEVICE(adreno_dev));
		return -ETIMEDOUT;
	}

	return 0;
}

static int check_ack_failure(struct adreno_device *adreno_dev,
	struct pending_cmd *ack)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (ack->results[2] != 0xffffffff)
		return 0;

	dev_err(&gmu->pdev->dev,
		"ACK error: sender id %d seqnum %d\n",
		MSG_HDR_GET_ID(ack->sent_hdr),
		MSG_HDR_GET_SEQNUM(ack->sent_hdr));

	return -EINVAL;
}

int a6xx_hfi_send_cmd_async(struct adreno_device *adreno_dev, void *data)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	u32 *cmd = data;
	u32 seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	int rc;
	struct pending_cmd pending_ack;

	*cmd = MSG_HDR_SET_SEQNUM(*cmd, seqnum);

	add_waiter(hfi, *cmd, &pending_ack);

	rc = a6xx_hfi_cmdq_write(adreno_dev, cmd);
	if (rc)
		goto done;

	rc = wait_ack_completion(adreno_dev, &pending_ack);
	if (rc)
		goto done;

	rc = check_ack_failure(adreno_dev, &pending_ack);

done:
	del_waiter(hfi, &pending_ack);

	return rc;
}

static void init_queues(struct a6xx_hfi *hfi)
{
	u32 gmuaddr = hfi->hfi_mem->gmuaddr;
	struct hfi_queue_table hfi_table = {
		.qtbl_hdr = {
			.version = 0,
			.size = sizeof(struct hfi_queue_table) >> 2,
			.qhdr0_offset =
				sizeof(struct hfi_queue_table_header) >> 2,
			.qhdr_size = sizeof(struct hfi_queue_header) >> 2,
			.num_q = HFI_QUEUE_MAX,
			.num_active_q = HFI_QUEUE_MAX,
		},
		.qhdr = {
			DEFINE_QHDR(gmuaddr, HFI_CMD_ID, 0),
			DEFINE_QHDR(gmuaddr, HFI_MSG_ID, 0),
			DEFINE_QHDR(gmuaddr, HFI_DBG_ID, 0),
			/* 4 DQs for RB priority 0 */
			DEFINE_QHDR(gmuaddr, 3, 0),
			DEFINE_QHDR(gmuaddr, 4, 0),
			DEFINE_QHDR(gmuaddr, 5, 0),
			DEFINE_QHDR(gmuaddr, 6, 0),
			/* 4 DQs for RB priority 1 */
			DEFINE_QHDR(gmuaddr, 7, 1),
			DEFINE_QHDR(gmuaddr, 8, 1),
			DEFINE_QHDR(gmuaddr, 9, 1),
			DEFINE_QHDR(gmuaddr, 10, 1),
			/* 3 DQs for RB priority 2 */
			DEFINE_QHDR(gmuaddr, 11, 2),
			DEFINE_QHDR(gmuaddr, 12, 2),
			DEFINE_QHDR(gmuaddr, 13, 2),
			/* 3 DQs for RB priority 3 */
			DEFINE_QHDR(gmuaddr, 14, 3),
			DEFINE_QHDR(gmuaddr, 15, 3),
			DEFINE_QHDR(gmuaddr, 16, 3),
		},
	};

	memcpy(hfi->hfi_mem->hostptr, &hfi_table, sizeof(hfi_table));
}

/* Total header sizes + queue sizes + 16 for alignment */
#define HFIMEM_SIZE (sizeof(struct hfi_queue_table) + 16 + \
	(SZ_4K * HFI_QUEUE_MAX))

static int hfi_f2h_main(void *arg);

int a6xx_hwsched_hfi_init(struct adreno_device *adreno_dev)
{
	struct a6xx_hwsched_hfi *hw_hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct a6xx_hfi *hfi = to_a6xx_hfi(adreno_dev);

	if (IS_ERR_OR_NULL(hw_hfi->big_ib)) {
		hw_hfi->big_ib = reserve_gmu_kernel_block(to_a6xx_gmu(adreno_dev),
				0,
				HWSCHED_MAX_IBS * sizeof(struct hfi_issue_ib),
				GMU_NONCACHED_KERNEL);
		if (IS_ERR(hw_hfi->big_ib))
			return PTR_ERR(hw_hfi->big_ib);
	}

	if (IS_ERR_OR_NULL(hfi->hfi_mem)) {
		hfi->hfi_mem = reserve_gmu_kernel_block(to_a6xx_gmu(adreno_dev),
				0, HFIMEM_SIZE, GMU_NONCACHED_KERNEL);
		if (IS_ERR(hfi->hfi_mem))
			return PTR_ERR(hfi->hfi_mem);
		init_queues(hfi);
	}

	if (IS_ERR_OR_NULL(hw_hfi->f2h_task))
		hw_hfi->f2h_task = kthread_run(hfi_f2h_main, adreno_dev, "gmu_f2h");

	return PTR_ERR_OR_ZERO(hw_hfi->f2h_task);
}

static int get_attrs(u32 flags)
{
	int attrs = IOMMU_READ;

	if (flags & HFI_MEMFLAG_GMU_PRIV)
		attrs |= IOMMU_PRIV;

	if (flags & HFI_MEMFLAG_GMU_WRITEABLE)
		attrs |= IOMMU_WRITE;

	return attrs;
}

static int gmu_import_buffer(struct adreno_device *adreno_dev,
	struct hfi_mem_alloc_entry *entry, u32 flags)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int attrs = get_attrs(flags);
	struct gmu_vma_entry *vma = &gmu->vma[GMU_NONCACHED_KERNEL];
	struct hfi_mem_alloc_desc *desc = &entry->desc;
	int ret;

	if (flags & HFI_MEMFLAG_GMU_CACHEABLE)
		vma = &gmu->vma[GMU_CACHE];

	if ((vma->next_va + desc->size) > (vma->start + vma->size)) {
		dev_err(&gmu->pdev->dev,
			"GMU mapping too big. available: %d required: %d\n",
			vma->next_va - vma->start, desc->size);
		return -ENOMEM;
	}


	ret = gmu_core_map_memdesc(gmu->domain, entry->md, vma->next_va, attrs);
	if (ret) {
		dev_err(&gmu->pdev->dev, "gmu map err: 0x%08x, %x\n",
			vma->next_va, attrs);
		return ret;
	}

	entry->md->gmuaddr = vma->next_va;
	vma->next_va += desc->size;
	return 0;
}

static struct hfi_mem_alloc_entry *lookup_mem_alloc_table(
	struct adreno_device *adreno_dev, struct hfi_mem_alloc_desc *desc)
{
	struct a6xx_hwsched_hfi *hw_hfi = to_a6xx_hwsched_hfi(adreno_dev);
	int i;

	for (i = 0; i < hw_hfi->mem_alloc_entries; i++) {
		struct hfi_mem_alloc_entry *entry = &hw_hfi->mem_alloc_table[i];

		if ((entry->desc.mem_kind == desc->mem_kind) &&
			(entry->desc.gmu_mem_handle == desc->gmu_mem_handle))
			return entry;
	}

	return NULL;
}

static struct hfi_mem_alloc_entry *get_mem_alloc_entry(
	struct adreno_device *adreno_dev, struct hfi_mem_alloc_desc *desc)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct hfi_mem_alloc_entry *entry =
		lookup_mem_alloc_table(adreno_dev, desc);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u64 flags = 0;
	u32 priv = 0;
	int ret;
	const char *memkind_string = desc->mem_kind < HFI_MEMKIND_MAX ?
			hfi_memkind_strings[desc->mem_kind] : "UNKNOWN";

	if (entry)
		return entry;

	if (hfi->mem_alloc_entries == ARRAY_SIZE(hfi->mem_alloc_table)) {
		dev_err(&gmu->pdev->dev,
			"Reached max mem alloc entries\n");
		return ERR_PTR(-ENOMEM);
	}

	entry = &hfi->mem_alloc_table[hfi->mem_alloc_entries];

	memcpy(&entry->desc, desc, sizeof(*desc));

	entry->desc.host_mem_handle = desc->gmu_mem_handle;

	if (desc->flags & HFI_MEMFLAG_GFX_PRIV)
		priv |= KGSL_MEMDESC_PRIVILEGED;

	if (!(desc->flags & HFI_MEMFLAG_GFX_WRITEABLE))
		flags |= KGSL_MEMFLAGS_GPUREADONLY;

	if (desc->flags & HFI_MEMFLAG_GFX_SECURE)
		flags |= KGSL_MEMFLAGS_SECURE;

	if (!(desc->flags & HFI_MEMFLAG_GFX_ACC)) {
		entry->md = reserve_gmu_kernel_block(gmu, 0, desc->size,
				(desc->flags & HFI_MEMFLAG_GMU_CACHEABLE) ?
				GMU_CACHE : GMU_NONCACHED_KERNEL);
		if (IS_ERR(entry->md)) {
			int ret = PTR_ERR(entry->md);

			memset(entry, 0, sizeof(*entry));
			return ERR_PTR(ret);
		}
		entry->desc.size = entry->md->size;
		entry->desc.gmu_addr = entry->md->gmuaddr;

		goto done;
	}

	entry->md = kgsl_allocate_global(device, desc->size, 0, flags, priv,
		memkind_string);
	if (IS_ERR(entry->md)) {
		int ret = PTR_ERR(entry->md);

		memset(entry, 0, sizeof(*entry));
		return ERR_PTR(ret);
	}

	entry->desc.size = entry->md->size;
	entry->desc.gpu_addr = entry->md->gpuaddr;

	if (!(desc->flags & HFI_MEMFLAG_GMU_ACC))
		goto done;

	 /*
	  * If gmu mapping fails, then we have to live with
	  * leaking the gpu global buffer allocated above.
	  */
	ret = gmu_import_buffer(adreno_dev, entry, desc->flags);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"gpuaddr: 0x%llx size: %lld bytes lost\n",
			entry->md->gpuaddr, entry->md->size);
		memset(entry, 0, sizeof(*entry));
		return ERR_PTR(ret);
	}

	entry->desc.gmu_addr = entry->md->gmuaddr;
done:
	hfi->mem_alloc_entries++;

	return entry;
}

static int process_mem_alloc(struct adreno_device *adreno_dev,
	struct hfi_mem_alloc_desc *mad)
{
	struct hfi_mem_alloc_entry *entry;

	entry = get_mem_alloc_entry(adreno_dev, mad);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	if (entry->md) {
		mad->gpu_addr = entry->md->gpuaddr;
		mad->gmu_addr = entry->md->gmuaddr;
	}

	/*
	 * GMU uses the host_mem_handle to check if this memalloc was
	 * successful
	 */
	mad->host_mem_handle = mad->gmu_mem_handle;

	return 0;
}

static int mem_alloc_reply(struct adreno_device *adreno_dev, void *rcvd)
{
	struct hfi_mem_alloc_cmd *in = (struct hfi_mem_alloc_cmd *)rcvd;
	struct hfi_mem_alloc_reply_cmd out = {0};
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	ret = process_mem_alloc(adreno_dev, &in->desc);
	if (ret)
		return ret;

	memcpy(&out.desc, &in->desc, sizeof(out.desc));

	out.hdr = ACK_MSG_HDR(F2H_MSG_MEM_ALLOC, sizeof(out));
	out.hdr = MSG_HDR_SET_SEQNUM(out.hdr,
			atomic_inc_return(&gmu->hfi.seqnum));
	out.req_hdr = in->hdr;

	return a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&out);
}

static int send_start_msg(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	int rc;
	struct hfi_start_cmd cmd;
	u32 rcvd[MAX_RCVD_SIZE];
	struct pending_cmd pending_ack = {0};

	rc = CMD_MSG_HDR(cmd, H2F_MSG_START);
	if (rc)
		return rc;

	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr, seqnum);

	pending_ack.sent_hdr = cmd.hdr;

	rc = a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&cmd);
	if (rc)
		return rc;

poll:
	rc = gmu_core_timed_poll_check(device, A6XX_GMU_GMU2HOST_INTR_INFO,
		HFI_IRQ_MSGQ_MASK, HFI_RSP_TIMEOUT, HFI_IRQ_MSGQ_MASK);

	if (rc) {
		dev_err(&gmu->pdev->dev,
			"Timed out processing MSG_START seqnum: %d\n",
			seqnum);
		gmu_core_fault_snapshot(device);
		return rc;
	}

	/* Clear the interrupt */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR,
		HFI_IRQ_MSGQ_MASK);

	if (a6xx_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd)) <= 0) {
		dev_err(&gmu->pdev->dev, "MSG_START: no payload\n");
		gmu_core_fault_snapshot(device);
		return -EINVAL;
	}

	if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
		rc = a6xx_receive_ack_cmd(gmu, rcvd, &pending_ack);
		if (rc)
			return rc;

		return check_ack_failure(adreno_dev, &pending_ack);
	}

	if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_MEM_ALLOC) {
		rc = mem_alloc_reply(adreno_dev, rcvd);
		if (rc)
			return rc;

		goto poll;
	}

	dev_err(&gmu->pdev->dev,
		"MSG_START: unexpected response id:%d, type:%d\n",
		MSG_HDR_GET_ID(rcvd[0]),
		MSG_HDR_GET_TYPE(rcvd[0]));

	gmu_core_fault_snapshot(device);

	return rc;
}

static void reset_hfi_queues(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_queue_table *tbl = gmu->hfi.hfi_mem->hostptr;
	u32 i;

	/* Flush HFI queues */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		struct hfi_queue_header *hdr = &tbl->qhdr[i];

		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		if (hdr->read_index != hdr->write_index) {
			dev_err(&gmu->pdev->dev,
			"HFI queue[%d] is not empty before close: rd=%d,wt=%d\n",
				i, hdr->read_index, hdr->write_index);
			hdr->read_index = hdr->write_index;

			gmu_core_fault_snapshot(KGSL_DEVICE(adreno_dev));
		}
	}
}

void a6xx_hwsched_hfi_stop(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);

	hfi->irq_mask &= ~HFI_IRQ_MSGQ_MASK;

	reset_hfi_queues(adreno_dev);

	kgsl_pwrctrl_axi(KGSL_DEVICE(adreno_dev), false);

	clear_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

}

static void enable_async_hfi(struct adreno_device *adreno_dev)
{
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);

	hfi->irq_mask |= HFI_IRQ_MSGQ_MASK;

	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), A6XX_GMU_GMU2HOST_INTR_MASK,
		(u32)~hfi->irq_mask);
}

static int enable_preemption(struct adreno_device *adreno_dev)
{
	u32 data;
	int ret;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	/*
	 * Bits [0:1] contains the preemption level
	 * Bit 2 is to enable/disable gmem save/restore
	 * Bit 3 is to enable/disable skipsaverestore
	 */
	data = FIELD_PREP(GENMASK(1, 0), adreno_dev->preempt.preempt_level) |
			FIELD_PREP(BIT(2), adreno_dev->preempt.usesgmem) |
			FIELD_PREP(BIT(3), adreno_dev->preempt.skipsaverestore);

	ret = a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_PREEMPTION, 1,
			data);
	if (ret)
		return ret;

	/*
	 * Bits[3:0] contain the preemption timeout enable bit per ringbuffer
	 * Bits[31:4] contain the timeout in ms
	 */
	return a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_VALUE_BIN_TIME, 1,
			FIELD_PREP(GENMASK(31, 4), 3000) |
			FIELD_PREP(GENMASK(3, 0), 0xf));
}

int a6xx_hwsched_hfi_start(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	ret = a6xx_gmu_hfi_start(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_generic_req(adreno_dev, &gmu->hfi.dcvs_table);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_generic_req(adreno_dev, &gmu->hfi.bw_table);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_acd_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_lm_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_bcl_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_HWSCHED, 1, 0);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_A6XX_KPROF,
			1, 0);
	if (ret)
		goto err;

	/* Enable the long ib timeout detection */
	if (adreno_long_ib_detect(adreno_dev)) {
		ret = a6xx_hfi_send_feature_ctrl(adreno_dev,
			HFI_FEATURE_BAIL_OUT_TIMER, 1, 0);
		if (ret)
			goto err;
	}

	if (gmu->log_stream_enable)
		a6xx_hfi_send_set_value(adreno_dev,
			HFI_VALUE_LOG_STREAM_ENABLE, 0, 1);

	if (gmu->log_group_mask)
		a6xx_hfi_send_set_value(adreno_dev, HFI_VALUE_LOG_GROUP, 0, gmu->log_group_mask);

	ret = a6xx_hfi_send_core_fw_start(adreno_dev);
	if (ret)
		goto err;

	ret = enable_preemption(adreno_dev);
	if (ret)
		goto err;

	ret = send_start_msg(adreno_dev);
	if (ret)
		goto err;

	enable_async_hfi(adreno_dev);

	set_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

	/* Request default DCVS level */
	ret = kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
	if (ret)
		goto err;

	/* Request default BW vote */
	ret = kgsl_pwrctrl_axi(device, true);

err:
	if (ret)
		a6xx_hwsched_hfi_stop(adreno_dev);

	return ret;
}

static int submit_raw_cmds(struct adreno_device *adreno_dev, void *cmds,
	const char *str)
{
	int ret;

	ret = a6xx_hfi_send_cmd_async(adreno_dev, cmds);
	if (ret)
		return ret;

	ret = gmu_core_timed_poll_check(KGSL_DEVICE(adreno_dev),
			A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, 0, 200, BIT(23));
	if (ret)
		a6xx_spin_idle_debug(adreno_dev, str);

	return ret;
}

static int cp_init(struct adreno_device *adreno_dev)
{
	u32 cmds[A6XX_CP_INIT_DWORDS + 1];

	cmds[0] = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW,
		(A6XX_CP_INIT_DWORDS + 1) << 2, HFI_MSG_CMD);

	a6xx_cp_init_cmds(adreno_dev, &cmds[1]);

	return submit_raw_cmds(adreno_dev, cmds,
			"CP initialization failed to idle\n");
}

static int send_switch_to_unsecure(struct adreno_device *adreno_dev)
{
	u32 cmds[3];

	cmds[0] = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW, sizeof(cmds),
			HFI_MSG_CMD);

	cmds[1] = cp_type7_packet(CP_SET_SECURE_MODE, 1);
	cmds[2] = 0;

	return  submit_raw_cmds(adreno_dev, cmds,
			"Switch to unsecure failed to idle\n");
}

int a6xx_hwsched_cp_init(struct adreno_device *adreno_dev)
{
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Program the ucode base for CP */
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_LO,
		lower_32_bits(fw->memdesc->gpuaddr));
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_HI,
		upper_32_bits(fw->memdesc->gpuaddr));

	ret = cp_init(adreno_dev);
	if (ret)
		return ret;

	ret = adreno_zap_shader_load(adreno_dev, a6xx_core->zap_name);
	if (ret)
		return ret;

	if (!adreno_dev->zap_loaded)
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			A6XX_RBBM_SECVID_TRUST_CNTL, 0x0);
	else
		ret = send_switch_to_unsecure(adreno_dev);

	return ret;
}

static bool is_queue_empty(struct adreno_device *adreno_dev, u32 queue_idx)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_memdesc *mem_addr = gmu->hfi.hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return true;

	if (hdr->read_index == hdr->write_index)
		return true;

	return false;
}

static int hfi_f2h_main(void *arg)
{
	struct adreno_device *adreno_dev = arg;
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);

	while (!kthread_should_stop()) {
		wait_event_interruptible(hfi->f2h_wq, !kthread_should_stop() &&
			!(is_queue_empty(adreno_dev, HFI_MSG_ID) &&
			is_queue_empty(adreno_dev, HFI_DBG_ID)));

		if (kthread_should_stop())
			break;

		process_msgq_irq(adreno_dev);
		process_dbgq_irq(adreno_dev);
	}

	return 0;
}

int a6xx_hwsched_hfi_probe(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hw_hfi = to_a6xx_hwsched_hfi(adreno_dev);

	gmu->hfi.irq = kgsl_request_irq(gmu->pdev, "kgsl_hfi_irq",
		a6xx_hwsched_hfi_handler, adreno_dev);

	if (gmu->hfi.irq < 0)
		return gmu->hfi.irq;

	hw_hfi->irq_mask = HFI_IRQ_MASK;

	rwlock_init(&hw_hfi->msglock);

	INIT_LIST_HEAD(&hw_hfi->msglist);

	init_waitqueue_head(&hw_hfi->f2h_wq);

	return 0;
}

void a6xx_hwsched_hfi_remove(struct adreno_device *adreno_dev)
{
	struct a6xx_hwsched_hfi *hw_hfi = to_a6xx_hwsched_hfi(adreno_dev);

	kthread_stop(hw_hfi->f2h_task);
}

static void add_profile_events(struct adreno_device *adreno_dev,
	struct kgsl_drawobj *drawobj, struct adreno_submit_time *time)
{
	unsigned long flags;
	u64 time_in_s;
	unsigned long time_in_ns;
	struct kgsl_context *context = drawobj->context;
	struct submission_info info = {0};

	/*
	 * Here we are attempting to create a mapping between the
	 * GPU time domain (alwayson counter) and the CPU time domain
	 * (local_clock) by sampling both values as close together as
	 * possible. This is useful for many types of debugging and
	 * profiling. In order to make this mapping as accurate as
	 * possible, we must turn off interrupts to avoid running
	 * interrupt handlers between the two samples.
	 */

	local_irq_save(flags);

	/* Read always on registers */
	time->ticks = a6xx_read_alwayson(adreno_dev);

	/* Trace the GPU time to create a mapping to ftrace time */
	trace_adreno_cmdbatch_sync(context->id, context->priority,
		drawobj->timestamp, time->ticks);

	/* Get the kernel clock for time since boot */
	time->ktime = local_clock();

	/* Get the timeofday for the wall time (for the user) */
	ktime_get_real_ts64(&time->utime);

	local_irq_restore(flags);

	/* Return kernel clock time to the client if requested */
	time_in_s = time->ktime;
	time_in_ns = do_div(time_in_s, 1000000000);

	info.inflight = -1;
	info.rb_id = adreno_get_level(context->priority);
	info.gmu_dispatch_queue = context->gmu_dispatch_queue;

	trace_adreno_cmdbatch_submitted(drawobj, &info, time->ticks,
		(unsigned long) time_in_s, time_in_ns / 1000, 0);

	log_kgsl_cmdbatch_submitted_event(context->id, drawobj->timestamp,
		context->priority, drawobj->flags);
}

static u32 get_next_dq(u32 priority)
{
	struct dq_info *info = &a6xx_hfi_dqs[priority];
	u32 next = info->base_dq_id + info->offset;

	info->offset = (info->offset + 1) % info->max_dq;

	return next;
}

static u32 get_dq_id(u32 priority)
{
	u32 level = adreno_get_level(priority);

	return get_next_dq(level);
}

static int send_context_register(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct hfi_register_ctxt_cmd cmd;
	struct kgsl_pagetable *pt = context->proc_priv->pagetable;
	int ret;

	ret = CMD_MSG_HDR(cmd, H2F_MSG_REGISTER_CONTEXT);
	if (ret)
		return ret;

	cmd.ctxt_id = context->id;
	cmd.flags = HFI_CTXT_FLAG_NOTIFY | context->flags;
	cmd.pt_addr = kgsl_mmu_pagetable_get_ttbr0(pt);
	cmd.ctxt_idr = pid_nr(context->proc_priv->pid);
	cmd.ctxt_bank = kgsl_mmu_pagetable_get_context_bank(pt);

	return a6xx_hfi_send_cmd_async(adreno_dev, &cmd);
}

static int send_context_pointers(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_context_pointers_cmd cmd;
	int ret;

	ret = CMD_MSG_HDR(cmd, H2F_MSG_CONTEXT_POINTERS);
	if (ret)
		return ret;

	cmd.ctxt_id = context->id;
	cmd.sop_addr = MEMSTORE_ID_GPU_ADDR(device, context->id, soptimestamp);
	cmd.eop_addr = MEMSTORE_ID_GPU_ADDR(device, context->id, eoptimestamp);
	if (context->user_ctxt_record)
		cmd.user_ctxt_record_addr =
			context->user_ctxt_record->memdesc.gpuaddr;
	else
		cmd.user_ctxt_record_addr = 0;

	return a6xx_hfi_send_cmd_async(adreno_dev, &cmd);
}

static int hfi_context_register(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (context->gmu_registered)
		return 0;

	ret = send_context_register(adreno_dev, context);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to register context %d: %d\n",
			context->id, ret);

		if (device->gmu_fault)
			adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);

		return ret;
	}

	ret = send_context_pointers(adreno_dev, context);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to register context %d pointers: %d\n",
			context->id, ret);

		if (device->gmu_fault)
			adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);

		return ret;
	}

	context->gmu_registered = true;
	context->gmu_dispatch_queue = get_dq_id(context->priority);

	return 0;
}

static void populate_ibs(struct adreno_device *adreno_dev,
	struct hfi_submit_cmd *cmd, struct kgsl_drawobj_cmd *cmdobj)
{
	struct hfi_issue_ib *issue_ib;
	struct kgsl_memobj_node *ib;

	if (cmdobj->numibs > HWSCHED_MAX_DISPATCH_NUMIBS) {
		struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);

		/*
		 * The dispatcher ensures that there is only one big IB inflight
		 */
		cmd->big_ib_gmu_va = hfi->big_ib->gmuaddr;
		cmd->flags |= CMDBATCH_INDIRECT;
		issue_ib = hfi->big_ib->hostptr;
	} else {
		issue_ib = (struct hfi_issue_ib *)&cmd[1];
	}

	list_for_each_entry(ib, &cmdobj->cmdlist, node) {
		issue_ib->addr = ib->gpuaddr;
		issue_ib->size = ib->size;
		issue_ib++;
	}

	cmd->numibs = cmdobj->numibs;
}

#define HFI_DSP_IRQ_BASE 2

#define DISPQ_IRQ_BIT(_idx) BIT((_idx) + HFI_DSP_IRQ_BASE)

int a6xx_hwsched_submit_cmdobj(struct adreno_device *adreno_dev,
	struct kgsl_drawobj_cmd *cmdobj)
{
	struct a6xx_hfi *hfi = to_a6xx_hfi(adreno_dev);
	int ret = 0;
	u32 cmd_sizebytes;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct hfi_submit_cmd *cmd;
	struct adreno_submit_time time = {0};

	ret = hfi_context_register(adreno_dev, drawobj->context);
	if (ret)
		return ret;

	/* Add a *issue_ib struct for each IB */
	if (cmdobj->numibs > HWSCHED_MAX_DISPATCH_NUMIBS ||
		test_bit(CMDOBJ_SKIP, &cmdobj->priv))
		cmd_sizebytes = sizeof(*cmd);
	else
		cmd_sizebytes = sizeof(*cmd) +
			(sizeof(struct hfi_issue_ib) * cmdobj->numibs);

	if (WARN_ON(cmd_sizebytes > HFI_MAX_MSG_SIZE))
		return -EMSGSIZE;

	cmd = kmalloc(cmd_sizebytes, GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;

	cmd->ctxt_id = drawobj->context->id;
	cmd->flags = HFI_CTXT_FLAG_NOTIFY;
	cmd->ts = drawobj->timestamp;

	if (test_bit(CMDOBJ_SKIP, &cmdobj->priv))
		goto skipib;

	populate_ibs(adreno_dev, cmd, cmdobj);

	if ((drawobj->flags & KGSL_DRAWOBJ_PROFILING) &&
		cmdobj->profiling_buf_entry) {

		time.drawobj = drawobj;

		cmd->profile_gpuaddr_lo =
			lower_32_bits(cmdobj->profiling_buffer_gpuaddr);
		cmd->profile_gpuaddr_hi =
			upper_32_bits(cmdobj->profiling_buffer_gpuaddr);

		/* Indicate to GMU to do user profiling for this submission */
		cmd->flags |= CMDBATCH_PROFILING;
	}

skipib:
	adreno_drawobj_set_constraint(KGSL_DEVICE(adreno_dev), drawobj);

	cmd->hdr = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD, cmd_sizebytes,
			HFI_MSG_CMD);
	cmd->hdr = MSG_HDR_SET_SEQNUM(cmd->hdr,
			atomic_inc_return(&hfi->seqnum));

	ret = a6xx_hfi_queue_write(adreno_dev,
		HFI_DSP_ID_0 + drawobj->context->gmu_dispatch_queue,
		(u32 *)cmd);
	if (ret)
		goto free;

	add_profile_events(adreno_dev, drawobj, &time);

	cmdobj->submit_ticks = time.ticks;

	/* Send interrupt to GMU to receive the message */
	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), A6XX_GMU_HOST2GMU_INTR_SET,
		DISPQ_IRQ_BIT(drawobj->context->gmu_dispatch_queue));

	/* Put the profiling information in the user profiling buffer */
	adreno_profile_submit_time(&time);

free:
	kfree(cmd);

	return ret;
}

static int send_context_unregister_hfi(struct adreno_device *adreno_dev,
	struct kgsl_context *context, u32 ts)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct pending_cmd pending_ack;
	struct hfi_unregister_ctxt_cmd cmd;
	u32 seqnum;
	int rc;

	/* Only send HFI if device is not in SLUMBER */
	if (!context->gmu_registered ||
		!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return 0;

	rc = CMD_MSG_HDR(cmd, H2F_MSG_UNREGISTER_CONTEXT);
	if (rc)
		return rc;

	cmd.ctxt_id = context->id,
	cmd.ts = ts,

	seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr, seqnum);

	add_waiter(hfi, cmd.hdr, &pending_ack);

	/*
	 * Although we know device is powered on, we can still enter SLUMBER
	 * because the wait for ack below is done without holding the mutex. So
	 * take an active count before releasing the mutex so as to avoid a
	 * concurrent SLUMBER sequence while GMU is un-registering this context.
	 */
	a6xx_hwsched_active_count_get(adreno_dev);

	rc = a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&cmd);
	if (rc)
		goto done;

	mutex_unlock(&device->mutex);

	rc = wait_for_completion_timeout(&pending_ack.complete,
			msecs_to_jiffies(30 * 1000));
	if (!rc) {
		dev_err(&gmu->pdev->dev,
			"Ack timeout for context unregister seq: %d ctx: %d ts: %d\n",
			MSG_HDR_GET_SEQNUM(pending_ack.sent_hdr),
			context->id, ts);
		rc = -ETIMEDOUT;

		mutex_lock(&device->mutex);

		gmu_core_fault_snapshot(device);

		/*
		 * Trigger dispatcher based reset and recovery. Invalidate the
		 * context so that any un-finished inflight submissions are not
		 * replayed after recovery.
		 */
		adreno_drawctxt_set_guilty(device, context);

		adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);

		goto done;
	}

	mutex_lock(&device->mutex);

	rc = check_ack_failure(adreno_dev, &pending_ack);
done:
	a6xx_hwsched_active_count_put(adreno_dev);

	del_waiter(hfi, &pending_ack);

	return rc;
}

void a6xx_hwsched_context_detach(struct adreno_context *drawctxt)
{
	struct kgsl_context *context = &drawctxt->base;
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = 0;

	mutex_lock(&device->mutex);

	ret = send_context_unregister_hfi(adreno_dev, context,
		drawctxt->internal_timestamp);

	if (!ret) {
		kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

		kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

		adreno_profile_process_results(adreno_dev);
	}

	context->gmu_registered = false;

	mutex_unlock(&device->mutex);
}

u32 a6xx_hwsched_preempt_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_get_value_cmd cmd;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	u32 seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	struct pending_cmd pending_ack;
	int rc;

	if (device->state != KGSL_STATE_ACTIVE)
		return 0;

	rc = CMD_MSG_HDR(cmd, H2F_MSG_GET_VALUE);
	if (rc)
		return 0;

	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr, seqnum);
	cmd.type = HFI_VALUE_PREEMPT_COUNT;
	cmd.subtype = 0;

	add_waiter(hfi, cmd.hdr, &pending_ack);

	rc = a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&cmd);
	if (rc)
		goto done;

	rc = wait_ack_completion(adreno_dev, &pending_ack);
	if (rc)
		goto done;

	rc = check_ack_failure(adreno_dev, &pending_ack);

done:
	del_waiter(hfi, &pending_ack);

	return rc ? 0 : pending_ack.results[2];
}
