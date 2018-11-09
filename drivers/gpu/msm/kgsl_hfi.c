/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "kgsl_device.h"
#include "kgsl_hfi.h"
#include "kgsl_gmu.h"
#include "adreno.h"
#include "kgsl_trace.h"
#include "kgsl_pwrctrl.h"

#define HFI_QUEUE_OFFSET(i)		\
		(ALIGN(sizeof(struct hfi_queue_table), SZ_16) + \
		 ((i) * HFI_QUEUE_SIZE))

#define HOST_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->hostptr + HFI_QUEUE_OFFSET(i))

#define GMU_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->gmuaddr + HFI_QUEUE_OFFSET(i))

#define MSG_HDR_GET_ID(hdr) ((hdr) & 0xFF)
#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_TYPE(hdr) (((hdr) >> 16) & 0xF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

/* Size is converted from Bytes to DWords */
#define CREATE_MSG_HDR(id, size, type) \
	(((type) << 16) | ((((size) >> 2) & 0xFF) << 8) | ((id) & 0xFF))
#define CMD_MSG_HDR(id, size) CREATE_MSG_HDR(id, size, HFI_MSG_CMD)
#define ACK_MSG_HDR(id, size) CREATE_MSG_HDR(id, size, HFI_MSG_ACK)

#define HFI_VER_MAJOR(hfi) (((hfi)->version >> 28) & 0xF)
#define HFI_VER_MINOR(hfi) (((hfi)->version >> 5) & 0x7FFFFF)
#define HFI_VER_BRANCH(hfi) ((hfi)->version & 0x1F)
#define HFI_VERSION(major, minor, branch) \
	((((major) & 0xF) << 28) | \
	 (((minor) & 0x7FFFFF) << 5) | \
	 ((branch) & 0x1F))

static void hfi_process_queue(struct gmu_device *gmu, uint32_t queue_idx,
	struct pending_cmd *ret_cmd);

/* Size in below functions are in unit of dwords */
static int hfi_queue_read(struct gmu_device *gmu, uint32_t queue_idx,
		unsigned int *output, unsigned int max_size)
{
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue;
	uint32_t msg_hdr;
	uint32_t i, read;
	uint32_t size;
	int result = 0;

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return -EINVAL;

	if (hdr->read_index == hdr->write_index) {
		hdr->rx_req = 1;
		result = -ENODATA;
		goto done;
	}

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

	if (HFI_VER_MAJOR(&gmu->hfi) >= 2)
		read = ALIGN(read, SZ_4) % hdr->queue_size;

	hdr->read_index = read;

done:
	return result;
}

/* Size in below functions are in unit of dwords */
static int hfi_queue_write(struct gmu_device *gmu, uint32_t queue_idx,
		uint32_t *msg)
{
	struct hfi_queue_table *tbl = gmu->hfi_mem->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue;
	struct kgsl_hfi *hfi = &gmu->hfi;
	uint32_t i, write, empty_space;
	uint32_t size = MSG_HDR_GET_SIZE(*msg);
	uint32_t id = MSG_HDR_GET_ID(*msg);

	if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
		return -EINVAL;

	if (size > HFI_MAX_MSG_SIZE) {
		dev_err(&gmu->pdev->dev,
			"Message too big to send: sz=%d, id=%d\n",
			size, id);
		return -EINVAL;
	}

	queue = HOST_QUEUE_START_ADDR(gmu->hfi_mem, queue_idx);

	trace_kgsl_hfi_send(id, size, MSG_HDR_GET_SEQNUM(*msg));

	mutex_lock(&hfi->cmdq_mutex);

	empty_space = (hdr->write_index >= hdr->read_index) ?
			(hdr->queue_size - (hdr->write_index - hdr->read_index))
			: (hdr->read_index - hdr->write_index);

	if (empty_space < size) {
		dev_err(&gmu->pdev->dev,
			"Insufficient bufsize %d for msg id=%d of size %d\n",
			empty_space, id, size);

		hdr->drop_cnt++;
		mutex_unlock(&hfi->cmdq_mutex);
		return -ENOSPC;
	}

	write = hdr->write_index;

	for (i = 0; i < size; i++) {
		queue[write] = msg[i];
		write = (write + 1) % hdr->queue_size;
	}

	/* Cookify any non used data at the end of the write buffer */
	if (HFI_VER_MAJOR(&gmu->hfi) >= 2) {
		for (; write % 4; write = (write + 1) % hdr->queue_size)
			queue[write] = 0xFAFAFAFA;
	}

	hdr->write_index = write;

	mutex_unlock(&hfi->cmdq_mutex);

	/*
	 * Memory barrier to make sure packet and write index are written before
	 * an interrupt is raised
	 */
	wmb();

	/* Send interrupt to GMU to receive the message */
	adreno_write_gmureg(ADRENO_DEVICE(hfi->kgsldev),
		ADRENO_REG_GMU_HOST2GMU_INTR_SET, 0x1);

	return 0;
}

