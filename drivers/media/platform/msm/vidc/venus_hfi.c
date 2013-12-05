/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/iommu.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/ocmem.h>
#include <mach/scm.h>
#include <mach/subsystem_restart.h>
#include <mach/msm_smem.h>
#include <asm/memory.h>
#include <linux/iopoll.h>
#include "hfi_packetization.h"
#include "venus_hfi.h"
#include "vidc_hfi_io.h"
#include "msm_vidc_debug.h"
#include <linux/iopoll.h>

#define FIRMWARE_SIZE			0X00A00000
#define REG_ADDR_OFFSET_BITMASK	0x000FFFFF

/*Workaround for simulator */
#define HFI_SIM_FW_BIAS		0x0

#define SHARED_QSIZE 0x1000000

static struct hal_device_data hal_ctxt;

static const u32 venus_qdss_entries[][2] = {
	{0xFC307000, 0x1000},
	{0xFC322000, 0x1000},
	{0xFC319000, 0x1000},
	{0xFC31A000, 0x1000},
	{0xFC31B000, 0x1000},
	{0xFC321000, 0x1000},
	{0xFA180000, 0x1000},
	{0xFA181000, 0x1000},
};

#define TZBSP_MEM_PROTECT_VIDEO_VAR 0x8
struct tzbsp_memprot {
	u32 cp_start;
	u32 cp_size;
	u32 cp_nonpixel_start;
	u32 cp_nonpixel_size;
};

struct tzbsp_resp {
	int ret;
};

#define TZBSP_VIDEO_SET_STATE 0xa

/* Poll interval in uS */
#define POLL_INTERVAL_US 50

enum tzbsp_video_state {
	TZBSP_VIDEO_STATE_SUSPEND = 0,
	TZBSP_VIDEO_STATE_RESUME
};

struct tzbsp_video_set_state_req {
	u32 state; /*shoud be tzbsp_video_state enum value*/
	u32 spare; /*reserved for future, should be zero*/
};


static inline int venus_hfi_clk_gating_off(struct venus_hfi_device *device);

static int venus_hfi_power_enable(void *dev);

static unsigned long venus_hfi_get_clock_rate(struct venus_core_clock *clock,
		int num_mbs_per_sec);

static void venus_hfi_dump_packet(u8 *packet)
{
	u32 c = 0, packet_size = *(u32 *)packet;
	const int row_size = 32;
	/* row must contain enough for 0xdeadbaad * 8 to be converted into
	 * "de ad ba ab " * 8 + '\0' */
	char row[3 * row_size];

	for (c = 0; c * row_size < packet_size; ++c) {
		int bytes_to_read = ((c + 1) * row_size > packet_size) ?
			packet_size % row_size : row_size;
		hex_dump_to_buffer(packet + c * row_size, bytes_to_read,
				row_size, 4, row, sizeof(row), false);
		dprintk(VIDC_PKT, "%s\n", row);
	}
}

static void venus_hfi_sim_modify_cmd_packet(u8 *packet)
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
			pkt->packet_buffer -= HFI_SIM_FW_BIAS;
		} else {
			struct
			hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			*pkt = (struct
			hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
			*) packet;
			pkt->packet_buffer -= HFI_SIM_FW_BIAS;
		}
		break;
	case HFI_CMD_SESSION_FILL_BUFFER:
	{
		struct hfi_cmd_session_fill_buffer_packet *pkt =
			(struct hfi_cmd_session_fill_buffer_packet *)packet;
		pkt->packet_buffer -= HFI_SIM_FW_BIAS;
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
			buff->buffer_addr -= HFI_SIM_FW_BIAS;
			buff->extra_data_addr -= HFI_SIM_FW_BIAS;
		} else {
			for (i = 0; i < pkt->num_buffers; i++)
				pkt->rg_buffer_info[i] -= HFI_SIM_FW_BIAS;
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
			buff->buffer_addr -= HFI_SIM_FW_BIAS;
			buff->extra_data_addr -= HFI_SIM_FW_BIAS;
		} else {
			for (i = 0; i < pkt->num_buffers; i++)
				pkt->rg_buffer_info[i] -= HFI_SIM_FW_BIAS;
		}
		break;
	}
	case HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER:
	{
		struct hfi_cmd_session_parse_sequence_header_packet *pkt =
			(struct hfi_cmd_session_parse_sequence_header_packet *)
		packet;
		pkt->packet_buffer -= HFI_SIM_FW_BIAS;
		break;
	}
	case HFI_CMD_SESSION_GET_SEQUENCE_HEADER:
	{
		struct hfi_cmd_session_get_sequence_header_packet *pkt =
			(struct hfi_cmd_session_get_sequence_header_packet *)
		packet;
		pkt->packet_buffer -= HFI_SIM_FW_BIAS;
		break;
	}
	default:
		break;
	}
}

static int venus_hfi_write_queue(void *info, u8 *packet, u32 *rx_req_is_set)
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
	if (!qinfo || !qinfo->q_array.align_virtual_addr) {
		dprintk(VIDC_WARN, "Queues have already been freed\n");
		return -EINVAL;
	}

	queue = (struct hfi_queue_header *) qinfo->q_hdr;

	if (!queue) {
		dprintk(VIDC_ERR, "queue not present");
		return -ENOENT;
	}

	venus_hfi_sim_modify_cmd_packet(packet);

	if (msm_vidc_debug & VIDC_PKT) {
		dprintk(VIDC_PKT, "%s: %p\n", __func__, qinfo);
		venus_hfi_dump_packet(packet);
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
	/* Memory barrier to make sure packet is written before updating the
	 * write index */
	mb();
	queue->qhdr_write_idx = new_write_idx;
	*rx_req_is_set = (1 == queue->qhdr_rx_req) ? 1 : 0;
	/*Memory barrier to make sure write index is updated before an
	 * interupt is raised on venus.*/
	mb();
	dprintk(VIDC_DBG, "Out : ");
	return 0;
}

static void venus_hfi_hal_sim_modify_msg_packet(u8 *packet)
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
			pkt_uc->packet_buffer += HFI_SIM_FW_BIAS;
		} else {
			struct
			hfi_msg_session_fill_buffer_done_compressed_packet
			*pkt = (struct
			hfi_msg_session_fill_buffer_done_compressed_packet
			*) packet;
			pkt->packet_buffer += HFI_SIM_FW_BIAS;
		}
		break;
	case HFI_MSG_SESSION_EMPTY_BUFFER_DONE:
	{
		struct hfi_msg_session_empty_buffer_done_packet *pkt =
		(struct hfi_msg_session_empty_buffer_done_packet *)packet;
		pkt->packet_buffer += HFI_SIM_FW_BIAS;
		break;
	}
	case HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE:
	{
		struct
		hfi_msg_session_get_sequence_header_done_packet
		*pkt =
		(struct hfi_msg_session_get_sequence_header_done_packet *)
		packet;
		pkt->sequence_header += HFI_SIM_FW_BIAS;
		break;
	}
	default:
		break;
	}
}

static int venus_hfi_read_queue(void *info, u8 *packet, u32 *pb_tx_req_is_set)
{
	struct hfi_queue_header *queue;
	u32 packet_size_in_words, new_read_idx;
	u32 *read_ptr;
	struct vidc_iface_q_info *qinfo;
	int rc = 0;

	if (!info || !packet || !pb_tx_req_is_set) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	qinfo =	(struct vidc_iface_q_info *) info;
	if (!qinfo || !qinfo->q_array.align_virtual_addr) {
		dprintk(VIDC_WARN, "Queues have already been freed\n");
		return -EINVAL;
	}
	/*Memory barrier to make sure data is valid before
	 *reading it*/
	mb();
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
	if (((packet_size_in_words << 2) <= VIDC_IFACEQ_MED_PKT_SIZE)
			&& queue->qhdr_read_idx <= queue->qhdr_q_size) {
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
	} else {
		dprintk(VIDC_WARN,
			"BAD packet received, read_idx: 0x%x, pkt_size: %d\n",
			queue->qhdr_read_idx, packet_size_in_words << 2);
		dprintk(VIDC_WARN, "Dropping this packet\n");
		new_read_idx = queue->qhdr_write_idx;
		rc = -ENODATA;
	}

	queue->qhdr_read_idx = new_read_idx;

	if (queue->qhdr_read_idx != queue->qhdr_write_idx)
		queue->qhdr_rx_req = 0;
	else
		queue->qhdr_rx_req = 1;

	*pb_tx_req_is_set = (1 == queue->qhdr_tx_req) ? 1 : 0;
	venus_hfi_hal_sim_modify_msg_packet(packet);
	if (msm_vidc_debug & VIDC_PKT) {
		dprintk(VIDC_PKT, "%s: %p\n", __func__, qinfo);
		venus_hfi_dump_packet(packet);
	}
	dprintk(VIDC_DBG, "Out : ");
	return rc;
}

static int venus_hfi_alloc(struct venus_hfi_device *dev, void *mem,
			u32 size, u32 align, u32 flags, u32 usage)
{
	struct vidc_mem_addr *vmem = NULL;
	struct msm_smem *alloc = NULL;
	int rc = 0;

	if (!dev || !dev->hal_client || !mem || !size) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}

	vmem = (struct vidc_mem_addr *)mem;
	dprintk(VIDC_INFO, "start to alloc: size:%d, Flags: %d", size, flags);

	venus_hfi_power_enable(dev);

	alloc = msm_smem_alloc(dev->hal_client, size, align, flags, usage, 1);
	dprintk(VIDC_DBG, "Alloc done");
	if (!alloc) {
		dprintk(VIDC_ERR, "Alloc failed\n");
		rc = -ENOMEM;
		goto fail_smem_alloc;
	}
	dprintk(VIDC_DBG, "venus_hfi_alloc:ptr=%p,size=%d",
			alloc->kvaddr, size);
	rc = msm_smem_cache_operations(dev->hal_client, alloc,
		SMEM_CACHE_CLEAN);
	if (rc) {
		dprintk(VIDC_WARN, "Failed to clean cache\n");
		dprintk(VIDC_WARN, "This may result in undefined behavior\n");
	}
	vmem->mem_size = alloc->size;
	vmem->mem_data = alloc;
	vmem->align_virtual_addr = (u8 *) alloc->kvaddr;
	vmem->align_device_addr = (u8 *)alloc->device_addr;
	return rc;
fail_smem_alloc:
	return rc;
}

static void venus_hfi_free(struct venus_hfi_device *dev, struct msm_smem *mem)
{
	if (!dev || !mem) {
		dprintk(VIDC_ERR, "invalid param %p %p\n", dev, mem);
		return;
	}
	venus_hfi_power_enable(dev);
	msm_smem_free(dev->hal_client, mem);
}

static void venus_hfi_write_register(struct venus_hfi_device *device, u32 reg,
				u32 value, u8 *vaddr)
{
	u32 hwiosymaddr = reg;
	u8 *base_addr;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return;
	}

	base_addr = device->hal_data->register_base_addr;
	if (!device->clocks_enabled) {
		dprintk(VIDC_WARN,
			"HFI Write register failed : Clocks are OFF\n");
		return;
	}
	reg &= REG_ADDR_OFFSET_BITMASK;
	if (reg == (u32)VIDC_CPU_CS_SCIACMDARG2) {
		/* workaround to offset of FW bias */
		struct hfi_queue_header *qhdr;
		struct hfi_queue_table_header *qtbl_hdr =
			(struct hfi_queue_table_header *)vaddr;

		qhdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(qtbl_hdr, 0);
		qhdr->qhdr_start_addr -= HFI_SIM_FW_BIAS;

		qhdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(qtbl_hdr, 1);
		qhdr->qhdr_start_addr -= HFI_SIM_FW_BIAS;

		qhdr = VIDC_IFACEQ_GET_QHDR_START_ADDR(qtbl_hdr, 2);
		qhdr->qhdr_start_addr -= HFI_SIM_FW_BIAS;
		value -= HFI_SIM_FW_BIAS;
	}

	hwiosymaddr = ((u32)base_addr + (hwiosymaddr));
	dprintk(VIDC_DBG, "Base addr: 0x%x, written to: 0x%x, Value: 0x%x...",
			(u32)base_addr, hwiosymaddr, value);
	writel_relaxed(value, hwiosymaddr);
	wmb();
}

static int venus_hfi_read_register(struct venus_hfi_device *device, u32 reg)
{
	int rc ;
	u8 *base_addr;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}

	base_addr = device->hal_data->register_base_addr;
	if (!device->clocks_enabled) {
		dprintk(VIDC_WARN,
			"HFI Read register failed : Clocks are OFF\n");
		return -EINVAL;
	}
	rc = readl_relaxed((u32)base_addr + reg);
	rmb();
	return rc;
}

