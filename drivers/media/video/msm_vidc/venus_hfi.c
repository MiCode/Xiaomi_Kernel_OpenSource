/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <asm/memory.h>
#include "hfi_packetization.h"
#include "venus_hfi.h"
#include "vidc_hfi_io.h"
#include "msm_vidc_debug.h"

#define FIRMWARE_SIZE			0X00A00000
#define REG_ADDR_OFFSET_BITMASK	0x000FFFFF

/*Workaround for simulator */
#define HFI_SIM_FW_BIAS		0x0

#define SHARED_QSIZE 0x1000000

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

static struct msm_bus_vectors enc_ocmem_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors enc_ocmem_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 138200000,
		.ib = 1222000000,
	},
};

static struct msm_bus_vectors enc_ocmem_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 414700000,
		.ib = 1222000000,
	},
};

static struct msm_bus_vectors enc_ocmem_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 940000000,
		.ib = 2444000000U,
	},
};

static struct msm_bus_vectors enc_ocmem_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 1880000000,
		.ib = 2444000000U,
	},
};

static struct msm_bus_vectors enc_ocmem_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 3008000000U,
		.ib = 3910400000U,
	},
};

static struct msm_bus_vectors enc_ocmem_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 3760000000U,
		.ib = 4888000000ULL,
	},
};

static struct msm_bus_vectors dec_ocmem_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dec_ocmem_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 176900000,
		.ib = 1556640000,
	},
};

static struct msm_bus_vectors dec_ocmem_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 456200000,
		.ib = 1556640000,
	},
};

static struct msm_bus_vectors dec_ocmem_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 864800000,
		.ib = 1556640000,
	},
};

static struct msm_bus_vectors dec_ocmem_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 1729600000,
		.ib = 3113280000U,
	},
};

static struct msm_bus_vectors dec_ocmem_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 2767360000U,
		.ib = 4981248000ULL,
	},
};

static struct msm_bus_vectors dec_ocmem_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 3459200000U,
		.ib = 6226560000ULL,
	},
};

static struct msm_bus_paths enc_ocmem_perf_vectors[]  = {
	{
		ARRAY_SIZE(enc_ocmem_init_vectors),
		enc_ocmem_init_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf1_vectors),
		enc_ocmem_perf1_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf2_vectors),
		enc_ocmem_perf2_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf3_vectors),
		enc_ocmem_perf3_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf4_vectors),
		enc_ocmem_perf4_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf5_vectors),
		enc_ocmem_perf5_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf6_vectors),
		enc_ocmem_perf6_vectors,
	},
};

static struct msm_bus_paths dec_ocmem_perf_vectors[]  = {
	{
		ARRAY_SIZE(dec_ocmem_init_vectors),
		dec_ocmem_init_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf1_vectors),
		dec_ocmem_perf1_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf2_vectors),
		dec_ocmem_perf2_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf3_vectors),
		dec_ocmem_perf3_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf4_vectors),
		dec_ocmem_perf4_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf5_vectors),
		dec_ocmem_perf5_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf6_vectors),
		dec_ocmem_perf6_vectors,
	},
};


static struct msm_bus_scale_pdata enc_ocmem_bus_data = {
	.usecase = enc_ocmem_perf_vectors,
	.num_usecases = ARRAY_SIZE(enc_ocmem_perf_vectors),
	.name = "msm_vidc_enc_ocmem",
};

static struct msm_bus_scale_pdata dec_ocmem_bus_data = {
	.usecase = dec_ocmem_perf_vectors,
	.num_usecases = ARRAY_SIZE(dec_ocmem_perf_vectors),
	.name = "msm_vidc_dec_ocmem",
};

static struct msm_bus_vectors enc_ddr_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};


static struct msm_bus_vectors enc_ddr_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 60000000,
		.ib = 664950000,
	},
};

static struct msm_bus_vectors enc_ddr_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 181000000,
		.ib = 664950000,
	},
};

static struct msm_bus_vectors enc_ddr_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 403000000,
		.ib = 664950000,
	},
};

static struct msm_bus_vectors enc_ddr_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 806000000,
		.ib = 1329900000,
	},
};

static struct msm_bus_vectors enc_ddr_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1289600000,
		.ib = 2127840000U,
	},
};

static struct msm_bus_vectors enc_ddr_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 161200000,
		.ib = 6400000000ULL,
	},
};

static struct msm_bus_vectors dec_ddr_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dec_ddr_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 110000000,
		.ib = 909000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 268000000,
		.ib = 909000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 505000000,
		.ib = 909000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1010000000,
		.ib = 1818000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1616000000,
		.ib = 2908800000U,
	},
};

static struct msm_bus_vectors dec_ddr_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2020000000U,
		.ib = 6400000000ULL,
	},
};

