/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */
#ifndef CLKBUF_CTL_H
#define CLKBUF_CTL_H

#include "mtk_clkbuf_common.h"

enum RC_CTRL_CMD {
	HW = 0,
	SW_OFF,
	SW_FPM,
	SW_LPM,
	SW_BBLPM = 3,
	MAX_RC_REQ_NUM
};

static const char * const rc_req_list[] = {
	"HW",
	"SW_OFF",
	"SW_FPM",
	"SW_LPM",
};

struct clkbuf_misc {
	bool flightmode;
	bool enable;
	bool init_done;
	bool debug;
	bool reg_debug;
	bool misc_debug;
	bool dws_debug;
	bool pmrc_en_debug;
};

extern int clk_buf_ctrl(const char *xo_name, bool onoff);
extern int clk_buf_hw_ctrl(const char *xo_name, bool onoff);
extern int clk_buf_dump_log(void);
extern int clk_buf_get_xo_en_sta(const char *xo_name);

extern int srclken_dump_sta_log(void);
extern int srclken_dump_cfg_log(void);
extern int srclken_dump_last_sta_log(void);
extern int clk_buf_voter_ctrl_by_id(const uint8_t subsys_id, enum RC_CTRL_CMD rc_req);

#endif /* CLKBUF_CTL_H */
