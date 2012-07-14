/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <mach/msm_hdmi_audio.h>
#include <mach/clk.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "msm_fb.h"
#include "external_common.h"
#include "hdmi_msm.h"
#include "mhl_8334.h"
#include "mhl_i2c_utils.h"


static struct i2c_device_id mhl_sii_i2c_id[] = {
	{ MHL_DRIVER_NAME, 0 },
	{ }
};

struct mhl_msm_state_t *mhl_msm_state;
spinlock_t mhl_state_lock;

static int mhl_i2c_probe(struct i2c_client *client,\
	const struct i2c_device_id *id);
static int mhl_i2c_remove(struct i2c_client *client);
static void force_usb_switch_open(void);
static void release_usb_switch_open(void);
static void switch_mode(enum mhl_st_type to_mode);
static irqreturn_t mhl_tx_isr(int irq, void *dev_id);

static struct i2c_driver mhl_sii_i2c_driver = {
	.driver = {
		.name = MHL_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mhl_i2c_probe,
	/*.remove =  __exit_p(mhl_i2c_remove),*/
	.remove =  mhl_i2c_remove,
	.id_table = mhl_sii_i2c_id,
};

static void mhl_sii_reset_pin(int on)
{
	gpio_set_value(mhl_msm_state->mhl_data->gpio_mhl_reset, on);
	return;
}

static int mhl_sii_reg_enable(void)
{
	static struct regulator *reg_8038_l20;
	static struct regulator *reg_8038_l11;
	int rc;

	pr_debug("Inside %s\n", __func__);
	if (!reg_8038_l20) {
		reg_8038_l20 = regulator_get(&mhl_msm_state->i2c_client->dev,
			"mhl_avcc12");
		if (IS_ERR(reg_8038_l20)) {
			pr_err("could not get reg_8038_l20, rc = %ld\n",
				PTR_ERR(reg_8038_l20));
			return -ENODEV;
		}
		rc = regulator_enable(reg_8038_l20);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"mhl_l20", rc);
			return rc;
		} else
		       pr_debug("REGULATOR L20 ENABLED\n");
	}

	if (!reg_8038_l11) {
		reg_8038_l11 = regulator_get(&mhl_msm_state->i2c_client->dev,
			"mhl_iovcc18");
		if (IS_ERR(reg_8038_l11)) {
			pr_err("could not get reg_8038_l11, rc = %ld\n",
				PTR_ERR(reg_8038_l11));
			return -ENODEV;
		}
		rc = regulator_enable(reg_8038_l11);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"mhl_l11", rc);
			return rc;
		} else
			pr_debug("REGULATOR L11 ENABLED\n");
	}

	return rc;
}


static void mhl_sii_power_on(void)
{
	int ret;
	pr_debug("MHL SII POWER ON\n");
	if (!mhl_msm_state->mhl_data->gpio_mhl_power) {
		pr_warn("%s: no power reqd for this platform\n", __func__);
		return;
	}

	ret = gpio_request(mhl_msm_state->mhl_data->gpio_mhl_power, "W_PWR");
	if (ret < 0) {
		pr_err("MHL_POWER_GPIO req failed: %d\n",
			ret);
		return;
	}
	ret = gpio_direction_output(mhl_msm_state->mhl_data->gpio_mhl_power,
		1);
	if (ret < 0) {
		pr_err(
		"SET GPIO MHL_POWER_GPIO direction failed: %d\n",
			ret);
		gpio_free(mhl_msm_state->mhl_data->gpio_mhl_power);
		return;
	}
	gpio_set_value(mhl_msm_state->mhl_data->gpio_mhl_power, 1);

	if (mhl_sii_reg_enable())
		pr_err("Regulator enable failed\n");

	pr_debug("MHL SII POWER ON Successful\n");
	return;
}

/*
 * Request for GPIO allocations
 * Set appropriate GPIO directions
 */
