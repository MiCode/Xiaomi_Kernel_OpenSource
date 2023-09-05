// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/dma-fence-array.h>
#include <linux/iommu.h>
#include <linux/sched/clock.h>
#include <soc/qcom/msm_performance.h>

#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_gen7_hwsched.h"
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
} gen7_hfi_dqs[KGSL_PRIORITY_MAX_RB_LEVELS] = {
	{ 4, 0, }, /* RB0 */
	{ 4, 4, }, /* RB1 */
	{ 3, 8, }, /* RB2 */
	{ 3, 11, }, /* RB3 */
}, gen7_hfi_dqs_lpac[KGSL_PRIORITY_MAX_RB_LEVELS + 1] = {
	{ 4, 0, }, /* RB0 */
	{ 4, 4, }, /* RB1 */
	{ 3, 8, }, /* RB2 */
	{ 2, 11, }, /* RB3 */
	{ 1, 13, }, /* RB LPAC */
};

struct gen7_hwsched_hfi *to_gen7_hwsched_hfi(
	struct adreno_device *adreno_dev)
{
	struct gen7_device *gen7_dev = container_of(adreno_dev,
					struct gen7_device, adreno_dev);
	struct gen7_hwsched_device *gen7_hwsched = container_of(gen7_dev,
					struct gen7_hwsched_device, gen7_dev);

	return &gen7_hwsched->hwsched_hfi;
}

int gen7_hfi_send_lpac_feature_ctrl(struct adreno_device *adreno_dev)
{
	if (!adreno_dev->lpac_enabled)
		return 0;

	return gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_LPAC, 1, 0);
}

static void add_waiter(struct gen7_hwsched_hfi *hfi, u32 hdr,
	struct pending_cmd *ack)
{
	memset(ack, 0x0, sizeof(*ack));

	init_completion(&ack->complete);
	write_lock_irq(&hfi->msglock);
	list_add_tail(&ack->node, &hfi->msglist);
	write_unlock_irq(&hfi->msglock);

	ack->sent_hdr = hdr;
}

static void del_waiter(struct gen7_hwsched_hfi *hfi, struct pending_cmd *ack)
{
	write_lock_irq(&hfi->msglock);
	list_del(&ack->node);
	write_unlock_irq(&hfi->msglock);
}

static void gen7_receive_ack_async(struct adreno_device *adreno_dev, void *rcvd)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
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
		if (CMP_HFI_ACK_HDR(cmd->sent_hdr, req_hdr)) {
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
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	context = kgsl_context_get(device, cmd->ctxt_id);
	if (context == NULL)
		return;

	info.timestamp = cmd->ts;
	info.rb_id = adreno_get_level(context);
	info.gmu_dispatch_queue = context->gmu_dispatch_queue;
	info.submitted_to_rb = cmd->submitted_to_rb;
	info.sop = cmd->sop;
	info.eop = cmd->eop;
	if (GMU_VER_MINOR(gmu->ver.hfi) < 4)
		info.active = cmd->eop - cmd->sop;
	else
		info.active = cmd->active;
	info.retired_on_gmu = cmd->retired_on_gmu;

	/* protected GPU work must not be reported */
	if  (!(context->flags & KGSL_CONTEXT_SECURE))
		kgsl_work_period_update(device, context->proc_priv->period,
					     info.active);

	trace_adreno_cmdbatch_retired(context, &info, 0, 0, 0);

	log_kgsl_cmdbatch_retired_event(context->id, cmd->ts,
		context->priority, 0, cmd->sop, cmd->eop);

	kgsl_context_put(context);
}

u32 gen7_hwsched_parse_payload(struct payload_section *payload, u32 key)
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
static u32 gen7_hwsched_lookup_key_value_legacy(struct adreno_device *adreno_dev,
	u32 type, u32 key)
{
	struct hfi_context_bad_cmd_legacy *cmd = adreno_dev->hwsched.ctxt_bad;
	u32 i = 0, payload_bytes;
	void *start;

	if (!cmd->hdr)
		return 0;

	payload_bytes = (MSG_HDR_GET_SIZE(cmd->hdr) << 2) -
			offsetof(struct hfi_context_bad_cmd_legacy, payload);

	start = &cmd->payload[0];

	while (i < payload_bytes) {
		struct payload_section *payload = start + i;

		if (payload->type == type)
			return gen7_hwsched_parse_payload(payload, key);

		i += struct_size(payload, data, payload->dwords);
	}

	return 0;
}

static u32 get_payload_rb_key_legacy(struct adreno_device *adreno_dev,
	u32 rb_id, u32 key)
{
	struct hfi_context_bad_cmd_legacy *cmd = adreno_dev->hwsched.ctxt_bad;
	u32 i = 0, payload_bytes;
	void *start;

	if (!cmd->hdr)
		return 0;

	payload_bytes = (MSG_HDR_GET_SIZE(cmd->hdr) << 2) -
			offsetof(struct hfi_context_bad_cmd_legacy, payload);

	start = &cmd->payload[0];

	while (i < payload_bytes) {
		struct payload_section *payload = start + i;

		if (payload->type == PAYLOAD_RB) {
			u32 id = gen7_hwsched_parse_payload(payload, KEY_RB_ID);

			if (id == rb_id)
				return gen7_hwsched_parse_payload(payload, key);
		}

		i += struct_size(payload, data, payload->dwords);
	}

	return 0;
}

static void log_gpu_fault_legacy(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct device *dev = &gmu->pdev->dev;
	struct hfi_context_bad_cmd_legacy *cmd = adreno_dev->hwsched.ctxt_bad;

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
			gen7_hwsched_lookup_key_value_legacy(adreno_dev, PAYLOAD_FAULT_REGS,
			KEY_CP_OPCODE_ERROR));
		break;
	case GMU_CP_PROTECTED_ERROR: {
		u32 status = gen7_hwsched_lookup_key_value_legacy(adreno_dev, PAYLOAD_FAULT_REGS,
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
			gen7_hwsched_lookup_key_value_legacy(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_HW_FAULT));
		break;
	case GMU_GPU_PREEMPT_TIMEOUT: {
		u32 cur, next, cur_rptr, cur_wptr, next_rptr, next_wptr;

		cur = gen7_hwsched_lookup_key_value_legacy(adreno_dev,
			PAYLOAD_PREEMPT_TIMEOUT, KEY_PREEMPT_TIMEOUT_CUR_RB_ID);
		next = gen7_hwsched_lookup_key_value_legacy(adreno_dev,
			PAYLOAD_PREEMPT_TIMEOUT,
			KEY_PREEMPT_TIMEOUT_NEXT_RB_ID);
		cur_rptr = get_payload_rb_key_legacy(adreno_dev, cur, KEY_RB_RPTR);
		cur_wptr = get_payload_rb_key_legacy(adreno_dev, cur, KEY_RB_WPTR);
		next_rptr = get_payload_rb_key_legacy(adreno_dev, next, KEY_RB_RPTR);
		next_wptr = get_payload_rb_key_legacy(adreno_dev, next, KEY_RB_WPTR);

		dev_crit_ratelimited(dev,
			"Preemption Fault: cur=%d R/W=0x%x/0x%x, next=%d R/W=0x%x/0x%x\n",
			cur, cur_rptr, cur_wptr, next, next_rptr, next_wptr);
		}
		break;
	case GMU_CP_GPC_ERROR:
		dev_crit_ratelimited(dev, "RBBM: GPC error\n");
		break;
	case GMU_CP_BV_OPCODE_ERROR:
		dev_crit_ratelimited(dev,
			"CP BV opcode error | opcode=0x%8.8x\n",
			gen7_hwsched_lookup_key_value_legacy(adreno_dev, PAYLOAD_FAULT_REGS,
			KEY_CP_BV_OPCODE_ERROR));
		break;
	case GMU_CP_BV_PROTECTED_ERROR: {
		u32 status = gen7_hwsched_lookup_key_value_legacy(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_BV_PROTECTED_ERROR);

		dev_crit_ratelimited(dev,
			"CP BV | Protected mode error | %s | addr=0x%5.5x | status=0x%8.8x\n",
			status & (1 << 20) ? "READ" : "WRITE",
			status & 0x3FFFF, status);
		}
		break;
	case GMU_CP_BV_HW_FAULT_ERROR:
		dev_crit_ratelimited(dev,
			"CP BV | Ringbuffer HW fault | status=0x%8.8x\n",
			gen7_hwsched_lookup_key_value_legacy(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_HW_FAULT));
		break;
	case GMU_CP_BV_ILLEGAL_INST_ERROR:
		dev_crit_ratelimited(dev, "CP BV Illegal instruction error\n");
		break;
	case GMU_CP_BV_UCODE_ERROR:
		dev_crit_ratelimited(dev, "CP BV ucode error interrupt\n");
		break;
	case GMU_CP_UNKNOWN_ERROR:
		fallthrough;
	default:
		dev_crit_ratelimited(dev, "Unknown GPU fault: %u\n",
			cmd->error);
		break;
	}
}

/* Look up a particular key's value for a given type of payload */
static u32 gen7_hwsched_lookup_key_value(struct adreno_device *adreno_dev,
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
			return gen7_hwsched_parse_payload(payload, key);

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
			u32 id = gen7_hwsched_parse_payload(payload, KEY_RB_ID);

			if (id == rb_id)
				return gen7_hwsched_parse_payload(payload, key);
		}

		i += struct_size(payload, data, payload->dwords);
	}

	return 0;
}

