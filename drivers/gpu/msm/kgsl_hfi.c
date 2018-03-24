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
#include "kgsl_gmu.h"
#include "adreno.h"
#include "kgsl_trace.h"

#define HFI_QUEUE_OFFSET(i)		\
		((sizeof(struct hfi_queue_table)) + \
		((i) * HFI_QUEUE_SIZE))

#define HOST_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->hostptr + HFI_QUEUE_OFFSET(i))

#define GMU_QUEUE_START_ADDR(hfi_mem, i) \
	((hfi_mem)->gmuaddr + HFI_QUEUE_OFFSET(i))

#define MSG_HDR_GET_ID(hdr) ((hdr) & 0xFF)
#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_TYPE(hdr) (((hdr) >> 16) & 0xF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

/* Size in below functions are in unit of dwords */
static int hfi_queue_read(struct gmu_device *gmu, uint32_t queue_idx,
		void *data, unsigned int max_size)
{
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue;
	uint32_t *output = data;
	uint32_t msg_hdr;
	uint32_t i, read;
	uint32_t size, id;
	int result = 0;

	if (hdr->read_index == hdr->write_index) {
		hdr->rx_req = 1;
		return -ENODATA;
	}

	queue = HOST_QUEUE_START_ADDR(mem_addr, queue_idx);
	msg_hdr = queue[hdr->read_index];
	size = MSG_HDR_GET_SIZE(msg_hdr);
	id = MSG_HDR_GET_ID(msg_hdr);

	if (size > max_size) {
		dev_err(&gmu->pdev->dev,
		"Received invalid msg: size=%d dwords, rd idx=%d, id=%d\n",
			size, hdr->read_index, id);
		return -EMSGSIZE;
	}

	read = hdr->read_index;

	if (read < hdr->queue_size) {
		for (i = 0; i < size; i++) {
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
	hdr->read_index = read;

	return result;
}

/* Size in below functions are in unit of dwords */
static int hfi_queue_write(struct gmu_device *gmu, uint32_t queue_idx,
		uint32_t *msg)
{
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	struct hfi_queue_table *tbl = gmu->hfi_mem->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue;
	struct kgsl_hfi *hfi = &gmu->hfi;
	uint32_t i, write, empty_space;
	uint32_t size = MSG_HDR_GET_SIZE(*msg);
	uint32_t id = MSG_HDR_GET_ID(*msg);

	if (hdr->enabled == 0)
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

	hdr->write_index = write;

	mutex_unlock(&hfi->cmdq_mutex);

	/*
	 * Memory barrier to make sure packet and write index are written before
	 * an interrupt is raised
	 */
	wmb();

	/* Send interrupt to GMU to receive the message */
	adreno_write_gmureg(ADRENO_DEVICE(device),
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
	int i;
	struct hfi_queue_table *tbl;
	struct hfi_queue_header *hdr;
	int queue_prio[HFI_QUEUE_MAX] = {
		HFI_H2F_QPRI_CMD,
		HFI_F2H_QPRI_MSG,
		HFI_F2H_QPRI_DEBUG
	};
	int queue_ids[HFI_QUEUE_MAX] = {0, 4, 5};

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
		hdr->type = QUEUE_HDR_TYPE(queue_ids[i], queue_prio[i], 0,  0);
		hdr->enabled = 0x1;
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

static void receive_ack_msg(struct gmu_device *gmu, struct hfi_msg_rsp *rsp)
{
	struct kgsl_hfi *hfi = &gmu->hfi;
	struct pending_msg *msg = NULL, *next;
	bool in_queue = false;

	trace_kgsl_hfi_receive(MSG_HDR_GET_ID(rsp->ret_hdr),
		MSG_HDR_GET_SIZE(rsp->ret_hdr),
		MSG_HDR_GET_SEQNUM(rsp->ret_hdr));

	spin_lock_bh(&hfi->msglock);
	list_for_each_entry_safe(msg, next, &hfi->msglist, node) {
		if (msg->msg_id == MSG_HDR_GET_ID(rsp->ret_hdr) &&
				msg->seqnum ==
				MSG_HDR_GET_SEQNUM(rsp->ret_hdr)) {
			in_queue = true;
			break;
		}
	}

	if (in_queue == false) {
		spin_unlock_bh(&hfi->msglock);
		dev_err(&gmu->pdev->dev,
				"Cannot find receiver of ack msg with id=%d\n",
				MSG_HDR_GET_ID(rsp->ret_hdr));
		return;
	}

	memcpy(&msg->results, (void *) rsp, MSG_HDR_GET_SIZE(rsp->hdr) << 2);
	complete(&msg->msg_complete);
	spin_unlock_bh(&hfi->msglock);
}

static void receive_err_msg(struct gmu_device *gmu, struct hfi_msg_rsp *rsp)
{
	struct hfi_fw_err_msg *err = (struct hfi_fw_err_msg *) rsp;

	dev_err(&gmu->pdev->dev, "FW error with error code %d\n",
			err->error_code);
}

#define MSG_HDR_SET_SEQNUM(hdr, num) \
	(((hdr) & 0xFFFFF) | ((num) << 20))

static int hfi_send_msg(struct gmu_device *gmu, void *data,
		struct pending_msg *ret_msg)
{
	int rc = 0;
	struct kgsl_hfi *hfi = &gmu->hfi;
	unsigned int seqnum = atomic_inc_return(&hfi->seqnum);
	uint32_t *msg = data;

	*msg = MSG_HDR_SET_SEQNUM(*msg, seqnum);
	if (MSG_HDR_GET_TYPE(*msg) != HFI_MSG_CMD)
		return hfi_queue_write(gmu, HFI_CMD_IDX, msg);

	/* For messages of type HFI_MSG_CMD we must handle the ack */
	init_completion(&ret_msg->msg_complete);
	ret_msg->msg_id = MSG_HDR_GET_ID(*msg);
	ret_msg->seqnum = MSG_HDR_GET_SEQNUM(*msg);

	spin_lock_bh(&hfi->msglock);
	list_add_tail(&ret_msg->node, &hfi->msglist);
	spin_unlock_bh(&hfi->msglock);

	rc = hfi_queue_write(gmu, HFI_CMD_IDX, msg);
	if (rc)
		goto done;

	rc = wait_for_completion_timeout(
			&ret_msg->msg_complete,
			msecs_to_jiffies(HFI_RSP_TIMEOUT));
	if (!rc) {
		dev_err(&gmu->pdev->dev,
				"Receiving GMU ack %d timed out\n",
				MSG_HDR_GET_ID(*msg));
		rc = -ETIMEDOUT;
		goto done;
	}

	/* If we got here we succeeded */
	rc = 0;
done:
	spin_lock_bh(&hfi->msglock);
	list_del(&ret_msg->node);
	spin_unlock_bh(&hfi->msglock);
	return rc;
}

#define CMD_MSG_HDR(id, size) \
	((HFI_MSG_CMD << 16) | (((size) & 0xFF) << 8) | ((id) & 0xFF))

static int hfi_send_gmu_init(struct gmu_device *gmu, uint32_t boot_state)
{
	struct hfi_gmu_init_cmd init_msg = {
		.hdr = CMD_MSG_HDR(H2F_MSG_INIT,
				sizeof(struct hfi_gmu_init_cmd) >> 2),
		.seg_id = 0,
		.dbg_buffer_addr = (unsigned int) gmu->dump_mem->gmuaddr,
		.dbg_buffer_size = (unsigned int) gmu->dump_mem->size,
		.boot_state = boot_state,
	};

	struct hfi_msg_rsp *rsp;
	int rc = 0;
	struct pending_msg msg;

	rc = hfi_send_msg(gmu, (uint32_t *)&init_msg, &msg);
	if (rc)
		return rc;

	rsp = &msg.results;
	rc = rsp->error;
	if (!rc)
		gmu->hfi.gmu_init_done = true;
	else
		dev_err(&gmu->pdev->dev,
			"gmu init message failed with error=%d\n", rc);
	return rc;
}

static int hfi_get_fw_version(struct gmu_device *gmu,
		uint32_t expected_ver, uint32_t *ver)
{
	struct hfi_fw_version_cmd fw_ver = {
		.hdr = CMD_MSG_HDR(H2F_MSG_FW_VER,
				sizeof(struct hfi_fw_version_cmd) >> 2),
		.supported_ver = expected_ver,
	};
	struct hfi_msg_rsp *rsp;
	int rc = 0;
	struct pending_msg msg;

	rc = hfi_send_msg(gmu, (uint32_t *)((void *)&fw_ver), &msg);
	if (rc)
		return rc;

	rsp = &msg.results;
	rc = rsp->error;
	if (!rc)
		*ver = rsp->payload[0];
	else
		dev_err(&gmu->pdev->dev,
			"gmu get fw ver failed with error=%d\n", rc);
	return rc;
}

static int hfi_send_lmconfig(struct gmu_device *gmu)
{
	struct hfi_lmconfig_cmd lmconfig = {
		.hdr = CMD_MSG_HDR(H2F_MSG_LM_CFG,
				sizeof(struct hfi_lmconfig_cmd) >> 2),
		.limit_conf = gmu->lm_config,
		.bcl_conf = gmu->bcl_config,
	};
	struct hfi_msg_rsp *rsp;
	int rc = 0;
	struct pending_msg msg;

	if (gmu->lm_dcvs_level > MAX_GX_LEVELS)
		lmconfig.lm_enable_bitmask = 0;
	else
		lmconfig.lm_enable_bitmask =
			(1 << (gmu->lm_dcvs_level + 1)) - 1;

	rc = hfi_send_msg(gmu, (uint32_t *)&lmconfig, &msg);
	if (rc)
		return rc;

	rsp = &msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send lmconfig failed with error=%d\n", rc);
	return rc;
}

static int hfi_send_perftbl(struct gmu_device *gmu)
{
	struct hfi_dcvstable_cmd dcvstbl = {
		.hdr = CMD_MSG_HDR(H2F_MSG_PERF_TBL,
				sizeof(struct hfi_dcvstable_cmd) >> 2),
		.gpu_level_num = gmu->num_gpupwrlevels,
		.gmu_level_num = gmu->num_gmupwrlevels,
	};
	struct hfi_msg_rsp *rsp;
	struct pending_msg msg;
	int i, rc = 0;

	for (i = 0; i < gmu->num_gpupwrlevels; i++) {
		dcvstbl.gx_votes[i].vote = gmu->rpmh_votes.gx_votes[i];
		/* Divide by 1000 to convert to kHz */
		dcvstbl.gx_votes[i].freq = gmu->gpu_freqs[i] / 1000;
	}

	for (i = 0; i < gmu->num_gmupwrlevels; i++) {
		dcvstbl.cx_votes[i].vote = gmu->rpmh_votes.cx_votes[i];
		dcvstbl.cx_votes[i].freq = gmu->gmu_freqs[i] / 1000;

	}

	rc = hfi_send_msg(gmu, (uint32_t *)&dcvstbl, &msg);
	if (rc)
		return rc;

	rsp = &msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send perf table failed with error=%d\n", rc);
	return rc;
}

static int hfi_send_bwtbl(struct gmu_device *gmu)
{
	struct hfi_bwtable_cmd bwtbl = {
		.hdr = CMD_MSG_HDR(H2F_MSG_BW_VOTE_TBL,
				sizeof(struct hfi_bwtable_cmd) >> 2),
		.bw_level_num = gmu->num_bwlevels,
		.cnoc_cmds_num =
			gmu->rpmh_votes.cnoc_votes.cmds_per_bw_vote,
		.cnoc_wait_bitmask =
			gmu->rpmh_votes.cnoc_votes.cmds_wait_bitmask,
		.ddr_cmds_num = gmu->rpmh_votes.ddr_votes.cmds_per_bw_vote,
		.ddr_wait_bitmask = gmu->rpmh_votes.ddr_votes.cmds_wait_bitmask,
	};
	struct hfi_msg_rsp *rsp;
	struct pending_msg msg;
	int i, j, rc = 0;

	for (i = 0; i < bwtbl.ddr_cmds_num; i++)
		bwtbl.ddr_cmd_addrs[i] = gmu->rpmh_votes.ddr_votes.cmd_addrs[i];

	for (i = 0; i < bwtbl.bw_level_num; i++)
		for (j = 0; j < bwtbl.ddr_cmds_num; j++)
			bwtbl.ddr_cmd_data[i][j] =
				gmu->rpmh_votes.ddr_votes.cmd_data[i][j];

	for (i = 0; i < bwtbl.cnoc_cmds_num; i++)
		bwtbl.cnoc_cmd_addrs[i] =
			gmu->rpmh_votes.cnoc_votes.cmd_addrs[i];

	for (i = 0; i < MAX_CNOC_LEVELS; i++)
		for (j = 0; j < bwtbl.cnoc_cmds_num; j++)
			bwtbl.cnoc_cmd_data[i][j] =
				gmu->rpmh_votes.cnoc_votes.cmd_data[i][j];

	rc = hfi_send_msg(gmu, (uint32_t *)&bwtbl, &msg);
	if (rc)
		return rc;

	rsp = &msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send bw table failed with error=%d\n", rc);
	return rc;
}

static int hfi_send_test(struct gmu_device *gmu)
{
	struct hfi_test_cmd test_cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_TEST, sizeof(test_cmd) >> 2),
	};
	struct pending_msg msg;

	return hfi_send_msg(gmu, (uint32_t *)&test_cmd, &msg);
}

#define DCVS_VOTE(perf, clk_opt) ((((clk_opt) & 0xF) << 28) | ((perf) & 0xFF))
#define BW_VOTE(bw) ((bw) & 0xFF)

static int hfi_send_gx_bw_perf_vote(struct gmu_device *gmu, uint32_t perf_idx,
		uint32_t bw_idx, enum rpm_ack_type ack_type)
{
	struct hfi_gx_bw_perf_vote_cmd cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_GX_BW_PERF_VOTE,
				sizeof(struct hfi_gx_bw_perf_vote_cmd) >> 2),
		.ack_type = ack_type,
		.freq = DCVS_VOTE(perf_idx, OPTION_AT_LEAST),
		.bw = BW_VOTE(bw_idx),
	};
	struct hfi_msg_rsp *rsp;
	int rc = 0;
	struct pending_msg msg;

	rc = hfi_send_msg(gmu, (uint32_t *)&cmd, &msg);
	if (rc)
		return rc;

	rsp = &msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send dcvs cmd failed with error=%d\n", rc);
	return rc;
}

