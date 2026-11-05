/*
 *  Silicon Integrated Co., Ltd haptic sih688x haptic config header file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _SIH688X_LRA_CONFIG_H_
#define _SIH688X_LRA_CONFIG_H_
#include "sih688x_reg.h"
#include "haptic.h"

#define LRA_NAME_LEN							8
#define HAPTIC_CONFIG_FILE_NUM					3
#define HAPTIC_CONFIG_FILE_INDEX				0
#define HAPTIC_CONFIG_FILE_BUF_LEN				64
#define HAPTIC_CONFIG_FILE_PATH					"mnt"
#define HAPTIC_CONFIG_MAX_REG_NUM				256
#define HAPTIC_F0_FILE_PATH_LEN					64
#define HAPTIC_F0_FILE_PATH						"/data"

typedef enum reg_operation {
	OPERATION_WRITE = 0,
	OPERATION_READ = 1,
	OPERATION_BIT = 2,
	OPERATION_END = 3,
} reg_op_e;

typedef enum reg_func_type {
	REG_FUNC_CONT = 0,
	REG_FUNC_RL = 1,
	REG_FUNC_VBAT = 2,
} reg_func_type_e;

typedef struct haptic_reg_format {
	uint8_t reg_addr;
	uint8_t reg_value;
} haptic_bin_file_reg_format_t;

typedef struct haptic_reg_config {
	uint8_t reg_num;
	haptic_bin_file_reg_format_t reg_cont[HAPTIC_CONFIG_MAX_REG_NUM];
} haptic_reg_config_t;

typedef struct reg_format {
	uint8_t addr;
	uint8_t val;
	uint8_t mask;	/* only useful for bit operation */
	reg_op_e operation;
} reg_format_t;

typedef struct lra_reg_config {
	char lra_name[LRA_NAME_LEN];
	haptic_reg_config_t *reg_config_list;
} lra_reg_config_t;

typedef struct lra_reg_func {
	char lra_name[LRA_NAME_LEN];
	reg_format_t *reg_cont_list;
	reg_format_t *reg_rl_list;
	reg_format_t *reg_vbat_list;
} lra_reg_func_t;

extern reg_format_t lra_9595_config_list[];
extern reg_format_t lra_0809_config_list[];
extern reg_format_t lra_0815_config_list[];
extern reg_format_t detect_rl_config_list[];
extern reg_format_t detect_vbat_config_list[];

void sih_load_reg_config(sih_haptic_t *sih_haptic, uint8_t func_type);
int sih_lra_config_load(sih_haptic_t *sih_haptic);

#endif