static void venus_hfi_set_registers(struct venus_hfi_device *device)
{
	struct reg_set *reg_set;
	int i;

	if (!device->res) {
		dprintk(VIDC_ERR,
			"device resources null, cannot set registers\n");
		return;
	}

	reg_set = &device->res->reg_set;
	for (i = 0; i < reg_set->count; i++) {
		venus_hfi_write_register(device,
				reg_set->reg_tbl[i].reg,
				reg_set->reg_tbl[i].value, 0);
	}
}

static int venus_hfi_core_start_cpu(struct venus_hfi_device *device)
{
	u32 ctrl_status = 0, count = 0, rc = 0;
	int max_tries = 100;
	venus_hfi_write_register(device,
			VIDC_WRAPPER_INTR_MASK, VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK, 0);
	venus_hfi_write_register(device,
			VIDC_CPU_CS_SCIACMDARG3, 1, 0);

	while (!ctrl_status && count < max_tries) {
		ctrl_status = venus_hfi_read_register(
				device,
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

static int venus_hfi_iommu_attach(struct venus_hfi_device *device)
{
	int rc = 0;
	struct iommu_domain *domain;
	int i;
	struct iommu_set *iommu_group_set;
	struct iommu_group *group;
	struct iommu_info *iommu_map;

	if (!device || !device->res)
		return -EINVAL;

	iommu_group_set = &device->res->iommu_group_set;
	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		group = iommu_map->group;
		domain = msm_get_iommu_domain(iommu_map->domain);
		if (IS_ERR_OR_NULL(domain)) {
			dprintk(VIDC_ERR,
				"Failed to get domain: %s\n", iommu_map->name);
			rc = PTR_ERR(domain);
			break;
		}
		rc = iommu_attach_group(domain, group);
		if (rc) {
			dprintk(VIDC_ERR,
				"IOMMU attach failed: %s\n", iommu_map->name);
			break;
		}
	}
	if (i < iommu_group_set->count) {
		i--;
		for (; i >= 0; i--) {
			iommu_map = &iommu_group_set->iommu_maps[i];
			group = iommu_map->group;
			domain = msm_get_iommu_domain(iommu_map->domain);
			if (group && domain)
				iommu_detach_group(domain, group);
		}
	}
	return rc;
}

static void venus_hfi_iommu_detach(struct venus_hfi_device *device)
{
	struct iommu_group *group;
	struct iommu_domain *domain;
	struct iommu_set *iommu_group_set;
	struct iommu_info *iommu_map;
	int i;

	if (!device || !device->res) {
		dprintk(VIDC_ERR, "Invalid paramter: %p\n", device);
		return;
	}

	iommu_group_set = &device->res->iommu_group_set;
	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		group = iommu_map->group;
		domain = msm_get_iommu_domain(iommu_map->domain);
		if (group && domain)
			iommu_detach_group(domain, group);
	}
}

static int venus_hfi_unvote_bus(void *dev,
				enum session_type type, enum mem_type mtype)
{
	int rc = 0;
	u32 handle = 0;
	struct venus_hfi_device *device = dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s invalid device handle %p",
			__func__, device);
		return -EINVAL;
	}

	if (mtype & DDR_MEM)
		handle = device->resources.bus_info.ddr_handle[type];
	if (mtype & OCMEM_MEM)
		handle = device->resources.bus_info.ocmem_handle[type];

	if (handle) {
		rc = msm_bus_scale_client_update_request(
				handle, 0);
		if (rc)
			dprintk(VIDC_ERR, "Failed to unvote bus: %d\n", rc);
	} else {
		dprintk(VIDC_ERR, "Failed to unvote bus, mtype: %d\n",
				mtype);
		rc = -EINVAL;
	}
	return rc;
}

static void venus_hfi_unvote_buses(void *dev, enum mem_type mtype)
{
	int i;
	struct venus_hfi_device *device = dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return;
	}

	for (i = 0; i < MSM_VIDC_MAX_DEVICES; i++) {
		if ((mtype & DDR_MEM) &&
			venus_hfi_unvote_bus(device, i, DDR_MEM))
			dprintk(VIDC_WARN,
				"Failed to unvote for DDR accesses\n");

		if ((mtype & OCMEM_MEM) &&
			venus_hfi_unvote_bus(device, i, OCMEM_MEM))
			dprintk(VIDC_WARN,
				"Failed to unvote for OCMEM accesses\n");
	}
}

static const u32 venus_hfi_bus_table[] = {
	36000,
	110400,
	244800,
	489000,
	783360,
	979200,
};

static int venus_hfi_get_bus_vector(struct venus_hfi_device *device, int load,
			enum session_type type, enum mem_type mtype)
{
	int num_rows = sizeof(venus_hfi_bus_table)/(sizeof(u32));
	int i, j;
	int idx = 0;

	if (!device || (mtype != DDR_MEM && mtype != OCMEM_MEM) ||
		(type != MSM_VIDC_ENCODER && type != MSM_VIDC_DECODER)) {
		dprintk(VIDC_ERR, "%s invalid params", __func__);
		return -EINVAL;
	}

	for (i = 0; i < num_rows; i++) {
		if (load <= venus_hfi_bus_table[i])
			break;
	}

	if (type == MSM_VIDC_ENCODER)
		idx = (mtype == DDR_MEM) ? BUS_IDX_ENC_DDR : BUS_IDX_ENC_OCMEM;
	else
		idx = (mtype == DDR_MEM) ? BUS_IDX_DEC_DDR : BUS_IDX_DEC_OCMEM;

	j = clamp(i, 0, num_rows-1) + 1;

	/* Ensure bus index remains within the supported range,
	* as specified in the device dtsi file */
	j = clamp(j, 0, device->res->bus_pdata[idx].num_usecases - 1);

	dprintk(VIDC_DBG, "Required bus = %d\n", j);
	return j;
}

static int venus_hfi_scale_bus(void *dev, int load,
				enum session_type type, enum mem_type mtype)
{
	int rc = 0;
	u32 handle = 0;
	struct venus_hfi_device *device = dev;
	int bus_vector = 0;

	if (!device) {
		dprintk(VIDC_ERR, "%s invalid device handle %p",
			__func__, device);
		return -EINVAL;
	}

	if (mtype & DDR_MEM)
		handle = device->resources.bus_info.ddr_handle[type];
	if (mtype & OCMEM_MEM)
		handle = device->resources.bus_info.ocmem_handle[type];

	if (handle) {
		bus_vector = venus_hfi_get_bus_vector(device, load,
				type, mtype);
		if (bus_vector < 0) {
			dprintk(VIDC_ERR, "Failed to get bus vector\n");
			return -EINVAL;
		}
		device->bus_load[type] = load;
		rc = msm_bus_scale_client_update_request(handle, bus_vector);
		if (rc)
			dprintk(VIDC_ERR, "Failed to scale bus: %d\n", rc);
	} else {
		dprintk(VIDC_ERR, "Failed to scale bus, mtype: %d\n",
				mtype);
		rc = -EINVAL;
	}

	return rc;
}

static int venus_hfi_scale_buses(void *dev, enum mem_type mtype)
{
	int i, rc = 0;
	struct venus_hfi_device *device = dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}
	for (i = 0; i < MSM_VIDC_MAX_DEVICES; i++) {
		if (mtype & DDR_MEM) {
			rc = venus_hfi_scale_bus(device, device->bus_load[i],
					i, DDR_MEM);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed to scale bus for DDR accesses, session type %d, load %u\n",
						i, device->bus_load[i]);
				goto err_scale_bus;
			}
		}

		if (mtype & OCMEM_MEM) {
			rc = venus_hfi_scale_bus(device, device->bus_load[i],
					i, OCMEM_MEM);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed to scale bus for OCMEM accesses, session type %d, load %u\n",
						i, device->bus_load[i]);
				goto err_scale_bus;
			}
		}
	}
err_scale_bus:
	return rc;
}

static int venus_hfi_iface_cmdq_write_nolock(struct venus_hfi_device *device,
					void *pkt);

static int venus_hfi_iface_cmdq_write(struct venus_hfi_device *device,
					void *pkt)
{
	int result = -EPERM;
	if (!device || !pkt) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	mutex_lock(&device->write_lock);
	mutex_lock(&device->clk_pwr_lock);
	result = venus_hfi_iface_cmdq_write_nolock(device, pkt);
	mutex_unlock(&device->clk_pwr_lock);
	mutex_unlock(&device->write_lock);
	return result;
}

static int venus_hfi_core_set_resource(void *device,
		struct vidc_resource_hdr *resource_hdr, void *resource_value,
		int locked)
{
	struct hfi_cmd_sys_set_resource_packet *pkt;
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct venus_hfi_device *dev;

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
	rc = locked ? venus_hfi_iface_cmdq_write(dev, pkt) :
			venus_hfi_iface_cmdq_write_nolock(dev, pkt);
	if (rc)
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_core_release_resource(void *device,
			struct vidc_resource_hdr *resource_hdr)
{
	struct hfi_cmd_sys_release_resource_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

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

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_set_ocmem(void *dev, struct ocmem_buf *ocmem, int locked)
{
	struct vidc_resource_hdr rhdr;
	struct venus_hfi_device *device = dev;
	int rc = 0;
	if (!device || !ocmem) {
		dprintk(VIDC_ERR, "Invalid params, core:%p, ocmem: %p\n",
			device, ocmem);
		return -EINVAL;
	}
	rhdr.resource_id = VIDC_RESOURCE_OCMEM;
	rhdr.resource_handle = (u32) &device->resources.ocmem;
	rhdr.size =	ocmem->len;
	rc = venus_hfi_core_set_resource(device, &rhdr, ocmem, locked);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to set OCMEM on driver\n");
		goto ocmem_set_failed;
	}
	dprintk(VIDC_DBG, "OCMEM set, addr = %lx, size: %ld\n",
		ocmem->addr, ocmem->len);
ocmem_set_failed:
	return rc;
}

static int venus_hfi_unset_ocmem(void *dev)
{
	struct vidc_resource_hdr rhdr;
	struct venus_hfi_device *device = dev;
	int rc = 0;

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid params, device:%p\n",
			__func__, device);
		rc = -EINVAL;
		goto ocmem_unset_failed;
	}
	if (!device->resources.ocmem.buf) {
		dprintk(VIDC_INFO, "%s Trying to free OCMEM which is not set",
			__func__);
		rc = -EINVAL;
		goto ocmem_unset_failed;
	}
	rhdr.resource_id = VIDC_RESOURCE_OCMEM;
	rhdr.resource_handle = (u32) &device->resources.ocmem;
	rc = venus_hfi_core_release_resource(device, &rhdr);
	if (rc)
		dprintk(VIDC_ERR, "Failed to unset OCMEM on driver\n");
ocmem_unset_failed:
	return rc;
}

static int __alloc_ocmem(void *dev, unsigned long size, int locked)
{
	int rc = 0;
	struct ocmem_buf *ocmem_buffer;
	struct venus_hfi_device *device = dev;

	if (!device || !size) {
		dprintk(VIDC_ERR, "%s Invalid param, core: %p, size: %lu\n",
			__func__, device, size);
		return -EINVAL;
	}
	ocmem_buffer = device->resources.ocmem.buf;
	if (!ocmem_buffer ||
		ocmem_buffer->len < size) {
		ocmem_buffer = ocmem_allocate(OCMEM_VIDEO, size);
		if (IS_ERR_OR_NULL(ocmem_buffer)) {
			dprintk(VIDC_ERR,
				"ocmem_allocate_nb failed: %d\n",
				(u32) ocmem_buffer);
			rc = -ENOMEM;
			goto ocmem_alloc_failed;
		}
		device->resources.ocmem.buf = ocmem_buffer;
		rc = venus_hfi_set_ocmem(device, ocmem_buffer, locked);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to set ocmem: %d\n", rc);
			goto ocmem_set_failed;
		}
		device->ocmem_size = size;
	} else
		dprintk(VIDC_DBG,
			"OCMEM is enough. reqd: %lu, available: %lu\n",
			size, ocmem_buffer->len);

	return rc;
ocmem_set_failed:
	ocmem_free(OCMEM_VIDEO, device->resources.ocmem.buf);
	device->resources.ocmem.buf = NULL;
ocmem_alloc_failed:
	return rc;
}

