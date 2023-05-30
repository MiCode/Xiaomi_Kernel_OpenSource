// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/nvmem-consumer.h>

#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_gen7_hfi.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

/* Below section is for all structures related to HFI queues */
#define HFI_QUEUE_MAX HFI_QUEUE_DEFAULT_CNT

/* Total header sizes + queue sizes + 16 for alignment */
#define HFIMEM_SIZE (sizeof(struct hfi_queue_table) + 16 + \
		(HFI_QUEUE_SIZE * HFI_QUEUE_MAX))

#define HOST_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->hostptr + HFI_QUEUE_OFFSET(i))

struct gen7_hfi *to_gen7_hfi(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	return &gmu->hfi;
}

/* Size in below functions are in unit of dwords */
int gen7_hfi_queue_read(struct gen7_gmu_device *gmu, u32 queue_idx,
		unsigned int *output, unsigned int max_size)
{
	struct kgsl_memdesc *mem_addr = gmu->hfi.hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	u32 *queue;
	u32 msg_hdr;
	u32 i, read;
	u32 size;
	int result = 0;

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return -EINVAL;

	if (hdr->read_index == hdr->write_index)
		return -ENODATA;

	/* Clear the output data before populating */
	memset(output, 0, max_size);

	queue = HOST_QUEUE_START_ADDR(mem_addr, queue_idx);
	msg_hdr = queue[hdr->read_index];
	size = MSG_HDR_GET_SIZE(msg_hdr);

	if (size > (max_size >> 2)) {
		dev_err(&gmu->pdev->dev,
		"HFI message too big: hdr:0x%x rd idx=%d\n",
			msg_hdr, hdr->read_index);
		result = -EMSGSIZE;
		goto done;
	}

	read = hdr->read_index;

	if (read < hdr->queue_size) {
		for (i = 0; i < size && i < (max_size >> 2); i++) {
			output[i] = queue[read];
			read = (read + 1)%hdr->queue_size;
		}
		result = size;
	} else {
		/* In case FW messed up */
		dev_err(&gmu->pdev->dev,
			"Read index %d greater than queue size %d\n",
			hdr->read_index, hdr->queue_size);
		result = -ENODATA;
	}

	read = ALIGN(read, SZ_4) % hdr->queue_size;

	hfi_update_read_idx(hdr, read);

	/* For acks, trace the packet for which this ack was sent */
	if (MSG_HDR_GET_TYPE(msg_hdr) == HFI_MSG_ACK)
		trace_kgsl_hfi_receive(MSG_HDR_GET_ID(output[1]),
			MSG_HDR_GET_SIZE(output[1]),
			MSG_HDR_GET_SEQNUM(output[1]));
	else
		trace_kgsl_hfi_receive(MSG_HDR_GET_ID(msg_hdr),
			MSG_HDR_GET_SIZE(msg_hdr), MSG_HDR_GET_SEQNUM(msg_hdr));

done:
	return result;
}

/* Size in below functions are in unit of dwords */
int gen7_hfi_queue_write(struct adreno_device *adreno_dev, u32 queue_idx,
		u32 *msg)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_queue_table *tbl = gmu->hfi.hfi_mem->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	u32 *queue;
	u32 i, write_idx, read_idx, empty_space;
	u32 size = MSG_HDR_GET_SIZE(*msg);
	u32 align_size = ALIGN(size, SZ_4);
	u32 id = MSG_HDR_GET_ID(*msg);

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return -EINVAL;

	queue = HOST_QUEUE_START_ADDR(gmu->hfi.hfi_mem, queue_idx);

	trace_kgsl_hfi_send(id, size, MSG_HDR_GET_SEQNUM(*msg));

	write_idx = hdr->write_index;
	read_idx = hdr->read_index;

	empty_space = (write_idx >= read_idx) ?
			(hdr->queue_size - (write_idx - read_idx))
			: (read_idx - write_idx);

	if (empty_space <= align_size)
		return -ENOSPC;

	for (i = 0; i < size; i++) {
		queue[write_idx] = msg[i];
		write_idx = (write_idx + 1) % hdr->queue_size;
	}

	/* Cookify any non used data at the end of the write buffer */
	for (; i < align_size; i++) {
		queue[write_idx] = 0xfafafafa;
		write_idx = (write_idx + 1) % hdr->queue_size;
	}

	hfi_update_write_idx(&hdr->write_index, write_idx);

	return 0;
}

