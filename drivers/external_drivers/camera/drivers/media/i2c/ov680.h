/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OV680_H__
#define __OV680_H__

#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/types.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/acpi.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#ifdef CONFIG_GMIN_INTEL_MID
#include <linux/atomisp_gmin_platform.h>
#else
#include <media/v4l2-chip-ident.h>
#endif

#define OV680_NAME "ov680"
#define OV680_CHIP_ID 0x680

#define I2C_MSG_LENGTH 0x2

#define REG_SC_BASE 0x6000
#define REG_SCCB_SLAVE_BASE 0x6100
#define REG_MIPI_RX1_BASE 0x6200
#define REG_MIPI_RX0_BASE 0x6300
#define REG_MIPI_RX3_BASE 0x6400
#define REG_MIPI_RX2_BASE 0x6500
#define REG_IMAGE_STITCH1_BASE 0x6600
#define REG_IMAGE_STITCH2_BASE 0x6700
#define REG_IMAGE_STITCH3_BASE 0x6800
#define REG_MIPI_TX_BASE 0x6900
#define REG_MIPI_BIST_BASE 0x6a00
#define REG_SCCB_MASTER_BASE 0x6af0
#define REG_MC_BASE 0x6b00
#define REG_AVERAGE1_BASE 0x6c00
#define REG_AVERAGE2_BASE 0x6c80
#define REG_AEC1_BASE 0x6d00
#define REG_AEC2_BASE 0x6d80
#define REG_AEC1_PK_BASE 0x6e00
#define REG_AWB1_PK_BASE 0x6f00
#define REG_AWB2_PK_BASE 0x6f80
#define REG_ISP_CONTRL_BASE 0x7000
#define REG_LSC_BASE 0x7180
#define REG_AWB1_BASE 0x7200
#define REG_CONTRAST1_BASE 0x7280
#define REG_DPC1_BASE 0x7300
#define REG_DNS1_BASE 0x7380
#define REG_CIP1_BASE 0x7400
#define REG_CMX1_BASE 0x7480
#define REG_GAMMA1_BASE 0x7500
#define REG_UVDARK1_BASE 0x7580
#define REG_SDE1_BASE 0x7600
#define REG_SCALE1_BASE 0x7680
#define REG_YUV_CROP1_BASE 0x7700
#define REG_PRE_ISP_2_BASE 0x7800
#define REG_LSC2_BASE 0x7880
#define REG_AWB2_BASE 0x7900
#define REG_CONSTRAST2_BASE 0x7980
#define REG_DPC2_BASE 0x7a00
#define REG_DNS2_BASE 0x7a80
#define REG_CIP2_BASE 0x7b00
#define REG_COLOR_MATRIX2_BASE 0x7b80
#define REG_GAMMA2_BASE 0x7c00
#define REG_UVDARK2_BASE 0x7c80
#define REG_SDE2_BASE 0x7d00
#define REG_SCALE2_BASE 0x7d80
#define REG_YUV_CROP2_BASE 0x7e00
#define REG_FIRWWARE_BASE 0x8000

#define REG_SC_00 (REG_SC_BASE + 0x00)
#define REG_SC_03 (REG_SC_BASE + 0x03)
#define REG_SC_06 (REG_SC_BASE + 0x06)
#define REG_SC_0A (REG_SC_BASE + 0x0a)
#define REG_SC_66 (REG_SC_BASE + 0x66)
#define REG_SC_90 (REG_SC_BASE + 0x90)
#define REG_SC_93 (REG_SC_BASE + 0x93)

#define REG_SC_03_GLOBAL_ENABLED  0x10
#define REG_SC_03_GLOBAL_DISABLED 0x11
#define REG_SC_66_GLOBAL_READY   0x18

#define REG_SCCB_SLAVE_03 (REG_SCCB_SLAVE_BASE + 0x03)

