/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "cam_cdm.h"

struct cam_cdm_pid_mid_data cdm_hw_2_1_pid_mid_data = {
	.cdm_pid = 2,
	.cdm_mid = 0,
	.ope_cdm_pid = 0,
	.ope_cdm_mid = 2,
};

struct cam_cdm_bl_pending_req_reg_params cdm_hw_2_1_bl_pending_req0 = {
	.rb_offset = 0x6c,
	.rb_mask = 0x1ff,
	.rb_num_fifo = 0x2,
	.rb_next_fifo_shift = 0x10,
};

struct cam_cdm_bl_pending_req_reg_params cdm_hw_2_1_bl_pending_req1 = {
	.rb_offset = 0x70,
	.rb_mask = 0x1ff,
	.rb_num_fifo = 0x2,
	.rb_next_fifo_shift = 0x10,
};

static struct cam_cdm_irq_regs cdm_hw_2_1_irq0 = {
	.irq_mask = 0x30,
	.irq_clear = 0x34,
	.irq_clear_cmd = 0x38,
	.irq_set = 0x3c,
	.irq_set_cmd = 0x40,
	.irq_status = 0x44,
};

static struct cam_cdm_irq_regs cdm_hw_2_1_irq1 = {
	.irq_mask = 0x130,
	.irq_clear = 0x134,
	.irq_clear_cmd = 0x138,
	.irq_set = 0x13c,
	.irq_set_cmd = 0x140,
	.irq_status = 0x144,
};

static struct cam_cdm_irq_regs cdm_hw_2_1_irq2 = {
	.irq_mask = 0x230,
	.irq_clear = 0x234,
	.irq_clear_cmd = 0x238,
	.irq_set = 0x23c,
	.irq_set_cmd = 0x240,
	.irq_status = 0x244,
};

static struct cam_cdm_irq_regs cdm_hw_2_1_irq3 = {
	.irq_mask = 0x330,
	.irq_clear = 0x334,
	.irq_clear_cmd = 0x338,
	.irq_set = 0x33c,
	.irq_set_cmd = 0x340,
	.irq_status = 0x344,
};

static struct cam_cdm_bl_fifo_regs cdm_hw_2_1_bl_fifo0 = {
	.bl_fifo_base = 0x50,
	.bl_fifo_len = 0x54,
	.bl_fifo_store = 0x58,
	.bl_fifo_cfg = 0x5c,
};

static struct cam_cdm_bl_fifo_regs cdm_hw_2_1_bl_fifo1 = {
	.bl_fifo_base = 0x150,
	.bl_fifo_len = 0x154,
	.bl_fifo_store = 0x158,
	.bl_fifo_cfg = 0x15c,
};

static struct cam_cdm_bl_fifo_regs cdm_hw_2_1_bl_fifo2 = {
	.bl_fifo_base = 0x250,
	.bl_fifo_len = 0x254,
	.bl_fifo_store = 0x258,
	.bl_fifo_cfg = 0x25c,
};

