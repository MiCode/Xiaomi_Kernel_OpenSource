/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_VFE170_H_
#define _CAM_VFE170_H_

#include "cam_vfe_camif_ver2.h"
#include "cam_vfe_bus_ver2.h"
#include "cam_irq_controller.h"
#include "cam_vfe_top_ver2.h"
#include "cam_vfe_core.h"

static struct cam_irq_register_set vfe170_top_irq_reg_set[2] = {
	{
		.mask_reg_offset   = 0x0000005C,
		.clear_reg_offset  = 0x00000064,
		.status_reg_offset = 0x0000006C,
	},
	{
		.mask_reg_offset   = 0x00000060,
		.clear_reg_offset  = 0x00000068,
		.status_reg_offset = 0x00000070,
	},
};

static struct cam_irq_controller_reg_info vfe170_top_irq_reg_info = {
	.num_registers = 2,
	.irq_reg_set = vfe170_top_irq_reg_set,
	.global_clear_offset  = 0x00000058,
	.global_clear_bitmask = 0x00000001,
};

static struct cam_vfe_camif_ver2_reg vfe170_camif_reg = {
	.camif_cmd                = 0x00000478,
	.camif_config             = 0x0000047C,
	.line_skip_pattern        = 0x00000488,
	.pixel_skip_pattern       = 0x0000048C,
	.skip_period              = 0x00000490,
	.irq_subsample_pattern    = 0x0000049C,
	.epoch_irq                = 0x000004A0,
	.raw_crop_width_cfg       = 0x00000CE4,
	.raw_crop_height_cfg      = 0x00000CE8,
	.reg_update_cmd           = 0x000004AC,
};

static struct cam_vfe_camif_reg_data vfe_170_camif_reg_data = {
	.raw_crop_first_pixel_shift      = 16,
	.raw_crop_first_pixel_mask       = 0xFFFF,
	.raw_crop_last_pixel_shift       = 0x0,
	.raw_crop_last_pixel_mask        = 0x3FFF,
	.raw_crop_first_line_shift       = 16,
	.raw_crop_first_line_mask        = 0xFFFF,
	.raw_crop_last_line_shift        = 0,
	.raw_crop_last_line_mask         = 0x3FFF,
	.input_mux_sel_shift             = 5,
	.input_mux_sel_mask              = 0x3,
	.extern_reg_update_shift         = 4,
	.extern_reg_update_mask          = 1,
	.pixel_pattern_shift             = 0,
	.pixel_pattern_mask              = 0x7,
	.reg_update_cmd_data             = 0x1,
	.epoch_line_cfg                  = 0x00140014,
	.sof_irq_mask                    = 0x00000001,
	.epoch0_irq_mask                 = 0x00000004,
	.reg_update_irq_mask             = 0x00000010,
	.eof_irq_mask                    = 0x00000002,
};

struct cam_vfe_top_ver2_reg_offset_module_ctrl lens_170_reg = {
	.reset    = 0x0000001C,
	.cgc_ovd  = 0x0000002C,
	.enable   = 0x00000040,
};

struct cam_vfe_top_ver2_reg_offset_module_ctrl stats_170_reg = {
	.reset    = 0x00000020,
	.cgc_ovd  = 0x00000030,
	.enable   = 0x00000044,
};

struct cam_vfe_top_ver2_reg_offset_module_ctrl color_170_reg = {
	.reset    = 0x00000024,
	.cgc_ovd  = 0x00000034,
	.enable   = 0x00000048,
};

struct cam_vfe_top_ver2_reg_offset_module_ctrl zoom_170_reg = {
	.reset    = 0x00000028,
	.cgc_ovd  = 0x00000038,
	.enable   = 0x0000004C,
};

