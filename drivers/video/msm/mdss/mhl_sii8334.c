/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/mhl_8334.h>

#include "mdss_fb.h"
#include "mdss_hdmi_tx.h"
#include "mdss_hdmi_edid.h"
#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_io_util.h"

#define MHL_DRIVER_NAME "sii8334"
#define COMPATIBLE_NAME "qcom,mhl-sii8334"

#define pr_debug_intr(...) pr_debug("\n")

enum mhl_gpio_type {
	MHL_TX_RESET_GPIO,
	MHL_TX_INTR_GPIO,
	MHL_TX_PMIC_PWR_GPIO,
	MHL_TX_MAX_GPIO,
};

enum mhl_vreg_type {
	MHL_TX_3V_VREG,
	MHL_TX_MAX_VREG,
};

struct mhl_tx_platform_data {
	/* Data filled from device tree nodes */
	struct dss_gpio *gpios[MHL_TX_MAX_GPIO];
	struct dss_vreg *vregs[MHL_TX_MAX_VREG];
	int irq;
};

struct mhl_tx_ctrl {
	struct platform_device *pdev;
	struct mhl_tx_platform_data *pdata;
	struct i2c_client *i2c_handle;
	uint8_t cur_state;
	uint8_t chip_rev_id;
	int mhl_mode;
	struct completion rgnd_done;
	void (*notify_usb_online)(int online);
	struct usb_ext_notification *mhl_info;
};


uint8_t slave_addrs[MAX_PAGES] = {
	DEV_PAGE_TPI_0    ,
	DEV_PAGE_TX_L0_0  ,
	DEV_PAGE_TX_L1_0  ,
	DEV_PAGE_TX_2_0   ,
	DEV_PAGE_TX_3_0   ,
	DEV_PAGE_CBUS     ,
	DEV_PAGE_DDC_EDID ,
	DEV_PAGE_DDC_SEGM ,
};

static irqreturn_t mhl_tx_isr(int irq, void *dev_id);
static void switch_mode(struct mhl_tx_ctrl *mhl_ctrl,
			enum mhl_st_type to_mode);
static void mhl_drive_hpd(struct mhl_tx_ctrl *mhl_ctrl,
			  uint8_t to_state);

static void mhl_init_reg_settings(struct mhl_tx_ctrl *mhl_ctrl,
	bool mhl_disc_en);

static int mhl_i2c_reg_read(struct i2c_client *client,
			    uint8_t slave_addr_index, uint8_t reg_offset)
{
	int rc = -1;
	uint8_t buffer = 0;

	rc = mdss_i2c_byte_read(client, slave_addrs[slave_addr_index],
				reg_offset, &buffer);
	if (rc) {
		pr_err("%s: slave=%x, off=%x\n",
		       __func__, slave_addrs[slave_addr_index], reg_offset);
		return rc;
	}
	return buffer;
}


static int mhl_i2c_reg_write(struct i2c_client *client,
			     uint8_t slave_addr_index, uint8_t reg_offset,
			     uint8_t value)
{
	return mdss_i2c_byte_write(client, slave_addrs[slave_addr_index],
				 reg_offset, &value);
}

static void mhl_i2c_reg_modify(struct i2c_client *client,
			       uint8_t slave_addr_index, uint8_t reg_offset,
			       uint8_t mask, uint8_t val)
{
	uint8_t temp;

	temp = mhl_i2c_reg_read(client, slave_addr_index, reg_offset);
	temp &= (~mask);
	temp |= (mask & val);
	mhl_i2c_reg_write(client, slave_addr_index, reg_offset, temp);
}


static int mhl_tx_get_dt_data(struct device *dev,
	struct mhl_tx_platform_data *pdata)
{
	int i, rc = 0;
	struct device_node *of_node = NULL;
	struct dss_gpio *temp_gpio = NULL;
	i = 0;

	if (!dev || !pdata) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	of_node = dev->of_node;
	if (!of_node) {
		pr_err("%s: invalid of_node\n", __func__);
		goto error;
	}

	pr_debug("%s: id=%d\n", __func__, dev->id);

	/* GPIOs */
	temp_gpio = NULL;
	temp_gpio = devm_kzalloc(dev, sizeof(struct dss_gpio), GFP_KERNEL);
	pr_debug("%s: gpios allocd\n", __func__);
	if (!(temp_gpio)) {
		pr_err("%s: can't alloc %d gpio mem\n", __func__, i);
		goto error;
	}
	/* RESET */
	temp_gpio->gpio = of_get_named_gpio(of_node, "mhl-rst-gpio", 0);
	snprintf(temp_gpio->gpio_name, 32, "%s", "mhl-rst-gpio");
	pr_debug("%s: rst gpio=[%d]\n", __func__,
		 temp_gpio->gpio);
	pdata->gpios[MHL_TX_RESET_GPIO] = temp_gpio;

	/* PWR */
	temp_gpio = NULL;
	temp_gpio = devm_kzalloc(dev, sizeof(struct dss_gpio), GFP_KERNEL);
	pr_debug("%s: gpios allocd\n", __func__);
	if (!(temp_gpio)) {
		pr_err("%s: can't alloc %d gpio mem\n", __func__, i);
		goto error;
	}
	temp_gpio->gpio = of_get_named_gpio(of_node, "mhl-pwr-gpio", 0);
	snprintf(temp_gpio->gpio_name, 32, "%s", "mhl-pwr-gpio");
	pr_debug("%s: pmic gpio=[%d]\n", __func__,
		 temp_gpio->gpio);
	pdata->gpios[MHL_TX_PMIC_PWR_GPIO] = temp_gpio;

