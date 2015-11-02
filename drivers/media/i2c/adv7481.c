/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <media/adv7481.h>
#include <media/msm_ba.h>

#include "adv7481_reg.h"

#define DRIVER_NAME "adv7481"

#define I2C_RW_DELAY		100
#define I2C_SW_RST_DELAY	10000
#define GPIO_HW_DELAY_LOW	100000
#define GPIO_HW_DELAY_HI	10000
#define SDP_MIN_SLEEP		5000
#define SDP_MAX_SLEEP		6000
#define SDP_NUM_TRIES		30
#define LOCK_MIN_SLEEP		5000
#define LOCK_MAX_SLEEP		6000
#define LOCK_NUM_TRIES		20

#define ONE_MHZ_TO_HZ		1000000

struct adv7481_state {
	/* Platform Data */
	struct adv7481_platform_data pdata;

	/* V4L2 Data */
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_hdl;
	struct v4l2_dv_timings timings;
	struct v4l2_ctrl *cable_det_ctrl;

	/* media entity controls */
	struct media_pad pad;

	struct workqueue_struct *work_queues;
	struct mutex		mutex;

	struct i2c_client *client;
	struct i2c_client *i2c_csi_txa;
	struct i2c_client *i2c_csi_txb;
	struct i2c_client *i2c_hdmi;
	struct i2c_client *i2c_edid;
	struct i2c_client *i2c_cp;
	struct i2c_client *i2c_sdp;
	struct i2c_client *i2c_rep;

	/* device status and Flags */
	int irq;
	int device_num;
	int powerup;

	/* routing configuration data */
	int csia_src;
	int csib_src;
	int mode;

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
0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27, 0x12, 0x48, 0x4C,
/* Established Timings */
0x21, 0x08, 0x00,
/* Standard Timings */
0x81, 0xC0, 0x81, 0x40, 0x3B, 0xC0, 0x3B, 0x40,
0x31, 0xC0, 0x31, 0x40, 0x01, 0x01, 0x01, 0x01,
/* Detailed Timings Block */
0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0xA0, 0x5A, 0x00, 0x00,
0x00, 0x1E,
/* Monitor Descriptor Block 2 */
0x8C, 0x0A, 0xD0, 0xB4, 0x20, 0xE0, 0x14, 0x10,
0x12, 0x48, 0x3A, 0x00, 0xD8, 0xA2, 0x00, 0x00,
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
0x5B,

/* Block 1 (Extension Block) */
/* Extension Header */
0x02, 0x03, 0x1E,
/* Display supports */
0x71,
/* Video Data Bock */
0x48, 0x84, 0x13, 0x3C, 0x03, 0x02, 0x11, 0x12,
0x01,
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
0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20,
0x6E, 0x28, 0x55, 0x00, 0xA0, 0x2A, 0x53, 0x00,
0x00, 0x1E,
/* Detailed Timing Descriptor */
0x8C, 0x0A, 0xD0, 0xB4, 0x20, 0xE0, 0x14, 0x10,
0x12, 0x48, 0x3A, 0x00, 0xD8, 0xA2, 0x00, 0x00,
0x00, 0x1E,
/* Detailed Timing Descriptor */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00,
/* Detailed Timing Descriptor */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00,
/* Detailed Timing Descriptor */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00,
/* DTD padding */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* Checksum */
0xC6
};

#define ADV7481_EDID_SIZE ARRAY_SIZE(adv7481_default_edid_data)

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
static int adv7481_wr_byte(struct i2c_client *i2c_client, unsigned int reg,
	unsigned int value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(i2c_client, reg & 0xFF, value);
	usleep_range(I2C_RW_DELAY, 2*I2C_RW_DELAY);

	return ret;
}

static int adv7481_rd_byte(struct i2c_client *i2c_client, unsigned int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(i2c_client, reg & 0xFF);
	usleep_range(I2C_RW_DELAY, 2*I2C_RW_DELAY);

	return ret;
}

static int adv7481_set_irq(struct adv7481_state *state)
{
	int ret = 0;

	ret = adv7481_wr_byte(state->client, IO_REG_PAD_CTRL_1_ADDR,
			ADV_REG_SETFIELD(1, IO_PDN_INT2) |
			ADV_REG_SETFIELD(1, IO_PDN_INT3) |
			ADV_REG_SETFIELD(1, IO_INV_LLC) |
			ADV_REG_SETFIELD(AD_MID_DRIVE_STRNGTH, IO_DRV_LLC_PAD));
	ret |= adv7481_wr_byte(state->client, IO_REG_INT1_CONF_ADDR,
			ADV_REG_SETFIELD(AD_ACTIVE_UNTIL_CLR,
				IO_INTRQ_DUR_SEL) |
			ADV_REG_SETFIELD(AD_OP_DRIVE_LOW, IO_INTRQ_OP_SEL));
	ret |= adv7481_wr_byte(state->client, IO_REG_INT2_CONF_ADDR,
			ADV_REG_SETFIELD(1, IO_CP_LOCK_UNLOCK_EDGE_SEL));
	ret |= adv7481_wr_byte(state->client, IO_REG_DATAPATH_INT_MASKB_ADDR,
			ADV_REG_SETFIELD(1, IO_CP_LOCK_CP_MB1) |
			ADV_REG_SETFIELD(1, IO_CP_UNLOCK_CP_MB1) |
			ADV_REG_SETFIELD(1, IO_VMUTE_REQUEST_HDMI_MB1) |
			ADV_REG_SETFIELD(1, IO_INT_SD_MB1));
	/* Set hpa */
	ret |= adv7481_wr_byte(state->client, IO_HDMI_LVL_INT_MASKB_3_ADDR,
			ADV_REG_SETFIELD(1, IO_CABLE_DET_A_MB1));

	if (ret)
		pr_err("%s: Failed %d to setup interrupt regs\n",
				__func__, ret);
	else
		enable_irq(state->irq);

	return ret;
}

