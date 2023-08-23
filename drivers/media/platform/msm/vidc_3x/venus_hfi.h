/*
 * Copyright (c) 2012-2015, 2018-2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __H_VENUS_HFI_H__
#define __H_VENUS_HFI_H__

#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/spinlock.h>
#include "vmem/vmem.h"
#include "vidc_hfi_api.h"
#include "vidc_hfi_helper.h"
#include "vidc_hfi_api.h"
#include "vidc_hfi.h"
#include "msm_vidc_resources.h"
#include "hfi_packetization.h"

#define HFI_MASK_QHDR_TX_TYPE			0xFF000000
#define HFI_MASK_QHDR_RX_TYPE			0x00FF0000
#define HFI_MASK_QHDR_PRI_TYPE			0x0000FF00
#define HFI_MASK_QHDR_Q_ID_TYPE			0x000000FF
#define HFI_Q_ID_HOST_TO_CTRL_CMD_Q		0x00
#define HFI_Q_ID_CTRL_TO_HOST_MSG_Q		0x01
#define HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q	0x02
#define HFI_MASK_QHDR_STATUS			0x000000FF

#define VIDC_MAX_UNCOMPRESSED_FMT_PLANES	3

#define VIDC_IFACEQ_NUMQ					3
#define VIDC_IFACEQ_CMDQ_IDX				0
#define VIDC_IFACEQ_MSGQ_IDX				1
#define VIDC_IFACEQ_DBGQ_IDX				2
#define VIDC_IFACEQ_MAX_BUF_COUNT			50
#define VIDC_IFACE_MAX_PARALLEL_CLNTS		16
#define VIDC_IFACEQ_DFLT_QHDR				0x01010000

#define VIDC_MAX_NAME_LENGTH 64
#define VIDC_MAX_PC_SKIP_COUNT 10
struct hfi_queue_table_header {
	u32 qtbl_version;
	u32 qtbl_size;
	u32 qtbl_qhdr0_offset;
	u32 qtbl_qhdr_size;
	u32 qtbl_num_q;
	u32 qtbl_num_active_q;
};

struct hfi_queue_header {
	u32 qhdr_status;
	u32 qhdr_start_addr;
	u32 qhdr_type;
	u32 qhdr_q_size;
	u32 qhdr_pkt_size;
	u32 qhdr_pkt_drop_cnt;
	u32 qhdr_rx_wm;
	u32 qhdr_tx_wm;
	u32 qhdr_rx_req;
	u32 qhdr_tx_req;
	u32 qhdr_rx_irq_status;
	u32 qhdr_tx_irq_status;
	u32 qhdr_read_idx;
	u32 qhdr_write_idx;
};

struct hfi_mem_map_table {
	u32 mem_map_num_entries;
	u32 mem_map_table_base_addr;
};

struct hfi_mem_map {
	u32 virtual_addr;
	u32 physical_addr;
	u32 size;
	u32 attr;
};

#define VIDC_IFACEQ_TABLE_SIZE (sizeof(struct hfi_queue_table_header) \
	+ sizeof(struct hfi_queue_header) * VIDC_IFACEQ_NUMQ)

#define VIDC_IFACEQ_QUEUE_SIZE	(VIDC_IFACEQ_MAX_PKT_SIZE *  \
	VIDC_IFACEQ_MAX_BUF_COUNT * VIDC_IFACE_MAX_PARALLEL_CLNTS)

#define VIDC_IFACEQ_GET_QHDR_START_ADDR(ptr, i)     \
	(void *)((ptr + sizeof(struct hfi_queue_table_header)) + \
		(i * sizeof(struct hfi_queue_header)))

#define QDSS_SIZE 4096
#define SFR_SIZE 4096

#define QUEUE_SIZE (VIDC_IFACEQ_TABLE_SIZE + \
	(VIDC_IFACEQ_QUEUE_SIZE * VIDC_IFACEQ_NUMQ))

#define ALIGNED_QDSS_SIZE ALIGN(QDSS_SIZE, SZ_4K)
#define ALIGNED_SFR_SIZE ALIGN(SFR_SIZE, SZ_4K)
#define ALIGNED_QUEUE_SIZE ALIGN(QUEUE_SIZE, SZ_4K)
#define SHARED_QSIZE ALIGN(ALIGNED_SFR_SIZE + ALIGNED_QUEUE_SIZE + \
			ALIGNED_QDSS_SIZE, SZ_1M)

enum vidc_hw_reg {
	VIDC_HWREG_CTRL_STATUS =  0x1,
	VIDC_HWREG_QTBL_INFO =  0x2,
	VIDC_HWREG_QTBL_ADDR =  0x3,
	VIDC_HWREG_CTRLR_RESET =  0x4,
	VIDC_HWREG_IFACEQ_FWRXREQ =  0x5,
	VIDC_HWREG_IFACEQ_FWTXREQ =  0x6,
	VIDC_HWREG_VHI_SOFTINTEN =  0x7,
	VIDC_HWREG_VHI_SOFTINTSTATUS =  0x8,
	VIDC_HWREG_VHI_SOFTINTCLR =  0x9,
	VIDC_HWREG_HVI_SOFTINTEN =  0xA,
};

struct vidc_mem_addr {
	phys_addr_t align_device_addr;
	u8 *align_virtual_addr;
	u32 mem_size;
	struct msm_smem mem_data;
};

struct vidc_iface_q_info {
	void *q_hdr;
	struct vidc_mem_addr q_array;
};

/*
 * These are helper macros to iterate over various lists within
 * venus_hfi_device->res.  The intention is to cut down on a lot of boiler-plate
 * code
 */