static int venus_hfi_alloc_ocmem(void *dev, unsigned long size)
{
	return __alloc_ocmem(dev, size, true);
}

static int venus_hfi_free_ocmem(void *dev)
{
	struct venus_hfi_device *device = dev;
	int rc = 0;

	if (!device) {
		dprintk(VIDC_ERR, "%s invalid device handle %p",
			__func__, device);
		return -EINVAL;
	}

	if (device->resources.ocmem.buf) {
		rc = ocmem_free(OCMEM_VIDEO, device->resources.ocmem.buf);
		if (rc)
			dprintk(VIDC_ERR, "Failed to free ocmem\n");
		device->resources.ocmem.buf = NULL;
	}
	return rc;
}

static inline int venus_hfi_tzbsp_set_video_state(enum tzbsp_video_state state)
{
	struct tzbsp_video_set_state_req cmd = {0};
	int tzbsp_rsp = 0;
	int rc = 0;
	cmd.state = state;
	cmd.spare = 0;
	rc = scm_call(SCM_SVC_BOOT, TZBSP_VIDEO_SET_STATE, &cmd, sizeof(cmd),
			&tzbsp_rsp, sizeof(tzbsp_rsp));
	if (rc) {
		dprintk(VIDC_ERR, "Failed scm_call %d\n", rc);
		return rc;
	}
	dprintk(VIDC_DBG, "Set state %d, resp %d\n", state, tzbsp_rsp);
	if (tzbsp_rsp) {
		dprintk(VIDC_ERR,
				"Failed to set video core state to suspend: %d\n",
				tzbsp_rsp);
		return -EINVAL;
	}
	return 0;
}

static inline int venus_hfi_reset_core(struct venus_hfi_device *device)
{
	int rc = 0;
	venus_hfi_write_register(device,
			VIDC_CTRL_INIT, 0x1, 0);
	rc = venus_hfi_core_start_cpu(device);
	if (rc)
		dprintk(VIDC_ERR, "Failed to start core");
	return rc;
}

static inline int venus_hfi_clk_enable(struct venus_hfi_device *device)
{
	int rc = 0;
	int i;
	struct venus_core_clock *cl;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	WARN(!mutex_is_locked(&device->clk_pwr_lock),
				"Clock/power lock must be acquired");
	if (device->clocks_enabled) {
		dprintk(VIDC_DBG, "Clocks already enabled");
		return 0;
	}

	for (i = 0; i <= device->clk_gating_level; i++) {
		cl = &device->resources.clock[i];
		rc = clk_enable(cl->clk);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to enable clocks\n");
			goto fail_clk_enable;
		} else {
			dprintk(VIDC_DBG, "Clock: %s enabled\n", cl->name);
		}
	}
	device->clocks_enabled = 1;
	++device->clk_cnt;
	return 0;
fail_clk_enable:
	for (i--; i >= 0; i--) {
		cl = &device->resources.clock[i];
		usleep(100);
		clk_disable(cl->clk);
	}
	return rc;
}

static inline void venus_hfi_clk_disable(struct venus_hfi_device *device)
{
	int i, rc = 0;
	struct venus_core_clock *cl;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return;
	}
	WARN(!mutex_is_locked(&device->clk_pwr_lock),
			"Clock/power lock must be acquired");
	if (!device->clocks_enabled) {
		dprintk(VIDC_DBG, "Clocks already disabled");
		return;
	}

	/* We get better power savings if we lower the venus core clock to the
	 * lowest level before disabling it. */
	rc = clk_set_rate(device->resources.clock[VCODEC_CLK].clk,
			venus_hfi_get_clock_rate(
			&device->resources.clock[VCODEC_CLK], 0));
	if (rc)
		dprintk(VIDC_WARN, "Failed to set clock rate to min: %d\n", rc);

	for (i = 0; i <= device->clk_gating_level; i++) {
		cl = &device->resources.clock[i];
		usleep(100);
		clk_disable(cl->clk);
	}
	device->clocks_enabled = 0;
	--device->clk_cnt;
}

static DECLARE_COMPLETION(pc_prep_done);
static DECLARE_COMPLETION(release_resources_done);

static int venus_hfi_halt_axi(struct venus_hfi_device *device)
{
	u32 reg;
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid input: %p\n", device);
		return -EINVAL;
	}
	if (venus_hfi_clk_gating_off(device)) {
		dprintk(VIDC_ERR, "Failed to turn off clk gating\n");
		return -EIO;
	}
	/* Halt AXI and AXI OCMEM VBIF Access */
	reg = venus_hfi_read_register(device, VENUS_VBIF_AXI_HALT_CTRL0);
	reg |= VENUS_VBIF_AXI_HALT_CTRL0_HALT_REQ;
	venus_hfi_write_register(device, VENUS_VBIF_AXI_HALT_CTRL0, reg, 0);

	/* Request for AXI bus port halt */
	rc = readl_poll_timeout((u32)device->hal_data->register_base_addr
			+ VENUS_VBIF_AXI_HALT_CTRL1,
			reg, reg & VENUS_VBIF_AXI_HALT_CTRL1_HALT_ACK,
			POLL_INTERVAL_US,
			VENUS_VBIF_AXI_HALT_ACK_TIMEOUT_US);
	if (rc)
		dprintk(VIDC_WARN, "AXI bus port halt timeout\n");
	return rc;
}

static inline int venus_hfi_power_off(struct venus_hfi_device *device)
{
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	if (!device->power_enabled) {
		dprintk(VIDC_DBG, "Power already disabled");
		goto already_disabled;
	}

	/*Temporarily enable clocks to make TZ call.*/
	rc = venus_hfi_clk_enable(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable clocks before TZ call");
		return rc;
	}
	rc = venus_hfi_tzbsp_set_video_state(TZBSP_VIDEO_STATE_SUSPEND);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to suspend video core %d\n", rc);
		venus_hfi_clk_disable(device);
		return rc;
	}
	venus_hfi_clk_disable(device);
	venus_hfi_iommu_detach(device);
	rc = regulator_disable(device->gdsc);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to disable GDSC, %d", rc);
		return rc;
	}
	if (device->res->has_ocmem)
		venus_hfi_unvote_buses(device, DDR_MEM|OCMEM_MEM);
	else
		venus_hfi_unvote_buses(device, DDR_MEM);

	device->power_enabled = 0;
	--device->pwr_cnt;
	dprintk(VIDC_INFO, "entering power collapse\n");
already_disabled:
	return rc;
}

static inline int venus_hfi_power_on(struct venus_hfi_device *device)
{
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}

	if (device->res->has_ocmem)
		rc = venus_hfi_scale_buses(device, DDR_MEM|OCMEM_MEM);
	else
		rc = venus_hfi_scale_buses(device, DDR_MEM);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to scale buses");
		goto err_scale_buses;
	}

	rc = regulator_enable(device->gdsc);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable GDSC %d", rc);
		goto err_enable_gdsc;
	}

	rc = venus_hfi_iommu_attach(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to attach iommu after power on");
		goto err_iommu_attach;
	}

	rc = venus_hfi_clk_enable(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable clocks");
		goto err_enable_clk;
	}


	/*
	 * Re-program all of the registers that get reset as a result of
	 * regulator_disable() and _enable()
	 */
	venus_hfi_set_registers(device);

	venus_hfi_write_register(device, VIDC_UC_REGION_ADDR,
			(u32)device->iface_q_table.align_device_addr, 0);
	venus_hfi_write_register(device,
			VIDC_UC_REGION_SIZE, SHARED_QSIZE, 0);
	venus_hfi_write_register(device, VIDC_CPU_CS_SCIACMDARG2,
		(u32)device->iface_q_table.align_device_addr,
		device->iface_q_table.align_virtual_addr);

	if (!IS_ERR_OR_NULL(device->sfr.align_device_addr))
		venus_hfi_write_register(device, VIDC_SFR_ADDR,
				(u32)device->sfr.align_device_addr, 0);
	if (!IS_ERR_OR_NULL(device->qdss.align_device_addr))
		venus_hfi_write_register(device, VIDC_MMAP_ADDR,
				(u32)device->qdss.align_device_addr, 0);

	/* Reboot the firmware */
	rc = venus_hfi_tzbsp_set_video_state(TZBSP_VIDEO_STATE_RESUME);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to resume video core %d\n", rc);
		goto err_set_video_state;
	}

	/* Wait for boot completion */
	rc = venus_hfi_reset_core(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to reset venus core");
		goto err_reset_core;
	}
	/*
	 * write_lock is already acquired at this point, so to avoid
	 * recursive lock in cmdq_write function, call nolock version
	 * of alloc_ocmem
	 */
	WARN_ON(!mutex_is_locked(&device->write_lock));
	rc = __alloc_ocmem(device, device->ocmem_size, false);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate OCMEM");
		goto err_alloc_ocmem;
	}
	device->power_enabled = 1;
	++device->pwr_cnt;
	dprintk(VIDC_INFO, "resuming from power collapse\n");
	return rc;
err_alloc_ocmem:
err_reset_core:
	venus_hfi_tzbsp_set_video_state(TZBSP_VIDEO_STATE_SUSPEND);
err_set_video_state:
	venus_hfi_clk_disable(device);
err_enable_clk:
	venus_hfi_iommu_detach(device);
err_iommu_attach:
	regulator_disable(device->gdsc);
err_enable_gdsc:
	if (device->res->has_ocmem)
		venus_hfi_unvote_buses(device, DDR_MEM|OCMEM_MEM);
	else
		venus_hfi_unvote_buses(device, DDR_MEM);
err_scale_buses:
	return rc;
}

static int venus_hfi_power_enable(void *dev)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	mutex_lock(&device->clk_pwr_lock);
	if (!device->power_enabled)
		rc = venus_hfi_power_on(device);
	mutex_unlock(&device->clk_pwr_lock);

	return rc;
}

static void venus_hfi_pm_hndlr(struct work_struct *work);
static DECLARE_DELAYED_WORK(venus_hfi_pm_work, venus_hfi_pm_hndlr);

static inline int venus_hfi_clk_gating_off(struct venus_hfi_device *device)
{
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	if (device->clocks_enabled) {
		dprintk(VIDC_DBG, "Clocks are already enabled");
		goto already_enabled;
	}
	cancel_delayed_work(&venus_hfi_pm_work);
	if (!device->power_enabled) {
		/*This will enable clocks as well*/
		rc = venus_hfi_power_on(device);
		if (rc) {
			dprintk(VIDC_ERR, "Failed venus power on");
			goto fail_clk_power_on;
		}
	} else {
		rc = venus_hfi_clk_enable(device);
		if (rc) {
			dprintk(VIDC_ERR, "Failed venus clock enable");
			goto fail_clk_power_on;
		}
	        venus_hfi_write_register(device,
			        VIDC_WRAPPER_INTR_MASK, VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK, 0);
	}
already_enabled:
	device->clocks_enabled = 1;
fail_clk_power_on:
	return rc;
}

static unsigned long venus_hfi_get_clock_rate(struct venus_core_clock *clock,
	int num_mbs_per_sec)
{
	int num_rows = clock->count;
	struct load_freq_table *table = clock->load_freq_tbl;
	unsigned long ret = table[0].freq;
	int i;
	for (i = 0; i < num_rows; i++) {
		if (num_mbs_per_sec > table[i].load)
			break;
		ret = table[i].freq;
	}
	dprintk(VIDC_PROF, "Required clock rate = %lu\n", ret);
	return ret;
}

static int venus_hfi_scale_clocks(void *dev, int load)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid args: %p\n", device);
		return -EINVAL;
	}
	device->clk_load = load;
	rc = clk_set_rate(device->resources.clock[VCODEC_CLK].clk,
		venus_hfi_get_clock_rate(&device->resources.clock[VCODEC_CLK],
			load));
	if (rc)
		dprintk(VIDC_ERR, "Failed to set clock rate: %d\n", rc);
	return rc;
}