static void log_gpu_fault(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct device *dev = &gmu->pdev->dev;
	struct hfi_context_bad_cmd *cmd = adreno_dev->hwsched.ctxt_bad;

	switch (cmd->error) {
	case GMU_GPU_HW_HANG:
		dev_crit_ratelimited(dev, "MISC: GPU hang detected\n");
		break;
	case GMU_GPU_SW_HANG:
		dev_crit_ratelimited(dev, "gpu timeout ctx %d ts %d\n",
			cmd->gc.ctxt_id, cmd->gc.ts);
		break;
	case GMU_CP_OPCODE_ERROR:
		dev_crit_ratelimited(dev,
			"CP opcode error interrupt | opcode=0x%8.8x\n",
			gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
			KEY_CP_OPCODE_ERROR));
		break;
	case GMU_CP_PROTECTED_ERROR: {
		u32 status = gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
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
			gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_HW_FAULT));
		break;
	case GMU_GPU_PREEMPT_TIMEOUT: {
		u32 cur, next, cur_rptr, cur_wptr, next_rptr, next_wptr;

		cur = gen7_hwsched_lookup_key_value(adreno_dev,
			PAYLOAD_PREEMPT_TIMEOUT, KEY_PREEMPT_TIMEOUT_CUR_RB_ID);
		next = gen7_hwsched_lookup_key_value(adreno_dev,
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
	case GMU_CP_GPC_ERROR:
		dev_crit_ratelimited(dev, "RBBM: GPC error\n");
		break;
	case GMU_CP_BV_OPCODE_ERROR:
		dev_crit_ratelimited(dev,
			"CP BV opcode error | opcode=0x%8.8x\n",
			gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
			KEY_CP_BV_OPCODE_ERROR));
		break;
	case GMU_CP_BV_PROTECTED_ERROR: {
		u32 status = gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_BV_PROTECTED_ERROR);

		dev_crit_ratelimited(dev,
			"CP BV | Protected mode error | %s | addr=0x%5.5x | status=0x%8.8x\n",
			status & (1 << 20) ? "READ" : "WRITE",
			status & 0x3FFFF, status);
		}
		break;
	case GMU_CP_BV_HW_FAULT_ERROR:
		dev_crit_ratelimited(dev,
			"CP BV | Ringbuffer HW fault | status=0x%8.8x\n",
			gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_HW_FAULT));
		break;
	case GMU_CP_BV_ILLEGAL_INST_ERROR:
		dev_crit_ratelimited(dev, "CP BV Illegal instruction error\n");
		break;
	case GMU_CP_BV_UCODE_ERROR:
		dev_crit_ratelimited(dev, "CP BV ucode error interrupt\n");
		break;
	case GMU_CP_LPAC_OPCODE_ERROR:
		dev_crit_ratelimited(dev,
			"CP LPAC opcode error | opcode=0x%8.8x\n",
			gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
			KEY_CP_LPAC_OPCODE_ERROR));
		break;
	case GMU_CP_LPAC_PROTECTED_ERROR: {
		u32 status = gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_LPAC_PROTECTED_ERROR);

		dev_crit_ratelimited(dev,
			"CP LPAC | Protected mode error | %s | addr=0x%5.5x | status=0x%8.8x\n",
			status & (1 << 20) ? "READ" : "WRITE",
			status & 0x3FFFF, status);
		}
		break;
	case GMU_CP_LPAC_HW_FAULT_ERROR:
		dev_crit_ratelimited(dev,
			"CP LPAC | Ringbuffer HW fault | status=0x%8.8x\n",
			gen7_hwsched_lookup_key_value(adreno_dev, PAYLOAD_FAULT_REGS,
				KEY_CP_LPAC_HW_FAULT));
		break;
	case GMU_CP_LPAC_ILLEGAL_INST_ERROR:
		dev_crit_ratelimited(dev, "CP LPAC Illegal instruction error\n");
		break;
	case GMU_CP_LPAC_UCODE_ERROR:
		dev_crit_ratelimited(dev, "CP LPAC ucode error interrupt\n");
		break;
	case GMU_GPU_LPAC_SW_HANG:
		dev_crit_ratelimited(dev, "LPAC: gpu timeout ctx %d ts %d\n",
			cmd->lpac.ctxt_id, cmd->lpac.ts);
		break;
	case GMU_CP_UNKNOWN_ERROR:
		fallthrough;
	default:
		dev_crit_ratelimited(dev, "Unknown GPU fault: %u\n",
			cmd->error);
		break;
	}
}

static u32 peek_next_header(struct gen7_gmu_device *gmu, uint32_t queue_idx)
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

static void process_ctx_bad(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (GMU_VER_MINOR(gmu->ver.hfi) < 2)
		log_gpu_fault_legacy(adreno_dev);
	else
		log_gpu_fault(adreno_dev);

	adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
}

static void gen7_hwsched_process_msgq(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);
	u32 rcvd[MAX_RCVD_SIZE], next_hdr;

	mutex_lock(&hw_hfi->msgq_mutex);

	for (;;) {
		next_hdr = peek_next_header(gmu, HFI_MSG_ID);

		if (!next_hdr)
			break;

		if (MSG_HDR_GET_ID(next_hdr) == F2H_MSG_CONTEXT_BAD) {
			gen7_hfi_queue_read(gmu, HFI_MSG_ID,
				(u32 *)adreno_dev->hwsched.ctxt_bad,
				HFI_MAX_MSG_SIZE);
			process_ctx_bad(adreno_dev);
			continue;
		}

		gen7_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd));

		/*
		 * We are assuming that there is only one outstanding ack
		 * because hfi sending thread waits for completion while
		 * holding the device mutex
		 */
		if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
			gen7_receive_ack_async(adreno_dev, rcvd);
		} else if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_TS_RETIRE) {
			log_profiling_info(adreno_dev, rcvd);
			adreno_hwsched_trigger(adreno_dev);
		}
	}
	mutex_unlock(&hw_hfi->msgq_mutex);
}

static void process_log_block(struct adreno_device *adreno_dev, void *data)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
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

static void gen7_hwsched_process_dbgq(struct adreno_device *adreno_dev, bool limited)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	u32 rcvd[MAX_RCVD_SIZE];
	bool recovery = false;

	while (gen7_hfi_queue_read(gmu, HFI_DBG_ID, rcvd, sizeof(rcvd)) > 0) {

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_ERR) {
			adreno_gen7_receive_err_req(gmu, rcvd);
			recovery = true;
			break;
		}

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_DEBUG)
			adreno_gen7_receive_debug_req(gmu, rcvd);

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_LOG_BLOCK)
			process_log_block(adreno_dev, rcvd);

		/* Process one debug queue message and return to not delay msgq processing */
		if (limited)
			break;
	}

	if (!recovery)
		return;

	adreno_hwsched_fault(adreno_dev, ADRENO_GMU_FAULT);
}

/* HFI interrupt handler */
static irqreturn_t gen7_hwsched_hfi_handler(int irq, void *data)
{
	struct adreno_device *adreno_dev = data;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status = 0;

	/*
	 * GEN7_GMU_GMU2HOST_INTR_INFO may have bits set not specified in hfi->irq_mask.
	 * Read and clear only those irq bits that we are processing here.
	 */
	gmu_core_regread(device, GEN7_GMU_GMU2HOST_INTR_INFO, &status);
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR, status & hfi->irq_mask);

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

static int check_ack_failure(struct adreno_device *adreno_dev,
	struct pending_cmd *ack)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (ack->results[2] != 0xffffffff)
		return 0;

	dev_err(&gmu->pdev->dev,
		"ACK error: sender id %d seqnum %d\n",
		MSG_HDR_GET_ID(ack->sent_hdr),
		MSG_HDR_GET_SEQNUM(ack->sent_hdr));

	return -EINVAL;
}

int gen7_hfi_send_cmd_async(struct adreno_device *adreno_dev, void *data, u32 size_bytes)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	u32 *cmd = data;
	u32 seqnum;
	int rc;
	struct pending_cmd pending_ack;

	seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	*cmd = MSG_HDR_SET_SEQNUM_SIZE(*cmd, seqnum, size_bytes >> 2);

	add_waiter(hfi, *cmd, &pending_ack);

	rc = gen7_hfi_cmdq_write(adreno_dev, cmd, size_bytes);
	if (rc)
		goto done;

	rc = adreno_hwsched_wait_ack_completion(adreno_dev, &gmu->pdev->dev, &pending_ack,
		gen7_hwsched_process_msgq);
	if (rc)
		goto done;

	rc = check_ack_failure(adreno_dev, &pending_ack);

done:
	del_waiter(hfi, &pending_ack);

	return rc;
}

static void init_queues(struct gen7_hfi *hfi, bool lpac_enabled)
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
			/* 2 DQs for RB priority 3 */
			DEFINE_QHDR(gmuaddr, 14, 3),
			DEFINE_QHDR(gmuaddr, 15, 3),
			/* 1 DQ for LPAC RB if LPAC is enabled */
			DEFINE_QHDR(gmuaddr, 16, lpac_enabled ? 4 : 3),
		},
	};

	memcpy(hfi->hfi_mem->hostptr, &hfi_table, sizeof(hfi_table));
}

/* Total header sizes + queue sizes + 16 for alignment */
#define HFIMEM_SIZE (sizeof(struct hfi_queue_table) + 16 + \
	(SZ_4K * HFI_QUEUE_MAX))

static int hfi_f2h_main(void *arg);

