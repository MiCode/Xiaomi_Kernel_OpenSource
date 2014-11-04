/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "timed_ctrl.h"

#ifndef __INLINE_TIMED_CTRL__
#include "timed_ctrl_private.h"
#endif /* __INLINE_TIMED_CTRL__ */

#include "assert_support.h"

void timed_ctrl_snd_commnd(
	const timed_ctrl_ID_t			ID,
	hrt_data				mask,
	hrt_data				condition,
	hrt_data				counter,
	hrt_address				addr,
	hrt_data				value)
{
	OP___assert(ID == TIMED_CTRL0_ID);
	OP___assert(TIMED_CTRL_BASE[ID] != (hrt_address)-1);

	timed_ctrl_reg_store(ID, _HRT_TIMED_CONTROLLER_CMD_REG_IDX, mask);
	timed_ctrl_reg_store(ID, _HRT_TIMED_CONTROLLER_CMD_REG_IDX, condition);
	timed_ctrl_reg_store(ID, _HRT_TIMED_CONTROLLER_CMD_REG_IDX, counter);
	timed_ctrl_reg_store(ID, _HRT_TIMED_CONTROLLER_CMD_REG_IDX, (hrt_data)addr);
	timed_ctrl_reg_store(ID, _HRT_TIMED_CONTROLLER_CMD_REG_IDX, value);
return;
}

/* pqiao TODO: make sure the following commands get
	correct BASE address both for csim and android */

void timed_ctrl_snd_sp_commnd(
	const timed_ctrl_ID_t			ID,
	hrt_data				mask,
	hrt_data				condition,
	hrt_data				counter,
	const sp_ID_t				SP_ID,
	hrt_address				offset,
	hrt_data				value)
{
	OP___assert(SP_ID < N_SP_ID);
	OP___assert(SP_DMEM_BASE[SP_ID] != (hrt_address)-1);

	timed_ctrl_snd_commnd(ID, mask, condition, counter,
				SP_DMEM_BASE[SP_ID]+offset, value);
return;
}

void timed_ctrl_snd_gpio_commnd(
	const timed_ctrl_ID_t			ID,
	hrt_data				mask,
	hrt_data				condition,
	hrt_data				counter,
	const gpio_ID_t				GPIO_ID,
	hrt_address				offset,
	hrt_data				value)
{
	OP___assert(GPIO_ID < N_GPIO_ID);
	OP___assert(GPIO_BASE[GPIO_ID] != (hrt_address)-1);

	timed_ctrl_snd_commnd(ID, mask, condition, counter,
				GPIO_BASE[GPIO_ID]+offset, value);
return;
}