static int hfi_notify_slumber(struct gmu_device *gmu,
		uint32_t init_perf_idx, uint32_t init_bw_idx)
{
	struct hfi_prep_slumber_cmd slumber_cmd = {
		.hdr = CMD_MSG_HDR(H2F_MSG_PREPARE_SLUMBER,
				sizeof(struct hfi_prep_slumber_cmd) >> 2),
		.init_bw_idx = init_bw_idx,
		.init_perf_idx = init_perf_idx,
	};
	struct hfi_msg_rsp *rsp;
	int rc = 0;
	struct pending_msg msg;

	if (init_perf_idx >= MAX_GX_LEVELS || init_bw_idx >= MAX_GX_LEVELS)
		return -EINVAL;

	rc = hfi_send_msg(gmu, (uint32_t *)&slumber_cmd, &msg);
	if (rc)
		return rc;

	rsp = &msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send slumber notification failed with error=%d\n",
			rc);
	return rc;
}

void hfi_receiver(unsigned long data)
{
	struct gmu_device *gmu;
	struct hfi_msg_rsp response;

	if (!data)
		return;

	gmu = (struct gmu_device *)data;

	while (hfi_queue_read(gmu, HFI_MSG_IDX,
			&response, sizeof(response)) > 0) {
		if (MSG_HDR_GET_SIZE(response.hdr) > (sizeof(response) >> 2)) {
			dev_err(&gmu->pdev->dev,
					"Ack is too large, id=%d, size=%d\n",
					MSG_HDR_GET_ID(response.ret_hdr),
					MSG_HDR_GET_SIZE(response.hdr));
			continue;
		}

		switch (MSG_HDR_GET_ID(response.hdr)) {
		case F2H_MSG_ACK:
			receive_ack_msg(gmu, &response);
			break;
		case F2H_MSG_ERR:
			receive_err_msg(gmu, &response);
			break;
		default:
			dev_err(&gmu->pdev->dev,
				"Invalid packet with id %d\n",
				MSG_HDR_GET_ID(response.hdr));
			break;
		}
	};
}