static int adv7481_set_edid(struct adv7481_state *state)
{
	int i;
	int ret = 0;
	uint8_t edid_state;

	/* Enable Manual Control of EDID on Port A */
	ret |= adv7481_wr_byte(state->i2c_rep, 0x74, 0x01);
	/* Disable Auto Enable of EDID */
	ret |= adv7481_wr_byte(state->i2c_rep, 0x7A, 0x08);
	/* Set Primary EDID Size to 256 Bytes */
	ret |= adv7481_wr_byte(state->i2c_rep, 0x70, 0x20);

	/*
	 * Readback EDID enable state after a combination of manual
	 * and automatic functions
	 */
	edid_state = adv7481_rd_byte(state->i2c_rep,
					HDMI_REG_RO_EDID_DEBUG_2_ADDR);
	pr_debug("%s: Readback EDID enable state: 0x%x\n", __func__,
			edid_state);

	for (i = 0; i < ADV7481_EDID_SIZE; i++) {
		ret |= adv7481_wr_byte(state->i2c_edid, i,
						adv7481_default_edid_data[i]);
	}

	return ret;
}

static irqreturn_t adv7481_irq(int irq, void *dev)
{
	struct adv7481_state *state = dev;

	schedule_delayed_work(&state->irq_delayed_work,
						msecs_to_jiffies(0));
	return IRQ_HANDLED;
}

static void adv7481_irq_delay_work(struct work_struct *work)
{
	struct adv7481_state *state;
	uint8_t status;

	state = container_of(work, struct adv7481_state,
				irq_delayed_work.work);

	mutex_lock(&state->mutex);

	/* workaround for irq trigger */
	status = adv7481_rd_byte(state->client,
			IO_REG_INT_RAW_STATUS_ADDR);

	pr_debug("%s: dev: %d got int raw status: 0x%x\n", __func__,
			state->device_num, status);

	status = adv7481_rd_byte(state->client,
			IO_REG_DATAPATH_INT_STATUS_ADDR);

	pr_debug("%s: dev: %d got datapath int status: 0x%x\n", __func__,
			state->device_num, status);

	adv7481_wr_byte(state->client,
			IO_REG_DATAPATH_INT_CLEAR_ADDR, status);

	status = adv7481_rd_byte(state->client,
			IO_REG_DATAPATH_RAW_STATUS_ADDR);

	pr_debug("%s: dev: %d got datapath rawstatus: 0x%x\n", __func__,
			state->device_num, status);

	status = adv7481_rd_byte(state->client,
			IO_HDMI_LVL_INT_STATUS_3_ADDR);

	pr_debug("%s: dev: %d got hdmi lvl int status 3: 0x%x\n", __func__,
			state->device_num, status);

	adv7481_wr_byte(state->client,
			IO_HDMI_LVL_INT_CLEAR_3_ADDR, status);

	mutex_unlock(&state->mutex);
}

static int adv7481_cec_wakeup(struct adv7481_state *state, bool enable)
{
	uint8_t val;
	int ret = 0;

	val = adv7481_rd_byte(state->client,
			IO_REG_PWR_DN2_XTAL_HIGH_ADDR);
	val = ADV_REG_GETFIELD(val, IO_PROG_XTAL_FREQ_HIGH);
	if (enable) {
		/* CEC wake up enabled in power-down mode */
		val |= ADV_REG_SETFIELD(1, IO_CTRL_CEC_WAKE_UP_PWRDN2B) |
			ADV_REG_SETFIELD(0, IO_CTRL_CEC_WAKE_UP_PWRDNB);
		ret = adv7481_wr_byte(state->client,
					IO_REG_PWR_DN2_XTAL_HIGH_ADDR, val);
	} else {
		/* CEC wake up disabled in power-down mode */
		val |= ADV_REG_SETFIELD(0, IO_CTRL_CEC_WAKE_UP_PWRDN2B) |
			ADV_REG_SETFIELD(1, IO_CTRL_CEC_WAKE_UP_PWRDNB);
		ret = adv7481_wr_byte(state->client,
					IO_REG_PWR_DN2_XTAL_HIGH_ADDR, val);
	}
	return ret;
}

/* Initialize adv7481 I2C Settings */
static int adv7481_dev_init(struct adv7481_state *state,
						struct i2c_client *client)
{
	int ret;

	mutex_lock(&state->mutex);

	/* Soft reset */
	ret = adv7481_wr_byte(state->client,
			IO_REG_MAIN_RST_ADDR, IO_REG_MAIN_RST_VALUE);
	/* Delay required following I2C reset and I2C transactions */
	usleep_range(I2C_SW_RST_DELAY, I2C_SW_RST_DELAY+1000);

	/* Disable CEC wake up in power-down mode */
	ret |= adv7481_cec_wakeup(state, 0);
	/* Setting Vid_Std to 720x480p60 */
	ret |= adv7481_wr_byte(state->client,
				IO_REG_CP_VID_STD_ADDR, 0x4A);

