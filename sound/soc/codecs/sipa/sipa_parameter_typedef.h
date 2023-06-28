/*
 * Copyright (C) 2020, SI-IN, Yun Shi (yun.shi@si-in.com).
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

#ifndef _SIPA_PARAMETER_TYPEDEF_H
#define _SIPA_PARAMETER_TYPEDEF_H

#include "sipa_common.h"

#define SIPA_FW_VER_MAIN		(1)
#define SIPA_FW_VER_SUB			(0)
#define SIPA_FW_VER_REV			(3)
#define SIPA_FW_VER				\
	((SIPA_FW_VER_MAIN << 16) | (SIPA_FW_VER_SUB << 8) | (SIPA_FW_VER_REV))

typedef enum __sipa_reg_proc_action {
	SIPA_REG_READ,
	SIPA_REG_WRITE,
	SIPA_REG_CHECK,
	SIPA_REG_PAD	// 此action用来填充数据或者提供额外的无实际动作的delay等
} SIPA_REG_PROC_ACTION;

typedef struct __sipa_param_cal_spk {
	uint32_t cal_ok;
	int32_t r0;
	int32_t t0;
	int32_t wire_r0;
	int32_t a;
} __packed SIPA_PARAM_CAL_SPK;

typedef struct __sipa_param_spk_model_param {
	int32_t f0;
	int32_t q;
	int32_t xthresh;
	int32_t xthresh_rdc;
} __packed SIPA_PARAM_SPK_MODEL_PARAM;

typedef struct __sipa_val_range {
	uint32_t begin;
	uint32_t end;
} __packed SIPA_VAL_RANGE;

typedef struct __sipa_reg_attr {
	uint32_t wr_mode;
	uint32_t default_val;
} __packed SIPA_REG_ATTR;

typedef struct __sipa_reg {
	uint32_t addr;
	uint32_t mask;
	uint32_t val;
} __packed SIPA_REG;

typedef struct __sipa_reg_common {
	uint32_t addr;
	uint32_t visible;
	uint32_t val[AUDIO_SCENE_NUM];
} __packed SIPA_REG_COMMON;

typedef struct __sipa_reg_proc {
	uint32_t addr;
	uint32_t mask;
	uint32_t action;
	uint32_t visible;
	uint32_t delay;		// us
	uint32_t val[AUDIO_SCENE_NUM];
} __packed SIPA_REG_PROC;

typedef struct __sipa_param_list {
	uint32_t offset;
	uint32_t num;
	uint32_t node_size;
} __packed SIPA_PARAM_LIST;

typedef struct __sipa_func0_to_reg {
	SIPA_VAL_RANGE valid_range;
	uint32_t bit_offset;
	SIPA_REG reg;
} __packed SIPA_FUNC0_TO_REG;

typedef struct __sipa_func1_to_reg {
	uint32_t func_code;
	SIPA_REG_PROC reg;
} __packed SIPA_FUNC1_TO_REG;

typedef struct __sipa_func2_to_reg {
	uint32_t func_code;
	SIPA_PARAM_LIST reg_list;
} __packed SIPA_FUNC2_TO_REG;

#if 0
typedef struct __sipa_reg_to_func {
	SIPA_REG reg;
	SIPA_PARAM_LIST func_list;	// type SIPA_COMMON_NODE
} __packed SIPA_REG_TO_FUNC;
#endif

typedef struct __sipa_trim_regs {
	uint32_t crc_width;
	SIPA_PARAM_LIST efuse;
	SIPA_PARAM_LIST crc;
	SIPA_PARAM_LIST default_set;
} __packed SIPA_TRIM_REGS;

/* chip related */
typedef struct __sipa_chip_cfg {
	uint32_t chip_type;
	uint32_t reg_addr_width;
	uint32_t reg_val_width;
	uint32_t chip_id_addr;
	SIPA_PARAM_LIST chip_id_ranges;

	uint32_t owi_mode[AUDIO_SCENE_NUM];
	SIPA_REG chip_en; 
	SIPA_PARAM_LIST init;
	SIPA_PARAM_LIST startup;
	SIPA_PARAM_LIST shutdown;
	SIPA_TRIM_REGS trim_regs;
	SIPA_FUNC0_TO_REG pvdd_limit;	//SIPA_PARAM_LIST pvdd_limit;

	/* move from dts */
	uint32_t en_dyn_ud_vdd;
	uint32_t en_dyn_ud_pvdd;
} __packed SIPA_CHIP_CFG;

typedef struct __sipa_extra_cfg {
	/* speaker related */
	int32_t spk_min_r0;
	int32_t spk_max_r0;
	int32_t spk_max_delta_r0;

	/* move from dts */
	uint32_t timer_task_hdl;
	uint32_t dyn_ud_vdd_port;
	uint32_t spk_model_flag;
	uint32_t en_spk_cal_dl;
} __packed SIPA_EXTRA_CFG;

typedef struct __sipa_param_writeable {
	uint32_t crc;
	uint32_t version;
	SIPA_PARAM_CAL_SPK cal_spk[SIPA_CHANNEL_NUM];
	SIPA_PARAM_SPK_MODEL_PARAM spk_model[SIPA_CHANNEL_NUM];
} __packed SIPA_PARAM_WRITEABLE;

typedef struct __sipa_param_fw {
	uint32_t crc;
	uint32_t version;
	uint32_t ch_en[SIPA_CHANNEL_NUM]; /* 0:diable, 1:enable, other:undefine */
	SIPA_PARAM_LIST chip_cfg[SIPA_CHANNEL_NUM];	/* for multi SI PA  compat*/
	SIPA_EXTRA_CFG extra_cfg[SIPA_CHANNEL_NUM];
	uint32_t data_size;
	uint8_t data[];
} __packed SIPA_PARAM_FW;

typedef struct __sipa_param {
	SIPA_PARAM_WRITEABLE writeable;
	SIPA_PARAM_FW fw;
} __packed SIPA_PARAM;

#endif

