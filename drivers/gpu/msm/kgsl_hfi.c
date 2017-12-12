/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

/* Size in below functions are in unit of dwords */
static int hfi_msgq_read(struct gmu_device *gmu,
		enum hfi_queue_type queue_idx, void *msg,
		unsigned int max_size)
{
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	struct hfi_queue_table *tbl = mem_addr->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue = HOST_QUEUE_START_ADDR(mem_addr, queue_idx);
	uint32_t *output = msg;
	struct hfi_msg_hdr *msg_hdr;
	int i, read, result = 0;

	if (hdr->read_index == hdr->write_index) {
		hdr->rx_req = 1;
		return -ENODATA;
	}

	msg_hdr = (struct hfi_msg_hdr *)&queue[hdr->read_index];

	if (msg_hdr->size > max_size) {
		dev_err(&gmu->pdev->dev,
			"Received invalid msg: size=%d dwords, rd idx=%d, id=%d\n",
			msg_hdr->size, hdr->read_index, msg_hdr->id);
		return -EMSGSIZE;
	}

	read = hdr->read_index;

	if (read < hdr->queue_size) {
		for (i = 0; i < msg_hdr->size; i++) {
			output[i] = queue[read];
			read = (read + 1)%hdr->queue_size;
		}
		result = msg_hdr->size;
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
static int hfi_cmdq_write(struct gmu_device *gmu,
		enum hfi_queue_type queue_idx,
		struct hfi_msg_hdr *msg)
{
	struct kgsl_device *device = container_of(gmu, struct kgsl_device, gmu);
	struct hfi_queue_table *tbl = gmu->hfi_mem->hostptr;
	struct hfi_queue_header *hdr = &tbl->qhdr[queue_idx];
	uint32_t *queue = HOST_QUEUE_START_ADDR(gmu->hfi_mem, queue_idx);
	uint32_t *input = (uint32_t *) msg;
	struct kgsl_hfi *hfi = &gmu->hfi;
	uint32_t i, write, empty_space;

	if (msg->size > HFI_MAX_MSG_SIZE) {
		dev_err(&gmu->pdev->dev,
			"Message too big to send: sz=%d, id=%d\n",
			msg->size, msg->id);
		return -EINVAL;
	}

	trace_kgsl_hfi_send(msg->id, msg->size, msg->seqnum);

	mutex_lock(&hfi->cmdq_mutex);

	empty_space = (hdr->write_index >= hdr->read_index) ?
			(hdr->queue_size - (hdr->write_index - hdr->read_index))
			: (hdr->read_index - hdr->write_index);

	if (empty_space < msg->size) {
		dev_err(&gmu->pdev->dev,
			"Insufficient bufsize %d for msg id=%d of size %d\n",
			empty_space, msg->id, msg->size);

		hdr->drop_cnt++;
		mutex_unlock(&hfi->cmdq_mutex);
		return -ENOSPC;
	}

	write = hdr->write_index;

	for (i = 0; i < msg->size; i++) {
		queue[write] = input[i];
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

	return msg->size;
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
		hdr->status = 0x1;
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

	trace_kgsl_hfi_receive(rsp->ret_hdr.id,
		rsp->ret_hdr.size,
		rsp->ret_hdr.seqnum);

	spin_lock_bh(&hfi->msglock);
	list_for_each_entry_safe(msg, next, &hfi->msglist, node) {
		if (msg->msg_id == rsp->ret_hdr.id &&
				msg->seqnum == rsp->ret_hdr.seqnum) {
			in_queue = true;
			break;
		}
	}

	if (in_queue == false) {
		spin_unlock_bh(&hfi->msglock);
		dev_err(&gmu->pdev->dev,
				"Cannot find receiver of ack msg with id=%d\n",
				rsp->ret_hdr.id);
		return;
	}

	memcpy(&msg->results, (void *) rsp, rsp->hdr.size << 2);
	complete(&msg->msg_complete);
	spin_unlock_bh(&hfi->msglock);
}

static void receive_err_msg(struct gmu_device *gmu, struct hfi_msg_rsp *rsp)
{
	struct hfi_fw_err_msg *err = (struct hfi_fw_err_msg *) rsp;

	dev_err(&gmu->pdev->dev, "FW error with error code %d\n",
			err->error_code);
}

static int hfi_send_msg(struct gmu_device *gmu, struct hfi_msg_hdr *msg,
		unsigned int size, struct pending_msg *ret_msg)
{
	int rc = 0;
	struct kgsl_hfi *hfi = &gmu->hfi;

	msg->seqnum = atomic_inc_return(&hfi->seqnum);
	if (msg->type != HFI_MSG_CMD) {
		if (hfi_cmdq_write(gmu, HFI_CMD_QUEUE, msg) != size)
			rc = -EINVAL;
		return rc;
	}

	/* For messages of type HFI_MSG_CMD we must handle the ack */
	init_completion(&ret_msg->msg_complete);
	ret_msg->msg_id = msg->id;
	ret_msg->seqnum = msg->seqnum;

	spin_lock_bh(&hfi->msglock);
	list_add_tail(&ret_msg->node, &hfi->msglist);
	spin_unlock_bh(&hfi->msglock);

	if (hfi_cmdq_write(gmu, HFI_CMD_QUEUE, msg) != size) {
		rc = -EINVAL;
		goto done;
	}

	rc = wait_for_completion_timeout(
			&ret_msg->msg_complete,
			msecs_to_jiffies(HFI_RSP_TIMEOUT));
	if (!rc) {
		dev_err(&gmu->pdev->dev,
				"Receiving GMU ack %d timed out\n", msg->id);
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

int hfi_send_gmu_init(struct gmu_device *gmu, uint32_t boot_state)
{
	struct hfi_gmu_init_cmd init_msg = {
		.hdr = {
			.id = H2F_MSG_INIT,
			.size = sizeof(init_msg) >> 2,
			.type = HFI_MSG_CMD,
		},
		.seg_id = 0,
		.dbg_buffer_addr = (unsigned int) gmu->dump_mem->gmuaddr,
		.dbg_buffer_size = (unsigned int) gmu->dump_mem->size,
		.boot_state = boot_state,
	};

	struct hfi_msg_rsp *rsp;
	uint32_t msg_size_dwords = (sizeof(init_msg)) >> 2;
	int rc = 0;
	struct pending_msg msg;

	rc = hfi_send_msg(gmu, &init_msg.hdr, msg_size_dwords, &msg);
	if (rc)
		return rc;

	rsp = (struct hfi_msg_rsp *) &msg.results;
	rc = rsp->error;
	if (!rc)
		gmu->hfi.gmu_init_done = true;
	else
		dev_err(&gmu->pdev->dev,
			"gmu init message failed with error=%d\n", rc);
	return rc;
}

int hfi_get_fw_version(struct gmu_device *gmu,
		uint32_t expected_ver, uint32_t *ver)
{
	struct hfi_fw_version_cmd fw_ver = {
		.hdr = {
			.id = H2F_MSG_FW_VER,
			.size = sizeof(fw_ver) >> 2,
			.type = HFI_MSG_CMD
		},
		.supported_ver = expected_ver,
	};
	struct hfi_msg_rsp *rsp;
	uint32_t msg_size_dwords = (sizeof(fw_ver)) >> 2;
	int rc = 0;
	struct pending_msg msg;

	rc = hfi_send_msg(gmu, &fw_ver.hdr, msg_size_dwords, &msg);
	if (rc)
		return rc;

	rsp = (struct hfi_msg_rsp *) &msg.results;
	rc = rsp->error;
	if (!rc)
		*ver = rsp->payload[0];
	else
		dev_err(&gmu->pdev->dev,
			"gmu get fw ver failed with error=%d\n", rc);
	return rc;
}

int hfi_send_lmconfig(struct gmu_device *gmu)
{
	struct hfi_lmconfig_cmd lmconfig = {
		.hdr = {
			.id =  H2F_MSG_LM_CFG,
			.size = sizeof(lmconfig) >> 2,
			.type = HFI_MSG_CMD
		},
		.limit_conf = gmu->lm_config,
		.bcl_conf.bcl = gmu->bcl_config
	};
	struct hfi_msg_rsp *rsp;
	uint32_t msg_size_dwords = (sizeof(lmconfig)) >> 2;
	int rc = 0;
	struct pending_msg msg;

	if (gmu->lm_dcvs_level > MAX_GX_LEVELS)
		lmconfig.lm_enable_bitmask = 0;
	else
		lmconfig.lm_enable_bitmask =
			(1 << (gmu->lm_dcvs_level + 1)) - 1;

	rc = hfi_send_msg(gmu, &lmconfig.hdr, msg_size_dwords, &msg);
	if (rc)
		return rc;

	rsp = (struct hfi_msg_rsp *) &msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send lmconfig failed with error=%d\n", rc);
	return rc;
}

int hfi_send_perftbl(struct gmu_device *gmu)
{
	struct hfi_dcvstable_cmd dcvstbl = {
		.hdr = {
			.id = H2F_MSG_PERF_TBL,
			.size = sizeof(dcvstbl) >> 2,
			.type = HFI_MSG_CMD
		},
	};
	struct hfi_msg_rsp *rsp;
	struct pending_msg msg;
	uint32_t msg_size = (sizeof(dcvstbl)) >> 2;
	int i, rc = 0;

	dcvstbl.gpu_level_num = gmu->num_gpupwrlevels;
	dcvstbl.gmu_level_num = gmu->num_gmupwrlevels;

	for (i = 0; i < gmu->num_gpupwrlevels; i++) {
		dcvstbl.gx_votes[i].vote = gmu->rpmh_votes.gx_votes[i];
		/* Divide by 1000 to convert to kHz */
		dcvstbl.gx_votes[i].freq = gmu->gpu_freqs[i] / 1000;
	}

	for (i = 0; i < gmu->num_gmupwrlevels; i++) {
		dcvstbl.cx_votes[i].vote = gmu->rpmh_votes.cx_votes[i];
		dcvstbl.cx_votes[i].freq = gmu->gmu_freqs[i] / 1000;

	}

	rc = hfi_send_msg(gmu, &dcvstbl.hdr, msg_size, &msg);
	if (rc)
		return rc;

	rsp = (struct hfi_msg_rsp *)&msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send perf table failed with error=%d\n", rc);
	return rc;
}

int hfi_send_bwtbl(struct gmu_device *gmu)
{
	struct hfi_bwtable_cmd bwtbl = {
		.hdr = {
			.id = H2F_MSG_BW_VOTE_TBL,
			.size = sizeof(bwtbl) >> 2,
			.type = HFI_MSG_CMD,
		},
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
	uint32_t msg_size_dwords = (sizeof(bwtbl)) >> 2;
	int i, j, rc = 0;

	for (i = 0; i < bwtbl.ddr_cmds_num; i++)
		bwtbl.ddr_cmd_addrs[i] = gmu->rpmh_votes.ddr_votes.cmd_addrs[i];

	for (i = 0; i < bwtbl.bw_level_num; i++)
		for (j = 0; j < bwtbl.ddr_cmds_num; j++)
			bwtbl.ddr_cmd_data[i][j] =
					gmu->rpmh_votes.
					ddr_votes.cmd_data[i][j];

	for (i = 0; i < bwtbl.cnoc_cmds_num; i++)
		bwtbl.cnoc_cmd_addrs[i] =
				gmu->rpmh_votes.cnoc_votes.cmd_addrs[i];

	for (i = 0; i < MAX_CNOC_LEVELS; i++)
		for (j = 0; j < bwtbl.cnoc_cmds_num; j++)
			bwtbl.cnoc_cmd_data[i][j] =
					gmu->rpmh_votes.cnoc_votes.
					cmd_data[i][j];

	rc = hfi_send_msg(gmu, &bwtbl.hdr, msg_size_dwords, &msg);
	if (rc)
		return rc;

	rsp = (struct hfi_msg_rsp *) &msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send bw table failed with error=%d\n", rc);
	return rc;
}

static int hfi_send_test(struct gmu_device *gmu)
{
	struct hfi_test_cmd test_msg = {
		.hdr = {
			.id = H2F_MSG_TEST,
			.size = sizeof(test_msg) >> 2,
			.type = HFI_MSG_CMD,
		},
	};
	uint32_t msg_size_dwords = (sizeof(test_msg)) >> 2;
	struct pending_msg msg;

	return hfi_send_msg(gmu, (struct hfi_msg_hdr *)&test_msg.hdr,
			msg_size_dwords, &msg);
}

int hfi_send_dcvs_vote(struct gmu_device *gmu, uint32_t perf_idx,
		uint32_t bw_idx, enum rpm_ack_type ack_type)
{
	struct hfi_dcvs_cmd dcvs_cmd = {
		.hdr = {
			.id = H2F_MSG_DCVS_VOTE,
			.size = sizeof(dcvs_cmd) >> 2,
			.type = HFI_MSG_CMD,
		},
		.ack_type = ack_type,
		.freq = {
			.perf_idx = perf_idx,
			.clkset_opt = OPTION_AT_LEAST,
		},
		.bw = {
			.bw_idx = bw_idx,
		},

	};
	struct hfi_msg_rsp *rsp;
	uint32_t msg_size_dwords = (sizeof(dcvs_cmd)) >> 2;
	int rc = 0;
	struct pending_msg msg;

	rc = hfi_send_msg(gmu, &dcvs_cmd.hdr, msg_size_dwords, &msg);
	if (rc)
		return rc;

	rsp = (struct hfi_msg_rsp *)&msg.results;
	rc = rsp->error;
	if (rc)
		dev_err(&gmu->pdev->dev,
			"gmu send dcvs cmd failed with error=%d\n", rc);
	return rc;
}

int hfi_notify_slumber(struct gmu_device *gmu,
		uint32_t init_perf_idx, uint32_t init_bw_idx)
{
	struct hfi_prep_slumber_cmd slumber_cmd = {
		.hdr = {
			.id = H2F_MSG_PREPARE_SLUMBER,
			.size = sizeof(slumber_cmd) >> 2,
			.type = HFI_MSG_CMD,
		},
		.init_bw_idx = init_bw_idx,
		.init_perf_idx = init_perf_idx,
	};
	struct hfi_msg_rsp *rsp;
	uint32_t msg_size_dwords = (sizeof(slumber_cmd)) >> 2;
	int rc = 0;
	struct pending_msg msg;

	if (init_perf_idx >= MAX_GX_LEVELS || init_bw_idx >= MAX_GX_LEVELS)
		return -EINVAL;

	rc = hfi_send_msg(gmu, &slumber_cmd.hdr, msg_size_dwords, &msg);
	if (rc)
		return rc;

	rsp = (struct hfi_msg_rsp *) &msg.results;
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

	while (hfi_msgq_read(gmu, HFI_MSG_QUEUE,
			&response, sizeof(response)) > 0) {
		if (response.hdr.size > (sizeof(response) >> 2)) {
			dev_err(&gmu->pdev->dev,
					"Ack is too large, id=%d, size=%d\n",
					response.ret_hdr.id,
					response.hdr.size);
			continue;
		}

		switch (response.hdr.id) {
		case F2H_MSG_ACK:
			receive_ack_msg(gmu, &response);
			break;
		case F2H_MSG_ERR:
			receive_err_msg(gmu, &response);
			break;
		default:
			dev_err(&gmu->pdev->dev,
				"Invalid packet with id %d\n", response.hdr.id);
			break;
		}
	};
}

int hfi_start(struct gmu_device *gmu, uint32_t boot_state)
{
	struct kgsl_device *device =
			container_of(gmu, struct kgsl_device, gmu);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct device *dev = &gmu->pdev->dev;
	int result;
	unsigned int ver = 0, major, minor;

	if (test_bit(GMU_HFI_ON, &gmu->flags))
		return 0;

	result = hfi_send_gmu_init(gmu, boot_state);
	if (result)
		return result;

	major = adreno_dev->gpucore->gpmu_major;
	minor = adreno_dev->gpucore->gpmu_minor;
	result = hfi_get_fw_version(gmu,
			FW_VERSION(major, minor), &ver);
	if (result)
		dev_err(dev, "Failed to get FW version via HFI\n");

	gmu->ver = ver;
	if (major != FW_VER_MAJOR(ver))
		WARN_ONCE(1, "FW version major %d error (expect %d)\n",
				FW_VER_MAJOR(ver),
				adreno_dev->gpucore->gpmu_major);

	if (minor > FW_VER_MINOR(ver))
		WARN_ONCE(1, "FW version minor %d error (expect %d)\n",
				FW_VER_MINOR(ver),
				adreno_dev->gpucore->gpmu_minor);

	result = hfi_send_perftbl(gmu);
	if (result)
		return result;

	result = hfi_send_bwtbl(gmu);
	if (result)
		return result;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM) &&
		test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag)) {
		gmu->lm_config.lm_type = 1;
		gmu->lm_config.lm_sensor_type = 1;
		gmu->lm_config.throttle_config = 1;
		gmu->lm_config.idle_throttle_en = 0;
		gmu->lm_config.acd_en = 0;
		gmu->bcl_config = 0;
		gmu->lm_dcvs_level = 0;

		result = hfi_send_lmconfig(gmu);
		if (result) {
			dev_err(dev, "Failure enabling LM (%d)\n",
					result);
			return result;
		}
	}

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
