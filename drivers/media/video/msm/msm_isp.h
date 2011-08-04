/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_ISP_H__
#define __MSM_ISP_H__

enum ISP_MESSAGE_ID {
	MSG_ID_RESET_ACK, /* 0 */
	MSG_ID_START_ACK,
	MSG_ID_STOP_ACK,
	MSG_ID_UPDATE_ACK,
	MSG_ID_OUTPUT_P,
	MSG_ID_OUTPUT_T,
	MSG_ID_OUTPUT_S,
	MSG_ID_OUTPUT_V,
	MSG_ID_SNAPSHOT_DONE,
	MSG_ID_STATS_AEC,
	MSG_ID_STATS_AF, /* 10 */
	MSG_ID_STATS_AWB,
	MSG_ID_STATS_RS,
	MSG_ID_STATS_CS,
	MSG_ID_STATS_IHIST,
	MSG_ID_STATS_SKIN,
	MSG_ID_EPOCH1,
	MSG_ID_EPOCH2,
	MSG_ID_SYNC_TIMER0_DONE,
	MSG_ID_SYNC_TIMER1_DONE,
	MSG_ID_SYNC_TIMER2_DONE, /* 20 */
	MSG_ID_ASYNC_TIMER0_DONE,
	MSG_ID_ASYNC_TIMER1_DONE,
	MSG_ID_ASYNC_TIMER2_DONE,
	MSG_ID_ASYNC_TIMER3_DONE,
	MSG_ID_AE_OVERFLOW,
	MSG_ID_AF_OVERFLOW,
	MSG_ID_AWB_OVERFLOW,
	MSG_ID_RS_OVERFLOW,
	MSG_ID_CS_OVERFLOW,
	MSG_ID_IHIST_OVERFLOW, /* 30 */
	MSG_ID_SKIN_OVERFLOW,
	MSG_ID_AXI_ERROR,
	MSG_ID_CAMIF_OVERFLOW,
	MSG_ID_VIOLATION,
	MSG_ID_CAMIF_ERROR,
	MSG_ID_BUS_OVERFLOW,
	MSG_ID_SOF_ACK,
	MSG_ID_STOP_REC_ACK,
};

#endif /* __MSM_ISP_H__ */
