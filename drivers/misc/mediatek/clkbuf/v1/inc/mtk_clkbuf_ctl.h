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
	SW_BBLPM,
	SW_FPM,
	SW_LPM,
	MAX_RC_REQ_NUM
};

static const char * const rc_req_list[] = {
	"HW",
	"SW_OFF",
	"SW_BBLPM",
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
extern int clk_buf_voter_ctrl_by_id(const uint8_t subsys_id, enum RC_CTRL_CMD rc_req);
extern int clk_buf_set_by_flightmode(bool on);
extern int clk_buf_control_bblpm(bool on);
extern int clk_buf_dump_log(void);
extern int clk_buf_get_xo_en_sta(const char *xo_name);
extern int clk_buf_bblpm_enter_cond(void);

#if defined(SRCLKEN_RC_SUPPORT)
extern int srclken_dump_sta_log(void);
extern int srclken_dump_cfg_log(void);
extern int srclken_dump_last_sta_log(void);
#else /* !defined(SRCLKEN_RC_SUPPORT) */
static inline int srclken_dump_sta_log(void)
{
	return -ENODEV;
}

static inline int srclken_dump_cfg_log(void)
{
	return -ENODEV;
}

static inline int srclken_dump_last_sta_log(void)
{
	return -ENODEV;
}
#endif /* defined(SRCLKEN_RC_SUPPORT) */

#endif /* CLKBUF_CTL_H */