	/* Configure I2C Maps and I2C Communication Settings */
	/* io_reg_f2 I2C Auto Increment */
	ret |= adv7481_wr_byte(state->client, IO_REG_I2C_CFG_ADDR,
				IO_REG_I2C_AUTOINC_EN_REG_VALUE);
	/* DPLL Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_DPLL_ADDR,
				IO_REG_DPLL_SADDR);
	/* CP Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_CP_ADDR,
				IO_REG_CP_SADDR);
	/* HDMI RX Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_HDMI_ADDR,
				IO_REG_HDMI_SADDR);
	/* EDID Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_EDID_ADDR,
				IO_REG_EDID_SADDR);
	/* HDMI RX Repeater Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_HDMI_REP_ADDR,
				IO_REG_HDMI_REP_SADDR);
	/* HDMI RX Info-frame Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_HDMI_INF_ADDR,
				IO_REG_HDMI_INF_SADDR);
	/* CBUS Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_CBUS_ADDR,
				IO_REG_CBUS_SADDR);
	/* CEC Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_CEC_ADDR,
					IO_REG_CEC_SADDR);
	/* SDP Main Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_SDP_ADDR,
				IO_REG_SDP_SADDR);
	/* CSI-TXB Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_CSI_TXB_ADDR,
				IO_REG_CSI_TXB_SADDR);
	/* CSI-TXA Map Address */
	ret |= adv7481_wr_byte(state->client, IO_REG_CSI_TXA_ADDR,
				IO_REG_CSI_TXA_SADDR);
	if (ret) {
		pr_err("%s: Failed dev init %d\n", __func__, ret);
		goto  err_exit;
	}

	/* Configure i2c clients */
	state->i2c_csi_txa = i2c_new_dummy(client->adapter,
				IO_REG_CSI_TXA_SADDR >> 1);
	state->i2c_csi_txb = i2c_new_dummy(client->adapter,
				IO_REG_CSI_TXB_SADDR >> 1);
	state->i2c_cp = i2c_new_dummy(client->adapter,
				IO_REG_CP_SADDR >> 1);
	state->i2c_hdmi = i2c_new_dummy(client->adapter,
				IO_REG_HDMI_SADDR >> 1);
	state->i2c_edid = i2c_new_dummy(client->adapter,
				IO_REG_EDID_SADDR >> 1);
	state->i2c_sdp = i2c_new_dummy(client->adapter,
				IO_REG_SDP_SADDR >> 1);
	state->i2c_rep = i2c_new_dummy(client->adapter,
				IO_REG_HDMI_REP_SADDR >> 1);

	if (!state->i2c_csi_txa || !state->i2c_csi_txb || !state->i2c_cp ||
		!state->i2c_sdp || !state->i2c_hdmi || !state->i2c_edid ||
		!state->i2c_rep) {
		pr_err("%s: Additional I2C Client Fail\n", __func__);
		ret = -EFAULT;
		goto err_exit;
	}

	ret = adv7481_set_edid(state);
	ret |= adv7481_set_irq(state);

err_exit:
	mutex_unlock(&state->mutex);

	return ret;
}

/* Initialize adv7481 hardware */
static int adv7481_hw_init(struct adv7481_platform_data *pdata,
						struct adv7481_state *state)
{
	int ret = 0;

	if (!pdata) {
		pr_err("%s: PDATA is NULL\n", __func__);
		return -EFAULT;
	}

	mutex_lock(&state->mutex);

	if (gpio_is_valid(pdata->rstb_gpio)) {
		ret = gpio_request(pdata->rstb_gpio, "rstb_gpio");
		if (ret) {
			pr_err("%s: Request GPIO Fail %d\n", __func__, ret);
			goto err_exit;
		}
		ret = gpio_direction_output(pdata->rstb_gpio, 0);
		usleep_range(GPIO_HW_DELAY_LOW, GPIO_HW_DELAY_LOW+1000);
		ret = gpio_direction_output(pdata->rstb_gpio, 1);
		usleep_range(GPIO_HW_DELAY_HI, GPIO_HW_DELAY_HI+1000);
		if (ret) {
			pr_err("%s: Set GPIO Fail %d\n", __func__, ret);
			goto err_exit;
		}
	}