int gen7_hwsched_hfi_init(struct adreno_device *adreno_dev)
{
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct gen7_hfi *hfi = to_gen7_hfi(adreno_dev);

	if (IS_ERR_OR_NULL(hw_hfi->big_ib)) {
		hw_hfi->big_ib = gen7_reserve_gmu_kernel_block(
				to_gen7_gmu(adreno_dev), 0,
				HWSCHED_MAX_IBS * sizeof(struct hfi_issue_ib),
				GMU_NONCACHED_KERNEL, 0);
		if (IS_ERR(hw_hfi->big_ib))
			return PTR_ERR(hw_hfi->big_ib);
	}

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LSR) &&
			IS_ERR_OR_NULL(hw_hfi->big_ib_recurring)) {
		hw_hfi->big_ib_recurring = gen7_reserve_gmu_kernel_block(
				to_gen7_gmu(adreno_dev), 0,
				HWSCHED_MAX_IBS * sizeof(struct hfi_issue_ib),
				GMU_NONCACHED_KERNEL, 0);
		if (IS_ERR(hw_hfi->big_ib_recurring))
			return PTR_ERR(hw_hfi->big_ib_recurring);
	}

	if (IS_ERR_OR_NULL(hfi->hfi_mem)) {
		hfi->hfi_mem = gen7_reserve_gmu_kernel_block(
				to_gen7_gmu(adreno_dev),
				0, HFIMEM_SIZE, GMU_NONCACHED_KERNEL, 0);
		if (IS_ERR(hfi->hfi_mem))
			return PTR_ERR(hfi->hfi_mem);
		init_queues(hfi, adreno_dev->lpac_enabled);
	}

	if (IS_ERR_OR_NULL(hw_hfi->f2h_task)) {
		hw_hfi->f2h_task = kthread_run(hfi_f2h_main, adreno_dev, "gmu_f2h");
		if (!IS_ERR(hw_hfi->f2h_task))
			sched_set_fifo(hw_hfi->f2h_task);
	}

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
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	u32 vma_id = (flags & HFI_MEMFLAG_GMU_CACHEABLE) ? GMU_CACHE : GMU_NONCACHED_KERNEL;
	struct hfi_mem_alloc_desc *desc = &entry->desc;

	return gen7_gmu_import_buffer(gmu, vma_id, entry->md, desc->size, get_attrs(flags));
}

static struct hfi_mem_alloc_entry *lookup_mem_alloc_table(
	struct adreno_device *adreno_dev, struct hfi_mem_alloc_desc *desc)
{
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);
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
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct hfi_mem_alloc_entry *entry =
		lookup_mem_alloc_table(adreno_dev, desc);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	u64 flags = 0;
	u32 priv = 0;
	int ret;
	const char *memkind_string = desc->mem_kind < HFI_MEMKIND_MAX ?
			hfi_memkind_strings[desc->mem_kind] : "UNKNOWN";

	if (entry)
		return entry;

	if (desc->mem_kind >= HFI_MEMKIND_MAX) {
		dev_err(&gmu->pdev->dev, "Invalid mem kind: %d\n",
			desc->mem_kind);
		return ERR_PTR(-EINVAL);
	}

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

	if (!(desc->flags & HFI_MEMFLAG_GFX_ACC) &&
		(desc->mem_kind != HFI_MEMKIND_HW_FENCE)) {
		if (desc->mem_kind == HFI_MEMKIND_MMIO_IPC_CORE)
			entry->md = gen7_reserve_gmu_kernel_block_fixed(gmu, 0,
					desc->size,
					(desc->flags & HFI_MEMFLAG_GMU_CACHEABLE) ?
					GMU_CACHE : GMU_NONCACHED_KERNEL,
					"qcom,ipc-core", get_attrs(desc->flags),
					desc->va_align);
		else
			entry->md = gen7_reserve_gmu_kernel_block(gmu, 0,
					desc->size,
					(desc->flags & HFI_MEMFLAG_GMU_CACHEABLE) ?
					GMU_CACHE : GMU_NONCACHED_KERNEL,
					desc->va_align);

		if (IS_ERR(entry->md)) {
			int ret = PTR_ERR(entry->md);

			memset(entry, 0, sizeof(*entry));
			return ERR_PTR(ret);
		}
		entry->desc.size = entry->md->size;
		entry->desc.gmu_addr = entry->md->gmuaddr;

		goto done;
	}

	/*
	 * Use pre-allocated memory descriptors to map the HFI_MEMKIND_HW_FENCE and
	 * HFI_MEMKIND_MEMSTORE
	 */
	switch (desc->mem_kind) {
	case HFI_MEMKIND_HW_FENCE:
		entry->md = &adreno_dev->hwsched.hw_fence.memdesc;
		break;
	case HFI_MEMKIND_MEMSTORE:
		entry->md = device->memstore;
		break;
	default:
		entry->md = kgsl_allocate_global(device, desc->size, 0, flags,
			priv, memkind_string);
		break;
	}
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
	struct hfi_mem_alloc_desc desc = {0};
	struct hfi_mem_alloc_reply_cmd out = {0};
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	u32 seqnum;
	int ret;

	hfi_get_mem_alloc_desc(rcvd, &desc);

	ret = process_mem_alloc(adreno_dev, &desc);
	if (ret)
		return ret;

	memcpy(&out.desc, &desc, sizeof(out.desc));

	out.hdr = ACK_MSG_HDR(F2H_MSG_MEM_ALLOC);

	seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	out.hdr = MSG_HDR_SET_SEQNUM_SIZE(out.hdr, seqnum, sizeof(out) >> 2);

	out.req_hdr = *(u32 *)rcvd;

	return gen7_hfi_cmdq_write(adreno_dev, (u32 *)&out, sizeof(out));
}

static int send_start_msg(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 seqnum;
	int ret, rc = 0;
	struct hfi_start_cmd cmd;
	u32 rcvd[MAX_RCVD_SIZE];
	struct pending_cmd pending_ack = {0};

	ret = CMD_MSG_HDR(cmd, H2F_MSG_START);
	if (ret)
		return ret;

	seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	cmd.hdr = MSG_HDR_SET_SEQNUM_SIZE(cmd.hdr, seqnum, sizeof(cmd) >> 2);

	pending_ack.sent_hdr = cmd.hdr;

	rc = gen7_hfi_cmdq_write(adreno_dev, (u32 *)&cmd, sizeof(cmd));
	if (rc)
		return rc;

poll:
	rc = gmu_core_timed_poll_check(device, GEN7_GMU_GMU2HOST_INTR_INFO,
		HFI_IRQ_MSGQ_MASK, HFI_RSP_TIMEOUT, HFI_IRQ_MSGQ_MASK);

	if (rc) {
		dev_err(&gmu->pdev->dev,
			"Timed out processing MSG_START seqnum: %d\n",
			seqnum);
		gmu_core_fault_snapshot(device);
		return rc;
	}

	/* Clear the interrupt */
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR,
		HFI_IRQ_MSGQ_MASK);

	if (gen7_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd)) <= 0) {
		dev_err(&gmu->pdev->dev, "MSG_START: no payload\n");
		gmu_core_fault_snapshot(device);
		return -EINVAL;
	}

	if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
		rc = gen7_receive_ack_cmd(gmu, rcvd, &pending_ack);
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

static void reset_hfi_mem_records(struct adreno_device *adreno_dev)
{
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct kgsl_memdesc *md = NULL;
	u32 i;

	for (i = 0; i < hw_hfi->mem_alloc_entries; i++) {
		struct hfi_mem_alloc_desc *desc = &hw_hfi->mem_alloc_table[i].desc;

		if (desc->flags & HFI_MEMFLAG_HOST_INIT) {
			md = hw_hfi->mem_alloc_table[i].md;
			memset(md->hostptr, 0x0, md->size);
		}
	}
}

static void reset_hfi_queues(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_queue_table *tbl = gmu->hfi.hfi_mem->hostptr;
	u32 i;

	/* Flush HFI queues */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		struct hfi_queue_header *hdr = &tbl->qhdr[i];

		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		hdr->read_index = hdr->write_index;
	}
}

void gen7_hwsched_hfi_stop(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);

	hfi->irq_mask &= ~HFI_IRQ_MSGQ_MASK;

	/*
	 * In some corner cases, it is possible that GMU put TS_RETIRE
	 * on the msgq after we have turned off gmu interrupts. Hence,
	 * drain the queue one last time before we reset HFI queues.
	 */
	gen7_hwsched_process_msgq(adreno_dev);

	/* Drain the debug queue before we reset HFI queues */
	gen7_hwsched_process_dbgq(adreno_dev, false);

	kgsl_pwrctrl_axi(KGSL_DEVICE(adreno_dev), false);

	clear_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

	/*
	 * Reset the hfi host access memory records, As GMU expects hfi memory
	 * records to be clear in bootup.
	 */
	reset_hfi_mem_records(adreno_dev);
}

static void enable_async_hfi(struct adreno_device *adreno_dev)
{
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);

	hfi->irq_mask |= HFI_IRQ_MSGQ_MASK;

	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), GEN7_GMU_GMU2HOST_INTR_MASK,
		(u32)~hfi->irq_mask);
}

static int enable_preemption(struct adreno_device *adreno_dev)
{
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
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

	ret = gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_PREEMPTION, 1,
			data);
	if (ret)
		return ret;

	if (gen7_core->qos_value) {
		int i;

		for (i = 0; i < KGSL_PRIORITY_MAX_RB_LEVELS; i++) {
			if (!gen7_core->qos_value[i])
				continue;

			gen7_hfi_send_set_value(adreno_dev,
				HFI_VALUE_RB_GPU_QOS, i,
				gen7_core->qos_value[i]);
		}
	}

	if (device->pwrctrl.rt_bus_hint) {
		ret = gen7_hfi_send_set_value(adreno_dev, HFI_VALUE_RB_IB_RULE, 0,
			device->pwrctrl.rt_bus_hint);
		if (ret)
			device->pwrctrl.rt_bus_hint = 0;
	}

	/*
	 * Bits[3:0] contain the preemption timeout enable bit per ringbuffer
	 * Bits[31:4] contain the timeout in ms
	 */
	return gen7_hfi_send_set_value(adreno_dev, HFI_VALUE_BIN_TIME, 1,
		FIELD_PREP(GENMASK(31, 4), ADRENO_PREEMPT_TIMEOUT) |
		FIELD_PREP(GENMASK(3, 0), 0xf));

}

