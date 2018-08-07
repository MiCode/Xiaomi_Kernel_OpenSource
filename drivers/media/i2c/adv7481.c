/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/media.h>
#include <media/v4l2-ioctl.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>

#include <media/adv7481.h>
#include <media/msm_ba.h>

#include "adv7481_reg.h"

#include "msm_cci.h"
#include "msm_camera_i2c.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"
#include "linux/hdmi.h"

#define DRIVER_NAME "adv7481"

#define I2C_RW_DELAY			1
#define I2C_SW_RST_DELAY		5000
#define GPIO_HW_RST_DELAY_HI	10000
#define GPIO_HW_RST_DELAY_LOW	10000
#define SDP_MIN_SLEEP		5000
#define SDP_MAX_SLEEP		6000
#define SDP_NUM_TRIES		50
#define LOCK_MIN_SLEEP		5000
#define LOCK_MAX_SLEEP		6000
#define LOCK_NUM_TRIES		200

#define MAX_DEFAULT_WIDTH       1280
#define MAX_DEFAULT_HEIGHT      720
#define MAX_DEFAULT_FRAME_RATE  60
#define MAX_DEFAULT_PIX_CLK_HZ  74240000

#define ONE_MHZ_TO_HZ			1000000
#define I2C_BLOCK_WRITE_SIZE	1024
#define ADV_REG_STABLE_DELAY	70		/* ms*/

#define AVI_INFOFRAME_SIZE		31
#define INFOFRAME_DATA_SIZE		28

enum adv7481_gpio_t {

	CCI_I2C_SDA = 0,
	CCI_I2C_SCL,

	ADV7481_GPIO_RST,

	ADV7481_GPIO_INT1,
	ADV7481_GPIO_INT2,
	ADV7481_GPIO_INT3,

	ADV7481_GPIO_MAX,
};

enum adv7481_resolution {
	RES_1080P = 0,
	RES_720P,
	RES_576P_480P,
	RES_MAX,
};

struct resolution_config {
	uint32_t lane_cnt;
	uint32_t settle_cnt;
	char resolution[20];
};

struct adv7481_state {
	struct device *dev;

	/* VREG */
	struct camera_vreg_t *cci_vreg;
	struct regulator *cci_reg_ptr[MAX_REGULATOR];
	int32_t regulator_count;

	/* I2C */
	struct msm_camera_i2c_client i2c_client;
	u32 cci_master;
	u32 i2c_slave_addr;

	/* V4L2 Data */
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_hdl;
	struct v4l2_dv_timings timings;
	struct v4l2_ctrl *cable_det_ctrl;

	/* media entity controls */
	struct media_pad pad;

	struct workqueue_struct *work_queues;
	struct mutex		mutex;

	uint8_t i2c_io_addr;
	uint8_t i2c_csi_txa_addr;
	uint8_t i2c_csi_txb_addr;
	uint8_t i2c_hdmi_addr;
	uint8_t i2c_hdmi_inf_addr;
	uint8_t i2c_edid_addr;
	uint8_t i2c_cp_addr;
	uint8_t i2c_sdp_addr;
	uint8_t i2c_rep_addr;
	uint8_t i2c_cbus_addr;

	/* device status and Flags */
	int irq;
	int device_num;
	int powerup;
	int cec_detected;
	int clocks_requested;

	/* GPIOs */
	struct gpio gpio_array[ADV7481_GPIO_MAX];

	/* routing configuration data */
	int csia_src;
	int csib_src;
	int mode;

	/* AVI Infoframe Params */
	struct avi_infoframe_params hdmi_avi_infoframe;

	/* resolution configuration */
	struct resolution_config res_configs[RES_MAX];

	/* CSI configuration data */
	int tx_auto_params;
	enum adv7481_mipi_lane tx_lanes;

	/* worker to handle interrupts */
	struct delayed_work irq_delayed_work;
};

struct adv7481_hdmi_params {
	uint16_t pll_lock;
	uint32_t tmds_freq;
	uint16_t vert_lock;
	uint16_t horz_lock;
	uint16_t pix_rep;
	uint16_t color_depth;
};

struct adv7481_vid_params {
	uint32_t pix_clk;
	uint16_t act_pix;
	uint16_t act_lines;
	uint16_t tot_pix;
	uint16_t tot_lines;
	uint32_t fr_rate;
	uint16_t intrlcd;
};

const uint8_t adv7481_default_edid_data[] = {
/* Block 0 (EDID Base Block) */
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
/* Vendor Identification */
0x45, 0x23, 0xDD, 0xDD, 0x01, 0x01, 0x01, 0x01,
/* Week of Manufacture */
0x1E,
/* Year of Manufacture */
0x19,
/* EDID Structure Version and Revision */
0x01, 0x03,
/* Display Parameters */
0x80, 0x10, 0x09, 0x78, 0x0A,
/* Color characteristics */
0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27, 0x12,
0x48, 0x4C,
/* Established Timings */
0x21, 0x08, 0x00,
/* Standard Timings */
0xD1, 0xC0, 0xD1, 0x40, 0x81, 0xC0, 0x81, 0x40,
0x3B, 0xC0, 0x3B, 0x40, 0x31, 0xC0, 0x31, 0x40,
/* Detailed Timings Block */
0x1A, 0x36, 0x80, 0xA0, 0x70, 0x38, 0x1F, 0x40,
0x30, 0x20, 0x35, 0x00, 0x40, 0x44, 0x21, 0x00,
0x00, 0x1E,
/* Monitor Descriptor Block 2 */
0x00, 0x19, 0x00, 0xA0, 0x50, 0xD0, 0x15, 0x20,
0x30, 0x20, 0x35, 0x00, 0x80, 0xD8, 0x10, 0x00,
0x00, 0x1E,
/* Monitor Descriptor Block 3 */
0x00, 0x00, 0x00, 0xFD, 0x00, 0x17, 0x4B, 0x0F,
0x46, 0x0F, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20,
/* Monitor Descriptor Block 4 */
0x00, 0x00, 0x00, 0xFC, 0x00, 0x54, 0x56, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20,
/* Extension Flag CEA */
0x01,
/* Checksum */
0x16,

/* Block 1 (Extension Block) */
/* Extension Header */
0x02, 0x03, 0x22,
/* Display supports */
0x71,
/* Video Data Block */
0x4C, 0x84, 0x13, 0x3C, 0x03, 0x02, 0x11, 0x12,
0x01, 0x90, 0x1F, 0x20, 0x22,
/* HDMI VSDB */
/* Deep color All, Max_TMDS_Clock = 150 MHz */
0x68, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x80,
/* hdmi_video_present=0, 3d_present=0 */
0x1E, 0x00,
/* Audio Data Block */
0x23,
0x09, 0x07, 0x07, /* LPCM, max 2 ch, 48k, 44.1k, 32k */
/* Speaker Allocation Data Block */
0x83, 0x01, 0x00, 0x00,
/* Detailed Timing Descriptor */
0x1A, 0x36, 0x80, 0xA0, 0x70, 0x38, 0x1F, 0x40,
0x30, 0x20, 0x35, 0x00, 0x40, 0x44, 0x21, 0x00,
0x00, 0x1E,
/* Detailed Timing Descriptor */
0x00, 0x19, 0x00, 0xA0, 0x50, 0xD0, 0x15, 0x20,
0x30, 0x20, 0x35, 0x00, 0x80, 0xD8, 0x10, 0x00,
0x00, 0x1E,
/* Detailed Timing Descriptor */
0x41, 0x0A, 0xD0, 0xA0, 0x20, 0xE0, 0x13, 0x10,
0x30, 0x20, 0x3A, 0x00, 0xD8, 0x90, 0x00, 0x00,
0x00, 0x18,
/* Detailed Timing Descriptor */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00,
/* Detailed Timing Descriptor */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00,
/* DTD padding */
0x00, 0x00, 0x00,
/* Checksum */
0x8C
};

#define ADV7481_EDID_SIZE ARRAY_SIZE(adv7481_default_edid_data)

static u32 adv7481_inp_to_ba(u32 adv_input);
static bool adv7481_is_timing_locked(struct adv7481_state *state);
static int adv7481_get_hdmi_timings(struct adv7481_state *state,
				struct adv7481_vid_params *vid_params,
				struct adv7481_hdmi_params *hdmi_params);
static int get_lane_cnt(struct resolution_config *configs,
			enum adv7481_resolution size, int w, int h);
static int get_settle_cnt(struct resolution_config *configs,
			enum adv7481_resolution size, int w, int h);

static inline struct v4l2_subdev *to_sd(struct v4l2_ctrl *ctrl)
{
	return &(container_of(ctrl->handler,
			struct adv7481_state, ctrl_hdl)->sd);
}

static inline struct adv7481_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct adv7481_state, sd);
}

/* I2C Rd/Rw Functions */
static int32_t adv7481_cci_i2c_write(struct msm_camera_i2c_client *i2c_client,
	uint8_t reg, uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	return i2c_client->i2c_func_tbl->i2c_write(i2c_client, reg,
				*data, data_type);
}

static int32_t adv7481_cci_i2c_write_seq(
	struct msm_camera_i2c_client *i2c_client,
	uint8_t reg, const uint8_t *data, uint32_t size)
{
	return i2c_client->i2c_func_tbl->i2c_write_seq(i2c_client, reg,
				(uint8_t *)data, size);
}

static int32_t adv7481_cci_i2c_read(struct msm_camera_i2c_client *i2c_client,
	uint8_t reg, uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	return i2c_client->i2c_func_tbl->i2c_read(i2c_client, reg,
				data, data_type);
}

static int32_t adv7481_cci_i2c_read_seq(
	struct msm_camera_i2c_client *i2c_client,
	uint8_t reg, uint8_t *data, uint32_t size)
{
	return i2c_client->i2c_func_tbl->i2c_read_seq(i2c_client, reg,
				data, size);
}

static int32_t adv7481_wr_byte(struct msm_camera_i2c_client *c_i2c_client,
	uint8_t sid, uint8_t reg, uint8_t data)
{
	uint16_t write_data = data;
	int ret = 0;

	c_i2c_client->cci_client->sid = sid;

	ret = adv7481_cci_i2c_write(c_i2c_client, reg, &write_data,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (ret < 0)
		pr_err("Error %d writing cci i2c\n", ret);

	return ret;
}

static int32_t adv7481_wr_block(struct msm_camera_i2c_client *c_i2c_client,
	uint8_t sid, uint8_t reg, const uint8_t *data, uint32_t size)
{
	int ret = 0;

	c_i2c_client->cci_client->sid = sid;

	ret = adv7481_cci_i2c_write_seq(c_i2c_client, reg, data, size);
	if (ret < 0)
		pr_err("Error %d writing cci i2c block data\n", ret);

	return ret;
}

static int32_t adv7481_rd_block(struct msm_camera_i2c_client *c_i2c_client,
	uint8_t sid, uint8_t reg, uint8_t *data, uint32_t size)
{
	int ret = 0;

	c_i2c_client->cci_client->sid = sid;

	ret = adv7481_cci_i2c_read_seq(c_i2c_client, reg, data, size);
	if (ret < 0)
		pr_err("Error %d reading cci i2c block data\n", ret);

	return ret;
}

static uint8_t adv7481_rd_byte(struct msm_camera_i2c_client *c_i2c_client,
	uint8_t sid, uint8_t reg)
{
	uint16_t data = 0;
	int ret = 0;

	c_i2c_client->cci_client->sid = sid;
	ret = adv7481_cci_i2c_read(c_i2c_client, reg, &data,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (ret < 0) {
		pr_err("Error %d reading cci i2c\n", ret);
		return ret;
	}

	return (uint8_t)(data & 0xFF);
}

static uint16_t adv7481_rd_word(struct msm_camera_i2c_client *c_i2c_client,
	uint8_t sid, uint8_t reg)
{
	uint16_t data = 0;
	int ret;

	c_i2c_client->cci_client->sid = sid;
	ret = adv7481_cci_i2c_read(c_i2c_client, reg, &data,
		MSM_CAMERA_I2C_WORD_DATA);
	if (ret < 0) {
		pr_err("Error %d reading cci i2c\n", ret);
		return ret;
	}

	return data;
}

static int adv7481_set_irq(struct adv7481_state *state)
{
	int ret = 0;

	ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_PAD_CTRL_1_ADDR,
			ADV_REG_SETFIELD(1, IO_PDN_INT2) |
			ADV_REG_SETFIELD(1, IO_PDN_INT3) |
			ADV_REG_SETFIELD(1, IO_INV_LLC) |
			ADV_REG_SETFIELD(AD_MID_DRIVE_STRNGTH, IO_DRV_LLC_PAD));
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_INT1_CONF_ADDR,
			ADV_REG_SETFIELD(AD_4_XTAL_PER,
				IO_INTRQ_DUR_SEL) |
			ADV_REG_SETFIELD(AD_OP_DRIVE_LOW, IO_INTRQ_OP_SEL));
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_INT2_CONF_ADDR,
			ADV_REG_SETFIELD(1, IO_CP_LOCK_UNLOCK_EDGE_SEL));
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_DATAPATH_INT_MASKB_ADDR,
			ADV_REG_SETFIELD(1, IO_CP_LOCK_CP_MB1) |
			ADV_REG_SETFIELD(1, IO_CP_UNLOCK_CP_MB1) |
			ADV_REG_SETFIELD(1, IO_VMUTE_REQUEST_HDMI_MB1) |
			ADV_REG_SETFIELD(1, IO_INT_SD_MB1));

	/* Set cable detect */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_HDMI_LVL_INT_MASKB_3_ADDR,
			ADV_REG_SETFIELD(1, IO_CABLE_DET_A_MB1) |
			ADV_REG_SETFIELD(1, IO_V_LOCKED_MB1) |
			ADV_REG_SETFIELD(1, IO_DE_REGEN_LCK_MB1));

	/* set CVBS lock/unlock interrupts */
	/* Select SDP MAP 1 */
	adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x20);
	adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_LOCK_UNLOCK_MASK_ADDR, 0x03);
	adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x00);

	if (ret)
		pr_err("%s: Failed %d to setup interrupt regs\n",
				__func__, ret);

	return ret;
}