static int venus_hfi_iface_cmdq_write_nolock(struct venus_hfi_device *device,
					void *pkt)
{
	u32 rx_req_is_set = 0;
	struct vidc_iface_q_info *q_info;
	int result = -EPERM;

	if (!device || !pkt) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	WARN(!mutex_is_locked(&device->write_lock),
			"Cmd queue write lock must be acquired");
	q_info = &device->iface_queues[VIDC_IFACEQ_CMDQ_IDX];
	if (!q_info) {
		dprintk(VIDC_ERR, "cannot write to shared Q's");
		goto err_q_null;
	}
	if (!venus_hfi_write_queue(q_info, (u8 *)pkt, &rx_req_is_set)) {
		WARN(!mutex_is_locked(&device->clk_pwr_lock),
					"Clock/power lock must be acquired");
		result = venus_hfi_clk_gating_off(device);
		if (result) {
			dprintk(VIDC_ERR, "%s : Clock enable failed\n",
					__func__);
			goto err_q_write;
		}
		result = venus_hfi_scale_clocks(device, device->clk_load);
		if (result) {
			dprintk(VIDC_ERR, "Clock scaling failed\n");
			goto err_q_write;
		}
		if (rx_req_is_set)
			venus_hfi_write_register(
				device,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		result = 0;
	} else {
		dprintk(VIDC_ERR, "venus_hfi_iface_cmdq_write:queue_full");
	}
err_q_write:
err_q_null:
	return result;
}

static int venus_hfi_iface_msgq_read(struct venus_hfi_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct vidc_iface_q_info *q_info;

	if (!pkt) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	mutex_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_MSGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared MSG Q's");
		rc = -ENODATA;
		goto read_error_null;
	}
	q_info = &device->iface_queues[VIDC_IFACEQ_MSGQ_IDX];
	if (!venus_hfi_read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		mutex_lock(&device->clk_pwr_lock);
		rc = venus_hfi_clk_gating_off(device);
		if (rc) {
			dprintk(VIDC_ERR,
					"%s : Clock enable failed\n", __func__);
			mutex_unlock(&device->clk_pwr_lock);
			goto read_error;
		}
		if (tx_req_is_set)
			venus_hfi_write_register(
				device,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		rc = 0;
		mutex_unlock(&device->clk_pwr_lock);
	} else {
		dprintk(VIDC_INFO, "venus_hfi_iface_msgq_read:queue_empty");
		rc = -ENODATA;
	}
read_error:
read_error_null:
	mutex_unlock(&device->read_lock);
	return rc;
}

static int venus_hfi_iface_dbgq_read(struct venus_hfi_device *device, void *pkt)
{
	u32 tx_req_is_set = 0;
	int rc = 0;
	struct vidc_iface_q_info *q_info;

	if (!pkt) {
		dprintk(VIDC_ERR, "Invalid Params");
		return -EINVAL;
	}
	mutex_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_DBGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared DBG Q's");
		rc = -ENODATA;
		goto dbg_error_null;
	}
	q_info = &device->iface_queues[VIDC_IFACEQ_DBGQ_IDX];
	if (!venus_hfi_read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		mutex_lock(&device->clk_pwr_lock);
		rc = venus_hfi_clk_gating_off(device);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s : Clock enable failed\n", __func__);
			mutex_unlock(&device->clk_pwr_lock);
			goto dbg_error;
		}
		if (tx_req_is_set)
			venus_hfi_write_register(
				device,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		rc = 0;
		mutex_unlock(&device->clk_pwr_lock);
	} else {
		dprintk(VIDC_INFO, "venus_hfi_iface_dbgq_read:queue_empty");
		rc = -ENODATA;
	}
dbg_error:
dbg_error_null:
	mutex_unlock(&device->read_lock);
	return rc;
}

static void venus_hfi_set_queue_hdr_defaults(struct hfi_queue_header *q_hdr)
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

static void venus_hfi_interface_queues_release(struct venus_hfi_device *device)
{
	int i;
	struct hfi_mem_map_table *qdss;
	struct hfi_mem_map *mem_map;
	int num_entries = sizeof(venus_qdss_entries)/(2 * sizeof(u32));
	int domain, partition;

	mutex_lock(&device->write_lock);
	mutex_lock(&device->read_lock);
	if (device->qdss.mem_data) {
		qdss = (struct hfi_mem_map_table *)
			device->qdss.align_virtual_addr;
		qdss->mem_map_num_entries = num_entries;
		qdss->mem_map_table_base_addr =
			(u32 *)((u32)device->qdss.align_device_addr +
				sizeof(struct hfi_mem_map_table));
		mem_map = (struct hfi_mem_map *)(qdss + 1);
		msm_smem_get_domain_partition(device->hal_client, 0,
			HAL_BUFFER_INTERNAL_CMD_QUEUE, &domain, &partition);
		for (i = 0; i < num_entries; i++) {
			msm_iommu_unmap_contig_buffer(
				(unsigned long)(mem_map[i].virtual_addr),
				domain, partition, SZ_4K);
		}
		venus_hfi_free(device, device->qdss.mem_data);
	}
	venus_hfi_free(device, device->iface_q_table.mem_data);
	venus_hfi_free(device, device->sfr.mem_data);

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
	mutex_unlock(&device->read_lock);
	mutex_unlock(&device->write_lock);
}
static int venus_hfi_get_qdss_iommu_virtual_addr(struct hfi_mem_map *mem_map,
						int domain, int partition)
{
	int i;
	int rc = 0;
	unsigned long iova = 0;
	int num_entries = sizeof(venus_qdss_entries)/(2 * sizeof(u32));

	for (i = 0; i < num_entries; i++) {
		rc = msm_iommu_map_contig_buffer(venus_qdss_entries[i][0],
			domain, partition, venus_qdss_entries[i][1],
			SZ_4K, 0, &iova);
		if (rc) {
			dprintk(VIDC_ERR,
				"IOMMU QDSS mapping failed for addr 0x%x",
				venus_qdss_entries[i][0]);
			rc = -ENOMEM;
			break;
		}
		mem_map[i].virtual_addr = (u32) iova;
		mem_map[i].physical_addr = venus_qdss_entries[i][0];
		mem_map[i].size = venus_qdss_entries[i][1];
		mem_map[i].attr = 0x0;
	}
	if (i < num_entries) {
		dprintk(VIDC_ERR,
			"IOMMU QDSS mapping failed, Freeing entries %d", i);
		for (--i; i >= 0; i--) {
			msm_iommu_unmap_contig_buffer(
				(unsigned long)(mem_map[i].virtual_addr),
				domain, partition, SZ_4K);
		}
	}
	return rc;
}

static int venus_hfi_interface_queues_init(struct venus_hfi_device *dev)
{
	struct hfi_queue_table_header *q_tbl_hdr;
	struct hfi_queue_header *q_hdr;
	u8 i;
	int rc = 0;
	struct hfi_mem_map_table *qdss;
	struct hfi_mem_map *mem_map;
	struct vidc_iface_q_info *iface_q;
	struct hfi_sfr_struct *vsfr;
	struct vidc_mem_addr *mem_addr;
	int offset = 0;
	int num_entries = sizeof(venus_qdss_entries)/(2 * sizeof(u32));
	int domain, partition;
	mem_addr = &dev->mem_addr;
	rc = venus_hfi_alloc(dev, (void *) mem_addr,
			QUEUE_SIZE, 1, 0,
			HAL_BUFFER_INTERNAL_CMD_QUEUE);
	if (rc) {
		dprintk(VIDC_ERR, "iface_q_table_alloc_fail");
		goto fail_alloc_queue;
	}
	dev->iface_q_table.align_virtual_addr = mem_addr->align_virtual_addr;
	dev->iface_q_table.align_device_addr = mem_addr->align_device_addr;
	dev->iface_q_table.mem_size = VIDC_IFACEQ_TABLE_SIZE;
	dev->iface_q_table.mem_data = mem_addr->mem_data;
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
		venus_hfi_set_queue_hdr_defaults(iface_q->q_hdr);
	}

	rc = venus_hfi_alloc(dev, (void *) mem_addr,
			QDSS_SIZE, 1, 0,
			HAL_BUFFER_INTERNAL_CMD_QUEUE);
	if (rc) {
		dprintk(VIDC_WARN,
			"qdss_alloc_fail: QDSS messages logging will not work");
		dev->qdss.align_device_addr = NULL;
	} else {
		dev->qdss.align_device_addr = mem_addr->align_device_addr;
		dev->qdss.align_virtual_addr = mem_addr->align_virtual_addr;
		dev->qdss.mem_size = QDSS_SIZE;
		dev->qdss.mem_data = mem_addr->mem_data;
	}
	rc = venus_hfi_alloc(dev, (void *) mem_addr,
			SFR_SIZE, 1, 0,
			HAL_BUFFER_INTERNAL_CMD_QUEUE);
	if (rc) {
		dprintk(VIDC_WARN, "sfr_alloc_fail: SFR not will work");
		dev->sfr.align_device_addr = NULL;
	} else {
		dev->sfr.align_device_addr = mem_addr->align_device_addr;
		dev->sfr.align_virtual_addr = mem_addr->align_virtual_addr;
		dev->sfr.mem_size = SFR_SIZE;
		dev->sfr.mem_data = mem_addr->mem_data;
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

	venus_hfi_write_register(dev,
			VIDC_UC_REGION_ADDR,
			(u32) dev->iface_q_table.align_device_addr, 0);
	venus_hfi_write_register(dev,
			VIDC_UC_REGION_SIZE, SHARED_QSIZE, 0);
	venus_hfi_write_register(dev,
		VIDC_CPU_CS_SCIACMDARG2,
		(u32) dev->iface_q_table.align_device_addr,
		dev->iface_q_table.align_virtual_addr);
	venus_hfi_write_register(dev,
		VIDC_CPU_CS_SCIACMDARG1, 0x01,
		dev->iface_q_table.align_virtual_addr);

	qdss = (struct hfi_mem_map_table *) dev->qdss.align_virtual_addr;
	qdss->mem_map_num_entries = num_entries;
	qdss->mem_map_table_base_addr =
		(u32 *)	((u32)dev->qdss.align_device_addr +
		sizeof(struct hfi_mem_map_table));
	mem_map = (struct hfi_mem_map *)(qdss + 1);
	msm_smem_get_domain_partition(dev->hal_client, 0,
		HAL_BUFFER_INTERNAL_CMD_QUEUE, &domain, &partition);
	rc = venus_hfi_get_qdss_iommu_virtual_addr(mem_map, domain, partition);
	if (rc) {
		dprintk(VIDC_ERR,
			"IOMMU mapping failed, Freeing qdss memdata");
		venus_hfi_free(dev, dev->qdss.mem_data);
		dev->qdss.mem_data = NULL;
	}
	if (!IS_ERR_OR_NULL(dev->qdss.align_device_addr))
		venus_hfi_write_register(dev,
			VIDC_MMAP_ADDR,
			(u32) dev->qdss.align_device_addr, 0);

	vsfr = (struct hfi_sfr_struct *) dev->sfr.align_virtual_addr;
	vsfr->bufSize = SFR_SIZE;
	if (!IS_ERR_OR_NULL(dev->sfr.align_device_addr))
		venus_hfi_write_register(dev,
			VIDC_SFR_ADDR, (u32)dev->sfr.align_device_addr , 0);
	return 0;
fail_alloc_queue:
	return -ENOMEM;
}

static int venus_hfi_sys_set_debug(struct venus_hfi_device *device, int debug)
{
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	int rc = 0;
	struct hfi_cmd_sys_set_property_packet *pkt =
		(struct hfi_cmd_sys_set_property_packet *) &packet;
	rc = create_pkt_cmd_sys_debug_config(pkt, debug);
	if (rc) {
		dprintk(VIDC_WARN,
			"Debug mode setting to FW failed\n");
		return -ENOTEMPTY;
	}
	if (venus_hfi_iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}
static int venus_hfi_sys_set_idle_message(struct venus_hfi_device *device,
	int enable)
{
	u8 packet[VIDC_IFACEQ_VAR_SMALL_PKT_SIZE];
	struct hfi_cmd_sys_set_property_packet *pkt =
		(struct hfi_cmd_sys_set_property_packet *) &packet;
	create_pkt_cmd_sys_idle_indicator(pkt, enable);
	if (venus_hfi_iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}


static int venus_hfi_core_init(void *device)
{
	struct hfi_cmd_sys_init_packet pkt;
	struct hfi_cmd_sys_get_property_packet version_pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "Invalid device");
		return -ENODEV;
	}

	dev->intr_status = 0;
	INIT_LIST_HEAD(&dev->sess_head);
	venus_hfi_set_registers(dev);

	if (!dev->hal_client) {
		dev->hal_client = msm_smem_new_client(SMEM_ION, dev->res);
		if (dev->hal_client == NULL) {
			dprintk(VIDC_ERR, "Failed to alloc ION_Client");
			rc = -ENODEV;
			goto err_core_init;
		}

		dprintk(VIDC_DBG, "Dev_Virt: 0x%x, Reg_Virt: 0x%x",
		dev->hal_data->device_base_addr,
		(u32) dev->hal_data->register_base_addr);

		rc = venus_hfi_interface_queues_init(dev);
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
	enable_irq(dev->hal_data->irq);
	venus_hfi_write_register(dev,
		VIDC_CTRL_INIT, 0x1, 0);
	rc = venus_hfi_core_start_cpu(dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to start core");
		rc = -ENODEV;
		goto err_core_init;
	}

	rc = create_pkt_cmd_sys_init(&pkt, HFI_VIDEO_ARCH_OX);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to create sys init pkt");
		goto err_core_init;
	}
	if (venus_hfi_iface_cmdq_write(dev, &pkt)) {
		rc = -ENOTEMPTY;
		goto err_core_init;
	}
	rc = create_pkt_cmd_sys_image_version(&version_pkt);
	if (rc || venus_hfi_iface_cmdq_write(dev, &version_pkt))
		dprintk(VIDC_WARN, "Failed to send image version pkt to f/w");

	return rc;
err_core_init:
	disable_irq_nosync(dev->hal_data->irq);
	return rc;
}

