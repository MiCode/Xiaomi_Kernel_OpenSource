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
#include <linux/delay.h>
#include <mach/ocmem.h>

#include <asm/memory.h>
#include "hfi_packetization.h"
#include "venus_hfi.h"
#include "vidc_hfi_io.h"
#include "msm_vidc_debug.h"

#define FIRMWARE_SIZE			0X00A00000
#define REG_ADDR_OFFSET_BITMASK	0x000FFFFF

/*Workaround for virtio */
#define HFI_VIRTIO_FW_BIAS		0x0

struct hal_device_data hal_ctxt;

static void hal_virtio_modify_cmd_packet(u8 *packet)
{
	struct hfi_cmd_sys_session_init_packet *sys_init;
	struct hal_session *sess;
	u8 i;

	if (!packet) {
		dprintk(VIDC_ERR, "Invalid Param");
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	qinfo =	(struct vidc_iface_q_info *) info;
	hal_virtio_modify_cmd_packet(packet);

	queue = (struct hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		dprintk(VIDC_ERR, "queue not present");
		return -ENOENT;
	}

	packet_size_in_words = (*(u32 *)packet) >> 2;
	dprintk(VIDC_DBG, "Packet_size in words: %d", packet_size_in_words);

	if (packet_size_in_words == 0) {
		dprintk(VIDC_ERR, "Zero packet size");
		return -ENODATA;
	}

	read_idx = queue->qhdr_read_idx;

	empty_space = (queue->qhdr_write_idx >=  read_idx) ?
		(queue->qhdr_q_size - (queue->qhdr_write_idx -  read_idx)) :
		(read_idx - queue->qhdr_write_idx);
	dprintk(VIDC_DBG, "Empty_space: %d", empty_space);
	if (empty_space <= packet_size_in_words) {
		queue->qhdr_tx_req =  1;
		dprintk(VIDC_ERR, "Insufficient size (%d) to write (%d)",
					  empty_space, packet_size_in_words);
		return -ENOTEMPTY;
	}

	queue->qhdr_tx_req =  0;

	new_write_idx = (queue->qhdr_write_idx + packet_size_in_words);
	write_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
		(queue->qhdr_write_idx << 2));
	dprintk(VIDC_DBG, "Write Ptr: %d", (u32) write_ptr);
	if (new_write_idx < queue->qhdr_q_size) {
		memcpy(write_ptr, packet, packet_size_in_words << 2);
	} else {
		new_write_idx -= queue->qhdr_q_size;
		memcpy(write_ptr, packet, (packet_size_in_words -
			new_write_idx) << 2);
		memcpy((void *)qinfo->q_array.align_virtual_addr,
			packet + ((packet_size_in_words - new_write_idx) << 2),
			new_write_idx  << 2);
	}
	queue->qhdr_write_idx = new_write_idx;
	*rx_req_is_set = (1 == queue->qhdr_rx_req) ? 1 : 0;
	dprintk(VIDC_DBG, "Out : ");
	return 0;
}