static int adv7481_reset_irq(struct adv7481_state *state)
{
	int ret = 0;

	disable_irq(state->irq);

	ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_DATAPATH_INT_MASKB_ADDR, 0x00);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_HDMI_LVL_INT_MASKB_3_ADDR, 0x00);

	return ret;
}

static int adv7481_set_edid(struct adv7481_state *state)
{
	int i;
	int ret = 0;
	uint8_t edid_state;
	uint32_t data_left = 0;
	uint32_t start_pos;

	/* Enable Manual Control of EDID on Port A */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_rep_addr, 0x74,
				0x01);
	/* Disable Auto Enable of EDID */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_rep_addr, 0x7A,
				0x08);
	/* Set Primary EDID Size to 256 Bytes */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_rep_addr, 0x70,
				0x20);

	/*
	 * Readback EDID enable state after a combination of manual
	 * and automatic functions
	 */
	edid_state = adv7481_rd_byte(&state->i2c_client, state->i2c_rep_addr,
					HDMI_REG_RO_EDID_DEBUG_2_ADDR);
	pr_debug("%s: Readback EDID enable state: 0x%x\n", __func__,
			edid_state);

	for (i = 0; i < ADV7481_EDID_SIZE && !ret; i += I2C_BLOCK_WRITE_SIZE)
		ret |= adv7481_wr_block(&state->i2c_client,
			state->i2c_edid_addr,
			i, &adv7481_default_edid_data[i],
			I2C_BLOCK_WRITE_SIZE);

	data_left = ADV7481_EDID_SIZE % I2C_BLOCK_WRITE_SIZE;
	start_pos = ADV7481_EDID_SIZE - data_left;
	if (data_left && !ret)
		ret |= adv7481_wr_block(&state->i2c_client,
			state->i2c_edid_addr,
			start_pos,
			&adv7481_default_edid_data[start_pos],
			data_left);

	return ret;
}

static irqreturn_t adv7481_irq(int irq, void *dev)
{
	struct adv7481_state *state = dev;

	schedule_delayed_work(&state->irq_delayed_work,
				msecs_to_jiffies(ADV_REG_STABLE_DELAY));
	return IRQ_HANDLED;
}

/* Request CCI clocks for adv7481 register access */
static int adv7481_request_cci_clks(struct adv7481_state *state)
{
	int ret = 0;

	if (state->clocks_requested == TRUE)
		return ret;

	ret = state->i2c_client.i2c_func_tbl->i2c_util(
				&state->i2c_client, MSM_CCI_INIT);
	if (ret < 0)
		pr_err("%s - cci_init failed\n", __func__);
	else
		state->clocks_requested = TRUE;

	/* enable camera voltage regulator */
	ret = msm_camera_enable_vreg(state->dev, state->cci_vreg,
			state->regulator_count, NULL, 0,
			&state->cci_reg_ptr[0], 1);
	if (ret < 0)
		pr_err("%s:cci enable_vreg failed\n", __func__);
	else
		pr_debug("%s - VREG Initialized...\n", __func__);

	return ret;
}

static int adv7481_release_cci_clks(struct adv7481_state *state)
{
	int ret = 0;

	if (state->clocks_requested == FALSE)
		return ret;

	ret = state->i2c_client.i2c_func_tbl->i2c_util(
				&state->i2c_client, MSM_CCI_RELEASE);
	if (ret < 0)
		pr_err("%s - cci_release failed\n", __func__);
	else
		state->clocks_requested = FALSE;

	/* disable camera voltage regulator */
	ret = msm_camera_enable_vreg(state->dev, state->cci_vreg,
			state->regulator_count, NULL, 0,
			&state->cci_reg_ptr[0], 0);
	if (ret < 0)
		pr_err("%s:cci disable vreg failed\n", __func__);
	else
		pr_debug("%s - VREG Initialized...\n", __func__);

	return ret;
}

static void adv7481_irq_delay_work(struct work_struct *work)
{
	struct adv7481_state *state;
	uint8_t int_raw_status;
	uint8_t int_status;
	uint8_t raw_status;

	state = container_of(work, struct adv7481_state,
				irq_delayed_work.work);

	mutex_lock(&state->mutex);

	/* Read raw irq status register */
	int_raw_status = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_INT_RAW_STATUS_ADDR);

	pr_debug("%s: dev: %d got int raw status: 0x%x\n", __func__,
			state->device_num, int_raw_status);
	state->cec_detected = ADV_REG_GETFIELD(int_raw_status, IO_INT_CEC_ST);

	while (int_raw_status) {
		if (ADV_REG_GETFIELD(int_raw_status, IO_INTRQ1_RAW)) {
			int lock_status = -1;
			struct v4l2_event event = {0};
			int *ptr = (int *)event.u.data;

			pr_debug("%s: dev: %d got intrq1_raw\n", __func__,
					state->device_num);
			int_status = adv7481_rd_byte(&state->i2c_client,
					state->i2c_io_addr,
					IO_REG_DATAPATH_INT_STATUS_ADDR);

			raw_status = adv7481_rd_byte(&state->i2c_client,
					state->i2c_io_addr,
					IO_REG_DATAPATH_RAW_STATUS_ADDR);

			adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
				IO_REG_DATAPATH_INT_CLEAR_ADDR, int_status);

			pr_debug("%s: dev: %d got datapath int status: 0x%x\n",
				__func__, state->device_num, int_status);

			pr_debug("%s: dev: %d got datapath raw status: 0x%x\n",
				__func__, state->device_num, raw_status);

			if ((state->mode == ADV7481_IP_CVBS_1) &&
				ADV_REG_GETFIELD(int_status, IO_INT_SD_ST) &&
				ADV_REG_GETFIELD(raw_status, IO_INT_SD_RAW)) {
				uint8_t sdp_sts = 0;

				adv7481_wr_byte(&state->i2c_client,
					state->i2c_sdp_addr, SDP_RW_MAP_REG,
					0x01);
				sdp_sts = adv7481_rd_byte(&state->i2c_client,
					state->i2c_sdp_addr,
					SDP_RO_MAIN_STATUS1_ADDR);
				pr_debug("%s: dev: %d got sdp status: 0x%x\n",
					__func__, state->device_num, sdp_sts);
				adv7481_wr_byte(&state->i2c_client,
					state->i2c_sdp_addr, SDP_RW_MAP_REG,
					0x00);
				if (ADV_REG_GETFIELD(sdp_sts,
					SDP_RO_MAIN_IN_LOCK)) {
					lock_status = 0;
					pr_debug(
					"%s: set lock_status SDP_IN_LOCK:0x%x\n",
					__func__, lock_status);
				} else {
					lock_status = 1;
					pr_debug(
					"%s: set lock_status SDP_UNLOCK:0x%x\n",
					__func__, lock_status);
				}
				adv7481_wr_byte(&state->i2c_client,
					state->i2c_sdp_addr, SDP_RW_MAP_REG,
					0x20);
				adv7481_wr_byte(&state->i2c_client,
					state->i2c_sdp_addr,
					SDP_RW_LOCK_UNLOCK_CLR_ADDR, sdp_sts);
				adv7481_wr_byte(&state->i2c_client,
					state->i2c_sdp_addr, SDP_RW_MAP_REG,
					0x00);
			} else if (state->mode == ADV7481_IP_HDMI) {
				if (ADV_REG_GETFIELD(int_status,
						IO_CP_LOCK_CP_ST) &&
					ADV_REG_GETFIELD(raw_status,
						IO_CP_LOCK_CP_RAW)) {
					lock_status = 0;
					pr_debug(
					"%s: set lock_status IO_CP_LOCK_CP_RAW:0x%x\n",
					__func__, lock_status);
				}
				if (ADV_REG_GETFIELD(int_status,
						IO_CP_UNLOCK_CP_ST) &&
					ADV_REG_GETFIELD(raw_status,
						IO_CP_UNLOCK_CP_RAW)) {
					lock_status = 1;
					pr_debug(
					"%s: set lock_status IO_CP_UNLOCK_CP_RAW:0x%x\n",
					__func__, lock_status);
				}
			}

			if (lock_status >= 0) {
				ptr[0] = adv7481_inp_to_ba(state->mode);
				ptr[1] = lock_status;
				event.type = lock_status ?
					V4L2_EVENT_MSM_BA_SIGNAL_LOST_LOCK :
					V4L2_EVENT_MSM_BA_SIGNAL_IN_LOCK;
				v4l2_subdev_notify(&state->sd,
					event.type, &event);
			}
		}

		if (ADV_REG_GETFIELD(int_raw_status, IO_INT_HDMI_ST)) {
			int cable_detected = 0;
			struct v4l2_event event = {0};
			int *ptr = (int *)event.u.data;

			ptr[0] = adv7481_inp_to_ba(state->mode);

			pr_debug("%s: dev: %d got int_hdmi_st\n", __func__,
				state->device_num);

			int_status = adv7481_rd_byte(&state->i2c_client,
				state->i2c_io_addr,
				IO_HDMI_LVL_INT_STATUS_3_ADDR);

			raw_status = adv7481_rd_byte(&state->i2c_client,
				state->i2c_io_addr,
				IO_HDMI_LVL_RAW_STATUS_3_ADDR);

			adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
				IO_HDMI_LVL_INT_CLEAR_3_ADDR, int_status);

			pr_debug("%s: dev: %d got hdmi lvl int status 3: 0x%x\n",
				__func__, state->device_num, int_status);
			pr_debug("%s: dev: %d got hdmi lvl raw status 3: 0x%x\n",
				__func__, state->device_num, raw_status);


			if (ADV_REG_GETFIELD(int_status, IO_CABLE_DET_A_ST)) {
				cable_detected = ADV_REG_GETFIELD(raw_status,
					IO_CABLE_DET_A_RAW);
				pr_debug("%s: set cable_detected: 0x%x\n",
					__func__, cable_detected);
				ptr[1] = cable_detected;
				event.type = V4L2_EVENT_MSM_BA_CABLE_DETECT;
				v4l2_subdev_notify(&state->sd,
					event.type, &event);
			}
			/* Assumption is that vertical sync int
			 * is the last one to come
			 */
			if (ADV_REG_GETFIELD(int_status, IO_V_LOCKED_ST)) {
				if (ADV_REG_GETFIELD(raw_status,
					IO_TMDSPLL_LCK_A_RAW) &&
					ADV_REG_GETFIELD(raw_status,
					IO_V_LOCKED_RAW) &&
					ADV_REG_GETFIELD(raw_status,
					IO_DE_REGEN_LCK_RAW)) {
					pr_debug("%s: port settings changed\n",
						__func__);
					event.type =
					V4L2_EVENT_MSM_BA_PORT_SETTINGS_CHANGED;
					v4l2_subdev_notify(&state->sd,
						event.type, &event);
				}
			}
		}
		int_raw_status = adv7481_rd_byte(&state->i2c_client,
				state->i2c_io_addr,
				IO_REG_INT_RAW_STATUS_ADDR);
	}
	mutex_unlock(&state->mutex);
}

