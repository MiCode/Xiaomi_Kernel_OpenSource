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


#ifndef _SIPA_CAL_SPK_H
#define _SIPA_CAL_SPK_H

void sipa_cal_spk_execute(uint32_t tuning_port_id,
	uint32_t ch, int32_t t0, int32_t wire_r0);

void sipa_cal_spk_update_probe(uint32_t timer_task_hdl,
	uint32_t tuning_port_id, uint32_t ch);

void sipa_cal_spk_update_remove(
	uint32_t timer_task_hdl, uint32_t ch);

#endif