static int gen7_hfi_send_perfcounter_feature_ctrl(struct adreno_device *adreno_dev)
{
	/*
	 * Perfcounter retention is disabled by default in GMU firmware.
	 * In case perfcounter retention behavior is overwritten by sysfs
	 * setting dynamically, send this HFI feature with 'enable = 0' to
	 * disable this feature in GMU firmware.
	 */
	if (adreno_dev->perfcounter)
		return gen7_hfi_send_feature_ctrl(adreno_dev,
				HFI_FEATURE_PERF_NORETAIN, 0, 0);

	return 0;
}

u32 gen7_hwsched_hfi_get_value(struct adreno_device *adreno_dev, u32 prop)
{
	struct hfi_get_value_cmd cmd;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct pending_cmd pending_ack;
	int rc;
	u32 seqnum;

	rc = CMD_MSG_HDR(cmd, H2F_MSG_GET_VALUE);
	if (rc)
		return 0;

	seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	cmd.hdr = MSG_HDR_SET_SEQNUM_SIZE(cmd.hdr, seqnum, sizeof(cmd) >> 2);
	cmd.type = prop;
	cmd.subtype = 0;

	add_waiter(hfi, cmd.hdr, &pending_ack);

	rc = gen7_hfi_cmdq_write(adreno_dev, (u32 *)&cmd, sizeof(cmd));
	if (rc)
		goto done;

	rc = adreno_hwsched_wait_ack_completion(adreno_dev, &gmu->pdev->dev, &pending_ack,
		gen7_hwsched_process_msgq);

done:
	del_waiter(hfi, &pending_ack);

	if (rc || (pending_ack.results[2] == UINT_MAX))
		return 0;

	return pending_ack.results[2];
}

static int gen7_hfi_send_hw_fence_feature_ctrl(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	int ret;

	if (!test_bit(ADRENO_HWSCHED_HW_FENCE, &hwsched->flags))
		return 0;

	ret = gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_HW_FENCE, 1, 0);
	if (ret && (ret == -ENOENT)) {
		dev_err(&gmu->pdev->dev, "GMU doesn't support HW_FENCE feature\n");
		adreno_hwsched_deregister_hw_fence(hwsched->hw_fence.handle);
		return 0;
	}

	return ret;
}

static int gen7_hfi_send_dms_feature_ctrl(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	if (!test_bit(ADRENO_DEVICE_DMS, &adreno_dev->priv))
		return 0;

	ret = gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_DMS, 1, 0);
	if (ret == -ENOENT) {
		dev_err(&gmu->pdev->dev, "GMU doesn't support DMS feature\n");
		clear_bit(ADRENO_DEVICE_DMS, &adreno_dev->priv);
		adreno_dev->dms_enabled = false;
		return 0;
	}

	return ret;
}

int gen7_hwsched_hfi_start(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	reset_hfi_queues(adreno_dev);

	ret = gen7_gmu_hfi_start(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_send_generic_req(adreno_dev, &gmu->hfi.dcvs_table,
		sizeof(gmu->hfi.dcvs_table));
	if (ret)
		goto err;

	ret = gen7_hfi_send_generic_req(adreno_dev, &gmu->hfi.bw_table, sizeof(gmu->hfi.bw_table));
	if (ret)
		goto err;

	ret = gen7_hfi_send_acd_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_send_bcl_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_send_ifpc_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_HWSCHED, 1, 0);
	if (ret)
		goto err;

	ret = gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_KPROF, 1, 0);
	if (ret)
		goto err;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LSR)) {
		ret = gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_LSR,
				1, 0);
		if (ret)
			goto err;
	}

	ret = gen7_hfi_send_hw_fence_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_send_perfcounter_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_send_dms_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	/* Enable the long ib timeout detection */
	if (adreno_long_ib_detect(adreno_dev)) {
		ret = gen7_hfi_send_feature_ctrl(adreno_dev,
			HFI_FEATURE_BAIL_OUT_TIMER, 1, 0);
		if (ret)
			goto err;
	}

	if (gmu->log_stream_enable)
		gen7_hfi_send_set_value(adreno_dev,
			HFI_VALUE_LOG_STREAM_ENABLE, 0, 1);

	if (gmu->log_group_mask)
		gen7_hfi_send_set_value(adreno_dev,
			HFI_VALUE_LOG_GROUP, 0, gmu->log_group_mask);

	ret = gen7_hfi_send_core_fw_start(adreno_dev);
	if (ret)
		goto err;

	ret = enable_preemption(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_send_lpac_feature_ctrl(adreno_dev);
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
		gen7_hwsched_hfi_stop(adreno_dev);

	return ret;
}

static int submit_raw_cmds(struct adreno_device *adreno_dev, void *cmds, u32 size_bytes,
	const char *str)
{
	int ret;

	ret = gen7_hfi_send_cmd_async(adreno_dev, cmds, size_bytes);
	if (ret)
		return ret;

	ret = gmu_core_timed_poll_check(KGSL_DEVICE(adreno_dev),
			GEN7_GPU_GMU_AO_GPU_CX_BUSY_STATUS, 0, 200, BIT(23));
	if (ret)
		gen7_spin_idle_debug(adreno_dev, str);

	return ret;
}

static void spin_idle_debug_lpac(struct adreno_device *adreno_dev,
				const char *str)
{
	struct kgsl_device *device = &adreno_dev->dev;
	u32 rptr, wptr, status, status3, intstatus, hwfault;
	bool val = adreno_is_preemption_enabled(adreno_dev);

	dev_err(device->dev, str);

	kgsl_regread(device, GEN7_CP_LPAC_RB_RPTR, &rptr);
	kgsl_regread(device, GEN7_CP_LPAC_RB_WPTR, &wptr);

	kgsl_regread(device, GEN7_RBBM_STATUS, &status);
	kgsl_regread(device, GEN7_RBBM_STATUS3, &status3);
	kgsl_regread(device, GEN7_RBBM_INT_0_STATUS, &intstatus);
	kgsl_regread(device, GEN7_CP_HW_FAULT, &hwfault);

	dev_err(device->dev,
		"LPAC rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
		val ? KGSL_LPAC_RB_ID : 1, rptr, wptr,
		status, status3, intstatus);

	dev_err(device->dev, " hwfault=%8.8X\n", hwfault);

	kgsl_device_snapshot(device, NULL, NULL, false);
}

static int submit_lpac_raw_cmds(struct adreno_device *adreno_dev, void *cmds, u32 size_bytes,
	const char *str)
{
	int ret;

	ret = gen7_hfi_send_cmd_async(adreno_dev, cmds, size_bytes);
	if (ret)
		return ret;

	ret = gmu_core_timed_poll_check(KGSL_DEVICE(adreno_dev),
			GEN7_GPU_GMU_AO_GPU_LPAC_BUSY_STATUS, 0, 200, BIT(23));
	if (ret)
		spin_idle_debug_lpac(adreno_dev, str);

	return ret;
}

static int cp_init(struct adreno_device *adreno_dev)
{
	u32 cmds[GEN7_CP_INIT_DWORDS + 1];

	cmds[0] = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW, HFI_MSG_CMD);

	gen7_cp_init_cmds(adreno_dev, &cmds[1]);

	return submit_raw_cmds(adreno_dev, cmds, sizeof(cmds),
			"CP initialization failed to idle\n");
}

static int send_switch_to_unsecure(struct adreno_device *adreno_dev)
{
	u32 cmds[3];

	cmds[0] = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW, HFI_MSG_CMD);

	cmds[1] = cp_type7_packet(CP_SET_SECURE_MODE, 1);
	cmds[2] = 0;

	return submit_raw_cmds(adreno_dev, cmds, sizeof(cmds),
			"Switch to unsecure failed to idle\n");
}

int gen7_hwsched_cp_init(struct adreno_device *adreno_dev)
{
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Program the ucode base for CP */
	kgsl_regwrite(device, GEN7_CP_SQE_INSTR_BASE_LO,
		lower_32_bits(fw->memdesc->gpuaddr));
	kgsl_regwrite(device, GEN7_CP_SQE_INSTR_BASE_HI,
		upper_32_bits(fw->memdesc->gpuaddr));

	ret = cp_init(adreno_dev);
	if (ret)
		return ret;

	ret = adreno_zap_shader_load(adreno_dev, gen7_core->zap_name);
	if (ret)
		return ret;

	if (!adreno_dev->zap_loaded)
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			GEN7_RBBM_SECVID_TRUST_CNTL, 0x0);
	else
		ret = send_switch_to_unsecure(adreno_dev);

	return ret;
}

int gen7_hwsched_lpac_cp_init(struct adreno_device *adreno_dev)
{
	u32 cmds[GEN7_CP_INIT_DWORDS + 1];

	if (!adreno_dev->lpac_enabled)
		return 0;

	cmds[0] = CREATE_MSG_HDR(H2F_MSG_ISSUE_LPAC_CMD_RAW, HFI_MSG_CMD);

	gen7_cp_init_cmds(adreno_dev, &cmds[1]);

	return submit_lpac_raw_cmds(adreno_dev, cmds, sizeof(cmds),
			"LPAC CP initialization failed to idle\n");
}