#define FW_VER_MAJOR(ver) (((ver) >> 28) & 0xF)
#define FW_VER_MINOR(ver) (((ver) >> 16) & 0xFFF)
#define FW_VERSION(major, minor) \
	((((major) & 0xF) << 28) | (((minor) & 0xFFF) << 16))

static int hfi_verify_fw_version(struct gmu_device *gmu)
{
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int result;
	unsigned int ver = 0, major, minor;

	major = adreno_dev->gpucore->gpmu_major;
	minor = adreno_dev->gpucore->gpmu_minor;

	result = hfi_get_fw_version(gmu, FW_VERSION(major, minor), &ver);
	if (result) {
		dev_err_once(&gmu->pdev->dev,
				"Failed to get FW version via HFI\n");
		return result;
	}

	/* For now, warn once. Could return error later if needed */
	if (major != FW_VER_MAJOR(ver))
		dev_err_once(&gmu->pdev->dev,
				"FW Major Error: Wanted %d, got %d\n",
				major, FW_VER_MAJOR(ver));

	if (minor > FW_VER_MINOR(ver))
		dev_err_once(&gmu->pdev->dev,
				"FW Minor Error: Wanted < %d, got %d\n",
				FW_VER_MINOR(ver), minor);

	/* Save the gmu version information */
	gmu->ver = ver;

	return 0;
}

