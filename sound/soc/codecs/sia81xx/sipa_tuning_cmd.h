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


#ifndef _SIPA_TUNING_CMD_H
#define _SIPA_TUNING_CMD_H

int sipa_tuning_cmd_set_en(uint32_t cal_id, uint32_t en);

int sipa_tuning_cmd_get_en(uint32_t cal_id);

int sipa_tuning_cmd_set_vdd(uint32_t cal_id, uint32_t ch,
	uint32_t vdd);

int sipa_tuning_cmd_print_monitor_data(
	uint32_t cal_id, uint32_t ch);

int sipa_tuning_cmd_set_volume(uint32_t cal_id,
	uint32_t ch, int32_t vol);

int sipa_tuning_cmd_set_spk_cal_val(uint32_t cal_id,
	uint32_t ch, int32_t r0, int32_t t0, int32_t a, int32_t wire_r0);

int sipa_tuning_cmd_get_spk_cal_val(uint32_t cal_id,
	uint32_t ch, int32_t *r0, int32_t *t0, int32_t *a, int32_t *wire_r0);

int sipa_tuning_cmd_cal_spk_r0(uint32_t cal_id,
	uint32_t ch, int32_t t0, int32_t wire_r0);

int sipa_tuning_cmd_get_f0(uint32_t cal_id, uint32_t ch, int32_t *f0);

int sipa_tuning_cmd_debug_show(uint32_t cal_id, uint32_t ch);

int sipa_tunning_cmd_set_hoc(
	uint32_t cal_id,
	uint32_t ch,
	uint32_t hoc
);

int sipa_auto_first_cal_r0(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch);

int sipa_auto_set_spk_model(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch);

int sipa_auto_get_spk_model(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch);

int sipa_tuning_cmd_get_rdc_temp(
	uint32_t cal_id,
	uint32_t ch,
	int32_t *instant_f0,
	int32_t *rdc,
	int32_t *temperature
);

int sipa_tuning_cmd_close_temp_limiter(
	uint32_t cal_id,
	uint32_t ch,
	uint32_t on);

int sipa_tuning_cmd_close_f0_tracking(
	uint32_t cal_id,
	uint32_t ch,
	uint32_t on);

int sipa_tuning_close_temp_f0_module(
	uint32_t timer_task_hdl,
	uint32_t tuning_port_id,
	uint32_t ch);

#endif
