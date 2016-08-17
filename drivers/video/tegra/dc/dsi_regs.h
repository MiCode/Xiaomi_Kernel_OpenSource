/*
 * drivers/video/tegra/dc/dsi_regs.h
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DSI_REG_H__
#define __DRIVERS_VIDEO_TEGRA_DC_DSI_REG_H__

enum {
	TEGRA_DSI_PAD_ENABLE,
	TEGRA_DSI_PAD_DISABLE,
};

/* These are word offsets from base (not byte offsets) */
enum {
	OP_DONE = 1,
};
#define DSI_INCR_SYNCPT 0x00
#define DSI_INCR_SYNCPT_COND(x)		(((x) & 0xff) << 8)
#define DSI_INCR_SYNCPT_INDX(x)		(((x) & 0xff) << 0)

#define DSI_INCR_SYNCPT_CNTRL 0x01
#define DSI_INCR_SYNCPT_ERROR 0x02
#define DSI_CTXSW 0x08
#define DSI_RD_DATA 0x09
#define DSI_WR_DATA 0x0a

#define DSI_POWER_CONTROL 0x0b
#define   DSI_POWER_CONTROL_LEG_DSI_ENABLE(x)		(((x) & 0x1) << 0)

#define DSI_INT_ENABLE 0x0c
#define DSI_INT_STATUS 0x0d
#define DSI_INT_MASK 0x0e

#define DSI_HOST_DSI_CONTROL 0x0f
enum {
	RESET_CRC = 1,
};
#define   DSI_HOST_CONTROL_FIFO_STAT_RESET(x)		(((x) & 0x1) << 21)
#define   DSI_HOST_DSI_CONTROL_CRC_RESET(x)		(((x) & 0x1) << 20)
enum {
	DSI_PHY_CLK_DIV1,
	DSI_PHY_CLK_DIV2,
};
#define   DSI_HOST_DSI_CONTROL_PHY_CLK_DIV(x)		(((x) & 0x7) << 16)
enum {
	SOL,
	FIFO_LEVEL,
	IMMEDIATE,
};
#define   DSI_HOST_DSI_CONTROL_HOST_TX_TRIG_SRC(x)	(((x) & 0x3) << 12)
enum {
	NORMAL,
	ENTER_ULPM,
	EXIT_ULPM,
};
#define   DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(x)	(((x) & 0x3) << 8)
#define   DSI_HOST_DSI_CONTROL_PERIPH_RESET(x)		(((x) & 0x1) << 7)
#define   DSI_HOST_DSI_CONTROL_RAW_DATA(x)		(((x) & 0x1) << 6)
enum {
	TEGRA_DSI_LOW,
	TEGRA_DSI_HIGH,
};
#define   DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(x)	(((x) & 0x1) << 5)
enum {
	HOST_ONLY,
	VIDEO_HOST,
};
#define   DSI_HOST_DSI_CONTROL_PKT_WR_FIFO_SEL(x)	(((x) & 0x1) << 4)
#define   DSI_HOST_DSI_CONTROL_IMM_BTA(x)		(((x) & 0x1) << 3)
#define   DSI_HOST_DSI_CONTROL_PKT_BTA(x)		(((x) & 0x1) << 2)
#define   DSI_HOST_DSI_CONTROL_CS_ENABLE(x)		(((x) & 0x1) << 1)
#define   DSI_HOST_DSI_CONTROL_ECC_ENABLE(x)		(((x) & 0x1) << 0)

#define DSI_CONTROL 0x10
#define   DSI_CONTROL_DBG_ENABLE(x)			(((x) & 0x1) << 31)
enum {
	CONTINUOUS,
	TX_ONLY,
};
#define   DSI_CONTROL_HS_CLK_CTRL(x)			(((x) & 0x1) << 20)
#define   DSI_CONTROL_VIRTUAL_CHANNEL(x)		(((x) & 0x3) << 16)
#define   DSI_CONTROL_DATA_FORMAT(x)			(((x) & 0x3) << 12)
#define   DSI_CONTROL_VID_TX_TRIG_SRC(x)		(((x) & 0x3) << 8)
#define   DSI_CONTROL_NUM_DATA_LANES(x)			(((x) & 0x3) << 4)
#define   DSI_CONTROL_VID_DCS_ENABLE(x)			(((x) & 0x1) << 3)
#define   DSI_CONTROL_VID_SOURCE(x)			(((x) & 0x1) << 2)
#define   DSI_CONTROL_VID_ENABLE(x)			(((x) & 0x1) << 1)
#define   DSI_CONTROL_HOST_ENABLE(x)			(((x) & 0x1) << 0)

