/*
 * drivers/video/tegra/dc/mipi_cal_regs.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_MIPI_CAL_REG_H__
#define __DRIVERS_VIDEO_TEGRA_DC_MIPI_CAL_REG_H__

#define MIPI_DSI_AUTOCAL_TIMEOUT_USEC 2000

#define MIPI_CAL_MIPI_CAL_CTRL_0	0x0
#define MIPI_CAL_NOISE_FLT(x)		(((x) & 0xf) << 26)
#define MIPI_CAL_PRESCALE(x)		(((x) & 0x3) << 24)
#define MIPI_CAL_CLKEN_OVR(x)		(((x) & 0x1) << 4)
#define MIPI_CAL_AUTOCAL_EN(x)		(((x) & 0x1) << 1)
#define MIPI_CAL_STARTCAL(x)		(((x) & 0x1) << 0)

#define MIPI_CAL_CILA_MIPI_CAL_CONFIG_0	0x14
#define MIPI_CAL_OVERIDEA(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELA(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSA(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSA(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSA(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_CILB_MIPI_CAL_CONFIG_0	0x18
#define MIPI_CAL_OVERIDEB(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELB(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSB(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSB(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSB(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_CILC_MIPI_CAL_CONFIG_0	0x1c
#define MIPI_CAL_OVERIDEC(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELC(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSC(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSC(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSC(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_CILD_MIPI_CAL_CONFIG_0	0x20
#define MIPI_CAL_OVERIDED(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELD(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSD(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSD(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSD(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_CILE_MIPI_CAL_CONFIG_0	0x24
#define MIPI_CAL_OVERIDEE(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELE(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSE(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSE(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSE(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_MIPI_BIAS_PAD_CFG0_0	0x58
#define MIPI_BIAS_PAD_PDVCLAMP(x)	(((x) & 0x1) << 1)
#define MIPI_BIAS_PAD_E_VCLAMP_REF(x)	(((x) & 0x1) << 0)

#define MIPI_CAL_MIPI_BIAS_PAD_CFG1_0	0x5c
#define PAD_TEST_SEL(x)			(((x) & 0x7) << 24)
#define PAD_DRIV_DN_REF(x)		(((x) & 0x7) << 16)
#define PAD_DRIV_UP_REF(x)		(((x) & 0x7) << 8)
#define PAD_TERM_REF(x)			(((x) & 0x7) << 0)

#define MIPI_CAL_MIPI_BIAS_PAD_CFG2_0	0x60
#define PAD_VCLAMP_LEVEL(x)		(((x) & 0x7) << 16)
#define PAD_SPARE(x)			(((x) & 0xff) << 8)
#define PAD_VAUXP_LEVEL(x)		(((x) & 0x7) << 4)
#define PAD_PDVREG(x)			(((x) & 0x1) << 1)
#define PAD_VBYPASS(x)			(((x) & 0x1) << 0)

#define MIPI_CAL_DSIA_MIPI_CAL_CONFIG_0	0x38
#define MIPI_CAL_OVERIDEDSIA(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELDSIA(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSDSIA(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSDSIA(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSDSIA(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_DSIB_MIPI_CAL_CONFIG_0	0x3c
#define MIPI_CAL_OVERIDEDSIB(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELDSIB(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSDSIB(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSDSIB(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSDSIB(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_DSIC_MIPI_CAL_CONFIG_0	0x40
#define MIPI_CAL_OVERIDEDSIC(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELDSIC(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSDSIC(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSDSIC(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSDSIC(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_DSID_MIPI_CAL_CONFIG_0	0x44
#define MIPI_CAL_OVERIDEDSID(x)		(((x) & 0x1) << 30)
#define MIPI_CAL_SELDSID(x)		(((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSDSID(x)		(((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSDSID(x)		(((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSDSID(x)		(((x) & 0x1f) << 0)

#define MIPI_CAL_MIPI_CAL_AUTOCAL_CTRL0_0 0x4
#define MIPI_CAL_AUTOCAL_PERIOD(x)  ((x) << 0)

#define MIPI_CAL_CIL_MIPI_CAL_STATUS_0 0x8
#define MIPI_AUTO_CAL_DONE_DSID(x)		(((x) & 0x1) << 31)
#define MIPI_AUTO_CAL_DONE_DSIC(x)		(((x) & 0x1) << 30)
#define MIPI_AUTO_CAL_DONE_DSIB(x)		(((x) & 0x1) << 29)
#define MIPI_AUTO_CAL_DONE_DSIA(x)		(((x) & 0x1) << 28)
#define MIPI_AUTO_CAL_DONE_CSIE(x)		(((x) & 0x1) << 24)
#define MIPI_AUTO_CAL_DONE_CSID(x)		(((x) & 0x1) << 23)
#define MIPI_AUTO_CAL_DONE_CSIC(x)		(((x) & 0x1) << 22)
#define MIPI_AUTO_CAL_DONE_CSIB(x)		(((x) & 0x1) << 21)
#define MIPI_AUTO_CAL_DONE_CSIA(x)		(((x) & 0x1) << 20)
#define MIPI_AUTO_CAL_DONE(x)		(((x) & 0x1) << 16)
#define MIPI_CAL_DRIV_DN_ADJ(x)		(((x) & 0xf) << 12)
#define MIPI_CAL_DRIV_UP_ADJ(x)		(((x) & 0xf) << 8)
#define MIPI_CAL_TERMADJ(x)		(((x) & 0xf) << 4)
#define MIPI_CAL_ACTIVE(x)		(((x) & 0x1) << 0)

#endif