	/* Only setup IRQ1 for now... */
	if (gpio_is_valid(pdata->irq1_gpio)) {
		ret = gpio_request(pdata->irq1_gpio, "irq_gpio");
		if (ret) {
			pr_err("%s: Failed to request irq_gpio %d\n",
					__func__, ret);
			goto err_exit;
		}

		ret = gpio_direction_input(pdata->irq1_gpio);
		if (ret) {
			pr_err("%s: Failed gpio_direction irq %d\n",
					__func__, ret);
			goto err_exit;
		}

		state->irq = gpio_to_irq(pdata->irq1_gpio);
		if (state->irq) {
			ret = request_irq(state->irq, adv7481_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
					DRIVER_NAME, state);
			if (ret) {
				pr_err("%s: Failed request_irq %d\n",
						__func__, ret);
				goto err_exit;
			}
		} else {
			pr_err("%s: Failed gpio_to_irq %d\n", __func__, ret);
			ret = -EINVAL;
			goto err_exit;
		}

		/* disable irq until chip interrupts are programmed */
		disable_irq(state->irq);

		INIT_DELAYED_WORK(&state->irq_delayed_work,
				adv7481_irq_delay_work);
	}

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
		temp = adv7481_rd_byte(state->client, CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(state->client, CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(state->client,
				CP_REG_BRIGHTNESS, ctrl->val);
		break;
	case V4L2_CID_CONTRAST:
		temp = adv7481_rd_byte(state->client, CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(state->client, CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(state->client,
				CP_REG_CONTRAST, ctrl->val);
		break;
	case V4L2_CID_SATURATION:
		temp = adv7481_rd_byte(state->client, CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(state->client, CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(state->client,
				CP_REG_SATURATION, ctrl->val);
		break;
	case V4L2_CID_HUE:
		temp = adv7481_rd_byte(state->client, CP_REG_VID_ADJ);
		temp |= CP_CTR_VID_ADJ_EN;
		ret = adv7481_wr_byte(state->client, CP_REG_VID_ADJ, temp);
		ret |= adv7481_wr_byte(state->client, CP_REG_HUE, ctrl->val);
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

static int adv7481_get_sd_timings(struct adv7481_state *state, int *sd_standard)
{
	int ret = 0;
	int sdp_stat, sdp_stat2;
	int timeout = 0;

	if (sd_standard == NULL)
		return -EINVAL;

	do {
		sdp_stat = adv7481_rd_byte(state->i2c_sdp,
					SDP_RO_MAIN_STATUS1_ADDR);
		usleep_range(SDP_MIN_SLEEP, SDP_MAX_SLEEP);
		timeout++;
		sdp_stat2 = adv7481_rd_byte(state->i2c_sdp,
					SDP_RO_MAIN_STATUS1_ADDR);
	} while ((sdp_stat != sdp_stat2) && (timeout < SDP_NUM_TRIES));

	if (sdp_stat != sdp_stat2) {
		pr_err("%s(%d), adv7481 SDP status unstable: 1\n",
							__func__, __LINE__);
		return -ETIMEDOUT;
	}

	if (!ADV_REG_GETFIELD(sdp_stat, SDP_RO_MAIN_IN_LOCK)) {
		pr_err("%s(%d), adv7481 SD Input NOT Locked: 0x%x\n",
				__func__, __LINE__, sdp_stat);
		return -EBUSY;
	}

	switch (ADV_REG_GETFIELD(sdp_stat, SDP_RO_MAIN_AD_RESULT)) {
	case AD_NTSM_M_J:
		*sd_standard = V4L2_STD_NTSC;
		break;
	case AD_NTSC_4_43:
		*sd_standard = V4L2_STD_NTSC_443;
		break;
	case AD_PAL_M:
		*sd_standard = V4L2_STD_PAL_M;
		break;
	case AD_PAL_60:
		*sd_standard = V4L2_STD_PAL_60;
		break;
	case AD_PAL_B_G:
		*sd_standard = V4L2_STD_PAL;
		break;
	case AD_SECAM:
		*sd_standard = V4L2_STD_SECAM;
		break;
	case AD_PAL_COMB_N:
		*sd_standard = V4L2_STD_PAL_Nc | V4L2_STD_PAL_N;
		break;
	case AD_SECAM_525:
		*sd_standard = V4L2_STD_SECAM;
		break;
	default:
		*sd_standard = V4L2_STD_UNKNOWN;
		break;
	}
	return ret;
}

static int adv7481_set_cvbs_mode(struct adv7481_state *state)
{
	int ret;
	uint8_t val;

	pr_debug("Enter %s\n", __func__);
	state->mode = ADV7481_IP_CVBS_1;
	/* cvbs video settings ntsc etc */
	ret = adv7481_wr_byte(state->client, 0x00, 0x30);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x0f, 0x00);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x00, 0x00);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x03, 0x42);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x04, 0x07);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x13, 0x00);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x17, 0x41);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x31, 0x12);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x52, 0xcd);
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x0e, 0xff);
	val = adv7481_rd_byte(state->client, IO_REG_CSI_PIX_EN_SEL_ADDR);
	/* Output of SD core routed to MIPI CSI 4-lane Tx */
	val |= ADV_REG_SETFIELD(0x10, IO_CTRL_CSI4_IN_SEL);
	ret |= adv7481_wr_byte(state->client, IO_REG_CSI_PIX_EN_SEL_ADDR, val);
	/* Enable autodetect */
	ret |= adv7481_wr_byte(state->i2c_sdp, 0x0e, 0x81);

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
	temp = adv7481_rd_byte(state->client, IO_REG_PWR_DOWN_CTRL_ADDR);
	val = ADV_REG_SETFIELD(1, IO_CTRL_RX_EN) |
				ADV_REG_SETFIELD(0, IO_CTRL_RX_PWDN) |
				ADV_REG_SETFIELD(0, IO_CTRL_XTAL_PWDN) |
				ADV_REG_SETFIELD(0, IO_CTRL_CORE_PWDN) |
				ADV_REG_SETFIELD(0, IO_CTRL_MASTER_PWDN);
	ret = adv7481_wr_byte(state->client, IO_REG_PWR_DOWN_CTRL_ADDR, val);
	/* SDR mode */
	ret |= adv7481_wr_byte(state->client, 0x11, 0x48);
	/* Set CP core to YUV out */
	ret |= adv7481_wr_byte(state->client, 0x04, 0x00);
	/* Set CP core to SDR 422 */
	ret |= adv7481_wr_byte(state->client, 0x12, 0xF2);
	/* Saturate both Luma and Chroma values to 254 */
	ret |= adv7481_wr_byte(state->client, 0x17, 0x80);
	/* Set CP core to enable AV codes */
	ret |= adv7481_wr_byte(state->client, 0x03, 0x86);
	/* ADI RS CP Core: */
	ret |= adv7481_wr_byte(state->i2c_cp, 0x7C, 0x00);
	/* Set CP core Phase Adjustment */
	ret |= adv7481_wr_byte(state->client, 0x0C, 0xE0);
	/* LLC/PIX/SPI PINS TRISTATED AUD Outputs Enabled */
	ret |= adv7481_wr_byte(state->client, IO_PAD_CTRLS_ADDR, 0xDD);
	/* Enable Tx A CSI 4-Lane & data from CP core */
	val = ADV_REG_SETFIELD(1, IO_CTRL_CSI4_EN) |
		ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
		ADV_REG_SETFIELD(0, IO_CTRL_CSI4_IN_SEL);
	ret |= adv7481_wr_byte(state->client, IO_REG_CSI_PIX_EN_SEL_ADDR,
		val);

	/* start to configure HDMI Rx once io-map is configured */
	/* Enable HDCP 1.1 */
	ret |= adv7481_wr_byte(state->i2c_rep, 0x40, 0x83);
	/* Foreground Channel = A */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x00, 0x08);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x98, 0xFF);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x99, 0xA3);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x9A, 0x00);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x9B, 0x0A);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x9D, 0x40);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0xCB, 0x09);
	/* ADI RS */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x3D, 0x10);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x3E, 0x7B);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x3F, 0x5E);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x4E, 0xFE);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x4F, 0x18);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x57, 0xA3);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x58, 0x04);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x85, 0x10);
	/* Enable All Terminations */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x83, 0x00);
	/* ADI RS */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0xA3, 0x01);
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0xBE, 0x00);
	/* HPA Manual Enable */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x6C, 0x01);
	/* HPA Asserted */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0xF8, 0x01);

	/* Audio Mute Speed Set to Fastest (Smallest Step Size) */
	ret |= adv7481_wr_byte(state->i2c_hdmi, 0x0F, 0x00);

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
		temp = adv7481_rd_byte(state->client,
				IO_REG_PWR_DOWN_CTRL_ADDR);
		temp |= val;
		adv7481_wr_byte(state->client, IO_REG_PWR_DOWN_CTRL_ADDR, temp);
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