static int adv7481_cec_wakeup(struct adv7481_state *state, bool enable)
{
	uint8_t val;
	int ret = 0;

	val = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_PWR_DN2_XTAL_HIGH_ADDR);
	val = ADV_REG_GETFIELD(val, IO_PROG_XTAL_FREQ_HIGH);
	if (enable) {
		/* CEC wake up enabled in power-down mode */
		val |= ADV_REG_SETFIELD(1, IO_CTRL_CEC_WAKE_UP_PWRDN2B) |
			ADV_REG_SETFIELD(0, IO_CTRL_CEC_WAKE_UP_PWRDNB);
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					IO_REG_PWR_DN2_XTAL_HIGH_ADDR, val);
	} else {
		/* CEC wake up disabled in power-down mode */
		val |= ADV_REG_SETFIELD(0, IO_CTRL_CEC_WAKE_UP_PWRDN2B) |
			ADV_REG_SETFIELD(1, IO_CTRL_CEC_WAKE_UP_PWRDNB);
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					IO_REG_PWR_DN2_XTAL_HIGH_ADDR, val);
	}
	return ret;
}

/* Initialize adv7481 I2C Settings */
static int adv7481_dev_init(struct adv7481_state *state)
{
	uint16_t chip_rev_id;
	int ret;

	mutex_lock(&state->mutex);

	chip_rev_id = adv7481_rd_word(&state->i2c_client, state->i2c_io_addr,
			IO_REG_CHIP_REV_ID_1_ADDR);
	pr_debug("%s: ADV7481 chip rev id: 0x%x", __func__, chip_rev_id);

	/* Disable CEC wake up in power-down mode */
	ret = adv7481_cec_wakeup(state, 0);
	/* Setting Vid_Std to 720x480p60 */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_CP_VID_STD_ADDR, 0x4A);

	/* Configure I2C Maps and I2C Communication Settings */
	/* io_reg_f2 I2C Auto Increment */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_I2C_CFG_ADDR, IO_REG_I2C_AUTOINC_EN_REG_VALUE);
	/* DPLL Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_DPLL_ADDR, IO_REG_DPLL_SADDR);
	/* CP Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_CP_ADDR, IO_REG_CP_SADDR);
	/* HDMI RX Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_HDMI_ADDR, IO_REG_HDMI_SADDR);
	/* EDID Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_EDID_ADDR, IO_REG_EDID_SADDR);
	/* HDMI RX Repeater Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_HDMI_REP_ADDR, IO_REG_HDMI_REP_SADDR);
	/* HDMI RX Info-frame Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_HDMI_INF_ADDR, IO_REG_HDMI_INF_SADDR);
	/* CBUS Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_CBUS_ADDR, IO_REG_CBUS_SADDR);
	/* CEC Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_CEC_ADDR, IO_REG_CEC_SADDR);
	/* SDP Main Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_SDP_ADDR, IO_REG_SDP_SADDR);
	/* CSI-TXB Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_CSI_TXB_ADDR, IO_REG_CSI_TXB_SADDR);
	/* CSI-TXA Map Address */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_CSI_TXA_ADDR, IO_REG_CSI_TXA_SADDR);
	if (ret) {
		pr_err("%s: Failed dev init %d\n", __func__, ret);
		goto  err_exit;
	}

	/* Configure i2c clients */
	state->i2c_csi_txa_addr = IO_REG_CSI_TXA_SADDR >> 1;
	state->i2c_csi_txb_addr = IO_REG_CSI_TXB_SADDR >> 1;
	state->i2c_cp_addr = IO_REG_CP_SADDR >> 1;
	state->i2c_hdmi_addr = IO_REG_HDMI_SADDR >> 1;
	state->i2c_hdmi_inf_addr = IO_REG_HDMI_INF_SADDR >> 1;
	state->i2c_edid_addr = IO_REG_EDID_SADDR >> 1;
	state->i2c_sdp_addr = IO_REG_SDP_SADDR >> 1;
	state->i2c_rep_addr = IO_REG_HDMI_REP_SADDR >> 1;
	state->i2c_cbus_addr = IO_REG_CBUS_SADDR >> 1;

	ret = adv7481_set_edid(state);
	ret |= adv7481_set_irq(state);

err_exit:
	mutex_unlock(&state->mutex);

	return ret;
}

/* Initialize adv7481 hardware */
static int adv7481_hw_init(struct adv7481_state *state)
{
	int ret = 0;

	mutex_lock(&state->mutex);

	/* Bring ADV7481 out of reset */
	ret = gpio_request_array(&state->gpio_array[ADV7481_GPIO_RST], 1);
	if (ret < 0) {
		pr_err("%s: Failed to request reset GPIO %d\n", __func__, ret);
		goto err_exit;
	}
	if (gpio_is_valid(state->gpio_array[ADV7481_GPIO_RST].gpio)) {
		ret |= gpio_direction_output(
			state->gpio_array[ADV7481_GPIO_RST].gpio, 0);
		udelay(GPIO_HW_RST_DELAY_LOW);
		ret |= gpio_direction_output(
			state->gpio_array[ADV7481_GPIO_RST].gpio, 1);
		udelay(GPIO_HW_RST_DELAY_HI);
		if (ret) {
			pr_err("%s: Set GPIO Fail %d\n", __func__, ret);
			goto err_exit;
		}
	}

	/* Only setup IRQ1 for now... */
	ret = gpio_request_array(&state->gpio_array[ADV7481_GPIO_INT1], 1);
	if (ret < 0) {
		pr_err("%s: Failed to request irq_gpio %d\n", __func__, ret);
		goto err_exit;
	}
	if (gpio_is_valid(state->gpio_array[ADV7481_GPIO_INT1].gpio)) {
		ret |= gpio_direction_input(
			state->gpio_array[ADV7481_GPIO_INT1].gpio);
		if (ret) {
			pr_err("%s: Failed gpio_direction irq %d\n",
					__func__, ret);
			goto err_exit;
		}
		state->irq = gpio_to_irq(
			state->gpio_array[ADV7481_GPIO_INT1].gpio);
		if (state->irq) {
			ret = request_irq(state->irq, adv7481_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
					DRIVER_NAME, state);
			if (ret) {
				pr_err("%s: Failed request_irq %d\n",
						__func__, ret);
				goto err_exit;
			}
			/* disable irq until chip interrupts are programmed */
			disable_irq(state->irq);
		} else {
			pr_err("%s: Failed gpio_to_irq %d\n", __func__, ret);
			ret = -EINVAL;
			goto err_exit;
		}
	}
	INIT_DELAYED_WORK(&state->irq_delayed_work,
			adv7481_irq_delay_work);

err_exit:
	mutex_unlock(&state->mutex);

	return ret;
}

static int adv7481_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct adv7481_state *state = to_state(sd);
	int temp = 0x0;
	int ret = 0;

	pr_debug("Enter %s: id = 0x%x\n", __func__, ctrl->id);
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		temp = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_BRIGHTNESS, ctrl->val);
		break;
	case V4L2_CID_CONTRAST:
		temp = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_CONTRAST, ctrl->val);
		break;
	case V4L2_CID_SATURATION:
		temp = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_SATURATION, ctrl->val);
		break;
	case V4L2_CID_HUE:
		temp = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
					CP_REG_HUE, ctrl->val);
		break;
	default:
		break;
	}
	return ret;
}

static int adv7481_powerup(struct adv7481_state *state, bool powerup)
{
	int ret = 0;

	if (powerup) {
		pr_debug("%s: powered up\n", __func__);
	} else {
		pr_debug("%s: powered off\n", __func__);
		ret = adv7481_cec_wakeup(state, !powerup);
	}
	return ret;
}

static int adv7481_s_power(struct v4l2_subdev *sd, int on)
{
	struct adv7481_state *state = to_state(sd);
	int ret;

	pr_debug("Enter %s\n", __func__);
	ret = mutex_lock_interruptible(&state->mutex);
	if (ret)
		return -EBUSY;

	ret = adv7481_powerup(state, on);
	if (ret == 0)
		state->powerup = on;

	mutex_unlock(&state->mutex);
	return ret;
}

static int adv7481_set_cec_logical_addr(struct adv7481_state *state, int *la)
{
	int rc = 0;
	uint8_t val;

	if (!la) {
		pr_err("%s: NULL pointer provided\n", __func__);
		return -EINVAL;
	}

	val = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
			CEC_REG_LOG_ADDR_MASK_ADDR);
	if (ADV_REG_GETFIELD(val, CEC_REG_LOG_ADDR_MASK0)) {
		val = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
			CEC_REG_LOGICAL_ADDRESS0_1_ADDR);
		val = ADV_REG_RSTFIELD(val, CEC_REG_LOGICAL_ADDRESS0);
		val |= ADV_REG_SETFIELD(*la, CEC_REG_LOGICAL_ADDRESS0);
		rc = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			CEC_REG_LOGICAL_ADDRESS0_1_ADDR, val);
	} else if (ADV_REG_GETFIELD(val, CEC_REG_LOG_ADDR_MASK1)) {
		val = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
			CEC_REG_LOGICAL_ADDRESS0_1_ADDR);
		val = ADV_REG_RSTFIELD(val, CEC_REG_LOGICAL_ADDRESS1);
		val |= ADV_REG_SETFIELD(*la, CEC_REG_LOGICAL_ADDRESS1);
		rc = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			CEC_REG_LOGICAL_ADDRESS0_1_ADDR, val);
	} else if (ADV_REG_GETFIELD(val, CEC_REG_LOG_ADDR_MASK2)) {
		val = ADV_REG_SETFIELD(*la, CEC_REG_LOGICAL_ADDRESS2);
		rc = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			CEC_REG_LOGICAL_ADDRESS2_ADDR, val);
	} else {
		pr_err("No cec logical address mask set\n");
	}

	return rc;
}

static int adv7481_cec_powerup(struct adv7481_state *state, int *powerup)
{
	int rc = 0;
	uint8_t val = 0;

	if (!powerup) {
		pr_err("%s: NULL pointer provided\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: set power %d\n", __func__, *powerup);

	val = ADV_REG_SETFIELD(*powerup, CEC_REG_CEC_POWER_UP);
	rc = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		CEC_REG_CEC_POWER_UP_ADDR, val);

	return rc;
}