/* Read as "for each 'thing' in a set of 'thingies'" */
#define venus_hfi_for_each_thing(__device, __thing, __thingy) \
	venus_hfi_for_each_thing_continue(__device, __thing, __thingy, 0)

#define venus_hfi_for_each_thing_reverse(__device, __thing, __thingy) \
	venus_hfi_for_each_thing_reverse_continue(__device, __thing, __thingy, \
			(__device)->res->__thingy##_set.count - 1)

/* TODO: the __from parameter technically not required since we can figure it
 * out with some pointer magic (i.e. __thing - __thing##_tbl[0]).  If this macro
 * sees extensive use, probably worth cleaning it up but for now omitting it
 * since it introduces unneccessary complexity.
 */
#define venus_hfi_for_each_thing_continue(__device, __thing, __thingy, __from) \
	for (__thing = &(__device)->res->\
			__thingy##_set.__thingy##_tbl[__from]; \
		__thing < &(__device)->res->__thingy##_set.__thingy##_tbl[0] + \
			((__device)->res->__thingy##_set.count - __from); \
		++__thing)

#define venus_hfi_for_each_thing_reverse_continue(__device, __thing, __thingy, \
		__from) \
	for (__thing = &(__device)->res->\
			__thingy##_set.__thingy##_tbl[__from]; \
		__thing >= &(__device)->res->__thingy##_set.__thingy##_tbl[0]; \
		--__thing)

/* Regular set helpers */
#define venus_hfi_for_each_regulator(__device, __rinfo) \
	venus_hfi_for_each_thing(__device, __rinfo, regulator)

#define venus_hfi_for_each_regulator_reverse(__device, __rinfo) \
	venus_hfi_for_each_thing_reverse(__device, __rinfo, regulator)

#define venus_hfi_for_each_regulator_reverse_continue(__device, __rinfo, \
		__from) \
	venus_hfi_for_each_thing_reverse_continue(__device, __rinfo, \
			regulator, __from)

/* Clock set helpers */
#define venus_hfi_for_each_clock(__device, __cinfo) \
	venus_hfi_for_each_thing(__device, __cinfo, clock)

#define venus_hfi_for_each_clock_reverse(__device, __cinfo) \
	venus_hfi_for_each_thing_reverse(__device, __cinfo, clock)

/* Bus set helpers */
#define venus_hfi_for_each_bus(__device, __binfo) \
	venus_hfi_for_each_thing(__device, __binfo, bus)
#define venus_hfi_for_each_bus_reverse(__device, __binfo) \
	venus_hfi_for_each_thing_reverse(__device, __binfo, bus)


/* Internal data used in vidc_hal not exposed to msm_vidc*/
struct hal_data {
	u32 irq;
	phys_addr_t firmware_base;
	u8 __iomem *register_base;
	u32 register_size;
};

struct imem {
	enum imem_type type;
	union {
		phys_addr_t vmem;
	};
};

struct venus_resources {
	struct msm_vidc_fw fw;
	struct imem imem;
};

enum venus_hfi_state {
	VENUS_STATE_DEINIT = 1,
	VENUS_STATE_INIT,
};

struct venus_hfi_device {
	struct list_head list;
	struct list_head sess_head;
	u32 intr_status;
	u32 device_id;
	u32 clk_freq;
	u32 last_packet_type;
	unsigned long clk_bitrate;
	unsigned long scaled_rate;
	struct msm_vidc_gov_data bus_vote;
	bool power_enabled;
	struct mutex lock;
	msm_vidc_callback callback;
	struct vidc_mem_addr iface_q_table;
	struct vidc_mem_addr qdss;
	struct vidc_mem_addr sfr;
	struct vidc_mem_addr mem_addr;
	struct vidc_iface_q_info iface_queues[VIDC_IFACEQ_NUMQ];
	struct hal_data *hal_data;
	struct workqueue_struct *vidc_workq;
	struct workqueue_struct *venus_pm_workq;
	int spur_count;
	int reg_count;
	struct venus_resources resources;
	struct msm_vidc_platform_resources *res;
	enum venus_hfi_state state;
	struct hfi_packetization_ops *pkt_ops;
	enum hfi_packetization_type packetization_type;
	struct msm_vidc_cb_info *response_pkt;
	struct pm_qos_request qos;
	unsigned int skip_pc_count;
	struct msm_vidc_capability *sys_init_capabilities;
};

void venus_hfi_delete_device(void *device);

int venus_hfi_initialize(struct hfi_device *hdev, u32 device_id,
		struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback);
bool venus_hfi_is_session_supported(unsigned long sessions_supported,
		enum vidc_vote_data_session session_type);

#endif