static u32 ba_inp_to_adv7481(u32 input)
{
	u32 adv_input = ADV7481_IP_HDMI;

	switch (input) {
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

	if (state->mode != adv_input) {
		ret = adv7481_set_ip_mode(state, adv_input);
		if (ret)
			pr_err("%s: Set input mode failed: %d\n",
				__func__, ret);
		else
			state->mode = adv_input;
	}

unlock_exit:
	mutex_unlock(&state->mutex);

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
	temp1 = adv7481_rd_byte(state->i2c_hdmi, HDMI_REG_HDMI_PARAM4_ADDR);
	hdmi_params->pll_lock = ADV_REG_GETFIELD(temp1,
				HDMI_REG_TMDS_PLL_LOCKED);
	if (hdmi_params->pll_lock) {
		temp1 = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_TMDS_FREQ_ADDR);
		temp2 = adv7481_rd_byte(state->i2c_hdmi,
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
		pr_err("%s: PLL not locked return EBUSY\n", __func__);
		return -EBUSY;
	}

	/* Check Timing Lock IO Map Status3:0x71[0] && 0x71[1] && 0x71[7] */
	do {
		temp1 = adv7481_rd_byte(state->client,
				IO_HDMI_LVL_RAW_STATUS_3_ADDR);
		temp2 = adv7481_rd_byte(state->i2c_cp,
				CP_REG_STDI_CH_ADDR);

		if (ADV_REG_GETFIELD(temp1, IO_DE_REGEN_LCK_RAW) &&
			ADV_REG_GETFIELD(temp1, IO_V_LOCKED_RAW) &&
			ADV_REG_GETFIELD(temp1, IO_TMDSPLL_LCK_A_RAW) &&
			ADV_REG_GETFIELD(temp2, CP_STDI_DVALID_CH1))
			break;
		count++;
		usleep_range(LOCK_MIN_SLEEP, LOCK_MAX_SLEEP);
	} while (count < LOCK_NUM_TRIES);

	if (count >= LOCK_NUM_TRIES) {
		pr_err("%s(%d), adv7481 HDMI DE regeneration block NOT Locked: 0x%x",
				__func__, __LINE__, temp1);
	}

	/* Check Timing Lock HDMI Map V:0x07[7], H:0x7[5] */
	do {
		temp1 = adv7481_rd_byte(state->i2c_hdmi,
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
	temp1 = adv7481_rd_byte(state->i2c_hdmi, HDMI_REG_FIELD1_HEIGHT1_ADDR);
	hdmi_params->color_depth = ADV_REG_GETFIELD(temp1,
				HDMI_REG_DEEP_COLOR_MODE);

	/* Check Interlaced and Field Factor */
	vid_params->intrlcd = ADV_REG_GETFIELD(temp1,
				HDMI_REG_HDMI_INTERLACED);
	fieldfactor = (vid_params->intrlcd == 1) ? 2 : 1;

	temp1 = adv7481_rd_byte(state->i2c_hdmi, HDMI_REG_HDMI_PARAM5_ADDR);
	hdmi_params->pix_rep = ADV_REG_GETFIELD(temp1,
				HDMI_REG_PIXEL_REPETITION);

	/* Get Active Timing Data HDMI Map  H:0x07[4:0] + 0x08[7:0] */
	temp1 = adv7481_rd_byte(state->i2c_hdmi, HDMI_REG_LINE_WIDTH_1_ADDR);
	temp2 = adv7481_rd_byte(state->i2c_hdmi, HDMI_REG_LINE_WIDTH_2_ADDR);
	vid_params->act_pix = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_LINE_WIDTH_1) << 8) & 0x1F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_LINE_WIDTH_2));

	/* Get Total Timing Data HDMI Map  H:0x1E[5:0] + 0x1F[7:0] */
	temp1 = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_TOTAL_LINE_WIDTH_1_ADDR);
	temp2 = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_TOTAL_LINE_WIDTH_2_ADDR);
	vid_params->tot_pix = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_TOTAL_LINE_WIDTH_1) << 8) & 0x3F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_TOTAL_LINE_WIDTH_2));

	/* Get Active Timing Data HDMI Map  V:0x09[4:0] + 0x0A[7:0] */
	temp1 = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_FIELD0_HEIGHT_1_ADDR);
	temp2 = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_FIELD0_HEIGHT_2_ADDR);
	vid_params->act_lines = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_FIELD0_HEIGHT_1) << 8) & 0x1F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_FIELD0_HEIGHT_2));

	/* Get Total Timing Data HDMI Map  V:0x26[5:0] + 0x27[7:0] */
	temp1 = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_FIELD0_TOTAL_HEIGHT_1_ADDR);
	temp2 = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_FIELD0_TOTAL_HEIGHT_2_ADDR);
	vid_params->tot_lines = (((ADV_REG_GETFIELD(temp1,
			HDMI_REG_FIELD0_TOT_HEIGHT_1) << 8) & 0x3F00) |
				ADV_REG_GETFIELD(temp2,
					HDMI_REG_FIELD0_TOT_HEIGHT_2));
	vid_params->tot_lines /= 2;

	vid_params->pix_clk = hdmi_params->tmds_freq;

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
		adv7481_get_hdmi_timings(state, &vid_params, &hdmi_params);
		timings->type = V4L2_DV_BT_656_1120;
		bt_timings->width = vid_params.act_pix;
		bt_timings->height = vid_params.act_lines;
		bt_timings->pixelclock = vid_params.pix_clk;
		bt_timings->interlaced = vid_params.intrlcd ?
				V4L2_DV_INTERLACED : V4L2_DV_PROGRESSIVE;
		if (bt_timings->interlaced == V4L2_DV_INTERLACED)
			bt_timings->height /= 2;
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

	pr_debug("Enter %s\n", __func__);
	tStatus = adv7481_rd_byte(state->i2c_sdp, SDP_RO_MAIN_STATUS1_ADDR);
	if (!ADV_REG_GETFIELD(tStatus, SDP_RO_MAIN_IN_LOCK))
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
		ret = adv7481_get_sd_timings(state, &temp);
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