static long adv7481_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct adv7481_state *state = to_state(sd);
	int *ret_val = arg;
	struct msm_ba_v4l2_ioctl_t adv_arg = *(struct msm_ba_v4l2_ioctl_t *)arg;
	long ret = 0;
	int param = 0;
	uint8_t status = 0;
	struct timespec ts;
	struct csi_ctrl_params user_csi;
	struct field_info_params user_field;
	struct adv7481_vid_params vid_params;
	struct adv7481_hdmi_params hdmi_params;

	struct device *dev = state->dev;
	union hdmi_infoframe hdmi_info_frame;
	uint8_t inf_buffer[AVI_INFOFRAME_SIZE];

	pr_debug("Enter %s with command: 0x%x", __func__, cmd);

	memset(&vid_params, 0, sizeof(struct adv7481_vid_params));
	memset(&hdmi_params, 0, sizeof(struct adv7481_hdmi_params));
	memset(&hdmi_info_frame, 0, sizeof(union hdmi_infoframe));
	memset(inf_buffer, 0, AVI_INFOFRAME_SIZE);

	if (!sd)
		return -EINVAL;

	switch (cmd) {
	case VIDIOC_HDMI_RX_CEC_S_LOGICAL:
		ret = adv7481_set_cec_logical_addr(state, arg);
		break;
	case VIDIOC_HDMI_RX_CEC_CLEAR_LOGICAL:
		ret = adv7481_set_cec_logical_addr(state, &param);
		break;
	case VIDIOC_HDMI_RX_CEC_G_PHYSICAL:
		if (ret_val) {
			*ret_val = 0;
		} else {
			pr_err("%s: NULL pointer provided\n", __func__);
			ret = -EINVAL;
		}
		break;
	case VIDIOC_HDMI_RX_CEC_G_CONNECTED:
		if (ret_val) {
			*ret_val = state->cec_detected;
		} else {
			pr_err("%s: NULL pointer provided\n", __func__);
			ret = -EINVAL;
		}
		break;
	case VIDIOC_HDMI_RX_CEC_S_ENABLE:
		ret = adv7481_cec_powerup(state, arg);
		break;
	case VIDIOC_G_CSI_PARAMS: {
		if (state->csia_src == ADV7481_IP_HDMI) {
			ret = adv7481_get_hdmi_timings(state,
					&vid_params, &hdmi_params);
			if (ret) {
				pr_err("%s:Error in adv7481_get_hdmi_timings\n",
						__func__);
				return -EINVAL;
			}
		}
		user_csi.settle_count = get_settle_cnt(state->res_configs,
			RES_MAX, vid_params.act_pix, vid_params.act_lines);
		user_csi.lane_count = get_lane_cnt(state->res_configs,
			RES_MAX, vid_params.act_pix, vid_params.act_lines);

		if (copy_to_user((void __user *)adv_arg.ptr,
			(void *)&user_csi, sizeof(struct csi_ctrl_params))) {
			pr_err("%s: Failed to copy CSI params\n", __func__);
			return -EINVAL;
		}
		break;
	}
	case VIDIOC_G_AVI_INFOFRAME: {
		int int_raw = adv7481_rd_byte(&state->i2c_client,
			state->i2c_io_addr,
			IO_HDMI_LVL_RAW_STATUS_1_ADDR);
		adv7481_wr_byte(&state->i2c_client,
			state->i2c_io_addr,
			IO_HDMI_LVL_INT_CLEAR_1_ADDR, int_raw);
		pr_debug("%s: VIDIOC_G_AVI_INFOFRAME\n", __func__);
		if (ADV_REG_GETFIELD(int_raw, IO_AVI_INFO_RAW)) {
			inf_buffer[0] = adv7481_rd_byte(&state->i2c_client,
				state->i2c_hdmi_inf_addr,
				HDMI_REG_AVI_PACKET_ID_ADDR);
			inf_buffer[1] = adv7481_rd_byte(&state->i2c_client,
				state->i2c_hdmi_inf_addr,
				HDMI_REG_AVI_INF_VERS_ADDR);
			inf_buffer[2] = adv7481_rd_byte(&state->i2c_client,
				state->i2c_hdmi_inf_addr,
				HDMI_REG_AVI_INF_LEN_ADDR);
			ret = adv7481_rd_block(&state->i2c_client,
				state->i2c_hdmi_inf_addr,
				HDMI_REG_AVI_INF_PB_ADDR,
				&inf_buffer[3],
				INFOFRAME_DATA_SIZE);
			if (ret) {
				pr_err("%s: Error in reading AVI Infoframe\n",
						__func__);
				return -EINVAL;
			}
			if (hdmi_infoframe_unpack(&hdmi_info_frame,
					(void *)inf_buffer) < 0) {
				pr_err("%s: Infoframe unpack fail\n", __func__);
				return -EINVAL;
			}
			hdmi_infoframe_log(KERN_ERR, dev, &hdmi_info_frame);
			state->hdmi_avi_infoframe.picture_aspect =
				(enum picture_aspect_ratio)
					hdmi_info_frame.avi.picture_aspect;
			state->hdmi_avi_infoframe.active_aspect =
				(enum active_format_aspect_ratio)
					hdmi_info_frame.avi.active_aspect;
			state->hdmi_avi_infoframe.video_code =
				hdmi_info_frame.avi.video_code;
		} else {
			pr_err("%s: No AVI Infoframe\n", __func__);
			return -EINVAL;
		}
		if (copy_to_user((void __user *)adv_arg.ptr,
				(void *)&state->hdmi_avi_infoframe,
				sizeof(struct avi_infoframe_params))) {
			pr_err("%s: Failed to copy AVI Infoframe\n", __func__);
			return -EINVAL;
		}
		break;
	}
	case VIDIOC_G_FIELD_INFO:
		/* Select SDP read-only Map 1 */
		adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x02);
		status = adv7481_rd_byte(&state->i2c_client,
				state->i2c_sdp_addr, SDP_RO_MAP_1_FIELD_ADDR);
		adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x00);

		user_field.even_field = ADV_REG_GETFIELD(status,
				SDP_RO_MAP_1_EVEN_FIELD);
		get_monotonic_boottime(&ts);
		user_field.field_ts.tv_sec = ts.tv_sec;
		user_field.field_ts.tv_usec = ts.tv_nsec/1000;

		if (copy_to_user((void __user *)adv_arg.ptr,
			(void *)&user_field,
			sizeof(struct field_info_params))) {
			pr_err("%s: Failed to copy FIELD params\n", __func__);
			return -EINVAL;
		}
		break;
	default:
		pr_err("Not a typewriter! Command: 0x%x", cmd);
		ret = -ENOTTY;
		break;
	}
	return ret;
}

static int adv7481_get_sd_timings(struct adv7481_state *state,
		int *sd_standard, struct adv7481_vid_params *vid_params)
{
	int ret = 0;
	int sdp_stat, sdp_stat2;
	int interlace_reg = 0;
	int timeout = 0;

	if (sd_standard == NULL)
		return -EINVAL;

	/* Select SDP read-only main Map */
	adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x01);
	do {
		sdp_stat = adv7481_rd_byte(&state->i2c_client,
				state->i2c_sdp_addr, SDP_RO_MAIN_STATUS1_ADDR);
		usleep_range(SDP_MIN_SLEEP, SDP_MAX_SLEEP);
		timeout++;
		sdp_stat2 = adv7481_rd_byte(&state->i2c_client,
				state->i2c_sdp_addr, SDP_RO_MAIN_STATUS1_ADDR);
	} while ((sdp_stat != sdp_stat2) && (timeout < SDP_NUM_TRIES));

	interlace_reg = adv7481_rd_byte(&state->i2c_client,
			state->i2c_sdp_addr, SDP_RO_MAIN_INTERLACE_STATE_ADDR);

	if (ADV_REG_GETFIELD(interlace_reg, SDP_RO_MAIN_INTERLACE_STATE))
		pr_debug("%s: Interlaced video detected\n", __func__);
	else
		pr_debug("%s: Interlaced video not detected\n", __func__);

	if (ADV_REG_GETFIELD(interlace_reg, SDP_RO_MAIN_FIELD_LEN))
		pr_debug("%s: Field length is correct\n", __func__);
	else
		pr_debug("%s: Field length is not correct\n", __func__);

	if (ADV_REG_GETFIELD(interlace_reg, SDP_RO_MAIN_SD_FIELD_RATE))
		pr_debug("%s: SD 50 Hz detected\n", __func__);
	else
		pr_debug("%s: SD 60 Hz detected\n", __func__);

	adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x00);

	if (sdp_stat != sdp_stat2) {
		pr_err("%s, adv7481 SDP status unstable: 1\n", __func__);
		return -ETIMEDOUT;
	}

	if (!ADV_REG_GETFIELD(sdp_stat, SDP_RO_MAIN_IN_LOCK)) {
		pr_err("%s(%d), adv7481 SD Input NOT Locked: 0x%x\n",
				__func__, __LINE__, sdp_stat);
		return -EBUSY;
	}
	vid_params->act_pix = 720;
	vid_params->intrlcd = 1;

	switch (ADV_REG_GETFIELD(sdp_stat, SDP_RO_MAIN_AD_RESULT)) {
	case AD_NTSM_M_J:
		*sd_standard = V4L2_STD_NTSC;
		pr_debug("%s, V4L2_STD_NTSC\n", __func__);
		vid_params->act_lines = 507;
		break;
	case AD_NTSC_4_43:
		*sd_standard = V4L2_STD_NTSC_443;
		pr_debug("%s, V4L2_STD_NTSC_443\n", __func__);
		vid_params->act_lines = 507;
		break;
	case AD_PAL_M:
		*sd_standard = V4L2_STD_PAL_M;
		pr_debug("%s, V4L2_STD_PAL_M\n", __func__);
		vid_params->act_lines = 576;
		break;
	case AD_PAL_60:
		*sd_standard = V4L2_STD_PAL_60;
		pr_debug("%s, V4L2_STD_PAL_60\n", __func__);
		vid_params->act_lines = 576;
		break;
	case AD_PAL_B_G:
		*sd_standard = V4L2_STD_PAL;
		pr_debug("%s, V4L2_STD_PAL\n", __func__);
		vid_params->act_lines = 576;
		break;
	case AD_SECAM:
		*sd_standard = V4L2_STD_SECAM;
		pr_debug("%s, V4L2_STD_SECAM\n", __func__);
		vid_params->act_lines = 576;
		break;
	case AD_PAL_COMB_N:
		*sd_standard = V4L2_STD_PAL_Nc | V4L2_STD_PAL_N;
		pr_debug("%s, V4L2_STD_PAL_Nc | V4L2_STD_PAL_N\n", __func__);
		vid_params->act_lines = 576;
		break;
	case AD_SECAM_525:
		*sd_standard = V4L2_STD_SECAM;
		pr_debug("%s, V4L2_STD_SECAM (AD_SECAM_525)\n", __func__);
		vid_params->act_lines = 576;
		break;
	default:
		*sd_standard = V4L2_STD_UNKNOWN;
		pr_debug("%s, V4L2_STD_UNKNOWN\n", __func__);
		vid_params->act_lines = 507;
		break;
	}
	pr_debug("%s(%d), adv7481 TMDS Resolution: %d x %d\n",
		__func__, __LINE__, vid_params->act_pix, vid_params->act_lines);
	return ret;
}

static int adv7481_set_cvbs_mode(struct adv7481_state *state)
{
	int ret;
	uint8_t val;

	pr_debug("Enter %s\n", __func__);
	state->mode = ADV7481_IP_CVBS_1;
	/* cvbs video settings ntsc etc */
	ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x00, 0x30);
	ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x0e, 0xff);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x0f, 0x00);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x52, 0xcd);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x00, 0x00);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		SDP_RW_MAP_REG, 0x80);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x9c, 0x00);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x9c, 0xff);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		SDP_RW_MAP_REG, 0x00);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x80, 0x51);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x81, 0x51);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x82, 0x68);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x03, 0x42);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x04, 0x07);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x13, 0x00);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x17, 0x41);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
		0x31, 0x12);

	val = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
				IO_REG_CSI_PIX_EN_SEL_ADDR);
	/* Output of SD core routed to MIPI CSI 4-lane Tx */
	val = ADV_REG_SETFIELD(1, IO_CTRL_CSI4_EN) |
		ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
		ADV_REG_SETFIELD(0x2, IO_CTRL_CSI4_IN_SEL);

	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
				IO_REG_CSI_PIX_EN_SEL_ADDR, val);

	return ret;
}

static int adv7481_set_hdmi_mode(struct adv7481_state *state)
{
	int ret;
	int temp;
	uint8_t val;

	pr_debug("Enter %s\n", __func__);
	state->mode = ADV7481_IP_HDMI;
	/* Configure IO setting for HDMI in and
	 * YUV 422 out via TxA CSI: 4-Lane
	 */
	/* Disable chip powerdown & Enable HDMI Rx block */
	temp = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
				IO_REG_PWR_DOWN_CTRL_ADDR);
	val = ADV_REG_SETFIELD(1, IO_CTRL_RX_EN) |
				ADV_REG_SETFIELD(0, IO_CTRL_RX_PWDN) |
				ADV_REG_SETFIELD(0, IO_CTRL_XTAL_PWDN) |
				ADV_REG_SETFIELD(0, IO_CTRL_CORE_PWDN) |
				ADV_REG_SETFIELD(0, IO_CTRL_MASTER_PWDN);
	ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
				IO_REG_PWR_DOWN_CTRL_ADDR, val);
	/* SDR mode */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x11, 0x48);
	/* Set CP core to YUV out */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x04, 0x00);
	/* Set CP core to SDR 422 */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x12, 0xF2);
	/* Saturate both Luma and Chroma values to 254 */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x17, 0x80);
	/* Set CP core to enable AV codes */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x03, 0x86);
	/* ADI RS CP Core: */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_cp_addr,
		0x7C, 0x00);
	/* Set CP core Phase Adjustment */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		0x0C, 0xE0);
	/* LLC/PIX/SPI PINS TRISTATED AUD Outputs Enabled */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_PAD_CTRLS_ADDR, 0xDD);
	/* Enable Tx A CSI 4-Lane & data from CP core */
	val = ADV_REG_SETFIELD(1, IO_CTRL_CSI4_EN) |
		ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
		ADV_REG_SETFIELD(0, IO_CTRL_CSI4_IN_SEL);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_CSI_PIX_EN_SEL_ADDR, val);

	/* start to configure HDMI Rx once io-map is configured */
	/* Enable HDCP 1.1 */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_rep_addr,
		0x40, 0x83);
	/* Foreground Channel = A */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x00, 0x08);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x98, 0xFF);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x99, 0xA3);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x9A, 0x00);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x9B, 0x0A);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x9D, 0x40);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0xCB, 0x09);
	/* ADI RS */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x3D, 0x10);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x3E, 0x7B);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x3F, 0x5E);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x4E, 0xFE);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x4F, 0x18);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x57, 0xA3);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x58, 0x04);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x85, 0x10);
	/* Enable All Terminations */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x83, 0x00);
	/* ADI RS */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0xA3, 0x01);
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0xBE, 0x00);
	/* HPA Manual Enable */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x6C, 0x01);
	/* HPA Asserted */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0xF8, 0x01);

	/* Audio Mute Speed Set to Fastest (Smallest Step Size) */
	ret |= adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
		0x0F, 0x00);

	return ret;
}