#define QUEUE_HDR_TYPE(id, prio, rtype, stype) \
	(((id) & 0xFF) | (((prio) & 0xFF) << 8) | \
	(((rtype) & 0xFF) << 16) | (((stype) & 0xFF) << 24))


/* Sizes of the queue and message are in unit of dwords */
void hfi_init(struct kgsl_hfi *hfi, struct gmu_memdesc *mem_addr,
		uint32_t queue_sz_bytes)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(hfi->kgsldev);
	int i;
	struct hfi_queue_table *tbl;
	struct hfi_queue_header *hdr;
	struct {
		unsigned int idx;
		unsigned int pri;
		unsigned int status;
	} queue[HFI_QUEUE_MAX] = {
		{ HFI_CMD_IDX, HFI_CMD_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_MSG_IDX, HFI_MSG_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_DBG_IDX, HFI_DBG_PRI, HFI_QUEUE_STATUS_ENABLED },
		{ HFI_DSP_IDX_0, HFI_DSP_PRI_0, HFI_QUEUE_STATUS_DISABLED },
	};

	/*
	 * Overwrite the queue IDs for A630, A615 and A616 as they use
	 * legacy firmware. Legacy firmware has different queue IDs for
	 * message, debug and dispatch queues.
	 */
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		queue[HFI_MSG_ID].idx = HFI_MSG_IDX_LEGACY;
		queue[HFI_DBG_ID].idx = HFI_DBG_IDX_LEGACY;
		queue[HFI_DSP_ID_0].idx = HFI_DSP_IDX_0_LEGACY;
	}

	/* Fill Table Header */
	tbl = mem_addr->hostptr;
	tbl->qtbl_hdr.version = 0;
	tbl->qtbl_hdr.size = sizeof(struct hfi_queue_table) >> 2;
	tbl->qtbl_hdr.qhdr0_offset = sizeof(struct hfi_queue_table_header) >> 2;
	tbl->qtbl_hdr.qhdr_size = sizeof(struct hfi_queue_header) >> 2;
	tbl->qtbl_hdr.num_q = HFI_QUEUE_MAX;
	tbl->qtbl_hdr.num_active_q = HFI_QUEUE_MAX;

	/* Fill I dividual Queue Headers */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		hdr->start_addr = GMU_QUEUE_START_ADDR(mem_addr, i);
		hdr->type = QUEUE_HDR_TYPE(queue[i].idx, queue[i].pri, 0,  0);
		hdr->status = queue[i].status;
		hdr->queue_size = queue_sz_bytes >> 2; /* convert to dwords */
		hdr->msg_size = 0;
		hdr->drop_cnt = 0;
		hdr->rx_wm = 0x1;
		hdr->tx_wm = 0x1;
		hdr->rx_req = 0x1;
		hdr->tx_req = 0x0;
		hdr->read_index = 0x0;
		hdr->write_index = 0x0;
	}

	mutex_init(&hfi->cmdq_mutex);
}

#define HDR_CMP_SEQNUM(out_hdr, in_hdr) \
	(MSG_HDR_GET_SEQNUM(out_hdr) == MSG_HDR_GET_SEQNUM(in_hdr))

static void receive_ack_cmd(struct gmu_device *gmu, void *rcvd,
	struct pending_cmd *ret_cmd)
{
	uint32_t *ack = rcvd;
	uint32_t hdr = ack[0];
	uint32_t req_hdr = ack[1];
	struct kgsl_hfi *hfi = &gmu->hfi;

