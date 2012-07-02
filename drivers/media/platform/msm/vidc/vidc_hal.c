/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <asm/memory.h>
#include "vidc_hal.h"
#include "vidc_hal_io.h"

#define FIRMWARE_SIZE			0X00A00000
#define REG_ADDR_OFFSET_BITMASK	0x000FFFFF

/*Workaround for virtio */
#define HFI_VIRTIO_FW_BIAS		0x14f00000

struct hal_device_data hal_ctxt;

static void hal_virtio_modify_cmd_packet(u8 *packet)
{
	struct hfi_cmd_sys_session_init_packet *sys_init;
	struct hal_session *sess;
	u8 i;

	if (!packet) {
		HAL_MSG_ERROR("Invalid Param: %s", __func__);
		return;
	}

	sys_init = (struct hfi_cmd_sys_session_init_packet *)packet;
	sess = (struct hal_session *) sys_init->session_id;
	switch (sys_init->packet_type) {
	case HFI_CMD_SESSION_EMPTY_BUFFER:
		if (sess->is_decoder) {
			struct hfi_cmd_session_empty_buffer_compressed_packet
			*pkt = (struct
			hfi_cmd_session_empty_buffer_compressed_packet
			*) packet;
			pkt->packet_buffer -= HFI_VIRTIO_FW_BIAS;
		} else {
			struct
			hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			*pkt = (struct
			hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			*) packet;
			pkt->packet_buffer -= HFI_VIRTIO_FW_BIAS;
		}
		break;
	case HFI_CMD_SESSION_FILL_BUFFER:
	{
		struct hfi_cmd_session_fill_buffer_packet *pkt =
			(struct hfi_cmd_session_fill_buffer_packet *)packet;
		pkt->packet_buffer -= HFI_VIRTIO_FW_BIAS;
		break;
	}
	case HFI_CMD_SESSION_SET_BUFFERS:
	{
		struct hfi_cmd_session_set_buffers_packet *pkt =
			(struct hfi_cmd_session_set_buffers_packet *)packet;
		if ((pkt->buffer_type == HFI_BUFFER_OUTPUT) ||
			(pkt->buffer_type == HFI_BUFFER_OUTPUT2)) {
			struct hfi_buffer_info *buff;
			buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
			buff->buffer_addr -= HFI_VIRTIO_FW_BIAS;
			buff->extra_data_addr -= HFI_VIRTIO_FW_BIAS;
		} else {
			for (i = 0; i < pkt->num_buffers; i++)
				pkt->rg_buffer_info[i] -= HFI_VIRTIO_FW_BIAS;
		}
		break;
	}
	case HFI_CMD_SESSION_RELEASE_BUFFERS:
	{
		struct hfi_cmd_session_release_buffer_packet *pkt =
			(struct hfi_cmd_session_release_buffer_packet *)packet;
		if ((pkt->buffer_type == HFI_BUFFER_OUTPUT) ||
			(pkt->buffer_type == HFI_BUFFER_OUTPUT2)) {
			struct hfi_buffer_info *buff;
			buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
			buff->buffer_addr -= HFI_VIRTIO_FW_BIAS;
			buff->extra_data_addr -= HFI_VIRTIO_FW_BIAS;
		} else {
			for (i = 0; i < pkt->num_buffers; i++)
				pkt->rg_buffer_info[i] -= HFI_VIRTIO_FW_BIAS;
		}
		break;
	}
	case HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER:
	{
		struct hfi_cmd_session_parse_sequence_header_packet *pkt =
			(struct hfi_cmd_session_parse_sequence_header_packet *)
		packet;
		pkt->packet_buffer -= HFI_VIRTIO_FW_BIAS;
		break;
	}
	case HFI_CMD_SESSION_GET_SEQUENCE_HEADER:
	{
		struct hfi_cmd_session_get_sequence_header_packet *pkt =
			(struct hfi_cmd_session_get_sequence_header_packet *)
		packet;
		pkt->packet_buffer -= HFI_VIRTIO_FW_BIAS;
		break;
	}
	default:
		break;
	}
}

static int write_queue(void *info, u8 *packet, u32 *rx_req_is_set)
{
	struct hfi_queue_header *queue;
	u32 packet_size_in_words, new_write_idx;
	struct vidc_iface_q_info *qinfo;
	u32 empty_space, read_idx;
	u32 *write_ptr;

	if (!info || !packet || !rx_req_is_set) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	}

	qinfo =	(struct vidc_iface_q_info *) info;
	HAL_MSG_LOW("In %s: ", __func__);
	hal_virtio_modify_cmd_packet(packet);

	queue = (struct hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		HAL_MSG_ERROR("queue not present");
		return -ENOENT;
	}

	packet_size_in_words = (*(u32 *)packet) >> 2;
	HAL_MSG_LOW("Packet_size in words: %d", packet_size_in_words);

	if (packet_size_in_words == 0) {
		HAL_MSG_ERROR("Zero packet size");
		return -ENODATA;
	}

	read_idx = queue->qhdr_read_idx;

	empty_space = (queue->qhdr_write_idx >=  read_idx) ?
		(queue->qhdr_q_size - (queue->qhdr_write_idx -  read_idx)) :
		(read_idx - queue->qhdr_write_idx);
	HAL_MSG_LOW("Empty_space: %d", empty_space);
	if (empty_space <= packet_size_in_words) {
		queue->qhdr_tx_req =  1;
		HAL_MSG_ERROR("Insufficient size (%d) to write (%d)",
					  empty_space, packet_size_in_words);
		return -ENOTEMPTY;
	}

	queue->qhdr_tx_req =  0;

	new_write_idx = (queue->qhdr_write_idx + packet_size_in_words);
	write_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
		(queue->qhdr_write_idx << 2));
	HAL_MSG_LOW("Write Ptr: %d", (u32) write_ptr);
	if (new_write_idx < queue->qhdr_q_size) {
		memcpy(write_ptr, packet, packet_size_in_words << 2);
	} else {
		new_write_idx -= queue->qhdr_q_size;
		memcpy(write_ptr, packet, (packet_size_in_words -
			new_write_idx) << 2);
		memcpy((void *)queue->qhdr_start_addr,
			packet + ((packet_size_in_words - new_write_idx) << 2),
			new_write_idx  << 2);
	}
	queue->qhdr_write_idx = new_write_idx;
	*rx_req_is_set = (1 == queue->qhdr_rx_req) ? 1 : 0;
	HAL_MSG_LOW("Out %s: ", __func__);
	return 0;
}

static void hal_virtio_modify_msg_packet(u8 *packet)
{
	struct hfi_msg_sys_session_init_done_packet *sys_idle;
	struct hal_session *sess;

	if (!packet) {
		HAL_MSG_ERROR("Invalid Param: %s", __func__);
		return;
	}

	sys_idle = (struct hfi_msg_sys_session_init_done_packet *)packet;
	sess = (struct hal_session *) sys_idle->session_id;

	switch (sys_idle->packet_type) {
	case HFI_MSG_SESSION_FILL_BUFFER_DONE:
		if (sess->is_decoder) {
			struct
			hfi_msg_session_fbd_uncompressed_plane0_packet
			*pkt_uc = (struct
			hfi_msg_session_fbd_uncompressed_plane0_packet
			*) packet;
			pkt_uc->packet_buffer += HFI_VIRTIO_FW_BIAS;
		} else {
			struct
			hfi_msg_session_fill_buffer_done_compressed_packet
			*pkt = (struct
			hfi_msg_session_fill_buffer_done_compressed_packet
			*) packet;
			pkt->packet_buffer += HFI_VIRTIO_FW_BIAS;
		}
		break;
	case HFI_MSG_SESSION_EMPTY_BUFFER_DONE:
	{
		struct hfi_msg_session_empty_buffer_done_packet *pkt =
		(struct hfi_msg_session_empty_buffer_done_packet *)packet;
		pkt->packet_buffer += HFI_VIRTIO_FW_BIAS;
		break;
	}
	case HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE:
	{
		struct
		hfi_msg_session_get_sequence_header_done_packet
		*pkt =
		(struct hfi_msg_session_get_sequence_header_done_packet *)
		packet;
		pkt->sequence_header += HFI_VIRTIO_FW_BIAS;
		break;
	}
	default:
		break;
	}
}