static int adv7481_set_analog_mux(struct adv7481_state *state, int input)
{
	int ain_sel = 0x0;

	switch (input) {
	case ADV7481_IP_CVBS_1:
	case ADV7481_IP_CVBS_1_HDMI_SIM:
		ain_sel = 0x0;
		break;
	case ADV7481_IP_CVBS_2:
	case ADV7481_IP_CVBS_2_HDMI_SIM:
		ain_sel = 0x1;
		break;
	case ADV7481_IP_CVBS_3:
	case ADV7481_IP_CVBS_3_HDMI_SIM:
		ain_sel = 0x2;
		break;
	case ADV7481_IP_CVBS_4:
	case ADV7481_IP_CVBS_4_HDMI_SIM:
		ain_sel = 0x3;
		break;
	case ADV7481_IP_CVBS_5:
	case ADV7481_IP_CVBS_5_HDMI_SIM:
		ain_sel = 0x4;
		break;
	case ADV7481_IP_CVBS_6:
	case ADV7481_IP_CVBS_6_HDMI_SIM:
		ain_sel = 0x5;
		break;
	case ADV7481_IP_CVBS_7:
	case ADV7481_IP_CVBS_7_HDMI_SIM:
		ain_sel = 0x6;
		break;
	case ADV7481_IP_CVBS_8:
	case ADV7481_IP_CVBS_8_HDMI_SIM:
		ain_sel = 0x7;
		break;
	}
	return 0;
}

static int adv7481_set_ip_mode(struct adv7481_state *state, int input)
{
	int ret = 0;

	pr_debug("Enter %s: input: %d\n", __func__, input);
	switch (input) {
	case ADV7481_IP_HDMI:
		ret = adv7481_set_hdmi_mode(state);
		break;
	case ADV7481_IP_CVBS_1:
	case ADV7481_IP_CVBS_2:
	case ADV7481_IP_CVBS_3:
	case ADV7481_IP_CVBS_4:
	case ADV7481_IP_CVBS_5:
	case ADV7481_IP_CVBS_6:
	case ADV7481_IP_CVBS_7:
	case ADV7481_IP_CVBS_8:
		ret = adv7481_set_cvbs_mode(state);
		ret |= adv7481_set_analog_mux(state, input);
		break;
	case ADV7481_IP_CVBS_1_HDMI_SIM:
	case ADV7481_IP_CVBS_2_HDMI_SIM:
	case ADV7481_IP_CVBS_3_HDMI_SIM:
	case ADV7481_IP_CVBS_4_HDMI_SIM:
	case ADV7481_IP_CVBS_5_HDMI_SIM:
	case ADV7481_IP_CVBS_6_HDMI_SIM:
	case ADV7481_IP_CVBS_7_HDMI_SIM:
	case ADV7481_IP_CVBS_8_HDMI_SIM:
		ret = adv7481_set_hdmi_mode(state);
		ret |= adv7481_set_cvbs_mode(state);
		ret |= adv7481_set_analog_mux(state, input);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int adv7481_set_op_src(struct adv7481_state *state,
				int output, int input)
{
	int ret = 0;
	int temp = 0;
	int val = 0;

	pr_debug("Enter %s: output: %d, input: %d\n", __func__, output, input);
	switch (output) {
	case ADV7481_OP_CSIA:
		switch (input) {
		case ADV7481_IP_CVBS_1:
		case ADV7481_IP_CVBS_2:
		case ADV7481_IP_CVBS_3:
		case ADV7481_IP_CVBS_4:
		case ADV7481_IP_CVBS_5:
		case ADV7481_IP_CVBS_6:
		case ADV7481_IP_CVBS_7:
		case ADV7481_IP_CVBS_8:
			val = 0x10;
			break;
		case ADV7481_IP_CVBS_1_HDMI_SIM:
		case ADV7481_IP_CVBS_2_HDMI_SIM:
		case ADV7481_IP_CVBS_3_HDMI_SIM:
		case ADV7481_IP_CVBS_4_HDMI_SIM:
		case ADV7481_IP_CVBS_5_HDMI_SIM:
		case ADV7481_IP_CVBS_6_HDMI_SIM:
		case ADV7481_IP_CVBS_7_HDMI_SIM:
		case ADV7481_IP_CVBS_8_HDMI_SIM:
		case ADV7481_IP_HDMI:
			val = 0x00;
			break;
		case ADV7481_IP_TTL:
			val = 0x01;
			break;
		default:
			ret = -EINVAL;
		}
		temp = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
				IO_REG_PWR_DOWN_CTRL_ADDR);
		temp |= val;
		adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
				IO_REG_PWR_DOWN_CTRL_ADDR, temp);
		state->csia_src = input;
		break;
	case ADV7481_OP_CSIB:
		if (input != ADV7481_IP_HDMI && input != ADV7481_IP_TTL)
			state->csib_src = input;
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static u32 ba_inp_to_adv7481(u32 ba_input)
{
	u32 adv_input = ADV7481_IP_HDMI;

	switch (ba_input) {
	case BA_IP_CVBS_0:
		adv_input = ADV7481_IP_CVBS_1;
		break;
	case BA_IP_CVBS_1:
		adv_input = ADV7481_IP_CVBS_2;
		break;
	case BA_IP_CVBS_2:
		adv_input = ADV7481_IP_CVBS_3;
		break;
	case BA_IP_CVBS_3:
		adv_input = ADV7481_IP_CVBS_4;
		break;
	case BA_IP_CVBS_4:
		adv_input = ADV7481_IP_CVBS_5;
		break;
	case BA_IP_CVBS_5:
		adv_input = ADV7481_IP_CVBS_6;
		break;
	case BA_IP_HDMI_1:
		adv_input = ADV7481_IP_HDMI;
		break;
	case BA_IP_MHL_1:
		adv_input = ADV7481_IP_HDMI;
		break;
	case BA_IP_TTL:
		adv_input = ADV7481_IP_TTL;
		break;
	default:
		adv_input = ADV7481_IP_HDMI;
		break;
	}
	return adv_input;
}

static u32 adv7481_inp_to_ba(u32 adv_input)
{
	u32 ba_input = BA_IP_HDMI_1;

	switch (adv_input) {
	case ADV7481_IP_CVBS_1:
		ba_input = BA_IP_CVBS_0;
		break;
	case ADV7481_IP_CVBS_2:
		ba_input = BA_IP_CVBS_1;
		break;
	case ADV7481_IP_CVBS_3:
		ba_input = BA_IP_CVBS_2;
		break;
	case ADV7481_IP_CVBS_4:
		ba_input = BA_IP_CVBS_3;
		break;
	case ADV7481_IP_CVBS_5:
		ba_input = BA_IP_CVBS_4;
		break;
	case ADV7481_IP_CVBS_6:
		ba_input = BA_IP_CVBS_5;
		break;
	case ADV7481_IP_HDMI:
		ba_input = BA_IP_HDMI_1;
		break;
	case ADV7481_IP_TTL:
		ba_input = BA_IP_TTL;
		break;
	default:
		ba_input = BA_IP_HDMI_1;
		break;
	}
	return ba_input;
}

static int adv7481_s_routing(struct v4l2_subdev *sd, u32 input,
				u32 output, u32 config)
{
	int adv_input = ba_inp_to_adv7481(input);
	struct adv7481_state *state = to_state(sd);
	int ret = mutex_lock_interruptible(&state->mutex);

	if (ret)
		return ret;

	pr_debug("Enter %s\n", __func__);
	ret = adv7481_set_op_src(state, output, adv_input);
	if (ret) {
		pr_err("%s: Output SRC Routing Error: %d\n", __func__, ret);
		goto unlock_exit;
	}

	ret = adv7481_set_ip_mode(state, adv_input);
	if (ret)
		pr_err("%s: Set input mode failed: %d\n", __func__, ret);
	else
		state->mode = adv_input;

unlock_exit:
	mutex_unlock(&state->mutex);

	return ret;
}

static bool adv7481_is_timing_locked(struct adv7481_state *state)
{
	bool ret = false;
	int val1 = 0;
	int val2 = 0;

	/* Check Timing Lock IO Map Status3:0x71[0] && 0x71[1] && 0x71[7] */
	val1 = adv7481_rd_byte(&state->i2c_client, state->i2c_io_addr,
			IO_HDMI_LVL_RAW_STATUS_3_ADDR);
	val2 = adv7481_rd_byte(&state->i2c_client, state->i2c_cp_addr,
			CP_REG_STDI_CH_ADDR);

	if (ADV_REG_GETFIELD(val1, IO_DE_REGEN_LCK_RAW) &&
		ADV_REG_GETFIELD(val1, IO_V_LOCKED_RAW) &&
		ADV_REG_GETFIELD(val1, IO_TMDSPLL_LCK_A_RAW) &&
		ADV_REG_GETFIELD(val2, CP_STDI_DVALID_CH1))
		ret = true;

	return ret;
}

static int get_settle_cnt(struct resolution_config *configs,
			enum adv7481_resolution size, int w, int h)
{
	int i;
	int ret = -EINVAL;
	char res_type[20] = "RES_MAX";

	if (w == 1920 && h == 1080) {
		strlcpy(res_type, "RES_1080P", sizeof(res_type));
	} else if (w == 1280 && h == 720) {
		strlcpy(res_type, "RES_720P", sizeof(res_type));
	} else if ((w == 720 && h == 576) || (w == 720 && h == 480)) {
		strlcpy(res_type, "RES_576P_480P", sizeof(res_type));
	} else {
		pr_err("%s: Resolution not supported\n", __func__);
		return ret;
	}

	for (i = 0; i < size; i++) {
		if (strcmp(configs[i].resolution, res_type) == 0) {
			pr_debug("%s: settle count is set to %d\n",
				__func__, configs[i].settle_cnt);
			ret = configs[i].settle_cnt;
			break;
		}
	}
	return ret;
}


static int get_lane_cnt(struct resolution_config *configs,
			enum adv7481_resolution size, int w, int h)
{
	int i;
	int ret = -EINVAL;
	char res_type[20] = "RES_MAX";

	if (w == 1920 && h == 1080) {
		strlcpy(res_type, "RES_1080P", sizeof(res_type));
	} else if (w == 1280 && h == 720) {
		strlcpy(res_type, "RES_720P", sizeof(res_type));
	} else if ((w == 720 && h == 576) || (w == 720 && h == 480)) {
		strlcpy(res_type, "RES_576P_480P", sizeof(res_type));
	} else {
		pr_err("%s: Resolution not supported\n", __func__);
		return ret;
	}

	for (i = 0; i < size; i++) {
		if (strcmp(configs[i].resolution, res_type) == 0) {
			pr_debug("%s: lane count is set to %d\n",
				__func__, configs[i].lane_cnt);
			ret = configs[i].lane_cnt;
			break;
		}
	}
	return ret;
}

static int adv7481_get_hdmi_timings(struct adv7481_state *state,
				struct adv7481_vid_params *vid_params,
				struct adv7481_hdmi_params *hdmi_params)
{
	int ret = 0;
	int temp1 = 0;
	int temp2 = 0;
	int fieldfactor = 0;
	uint32_t count = 0;

	pr_debug("Enter %s\n", __func__);
	/* Check TMDS PLL Lock and Frequency */
	temp1 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_HDMI_PARAM4_ADDR);
	hdmi_params->pll_lock = ADV_REG_GETFIELD(temp1,
				HDMI_REG_TMDS_PLL_LOCKED);
	if (hdmi_params->pll_lock) {
		temp1 = adv7481_rd_byte(&state->i2c_client,
				state->i2c_hdmi_addr, HDMI_REG_TMDS_FREQ_ADDR);
		temp2 = adv7481_rd_byte(&state->i2c_client,
				state->i2c_hdmi_addr,
				HDMI_REG_TMDS_FREQ_FRAC_ADDR);
		hdmi_params->tmds_freq = ADV_REG_GETFIELD(temp1,
				HDMI_REG_TMDS_FREQ);
		hdmi_params->tmds_freq = (hdmi_params->tmds_freq << 1)
				+ ADV_REG_GETFIELD(temp2,
				HDMI_REG_TMDS_FREQ_0);
		hdmi_params->tmds_freq *= ONE_MHZ_TO_HZ;
		hdmi_params->tmds_freq += ADV_REG_GETFIELD(temp2,
				HDMI_REG_TMDS_FREQ_FRAC)*ONE_MHZ_TO_HZ/128;
	} else {
		pr_err("%s(%d): PLL not locked return EBUSY\n",
				__func__, __LINE__);
		ret = -EBUSY;
		goto set_default;
	}

	/* Check Timing Lock */
	do {
		if (adv7481_is_timing_locked(state))
			break;
		count++;
		usleep_range(LOCK_MIN_SLEEP, LOCK_MAX_SLEEP);
	} while (count < LOCK_NUM_TRIES);

	if (count >= LOCK_NUM_TRIES) {
		pr_err("%s(%d), HDMI DE regeneration block NOT Locked\n",
				__func__, __LINE__);
	}

	/* Check Timing Lock HDMI Map V:0x07[7], H:0x7[5] */
	do {
		temp1 = adv7481_rd_byte(&state->i2c_client,
				state->i2c_hdmi_addr,
				HDMI_REG_LINE_WIDTH_1_ADDR);

		if (ADV_REG_GETFIELD(temp1, HDMI_VERT_FILTER_LOCKED) &&
			ADV_REG_GETFIELD(temp1, HDMI_DE_REGEN_FILTER_LCK)) {
			break;
		}
		count++;
		usleep_range(LOCK_MIN_SLEEP, LOCK_MAX_SLEEP);
	} while (count < LOCK_NUM_TRIES);

	if (count >= LOCK_NUM_TRIES) {
		pr_err("%s(%d), adv7481 HDMI DE filter NOT Locked: 0x%x\n",
				__func__, __LINE__, temp1);
	}

	/* Check HDMI Parameters */
	temp1 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_FIELD1_HEIGHT1_ADDR);
	hdmi_params->color_depth = ADV_REG_GETFIELD(temp1,
				HDMI_REG_DEEP_COLOR_MODE);

	/* Check Interlaced and Field Factor */
	vid_params->intrlcd = ADV_REG_GETFIELD(temp1,
				HDMI_REG_HDMI_INTERLACED);
	fieldfactor = (vid_params->intrlcd == 1) ? 2 : 1;

	temp1 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_HDMI_PARAM5_ADDR);
	hdmi_params->pix_rep = ADV_REG_GETFIELD(temp1,
				HDMI_REG_PIXEL_REPETITION);

	/* Get Active Timing Data HDMI Map  H:0x07[4:0] + 0x08[7:0] */
	temp1 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_LINE_WIDTH_1_ADDR);
	temp2 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_LINE_WIDTH_2_ADDR);
	vid_params->act_pix = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_LINE_WIDTH_1) << 8) & 0x1F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_LINE_WIDTH_2));

	/* Get Total Timing Data HDMI Map  H:0x1E[5:0] + 0x1F[7:0] */
	temp1 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_TOTAL_LINE_WIDTH_1_ADDR);
	temp2 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_TOTAL_LINE_WIDTH_2_ADDR);
	vid_params->tot_pix = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_TOTAL_LINE_WIDTH_1) << 8) & 0x3F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_TOTAL_LINE_WIDTH_2));

	/* Get Active Timing Data HDMI Map  V:0x09[4:0] + 0x0A[7:0] */
	temp1 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_FIELD0_HEIGHT_1_ADDR);
	temp2 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_FIELD0_HEIGHT_2_ADDR);
	vid_params->act_lines = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_FIELD0_HEIGHT_1) << 8) & 0x1F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_FIELD0_HEIGHT_2));

	/* Get Total Timing Data HDMI Map  V:0x26[5:0] + 0x27[7:0] */
	temp1 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_FIELD0_TOTAL_HEIGHT_1_ADDR);
	temp2 = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_FIELD0_TOTAL_HEIGHT_2_ADDR);
	vid_params->tot_lines = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_FIELD0_TOT_HEIGHT_1) << 8) & 0x3F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_FIELD0_TOT_HEIGHT_2));
	vid_params->tot_lines /= 2;

	vid_params->pix_clk = hdmi_params->tmds_freq;

	vid_params->act_lines = vid_params->act_lines * fieldfactor;

	switch (hdmi_params->color_depth) {
	case CD_10BIT:
		vid_params->pix_clk = ((vid_params->pix_clk*4)/5);
		break;
	case CD_12BIT:
		vid_params->pix_clk = ((vid_params->pix_clk*2)/3);
		break;
	case CD_16BIT:
		vid_params->pix_clk = (vid_params->pix_clk/2);
		break;
	case CD_8BIT:
	default:
		vid_params->pix_clk /= 1;
		break;
	}

	if ((vid_params->tot_pix != 0) && (vid_params->tot_lines != 0)) {
		vid_params->fr_rate =
			DIV_ROUND_CLOSEST(vid_params->pix_clk * fieldfactor,
				vid_params->tot_lines);
		vid_params->fr_rate = DIV_ROUND_CLOSEST(vid_params->fr_rate,
						vid_params->tot_pix);
		vid_params->fr_rate = DIV_ROUND_CLOSEST(vid_params->fr_rate,
						(hdmi_params->pix_rep + 1));
	}