	if (ret_cmd == NULL)
		return;

	trace_kgsl_hfi_receive(MSG_HDR_GET_ID(req_hdr),
		MSG_HDR_GET_SIZE(req_hdr),
		MSG_HDR_GET_SEQNUM(req_hdr));

	if (HDR_CMP_SEQNUM(ret_cmd->sent_hdr, req_hdr)) {
		memcpy(&ret_cmd->results, ack, MSG_HDR_GET_SIZE(hdr) << 2);
		return;
	}

	/* Didn't find the sender, list the waiter */
	dev_err_ratelimited(&gmu->pdev->dev,
		"HFI ACK: Cannot find sender for 0x%8.8x Waiter: 0x%8.8x\n",
		req_hdr, ret_cmd->sent_hdr);

	adreno_set_gpu_fault(ADRENO_DEVICE(hfi->kgsldev), ADRENO_GMU_FAULT);
	adreno_dispatcher_schedule(hfi->kgsldev);
}

#define MSG_HDR_SET_SEQNUM(hdr, num) \
	(((hdr) & 0xFFFFF) | ((num) << 20))

static int poll_adreno_gmu_reg(struct adreno_device *adreno_dev,
	enum adreno_regs offset_name, unsigned int expected_val,
	unsigned int mask, unsigned int timeout_ms)
{
	unsigned int val;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	while (time_is_after_jiffies(timeout)) {
		adreno_read_gmureg(adreno_dev, offset_name, &val);
		if ((val & mask) == expected_val)
			return 0;
		usleep_range(10, 100);
	}

	/* Check one last time */
	adreno_read_gmureg(adreno_dev, offset_name, &val);
	if ((val & mask) == expected_val)
		return 0;

	return -ETIMEDOUT;
}

static int hfi_send_cmd(struct gmu_device *gmu, uint32_t queue_idx,
		void *data, struct pending_cmd *ret_cmd)
{
	int rc;
	uint32_t *cmd = data;
	struct kgsl_hfi *hfi = &gmu->hfi;
	unsigned int seqnum = atomic_inc_return(&hfi->seqnum);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(hfi->kgsldev);

	*cmd = MSG_HDR_SET_SEQNUM(*cmd, seqnum);
	if (ret_cmd == NULL)
		return hfi_queue_write(gmu, queue_idx, cmd);

	ret_cmd->sent_hdr = cmd[0];

	rc = hfi_queue_write(gmu, queue_idx, cmd);
	if (rc)
		return rc;

	rc = poll_adreno_gmu_reg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_INFO,
		HFI_IRQ_MSGQ_MASK, HFI_IRQ_MSGQ_MASK, HFI_RSP_TIMEOUT);

	if (rc) {
		dev_err(&gmu->pdev->dev,
		"Timed out waiting on ack for 0x%8.8x (id %d, sequence %d)\n",
		cmd[0], MSG_HDR_GET_ID(*cmd), MSG_HDR_GET_SEQNUM(*cmd));
		return rc;
	}

	/* Clear the interrupt */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_CLR,
		HFI_IRQ_MSGQ_MASK);

	hfi_process_queue(gmu, HFI_MSG_ID, ret_cmd);

	return rc;
}

static int hfi_send_generic_req(struct gmu_device *gmu, uint32_t queue,
		void *cmd)
{
	struct pending_cmd ret_cmd;
	int rc;

	memset(&ret_cmd, 0, sizeof(ret_cmd));

	rc = hfi_send_cmd(gmu, queue, cmd, &ret_cmd);
	if (rc)
		return rc;

	if (ret_cmd.results[2])
		dev_err(&gmu->pdev->dev,
				"HFI ACK failure: Req 0x%8.8X Error 0x%X\n",
				ret_cmd.results[1],
				ret_cmd.results[2]);

	return ret_cmd.results[2] ? -EINVAL : 0;
}

static int hfi_send_gmu_init(struct gmu_device *gmu, uint32_t boot_state)
{
	struct hfi_gmu_init_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_INIT, sizeof(cmd)),
		.seg_id = 0,
		.dbg_buffer_addr = (unsigned int) gmu->dump_mem->gmuaddr,
		.dbg_buffer_size = (unsigned int) gmu->dump_mem->size,
		.boot_state = boot_state,
	};

	return hfi_send_generic_req(gmu, HFI_CMD_ID, &cmd);
}