#define REG_MC_17 (REG_MC_BASE + 0x17)
#define REG_MC_18 (REG_MC_BASE + 0x18)
#define REG_MC_19 (REG_MC_BASE + 0x19)
#define REG_MC_1A (REG_MC_BASE + 0x1a)
#define REG_MC_1B (REG_MC_BASE + 0x1b)
#define REG_MC_1C (REG_MC_BASE + 0x1c)
#define REG_MC_1D (REG_MC_BASE + 0x1d)

#define REG_YUV_CROP1_08 (REG_YUV_CROP1_BASE + 0x08)
#define REG_YUV_CROP1_0B (REG_YUV_CROP1_BASE + 0x0b)

/* detect input sensor */
#define OV680_MAX_INPUT_SENSOR_NUM 2
#define OV680_SENSOR_0_ID 0x20
#define OV680_SENSOR_1_ID 0x6c
#define OV680_SENSOR_REG0_VAL 0x97
#define OV680_SENSOR_REG1_VAL 0x28

/* ov680 command set definition */
#define OV680_CMD_CIR_REG REG_MC_17 /* command interrupt register */
#define OV680_CMD_OP_REG REG_MC_18 /* operation function */
#define OV680_CMD_SUB_OP_REG REG_MC_19 /* sub function */
#define OV680_CMD_PARAMETER_1 REG_MC_1A
#define OV680_CMD_PARAMETER_2 REG_MC_1B
#define OV680_CMD_PARAMETER_3 REG_MC_1C
#define OV680_CMD_PARAMETER_4 REG_MC_1D

#define OV680_CMD_CIR_IDLE_STATE 0x10
#define OV680_CMD_CIR_OP_STATE 0x80
#define OV680_CMD_CIR_SENSOR_ACCESS_STATE 0xf0

#define OV680_CMD_SENSOR_CONFIG 0x81
#define OV680_CMD_STREAM_CONFIG 0x82
#define OV680_CMD_ISP_CONFIG 0x83
#define OV680_CMD_READ_SENSOR 0x84
#define OV680_CMD_WRITE_SENSOR 0x85

#define OV680_CONFIG_SENSOR_720p 0x00
#define OV680_CONFIG_SENSOR_VGA 0x01
#define OV680_CONFIG_SENSOR_15FPS 0x11
#define OV680_CONFIG_SENSOR_30FPS 0x13
#define OV680_CONFIG_SENSOR_60FPS 0x15

/* stream config sub function */
#define OV680_CONFIG_STREAM_DEFAULT 0x01
#define OV680_CONFIG_STREAM_S0_S0_A 0x00
#define OV680_CONFIG_STREAM_S1_S1_A 0x11
#define OV680_CONFIG_STREAM_S0_S1_A 0x01
#define OV680_CONFIG_STREAM_S1_S0_A 0x10
/* for 0x6b1b - virtual channel*/
#define OV680_CONFIG_STREAM_INDEX_VC0 0x00
#define OV680_CONFIG_STREAM_INDEX_VC1 0x55
#define OV680_CONFIG_STREAM_INDEX_VC2 0xAA
#define OV680_CONFIG_STREAM_INDEX_VC3 0xFF
/* for 0x6b1c - index output group*/
#define OV680_CONFIG_STREAM_INDEX_GROUP_1_SENSOR 0x01 /* 1280 x 720 */
#define OV680_CONFIG_STREAM_INDEX_GROUP_2_SENSOR 0x02 /* 2560 x 720 */
#define OV680_CONFIG_STREAM_INDEX_GROUP_NA_1 0x03
#define OV680_CONFIG_STREAM_INDEX_GROUP_NA_2 0x04
/* for 0x6b1d - index output group*/
#define OV680_CONFIG_STREAM_INDEX_GROUP_720P 0x00 /* 1280 x 720 */
#define OV680_CONFIG_STREAM_INDEX_GROUP_VGA 0x01 /* 640 x 480 */