static bool is_queue_empty(struct adreno_device *adreno_dev, u32 queue_idx)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
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
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);

	while (!kthread_should_stop()) {
		wait_event_interruptible(hfi->f2h_wq, kthread_should_stop() ||
			(!(is_queue_empty(adreno_dev, HFI_MSG_ID) &&
			is_queue_empty(adreno_dev, HFI_DBG_ID)) &&
			(hfi->irq_mask & HFI_IRQ_MSGQ_MASK)));

		if (kthread_should_stop())
			break;

		gen7_hwsched_process_msgq(adreno_dev);
		gen7_hwsched_process_dbgq(adreno_dev, true);
	}

	return 0;
}

int gen7_hwsched_hfi_probe(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);

	gmu->hfi.irq = kgsl_request_irq(gmu->pdev, "hfi",
		gen7_hwsched_hfi_handler, adreno_dev);

	if (gmu->hfi.irq < 0)
		return gmu->hfi.irq;

	hw_hfi->irq_mask = HFI_IRQ_MASK;

	rwlock_init(&hw_hfi->msglock);

	INIT_LIST_HEAD(&hw_hfi->msglist);

	init_waitqueue_head(&hw_hfi->f2h_wq);

	mutex_init(&hw_hfi->msgq_mutex);

	return 0;
}

void gen7_hwsched_hfi_remove(struct adreno_device *adreno_dev)
{
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);

	if (hw_hfi->f2h_task)
		kthread_stop(hw_hfi->f2h_task);
}

static void gen7_add_profile_events(struct adreno_device *adreno_dev,
	struct kgsl_drawobj_cmd *cmdobj, struct adreno_submit_time *time)
{
	unsigned long flags;
	u64 time_in_s;
	unsigned long time_in_ns;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct kgsl_context *context = drawobj->context;
	struct submission_info info = {0};
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;

	if (!time)
		return;

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
	time->ticks = gen7_read_alwayson(adreno_dev);

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

	info.inflight = hwsched->inflight;
	info.rb_id = adreno_get_level(context);
	info.gmu_dispatch_queue = context->gmu_dispatch_queue;

	cmdobj->submit_ticks = time->ticks;

	msm_perf_events_update(MSM_PERF_GFX, MSM_PERF_SUBMIT,
		pid_nr(context->proc_priv->pid),
		context->id, drawobj->timestamp,
		!!(drawobj->flags & KGSL_DRAWOBJ_END_OF_FRAME));
	trace_adreno_cmdbatch_submitted(drawobj, &info, time->ticks,
		(unsigned long) time_in_s, time_in_ns / 1000, 0);

	log_kgsl_cmdbatch_submitted_event(context->id, drawobj->timestamp,
			context->priority, drawobj->flags);
}

static void init_gmu_context_queue(struct adreno_context *drawctxt)
{
	struct kgsl_memdesc *md = &drawctxt->gmu_context_queue;
	struct gmu_context_queue_header *hdr = md->hostptr;

	hdr->start_addr = md->gmuaddr + sizeof(*hdr);
	hdr->queue_size = (md->size - sizeof(*hdr)) >> 2;
	hdr->hw_fence_buffer_va = drawctxt->gmu_hw_fence_queue.gmuaddr;
	hdr->hw_fence_buffer_size = drawctxt->gmu_hw_fence_queue.size;
}

static u32 get_dq_id(struct adreno_device *adreno_dev, struct kgsl_context *context)
{
	struct dq_info *info;
	u32 next;
	u32 priority = adreno_get_level(context);

	if (adreno_dev->lpac_enabled)
		info = &gen7_hfi_dqs_lpac[priority];
	else
		info = &gen7_hfi_dqs[priority];

	next = info->base_dq_id + info->offset;

	info->offset = (info->offset + 1) % info->max_dq;

	return next;
}

static int allocate_context_queues(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt)
{
	int ret = 0;

	if (!adreno_hwsched_context_queue_enabled(adreno_dev))
		return 0;

	if (test_bit(ADRENO_HWSCHED_HW_FENCE, &adreno_dev->hwsched.flags) &&
		!drawctxt->gmu_hw_fence_queue.gmuaddr) {
		ret = gen7_alloc_gmu_kernel_block(
			to_gen7_gmu(adreno_dev), &drawctxt->gmu_hw_fence_queue,
			HW_FENCE_QUEUE_SIZE, GMU_NONCACHED_KERNEL,
			IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV);
		if (ret) {
			memset(&drawctxt->gmu_hw_fence_queue, 0x0,
				sizeof(drawctxt->gmu_hw_fence_queue));
			return ret;
		}
	}

	if (!drawctxt->gmu_context_queue.gmuaddr) {
		ret = gen7_alloc_gmu_kernel_block(
			to_gen7_gmu(adreno_dev), &drawctxt->gmu_context_queue,
			SZ_4K, GMU_NONCACHED_KERNEL,
			IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV);
		if (ret) {
			memset(&drawctxt->gmu_context_queue, 0x0,
				sizeof(drawctxt->gmu_context_queue));
			return ret;
		}
		init_gmu_context_queue(drawctxt);
	}

	return 0;
}

static int send_context_register(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct hfi_register_ctxt_cmd cmd;
	struct kgsl_pagetable *pt = context->proc_priv->pagetable;
	int ret, asid = kgsl_mmu_pagetable_get_asid(pt, context);

	if (asid < 0)
		return asid;

	ret = CMD_MSG_HDR(cmd, H2F_MSG_REGISTER_CONTEXT);
	if (ret)
		return ret;

	ret = allocate_context_queues(adreno_dev, drawctxt);
	if (ret)
		return ret;

	cmd.ctxt_id = context->id;
	cmd.flags = HFI_CTXT_FLAG_NOTIFY | context->flags;

	/*
	 * HLOS SMMU driver programs context bank to look up ASID from TTBR0 during a page
	 * table walk. So the TLB entries are tagged with the ASID from TTBR0. TLBIASID
	 * invalidates TLB entries whose ASID matches the value that was written to the
	 * CBn_TLBIASID register. Set ASID along with PT address.
	 */
	cmd.pt_addr = kgsl_mmu_pagetable_get_ttbr0(pt) |
		FIELD_PREP(GENMASK_ULL(63, KGSL_IOMMU_ASID_START_BIT), asid);
	cmd.ctxt_idr = context->id;
	cmd.ctxt_bank = kgsl_mmu_pagetable_get_context_bank(pt, context);

	return gen7_hfi_send_cmd_async(adreno_dev, &cmd, sizeof(cmd));
}

static int send_context_pointers(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_context_pointers_cmd cmd = {0};
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
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

	if (adreno_hwsched_context_queue_enabled(adreno_dev))
		cmd.gmu_context_queue_addr = drawctxt->gmu_context_queue.gmuaddr;

	return gen7_hfi_send_cmd_async(adreno_dev, &cmd, sizeof(cmd));
}

static int hfi_context_register(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (context->gmu_registered)
		return 0;

	ret = send_context_register(adreno_dev, context);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to register context %u: %d\n",
			context->id, ret);

		if (device->gmu_fault)
			adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);

		return ret;
	}

	ret = send_context_pointers(adreno_dev, context);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to register context %u pointers: %d\n",
			context->id, ret);

		if (device->gmu_fault)
			adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);

		return ret;
	}

	context->gmu_registered = true;
	if (adreno_hwsched_context_queue_enabled(adreno_dev))
		context->gmu_dispatch_queue = UINT_MAX;
	else
		context->gmu_dispatch_queue = get_dq_id(adreno_dev, context);

	return 0;
}

static void populate_ibs(struct adreno_device *adreno_dev,
	struct hfi_submit_cmd *cmd, struct kgsl_drawobj_cmd *cmdobj)
{
	struct hfi_issue_ib *issue_ib;
	struct kgsl_memobj_node *ib;

	if (cmdobj->numibs > HWSCHED_MAX_DISPATCH_NUMIBS) {
		struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
		struct kgsl_memdesc *big_ib;

		if (test_bit(CMDOBJ_RECURRING_START, &cmdobj->priv))
			big_ib = hfi->big_ib_recurring;
		else
			big_ib = hfi->big_ib;
		/*
		 * The dispatcher ensures that there is only one big IB inflight
		 */
		cmd->big_ib_gmu_va = big_ib->gmuaddr;
		cmd->flags |= CMDBATCH_INDIRECT;
		issue_ib = big_ib->hostptr;
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

int gen7_gmu_context_queue_write(struct adreno_device *adreno_dev,
	struct kgsl_memdesc *gmu_context_queue, u32 *msg, u32 size_bytes,
	struct kgsl_drawobj *drawobj, struct adreno_submit_time *time)
{
	struct gmu_context_queue_header *hdr = gmu_context_queue->hostptr;
	u32 *queue = gmu_context_queue->hostptr + sizeof(*hdr);
	u32 i, empty_space, write_idx = hdr->write_index, read_idx = hdr->read_index;
	u32 size_dwords = size_bytes >> 2;
	u32 align_size = ALIGN(size_dwords, SZ_4);
	u32 id = MSG_HDR_GET_ID(*msg);
	struct kgsl_drawobj_cmd *cmdobj = NULL;

	empty_space = (write_idx >= read_idx) ?
			(hdr->queue_size - (write_idx - read_idx))
			: (read_idx - write_idx);

	if (empty_space <= align_size)
		return -ENOSPC;

	if (!IS_ALIGNED(size_bytes, sizeof(u32)))
		return -EINVAL;

	for (i = 0; i < size_dwords; i++) {
		queue[write_idx] = msg[i];
		write_idx = (write_idx + 1) % hdr->queue_size;
	}

	/* Cookify any non used data at the end of the write buffer */
	for (; i < align_size; i++) {
		queue[write_idx] = 0xfafafafa;
		write_idx = (write_idx + 1) % hdr->queue_size;
	}

	/* Ensure packet is written out before proceeding */
	wmb();

	if (!drawobj)
		goto done;

	if (drawobj->type & SYNCOBJ_TYPE) {
		struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);

		trace_adreno_syncobj_submitted(drawobj->context->id, drawobj->timestamp,
			syncobj->numsyncs, gen7_read_alwayson(adreno_dev));
		goto done;
	}

	cmdobj = CMDOBJ(drawobj);

	gen7_add_profile_events(adreno_dev, cmdobj, time);

	/*
	 * Put the profiling information in the user profiling buffer.
	 * The hfi_update_write_idx below has a wmb() before the actual
	 * write index update to ensure that the GMU does not see the
	 * packet before the profile data is written out.
	 */
	adreno_profile_submit_time(time);

done:
	trace_kgsl_hfi_send(id, size_dwords, MSG_HDR_GET_SEQNUM(*msg));

	hfi_update_write_idx(&hdr->write_index, write_idx);

	return 0;
}

static u32 get_irq_bit(struct adreno_device *adreno_dev, struct kgsl_drawobj *drawobj)
{
	if (!adreno_hwsched_context_queue_enabled(adreno_dev))
		return drawobj->context->gmu_dispatch_queue;

	if (adreno_is_preemption_enabled(adreno_dev))
		return adreno_get_level(drawobj->context);

	if (kgsl_context_is_lpac(drawobj->context))
		return 1;

	return 0;
}

static int add_gmu_waiter(struct adreno_device *adreno_dev,
	struct dma_fence *fence)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = msm_hw_fence_wait_update(adreno_dev->hwsched.hw_fence.handle,
			&fence, 1, true);