static void hal_virtio_modify_msg_packet(u8 *packet)
{
	struct hfi_msg_sys_session_init_done_packet *sys_idle;
	struct hal_session *sess;

	if (!packet) {
		dprintk(VIDC_ERR, "Invalid Param: ");
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	qinfo =	(struct vidc_iface_q_info *) info;
	queue = (struct hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		dprintk(VIDC_ERR, "Queue memory is not allocated\n");
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
	dprintk(VIDC_DBG, "packet_size_in_words: %d", packet_size_in_words);
	if (packet_size_in_words == 0) {
		dprintk(VIDC_ERR, "Zero packet size");
		return -ENODATA;
	}

	new_read_idx = queue->qhdr_read_idx + packet_size_in_words;
	dprintk(VIDC_DBG, "Read Ptr: %d", (u32) new_read_idx);
	if (new_read_idx < queue->qhdr_q_size) {
		memcpy(packet, read_ptr,
			packet_size_in_words << 2);
	} else {
		new_read_idx -= queue->qhdr_q_size;
		memcpy(packet, read_ptr,
			(packet_size_in_words - new_read_idx) << 2);
		memcpy(packet + ((packet_size_in_words -
			new_read_idx) << 2),
			(u8 *)qinfo->q_array.align_virtual_addr,
			new_read_idx << 2);
	}

	queue->qhdr_read_idx = new_read_idx;

	if (queue->qhdr_read_idx != queue->qhdr_write_idx)
		queue->qhdr_rx_req = 0;
	else
		queue->qhdr_rx_req = 1;

	*pb_tx_req_is_set = (1 == queue->qhdr_tx_req) ? 1 : 0;
	hal_virtio_modify_msg_packet(packet);
	dprintk(VIDC_DBG, "Out : ");
	return 0;
}

static int vidc_hal_alloc(void *mem, void *clnt, u32 size, u32 align, u32 flags,
		int domain)
{
	struct vidc_mem_addr *vmem;
	struct msm_smem *alloc;
	int rc = 0;

	if (!mem || !clnt || !size) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	vmem = (struct vidc_mem_addr *)mem;
	dprintk(VIDC_INFO, "start to alloc: size:%d, Flags: %d", size, flags);

	alloc  = msm_smem_alloc(clnt, size, align, flags, domain, 1, 1);
	dprintk(VIDC_DBG, "Alloc done");
	if (!alloc) {
		dprintk(VIDC_ERR, "Alloc failed\n");
		rc = -ENOMEM;
		goto fail_smem_alloc;
	}
	rc = msm_smem_clean_invalidate(clnt, alloc);
	if (rc) {
		dprintk(VIDC_ERR, "NOTE: Failed to clean caches\n");
		goto fail_clean_cache;
	}
	dprintk(VIDC_DBG, "vidc_hal_alloc:ptr=%p,size=%d",
			alloc->kvaddr, size);
	vmem->mem_size = alloc->size;
	vmem->mem_data = alloc;
	vmem->align_virtual_addr = (u8 *) alloc->kvaddr;
	vmem->align_device_addr = (u8 *)alloc->device_addr;
	return rc;
fail_clean_cache:
	msm_smem_free(clnt, alloc);
fail_smem_alloc:
	return rc;
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
	dprintk(VIDC_DBG, "Base addr: 0x%x, written to: 0x%x, Value: 0x%x...",
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	spin_lock(&device->write_lock);
	q_info = &device->iface_queues[VIDC_IFACEQ_CMDQ_IDX];
	if (!q_info) {
		dprintk(VIDC_ERR, "cannot write to shared Q's");
		goto err_q_write;
	}

	if (!write_queue(q_info, (u8 *)pkt, &rx_req_is_set)) {
		if (rx_req_is_set)
			write_register(device->hal_data->register_base_addr,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		result = 0;
	} else {
		dprintk(VIDC_ERR, "vidc_hal_iface_cmdq_write:queue_full");
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	spin_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_MSGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared MSG Q's");
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
		dprintk(VIDC_INFO, "vidc_hal_iface_msgq_read:queue_empty");
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	spin_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_DBGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared DBG Q's");
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
		dprintk(VIDC_INFO, "vidc_hal_iface_dbgq_read:queue_empty");
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
	q_hdr->qhdr_q_size = VIDC_IFACEQ_QUEUE_SIZE / 4;
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

	vidc_hal_free(device->hal_client, device->mem_addr.mem_data);

	for (i = 0; i < VIDC_IFACEQ_NUMQ; i++) {
		device->iface_queues[i].q_hdr = NULL;
		device->iface_queues[i].q_array.mem_data = NULL;
		device->iface_queues[i].q_array.align_virtual_addr = NULL;
		device->iface_queues[i].q_array.align_device_addr = NULL;
	}
	device->iface_q_table.align_virtual_addr = NULL;
	device->iface_q_table.align_device_addr = NULL;

	device->qdss.align_virtual_addr = NULL;
	device->qdss.align_device_addr = NULL;

	device->sfr.align_virtual_addr = NULL;
	device->sfr.align_device_addr = NULL;

	device->mem_addr.align_virtual_addr = NULL;
	device->mem_addr.align_device_addr = NULL;

	msm_smem_delete_client(device->hal_client);
	device->hal_client = NULL;
}

static int vidc_hal_interface_queues_init(struct hal_device *dev, int domain)
{
	struct hfi_queue_table_header *q_tbl_hdr;
	struct hfi_queue_header *q_hdr;
	u8 i;
	int rc = 0;
	struct vidc_iface_q_info *iface_q;
	struct hfi_sfr_struct *vsfr;
	struct vidc_mem_addr *mem_addr;
	int offset = 0;
	int size_1m = 1024 * 1024;
	int uc_size = (UC_SIZE + size_1m - 1) & (~(size_1m - 1));
	mem_addr = &dev->mem_addr;
	rc = vidc_hal_alloc((void *) mem_addr,
			dev->hal_client, uc_size, 1,
			0, domain);
	if (rc) {
		dprintk(VIDC_ERR, "iface_q_table_alloc_fail");
		return -ENOMEM;
	}
	dev->iface_q_table.align_virtual_addr = mem_addr->align_virtual_addr;
	dev->iface_q_table.align_device_addr = mem_addr->align_device_addr;
	dev->iface_q_table.mem_size = VIDC_IFACEQ_TABLE_SIZE;
	dev->iface_q_table.mem_data = NULL;
	offset += dev->iface_q_table.mem_size;

	for (i = 0; i < VIDC_IFACEQ_NUMQ; i++) {
		iface_q = &dev->iface_queues[i];
		iface_q->q_array.align_device_addr =
			mem_addr->align_device_addr + offset;
		iface_q->q_array.align_virtual_addr =
			mem_addr->align_virtual_addr + offset;
		iface_q->q_array.mem_size = VIDC_IFACEQ_QUEUE_SIZE;
		iface_q->q_array.mem_data = NULL;
		offset += iface_q->q_array.mem_size;
		iface_q->q_hdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(
				dev->iface_q_table.align_virtual_addr, i);
		vidc_hal_set_queue_hdr_defaults(iface_q->q_hdr);
	}

	dev->qdss.align_device_addr = mem_addr->align_device_addr + offset;
	dev->qdss.align_virtual_addr = mem_addr->align_virtual_addr + offset;
	dev->qdss.mem_size = QDSS_SIZE;
	dev->qdss.mem_data = NULL;
	offset += dev->qdss.mem_size;

	dev->sfr.align_device_addr = mem_addr->align_device_addr + offset;
	dev->sfr.align_virtual_addr = mem_addr->align_virtual_addr + offset;
	dev->sfr.mem_size = SFR_SIZE;
	dev->sfr.mem_data = NULL;
	offset += dev->sfr.mem_size;

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
			VIDC_UC_REGION_ADDR,
			(u32) mem_addr->align_device_addr, 0);
	write_register(dev->hal_data->register_base_addr,
			VIDC_UC_REGION_SIZE, mem_addr->mem_size, 0);
	write_register(dev->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG2,
		(u32) dev->iface_q_table.align_device_addr,
		dev->iface_q_table.align_virtual_addr);
	write_register(dev->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG1, 0x01,
		dev->iface_q_table.align_virtual_addr);
	write_register(dev->hal_data->register_base_addr,
			VIDC_MMAP_ADDR,
			(u32) dev->qdss.align_device_addr, 0);

	vsfr = (struct hfi_sfr_struct *) dev->sfr.align_virtual_addr;
	vsfr->bufSize = SFR_SIZE;

	write_register(dev->hal_data->register_base_addr,
			VIDC_SFR_ADDR, (u32)dev->sfr.align_device_addr , 0);
	return 0;
}

static int vidc_hal_core_start_cpu(struct hal_device *device)
{
	u32 ctrl_status = 0, count = 0, rc = 0;
	int max_tries = 100;
	write_register(device->hal_data->register_base_addr,
			VIDC_WRAPPER_INTR_MASK, 0x8, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_CPU_CS_SCIACMDARG3, 1, 0);

	while (!ctrl_status && count < max_tries) {
		ctrl_status = read_register(
		device->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG0);
		if ((ctrl_status & 0xFE) == 0x4) {
			dprintk(VIDC_ERR, "invalid setting for UC_REGION\n");
			break;
		}
		usleep_range(500, 1000);
		count++;
	}
	if (count >= max_tries)
		rc = -ETIME;
	return rc;
}

static void set_vbif_registers(struct hal_device *device)
{
	/*Disable Dynamic clock gating for Venus VBIF*/
	write_register(device->hal_data->register_base_addr,
				   VIDC_VENUS_VBIF_CLK_ON, 1, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_AXI_AOOO_EN, 0x00001FFF, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_AXI_AOOO, 0x1FFF1FFF, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF0, 0x10101001, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF1, 0x10101010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF2, 0x10101010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF3, 0x00000010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF0, 0x1010100f, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF1, 0x10101010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF2, 0x10101010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF3, 0x00000010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_RD_LIM_CONF0, 0x00001010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_WR_LIM_CONF0, 0x00001010, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_ARB_CTL, 0x00000030, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VENUS_VBIF_DDR_OUT_MAX_BURST, 0x00000707, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VENUS_VBIF_OCMEM_OUT_MAX_BURST, 0x00000707, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VENUS_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000001, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VENUS0_WRAPPER_VBIF_REQ_PRIORITY, 0x5555556, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_VENUS0_WRAPPER_VBIF_PRIORITY_LEVEL, 0, 0);
}

static int vidc_hal_sys_set_debug(struct hal_device *device, int debug)
{
	struct hfi_debug_config *hfi;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hfi_cmd_sys_set_property_packet *pkt =
		(struct hfi_cmd_sys_set_property_packet *) &packet;
	pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
		sizeof(struct hfi_debug_config) + sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_DEBUG_CONFIG;
	hfi = (struct hfi_debug_config *) &pkt->rg_property_data[1];
	hfi->debug_config = debug;
	hfi->debug_mode = HFI_DEBUG_MODE_QUEUE;
	if (vidc_hal_iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

int vidc_hal_core_init(void *device, int domain)
{
	struct hfi_cmd_sys_init_packet pkt;
	int rc = 0;
	struct hal_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "Invalid device");
		return -ENODEV;
	}
	dev->intr_status = 0;
	enable_irq(dev->hal_data->irq);
	INIT_LIST_HEAD(&dev->sess_head);
	spin_lock_init(&dev->read_lock);
	spin_lock_init(&dev->write_lock);
	set_vbif_registers(dev);

	if (!dev->hal_client) {
		dev->hal_client = msm_smem_new_client(SMEM_ION);
		if (dev->hal_client == NULL) {
			dprintk(VIDC_ERR, "Failed to alloc ION_Client");
			rc = -ENODEV;
			goto err_core_init;
		}

		dprintk(VIDC_DBG, "Dev_Virt: 0x%x, Reg_Virt: 0x%x",
		dev->hal_data->device_base_addr,
		(u32) dev->hal_data->register_base_addr);

		rc = vidc_hal_interface_queues_init(dev, domain);
		if (rc) {
			dprintk(VIDC_ERR, "failed to init queues");
			rc = -ENOMEM;
			goto err_core_init;
		}
	} else {
		dprintk(VIDC_ERR, "hal_client exists");
		rc = -EEXIST;
		goto err_core_init;
	}
	write_register(dev->hal_data->register_base_addr,
		VIDC_CTRL_INIT, 0x1, 0);
	rc = vidc_hal_core_start_cpu(dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to start core");
		rc = -ENODEV;
		goto err_core_init;
	}

	rc = create_pkt_cmd_sys_init(&pkt, HFI_ARCH_OX_OFFSET);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to create sys init pkt");
		goto err_core_init;
	}
	if (vidc_hal_iface_cmdq_write(dev, &pkt)) {
		rc = -ENOTEMPTY;
		goto err_core_init;
	}
	return rc;
err_core_init:
	disable_irq_nosync(dev->hal_data->irq);
	return rc;
}

int vidc_hal_core_release(void *device)
{
	struct hal_device *dev;
	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device");
		return -ENODEV;
	}
	if (dev->hal_client) {
		write_register(dev->hal_data->register_base_addr,
				VIDC_CPU_CS_SCIACMDARG3, 0, 0);
		disable_irq_nosync(dev->hal_data->irq);
		vidc_hal_interface_queues_release(dev);
	}
	dprintk(VIDC_INFO, "HAL exited\n");
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
		dprintk(VIDC_ERR, "invalid device");
		return -ENODEV;
	}

	rc = create_pkt_cmd_sys_pc_prep(&pkt);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to create sys pc prep pkt");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
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
		dprintk(VIDC_DBG, "INTERRUPT for device: 0x%x: "
			"times: %d interrupt_status: %d",
			(u32) device, ++device->reg_count, intr_status);
	} else {
		dprintk(VIDC_INFO, "SPURIOUS_INTR for device: 0x%x: "
			"times: %d interrupt_status: %d",
			(u32) device, ++device->spur_count, intr_status);
	}
	write_register(device->hal_data->register_base_addr,
			VIDC_CPU_CS_A2HSOFTINTCLR, 1, 0);
	write_register(device->hal_data->register_base_addr,
			VIDC_WRAPPER_INTR_CLEAR, intr_status, 0);
	dprintk(VIDC_DBG, "Cleared WRAPPER/A2H interrupt");
}