#define DSI_SOL_DELAY 0x11
#define DSI_SOL_DELAY_SOL_DELAY(x)			(((x) & 0xffff) << 0)

#define DSI_MAX_THRESHOLD 0x12
#define DSI_MAX_THRESHOLD_MAX_THRESHOLD(x)		(((x) & 0xffff) << 0)

#define DSI_TRIGGER 0x13
#define DSI_TRIGGER_HOST_TRIGGER(x)			(((x) & 0x1) << 1)
#define DSI_TRIGGER_VID_TRIGGER(x)			(((x) & 0x1) << 0)

#define DSI_TX_CRC 0x14
#define DSI_TX_CRC_TX_CRC(x)			(((x) & 0xffffffff) << 0)

#define DSI_STATUS 0x15
#define DSI_STATUS_IDLE(x)			(((x) & 0x1) << 10)
#define DSI_STATUS_LB_UNDERFLOW(x)		(((x) & 0x1) << 9)
#define DSI_STATUS_LB_OVERFLOW(x)		(((x) & 0x1) << 8)
#define DSI_STATUS_RD_FIFO_COUNT(x)		(((x) & 0x1f) << 0)

#define DSI_INIT_SEQ_CONTROL 0x1a
#define   DSI_INIT_SEQ_CONTROL_DSI_FRAME_INIT_BYTE_COUNT(x) \
				(((x) & 0x3f) << 8)
#define   DSI_INIT_SEQ_CONTROL_DSI_SEND_INIT_SEQUENCE(x) \
				(((x) & 0xff) << 0)

#define DSI_INIT_SEQ_DATA_0 0x1b
#define DSI_INIT_SEQ_DATA_1 0x1c
#define DSI_INIT_SEQ_DATA_2 0x1d
#define DSI_INIT_SEQ_DATA_3 0x1e
#define DSI_INIT_SEQ_DATA_4 0x1f
#define DSI_INIT_SEQ_DATA_5 0x20
#define DSI_INIT_SEQ_DATA_6 0x21
#define DSI_INIT_SEQ_DATA_7 0x22