	if (ret)
		dev_err_ratelimited(device->dev,
			"Failed to add GMU as waiter ret:%d fence ctx:%ld ts:%ld\n",
			ret, fence->context, fence->seqno);

	return ret;
}

static void populate_kgsl_fence(struct hfi_syncobj *obj,
	struct dma_fence *fence)
{
	struct kgsl_sync_fence *kfence = (struct kgsl_sync_fence *)fence;
	struct kgsl_sync_timeline *ktimeline = kfence->parent;
	unsigned long flags;

	obj->flags |= GMU_SYNCOBJ_KGSL_FENCE;

	spin_lock_irqsave(&ktimeline->lock, flags);
	/* This means that the context is going away. Mark the fence as triggered */
	if (!ktimeline->context) {
		obj->flags |= GMU_SYNCOBJ_RETIRED;
		spin_unlock_irqrestore(&ktimeline->lock, flags);
		return;
	}
	obj->ctxt_id = ktimeline->context->id;
	spin_unlock_irqrestore(&ktimeline->lock, flags);

	obj->seq_no =  kfence->timestamp;
}

static int _submit_hw_fence(struct adreno_device *adreno_dev,
	struct kgsl_drawobj *drawobj, void *cmdbuf)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	int i, j;
	u32 cmd_sizebytes;
	struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);
	struct hfi_submit_syncobj *cmd;
	struct hfi_syncobj *obj = NULL;
	u32 seqnum;

	/* Add hfi_syncobj struct for sync object */
	cmd_sizebytes = sizeof(*cmd) +
			(sizeof(struct hfi_syncobj) *
			syncobj->num_hw_fence);

	if (WARN_ON(cmd_sizebytes > HFI_MAX_MSG_SIZE))
		return -EMSGSIZE;

	memset(cmdbuf, 0x0, cmd_sizebytes);
	cmd = cmdbuf;
	cmd->num_syncobj = syncobj->num_hw_fence;
	obj = (struct hfi_syncobj *)&cmd[1];

	for (i = 0; i < syncobj->numsyncs; i++) {
		struct kgsl_drawobj_sync_event *event = &syncobj->synclist[i];
		struct kgsl_sync_fence_cb *kcb = event->handle;
		struct dma_fence **fences;
		struct dma_fence_array *array;
		u32 num_fences;

		if (!kcb)
			return -EINVAL;

		array = to_dma_fence_array(kcb->fence);
		if (array != NULL) {
			num_fences = array->num_fences;
			fences = array->fences;
		} else {
			num_fences = 1;
			fences = &kcb->fence;
		}

		for (j = 0; j < num_fences; j++) {

			if (is_kgsl_fence(fences[j])) {
				populate_kgsl_fence(obj, fences[j]);
			} else {
				int ret = add_gmu_waiter(adreno_dev, fences[j]);

				if (ret) {
					syncobj->flags &= ~KGSL_SYNCOBJ_HW;
					return ret;
				}

				obj->ctxt_id = fences[j]->context;
				obj->seq_no =  fences[j]->seqno;
			}
			trace_adreno_input_hw_fence(drawobj->context->id, obj->ctxt_id,
				obj->seq_no, obj->flags, fences[j]->ops->get_timeline_name ?
				fences[j]->ops->get_timeline_name(fences[j]) : "unknown");

			obj++;
		}
	}

	/*
	 * Attach a timestamp to this SYNCOBJ to keep track whether GMU has deemed it signaled
	 * or not.
	 */
	drawobj->timestamp = ++drawctxt->syncobj_timestamp;
	cmd->timestamp = drawobj->timestamp;

	seqnum = atomic_inc_return(&adreno_dev->hwsched.submission_seqnum);
	cmd->hdr = CREATE_MSG_HDR(H2F_MSG_ISSUE_SYNCOBJ, HFI_MSG_CMD);
	cmd->hdr = MSG_HDR_SET_SEQNUM_SIZE(cmd->hdr, seqnum, cmd_sizebytes >> 2);

	return gen7_gmu_context_queue_write(adreno_dev, &drawctxt->gmu_context_queue,
			(u32 *)cmd, cmd_sizebytes, drawobj, NULL);

}

/**
 * check_context_inflight_fences - When this context has been un-registered with the GMU, make sure
 * all the hardware fences(that were sent to GMU) for this context have been sent to TxQueue. If
 * not, then log an error, take a snapshot and trigger recovery.
 */
static int check_context_inflight_fences(struct adreno_device *adreno_dev,
	struct adreno_context *input_ctxt)
{
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	struct adreno_hw_fence_entry *fence, *tmp;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;

	list_for_each_entry_safe(fence, tmp, &hwsched->hw_fence_list, node) {
		struct kgsl_sync_fence *kfence = fence->kfence;
		struct adreno_context *drawctxt = fence->drawctxt;
		struct gmu_context_queue_header *hdr =
				drawctxt->gmu_context_queue.hostptr;

		if (drawctxt != input_ctxt)
			continue;

		if (timestamp_cmp(kfence->timestamp, hdr->out_fence_ts) > 0) {
			dev_err(device->dev, "detached ctx:%d has unsignaled fence ts:%d retired:%d\n",
				input_ctxt->base.id, kfence->timestamp, hdr->out_fence_ts);
			/* Take snapshot and trigger recovery if there are un-triggered fences */
			gmu_core_fault_snapshot(device);
			adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
			return -EINVAL;
		}

		adreno_hwsched_remove_hw_fence_entry(adreno_dev, fence);
	}

	return ret;
}

/**
 * gen7_send_hw_fence_hfi - This function sends the hardware fence info to the GMU using the
 * H2F_MSG_HW_FENCE_INFO packet. If GMU acks the packet with an error code, that means GMU
 * can't process this packet. Hence, return error to the caller to indicate that this fence
 * cannot be treated as a hardware fence.
 */
static int gen7_send_hw_fence_hfi(struct adreno_device *adreno_dev,
	struct adreno_hw_fence_entry *entry, u32 flags)
{
	struct kgsl_sync_fence *kfence = entry->kfence;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_hw_fence_info cmd = {0};
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	int ret = 0;
	struct pending_cmd pending_ack;

	ret = CMD_MSG_HDR(cmd, H2F_MSG_HW_FENCE_INFO);
	if (ret)
		return ret;

	cmd.gmu_ctxt_id = entry->drawctxt->base.id;
	cmd.ctxt_id = kfence->fence.context;
	cmd.ts = kfence->fence.seqno;
	cmd.flags |= flags;

	cmd.hash_index = kfence->hw_fence_index;

	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr,
		atomic_inc_return(&gmu->hfi.seqnum));

	add_waiter(hfi, cmd.hdr, &pending_ack);

	ret = gen7_hfi_cmdq_write(adreno_dev, (u32 *)&cmd, sizeof(cmd));
	if (ret)
		goto done;

	ret = adreno_hwsched_wait_ack_completion(adreno_dev, &gmu->pdev->dev, &pending_ack,
		gen7_hwsched_process_msgq);
	if (ret)
		goto done;

	ret = check_ack_failure(adreno_dev, &pending_ack);

	/*
	 * A non-zero value in pending_ack.results[2] means GMU failed to accept this fence. Return
	 * this value to the caller to indicate this failure.
	 */
	if (!ret)
		ret = pending_ack.results[2];

done:
	del_waiter(hfi, &pending_ack);

	return ret;
}

/* Request GMU to add this fence to TxQueue without checking memstore */
int gen7_hwsched_trigger_hw_fence(struct adreno_device *adreno_dev,
	struct adreno_hw_fence_entry *entry)
{
	return gen7_send_hw_fence_hfi(adreno_dev, entry, HW_FENCE_FLAG_SKIP_MEMSTORE);
}

int gen7_hwsched_send_hw_fence(struct adreno_device *adreno_dev,
	struct adreno_hw_fence_entry *entry)
{
	struct adreno_context *drawctxt = entry->drawctxt;
	int ret = 0;