	/* INTR */
	temp_gpio = NULL;
	temp_gpio = devm_kzalloc(dev, sizeof(struct dss_gpio), GFP_KERNEL);
	pr_debug("%s: gpios allocd\n", __func__);
	if (!(temp_gpio)) {
		pr_err("%s: can't alloc %d gpio mem\n", __func__, i);
		goto error;
	}
	temp_gpio->gpio = of_get_named_gpio(of_node, "mhl-intr-gpio", 0);
	snprintf(temp_gpio->gpio_name, 32, "%s", "mhl-intr-gpio");
	pr_debug("%s: intr gpio=[%d]\n", __func__,
		 temp_gpio->gpio);
	pdata->gpios[MHL_TX_INTR_GPIO] = temp_gpio;

	return 0;
error:
	pr_err("%s: ret due to err\n", __func__);
	for (i = 0; i < MHL_TX_MAX_GPIO; i++)
		if (pdata->gpios[i])
			devm_kfree(dev, pdata->gpios[i]);
	return rc;
} /* mhl_tx_get_dt_data */

static int mhl_sii_reset_pin(struct mhl_tx_ctrl *mhl_ctrl, int on)
{
	gpio_set_value(mhl_ctrl->pdata->gpios[MHL_TX_RESET_GPIO]->gpio,
		       on);
	return 0;
}

/*  USB_HANDSHAKING FUNCTIONS */
static int mhl_sii_device_discovery(void *data, int id,
			     void (*usb_notify_cb)(int online))
{
	int timeout, rc;
	struct mhl_tx_ctrl *mhl_ctrl = data;

	if (id) {
		/* When MHL cable is disconnected we get a sii8334
		 * mhl_disconnect interrupt which is handled separately.
		 */
		pr_debug("%s: USB ID pin high\n", __func__);
		return id;
	}

	if (!mhl_ctrl || !usb_notify_cb) {
		pr_warn("%s: cb || ctrl is NULL\n", __func__);
		/* return "USB" so caller can proceed */
		return -EINVAL;
	}

	if (!mhl_ctrl->notify_usb_online)
		mhl_ctrl->notify_usb_online = usb_notify_cb;

	mhl_sii_reset_pin(mhl_ctrl, 0);
	msleep(50);
	mhl_sii_reset_pin(mhl_ctrl, 1);
	/* TX PR-guide requires a 100 ms wait here */

	msleep(100);
	mhl_init_reg_settings(mhl_ctrl, true);

	if (mhl_ctrl->cur_state == POWER_STATE_D3) {
		/* give MHL driver chance to handle RGND interrupt */
		INIT_COMPLETION(mhl_ctrl->rgnd_done);
		timeout = wait_for_completion_interruptible_timeout
			(&mhl_ctrl->rgnd_done, HZ/2);
		if (!timeout) {
			/* most likely nothing plugged in USB */
			/* USB HOST connected or already in USB mode */
			pr_debug("Timedout Returning from discovery mode\n");
			return 0;
		}
		rc = mhl_ctrl->mhl_mode ? 0 : 1;
	} else {
		/* not in D3. already in MHL mode */
		rc = 0;
	}
	return rc;
}

static void cbus_reset(struct i2c_client *client)
{
	uint8_t i;

	/*
	 * REG_SRST
	 */
	MHL_SII_REG_NAME_MOD(REG_SRST, BIT3, BIT3);
	msleep(20);
	MHL_SII_REG_NAME_MOD(REG_SRST, BIT3, 0x00);
	/*
	 * REG_INTR1 and REG_INTR4
	 */
	MHL_SII_REG_NAME_WR(REG_INTR1_MASK, BIT6);
	MHL_SII_REG_NAME_WR(REG_INTR4_MASK,
		BIT0 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6);

	MHL_SII_REG_NAME_WR(REG_INTR5_MASK, 0x00);

	/* Unmask CBUS1 Intrs */
	MHL_SII_CBUS_WR(0x0009,
		BIT2 | BIT3 | BIT4 | BIT5 | BIT6);

	/* Unmask CBUS2 Intrs */
	MHL_SII_CBUS_WR(0x001F, BIT2 | BIT3);

	for (i = 0; i < 4; i++) {
		/*
		 * Enable WRITE_STAT interrupt for writes to
		 * all 4 MSC Status registers.
		 */
		MHL_SII_CBUS_WR((0xE0 + i), 0xFF);

		/*
		 * Enable SET_INT interrupt for writes to
		 * all 4 MSC Interrupt registers.
		 */
		MHL_SII_CBUS_WR((0xF0 + i), 0xFF);
	}
	return;
}

static void init_cbus_regs(struct i2c_client *client)
{
	uint8_t		regval;

	/* Increase DDC translation layer timer*/
	MHL_SII_CBUS_WR(0x0007, 0xF2);
	/* Drive High Time */
	MHL_SII_CBUS_WR(0x0036, 0x03);
	/* Use programmed timing */
	MHL_SII_CBUS_WR(0x0039, 0x30);
	/* CBUS Drive Strength */
	MHL_SII_CBUS_WR(0x0040, 0x03);
	/*
	 * Write initial default settings
	 * to devcap regs: default settings
	 */
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_DEV_STATE, DEVCAP_VAL_DEV_STATE);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_MHL_VERSION, DEVCAP_VAL_MHL_VERSION);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_DEV_CAT, DEVCAP_VAL_DEV_CAT);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_ADOPTER_ID_H, DEVCAP_VAL_ADOPTER_ID_H);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_ADOPTER_ID_L, DEVCAP_VAL_ADOPTER_ID_L);
	MHL_SII_CBUS_WR(0x0080 | DEVCAP_OFFSET_VID_LINK_MODE,
			  DEVCAP_VAL_VID_LINK_MODE);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_AUD_LINK_MODE,
			  DEVCAP_VAL_AUD_LINK_MODE);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_VIDEO_TYPE, DEVCAP_VAL_VIDEO_TYPE);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_LOG_DEV_MAP, DEVCAP_VAL_LOG_DEV_MAP);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_BANDWIDTH, DEVCAP_VAL_BANDWIDTH);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_FEATURE_FLAG, DEVCAP_VAL_FEATURE_FLAG);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_DEVICE_ID_H, DEVCAP_VAL_DEVICE_ID_H);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_DEVICE_ID_L, DEVCAP_VAL_DEVICE_ID_L);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_SCRATCHPAD_SIZE,
			  DEVCAP_VAL_SCRATCHPAD_SIZE);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_INT_STAT_SIZE,
			  DEVCAP_VAL_INT_STAT_SIZE);
	MHL_SII_CBUS_WR(0x0080 |
			  DEVCAP_OFFSET_RESERVED, DEVCAP_VAL_RESERVED);

	/* Make bits 2,3 (initiator timeout) to 1,1
	 * for register CBUS_LINK_CONTROL_2
	 * REG_CBUS_LINK_CONTROL_2
	 */
	regval = MHL_SII_CBUS_RD(0x0031);
	regval = (regval | 0x0C);
	/* REG_CBUS_LINK_CONTROL_2 */
	MHL_SII_CBUS_WR(0x0031, regval);
	 /* REG_MSC_TIMEOUT_LIMIT */
	MHL_SII_CBUS_WR(0x0022, 0x0F);
	/* REG_CBUS_LINK_CONTROL_1 */
	MHL_SII_CBUS_WR(0x0030, 0x01);
	/* disallow vendor specific commands */
	MHL_SII_CBUS_MOD(0x002E, BIT4, BIT4);
}