static int hfi_get_fw_version(struct gmu_device *gmu,
		uint32_t expected_ver, uint32_t *ver)
{
	struct hfi_fw_version_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_FW_VER, sizeof(cmd)),
		.supported_ver = expected_ver,
	};
	int rc;
	struct pending_cmd ret_cmd;

	memset(&ret_cmd, 0, sizeof(ret_cmd));

	rc = hfi_send_cmd(gmu, HFI_CMD_ID, &cmd, &ret_cmd);
	if (rc)
		return rc;

	rc = ret_cmd.results[2];
	if (!rc)
		*ver = ret_cmd.results[3];
	else
		dev_err(&gmu->pdev->dev,
			"gmu get fw ver failed with error=%d\n", rc);

	return rc;
}

static int hfi_send_core_fw_start(struct gmu_device *gmu)
{
	struct hfi_core_fw_start_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_CORE_FW_START, sizeof(cmd)),
		.handle = 0x0,
	};

	return hfi_send_generic_req(gmu, HFI_CMD_ID, &cmd);
}

static const char * const hfi_features[] = {
	[HFI_FEATURE_ECP] = "ECP",
	[HFI_FEATURE_ACD] = "ACD",
	[HFI_FEATURE_LM] = "LM",
};

static const char *feature_to_string(uint32_t feature)
{
	if (feature < ARRAY_SIZE(hfi_features) && hfi_features[feature])
		return hfi_features[feature];

	return "unknown";
}

static int hfi_send_feature_ctrl(struct gmu_device *gmu,
		uint32_t feature, uint32_t enable, uint32_t data)
{
	struct hfi_feature_ctrl_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_FEATURE_CTRL, sizeof(cmd)),
		.feature = feature,
		.enable = enable,
		.data = data,
	};
	int ret;

	ret = hfi_send_generic_req(gmu, HFI_CMD_ID, &cmd);
	if (ret)
		dev_err(&gmu->pdev->dev,
				"Unable to %s feature %s (%d)\n",
				enable ? "enable" : "disable",
				feature_to_string(feature),
				feature);
	return ret;
}

static int hfi_send_dcvstbl_v1(struct gmu_device *gmu)
{
	struct hfi_dcvstable_v1_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_PERF_TBL, sizeof(cmd)),
		.gpu_level_num = gmu->num_gpupwrlevels,
		.gmu_level_num = gmu->num_gmupwrlevels,
	};
	int i;

	for (i = 0; i < gmu->num_gpupwrlevels; i++) {
		cmd.gx_votes[i].vote = gmu->rpmh_votes.gx_votes[i];
		/* Divide by 1000 to convert to kHz */
		cmd.gx_votes[i].freq = gmu->gpu_freqs[i] / 1000;
	}

	for (i = 0; i < gmu->num_gmupwrlevels; i++) {
		cmd.cx_votes[i].vote = gmu->rpmh_votes.cx_votes[i];
		cmd.cx_votes[i].freq = gmu->gmu_freqs[i] / 1000;
	}

	return hfi_send_generic_req(gmu, HFI_CMD_ID, &cmd);
}

static int hfi_send_get_value(struct gmu_device *gmu,
		struct hfi_get_value_req *req)
{
	struct hfi_get_value_cmd *cmd = &req->cmd;
	struct pending_cmd ret_cmd;
	struct hfi_get_value_reply_cmd *reply =
		(struct hfi_get_value_reply_cmd *)ret_cmd.results;
	int rc;

	cmd->hdr = CMD_MSG_HDR(H2F_MSG_GET_VALUE, sizeof(*cmd));

	rc = hfi_send_cmd(gmu, HFI_CMD_ID, cmd, &ret_cmd);
	if (rc)
		return rc;

	memset(&req->data, 0, sizeof(req->data));
	memcpy(&req->data, &reply->data,
			(MSG_HDR_GET_SIZE(reply->hdr) - 2) << 2);
	return 0;
}