static int adv7481_g_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int ret;
	struct adv7481_vid_params vid_params;
	struct adv7481_hdmi_params hdmi_params;
	struct adv7481_state *state = to_state(sd);

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
		adv7481_get_hdmi_timings(state, &vid_params, &hdmi_params);
		fmt->width = vid_params.act_pix;
		fmt->height = vid_params.act_lines;
		if (vid_params.intrlcd)
			fmt->height /= 2;
		break;
	default:
		return -EINVAL;
	}
	mutex_unlock(&state->mutex);
	fmt->code = V4L2_MBUS_FMT_UYVY8_2X8;
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
		val = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR);
		val |= ADV_REG_SETFIELD(1, HDMI_MUX_SPDIF_TO_I2S_EN);
		ret = adv7481_wr_byte(state->i2c_hdmi,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR, val);
	} else {
		/* Configure I2S_SDATA output pin as an I2S output 0x6E[3] */
		val = adv7481_rd_byte(state->i2c_hdmi,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR);
		val &= ~ADV_REG_SETFIELD(1, HDMI_MUX_SPDIF_TO_I2S_EN);
		ret = adv7481_wr_byte(state->i2c_hdmi,
				HDMI_REG_MUX_SPDIF_TO_I2S_ADDR, val);
	}
	return ret;
}

static int adv7481_csi_powerdown(struct adv7481_state *state,
		enum adv7481_output output)
{
	int ret;
	struct i2c_client *csi_map;
	uint8_t val = 0;

	pr_debug("Enter %s for output: %d\n", __func__, output);
	/* Select CSI TX to configure data */
	if (output == ADV7481_OP_CSIA) {
		csi_map = state->i2c_csi_txa;
	} else if (output == ADV7481_OP_CSIB) {
		csi_map = state->i2c_csi_txb;
	} else if (output == ADV7481_OP_TTL) {
		/* For now use TxA */
		csi_map = state->i2c_csi_txa;
	} else {
		/* Default to TxA */
		csi_map = state->i2c_csi_txa;
	}
	/* CSI Tx: power down DPHY */
	ret = adv7481_wr_byte(csi_map, CSI_REG_TX_DPHY_PWDN_ADDR,
			ADV_REG_SETFIELD(1, CSI_CTRL_DPHY_PWDN));
	/* ADI Required Write */
	ret |= adv7481_wr_byte(csi_map, 0x31, 0x82);
	ret |= adv7481_wr_byte(csi_map, 0x1e, 0x00);
	/* CSI TxA: # Lane : Power Off */
	val = ADV_REG_SETFIELD(1, CSI_CTRL_TX_PWRDN) |
			ADV_REG_SETFIELD(state->tx_lanes, CSI_CTRL_NUM_LANES);
	ret |= adv7481_wr_byte(csi_map, CSI_REG_TX_CFG1_ADDR, val);
	/*
	 * ADI Recommended power down sequence
	 * DPHY and CSI Tx A Power down Sequence
	 * CSI TxA: MIPI PLL DIS
	 */
	ret |= adv7481_wr_byte(csi_map, 0xda, 0x00);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(csi_map, 0xc1, 0x3b);

	pr_debug("Exit %s, ret: %d\n", __func__, ret);

	return ret;
}

static int adv7481_csi_powerup(struct adv7481_state *state,
		enum adv7481_output output)
{
	int ret;
	struct i2c_client *csi_map;
	uint8_t val = 0;
	uint8_t csi_sel = 0;