static int venus_hfi_core_release(void *device)
{
	struct venus_hfi_device *dev;
	int rc = 0;
	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device");
		return -ENODEV;
	}
	if (dev->hal_client) {
		mutex_lock(&dev->clk_pwr_lock);
		rc = venus_hfi_clk_gating_off(device);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s : Clock enable failed\n", __func__);
			mutex_unlock(&dev->clk_pwr_lock);
			return -EIO;
		}
		venus_hfi_write_register(dev,
				VIDC_CPU_CS_SCIACMDARG3, 0, 0);
		if (!(dev->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK))
			disable_irq_nosync(dev->hal_data->irq);
		dev->intr_status = 0;
		mutex_unlock(&dev->clk_pwr_lock);
	}
	dprintk(VIDC_INFO, "HAL exited\n");
	return 0;
}

static int venus_hfi_get_q_size(struct venus_hfi_device *dev,
	unsigned int q_index)
{
	struct hfi_queue_header *queue;
	struct vidc_iface_q_info *q_info;
	u32 write_ptr, read_ptr;
	u32 rc = 0;
	if (q_index >= VIDC_IFACEQ_NUMQ) {
		dprintk(VIDC_ERR, "Invalid q index: %d\n", q_index);
		return -ENOENT;
	}

	WARN(!mutex_is_locked(&dev->write_lock),
			"Cmdq write lock should be acquired");
	q_info = &dev->iface_queues[q_index];
	if (!q_info) {
		dprintk(VIDC_ERR, "cannot read shared Q's");
		return -ENOENT;
	}
	queue = (struct hfi_queue_header *) q_info->q_hdr;
	if (!queue) {
		dprintk(VIDC_ERR, "queue not present");
		return -ENOENT;
	}
	write_ptr = (u32)queue->qhdr_write_idx;
	read_ptr = (u32)queue->qhdr_read_idx;
	rc = read_ptr - write_ptr;
	return rc;
}

static inline void venus_hfi_clk_gating_on(struct venus_hfi_device *device)
{
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return;
	}
	if (!device->clocks_enabled) {
		dprintk(VIDC_DBG, "Clocks are already disabled");
		goto already_disabled;
	}
	/*SYS Idle should be last message so mask any further interrupts
	 * until clocks are enabled again.*/
	if (!venus_hfi_get_q_size(device, VIDC_IFACEQ_MSGQ_IDX)) {
		venus_hfi_write_register(device,
				VIDC_WRAPPER_INTR_MASK,
				VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK |
				VIDC_WRAPPER_INTR_MASK_A2HCPU_BMSK, 0);
	}
	venus_hfi_clk_disable(device);
	if (!queue_delayed_work(device->venus_pm_workq, &venus_hfi_pm_work,
			msecs_to_jiffies(msm_vidc_pwr_collapse_delay)))
		dprintk(VIDC_DBG, "PM work already scheduled\n");
already_disabled:
	device->clocks_enabled = 0;
}

static void venus_hfi_core_clear_interrupt(struct venus_hfi_device *device)
{
	u32 intr_status = 0;
	int rc = 0;

	if (!device->callback)
		return;

	mutex_lock(&device->write_lock);
	mutex_lock(&device->clk_pwr_lock);
	rc = venus_hfi_clk_gating_off(device);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s : Clock enable failed\n", __func__);
		goto err_clk_gating_off;
	}
	intr_status = venus_hfi_read_register(
			device,
			VIDC_WRAPPER_INTR_STATUS);

	if ((intr_status & VIDC_WRAPPER_INTR_STATUS_A2H_BMSK) ||
		(intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK) ||
		(intr_status &
			VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK)) {
		device->intr_status |= intr_status;
		dprintk(VIDC_DBG, "INTERRUPT for device: 0x%x: "
			"times: %d interrupt_status: %d",
			(u32) device, ++device->reg_count, intr_status);
	} else {
		dprintk(VIDC_INFO, "SPURIOUS_INTR for device: 0x%x: "
			"times: %d interrupt_status: %d",
			(u32) device, ++device->spur_count, intr_status);
	}
	venus_hfi_write_register(device,
			VIDC_CPU_CS_A2HSOFTINTCLR, 1, 0);
	venus_hfi_write_register(device,
			VIDC_WRAPPER_INTR_CLEAR, intr_status, 0);
	dprintk(VIDC_DBG, "Cleared WRAPPER/A2H interrupt");
err_clk_gating_off:
	mutex_unlock(&device->clk_pwr_lock);
	mutex_unlock(&device->write_lock);
}

static int venus_hfi_core_ping(void *device)
{
	struct hfi_cmd_sys_ping_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

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

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_core_trigger_ssr(void *device,
	enum hal_ssr_trigger_type type)
{
	struct hfi_cmd_sys_test_ssr_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device");
		return -ENODEV;
	}

	rc = create_pkt_ssr_cmd(type, &pkt);
	if (rc) {
		dprintk(VIDC_ERR, "core_ping: failed to create packet");
		goto err_create_pkt;
	}

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_session_set_property(void *sess,
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

	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		return -ENOTEMPTY;

	return rc;
}

static int venus_hfi_session_get_property(void *sess,
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
	case HAL_PARAM_BUFFER_ALLOC_MODE:
		break;
	case HAL_PARAM_VDEC_FRAME_ASSEMBLY:
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

static void *venus_hfi_session_init(void *device, u32 session_id,
		enum hal_domain session_type, enum hal_video_codec codec_type)
{
	struct hfi_cmd_sys_session_init_packet pkt;
	struct hal_session *new_session;
	struct venus_hfi_device *dev;

	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device");
		return NULL;
	}

	new_session = (struct hal_session *)
		kzalloc(sizeof(struct hal_session), GFP_KERNEL);
	if (!new_session) {
		dprintk(VIDC_ERR, "new session fail: Out of memory\n");
		return NULL;
	}
	new_session->session_id = (u32) session_id;
	if (session_type == 1)
		new_session->is_decoder = 0;
	else if (session_type == 2)
		new_session->is_decoder = 1;
	new_session->device = dev;

	mutex_lock(&dev->session_lock);
	list_add_tail(&new_session->list, &dev->sess_head);
	mutex_unlock(&dev->session_lock);

	if (create_pkt_cmd_sys_session_init(&pkt, (u32)new_session,
			session_type, codec_type)) {
		dprintk(VIDC_ERR, "session_init: failed to create packet");
		goto err_session_init_fail;
	}

	if (venus_hfi_sys_set_debug(dev, msm_fw_debug))
		dprintk(VIDC_ERR, "Setting fw_debug msg ON failed");
	if (venus_hfi_sys_set_idle_message(dev, msm_fw_low_power_mode))
		dprintk(VIDC_ERR, "Setting idle response ON failed");
	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		goto err_session_init_fail;
	return (void *) new_session;

err_session_init_fail:
	kfree(new_session);
	return NULL;
}

static int venus_hfi_send_session_cmd(void *session_id,
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

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_session_end(void *session)
{
	return venus_hfi_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_END);
}

static int venus_hfi_session_abort(void *session)
{
	return venus_hfi_send_session_cmd(session,
		HFI_CMD_SYS_SESSION_ABORT);
}

static int venus_hfi_session_clean(void *session)
{
	struct hal_session *sess_close;
	if (!session) {
		dprintk(VIDC_ERR, "Invalid Params %s", __func__);
		return -EINVAL;
	}
	sess_close = session;
	dprintk(VIDC_DBG, "deleted the session: 0x%p",
			sess_close);
	mutex_lock(&((struct venus_hfi_device *)
			sess_close->device)->session_lock);
	list_del(&sess_close->list);
	mutex_unlock(&((struct venus_hfi_device *)
			sess_close->device)->session_lock);
	kfree(sess_close);
	return 0;
}

static int venus_hfi_session_set_buffers(void *sess,
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
	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_release_buffers(void *sess,
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
	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_load_res(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_LOAD_RESOURCES);
}

static int venus_hfi_session_release_res(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_RELEASE_RESOURCES);
}

static int venus_hfi_session_start(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_START);
}

static int venus_hfi_session_stop(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_STOP);
}

static int venus_hfi_session_suspend(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_SUSPEND);
}

static int venus_hfi_session_resume(void *sess)
{
	return venus_hfi_send_session_cmd(sess,
		HFI_CMD_SESSION_RESUME);
}

static int venus_hfi_session_etb(void *sess,
				struct vidc_frame_data *input_frame)
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
		if (venus_hfi_iface_cmdq_write(session->device, &pkt))
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
		if (venus_hfi_iface_cmdq_write(session->device, &pkt))
			rc = -ENOTEMPTY;
	}
err_create_pkt:
	return rc;
}

