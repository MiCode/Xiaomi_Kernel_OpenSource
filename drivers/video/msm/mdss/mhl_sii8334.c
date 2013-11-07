/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/vmalloc.h>
#include <linux/input.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/mhl_8334.h>

#include "mdss_fb.h"
#include "mdss_hdmi_tx.h"
#include "mdss_hdmi_edid.h"
#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_io_util.h"
#include "mhl_msc.h"
#include "mdss_hdmi_mhl.h"

#define MHL_DRIVER_NAME "sii8334"
#define COMPATIBLE_NAME "qcom,mhl-sii8334"
#define MAX_CURRENT 700000

#define pr_debug_intr(...)

#define MSC_START_BIT_MSC_CMD        (0x01 << 0)
#define MSC_START_BIT_VS_CMD        (0x01 << 1)
#define MSC_START_BIT_READ_REG        (0x01 << 2)
#define MSC_START_BIT_WRITE_REG        (0x01 << 3)
#define MSC_START_BIT_WRITE_BURST        (0x01 << 4)

/* supported RCP key code */
u16 support_rcp_key_code_tbl[] = {
	KEY_ENTER,		/* 0x00 Select */
	KEY_UP,			/* 0x01 Up */
	KEY_DOWN,		/* 0x02 Down */
	KEY_LEFT,		/* 0x03 Left */
	KEY_RIGHT,		/* 0x04 Right */
	KEY_UNKNOWN,		/* 0x05 Right-up */
	KEY_UNKNOWN,		/* 0x06 Right-down */
	KEY_UNKNOWN,		/* 0x07 Left-up */
	KEY_UNKNOWN,		/* 0x08 Left-down */
	KEY_MENU,		/* 0x09 Root Menu */
	KEY_OPTION,		/* 0x0A Setup Menu */
	KEY_UNKNOWN,		/* 0x0B Contents Menu */
	KEY_UNKNOWN,		/* 0x0C Favorite Menu */
	KEY_EXIT,		/* 0x0D Exit */
	KEY_RESERVED,		/* 0x0E */
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,		/* 0x1F */
	KEY_NUMERIC_0,		/* 0x20 NUMERIC_0 */
	KEY_NUMERIC_1,		/* 0x21 NUMERIC_1 */
	KEY_NUMERIC_2,		/* 0x22 NUMERIC_2 */
	KEY_NUMERIC_3,		/* 0x23 NUMERIC_3 */
	KEY_NUMERIC_4,		/* 0x24 NUMERIC_4 */
	KEY_NUMERIC_5,		/* 0x25 NUMERIC_5 */
	KEY_NUMERIC_6,		/* 0x26 NUMERIC_6 */
	KEY_NUMERIC_7,		/* 0x27 NUMERIC_7 */
	KEY_NUMERIC_8,		/* 0x28 NUMERIC_8 */
	KEY_NUMERIC_9,		/* 0x29 NUMERIC_9 */
	KEY_DOT,		/* 0x2A Dot */
	KEY_ENTER,		/* 0x2B Enter */
	KEY_ESC,		/* 0x2C Clear */
	KEY_RESERVED,		/* 0x2D */
	KEY_RESERVED,		/* 0x2E */
	KEY_RESERVED,		/* 0x2F */
	KEY_UNKNOWN,		/* 0x30 Channel Up */
	KEY_UNKNOWN,		/* 0x31 Channel Down */
	KEY_UNKNOWN,		/* 0x32 Previous Channel */
	KEY_UNKNOWN,		/* 0x33 Sound Select */
	KEY_UNKNOWN,		/* 0x34 Input Select */
	KEY_UNKNOWN,		/* 0x35 Show Information */
	KEY_UNKNOWN,		/* 0x36 Help */
	KEY_UNKNOWN,		/* 0x37 Page Up */
	KEY_UNKNOWN,		/* 0x38 Page Down */
	KEY_RESERVED,		/* 0x39 */
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,		/* 0x3F */
	KEY_RESERVED,		/* 0x40 */
	KEY_VOLUMEUP,		/* 0x41 Volume Up */
	KEY_VOLUMEDOWN,		/* 0x42 Volume Down */
	KEY_MUTE,		/* 0x43 Mute */
	KEY_PLAY,		/* 0x44 Play */
	KEY_STOP,		/* 0x45 Stop */
	KEY_PAUSE,		/* 0x46 Pause */
	KEY_UNKNOWN,		/* 0x47 Record */
	KEY_REWIND,		/* 0x48 Rewind */
	KEY_FASTFORWARD,	/* 0x49 Fast Forward */
	KEY_UNKNOWN,		/* 0x4A Eject */
	KEY_FORWARD,		/* 0x4B Forward */
	KEY_BACK,		/* 0x4C Backward */
	KEY_RESERVED,		/* 0x4D */
	KEY_RESERVED,
	KEY_RESERVED,		/* 0x4F */
	KEY_UNKNOWN,		/* 0x50 Angle */
	KEY_UNKNOWN,		/* 0x51 Subtitle */
	KEY_RESERVED,		/* 0x52 */
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,		/* 0x5F */
	KEY_PLAYPAUSE,		/* 0x60 Play Function */
	KEY_PLAYPAUSE,		/* 0x61 Pause_Play Function */
	KEY_UNKNOWN,		/* 0x62 Record Function */
	KEY_PAUSE,		/* 0x63 Pause Record Function */
	KEY_STOP,		/* 0x64 Stop Function  */
	KEY_MUTE,		/* 0x65 Mute Function */
	KEY_UNKNOWN,		/* 0x66 Restore Volume Function */
	KEY_UNKNOWN,		/* 0x67 Tune Function */
	KEY_UNKNOWN,		/* 0x68 Select Media Function */
	KEY_RESERVED,		/* 0x69 */
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,		/* 0x70 */
	KEY_BLUE,			/* 0x71 F1 */
	KEY_RED,			/* 0x72 F2 */
	KEY_GREEN,			/* 0x73 F3 */
	KEY_YELLOW,			/* 0x74 F4 */
	KEY_UNKNOWN,		/* 0x75 F5 */
	KEY_RESERVED,		/* 0x76 */
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,		/* 0x7D */
	KEY_VENDOR,		/* Vendor Specific */
	KEY_RESERVED,		/* 0x7F */
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
			enum mhl_st_type to_mode, bool hpd_off);
static void mhl_init_reg_settings(struct mhl_tx_ctrl *mhl_ctrl,
				  bool mhl_disc_en);