	pr_debug("Enter %s for output: %d\n", __func__, output);
	/* Select CSI TX to configure data */
	if (output == ADV7481_OP_CSIA) {
		csi_sel = ADV_REG_SETFIELD(1, IO_CTRL_CSI4_EN) |
			ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
			ADV_REG_SETFIELD(0, IO_CTRL_CSI4_IN_SEL);
		csi_map = state->i2c_csi_txa;
	} else if (output == ADV7481_OP_CSIB) {
		/* Enable 1-Lane MIPI Tx, enable pixel output and
		 * route SD through Pixel port
		 */
		csi_sel = ADV_REG_SETFIELD(1, IO_CTRL_CSI1_EN) |
			ADV_REG_SETFIELD(1, IO_CTRL_PIX_OUT_EN) |
			ADV_REG_SETFIELD(1, IO_CTRL_SD_THRU_PIX_OUT) |
			ADV_REG_SETFIELD(0, IO_CTRL_CSI4_IN_SEL);
		csi_map = state->i2c_csi_txb;
	} else if (output == ADV7481_OP_TTL) {
		/* For now use TxA */
		csi_map = state->i2c_csi_txa;
	} else {
		/* Default to TxA */
		csi_map = state->i2c_csi_txa;
	}

	/* Enable Tx A/B CSI #-lane */
	ret = adv7481_wr_byte(state->client,
			IO_REG_CSI_PIX_EN_SEL_ADDR, csi_sel);
	/* TXA MIPI lane settings for CSI */
	/* CSI TxA: # Lane : Power Off */
	val = ADV_REG_SETFIELD(1, CSI_CTRL_TX_PWRDN) |
			ADV_REG_SETFIELD(state->tx_lanes, CSI_CTRL_NUM_LANES);
	ret |= adv7481_wr_byte(csi_map, CSI_REG_TX_CFG1_ADDR, val);
	/* CSI TxA: Auto D-PHY Timing */
	val |= ADV_REG_SETFIELD(1, CSI_CTRL_AUTO_PARAMS);
	ret |= adv7481_wr_byte(csi_map, CSI_REG_TX_CFG1_ADDR, val);

	/* DPHY and CSI Tx A */
	ret |= adv7481_wr_byte(csi_map, 0xdb, 0x10);
	ret |= adv7481_wr_byte(csi_map, 0xd6, 0x07);
	ret |= adv7481_wr_byte(csi_map, 0xc4, 0x0a);
	ret |= adv7481_wr_byte(csi_map, 0x71, 0x33);
	ret |= adv7481_wr_byte(csi_map, 0x72, 0x11);
	/* CSI TxA: power up DPHY */
	ret |= adv7481_wr_byte(csi_map, 0xf0, 0x00);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(csi_map, 0x31, 0x82);
	ret |= adv7481_wr_byte(csi_map, 0x1e, 0x40);
	/* adi Recommended power up sequence */
	/* DPHY and CSI Tx A Power up Sequence */
	/* CSI TxA: MIPI PLL EN */
	ret |= adv7481_wr_byte(csi_map, 0xda, 0x01);
	msleep(200);
	/* CSI TxA: # MIPI Lane : Power ON */
	val = ADV_REG_SETFIELD(0, CSI_CTRL_TX_PWRDN) |
			ADV_REG_SETFIELD(1, CSI_CTRL_AUTO_PARAMS) |
			ADV_REG_SETFIELD(state->tx_lanes, CSI_CTRL_NUM_LANES);
	ret |= adv7481_wr_byte(csi_map, CSI_REG_TX_CFG1_ADDR, val);
	msleep(100);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(csi_map, 0xc1, 0x2b);
	msleep(100);
	/* ADI Required Write */
	ret |= adv7481_wr_byte(csi_map, 0x31, 0x80);

	pr_debug("Exit %s, ret: %d\n", __func__, ret);

	return ret;
}