static int venus_hfi_session_ftb(void *sess,
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

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_parse_seq_hdr(void *sess,
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

	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_get_seq_hdr(void *sess,
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

	if (venus_hfi_iface_cmdq_write(session->device, pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_get_buf_req(void *sess)
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

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_session_flush(void *sess, enum hal_flush flush_mode)
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

	if (venus_hfi_iface_cmdq_write(session->device, &pkt))
		rc = -ENOTEMPTY;
err_create_pkt:
	return rc;
}

static int venus_hfi_check_core_registered(
	struct hal_device_data core, u32 fw_addr,
	u32 reg_addr, u32 reg_size, u32 irq)
{
	struct venus_hfi_device *device;
	struct list_head *curr, *next;

	if (core.dev_count) {
		list_for_each_safe(curr, next, &core.dev_head) {
			device = list_entry(curr,
				struct venus_hfi_device, list);
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

static void venus_hfi_process_sys_watchdog_timeout(
				struct venus_hfi_device *device)
{
	struct msm_vidc_cb_cmd_done cmd_done;
	memset(&cmd_done, 0, sizeof(struct msm_vidc_cb_cmd_done));
	cmd_done.device_id = device->device_id;
	device->callback(SYS_WATCHDOG_TIMEOUT, &cmd_done);
}

static int venus_hfi_core_pc_prep(void *device)
{
	struct hfi_cmd_sys_pc_prep_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

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

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		rc = -ENOTEMPTY;

err_create_pkt:
	return rc;
}

static int venus_hfi_unset_free_ocmem(struct venus_hfi_device *device)
{
	int rc = 0;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid param: %p\n", device);
		return -EINVAL;
	}

	init_completion(&release_resources_done);
	rc = venus_hfi_unset_ocmem(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to unset OCMEM during PC %d\n", rc);
		goto ocmem_unset_failed;
	}
	rc = wait_for_completion_timeout(&release_resources_done,
			msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR,
				"Wait interrupted or timeout for RELEASE_RESOURCES: %d\n",
				rc);
		rc = -EIO;
		goto release_resources_failed;
	}

	rc = venus_hfi_free_ocmem(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to free OCMEM during PC\n");
		goto ocmem_free_failed;
	}
	return rc;

ocmem_free_failed:
	venus_hfi_alloc_ocmem(device, device->ocmem_size);
release_resources_failed:
ocmem_unset_failed:
	return rc;
}

static int venus_hfi_prepare_pc(struct venus_hfi_device *device)
{
	int rc = 0;
	init_completion(&pc_prep_done);
	rc = venus_hfi_core_pc_prep(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to prepare venus for power off");
		goto err_pc_prep;
	}
	rc = wait_for_completion_timeout(&pc_prep_done,
			msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR,
				"Wait interrupted or timeout for PC_PREP_DONE: %d\n",
				rc);
		rc = -EIO;
		goto err_pc_prep;
	}
	rc = 0;
err_pc_prep:
	return rc;
}

static void venus_hfi_pm_hndlr(struct work_struct *work)
{
	int rc = 0;
	struct venus_hfi_device *device = list_first_entry(
			&hal_ctxt.dev_head, struct venus_hfi_device, list);
	mutex_lock(&device->clk_pwr_lock);
	if (device->clocks_enabled || !device->power_enabled) {
		dprintk(VIDC_DBG,
				"Clocks status: %d, Power status: %d, ignore power off\n",
				device->clocks_enabled, device->power_enabled);
		goto clks_enabled;
	}
	mutex_unlock(&device->clk_pwr_lock);

	rc = venus_hfi_unset_free_ocmem(device);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to unset and free OCMEM for PC %d\n",
				rc);
		return;
	}

	rc = venus_hfi_prepare_pc(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to prepare for PC %d\n", rc);
		venus_hfi_alloc_ocmem(device, device->ocmem_size);
		return;
	}

	mutex_lock(&device->clk_pwr_lock);
	if (device->clocks_enabled) {
		dprintk(VIDC_ERR,
				"Clocks are still enabled after PC_PREP_DONE, ignore power off");
		goto clks_enabled;
	}

	rc = venus_hfi_power_off(device);
	if (rc)
		dprintk(VIDC_ERR, "Failed venus power off");
clks_enabled:
	mutex_unlock(&device->clk_pwr_lock);
}

static int venus_hfi_try_clk_gating(struct venus_hfi_device *device)
{
	int rc = 0;
	u32 ctrl_status = 0;
	if (!device) {
		dprintk(VIDC_ERR, "invalid device");
		return -ENODEV;
	}
	mutex_lock(&device->write_lock);
	mutex_lock(&device->clk_pwr_lock);
	rc = venus_hfi_get_q_size(device, VIDC_IFACEQ_CMDQ_IDX);
	ctrl_status = venus_hfi_read_register(
		device,
		VIDC_CPU_CS_SCIACMDARG0);
	dprintk(VIDC_DBG,
			"venus_hfi_try_clk_gating - rc %d, ctrl_status 0x%x",
			rc, ctrl_status);
	if (((ctrl_status & VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK)
		|| (ctrl_status & VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY))
			&& !rc)
		venus_hfi_clk_gating_on(device);
	else
		dprintk(VIDC_DBG, "Ignore clock gating");
	mutex_unlock(&device->clk_pwr_lock);
	mutex_unlock(&device->write_lock);
	return rc;
}
static void venus_hfi_process_msg_event_notify(
	struct venus_hfi_device *device, void *packet)
{
	struct hfi_sfr_struct *vsfr = NULL;
	struct hfi_msg_event_notify_packet *event_pkt;
	struct vidc_hal_msg_pkt_hdr *msg_hdr;

	msg_hdr = (struct vidc_hal_msg_pkt_hdr *)packet;
	event_pkt =
		(struct hfi_msg_event_notify_packet *)msg_hdr;
	if (event_pkt && event_pkt->event_id ==
		HFI_EVENT_SYS_ERROR) {
		vsfr = (struct hfi_sfr_struct *)
				device->sfr.align_virtual_addr;
		if (vsfr)
			dprintk(VIDC_ERR, "SFR Message from FW : %s",
				vsfr->rg_data);
	}
}
static void venus_hfi_response_handler(struct venus_hfi_device *device)
{
	u8 packet[VIDC_IFACEQ_MED_PKT_SIZE];
	u32 rc = 0;
	struct hfi_sfr_struct *vsfr = NULL;
	dprintk(VIDC_INFO, "#####venus_hfi_response_handler#####\n");
	if (device) {
		if ((device->intr_status &
			VIDC_WRAPPER_INTR_CLEAR_A2HWD_BMSK)) {
			dprintk(VIDC_ERR, "Received: Watchdog timeout %s",
				__func__);
			vsfr = (struct hfi_sfr_struct *)
					device->sfr.align_virtual_addr;
			if (vsfr)
				dprintk(VIDC_ERR,
					"SFR Message from FW : %s",
						vsfr->rg_data);
			venus_hfi_process_sys_watchdog_timeout(device);
		}

		while (!venus_hfi_iface_msgq_read(device, packet)) {
			rc = hfi_process_msg_packet(device->callback,
				device->device_id,
				(struct vidc_hal_msg_pkt_hdr *) packet,
				&device->sess_head, &device->session_lock);
			if (rc == HFI_MSG_EVENT_NOTIFY) {
				venus_hfi_process_msg_event_notify(
					device, (void *)packet);
			} else if (rc == HFI_MSG_SYS_RELEASE_RESOURCE) {
				dprintk(VIDC_DBG,
					"Received HFI_MSG_SYS_RELEASE_RESOURCE\n");
				complete(&release_resources_done);
			}
		}
		while (!venus_hfi_iface_dbgq_read(device, packet)) {
			struct hfi_msg_sys_debug_packet *pkt =
				(struct hfi_msg_sys_debug_packet *) packet;
			dprintk(VIDC_FW, "FW-SAYS: %s", pkt->rg_msg_data);
		}
		switch (rc) {
		case HFI_MSG_SYS_IDLE:
			dprintk(VIDC_DBG,
					"Received HFI_MSG_SYS_IDLE\n");
			rc = venus_hfi_try_clk_gating(device);
			break;
		case HFI_MSG_SYS_PC_PREP_DONE:
			dprintk(VIDC_DBG,
					"Received HFI_MSG_SYS_PC_PREP_DONE\n");
			rc = venus_hfi_try_clk_gating(device);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed clk gating after PC_PREP_DONE\n");
			complete(&pc_prep_done);
			break;
		}
	} else {
		dprintk(VIDC_ERR, "SPURIOUS_INTERRUPT");
	}
}

static void venus_hfi_core_work_handler(struct work_struct *work)
{
	struct venus_hfi_device *device = list_first_entry(
		&hal_ctxt.dev_head, struct venus_hfi_device, list);

	dprintk(VIDC_INFO, " GOT INTERRUPT () ");
	if (!device->callback) {
		dprintk(VIDC_ERR, "No interrupt callback function: %p\n",
				device);
		return;
	}
	venus_hfi_core_clear_interrupt(device);
	venus_hfi_response_handler(device);
	if (!(device->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK))
		enable_irq(device->hal_data->irq);
}
static DECLARE_WORK(venus_hfi_work, venus_hfi_core_work_handler);

static irqreturn_t venus_hfi_isr(int irq, void *dev)
{
	struct venus_hfi_device *device = dev;
	dprintk(VIDC_INFO, "vidc_hal_isr() %d ", irq);
	disable_irq_nosync(irq);
	queue_work(device->vidc_workq, &venus_hfi_work);
	dprintk(VIDC_INFO, "vidc_hal_isr() %d ", irq);
	return IRQ_HANDLED;
}

static int venus_hfi_init_regs_and_interrupts(
		struct venus_hfi_device *device,
		struct msm_vidc_platform_resources *res)
{
	struct hal_data *hal = NULL;
	int rc = 0;

	device->base_addr = res->fw_base_addr;
	device->register_base = res->register_base;
	device->register_size = res->register_size;
	device->irq = res->irq;

	rc = venus_hfi_check_core_registered(hal_ctxt, device->base_addr,
			device->register_base, device->register_size,
			device->irq);
	if (!rc) {
		dprintk(VIDC_ERR, "Core present/Already added");
		rc = -EEXIST;
		goto err_core_init;
	}

	dprintk(VIDC_DBG, "HAL_DATA will be assigned now");
	hal = (struct hal_data *)
		kzalloc(sizeof(struct hal_data), GFP_KERNEL);
	if (!hal) {
		dprintk(VIDC_ERR, "Failed to alloc");
		rc = -ENOMEM;
		goto err_core_init;
	}
	hal->irq = device->irq;
	hal->device_base_addr = device->base_addr;
	hal->register_base_addr =
		ioremap_nocache(device->register_base, device->register_size);
	if (!hal->register_base_addr) {
		dprintk(VIDC_ERR,
			"could not map reg addr %d of size %d",
			device->register_base, device->register_size);
		goto error_irq_fail;
	}

	device->hal_data = hal;
	rc = request_irq(device->irq, venus_hfi_isr, IRQF_TRIGGER_HIGH,
			"msm_vidc", device);
	if (unlikely(rc)) {
		dprintk(VIDC_ERR, "() :request_irq failed\n");
		goto error_irq_fail;
	}
	disable_irq_nosync(device->irq);
	return rc;

error_irq_fail:
	kfree(hal);
err_core_init:
	return rc;

}

static inline int venus_hfi_init_clocks(struct msm_vidc_platform_resources *res,
		struct venus_hfi_device *device)
{
	struct venus_core_clock *cl;
	int i;
	int rc = 0;
	struct venus_core_clock *clock;
	if (!res || !device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	clock = device->resources.clock;
	strlcpy(clock[VCODEC_CLK].name, "core_clk",
		sizeof(clock[VCODEC_CLK].name));
	strlcpy(clock[VCODEC_AHB_CLK].name, "iface_clk",
		sizeof(clock[VCODEC_AHB_CLK].name));
	strlcpy(clock[VCODEC_AXI_CLK].name, "bus_clk",
		sizeof(clock[VCODEC_AXI_CLK].name));

	if (res->has_ocmem) {
		strlcpy(clock[VCODEC_OCMEM_CLK].name, "mem_clk",
			sizeof(clock[VCODEC_OCMEM_CLK].name));
	}

	clock[VCODEC_CLK].count = res->load_freq_tbl_size;
	memcpy((void *)clock[VCODEC_CLK].load_freq_tbl, res->load_freq_tbl,
		clock[VCODEC_CLK].count * sizeof(*res->load_freq_tbl));

	dprintk(VIDC_DBG, "count = %d\n", clock[VCODEC_CLK].count);
	if (!clock[VCODEC_CLK].count) {
		dprintk(VIDC_ERR, "Failed to read clock frequency\n");
		goto fail_init_clocks;
	}
	for (i = 0; i <	clock[VCODEC_CLK].count; i++) {
		dprintk(VIDC_DBG,
				"load = %d, freq = %d\n",
				clock[VCODEC_CLK].load_freq_tbl[i].load,
				clock[VCODEC_CLK].load_freq_tbl[i].freq
			  );
	}

	for (i = 0; i < VCODEC_MAX_CLKS; i++) {
		if (i == VCODEC_OCMEM_CLK && !res->has_ocmem)
			continue;
		cl = &device->resources.clock[i];
		if (!cl->clk) {
			cl->clk = devm_clk_get(&res->pdev->dev, cl->name);
			if (IS_ERR_OR_NULL(cl->clk)) {
				dprintk(VIDC_ERR,
					"Failed to get clock: %s\n", cl->name);
				rc = PTR_ERR(cl->clk);
				break;
			}
		}
	}

	if (i < VCODEC_MAX_CLKS) {
		for (--i; i >= 0; i--) {
			if (i == VCODEC_OCMEM_CLK && !res->has_ocmem)
				continue;
			cl = &device->resources.clock[i];
			clk_put(cl->clk);
		}
	}
fail_init_clocks:
	return rc;
}

static inline void venus_hfi_deinit_clocks(struct venus_hfi_device *device)
{
	int i;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid args\n");
		return;
	}

	for (i = 0; i < VCODEC_MAX_CLKS; i++) {
		if (i == VCODEC_OCMEM_CLK && !device->res->has_ocmem)
			continue;
		clk_put(device->resources.clock[i].clk);
	}
}
static inline void venus_hfi_disable_clks(struct venus_hfi_device *device)
{
	int i;
	struct venus_core_clock *cl;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return;
	}
	mutex_lock(&device->clk_pwr_lock);
	if (device->clocks_enabled) {
		for (i = VCODEC_CLK; i < VCODEC_MAX_CLKS; i++) {
			if (i == VCODEC_OCMEM_CLK && !device->res->has_ocmem)
				continue;
			cl = &device->resources.clock[i];
			usleep(100);
			clk_disable(cl->clk);
		}
	} else {
		for (i = device->clk_gating_level + 1;
			i < VCODEC_MAX_CLKS; i++) {
			cl = &device->resources.clock[i];
			usleep(100);
			clk_disable(cl->clk);
		}
	}
	for (i = VCODEC_CLK; i < VCODEC_MAX_CLKS; i++) {
		if (i == VCODEC_OCMEM_CLK && !device->res->has_ocmem)
			continue;
		cl = &device->resources.clock[i];
		clk_unprepare(cl->clk);
	}
	device->clocks_enabled = 0;
	--device->clk_cnt;
	mutex_unlock(&device->clk_pwr_lock);
}
static inline int venus_hfi_enable_clks(struct venus_hfi_device *device)
{
	int i = 0;
	struct venus_core_clock *cl;
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	mutex_lock(&device->clk_pwr_lock);
	for (i = VCODEC_CLK; i < VCODEC_MAX_CLKS; i++) {
		if (i == VCODEC_OCMEM_CLK && !device->res->has_ocmem)
			continue;
		cl = &device->resources.clock[i];
		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to enable clocks\n");
			goto fail_clk_enable;
		} else {
			dprintk(VIDC_DBG, "Clock: %s enabled\n", cl->name);
		}
	}
	device->clocks_enabled = 1;
	++device->clk_cnt;
	mutex_unlock(&device->clk_pwr_lock);
	return rc;
fail_clk_enable:
	for (; i >= 0; i--) {
		cl = &device->resources.clock[i];
		usleep(100);
		clk_disable_unprepare(cl->clk);
	}
	mutex_unlock(&device->clk_pwr_lock);
	return rc;
}
static int venus_hfi_register_iommu_domains(struct venus_hfi_device *device,
					struct msm_vidc_platform_resources *res)
{
	struct iommu_domain *domain;
	int rc = 0, i = 0;
	struct iommu_set *iommu_group_set;
	struct iommu_info *iommu_map;