static struct cam_vfe_top_ver2_reg_offset_common vfe170_top_common_reg = {
	.hw_version               = 0x00000000,
	.hw_capability            = 0x00000004,
	.lens_feature             = 0x00000008,
	.stats_feature            = 0x0000000C,
	.color_feature            = 0x00000010,
	.zoom_feature             = 0x00000014,
	.global_reset_cmd         = 0x00000018,
	.module_ctrl              = {
		&lens_170_reg,
		&stats_170_reg,
		&color_170_reg,
		&zoom_170_reg,
	},
	.bus_cgc_ovd              = 0x0000003C,
	.core_cfg                 = 0x00000050,
	.three_D_cfg              = 0x00000054,
	.violation_status         = 0x0000007C,
	.reg_update_cmd           = 0x000004AC,
};

static struct cam_vfe_rdi_ver2_reg vfe170_rdi_reg = {
	.reg_update_cmd           = 0x000004AC,
};

static struct cam_vfe_rdi_reg_data  vfe_170_rdi_0_data = {
	.reg_update_cmd_data      = 0x2,
	.sof_irq_mask             = 0x8000000,
	.reg_update_irq_mask      = 0x20,
};

static struct cam_vfe_rdi_reg_data  vfe_170_rdi_1_data = {
	.reg_update_cmd_data      = 0x4,
	.sof_irq_mask             = 0x10000000,
	.reg_update_irq_mask      = 0x40,
};

static struct cam_vfe_rdi_reg_data  vfe_170_rdi_2_data = {
	.reg_update_cmd_data      = 0x8,
	.sof_irq_mask             = 0x20000000,
	.reg_update_irq_mask      = 0x80,
};

static struct cam_vfe_top_ver2_hw_info vfe170_top_hw_info = {
	.common_reg = &vfe170_top_common_reg,
	.camif_hw_info = {
		.common_reg = &vfe170_top_common_reg,
		.camif_reg =  &vfe170_camif_reg,
		.reg_data  =  &vfe_170_camif_reg_data,
		},
	.rdi_hw_info = {
		.common_reg = &vfe170_top_common_reg,
		.rdi_reg    = &vfe170_rdi_reg,
		.reg_data = {
			&vfe_170_rdi_0_data,
			&vfe_170_rdi_1_data,
			&vfe_170_rdi_2_data,
			NULL,
			},
		},
	.mux_type = {
		CAM_VFE_CAMIF_VER_2_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
	},
};

static struct cam_irq_register_set vfe170_bus_irq_reg[3] = {
		{
			.mask_reg_offset   = 0x00002044,
			.clear_reg_offset  = 0x00002050,
			.status_reg_offset = 0x0000205C,
		},
		{
			.mask_reg_offset   = 0x00002048,
			.clear_reg_offset  = 0x00002054,
			.status_reg_offset = 0x00002060,
		},
		{
			.mask_reg_offset   = 0x0000204C,
			.clear_reg_offset  = 0x00002058,
			.status_reg_offset = 0x00002064,
		},
};

static struct cam_vfe_bus_ver2_reg_offset_ubwc_client ubwc_regs_client_3 = {
	.tile_cfg         = 0x0000252C,
	.h_init           = 0x00002530,
	.v_init           = 0x00002534,
	.meta_addr        = 0x00002538,
	.meta_offset      = 0x0000253C,
	.meta_stride      = 0x00002540,
	.mode_cfg         = 0x00002544,
};

static struct cam_vfe_bus_ver2_reg_offset_ubwc_client ubwc_regs_client_4 = {
	.tile_cfg         = 0x0000262C,
	.h_init           = 0x00002630,
	.v_init           = 0x00002634,
	.meta_addr        = 0x00002638,
	.meta_offset      = 0x0000263C,
	.meta_stride      = 0x00002640,
	.mode_cfg         = 0x00002644,
};