static struct cam_cdm_bl_fifo_regs cdm_hw_2_1_bl_fifo3 = {
	.bl_fifo_base = 0x350,
	.bl_fifo_len = 0x354,
	.bl_fifo_store = 0x358,
	.bl_fifo_cfg = 0x35c,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg0 = {
	.scratch_reg = 0x90,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg1 = {
	.scratch_reg = 0x94,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg2 = {
	.scratch_reg = 0x98,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg3 = {
	.scratch_reg = 0x9c,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg4 = {
	.scratch_reg = 0xa0,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg5 = {
	.scratch_reg = 0xa4,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg6 = {
	.scratch_reg = 0xa8,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg7 = {
	.scratch_reg = 0xac,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg8 = {
	.scratch_reg = 0xb0,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg9 = {
	.scratch_reg = 0xb4,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg10  = {
	.scratch_reg = 0xb8,
};

static struct cam_cdm_scratch_reg cdm_2_1_scratch_reg11  = {
	.scratch_reg = 0xbc,
};

static struct cam_cdm_perf_mon_regs cdm_2_1_perf_mon0 = {
	.perf_mon_ctrl = 0x110,
	.perf_mon_0 = 0x114,
	.perf_mon_1 = 0x118,
	.perf_mon_2 = 0x11c,
};

static struct cam_cdm_perf_mon_regs cdm_2_1_perf_mon1 = {
	.perf_mon_ctrl = 0x120,
	.perf_mon_0 = 0x124,
	.perf_mon_1 = 0x128,
	.perf_mon_2 = 0x12c,
};

static struct cam_cdm_comp_wait_status cdm_2_1_comp_wait_status0 = {
	.comp_wait_status = 0x88,
};

static struct cam_cdm_comp_wait_status cdm_2_1_comp_wait_status1 = {
	.comp_wait_status = 0x8c,
};

static struct cam_cdm_icl_data_regs cdm_2_1_icl_data = {
	.icl_last_data_0 = 0x1c0,
	.icl_last_data_1 = 0x1c4,
	.icl_last_data_2 = 0x1c8,
	.icl_inv_data = 0x1cc,
};

static struct cam_cdm_icl_misc_regs cdm_2_1_icl_misc = {
	.icl_inv_bl_addr = 0x1d0,
	.icl_status = 0x1d8,
};

static struct cam_cdm_icl_regs cdm_2_1_icl = {
	.data_regs = &cdm_2_1_icl_data,
	.misc_regs = &cdm_2_1_icl_misc,
};

static struct cam_cdm_common_regs cdm_hw_2_1_cmn_reg_offset = {
	.cdm_hw_version = 0x0,
	.cam_version = NULL,
	.rst_cmd = 0x10,
	.cgc_cfg = 0x14,
	.core_cfg = 0x18,
	.core_en = 0x1c,
	.fe_cfg = 0x20,
	.irq_context_status = 0x2c,
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
			&cdm_hw_2_1_bl_pending_req0,
			&cdm_hw_2_1_bl_pending_req1,
		},
	.comp_wait = {
			&cdm_2_1_comp_wait_status0,
			&cdm_2_1_comp_wait_status1,
		},
	.perf_mon = {
			&cdm_2_1_perf_mon0,
			&cdm_2_1_perf_mon1,
		},
	.scratch = {
			&cdm_2_1_scratch_reg0,
			&cdm_2_1_scratch_reg1,
			&cdm_2_1_scratch_reg2,
			&cdm_2_1_scratch_reg3,
			&cdm_2_1_scratch_reg4,
			&cdm_2_1_scratch_reg5,
			&cdm_2_1_scratch_reg6,
			&cdm_2_1_scratch_reg7,
			&cdm_2_1_scratch_reg8,
			&cdm_2_1_scratch_reg9,
			&cdm_2_1_scratch_reg10,
			&cdm_2_1_scratch_reg11,
		},
	.perf_reg = NULL,
	.icl_reg = &cdm_2_1_icl,
	.spare = 0x3fc,
	.priority_group_bit_offset = 20,
	.cdm_pid_mid_info = &cdm_hw_2_1_pid_mid_data,
};

static struct cam_cdm_common_reg_data cdm_hw_2_1_cmn_reg_data = {
	.num_bl_fifo = 0x4,
	.num_bl_fifo_irq = 0x4,
	.num_bl_pending_req_reg = 0x2,
	.num_scratch_reg = 0xc,
};

struct cam_cdm_hw_reg_offset cam_cdm_2_1_reg_offset = {
	.cmn_reg = &cdm_hw_2_1_cmn_reg_offset,
	.bl_fifo_reg = {
			&cdm_hw_2_1_bl_fifo0,
			&cdm_hw_2_1_bl_fifo1,
			&cdm_hw_2_1_bl_fifo2,
			&cdm_hw_2_1_bl_fifo3,
		},
	.irq_reg = {
			&cdm_hw_2_1_irq0,
			&cdm_hw_2_1_irq1,
			&cdm_hw_2_1_irq2,
			&cdm_hw_2_1_irq3,
		},
	.reg_data = &cdm_hw_2_1_cmn_reg_data,
};