static int mhl_sii_gpio_setup(int on)
{
	int ret;
	if (on) {
		if (mhl_msm_state->mhl_data->gpio_hdmi_mhl_mux) {
			ret = gpio_request(mhl_msm_state->\
				mhl_data->gpio_hdmi_mhl_mux, "W_MUX");
			if (ret < 0) {
				pr_err("GPIO HDMI_MHL MUX req failed:%d\n",
					ret);
				return -EBUSY;
			}
			ret = gpio_direction_output(
				mhl_msm_state->mhl_data->gpio_hdmi_mhl_mux, 0);
			if (ret < 0) {
				pr_err("SET GPIO HDMI_MHL dir failed:%d\n",
					ret);
				gpio_free(mhl_msm_state->\
					mhl_data->gpio_hdmi_mhl_mux);
				return -EBUSY;
			}
			msleep(50);
			gpio_set_value(mhl_msm_state->\
				mhl_data->gpio_hdmi_mhl_mux, 0);
			pr_debug("SET GPIO HDMI MHL MUX %d to 0\n",
				mhl_msm_state->mhl_data->gpio_hdmi_mhl_mux);
		}

		ret = gpio_request(mhl_msm_state->mhl_data->gpio_mhl_reset,
			"W_RST#");
		if (ret < 0) {
			pr_err("GPIO RESET request failed: %d\n", ret);
			return -EBUSY;
		}
		ret = gpio_direction_output(mhl_msm_state->\
			mhl_data->gpio_mhl_reset, 1);
		if (ret < 0) {
			pr_err("SET GPIO RESET direction failed: %d\n", ret);
			gpio_free(mhl_msm_state->mhl_data->gpio_mhl_reset);
			gpio_free(mhl_msm_state->mhl_data->gpio_hdmi_mhl_mux);
			return -EBUSY;
		}
		ret = gpio_request(mhl_msm_state->mhl_data->gpio_mhl_int,
			"W_INT");
		if (ret < 0) {
			pr_err("GPIO INT request failed: %d\n", ret);
			gpio_free(mhl_msm_state->mhl_data->gpio_mhl_reset);
			gpio_free(mhl_msm_state->mhl_data->gpio_hdmi_mhl_mux);
			return -EBUSY;
		}
		ret = gpio_direction_input(mhl_msm_state->\
			mhl_data->gpio_mhl_int);
		if (ret < 0) {
			pr_err("SET GPIO INTR direction failed: %d\n", ret);
			gpio_free(mhl_msm_state->mhl_data->gpio_mhl_reset);
			gpio_free(mhl_msm_state->mhl_data->gpio_mhl_int);
			gpio_free(mhl_msm_state->mhl_data->gpio_hdmi_mhl_mux);
			return -EBUSY;
		}
	} else {
		gpio_free(mhl_msm_state->mhl_data->gpio_mhl_reset);
		gpio_free(mhl_msm_state->mhl_data->gpio_mhl_int);
		gpio_free(mhl_msm_state->mhl_data->gpio_hdmi_mhl_mux);
		gpio_free(mhl_msm_state->mhl_data->gpio_mhl_power);
	}

	return 0;
}

bool mhl_is_connected(void)
{
	return true;
}

static void cbus_reset(void)
{
	uint8_t i;

	/*
	 * REG_SRST
	 */
	mhl_i2c_reg_modify(TX_PAGE_3, 0x0000, BIT3, BIT3);
	msleep(20);
	mhl_i2c_reg_modify(TX_PAGE_3, 0x0000, BIT3, 0x00);
	/*
	 * REG_INTR1 and REG_INTR4
	 */
	mhl_i2c_reg_write(TX_PAGE_L0, 0x0075, BIT6 | BIT5);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0022,
		BIT0 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6);
	/* REG5 */
	if (mhl_msm_state->chip_rev_id < 1)
		mhl_i2c_reg_write(TX_PAGE_3, 0x0024, BIT3 | BIT4);
	else
		/*REG5 Mask disabled due to auto FIFO reset ??*/
		mhl_i2c_reg_write(TX_PAGE_3, 0x0024, 0x00);

	/* Unmask CBUS1 Intrs */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0009,
		BIT2 | BIT3 | BIT4 | BIT5 | BIT6);

	/* Unmask CBUS2 Intrs */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x001F, BIT2 | BIT3);

	for (i = 0; i < 4; i++) {
		/*
		 * Enable WRITE_STAT interrupt for writes to
		 * all 4 MSC Status registers.
		 */
		mhl_i2c_reg_write(TX_PAGE_CBUS, (0xE0 + i), 0xFF);

		/*
		 * Enable SET_INT interrupt for writes to
		 * all 4 MSC Interrupt registers.
		 */
		mhl_i2c_reg_write(TX_PAGE_CBUS, (0xF0 + i), 0xFF);
	}
}

