/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "cam_cdm.h"

static struct cam_version_reg cdm_hw_1_0_titan_version = {
	.hw_version = 0x4,
};

struct cam_cdm_bl_pending_req_reg_params cdm_hw_1_0_bl_pending_req0 = {
	.rb_offset = 0x6c,
	.rb_mask = 0x7F,
	.rb_num_fifo = 0x1,
	.rb_next_fifo_shift = 0x0,
};

static struct cam_cdm_irq_regs cdm_hw_1_0_irq0 = {
	.irq_mask = 0x30,
	.irq_clear = 0x34,
	.irq_clear_cmd = 0x38,
	.irq_set = 0x3c,
	.irq_set_cmd = 0x40,
	.irq_status = 0x44,
};

static struct cam_cdm_bl_fifo_regs cdm_hw_1_0_bl_fifo0 = {
	.bl_fifo_base = 0x50,
	.bl_fifo_len = 0x54,
	.bl_fifo_store = 0x58,
	.bl_fifo_cfg = 0x5c,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg0 = {
	.scratch_reg = 0x90,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg1 = {
	.scratch_reg = 0x94,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg2 = {
	.scratch_reg = 0x98,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg3 = {
	.scratch_reg = 0x9c,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg4 = {
	.scratch_reg = 0xa0,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg5 = {
	.scratch_reg = 0xa4,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg6 = {
	.scratch_reg = 0xa8,
};

static struct cam_cdm_scratch_reg cdm_1_0_scratch_reg7 = {
	.scratch_reg = 0xac,
};

static struct cam_cdm_perf_mon_regs cdm_1_0_perf_mon0 = {
	.perf_mon_ctrl = 0x110,
	.perf_mon_0 = 0x114,
	.perf_mon_1 = 0x118,
	.perf_mon_2 = 0x11c,
};

static struct cam_cdm_common_regs cdm_hw_1_0_cmn_reg_offset = {
	.cdm_hw_version = 0x0,
	.cam_version = &cdm_hw_1_0_titan_version,
	.rst_cmd = 0x10,
	.cgc_cfg = 0x14,
	.core_cfg = 0x18,
	.core_en = 0x1c,
	.fe_cfg = 0x20,
	.bl_fifo_rb = 0x60,
	.bl_fifo_base_rb = 0x64,
	.bl_fifo_len_rb = 0x68,
	.usr_data = 0x80,
	.wait_status = 0x84,
	.last_ahb_addr = 0xd0,
	.last_ahb_data = 0xd4,
	.core_debug = 0xd8,
	.last_ahb_err_addr = 0xe0,
	.last_ahb_err_data = 0xe4,
	.current_bl_base = 0xe8,
	.current_bl_len = 0xec,
	.current_used_ahb_base = 0xf0,
	.debug_status = 0xf4,
	.bus_misr_cfg0 = 0x100,
	.bus_misr_cfg1 = 0x104,
	.bus_misr_rd_val = 0x108,
	.pending_req = {
			&cdm_hw_1_0_bl_pending_req0,
			NULL,
		},
	.comp_wait = { NULL, NULL },
	.perf_mon = {
			&cdm_1_0_perf_mon0,
			NULL,
		},
	.scratch = {
			&cdm_1_0_scratch_reg0,
			&cdm_1_0_scratch_reg1,
			&cdm_1_0_scratch_reg2,
			&cdm_1_0_scratch_reg3,
			&cdm_1_0_scratch_reg4,
			&cdm_1_0_scratch_reg5,
			&cdm_1_0_scratch_reg6,
			&cdm_1_0_scratch_reg7,
			NULL,
			NULL,
			NULL,
			NULL,
		},
	.perf_reg = NULL,
	.icl_reg = NULL,
	.spare = 0x200,
};

static struct cam_cdm_common_reg_data cdm_hw_1_0_cmn_reg_data = {
	.num_bl_fifo = 0x1,
	.num_bl_fifo_irq = 0x1,
	.num_bl_pending_req_reg = 0x1,
	.num_scratch_reg = 0x8,
};

static struct cam_cdm_hw_reg_offset cam_cdm_1_0_reg_offset = {
	.cmn_reg = &cdm_hw_1_0_cmn_reg_offset,
	.bl_fifo_reg = {
			&cdm_hw_1_0_bl_fifo0,
			NULL,
			NULL,
			NULL,
		},
	.irq_reg = {
			&cdm_hw_1_0_irq0,
			NULL,
			NULL,
			NULL,
		},
	.reg_data = &cdm_hw_1_0_cmn_reg_data,
};
