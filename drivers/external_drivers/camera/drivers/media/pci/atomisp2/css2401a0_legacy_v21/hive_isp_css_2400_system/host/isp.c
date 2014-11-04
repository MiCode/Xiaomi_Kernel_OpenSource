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

#include "isp.h"

#ifndef __INLINE_ISP__
#include "isp_private.h"
#endif /* __INLINE_ISP__ */

#include "assert_support.h"

void cnd_isp_irq_enable(
	const isp_ID_t		ID,
	const bool		cnd)
{
	if (cnd) {
		isp_ctrl_setbit(ID, ISP_IRQ_READY_REG, ISP_IRQ_READY_BIT);
/* Enabling the IRQ immediately triggers an interrupt, clear it */
		isp_ctrl_setbit(ID, ISP_IRQ_CLEAR_REG, ISP_IRQ_CLEAR_BIT);
	} else {
		isp_ctrl_clearbit(ID, ISP_IRQ_READY_REG,
			ISP_IRQ_READY_BIT);
	}
return;
}

void isp_get_state(
	const isp_ID_t		ID,
	isp_state_t			*state,
	isp_stall_t			*stall)
{
	hrt_data sc = isp_ctrl_load(ID, ISP_SC_REG);

	assert(state != NULL);
	assert(stall != NULL);

	state->pc = isp_ctrl_load(ID, ISP_PC_REG);
	state->status_register = sc;
	state->is_broken = isp_ctrl_getbit(ID, ISP_SC_REG, ISP_BROKEN_BIT);
	state->is_idle = isp_ctrl_getbit(ID, ISP_SC_REG, ISP_IDLE_BIT);
	state->is_sleeping = isp_ctrl_getbit(ID, ISP_SC_REG, ISP_SLEEPING_BIT);
	state->is_stalling = isp_ctrl_getbit(ID, ISP_SC_REG, ISP_STALLING_BIT);
	stall->stat_ctrl =
		!isp_ctrl_getbit(ID, ISP_CTRL_SINK_REG, ISP_CTRL_SINK_BIT);
	stall->pmem =
		!isp_ctrl_getbit(ID, ISP_PMEM_SINK_REG, ISP_PMEM_SINK_BIT);
	stall->dmem =
		!isp_ctrl_getbit(ID, ISP_DMEM_SINK_REG, ISP_DMEM_SINK_BIT);
	stall->vmem =
		!isp_ctrl_getbit(ID, ISP_VMEM_SINK_REG, ISP_VMEM_SINK_BIT);
	stall->fifo0 =
		!isp_ctrl_getbit(ID, ISP_FIFO0_SINK_REG, ISP_FIFO0_SINK_BIT);
	stall->fifo1 =
		!isp_ctrl_getbit(ID, ISP_FIFO1_SINK_REG, ISP_FIFO1_SINK_BIT);
	stall->fifo2 =
		!isp_ctrl_getbit(ID, ISP_FIFO2_SINK_REG, ISP_FIFO2_SINK_BIT);
	stall->fifo3 =
		!isp_ctrl_getbit(ID, ISP_FIFO3_SINK_REG, ISP_FIFO3_SINK_BIT);
	stall->fifo4 =
		!isp_ctrl_getbit(ID, ISP_FIFO4_SINK_REG, ISP_FIFO4_SINK_BIT);
	stall->fifo5 =
		!isp_ctrl_getbit(ID, ISP_FIFO5_SINK_REG, ISP_FIFO5_SINK_BIT);
	stall->fifo6 =
		!isp_ctrl_getbit(ID, ISP_FIFO6_SINK_REG, ISP_FIFO6_SINK_BIT);
	stall->vamem1 =
		!isp_ctrl_getbit(ID, ISP_VAMEM1_SINK_REG, ISP_VAMEM1_SINK_BIT);
	stall->vamem2 =
		!isp_ctrl_getbit(ID, ISP_VAMEM2_SINK_REG, ISP_VAMEM2_SINK_BIT);
	stall->vamem3 =
		!isp_ctrl_getbit(ID, ISP_VAMEM3_SINK_REG, ISP_VAMEM3_SINK_BIT);
	stall->hmem =
		!isp_ctrl_getbit(ID, ISP_HMEM_SINK_REG, ISP_HMEM_SINK_BIT);
/*
	stall->icache_master =
		!isp_ctrl_getbit(ID, ISP_ICACHE_MT_SINK_REG,
			ISP_ICACHE_MT_SINK_BIT);
 */
return;
}