/* config isp */
#define OV680_CONFIG_ISP_AEC_AGC_ON 0x00
#define OV680_CONFIG_ISP_AEC_AGC_OFF 0x01
#define OV680_CONFIG_ISP_AEC_TARGET_SET 0x02
#define OV680_CONFIG_ISP_AEC_WEIGHT_SET 0x03

#define OV680_CONFIG_ISP_DPC_ON 0x00
#define OV680_CONFIG_ISP_DPC_OFF 0x01
#define OV680_CONFIG_ISP_ISP_ON 0x02
#define OV680_CONFIG_ISP_ISP_OFF 0x03
#define OV680_CONFIG_ISP_LENC_ON 0x02
#define OV680_CONFIG_ISP_LENS_OFF 0x03

#define OV680_FIRMWARE_SIZE (33020) /* size =0xb610 - 0x8000 */
#define OV680_MAX_RATIO_MISMATCH 10 /* Unit in percentage */

enum ov680_tok_type {
	OV680_8BIT  = 0x0001,
	OV680_16BIT = 0x0002,
	OV680_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	OV680_TOK_DELAY  = 0xfe00	/* delay token for reg list */
};
#define OV680_TOK_MASK	0xfff0

/**
 * struct ov680_reg - 680 sensor  register format
 * @reg: 16-bit offset to register
 * @val: 8-bit register value
 */
struct ov680_reg {
	enum ov680_tok_type type;
	u16 reg;
	u8 val;
};

#define OV680_MAX_WRITE_BUF_SIZE	32
struct ov680_write_buffer {
	u16 addr;
	u8 data[OV680_MAX_WRITE_BUF_SIZE];
};

struct ov680_write_ctrl {
	int index;
	struct ov680_write_buffer buffer;
};

enum ov680_contexts {
	CONTEXT_PREVIEW = 0,
	CONTEXT_SNAPSHOT,
	CONTEXT_VIDEO,
	CONTEXT_NUM
};

struct ov680_res_struct {
	u16 width;
	u16 height;
	u16 fps;
};

struct ov680_firmware {
	u32 cmd_count; /* The total count of commands stored in FW */
	u32 cmd_size; /* The size of each command's storing space */
};

struct ov680_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct mutex input_lock; /* Serialize access to the device settings */
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int fw_index;
	int streaming;
	unsigned int num_lanes;
	int bayer_fmt;
	enum v4l2_mbus_pixelcode mbus_pixelcode;
	bool sys_activated;
	bool power_on;
	bool probed;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *run_mode;

	enum ov680_contexts cur_context;

	const struct firmware *fw;
	const struct ov680_reg *ov680_fw;
};

static struct ov680_res_struct ov680_res_list[] = {
	{
		.width = 1280,
		.height = 1440,
		.fps = 30,
	},
};
#define N_FW (ARRAY_SIZE(ov680_res_list))

/* ov680 init clock pll instructions */
static struct ov680_reg const ov680_init_clock_pll[] = {
	/*
	 *----Start OV680 Clock and PLL configuration-----
	 *new setting for 19.2M---------------------------
	 *CCLK (sensor input clock/camera clock output)
	 */

	/* CCLK (camera clock) divider, 19.2/2=9.6MHz */
	{OV680_8BIT, 0x601a, 0x02},