static void init_cbus_regs(void)
{
	uint8_t		regval;

	/* Increase DDC translation layer timer*/
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0007, 0xF2);
	/* Drive High Time */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0036, 0x03);
	/* Use programmed timing */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0039, 0x30);
	/* CBUS Drive Strength */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0040, 0x03);
	/*
	 * Write initial default settings
	 * to devcap regs: default settings
	 */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_DEV_STATE,
		DEVCAP_VAL_DEV_STATE);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_MHL_VERSION,
		DEVCAP_VAL_MHL_VERSION);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_DEV_CAT,
		DEVCAP_VAL_DEV_CAT);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_ADOPTER_ID_H,
		DEVCAP_VAL_ADOPTER_ID_H);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_ADOPTER_ID_L,
		DEVCAP_VAL_ADOPTER_ID_L);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_VID_LINK_MODE,
		DEVCAP_VAL_VID_LINK_MODE);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_AUD_LINK_MODE,
		DEVCAP_VAL_AUD_LINK_MODE);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_VIDEO_TYPE,
		DEVCAP_VAL_VIDEO_TYPE);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_LOG_DEV_MAP,
		DEVCAP_VAL_LOG_DEV_MAP);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_BANDWIDTH,
		DEVCAP_VAL_BANDWIDTH);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_FEATURE_FLAG,
		DEVCAP_VAL_FEATURE_FLAG);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_DEVICE_ID_H,
		DEVCAP_VAL_DEVICE_ID_H);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_DEVICE_ID_L,
		DEVCAP_VAL_DEVICE_ID_L);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_SCRATCHPAD_SIZE,
		DEVCAP_VAL_SCRATCHPAD_SIZE);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_INT_STAT_SIZE,
		DEVCAP_VAL_INT_STAT_SIZE);
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0080 | DEVCAP_OFFSET_RESERVED,
		DEVCAP_VAL_RESERVED);

	/* Make bits 2,3 (initiator timeout) to 1,1
	 * for register CBUS_LINK_CONTROL_2
	 * REG_CBUS_LINK_CONTROL_2
	 */
	regval = mhl_i2c_reg_read(TX_PAGE_CBUS, 0x0031);
	regval = (regval | 0x0C);
	/* REG_CBUS_LINK_CONTROL_2 */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0031, regval);
	 /* REG_MSC_TIMEOUT_LIMIT */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0022, 0x0F);
	/* REG_CBUS_LINK_CONTROL_1 */
	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x0030, 0x01);
	/* disallow vendor specific commands */
	mhl_i2c_reg_modify(TX_PAGE_CBUS, 0x002E, BIT4, BIT4);
}

/*
 * Configure the initial reg settings
 */