	/*
	 * This function may be called after hang recovery to send pending hardware fences to the
	 * GMU. So make sure we register the context first.
	 */
	ret = hfi_context_register(adreno_dev, &drawctxt->base);
	if (ret)
		return ret;

	return gen7_send_hw_fence_hfi(adreno_dev, entry, 0);
}

/**
 * process_hw_fence_queue - This function walks the draw context's list of hardware fences
 * and sends the ones which have a timestamp less than or equal to the timestamp that just
 * got submitted to the GMU.
 */
static int process_hw_fence_queue(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt, u32 ts)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_hw_fence_entry *entry = NULL, *next;
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	int ret = 0;

	/* This list is sorted with smallest timestamp at head and highest timestamp at tail */
	list_for_each_entry_safe(entry, next, &drawctxt->hw_fence_list, node) {
		struct kgsl_sync_fence *kfence = entry->kfence;

		if (timestamp_cmp(kfence->timestamp, ts) > 0)
			break;

		ret = gen7_send_hw_fence_hfi(adreno_dev, entry, 0);
		if (!ret) {
			list_del_init(&entry->node);
			/*
			 * A fence that is sent to GMU must be added to the hwsched hardware fence
			 * list so that we can keep track of when GMU sends it to the TxQueue
			 */
			list_add_tail(&entry->node, &hwsched->hw_fence_list);
			continue;
		}

		/* Trigger recovery if we hit a GMU fault while sending this fence to GMU */
		if (device->gmu_fault)
			adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
		break;
	}

	return ret;
}

/* Size in below functions are in unit of dwords */
static int gen7_hfi_dispatch_queue_write(struct adreno_device *adreno_dev, u32 queue_idx,
	u32 *msg, u32 size_bytes, struct kgsl_drawobj_cmd *cmdobj, struct adreno_submit_time *time)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_queue_table *tbl = gmu->hfi.hfi_mem->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	u32 *queue;
	u32 i, write, empty_space;
	u32 size_dwords = size_bytes >> 2;
	u32 align_size = ALIGN(size_dwords, SZ_4);
	u32 id = MSG_HDR_GET_ID(*msg);

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED || !IS_ALIGNED(size_bytes, sizeof(u32)))
		return -EINVAL;

	queue = HOST_QUEUE_START_ADDR(gmu->hfi.hfi_mem, queue_idx);

	empty_space = (hdr->write_index >= hdr->read_index) ?
			(hdr->queue_size - (hdr->write_index - hdr->read_index))
			: (hdr->read_index - hdr->write_index);

	if (empty_space <= align_size)
		return -ENOSPC;

	write = hdr->write_index;

	for (i = 0; i < size_dwords; i++) {
		queue[write] = msg[i];
		write = (write + 1) % hdr->queue_size;
	}

	/* Cookify any non used data at the end of the write buffer */
	for (; i < align_size; i++) {
		queue[write] = 0xfafafafa;
		write = (write + 1) % hdr->queue_size;
	}

	/* Ensure packet is written out before proceeding */
	wmb();

	if (!cmdobj)
		goto done;

	gen7_add_profile_events(adreno_dev, cmdobj, time);

	/*
	 * Put the profiling information in the user profiling buffer.
	 * The hfi_update_write_idx below has a wmb() before the actual
	 * write index update to ensure that the GMU does not see the
	 * packet before the profile data is written out.
	 */
	adreno_profile_submit_time(time);

done:
	trace_kgsl_hfi_send(id, size_dwords, MSG_HDR_GET_SEQNUM(*msg));

	hfi_update_write_idx(&hdr->write_index, write);

	return 0;
}

int gen7_hwsched_submit_drawobj(struct adreno_device *adreno_dev, struct kgsl_drawobj *drawobj)
{
	int ret = 0;
	u32 cmd_sizebytes, seqnum;
	struct kgsl_drawobj_cmd *cmdobj = NULL;
	struct hfi_submit_cmd *cmd;
	struct adreno_submit_time time = {0};
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	static void *cmdbuf;

	if (cmdbuf == NULL) {
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

		cmdbuf = devm_kzalloc(&device->pdev->dev, HFI_MAX_MSG_SIZE,
				GFP_KERNEL);
		if (!cmdbuf)
			return -ENOMEM;
	}

	ret = hfi_context_register(adreno_dev, drawobj->context);
	if (ret)
		return ret;

	if ((drawobj->type & SYNCOBJ_TYPE) != 0)
		return _submit_hw_fence(adreno_dev, drawobj, cmdbuf);

	cmdobj = CMDOBJ(drawobj);

	/*
	 * If the MARKER object is retired, it doesn't need to be dispatched to GMU. Simply trigger
	 * any pending fences that are less than/equal to this object's timestamp.
	 */
	if (test_bit(CMDOBJ_MARKER_EXPIRED, &cmdobj->priv))
		return process_hw_fence_queue(adreno_dev, drawctxt, drawobj->timestamp);

	/* Add a *issue_ib struct for each IB */
	if (cmdobj->numibs > HWSCHED_MAX_DISPATCH_NUMIBS ||
		test_bit(CMDOBJ_SKIP, &cmdobj->priv))
		cmd_sizebytes = sizeof(*cmd);
	else
		cmd_sizebytes = sizeof(*cmd) +
			(sizeof(struct hfi_issue_ib) * cmdobj->numibs);

	if (WARN_ON(cmd_sizebytes > HFI_MAX_MSG_SIZE))
		return -EMSGSIZE;

	memset(cmdbuf, 0x0, cmd_sizebytes);

	cmd = cmdbuf;

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

	seqnum = atomic_inc_return(&adreno_dev->hwsched.submission_seqnum);
	cmd->hdr = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD, HFI_MSG_CMD);
	cmd->hdr = MSG_HDR_SET_SEQNUM_SIZE(cmd->hdr, seqnum, cmd_sizebytes >> 2);

	if (adreno_hwsched_context_queue_enabled(adreno_dev))
		ret = gen7_gmu_context_queue_write(adreno_dev,
			&drawctxt->gmu_context_queue, (u32 *)cmd, cmd_sizebytes, drawobj, &time);
	else
		ret = gen7_hfi_dispatch_queue_write(adreno_dev,
			HFI_DSP_ID_0 + drawobj->context->gmu_dispatch_queue,
			(u32 *)cmd, cmd_sizebytes, cmdobj, &time);
	if (ret)
		return ret;

	/* Send interrupt to GMU to receive the message */
	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), GEN7_GMU_HOST2GMU_INTR_SET,
		DISPQ_IRQ_BIT(get_irq_bit(adreno_dev, drawobj)));

	return process_hw_fence_queue(adreno_dev, drawctxt, drawobj->timestamp);
}

int gen7_hwsched_send_recurring_cmdobj(struct adreno_device *adreno_dev,
	struct kgsl_drawobj_cmd *cmdobj)
{
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct hfi_submit_cmd *cmd;
	struct kgsl_memobj_node *ib;
	u32 cmd_sizebytes;
	int ret;
	static bool active;

	if (adreno_gpu_halt(adreno_dev) || hwsched_in_fault(hwsched))
		return -EBUSY;

	if (test_bit(CMDOBJ_RECURRING_STOP, &cmdobj->priv)) {
		cmdobj->numibs = 0;
	} else {
		list_for_each_entry(ib, &cmdobj->cmdlist, node)
			cmdobj->numibs++;
	}

	if (cmdobj->numibs > HWSCHED_MAX_IBS)
		return -EINVAL;

	if (cmdobj->numibs > HWSCHED_MAX_DISPATCH_NUMIBS)
		cmd_sizebytes = sizeof(*cmd);
	else
		cmd_sizebytes = sizeof(*cmd) +
			(sizeof(struct hfi_issue_ib) * cmdobj->numibs);

	if (WARN_ON(cmd_sizebytes > HFI_MAX_MSG_SIZE))
		return -EMSGSIZE;

	cmd = kzalloc(cmd_sizebytes, GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;

	if (test_bit(CMDOBJ_RECURRING_START, &cmdobj->priv)) {
		if (!active) {
			ret = adreno_active_count_get(adreno_dev);
			if (ret) {
				kfree(cmd);
				return ret;
			}
			active = true;
		}
		cmd->flags |= CMDBATCH_RECURRING_START;
		populate_ibs(adreno_dev, cmd, cmdobj);
	} else
		cmd->flags |= CMDBATCH_RECURRING_STOP;

	cmd->ctxt_id = drawobj->context->id;

	ret = hfi_context_register(adreno_dev, drawobj->context);
	if (ret) {
		adreno_active_count_put(adreno_dev);
		active = false;
		kfree(cmd);
		return ret;
	}

	cmd->hdr = CREATE_MSG_HDR(H2F_MSG_ISSUE_RECURRING_CMD, HFI_MSG_CMD);

	ret = gen7_hfi_send_cmd_async(adreno_dev, cmd, sizeof(*cmd));

	kfree(cmd);

	if (ret) {
		adreno_active_count_put(adreno_dev);
		active = false;
		return ret;
	}

	if (test_bit(CMDOBJ_RECURRING_STOP, &cmdobj->priv)) {
		adreno_hwsched_retire_cmdobj(hwsched, hwsched->recurring_cmdobj);
		del_timer_sync(&hwsched->lsr_timer);
		hwsched->recurring_cmdobj = NULL;
		if (active)
			adreno_active_count_put(adreno_dev);
		active = false;
		return ret;
	}

	hwsched->recurring_cmdobj = cmdobj;
	/* Star LSR timer for power stats collection */
	mod_timer(&hwsched->lsr_timer, jiffies + msecs_to_jiffies(10));
	return ret;
}

/* We don't want to unnecessarily wake the GMU to trigger hardware fences */
static void drain_context_hw_fence_cpu(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt)
{
	struct adreno_hw_fence_entry *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, &drawctxt->hw_fence_list, node) {