/*
 * Configure the initial reg settings
 */
static void mhl_init_reg_settings(struct mhl_tx_ctrl *mhl_ctrl,
	bool mhl_disc_en)
{
	/*
	 * ============================================
	 * POWER UP
	 * ============================================
	 */
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* Power up 1.2V core */
	MHL_SII_PAGE1_WR(0x003D, 0x3F);
	/*
	 * Wait for the source power to be enabled
	 * before enabling pll clocks.
	 */
	msleep(50);
	/* Enable Tx PLL Clock */
	MHL_SII_PAGE2_WR(0x0011, 0x01);
	/* Enable Tx Clock Path and Equalizer */
	MHL_SII_PAGE2_WR(0x0012, 0x11);
	/* Tx Source Termination ON */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL1, 0x10);
	/* Enable 1X MHL Clock output */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL6, 0xAC);
	/* Tx Differential Driver Config */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL2, 0x3C);
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL4, 0xD9);
	/* PLL Bandwidth Control */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL8, 0x02);
	/*
	 * ============================================
	 * Analog PLL Control
	 * ============================================
	 */
	/* Enable Rx PLL clock */
	MHL_SII_REG_NAME_WR(REG_TMDS_CCTRL,  0x00);
	MHL_SII_PAGE0_WR(0x00F8, 0x0C);
	MHL_SII_PAGE0_WR(0x0085, 0x02);
	MHL_SII_PAGE2_WR(0x0000, 0x00);
	MHL_SII_PAGE2_WR(0x0013, 0x60);
	/* PLL Cal ref sel */
	MHL_SII_PAGE2_WR(0x0017, 0x03);
	/* VCO Cal */
	MHL_SII_PAGE2_WR(0x001A, 0x20);
	/* Auto EQ */
	MHL_SII_PAGE2_WR(0x0022, 0xE0);
	MHL_SII_PAGE2_WR(0x0023, 0xC0);
	MHL_SII_PAGE2_WR(0x0024, 0xA0);
	MHL_SII_PAGE2_WR(0x0025, 0x80);
	MHL_SII_PAGE2_WR(0x0026, 0x60);
	MHL_SII_PAGE2_WR(0x0027, 0x40);
	MHL_SII_PAGE2_WR(0x0028, 0x20);
	MHL_SII_PAGE2_WR(0x0029, 0x00);
	/* Rx PLL Bandwidth 4MHz */
	MHL_SII_PAGE2_WR(0x0031, 0x0A);
	/* Rx PLL Bandwidth value from I2C */
	MHL_SII_PAGE2_WR(0x0045, 0x06);
	MHL_SII_PAGE2_WR(0x004B, 0x06);
	/* Manual zone control */
	MHL_SII_PAGE2_WR(0x004C, 0xE0);
	/* PLL Mode value */
	MHL_SII_PAGE2_WR(0x004D, 0x00);
	MHL_SII_PAGE0_WR(0x0008, 0x35);
	/*
	 * Discovery Control and Status regs
	 * Setting De-glitch time to 50 ms (default)
	 * Switch Control Disabled
	 */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL2, 0xAD);
	/* 1.8V CBUS VTH */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL5, 0x55);
	/* RGND and single Discovery attempt */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL6, 0x11);
	/* Ignore VBUS */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL8, 0x82);
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL9, 0x24);

	/* Enable CBUS Discovery */
	if (mhl_disc_en) {
		/* Enable MHL Discovery */
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL1, 0x27);
		/* Pull-up resistance off for IDLE state */
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL4, 0x8C);
	} else {
		/* Disable MHL Discovery */
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL1, 0x26);
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL4, 0x8C);
	}

	MHL_SII_REG_NAME_WR(REG_DISC_CTRL7, 0x20);
	/* MHL CBUS Discovery - immediate comm.  */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL3, 0x86);

	MHL_SII_PAGE3_WR(0x3C, 0x80);

	if (mhl_ctrl->cur_state != POWER_STATE_D3)
		MHL_SII_REG_NAME_MOD(REG_INT_CTRL, BIT5 | BIT4, BIT4);

	/* Enable Auto Soft RESET */
	MHL_SII_REG_NAME_WR(REG_SRST, 0x084);
	/* HDMI Transcode mode enable */
	MHL_SII_PAGE0_WR(0x000D, 0x1C);

	cbus_reset(client);
	init_cbus_regs(client);
}


