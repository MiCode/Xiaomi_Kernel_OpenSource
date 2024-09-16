/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "gps_dl_config.h"
#include "gps_dl_log.h"
#include "gps_dl_hw_api.h"
#include "gps_dsp_fsm.h"

#define GPS_DSP_REG_POLL_MAX (11)
const unsigned int c_gps_dsp_reg_list[GPS_DATA_LINK_NUM][GPS_DSP_REG_POLL_MAX] = {
	/* 8: HW TICK H/L, BG tick H/L, TX_END/TX_RD, RX_END/RX_WR
	 * 3: PC, GALMAN CNT, WRHOST CNT
	 */
	{
		0x5028, 0x5029, 0x0100, 0x0101, 0x4882, 0x4883, 0x4886, 0x4887,
		0xEF00, 0xEF01, 0xEF02,
	},
	{
		0x5014, 0x5015, 0x0100, 0x0101, 0x4882, 0x4883, 0x4886, 0x4887,
		0xEF00, 0xEF01, 0xEF02,
	}
};

#define GPS_DSP_REG_DBG_POLL_MAX (20)
const unsigned int c_gps_dsp_reg_dbg_list[GPS_DATA_LINK_NUM][GPS_DSP_REG_DBG_POLL_MAX] = {
	/* 9: PC, GALMAN CNT, WRHOST CNT, DBTT CNT, NEXT CNT, BG TICK H/L, HW TICK H/L
	 * 11: USRT CTL, STA, TX_END/RD/MAX, RX_MAX/END/WR, TX_CNT, RX_CNT, MISC
	 */
	{
		0xEF00, 0xEF01, 0xEF02, 0xEF03, 0xEF04, 0x0100, 0x0101, 0x5028, 0x5029,
		0x4880, 0x4881, 0x4882, 0x4883, 0x4884, 0x4885, 0x4886, 0x4887, 0x4888, 0x4889, 0x488a,
	},
	{
		0xEF00, 0xEF01, 0xEF02, 0xEF03, 0xEF04, 0x0100, 0x0101, 0x5014, 0x5015,
		0x4880, 0x4881, 0x4882, 0x4883, 0x4884, 0x4885, 0x4886, 0x4887, 0x4888, 0x4889, 0x488a,
	}
};



struct gps_each_dsp_reg_read_context {
	bool poll_ongoing;
	int poll_index;
	unsigned int poll_addr;

	/* a "poll" means one register in c_gps_dsp_reg_list.
	 * a "round" means a round to poll all registers in c_gps_dsp_reg_list.
	 * sometimes for debug we need for several rounds
	 * to check the changing of the values of each register.
	 */
	unsigned int round_max;
	unsigned int round_index;
	const unsigned int *poll_list_ptr;
	int poll_list_len;
};

struct gps_each_dsp_reg_read_context g_gps_each_dsp_reg_read_context[GPS_DATA_LINK_NUM];


enum GDL_RET_STATUS gps_each_dsp_reg_read_request(
	enum gps_dl_link_id_enum link_id, unsigned int reg_addr)
{
	enum GDL_RET_STATUS ret;

	ASSERT_LINK_ID(link_id, GDL_FAIL_INVAL);
	ret = gps_dl_hw_mcub_dsp_read_request(link_id, reg_addr);

	if (ret == GDL_OKAY) {
		g_gps_each_dsp_reg_read_context[link_id].poll_addr = reg_addr;
		g_gps_each_dsp_reg_read_context[link_id].poll_ongoing = true;
	}

	return ret;
}

