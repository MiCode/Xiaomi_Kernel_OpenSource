/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#ifndef _VCD_POWERSM_H_
#define _VCD_POWERSM_H_

#define VCD_EVT_PWR_BASE                0x5000
#define VCD_EVT_PWR_DEV_INIT_BEGIN      (VCD_EVT_PWR_BASE + 0x1)
#define VCD_EVT_PWR_DEV_INIT_END        (VCD_EVT_PWR_BASE + 0x2)
#define VCD_EVT_PWR_DEV_INIT_FAIL       (VCD_EVT_PWR_BASE + 0x3)
#define VCD_EVT_PWR_DEV_TERM_BEGIN      (VCD_EVT_PWR_BASE + 0x4)
#define VCD_EVT_PWR_DEV_TERM_END        (VCD_EVT_PWR_BASE + 0x5)
#define VCD_EVT_PWR_DEV_TERM_FAIL       (VCD_EVT_PWR_BASE + 0x6)
#define VCD_EVT_PWR_DEV_SLEEP_BEGIN     (VCD_EVT_PWR_BASE + 0x7)
#define VCD_EVT_PWR_DEV_SLEEP_END       (VCD_EVT_PWR_BASE + 0x8)
#define VCD_EVT_PWR_DEV_SET_PERFLVL     (VCD_EVT_PWR_BASE + 0x9)
#define VCD_EVT_PWR_DEV_HWTIMEOUT       (VCD_EVT_PWR_BASE + 0xa)
#define VCD_EVT_PWR_CLNT_CMD_BEGIN      (VCD_EVT_PWR_BASE + 0xb)
#define VCD_EVT_PWR_CLNT_CMD_END        (VCD_EVT_PWR_BASE + 0xc)
#define VCD_EVT_PWR_CLNT_CMD_FAIL       (VCD_EVT_PWR_BASE + 0xd)
#define VCD_EVT_PWR_CLNT_PAUSE          (VCD_EVT_PWR_BASE + 0xe)
#define VCD_EVT_PWR_CLNT_RESUME         (VCD_EVT_PWR_BASE + 0xf)
#define VCD_EVT_PWR_CLNT_FIRST_FRAME    (VCD_EVT_PWR_BASE + 0x10)
#define VCD_EVT_PWR_CLNT_LAST_FRAME     (VCD_EVT_PWR_BASE + 0x11)
#define VCD_EVT_PWR_CLNT_ERRFATAL       (VCD_EVT_PWR_BASE + 0x12)

enum vcd_pwr_clk_state {
	VCD_PWRCLK_STATE_OFF = 0,
	VCD_PWRCLK_STATE_ON_NOTCLOCKED,
	VCD_PWRCLK_STATE_ON_CLOCKED,
	VCD_PWRCLK_STATE_ON_CLOCKGATED
};

#endif