static void switch_mode(struct mhl_tx_ctrl *mhl_ctrl, enum mhl_st_type to_mode)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	switch (to_mode) {
	case POWER_STATE_D0_NO_MHL:
		mhl_ctrl->cur_state = to_mode;
		mhl_init_reg_settings(mhl_ctrl, true);
		/* REG_DISC_CTRL1 */
		MHL_SII_REG_NAME_MOD(REG_DISC_CTRL1, BIT1 | BIT0, BIT0);

		/* TPI_DEVICE_POWER_STATE_CTRL_REG */
		mhl_i2c_reg_modify(client, TX_PAGE_TPI, 0x001E, BIT1 | BIT0,
			0x00);
		break;
	case POWER_STATE_D0_MHL:
		mhl_ctrl->cur_state = to_mode;
		break;
	case POWER_STATE_D3:
		if (mhl_ctrl->cur_state == POWER_STATE_D3)
			break;

		/* Force HPD to 0 when not in MHL mode.  */
		mhl_drive_hpd(mhl_ctrl, HPD_DOWN);
		/*
		 * Change TMDS termination to high impedance
		 * on disconnection.
		 */
		MHL_SII_REG_NAME_WR(REG_MHLTX_CTL1, 0xD0);
		msleep(50);
		MHL_SII_REG_NAME_MOD(REG_DISC_CTRL1, BIT1 | BIT0, 0x00);
		MHL_SII_PAGE3_MOD(0x003D, BIT0, 0x00);
		mhl_ctrl->cur_state = POWER_STATE_D3;
		break;
	default:
		break;
	}
}

static void mhl_drive_hpd(struct mhl_tx_ctrl *mhl_ctrl, uint8_t to_state)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	pr_debug("%s: To state=[0x%x]\n", __func__, to_state);
	if (to_state == HPD_UP) {
		/*
		 * Drive HPD to UP state
		 *
		 * The below two reg configs combined
		 * enable TMDS output.
		 */

		/* Enable TMDS on TMDS_CCTRL */
		MHL_SII_REG_NAME_MOD(REG_TMDS_CCTRL, BIT4, BIT4);

		/*
		 * Set HPD_OUT_OVR_EN = HPD State
		 * EDID read and Un-force HPD (from low)
		 * propogate to src let HPD float by clearing
		 * HPD OUT OVRRD EN
		 */
		MHL_SII_REG_NAME_MOD(REG_INT_CTRL, BIT4, 0x00);
	} else {
		/*
		 * Drive HPD to DOWN state
		 * Disable TMDS Output on REG_TMDS_CCTRL
		 * Enable/Disable TMDS output (MHL TMDS output only)
		 */
		MHL_SII_REG_NAME_MOD(REG_INT_CTRL, BIT4, BIT4);
		MHL_SII_REG_NAME_MOD(REG_TMDS_CCTRL, BIT4, 0x00);
	}
	return;
}

static void mhl_msm_connection(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t val;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	pr_debug("%s: cur st [0x%x]\n", __func__,
		mhl_ctrl->cur_state);

	if (mhl_ctrl->cur_state == POWER_STATE_D0_MHL) {
		/* Already in D0 - MHL power state */
		pr_err("%s: cur st not D0\n", __func__);
		return;
	}
	/* spin_lock_irqsave(&mhl_state_lock, flags); */
	switch_mode(mhl_ctrl, POWER_STATE_D0_MHL);
	/* spin_unlock_irqrestore(&mhl_state_lock, flags); */

	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL1, 0x10);
	MHL_SII_CBUS_WR(0x07, 0xF2);

	/*
	 * Keep the discovery enabled. Need RGND interrupt
	 * Possibly chip disables discovery after MHL_EST??
	 * Need to re-enable here
	 */
	val = MHL_SII_PAGE3_RD(0x10);
	MHL_SII_PAGE3_WR(0x10, val | BIT0);

	return;
}

static void mhl_msm_disconnection(struct mhl_tx_ctrl *mhl_ctrl)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	/*
	 * MHL TX CTL1
	 * Disabling Tx termination
	 */
	MHL_SII_PAGE3_WR(0x30, 0xD0);

	switch_mode(mhl_ctrl, POWER_STATE_D3);
	return;
}

static int  mhl_msm_read_rgnd_int(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t rgnd_imp;
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	/* DISC STATUS REG 2 */
	rgnd_imp = (mhl_i2c_reg_read(client,
				     TX_PAGE_3, 0x001C) & (BIT1 | BIT0));
	pr_debug("imp range read=%02X\n", (int)rgnd_imp);

	if (0x02 == rgnd_imp) {
		pr_debug("%s: mhl sink\n", __func__);
		mhl_ctrl->mhl_mode = 1;
		if (mhl_ctrl->notify_usb_online)
			mhl_ctrl->notify_usb_online(1);
	} else {
		pr_debug("%s: non-mhl sink\n", __func__);
		mhl_ctrl->mhl_mode = 0;
		switch_mode(mhl_ctrl, POWER_STATE_D3);
	}
	complete(&mhl_ctrl->rgnd_done);
	return mhl_ctrl->mhl_mode ?
		MHL_DISCOVERY_RESULT_MHL : MHL_DISCOVERY_RESULT_USB;
}

static void force_usb_switch_open(struct mhl_tx_ctrl *mhl_ctrl)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/*disable discovery*/
	MHL_SII_REG_NAME_MOD(REG_DISC_CTRL1, BIT0, 0);
	/* force USB ID switch to open*/
	MHL_SII_REG_NAME_MOD(REG_DISC_CTRL6, BIT6, BIT6);
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL3, 0x86);
	/* force HPD to 0 when not in mhl mode. */
	MHL_SII_REG_NAME_MOD(REG_INT_CTRL, BIT5 | BIT4, BIT4);
}

static void release_usb_switch_open(struct mhl_tx_ctrl *mhl_ctrl)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	msleep(50);
	MHL_SII_REG_NAME_MOD(REG_DISC_CTRL6, BIT6, 0x00);
	MHL_SII_REG_NAME_MOD(REG_DISC_CTRL1, BIT0, BIT0);
}