int hfi_start(struct gmu_device *gmu, uint32_t boot_state)
{
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int result;

	if (test_bit(GMU_HFI_ON, &gmu->flags))
		return 0;

	result = hfi_send_gmu_init(gmu, boot_state);
	if (result)
		return result;

	result = hfi_verify_fw_version(gmu);
	if (result)
		return result;

	result = hfi_send_perftbl(gmu);
	if (result)
		return result;

	result = hfi_send_bwtbl(gmu);
	if (result)
		return result;

	/* Tell the GMU we are sending no more HFIs until the next boot */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		result = hfi_send_test(gmu);
		if (result)
			return result;
	}

	set_bit(GMU_HFI_ON, &gmu->flags);
	return 0;
}

void hfi_stop(struct gmu_device *gmu)
{
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr;
	unsigned int i;


	if (!test_bit(GMU_HFI_ON, &gmu->flags))
		return;

	/* Flush HFI queues */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		hdr = &tbl->qhdr[i];

		if (hdr->read_index != hdr->write_index)
			dev_err(&gmu->pdev->dev,
			"HFI queue at idx %d is not empty before close: rd=%d,wt=%d",
				i, hdr->read_index, hdr->write_index);

		hdr->read_index = 0x0;
		hdr->write_index = 0x0;
	}

	clear_bit(GMU_HFI_ON, &gmu->flags);
}

/* Entry point for external HFI requests */
int hfi_send_req(struct gmu_device *gmu, unsigned int id, void *data)
{
	switch (id) {
	case H2F_MSG_LM_CFG: {
		return hfi_send_lmconfig(gmu);
	}
	case H2F_MSG_GX_BW_PERF_VOTE: {
		struct hfi_dcvs_vote *req = (struct hfi_dcvs_vote *)data;

		return hfi_send_gx_bw_perf_vote(gmu, req->perf_idx, req->bw_idx,
				req->ack_type);
	}
	case H2F_MSG_PREPARE_SLUMBER: {
		struct hfi_dcvs_vote *req = (struct hfi_dcvs_vote *)data;

		return hfi_notify_slumber(gmu, req->perf_idx, req->bw_idx);
	}
	default:
		break;
	}

	return -EINVAL;
}