static void mhl_init_reg_settings(void)
{

	/*
	 * ============================================
	 * POWER UP
	 * ============================================
	 */

	/* Power up 1.2V core */
	mhl_i2c_reg_write(TX_PAGE_L1, 0x003D, 0x3F);
	/*
	 * Wait for the source power to be enabled
	 * before enabling pll clocks.
	 */
	msleep(50);
	/* Enable Tx PLL Clock */
	mhl_i2c_reg_write(TX_PAGE_2, 0x0011, 0x01);
	/* Enable Tx Clock Path and Equalizer */
	mhl_i2c_reg_write(TX_PAGE_2, 0x0012, 0x11);
	/* Tx Source Termination ON */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0030, 0x10);
	/* Enable 1X MHL Clock output */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0035, 0xAC);
	/* Tx Differential Driver Config */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0031, 0x3C);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0033, 0xD9);
	/* PLL Bandwidth Control */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0037, 0x02);
	/*
	 * ============================================
	 * Analog PLL Control
	 * ============================================
	 */
	/* Enable Rx PLL clock */
	mhl_i2c_reg_write(TX_PAGE_L0, 0x0080, 0x00);
	mhl_i2c_reg_write(TX_PAGE_L0, 0x00F8, 0x0C);
	mhl_i2c_reg_write(TX_PAGE_L0, 0x0085, 0x02);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0000, 0x00);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0013, 0x60);
	/* PLL Cal ref sel */
	mhl_i2c_reg_write(TX_PAGE_2, 0x0017, 0x03);
	/* VCO Cal */
	mhl_i2c_reg_write(TX_PAGE_2, 0x001A, 0x20);
	/* Auto EQ */
	mhl_i2c_reg_write(TX_PAGE_2, 0x0022, 0xE0);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0023, 0xC0);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0024, 0xA0);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0025, 0x80);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0026, 0x60);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0027, 0x40);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0028, 0x20);
	mhl_i2c_reg_write(TX_PAGE_2, 0x0029, 0x00);
	/* Rx PLL Bandwidth 4MHz */
	mhl_i2c_reg_write(TX_PAGE_2, 0x0031, 0x0A);
	/* Rx PLL Bandwidth value from I2C */
	mhl_i2c_reg_write(TX_PAGE_2, 0x0045, 0x06);
	mhl_i2c_reg_write(TX_PAGE_2, 0x004B, 0x06);
	/* Manual zone control */
	mhl_i2c_reg_write(TX_PAGE_2, 0x004C, 0xE0);
	/* PLL Mode value */
	mhl_i2c_reg_write(TX_PAGE_2, 0x004D, 0x00);
	mhl_i2c_reg_write(TX_PAGE_L0, 0x0008, 0x35);
	/*
	 * Discovery Control and Status regs
	 * Setting De-glitch time to 50 ms (default)
	 * Switch Control Disabled
	 */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0011, 0xAD);
	/* 1.8V CBUS VTH */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0014, 0x55);
	/* RGND and single Discovery attempt */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0015, 0x11);
	/* Ignore VBUS */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0017, 0x82);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0018, 0x24);
	/* Pull-up resistance off for IDLE state */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0013, 0x8C);
	/* Enable CBUS Discovery */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0010, 0x27);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0016, 0x20);
	/* MHL CBUS Discovery - immediate comm.  */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0012, 0x86);
	/* Do not force HPD to 0 during wake-up from D3 */
	if (mhl_msm_state->cur_state != POWER_STATE_D3) {
		mhl_i2c_reg_modify(TX_PAGE_3, 0x0020,
			       BIT5 | BIT4, BIT4);
	}
	/* Enable Auto Soft RESET */
	mhl_i2c_reg_write(TX_PAGE_3, 0x0000, 0x084);
	/* HDMI Transcode mode enable */
	mhl_i2c_reg_write(TX_PAGE_L0, 0x000D, 0x1C);

	cbus_reset();
	init_cbus_regs();
}

static int mhl_chip_init(void)
{
	/* Read the chip rev ID */
	mhl_msm_state->chip_rev_id = mhl_i2c_reg_read(TX_PAGE_L0, 0x04);
	pr_debug("MHL: chip rev ID read=[%x]\n", mhl_msm_state->chip_rev_id);

	/* Reset the TX chip */
	mhl_sii_reset_pin(1);
	msleep(20);
	mhl_sii_reset_pin(0);
	msleep(20);
	mhl_sii_reset_pin(1);
	/* MHL spec requires a 100 ms wait here.  */
	msleep(100);

	mhl_init_reg_settings();

	/*
	 * Power down the chip to the
	 * D3 - a low power standby mode
	 * cable impedance measurement logic is operational
	 */
	switch_mode(POWER_STATE_D3);
	return 0;
}

/*
 * I2C probe
 */
static int mhl_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	mhl_msm_state->mhl_data = kzalloc(sizeof(struct msm_mhl_platform_data),
		GFP_KERNEL);
	if (!(mhl_msm_state->mhl_data)) {
		ret = -ENOMEM;
		pr_err("MHL I2C Probe failed - no mem\n");
		goto probe_exit;
	}
	mhl_msm_state->i2c_client = client;

	spin_lock_init(&mhl_state_lock);

	i2c_set_clientdata(client, mhl_msm_state);
	mhl_msm_state->mhl_data = client->dev.platform_data;
	pr_debug("MHL: mhl_msm_state->mhl_data->irq=[%d]\n",
		mhl_msm_state->mhl_data->irq);

	/* Init GPIO stuff here */
	ret = mhl_sii_gpio_setup(1);
	if (ret == -1) {
		pr_err("MHL: mhl_gpio_init has failed\n");
		ret = -ENODEV;
		goto probe_exit;
	}

	mhl_sii_power_on();

	pr_debug("I2C PROBE successful\n");
	return 0;

probe_exit:
	if (mhl_msm_state->mhl_data) {
		/* free the gpios */
		mhl_sii_gpio_setup(0);
		kfree(mhl_msm_state->mhl_data);
		mhl_msm_state->mhl_data = NULL;
	}
	return ret;
}