#define DSI_PKT_SEQ_0_LO 0x23
#define   DSI_PKT_SEQ_0_LO_SEQ_0_FORCE_LP(x)	(((x) & 0x1) << 30)
#define   DSI_PKT_SEQ_0_LO_PKT_02_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_0_LO_PKT_02_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_0_LO_PKT_02_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_0_LO_PKT_01_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_0_LO_PKT_01_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_0_LO_PKT_01_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_0_LO_PKT_00_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_0_LO_PKT_00_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_0_LO_PKT_00_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_0_HI 0x24
#define   DSI_PKT_SEQ_0_HI_PKT_05_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_0_HI_PKT_05_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_0_HI_PKT_05_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_0_HI_PKT_04_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_0_HI_PKT_04_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_0_HI_PKT_04_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_0_HI_PKT_03_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_0_HI_PKT_03_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_0_HI_PKT_03_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_1_LO 0x25
#define   DSI_PKT_SEQ_1_LO_SEQ_1_FORCE_LP(x)	(((x) & 0x1) << 30)
#define   DSI_PKT_SEQ_1_LO_PKT_12_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_1_LO_PKT_12_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_1_LO_PKT_12_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_1_LO_PKT_11_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_1_LO_PKT_11_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_1_LO_PKT_11_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_1_LO_PKT_10_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_1_LO_PKT_10_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_1_LO_PKT_10_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_1_HI 0x26
#define   DSI_PKT_SEQ_1_HI_PKT_15_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_1_HI_PKT_15_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_1_HI_PKT_15_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_1_HI_PKT_14_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_1_HI_PKT_14_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_1_HI_PKT_14_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_1_HI_PKT_13_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_1_HI_PKT_13_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_1_HI_PKT_13_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_2_LO 0x27
#define   DSI_PKT_SEQ_2_LO_SEQ_2_FORCE_LP(x)	(((x) & 0x1) << 30)
#define   DSI_PKT_SEQ_2_LO_PKT_22_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_2_LO_PKT_22_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_2_LO_PKT_22_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_2_LO_PKT_21_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_2_LO_PKT_21_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_2_LO_PKT_21_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_2_LO_PKT_20_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_2_LO_PKT_20_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_2_LO_PKT_20_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_2_HI 0x28
#define   DSI_PKT_SEQ_2_HI_PKT_25_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_2_HI_PKT_25_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_2_HI_PKT_25_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_2_HI_PKT_24_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_2_HI_PKT_24_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_2_HI_PKT_24_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_2_HI_PKT_23_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_2_HI_PKT_23_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_2_HI_PKT_23_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_3_LO 0x29
#define   DSI_PKT_SEQ_3_LO_SEQ_3_FORCE_LP(x)	(((x) & 0x1) << 30)
#define   DSI_PKT_SEQ_3_LO_PKT_32_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_3_LO_PKT_32_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_3_LO_PKT_32_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_3_LO_PKT_31_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_3_LO_PKT_31_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_3_LO_PKT_31_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_3_LO_PKT_30_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_3_LO_PKT_30_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_3_LO_PKT_30_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_3_HI 0x2a
#define   DSI_PKT_SEQ_3_HI_PKT_35_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_3_HI_PKT_35_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_3_HI_PKT_35_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_3_HI_PKT_34_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_3_HI_PKT_34_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_3_HI_PKT_34_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_3_HI_PKT_33_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_3_HI_PKT_33_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_3_HI_PKT_33_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_4_LO 0x2b
#define   DSI_PKT_SEQ_4_LO_SEQ_4_FORCE_LP(x)	(((x) & 0x1) << 30)
#define   DSI_PKT_SEQ_4_LO_PKT_42_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_4_LO_PKT_42_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_4_LO_PKT_42_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_4_LO_PKT_41_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_4_LO_PKT_41_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_4_LO_PKT_41_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_4_LO_PKT_40_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_4_LO_PKT_40_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_4_LO_PKT_40_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_4_HI 0x2c
#define   DSI_PKT_SEQ_4_HI_PKT_45_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_4_HI_PKT_45_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_4_HI_PKT_45_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_4_HI_PKT_44_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_4_HI_PKT_44_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_4_HI_PKT_44_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_4_HI_PKT_43_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_4_HI_PKT_43_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_4_HI_PKT_43_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_5_LO 0x2d
#define   DSI_PKT_SEQ_5_LO_SEQ_5_FORCE_LP(x)	(((x) & 0x1) << 30)
#define   DSI_PKT_SEQ_5_LO_PKT_52_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_5_LO_PKT_52_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_5_LO_PKT_52_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_5_LO_PKT_51_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_5_LO_PKT_51_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_5_LO_PKT_51_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_5_LO_PKT_50_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_5_LO_PKT_50_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_5_LO_PKT_50_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_PKT_SEQ_5_HI 0x2e
#define   DSI_PKT_SEQ_5_HI_PKT_55_EN(x)		(((x) & 0x1) << 29)
#define   DSI_PKT_SEQ_5_HI_PKT_55_ID(x)		(((x) & 0x3f) << 23)
#define   DSI_PKT_SEQ_5_HI_PKT_55_SIZE(x)	(((x) & 0x7) << 20)
#define   DSI_PKT_SEQ_5_HI_PKT_54_EN(x)		(((x) & 0x1) << 19)
#define   DSI_PKT_SEQ_5_HI_PKT_54_ID(x)		(((x) & 0x3f) << 13)
#define   DSI_PKT_SEQ_5_HI_PKT_54_SIZE(x)	(((x) & 0x7) << 10)
#define   DSI_PKT_SEQ_5_HI_PKT_53_EN(x)		(((x) & 0x1) << 9)
#define   DSI_PKT_SEQ_5_HI_PKT_53_ID(x)		(((x) & 0x3f) << 3)
#define   DSI_PKT_SEQ_5_HI_PKT_53_SIZE(x)	(((x) & 0x7) << 0)

#define DSI_DCS_CMDS 0x33
#define   DSI_DCS_CMDS_LT5_DCS_CMD(x)		(((x) & 0xff) << 8)
#define   DSI_DCS_CMDS_LT3_DCS_CMD(x)		(((x) & 0xff) << 0)