static int hfi_send_dcvstbl(struct gmu_device *gmu)
{
	struct hfi_dcvstable_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_PERF_TBL, sizeof(cmd)),
		.gpu_level_num = gmu->num_gpupwrlevels,
		.gmu_level_num = gmu->num_gmupwrlevels,
	};
	int i;

	for (i = 0; i < gmu->num_gpupwrlevels; i++) {
		cmd.gx_votes[i].vote = gmu->rpmh_votes.gx_votes[i];
		/* Hardcode this to the max threshold since it is not used */
		cmd.gx_votes[i].acd = 0xFFFFFFFF;
		/* Divide by 1000 to convert to kHz */
		cmd.gx_votes[i].freq = gmu->gpu_freqs[i] / 1000;
	}

	for (i = 0; i < gmu->num_gmupwrlevels; i++) {
		cmd.cx_votes[i].vote = gmu->rpmh_votes.cx_votes[i];
		cmd.cx_votes[i].freq = gmu->gmu_freqs[i] / 1000;
	}

	return hfi_send_generic_req(gmu, HFI_CMD_ID, &cmd);
}

static int hfi_send_bwtbl(struct gmu_device *gmu)
{
	struct hfi_bwtable_cmd *cmd = &gmu->hfi.bwtbl_cmd;

	cmd->hdr = CMD_MSG_HDR(H2F_MSG_BW_VOTE_TBL, sizeof(*cmd));

	return hfi_send_generic_req(gmu, HFI_CMD_ID, cmd);
}

static int hfi_send_acd_tbl(struct gmu_device *gmu)
{
	struct hfi_acd_table_cmd *cmd = &gmu->hfi.acd_tbl_cmd;

	cmd->hdr = CMD_MSG_HDR(H2F_MSG_ACD_TBL, sizeof(*cmd));

	return hfi_send_generic_req(gmu, HFI_CMD_IDX, cmd);
}

static int hfi_send_test(struct gmu_device *gmu)
{
	struct hfi_test_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_TEST, sizeof(cmd)),
	};

	return hfi_send_generic_req(gmu, HFI_CMD_ID, &cmd);
}

static void receive_err_req(struct gmu_device *gmu, void *rcvd)
{
	struct hfi_err_cmd *cmd = rcvd;

	dev_err(&gmu->pdev->dev, "HFI Error Received: %d %d %s\n",
			((cmd->error_code >> 16) & 0xFFFF),
			(cmd->error_code & 0xFFFF),
			(char *) cmd->data);
}

static void receive_debug_req(struct gmu_device *gmu, void *rcvd)
{
	struct hfi_debug_cmd *cmd = rcvd;

	dev_dbg(&gmu->pdev->dev, "HFI Debug Received: %d %d %d\n",
			cmd->type, cmd->timestamp, cmd->data);
}

static void hfi_v1_receiver(struct gmu_device *gmu, uint32_t *rcvd,
	struct pending_cmd *ret_cmd)
{
	/* V1 ACK Handler */
	if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_V1_MSG_ACK) {
		receive_ack_cmd(gmu, rcvd, ret_cmd);
		return;
	}

	/* V1 Request Handler */
	switch (MSG_HDR_GET_ID(rcvd[0])) {
	case F2H_MSG_ERR: /* No Reply */
		receive_err_req(gmu, rcvd);
		break;
	case F2H_MSG_DEBUG: /* No Reply */
		receive_debug_req(gmu, rcvd);
		break;
	default: /* No Reply */
		dev_err(&gmu->pdev->dev,
				"HFI V1 request %d not supported\n",
				MSG_HDR_GET_ID(rcvd[0]));
		break;
	}
}

static void hfi_process_queue(struct gmu_device *gmu, uint32_t queue_idx,
	struct pending_cmd *ret_cmd)
{
	uint32_t rcvd[MAX_RCVD_SIZE];

	while (hfi_queue_read(gmu, queue_idx, rcvd, sizeof(rcvd)) > 0) {
		/* Special case if we're v1 */
		if (HFI_VER_MAJOR(&gmu->hfi) < 2) {
			hfi_v1_receiver(gmu, rcvd, ret_cmd);
			continue;
		}

		/* V2 ACK Handler */
		if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
			receive_ack_cmd(gmu, rcvd, ret_cmd);
			continue;
		}