int vidc_hal_core_set_resource(void *device,
		struct vidc_resource_hdr *resource_hdr, void *resource_value)
{
	struct hfi_cmd_sys_set_resource_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct hal_device *dev;

	if (!device || !resource_hdr || !resource_value) {
		dprintk(VIDC_ERR, "set_res: Invalid Params");
		return -EINVAL;
	} else {
		dev = device;
	}

	pkt = (struct hfi_cmd_sys_set_resource_packet *) packet;

	rc = create_pkt_set_cmd_sys_resource(pkt, resource_hdr,
							resource_value);
	if (rc) {
		dprintk(VIDC_ERR, "set_res: failed to create packet");
		goto err_create_pkt;
	}
	if (vidc_hal_iface_cmdq_write(dev, pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

int vidc_hal_core_release_resource(void *device,
			struct vidc_resource_hdr *resource_hdr)
{
	struct hfi_cmd_sys_release_resource_packet pkt;
	int rc = 0;
	struct hal_device *dev;

	if (!device || !resource_hdr) {
		dprintk(VIDC_ERR, "Inv-Params in rel_res");
		return -EINVAL;
	} else {
		dev = device;
	}

	rc = create_pkt_cmd_sys_release_resource(&pkt, resource_hdr);
	if (rc) {
		dprintk(VIDC_ERR, "release_res: failed to create packet");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
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
		dprintk(VIDC_ERR, "invalid device");
		return -ENODEV;
	}

	rc = create_pkt_cmd_sys_ping(&pkt);
	if (rc) {
		dprintk(VIDC_ERR, "core_ping: failed to create packet");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

int vidc_hal_session_set_property(void *sess,
	enum hal_property ptype, void *pdata)
{
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	struct hfi_cmd_session_set_property_packet *pkt =
		(struct hfi_cmd_session_set_property_packet *) &packet;
	struct hal_session *session;
	int rc = 0;

	if (!sess || !pdata) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	} else {
		session = sess;
	}

	dprintk(VIDC_INFO, "in set_prop,with prop id: 0x%x", ptype);

	if (create_pkt_cmd_session_set_property(pkt, (u32)session, ptype,
				pdata)) {
		dprintk(VIDC_ERR, "set property: failed to create packet");
		return -EINVAL;
	}

	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		return -ENOTEMPTY;

	return rc;
}

int vidc_hal_session_get_property(void *sess,
	enum hal_property ptype, void *pdata)
{
	struct hal_session *session;

	if (!sess || !pdata) {
		dprintk(VIDC_ERR, "Invalid Params in ");
		return -EINVAL;
	} else {
		session = sess;
	}
	dprintk(VIDC_INFO, "IN func: , with property id: %d", ptype);

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
		dprintk(VIDC_INFO, "DEFAULT: Calling 0x%x", ptype);
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
		dprintk(VIDC_ERR, "invalid device");
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

	if (create_pkt_cmd_sys_session_init(&pkt, (u32)new_session,
			session_type, codec_type)) {
		dprintk(VIDC_ERR, "session_init: failed to create packet");
		goto err_session_init_fail;
	}

	if (vidc_hal_iface_cmdq_write(dev, &pkt))
		goto err_session_init_fail;
	if (vidc_hal_sys_set_debug(dev, msm_fw_debug))
		dprintk(VIDC_ERR, "Setting fw_debug msg ON failed");
	return (void *) new_session;

err_session_init_fail:
	kfree(new_session);
	return NULL;
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
		dprintk(VIDC_ERR, "invalid session");
		return -ENODEV;
	}

	rc = create_pkt_cmd_session_cmd(&pkt, pkt_type, (u32)session);
	if (rc) {
		dprintk(VIDC_ERR, "send session cmd: create pkt failed");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
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
	struct hfi_cmd_session_set_buffers_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	struct hal_session *session;

	if (!sess || !buffer_info) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	} else {
		session = sess;
	}

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_cmd_session_set_buffers_packet *)packet;

	rc = create_pkt_cmd_session_set_buffers(pkt,
			(u32)session, buffer_info);
	if (rc) {
		dprintk(VIDC_ERR, "set buffers: failed to create packet");
		goto err_create_pkt;
	}

	dprintk(VIDC_INFO, "set buffers: 0x%x", buffer_info->buffer_type);
	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

int vidc_hal_session_release_buffers(void *sess,
	struct vidc_buffer_addr_info *buffer_info)
{
	struct hfi_cmd_session_release_buffer_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_LARGE_PKT_SIZE];
	int rc = 0;
	struct hal_session *session;

	if (!sess || !buffer_info) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	} else {
		session = sess;
	}

	if (buffer_info->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_cmd_session_release_buffer_packet *) packet;

	rc = create_pkt_cmd_session_release_buffers(pkt,
					(u32)session, buffer_info);
	if (rc) {
		dprintk(VIDC_ERR, "release buffers: failed to create packet");
		goto err_create_pkt;
	}

	dprintk(VIDC_INFO, "Release buffers: 0x%x", buffer_info->buffer_type);
	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	} else {
		session = sess;
	}