static struct cam_vfe_bus_ver2_hw_info vfe170_bus_hw_info = {
	.common_reg = {
		.hw_version                   = 0x00002000,
		.hw_capability                = 0x00002004,
		.sw_reset                     = 0x00002008,
		.cgc_ovd                      = 0x0000200C,
		.pwr_iso_cfg                  = 0x000020CC,
		.dual_master_comp_cfg         = 0x00002028,
		.irq_reg_info = {
			.num_registers        = 3,
			.irq_reg_set          = vfe170_bus_irq_reg,
			.global_clear_offset  = 0x00002068,
			.global_clear_bitmask = 0x00000001,
		},
		.comp_error_status            = 0x0000206C,
		.comp_ovrwr_status            = 0x00002070,
		.dual_comp_error_status       = 0x00002074,
		.dual_comp_ovrwr_status       = 0x00002078,
		.addr_sync_cfg                = 0x0000207C,
		.addr_sync_frame_hdr          = 0x00002080,
		.addr_sync_no_sync            = 0x00002084,
	},
	.num_client = 20,
	.bus_client_reg = {
		/* BUS Client 0 */
		{
			.status0                  = 0x00002200,
			.status1                  = 0x00002204,
			.cfg                      = 0x00002208,
			.header_addr              = 0x0000220C,
			.header_cfg               = 0x00002210,
			.image_addr               = 0x00002214,
			.image_addr_offset        = 0x00002218,
			.buffer_width_cfg         = 0x0000221C,
			.buffer_height_cfg        = 0x00002220,
			.packer_cfg               = 0x00002224,
			.stride                   = 0x00002228,
			.irq_subsample_period     = 0x00002248,
			.irq_subsample_pattern    = 0x0000224C,
			.framedrop_period         = 0x00002250,
			.framedrop_pattern        = 0x00002254,
			.frame_inc                = 0x00002258,
			.burst_limit              = 0x0000225C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 1 */
		{
			.status0                  = 0x00002300,
			.status1                  = 0x00002304,
			.cfg                      = 0x00002308,
			.header_addr              = 0x0000230C,
			.header_cfg               = 0x00002310,
			.image_addr               = 0x00002314,
			.image_addr_offset        = 0x00002318,
			.buffer_width_cfg         = 0x0000231C,
			.buffer_height_cfg        = 0x00002320,
			.packer_cfg               = 0x00002324,
			.stride                   = 0x00002328,
			.irq_subsample_period     = 0x00002348,
			.irq_subsample_pattern    = 0x0000234C,
			.framedrop_period         = 0x00002350,
			.framedrop_pattern        = 0x00002354,
			.frame_inc                = 0x00002358,
			.burst_limit              = 0x0000235C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 2 */
		{
			.status0                  = 0x00002400,
			.status1                  = 0x00002404,
			.cfg                      = 0x00002408,
			.header_addr              = 0x0000240C,
			.header_cfg               = 0x00002410,
			.image_addr               = 0x00002414,
			.image_addr_offset        = 0x00002418,
			.buffer_width_cfg         = 0x0000241C,
			.buffer_height_cfg        = 0x00002420,
			.packer_cfg               = 0x00002424,
			.stride                   = 0x00002428,
			.irq_subsample_period     = 0x00002448,
			.irq_subsample_pattern    = 0x0000244C,
			.framedrop_period         = 0x00002450,
			.framedrop_pattern        = 0x00002454,
			.frame_inc                = 0x00002458,
			.burst_limit              = 0x0000245C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 3 */
		{
			.status0                  = 0x00002500,
			.status1                  = 0x00002504,
			.cfg                      = 0x00002508,
			.header_addr              = 0x0000250C,
			.header_cfg               = 0x00002510,
			.image_addr               = 0x00002514,
			.image_addr_offset        = 0x00002518,
			.buffer_width_cfg         = 0x0000251C,
			.buffer_height_cfg        = 0x00002520,
			.packer_cfg               = 0x00002524,
			.stride                   = 0x00002528,
			.irq_subsample_period     = 0x00002548,
			.irq_subsample_pattern    = 0x0000254C,
			.framedrop_period         = 0x00002550,
			.framedrop_pattern        = 0x00002554,
			.frame_inc                = 0x00002558,
			.burst_limit              = 0x0000255C,
			.ubwc_regs                = &ubwc_regs_client_3,
		},
		/* BUS Client 4 */
		{
			.status0                  = 0x00002600,
			.status1                  = 0x00002604,
			.cfg                      = 0x00002608,
			.header_addr              = 0x0000260C,
			.header_cfg               = 0x00002610,
			.image_addr               = 0x00002614,
			.image_addr_offset        = 0x00002618,
			.buffer_width_cfg         = 0x0000261C,
			.buffer_height_cfg        = 0x00002620,
			.packer_cfg               = 0x00002624,
			.stride                   = 0x00002628,
			.irq_subsample_period     = 0x00002648,
			.irq_subsample_pattern    = 0x0000264C,
			.framedrop_period         = 0x00002650,
			.framedrop_pattern        = 0x00002654,
			.frame_inc                = 0x00002658,
			.burst_limit              = 0x0000265C,
			.ubwc_regs                = &ubwc_regs_client_4,
		},
		/* BUS Client 5 */
		{
			.status0                  = 0x00002700,
			.status1                  = 0x00002704,
			.cfg                      = 0x00002708,
			.header_addr              = 0x0000270C,
			.header_cfg               = 0x00002710,
			.image_addr               = 0x00002714,
			.image_addr_offset        = 0x00002718,
			.buffer_width_cfg         = 0x0000271C,
			.buffer_height_cfg        = 0x00002720,
			.packer_cfg               = 0x00002724,
			.stride                   = 0x00002728,
			.irq_subsample_period     = 0x00002748,
			.irq_subsample_pattern    = 0x0000274C,
			.framedrop_period         = 0x00002750,
			.framedrop_pattern        = 0x00002754,
			.frame_inc                = 0x00002758,
			.burst_limit              = 0x0000275C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 6 */
		{
			.status0                  = 0x00002800,
			.status1                  = 0x00002804,
			.cfg                      = 0x00002808,
			.header_addr              = 0x0000280C,
			.header_cfg               = 0x00002810,
			.image_addr               = 0x00002814,
			.image_addr_offset        = 0x00002818,
			.buffer_width_cfg         = 0x0000281C,
			.buffer_height_cfg        = 0x00002820,
			.packer_cfg               = 0x00002824,
			.stride                   = 0x00002828,
			.irq_subsample_period     = 0x00002848,
			.irq_subsample_pattern    = 0x0000284C,
			.framedrop_period         = 0x00002850,
			.framedrop_pattern        = 0x00002854,
			.frame_inc                = 0x00002858,
			.burst_limit              = 0x0000285C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 7 */
		{
			.status0                  = 0x00002900,
			.status1                  = 0x00002904,
			.cfg                      = 0x00002908,
			.header_addr              = 0x0000290C,
			.header_cfg               = 0x00002910,
			.image_addr               = 0x00002914,
			.image_addr_offset        = 0x00002918,
			.buffer_width_cfg         = 0x0000291C,
			.buffer_height_cfg        = 0x00002920,
			.packer_cfg               = 0x00002924,
			.stride                   = 0x00002928,
			.irq_subsample_period     = 0x00002948,
			.irq_subsample_pattern    = 0x0000294C,
			.framedrop_period         = 0x00002950,
			.framedrop_pattern        = 0x00002954,
			.frame_inc                = 0x00002958,
			.burst_limit              = 0x0000295C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 8 */
		{
			.status0                  = 0x00002A00,
			.status1                  = 0x00002A04,
			.cfg                      = 0x00002A08,
			.header_addr              = 0x00002A0C,
			.header_cfg               = 0x00002A10,
			.image_addr               = 0x00002A14,
			.image_addr_offset        = 0x00002A18,
			.buffer_width_cfg         = 0x00002A1C,
			.buffer_height_cfg        = 0x00002A20,
			.packer_cfg               = 0x00002A24,
			.stride                   = 0x00002A28,
			.irq_subsample_period     = 0x00002A48,
			.irq_subsample_pattern    = 0x00002A4C,
			.framedrop_period         = 0x00002A50,
			.framedrop_pattern        = 0x00002A54,
			.frame_inc                = 0x00002A58,
			.burst_limit              = 0x00002A5C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 9 */
		{
			.status0                  = 0x00002B00,
			.status1                  = 0x00002B04,
			.cfg                      = 0x00002B08,
			.header_addr              = 0x00002B0C,
			.header_cfg               = 0x00002B10,
			.image_addr               = 0x00002B14,
			.image_addr_offset        = 0x00002B18,
			.buffer_width_cfg         = 0x00002B1C,
			.buffer_height_cfg        = 0x00002B20,
			.packer_cfg               = 0x00002B24,
			.stride                   = 0x00002B28,
			.irq_subsample_period     = 0x00002B48,
			.irq_subsample_pattern    = 0x00002B4C,
			.framedrop_period         = 0x00002B50,
			.framedrop_pattern        = 0x00002B54,
			.frame_inc                = 0x00002B58,
			.burst_limit              = 0x00002B5C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 10 */
		{
			.status0                  = 0x00002C00,
			.status1                  = 0x00002C04,
			.cfg                      = 0x00002C08,
			.header_addr              = 0x00002C0C,
			.header_cfg               = 0x00002C10,
			.image_addr               = 0x00002C14,
			.image_addr_offset        = 0x00002C18,
			.buffer_width_cfg         = 0x00002C1C,
			.buffer_height_cfg        = 0x00002C20,
			.packer_cfg               = 0x00002C24,
			.stride                   = 0x00002C28,
			.irq_subsample_period     = 0x00002C48,
			.irq_subsample_pattern    = 0x00002C4C,
			.framedrop_period         = 0x00002C50,
			.framedrop_pattern        = 0x00002C54,
			.frame_inc                = 0x00002C58,
			.burst_limit              = 0x00002C5C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 11 */
		{
			.status0                  = 0x00002D00,
			.status1                  = 0x00002D04,
			.cfg                      = 0x00002D08,
			.header_addr              = 0x00002D0C,
			.header_cfg               = 0x00002D10,
			.image_addr               = 0x00002D14,
			.image_addr_offset        = 0x00002D18,
			.buffer_width_cfg         = 0x00002D1C,
			.buffer_height_cfg        = 0x00002D20,
			.packer_cfg               = 0x00002D24,
			.stride                   = 0x00002D28,
			.irq_subsample_period     = 0x00002D48,
			.irq_subsample_pattern    = 0x00002D4C,
			.framedrop_period         = 0x00002D50,
			.framedrop_pattern        = 0x00002D54,
			.frame_inc                = 0x00002D58,
			.burst_limit              = 0x00002D5C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 12 */
		{
			.status0                  = 0x00002E00,
			.status1                  = 0x00002E04,
			.cfg                      = 0x00002E08,
			.header_addr              = 0x00002E0C,
			.header_cfg               = 0x00002E10,
			.image_addr               = 0x00002E14,
			.image_addr_offset        = 0x00002E18,
			.buffer_width_cfg         = 0x00002E1C,
			.buffer_height_cfg        = 0x00002E20,
			.packer_cfg               = 0x00002E24,
			.stride                   = 0x00002E28,
			.irq_subsample_period     = 0x00002E48,
			.irq_subsample_pattern    = 0x00002E4C,
			.framedrop_period         = 0x00002E50,
			.framedrop_pattern        = 0x00002E54,
			.frame_inc                = 0x00002E58,
			.burst_limit              = 0x00002E5C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 13 */
		{
			.status0                  = 0x00002F00,
			.status1                  = 0x00002F04,
			.cfg                      = 0x00002F08,
			.header_addr              = 0x00002F0C,
			.header_cfg               = 0x00002F10,
			.image_addr               = 0x00002F14,
			.image_addr_offset        = 0x00002F18,
			.buffer_width_cfg         = 0x00002F1C,
			.buffer_height_cfg        = 0x00002F20,
			.packer_cfg               = 0x00002F24,
			.stride                   = 0x00002F28,
			.irq_subsample_period     = 0x00002F48,
			.irq_subsample_pattern    = 0x00002F4C,
			.framedrop_period         = 0x00002F50,
			.framedrop_pattern        = 0x00002F54,
			.frame_inc                = 0x00002F58,
			.burst_limit              = 0x00002F5C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 14 */
		{
			.status0                  = 0x00003000,
			.status1                  = 0x00003004,
			.cfg                      = 0x00003008,
			.header_addr              = 0x0000300C,
			.header_cfg               = 0x00003010,
			.image_addr               = 0x00003014,
			.image_addr_offset        = 0x00003018,
			.buffer_width_cfg         = 0x0000301C,
			.buffer_height_cfg        = 0x00003020,
			.packer_cfg               = 0x00003024,
			.stride                   = 0x00003028,
			.irq_subsample_period     = 0x00003048,
			.irq_subsample_pattern    = 0x0000304C,
			.framedrop_period         = 0x00003050,
			.framedrop_pattern        = 0x00003054,
			.frame_inc                = 0x00003058,
			.burst_limit              = 0x0000305C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 15 */
		{
			.status0                  = 0x00003100,
			.status1                  = 0x00003104,
			.cfg                      = 0x00003108,
			.header_addr              = 0x0000310C,
			.header_cfg               = 0x00003110,
			.image_addr               = 0x00003114,
			.image_addr_offset        = 0x00003118,
			.buffer_width_cfg         = 0x0000311C,
			.buffer_height_cfg        = 0x00003120,
			.packer_cfg               = 0x00003124,
			.stride                   = 0x00003128,
			.irq_subsample_period     = 0x00003148,
			.irq_subsample_pattern    = 0x0000314C,
			.framedrop_period         = 0x00003150,
			.framedrop_pattern        = 0x00003154,
			.frame_inc                = 0x00003158,
			.burst_limit              = 0x0000315C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 16 */
		{
			.status0                  = 0x00003200,
			.status1                  = 0x00003204,
			.cfg                      = 0x00003208,
			.header_addr              = 0x0000320C,
			.header_cfg               = 0x00003210,
			.image_addr               = 0x00003214,
			.image_addr_offset        = 0x00003218,
			.buffer_width_cfg         = 0x0000321C,
			.buffer_height_cfg        = 0x00003220,
			.packer_cfg               = 0x00003224,
			.stride                   = 0x00003228,
			.irq_subsample_period     = 0x00003248,
			.irq_subsample_pattern    = 0x0000324C,
			.framedrop_period         = 0x00003250,
			.framedrop_pattern        = 0x00003254,
			.frame_inc                = 0x00003258,
			.burst_limit              = 0x0000325C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 17 */
		{
			.status0                  = 0x00003300,
			.status1                  = 0x00003304,
			.cfg                      = 0x00003308,
			.header_addr              = 0x0000330C,
			.header_cfg               = 0x00003310,
			.image_addr               = 0x00003314,
			.image_addr_offset        = 0x00003318,
			.buffer_width_cfg         = 0x0000331C,
			.buffer_height_cfg        = 0x00003320,
			.packer_cfg               = 0x00003324,
			.stride                   = 0x00003328,
			.irq_subsample_period     = 0x00003348,
			.irq_subsample_pattern    = 0x0000334C,
			.framedrop_period         = 0x00003350,
			.framedrop_pattern        = 0x00003354,
			.frame_inc                = 0x00003358,
			.burst_limit              = 0x0000335C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 18 */
		{
			.status0                  = 0x00003400,
			.status1                  = 0x00003404,
			.cfg                      = 0x00003408,
			.header_addr              = 0x0000340C,
			.header_cfg               = 0x00003410,
			.image_addr               = 0x00003414,
			.image_addr_offset        = 0x00003418,
			.buffer_width_cfg         = 0x0000341C,
			.buffer_height_cfg        = 0x00003420,
			.packer_cfg               = 0x00003424,
			.stride                   = 0x00003428,
			.irq_subsample_period     = 0x00003448,
			.irq_subsample_pattern    = 0x0000344C,
			.framedrop_period         = 0x00003450,
			.framedrop_pattern        = 0x00003454,
			.frame_inc                = 0x00003458,
			.burst_limit              = 0x0000345C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 19 */
		{
			.status0                  = 0x00003500,
			.status1                  = 0x00003504,
			.cfg                      = 0x00003508,
			.header_addr              = 0x0000350C,
			.header_cfg               = 0x00003510,
			.image_addr               = 0x00003514,
			.image_addr_offset        = 0x00003518,
			.buffer_width_cfg         = 0x0000351C,
			.buffer_height_cfg        = 0x00003520,
			.packer_cfg               = 0x00003524,
			.stride                   = 0x00003528,
			.irq_subsample_period     = 0x00003548,
			.irq_subsample_pattern    = 0x0000354C,
			.framedrop_period         = 0x00003550,
			.framedrop_pattern        = 0x00003554,
			.frame_inc                = 0x00003558,
			.burst_limit              = 0x0000355C,
			.ubwc_regs                = NULL,
		},
	},
	.comp_grp_reg = {
		/* CAM_VFE_BUS_VER2_COMP_GRP_0 */
		{
			.comp_mask                    = 0x00002010,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_1 */
		{
			.comp_mask                    = 0x00002014,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_2 */
		{
			.comp_mask                    = 0x00002018,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_3 */
		{
			.comp_mask                    = 0x0000201C,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_4 */
		{
			.comp_mask                    = 0x00002020,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_5 */
		{
			.comp_mask                    = 0x00002024,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0 */
		{
			.comp_mask                    = 0x0000202C,
			.addr_sync_mask               = 0x00002088,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_1 */
		{
			.comp_mask                    = 0x00002030,
			.addr_sync_mask               = 0x0000208C,

		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_2 */
		{
			.comp_mask                    = 0x00002034,
			.addr_sync_mask               = 0x00002090,

		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_3 */
		{
			.comp_mask                    = 0x00002038,
			.addr_sync_mask               = 0x00002094,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_4 */
		{
			.comp_mask                    = 0x0000203C,
			.addr_sync_mask               = 0x00002098,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5 */
		{
			.comp_mask                    = 0x00002040,
			.addr_sync_mask               = 0x0000209C,
		},
	},
	.num_out = 18,
	.vfe_out_hw_info = {
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RDI0,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RDI1,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RDI2,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_FULL,
			.max_width     = 4096,
			.max_height    = 4096,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_DS4,
			.max_width     = 1920,
			.max_height    = 1080,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_DS16,
			.max_width     = 1920,
			.max_height    = 1080,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RAW_DUMP,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_FD,
			.max_width     = 1920,
			.max_height    = 1080,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_PDAF,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BE,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BHIST,
			.max_width     = 1920,
			.max_height    = 1080,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_TL_BG,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_BF,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_AWB_BG,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_BHIST,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_RS,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_CS,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER2_VFE_OUT_STATS_IHIST,
			.max_width     = -1,
			.max_height    = -1,
		},
	},
};

struct cam_vfe_hw_info cam_vfe170_hw_info = {
	.irq_reg_info                  = &vfe170_top_irq_reg_info,

	.bus_version                   = CAM_VFE_BUS_VER_2_0,
	.bus_hw_info                   = &vfe170_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_2_0,
	.top_hw_info                   = &vfe170_top_hw_info,

	.camif_version                 = CAM_VFE_CAMIF_VER_2_0,
	.camif_reg                     = &vfe170_camif_reg,

};

#endif /* _CAM_VFE170_H_ */