static int mhl_i2c_remove(struct i2c_client *client)
{
	pr_debug("%s\n", __func__);
	mhl_sii_gpio_setup(0);
	kfree(mhl_msm_state->mhl_data);
	return 0;
}

static int __init mhl_msm_init(void)
{
	int32_t     ret;

	pr_debug("%s\n", __func__);
	mhl_msm_state = kzalloc(sizeof(struct mhl_msm_state_t), GFP_KERNEL);
	if (!mhl_msm_state) {
		pr_err("mhl_msm_init FAILED: out of memory\n");
		ret = -ENOMEM;
		goto init_exit;
	}

	mhl_msm_state->i2c_client = NULL;
	ret = i2c_add_driver(&mhl_sii_i2c_driver);
	if (ret) {
		pr_err("MHL: I2C driver add failed: %d\n", ret);
		ret = -ENODEV;
		goto init_exit;
	} else {
		if (mhl_msm_state->i2c_client == NULL) {
			pr_err("MHL: I2C driver add failed\n");
			ret = -ENODEV;
			goto init_exit;
		}
		pr_info("MHL: I2C driver added\n");
	}

	/* Request IRQ stuff here */
	pr_debug("MHL: mhl_msm_state->mhl_data->irq=[%d]\n",
		mhl_msm_state->mhl_data->irq);
	ret = request_threaded_irq(mhl_msm_state->mhl_data->irq, NULL,
				   &mhl_tx_isr,
				 IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				 "mhl_tx_isr", mhl_msm_state);
	if (ret != 0) {
		pr_err("request_threaded_irq failed, status: %d\n",
			ret);
		ret = -EACCES; /* Error code???? */
		goto init_exit;
	} else
		pr_debug("request_threaded_irq succeeded\n");

	mhl_msm_state->cur_state = POWER_STATE_D0_MHL;

	/* MHL SII 8334 chip specific init */
	mhl_chip_init();
	return 0;

init_exit:
	pr_err("Exiting from the init with err\n");
	i2c_del_driver(&mhl_sii_i2c_driver);
	if (!mhl_msm_state) {
		kfree(mhl_msm_state);
		mhl_msm_state = NULL;
	 }
	 return ret;
}

static void switch_mode(enum mhl_st_type to_mode)
{
	unsigned long flags;

	switch (to_mode) {
	case POWER_STATE_D0_NO_MHL:
		break;
	case POWER_STATE_D0_MHL:
		mhl_init_reg_settings();

		/* REG_DISC_CTRL1 */
		mhl_i2c_reg_modify(TX_PAGE_3, 0x0010, BIT1, 0);

		/*
		 * TPI_DEVICE_POWER_STATE_CTRL_REG
		 * TX_POWER_STATE_MASK = BIT1 | BIT0
		 */
		mhl_i2c_reg_modify(TX_PAGE_TPI, 0x001E, BIT1 | BIT0, 0x00);
		break;
	case POWER_STATE_D3:
		if (mhl_msm_state->cur_state != POWER_STATE_D3) {
			/* Force HPD to 0 when not in MHL mode.  */
			mhl_i2c_reg_modify(TX_PAGE_3, 0x0020,
				BIT5 | BIT4, BIT4);

			/*
			 * Change TMDS termination to high impedance
			 * on disconnection.
			 */
			mhl_i2c_reg_write(TX_PAGE_3, 0x0030, 0xD0);
			mhl_i2c_reg_modify(TX_PAGE_L1, 0x003D,
				BIT1 | BIT0, BIT0);
			spin_lock_irqsave(&mhl_state_lock, flags);
			mhl_msm_state->cur_state = POWER_STATE_D3;
			spin_unlock_irqrestore(&mhl_state_lock, flags);
		}
		break;
	default:
		break;
	}
}

static void mhl_drive_hpd(uint8_t to_state)
{
	pr_debug("%s: To state=[0x%x]\n", __func__, to_state);
	if (to_state == HPD_UP) {
		/*
		 * Drive HPD to UP state
		 *
		 * The below two reg configs combined
		 * enable TMDS output.
		 */

		/* Enable TMDS on TMDS_CCTRL */
		mhl_i2c_reg_modify(TX_PAGE_L0, 0x0080, BIT4, BIT4);

		/*
		 * Set HPD_OUT_OVR_EN = HPD State
		 * EDID read and Un-force HPD (from low)
		 * propogate to src let HPD float by clearing
		 * HPD OUT OVRRD EN
		 */
		mhl_i2c_reg_modify(TX_PAGE_3, 0x0020, BIT4, 0x00);
	} else {
		/*
		 * Drive HPD to DOWN state
		 * Disable TMDS Output on REG_TMDS_CCTRL
		 * Enable/Disable TMDS output (MHL TMDS output only)
		 */
		mhl_i2c_reg_modify(TX_PAGE_L0, 0x0080, BIT4, 0x00);
	}
	return;
}