		/* V2 Request Handler */
		switch (MSG_HDR_GET_ID(rcvd[0])) {
		case F2H_MSG_ERR: /* No Reply */
			receive_err_req(gmu, rcvd);
			break;
		case F2H_MSG_DEBUG: /* No Reply */
			receive_debug_req(gmu, rcvd);
			break;
		default: /* No Reply */
			dev_err(&gmu->pdev->dev,
				"HFI request %d not supported\n",
				MSG_HDR_GET_ID(rcvd[0]));
			break;
		}
	}
}

void hfi_receiver(unsigned long data)
{
	/* Process all asynchronous read (firmware to host) queues */
	hfi_process_queue((struct gmu_device *) data, HFI_DBG_ID, NULL);
}

#define GMU_VER_MAJOR(ver) (((ver) >> 28) & 0xF)
#define GMU_VER_MINOR(ver) (((ver) >> 16) & 0xFFF)
#define GMU_VERSION(major, minor) \
	((((major) & 0xF) << 28) | (((minor) & 0xFFF) << 16))

static int hfi_verify_fw_version(struct kgsl_device *device,
		struct gmu_device *gmu)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int result;
	unsigned int ver, major, minor;

	/* GMU version is already known, so don't waste time finding again */
	if (gmu->ver != ~0U)
		return 0;

	/* Read the HFI version from the register */
	adreno_read_gmureg(adreno_dev,
		ADRENO_REG_GMU_HFI_VERSION_INFO, &gmu->hfi.version);

	major = adreno_dev->gpucore->gpmu_major;
	minor = adreno_dev->gpucore->gpmu_minor;

	result = hfi_get_fw_version(gmu, GMU_VERSION(major, minor), &ver);
	if (result) {
		dev_err_once(&gmu->pdev->dev,
				"Failed to get FW version via HFI\n");
		return result;
	}

	/* For now, warn once. Could return error later if needed */
	if (major != GMU_VER_MAJOR(ver))
		dev_err_once(&gmu->pdev->dev,
				"FW Major Error: Wanted %d, got %d\n",
				major, GMU_VER_MAJOR(ver));

	if (minor > GMU_VER_MINOR(ver))
		dev_err_once(&gmu->pdev->dev,
				"FW Minor Error: Wanted < %d, got %d\n",
				GMU_VER_MINOR(ver), minor);

	/* Save the gmu version information */
	gmu->ver = ver;

	return 0;
}

static int hfi_send_acd_feature_ctrl(struct gmu_device *gmu,
		struct adreno_device *adreno_dev)
{
	int ret = 0;

	if (test_bit(ADRENO_ACD_CTRL, &adreno_dev->pwrctrl_flag)) {
		ret = hfi_send_acd_tbl(gmu);
		if (!ret)
			ret = hfi_send_feature_ctrl(gmu, HFI_FEATURE_ACD, 1, 0);
	}

	return ret;
}

int hfi_start(struct kgsl_device *device,
		struct gmu_device *gmu, uint32_t boot_state)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr;
	int result, i;

	if (test_bit(GMU_HFI_ON, &device->gmu_core.flags))
		return 0;

	/* Force read_index to the write_index no matter what */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		if (hdr->read_index != hdr->write_index) {
			dev_err(&gmu->pdev->dev,
				"HFI Q[%d] Index Error: read:0x%X write:0x%X\n",
				i, hdr->read_index, hdr->write_index);
			hdr->read_index = hdr->write_index;
		}
	}

	if (!adreno_is_a640(adreno_dev) && !adreno_is_a680(adreno_dev)) {
		result = hfi_send_gmu_init(gmu, boot_state);
		if (result)
			return result;
	}

	result = hfi_verify_fw_version(device, gmu);
	if (result)
		return result;

	if (HFI_VER_MAJOR(&gmu->hfi) < 2)
		result = hfi_send_dcvstbl_v1(gmu);
	else
		result = hfi_send_dcvstbl(gmu);
	if (result)
		return result;

	result = hfi_send_bwtbl(gmu);
	if (result)
		return result;

	/*
	 * If quirk is enabled send H2F_MSG_TEST and tell the GMU
	 * we are sending no more HFIs until the next boot otherwise
	 * send H2F_MSG_CORE_FW_START and features for A640 devices
	 */
	if (HFI_VER_MAJOR(&gmu->hfi) >= 2) {
		result = hfi_send_feature_ctrl(gmu, HFI_FEATURE_ECP, 0, 0);
		if (result)
			return result;

		result = hfi_send_acd_feature_ctrl(gmu, adreno_dev);
		if (result)
			return result;

		if (test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag)) {
			result = hfi_send_feature_ctrl(gmu, HFI_FEATURE_LM, 1,
					device->pwrctrl.throttle_mask);
			if (result)
				return result;
		}

		result = hfi_send_core_fw_start(gmu);
		if (result)
			return result;
	} else {
		if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
			result = hfi_send_test(gmu);
			if (result)
				return result;
		}
	}
	set_bit(GMU_HFI_ON, &device->gmu_core.flags);
	return 0;
}