int mhl_i2c_reg_read(struct i2c_client *client,
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


int mhl_i2c_reg_write(struct i2c_client *client,
			     uint8_t slave_addr_index, uint8_t reg_offset,
			     uint8_t value)
{
	return mdss_i2c_byte_write(client, slave_addrs[slave_addr_index],
				 reg_offset, &value);
}

void mhl_i2c_reg_modify(struct i2c_client *client,
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
	struct platform_device *hdmi_pdev = NULL;
	struct device_node *hdmi_tx_node = NULL;
	int dt_gpio;
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
	dt_gpio = of_get_named_gpio(of_node, "mhl-rst-gpio", 0);
	if (dt_gpio < 0) {
		pr_err("%s: Can't get mhl-rst-gpio\n", __func__);
		goto error;
	}

	temp_gpio->gpio = dt_gpio;
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
	dt_gpio = of_get_named_gpio(of_node, "mhl-pwr-gpio", 0);
	if (dt_gpio < 0) {
		pr_err("%s: Can't get mhl-pwr-gpio\n", __func__);
		goto error;
	}

	temp_gpio->gpio = dt_gpio;
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
	dt_gpio = of_get_named_gpio(of_node, "mhl-intr-gpio", 0);
	if (dt_gpio < 0) {
		pr_err("%s: Can't get mhl-intr-gpio\n", __func__);
		goto error;
	}

	temp_gpio->gpio = dt_gpio;
	snprintf(temp_gpio->gpio_name, 32, "%s", "mhl-intr-gpio");
	pr_debug("%s: intr gpio=[%d]\n", __func__,
		 temp_gpio->gpio);
	pdata->gpios[MHL_TX_INTR_GPIO] = temp_gpio;

	/* parse phandle for hdmi tx */
	hdmi_tx_node = of_parse_phandle(of_node, "qcom,hdmi-tx-map", 0);
	if (!hdmi_tx_node) {
		pr_err("%s: can't find hdmi phandle\n", __func__);
		goto error;
	}

	hdmi_pdev = of_find_device_by_node(hdmi_tx_node);
	if (!hdmi_pdev) {
		pr_err("%s: can't find the device by node\n", __func__);
		goto error;
	}
	pr_debug("%s: hdmi_pdev [0X%x] to pdata->pdev\n",
	       __func__, (unsigned int)hdmi_pdev);

	pdata->hdmi_pdev = hdmi_pdev;

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
	if (mhl_ctrl->pdata->gpios[MHL_TX_RESET_GPIO]) {
		gpio_set_value(
			mhl_ctrl->pdata->gpios[MHL_TX_RESET_GPIO]->gpio,
			on);
	}
	return 0;
}


static int mhl_sii_wait_for_rgnd(struct mhl_tx_ctrl *mhl_ctrl)
{
	int timeout;

	pr_debug("%s:%u\n", __func__, __LINE__);
	INIT_COMPLETION(mhl_ctrl->rgnd_done);
	/*
	 * after toggling reset line and enabling disc
	 * tx can take a while to generate intr
	 */
	timeout = wait_for_completion_interruptible_timeout
		(&mhl_ctrl->rgnd_done, HZ * 3);
	if (!timeout) {
		/*
		 * most likely nothing plugged in USB
		 * USB HOST connected or already in USB mode
		 */
		pr_warn("%s:%u timedout\n", __func__, __LINE__);
		return -ENODEV;
	}
	return mhl_ctrl->mhl_mode ? 0 : 1;
}

/*  USB_HANDSHAKING FUNCTIONS */
static int mhl_sii_device_discovery(void *data, int id,
			     void (*usb_notify_cb)(void *, int), void *ctx)
{
	int rc;
	struct mhl_tx_ctrl *mhl_ctrl = data;
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	unsigned long flags;

	if (!mhl_ctrl->irq_req_done) {
		rc = request_threaded_irq(mhl_ctrl->i2c_handle->irq, NULL,
					  &mhl_tx_isr,
					  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					  client->dev.driver->name, mhl_ctrl);
		if (rc) {
			pr_err("request_threaded_irq failed, status: %d\n",
			       rc);
			return -EINVAL;
		} else {
			pr_debug("request_threaded_irq succeeded\n");
			mhl_ctrl->irq_req_done = true;
		}
	} else {
		enable_irq(client->irq);
	}

	/* wait for i2c interrupt line to be activated */
	msleep(100);

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

	if (!mhl_ctrl->notify_usb_online) {
		mhl_ctrl->notify_usb_online = usb_notify_cb;
		mhl_ctrl->notify_ctx = ctx;
	}

	if (!mhl_ctrl->disc_enabled) {
		spin_lock_irqsave(&mhl_ctrl->lock, flags);
		mhl_ctrl->tx_powered_off = false;
		spin_unlock_irqrestore(&mhl_ctrl->lock, flags);
		mhl_sii_reset_pin(mhl_ctrl, 0);
		msleep(50);
		mhl_sii_reset_pin(mhl_ctrl, 1);
		/* chipset PR recommends waiting for at least 100 ms
		 * the chipset needs longer to come out of D3 state.
		 */
		msleep(100);
		mhl_init_reg_settings(mhl_ctrl, true);
		/* allow tx to enable dev disc after D3 state */
		msleep(100);
		rc = mhl_sii_wait_for_rgnd(mhl_ctrl);
	} else {
		if (mhl_ctrl->cur_state == POWER_STATE_D3) {
			rc = mhl_sii_wait_for_rgnd(mhl_ctrl);
		} else {
			/* in MHL mode */
			pr_debug("%s:%u\n", __func__, __LINE__);
			rc = 0;
		}
	}
	pr_debug("%s: ret result: %s\n", __func__, rc ? "usb" : " mhl");
	return rc;
}

static int mhl_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct mhl_tx_ctrl *mhl_ctrl =
		container_of(psy, struct mhl_tx_ctrl, mhl_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = mhl_ctrl->current_val;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = mhl_ctrl->vbus_active;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = mhl_ctrl->vbus_active && mhl_ctrl->mhl_mode;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mhl_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct mhl_tx_ctrl *mhl_ctrl =
		container_of(psy, struct mhl_tx_ctrl, mhl_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		mhl_ctrl->vbus_active = val->intval;
		if (mhl_ctrl->vbus_active)
			mhl_ctrl->current_val = MAX_CURRENT;
		else
			mhl_ctrl->current_val = 0;
		power_supply_changed(psy);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static char *mhl_pm_power_supplied_to[] = {
	"usb",
};

static enum power_supply_property mhl_pm_power_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static void cbus_reset(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t i;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* Read the chip rev ID */
	mhl_ctrl->chip_rev_id = MHL_SII_PAGE0_RD(0x04);
	pr_debug("MHL: chip rev ID read=[%x]\n", mhl_ctrl->chip_rev_id);

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

	if (mhl_ctrl->chip_rev_id < 1)
		MHL_SII_REG_NAME_WR(REG_INTR5_MASK, BIT3 | BIT4);
	else
		MHL_SII_REG_NAME_WR(REG_INTR5_MASK, 0x00);

	/* Unmask CBUS1 Intrs */
	MHL_SII_REG_NAME_WR(REG_CBUS_INTR_ENABLE,
		BIT2 | BIT3 | BIT4 | BIT5 | BIT6);

	/* Unmask CBUS2 Intrs */
	MHL_SII_REG_NAME_WR(REG_CBUS_MSC_INT2_ENABLE, BIT2 | BIT3);

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
}

static void init_cbus_regs(struct i2c_client *client)
{
	uint8_t		regval;

	/* Increase DDC translation layer timer*/
	MHL_SII_CBUS_WR(0x0007, 0xF2);
	/* Drive High Time */
	MHL_SII_CBUS_WR(0x0036, 0x0B);
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
	uint8_t regval;

	/*
	 * ============================================
	 * POWER UP
	 * ============================================
	 */
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* Power up 1.2V core */
	MHL_SII_PAGE1_WR(0x003D, 0x3F);
	/* Enable Tx PLL Clock */
	MHL_SII_PAGE2_WR(0x0011, 0x01);
	/* Enable Tx Clock Path and Equalizer */
	MHL_SII_PAGE2_WR(0x0012, 0x11);
	/* Tx Source Termination ON */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL1, 0x10);
	/* Enable 1X MHL Clock output */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL6, 0xBC);
	/* Tx Differential Driver Config */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL2, 0x3C);
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL4, 0xC8);
	/* PLL Bandwidth Control */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL7, 0x03);
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL8, 0x0A);
	/*
	 * ============================================
	 * Analog PLL Control
	 * ============================================
	 */
	/* Enable Rx PLL clock */
	MHL_SII_REG_NAME_WR(REG_TMDS_CCTRL,  0x08);
	MHL_SII_PAGE0_WR(0x00F8, 0x8C);
	MHL_SII_PAGE0_WR(0x0085, 0x02);
	MHL_SII_PAGE2_WR(0x0000, 0x00);
	regval = MHL_SII_PAGE2_RD(0x0005);
	regval &= ~BIT5;
	MHL_SII_PAGE2_WR(0x0005, regval);
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
	MHL_SII_PAGE2_WR(0x004C, 0x60);
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
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL5, 0x57);
	/* RGND and single Discovery attempt */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL6, 0x11);
	/* Ignore VBUS */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL8, 0x82);

	/* Enable CBUS Discovery */
	if (mhl_disc_en) {
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL9, 0x24);
		/* Enable MHL Discovery */
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL1, 0x27);
		/* Pull-up resistance off for IDLE state */
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL4, 0x8C);
	} else {
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL9, 0x26);
		/* Disable MHL Discovery */
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL1, 0x26);
		MHL_SII_REG_NAME_WR(REG_DISC_CTRL4, 0x8C);
	}

	MHL_SII_REG_NAME_WR(REG_DISC_CTRL7, 0x20);
	/* MHL CBUS Discovery - immediate comm.  */
	MHL_SII_REG_NAME_WR(REG_DISC_CTRL3, 0x86);

	MHL_SII_PAGE3_WR(0x3C, 0x80);

	MHL_SII_REG_NAME_MOD(REG_INT_CTRL,
			     (BIT6 | BIT5 | BIT4), (BIT6 | BIT4));

	/* Enable Auto Soft RESET */
	MHL_SII_REG_NAME_WR(REG_SRST, 0x084);
	/* HDMI Transcode mode enable */
	MHL_SII_PAGE0_WR(0x000D, 0x1C);

	cbus_reset(mhl_ctrl);
	init_cbus_regs(client);
}