static void scdt_st_chg(struct i2c_client *client)
{
	uint8_t tmds_cstat;
	uint8_t mhl_fifo_status;

	/* tmds cstat */
	tmds_cstat = MHL_SII_PAGE3_RD(0x0040);
	pr_debug("%s: tmds cstat: 0x%02x\n", __func__,
		 tmds_cstat);

	if (!(tmds_cstat & BIT1))
		return;

	mhl_fifo_status = MHL_SII_REG_NAME_RD(REG_INTR5);
	pr_debug("%s: mhl fifo st: 0x%02x\n", __func__,
		 mhl_fifo_status);
	if (mhl_fifo_status & 0x0C) {
		MHL_SII_REG_NAME_WR(REG_INTR5,  0x0C);
		pr_debug("%s: mhl fifo rst\n", __func__);
		MHL_SII_REG_NAME_WR(REG_SRST, 0x94);
		MHL_SII_REG_NAME_WR(REG_SRST, 0x84);
	}
}


static void dev_detect_isr(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t status, reg ;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* INTR_STATUS4 */
	status = MHL_SII_REG_NAME_RD(REG_INTR4);
	pr_debug("%s: reg int4 st=%02X\n", __func__, status);

	if ((0x00 == status) &&\
	    (mhl_ctrl->cur_state == POWER_STATE_D3)) {
		pr_err("%s: invalid intr\n", __func__);
		return;
	}

	if (0xFF == status) {
		pr_debug("%s: invalid intr 0xff\n", __func__);
		MHL_SII_REG_NAME_WR(REG_INTR4, status);
		return;
	}

	if ((status & BIT0) && (mhl_ctrl->chip_rev_id < 1)) {
		pr_debug("%s: scdt intr\n", __func__);
		scdt_st_chg(client);
	}

	if (status & BIT1)
		pr_debug("mhl: int4 bit1 set\n");

	/* mhl_est interrupt */
	if (status & BIT2) {
		pr_debug("%s: mhl_est st=%02X\n", __func__,
			 (int) status);
		mhl_msm_connection(mhl_ctrl);
	} else if (status & BIT3) {
		pr_debug("%s: uUSB-a type dev detct\n", __func__);
		/* Short RGND */
		MHL_SII_REG_NAME_MOD(REG_DISC_STAT2, BIT0 | BIT1, 0x00);
		mhl_msm_disconnection(mhl_ctrl);
		if (mhl_ctrl->notify_usb_online)
			mhl_ctrl->notify_usb_online(0);

	}

	if (status & BIT5) {
		/* clr intr - reg int4 */
		pr_debug("%s: mhl discon: int4 st=%02X\n", __func__,
			 (int)status);
		reg = MHL_SII_REG_NAME_RD(REG_INTR4);
		MHL_SII_REG_NAME_WR(REG_INTR4, reg);
		mhl_msm_disconnection(mhl_ctrl);
		if (mhl_ctrl->notify_usb_online)
			mhl_ctrl->notify_usb_online(0);
	}

	if ((mhl_ctrl->cur_state != POWER_STATE_D0_NO_MHL) &&\
	    (status & BIT6)) {
		/* rgnd rdy Intr */
		pr_debug("%s: rgnd ready intr\n", __func__);
		switch_mode(mhl_ctrl, POWER_STATE_D0_NO_MHL);
		mhl_msm_read_rgnd_int(mhl_ctrl);
	}

	/* Can't succeed at these in D3 */
	if ((mhl_ctrl->cur_state != POWER_STATE_D3) &&\
	     (status & BIT4)) {
		/* cbus lockout interrupt?
		 * Hardware detection mechanism figures that
		 * CBUS line is latched and raises this intr
		 * where we force usb switch open and release
		 */
		pr_warn("%s: cbus locked out!\n", __func__);
		force_usb_switch_open(mhl_ctrl);
		release_usb_switch_open(mhl_ctrl);
	}
	MHL_SII_REG_NAME_WR(REG_INTR4, status);

	return;
}

static void mhl_misc_isr(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t intr_5_stat;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/*
	 * Clear INT 5
	 * INTR5 is related to FIFO underflow/overflow reset
	 * which is handled in 8334 by auto FIFO reset
	 */
	intr_5_stat = MHL_SII_REG_NAME_RD(REG_INTR5);
	MHL_SII_REG_NAME_WR(REG_INTR5,  intr_5_stat);
}


static void mhl_hpd_stat_isr(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t intr_1_stat;
	uint8_t cbus_stat;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* INTR STATUS 1 */
	intr_1_stat = MHL_SII_PAGE0_RD(0x0071);

	if (!intr_1_stat)
		return;

	/* Clear interrupts */
	MHL_SII_PAGE0_WR(0x0071, intr_1_stat);
	if (BIT6 & intr_1_stat) {
		/*
		 * HPD status change event is pending
		 * Read CBUS HPD status for this info
		 * MSC REQ ABRT REASON
		 */
		cbus_stat = MHL_SII_CBUS_RD(0x0D);
		if (BIT6 & cbus_stat)
			mhl_drive_hpd(mhl_ctrl, HPD_UP);
	}
	return;
}