	if (!device || !res)
		return -EINVAL;

	iommu_group_set = &device->res->iommu_group_set;

	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		iommu_map->group = iommu_group_find(iommu_map->name);
		if (!iommu_map->group) {
			dprintk(VIDC_DBG, "Failed to find group :%s\n",
				iommu_map->name);
			rc = -EPROBE_DEFER;
			goto fail_group;
		}
		domain = iommu_group_get_iommudata(iommu_map->group);
		if (!domain) {
			dprintk(VIDC_ERR,
				"Failed to get domain data for group %p",
				iommu_map->group);
			rc = -EINVAL;
			goto fail_group;
		}
		iommu_map->domain = msm_find_domain_no(domain);
		if (iommu_map->domain < 0) {
			dprintk(VIDC_ERR,
				"Failed to get domain index for domain %p",
				domain);
			rc = -EINVAL;
			goto fail_group;
		}
	}
	return rc;

fail_group:
	for (--i; i >= 0; i--) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		if (iommu_map->group)
			iommu_group_put(iommu_map->group);
		iommu_map->group = NULL;
		iommu_map->domain = -1;
	}
	return rc;
}

static void venus_hfi_deregister_iommu_domains(struct venus_hfi_device *device)
{
	struct iommu_set *iommu_group_set;
	struct iommu_info *iommu_map;
	int i = 0;

	if (!device)
		return;

	iommu_group_set = &device->res->iommu_group_set;
	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		if (iommu_map->group)
			iommu_group_put(iommu_map->group);
		iommu_map->group = NULL;
		iommu_map->domain = -1;
	}
}

static void venus_hfi_deinit_bus(struct venus_hfi_device *device)
{
	struct venus_bus_info *bus_info;
	int i = 0;

	if (!device)
		return;

	bus_info = &device->resources.bus_info;

	for (i = 0; i < MSM_VIDC_MAX_DEVICES; i++) {
		if (bus_info->ddr_handle[i]) {
			msm_bus_scale_unregister_client(
			   bus_info->ddr_handle[i]);
			bus_info->ddr_handle[i] = 0;
		}

		if (bus_info->ocmem_handle[i]) {
			msm_bus_scale_unregister_client(
			   bus_info->ocmem_handle[i]);
			bus_info->ocmem_handle[i] = 0;
		}
	}
}

static int venus_hfi_init_bus(struct venus_hfi_device *device)
{
	struct venus_bus_info *bus_info;
	int rc = 0;
	if ((!device) || (!device->res->bus_pdata))
		return -EINVAL;

	bus_info = &device->resources.bus_info;

	bus_info->ddr_handle[MSM_VIDC_ENCODER] =
		msm_bus_scale_register_client(
			&device->res->bus_pdata[BUS_IDX_ENC_DDR]);
	if (!bus_info->ddr_handle[MSM_VIDC_ENCODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto err_init_bus;
	}
	bus_info->ddr_handle[MSM_VIDC_DECODER] =
		msm_bus_scale_register_client(
			&device->res->bus_pdata[BUS_IDX_DEC_DDR]);
	if (!bus_info->ddr_handle[MSM_VIDC_DECODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto err_init_bus;
	}

	if (device->res->has_ocmem) {
		bus_info->ocmem_handle[MSM_VIDC_ENCODER] =
			msm_bus_scale_register_client(
				&device->res->bus_pdata[BUS_IDX_ENC_OCMEM]);
		if (!bus_info->ocmem_handle[MSM_VIDC_ENCODER]) {
			dprintk(VIDC_ERR,
				"Failed to register bus scale client\n");
			goto err_init_bus;
		}

		bus_info->ocmem_handle[MSM_VIDC_DECODER] =
			msm_bus_scale_register_client(
				&device->res->bus_pdata[BUS_IDX_DEC_OCMEM]);
		if (!bus_info->ocmem_handle[MSM_VIDC_DECODER]) {
			dprintk(VIDC_ERR,
				"Failed to register bus scale client\n");
			goto err_init_bus;
		}
	}

	return rc;
err_init_bus:
	venus_hfi_deinit_bus(device);
	return -EINVAL;
}

static int venus_hfi_ocmem_notify_handler(struct notifier_block *this,
		unsigned long event, void *data)
{
	struct ocmem_buf *buff = data;
	struct venus_hfi_device *device;
	struct venus_resources *resources;
	struct on_chip_mem *ocmem;
	int rc = NOTIFY_DONE;
	if (event == OCMEM_ALLOC_GROW) {
		ocmem = container_of(this, struct on_chip_mem, vidc_ocmem_nb);
		if (!ocmem) {
			dprintk(VIDC_ERR, "Wrong handler passed\n");
			rc = NOTIFY_BAD;
			goto err_ocmem_notify;
		}
		resources = container_of(ocmem,
			struct venus_resources, ocmem);
		device = container_of(resources,
			struct venus_hfi_device, resources);
		if (venus_hfi_set_ocmem(device, buff, 1)) {
			dprintk(VIDC_ERR, "Failed to set ocmem: %d\n", rc);
			goto err_ocmem_notify;
		}
		rc = NOTIFY_OK;
	}

err_ocmem_notify:
	return rc;
}

static void venus_hfi_ocmem_init(struct venus_hfi_device *device)
{
	struct on_chip_mem *ocmem;

	ocmem = &device->resources.ocmem;
	ocmem->vidc_ocmem_nb.notifier_call = venus_hfi_ocmem_notify_handler;
	ocmem->handle =
		ocmem_notifier_register(OCMEM_VIDEO, &ocmem->vidc_ocmem_nb);
	if (IS_ERR_OR_NULL(ocmem->handle)) {
		dprintk(VIDC_WARN,
				"Failed to register OCMEM notifier. Performance might be impacted\n");
		ocmem->handle = NULL;
	}
}

static void venus_hfi_deinit_ocmem(struct venus_hfi_device *device)
{
	if (device->resources.ocmem.handle)
		ocmem_notifier_unregister(device->resources.ocmem.handle,
				&device->resources.ocmem.vidc_ocmem_nb);
}


static int venus_hfi_init_resources(struct venus_hfi_device *device,
				struct msm_vidc_platform_resources *res)
{
	int rc = 0;

	device->res = res;
	if (!res) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", res);
		return -ENODEV;
	}
	device->gdsc = devm_regulator_get(&res->pdev->dev, "vdd");
	if (IS_ERR(device->gdsc)) {
		dprintk(VIDC_ERR, "Failed to get Venus GDSC\n");
		return -ENODEV;
	}
	rc = venus_hfi_init_clocks(res, device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init clocks\n");
		rc = -ENODEV;
		goto err_init_clocks;
	}

	rc = venus_hfi_init_bus(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init bus: %d\n", rc);
		goto err_init_bus;
	}

	rc = venus_hfi_register_iommu_domains(device, res);
	if (rc) {
		if (rc != -EPROBE_DEFER) {
			dprintk(VIDC_ERR,
				"Failed to register iommu domains: %d\n", rc);
		}
		goto err_register_iommu_domain;
	}

	if (res->has_ocmem)
		venus_hfi_ocmem_init(device);

	return rc;

err_register_iommu_domain:
	venus_hfi_deinit_bus(device);
err_init_bus:
	venus_hfi_deinit_clocks(device);
err_init_clocks:
	device->gdsc = NULL;
	return rc;
}

static void venus_hfi_deinit_resources(struct venus_hfi_device *device)
{
	if (device->res->has_ocmem)
		venus_hfi_deinit_ocmem(device);
	venus_hfi_deregister_iommu_domains(device);
	venus_hfi_deinit_bus(device);
	venus_hfi_deinit_clocks(device);
	device->gdsc = NULL;
}

static int venus_hfi_iommu_get_domain_partition(void *dev, u32 flags,
			u32 buffer_type, int *domain, int *partition)
{
	struct venus_hfi_device *device = dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s: Invalid param device: %p\n",
		 __func__, device);
		return -EINVAL;
	}

	msm_smem_get_domain_partition(device->hal_client, flags, buffer_type,
			domain, partition);
	return 0;
}

static int protect_cp_mem(struct venus_hfi_device *device)
{
	struct tzbsp_memprot memprot;
	unsigned int resp = 0;
	int rc = 0;
	struct iommu_set *iommu_group_set;
	struct iommu_info *iommu_map;
	int i;

	if (!device)
		return -EINVAL;

	iommu_group_set = &device->res->iommu_group_set;
	if (!iommu_group_set) {
		dprintk(VIDC_ERR, "invalid params: %p\n", iommu_group_set);
		return -EINVAL;
	}

	memprot.cp_start = 0x0;
	memprot.cp_size = 0x0;
	memprot.cp_nonpixel_start = 0x0;
	memprot.cp_nonpixel_size = 0x0;

	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		if (strcmp(iommu_map->name, "venus_ns") == 0)
			memprot.cp_size = iommu_map->addr_range[0].start;

		if (strcmp(iommu_map->name, "venus_sec_non_pixel") == 0) {
			memprot.cp_nonpixel_start =
				iommu_map->addr_range[0].start;
			memprot.cp_nonpixel_size =
				iommu_map->addr_range[0].size;
		} else if (strcmp(iommu_map->name, "venus_cp") == 0) {
			memprot.cp_nonpixel_start =
				iommu_map->addr_range[1].start;
		}
	}

	rc = scm_call(SCM_SVC_MP, TZBSP_MEM_PROTECT_VIDEO_VAR, &memprot,
			sizeof(memprot), &resp, sizeof(resp));
	if (rc)
		dprintk(VIDC_ERR,
		"Failed to protect memory , rc is :%d, response : %d\n",
		rc, resp);
	return rc;
}

static int venus_hfi_load_fw(void *dev)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;

	if (!device || !device->gdsc) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return -EINVAL;
	}
	device->clk_gating_level = VCODEC_CLK;
	rc = venus_hfi_iommu_attach(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to attach iommu");
		goto fail_iommu_attach;
	}

	mutex_lock(&device->clk_pwr_lock);
	if (!device->resources.fw.cookie) {
		rc = regulator_enable(device->gdsc);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to enable GDSC %d", rc);
			mutex_unlock(&device->clk_pwr_lock);
			goto fail_enable_gdsc;
		}
		device->resources.fw.cookie = subsystem_get("venus");
	}

	if (IS_ERR_OR_NULL(device->resources.fw.cookie)) {
		dprintk(VIDC_ERR, "Failed to download firmware\n");
		rc = -ENOMEM;
		mutex_unlock(&device->clk_pwr_lock);
		goto fail_load_fw;
	}
	device->power_enabled = 1;
	++device->pwr_cnt;
	mutex_unlock(&device->clk_pwr_lock);
	/*Clocks can be enabled only after pil_get since
	 * gdsc is turned-on in pil_get*/
	rc = venus_hfi_enable_clks(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to enable clocks: %d\n", rc);
		goto fail_enable_clks;
	}

	rc = protect_cp_mem(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to protect memory\n");
		goto fail_protect_mem;
	}

	return rc;
fail_protect_mem:
	venus_hfi_disable_clks(device);