	if (session->is_decoder) {
		struct hfi_cmd_session_empty_buffer_compressed_packet pkt;

		rc = create_pkt_cmd_session_etb_decoder(&pkt,
					(u32)session, input_frame);
		if (rc) {
			dprintk(VIDC_ERR,
			"Session etb decoder: failed to create pkt");
			goto err_create_pkt;
		}
		dprintk(VIDC_DBG, "Q DECODER INPUT BUFFER");
		if (vidc_hal_iface_cmdq_write(session->device, &pkt))
			rc = -ENOTEMPTY;
	} else {
		struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			pkt;

		rc =  create_pkt_cmd_session_etb_encoder(&pkt,
					(u32)session, input_frame);
		if (rc) {
			dprintk(VIDC_ERR,
			"Session etb encoder: failed to create pkt");
			goto err_create_pkt;
		}
		dprintk(VIDC_DBG, "Q ENCODER INPUT BUFFER");
		if (vidc_hal_iface_cmdq_write(session->device, &pkt))
			rc = -ENOTEMPTY;
	}
err_create_pkt:
	return rc;
}

int vidc_hal_session_ftb(void *sess,
	struct vidc_frame_data *output_frame)
{
	struct hfi_cmd_session_fill_buffer_packet pkt;
	int rc = 0;
	struct hal_session *session;