static void switch_mode(struct mhl_tx_ctrl *mhl_ctrl, enum mhl_st_type to_mode,
			bool hpd_off)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	unsigned long flags;
	int rc;
	struct msm_hdmi_mhl_ops *hdmi_mhl_ops = mhl_ctrl->hdmi_mhl_ops;

	pr_debug("%s: tx pwr on\n", __func__);
	spin_lock_irqsave(&mhl_ctrl->lock, flags);
	mhl_ctrl->tx_powered_off = false;
	spin_unlock_irqrestore(&mhl_ctrl->lock, flags);

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
		if (mhl_ctrl->cur_state == POWER_STATE_D3) {
			pr_debug("%s: mhl tx already in low power mode\n",
				__func__);
			break;
		}

		/* Force HPD to 0 when not in MHL mode.  */
		mhl_drive_hpd(mhl_ctrl, HPD_DOWN);
		mhl_tmds_ctrl(mhl_ctrl, TMDS_DISABLE);
		/*
		 * Change TMDS termination to high impedance
		 * on disconnection.
		 */
		MHL_SII_REG_NAME_WR(REG_MHLTX_CTL1, 0xD0);
		msleep(50);
		if (!mhl_ctrl->disc_enabled)
			MHL_SII_REG_NAME_MOD(REG_DISC_CTRL1, BIT1 | BIT0, 0x00);
		if (hdmi_mhl_ops && hpd_off) {
			rc = hdmi_mhl_ops->set_upstream_hpd(
				mhl_ctrl->pdata->hdmi_pdev, 0);
			pr_debug("%s: hdmi unset hpd %s\n", __func__,
				 rc ? "failed" : "passed");
		}
		mhl_ctrl->cur_state = POWER_STATE_D3;
		break;
	default:
		break;
	}
}

static bool is_mhl_powered(void *mhl_ctx)
{
	struct mhl_tx_ctrl *mhl_ctrl = (struct mhl_tx_ctrl *)mhl_ctx;
	unsigned long flags;
	bool r = false;

	spin_lock_irqsave(&mhl_ctrl->lock, flags);
	if (mhl_ctrl->tx_powered_off)
		r = false;
	else
		r = true;
	spin_unlock_irqrestore(&mhl_ctrl->lock, flags);

	pr_debug("%s: ret pwr state as %x\n", __func__, r);
	return r;
}

void mhl_tmds_ctrl(struct mhl_tx_ctrl *mhl_ctrl, uint8_t on)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	if (on) {
		MHL_SII_REG_NAME_MOD(REG_TMDS_CCTRL, BIT4, BIT4);
		mhl_ctrl->tmds_en_state = true;
	} else {
		MHL_SII_REG_NAME_MOD(REG_TMDS_CCTRL, BIT4, 0x00);
		mhl_ctrl->tmds_en_state = false;
	}
}