fail_enable_clks:
	subsystem_put(device->resources.fw.cookie);
fail_load_fw:
	mutex_lock(&device->clk_pwr_lock);
	device->resources.fw.cookie = NULL;
	regulator_disable(device->gdsc);
	device->power_enabled = 0;
	--device->pwr_cnt;
	mutex_unlock(&device->clk_pwr_lock);
fail_enable_gdsc:
	venus_hfi_iommu_detach(device);
fail_iommu_attach:
	return rc;
}

static void venus_hfi_unload_fw(void *dev)
{
	struct venus_hfi_device *device = dev;
	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return;
	}
	if (device->resources.fw.cookie) {
		flush_workqueue(device->vidc_workq);
		flush_workqueue(device->venus_pm_workq);
		subsystem_put(device->resources.fw.cookie);
		venus_hfi_interface_queues_release(dev);
		/* IOMMU operations need to be done before AXI halt.*/
		venus_hfi_iommu_detach(device);
		/* Halt the AXI to make sure there are no pending transactions.
		 * Clocks should be unprepared after making sure axi is halted.
		 */
		if(venus_hfi_halt_axi(device))
			dprintk(VIDC_WARN, "Failed to halt AXI\n");
		venus_hfi_disable_clks(device);
		mutex_lock(&device->clk_pwr_lock);
		regulator_disable(device->gdsc);
		device->power_enabled = 0;
		--device->pwr_cnt;
		mutex_unlock(&device->clk_pwr_lock);
		device->resources.fw.cookie = NULL;
	}
}

static int venus_hfi_get_fw_info(void *dev, enum fw_info info)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return -EINVAL;
	}

	switch (info) {
	case FW_BASE_ADDRESS:
		rc = device->base_addr;
		break;

	case FW_REGISTER_BASE:
		rc = device->register_base;
		break;

	case FW_REGISTER_SIZE:
		rc = device->register_size;
		break;

	case FW_IRQ:
		rc = device->irq;
		break;

	default:
		dprintk(VIDC_ERR, "Invalid fw info requested");
	}
	return rc;
}

static int venus_hfi_get_info(void *dev, enum dev_info info)
{
	int rc = 0;
	struct venus_hfi_device *device = dev;
	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid parameter: %p\n",
				__func__, device);
		return -EINVAL;
	}

	mutex_lock(&device->clk_pwr_lock);
	switch (info) {
	case DEV_CLOCK_COUNT:
		rc = device->clk_cnt;
		break;
	case DEV_CLOCK_ENABLED:
		rc = device->clocks_enabled;
		break;
	case DEV_PWR_COUNT:
		rc = device->pwr_cnt;
		break;
	case DEV_PWR_ENABLED:
		rc = device->power_enabled;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid device info requested");
	}
	mutex_unlock(&device->clk_pwr_lock);
	return rc;
}

int venus_hfi_get_stride_scanline(int color_fmt,
	int width, int height, int *stride, int *scanlines) {
	*stride = VENUS_Y_STRIDE(color_fmt, width);
	*scanlines = VENUS_Y_SCANLINES(color_fmt, height);
	return 0;
}

int venus_hfi_get_core_capabilities(void)
{
	int i = 0, rc = 0, j = 0, venus_version_length = 0;
	u32 smem_block_size = 0;
	u8 *smem_table_ptr;
	char version[256];
	const u32 version_string_size = 128;
	char venus_version[] = "VIDEO.VE.1.4";
	u8 version_info[256];
	const u32 smem_image_index_venus = 14 * 128;
	/* Venus version is stored at 14th entry in smem table */

	smem_table_ptr = smem_get_entry(SMEM_IMAGE_VERSION_TABLE,
			&smem_block_size);
	if (smem_table_ptr &&
			((smem_image_index_venus + version_string_size) <=
			smem_block_size)) {
		memcpy(version_info, smem_table_ptr + smem_image_index_venus,
				version_string_size);
	} else {
		dprintk(VIDC_ERR,
			"%s: failed to read version info from smem table\n",
			__func__);
		return -EINVAL;
	}

	while (version_info[i++] != 'V' && i < version_string_size)
		;

	venus_version_length = strlen(venus_version);
	for (i--, j = 0; i < version_string_size && j < venus_version_length;
		i++)
		version[j++] = version_info[i];
	version[venus_version_length] = '\0';
	dprintk(VIDC_DBG, "F/W version retrieved : %s\n", version);

	if (strcmp((const char *)version, (const char *)venus_version) == 0)
		rc = HAL_VIDEO_ENCODER_ROTATION_CAPABILITY |
			HAL_VIDEO_ENCODER_SCALING_CAPABILITY |
			HAL_VIDEO_ENCODER_DEINTERLACE_CAPABILITY |
			HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY;
	return rc;
}

int venus_hfi_capability_check(u32 fourcc, u32 width,
		u32 *max_width, u32 *max_height)
{
	int rc = 0;
	if (!max_width || !max_height) {
		dprintk(VIDC_ERR, "%s - invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (msm_vp8_low_tier && fourcc == V4L2_PIX_FMT_VP8) {
		*max_width = DEFAULT_WIDTH;
		*max_height = DEFAULT_HEIGHT;
	}
	if (width > *max_width) {
		dprintk(VIDC_ERR,
		"Unsupported width = %u supported max width = %u",
		width, *max_width);
		rc = -ENOTSUPP;
	}
	return rc;
}

static void *venus_hfi_add_device(u32 device_id,
			struct msm_vidc_platform_resources *res,
			hfi_cmd_response_callback callback)
{
	struct venus_hfi_device *hdevice = NULL;
	int rc = 0;

	if (!res || !callback) {
		dprintk(VIDC_ERR, "Invalid Paramters");
		return NULL;
	}

	dprintk(VIDC_INFO, "entered , device_id: %d", device_id);

	hdevice = (struct venus_hfi_device *)
			kzalloc(sizeof(struct venus_hfi_device), GFP_KERNEL);
	if (!hdevice) {
		dprintk(VIDC_ERR, "failed to allocate new device");
		goto err_alloc;
	}

	rc = venus_hfi_init_regs_and_interrupts(hdevice, res);
	if (rc)
		goto err_init_regs;

	hdevice->device_id = device_id;
	hdevice->callback = callback;
	hdevice->clocks_enabled = 0;
	hdevice->clk_cnt = 0;
	hdevice->power_enabled = 0;
	hdevice->pwr_cnt = 0;

	hdevice->vidc_workq = create_singlethread_workqueue(
		"msm_vidc_workerq_venus");
	if (!hdevice->vidc_workq) {
		dprintk(VIDC_ERR, ": create vidc workq failed\n");
		goto error_createq;
	}
	hdevice->venus_pm_workq = create_singlethread_workqueue(
			"pm_workerq_venus");
	if (!hdevice->venus_pm_workq) {
		dprintk(VIDC_ERR, ": create pm workq failed\n");
		goto error_createq_pm;
	}

	mutex_init(&hdevice->read_lock);
	mutex_init(&hdevice->write_lock);
	mutex_init(&hdevice->session_lock);
	mutex_init(&hdevice->clk_pwr_lock);

	if (hal_ctxt.dev_count == 0)
		INIT_LIST_HEAD(&hal_ctxt.dev_head);

	INIT_LIST_HEAD(&hdevice->list);
	list_add_tail(&hdevice->list, &hal_ctxt.dev_head);
	hal_ctxt.dev_count++;

	return (void *) hdevice;
error_createq_pm:
	destroy_workqueue(hdevice->vidc_workq);
error_createq:
err_init_regs:
	kfree(hdevice);
err_alloc:
	return NULL;
}

static void *venus_hfi_get_device(u32 device_id,
				struct msm_vidc_platform_resources *res,
				hfi_cmd_response_callback callback)
{
	struct venus_hfi_device *device;
	int rc = 0;

	if (!res || !callback) {
		dprintk(VIDC_ERR, "Invalid params: %p %p\n", res, callback);
		return NULL;
	}

	device = venus_hfi_add_device(device_id, res, &handle_cmd_response);
	if (!device) {
		dprintk(VIDC_ERR, "Failed to create HFI device\n");
		return NULL;
	}

	rc = venus_hfi_init_resources(device, res);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			dprintk(VIDC_ERR, "Failed to init resources: %d\n", rc);
		goto err_fail_init_res;
	}
	return device;

err_fail_init_res:
	venus_hfi_delete_device(device);
	return ERR_PTR(rc);
}

void venus_hfi_delete_device(void *device)
{
	struct venus_hfi_device *close, *tmp, *dev;

	if (device) {
		venus_hfi_deinit_resources(device);
		dev = (struct venus_hfi_device *) device;
		list_for_each_entry_safe(close, tmp, &hal_ctxt.dev_head, list) {
			if (close->hal_data->irq == dev->hal_data->irq) {
				hal_ctxt.dev_count--;
				free_irq(dev->hal_data->irq, close);
				list_del(&close->list);
				destroy_workqueue(close->vidc_workq);
				destroy_workqueue(close->venus_pm_workq);
				kfree(close->hal_data);
				kfree(close);
				break;
			}
		}

	}
}

static void venus_init_hfi_callbacks(struct hfi_device *hdev)
{
	hdev->core_init = venus_hfi_core_init;
	hdev->core_release = venus_hfi_core_release;
	hdev->core_pc_prep = venus_hfi_core_pc_prep;
	hdev->core_ping = venus_hfi_core_ping;
	hdev->core_trigger_ssr = venus_hfi_core_trigger_ssr;
	hdev->session_init = venus_hfi_session_init;
	hdev->session_end = venus_hfi_session_end;
	hdev->session_abort = venus_hfi_session_abort;
	hdev->session_clean = venus_hfi_session_clean;
	hdev->session_set_buffers = venus_hfi_session_set_buffers;
	hdev->session_release_buffers = venus_hfi_session_release_buffers;
	hdev->session_load_res = venus_hfi_session_load_res;
	hdev->session_release_res = venus_hfi_session_release_res;
	hdev->session_start = venus_hfi_session_start;
	hdev->session_stop = venus_hfi_session_stop;
	hdev->session_suspend = venus_hfi_session_suspend;
	hdev->session_resume = venus_hfi_session_resume;
	hdev->session_etb = venus_hfi_session_etb;
	hdev->session_ftb = venus_hfi_session_ftb;
	hdev->session_parse_seq_hdr = venus_hfi_session_parse_seq_hdr;
	hdev->session_get_seq_hdr = venus_hfi_session_get_seq_hdr;
	hdev->session_get_buf_req = venus_hfi_session_get_buf_req;
	hdev->session_flush = venus_hfi_session_flush;
	hdev->session_set_property = venus_hfi_session_set_property;
	hdev->session_get_property = venus_hfi_session_get_property;
	hdev->scale_clocks = venus_hfi_scale_clocks;
	hdev->scale_bus = venus_hfi_scale_bus;
	hdev->unvote_bus = venus_hfi_unvote_bus;
	hdev->unset_ocmem = venus_hfi_unset_ocmem;
	hdev->alloc_ocmem = venus_hfi_alloc_ocmem;
	hdev->free_ocmem = venus_hfi_free_ocmem;
	hdev->iommu_get_domain_partition = venus_hfi_iommu_get_domain_partition;
	hdev->load_fw = venus_hfi_load_fw;
	hdev->unload_fw = venus_hfi_unload_fw;
	hdev->get_fw_info = venus_hfi_get_fw_info;
	hdev->get_info = venus_hfi_get_info;
	hdev->get_stride_scanline = venus_hfi_get_stride_scanline;
	hdev->capability_check = venus_hfi_capability_check;
	hdev->get_core_capabilities = venus_hfi_get_core_capabilities;
	hdev->power_enable = venus_hfi_power_enable;
}

int venus_hfi_initialize(struct hfi_device *hdev, u32 device_id,
		struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback)
{
	int rc = 0;

	if (!hdev || !res || !callback) {
		dprintk(VIDC_ERR, "Invalid params: %p %p %p\n",
			hdev, res, callback);
		rc = -EINVAL;
		goto err_venus_hfi_init;
	}
	hdev->hfi_device_data = venus_hfi_get_device(device_id, res, callback);

	if (IS_ERR_OR_NULL(hdev->hfi_device_data)) {
		rc = PTR_ERR(hdev->hfi_device_data);
		rc = !rc ? -EINVAL : rc;
		goto err_venus_hfi_init;
	}

	venus_init_hfi_callbacks(hdev);

err_venus_hfi_init:
	return rc;
}

