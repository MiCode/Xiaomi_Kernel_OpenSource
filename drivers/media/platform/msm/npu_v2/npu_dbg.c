/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * Includes
 */
#include "npu_common.h"
#include "npu_firmware.h"
#include "npu_hw.h"
#include "npu_hw_access.h"
#include "npu_mgr.h"

/*
 * Function Definitions - Debug
 */
void npu_dump_ipc_packet(struct npu_device *npu_dev, void *cmd_ptr)
{
	int32_t *ptr = (int32_t *)cmd_ptr;
	uint32_t cmd_pkt_size = 0;
	int i;

	cmd_pkt_size = (*(uint32_t *)cmd_ptr);

	NPU_ERR("IPC packet size %d content:\n", cmd_pkt_size);
	for (i = 0; i < cmd_pkt_size/4; i++)
		NPU_ERR("%x\n", ptr[i]);
}

static void npu_dump_ipc_queue(struct npu_device *npu_dev, uint32_t target_que)
{
	struct hfi_queue_header queue;
	size_t offset = (size_t)IPC_ADDR +
		sizeof(struct hfi_queue_tbl_header) +
		target_que * sizeof(struct hfi_queue_header);
	int32_t *ptr = (int32_t *)&queue;
	size_t content_off;
	uint32_t *content, content_size;
	int i;

	MEMR(npu_dev, (void *)((size_t)offset), (uint8_t *)&queue,
		HFI_QUEUE_HEADER_SIZE);

	NPU_ERR("DUMP IPC queue %d:\n", target_que);
	NPU_ERR("Header size %d:\n", HFI_QUEUE_HEADER_SIZE);
	NPU_ERR("============QUEUE HEADER=============\n");
	for (i = 0; i < HFI_QUEUE_HEADER_SIZE/4; i++)
		NPU_ERR("%x\n", ptr[i]);

	content_off = (size_t)(IPC_ADDR + queue.qhdr_start_offset +
		queue.qhdr_read_idx);
	if (queue.qhdr_write_idx >= queue.qhdr_read_idx)
		content_size = queue.qhdr_write_idx - queue.qhdr_read_idx;
	else
		content_size = queue.qhdr_q_size - queue.qhdr_read_idx +
			queue.qhdr_write_idx;

	NPU_ERR("Content size %d:\n", content_size);
	if (content_size == 0)
		return;

	content = kzalloc(content_size, GFP_KERNEL);
	if (!content) {
		NPU_ERR("failed to allocate IPC queue content buffer\n");
		return;
	}

	if (queue.qhdr_write_idx >= queue.qhdr_read_idx) {
		MEMR(npu_dev, (void *)content_off, content, content_size);
	} else {
		MEMR(npu_dev, (void *)content_off, content,
			queue.qhdr_q_size - queue.qhdr_read_idx);

		MEMR(npu_dev, (void *)((size_t)IPC_ADDR +
			queue.qhdr_start_offset),
			(void *)((size_t)content + queue.qhdr_q_size -
			queue.qhdr_read_idx), queue.qhdr_write_idx);
	}

	NPU_ERR("============QUEUE CONTENT=============\n");
	for (i = 0; i < content_size/4; i++)
		NPU_ERR("%x\n", content[i]);

	NPU_ERR("DUMP IPC queue %d END\n", target_que);
	kfree(content);
}

static void npu_dump_dbg_registers(struct npu_device *npu_dev)
{
	uint32_t reg_val, reg_addr;
	int i;

	NPU_ERR("============Debug Registers=============\n");
	reg_addr = NPU_GPR0;
	for (i = 0; i < 16; i++) {
		reg_val = REGR(npu_dev, reg_addr);
		NPU_ERR("npu dbg register %d : 0x%x\n", i, reg_val);
		reg_addr += 4;
	}
}

static void npu_dump_all_ipc_queue(struct npu_device *npu_dev)
{
	int i;

	for (i = 0; i < NPU_HFI_NUMBER_OF_QS; i++)
		npu_dump_ipc_queue(npu_dev, i);
}

void npu_dump_debug_info(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	if (host_ctx->fw_state != FW_ENABLED) {
		NPU_WARN("NPU is disabled\n");
		return;
	}

	npu_dump_dbg_registers(npu_dev);
	npu_dump_all_ipc_queue(npu_dev);
}