void mhl_drive_hpd(struct mhl_tx_ctrl *mhl_ctrl, uint8_t to_state)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	unsigned long flags;

	pr_debug("%s: To state=[0x%x]\n", __func__, to_state);
	if (to_state == HPD_UP) {
		/*
		 * Drive HPD to UP state
		 * Set HPD_OUT_OVR_EN = HPD State
		 * EDID read and Un-force HPD (from low)
		 * propogate to src let HPD float by clearing
		 * HPD OUT OVRRD EN
		 */
		spin_lock_irqsave(&mhl_ctrl->lock, flags);
		mhl_ctrl->tx_powered_off = false;
		spin_unlock_irqrestore(&mhl_ctrl->lock, flags);
		MHL_SII_REG_NAME_MOD(REG_INT_CTRL, BIT4, 0);
	} else {
		/* Drive HPD to DOWN state */
		MHL_SII_REG_NAME_MOD(REG_INT_CTRL, (BIT4 | BIT5), BIT4);
	}
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
	switch_mode(mhl_ctrl, POWER_STATE_D0_MHL, true);

	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL1, 0x10);
	MHL_SII_CBUS_WR(0x07, 0xF2);

	/*
	 * Keep the discovery enabled. Need RGND interrupt
	 * Possibly chip disables discovery after MHL_EST??
	 * Need to re-enable here
	 */
	val = MHL_SII_PAGE3_RD(0x10);
	MHL_SII_PAGE3_WR(0x10, val | BIT0);

	/*
	 * indicate DCAP_RDY and DCAP_CHG
	 * to the peer only after
	 * msm conn has been established
	 */
	mhl_msc_send_write_stat(mhl_ctrl,
				MHL_STATUS_REG_CONNECTED_RDY,
				MHL_STATUS_DCAP_RDY);

	mhl_msc_send_set_int(mhl_ctrl,
			     MHL_RCHANGE_INT,
			     MHL_INT_DCAP_CHG,
			     MSC_PRIORITY_SEND);

}

static void mhl_msm_disconnection(struct mhl_tx_ctrl *mhl_ctrl)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* disabling Tx termination */
	MHL_SII_REG_NAME_WR(REG_MHLTX_CTL1, 0xD0);
	switch_mode(mhl_ctrl, POWER_STATE_D3, true);
	mhl_msc_clear(mhl_ctrl);
}