	/* Common initial setting */
	/* Reset MC to disable looking for F/W from E2PROM */
	{OV680_8BIT, 0x6b00, 0x10},
	/* MIPI Rx CIF sel Rx0 PHY clk (MIPI receiver sync clock select */
	{OV680_8BIT, 0x6001, 0xa8},
	/* Clock Drive Strength --YM (CMD_RDY driver - open drain) */
	{OV680_8BIT, 0x6060, 0x3C},
	/* PLL1 - for system clock */
	{OV680_8BIT, 0x6020, 0x64},
	{OV680_8BIT, 0x6021, 0x22},
	{OV680_8BIT, 0x6022, 0x15},
	{OV680_8BIT, 0x6023, 0x12},
	{OV680_8BIT, 0x6024, 0x90},
	/* sys clk = pll_sysclk/2 = 46.4 MHz */
	{OV680_8BIT, 0x6018, 0x02},
	/* PLL2 - for MIPI */
	{OV680_8BIT, 0x6025, 0x59},
	{OV680_8BIT, 0x6026, 0x22},
	{OV680_8BIT, 0x6027, 0x15},
	{OV680_8BIT, 0x6028, 0x01},
	{OV680_8BIT, 0x6029, 0xa0},
	/* mipi_pclk = 96/2 = 48 */
	{OV680_8BIT, 0x6019, 0x02},
	/* 1000/mipi_clk */
	{OV680_8BIT, 0x6937, 0x11},
	/* Sel PLL2 sys_clk as Tx clk, PLL1 sys_clk as sys clk */
	{OV680_8BIT, 0x6103, 0x71},
	/* CCLK (sensor input clock) */
	/* Release AEC2&1, ISP2&1 */
	{OV680_8BIT, 0x6009, 0x00},
	/* Release Tx, stch 1~3, AWB1&2 */
	{OV680_8BIT, 0x600a, 0x00},
	/* Release Rx0~3 */
	{OV680_8BIT, 0x600b, 0x00},
	/* Release Tx, Rx PHY */
	{OV680_8BIT, 0x600c, 0x00},
	/* Enable CCLK, SCCB slave clock, MC, MC RAM */
	{OV680_8BIT, 0x6010, 0x3c},
	/* AEC2&1, ISP2&1 */
	{OV680_8BIT, 0x6011, 0xff},
	/* Sync FIFO, stch FIFO, Tx, stch 1~3, AWB1&2 */
	{OV680_8BIT, 0x6012, 0xff},
	/* Enable Rx0~Rx3 PHY clock */
	{OV680_8BIT, 0x6002, 0xff},

	{OV680_TOK_TERM, 0, 0}
};

/* ov680 pll change before fw loading */
static struct ov680_reg const ov680_dw_fw_change_pll[] = {
	/*
	 * OV680 Firmware download--Start
	 * ----inlude OV680 and OV6710 setting in firmware-------------
	 * change to pll clock
	 */

	/* Select PLL2 as system clock & MIPI transmitter clock source */
	{OV680_8BIT, 0x6103, 0x20},

	{OV680_8BIT, 0x6b00, 0x14},
	{OV680_8BIT, 0x6004, 0x00},
	{OV680_8BIT, 0x6008, 0x00},
	{OV680_8BIT, 0x6010, 0xff},
	{OV680_8BIT, 0x6101, 0x02},
	{OV680_8BIT, 0x6b0c, 0x00},
	{OV680_8BIT, 0x6b0d, 0x00},

	{OV680_TOK_TERM, 0, 0}
};

/* ov680 pll restore after fw loading */
static struct ov680_reg const ov680_dw_fw_change_back_pll[] = {
	/* Change back PLL */
	{OV680_8BIT, 0x6103, 0x71},

	{OV680_8BIT, 0x6b56, 0x22},
	{OV680_8BIT, 0x6b57, 0xab},
	{OV680_8BIT, 0x6b00, 0x1c},
	{OV680_8BIT, 0x6b01, 0x04},
	{OV680_8BIT, 0x6b0e, 0xff},

	{OV680_TOK_TERM, 0, 0}
};

static struct ov680_reg const ov680_720p_2s_embedded_stream_on[] = {
	{OV680_8BIT, 0x6003, 0x10},

	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x01},
	{OV680_8BIT, 0x6B1B, 0x00},
	{OV680_8BIT, 0x6B1C, 0x01},
	{OV680_8BIT, 0x6B17, 0xF0},

	{OV680_8BIT, 0x6011, 0xFF},  /* AEC on */

	{OV680_TOK_TERM, 0, 0}
};