static struct msm_bus_paths enc_ddr_perf_vectors[]  = {
	{
		ARRAY_SIZE(enc_ddr_init_vectors),
		enc_ddr_init_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf1_vectors),
		enc_ddr_perf1_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf2_vectors),
		enc_ddr_perf2_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf3_vectors),
		enc_ddr_perf3_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf4_vectors),
		enc_ddr_perf4_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf5_vectors),
		enc_ddr_perf5_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf6_vectors),
		enc_ddr_perf6_vectors,
	},
};

static struct msm_bus_paths dec_ddr_perf_vectors[]  = {
	{
		ARRAY_SIZE(dec_ddr_init_vectors),
		dec_ddr_init_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf1_vectors),
		dec_ddr_perf1_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf2_vectors),
		dec_ddr_perf2_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf3_vectors),
		dec_ddr_perf3_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf4_vectors),
		dec_ddr_perf4_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf5_vectors),
		dec_ddr_perf5_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf6_vectors),
		dec_ddr_perf6_vectors,
	},
};

static struct msm_bus_scale_pdata enc_ddr_bus_data = {
	.usecase = enc_ddr_perf_vectors,
	.num_usecases = ARRAY_SIZE(enc_ddr_perf_vectors),
	.name = "msm_vidc_enc_ddr",
};

static struct msm_bus_scale_pdata dec_ddr_bus_data = {
	.usecase = dec_ddr_perf_vectors,
	.num_usecases = ARRAY_SIZE(dec_ddr_perf_vectors),
	.name = "msm_vidc_dec_ddr",
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
	venus_hfi_sim_modify_cmd_packet(packet);

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
	venus_hfi_hal_sim_modify_msg_packet(packet);
	dprintk(VIDC_DBG, "Out : ");
	return 0;
}