int gen7_hfi_cmdq_write(struct adreno_device *adreno_dev, u32 *msg)
{
	int ret;

	ret = gen7_hfi_queue_write(adreno_dev, HFI_CMD_ID, msg);

	/*
	 * Memory barrier to make sure packet and write index are written before
	 * an interrupt is raised
	 */
	wmb();

	/* Send interrupt to GMU to receive the message */
	if (!ret)
		gmu_core_regwrite(KGSL_DEVICE(adreno_dev),
			GEN7_GMU_HOST2GMU_INTR_SET, 0x1);

	return ret;
}

/* Sizes of the queue and message are in unit of dwords */
static void init_queues(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_memdesc *mem_addr = gmu->hfi.hfi_mem;
	int i;
	struct hfi_queue_table *tbl;
	struct hfi_queue_header *hdr;
	struct {
		unsigned int idx;
		unsigned int pri;
		unsigned int status;
	} queue[HFI_QUEUE_MAX] = {
		{ HFI_CMD_ID, HFI_CMD_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_MSG_ID, HFI_MSG_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_DBG_ID, HFI_DBG_PRI, HFI_QUEUE_STATUS_ENABLED },
	};

	/* Fill Table Header */
	tbl = mem_addr->hostptr;
	tbl->qtbl_hdr.version = 0;
	tbl->qtbl_hdr.size = sizeof(struct hfi_queue_table) >> 2;
	tbl->qtbl_hdr.qhdr0_offset = sizeof(struct hfi_queue_table_header) >> 2;
	tbl->qtbl_hdr.qhdr_size = sizeof(struct hfi_queue_header) >> 2;
	tbl->qtbl_hdr.num_q = HFI_QUEUE_MAX;
	tbl->qtbl_hdr.num_active_q = HFI_QUEUE_MAX;

	memset(&tbl->qhdr[0], 0, sizeof(tbl->qhdr));

	/* Fill Individual Queue Headers */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		hdr->start_addr = GMU_QUEUE_START_ADDR(mem_addr->gmuaddr, i);
		hdr->type = QUEUE_HDR_TYPE(queue[i].idx, queue[i].pri, 0, 0);
		hdr->status = queue[i].status;
		hdr->queue_size = HFI_QUEUE_SIZE >> 2; /* convert to dwords */
	}
}

int gen7_hfi_init(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hfi *hfi = &gmu->hfi;

	/* Allocates & maps memory for HFI */
	if (IS_ERR_OR_NULL(hfi->hfi_mem)) {
		hfi->hfi_mem = gen7_reserve_gmu_kernel_block(gmu, 0,
				HFIMEM_SIZE, GMU_NONCACHED_KERNEL, 0);
		if (!IS_ERR(hfi->hfi_mem))
			init_queues(adreno_dev);
	}

	return PTR_ERR_OR_ZERO(hfi->hfi_mem);
}

int gen7_receive_ack_cmd(struct gen7_gmu_device *gmu, void *rcvd,
	struct pending_cmd *ret_cmd)
{
	struct adreno_device *adreno_dev = gen7_gmu_to_adreno(gmu);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 *ack = rcvd;
	u32 hdr = ack[0];
	u32 req_hdr = ack[1];

	if (ret_cmd == NULL)
		return -EINVAL;

	if (HDR_CMP_SEQNUM(ret_cmd->sent_hdr, req_hdr)) {
		memcpy(&ret_cmd->results, ack, MSG_HDR_GET_SIZE(hdr) << 2);
		return 0;
	}

	/* Didn't find the sender, list the waiter */
	dev_err_ratelimited(&gmu->pdev->dev,
		"HFI ACK: Cannot find sender for 0x%8.8x Waiter: 0x%8.8x\n",
		req_hdr, ret_cmd->sent_hdr);

	gmu_core_fault_snapshot(device);

