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
	MAX_RC_REQ_NUM
};

static const char * const rc_req_list[] = {
	"HW",
	"SW_OFF",
	"SW_BBLPM",
	"SW_FPM",
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

extern int clkbuf_init(struct platform_device *pdev);
extern int clk_buf_hw_ctrl(const char *xo_name, bool onoff);
extern int clk_buf_dump_log(void);
extern int clk_buf_get_xo_en_sta(const char *xo_name);

static inline int clk_buf_control_bblpm(bool on)
{
	return -ENODEV;
}

#if defined(SRCLKEN_RC_SUPPORT)
extern int srclken_dump_sta_log(void);
extern int srclken_dump_cfg_log(void);
extern int srclken_dump_last_sta_log(void);
extern int clk_buf_voter_ctrl_by_id(const uint8_t subsys_id, enum RC_CTRL_CMD rc_req);
extern int srclken_pwr_ctrl_by_id(const uint8_t subsys_id, bool onoff);
extern int clk_buf_set_voter_by_name(const char *xo_name, const char *voter);
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
static inline int clk_buf_voter_ctrl_by_id(const uint8_t subsys_id, enum RC_CTRL_CMD rc_req)
{
	return -ENODEV;
}
static inline int srclken_pwr_ctrl_by_id(const uint8_t subsys_id, bool onoff)
{
	return -ENODEV;
}
static inline int clk_buf_set_voter_by_name(const char *xo_name, const char *voter)
{
	return -ENODEV;
}
#endif /* defined(SRCLKEN_RC_SUPPORT) */

extern const struct dev_pm_ops clk_buf_suspend_ops;

#endif /* CLKBUF_CTL_H */