static int adv7481_set_op_stream(struct adv7481_state *state, bool on)
{
	int ret = 0;

	pr_debug("Enter %s: on: %d, a src: %d, b src: %d\n",
			__func__, on, state->csia_src, state->csib_src);
	if (on && state->csia_src != ADV7481_IP_NONE)
		if (ADV7481_IP_HDMI == state->csia_src) {
			state->tx_lanes = ADV7481_MIPI_2LANE;
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
				state->tx_lanes = ADV7481_MIPI_1LANE;
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
	uint8_t val1 = 0;
	uint8_t val2 = 0;
	uint32_t count = 0;

	pr_debug("Enter %s\n", __func__);
	if (ADV7481_IP_HDMI == state->mode) {
		/*
		 * Check Timing Lock IO Map Status3:0x71[0] &&
		 * 0x71[1] && 0x71[7]
		 */
		do {
			val1 = adv7481_rd_byte(state->client,
					IO_HDMI_LVL_RAW_STATUS_3_ADDR);
			val2 = adv7481_rd_byte(state->i2c_cp,
					CP_REG_STDI_CH_ADDR);

			if (ADV_REG_GETFIELD(val1, IO_DE_REGEN_LCK_RAW) &&
				ADV_REG_GETFIELD(val1, IO_V_LOCKED_RAW) &&
				ADV_REG_GETFIELD(val1, IO_TMDSPLL_LCK_A_RAW) &&
				ADV_REG_GETFIELD(val2, CP_STDI_DVALID_CH1))
				break;
			count++;
			usleep_range(LOCK_MIN_SLEEP, LOCK_MAX_SLEEP);
		} while (count < LOCK_NUM_TRIES);

		if (count >= LOCK_NUM_TRIES) {
			pr_err("%s(%d), HDMI DE regeneration block NOT Locked: 0x%x, 0x%x",
					__func__, __LINE__, val1, val2);
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	} else {
		val1 = adv7481_rd_byte(state->i2c_sdp,
						SDP_RO_MAIN_STATUS1_ADDR);
		if (!ADV_REG_GETFIELD(val1, SDP_RO_MAIN_IN_LOCK)) {
			pr_err("%s(%d), SD Input NOT Locked: 0x%x\n",
					__func__, __LINE__, val1);
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
	.g_mbus_fmt = adv7481_g_mbus_fmt,
	.querystd = adv7481_query_sd_std,
	.g_dv_timings = adv7481_query_dv_timings,
	.g_input_status = adv7481_g_input_status,
	.s_stream = adv7481_s_stream,
};

static const struct v4l2_subdev_core_ops adv7481_core_ops = {
	.s_power = adv7481_s_power,
};

static const struct v4l2_ctrl_ops adv7481_ctrl_ops = {
	.s_ctrl = adv7481_s_ctrl,
};

static const struct v4l2_subdev_ops adv7481_ops = {
	.core = &adv7481_core_ops,
	.video = &adv7481_video_ops,
};

static int adv7481_init_v4l2_controls(struct adv7481_state *state)
{
	v4l2_ctrl_handler_init(&state->ctrl_hdl, 4);

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
		int err = state->ctrl_hdl.error;

		v4l2_ctrl_handler_free(&state->ctrl_hdl);
		return err;
	}
	v4l2_ctrl_handler_setup(&state->ctrl_hdl);

	return 0;
}

static int adv7481_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct adv7481_state *state;
	struct adv7481_platform_data *pdata = NULL;
	struct v4l2_subdev *sd;
	struct v4l2_ctrl_handler *hdl;
	int ret;

	pr_debug("Attempting to probe...\n");
	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s %s Check i2c Functionality Fail\n",
				__func__, client->name);
		ret = -EIO;
		goto err;
	}
	v4l_info(client, "chip found @ 0x%02x (%s)\n",
			 client->addr, client->adapter->name);

	/* Create 7481 State */
	state = devm_kzalloc(&client->dev,
				sizeof(struct adv7481_state), GFP_KERNEL);
	if (state == NULL) {
		ret = -ENOMEM;
		pr_err("Check Kzalloc Fail\n");
		goto err_mem;
	}
	state->client = client;
	mutex_init(&state->mutex);

	/* Get and Check Platform Data */
	pdata = (struct adv7481_platform_data *) client->dev.platform_data;
	if (!pdata) {
		ret = -ENOMEM;
		pr_err("Getting Platform data failed\n");
		goto err_mem;
	}

	/* Configure and Register V4L2 I2C Sub-device */
	sd = &state->sd;
	v4l2_i2c_subdev_init(sd, client, &adv7481_ops);
	state->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;

	/* Register as Media Entity */
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	state->sd.entity.flags |= MEDIA_ENT_T_V4L2_SUBDEV;
	ret = media_entity_init(&state->sd.entity, 1, &state->pad, 0);
	if (ret) {
		ret = -EIO;
		pr_err("Media entity init failed\n");
		goto err_media_entity;
	}

	/* Initialize HW Config */
	ret = adv7481_hw_init(pdata, state);
	if (ret) {
		ret = -EIO;
		pr_err("HW Initialisation Failed\n");
		goto err_media_entity;
	}

	/* Register V4l2 Control Functions */
	hdl = &state->ctrl_hdl;
	v4l2_ctrl_handler_init(hdl, 4);
	adv7481_init_v4l2_controls(state);

	/* Initials ADV7481 State Settings */
	state->tx_auto_params = ADV7481_AUTO_PARAMS;
	state->tx_lanes = ADV7481_MIPI_2LANE;

	/* Initialize SW Init Settings and I2C sub maps 7481 */
	ret = adv7481_dev_init(state, client);
	if (ret) {
		ret = -EIO;
		pr_err("SW Initialisation Failed\n");
		goto err_media_entity;
	}

	/* Set hdmi settings */
	ret = adv7481_set_hdmi_mode(state);

	/* BA registration */
	ret |= msm_ba_register_subdev_node(sd);
	if (ret) {
		ret = -EIO;
		pr_err("BA INIT FAILED\n");
		goto err_media_entity;
	}
	pr_debug("Probe successful!\n");

	return ret;

err_media_entity:
	media_entity_cleanup(&sd->entity);
err_mem:
	kfree(state);
err:
	if (!ret)
		ret = 1;
	return ret;
}

static int adv7481_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adv7481_state *state = to_state(sd);

	msm_ba_unregister_subdev_node(sd);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);

	v4l2_ctrl_handler_free(&state->ctrl_hdl);

	if (state->irq > 0)
		free_irq(state->irq, state);

	i2c_unregister_device(state->i2c_csi_txa);
	i2c_unregister_device(state->i2c_csi_txb);
	i2c_unregister_device(state->i2c_hdmi);
	i2c_unregister_device(state->i2c_edid);
	i2c_unregister_device(state->i2c_cp);
	i2c_unregister_device(state->i2c_sdp);
	i2c_unregister_device(state->i2c_rep);
	mutex_destroy(&state->mutex);
	kfree(state);

	return 0;
}

static const struct i2c_device_id adv7481_id[] = {
	{ DRIVER_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, adv7481_id);


static struct i2c_driver adv7481_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = KBUILD_MODNAME,
	},
	.probe = adv7481_probe,
	.remove = adv7481_remove,
	.id_table = adv7481_id,
};

module_i2c_driver(adv7481_driver);

MODULE_DESCRIPTION("ADI ADV7481 HDMI/MHL/SD video receiver");