	return -ENODEV;
}

static int poll_gmu_reg(struct adreno_device *adreno_dev,
	u32 offsetdwords, unsigned int expected_val,
	unsigned int mask, unsigned int timeout_ms)
{
	unsigned int val;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
	bool nmi = false;

	while (time_is_after_jiffies(timeout)) {
		gmu_core_regread(device, offsetdwords, &val);
		if ((val & mask) == expected_val)
			return 0;

		/*
		 * If GMU firmware fails any assertion, error message is sent
		 * to KMD and NMI is triggered. So check if GMU is in NMI and
		 * timeout early. Bits [11:9] of A6XX_GMU_CM3_FW_INIT_RESULT
		 * contain GMU reset status. Non zero value here indicates that
		 * GMU reset is active, NMI handler would eventually complete
		 * and GMU would wait for recovery.
		 */
		gmu_core_regread(device, GEN7_GMU_CM3_FW_INIT_RESULT, &val);
		if (val & 0xE00) {
			nmi = true;
			break;
		}

		usleep_range(10, 100);
	}

	/* Check one last time */
	gmu_core_regread(device, offsetdwords, &val);
	if ((val & mask) == expected_val)
		return 0;

	dev_err(&gmu->pdev->dev,
		"Reg poll %s: offset 0x%x, want 0x%x, got 0x%x\n",
		nmi ? "abort" : "timeout", offsetdwords, expected_val,
		val & mask);

	return -ETIMEDOUT;
}

static int gen7_hfi_send_cmd_wait_inline(struct adreno_device *adreno_dev,
	void *data, struct pending_cmd *ret_cmd)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int rc;
	u32 *cmd = data;
	struct gen7_hfi *hfi = &gmu->hfi;
	unsigned int seqnum = atomic_inc_return(&hfi->seqnum);

	*cmd = MSG_HDR_SET_SEQNUM(*cmd, seqnum);
	if (ret_cmd == NULL)
		return gen7_hfi_cmdq_write(adreno_dev, cmd);

	ret_cmd->sent_hdr = cmd[0];

	rc = gen7_hfi_cmdq_write(adreno_dev, cmd);
	if (rc)
		return rc;

	rc = poll_gmu_reg(adreno_dev, GEN7_GMU_GMU2HOST_INTR_INFO,
		HFI_IRQ_MSGQ_MASK, HFI_IRQ_MSGQ_MASK, HFI_RSP_TIMEOUT);

	if (rc) {
		gmu_core_fault_snapshot(device);
		dev_err(&gmu->pdev->dev,
		"Timed out waiting on ack for 0x%8.8x (id %d, sequence %d)\n",
		cmd[0], MSG_HDR_GET_ID(*cmd), MSG_HDR_GET_SEQNUM(*cmd));
		return rc;
	}

	/* Clear the interrupt */
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR,
		HFI_IRQ_MSGQ_MASK);

	rc = gen7_hfi_process_queue(gmu, HFI_MSG_ID, ret_cmd);

	return rc;
}

int gen7_hfi_send_generic_req(struct adreno_device *adreno_dev, void *cmd)
{
	struct pending_cmd ret_cmd;
	int rc;

	memset(&ret_cmd, 0, sizeof(ret_cmd));

	rc = gen7_hfi_send_cmd_wait_inline(adreno_dev, cmd, &ret_cmd);
	if (rc)
		return rc;

	if (ret_cmd.results[2]) {
		struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

		gmu_core_fault_snapshot(device);
		dev_err(&gmu->pdev->dev,
				"HFI ACK failure: Req=0x%8.8X, Result=0x%8.8X\n",
				ret_cmd.results[1],
				ret_cmd.results[2]);
		return -EINVAL;
	}

	return 0;
}

int gen7_hfi_send_core_fw_start(struct adreno_device *adreno_dev)
{
	struct hfi_core_fw_start_cmd cmd = {
		.handle = 0x0,
	};
	int ret;

	ret = CMD_MSG_HDR(cmd, H2F_MSG_CORE_FW_START);
	if (ret)
		return ret;

	return gen7_hfi_send_generic_req(adreno_dev, &cmd);
}

