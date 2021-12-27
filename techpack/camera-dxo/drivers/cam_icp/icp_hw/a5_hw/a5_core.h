/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef CAM_A5_CORE_H
#define CAM_A5_CORE_H

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "cam_a5_hw_intf.h"

#define A5_QGIC_BASE            0
#define A5_SIERRA_BASE          1
#define A5_CSR_BASE             2

#define A5_HOST_INT             0x1
#define A5_WDT_0                0x2
#define A5_WDT_1                0x4

#define ICP_SIERRA_A5_CSR_ACCESS 0x3C

#define ELF_GUARD_PAGE          (2 * 1024 * 1024)

struct cam_a5_device_hw_info {
	uint32_t hw_ver;
	uint32_t nsec_reset;
	uint32_t a5_control;
	uint32_t a5_host_int_en;
	uint32_t a5_host_int;
	uint32_t a5_host_int_clr;
	uint32_t a5_host_int_status;
	uint32_t a5_host_int_set;
	uint32_t host_a5_int;
	uint32_t fw_version;
	uint32_t init_req;
	uint32_t init_response;
	uint32_t shared_mem_ptr;
	uint32_t shared_mem_size;
	uint32_t qtbl_ptr;
	uint32_t uncached_heap_ptr;
	uint32_t uncached_heap_size;
	uint32_t a5_status;
};

/**
 * struct cam_a5_device_hw_info
 * @a5_hw_info: A5 hardware info
 * @fw_elf: start address of fw start with elf header
 * @fw: start address of fw blob
 * @fw_buf: smmu alloc/mapped fw buffer
 * @fw_buf_len: fw buffer length
 * @query_cap: A5 query info from firmware
 * @a5_acquire: Acquire information of A5
 * @irq_cb: IRQ callback
 * @cpas_handle: CPAS handle for A5
 * @cpast_start: state variable for cpas
 */
struct cam_a5_device_core_info {
	struct cam_a5_device_hw_info *a5_hw_info;
	const struct firmware *fw_elf;
	void *fw;
	uint32_t fw_buf;
	uintptr_t fw_kva_addr;
	uint64_t fw_buf_len;
	struct cam_icp_a5_query_cap query_cap;
	struct cam_icp_a5_acquire_dev a5_acquire[8];
	struct cam_icp_a5_set_irq_cb irq_cb;
	uint32_t cpas_handle;
	bool cpas_start;
};

int cam_a5_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_a5_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_a5_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);

irqreturn_t cam_a5_irq(int irq_num, void *data);
#endif /* CAM_A5_CORE_H */