		adreno_hwsched_trigger_hw_fence_cpu(adreno_dev, entry);

		adreno_hwsched_remove_hw_fence_entry(adreno_dev, entry);
	}
}

int gen7_hwsched_drain_context_hw_fences(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt)
{
	struct adreno_hw_fence_entry *entry, *tmp;
	int ret = 0;

	list_for_each_entry_safe(entry, tmp, &drawctxt->hw_fence_list, node) {

		ret = gen7_hwsched_trigger_hw_fence(adreno_dev, entry);
		if (ret)
			return ret;

		adreno_hwsched_remove_hw_fence_entry(adreno_dev, entry);
	}

	return 0;
}

static int send_context_unregister_hfi(struct adreno_device *adreno_dev,
	struct kgsl_context *context, u32 ts)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct pending_cmd pending_ack;
	struct hfi_unregister_ctxt_cmd cmd;
	u32 seqnum;
	int rc, ret;

	/* Only send HFI if device is not in SLUMBER */
	if (!context->gmu_registered ||
		!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags)) {
		drain_context_hw_fence_cpu(adreno_dev, drawctxt);
		return 0;
	}

	ret = CMD_MSG_HDR(cmd, H2F_MSG_UNREGISTER_CONTEXT);
	if (ret)
		return ret;

	cmd.ctxt_id = context->id,
	cmd.ts = ts,

	seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	cmd.hdr = MSG_HDR_SET_SEQNUM_SIZE(cmd.hdr, seqnum, sizeof(cmd) >> 2);

	add_waiter(hfi, cmd.hdr, &pending_ack);

	/*
	 * Although we know device is powered on, we can still enter SLUMBER
	 * because the wait for ack below is done without holding the mutex. So
	 * take an active count before releasing the mutex so as to avoid a
	 * concurrent SLUMBER sequence while GMU is un-registering this context.
	 */
	gen7_hwsched_active_count_get(adreno_dev);

	rc = gen7_hfi_cmdq_write(adreno_dev, (u32 *)&cmd, sizeof(cmd));
	if (rc)
		goto done;

	mutex_unlock(&device->mutex);

	rc = wait_for_completion_timeout(&pending_ack.complete,
			msecs_to_jiffies(30 * 1000));
	if (!rc) {
		dev_err(&gmu->pdev->dev,
			"Ack timeout for context unregister seq: %d ctx: %u ts: %u\n",
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

	rc = gen7_hwsched_drain_context_hw_fences(adreno_dev, drawctxt);
	if (rc) {
		adreno_drawctxt_set_guilty(device, context);
		adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
		goto done;
	}

	rc = check_context_inflight_fences(adreno_dev, drawctxt);
	if (rc)
		goto done;

	rc = check_ack_failure(adreno_dev, &pending_ack);
done:
	gen7_hwsched_active_count_put(adreno_dev);

	del_waiter(hfi, &pending_ack);

	return rc;
}

void gen7_hwsched_context_detach(struct adreno_context *drawctxt)
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

u32 gen7_hwsched_preempt_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (device->state != KGSL_STATE_ACTIVE)
		return 0;

	return gen7_hwsched_hfi_get_value(adreno_dev, HFI_VALUE_PREEMPT_COUNT);
}

void gen7_hwsched_context_destroy(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt)
{
	if (!adreno_hwsched_context_queue_enabled(adreno_dev))
		return;

	if (drawctxt->gmu_context_queue.gmuaddr)
		gen7_free_gmu_block(to_gen7_gmu(adreno_dev), &drawctxt->gmu_context_queue);

	if (drawctxt->gmu_hw_fence_queue.gmuaddr)
		gen7_free_gmu_block(to_gen7_gmu(adreno_dev), &drawctxt->gmu_hw_fence_queue);
}

static int register_global_ctxt(struct adreno_device *adreno_dev)
{
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_register_ctxt_cmd rcmd = {0};
	struct hfi_context_pointers_cmd pcmd = {0};
	int ret;

	if (hwsched->global_ctxt_gmu_registered)
		return 0;

	if (adreno_hwsched_context_queue_enabled(adreno_dev) && !hwsched->global_ctxtq.hostptr) {
		struct gmu_context_queue_header *hdr;

		ret = gen7_alloc_gmu_kernel_block(to_gen7_gmu(adreno_dev), &hwsched->global_ctxtq,
			SZ_4K, GMU_NONCACHED_KERNEL, IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV);
		if (ret) {
			memset(&hwsched->global_ctxtq, 0x0, sizeof(hwsched->global_ctxtq));
			return ret;
		}

		hdr = hwsched->global_ctxtq.hostptr;
		hdr->start_addr = hwsched->global_ctxtq.gmuaddr + sizeof(*hdr);
		hdr->queue_size = (hwsched->global_ctxtq.size - sizeof(*hdr)) >> 2;
	}

	ret = CMD_MSG_HDR(rcmd, H2F_MSG_REGISTER_CONTEXT);
	if (ret)
		return ret;

	rcmd.ctxt_id = KGSL_GLOBAL_CTXT_ID;
	rcmd.flags = (KGSL_CONTEXT_PRIORITY_HIGH << KGSL_CONTEXT_PRIORITY_SHIFT);

	ret = gen7_hfi_send_cmd_async(adreno_dev, &rcmd, sizeof(rcmd));
	if (ret)
		return ret;

	ret = CMD_MSG_HDR(pcmd, H2F_MSG_CONTEXT_POINTERS);
	if (ret)
		return ret;

	pcmd.ctxt_id = KGSL_GLOBAL_CTXT_ID;
	pcmd.sop_addr = MEMSTORE_ID_GPU_ADDR(device, KGSL_GLOBAL_CTXT_ID, soptimestamp);
	pcmd.eop_addr = MEMSTORE_ID_GPU_ADDR(device, KGSL_GLOBAL_CTXT_ID, eoptimestamp);

	if (adreno_hwsched_context_queue_enabled(adreno_dev))
		pcmd.gmu_context_queue_addr = hwsched->global_ctxtq.gmuaddr;

	ret = gen7_hfi_send_cmd_async(adreno_dev, &pcmd, sizeof(pcmd));
	if (!ret)
		hwsched->global_ctxt_gmu_registered = true;

	return ret;
}

static int submit_global_ctxt_cmd(struct adreno_device *adreno_dev, u64 gpuaddr, u32 size)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	struct {
		struct hfi_submit_cmd submit_cmd;
		struct hfi_issue_ib issue_ib;
	} cmd = {0};
	u32 seqnum, cmd_size = sizeof(cmd);
	static u32 ts;
	int ret;

	cmd.submit_cmd.ctxt_id = KGSL_GLOBAL_CTXT_ID;
	cmd.submit_cmd.ts = ++ts;
	cmd.submit_cmd.numibs = 1;

	cmd.issue_ib.addr = gpuaddr;
	cmd.issue_ib.size = size;

	seqnum = atomic_inc_return(&hwsched->submission_seqnum);
	cmd.submit_cmd.hdr = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD, HFI_MSG_CMD);
	cmd.submit_cmd.hdr = MSG_HDR_SET_SEQNUM_SIZE(cmd.submit_cmd.hdr, seqnum, cmd_size >> 2);

	if (adreno_hwsched_context_queue_enabled(adreno_dev))
		ret = gen7_gmu_context_queue_write(adreno_dev,
			  &hwsched->global_ctxtq, (u32 *)&cmd, cmd_size, NULL, NULL);
	else
		ret = gen7_hfi_dispatch_queue_write(adreno_dev, HFI_DSP_ID_0,
			(u32 *)&cmd, cmd_size, NULL, NULL);

	/* Send interrupt to GMU to receive the message */
	if (!ret)
		gmu_core_regwrite(device, GEN7_GMU_HOST2GMU_INTR_SET, DISPQ_IRQ_BIT(0));

	return ret;
}

int gen7_hwsched_counter_inline_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		u32 counter, u32 countable)
{
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32 val, *cmds, count = 0;
	int ret;

	ret = register_global_ctxt(adreno_dev);
	if (ret)
		goto err;

	ret = adreno_allocate_global(device, &hfi->perfctr_scratch,
		PAGE_SIZE, 0, KGSL_MEMFLAGS_GPUREADONLY, 0, "perfctr_scratch");
	if (ret)
		goto err;

	if (group->flags & ADRENO_PERFCOUNTER_GROUP_RESTORE)
		gen7_perfcounter_update(adreno_dev, reg, false,
				FIELD_PREP(GENMASK(13, 12), PIPE_NONE));

	cmds = hfi->perfctr_scratch->hostptr;

	cmds[count++] = cp_type7_packet(CP_WAIT_FOR_IDLE, 0);
	cmds[count++] = cp_type4_packet(reg->select, 1);
	cmds[count++] = countable;

	ret = submit_global_ctxt_cmd(adreno_dev, hfi->perfctr_scratch->gpuaddr, count << 2);
	if (ret)
		goto err;

	/* Wait till the register is programmed with the countable */
	ret = kgsl_regmap_read_poll_timeout(&device->regmap, reg->select, val,
				val == countable, 100, ADRENO_IDLE_TIMEOUT);
	if (!ret) {
		reg->value = 0;
		return ret;
	}

err:
	dev_err(device->dev, "Perfcounter %s/%u/%u start via commands failed\n",
			group->name, counter, countable);
	return ret;
}