static int read_queue(void *info, u8 *packet, u32 *pb_tx_req_is_set)
{
	struct hfi_queue_header *queue;
	u32 packet_size_in_words, new_read_idx;
	u32 *read_ptr;
	struct vidc_iface_q_info *qinfo;

	if (!info || !packet || !pb_tx_req_is_set) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	}

	qinfo =	(struct vidc_iface_q_info *) info;
	HAL_MSG_LOW("In %s: ", __func__);
	queue = (struct hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		HAL_MSG_ERROR("Queue memory is not allocated\n");
		return -ENOMEM;
	}

	if (queue->qhdr_read_idx == queue->qhdr_write_idx) {
		queue->qhdr_rx_req = 1;
		*pb_tx_req_is_set = 0;
		return -EPERM;
	}

	read_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
				(queue->qhdr_read_idx << 2));
	packet_size_in_words = (*read_ptr) >> 2;
	HAL_MSG_LOW("packet_size_in_words: %d", packet_size_in_words);
	if (packet_size_in_words == 0) {
		HAL_MSG_ERROR("Zero packet size");
		return -ENODATA;
	}

	new_read_idx = queue->qhdr_read_idx + packet_size_in_words;
	HAL_MSG_LOW("Read Ptr: %d", (u32) new_read_idx);
	if (new_read_idx < queue->qhdr_q_size) {
		memcpy(packet, read_ptr,
			packet_size_in_words << 2);
	} else {
		new_read_idx -= queue->qhdr_q_size;
		memcpy(packet, read_ptr,
			(packet_size_in_words - new_read_idx) << 2);
		memcpy(packet + ((packet_size_in_words -
				new_read_idx) << 2),
			(u8 *)queue->qhdr_start_addr, new_read_idx << 2);
	}

	queue->qhdr_read_idx = new_read_idx;

	if (queue->qhdr_read_idx != queue->qhdr_write_idx)
		queue->qhdr_rx_req = 0;
	else
		queue->qhdr_rx_req = 1;

	*pb_tx_req_is_set = (1 == queue->qhdr_tx_req) ? 1 : 0;
	hal_virtio_modify_msg_packet(packet);
	HAL_MSG_LOW("Out %s: ", __func__);
	return 0;
}

static int vidc_hal_alloc(void *mem, void *clnt, u32 size, u32 align, u32 flags)
{
	struct vidc_mem_addr *vmem;
	struct msm_smem *alloc;

	if (!mem || !clnt || !size) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	}
	vmem = (struct vidc_mem_addr *)mem;
	HAL_MSG_HIGH("start to alloc: size:%d, Flags: %d", size, flags);

	alloc  = msm_smem_alloc(clnt, size, align, flags);
	HAL_MSG_LOW("Alloc done");
	if (!alloc) {
		HAL_MSG_HIGH("Alloc fail in %s", __func__);
		return -ENOMEM;
	} else {
		HAL_MSG_MEDIUM("vidc_hal_alloc:ptr=%p,size=%d",
					   alloc->kvaddr, size);
		vmem->mem_size = alloc->size;
		vmem->mem_data = alloc;
		vmem->align_virtual_addr = (u8 *) alloc->kvaddr;
		vmem->align_device_addr = (u8 *)alloc->device_addr;
	}
	return 0;
}

static void vidc_hal_free(struct smem_client *clnt, struct msm_smem *mem)
{
	msm_smem_free(clnt, mem);
}

static void write_register(u8 *base_addr, u32 reg, u32 value, u8 *vaddr)
{
	u32 hwiosymaddr = reg;

	reg &= REG_ADDR_OFFSET_BITMASK;
	if (reg == (u32)VIDC_CPU_CS_SCIACMDARG2) {
		/* workaround to offset of FW bias */
		struct hfi_queue_header *qhdr;
		struct hfi_queue_table_header *qtbl_hdr =
			(struct hfi_queue_table_header *)vaddr;

		qhdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(qtbl_hdr, 0);
		qhdr->qhdr_start_addr -= HFI_VIRTIO_FW_BIAS;

		qhdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(qtbl_hdr, 1);
		qhdr->qhdr_start_addr -= HFI_VIRTIO_FW_BIAS;

		qhdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(qtbl_hdr, 2);
		qhdr->qhdr_start_addr -= HFI_VIRTIO_FW_BIAS;
		value -= HFI_VIRTIO_FW_BIAS;
	}

	hwiosymaddr = ((u32)base_addr + (hwiosymaddr));
	HAL_MSG_LOW("Base addr: 0x%x, written to: 0x%x, Value: 0x%x...",
			(u32)base_addr, hwiosymaddr, value);
	writel_relaxed(value, hwiosymaddr);
	wmb();
}

static int read_register(u8 *base_addr, u32 reg)
{
	int rc = readl_relaxed((u32)base_addr + reg);
	rmb();
	return rc;
}

static int vidc_hal_iface_cmdq_write(struct hal_device *device, void *pkt)
{
	u32 rx_req_is_set = 0;
	struct vidc_iface_q_info *q_info;
	int result = -EPERM;

	if (!device || !pkt) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	}

	spin_lock(&device->write_lock);
	q_info = &device->iface_queues[VIDC_IFACEQ_CMDQ_IDX];
	if (!q_info) {
		HAL_MSG_ERROR("cannot write to shared Q's");
		goto err_q_write;
	}

	if (!write_queue(q_info, (u8 *)pkt, &rx_req_is_set)) {
		if (rx_req_is_set)
			write_register(device->hal_data->register_base_addr,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		result = 0;
	} else {
		HAL_MSG_ERROR("vidc_hal_iface_cmdq_write:queue_full");
	}
err_q_write:
	spin_unlock(&device->write_lock);
	return result;
}

int vidc_hal_iface_msgq_read(struct hal_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct vidc_iface_q_info *q_info;

	if (!pkt) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	}
	spin_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_MSGQ_IDX].
		q_array.align_virtual_addr == 0) {
		HAL_MSG_ERROR("cannot read from shared MSG Q's");
		rc = -ENODATA;
		goto read_error;
	}
	q_info = &device->iface_queues[VIDC_IFACEQ_MSGQ_IDX];

	if (!read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		if (tx_req_is_set)
			write_register(device->hal_data->register_base_addr,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		rc = 0;
	} else {
		HAL_MSG_ERROR("vidc_hal_iface_msgq_read:queue_empty");
		rc = -ENODATA;
	}
read_error:
	spin_unlock(&device->read_lock);
	return rc;
}

int vidc_hal_iface_dbgq_read(struct hal_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct vidc_iface_q_info *q_info;

	if (!pkt) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	}
	spin_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_DBGQ_IDX].
		q_array.align_virtual_addr == 0) {
		HAL_MSG_ERROR("cannot read from shared DBG Q's");
		rc = -ENODATA;
		goto dbg_error;
	}
	q_info = &device->iface_queues[VIDC_IFACEQ_DBGQ_IDX];
	if (!read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		if (tx_req_is_set)
			write_register(device->hal_data->register_base_addr,
			VIDC_CPU_IC_SOFTINT,
			1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		rc = 0;
	} else {
		HAL_MSG_MEDIUM("vidc_hal_iface_dbgq_read:queue_empty");
		rc = -ENODATA;
	}
dbg_error:
	spin_unlock(&device->read_lock);
	return rc;
}

static void vidc_hal_set_queue_hdr_defaults(struct hfi_queue_header *q_hdr)
{
	q_hdr->qhdr_status = 0x1;
	q_hdr->qhdr_type = VIDC_IFACEQ_DFLT_QHDR;
	q_hdr->qhdr_q_size = VIDC_IFACEQ_QUEUE_SIZE;
	q_hdr->qhdr_pkt_size = 0;
	q_hdr->qhdr_rx_wm = 0x1;
	q_hdr->qhdr_tx_wm = 0x1;
	q_hdr->qhdr_rx_req = 0x1;
	q_hdr->qhdr_tx_req = 0x0;
	q_hdr->qhdr_rx_irq_status = 0x0;
	q_hdr->qhdr_tx_irq_status = 0x0;
	q_hdr->qhdr_read_idx = 0x0;
	q_hdr->qhdr_write_idx = 0x0;
}

static void vidc_hal_interface_queues_release(struct hal_device *device)
{
	int i;
	for (i = 0; i < VIDC_IFACEQ_NUMQ; i++) {
		vidc_hal_free(device->hal_client,
			device->iface_queues[i].q_array.mem_data);
		device->iface_queues[i].q_hdr = NULL;
		device->iface_queues[i].q_array.mem_data = NULL;
		device->iface_queues[i].q_array.align_virtual_addr = NULL;
		device->iface_queues[i].q_array.align_device_addr = NULL;
	}
	vidc_hal_free(device->hal_client,
				device->iface_q_table.mem_data);
	device->iface_q_table.align_virtual_addr = NULL;
	device->iface_q_table.align_device_addr = NULL;
	msm_smem_delete_client(device->hal_client);
	device->hal_client = NULL;
}