	if (!sess || !output_frame) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	} else {
		session = sess;
	}

	rc = create_pkt_cmd_session_ftb(&pkt, (u32)session, output_frame);
	if (rc) {
		dprintk(VIDC_ERR, "Session ftb: failed to create pkt");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	} else {
		session = sess;
	}

	pkt = (struct hfi_cmd_session_parse_sequence_header_packet *) packet;

	rc = create_pkt_cmd_session_parse_seq_header(pkt, (u32)session,
							seq_hdr);
	if (rc) {
		dprintk(VIDC_ERR,
		"Session parse seq hdr: failed to create pkt");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
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
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	} else {
		session = sess;
	}

	pkt = (struct hfi_cmd_session_get_sequence_header_packet *) packet;
	rc = create_pkt_cmd_session_get_seq_hdr(pkt, (u32)session, seq_hdr);
	if (rc) {
		dprintk(VIDC_ERR, "Session get seq hdr: failed to create pkt");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
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
		dprintk(VIDC_ERR, "invalid session");
		return -ENODEV;
	}

	rc = create_pkt_cmd_session_get_buf_req(&pkt, (u32)session);
	if (rc) {
		dprintk(VIDC_ERR, "Session get buf req: failed to create pkt");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
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
		dprintk(VIDC_ERR, "invalid session");
		return -ENODEV;
	}

	rc = create_pkt_cmd_session_flush(&pkt, (u32)session, flush_mode);
	if (rc) {
		dprintk(VIDC_ERR, "Session flush: failed to create pkt");
		goto err_create_pkt;
	}

	if (vidc_hal_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
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
				(CONTAINS(device->hal_data->
						device_base_addr,
						FIRMWARE_SIZE, fw_addr) ||
				CONTAINS(fw_addr, FIRMWARE_SIZE,
						device->hal_data->
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
				OVERLAPS(device->hal_data->
						device_base_addr,
						FIRMWARE_SIZE, fw_addr,
						FIRMWARE_SIZE) ||
				OVERLAPS(fw_addr, FIRMWARE_SIZE,
						device->hal_data->
						device_base_addr,
						FIRMWARE_SIZE))) {
				return 0;
			} else {
				dprintk(VIDC_INFO, "Device not registered");
				return -EINVAL;
			}
		}
	} else {
		dprintk(VIDC_INFO, "no device Registered");
	}
	return -EINVAL;
}