#define DSI_PKT_LEN_0_1 0x34
#define   DSI_PKT_LEN_0_1_LENGTH_1(x)		(((x) & 0xffff) << 16)
#define   DSI_PKT_LEN_0_1_LENGTH_0(x)		(((x) & 0xffff) << 0)

#define DSI_PKT_LEN_2_3 0x35
#define   DSI_PKT_LEN_2_3_LENGTH_3(x)		(((x) & 0xffff) << 16)
#define   DSI_PKT_LEN_2_3_LENGTH_2(x)		(((x) & 0xffff) << 0)


#define DSI_PKT_LEN_4_5 0x36
#define   DSI_PKT_LEN_4_5_LENGTH_5(x)		(((x) & 0xffff) << 16)
#define   DSI_PKT_LEN_4_5_LENGTH_4(x)		(((x) & 0xffff) << 0)

#define DSI_PKT_LEN_6_7 0x37
#define   DSI_PKT_LEN_6_7_LENGTH_7(x)		(((x) & 0xffff) << 16)
#define   DSI_PKT_LEN_6_7_LENGTH_6(x)		(((x) & 0xffff) << 0)

#define DSI_PHY_TIMING_0 0x3c
#define   DSI_PHY_TIMING_0_THSDEXIT(x)		(((x) & 0xff) << 24)
#define   DSI_PHY_TIMING_0_THSTRAIL(x)		(((x) & 0xff) << 16)
#define   DSI_PHY_TIMING_0_TDATZERO(x)		(((x) & 0xff) << 8)
#define   DSI_PHY_TIMING_0_THSPREPR(x)		(((x) & 0xff) << 0)

#define DSI_PHY_TIMING_1 0x3d
#define   DSI_PHY_TIMING_1_TCLKTRAIL(x)		(((x) & 0xff) << 24)
#define	  DSI_PHY_TIMING_1_TCLKPOST(x)		(((x) & 0xff) << 16)
#define   DSI_PHY_TIMING_1_TCLKZERO(x)		(((x) & 0xff) << 8)
#define   DSI_PHY_TIMING_1_TTLPX(x)		(((x) & 0xff) << 0)

#define DSI_PHY_TIMING_2 0x3e
#define   DSI_PHY_TIMING_2_TCLKPREPARE(x)	(((x) & 0xff) << 16)
#define	  DSI_PHY_TIMING_2_TCLKPRE(x)		(((x) & 0xff) << 8)
#define   DSI_PHY_TIMING_2_TWAKEUP(x)		(((x) & 0xff) << 0)

#define DSI_BTA_TIMING 0x3f
#define   DSI_BTA_TIMING_TTAGET(x)		(((x) & 0xff) << 16)
#define	  DSI_BTA_TIMING_TTASURE(x)		(((x) & 0xff) << 8)
#define   DSI_BTA_TIMING_TTAGO(x)		(((x) & 0xff) << 0)


#define DSI_TIMEOUT_0 0x44
#define	  DSI_TIMEOUT_0_LRXH_TO(x)		(((x) & 0xffff) << 16)
#define   DSI_TIMEOUT_0_HTX_TO(x)		(((x) & 0xffff) << 0)

#define DSI_TIMEOUT_1 0x45
#define	  DSI_TIMEOUT_1_PR_TO(x)		(((x) & 0xffff) << 16)
#define   DSI_TIMEOUT_1_TA_TO(x)		(((x) & 0xffff) << 0)

#define DSI_TO_TALLY 0x46
enum {
	IN_RESET,
	READY,
};
#define DSI_TO_TALLY_P_RESET_STATUS(x)		(((x) & 0x1) << 24)
#define DSI_TO_TALLY_TA_TALLY(x)		(((x) & 0xff) << 16)
#define DSI_TO_TALLY_LRXH_TALLY(x)		(((x) & 0xff) << 8)
#define DSI_TO_TALLY_HTX_TALLY(x)		(((x) & 0xff) << 0)