static struct ov680_reg const ov680_720p_2s_embedded_factory_stream_on[] = {
/* BEGIN ov680 factory mode */
	{OV680_8BIT, 0x6003, 0x11},
	{OV680_8BIT, 0x6011, 0xCF}, /* AEC off */

	/* 0x0202=0x03 default exposure 33ms */
	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x02},
	{OV680_8BIT, 0x6B1B, 0x02},
	{OV680_8BIT, 0x6B1C, 0x03},
	{OV680_8BIT, 0x6B17, 0xF0},

	/* 0x0203=0x20 default exposure 33ms */
	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x02},
	{OV680_8BIT, 0x6B1B, 0x03},
	{OV680_8BIT, 0x6B1C, 0x20},
	{OV680_8BIT, 0x6B17, 0xF0},

	/* 0x0204=00 Sensor 0 Gain=0 */
	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x02},
	{OV680_8BIT, 0x6B1B, 0x04},
	{OV680_8BIT, 0x6B1C, 0x00},
	{OV680_8BIT, 0x6B17, 0xF0},

	/* 0x0205=00 Sensor 1 Gain=0 */
	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x02},
	{OV680_8BIT, 0x6B1B, 0x05},
	{OV680_8BIT, 0x6B1C, 0x00},
	{OV680_8BIT, 0x6B17, 0xF0},

	/* Sensor 0 Disable sharpening */
	{OV680_8BIT, 0x7408, 0x00},
	{OV680_8BIT, 0x7409, 0x00},
	{OV680_8BIT, 0x740A, 0x00},
	{OV680_8BIT, 0x740B, 0x00},
	{OV680_8BIT, 0x740C, 0x08},

	/* Sensor 0 Disable AGC and set gain to 0 */
	{OV680_8BIT, 0x6E01, 0x03},
	{OV680_8BIT, 0x6E02, 0x20},
	{OV680_8BIT, 0x6E03, 0x03},
	{OV680_8BIT, 0x6E04, 0x00},
	{OV680_8BIT, 0x6E05, 0x00},

	/* Sensor 1 Disable sharpening */
	{OV680_8BIT, 0x7B08, 0x00},
	{OV680_8BIT, 0x7B09, 0x00},
	{OV680_8BIT, 0x7B0A, 0x00},
	{OV680_8BIT, 0x7B0B, 0x00},
	{OV680_8BIT, 0x7B0C, 0x08},

	/* Sensor 1 Disable AGC and set gain to 0 */
	{OV680_8BIT, 0x6E81, 0x03},
	{OV680_8BIT, 0x6E82, 0x20},
	{OV680_8BIT, 0x6E83, 0x03},
	{OV680_8BIT, 0x6E84, 0x00},
	{OV680_8BIT, 0x6E85, 0x00},
/* END ov680 factory mode */

	/*
	 * Embedded stream on commands,
	 * do not enable AEC
	 */
	{OV680_8BIT, 0x6003, 0x10},

	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x01},
	{OV680_8BIT, 0x6B1B, 0x00},
	{OV680_8BIT, 0x6B1C, 0x01},
	{OV680_8BIT, 0x6B17, 0xF0},

	{OV680_TOK_TERM, 0, 0}
};

static struct ov680_reg const ov680_720p_2s_embedded_stream_off[] = {
	{OV680_8BIT, 0x6003, 0x11},
	{OV680_8BIT, 0x6011, 0xcf}, /* AEC off */

	/* set min exposure start */
	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x02},
	{OV680_8BIT, 0x6B1B, 0x02},
	{OV680_8BIT, 0x6B1C, 0x00},
	{OV680_8BIT, 0x6B17, 0xF0},

	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x02},
	{OV680_8BIT, 0x6B1B, 0x03},
	{OV680_8BIT, 0x6B1C, 0x10},
	{OV680_8BIT, 0x6B17, 0xF0},
	/* set min exposure end */

	/* sleep sensor */
	{OV680_8BIT, 0x6B18, 0x85},
	{OV680_8BIT, 0x6B19, 0x90},
	{OV680_8BIT, 0x6B1A, 0x01},
	{OV680_8BIT, 0x6B1B, 0x00},
	{OV680_8BIT, 0x6B1C, 0x00},
	{OV680_8BIT, 0x6B17, 0xF0},

	{OV680_TOK_TERM, 0, 0}
};