static int mhl_msm_read_rgnd_int(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t rgnd_imp;
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	struct msm_hdmi_mhl_ops *hdmi_mhl_ops = mhl_ctrl->hdmi_mhl_ops;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&mhl_ctrl->lock, flags);
	mhl_ctrl->tx_powered_off = false;
	spin_unlock_irqrestore(&mhl_ctrl->lock, flags);

	/* DISC STATUS REG 2 */
	rgnd_imp = (mhl_i2c_reg_read(client, TX_PAGE_3, 0x001C) &
		    (BIT1 | BIT0));
	pr_debug("imp range read=%02X\n", (int)rgnd_imp);

	if (0x02 == rgnd_imp) {
		pr_debug("%s: mhl sink\n", __func__);
		if (hdmi_mhl_ops) {
			rc = hdmi_mhl_ops->set_upstream_hpd(
				mhl_ctrl->pdata->hdmi_pdev, 1);
			pr_debug("%s: hdmi set hpd %s\n", __func__,
				 rc ? "failed" : "passed");
		}
		mhl_ctrl->mhl_mode = 1;
		power_supply_changed(&mhl_ctrl->mhl_psy);
		if (mhl_ctrl->notify_usb_online)
			mhl_ctrl->notify_usb_online(mhl_ctrl->notify_ctx, 1);
	} else {
		pr_debug("%s: non-mhl sink\n", __func__);
		mhl_ctrl->mhl_mode = 0;
		switch_mode(mhl_ctrl, POWER_STATE_D3, true);
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


static int dev_detect_isr(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t status, reg;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	/* INTR_STATUS4 */
	status = MHL_SII_REG_NAME_RD(REG_INTR4);
	pr_debug("%s: reg int4 st=%02X\n", __func__, status);

	if ((0x00 == status) &&\
	    (mhl_ctrl->cur_state == POWER_STATE_D3)) {
		pr_warn("%s: invalid intr\n", __func__);
		return 0;
	}

	if (0xFF == status) {
		pr_warn("%s: invalid intr 0xff\n", __func__);
		MHL_SII_REG_NAME_WR(REG_INTR4, status);
		return 0;
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
		power_supply_changed(&mhl_ctrl->mhl_psy);
		if (mhl_ctrl->notify_usb_online)
			mhl_ctrl->notify_usb_online(mhl_ctrl->notify_ctx, 0);
		return 0;
	}

	if (status & BIT5) {
		/* clr intr - reg int4 */
		pr_debug("%s: mhl discon: int4 st=%02X\n", __func__,
			 (int)status);
		mhl_ctrl->mhl_det_discon = true;

		reg = MHL_SII_REG_NAME_RD(REG_INTR4);
		MHL_SII_REG_NAME_WR(REG_INTR4, reg);
		mhl_msm_disconnection(mhl_ctrl);
		power_supply_changed(&mhl_ctrl->mhl_psy);
		if (mhl_ctrl->notify_usb_online)
			mhl_ctrl->notify_usb_online(mhl_ctrl->notify_ctx, 0);
		return 0;
	}

	if ((mhl_ctrl->cur_state != POWER_STATE_D0_NO_MHL) &&\
	    (status & BIT6)) {
		/* rgnd rdy Intr */
		pr_debug("%s: rgnd ready intr\n", __func__);
		switch_mode(mhl_ctrl, POWER_STATE_D0_NO_MHL, true);
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
	return 0;
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

static void mhl_tx_down(struct mhl_tx_ctrl *mhl_ctrl)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	unsigned long flags;
	uint8_t reg;

	switch_mode(mhl_ctrl, POWER_STATE_D3, true);

	reg = MHL_SII_REG_NAME_RD(REG_INTR1);
	MHL_SII_REG_NAME_WR(REG_INTR1, reg);

	reg = MHL_SII_REG_NAME_RD(REG_INTR4);
	MHL_SII_REG_NAME_WR(REG_INTR4, reg);

	/* disable INTR1 and INTR4 */
	MHL_SII_REG_NAME_MOD(REG_INTR1_MASK, BIT6, 0x0);
	MHL_SII_REG_NAME_MOD(REG_INTR4_MASK,
		(BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6), 0x0);

	MHL_SII_PAGE1_MOD(0x003D, BIT0, 0x00);
	spin_lock_irqsave(&mhl_ctrl->lock, flags);
	mhl_ctrl->tx_powered_off = true;
	spin_unlock_irqrestore(&mhl_ctrl->lock, flags);
	pr_debug("%s: disabled\n", __func__);
	disable_irq_nosync(client->irq);
}

static void mhl_hpd_stat_isr(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t intr_1_stat, cbus_stat, t;
	unsigned long flags;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	if (!is_mhl_powered(mhl_ctrl))
		return;

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
		pr_debug("%s: cbus_stat=[0x%02x] cur_pwr=[%u]\n",
			 __func__, cbus_stat, mhl_ctrl->cur_state);

		spin_lock_irqsave(&mhl_ctrl->lock, flags);
		t = mhl_ctrl->dwnstream_hpd;
		pr_debug("%s: %u: dwnstrm_hpd=0x%02x\n",
			 __func__, __LINE__, mhl_ctrl->dwnstream_hpd);
		spin_unlock_irqrestore(&mhl_ctrl->lock, flags);

		if (BIT6 & (cbus_stat ^ t)) {
			u8 status = cbus_stat & BIT6;
			mhl_drive_hpd(mhl_ctrl, status ? HPD_UP : HPD_DOWN);
			if (!status && mhl_ctrl->mhl_det_discon) {
				pr_debug("%s:%u: power_down\n",
					 __func__, __LINE__);
				mhl_tx_down(mhl_ctrl);
			}
			spin_lock_irqsave(&mhl_ctrl->lock, flags);
			mhl_ctrl->dwnstream_hpd = cbus_stat;
			pr_debug("%s: %u: dwnstrm_hpd=0x%02x\n",
				 __func__, __LINE__, mhl_ctrl->dwnstream_hpd);
			spin_unlock_irqrestore(&mhl_ctrl->lock, flags);
			mhl_ctrl->mhl_det_discon = false;
		}
	}
}

static void mhl_sii_cbus_process_errors(struct i2c_client *client,
					u8 int_status)
{
	u8 abort_reason = 0;

	if (int_status & BIT2) {
		abort_reason = MHL_SII_REG_NAME_RD(REG_DDC_ABORT_REASON);
		pr_debug("%s: CBUS DDC Abort Reason(0x%02x)\n",
			 __func__, abort_reason);
	}
	if (int_status & BIT5) {
		abort_reason = MHL_SII_REG_NAME_RD(REG_PRI_XFR_ABORT_REASON);
		pr_debug("%s: CBUS MSC Requestor Abort Reason(0x%02x)\n",
			 __func__, abort_reason);
		MHL_SII_REG_NAME_WR(REG_PRI_XFR_ABORT_REASON, 0xFF);
	}
	if (int_status & BIT6) {
		abort_reason = MHL_SII_REG_NAME_RD(
			REG_CBUS_PRI_FWR_ABORT_REASON);
		pr_debug("%s: CBUS MSC Responder Abort Reason(0x%02x)\n",
			 __func__, abort_reason);
		MHL_SII_REG_NAME_WR(REG_CBUS_PRI_FWR_ABORT_REASON, 0xFF);
	}
}

int mhl_send_msc_command(struct mhl_tx_ctrl *mhl_ctrl,
			 struct msc_command_struct *req)
{
	int timeout;
	u8 start_bit = 0x00;
	u8 *burst_data;
	int i;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	if (mhl_ctrl->cur_state != POWER_STATE_D0_MHL) {
		pr_debug("%s: power_state:%02x CBUS(0x0A):%02x\n",
			 __func__,
			 mhl_ctrl->cur_state,
			 MHL_SII_REG_NAME_RD(REG_CBUS_BUS_STATUS));
		return -EFAULT;
	}

	if (!req)
		return -EFAULT;

	pr_debug("%s: command=0x%02x offset=0x%02x %02x %02x",
		 __func__,
		 req->command,
		 req->offset,
		 req->payload.data[0],
		 req->payload.data[1]);

	/* REG_CBUS_PRI_ADDR_CMD = REQ CBUS CMD or OFFSET */
	MHL_SII_REG_NAME_WR(REG_CBUS_PRI_ADDR_CMD, req->offset);
	MHL_SII_REG_NAME_WR(REG_CBUS_PRI_WR_DATA_1ST,
			    req->payload.data[0]);

	switch (req->command) {
	case MHL_SET_INT:
	case MHL_WRITE_STAT:
		start_bit = MSC_START_BIT_WRITE_REG;
		break;
	case MHL_READ_DEVCAP:
		start_bit = MSC_START_BIT_READ_REG;
		break;
	case MHL_GET_STATE:
	case MHL_GET_VENDOR_ID:
	case MHL_SET_HPD:
	case MHL_CLR_HPD:
	case MHL_GET_SC1_ERRORCODE:
	case MHL_GET_DDC_ERRORCODE:
	case MHL_GET_MSC_ERRORCODE:
	case MHL_GET_SC3_ERRORCODE:
		start_bit = MSC_START_BIT_MSC_CMD;
		MHL_SII_REG_NAME_WR(REG_CBUS_PRI_ADDR_CMD, req->command);
		break;
	case MHL_MSC_MSG:
		start_bit = MSC_START_BIT_VS_CMD;
		MHL_SII_REG_NAME_WR(REG_CBUS_PRI_WR_DATA_2ND,
				    req->payload.data[1]);
		MHL_SII_REG_NAME_WR(REG_CBUS_PRI_ADDR_CMD, req->command);
		break;
	case MHL_WRITE_BURST:
		start_bit = MSC_START_BIT_WRITE_BURST;
		MHL_SII_REG_NAME_WR(REG_MSC_WRITE_BURST_LEN, req->length - 1);
		if (!(req->payload.burst_data)) {
			pr_err("%s: burst data is null!\n", __func__);
			goto cbus_send_fail;
		}
		burst_data = req->payload.burst_data;
		for (i = 0; i < req->length; i++, burst_data++)
			MHL_SII_REG_NAME_WR(REG_CBUS_SCRATCHPAD_0 + i,
				*burst_data);
		break;
	default:
		pr_err("%s: unknown command! (%02x)\n",
		       __func__, req->command);
		goto cbus_send_fail;
	}

	INIT_COMPLETION(mhl_ctrl->msc_cmd_done);
	MHL_SII_REG_NAME_WR(REG_CBUS_PRI_START, start_bit);
	timeout = wait_for_completion_interruptible_timeout
		(&mhl_ctrl->msc_cmd_done, msecs_to_jiffies(T_ABORT_NEXT));
	if (!timeout) {
		pr_err("%s: cbus_command_send timed out!\n", __func__);
		goto cbus_send_fail;
	}

	switch (req->command) {
	case MHL_READ_DEVCAP:
		req->retval = MHL_SII_REG_NAME_RD(REG_CBUS_PRI_RD_DATA_1ST);
		break;
	case MHL_MSC_MSG:
		/* check if MSC_MSG NACKed */
		if (MHL_SII_REG_NAME_RD(REG_MSC_WRITE_BURST_LEN) & BIT6)
			return -EAGAIN;
	default:
		req->retval = 0;
		break;
	}
	mhl_msc_command_done(mhl_ctrl, req);
	pr_debug("%s: msc cmd done\n", __func__);
	return 0;

cbus_send_fail:
	return -EFAULT;
}

/* read scratchpad */
void mhl_read_scratchpad(struct mhl_tx_ctrl *mhl_ctrl)
{
	struct i2c_client *client = mhl_ctrl->i2c_handle;
	int i;

	for (i = 0; i < MHL_SCRATCHPAD_SIZE; i++) {
		mhl_ctrl->scrpd.data[i] = MHL_SII_REG_NAME_RD(
			REG_CBUS_SCRATCHPAD_0 + i);
	}
}

static void mhl_cbus_isr(struct mhl_tx_ctrl *mhl_ctrl)
{
	uint8_t regval;
	int req_done = 0;
	uint8_t sub_cmd = 0x0;
	uint8_t cmd_data = 0x0;
	int msc_msg_recved = 0;
	int rc = -1;
	unsigned long flags;
	struct i2c_client *client = mhl_ctrl->i2c_handle;

	regval = MHL_SII_REG_NAME_RD(REG_CBUS_INTR_STATUS);
	if (regval == 0xff)
		return;

	if (regval)
		MHL_SII_REG_NAME_WR(REG_CBUS_INTR_STATUS, regval);

	pr_debug("%s: CBUS_INT = %02x\n", __func__, regval);

	/* MSC_MSG (RCP/RAP) */
	if (regval & BIT3) {
		sub_cmd = MHL_SII_REG_NAME_RD(REG_CBUS_PRI_VS_CMD);
		cmd_data = MHL_SII_REG_NAME_RD(REG_CBUS_PRI_VS_DATA);
		msc_msg_recved = 1;
	}
	/* MSC_MT_ABRT/MSC_MR_ABRT/DDC_ABORT */
	if (regval & (BIT6 | BIT5 | BIT2))
		mhl_sii_cbus_process_errors(client, regval);

	/* MSC_REQ_DONE */
	if (regval & BIT4)
		req_done = 1;

	/* look for interrupts on CBUS_MSC_INT2 */
	regval  = MHL_SII_REG_NAME_RD(REG_CBUS_MSC_INT2_STATUS);

	/* clear all interrupts */
	if (regval)
		MHL_SII_REG_NAME_WR(REG_CBUS_MSC_INT2_STATUS, regval);

	pr_debug("%s: CBUS_MSC_INT2 = %02x\n", __func__, regval);

	/* received SET_INT */
	if (regval & BIT2) {
		uint8_t intr;
		intr = MHL_SII_REG_NAME_RD(REG_CBUS_SET_INT_0);
		MHL_SII_REG_NAME_WR(REG_CBUS_SET_INT_0, intr);
		mhl_msc_recv_set_int(mhl_ctrl, 0, intr);
		if (intr & MHL_INT_DCAP_CHG) {
			/* No need to go to low power mode */
			spin_lock_irqsave(&mhl_ctrl->lock, flags);
			mhl_ctrl->dwnstream_hpd = 0x00;
			pr_debug("%s: %u: dwnstrm_hpd=0x%02x\n",
				 __func__, __LINE__, mhl_ctrl->dwnstream_hpd);
			spin_unlock_irqrestore(&mhl_ctrl->lock, flags);
		}

		pr_debug("%s: MHL_INT_0 = %02x\n", __func__, intr);
		intr = MHL_SII_REG_NAME_RD(REG_CBUS_SET_INT_1);
		MHL_SII_REG_NAME_WR(REG_CBUS_SET_INT_1, intr);
		mhl_msc_recv_set_int(mhl_ctrl, 1, intr);

		pr_debug("%s: MHL_INT_1 = %02x\n", __func__, intr);
		MHL_SII_REG_NAME_WR(REG_CBUS_SET_INT_2, 0xFF);
		MHL_SII_REG_NAME_WR(REG_CBUS_SET_INT_3, 0xFF);
	}

	/* received WRITE_STAT */
	if (regval & BIT3) {
		uint8_t stat;
		stat = MHL_SII_REG_NAME_RD(REG_CBUS_WRITE_STAT_0);
		mhl_msc_recv_write_stat(mhl_ctrl, 0, stat);

		pr_debug("%s: MHL_STATUS_0 = %02x\n", __func__, stat);
		stat = MHL_SII_REG_NAME_RD(REG_CBUS_WRITE_STAT_1);
		mhl_msc_recv_write_stat(mhl_ctrl, 1, stat);
		pr_debug("%s: MHL_STATUS_1 = %02x\n", __func__, stat);

		MHL_SII_REG_NAME_WR(REG_CBUS_WRITE_STAT_0, 0xFF);
		MHL_SII_REG_NAME_WR(REG_CBUS_WRITE_STAT_1, 0xFF);
		MHL_SII_REG_NAME_WR(REG_CBUS_WRITE_STAT_2, 0xFF);
		MHL_SII_REG_NAME_WR(REG_CBUS_WRITE_STAT_3, 0xFF);
	}

	/* received MSC_MSG */
	if (msc_msg_recved) {
		/*mhl msc recv msc msg*/
		rc = mhl_msc_recv_msc_msg(mhl_ctrl, sub_cmd, cmd_data);
		if (rc)
			pr_err("MHL: mhl msc recv msc msg failed(%d)!\n", rc);
	}
	/* complete last command */
	if (req_done)
		complete_all(&mhl_ctrl->msc_cmd_done);

}

static irqreturn_t mhl_tx_isr(int irq, void *data)
{
	int rc;
	struct mhl_tx_ctrl *mhl_ctrl = (struct mhl_tx_ctrl *)data;
	unsigned long flags;

	pr_debug("%s: Getting Interrupts\n", __func__);

	spin_lock_irqsave(&mhl_ctrl->lock, flags);
	if (mhl_ctrl->tx_powered_off) {
		pr_warn("%s: powered off\n", __func__);
		spin_unlock_irqrestore(&mhl_ctrl->lock, flags);
		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&mhl_ctrl->lock, flags);

	/*
	 * Check RGND, MHL_EST, CBUS_LOCKOUT, SCDT
	 * interrupts. In D3, we get only RGND
	 */
	rc = dev_detect_isr(mhl_ctrl);
	if (rc)
		pr_debug("%s: dev_detect_isr rc=[%d]\n", __func__, rc);

	pr_debug("%s: cur pwr state is [0x%x]\n",
		 __func__, mhl_ctrl->cur_state);

	/*
	 * If dev_detect_isr() didn't move the tx to D3
	 * on disconnect, continue to check other
	 * interrupt sources.
	 */
	mhl_misc_isr(mhl_ctrl);

	/*
	 * Check for any peer messages for DCAP_CHG, MSC etc
	 * Dispatch to have the CBUS module working only
	 * once connected.
	 */
	mhl_cbus_isr(mhl_ctrl);
	mhl_hpd_stat_isr(mhl_ctrl);

	return IRQ_HANDLED;
}


static int mhl_sii_reg_config(struct i2c_client *client, bool enable)
{
	static struct regulator *reg_8941_l24;
	static struct regulator *reg_8941_l02;
	static struct regulator *reg_8941_smps3a;
	static struct regulator *reg_8941_vdda;
	int rc = -EINVAL;

	pr_debug("%s\n", __func__);
	if (!reg_8941_l24) {
		reg_8941_l24 = regulator_get(&client->dev,
			"avcc_18");
		if (IS_ERR(reg_8941_l24)) {
			pr_err("could not get 8941 l24, rc = %ld\n",
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
			goto l24_fail;
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
			goto l24_fail;
		}
		if (enable)
			rc = regulator_enable(reg_8941_l02);
		else
			rc = regulator_disable(reg_8941_l02);
		if (rc) {
			pr_debug("'%s' regulator configure[%u] failed, rc=%d\n",
				 "avcc_1.2V", enable, rc);
			goto l02_fail;
		} else {
			pr_debug("%s: vreg L02 %s\n",
				 __func__, (enable ? "enabled" : "disabled"));
		}
	}

	if (!reg_8941_smps3a) {
		reg_8941_smps3a = regulator_get(&client->dev,
			"smps3a");
		if (IS_ERR(reg_8941_smps3a)) {
			pr_err("could not get vreg smps3a, rc = %ld\n",
				PTR_ERR(reg_8941_smps3a));
			goto l02_fail;
		}
		if (enable)
			rc = regulator_enable(reg_8941_smps3a);
		else
			rc = regulator_disable(reg_8941_smps3a);
		if (rc) {
			pr_err("'%s' regulator config[%u] failed, rc=%d\n",
			       "SMPS3A", enable, rc);
			goto smps3a_fail;
		} else {
			pr_debug("%s: vreg SMPS3A %s\n",
				 __func__, (enable ? "enabled" : "disabled"));
		}
	}

	if (!reg_8941_vdda) {
		reg_8941_vdda = regulator_get(&client->dev,
			"vdda");
		if (IS_ERR(reg_8941_vdda)) {
			pr_err("could not get vreg vdda, rc = %ld\n",
				PTR_ERR(reg_8941_vdda));
			goto smps3a_fail;
		}
		if (enable)
			rc = regulator_enable(reg_8941_vdda);
		else
			rc = regulator_disable(reg_8941_vdda);
		if (rc) {
			pr_err("'%s' regulator config[%u] failed, rc=%d\n",
			       "VDDA", enable, rc);
			goto vdda_fail;
		} else {
			pr_debug("%s: vreg VDDA %s\n",
				 __func__, (enable ? "enabled" : "disabled"));
		}
	}

	return rc;

vdda_fail:
	regulator_disable(reg_8941_vdda);
	regulator_put(reg_8941_vdda);
smps3a_fail:
	regulator_disable(reg_8941_smps3a);
	regulator_put(reg_8941_smps3a);
l02_fail:
	regulator_disable(reg_8941_l02);
	regulator_put(reg_8941_l02);
l24_fail:
	regulator_disable(reg_8941_l24);
	regulator_put(reg_8941_l24);

	return -EINVAL;
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
			goto vreg_config_failed;
		}

		ret = mhl_sii_reg_config(client, true);
		if (ret) {
			pr_err("%s: regulator enable failed\n", __func__);
			goto vreg_config_failed;
		}
		pr_debug("%s: mhl sii power on successful\n", __func__);
	} else {
		pr_warn("%s: turning off pwr controls\n", __func__);
		mhl_sii_reg_config(client, false);
		gpio_free(pwr_gpio);
	}
	pr_debug("%s: successful\n", __func__);
	return 0;
vreg_config_failed:
	gpio_free(pwr_gpio);
	return -EINVAL;
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
	struct msm_hdmi_mhl_ops *hdmi_mhl_ops = NULL;

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
	mhl_ctrl->disc_enabled = false;
	INIT_WORK(&mhl_ctrl->mhl_msc_send_work, mhl_msc_send_work);
	mhl_ctrl->cur_state = POWER_STATE_D0_MHL;
	INIT_LIST_HEAD(&mhl_ctrl->list_cmd);
	init_completion(&mhl_ctrl->msc_cmd_done);
	spin_lock_init(&mhl_ctrl->lock);
	mhl_ctrl->msc_send_workqueue = create_singlethread_workqueue
		("mhl_msc_cmd_queue");

	mhl_ctrl->input = input_allocate_device();
	if (mhl_ctrl->input) {
		int i;
		struct input_dev *input = mhl_ctrl->input;

		mhl_ctrl->rcp_key_code_tbl = vmalloc(
			sizeof(support_rcp_key_code_tbl));
		if (!mhl_ctrl->rcp_key_code_tbl) {
			pr_err("%s: no alloc mem for rcp keycode tbl\n",
			       __func__);
			return -ENOMEM;
		}

		mhl_ctrl->rcp_key_code_tbl_len = sizeof(
			support_rcp_key_code_tbl);
		memcpy(mhl_ctrl->rcp_key_code_tbl,
		       &support_rcp_key_code_tbl[0],
		       mhl_ctrl->rcp_key_code_tbl_len);

		input->phys = "cbus/input0";
		input->id.bustype = BUS_VIRTUAL;
		input->id.vendor  = 0x1095;
		input->id.product = 0x8334;
		input->id.version = 0xA;

		input->name = "mhl-rcp";

		input->keycode = support_rcp_key_code_tbl;
		input->keycodesize = sizeof(u16);
		input->keycodemax = ARRAY_SIZE(support_rcp_key_code_tbl);

		input->evbit[0] = EV_KEY;
		for (i = 0; i < ARRAY_SIZE(support_rcp_key_code_tbl); i++) {
			if (support_rcp_key_code_tbl[i] > 1)
				input_set_capability(input, EV_KEY,
					support_rcp_key_code_tbl[i]);
		}

		if (input_register_device(input) < 0) {
			pr_warn("%s: failed to register input device\n",
				__func__);
			input_free_device(input);
			mhl_ctrl->input = NULL;
		}
	}

	mhl_ctrl->dwnstream_hpd = 0;
	mhl_ctrl->tx_powered_off = false;


	init_completion(&mhl_ctrl->rgnd_done);


	mhl_ctrl->mhl_psy.name = "ext-vbus";
	mhl_ctrl->mhl_psy.type = POWER_SUPPLY_TYPE_USB_DCP;
	mhl_ctrl->mhl_psy.supplied_to = mhl_pm_power_supplied_to;
	mhl_ctrl->mhl_psy.num_supplicants = ARRAY_SIZE(
					mhl_pm_power_supplied_to);
	mhl_ctrl->mhl_psy.properties = mhl_pm_power_props;
	mhl_ctrl->mhl_psy.num_properties = ARRAY_SIZE(mhl_pm_power_props);
	mhl_ctrl->mhl_psy.get_property = mhl_power_get_property;
	mhl_ctrl->mhl_psy.set_property = mhl_power_set_property;

	rc = power_supply_register(&client->dev, &mhl_ctrl->mhl_psy);
	if (rc < 0) {
		dev_err(&client->dev, "%s:power_supply_register ext_vbus_psy failed\n",
			__func__);
		goto failed_probe;
	}

	hdmi_mhl_ops = devm_kzalloc(&client->dev,
				    sizeof(struct msm_hdmi_mhl_ops),
				    GFP_KERNEL);
	if (!hdmi_mhl_ops) {
		pr_err("%s: alloc hdmi mhl ops failed\n", __func__);
		rc = -ENOMEM;
		goto failed_probe_pwr;
	}

	pr_debug("%s: i2c client addr is [%x]\n", __func__, client->addr);
	if (mhl_ctrl->pdata->hdmi_pdev) {
		rc = msm_hdmi_register_mhl(mhl_ctrl->pdata->hdmi_pdev,
					   hdmi_mhl_ops, mhl_ctrl);
		if (rc) {
			pr_err("%s: register with hdmi failed\n", __func__);
			rc = -EPROBE_DEFER;
			goto failed_probe_pwr;
		}
	}

	if (!hdmi_mhl_ops || !hdmi_mhl_ops->tmds_enabled ||
	    !hdmi_mhl_ops->set_mhl_max_pclk) {
		pr_err("%s: func ptr is NULL\n", __func__);
		rc = -EINVAL;
		goto failed_probe_pwr;
	}
	mhl_ctrl->hdmi_mhl_ops = hdmi_mhl_ops;

	rc = hdmi_mhl_ops->set_mhl_max_pclk(
		mhl_ctrl->pdata->hdmi_pdev, MAX_MHL_PCLK);
	if (rc) {
		pr_err("%s: can't set max mhl pclk\n", __func__);
		goto failed_probe_pwr;
	}

	mhl_info = devm_kzalloc(&client->dev, sizeof(*mhl_info), GFP_KERNEL);
	if (!mhl_info) {
		pr_err("%s: alloc mhl info failed\n", __func__);
		rc = -ENOMEM;
		goto failed_probe_pwr;
	}

	mhl_info->ctxt = mhl_ctrl;
	mhl_info->notify = mhl_sii_device_discovery;
	if (msm_register_usb_ext_notification(mhl_info)) {
		pr_err("%s: register for usb notifcn failed\n", __func__);
		rc = -EPROBE_DEFER;
		goto failed_probe_pwr;
	}
	mhl_ctrl->mhl_info = mhl_info;
	mhl_register_msc(mhl_ctrl);
	return 0;

failed_probe_pwr:
	power_supply_unregister(&mhl_ctrl->mhl_psy);
failed_probe:
	free_irq(mhl_ctrl->i2c_handle->irq, mhl_ctrl);
	mhl_gpio_config(mhl_ctrl, 0);
	mhl_vreg_config(mhl_ctrl, 0);
	/* do not deep-free */
	if (mhl_info)
		devm_kfree(&client->dev, mhl_info);
failed_dt_data:
	if (pdata)
		devm_kfree(&client->dev, pdata);
failed_no_mem:
	if (mhl_ctrl)
		devm_kfree(&client->dev, mhl_ctrl);
	mhl_info = NULL;
	pdata = NULL;
	mhl_ctrl = NULL;
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

#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)
static int mhl_i2c_suspend_sub(struct i2c_client *client)
{
	struct mhl_tx_ctrl *mhl_ctrl = i2c_get_clientdata(client);

	if (mhl_ctrl->irq_req_done) {
		enable_irq_wake(client->irq);
		disable_irq(client->irq);
	}
	return 0;
}

static int mhl_i2c_resume_sub(struct i2c_client *client)
{
	struct mhl_tx_ctrl *mhl_ctrl = i2c_get_clientdata(client);

	if (mhl_ctrl->irq_req_done)
		disable_irq_wake(client->irq);
	return 0;
}
#endif /* defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP) */

#if defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP)
static int mhl_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
	if (!client)
		return -ENODEV;
	pr_debug("%s: mhl suspend\n", __func__);
	return mhl_i2c_suspend_sub(client);
}