static int vidc_hal_interface_queues_init(struct hal_device *dev)
{
	struct hfi_queue_table_header *q_tbl_hdr;
	struct hfi_queue_header *q_hdr;
	u8 i;
	int rc = 0;
	struct vidc_iface_q_info *iface_q;

	rc = vidc_hal_alloc((void *) &dev->iface_q_table,
					dev->hal_client,
			VIDC_IFACEQ_TABLE_SIZE, 1, 0);
	if (rc) {
		HAL_MSG_ERROR("%s:iface_q_table_alloc_fail", __func__);
		return -ENOMEM;
	}
	q_tbl_hdr = (struct hfi_queue_table_header *)
			dev->iface_q_table.align_virtual_addr;
	q_tbl_hdr->qtbl_version = 0;
	q_tbl_hdr->qtbl_size = VIDC_IFACEQ_TABLE_SIZE;
	q_tbl_hdr->qtbl_qhdr0_offset = sizeof(
		struct hfi_queue_table_header);
	q_tbl_hdr->qtbl_qhdr_size = sizeof(
		struct hfi_queue_header);
	q_tbl_hdr->qtbl_num_q = VIDC_IFACEQ_NUMQ;
	q_tbl_hdr->qtbl_num_active_q = VIDC_IFACEQ_NUMQ;

	for (i = 0; i < VIDC_IFACEQ_NUMQ; i++) {
		iface_q = &dev->iface_queues[i];
		rc = vidc_hal_alloc((void *) &iface_q->q_array,
				dev->hal_client, VIDC_IFACEQ_QUEUE_SIZE,
				1, 0);
		if (rc) {
			HAL_MSG_ERROR("%s:iface_q_table_alloc[%d]_fail",
						__func__, i);
			vidc_hal_interface_queues_release(dev);
			return -ENOMEM;
		} else {
			iface_q->q_hdr =
				VIDC_IFACEQ_GET_QHDR_START_ADDR(
			dev->iface_q_table.align_virtual_addr, i);
			vidc_hal_set_queue_hdr_defaults(iface_q->q_hdr);
		}
	}

	iface_q = &dev->iface_queues[VIDC_IFACEQ_CMDQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = (u32)
		iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_HOST_TO_CTRL_CMD_Q;

	iface_q = &dev->iface_queues[VIDC_IFACEQ_MSGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = (u32)
		iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_MSG_Q;

	iface_q = &dev->iface_queues[VIDC_IFACEQ_DBGQ_IDX];
	q_hdr = iface_q->q_hdr;
	q_hdr->qhdr_start_addr = (u32)
		iface_q->q_array.align_device_addr;
	q_hdr->qhdr_type |= HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q;
	write_register(dev->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG2,
		(u32) dev->iface_q_table.align_device_addr,
		dev->iface_q_table.align_virtual_addr);
	write_register(dev->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG1, 0x01,
		dev->iface_q_table.align_virtual_addr);
	return 0;
}

static int vidc_hal_core_start_cpu(struct hal_device *device)
{
	u32 ctrl_status = 0, count = 0, rc = 0;
	write_register(device->hal_data->register_base_addr,
			VIDC_WRAPPER_INTR_MASK, 0, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_CPU_CS_SCIACMDARG3, 1, 0);
	while (!ctrl_status && count < 25) {
		ctrl_status = read_register(
		device->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG0);
		count++;
	}
	if (count >= 25)
		rc = -ETIME;
	return rc;
}

int vidc_hal_core_init(void *device)
{
	struct hfi_cmd_sys_init_packet pkt;
	int rc = 0;
	struct hal_device *dev;

	if (device) {
		dev = device;
	} else {
		HAL_MSG_ERROR("%s:invalid device", __func__);
		return -ENODEV;
	}
	enable_irq(dev->hal_data->irq);
	INIT_LIST_HEAD(&dev->sess_head);
	spin_lock_init(&dev->read_lock);
	spin_lock_init(&dev->write_lock);

	if (!dev->hal_client) {
		dev->hal_client = msm_smem_new_client(SMEM_ION);
		if (dev->hal_client == NULL) {
			HAL_MSG_ERROR("Failed to alloc ION_Client");
			rc = -ENODEV;
			goto err_no_mem;
		}

		HAL_MSG_ERROR("Device_Virt_Address : 0x%x,"
		"Register_Virt_Addr: 0x%x",
		(u32) dev->hal_data->device_base_addr,
		(u32) dev->hal_data->register_base_addr);

		rc = vidc_hal_interface_queues_init(dev);
		if (rc) {
			HAL_MSG_ERROR("failed to init queues");
			rc = -ENOMEM;
			goto err_no_mem;
		}
	} else {
		HAL_MSG_ERROR("hal_client exists");
		rc = -EEXIST;
		goto err_no_mem;
	}
	rc = vidc_hal_core_start_cpu(dev);
	if (rc) {
		HAL_MSG_ERROR("Failed to start core");
		rc = -ENODEV;
		goto err_no_dev;
	}
	pkt.size = sizeof(struct hfi_cmd_sys_init_packet);
	pkt.packet_type = HFI_CMD_SYS_INIT;
	pkt.arch_type = HFI_ARCH_OX_OFFSET;
	if (vidc_hal_iface_cmdq_write(dev, &pkt)) {
		rc = -ENOTEMPTY;
		goto err_write_fail;
	}
	return rc;
err_no_dev:
err_write_fail:
err_no_mem:
	disable_irq_nosync(dev->hal_data->irq);
	return rc;
}

int vidc_hal_core_release(void *device)
{
	struct hal_device *dev;
	if (device) {
		dev = device;
	} else {
		HAL_MSG_ERROR("%s:invalid device", __func__);
		return -ENODEV;
	}
	write_register(dev->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG3, 0, 0);
	HAL_MSG_INFO("\nHAL exited\n");
	return 0;
}

int vidc_hal_core_pc_prep(void *device)
{
	struct hfi_cmd_sys_pc_prep_packet pkt;
	int rc = 0;
	struct hal_device *dev;

	if (device) {
		dev = device;
	} else {
		HAL_MSG_ERROR("%s:invalid device", __func__);
		return -ENODEV;
	}
	pkt.size = sizeof(struct hfi_cmd_sys_pc_prep_packet);
	pkt.packet_type = HFI_CMD_SYS_PC_PREP;
	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;
	return rc;
}

static void vidc_hal_core_clear_interrupt(struct hal_device *device)
{
	u32 intr_status = 0;

	if (!device->callback)
		return;

	intr_status = read_register(
		device->hal_data->register_base_addr,
		VIDC_WRAPPER_INTR_STATUS);

	if ((intr_status & VIDC_WRAPPER_INTR_STATUS_A2H_BMSK) ||
		(intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK)) {
		device->intr_status |= intr_status;
		HAL_MSG_ERROR("INTERRUPT for device: 0x%x: "
			"times: %d interrupt_status: %d",
			(u32) device, ++device->reg_count, intr_status);
	} else {
		HAL_MSG_ERROR("SPURIOUS_INTR for device: 0x%x: "
			"times: %d interrupt_status: %d",
			(u32) device, ++device->spur_count, intr_status);
	}
	write_register(device->hal_data->register_base_addr,
			VIDC_CPU_CS_A2HSOFTINTCLR, 1, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_WRAPPER_INTR_CLEAR, intr_status, 0);
	HAL_MSG_ERROR("Cleared WRAPPER/A2H interrupt");
}

int vidc_hal_core_set_resource(void *device,
		struct vidc_resource_hdr *resource_hdr, void *resource_value)
{
	struct hfi_cmd_sys_set_resource_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct hal_device *dev;

	if (!device || !resource_hdr || !resource_value) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		dev = device;
	}

	pkt = (struct hfi_cmd_sys_set_resource_packet *) packet;

	pkt->size = sizeof(struct hfi_cmd_sys_set_resource_packet);
	pkt->packet_type = HFI_CMD_SYS_SET_RESOURCE;
	pkt->resource_handle = resource_hdr->resource_handle;

	switch (resource_hdr->resource_id) {
	case VIDC_RESOURCE_OCMEM:
	{
		struct hfi_resource_ocmem *hfioc_mem =
			(struct hfi_resource_ocmem *)
			&pkt->rg_resource_data[0];
		struct vidc_mem_addr *vidc_oc_mem =
			(struct vidc_mem_addr *) resource_value;

		pkt->resource_type = HFI_RESOURCE_OCMEM;
		hfioc_mem->size = (u32) vidc_oc_mem->mem_size;
		hfioc_mem->mem = (u8 *) vidc_oc_mem->align_device_addr;
		pkt->size += sizeof(struct hfi_resource_ocmem);
		if (vidc_hal_iface_cmdq_write(dev, pkt))
			rc = -ENOTEMPTY;
		break;
	}
	default:
		HAL_MSG_INFO("In %s called for resource %d",
					 __func__, resource_hdr->resource_id);
		break;
	}
	return rc;
}

int vidc_hal_core_release_resource(void *device,
			struct vidc_resource_hdr *resource_hdr)
{
	struct hfi_cmd_sys_release_resource_packet pkt;
	int rc = 0;
	struct hal_device *dev;

	if (!device || !resource_hdr) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		dev = device;
	}

	pkt.size = sizeof(struct hfi_cmd_sys_release_resource_packet);
	pkt.packet_type = HFI_CMD_SYS_RELEASE_RESOURCE;
	pkt.resource_type = resource_hdr->resource_id;
	pkt.resource_handle = resource_hdr->resource_handle;

	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_core_ping(void *device)
{
	struct hfi_cmd_sys_ping_packet pkt;
	int rc = 0;
	struct hal_device *dev;

	if (device) {
		dev = device;
	} else {
		HAL_MSG_ERROR("%s:invalid device", __func__);
		return -ENODEV;
	}
	pkt.size = sizeof(struct hfi_cmd_sys_ping_packet);
	pkt.packet_type = HFI_CMD_SYS_PING;

	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;
	return rc;
}
static u32 get_hfi_buffer(int hal_buffer)
{
	u32 buffer;
	switch (hal_buffer) {
	case HAL_BUFFER_INPUT:
		buffer = HFI_BUFFER_INPUT;
		break;
	case HAL_BUFFER_OUTPUT:
		buffer = HFI_BUFFER_OUTPUT;
		break;
	case HAL_BUFFER_OUTPUT2:
		buffer = HFI_BUFFER_OUTPUT;
		break;
	case HAL_BUFFER_EXTRADATA_INPUT:
		buffer = HFI_BUFFER_EXTRADATA_INPUT;
		break;
	case HAL_BUFFER_EXTRADATA_OUTPUT:
		buffer = HFI_BUFFER_EXTRADATA_OUTPUT;
		break;
	case HAL_BUFFER_EXTRADATA_OUTPUT2:
		buffer = HFI_BUFFER_EXTRADATA_OUTPUT2;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH:
		buffer = HFI_BUFFER_INTERNAL_SCRATCH;
		break;
	case HAL_BUFFER_INTERNAL_PERSIST:
		buffer = HFI_BUFFER_INTERNAL_PERSIST;
		break;
	default:
		HAL_MSG_ERROR("Invalid buffer type : 0x%x\n", hal_buffer);
		buffer = 0;
		break;
	}
	return buffer;
}
int vidc_hal_session_set_property(void *sess,
	enum hal_property ptype, void *pdata)
{
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	struct hfi_cmd_session_set_property_packet *pkt =
		(struct hfi_cmd_session_set_property_packet *) &packet;
	struct hal_session *session;

	if (!sess || !pdata) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}

	HAL_MSG_INFO("IN func: %s, with property id: %d", __func__, ptype);
	pkt->size = sizeof(struct hfi_cmd_session_set_property_packet);
	pkt->packet_type = HFI_CMD_SESSION_SET_PROPERTY;
	pkt->session_id = (u32) session;
	pkt->num_properties = 1;

	switch (ptype) {
	case HAL_CONFIG_FRAME_RATE:
	{
		struct hfi_frame_rate *hfi;
		u32 buffer;
		struct hal_frame_rate *prop =
			(struct hal_frame_rate *) pdata;
		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_FRAME_RATE;
		hfi = (struct hfi_frame_rate *) &pkt->rg_property_data[1];
		buffer = get_hfi_buffer(prop->buffer_type);
		if (buffer)
			hfi->buffer_type = buffer;
		else
			return -EINVAL;
		hfi->frame_rate = prop->frame_rate;
		pkt->size += sizeof(u32) + sizeof(struct hfi_frame_rate);
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT:
	{
		u32 buffer;
		struct hfi_uncompressed_format_select *hfi;
		struct hal_uncompressed_format_select *prop =
			(struct hal_uncompressed_format_select *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SELECT;
		hfi = (struct hfi_uncompressed_format_select *)
			&pkt->rg_property_data[1];
		buffer = get_hfi_buffer(prop->buffer_type);
		if (buffer)
			hfi->buffer_type = buffer;
		else
			return -EINVAL;
		hfi->format = prop->format;
		pkt->size += sizeof(u32) + sizeof(struct
			hfi_uncompressed_format_select);
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO:
		break;
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO:
		break;
	case HAL_PARAM_EXTRA_DATA_HEADER_CONFIG:
		break;
	case HAL_PARAM_FRAME_SIZE:
	{
		u32 buffer;
		struct hfi_frame_size *hfi;
		struct hal_frame_size *prop = (struct hal_frame_size *) pdata;
		pkt->rg_property_data[0] = HFI_PROPERTY_PARAM_FRAME_SIZE;
		hfi = (struct hfi_frame_size *) &pkt->rg_property_data[1];
		buffer = get_hfi_buffer(prop->buffer_type);
		if (buffer)
			hfi->buffer_type = buffer;
		else
			return -EINVAL;
		hfi->height = prop->height;
		hfi->width = prop->width;
		pkt->size += sizeof(u32) + sizeof(struct hfi_frame_size);
		break;
	}
	case HAL_CONFIG_REALTIME:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_REALTIME;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_BUFFER_COUNT_ACTUAL:
	{
		u32 buffer;
		struct hfi_buffer_count_actual *hfi;
		struct hal_buffer_count_actual *prop =
			(struct hal_buffer_count_actual *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_BUFFER_COUNT_ACTUAL;
		hfi = (struct hfi_buffer_count_actual *)
				&pkt->rg_property_data[1];
		hfi->buffer_count_actual = prop->buffer_count_actual;
		buffer = get_hfi_buffer(prop->buffer_type);
		if (buffer)
			hfi->buffer_type = buffer;
		else
			return -EINVAL;
		pkt->size += sizeof(u32) + sizeof(struct
					hfi_buffer_count_actual);
		break;
	}
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT:
	{
		struct hal_nal_stream_format_supported *prop =
			(struct hal_nal_stream_format_supported *)pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT;
		HAL_MSG_ERROR("\ndata is :%d",
				prop->nal_stream_format_supported);
		switch (prop->nal_stream_format_supported) {
		case HAL_NAL_FORMAT_STARTCODES:
			pkt->rg_property_data[1] =
				HFI_NAL_FORMAT_STARTCODES;
			break;
		case HAL_NAL_FORMAT_ONE_NAL_PER_BUFFER:
			pkt->rg_property_data[1] =
				HFI_NAL_FORMAT_ONE_NAL_PER_BUFFER;
			break;
		case HAL_NAL_FORMAT_ONE_BYTE_LENGTH:
			pkt->rg_property_data[1] =
				HFI_NAL_FORMAT_ONE_BYTE_LENGTH;
			break;
		case HAL_NAL_FORMAT_TWO_BYTE_LENGTH:
			pkt->rg_property_data[1] =
				HFI_NAL_FORMAT_TWO_BYTE_LENGTH;
			break;
		case HAL_NAL_FORMAT_FOUR_BYTE_LENGTH:
			pkt->rg_property_data[1] =
				HFI_NAL_FORMAT_FOUR_BYTE_LENGTH;
			break;
		default:
			HAL_MSG_ERROR("Invalid nal format: 0x%x",
				  prop->nal_stream_format_supported);
			break;
		}
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_OUTPUT_ORDER:
	{
		int *data = (int *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_OUTPUT_ORDER;
		switch (*data) {
		case HAL_OUTPUT_ORDER_DECODE:
			pkt->rg_property_data[1] = HFI_OUTPUT_ORDER_DECODE;
			break;
		case HAL_OUTPUT_ORDER_DISPLAY:
			pkt->rg_property_data[1] = HFI_OUTPUT_ORDER_DISPLAY;
			break;
		default:
			HAL_MSG_ERROR("invalid output order: 0x%x",
						  *data);
			break;
		}
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_PICTURE_TYPE_DECODE:
	{
		struct hfi_enable_picture *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_PICTURE_TYPE_DECODE;
		hfi = (struct hfi_enable_picture *) &pkt->rg_property_data[1];
		hfi->picture_type = (u32) pdata;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_MULTI_STREAM:
	{
		u32 buffer;
		struct hfi_multi_stream *hfi;
		struct hal_multi_stream *prop =
			(struct hal_multi_stream *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM;
		hfi = (struct hfi_multi_stream *) &pkt->rg_property_data[1];
		buffer = get_hfi_buffer(prop->buffer_type);
		if (buffer)
			hfi->buffer_type = buffer;
		else
			return -EINVAL;
		hfi->enable = prop->enable;
		hfi->width = prop->width;
		hfi->height = prop->height;
		pkt->size += sizeof(u32) + sizeof(struct hfi_multi_stream);
		break;
	}
	case HAL_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT:
	{
		struct hfi_display_picture_buffer_count *hfi;
		struct hal_display_picture_buffer_count *prop =
			(struct hal_display_picture_buffer_count *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT;
		hfi = (struct hfi_display_picture_buffer_count *)
			&pkt->rg_property_data[1];
		hfi->count = prop->count;
		hfi->enable = prop->enable;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_display_picture_buffer_count);
		break;
	}
	case HAL_PARAM_DIVX_FORMAT:
	{
		int *data = pdata;
		pkt->rg_property_data[0] = HFI_PROPERTY_PARAM_DIVX_FORMAT;
		switch (*data) {
		case HAL_DIVX_FORMAT_4:
			pkt->rg_property_data[1] = HFI_DIVX_FORMAT_4;
			break;
		case HAL_DIVX_FORMAT_5:
			pkt->rg_property_data[1] = HFI_DIVX_FORMAT_5;
			break;
		case HAL_DIVX_FORMAT_6:
			pkt->rg_property_data[1] = HFI_DIVX_FORMAT_6;
			break;
		default:
			HAL_MSG_ERROR("Invalid divx format: 0x%x", *data);
			break;
		}
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP_REPORTING;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_CONTINUE_DATA_TRANSFER;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VENC_REQUEST_IFRAME:
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_REQUEST_SYNC_FRAME;
		break;
	case HAL_PARAM_VENC_MPEG4_SHORT_HEADER:
		break;
	case HAL_PARAM_VENC_MPEG4_AC_PREDICTION:
		break;
	case HAL_CONFIG_VENC_TARGET_BITRATE:
	{
		struct hfi_bitrate *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE;
		hfi = (struct hfi_bitrate *) &pkt->rg_property_data[1];
		hfi->bit_rate = ((struct hal_bitrate *)pdata)->bit_rate;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
	{
		struct hfi_profile_level *hfi;
		struct hal_profile_level *prop =
			(struct hal_profile_level *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT;
		hfi = (struct hfi_profile_level *)
			&pkt->rg_property_data[1];
		hfi->level = (u32) prop->level;
		hfi->profile = prop->profile;
		if (!hfi->profile)
			hfi->profile = HFI_H264_PROFILE_HIGH;
		if (!hfi->level)
			hfi->level = 1;
		pkt->size += sizeof(u32) + sizeof(struct hfi_profile_level);
		break;
	}
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL:
	{
		struct hfi_h264_entropy_control *hfi;
		struct hal_h264_entropy_control *prop =
			(struct hal_h264_entropy_control *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_ENTROPY_CONTROL;
		hfi = (struct hfi_h264_entropy_control *)
			&pkt->rg_property_data[1];
		switch (prop->entropy_mode) {
		case HAL_H264_ENTROPY_CAVLC:
			hfi->cabac_model = HFI_H264_ENTROPY_CAVLC;
			break;
		case HAL_H264_ENTROPY_CABAC:
			hfi->cabac_model = HFI_H264_ENTROPY_CABAC;
			switch (prop->cabac_model) {
			case HAL_H264_CABAC_MODEL_0:
				hfi->cabac_model = HFI_H264_CABAC_MODEL_0;
				break;
			case HAL_H264_CABAC_MODEL_1:
				hfi->cabac_model = HFI_H264_CABAC_MODEL_1;
				break;
			case HAL_H264_CABAC_MODEL_2:
				hfi->cabac_model = HFI_H264_CABAC_MODEL_2;
				break;
			default:
				HAL_MSG_ERROR("Invalid cabac model 0x%x",
							  prop->entropy_mode);
				break;
			}
		break;
		default:
			HAL_MSG_ERROR("Invalid entropy selected: 0x%x",
				prop->cabac_model);
			break;
		}
		pkt->size += sizeof(u32) + sizeof(
			struct hfi_h264_entropy_control);
		break;
	}
	case HAL_PARAM_VENC_RATE_CONTROL:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_RATE_CONTROL;
		switch ((enum hal_rate_control)pdata) {
		case HAL_RATE_CONTROL_OFF:
		pkt->rg_property_data[1] = HFI_RATE_CONTROL_OFF;
			break;
		case HAL_RATE_CONTROL_CBR_CFR:
		pkt->rg_property_data[1] = HFI_RATE_CONTROL_CBR_CFR;
			break;
		case HAL_RATE_CONTROL_CBR_VFR:
		pkt->rg_property_data[1] = HFI_RATE_CONTROL_CBR_VFR;
			break;
		case HAL_RATE_CONTROL_VBR_CFR:
		pkt->rg_property_data[1] = HFI_RATE_CONTROL_VBR_CFR;
			break;
		case HAL_RATE_CONTROL_VBR_VFR:
		pkt->rg_property_data[1] = HFI_RATE_CONTROL_VBR_VFR;
			break;
		default:
			HAL_MSG_ERROR("Invalid Rate control setting: 0x%x",
						  (int) pdata);
			break;
		}
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_MPEG4_TIME_RESOLUTION:
	{
		struct hfi_mpeg4_time_resolution *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MPEG4_TIME_RESOLUTION;
		hfi = (struct hfi_mpeg4_time_resolution *)
			&pkt->rg_property_data[1];
		hfi->time_increment_resolution =
			((struct hal_mpeg4_time_resolution *)pdata)->
					time_increment_resolution;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_MPEG4_HEADER_EXTENSION:
	{
		struct hfi_mpeg4_header_extension *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MPEG4_HEADER_EXTENSION;
		hfi = (struct hfi_mpeg4_header_extension *)
			&pkt->rg_property_data[1];
		hfi->header_extension = (u32) pdata;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_H264_DEBLOCK_CONTROL:
	{
		struct hfi_h264_db_control *hfi;
		struct hal_h264_db_control *prop =
			(struct hal_h264_db_control *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_DEBLOCK_CONTROL;
		hfi = (struct hfi_h264_db_control *) &pkt->rg_property_data[1];
		switch (prop->mode) {
		case HAL_H264_DB_MODE_DISABLE:
			hfi->mode = HFI_H264_DB_MODE_DISABLE;
			break;
		case HAL_H264_DB_MODE_SKIP_SLICE_BOUNDARY:
			hfi->mode = HFI_H264_DB_MODE_SKIP_SLICE_BOUNDARY;
			break;
		case HAL_H264_DB_MODE_ALL_BOUNDARY:
			hfi->mode = HFI_H264_DB_MODE_ALL_BOUNDARY;
			break;
		default:
			HAL_MSG_ERROR("Invalid deblocking mode: 0x%x",
						  prop->mode);
			break;
		}
		hfi->slice_alpha_offset = prop->slice_alpha_offset;
		hfi->slice_beta_offset = prop->slice_beta_offset;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_h264_db_control);
		break;
	}
	case HAL_PARAM_VENC_TEMPORAL_SPATIAL_TRADEOFF:
	{
		struct hfi_temporal_spatial_tradeoff *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_TEMPORAL_SPATIAL_TRADEOFF;
		hfi = (struct hfi_temporal_spatial_tradeoff *)
			&pkt->rg_property_data[1];
		hfi->ts_factor = ((struct hfi_temporal_spatial_tradeoff *)
					pdata)->ts_factor;
		pkt->size += sizeof(u32)  * 2;
		break;
	}
	case HAL_PARAM_VENC_SESSION_QP:
	{
		struct hfi_quantization *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_SESSION_QP;
		hfi = (struct hfi_quantization *) &pkt->rg_property_data[1];
		memcpy(hfi, (struct hfi_quantization *) pdata,
				sizeof(struct hfi_quantization));
		pkt->size += sizeof(u32) + sizeof(struct hfi_quantization);
		break;
	}
	case HAL_CONFIG_VENC_INTRA_PERIOD:
	{
		struct hfi_intra_period *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD;
		hfi = (struct hfi_intra_period *) &pkt->rg_property_data[1];
		memcpy(hfi, (struct hfi_intra_period *) pdata,
				sizeof(struct hfi_intra_period));
		pkt->size += sizeof(u32) + sizeof(struct hfi_intra_period);
		break;
	}
	case HAL_CONFIG_VENC_IDR_PERIOD:
	{
		struct hfi_idr_period *hfi;
		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD;
		hfi = (struct hfi_idr_period *) &pkt->rg_property_data[1];
		hfi->idr_period = ((struct hfi_idr_period *) pdata)->idr_period;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VPE_OPERATIONS:
		break;
	case HAL_PARAM_VENC_INTRA_REFRESH:
	{
		struct hfi_intra_refresh *hfi;
		struct hal_intra_refresh *prop =
			(struct hal_intra_refresh *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH;
		hfi = (struct hfi_intra_refresh *) &pkt->rg_property_data[1];
		switch (prop->mode) {
		case HAL_INTRA_REFRESH_NONE:
			hfi->mode = HFI_INTRA_REFRESH_NONE;
			break;
		case HAL_INTRA_REFRESH_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_CYCLIC:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC;
			break;
		case HAL_INTRA_REFRESH_CYCLIC_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_RANDOM:
			hfi->mode = HFI_INTRA_REFRESH_RANDOM;
			break;
		default:
			HAL_MSG_ERROR("Invalid intra refresh setting: 0x%x",
				prop->mode);
			break;
		}
		hfi->air_mbs = prop->air_mbs;
		hfi->air_ref = prop->air_ref;
		hfi->cir_mbs = prop->cir_mbs;
		pkt->size += sizeof(u32) + sizeof(struct hfi_intra_refresh);
		break;
	}
	case HAL_PARAM_VENC_MULTI_SLICE_CONTROL:
	{
		struct hfi_multi_slice_control *hfi;
		struct hal_multi_slice_control *prop =
			(struct hal_multi_slice_control *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_CONTROL;
		hfi = (struct hfi_multi_slice_control *)
			&pkt->rg_property_data[1];
		switch (prop->multi_slice) {
		case HAL_MULTI_SLICE_OFF:
			hfi->multi_slice = HFI_MULTI_SLICE_OFF;
			break;
		case HAL_MULTI_SLICE_GOB:
			hfi->multi_slice = HFI_MULTI_SLICE_GOB;
			break;
		case HAL_MULTI_SLICE_BY_MB_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_MB_COUNT;
			break;
		case HAL_MULTI_SLICE_BY_BYTE_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_BYTE_COUNT;
			break;
		default:
			HAL_MSG_ERROR("Invalid slice settings: 0x%x",
				prop->multi_slice);
			break;
		}
		pkt->size += sizeof(u32) + sizeof(struct
					hfi_multi_slice_control);
		break;
	}
	case HAL_CONFIG_VPE_DEINTERLACE:
		break;
	case HAL_SYS_DEBUG_CONFIG:
	{
		struct hfi_debug_config *hfi;
		pkt->rg_property_data[0] = HFI_PROPERTY_SYS_DEBUG_CONFIG;
		hfi = (struct hfi_debug_config *) &pkt->rg_property_data[1];
		hfi->debug_config = ((struct hal_debug_config *)
					pdata)->debug_config;
		pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
			sizeof(struct hfi_debug_config);
		break;
	}
	/* FOLLOWING PROPERTIES ARE NOT IMPLEMENTED IN CORE YET */
	case HAL_CONFIG_BUFFER_REQUIREMENTS:
	case HAL_CONFIG_PRIORITY:
	case HAL_CONFIG_BATCH_INFO:
	case HAL_PARAM_METADATA_PASS_THROUGH:
	case HAL_SYS_IDLE_INDICATOR:
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED:
	case HAL_PARAM_INTERLACE_FORMAT_SUPPORTED:
	case HAL_PARAM_CHROMA_SITE:
	case HAL_PARAM_PROPERTIES_SUPPORTED:
	case HAL_PARAM_PROFILE_LEVEL_SUPPORTED:
	case HAL_PARAM_CAPABILITY_SUPPORTED:
	case HAL_PARAM_NAL_STREAM_FORMAT_SUPPORTED:
	case HAL_PARAM_MULTI_VIEW_FORMAT:
	case HAL_PARAM_MAX_SEQUENCE_HEADER_SIZE:
	case HAL_PARAM_CODEC_SUPPORTED:
	case HAL_PARAM_VDEC_MULTI_VIEW_SELECT:
	case HAL_PARAM_VDEC_MB_QUANTIZATION:
	case HAL_PARAM_VDEC_NUM_CONCEALED_MB:
	case HAL_PARAM_VDEC_H264_ENTROPY_SWITCHING:
	case HAL_PARAM_VENC_SLICE_DELIVERY_MODE:
	case HAL_PARAM_VENC_MPEG4_DATA_PARTITIONING:

	case HAL_CONFIG_BUFFER_COUNT_ACTUAL:
	case HAL_CONFIG_VDEC_MULTI_STREAM:
	case HAL_PARAM_VENC_MULTI_SLICE_INFO:
	case HAL_CONFIG_VENC_TIMESTAMP_SCALE:
	case HAL_PARAM_VENC_LOW_LATENCY:
	default:
		HAL_MSG_INFO("DEFAULT: Calling 0x%x", ptype);
		break;
	}
	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		return -ENOTEMPTY;
	return 0;
}

int vidc_hal_session_get_property(void *sess,
	enum hal_property ptype, void *pdata)
{
	struct hal_session *session;

	if (!sess || !pdata) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}
	HAL_MSG_INFO("IN func: %s, with property id: %d", __func__, ptype);

	switch (ptype) {
	case HAL_CONFIG_FRAME_RATE:
		break;
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT:
		break;
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO:
		break;
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO:
		break;
	case HAL_PARAM_EXTRA_DATA_HEADER_CONFIG:
		break;
	case HAL_PARAM_FRAME_SIZE:
		break;
	case HAL_CONFIG_REALTIME:
		break;
	case HAL_PARAM_BUFFER_COUNT_ACTUAL:
		break;
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT:
		break;
	case HAL_PARAM_VDEC_OUTPUT_ORDER:
		break;
	case HAL_PARAM_VDEC_PICTURE_TYPE_DECODE:
		break;
	case HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO:
		break;
	case HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER:
		break;
	case HAL_PARAM_VDEC_MULTI_STREAM:
		break;
	case HAL_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT:
		break;
	case HAL_PARAM_DIVX_FORMAT:
		break;
	case HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING:
		break;
	case HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER:
		break;
	case HAL_CONFIG_VDEC_MB_ERROR_MAP:
		break;
	case HAL_CONFIG_VENC_REQUEST_IFRAME:
		break;
	case HAL_PARAM_VENC_MPEG4_SHORT_HEADER:
		break;
	case HAL_PARAM_VENC_MPEG4_AC_PREDICTION:
		break;
	case HAL_CONFIG_VENC_TARGET_BITRATE:
		break;
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
		break;
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL:
		break;
	case HAL_PARAM_VENC_RATE_CONTROL:
		break;
	case HAL_PARAM_VENC_MPEG4_TIME_RESOLUTION:
		break;
	case HAL_PARAM_VENC_MPEG4_HEADER_EXTENSION:
		break;
	case HAL_PARAM_VENC_H264_DEBLOCK_CONTROL:
		break;
	case HAL_PARAM_VENC_TEMPORAL_SPATIAL_TRADEOFF:
		break;
	case HAL_PARAM_VENC_SESSION_QP:
		break;
	case HAL_CONFIG_VENC_INTRA_PERIOD:
		break;
	case HAL_CONFIG_VENC_IDR_PERIOD:
		break;
	case HAL_CONFIG_VPE_OPERATIONS:
		break;
	case HAL_PARAM_VENC_INTRA_REFRESH:
		break;
	case HAL_PARAM_VENC_MULTI_SLICE_CONTROL:
		break;
	case HAL_CONFIG_VPE_DEINTERLACE:
		break;
	case HAL_SYS_DEBUG_CONFIG:
		break;
	/*FOLLOWING PROPERTIES ARE NOT IMPLEMENTED IN CORE YET*/
	case HAL_CONFIG_BUFFER_REQUIREMENTS:
	case HAL_CONFIG_PRIORITY:
	case HAL_CONFIG_BATCH_INFO:
	case HAL_PARAM_METADATA_PASS_THROUGH:
	case HAL_SYS_IDLE_INDICATOR:
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED:
	case HAL_PARAM_INTERLACE_FORMAT_SUPPORTED:
	case HAL_PARAM_CHROMA_SITE:
	case HAL_PARAM_PROPERTIES_SUPPORTED:
	case HAL_PARAM_PROFILE_LEVEL_SUPPORTED:
	case HAL_PARAM_CAPABILITY_SUPPORTED:
	case HAL_PARAM_NAL_STREAM_FORMAT_SUPPORTED:
	case HAL_PARAM_MULTI_VIEW_FORMAT:
	case HAL_PARAM_MAX_SEQUENCE_HEADER_SIZE:
	case HAL_PARAM_CODEC_SUPPORTED:
	case HAL_PARAM_VDEC_MULTI_VIEW_SELECT:
	case HAL_PARAM_VDEC_MB_QUANTIZATION:
	case HAL_PARAM_VDEC_NUM_CONCEALED_MB:
	case HAL_PARAM_VDEC_H264_ENTROPY_SWITCHING:
	case HAL_PARAM_VENC_SLICE_DELIVERY_MODE:
	case HAL_PARAM_VENC_MPEG4_DATA_PARTITIONING:

	case HAL_CONFIG_BUFFER_COUNT_ACTUAL:
	case HAL_CONFIG_VDEC_MULTI_STREAM:
	case HAL_PARAM_VENC_MULTI_SLICE_INFO:
	case HAL_CONFIG_VENC_TIMESTAMP_SCALE:
	case HAL_PARAM_VENC_LOW_LATENCY:
	default:
		HAL_MSG_INFO("DEFAULT: Calling 0x%x", ptype);
		break;
	}
	return 0;
}

void *vidc_hal_session_init(void *device, u32 session_id,
	enum hal_domain session_type, enum hal_video_codec codec_type)
{
	struct hfi_cmd_sys_session_init_packet pkt;
	struct hal_session *new_session;
	struct hal_device *dev;

	if (device) {
		dev = device;
	} else {
		HAL_MSG_ERROR("%s:invalid device", __func__);
		return NULL;
	}

	new_session = (struct hal_session *)
		kzalloc(sizeof(struct hal_session), GFP_KERNEL);
	new_session->session_id = (u32) session_id;
	if (session_type == 1)
		new_session->is_decoder = 0;
	else if (session_type == 2)
		new_session->is_decoder = 1;
	new_session->device = dev;
	list_add_tail(&new_session->list, &dev->sess_head);
	pkt.size = sizeof(struct hfi_cmd_sys_session_init_packet);
	pkt.packet_type = HFI_CMD_SYS_SESSION_INIT;
	pkt.session_id = (u32) new_session;
	pkt.session_domain = session_type;
	pkt.session_codec = codec_type;
	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		return NULL;
	return (void *) new_session;
}

static int vidc_hal_send_session_cmd(void *session_id,
	 int pkt_type)
{
	struct vidc_hal_session_cmd_pkt pkt;
	int rc = 0;
	struct hal_session *session;

	if (session_id) {
		session = session_id;
	} else {
		HAL_MSG_ERROR("%s:invalid session", __func__);
		return -ENODEV;
	}

	pkt.size = sizeof(struct vidc_hal_session_cmd_pkt);
	pkt.packet_type = pkt_type;
	pkt.session_id = (u32) session;

	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_session_end(void *session)
{
	return vidc_hal_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_END);
}

int vidc_hal_session_abort(void *session)
{
	return vidc_hal_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_ABORT);
}

int vidc_hal_session_set_buffers(void *sess,
	struct vidc_buffer_addr_info *buffer_info)
{
	u32 buffer;
	struct hfi_cmd_session_set_buffers_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	u16 i;
	struct hal_session *session;

	if (!sess || !buffer_info) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_cmd_session_set_buffers_packet *)packet;

	pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) +
		((buffer_info->num_buffers - 1) * sizeof(u32));
	pkt->packet_type = HFI_CMD_SESSION_SET_BUFFERS;
	pkt->session_id = (u32) session;
	pkt->buffer_mode = HFI_BUFFER_MODE_STATIC;
	pkt->buffer_size = buffer_info->buffer_size;
	pkt->min_buffer_size = buffer_info->buffer_size;
	pkt->num_buffers = buffer_info->num_buffers;

	if ((buffer_info->buffer_type == HAL_BUFFER_OUTPUT) ||
		(buffer_info->buffer_type == HAL_BUFFER_OUTPUT2)) {
		struct hfi_buffer_info *buff;
		pkt->extra_data_size = buffer_info->extradata_size;
		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) -
			sizeof(u32) + ((buffer_info->num_buffers) *
			sizeof(struct hfi_buffer_info));
		buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
		for (i = 0; i < pkt->num_buffers; i++) {
			buff->buffer_addr =
				buffer_info->align_device_addr;
			buff->extra_data_addr =
				buffer_info->extradata_addr;
		}
	} else {
		pkt->extra_data_size = 0;
		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) +
			((buffer_info->num_buffers - 1) * sizeof(u32));
		for (i = 0; i < pkt->num_buffers; i++)
			pkt->rg_buffer_info[i] =
			buffer_info->align_device_addr;
	}
	buffer = get_hfi_buffer(buffer_info->buffer_type);
	if (buffer)
		pkt->buffer_type = buffer;
	else
		return -EINVAL;
	HAL_MSG_INFO("set buffers: 0x%x", buffer_info->buffer_type);
	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_session_release_buffers(void *sess,
	struct vidc_buffer_addr_info *buffer_info)
{
	u32 buffer;
	struct hfi_cmd_session_release_buffer_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	u32 i;
	struct hal_session *session;

	if (!sess || !buffer_info) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_cmd_session_release_buffer_packet *) packet;
	pkt->size = sizeof(struct hfi_cmd_session_release_buffer_packet) +
		((buffer_info->num_buffers - 1) * sizeof(u32));
	pkt->packet_type = HFI_CMD_SESSION_RELEASE_BUFFERS;
	pkt->session_id = (u32) session;
	pkt->buffer_size = buffer_info->buffer_size;
	pkt->num_buffers = buffer_info->num_buffers;

	if ((buffer_info->buffer_type == HAL_BUFFER_OUTPUT) ||
		(buffer_info->buffer_type == HAL_BUFFER_OUTPUT2)) {
		struct hfi_buffer_info *buff;
		buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
		for (i = 0; i < pkt->num_buffers; i++) {
			buff->buffer_addr =
				buffer_info->align_device_addr;
			buff->extra_data_addr =
				buffer_info->extradata_addr;
		}
		pkt->extra_data_size = buffer_info->extradata_size;
		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) -
			sizeof(u32) + ((buffer_info->num_buffers) *
			sizeof(struct hfi_buffer_info));
	} else {
		for (i = 0; i < pkt->num_buffers; i++)
			pkt->rg_buffer_info[i] =
			buffer_info->align_device_addr;
		pkt->extra_data_size = 0;
		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) +
			((buffer_info->num_buffers - 1) * sizeof(u32));
	}
	buffer = get_hfi_buffer(buffer_info->buffer_type);
	if (buffer)
		pkt->buffer_type = buffer;
	else
		return -EINVAL;
	HAL_MSG_INFO("Release buffers: 0x%x", buffer_info->buffer_type);
	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_session_load_res(void *sess)
{
	return vidc_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_LOAD_RESOURCES);
}

int vidc_hal_session_release_res(void *sess)
{
	return vidc_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_RELEASE_RESOURCES);
}

int vidc_hal_session_start(void *sess)
{
	return vidc_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_START);
}

int vidc_hal_session_stop(void *sess)
{
	return vidc_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_STOP);
}

int vidc_hal_session_suspend(void *sess)
{
	return vidc_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_SUSPEND);
}

int vidc_hal_session_resume(void *sess)
{
	return vidc_hal_send_session_cmd(sess,
		HFI_CMD_SESSION_RESUME);
}

int vidc_hal_session_etb(void *sess, struct vidc_frame_data *input_frame)
{
	int rc = 0;
	struct hal_session *session;

	if (!sess || !input_frame) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}

	if (session->is_decoder) {
		struct hfi_cmd_session_empty_buffer_compressed_packet pkt;
		pkt.size = sizeof(
			struct hfi_cmd_session_empty_buffer_compressed_packet);
		pkt.packet_type = HFI_CMD_SESSION_EMPTY_BUFFER;
		pkt.session_id = (u32) session;
		pkt.time_stamp_hi = (int) (((u64)input_frame->timestamp) >> 32);
		pkt.time_stamp_lo = (int) input_frame->timestamp;
		pkt.flags = input_frame->flags;
		pkt.mark_target = input_frame->mark_target;
		pkt.mark_data = input_frame->mark_data;
		pkt.offset = input_frame->offset;
		pkt.alloc_len = input_frame->alloc_len;
		pkt.filled_len = input_frame->filled_len;
		pkt.input_tag = input_frame->clnt_data;
		pkt.packet_buffer = (u8 *) input_frame->device_addr;
		HAL_MSG_ERROR("### Q DECODER INPUT BUFFER ###");
		if (vidc_hal_iface_cmdq_write(session->device, &pkt))
			rc = -ENOTEMPTY;
	} else {
		struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			pkt;
		pkt.size = sizeof(struct
		hfi_cmd_session_empty_buffer_uncompressed_plane0_packet);
		pkt.packet_type = HFI_CMD_SESSION_EMPTY_BUFFER;
		pkt.session_id = (u32) session;
		pkt.view_id = 0;
		pkt.time_stamp_hi = (u32) (((u64)input_frame->timestamp) >> 32);
		pkt.time_stamp_lo = (u32) input_frame->timestamp;
		pkt.flags = input_frame->flags;
		pkt.mark_target = input_frame->mark_target;
		pkt.mark_data = input_frame->mark_data;
		pkt.offset = input_frame->offset;
		pkt.alloc_len = input_frame->alloc_len;
		pkt.filled_len = input_frame->filled_len;
		pkt.input_tag = input_frame->clnt_data;
		pkt.packet_buffer = (u8 *) input_frame->device_addr;
		HAL_MSG_ERROR("### Q ENCODER INPUT BUFFER ###");
		if (vidc_hal_iface_cmdq_write(session->device, &pkt))
			rc = -ENOTEMPTY;
	}
	return rc;
}

int vidc_hal_session_ftb(void *sess,
	struct vidc_frame_data *output_frame)
{
	struct hfi_cmd_session_fill_buffer_packet pkt;
	int rc = 0;
	struct hal_session *session;

	if (!sess || !output_frame) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}

	pkt.size = sizeof(struct hfi_cmd_session_fill_buffer_packet);
	pkt.packet_type = HFI_CMD_SESSION_FILL_BUFFER;
	pkt.session_id = (u32) session;
	if (output_frame->buffer_type == HAL_BUFFER_OUTPUT)
		pkt.stream_id = 0;
	else if (output_frame->buffer_type == HAL_BUFFER_OUTPUT2)
		pkt.stream_id = 1;
	pkt.packet_buffer = (u8 *) output_frame->device_addr;
	pkt.extra_data_buffer =
		(u8 *) output_frame->extradata_addr;

	HAL_MSG_INFO("### Q OUTPUT BUFFER ###");
	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_session_parse_seq_hdr(void *sess,
	struct vidc_seq_hdr *seq_hdr)
{
	struct hfi_cmd_session_parse_sequence_header_packet *pkt;
	int rc = 0;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hal_session *session;

	if (!sess || !seq_hdr) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}

	pkt = (struct hfi_cmd_session_parse_sequence_header_packet *) packet;
	pkt->size = sizeof(struct hfi_cmd_session_parse_sequence_header_packet);
	pkt->packet_type = HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER;
	pkt->session_id = (u32) session;
	pkt->header_len = seq_hdr->seq_hdr_len;
	pkt->packet_buffer = seq_hdr->seq_hdr;

	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_session_get_seq_hdr(void *sess,
	struct vidc_seq_hdr *seq_hdr)
{
	struct hfi_cmd_session_get_sequence_header_packet *pkt;
	int rc = 0;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hal_session *session;

	if (!sess || !seq_hdr) {
		HAL_MSG_ERROR("Invalid Params in %s", __func__);
		return -EINVAL;
	} else {
		session = sess;
	}

	pkt = (struct hfi_cmd_session_get_sequence_header_packet *) packet;
	pkt->size = sizeof(struct hfi_cmd_session_get_sequence_header_packet);
	pkt->packet_type = HFI_CMD_SESSION_GET_SEQUENCE_HEADER;
	pkt->session_id = (u32) session;
	pkt->buffer_len = seq_hdr->seq_hdr_len;
	pkt->packet_buffer = seq_hdr->seq_hdr;

	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_session_get_buf_req(void *sess)
{
	struct hfi_cmd_session_get_property_packet pkt;
	int rc = 0;
	struct hal_session *session;

	if (sess) {
		session = sess;
	} else {
		HAL_MSG_ERROR("%s:invalid session", __func__);
		return -ENODEV;
	}

	pkt.size = sizeof(struct hfi_cmd_session_get_property_packet);
	pkt.packet_type = HFI_CMD_SESSION_GET_PROPERTY;
	pkt.session_id = (u32) session;
	pkt.num_properties = 1;
	pkt.rg_property_data[0] = HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS;
	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
	return rc;
}

int vidc_hal_session_flush(void *sess, enum hal_flush flush_mode)
{
	struct hfi_cmd_session_flush_packet pkt;
	int rc = 0;
	struct hal_session *session;

	if (sess) {
		session = sess;
	} else {
		HAL_MSG_ERROR("%s:invalid session", __func__);
		return -ENODEV;
	}

	pkt.size = sizeof(struct hfi_cmd_session_flush_packet);
	pkt.packet_type = HFI_CMD_SESSION_FLUSH;
	pkt.session_id = (u32) session;
	switch (flush_mode) {
	case HAL_FLUSH_INPUT:
		pkt.flush_type = HFI_FLUSH_INPUT;
		break;
	case HAL_FLUSH_OUTPUT:
		pkt.flush_type = HFI_FLUSH_OUTPUT;
		break;
	case HAL_FLUSH_OUTPUT2:
		pkt.flush_type = HFI_FLUSH_OUTPUT2;
		break;
	case HAL_FLUSH_ALL:
		pkt.flush_type = HFI_FLUSH_ALL;
		break;
	default:
		HAL_MSG_ERROR("Invalid flush mode: 0x%x\n", flush_mode);
		break;
	}
	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
	return rc;
}

static int vidc_hal_check_core_registered(
	struct hal_device_data core, u32 fw_addr,
	u32 reg_addr, u32 reg_size, u32 irq)
{
	struct hal_device *device;
	struct list_head *curr, *next;

	if (core.dev_count) {
		list_for_each_safe(curr, next, &core.dev_head) {
			device = list_entry(curr, struct hal_device, list);
			if (device && device->hal_data->irq == irq &&
				(CONTAINS((u32)device->hal_data->
						device_base_addr,
						FIRMWARE_SIZE, fw_addr) ||
				CONTAINS(fw_addr, FIRMWARE_SIZE,
						(u32)device->hal_data->
						device_base_addr) ||
				CONTAINS((u32)device->hal_data->
						register_base_addr,
						reg_size, reg_addr) ||
				CONTAINS(reg_addr, reg_size,
						(u32)device->hal_data->
						register_base_addr) ||
				OVERLAPS((u32)device->hal_data->
						register_base_addr,
						reg_size, reg_addr, reg_size) ||
				OVERLAPS(reg_addr, reg_size,
						(u32)device->hal_data->
						register_base_addr, reg_size) ||
				OVERLAPS((u32)device->hal_data->
						device_base_addr,
						FIRMWARE_SIZE, fw_addr,
						FIRMWARE_SIZE) ||
				OVERLAPS(fw_addr, FIRMWARE_SIZE,
						(u32)device->hal_data->
						device_base_addr,
						FIRMWARE_SIZE))) {
				return 0;
			} else {
				HAL_MSG_INFO("Device not registered");
				return -EINVAL;
			}
		}
	} else {
		HAL_MSG_INFO("no device Registered");
	}
	return -EINVAL;
}

static void vidc_hal_core_work_handler(struct work_struct *work)
{
	struct hal_device *device = list_first_entry(
		&hal_ctxt.dev_head, struct hal_device, list);

	HAL_MSG_INFO(" GOT INTERRUPT %s() ", __func__);
	if (!device->callback) {
		HAL_MSG_ERROR("No callback function	"
					  "to process interrupt: %p\n", device);
		return;
	}
	vidc_hal_core_clear_interrupt(device);
	vidc_hal_response_handler(device);
	enable_irq(device->hal_data->irq);
}
static DECLARE_WORK(vidc_hal_work, vidc_hal_core_work_handler);

static irqreturn_t vidc_hal_isr(int irq, void *dev)
{
	struct hal_device *device = dev;
	HAL_MSG_MEDIUM("\n vidc_hal_isr() %d ", irq);
	disable_irq_nosync(irq);
	queue_work(device->vidc_workq, &vidc_hal_work);
	HAL_MSG_MEDIUM("\n vidc_hal_isr() %d ", irq);
	return IRQ_HANDLED;
}

void *vidc_hal_add_device(u32 device_id, u32 fw_base_addr, u32 reg_base,
		u32 reg_size, u32 irq,
		void (*callback) (enum command_response cmd, void *data))
{
	struct hal_device *hdevice = NULL;
	struct hal_data *hal = NULL;
	int rc = 0;

	if (device_id || !fw_base_addr || !reg_base || !reg_size ||
			!irq || !callback) {
		HAL_MSG_ERROR("Invalid Paramters");
		return NULL;
	} else {
		HAL_MSG_INFO("entered %s, device_id: %d", __func__, device_id);
	}

	if (vidc_hal_check_core_registered(hal_ctxt, fw_base_addr,
						reg_base, reg_size, irq)) {
		HAL_MSG_LOW("HAL_DATA will be assigned now");
		hal = (struct hal_data *)
			kzalloc(sizeof(struct hal_data), GFP_KERNEL);
		if (!hal) {
			HAL_MSG_ERROR("Failed to alloc");
			return NULL;
		}
		hal->irq = irq;
		hal->device_base_addr =
			ioremap_nocache(fw_base_addr, FIRMWARE_SIZE);
		if (!hal->device_base_addr) {
			HAL_MSG_ERROR("could not map fw addr %d of size %d",
						  fw_base_addr, FIRMWARE_SIZE);
			goto err_map;
		}
		hal->register_base_addr =
			ioremap_nocache(reg_base, reg_size);
		if (!hal->register_base_addr) {
			HAL_MSG_ERROR("could not map reg addr %d of size %d",
						  reg_base, reg_size);
			goto err_map;
		}
		INIT_LIST_HEAD(&hal_ctxt.dev_head);
	} else {
		HAL_MSG_ERROR("Core present/Already added");
		return NULL;
	}

	hdevice = (struct hal_device *)
			kzalloc(sizeof(struct hal_device), GFP_KERNEL);
	if (!hdevice) {
		HAL_MSG_ERROR("failed to allocate new device");
		goto err_map;
	}

	INIT_LIST_HEAD(&hdevice->list);
	list_add_tail(&hdevice->list, &hal_ctxt.dev_head);
	hal_ctxt.dev_count++;
	hdevice->device_id = device_id;
	hdevice->hal_data = hal;
	hdevice->callback = callback;

	hdevice->vidc_workq = create_singlethread_workqueue(
		"msm_vidc_workerq");
	if (!hdevice->vidc_workq) {
		HAL_MSG_ERROR("%s: create workq failed\n", __func__);
		goto error_createq;
	}

	rc = request_irq(irq, vidc_hal_isr, IRQF_TRIGGER_HIGH,
			"msm_vidc", hdevice);
	if (unlikely(rc)) {
		HAL_MSG_ERROR("%s() :request_irq failed\n", __func__);
		goto error_irq_fail;
	}
	disable_irq_nosync(irq);
	return (void *) hdevice;
error_irq_fail:
	destroy_workqueue(hdevice->vidc_workq);
error_createq:
	hal_ctxt.dev_count--;
	list_del(&hal_ctxt.dev_head);
err_map:
	kfree(hal);
	return NULL;
}

void vidc_hal_delete_device(void *device)
{
	struct hal_device *close, *dev;

	if (device) {
		dev = (struct hal_device *) device;
		list_for_each_entry(close, &hal_ctxt.dev_head, list) {
			if (close->hal_data->irq == dev->hal_data->irq) {
				hal_ctxt.dev_count--;
				free_irq(dev->hal_data->irq, NULL);
				list_del(&close->list);
				destroy_workqueue(close->vidc_workq);
				kfree(close->hal_data);
				kfree(close);
				break;
			}
		}

	}
}