static struct ov680_reg const ov680_720p_2s_embedded_line[] = {
	/* Embedded Line Additional Setting 07/31/2014 */
	{OV680_8BIT, 0x6b18, 0x81}, /* TYPE_Sensor_Config */
	{OV680_8BIT, 0x6b19, 0x05}, /* 2560x720p SBS */
	{OV680_8BIT, 0x6b17, 0x80}, /* streaming */
	{OV680_TOK_DELAY, 0x0, 0x64}, /* sleep 100ms */
	{OV680_TOK_DELAY, 0x0, 0x64}, /* sleep 100ms */

	{OV680_8BIT, 0x6099, 0x00},
	{OV680_8BIT, 0x6914, 0x52},
	{OV680_8BIT, 0x6096, 0x11},

	{OV680_8BIT, 0x6b01, 0x24},
	{OV680_8BIT, 0x6b02, 0xc0},
	{OV680_8BIT, 0x6003, 0x10},

	{OV680_TOK_DELAY, 0x0, 0x64}, /* sleep 100ms */
	{OV680_TOK_DELAY, 0x0, 0x64}, /* sleep 100ms */

	{OV680_8BIT, 0x600a, 0x00},
	{OV680_TOK_TERM, 0, 0}
};

static struct ov680_reg const ov680_embedded_line_off[] = {
	/* @@ embedded line off */
	{OV680_8BIT, 0x6b18, 0x81}, /* TYPE_Sensor_Config */
	{OV680_8BIT, 0x6b19, 0x05}, /* 2560x720p SBS */
	{OV680_8BIT, 0x6b17, 0x80}, /* streaming */
	{OV680_TOK_DELAY, 0x0, 0x64}, /* sleep 100ms */

	{OV680_8BIT, 0x6003, 0x11},
	{OV680_8BIT, 0x6b01, 0x04},
	{OV680_8BIT, 0x6b02, 0x00},
	{OV680_8BIT, 0x6b03, 0x00},

	{OV680_8BIT, 0x6096, 0x10},
	{OV680_8BIT, 0x6914, 0x00},
	{OV680_8BIT, 0x7002, 0x02},
	{OV680_8BIT, 0x7032, 0x02},

	{OV680_8BIT, 0x700c, 0x04},
	{OV680_8BIT, 0x700d, 0xff},
	{OV680_8BIT, 0x700e, 0x02},
	{OV680_8BIT, 0x700f, 0xcf},

	{OV680_8BIT, 0x7704, 0x05},
	{OV680_8BIT, 0x7705, 0x00},
	{OV680_8BIT, 0x7706, 0x02},
	{OV680_8BIT, 0x7707, 0xd0},

	{OV680_8BIT, 0x703c, 0x04},
	{OV680_8BIT, 0x703d, 0xff},
	{OV680_8BIT, 0x703e, 0x02},
	{OV680_8BIT, 0x703f, 0xcf},

	{OV680_8BIT, 0x7800, 0x00},
	{OV680_8BIT, 0x7801, 0x01},
	{OV680_8BIT, 0x7811, 0x30},

	{OV680_8BIT, 0x7e04, 0x05},
	{OV680_8BIT, 0x7e05, 0x00},
	{OV680_8BIT, 0x7e06, 0x02},
	{OV680_8BIT, 0x7e07, 0xd0},

	{OV680_8BIT, 0x7002, 0x02},
	{OV680_8BIT, 0x7032, 0x02},

