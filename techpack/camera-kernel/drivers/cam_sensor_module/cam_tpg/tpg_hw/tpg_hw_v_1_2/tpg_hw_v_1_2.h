/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __TPG_HW_V_1_2_H__
#define __TPG_HW_V_1_2_H__

#include "../tpg_hw.h"

struct cam_tpg_ver_1_2_reg_offset {
	uint32_t tpg_hw_version;
	uint32_t tpg_hw_status;
	uint32_t tpg_module_cfg;
	uint32_t tpg_cfg0;
	uint32_t tpg_cfg1;
	uint32_t tpg_cfg2;
	uint32_t tpg_cfg3;
	uint32_t tpg_spare;

	/* configurations */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
	uint32_t tpg_en_shift;
	uint32_t tpg_hbi_shift;
	uint32_t tpg_dt_shift;
	uint32_t tpg_rotate_period_shift;
	uint32_t tpg_split_en_shift;
	uint32_t top_mux_reg_offset;
	uint32_t tpg_mux_sel_tpg_0_shift;
	uint32_t tpg_mux_sel_tpg_1_shift;
};


/**
 * @brief initialize the tpg hw instance
 *
 * @param hw   : tpg hw instance
 * @param data : argument for initialize
 *
 * @return     : 0 on success
 */
int tpg_hw_v_1_2_init(struct tpg_hw *hw, void *data);

/**
 * @brief start tpg hw
 *
 * @param hw    : tpg hw instance
 * @param data  : tpg hw instance data
 *
 * @return      : 0 on success
 */
int tpg_hw_v_1_2_start(struct tpg_hw *hw, void *data);

/**
 * @brief stop tpg hw
 *
 * @param hw   : tpg hw instance
 * @param data : argument for tpg hw stop
 *
 * @return     : 0 on success
 */
int tpg_hw_v_1_2_stop(struct tpg_hw *hw, void *data);

/**
 * @brief process a command send from hw layer
 *
 * @param hw  : tpg hw instance
 * @param cmd : command to process
 * @param arg : argument corresponding to command
 *
 * @return    : 0 on success
 */
int tpg_hw_v_1_2_process_cmd(struct tpg_hw *hw,
		uint32_t cmd, void *arg);

/**
 * @brief  dump hw status registers
 *
 * @param hw   : tpg hw instance
 * @param data : argument for status dump
 *
 * @return     : 0 on sucdess
 */
int tpg_hw_v_1_2_dump_status(struct tpg_hw *hw, void *data);

#endif