set_default:
	if (ret) {
		pr_debug("%s(%d), error %d resort to default fmt\n",
			__func__, __LINE__, ret);
		vid_params->act_pix = MAX_DEFAULT_WIDTH;
		vid_params->act_lines = MAX_DEFAULT_HEIGHT;
		vid_params->fr_rate = MAX_DEFAULT_FRAME_RATE;
		vid_params->pix_clk = MAX_DEFAULT_PIX_CLK_HZ;
		vid_params->intrlcd = 0;
		ret = 0;
	}

	pr_debug("%s(%d), adv7481 TMDS Resolution: %d x %d @ %d fps\n",
			__func__, __LINE__,
			vid_params->act_pix, vid_params->act_lines,
			vid_params->fr_rate);
	return ret;
}

static int adv7481_query_dv_timings(struct v4l2_subdev *sd,
			struct v4l2_dv_timings *timings)
{
	int ret;
	struct adv7481_state *state = to_state(sd);
	struct adv7481_vid_params vid_params;
	struct adv7481_hdmi_params hdmi_params;
	struct v4l2_bt_timings *bt_timings = &timings->bt;

	if (!timings)
		return -EINVAL;

	pr_debug("Enter %s\n", __func__);
	ret = mutex_lock_interruptible(&state->mutex);
	if (ret)
		return ret;

	memset(timings, 0, sizeof(struct v4l2_dv_timings));
	memset(&vid_params, 0, sizeof(struct adv7481_vid_params));
	memset(&hdmi_params, 0, sizeof(struct adv7481_hdmi_params));

	switch (state->mode) {
	case ADV7481_IP_HDMI:
	case ADV7481_IP_CVBS_1_HDMI_SIM:
		ret = adv7481_get_hdmi_timings(state, &vid_params,
			&hdmi_params);
		if (!ret) {
			timings->type = V4L2_DV_BT_656_1120;
			bt_timings->width = vid_params.act_pix;
			bt_timings->height = vid_params.act_lines;
			bt_timings->pixelclock = vid_params.pix_clk;
			bt_timings->interlaced = vid_params.intrlcd ?
				V4L2_DV_INTERLACED : V4L2_DV_PROGRESSIVE;
			if (bt_timings->interlaced == V4L2_DV_INTERLACED)
				bt_timings->height /= 2;
		} else {
			pr_err(
			"%s: Error in adv7481_get_hdmi_timings. ret %d\n",
			__func__, ret);
		}
		break;
	default:
		return -EINVAL;
	}
	mutex_unlock(&state->mutex);
	return ret;
}

static int adv7481_query_sd_std(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	int ret = 0;
	int temp = 0;
	struct adv7481_state *state = to_state(sd);
	uint8_t tStatus = 0x0;
	uint32_t count = 0;
	struct adv7481_vid_params vid_params;

	memset(&vid_params, 0, sizeof(vid_params));

	pr_debug("Enter %s\n", __func__);
	/* Select SDP read-only main Map */
	adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x01);
	do {
		tStatus = adv7481_rd_byte(&state->i2c_client,
				state->i2c_sdp_addr, SDP_RO_MAIN_STATUS1_ADDR);
		if (ADV_REG_GETFIELD(tStatus, SDP_RO_MAIN_IN_LOCK))
			break;
		count++;
		usleep_range(LOCK_MIN_SLEEP, LOCK_MAX_SLEEP);
	} while (count < LOCK_NUM_TRIES);

	adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x00);
	if (count >= LOCK_NUM_TRIES)
		pr_err("%s(%d), adv7481 SD Input NOT Locked: 0x%x\n",
			__func__, __LINE__, tStatus);

	if (!std)
		return -EINVAL;

	switch (state->mode) {
	case ADV7481_IP_CVBS_1:
	case ADV7481_IP_CVBS_2:
	case ADV7481_IP_CVBS_3:
	case ADV7481_IP_CVBS_4:
	case ADV7481_IP_CVBS_5:
	case ADV7481_IP_CVBS_6:
	case ADV7481_IP_CVBS_7:
	case ADV7481_IP_CVBS_8:
	case ADV7481_IP_CVBS_1_HDMI_SIM:
	case ADV7481_IP_CVBS_2_HDMI_SIM:
	case ADV7481_IP_CVBS_3_HDMI_SIM:
	case ADV7481_IP_CVBS_4_HDMI_SIM:
	case ADV7481_IP_CVBS_5_HDMI_SIM:
	case ADV7481_IP_CVBS_6_HDMI_SIM:
	case ADV7481_IP_CVBS_7_HDMI_SIM:
	case ADV7481_IP_CVBS_8_HDMI_SIM:
		ret = adv7481_get_sd_timings(state, &temp, &vid_params);
		break;
	default:
		return -EINVAL;
	}

	if (!tStatus)
		*std = (v4l2_std_id) temp;
	else
		*std = V4L2_STD_UNKNOWN;

	return ret;
}

static int adv7481_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	pr_debug("Enter %s\n", __func__);

	interval->interval.numerator = 1;
	interval->interval.denominator = 60;

	return 0;
}

static int adv7481_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	int ret;
	int sd_standard;
	struct adv7481_vid_params vid_params;
	struct adv7481_hdmi_params hdmi_params;
	struct adv7481_state *state = to_state(sd);
	struct v4l2_mbus_framefmt *fmt = &format->format;

	if (!fmt)
		return -EINVAL;

	pr_debug("Enter %s\n", __func__);
	ret = mutex_lock_interruptible(&state->mutex);
	if (ret)
		return ret;

	memset(&vid_params, 0, sizeof(struct adv7481_vid_params));
	memset(&hdmi_params, 0, sizeof(struct adv7481_hdmi_params));

	switch (state->mode) {
	case ADV7481_IP_HDMI:
	case ADV7481_IP_CVBS_1_HDMI_SIM:
		ret = adv7481_get_hdmi_timings(state, &vid_params,
			&hdmi_params);
		if (!ret) {
			fmt->width = vid_params.act_pix;
			fmt->height = vid_params.act_lines;
			fmt->field = V4L2_FIELD_NONE;
			if (vid_params.intrlcd)
				fmt->field = V4L2_FIELD_INTERLACED;
		} else {
			pr_err("%s: Error %d in adv7481_get_hdmi_timings\n",
				__func__, ret);
		}
		break;
	case ADV7481_IP_CVBS_1:
		fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
		fmt->colorspace = V4L2_COLORSPACE_SMPTE170M;
		ret = adv7481_get_sd_timings(state, &sd_standard, &vid_params);
		if (!ret) {
			fmt->width = vid_params.act_pix;
			fmt->height = vid_params.act_lines;
			fmt->field = V4L2_FIELD_INTERLACED;
		} else {
			pr_err("%s: Unable to get sd_timings\n", __func__);
		}
		break;
	default:
		return -EINVAL;
	}
	mutex_unlock(&state->mutex);
	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	fmt->colorspace = V4L2_COLORSPACE_SMPTE170M;
	return ret;
}