static int venus_hfi_alloc(void *mem, void *clnt, u32 size, u32 align,
						   u32 flags, int domain)
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
	dprintk(VIDC_DBG, "venus_hfi_alloc:ptr=%p,size=%d",
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

static void venus_hfi_free(struct smem_client *clnt, struct msm_smem *mem)
{
	msm_smem_free(clnt, mem);
}

static void venus_hfi_write_register(u8 *base_addr, u32 reg,
				u32 value, u8 *vaddr)
{
	u32 hwiosymaddr = reg;

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

static int venus_hfi_read_register(u8 *base_addr, u32 reg)
{
	int rc = readl_relaxed((u32)base_addr + reg);
	rmb();
	return rc;
}

static int venus_hfi_iface_cmdq_write(struct venus_hfi_device *device,
					void *pkt)
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

	if (!venus_hfi_write_queue(q_info, (u8 *)pkt, &rx_req_is_set)) {
		if (rx_req_is_set)
			venus_hfi_write_register(
				device->hal_data->register_base_addr,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		result = 0;
	} else {
		dprintk(VIDC_ERR, "venus_hfi_iface_cmdq_write:queue_full");
	}
err_q_write:
	spin_unlock(&device->write_lock);
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
	spin_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_MSGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared MSG Q's");
		rc = -ENODATA;
		goto read_error;
	}
	q_info = &device->iface_queues[VIDC_IFACEQ_MSGQ_IDX];

	if (!venus_hfi_read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		if (tx_req_is_set)
			venus_hfi_write_register(
				device->hal_data->register_base_addr,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		rc = 0;
	} else {
		dprintk(VIDC_INFO, "venus_hfi_iface_msgq_read:queue_empty");
		rc = -ENODATA;
	}
read_error:
	spin_unlock(&device->read_lock);
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
	spin_lock(&device->read_lock);
	if (device->iface_queues[VIDC_IFACEQ_DBGQ_IDX].
		q_array.align_virtual_addr == 0) {
		dprintk(VIDC_ERR, "cannot read from shared DBG Q's");
		rc = -ENODATA;
		goto dbg_error;
	}
	q_info = &device->iface_queues[VIDC_IFACEQ_DBGQ_IDX];
	if (!venus_hfi_read_queue(q_info, (u8 *)pkt, &tx_req_is_set)) {
		if (tx_req_is_set)
			venus_hfi_write_register(
				device->hal_data->register_base_addr,
				VIDC_CPU_IC_SOFTINT,
				1 << VIDC_CPU_IC_SOFTINT_H2A_SHFT, 0);
		rc = 0;
	} else {
		dprintk(VIDC_INFO, "venus_hfi_iface_dbgq_read:queue_empty");
		rc = -ENODATA;
	}
dbg_error:
	spin_unlock(&device->read_lock);
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

	if (device->qdss.mem_data) {
		qdss = (struct hfi_mem_map_table *)
			device->qdss.align_virtual_addr;
		qdss->mem_map_num_entries = num_entries;
		qdss->mem_map_table_base_addr =
			(u32 *)((u32)device->qdss.align_device_addr +
				sizeof(struct hfi_mem_map_table));
		mem_map = (struct hfi_mem_map *)(qdss + 1);
		for (i = 0; i < num_entries; i++) {
			msm_iommu_unmap_contig_buffer(
				(unsigned long)(mem_map[i].virtual_addr),
				device->resources.io_map[NS_MAP].domain,
				1, SZ_4K);
		}
		venus_hfi_free(device->hal_client, device->qdss.mem_data);
	}
	venus_hfi_free(device->hal_client, device->iface_q_table.mem_data);
	venus_hfi_free(device->hal_client, device->sfr.mem_data);

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
static int venus_hfi_get_qdss_iommu_virtual_addr(struct hfi_mem_map *mem_map,
	int domain)
{
	int i;
	int rc = 0;
	unsigned long iova = 0;
	int num_entries = sizeof(venus_qdss_entries)/(2 * sizeof(u32));

	for (i = 0; i < num_entries; i++) {
		rc = msm_iommu_map_contig_buffer(venus_qdss_entries[i][0],
			domain, 1 , venus_qdss_entries[i][1],
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
				domain, 1, SZ_4K);
		}
	}
	return rc;
}

static int venus_hfi_interface_queues_init(struct venus_hfi_device *dev,
					int domain)
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
	mem_addr = &dev->mem_addr;
	rc = venus_hfi_alloc((void *) mem_addr,
			dev->hal_client, QUEUE_SIZE, 1,
			0, domain);
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

	rc = venus_hfi_alloc((void *) mem_addr,
			dev->hal_client, QDSS_SIZE, 1,
			0, domain);
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
	rc = venus_hfi_alloc((void *) mem_addr,
			dev->hal_client, SFR_SIZE, 1,
			0, domain);
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

	venus_hfi_write_register(dev->hal_data->register_base_addr,
			VIDC_UC_REGION_ADDR,
			(u32) dev->iface_q_table.align_device_addr, 0);
	venus_hfi_write_register(dev->hal_data->register_base_addr,
			VIDC_UC_REGION_SIZE, SHARED_QSIZE, 0);
	venus_hfi_write_register(dev->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG2,
		(u32) dev->iface_q_table.align_device_addr,
		dev->iface_q_table.align_virtual_addr);
	venus_hfi_write_register(dev->hal_data->register_base_addr,
		VIDC_CPU_CS_SCIACMDARG1, 0x01,
		dev->iface_q_table.align_virtual_addr);

	qdss = (struct hfi_mem_map_table *) dev->qdss.align_virtual_addr;
	qdss->mem_map_num_entries = num_entries;
	qdss->mem_map_table_base_addr =
		(u32 *)	((u32)dev->qdss.align_device_addr +
		sizeof(struct hfi_mem_map_table));
	mem_map = (struct hfi_mem_map *)(qdss + 1);
	rc = venus_hfi_get_qdss_iommu_virtual_addr(mem_map, domain);
	if (rc) {
		dprintk(VIDC_ERR,
			"IOMMU mapping failed, Freeing qdss memdata");
		venus_hfi_free(dev->hal_client, dev->qdss.mem_data);
		dev->qdss.mem_data = NULL;
	}
	if (!IS_ERR_OR_NULL(dev->qdss.align_device_addr))
		venus_hfi_write_register(dev->hal_data->register_base_addr,
			VIDC_MMAP_ADDR,
			(u32) dev->qdss.align_device_addr, 0);

	vsfr = (struct hfi_sfr_struct *) dev->sfr.align_virtual_addr;
	vsfr->bufSize = SFR_SIZE;
	if (!IS_ERR_OR_NULL(dev->sfr.align_device_addr))
		venus_hfi_write_register(dev->hal_data->register_base_addr,
			VIDC_SFR_ADDR, (u32)dev->sfr.align_device_addr , 0);
	return 0;
fail_alloc_queue:
	return -ENOMEM;
}

static int venus_hfi_core_start_cpu(struct venus_hfi_device *device)
{
	u32 ctrl_status = 0, count = 0, rc = 0;
	int max_tries = 100;
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_WRAPPER_INTR_MASK, 0x8, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_CPU_CS_SCIACMDARG3, 1, 0);

	while (!ctrl_status && count < max_tries) {
		ctrl_status = venus_hfi_read_register(
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

static void venus_hfi_set_vbif_registers(struct venus_hfi_device *device)
{
	/*Disable Dynamic clock gating for Venus VBIF*/
	venus_hfi_write_register(device->hal_data->register_base_addr,
				   VIDC_VENUS_VBIF_CLK_ON, 1, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_AXI_AOOO_EN, 0x00001FFF, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_AXI_AOOO, 0x1FFF1FFF, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF0, 0x10101001, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF1, 0x10101010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF2, 0x10101010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_RD_LIM_CONF3, 0x00000010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF0, 0x1010100f, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF1, 0x10101010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF2, 0x10101010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_IN_WR_LIM_CONF3, 0x00000010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_RD_LIM_CONF0, 0x00001010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_OUT_WR_LIM_CONF0, 0x00001010, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VBIF_ARB_CTL, 0x00000030, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VENUS_VBIF_DDR_OUT_MAX_BURST, 0x00000707, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VENUS_VBIF_OCMEM_OUT_MAX_BURST, 0x00000707, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VENUS_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000001, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VENUS0_WRAPPER_VBIF_REQ_PRIORITY, 0x5555556, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_VENUS0_WRAPPER_VBIF_PRIORITY_LEVEL, 0, 0);
}

static int venus_hfi_sys_set_debug(struct venus_hfi_device *device, int debug)
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
	if (msm_fw_debug_mode <= HFI_DEBUG_MODE_QDSS)
		hfi->debug_mode = msm_fw_debug_mode;
	if (venus_hfi_iface_cmdq_write(device, pkt))
		return -ENOTEMPTY;
	return 0;
}

static int venus_hfi_core_init(void *device)
{
	struct hfi_cmd_sys_init_packet pkt;
	int rc = 0;
	struct venus_hfi_device *dev;

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
	venus_hfi_set_vbif_registers(dev);

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

		rc = venus_hfi_interface_queues_init(dev,
				dev->resources.io_map[NS_MAP].domain);
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
	venus_hfi_write_register(dev->hal_data->register_base_addr,
		VIDC_CTRL_INIT, 0x1, 0);
	rc = venus_hfi_core_start_cpu(dev);
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
	if (venus_hfi_iface_cmdq_write(dev, &pkt)) {
		rc = -ENOTEMPTY;
		goto err_core_init;
	}
	return rc;
err_core_init:
	disable_irq_nosync(dev->hal_data->irq);
	return rc;
}

static int venus_hfi_core_release(void *device)
{
	struct venus_hfi_device *dev;
	if (device) {
		dev = device;
	} else {
		dprintk(VIDC_ERR, "invalid device");
		return -ENODEV;
	}
	if (dev->hal_client) {
		venus_hfi_write_register(dev->hal_data->register_base_addr,
				VIDC_CPU_CS_SCIACMDARG3, 0, 0);
		if (!(dev->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK))
			disable_irq_nosync(dev->hal_data->irq);
		dev->intr_status = 0;
		venus_hfi_interface_queues_release(dev);
	}
	dprintk(VIDC_INFO, "HAL exited\n");
	return 0;
}

int venus_hfi_core_pc_prep(void *device)
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

static void venus_hfi_core_clear_interrupt(struct venus_hfi_device *device)
{
	u32 intr_status = 0;

	if (!device->callback)
		return;

	intr_status = venus_hfi_read_register(
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
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_CPU_CS_A2HSOFTINTCLR, 1, 0);
	venus_hfi_write_register(device->hal_data->register_base_addr,
			VIDC_WRAPPER_INTR_CLEAR, intr_status, 0);
	dprintk(VIDC_DBG, "Cleared WRAPPER/A2H interrupt");
}

static int venus_hfi_core_set_resource(void *device,
		struct vidc_resource_hdr *resource_hdr, void *resource_value)
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
	if (venus_hfi_iface_cmdq_write(dev, pkt))
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

	if (venus_hfi_iface_cmdq_write(dev, &pkt))
		goto err_session_init_fail;
	if (venus_hfi_sys_set_debug(dev, msm_fw_debug))
		dprintk(VIDC_ERR, "Setting fw_debug msg ON failed");
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

static void venus_hfi_response_handler(struct venus_hfi_device *device)
{
	u8 packet[VIDC_IFACEQ_MED_PKT_SIZE];

	dprintk(VIDC_INFO, "#####venus_hfi_response_handler#####\n");
	if (device) {
		if ((device->intr_status &
			VIDC_WRAPPER_INTR_CLEAR_A2HWD_BMSK)) {
			dprintk(VIDC_ERR, "Received: Watchdog timeout %s",
				__func__);
			venus_hfi_process_sys_watchdog_timeout(device);
		}

		while (!venus_hfi_iface_msgq_read(device, packet)) {
			hfi_process_msg_packet(device->callback,
				device->device_id,
				(struct vidc_hal_msg_pkt_hdr *) packet);
		}
		while (!venus_hfi_iface_dbgq_read(device, packet)) {
			struct hfi_msg_sys_debug_packet *pkt =
				(struct hfi_msg_sys_debug_packet *) packet;
			dprintk(VIDC_FW, "FW-SAYS: %s", pkt->rg_msg_data);
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
		struct venus_hfi_device *device, struct platform_device *pdev)
{
	struct hal_data *hal = NULL;
	int rc = 0;
	struct resource *res;

	device->base_addr = 0x0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dprintk(VIDC_ERR, "Failed to get IORESOURCE_MEM\n");
		rc = -ENODEV;
		goto err_core_init;
	}
	device->register_base = res->start;
	device->register_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dprintk(VIDC_ERR, "Failed to get IORESOURCE_IRQ\n");
		rc = -ENODEV;
		goto err_core_init;
	}
	device->irq = res->start;

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

static size_t read_u32_array(struct platform_device *pdev,
		char *name, u32 *arr, size_t size)
{
	int len;
	size_t sz = 0;
	struct device_node *np = pdev->dev.of_node;
	if (!of_get_property(np, name, &len)) {
		dprintk(VIDC_ERR, "Failed to read %s from device tree\n",
			name);
		goto fail_read;
	}
	sz = len / sizeof(u32);
	if (sz <= 0) {
		dprintk(VIDC_ERR, "%s not specified in device tree\n",
			name);
		goto fail_read;
	}
	if (sz > size) {
		dprintk(VIDC_ERR, "Not enough memory to store %s values\n",
			name);
		goto fail_read;
	}
	if (of_property_read_u32_array(np, name, arr, sz)) {
		dprintk(VIDC_ERR,
			"error while reading %s from device tree\n",
			name);
		goto fail_read;
	}
	return sz;
fail_read:
	sz = 0;
	return sz;
}

static inline int venus_hfi_init_clocks(struct platform_device *pdev,
		struct venus_hfi_device *device)
{
	struct venus_core_clock *cl;
	int i;
	int rc = 0;
	struct venus_core_clock *clock;
	if (!device) {
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
	strlcpy(clock[VCODEC_OCMEM_CLK].name, "mem_clk",
		sizeof(clock[VCODEC_OCMEM_CLK].name));

	clock[VCODEC_CLK].count = read_u32_array(pdev,
		"load-freq-tbl", (u32 *)clock[VCODEC_CLK].load_freq_tbl,
		(sizeof(clock[VCODEC_CLK].load_freq_tbl)/sizeof(u32)));
	clock[VCODEC_CLK].count /= 2;
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
		cl = &device->resources.clock[i];
		if (!cl->clk) {
			cl->clk = devm_clk_get(&pdev->dev, cl->name);
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
	for (i = 0; i < VCODEC_MAX_CLKS; i++)
		clk_put(device->resources.clock[i].clk);
}

static unsigned long venus_hfi_get_clock_rate(struct venus_core_clock *clock,
	int num_mbs_per_sec)
{
	int num_rows = clock->count;
	struct load_freq_table *table = clock->load_freq_tbl;
	unsigned long ret = table[num_rows-1].freq;
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

	rc = clk_set_rate(device->resources.clock[VCODEC_CLK].clk,
		venus_hfi_get_clock_rate(&device->resources.clock[VCODEC_CLK],
		load));
	if (rc)
		dprintk(VIDC_ERR, "Failed to set clock rate: %d\n", rc);
	return rc;
}

static inline int venus_hfi_enable_clks(struct venus_hfi_device *device)
{
	int i;
	struct venus_core_clock *cl;
	int rc = 0;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return -EINVAL;
	}
	for (i = 0; i < VCODEC_MAX_CLKS; i++) {
		cl = &device->resources.clock[i];
		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to enable clocks\n");
			goto fail_clk_enable;
		} else {
			dprintk(VIDC_DBG, "Clock: %s enabled\n", cl->name);
		}
	}
	return rc;
fail_clk_enable:
	for (; i >= 0; i--) {
		cl = &device->resources.clock[i];
		clk_disable_unprepare(cl->clk);
	}
	return rc;
}

static inline void venus_hfi_disable_clks(struct venus_hfi_device *device)
{
	int i;
	struct venus_core_clock *cl;
	if (!device) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", device);
		return;
	}
	for (i = 0; i < VCODEC_MAX_CLKS; i++) {
		cl = &device->resources.clock[i];
		clk_disable_unprepare(cl->clk);
	}
}

static int venus_hfi_register_iommu_domains(struct venus_hfi_device *device,
					struct platform_device *pdev)
{
	size_t len;
	struct msm_iova_partition partition[2];
	struct msm_iova_layout layout;
	int rc = 0;
	int i;
	struct msm_vidc_iommu_info *io_map;

	if (!device)
		return -EINVAL;

	io_map = device->resources.io_map;

	strlcpy(io_map[CP_MAP].name, "vidc-cp-map",
			sizeof(io_map[CP_MAP].name));
	strlcpy(io_map[CP_MAP].ctx, "venus_cp",
			sizeof(io_map[CP_MAP].ctx));
	strlcpy(io_map[NS_MAP].name, "vidc-ns-map",
			sizeof(io_map[NS_MAP].name));
	strlcpy(io_map[NS_MAP].ctx, "venus_ns",
			sizeof(io_map[NS_MAP].ctx));

	for (i = 0; i < MAX_MAP; i++) {
		len = read_u32_array(pdev, io_map[i].name,
				io_map[i].addr_range,
				(sizeof(io_map[i].addr_range)/sizeof(u32)));
		if (!len) {
			dprintk(VIDC_ERR,
				"Error in reading cp address range\n");
			rc = -EINVAL;
			break;
		}
		partition[0].start = io_map[i].addr_range[0];
		if (i == NS_MAP) {
			partition[0].size =
				io_map[i].addr_range[1] - SHARED_QSIZE;
			partition[1].start =
				partition[0].start + io_map[i].addr_range[1]
					- SHARED_QSIZE;
			partition[1].size = SHARED_QSIZE;
			layout.npartitions = 2;
			layout.is_secure = 0;
		} else {
			partition[0].size = io_map[i].addr_range[1];
			layout.npartitions = 1;
			layout.is_secure = 1;
		}
		layout.partitions = &partition[0];
		layout.client_name = io_map[i].name;
		layout.domain_flags = 0;
		dprintk(VIDC_DBG, "Registering domain 1 with: %lx, %lx, %s\n",
			partition[0].start, partition[0].size,
			layout.client_name);
		dprintk(VIDC_DBG, "Registering domain 2 with: %lx, %lx, %s\n",
			partition[1].start, partition[1].size,
			layout.client_name);
		io_map[i].domain = msm_register_domain(&layout);
		if (io_map[i].domain < 0) {
			dprintk(VIDC_ERR, "Failed to register cp domain\n");
			rc = -EINVAL;
			break;
		}
	}
	/* There is no api provided as msm_unregister_domain, so
	 * we are not able to unregister the previously
	 * registered domains if any domain registration fails.*/
	BUG_ON(i < MAX_MAP);
	return rc;
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
	if (!device)
		return -EINVAL;

	bus_info = &device->resources.bus_info;

	bus_info->ddr_handle[MSM_VIDC_ENCODER] =
		msm_bus_scale_register_client(&enc_ddr_bus_data);
	if (!bus_info->ddr_handle[MSM_VIDC_ENCODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto err_init_bus;
	}
	bus_info->ddr_handle[MSM_VIDC_DECODER] =
		msm_bus_scale_register_client(&dec_ddr_bus_data);
	if (!bus_info->ddr_handle[MSM_VIDC_DECODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto err_init_bus;
	}
	bus_info->ocmem_handle[MSM_VIDC_ENCODER] =
		msm_bus_scale_register_client(&enc_ocmem_bus_data);
	if (!bus_info->ocmem_handle[MSM_VIDC_ENCODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto err_init_bus;
	}
	bus_info->ocmem_handle[MSM_VIDC_DECODER] =
		msm_bus_scale_register_client(&dec_ocmem_bus_data);
	if (!bus_info->ocmem_handle[MSM_VIDC_DECODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto err_init_bus;
	}
	return rc;
err_init_bus:
	venus_hfi_deinit_bus(device);
	return -EINVAL;
}


static const u32 venus_hfi_bus_table[] = {
	36000,
	110400,
	244800,
	489000,
	783360,
	979200,
};

static int venus_hfi_get_bus_vector(int load)
{
	int num_rows = sizeof(venus_hfi_bus_table)/(sizeof(u32));
	int i, j;
	for (i = 0; i < num_rows; i++) {
		if (load <= venus_hfi_bus_table[i])
			break;
	}
	j = clamp(i, 0, num_rows-1) + 1;
	dprintk(VIDC_DBG, "Required bus = %d\n", j);
	return j;
}

static int venus_hfi_scale_bus(void *dev, int load,
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
				handle, venus_hfi_get_bus_vector(load));
		if (rc)
			dprintk(VIDC_ERR, "Failed to scale bus: %d\n", rc);
	} else {
		dprintk(VIDC_ERR, "Failed to scale bus, mtype: %d\n",
				mtype);
		rc = -EINVAL;
	}

	return rc;
}

static int venus_hfi_set_ocmem(void *dev, struct ocmem_buf *ocmem)
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
	rc = venus_hfi_core_set_resource(device, &rhdr, ocmem);
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
	if (!device || !device->resources.ocmem.buf) {
		dprintk(VIDC_ERR, "%s Invalid params, device:%p\n",
			__func__, device);
		return -EINVAL;
	}
	rhdr.resource_id = VIDC_RESOURCE_OCMEM;
	rhdr.resource_handle = (u32) &device->resources.ocmem;
	rc = venus_hfi_core_release_resource(device, &rhdr);
	if (rc)
		dprintk(VIDC_ERR, "Failed to set OCMEM on driver\n");

	return rc;
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
		if (venus_hfi_set_ocmem(device, buff)) {
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
	if (!ocmem->handle) {
		dprintk(VIDC_WARN, "Failed to register OCMEM notifier.");
		dprintk(VIDC_INFO, " Performance will be impacted\n");
	}
}

static int venus_hfi_alloc_ocmem(void *dev, unsigned long size)
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
		ocmem_buffer = ocmem_allocate_nb(OCMEM_VIDEO, size);
		if (IS_ERR_OR_NULL(ocmem_buffer)) {
			dprintk(VIDC_ERR,
				"ocmem_allocate_nb failed: %d\n",
				(u32) ocmem_buffer);
			rc = -ENOMEM;
		}
		device->resources.ocmem.buf = ocmem_buffer;
		rc = venus_hfi_set_ocmem(device, ocmem_buffer);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to set ocmem: %d\n", rc);
			goto ocmem_set_failed;
		}
	} else
		dprintk(VIDC_DBG,
			"OCMEM is enough. reqd: %lu, available: %lu\n",
			size, ocmem_buffer->len);

ocmem_set_failed:
	return rc;
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

static int venus_hfi_is_ocmem_present(void *dev)
{
	struct venus_hfi_device *device = dev;
	if (!device) {
		dprintk(VIDC_ERR, "%s invalid device handle %p",
			__func__, device);
		return -EINVAL;
	}

	return device->resources.ocmem.buf ? 1 : 0;
}

static void venus_hfi_deinit_ocmem(struct venus_hfi_device *device)
{
	if (device->resources.ocmem.handle)
		ocmem_notifier_unregister(device->resources.ocmem.handle,
				&device->resources.ocmem.vidc_ocmem_nb);
}


static int venus_hfi_init_resources(struct venus_hfi_device *device,
				struct platform_device *pdev)
{
	int rc = 0;

	rc = venus_hfi_init_clocks(pdev, device);
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

	rc = venus_hfi_register_iommu_domains(device, pdev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register iommu domains: %d\n", rc);
		goto err_register_iommu_domain;
	}

	venus_hfi_ocmem_init(device);
	return rc;

err_register_iommu_domain:
	venus_hfi_deinit_bus(device);
err_init_bus:
	venus_hfi_deinit_clocks(device);
err_init_clocks:
	return rc;
}

static void venus_hfi_deinit_resources(struct venus_hfi_device *device)
{
	venus_hfi_deinit_ocmem(device);
	venus_hfi_deinit_bus(device);
	venus_hfi_deinit_clocks(device);
}

static int venus_hfi_iommu_attach(struct venus_hfi_device *device)
{
	int rc;
	struct iommu_domain *domain;
	int i;
	struct msm_vidc_iommu_info *io_map;
	struct device *dev;

	if (!device)
		return -EINVAL;

	for (i = 0; i < MAX_MAP; i++) {
		io_map = &device->resources.io_map[i];
		dev = msm_iommu_get_ctx(io_map->ctx);
		domain = msm_get_iommu_domain(io_map->domain);
		if (IS_ERR_OR_NULL(domain)) {
			dprintk(VIDC_ERR,
				"Failed to get domain: %s\n", io_map->name);
			rc = PTR_ERR(domain);
			break;
		}
		rc = iommu_attach_device(domain, dev);
		if (rc) {
			dprintk(VIDC_ERR,
				"IOMMU attach failed: %s\n", io_map->name);
			break;
		}
	}
	if (i < MAX_MAP) {
		i--;
		for (; i >= 0; i--) {
			io_map = &device->resources.io_map[i];
			dev = msm_iommu_get_ctx(io_map->ctx);
			domain = msm_get_iommu_domain(io_map->domain);
			if (dev && domain)
				iommu_detach_device(domain, dev);
		}
	}
	return rc;
}

static void venus_hfi_iommu_detach(struct venus_hfi_device *device)
{
	struct device *dev;
	struct iommu_domain *domain;
	struct msm_vidc_iommu_info *io_map;
	int i;

	if (!device) {
		dprintk(VIDC_ERR, "Invalid paramter: %p\n", device);
		return;
	}

	for (i = 0; i < MAX_MAP; i++) {
		io_map = &device->resources.io_map[i];
		dev = msm_iommu_get_ctx(io_map->ctx);
		domain = msm_get_iommu_domain(io_map->domain);
		if (dev && domain)
			iommu_detach_device(domain, dev);
	}
}

static int venus_hfi_get_domain(void *dev, enum msm_vidc_io_maps iomap)
{
	struct venus_hfi_device *device = dev;
	if (!device || iomap < CP_MAP || iomap >= MAX_MAP) {
		dprintk(VIDC_ERR, "%s: Invalid parameter: %p iomap: %d\n",
				__func__, device, iomap);
		return -EINVAL;
	}
	return device->resources.io_map[iomap].domain;
}

static int venus_hfi_iommu_get_map(void *dev,
			struct msm_vidc_iommu_info maps[MAX_MAP])
{
	int i = 0;
	struct venus_hfi_device *device = dev;

	if (!device || !maps) {
		dprintk(VIDC_ERR, "%s: Invalid param device: %p maps: %p\n",
		 __func__, device, maps);
		return -EINVAL;
	}

	for (i = 0; i < MAX_MAP; i++)
		maps[i] = device->resources.io_map[i];

	return 0;
}

static int protect_cp_mem(struct venus_hfi_device *device)
{
	struct tzbsp_memprot memprot;
	unsigned int resp = 0;
	int rc = 0;
	struct msm_vidc_iommu_info *io_map;

	if (!device)
		return -EINVAL;

	io_map = device->resources.io_map;
	if (!io_map) {
		dprintk(VIDC_ERR, "invalid params: %p\n", io_map);
		return -EINVAL;
	}
	memprot.cp_start = 0x0;
	memprot.cp_size = io_map[CP_MAP].addr_range[0] +
			io_map[CP_MAP].addr_range[1];
	memprot.cp_nonpixel_start = 0;
	memprot.cp_nonpixel_size = 0;

	rc = scm_call(SCM_SVC_CP, TZBSP_MEM_PROTECT_VIDEO_VAR, &memprot,
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

	if (!device) {
		dprintk(VIDC_ERR, "%s Invalid paramter: %p\n",
			__func__, device);
		return -EINVAL;
	}

	if (!device->resources.fw.cookie)
		device->resources.fw.cookie = subsystem_get("venus");

	if (IS_ERR_OR_NULL(device->resources.fw.cookie)) {
		dprintk(VIDC_ERR, "Failed to download firmware\n");
		rc = -ENOMEM;
		goto fail_load_fw;
	}
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
		goto fail_iommu_attach;
	}

	rc = venus_hfi_iommu_attach(device);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to attach iommu");
		goto fail_iommu_attach;
	}
	return rc;
fail_iommu_attach:
	venus_hfi_disable_clks(device);
fail_enable_clks:
	subsystem_put(device->resources.fw.cookie);
	device->resources.fw.cookie = NULL;
fail_load_fw:
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
		venus_hfi_iommu_detach(device);
		venus_hfi_disable_clks(device);
		subsystem_put(device->resources.fw.cookie);
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

int venus_hfi_get_stride_scanline(int color_fmt,
	int width, int height, int *stride, int *scanlines) {
	*stride = VENUS_Y_STRIDE(color_fmt, width);
	*scanlines = VENUS_Y_SCANLINES(color_fmt, height);
	return 0;
}

static void *venus_hfi_add_device(u32 device_id, struct platform_device *pdev,
		void (*callback) (enum command_response cmd, void *data))
{
	struct venus_hfi_device *hdevice = NULL;
	int rc = 0;

	if (device_id || !pdev || !callback) {
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

	rc = venus_hfi_init_regs_and_interrupts(hdevice, pdev);
	if (rc)
		goto err_init_regs;

	INIT_LIST_HEAD(&hal_ctxt.dev_head);
	INIT_LIST_HEAD(&hdevice->list);
	list_add_tail(&hdevice->list, &hal_ctxt.dev_head);
	hal_ctxt.dev_count++;
	hdevice->device_id = device_id;

	hdevice->callback = callback;

	hdevice->vidc_workq = create_singlethread_workqueue(
		"msm_vidc_workerq");
	if (!hdevice->vidc_workq) {
		dprintk(VIDC_ERR, ": create workq failed\n");
		goto error_createq;
	}

	return (void *) hdevice;
error_createq:
	hal_ctxt.dev_count--;
	list_del(&hal_ctxt.dev_head);
err_init_regs:
	kfree(hdevice);
err_alloc:
	return NULL;
}

static void *venus_hfi_get_device(u32 device_id,
				struct platform_device *pdev,
				hfi_cmd_response_callback callback)
{
	struct venus_hfi_device *device;
	int rc = 0;

	if (!pdev || !callback) {
		dprintk(VIDC_ERR, "Invalid params: %p %p\n", pdev, callback);
		return NULL;
	}

	device = venus_hfi_add_device(device_id, pdev, &handle_cmd_response);
	if (!device) {
		dprintk(VIDC_ERR, "Failed to create HFI device\n");
		return NULL;
	}

	rc = venus_hfi_init_resources(device, pdev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init resources: %d\n", rc);
		goto err_fail_init_res;
	}
	return device;

err_fail_init_res:
	venus_hfi_delete_device(device);
	return NULL;
}

void venus_hfi_delete_device(void *device)
{
	struct venus_hfi_device *close, *dev;

	if (device) {
		venus_hfi_deinit_resources(device);
		dev = (struct venus_hfi_device *) device;
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
	hdev->unset_ocmem = venus_hfi_unset_ocmem;
	hdev->alloc_ocmem = venus_hfi_alloc_ocmem;
	hdev->free_ocmem = venus_hfi_free_ocmem;
	hdev->is_ocmem_present = venus_hfi_is_ocmem_present;
	hdev->get_domain = venus_hfi_get_domain;
	hdev->iommu_get_map = venus_hfi_iommu_get_map;
	hdev->load_fw = venus_hfi_load_fw;
	hdev->unload_fw = venus_hfi_unload_fw;
	hdev->get_fw_info = venus_hfi_get_fw_info;
	hdev->get_stride_scanline = venus_hfi_get_stride_scanline;
}

int venus_hfi_initialize(struct hfi_device *hdev, u32 device_id,
	struct platform_device *pdev, hfi_cmd_response_callback callback)
{
	int rc = 0;

	if (!hdev || !callback) {
		dprintk(VIDC_ERR, "Invalid params: %p %p\n", pdev, callback);
		rc = -EINVAL;
		goto err_venus_hfi_init;
	}
	hdev->hfi_device_data = venus_hfi_get_device(device_id, pdev, callback);

	venus_init_hfi_callbacks(hdev);

err_venus_hfi_init:
	return rc;
}

