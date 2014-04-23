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

#ifndef __H_VENUS_HFI_H__
#define __H_VENUS_HFI_H__

#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <mach/ocmem.h>
#include <mach/iommu_domains.h>

#include "vidc_hfi_api.h"
#include "msm_smem.h"
#include "vidc_hfi_helper.h"
#include "vidc_hfi_api.h"
#include "vidc_hfi.h"
#include "msm_vidc_resources.h"

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
	u32 *mem_map_table_base_addr;
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
	(void *)((((u32)ptr) + sizeof(struct hfi_queue_table_header)) + \
		(i * sizeof(struct hfi_queue_header)))

#define QDSS_SIZE 4096
#define SFR_SIZE 4096

#define QUEUE_SIZE (VIDC_IFACEQ_TABLE_SIZE + \
	(VIDC_IFACEQ_QUEUE_SIZE * VIDC_IFACEQ_NUMQ))

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

enum bus_index {
	BUS_IDX_ENC_OCMEM,
	BUS_IDX_DEC_OCMEM,
	BUS_IDX_ENC_DDR,
	BUS_IDX_DEC_DDR,
	BUS_IDX_MAX
};

struct vidc_mem_addr {
	u8 *align_device_addr;
	u8 *align_virtual_addr;
	u32 mem_size;
	struct msm_smem *mem_data;
};

struct vidc_iface_q_info {
	void *q_hdr;
	struct vidc_mem_addr q_array;
};

/* Internal data used in vidc_hal not exposed to msm_vidc*/

struct hal_data {
	u32 irq;
	u32 device_base_addr;
	u8 *register_base_addr;
};

enum vidc_clocks {
	VCODEC_CLK,
	VCODEC_AHB_CLK,
	VCODEC_AXI_CLK,
	VCODEC_OCMEM_CLK,
	VCODEC_MAX_CLKS
};

struct venus_core_clock {
	char name[VIDC_MAX_NAME_LENGTH];
	struct clk *clk;
	u32 count;
	struct load_freq_table load_freq_tbl[8];
};

struct venus_bus_info {
	u32 ddr_handle[MSM_VIDC_MAX_DEVICES];
	u32 ocmem_handle[MSM_VIDC_MAX_DEVICES];
};

struct on_chip_mem {
	struct ocmem_buf *buf;
	struct notifier_block vidc_ocmem_nb;
	void *handle;
};

struct venus_resources {
	struct msm_vidc_fw fw;
	struct venus_core_clock clock[VCODEC_MAX_CLKS];
	struct venus_bus_info bus_info;
	struct on_chip_mem ocmem;
};

struct venus_hfi_device {
	struct list_head list;
	struct list_head sess_head;
	u32 intr_status;
	u32 device_id;
	u32 clk_load;
	u32 bus_load[MSM_VIDC_MAX_DEVICES];
	unsigned long ocmem_size;
	u32 clocks_enabled;
	u32 power_enabled;
	enum vidc_clocks clk_gating_level;
	struct mutex read_lock;
	struct mutex write_lock;
	struct mutex clk_pwr_lock;
	struct mutex session_lock;
	msm_vidc_callback callback;
	struct vidc_mem_addr iface_q_table;
	struct vidc_mem_addr qdss;
	struct vidc_mem_addr sfr;
	struct vidc_mem_addr mem_addr;
	struct vidc_iface_q_info iface_queues[VIDC_IFACEQ_NUMQ];
	struct smem_client *hal_client;
	struct hal_data *hal_data;
	struct workqueue_struct *vidc_workq;
	struct workqueue_struct *venus_pm_workq;
	int spur_count;
	int reg_count;
	u32 base_addr;
	u32 register_base;
	u32 register_size;
	u32 irq;
	int clk_cnt;
	int pwr_cnt;
	struct venus_resources resources;
	struct msm_vidc_platform_resources *res;
	struct regulator *gdsc;
};

void venus_hfi_delete_device(void *device);
int venus_hfi_initialize(struct hfi_device *hdev, u32 device_id,
		struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback);
#endif
