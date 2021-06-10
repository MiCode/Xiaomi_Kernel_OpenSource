/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __MDP_BASE_H__
#define __MDP_BASE_H__

#define MDP_HW_CHECK

static u32 mdp_engine_port[ENGBASE_COUNT] = {
	0,	/*ENGBASE_MMSYS_CONFIG*/
	0,	/*ENGBASE_MDP_RDMA0*/
	0,	/*ENGBASE_MDP_RDMA1*/
	0,	/*ENGBASE_MDP_RSZ0*/
	0,	/*ENGBASE_MDP_RSZ1*/
	0,	/*ENGBASE_MDP_WROT0*/
	0,	/*ENGBASE_MDP_WROT1*/
	0,	/*ENGBASE_MDP_WDMA*/
	0,	/*ENGBASE_MDP_TDSHP0*/
	0,	/*ENGBASE_MDP_COLOR0*/
	0,	/*ENGBASE_MDP_AAL0*/
	0,	/*ENGBASE_MDP_CCORR0*/
	0,	/*ENGBASE_MDP_HDR0*/
	0,	/*ENGBASE_MMSYS_MUTEX*/
	0,	/*ENGBASE_IMGSYS*/
	0,	/*ENGBASE_ISP_DIP1*/
	0,	/*ENGBASE_ISP_WPE_A*/
	0,	/*ENGBASE_ISP_WPE_B*/
};

static u32 mdp_base[] = {
	[ENGBASE_MMSYS_CONFIG] = 0x14000000,
	[ENGBASE_MDP_RDMA0] = 0x14001000,
	[ENGBASE_MDP_RDMA1] = 0x14002000,
	[ENGBASE_MDP_RSZ0] = 0x14003000,
	[ENGBASE_MDP_RSZ1] = 0x14004000,
	[ENGBASE_MDP_WROT0] = 0x14005000,
	[ENGBASE_MDP_WROT1] = 0x14020000,
	[ENGBASE_MDP_WDMA] = 0x1400d000,
	[ENGBASE_MDP_TDSHP0] = 0x14007000,
	[ENGBASE_MDP_COLOR0] = 0x1400e000,
	[ENGBASE_MDP_AAL0] = 0x1401b000,
	[ENGBASE_MDP_CCORR0] = 0x10000000,
	[ENGBASE_MDP_HDR0] = 0x1401c000,
	[ENGBASE_MMSYS_MUTEX] = 0x14016000,
	[ENGBASE_IMGSYS] = 0x15020000,
	[ENGBASE_ISP_DIP1] = 0x15022000,
	[ENGBASE_ISP_WPE_A] = 0x1502a000,
	[ENGBASE_ISP_WPE_B] = 0x1502d000,
};
#endif