static int adv7481_set_audio_spdif(struct adv7481_state *state,
		bool on)
{
	int ret;
	uint8_t val;

	if (on) {
		/* Configure I2S_SDATA output pin as an SPDIF output 0x6E[3] */
		val = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR);
		val |= ADV_REG_SETFIELD(1, HDMI_MUX_SPDIF_TO_I2S_EN);
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR, val);
	} else {
		/* Configure I2S_SDATA output pin as an I2S output 0x6E[3] */
		val = adv7481_rd_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR);
		val &= ~ADV_REG_SETFIELD(1, HDMI_MUX_SPDIF_TO_I2S_EN);
		ret = adv7481_wr_byte(&state->i2c_client, state->i2c_hdmi_addr,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR, val);
	}
	return ret;
}

static int adv7481_csi_powerdown(struct adv7481_state *state,
		enum adv7481_output output)
{
	int ret;
	uint8_t csi_map;
	uint8_t val = 0;

	pr_debug("Enter %s for output: %d\n", __func__, output);
	/* Select CSI TX to configure data */
	if (output == ADV7481_OP_CSIA) {
		csi_map = state->i2c_csi_txa_addr;
	} else if (output == ADV7481_OP_CSIB) {
		csi_map = state->i2c_csi_txb_addr;
	} else if (output == ADV7481_OP_TTL) {
		/* For now use TxA */
		csi_map = state->i2c_csi_txa_addr;
	} else {
		/* Default to TxA */
		csi_map = state->i2c_csi_txa_addr;
	}
	/* CSI Tx: power down DPHY */
	ret = adv7481_wr_byte(&state->i2c_client, csi_map,
			CSI_REG_TX_DPHY_PWDN_ADDR,
			ADV_REG_SETFIELD(1, CSI_CTRL_DPHY_PWDN));
	/* ADI Required Write */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0x31, 0x82);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0x1e, 0x00);
	/* CSI TxA: # Lane : Power Off */
	val = ADV_REG_SETFIELD(1, CSI_CTRL_TX_PWRDN) |
			ADV_REG_SETFIELD(state->tx_lanes, CSI_CTRL_NUM_LANES);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map,
			CSI_REG_TX_CFG1_ADDR, val);
	/*
	 * ADI Recommended power down sequence
	 * DPHY and CSI Tx A Power down Sequence
	 * CSI TxA: MIPI PLL DIS
	 */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xda, 0x00);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xc1, 0x3b);

	pr_debug("Exit %s, ret: %d\n", __func__, ret);

	return ret;
}

static int adv7481_csi_powerup(struct adv7481_state *state,
		enum adv7481_output output)
{
	int ret;
	uint8_t csi_map;
	uint8_t val = 0;
	uint8_t csi_sel = 0;

	pr_debug("Enter %s for output: %d\n", __func__, output);
	/* Select CSI TX to configure data */
	if (output == ADV7481_OP_CSIA) {
		if (state->csia_src == ADV7481_IP_HDMI) {
			csi_sel = ADV_REG_SETFIELD(1, IO_CTRL_CSI4_EN) |
				ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
				ADV_REG_SETFIELD(0, IO_CTRL_CSI4_IN_SEL);
		} else {
			csi_sel = ADV_REG_SETFIELD(1, IO_CTRL_CSI4_EN) |
				ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
				ADV_REG_SETFIELD(0x2, IO_CTRL_CSI4_IN_SEL);
		}
		csi_map = state->i2c_csi_txa_addr;
	} else if (output == ADV7481_OP_CSIB) {
		/* Enable 1-Lane MIPI Tx, enable pixel output and
		 * route SD through Pixel port
		 */
		csi_sel = ADV_REG_SETFIELD(1, IO_CTRL_CSI1_EN) |
			ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
			ADV_REG_SETFIELD(1, IO_CTRL_SD_THRU_PIX_OUT) |
			ADV_REG_SETFIELD(0, IO_CTRL_CSI4_IN_SEL);
		csi_map = state->i2c_csi_txb_addr;
	} else if (output == ADV7481_OP_TTL) {
		/* For now use TxA */
		csi_map = state->i2c_csi_txa_addr;
	} else {
		/* Default to TxA */
		csi_map = state->i2c_csi_txa_addr;
	}

	/* Enable Tx A/B CSI #-lane */
	ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
			IO_REG_CSI_PIX_EN_SEL_ADDR, csi_sel);
	/* TXA MIPI lane settings for CSI */
	/* CSI TxA: # Lane : Power Off */
	val = ADV_REG_SETFIELD(1, CSI_CTRL_TX_PWRDN) |
			ADV_REG_SETFIELD(state->tx_lanes, CSI_CTRL_NUM_LANES);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map,
			CSI_REG_TX_CFG1_ADDR, val);
	/* CSI TxA: Auto D-PHY Timing */
	val |= ADV_REG_SETFIELD(1, CSI_CTRL_AUTO_PARAMS);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map,
			CSI_REG_TX_CFG1_ADDR, val);

	/* DPHY and CSI Tx A */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xdb, 0x10);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xd6, 0x07);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xc4, 0x0a);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0x71, 0x33);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0x72, 0x11);
	/* CSI TxA: power up DPHY */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xf0, 0x00);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0x31, 0x82);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0x1e, 0x40);
	/* adi Recommended power up sequence */
	/* DPHY and CSI Tx A Power up Sequence */
	/* CSI TxA: MIPI PLL EN */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xda, 0x01);
	msleep(200);
	/* CSI TxA: # MIPI Lane : Power ON */
	val = ADV_REG_SETFIELD(0, CSI_CTRL_TX_PWRDN) |
			ADV_REG_SETFIELD(1, CSI_CTRL_AUTO_PARAMS) |
			ADV_REG_SETFIELD(state->tx_lanes, CSI_CTRL_NUM_LANES);
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map,
			CSI_REG_TX_CFG1_ADDR, val);
	msleep(100);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0xc1, 0x2b);
	msleep(100);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(&state->i2c_client, csi_map, 0x31, 0x80);

	pr_debug("Exit %s, ret: %d\n", __func__, ret);

	return ret;
}

static int adv7481_set_op_stream(struct adv7481_state *state, bool on)
{
	int ret = 0;
	struct adv7481_vid_params vid_params;
	struct adv7481_hdmi_params hdmi_params;

	pr_debug("Enter %s: on: %d, a src: %d, b src: %d\n",
			__func__, on, state->csia_src, state->csib_src);
	memset(&vid_params, 0, sizeof(struct adv7481_vid_params));
	memset(&hdmi_params, 0, sizeof(struct adv7481_hdmi_params));

	if (on && state->csia_src != ADV7481_IP_NONE)
		if (state->csia_src == ADV7481_IP_HDMI) {
			ret = adv7481_get_hdmi_timings(state, &vid_params,
				&hdmi_params);
			if (ret) {
				pr_err("%s: Error %d in adv7481_get_hdmi_timings\n",
					__func__, ret);
				return -EINVAL;
			}
			state->tx_lanes = get_lane_cnt(state->res_configs,
			RES_MAX, vid_params.act_pix, vid_params.act_lines);

			if (state->tx_lanes < 0) {
				pr_err("%s: Invalid lane count\n", __func__);
				return -EINVAL;
			}
			ret = adv7481_set_audio_spdif(state, on);
			ret |= adv7481_csi_powerup(state, ADV7481_OP_CSIA);
		} else {
			state->tx_lanes = ADV7481_MIPI_1LANE;
			ret = adv7481_csi_powerup(state, ADV7481_OP_CSIA);
		}
	else if (on && state->csib_src != ADV7481_IP_NONE) {
		/* CSI Tx B is always 1 lane */
		state->tx_lanes = ADV7481_MIPI_1LANE;
		ret = adv7481_csi_powerup(state, ADV7481_OP_CSIB);
	} else {
		/* Turn off */
		if (ADV7481_IP_NONE != state->csia_src) {
			if (ADV7481_IP_HDMI == state->csia_src) {
				state->tx_lanes = ADV7481_MIPI_4LANE;
				ret = adv7481_set_audio_spdif(state, on);
			} else {
				state->tx_lanes = ADV7481_MIPI_1LANE;
			}
			ret |= adv7481_csi_powerdown(state, ADV7481_OP_CSIA);
		} else if (ADV7481_IP_NONE != state->csib_src) {
			/* CSI Tx B is always 1 lane */
			state->tx_lanes = ADV7481_MIPI_1LANE;
			ret = adv7481_csi_powerdown(state, ADV7481_OP_CSIB);
		} else {
			/* CSI TxA and all 4 lanes off */
			state->tx_lanes = ADV7481_MIPI_4LANE;
			ret = adv7481_csi_powerdown(state, ADV7481_OP_CSIA);
			/* CSI Tx B is always 1 lane */
			state->tx_lanes = ADV7481_MIPI_1LANE;
			ret |= adv7481_csi_powerdown(state, ADV7481_OP_CSIB);
		}
	}
	return ret;
}

static int adv7481_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	int ret = 0;
	struct adv7481_state *state = to_state(sd);
	uint8_t val = 0;
	uint32_t count = 0;

	*status = 0;
	pr_debug("Enter %s\n", __func__);
	if (ADV7481_IP_HDMI == state->mode) {
		/* Check Timing Lock */
		do {
			if (adv7481_is_timing_locked(state))
				break;
			count++;
			usleep_range(LOCK_MIN_SLEEP, LOCK_MAX_SLEEP);
		} while (count < LOCK_NUM_TRIES);

		if (count >= LOCK_NUM_TRIES) {
			pr_err("%s(%d), HDMI DE regeneration block NOT Locked\n",
					__func__, __LINE__);
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	} else {
		/* Select SDP read-only main Map */
		adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x01);
		do {
			val = adv7481_rd_byte(&state->i2c_client,
				state->i2c_sdp_addr, SDP_RO_MAIN_STATUS1_ADDR);
			if (ADV_REG_GETFIELD(val, SDP_RO_MAIN_IN_LOCK))
				break;
			count++;
			usleep_range(LOCK_MIN_SLEEP, LOCK_MAX_SLEEP);
		} while (count < LOCK_NUM_TRIES);

		adv7481_wr_byte(&state->i2c_client, state->i2c_sdp_addr,
				SDP_RW_MAP_REG, 0x00);
		if (count >= LOCK_NUM_TRIES) {
			pr_err("%s(%d), SD Input NOT Locked: 0x%x\n",
					__func__, __LINE__, val);
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	}
	return ret;
}

static int adv7481_s_stream(struct v4l2_subdev *sd, int on)
{
	struct adv7481_state *state = to_state(sd);
	int ret;

	ret = adv7481_set_op_stream(state, on);
	return ret;
}

static const struct v4l2_subdev_video_ops adv7481_video_ops = {
	.s_routing = adv7481_s_routing,
	.g_frame_interval = adv7481_g_frame_interval,
	.querystd = adv7481_query_sd_std,
	.g_dv_timings = adv7481_query_dv_timings,
	.g_input_status = adv7481_g_input_status,
	.s_stream = adv7481_s_stream,
};

static const struct v4l2_subdev_core_ops adv7481_core_ops = {
	.s_power = adv7481_s_power,
	.ioctl = adv7481_ioctl,
};

static const struct v4l2_subdev_pad_ops adv7481_pad_ops = {
	.get_fmt = adv7481_get_fmt,
};

static const struct v4l2_ctrl_ops adv7481_ctrl_ops = {
	.s_ctrl = adv7481_s_ctrl,
};

static const struct v4l2_subdev_ops adv7481_ops = {
	.core = &adv7481_core_ops,
	.video = &adv7481_video_ops,
	.pad = &adv7481_pad_ops,
};