static const char *feature_to_string(u32 feature)
{
	if (feature == HFI_FEATURE_ACD)
		return "ACD";
	else if (feature == HFI_FEATURE_LM)
		return "LM";

	return "unknown";
}

/* For sending hfi message inline to handle GMU return type error */
static int gen7_hfi_send_generic_req_v5(struct adreno_device *adreno_dev, void *cmd)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct pending_cmd ret_cmd = {0};
	int rc;

	if (GMU_VER_MINOR(gmu->ver.hfi) <= 4)
		return gen7_hfi_send_generic_req(adreno_dev, cmd);

	rc = gen7_hfi_send_cmd_wait_inline(adreno_dev, cmd, &ret_cmd);
	if (rc)
		return rc;

	switch (ret_cmd.results[3]) {
	case GMU_SUCCESS:
		rc = ret_cmd.results[2];
		break;
	case GMU_ERROR_NO_ENTRY:
		/* Unique error to handle undefined HFI msgs by caller */
		rc = -ENOENT;
		break;
	default:
		gmu_core_fault_snapshot(KGSL_DEVICE(adreno_dev));
		dev_err(&gmu->pdev->dev,
			"HFI ACK: Req=0x%8.8X, Result=0x%8.8X Error:0x%8.8X\n",
			ret_cmd.results[1], ret_cmd.results[2], ret_cmd.results[3]);
		rc = -EINVAL;
		break;
	}

	return rc;
}

int gen7_hfi_send_feature_ctrl(struct adreno_device *adreno_dev,
	u32 feature, u32 enable, u32 data)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_feature_ctrl_cmd cmd = {
		.feature = feature,
		.enable = enable,
		.data = data,
	};
	int ret;

	ret = CMD_MSG_HDR(cmd, H2F_MSG_FEATURE_CTRL);
	if (ret)
		return ret;

	ret = gen7_hfi_send_generic_req_v5(adreno_dev, &cmd);
	if (ret < 0)
		dev_err(&gmu->pdev->dev,
				"Unable to %s feature %s (%d)\n",
				enable ? "enable" : "disable",
				feature_to_string(feature),
				feature);
	return ret;
}

int gen7_hfi_send_set_value(struct adreno_device *adreno_dev,
		u32 type, u32 subtype, u32 data)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_set_value_cmd cmd = {
		.type = type,
		.subtype = subtype,
		.data = data,
	};
	int ret;

	ret = CMD_MSG_HDR(cmd, H2F_MSG_SET_VALUE);
	if (ret)
		return ret;

	ret = gen7_hfi_send_generic_req_v5(adreno_dev, &cmd);
	if (ret < 0)
		dev_err(&gmu->pdev->dev,
			"Unable to set HFI Value %d, %d to %d, error = %d\n",
			type, subtype, data, ret);
	return ret;
}

void adreno_gen7_receive_err_req(struct gen7_gmu_device *gmu, void *rcvd)
{
	struct hfi_err_cmd *cmd = rcvd;

	dev_err(&gmu->pdev->dev, "HFI Error Received: %d %d %.16s\n",
			((cmd->error_code >> 16) & 0xffff),
			(cmd->error_code & 0xffff),
			(char *) cmd->data);
}

void adreno_gen7_receive_debug_req(struct gen7_gmu_device *gmu, void *rcvd)
{
	struct hfi_debug_cmd *cmd = rcvd;

	dev_dbg(&gmu->pdev->dev, "HFI Debug Received: %d %d %d\n",
			cmd->type, cmd->timestamp, cmd->data);
}

int gen7_hfi_process_queue(struct gen7_gmu_device *gmu,
		u32 queue_idx, struct pending_cmd *ret_cmd)
{
	u32 rcvd[MAX_RCVD_SIZE];

	while (gen7_hfi_queue_read(gmu, queue_idx, rcvd, sizeof(rcvd)) > 0) {
		/* ACK Handler */
		if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
			int ret = gen7_receive_ack_cmd(gmu, rcvd, ret_cmd);

			if (ret)
				return ret;
			continue;
		}

		/* Request Handler */
		switch (MSG_HDR_GET_ID(rcvd[0])) {
		case F2H_MSG_ERR: /* No Reply */
			adreno_gen7_receive_err_req(gmu, rcvd);
			break;
		case F2H_MSG_DEBUG: /* No Reply */
			adreno_gen7_receive_debug_req(gmu, rcvd);
			break;
		default: /* No Reply */
			dev_err(&gmu->pdev->dev,
				"HFI request %d not supported\n",
				MSG_HDR_GET_ID(rcvd[0]));
			break;
		}
	}

	return 0;
}