static int mhl_i2c_resume(struct i2c_client *client)
{
	if (!client)
		return -ENODEV;
	pr_debug("%s: mhl resume\n", __func__);
	return mhl_i2c_resume_sub(client);
}
#else
#define mhl_i2c_suspend NULL
#define mhl_i2c_resume NULL
#endif /* defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP) */

#ifdef CONFIG_PM_SLEEP
static int mhl_i2c_pm_suspend(struct device *dev)
{
	struct i2c_client *client =
		container_of(dev, struct i2c_client, dev);

	if (!client)
		return -ENODEV;
	pr_debug("%s: mhl pm suspend\n", __func__);
	return mhl_i2c_suspend_sub(client);

}

static int mhl_i2c_pm_resume(struct device *dev)
{
	struct i2c_client *client =
		container_of(dev, struct i2c_client, dev);

	if (!client)
		return -ENODEV;
	pr_debug("%s: mhl pm resume\n", __func__);
	return mhl_i2c_resume_sub(client);
}

static const struct dev_pm_ops mhl_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mhl_i2c_pm_suspend, mhl_i2c_pm_resume)
};
#endif /* CONFIG_PM_SLEEP */

static struct of_device_id mhl_match_table[] = {
	{.compatible = COMPATIBLE_NAME,},
	{ },
};

static struct i2c_driver mhl_sii_i2c_driver = {
	.driver = {
		.name = MHL_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mhl_match_table,
#ifdef CONFIG_PM_SLEEP
		.pm = &mhl_i2c_pm_ops,
#endif /* CONFIG_PM_SLEEP */
	},
	.probe = mhl_i2c_probe,
	.remove =  mhl_i2c_remove,
#if defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP)
	.suspend = mhl_i2c_suspend,
	.resume = mhl_i2c_resume,
#endif /* defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP) */
	.id_table = mhl_sii_i2c_id,
};

module_i2c_driver(mhl_sii_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MHL SII 8334 TX Driver");