static int adv7481_init_v4l2_controls(struct adv7481_state *state)
{
	int ret = 0;

	ret = v4l2_ctrl_handler_init(&state->ctrl_hdl, 4);
	if (ret) {
		pr_err("%s: v4l2_ctrl_handler_init failed, ret: %d\n",
			__func__, ret);
		return ret;
	}

	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7481_ctrl_ops,
			  V4L2_CID_BRIGHTNESS, -128, 127, 1, 0);
	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7481_ctrl_ops,
			  V4L2_CID_CONTRAST, 0, 255, 1, 128);
	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7481_ctrl_ops,
			  V4L2_CID_SATURATION, 0, 255, 1, 128);
	v4l2_ctrl_new_std(&state->ctrl_hdl, &adv7481_ctrl_ops,
			  V4L2_CID_HUE, -127, 128, 1, 0);

	state->sd.ctrl_handler = &state->ctrl_hdl;
	if (state->ctrl_hdl.error) {
		ret = state->ctrl_hdl.error;

		v4l2_ctrl_handler_free(&state->ctrl_hdl);
	} else {
		ret = v4l2_ctrl_handler_setup(&state->ctrl_hdl);
		if (ret)
			pr_err("%s: v4l2_ctrl_handler_init failed, ret: %d\n",
				__func__, ret);
	}

	pr_err("%s: Exit with ret: %d\n", __func__, ret);
	return ret;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static int adv7481_cci_init(struct adv7481_state *state)
{
	struct msm_camera_cci_client *cci_client = NULL;
	int ret = 0;

	pr_err("%s: Enter\n", __func__);

	state->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	state->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	state->i2c_client.cci_client = kzalloc(sizeof(
			struct msm_camera_cci_client), GFP_KERNEL);
	cci_client = state->i2c_client.cci_client;
	if (!cci_client) {
		ret = -ENOMEM;
		goto err_cci_init;
	}
	cci_client->cci_subdev = msm_cci_get_subdev();
	pr_debug("%s cci_subdev: %p\n", __func__, cci_client->cci_subdev);
	if (!cci_client->cci_subdev) {
		ret = -EPROBE_DEFER;
		goto err_cci_init;
	}
	cci_client->cci_i2c_master = state->cci_master;
	cci_client->sid = state->i2c_slave_addr;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->i2c_freq_mode = I2C_CUSTOM_MODE;
	ret = state->i2c_client.i2c_func_tbl->i2c_util(
				&state->i2c_client, MSM_CCI_INIT);
	if (ret < 0)
		pr_err("%s - cci_init failed\n", __func__);
	else
		state->clocks_requested = TRUE;

	pr_debug("%s i2c_client.client: %p\n", __func__,
		state->i2c_client.client);

err_cci_init:
	return ret;
}

static int adv7481_parse_dt(struct platform_device *pdev,
			struct adv7481_state *state)
{
	struct device_node *np = state->dev->of_node;
	uint32_t i = 0;
	uint32_t lane_count[RES_MAX];
	uint32_t settle_count[RES_MAX];
	static const char *resolution_array[RES_MAX];
	int gpio_count = 0;
	struct resource *adv_addr_res = NULL;
	int ret = 0;

	/* config CCI */
	ret = of_property_read_u32(np, "qcom,cci-master",
			&state->cci_master);
	if (ret < 0 || state->cci_master >= MASTER_MAX) {
		pr_err("%s: failed to read cci master . ret %d\n",
			__func__, ret);
		goto exit;
	}
	pr_debug("%s: cci_master: 0x%x\n", __func__, state->cci_master);
	/* read CSI data line */
	ret = of_property_read_u32_array(np, "tx-lanes",
				lane_count, RES_MAX);
	if (ret < 0) {
		pr_err("%s: failed to read data lane array . ret %d\n",
			__func__, ret);
		goto exit;
	}
	/* read settle count */
	ret = of_property_read_u32_array(np, "settle-count",
				settle_count, RES_MAX);
	if (ret < 0) {
		pr_err("%s: failed to read settle count . ret %d\n",
			__func__, ret);
		goto exit;
	}
	/* read resolution array */
	ret = of_property_read_string_array(np, "res-array",
				resolution_array, RES_MAX);
	if (ret < 0) {
		pr_err("%s: failed to read resolution array . ret %d\n",
			__func__, ret);
		goto exit;
	}
	for (i = 0; i < RES_MAX; i++) {
		state->res_configs[i].lane_cnt = (uint32_t)lane_count[i];
		state->res_configs[i].settle_cnt = (uint32_t)settle_count[i];
		strlcpy(state->res_configs[i].resolution, resolution_array[i],
			sizeof(state->res_configs[i].resolution));
	}
	adv_addr_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!adv_addr_res) {
		pr_err("%s: failed to read adv7481 resource.\n", __func__);
		goto exit;
	}
	state->i2c_slave_addr = adv_addr_res->start;
	pr_debug("%s: i2c_slave_addr: 0x%x\n", __func__, state->i2c_slave_addr);
	state->i2c_io_addr = (uint8_t)state->i2c_slave_addr;

	gpio_count = of_gpio_count(np);
	if (gpio_count != ADV7481_GPIO_MAX) {
		ret = -EFAULT;
		pr_err("%s: dt gpio count %d doesn't match required. ret %d\n",
			__func__, gpio_count, ret);
		goto exit;
	}
	for (i = 0; i < ADV7481_GPIO_MAX; i++) {
		state->gpio_array[i].gpio = of_get_gpio_flags(np, i,
			(enum of_gpio_flags *)&state->gpio_array[i].flags);
		if (!gpio_is_valid(state->gpio_array[i].gpio)) {
			pr_err("invalid gpio setting for index %d\n", i);
			ret = -EFAULT;
			goto exit;
		}
		pr_debug("%s: gpio_array[%d] = %d flag = %ld\n", __func__, i,
			state->gpio_array[i].gpio, state->gpio_array[i].flags);
	}

exit:
	return ret;
}

static const struct of_device_id adv7481_id[] = {
	{ .compatible = "qcom,adv7481", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, adv7481_id);

static int adv7481_probe(struct platform_device *pdev)
{
	struct adv7481_state *state;
	const struct of_device_id *device_id;
	struct v4l2_subdev *sd;
	int ret;

	device_id = of_match_device(adv7481_id, &pdev->dev);
	if (!device_id) {
		pr_err("%s: device_id is NULL\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	/* Create 7481 State */
	state = devm_kzalloc(&pdev->dev,
			sizeof(struct adv7481_state), GFP_KERNEL);
	if (state == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	platform_set_drvdata(pdev, state);
	state->dev = &pdev->dev;

	mutex_init(&state->mutex);
	ret = adv7481_parse_dt(pdev, state);
	if (ret < 0) {
		pr_err("Error parsing dt tree\n");
		goto err_mem_free;
	}

	ret = adv7481_cci_init(state);
	if (ret < 0) {
		pr_err("%s: failed adv7481_cci_init ret %d\n", __func__, ret);
		goto err_mem_free;
	}

	/* config VREG */
	ret = msm_camera_get_dt_vreg_data(pdev->dev.of_node,
			&(state->cci_vreg), &(state->regulator_count));
	if (ret < 0) {
		pr_err("%s:cci get_dt_vreg failed\n", __func__);
		goto err_mem_free;
	}

	ret = msm_camera_config_vreg(&pdev->dev, state->cci_vreg,
			state->regulator_count, NULL, 0,
			&state->cci_reg_ptr[0], 1);
	if (ret < 0) {
		pr_err("%s:cci config_vreg failed\n", __func__);
		goto err_mem_free;
	}

	ret = msm_camera_enable_vreg(&pdev->dev, state->cci_vreg,
			state->regulator_count, NULL, 0,
			&state->cci_reg_ptr[0], 1);
	if (ret < 0) {
		pr_err("%s:cci enable_vreg failed\n", __func__);
		goto err_mem_free;
	}
	pr_debug("%s - VREG Initialized...\n", __func__);

	/* Configure and Register V4L2 Sub-device */
	sd = &state->sd;
	v4l2_subdev_init(sd, &adv7481_ops);
	sd->owner = pdev->dev.driver->owner;
	v4l2_set_subdevdata(sd, state);
	strlcpy(sd->name, DRIVER_NAME, sizeof(sd->name));
	state->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;

	/* Register as Media Entity */
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	state->sd.entity.flags |= MEDIA_ENT_T_V4L2_SUBDEV;
	ret = media_entity_init(&state->sd.entity, 1, &state->pad, 0);
	if (ret) {
		ret = -EIO;
		pr_err("%s(%d): Media entity init failed\n",
			__func__, __LINE__);
		goto err_media_entity;
	}

	/* Initialize HW Config */
	ret = adv7481_hw_init(state);
	if (ret) {
		ret = -EIO;
		pr_err("%s: HW Initialisation Failed\n", __func__);
		goto err_media_entity;
	}

	/* Soft reset */
	ret = adv7481_wr_byte(&state->i2c_client, state->i2c_io_addr,
		IO_REG_MAIN_RST_ADDR, IO_REG_MAIN_RST_VALUE);
	if (ret) {
		pr_err("%s: Failed Soft reset %d\n", __func__, ret);
		goto err_media_entity;
	}
	/* Delay required following I2C reset and I2C transactions */
	udelay(I2C_SW_RST_DELAY);

	/* Register V4l2 Control Functions */
	ret = adv7481_init_v4l2_controls(state);
	if (ret) {
		pr_err("%s: V4L2 Controls Initialisation Failed %d\n",
			__func__, ret);
		goto err_media_entity;
	}

	/* Initial ADV7481 State Settings */
	state->tx_auto_params = ADV7481_AUTO_PARAMS;

	/* Initialize SW Init Settings and I2C sub maps 7481 */
	ret = adv7481_dev_init(state);
	if (ret) {
		ret = -EIO;
		pr_err("%s(%d): SW Initialisation Failed\n",
			__func__, __LINE__);
		goto err_media_entity;
	}

	/* BA registration */
	ret = msm_ba_register_subdev_node(sd);
	if (ret) {
		ret = -EIO;
		pr_err("%s: BA init failed\n", __func__);
		goto err_media_entity;
	}
	enable_irq(state->irq);
	pr_info("ADV7481 Probe successful!\n");

	return ret;

err_media_entity:
	media_entity_cleanup(&sd->entity);

err_mem_free:
	adv7481_release_cci_clks(state);
	devm_kfree(&pdev->dev, state);

err:
	return ret;
}

static int adv7481_remove(struct platform_device *pdev)
{
	struct adv7481_state *state = platform_get_drvdata(pdev);

	msm_ba_unregister_subdev_node(&state->sd);
	v4l2_device_unregister_subdev(&state->sd);
	media_entity_cleanup(&state->sd.entity);

	v4l2_ctrl_handler_free(&state->ctrl_hdl);

	adv7481_reset_irq(state);
	if (state->irq > 0)
		free_irq(state->irq, state);

	cancel_delayed_work(&state->irq_delayed_work);
	mutex_destroy(&state->mutex);
	devm_kfree(&pdev->dev, state);

	return 0;
}

static int adv7481_suspend(struct device *dev)
{
	struct adv7481_state *state;
	int ret;

	state = (struct adv7481_state *)dev_get_drvdata(dev);

	/* release CCI clocks */
	ret = adv7481_release_cci_clks(state);
	if (ret)
		pr_err("%s: adv7481 release cci clocks failed\n", __func__);
	else
		pr_debug("released cci clocks in suspend");

	return 0;
}

static int adv7481_resume(struct device *dev)
{
	struct adv7481_state *state;
	int ret;

	state = (struct adv7481_state *)dev_get_drvdata(dev);

	/* Request CCI clocks */
	ret = adv7481_request_cci_clks(state);
	if (ret)
		pr_err("%s: adv7481 request cci clocks failed\n", __func__);
	else
		pr_debug("requested cci clocks in resume");

	return 0;
}

static SIMPLE_DEV_PM_OPS(adv7481_pm_ops, adv7481_suspend, adv7481_resume);
#define ADV7481_PM_OPS (&adv7481_pm_ops)

static struct platform_driver adv7481_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = KBUILD_MODNAME,
		.of_match_table = adv7481_id,
		.pm = ADV7481_PM_OPS,
	},
	.probe = adv7481_probe,
	.remove = adv7481_remove,
};

module_driver(adv7481_driver, platform_driver_register,
		platform_driver_unregister);

MODULE_DESCRIPTION("ADI ADV7481 HDMI/MHL/SD video receiver");
MODULE_LICENSE("GPL v2");
