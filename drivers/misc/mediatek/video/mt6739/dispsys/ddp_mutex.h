/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DSI_MUTEX_H__
#define __DSI_MUTEX_H__

#include "ddp_hal.h"
#include "ddp_path.h"
#include "cmdq_record.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct module_map_s {
	enum DISP_MODULE_ENUM module;
	int bit;
	int mod_num;
} module_map_t;

extern unsigned int module_list_scenario[DDP_SCENARIO_MAX][DDP_ENING_NUM];

void ddp_mutex_interrupt_enable(int mutex_id, void *handle);
void ddp_mutex_interrupt_disable(int mutex_id, void *handle);
void ddp_mutex_reset(int mutex_id, void *handle);
int ddp_is_moudule_in_mutex(int mutex_id, enum DISP_MODULE_ENUM module);
void ddp_mutex_clear(int mutex_id, void *handle);
int ddp_mutex_set_sof_wait(int mutex_id, struct cmdqRecStruct *handle, int wait);
int ddp_mutex_enable(int mutex_id, enum DDP_SCENARIO_ENUM scenario, enum DDP_MODE mode, void *handle);
int ddp_mutex_set(int mutex_id, enum DDP_SCENARIO_ENUM scenario, enum DDP_MODE mode, void *handle);
void ddp_check_mutex(int mutex_id, enum DDP_SCENARIO_ENUM scenario, enum DDP_MODE mode);
char *ddp_get_mutex_sof_name(unsigned int regval);

#ifdef __cplusplus
}
#endif
#endif