void gps_each_dsp_reg_gourp_read_next(enum gps_dl_link_id_enum link_id, bool restart)
{
	unsigned int reg_addr;
	enum GDL_RET_STATUS ret;
	struct gps_each_dsp_reg_read_context *p_read_context;
	int i;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	p_read_context = &g_gps_each_dsp_reg_read_context[link_id];
	if (restart) {
		p_read_context->poll_index = 0;
		p_read_context->round_index = 0;
	} else {
		p_read_context->poll_index++;
		if (p_read_context->poll_index >= p_read_context->poll_list_len) {
			p_read_context->round_index++;
			if (p_read_context->round_index >= p_read_context->round_max) {
				/* all polling end */
				return;
			}
			/* next round */
			p_read_context->poll_index = 0;
		}
	}

	i = p_read_context->poll_index;
	reg_addr = p_read_context->poll_list_ptr[i];
	ret = gps_each_dsp_reg_read_request(link_id, reg_addr);
	GDL_LOGXD(link_id, "i = %d/%d, addr = 0x%04x, status = %s",
		i, p_read_context->poll_list_len, reg_addr, gdl_ret_to_name(ret));
}

void gps_each_dsp_reg_read_ack(
	enum gps_dl_link_id_enum link_id, const struct gps_dl_hal_mcub_info *p_d2a)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	GDL_LOGXI(link_id,
		"n = %d/%d, addr = 0x%04x, val = 0x%04x/0x%04x, round = %d/%d",
		g_gps_each_dsp_reg_read_context[link_id].poll_index + 1,
		g_gps_each_dsp_reg_read_context[link_id].poll_list_len,
		g_gps_each_dsp_reg_read_context[link_id].poll_addr,
		p_d2a->dat0, p_d2a->dat1,
		g_gps_each_dsp_reg_read_context[link_id].round_index + 1,
		g_gps_each_dsp_reg_read_context[link_id].round_max);

	g_gps_each_dsp_reg_read_context[link_id].poll_ongoing = false;
	gps_each_dsp_reg_gourp_read_next(link_id, false);
}

void gps_each_dsp_reg_gourp_read_start(enum gps_dl_link_id_enum link_id,
	bool dbg, unsigned int round_max)
{
	unsigned int a2d_flag;
	struct gps_dl_hal_mcub_info d2a;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	if (g_gps_each_dsp_reg_read_context[link_id].poll_ongoing) {
		GDL_LOGXW(link_id, "n = %d/%d, addr = 0x%04x, seem busy, check it",
			g_gps_each_dsp_reg_read_context[link_id].poll_index + 1,
			g_gps_each_dsp_reg_read_context[link_id].poll_list_len,
			g_gps_each_dsp_reg_read_context[link_id].poll_addr);

		/* TODO: show hw status */
		a2d_flag = gps_dl_hw_get_mcub_a2d_flag(link_id);
		gps_dl_hw_get_mcub_info(link_id, &d2a);
		GDL_LOGXW(link_id, "a2d_flag = %d, d2a_flag = %d, d0 = 0x%04x, d1 = 0x%04x",
			a2d_flag, d2a.flag, d2a.dat0, d2a.dat1);

		if (a2d_flag & GPS_MCUB_A2DF_MASK_DSP_REG_READ_REQ ||
			d2a.flag & GPS_MCUB_D2AF_MASK_DSP_REG_READ_READY) {
			/* real busy, bypass */
			return;
		}
	}

	if (dbg) {
		g_gps_each_dsp_reg_read_context[link_id].poll_list_ptr =
			&c_gps_dsp_reg_dbg_list[link_id][0];
		g_gps_each_dsp_reg_read_context[link_id].poll_list_len = GPS_DSP_REG_DBG_POLL_MAX;
	} else {
		g_gps_each_dsp_reg_read_context[link_id].poll_list_ptr =
			&c_gps_dsp_reg_list[link_id][0];
		g_gps_each_dsp_reg_read_context[link_id].poll_list_len = GPS_DSP_REG_POLL_MAX;
	}
	g_gps_each_dsp_reg_read_context[link_id].round_max = round_max;
	gps_each_dsp_reg_gourp_read_next(link_id, true);
}

void gps_each_dsp_reg_gourp_read_init(enum gps_dl_link_id_enum link_id)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	memset(&g_gps_each_dsp_reg_read_context[link_id], 0,
		sizeof(g_gps_each_dsp_reg_read_context[link_id]));
}