	{OV680_8BIT, 0x608c, 0x05},
	{OV680_8BIT, 0x608d, 0x00},
	{OV680_8BIT, 0x608e, 0x05},
	{OV680_8BIT, 0x608f, 0xa0},
	{OV680_8BIT, 0x6090, 0x05},
	{OV680_8BIT, 0x6091, 0x00},
	{OV680_8BIT, 0x6092, 0x02},
	{OV680_8BIT, 0x6093, 0xd0},

	{OV680_8BIT, 0x6AF1, 0x20},
	{OV680_8BIT, 0x6AF2, 0x01},
	{OV680_8BIT, 0x6AF3, 0x00},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x20},
	{OV680_8BIT, 0x6AF2, 0x43},
	{OV680_8BIT, 0x6AF3, 0x07},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x20},
	{OV680_8BIT, 0x6AF2, 0x01},
	{OV680_8BIT, 0x6AF3, 0x00},
	{OV680_8BIT, 0x6AF5, 0x01},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x6c},
	{OV680_8BIT, 0x6AF2, 0x01},
	{OV680_8BIT, 0x6AF3, 0x00},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_TOK_DELAY, 0x0, 0x64}, /* sleep 100ms */

	{OV680_8BIT, 0x6AF1, 0x6c},
	{OV680_8BIT, 0x6AF2, 0x43},
	{OV680_8BIT, 0x6AF3, 0x07},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x6c},
	{OV680_8BIT, 0x6AF2, 0x01},
	{OV680_8BIT, 0x6AF3, 0x00},
	{OV680_8BIT, 0x6AF5, 0x01},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6880, 0x01},
	{OV680_8BIT, 0x6881, 0x60},
	{OV680_8BIT, 0x6840, 0x01},
	{OV680_8BIT, 0x6841, 0x60},

	{OV680_8BIT, 0x6b02, 0x0c},
	{OV680_8BIT, 0x6b03, 0x00},

	/*
	 * sensor system change clock -48M to match OV8858 clock support
	 * for HW sync
	 */
	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x01},
	{OV680_8BIT, 0x6AF3, 0x00},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x30},
	{OV680_8BIT, 0x6AF3, 0x02},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x38},
	{OV680_8BIT, 0x6AF3, 0x23},
	{OV680_8BIT, 0x6AF5, 0x30},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x38},
	{OV680_8BIT, 0x6AF3, 0x26},
	{OV680_8BIT, 0x6AF5, 0x05},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x38},
	{OV680_8BIT, 0x6AF3, 0x27},
	{OV680_8BIT, 0x6AF5, 0xfd},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x43},
	{OV680_8BIT, 0x6AF3, 0x0d},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x43},
	{OV680_8BIT, 0x6AF3, 0x0e},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x43},
	{OV680_8BIT, 0x6AF3, 0x0f},
	{OV680_8BIT, 0x6AF5, 0x00},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x4f},
	{OV680_8BIT, 0x6AF3, 0x07},
	{OV680_8BIT, 0x6AF5, 0x23},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x4f},
	{OV680_8BIT, 0x6AF3, 0x0f},
	{OV680_8BIT, 0x6AF5, 0xa2},
	{OV680_8BIT, 0x6AF9, 0x37},

	{OV680_TOK_DELAY, 0x0, 0xC6}, /* sleep 198ms */

	/* sensor wake up */
	{OV680_8BIT, 0x6AF1, 0x90},
	{OV680_8BIT, 0x6AF2, 0x01},
	{OV680_8BIT, 0x6AF3, 0x00},
	{OV680_8BIT, 0x6AF5, 0x01},
	{OV680_8BIT, 0x6AF9, 0x37},

	/* Turn on AEC/AGC */
	{OV680_8BIT, 0x6B48, 0x00},
	{OV680_8BIT, 0x6B19, 0x00},
	{OV680_8BIT, 0x6B18, 0x83},
	{OV680_8BIT, 0x6B17, 0x80},

	{OV680_TOK_DELAY, 0x0, 0x64}, /* sleep 100ms */

	{OV680_TOK_TERM, 0, 0}
};

#endif