static void vidc_hal_core_work_handler(struct work_struct *work)
{
	struct hal_device *device = list_first_entry(
		&hal_ctxt.dev_head, struct hal_device, list);

	dprintk(VIDC_INFO, " GOT INTERRUPT () ");
	if (!device->callback) {
		dprintk(VIDC_ERR, "No interrupt callback function: %p\n",
				device);
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
	dprintk(VIDC_INFO, "vidc_hal_isr() %d ", irq);
	disable_irq_nosync(irq);
	queue_work(device->vidc_workq, &vidc_hal_work);
	dprintk(VIDC_INFO, "vidc_hal_isr() %d ", irq);
	return IRQ_HANDLED;
}

void *vidc_hal_add_device(u32 device_id, u32 fw_base_addr, u32 reg_base,
		u32 reg_size, u32 irq,
		void (*callback) (enum command_response cmd, void *data))
{
	struct hal_device *hdevice = NULL;
	struct hal_data *hal = NULL;
	int rc = 0;

	if (device_id || !reg_base || !reg_size ||
			!irq || !callback) {
		dprintk(VIDC_ERR, "Invalid Paramters");
		return NULL;
	} else {
		dprintk(VIDC_INFO, "entered , device_id: %d", device_id);
	}

	if (vidc_hal_check_core_registered(hal_ctxt, fw_base_addr,
						reg_base, reg_size, irq)) {
		dprintk(VIDC_DBG, "HAL_DATA will be assigned now");
		hal = (struct hal_data *)
			kzalloc(sizeof(struct hal_data), GFP_KERNEL);
		if (!hal) {
			dprintk(VIDC_ERR, "Failed to alloc");
			return NULL;
		}
		hal->irq = irq;
		hal->device_base_addr = fw_base_addr;
		hal->register_base_addr =
			ioremap_nocache(reg_base, reg_size);
		if (!hal->register_base_addr) {
			dprintk(VIDC_ERR,
				"could not map reg addr %d of size %d",
				reg_base, reg_size);
			goto err_map;
		}
		INIT_LIST_HEAD(&hal_ctxt.dev_head);
	} else {
		dprintk(VIDC_ERR, "Core present/Already added");
		return NULL;
	}

	hdevice = (struct hal_device *)
			kzalloc(sizeof(struct hal_device), GFP_KERNEL);
	if (!hdevice) {
		dprintk(VIDC_ERR, "failed to allocate new device");
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
		dprintk(VIDC_ERR, ": create workq failed\n");
		goto error_createq;
	}

	rc = request_irq(irq, vidc_hal_isr, IRQF_TRIGGER_HIGH,
			"msm_vidc", hdevice);
	if (unlikely(rc)) {
		dprintk(VIDC_ERR, "() :request_irq failed\n");
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
				free_irq(dev->hal_data->irq, close);
				list_del(&close->list);
				destroy_workqueue(close->vidc_workq);
				kfree(close->hal_data);
				kfree(close);
				break;
			}
		}

	}
}