static void mhl_msm_connection(void)
{
	uint8_t val;
	unsigned long flags;

	pr_debug("%s: cur state = [0x%x]\n", __func__,
		mhl_msm_state->cur_state);

	if (mhl_msm_state->cur_state == POWER_STATE_D0_MHL) {
		/* Already in D0 - MHL power state */
		return;
	}
	spin_lock_irqsave(&mhl_state_lock, flags);
	mhl_msm_state->cur_state = POWER_STATE_D0_MHL;
	spin_unlock_irqrestore(&mhl_state_lock, flags);

	mhl_i2c_reg_write(TX_PAGE_3, 0x30, 0x10);

	mhl_i2c_reg_write(TX_PAGE_CBUS, 0x07, 0xF2);

	/*
	 * Keep the discovery enabled. Need RGND interrupt
	 * Possibly chip disables discovery after MHL_EST??
	 * Need to re-enable here
	 */
	val = mhl_i2c_reg_read(TX_PAGE_3, 0x10);
	mhl_i2c_reg_write(TX_PAGE_3, 0x10, val | BIT(0));

	return;
}

static void mhl_msm_disconnection(void)
{
	uint8_t reg;
	/* Clear interrupts - REG INTR4 */
	reg = mhl_i2c_reg_read(TX_PAGE_3, 0x0021);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0021, reg);
	/*
	 * MHL TX CTL1
	 * Disabling Tx termination
	 */
	mhl_i2c_reg_write(TX_PAGE_3, 0x30, 0xD0);
	/*
	 * MSC REQUESTOR ABORT REASON
	 * Clear CBUS_HPD status
	 */
	mhl_i2c_reg_modify(TX_PAGE_CBUS, 0x000D, BIT6, 0x00);
	/* Change HPD line to drive it low */
	mhl_drive_hpd(HPD_DOWN);
	/* switch power state to D3 */
	switch_mode(POWER_STATE_D3);
	return;
}

/*
 * If hardware detected a change in impedence and raised an INTR
 * We check the range of this impedence to infer if the connected
 * device is MHL or USB and take appropriate actions.
 */
static void mhl_msm_read_rgnd_int(void)
{
	uint8_t rgnd_imp;

	/*
	 * DISC STATUS REG 2
	 * 1:0 RGND
	 * 00  - open (USB)
	 * 01  - 2 kOHM (USB)
	 * 10  - 1 kOHM ***(MHL)**** It's range 800 - 1200 OHM from MHL spec
	 * 11  - short (USB)
	 */
	rgnd_imp = mhl_i2c_reg_read(TX_PAGE_3, 0x001C);
	pr_debug("Imp Range read = %02X\n", (int)rgnd_imp);


	if (0x02 == rgnd_imp) {
		pr_debug("MHL: MHL DEVICE!!!\n");
		/*
		 * Handling the MHL event in driver
		 */
		mhl_i2c_reg_modify(TX_PAGE_3, 0x0018, BIT0, BIT0);
	} else {
		pr_debug("MHL: NON-MHL DEVICE!!!\n");
		mhl_i2c_reg_modify(TX_PAGE_3, 0x0018, BIT3, BIT3);
	}
}

static void force_usb_switch_open(void)
{
	/*DISABLE_DISCOVERY*/
	mhl_i2c_reg_modify(TX_PAGE_3, 0x0010, BIT0, 0);
	/* Force USB ID switch to open*/
	mhl_i2c_reg_modify(TX_PAGE_3, 0x0015, BIT6, BIT6);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0012, 0x86);
	/* Force HPD to 0 when not in Mobile HD mode. */
	mhl_i2c_reg_modify(TX_PAGE_3, 0x0020, BIT5 | BIT4, BIT4);
}

static void release_usb_switch_open(void)
{
	msleep(50);
	mhl_i2c_reg_modify(TX_PAGE_3, 0x0015, BIT6, 0x00);
	mhl_i2c_reg_modify(TX_PAGE_3, 0x0010, BIT0, BIT0);
}