static void clear_all_intrs(struct i2c_client *client)
{
	uint8_t regval = 0x00;

	pr_debug_intr("********* exiting isr mask check ?? *************\n");
	pr_debug_intr("int1 mask = %02X\n",
		(int) MHL_SII_REG_NAME_RD(REG_INTR1));
	pr_debug_intr("int3 mask = %02X\n",
		(int) MHL_SII_PAGE0_RD(0x0077));
	pr_debug_intr("int4 mask = %02X\n",
		(int) MHL_SII_REG_NAME_RD(REG_INTR4));
	pr_debug_intr("int5 mask = %02X\n",
		(int) MHL_SII_REG_NAME_RD(REG_INTR5));
	pr_debug_intr("cbus1 mask = %02X\n",
		(int) MHL_SII_CBUS_RD(0x0009));
	pr_debug_intr("cbus2 mask = %02X\n",
		(int) MHL_SII_CBUS_RD(0x001F));
	pr_debug_intr("********* end of isr mask check *************\n");

	regval = MHL_SII_REG_NAME_RD(REG_INTR1);
	pr_debug_intr("int1 st = %02X\n", (int)regval);
	MHL_SII_REG_NAME_WR(REG_INTR1, regval);

	regval =  MHL_SII_REG_NAME_RD(REG_INTR2);
	pr_debug_intr("int2 st = %02X\n", (int)regval);
	MHL_SII_REG_NAME_WR(REG_INTR2, regval);

	regval =  MHL_SII_PAGE0_RD(0x0073);
	pr_debug_intr("int3 st = %02X\n", (int)regval);
	MHL_SII_PAGE0_WR(0x0073, regval);

	regval =  MHL_SII_REG_NAME_RD(REG_INTR4);
	pr_debug_intr("int4 st = %02X\n", (int)regval);
	MHL_SII_REG_NAME_WR(REG_INTR4, regval);

	regval =  MHL_SII_REG_NAME_RD(REG_INTR5);
	pr_debug_intr("int5 st = %02X\n", (int)regval);
	MHL_SII_REG_NAME_WR(REG_INTR5, regval);

	regval =  MHL_SII_CBUS_RD(0x0008);
	pr_debug_intr("cbusInt st = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x0008, regval);

	regval =  MHL_SII_CBUS_RD(0x001E);
	pr_debug_intr("CBUS intR_2: %d\n", (int)regval);
	MHL_SII_CBUS_WR(0x001E, regval);

	regval =  MHL_SII_CBUS_RD(0x00A0);
	pr_debug_intr("A0 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00A0, regval);

	regval =  MHL_SII_CBUS_RD(0x00A1);
	pr_debug_intr("A1 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00A1, regval);

	regval =  MHL_SII_CBUS_RD(0x00A2);
	pr_debug_intr("A2 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00A2, regval);

	regval =  MHL_SII_CBUS_RD(0x00A3);
	pr_debug_intr("A3 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00A3, regval);

	regval =  MHL_SII_CBUS_RD(0x00B0);
	pr_debug_intr("B0 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00B0, regval);

	regval =  MHL_SII_CBUS_RD(0x00B1);
	pr_debug_intr("B1 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00B1, regval);

	regval =  MHL_SII_CBUS_RD(0x00B2);
	pr_debug_intr("B2 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00B2, regval);

	regval =  MHL_SII_CBUS_RD(0x00B3);
	pr_debug_intr("B3 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00B3, regval);

	regval =  MHL_SII_CBUS_RD(0x00E0);
	pr_debug_intr("E0 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00E0, regval);

	regval =  MHL_SII_CBUS_RD(0x00E1);
	pr_debug_intr("E1 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00E1, regval);

	regval =  MHL_SII_CBUS_RD(0x00E2);
	pr_debug_intr("E2 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00E2, regval);

	regval =  MHL_SII_CBUS_RD(0x00E3);
	pr_debug_intr("E3 st set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00E3, regval);

	regval =  MHL_SII_CBUS_RD(0x00F0);
	pr_debug_intr("F0 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00F0, regval);

	regval =  MHL_SII_CBUS_RD(0x00F1);
	pr_debug_intr("F1 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00F1, regval);

	regval =  MHL_SII_CBUS_RD(0x00F2);
	pr_debug_intr("F2 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00F2, regval);

	regval =  MHL_SII_CBUS_RD(0x00F3);
	pr_debug_intr("F3 int set = %02X\n", (int)regval);
	MHL_SII_CBUS_WR(0x00F3, regval);
	pr_debug_intr("********* end of exiting in isr *************\n");
}


static irqreturn_t mhl_tx_isr(int irq, void *data)
{
	struct mhl_tx_ctrl *mhl_ctrl = (struct mhl_tx_ctrl *)data;
	pr_debug("%s: Getting Interrupts\n", __func__);

	/*
	 * Check RGND, MHL_EST, CBUS_LOCKOUT, SCDT
	 * interrupts. In D3, we get only RGND
	 */
	dev_detect_isr(mhl_ctrl);

	pr_debug("%s: cur pwr state is [0x%x]\n",
		 __func__, mhl_ctrl->cur_state);
	if (mhl_ctrl->cur_state == POWER_STATE_D0_MHL) {
		/*
		 * If dev_detect_isr() didn't move the tx to D3
		 * on disconnect, continue to check other
		 * interrupt sources.
		 */
		mhl_misc_isr(mhl_ctrl);

		/*
		 * Check for any peer messages for DCAP_CHG etc
		 * Dispatch to have the CBUS module working only
		 * once connected.
		mhl_cbus_isr(mhl_ctrl);
		 */
		mhl_hpd_stat_isr(mhl_ctrl);
	}

	clear_all_intrs(mhl_ctrl->i2c_handle);

	return IRQ_HANDLED;
}

static int mhl_tx_chip_init(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t chip_rev_id = 0x00;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* Reset the TX chip */
	mhl_sii_reset_pin(mhl_ctrl, 0);
	msleep(20);
	mhl_sii_reset_pin(mhl_ctrl, 1);
	/* TX PR-guide requires a 100 ms wait here */
	msleep(100);

	/* Read the chip rev ID */
	chip_rev_id = MHL_SII_PAGE0_RD(0x04);
	pr_debug("MHL: chip rev ID read=[%x]\n", chip_rev_id);
	mhl_ctrl->chip_rev_id = chip_rev_id;

	/*
	 * Need to disable MHL discovery if
	 * MHL-USB handshake is implemented
	 */
	mhl_init_reg_settings(mhl_ctrl, true);
	switch_mode(mhl_ctrl, POWER_STATE_D3);
	return 0;
}

static int mhl_sii_reg_config(struct i2c_client *client, bool enable)
{
	static struct regulator *reg_8941_l24;
	static struct regulator *reg_8941_l02;
	static struct regulator *reg_8941_smps3a;
	static struct regulator *reg_8941_vdda;
	int rc;

	pr_debug("Inside %s\n", __func__);
	if (!reg_8941_l24) {
		reg_8941_l24 = regulator_get(&client->dev,
			"avcc_18");
		if (IS_ERR(reg_8941_l24)) {
			pr_err("could not get reg_8038_l20, rc = %ld\n",
				PTR_ERR(reg_8941_l24));
			return -ENODEV;
		}
		if (enable)
			rc = regulator_enable(reg_8941_l24);
		else
			rc = regulator_disable(reg_8941_l24);
		if (rc) {
			pr_err("'%s' regulator config[%u] failed, rc=%d\n",
			       "avcc_1.8V", enable, rc);
			return rc;
		} else {
			pr_debug("%s: vreg L24 %s\n",
				 __func__, (enable ? "enabled" : "disabled"));
		}
	}

	if (!reg_8941_l02) {
		reg_8941_l02 = regulator_get(&client->dev,
			"avcc_12");
		if (IS_ERR(reg_8941_l02)) {
			pr_err("could not get reg_8941_l02, rc = %ld\n",
				PTR_ERR(reg_8941_l02));
			return -ENODEV;
		}
		if (enable)
			rc = regulator_enable(reg_8941_l02);
		else
			rc = regulator_disable(reg_8941_l02);
		if (rc) {
			pr_debug("'%s' regulator configure[%u] failed, rc=%d\n",
				 "avcc_1.2V", enable, rc);
			return rc;
		} else {
			pr_debug("%s: vreg L02 %s\n",
				 __func__, (enable ? "enabled" : "disabled"));
		}
	}

	if (!reg_8941_smps3a) {
		reg_8941_smps3a = regulator_get(&client->dev,
			"smps3a");
		if (IS_ERR(reg_8941_smps3a)) {
			pr_err("could not get reg_8038_l20, rc = %ld\n",
				PTR_ERR(reg_8941_smps3a));
			return -ENODEV;
		}
		if (enable)
			rc = regulator_enable(reg_8941_smps3a);
		else
			rc = regulator_disable(reg_8941_smps3a);
		if (rc) {
			pr_err("'%s' regulator config[%u] failed, rc=%d\n",
			       "SMPS3A", enable, rc);
			return rc;
		} else {
			pr_debug("%s: vreg SMPS3A %s\n",
				 __func__, (enable ? "enabled" : "disabled"));
		}
	}

	if (!reg_8941_vdda) {
		reg_8941_vdda = regulator_get(&client->dev,
			"vdda");
		if (IS_ERR(reg_8941_vdda)) {
			pr_err("could not get reg_8038_l20, rc = %ld\n",
				PTR_ERR(reg_8941_vdda));
			return -ENODEV;
		}
		if (enable)
			rc = regulator_enable(reg_8941_vdda);
		else
			rc = regulator_disable(reg_8941_vdda);
		if (rc) {
			pr_err("'%s' regulator config[%u] failed, rc=%d\n",
			       "VDDA", enable, rc);
			return rc;
		} else {
			pr_debug("%s: vreg VDDA %s\n",
				 __func__, (enable ? "enabled" : "disabled"));
		}
	}

	return rc;
}


static int mhl_vreg_config(struct mhl_tx_ctrl *mhl_ctrl, uint8_t on)
{
	int ret;
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	int pwr_gpio = mhl_ctrl->pdata->gpios[MHL_TX_PMIC_PWR_GPIO]->gpio;

	pr_debug("%s\n", __func__);
	if (on) {
		ret = gpio_request(pwr_gpio,
		    mhl_ctrl->pdata->gpios[MHL_TX_PMIC_PWR_GPIO]->gpio_name);
		if (ret < 0) {
			pr_err("%s: mhl pwr gpio req failed: %d\n",
			       __func__, ret);
			return ret;
		}
		ret = gpio_direction_output(pwr_gpio, 1);
		if (ret < 0) {
			pr_err("%s: set gpio MHL_PWR_EN dircn failed: %d\n",
			       __func__, ret);
			return ret;
		}

		ret = mhl_sii_reg_config(client, true);
		if (ret) {
			pr_err("%s: regulator enable failed\n", __func__);
			return -EINVAL;
		}
		pr_debug("%s: mhl sii power on successful\n", __func__);
	} else {
		pr_warn("%s: turning off pwr controls\n", __func__);
		mhl_sii_reg_config(client, false);
		gpio_free(pwr_gpio);
	}
	pr_debug("%s: successful\n", __func__);
	return 0;
}

/*
 * Request for GPIO allocations
 * Set appropriate GPIO directions
 */
static int mhl_gpio_config(struct mhl_tx_ctrl *mhl_ctrl, int on)
{
	int ret;
	struct dss_gpio *temp_reset_gpio, *temp_intr_gpio;

	/* caused too many line spills */
	temp_reset_gpio = mhl_ctrl->pdata->gpios[MHL_TX_RESET_GPIO];
	temp_intr_gpio = mhl_ctrl->pdata->gpios[MHL_TX_INTR_GPIO];

	if (on) {
		if (gpio_is_valid(temp_reset_gpio->gpio)) {
			ret = gpio_request(temp_reset_gpio->gpio,
					   temp_reset_gpio->gpio_name);
			if (ret < 0) {
				pr_err("%s:rst_gpio=[%d] req failed:%d\n",
				       __func__, temp_reset_gpio->gpio, ret);
				return -EBUSY;
			}
			ret = gpio_direction_output(temp_reset_gpio->gpio, 0);
			if (ret < 0) {
				pr_err("%s: set dirn rst failed: %d\n",
				       __func__, ret);
				return -EBUSY;
			}
		}
		if (gpio_is_valid(temp_intr_gpio->gpio)) {
			ret = gpio_request(temp_intr_gpio->gpio,
					   temp_intr_gpio->gpio_name);
			if (ret < 0) {
				pr_err("%s: intr_gpio req failed: %d\n",
				       __func__, ret);
				return -EBUSY;
			}
			ret = gpio_direction_input(temp_intr_gpio->gpio);
			if (ret < 0) {
				pr_err("%s: set dirn intr failed: %d\n",
				       __func__, ret);
				return -EBUSY;
			}
			mhl_ctrl->i2c_handle->irq = gpio_to_irq(
				temp_intr_gpio->gpio);
			pr_debug("%s: gpio_to_irq=%d\n",
				 __func__, mhl_ctrl->i2c_handle->irq);
		}
	} else {
		pr_warn("%s: freeing gpios\n", __func__);
		gpio_free(temp_intr_gpio->gpio);
		gpio_free(temp_reset_gpio->gpio);
	}
	pr_debug("%s: successful\n", __func__);
	return 0;
}

static int mhl_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int rc = 0;
	struct mhl_tx_platform_data *pdata = NULL;
	struct mhl_tx_ctrl *mhl_ctrl;
	struct usb_ext_notification *mhl_info = NULL;

	mhl_ctrl = devm_kzalloc(&client->dev, sizeof(*mhl_ctrl), GFP_KERNEL);
	if (!mhl_ctrl) {
		pr_err("%s: FAILED: cannot alloc hdmi tx ctrl\n", __func__);
		rc = -ENOMEM;
		goto failed_no_mem;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			     sizeof(struct mhl_tx_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			rc = -ENOMEM;
			goto failed_no_mem;
		}

		rc = mhl_tx_get_dt_data(&client->dev, pdata);
		if (rc) {
			pr_err("%s: FAILED: parsing device tree data; rc=%d\n",
				__func__, rc);
			goto failed_dt_data;
		}
		mhl_ctrl->i2c_handle = client;
		mhl_ctrl->pdata = pdata;
		i2c_set_clientdata(client, mhl_ctrl);
	}

	/*
	 * Regulator init
	 */
	rc = mhl_vreg_config(mhl_ctrl, 1);
	if (rc) {
		pr_err("%s: vreg init failed [%d]\n",
			__func__, rc);
		goto failed_probe;
	}

	/*
	 * GPIO init
	 */
	rc = mhl_gpio_config(mhl_ctrl, 1);
	if (rc) {
		pr_err("%s: gpio init failed [%d]\n",
			__func__, rc);
		goto failed_probe;
	}

	/*
	 * Other initializations
	 * such tx specific
	 */
	rc = mhl_tx_chip_init(mhl_ctrl);
	if (rc) {
		pr_err("%s: tx chip init failed [%d]\n",
			__func__, rc);
		goto failed_probe;
	}

	init_completion(&mhl_ctrl->rgnd_done);

	pr_debug("%s: IRQ from GPIO INTR = %d\n",
		__func__, mhl_ctrl->i2c_handle->irq);
	pr_debug("%s: Driver name = [%s]\n", __func__,
		client->dev.driver->name);
	rc = request_threaded_irq(mhl_ctrl->i2c_handle->irq, NULL,
				   &mhl_tx_isr,
				  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				 client->dev.driver->name, mhl_ctrl);
	if (rc) {
		pr_err("request_threaded_irq failed, status: %d\n",
			rc);
		goto failed_probe;
	} else {
		pr_debug("request_threaded_irq succeeded\n");
	}
	pr_debug("%s: i2c client addr is [%x]\n", __func__, client->addr);

	mhl_info = devm_kzalloc(&client->dev, sizeof(*mhl_info), GFP_KERNEL);
	if (!mhl_info) {
		pr_err("%s: alloc mhl info failed\n", __func__);
		goto failed_probe;
	}

	mhl_info->ctxt = mhl_ctrl;
	mhl_info->notify = mhl_sii_device_discovery;
	if (msm_register_usb_ext_notification(mhl_info)) {
		pr_err("%s: register for usb notifcn failed\n", __func__);
		goto failed_probe;
	}
	mhl_ctrl->mhl_info = mhl_info;
	return 0;
failed_probe:
	/* do not deep-free */
	if (mhl_info)
		devm_kfree(&client->dev, mhl_info);
failed_dt_data:
	if (pdata)
		devm_kfree(&client->dev, pdata);
failed_no_mem:
	if (mhl_ctrl)
		devm_kfree(&client->dev, mhl_ctrl);
	pr_err("%s: PROBE FAILED, rc=%d\n", __func__, rc);
	return rc;
}


static int mhl_i2c_remove(struct i2c_client *client)
{
	struct mhl_tx_ctrl *mhl_ctrl = i2c_get_clientdata(client);

	if (!mhl_ctrl) {
		pr_warn("%s: i2c get client data failed\n", __func__);
		return -EINVAL;
	}

	free_irq(mhl_ctrl->i2c_handle->irq, mhl_ctrl);
	mhl_gpio_config(mhl_ctrl, 0);
	mhl_vreg_config(mhl_ctrl, 0);
	if (mhl_ctrl->mhl_info)
		devm_kfree(&client->dev, mhl_ctrl->mhl_info);
	if (mhl_ctrl->pdata)
		devm_kfree(&client->dev, mhl_ctrl->pdata);
	devm_kfree(&client->dev, mhl_ctrl);
	return 0;
}

static struct i2c_device_id mhl_sii_i2c_id[] = {
	{ MHL_DRIVER_NAME, 0 },
	{ }
};


MODULE_DEVICE_TABLE(i2c, mhl_sii_i2c_id);

static struct of_device_id mhl_match_table[] = {
	{.compatible = COMPATIBLE_NAME,},
	{ },
};

static struct i2c_driver mhl_sii_i2c_driver = {
	.driver = {
		.name = MHL_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mhl_match_table,
	},
	.probe = mhl_i2c_probe,
	.remove =  mhl_i2c_remove,
	.id_table = mhl_sii_i2c_id,
};

module_i2c_driver(mhl_sii_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MHL SII 8334 TX Driver");