int gen7_hfi_send_bcl_feature_ctrl(struct adreno_device *adreno_dev)
{
	if (!adreno_dev->bcl_enabled)
		return 0;

	return gen7_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_BCL, 1, adreno_dev->bcl_data);
}

#define EVENT_PWR_ACD_THROTTLE_PROF 44

int gen7_hfi_send_acd_feature_ctrl(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;

	if (adreno_dev->acd_enabled) {
		ret = gen7_hfi_send_feature_ctrl(adreno_dev,
			HFI_FEATURE_ACD, 1, 0);
		if (ret)
			return ret;

		ret = gen7_hfi_send_generic_req(adreno_dev,
				&gmu->hfi.acd_table);
		if (ret)
			return ret;

		gen7_hfi_send_set_value(adreno_dev, HFI_VALUE_LOG_EVENT_ON,
				EVENT_PWR_ACD_THROTTLE_PROF, 0);
	}

	return 0;
}

int gen7_hfi_send_ifpc_feature_ctrl(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (gmu->idle_level == GPU_HW_IFPC)
		return gen7_hfi_send_feature_ctrl(adreno_dev,
				HFI_FEATURE_IFPC, 1, 0x1680);
	return 0;
}

static void reset_hfi_queues(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_memdesc *mem_addr = gmu->hfi.hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr;
	unsigned int i;

	/* Flush HFI queues */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		hdr->read_index = hdr->write_index;
	}
}

int gen7_hfi_start(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int result;

	reset_hfi_queues(adreno_dev);

	result = gen7_hfi_send_generic_req(adreno_dev, &gmu->hfi.dcvs_table);
	if (result)
		goto err;

	result = gen7_hfi_send_generic_req(adreno_dev, &gmu->hfi.bw_table);
	if (result)
		goto err;

	result = gen7_hfi_send_acd_feature_ctrl(adreno_dev);
	if (result)
		goto err;

	result = gen7_hfi_send_bcl_feature_ctrl(adreno_dev);
	if (result)
		goto err;

	result = gen7_hfi_send_ifpc_feature_ctrl(adreno_dev);
	if (result)
		goto err;

	result = gen7_hfi_send_core_fw_start(adreno_dev);
	if (result)
		goto err;

	set_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

	/* Request default DCVS level */
	result = kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
	if (result)
		goto err;

	/* Request default BW vote */
	result = kgsl_pwrctrl_axi(device, true);

err:
	if (result)
		gen7_hfi_stop(adreno_dev);

	return result;

}

void gen7_hfi_stop(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_pwrctrl_axi(device, false);

	clear_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);
}

/* HFI interrupt handler */
irqreturn_t gen7_hfi_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct gen7_gmu_device *gmu = to_gen7_gmu(ADRENO_DEVICE(device));
	unsigned int status = 0;

	gmu_core_regread(device, GEN7_GMU_GMU2HOST_INTR_INFO, &status);
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR, HFI_IRQ_MASK);

	if (status & HFI_IRQ_DBGQ_MASK)
		gen7_hfi_process_queue(gmu, HFI_DBG_ID, NULL);
	if (status & HFI_IRQ_CM3_FAULT_MASK) {
		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU CM3 fault interrupt received\n");
		atomic_set(&gmu->cm3_fault, 1);

		/* make sure other CPUs see the update */
		smp_wmb();
	}
	if (status & ~HFI_IRQ_MASK)
		dev_err_ratelimited(&gmu->pdev->dev,
				"Unhandled HFI interrupts 0x%lx\n",
				status & ~HFI_IRQ_MASK);

	return IRQ_HANDLED;
}