static void int_4_isr(void)
{
	uint8_t status;

	/* INTR_STATUS4 */
	status = mhl_i2c_reg_read(TX_PAGE_3, 0x0021);

	/*
	 * When I2C is inoperational (D3) and
	 * a previous interrupt brought us here,
	 * do nothing.
	 */
	if ((0x00 == status) && (mhl_msm_state->cur_state == POWER_STATE_D3)) {
		mhl_chip_init();
		return;
	}
	if (0xFF != status) {
		if ((status & BIT0) && (mhl_msm_state->chip_rev_id < 1)) {
			uint8_t tmds_cstat;
			uint8_t mhl_fifo_status;

			/* TMDS CSTAT */
			tmds_cstat = mhl_i2c_reg_read(TX_PAGE_3, 0x0040);

			pr_debug("TMDS CSTAT: 0x%02x\n", tmds_cstat);

			if (tmds_cstat & 0x02) {
				mhl_fifo_status = mhl_i2c_reg_read(TX_PAGE_3,
					0x0023);
				pr_debug("MHL FIFO status: 0x%02x\n",
					mhl_fifo_status);
				if (mhl_fifo_status & 0x0C) {
					mhl_i2c_reg_write(TX_PAGE_3, 0x0023,
						0x0C);

					pr_debug("Apply MHL FIFO Reset\n");
					mhl_i2c_reg_write(TX_PAGE_3, 0x0000,
						0x94);
					mhl_i2c_reg_write(TX_PAGE_3, 0x0000,
						0x84);
				}
			}
		}

		if (status & BIT1)
			pr_debug("MHL: INT4 BIT1 is set\n");

		/* MHL_EST interrupt */
		if (status & BIT2) {
			pr_debug("mhl_msm_connection() from ISR\n");
			mhl_connect_api(true);
			mhl_msm_connection();
			pr_debug("MHL Connect  Drv: INT4 Status = %02X\n",
				(int) status);
		} else if (status & BIT3) {
			pr_debug("MHL: uUSB-A type device detected.\n");
			mhl_i2c_reg_write(TX_PAGE_3, 0x001C, 0x80);
			switch_mode(POWER_STATE_D3);
		}

		if (status & BIT5) {
			mhl_connect_api(false);
			mhl_msm_disconnection();
			pr_debug("MHL Disconn Drv: INT4 Status = %02X\n",
				(int)status);
		}

		if ((mhl_msm_state->cur_state != POWER_STATE_D0_MHL) &&\
			(status & BIT6)) {
			/* RGND READY Intr */
			switch_mode(POWER_STATE_D0_MHL);
			mhl_msm_read_rgnd_int();
		}

		/* Can't succeed at these in D3 */
		if (mhl_msm_state->cur_state != POWER_STATE_D3) {
			/* CBUS Lockout interrupt? */
			/*
			 * Hardware detection mechanism figures that
			 * CBUS line is latched and raises this intr
			 * where we force usb switch open and release
			 */
			if (status & BIT4) {
				force_usb_switch_open();
				release_usb_switch_open();
			}
		}
	}
	pr_debug("MHL END  Drv: INT4 Status = %02X\n", (int) status);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0021, status);

	return;
}

static void int_5_isr(void)
{
	uint8_t intr_5_stat;

	/*
	 * Clear INT 5 ??
	 * Probably need to revisit this later
	 * INTR5 is related to FIFO underflow/overflow reset
	 * which is handled in 8334 by auto FIFO reset
	 */
	intr_5_stat = mhl_i2c_reg_read(TX_PAGE_3, 0x0023);
	mhl_i2c_reg_write(TX_PAGE_3, 0x0023, intr_5_stat);
}


static void int_1_isr(void)
{
	/* This ISR mainly handles the HPD status changes */
	uint8_t intr_1_stat;
	uint8_t cbus_stat;

	/* INTR STATUS 1 */
	intr_1_stat = mhl_i2c_reg_read(TX_PAGE_L0, 0x0071);

	if (intr_1_stat) {
		/* Clear interrupts */
		mhl_i2c_reg_write(TX_PAGE_L0, 0x0071, intr_1_stat);
		if (BIT6 & intr_1_stat) {
			/*
			 * HPD status change event is pending
			 * Read CBUS HPD status for this info
			 */

			/* MSC REQ ABRT REASON */
			cbus_stat = mhl_i2c_reg_read(TX_PAGE_CBUS, 0x0D);
			if (BIT6 & cbus_stat)
				mhl_drive_hpd(HPD_UP);
		}
	}
	return;
}