#define DSI_PAD_CONTROL 0x4b
#define DSI_PAD_CONTROL_PAD_PULLDN_ENAB(x)	(((x) & 0x1) << 28)
#define DSI_PAD_CONTROL_PAD_SLEWUPADJ(x)	(((x) & 0x7) << 24)
#define DSI_PAD_CONTROL_PAD_SLEWDNADJ(x)	(((x) & 0x7) << 20)
#define DSI_PAD_CONTROL_PAD_PREEMP_EN(x)	(((x) & 0x1) << 19)
#define DSI_PAD_CONTROL_PAD_PDIO_CLK(x)		(((x) & 0x1) << 18)
#define DSI_PAD_CONTROL_PAD_PDIO(x)		(((x) & 0x3) << 16)
#define DSI_PAD_CONTROL_PAD_LPUPADJ(x)		(((x) & 0x3) << 14)
#define DSI_PAD_CONTROL_PAD_LPDNADJ(x)		(((x) & 0x3) << 12)

#define DSI_PAD_CONTROL_0_VS1 0x4b
#define DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_CLK_ENAB(x)	(((x) & 0x1) << 24)
#define DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_ENAB(x)	(((x) & 0xf) << 16)
#define DSI_PAD_CONTROL_0_VS1_PAD_PDIO_CLK(x)		(((x) & 0x1) << 8)
#define DSI_PAD_CONTROL_0_VS1_PAD_PDIO(x)		(((x) & 0xf) << 0)

#define DSI_PAD_CONTROL_CD 0x4c
#define DSI_PAD_CONTROL_CD_VS1 0x4c
#define DSI_PAD_CD_STATUS 0x4d
#define DSI_PAD_CD_STATUS_VS1 0x4d

#define DSI_PAD_CONTROL_1_VS1 0x4f
#define DSI_PAD_OUTADJ3(x)	(((x) & 0x7) << 12)
#define DSI_PAD_OUTADJ2(x)	(((x) & 0x7) << 8)
#define DSI_PAD_OUTADJ1(x)	(((x) & 0x7) << 4)
#define DSI_PAD_OUTADJ0(x)	(((x) & 0x7) << 0)

#define DSI_PAD_CONTROL_2_VS1 0x50
#define DSI_PAD_SLEWUPADJ(x)	(((x) & 0x7) << 16)
#define DSI_PAD_SLEWDNADJ(x)	(((x) & 0x7) << 12)
#define DSI_PAD_LPUPADJ(x)	(((x) & 0x7) << 8)
#define DSI_PAD_LPDNADJ(x)	(((x) & 0x7) << 4)
#define DSI_PAD_OUTADJCLK(x)	(((x) & 0x7) << 0)

#define DSI_PAD_CONTROL_3_VS1 0x51
#define DSI_PAD_PDVCLAMP(x)	(((x) & 0x1) << 28)
#define DSI_PAD_BANDWD_IN(x)	(((x) & 0x1) << 16)
#define DSI_PAD_PREEMP_PD_CLK(x)	(((x) & 0x3) << 12)
#define DSI_PAD_PREEMP_PU_CLK(x)	(((x) & 0x3) << 8)
#define DSI_PAD_PREEMP_PD(x)	(((x) & 0x3) << 4)
#define DSI_PAD_PREEMP_PU(x)	(((x) & 0x3) << 0)

#define DSI_PAD_CONTROL_4_VS1 0x52
#define DSI_PAD_HS_BSO_CLK(x)	(((x) & 0x1) << 28)
#define DSI_PAD_HS_BSO(x)	(((x) & 0xf) << 20)
#define DSI_PAD_LP_BSO_CLK(x)	(((x) & 0x1) << 16)
#define DSI_PAD_LP_BSO(x)	(((x) & 0xf) << 8)
#define DSI_PAD_TXBW_EN(x)	(((x) & 0x1) << 4)
#define DSI_PAD_REV_CLK(x)	(((x) & 0x1) << 0)

#define DSI_VID_MODE_CONTROL 0x4e

#define DSI_GANGED_MODE_CONTROL 0x53
#define DSI_GANGED_MODE_CONTROL_EN(x)			(((x) & 0x1) << 0)

#define DSI_GANGED_MODE_START 0x54
#define DSI_GANGED_MODE_START_POINTER(x)		(((x) & 0x1fff) << 0)

#define DSI_GANGED_MODE_SIZE 0x55
#define DSI_GANGED_MODE_SIZE_VALID_LOW_WIDTH(x)		(((x) & 0x1fff) << 16)
#define DSI_GANGED_MODE_SIZE_VALID_HIGH_WIDTH(x)	(((x) & 0x1fff) << 0)

#endif