void hfi_stop(struct gmu_device *gmu)
{
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr;
	struct kgsl_hfi *hfi = &gmu->hfi;
	struct kgsl_device *device = hfi->kgsldev;
	unsigned int i;


	if (!test_bit(GMU_HFI_ON, &device->gmu_core.flags))
		return;

	/* Flush HFI queues */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];
		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		if (hdr->read_index != hdr->write_index)
			dev_err(&gmu->pdev->dev,
			"HFI queue[%d] is not empty before close: rd=%d,wt=%d",
				i, hdr->read_index, hdr->write_index);
	}

	clear_bit(GMU_HFI_ON, &device->gmu_core.flags);
}

/* Entry point for external HFI requests */
int hfi_send_req(struct gmu_device *gmu, unsigned int id, void *data)
{
	switch (id) {
	case H2F_MSG_LM_CFG: {
		struct hfi_lmconfig_cmd *cmd = data;

		cmd->hdr = CMD_MSG_HDR(H2F_MSG_LM_CFG, sizeof(*cmd));

		return hfi_send_generic_req(gmu, HFI_CMD_ID, &cmd);
	}
	case H2F_MSG_GX_BW_PERF_VOTE: {
		struct hfi_gx_bw_perf_vote_cmd *cmd = data;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return hfi_send_generic_req(gmu, HFI_CMD_ID, cmd);
	}
	case H2F_MSG_PREPARE_SLUMBER: {
		struct hfi_prep_slumber_cmd *cmd = data;

		if (cmd->freq >= MAX_GX_LEVELS || cmd->bw >= MAX_GX_LEVELS)
			return -EINVAL;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return hfi_send_generic_req(gmu, HFI_CMD_ID, cmd);
	}
	case H2F_MSG_START: {
		struct hfi_start_cmd *cmd = data;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return hfi_send_generic_req(gmu, HFI_CMD_ID, cmd);
	}
	case H2F_MSG_GET_VALUE: {
		return hfi_send_get_value(gmu, data);
	}
	case H2F_MSG_SET_VALUE: {
		struct hfi_set_value_cmd *cmd = data;

		cmd->hdr = CMD_MSG_HDR(id, sizeof(*cmd));

		return hfi_send_generic_req(gmu, HFI_CMD_ID, cmd);
	}
	default:
		break;
	}

	return -EINVAL;
}

/* HFI interrupt handler */
irqreturn_t hfi_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct kgsl_hfi *hfi = &gmu->hfi;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int status = 0;

	adreno_read_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_GMU2HOST_INTR_INFO, &status);
	adreno_write_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_GMU2HOST_INTR_CLR, status);

	if (status & HFI_IRQ_DBGQ_MASK)
		tasklet_hi_schedule(&hfi->tasklet);
	if (status & HFI_IRQ_CM3_FAULT_MASK) {
		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU CM3 fault interrupt received\n");
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
		adreno_dispatcher_schedule(device);
	}
	if (status & ~HFI_IRQ_MASK)
		dev_err_ratelimited(&gmu->pdev->dev,
				"Unhandled HFI interrupts 0x%lx\n",
				status & ~HFI_IRQ_MASK);

	return IRQ_HANDLED;
}