/*
 * RCP, RAP messages - mandatory for compliance
 *
 */
static void mhl_cbus_isr(void)
{
	uint8_t regval;
	int req_done = FALSE;
	uint8_t sub_cmd;
	uint8_t cmd_data;
	int msc_msg_recved = FALSE;
	int rc = -1;

	regval  = mhl_i2c_reg_read(TX_PAGE_CBUS, 0x08);
	if (regval == 0xff)
		return;

	/* clear all interrupts that were raised even if we did not process */
	if (regval)
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0x08, regval);

	pr_debug("%s: CBUS_INT = %02x\n", __func__, regval);

	/* MSC_MSG (RCP/RAP) */
	if (regval & BIT(3)) {
		sub_cmd = mhl_i2c_reg_read(TX_PAGE_CBUS, 0x18);
		cmd_data = mhl_i2c_reg_read(TX_PAGE_CBUS, 0x19);
		msc_msg_recved = TRUE;
	}

	/* MSC_REQ_DONE */
	if (regval & BIT(4))
		req_done = TRUE;

	/* Now look for interrupts on CBUS_MSC_INT2 */
	regval  = mhl_i2c_reg_read(TX_PAGE_CBUS, 0x1E);

	/* clear all interrupts that were raised */
	/* even if we did not process */
	if (regval)
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0x1E, regval);

	pr_debug("%s: CBUS_MSC_INT2 = %02x\n", __func__, regval);

	/* received SET_INT */
	if (regval & BIT(2)) {
		uint8_t intr;
		intr = mhl_i2c_reg_read(TX_PAGE_CBUS, 0xA0);
		pr_debug("%s: MHL_INT_0 = %02x\n", __func__, intr);
		intr = mhl_i2c_reg_read(TX_PAGE_CBUS, 0xA1);
		pr_debug("%s: MHL_INT_1 = %02x\n", __func__, intr);
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xA0, 0xFF);
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xA1, 0xFF);
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xA2, 0xFF);
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xA3, 0xFF);
	}

	/* received WRITE_STAT */
	if (regval & BIT(3)) {
		uint8_t stat;
		stat = mhl_i2c_reg_read(TX_PAGE_CBUS, 0xB0);
		pr_debug("%s: MHL_STATUS_0 = %02x\n", __func__, stat);
		stat = mhl_i2c_reg_read(TX_PAGE_CBUS, 0xB1);
		pr_debug("%s: MHL_STATUS_1 = %02x\n", __func__, stat);

		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xB0, 0xFF);
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xB1, 0xFF);
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xB2, 0xFF);
		mhl_i2c_reg_write(TX_PAGE_CBUS, 0xB3, 0xFF);
	}

	/* received MSC_MSG */
	if (msc_msg_recved) {
		/*mhl msc recv msc msg*/
		if (rc)
			pr_err("MHL: mhl msc recv msc msg failed(%d)!\n", rc);
	}

	return;
}

static irqreturn_t mhl_tx_isr(int irq, void *dev_id)
{
	/*
	 * Check RGND, MHL_EST, CBUS_LOCKOUT, SCDT
	 * interrupts. In D3, we get only RGND
	 */
	int_4_isr();

	pr_debug("MHL: Current POWER state is [0x%x]\n",
		mhl_msm_state->cur_state);
	if (mhl_msm_state->cur_state == POWER_STATE_D0_MHL) {
		/*
		 * If int_4_isr() didn't move the tx to D3
		 * on disconnect, continue to check other
		 * interrupt sources.
		 */
		int_5_isr();

		/*
		 * Check for any peer messages for DCAP_CHG etc
		 * Dispatch to have the CBUS module working only
		 * once connected.
		 */
		mhl_cbus_isr();
		int_1_isr();
	}
	return IRQ_HANDLED;
}

static void __exit mhl_msm_exit(void)
{
	pr_warn("MHL: Exiting, Bye\n");
	/*
	 * Delete driver if i2c client structure is NULL
	 */
	i2c_del_driver(&mhl_sii_i2c_driver);
	if (!mhl_msm_state) {
		kfree(mhl_msm_state);
		mhl_msm_state = NULL;
	 }
}

module_init(mhl_msm_init);
module_exit(mhl_msm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MHL SII 8334 TX driver");
