/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include "msm_dba_internal.h"

#define ADV7533_REG_CHIP_REVISION (0x00)
#define ADV7533_DSI_CEC_I2C_ADDR_REG (0xE1)
#define ADV7533_RESET_DELAY (100)

#define PINCTRL_STATE_ACTIVE    "pmx_adv7533_active"
#define PINCTRL_STATE_SUSPEND   "pmx_adv7533_suspend"

#define MDSS_MAX_PANEL_LEN      256
#define EDID_SEG_SIZE 0x100

#define HPD_INT_ENABLE           BIT(7)
#define MONITOR_SENSE_INT_ENABLE BIT(6)
#define EDID_READY_INT_ENABLE    BIT(2)

#define MAX_WAIT_TIME (100)
#define MAX_RW_TRIES (3)

enum adv7533_i2c_addr {
	ADV7533_MAIN = 0x3D,
	ADV7533_CEC_DSI = 0x3C,
};

enum adv7533_video_mode {
	ADV7533_VIDEO_PATTERN,
	ADV7533_VIDEO_480P,
	ADV7533_VIDEO_720P,
	ADV7533_VIDEO_1080P,
};

struct adv7533_reg_cfg {
	u8 i2c_addr;
	u8 reg;
	u8 val;
	int sleep_in_ms;
};

struct adv7533_platform_data {
	u8 main_i2c_addr;
	u8 cec_dsi_i2c_addr;
	u8 video_mode;
	int irq;
	u32 irq_gpio;
	u32 irq_flags;
	u32 hpd_irq_gpio;
	u32 hpd_irq_flags;
	u32 switch_gpio;
	u32 switch_flags;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	bool audio;
	bool disable_gpios;
	bool adv_output;
	void *edid_data;
	u8 edid_buf[EDID_SEG_SIZE];
	struct workqueue_struct *workq;
	struct delayed_work adv7533_intr_work_id;
	struct msm_dba_device_info dev_info;
	msm_dba_cb client_cb;
	void *client_cb_data;
};

static struct adv7533_reg_cfg adv7533_init_setup[] = {
	/* HPD override */
	{ADV7533_MAIN, 0xD6, 0x48, 5},
	/* power down */
	{ADV7533_MAIN, 0x41, 0x50, 5},
	/* color space */
	{ADV7533_MAIN, 0x16, 0x20, 0},
	/* Fixed */
	{ADV7533_MAIN, 0x9A, 0xE0, 0},
	/* HDCP */
	{ADV7533_MAIN, 0xBA, 0x70, 0},
	/* Fixed */
	{ADV7533_MAIN, 0xDE, 0x82, 0},
	/* V1P2 */
	{ADV7533_MAIN, 0xE4, 0x40, 0},
	/* Fixed */
	{ADV7533_MAIN, 0xE5, 0x80, 0},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x15, 0xD0, 0},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x17, 0xD0, 0},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x24, 0x20, 0},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x57, 0x11, 0},
};

static struct adv7533_reg_cfg adv7533_video_setup[] = {
	/* power up */
	{ADV7533_MAIN, 0x41, 0x10, 0},
	/* hdmi enable */
	{ADV7533_CEC_DSI, 0x03, 0x89, 0},
	/* hdmi mode, hdcp */
	{ADV7533_MAIN, 0xAF, 0x06, 0},
	/* color depth */
	{ADV7533_MAIN, 0x4C, 0x04, 0},
	/* down dither */
	{ADV7533_MAIN, 0x49, 0x02, 0},
};

static struct adv7533_reg_cfg I2S_cfg[] = {
	{ADV7533_MAIN, 0x0D, 0x18},	/* Bit width = 16Bits*/
	{ADV7533_MAIN, 0x15, 0x20},	/* Sampling Frequency = 48kHz*/
	{ADV7533_MAIN, 0x02, 0x18},	/* N value 6144 --> 0x1800*/
	{ADV7533_MAIN, 0x14, 0x02},	/* Word Length = 16Bits*/
	{ADV7533_MAIN, 0x73, 0x01},	/* Channel Count = 2 channels */
};

static struct adv7533_reg_cfg irq_config[] = {
	{ADV7533_CEC_DSI, 0x38, BIT(6)},
	{ADV7533_MAIN, 0x94, HPD_INT_ENABLE |
		MONITOR_SENSE_INT_ENABLE |
		EDID_READY_INT_ENABLE},
};

static struct i2c_client *client;
static struct adv7533_platform_data *pdata;
static char mdss_mdp_panel[MDSS_MAX_PANEL_LEN];

static int adv7533_read(u8 addr, u8 reg, u8 *buf, u8 len)
{
	int ret = 0, i = 0;
	struct i2c_msg msg[2];

	if (!client) {
		pr_err("%s: no adv7533 i2c client\n", __func__);
		ret = -ENODEV;
		goto r_err;
	}

	if (NULL == buf) {
		pr_err("%s: no adv7533 i2c client\n", __func__);
		ret = -EINVAL;
		goto r_err;
	}

	client->addr = addr;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	do {
		if (i2c_transfer(client->adapter, msg, 2) == 2) {
			ret = 0;
			goto r_err;
		}
		msleep(MAX_WAIT_TIME);
	} while (++i < MAX_RW_TRIES);

	ret = -EIO;
	pr_err("%s adv7533 i2c read failed after %d tries\n", __func__,
		MAX_RW_TRIES);

r_err:
	return ret;
}

int adv7533_read_byte(u8 addr, u8 reg, u8 *buf)
{
	return adv7533_read(addr, reg, buf, 1);
}

static int adv7533_write_byte(u8 addr, u8 reg, u8 val)
{
	int ret = 0, i = 0;
	u8 buf[2] = {reg, val};
	struct i2c_msg msg[1];

	if (!client) {
		pr_err("%s: no adv7533 i2c client\n", __func__);
		ret = -ENODEV;
		goto w_err;
	}

	client->addr = addr;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	do {
		if (i2c_transfer(client->adapter, msg, 1) >= 1) {
			ret = 0;
			goto w_err;
		}
		msleep(MAX_WAIT_TIME);
	} while (++i < MAX_RW_TRIES);

	ret = -EIO;
	pr_err("%s: adv7533 i2c write failed after %d tries\n", __func__,
		MAX_RW_TRIES);

w_err:
	if (ret != 0)
		pr_err("%s: Exiting with ret = %d after %d retries\n",
			__func__, ret, i);
	else
		pr_debug("[%s,%d] I2C write(0x%02x) [0x%02x]=0x%02x\n",
			__func__, __LINE__, addr, reg, val);
	return ret;
}

static int adv7533_read_reg(struct msm_dba_device_info *dev, u32 reg, u32 *val)
{
	int rc = 0;
	u8 byte_val = 0, addr = 0;
	struct adv7533_platform_data *pdata = NULL;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = container_of(dev, struct adv7533_platform_data, dev_info);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	addr = (reg & 0x0100)?(pdata->cec_dsi_i2c_addr):(pdata->main_i2c_addr);
	rc = adv7533_read_byte(addr, (reg & 0xff), &byte_val);
	if (rc) {
		pr_err("%s: read reg=0x%02x failed @ addr=0x%02x\n",
			__func__, reg, addr);
	} else {
		pr_debug("%s: read reg=0x%02x ok value=0x%02x @ addr=0x%02x\n",
			__func__, reg, byte_val, addr);
		*val = (u32)byte_val;
	}

	return rc;
}

static int adv7533_write_reg(struct msm_dba_device_info *dev, u32 reg, u32 val)
{
	int rc = 0;
	u8 addr = 0;
	struct adv7533_platform_data *pdata = NULL;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = container_of(dev, struct adv7533_platform_data, dev_info);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	addr = (reg & 0x0100)?(pdata->cec_dsi_i2c_addr):(pdata->main_i2c_addr);
	rc = adv7533_write_byte(addr, (reg & 0xff), val);
	if (rc)
		pr_err("%s: write reg=0x%02x failed @ addr=0x%02x\n",
			__func__, reg, pdata->main_i2c_addr);
	else
		pr_debug("%s: write reg=0x%02x ok value=0x%02x @ addr=0x%02x\n",
			__func__, reg, val, pdata->main_i2c_addr);

	return rc;
}

static int adv7533_dump_debug_info(struct msm_dba_device_info *dev, u32 flags)
{
	int rc = 0;
	u8 byte_val = 0;
	u16 addr = 0;
	struct adv7533_platform_data *pdata = NULL;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = container_of(dev, struct adv7533_platform_data, dev_info);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	/* dump main addr*/
	pr_err("========Main I2C=0x%02x Start==========\n",
		pdata->main_i2c_addr);
	for (addr = 0; addr <= 0xFF; addr++) {
		rc = adv7533_read_byte(pdata->main_i2c_addr,
			(u8)addr, &byte_val);
		if (rc)
			pr_err("%s: read reg=0x%02x failed @ addr=0x%02x\n",
				__func__, addr, pdata->main_i2c_addr);
		else
			pr_err("0x%02x -> 0x%02X\n", addr, byte_val);
	}
	pr_err("========Main I2C=0x%02x End==========\n",
		pdata->main_i2c_addr);
	/* dump CEC addr*/
	pr_err("=======CEC I2C=0x%02x Start=========\n",
		pdata->cec_dsi_i2c_addr);
	for (addr = 0; addr <= 0xFF; addr++) {
		rc = adv7533_read_byte(pdata->cec_dsi_i2c_addr,
			(u8)addr, &byte_val);
		if (rc)
			pr_err("%s: read reg=0x%02x failed @ addr=0x%02x\n",
				__func__, addr, pdata->cec_dsi_i2c_addr);
		else
			pr_err("0x%02x -> 0x%02X\n", addr, byte_val);
	}
	pr_err("========CEC I2C=0x%02x End==========\n",
		pdata->cec_dsi_i2c_addr);

	return rc;
}

static int adv7533_write_regs(struct adv7533_platform_data *pdata,
	struct adv7533_reg_cfg *cfg, int size)
{
	int ret = 0;
	int i;

	for (i = 0; i < size; i++) {
		switch (cfg[i].i2c_addr) {
		case ADV7533_MAIN:
			ret = adv7533_write_byte(pdata->main_i2c_addr,
				cfg[i].reg, cfg[i].val);
			if (ret != 0)
				pr_err("%s: adv7533_write_byte returned %d\n",
					__func__, ret);
			break;
		case ADV7533_CEC_DSI:
			ret = adv7533_write_byte(pdata->cec_dsi_i2c_addr,
				cfg[i].reg, cfg[i].val);
			if (ret != 0)
				pr_err("%s: adv7533_write_byte returned %d\n",
					__func__, ret);
			break;
		default:
			ret = -EINVAL;
			pr_err("%s: Default case? BUG!\n", __func__);
			break;
		}
		if (ret != 0) {
			pr_err("%s: adv7533 reg writes failed. ", __func__);
			pr_err("Last write %02X to %02X\n",
				cfg[i].val, cfg[i].reg);
			goto w_regs_fail;
		}
		if (cfg[i].sleep_in_ms)
			msleep(cfg[i].sleep_in_ms);
	}

w_regs_fail:
	if (ret != 0)
		pr_err("%s: Exiting with ret = %d after %d writes\n",
			__func__, ret, i);
	return ret;
}

static int adv7533_read_device_rev(struct adv7533_platform_data *pdata)
{
	u8 rev = 0;
	int ret;

	ret = adv7533_read_byte(pdata->main_i2c_addr, ADV7533_REG_CHIP_REVISION,
							&rev);

	if (!ret)
		pr_debug("%s: adv7533 revision 0x%X\n", __func__, rev);
	else
		pr_err("%s: adv7533 rev error\n", __func__);

	return ret;
}

static int adv7533_program_i2c_addr(struct adv7533_platform_data *pdata)
{
	u8 i2c_8bits = pdata->cec_dsi_i2c_addr << 1;
	int ret = 0;

	if (pdata->cec_dsi_i2c_addr != ADV7533_CEC_DSI) {
		ret = adv7533_write_byte(pdata->main_i2c_addr,
					ADV7533_DSI_CEC_I2C_ADDR_REG,
					i2c_8bits);

		if (ret)
			pr_err("%s: write err CEC_ADDR[0x%02x] main_addr=0x%02x\n",
				__func__, ADV7533_DSI_CEC_I2C_ADDR_REG,
				pdata->main_i2c_addr);
	}

	return ret;
}

static int adv7533_parse_dt(struct device *dev,
	struct adv7533_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;
	int ret = 0;

	ret = of_property_read_u32(np, "instance_id", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "instance_id",
		temp_val);
	if (ret)
		goto end;
	pdata->dev_info.instance_id = temp_val;

	ret = of_property_read_u32(np, "adi,main-addr", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adi,main-addr",
		temp_val);
	if (ret)
		goto end;
	pdata->main_i2c_addr = (u8)temp_val;

	ret = of_property_read_u32(np, "adi,cec-dsi-addr", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adi,cec-dsi-addr",
		temp_val);
	if (ret)
		goto end;
	pdata->cec_dsi_i2c_addr = (u8)temp_val;

	ret = of_property_read_u32(np, "adi,video-mode", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adi,video-mode",
		temp_val);
	if (ret)
		goto end;
	pdata->video_mode = (u8)temp_val;

	pdata->audio = of_property_read_bool(np, "adi,enable-audio");

	/* Get pinctrl if target uses pinctrl */
	pdata->ts_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pdata->ts_pinctrl)) {
		ret = PTR_ERR(pdata->ts_pinctrl);
		pr_err("%s: Pincontrol DT property returned %X\n",
			__func__, ret);
	}

	pdata->pinctrl_state_active = pinctrl_lookup_state(pdata->ts_pinctrl,
		"pmx_adv7533_active");
	if (IS_ERR_OR_NULL(pdata->pinctrl_state_active)) {
		ret = PTR_ERR(pdata->pinctrl_state_active);
		pr_err("Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, ret);
	}

	pdata->pinctrl_state_suspend = pinctrl_lookup_state(pdata->ts_pinctrl,
		"pmx_adv7533_suspend");
	if (IS_ERR_OR_NULL(pdata->pinctrl_state_suspend)) {
		ret = PTR_ERR(pdata->pinctrl_state_suspend);
		pr_err("Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, ret);
	}

	pdata->disable_gpios = of_property_read_bool(np,
			"adi,disable-gpios");

	if (!(pdata->disable_gpios)) {
		pdata->irq_gpio = of_get_named_gpio_flags(np,
				"adi,irq-gpio", 0, &pdata->irq_flags);

		pdata->hpd_irq_gpio = of_get_named_gpio_flags(np,
				"adi,hpd-irq-gpio", 0,
				&pdata->hpd_irq_flags);

		pdata->switch_gpio = of_get_named_gpio_flags(np,
				"adi,switch-gpio", 0, &pdata->switch_flags);
	}

end:
	return ret;
}

static int adv7533_gpio_configure(struct adv7533_platform_data *pdata,
	bool on)
{
	int ret = 0;

	if (pdata->disable_gpios)
		return 0;

	if (on) {
		if (gpio_is_valid(pdata->irq_gpio)) {
			ret = gpio_request(pdata->irq_gpio, "adv7533_irq_gpio");
			if (ret) {
				pr_err("%d unable to request gpio [%d] ret=%d\n",
					__LINE__, pdata->irq_gpio, ret);
				goto err_none;
			}
			ret = gpio_direction_input(pdata->irq_gpio);
			if (ret) {
				pr_err("unable to set dir for gpio[%d]\n",
					pdata->irq_gpio);
				goto err_irq_gpio;
			}
		} else {
			pr_err("irq gpio not provided\n");
			goto err_none;
		}

		if (gpio_is_valid(pdata->switch_gpio)) {
			ret = gpio_request(pdata->switch_gpio,
				"adv7533_switch_gpio");
			if (ret) {
				pr_err("%d unable to request gpio [%d] ret=%d\n",
					__LINE__, pdata->irq_gpio, ret);
				goto err_hpd_irq_gpio;
			}

			ret = gpio_direction_output(pdata->switch_gpio, 0);
			if (ret) {
				pr_err("unable to set dir for gpio [%d]\n",
					pdata->switch_gpio);
				goto err_switch_gpio;
			}

			gpio_set_value(pdata->switch_gpio,
				!pdata->switch_flags);
			msleep(ADV7533_RESET_DELAY);
		}

		 goto err_none;
	} else {
		if (gpio_is_valid(pdata->irq_gpio))
			gpio_free(pdata->irq_gpio);
		if (gpio_is_valid(pdata->hpd_irq_gpio))
			gpio_free(pdata->hpd_irq_gpio);
		if (gpio_is_valid(pdata->switch_gpio))
			gpio_free(pdata->switch_gpio);

		goto err_none;
	}

err_switch_gpio:
	if (gpio_is_valid(pdata->switch_gpio))
		gpio_free(pdata->switch_gpio);
err_hpd_irq_gpio:
	if (gpio_is_valid(pdata->hpd_irq_gpio))
		gpio_free(pdata->hpd_irq_gpio);
err_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_none:
	return ret;
}

u32 adv7533_read_edid(struct adv7533_platform_data *pdata,
	u32 size, char *edid_buf)
{
	u32 ret = 0, ndx;
	u8 edid_addr;
	u32 read_size = size / 2;

	if (!edid_buf)
		return 0;

	pr_debug("%s: size %d\n", __func__, size);

	ret = adv7533_read(pdata->main_i2c_addr, 0x43, &edid_addr, 1);
	if (ret) {
		pr_err("%s: Error reading edid addr\n", __func__);
		goto end;
	}

	pr_debug("%s: edid address 0x%x\n", __func__, edid_addr);

	ret = adv7533_read(edid_addr >> 1, 0x00, edid_buf, read_size);
	if (ret) {
		pr_err("%s: Error reading edid 0\n", __func__);
		goto end;
	}

	ret = adv7533_read(edid_addr >> 1, read_size,
		edid_buf + read_size, read_size);
	if (ret) {
		pr_err("%s: Error reading edid 1\n", __func__);
		goto end;
	}

	for (ndx = 0; ndx < size; ndx += 4)
		pr_info("%s: EDID[%02x-%02x] %02x %02x %02x %02x\n",
			__func__, ndx, ndx + 3,
			edid_buf[ndx + 0], edid_buf[ndx + 1],
			edid_buf[ndx + 2], edid_buf[ndx + 3]);
end:
	return ret;
}

static void adv7533_intr_work(struct work_struct *work)
{
	u8 int_status;
	u8 int_state;
	int ret;
	struct adv7533_platform_data *pdata;
	struct delayed_work *dw = to_delayed_work(work);

	pdata = container_of(dw, struct adv7533_platform_data,
			adv7533_intr_work_id);
	if (!pdata) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	/* READ Interrupt registers */
	adv7533_read_byte(pdata->main_i2c_addr, 0x96, &int_status);
	adv7533_read_byte(pdata->main_i2c_addr, 0x42, &int_state);

	if (int_status & BIT(6)) {
		if (int_state & BIT(5)) {
			pr_info("%s: Rx SENSE CABLE CONNECTED\n", __func__);

			/* initiate edid read in adv7533 */
			ret = adv7533_write_byte(pdata->main_i2c_addr,
				0xC9, 0x13);
			if (ret) {
				pr_err("%s: Failed: edid read adv %d\n",
					__func__, ret);
				pdata->client_cb(pdata->client_cb_data,
					MSM_DBA_CB_HPD_CONNECT);
			}

		} else {
			pr_info("%s: NO Rx SENSE CABLE DISCONNECTED\n",
				__func__);
			pdata->client_cb(pdata->client_cb_data,
				MSM_DBA_CB_HPD_DISCONNECT);
		}
	}

	/* EDID ready for read */
	if (int_status & BIT(2)) {
		pr_info("%s: EDID READY\n", __func__);

		ret = adv7533_read_edid(pdata, sizeof(pdata->edid_buf),
			pdata->edid_buf);
		if (ret)
			pr_err("%s: edid read failed\n", __func__);

		pdata->client_cb(pdata->client_cb_data,
			MSM_DBA_CB_HPD_CONNECT);
	}

	/* Clear interrupts */
	ret = adv7533_write_byte(pdata->main_i2c_addr, 0x96, int_status);
	if (ret)
		pr_err("%s: Failed: clear interrupts %d\n", __func__, ret);

	ret = adv7533_write_regs(pdata, irq_config, ARRAY_SIZE(irq_config));
	if (ret)
		pr_err("%s: Failed: irq config %d\n", __func__, ret);
}

static irqreturn_t adv7533_irq(int irq, void *data)
{
	int ret;

	/* disable interrupts */
	ret = adv7533_write_byte(pdata->main_i2c_addr, 0x94, 0x00);
	if (ret)
		pr_err("%s: Failed: disable interrupts %d\n",
			__func__, ret);

	queue_delayed_work(pdata->workq, &pdata->adv7533_intr_work_id, 0);

	return IRQ_HANDLED;
}

static struct i2c_device_id adv7533_id[] = {
	{ "adv7533", 0},
	{}
};

static struct adv7533_platform_data *adv7533_get_platform_data(void *client)
{
	struct adv7533_platform_data *pdata = NULL;
	struct msm_dba_device_info *dev;
	struct msm_dba_client_info *cinfo =
		(struct msm_dba_client_info *)client;

	if (!cinfo) {
		pr_err("%s: invalid client data\n", __func__);
		goto end;
	}

	dev = cinfo->dev;
	if (!dev) {
		pr_err("%s: invalid device data\n", __func__);
		goto end;
	}

	pdata = container_of(dev, struct adv7533_platform_data, dev_info);
	if (!pdata)
		pr_err("%s: invalid platform data\n", __func__);

end:
	return pdata;
}

/* Device Operations */
static int adv7533_power_on(void *client, bool on, u32 flags)
{
	int ret = 0;
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	ret = adv7533_write_regs(pdata, adv7533_init_setup,
		ARRAY_SIZE(adv7533_init_setup));
	if (ret) {
		pr_err("%s: Failed to write common config\n",
			__func__);
		goto end;
	}

	ret = adv7533_write_regs(pdata, irq_config, ARRAY_SIZE(irq_config));
	if (ret) {
		pr_err("%s: Failed: irq config %d\n", __func__, ret);
		goto end;
	}

	if (pdata->workq)
		queue_delayed_work(pdata->workq, &pdata->adv7533_intr_work_id,
					HZ/2);

end:
	return ret;
}

static void adv7533_video_timing_setup(struct adv7533_platform_data *pdata,
	struct msm_dba_video_cfg *cfg)
{
	u32 h_total, hpw, hfp, hbp;
	u32 v_total, vpw, vfp, vbp;
	u32 num_lanes;
	u8 addr = 0;

	if (!pdata || !cfg) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	h_total = cfg->h_active + cfg->h_front_porch +
	      cfg->h_pulse_width + cfg->h_back_porch;
	v_total = cfg->v_active + cfg->v_front_porch +
	      cfg->v_pulse_width + cfg->v_back_porch;

	hpw = cfg->h_pulse_width;
	hfp = cfg->h_front_porch;
	hbp = cfg->h_back_porch;

	vpw = cfg->v_pulse_width;
	vfp = cfg->v_front_porch;
	vbp = cfg->v_back_porch;
	addr = pdata->cec_dsi_i2c_addr;
	num_lanes = cfg->num_of_input_lanes;

	pr_debug("h_total 0x%x, h_active 0x%x, hfp 0x%d, hpw 0x%x, hbp 0x%x\n",
		h_total, cfg->h_active, cfg->h_front_porch,
		cfg->h_pulse_width, cfg->h_back_porch);

	pr_debug("v_total 0x%x, v_active 0x%x, vfp 0x%x, vpw 0x%x, vbp 0x%x\n",
		v_total, cfg->v_active, cfg->v_front_porch,
		cfg->v_pulse_width, cfg->v_back_porch);

	/* lane setup */
	adv7533_write_byte(addr, 0x1C, ((num_lanes & 0xF) << 4));

	/* h_width */
	adv7533_write_byte(addr, 0x28, ((h_total & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x29, ((h_total & 0xF) << 4));

	/* hsync_width */
	adv7533_write_byte(addr, 0x2A, ((hpw & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x2B, ((hpw & 0xF) << 4));

	/* hfp */
	adv7533_write_byte(addr, 0x2C, ((hfp & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x2D, ((hfp & 0xF) << 4));

	/* hbp */
	adv7533_write_byte(addr, 0x2E, ((hbp & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x2F, ((hbp & 0xF) << 4));

	/* v_total */
	adv7533_write_byte(addr, 0x30, ((v_total & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x31, ((v_total & 0xF) << 4));

	/* vsync_width */
	adv7533_write_byte(addr, 0x32, ((vpw & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x33, ((vpw & 0xF) << 4));

	/* vfp */
	adv7533_write_byte(addr, 0x34, ((vfp & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x35, ((vfp & 0xF) << 4));

	/* vbp */
	adv7533_write_byte(addr, 0x36, ((vbp & 0xFF0) >> 4));
	adv7533_write_byte(addr, 0x37, ((vbp & 0xF) << 4));
}

static int adv7533_video_on(void *client, bool on,
	struct msm_dba_video_cfg *cfg, u32 flags)
{
	int ret = 0;
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	adv7533_video_timing_setup(pdata, cfg);

	ret = adv7533_write_regs(pdata, adv7533_video_setup,
		ARRAY_SIZE(adv7533_video_setup));
	if (ret)
		pr_err("%s: Failed: video setup\n", __func__);
end:
	return ret;
}

static int adv7533_get_edid_size(void *client,
	u32 *size, u32 flags)
{
	int ret = 0;

	if (!size) {
		ret = -EINVAL;
		goto end;
	}

	*size = EDID_SEG_SIZE;

end:
	return ret;
}

static int adv7533_get_raw_edid(void *client,
	u32 size, char *buf, u32 flags)
{
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data(client);

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		goto end;
	}

	size = size > sizeof(pdata->edid_buf) ? sizeof(pdata->edid_buf) : size;

	memcpy(buf, pdata->edid_buf, size);
end:
	return 0;
}

static int adv7533_reg_fxn(struct msm_dba_client_info *cinfo)
{
	int ret = 0;
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data((void *)cinfo);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	pdata->client_cb = cinfo->cb;
	pdata->client_cb_data = cinfo->cb_data;

end:
	return ret;
}

static int adv7533_register_dba(struct adv7533_platform_data *pdata)
{
	struct msm_dba_ops *client_ops;

	if (!pdata)
		return -EINVAL;

	client_ops = &pdata->dev_info.client_ops;

	client_ops->power_on = adv7533_power_on;
	client_ops->video_on = adv7533_video_on;
	client_ops->get_edid_size = adv7533_get_edid_size;
	client_ops->get_raw_edid = adv7533_get_raw_edid;
	pdata->dev_info.dev_ops.read_reg = adv7533_read_reg;
	pdata->dev_info.dev_ops.write_reg = adv7533_write_reg;
	pdata->dev_info.dev_ops.dump_debug_info = adv7533_dump_debug_info;

	strlcpy(pdata->dev_info.chip_name, "adv7533",
		sizeof(pdata->dev_info.chip_name));

	pdata->dev_info.reg_fxn = adv7533_reg_fxn;

	mutex_init(&pdata->dev_info.dev_mutex);

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	return msm_dba_add_probed_device(&pdata->dev_info);
}

static void adv7533_unregister_dba(struct adv7533_platform_data *pdata)
{
	if (!pdata)
		return;

	msm_dba_remove_probed_device(&pdata->dev_info);
}

static int adv7533_probe(struct i2c_client *client_,
	 const struct i2c_device_id *id)
{
	int ret = 0;

	client = client_;
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct adv7533_platform_data), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		ret = adv7533_parse_dt(&client->dev, pdata);
		if (ret) {
			pr_err("%s: Failed to parse DT\n", __func__);
			goto err_dt_parse;
		}
	}

	ret = adv7533_read_device_rev(pdata);
	if (ret != 0) {
		pr_err("%s: Failed to read revision\n", __func__);
		goto err_dt_parse;
	}

	ret = adv7533_program_i2c_addr(pdata);
	if (ret != 0) {
		pr_err("%s: Failed to program i2c addr\n", __func__);
		goto err_dt_parse;
	}

	ret = adv7533_register_dba(pdata);
	if (ret) {
		pr_err("%s: Error registering with DBA %d\n",
			__func__, ret);
		goto err_dba_reg;
	}

	ret = pinctrl_select_state(pdata->ts_pinctrl,
		pdata->pinctrl_state_active);
	if (ret < 0) {
		pr_err("%s: Failed to select %s pinstate %d\n",
			__func__, PINCTRL_STATE_ACTIVE, ret);
		goto err_dba_reg;
	}

	pdata->adv_output = true;

	if (!(pdata->disable_gpios)) {
		ret = adv7533_gpio_configure(pdata, true);
		if (ret) {
			pr_err("%s: Failed to configure GPIOs\n", __func__);
			goto err_gpio_cfg;
		}

		if (pdata->adv_output) {
			gpio_set_value(pdata->switch_gpio, pdata->switch_flags);
		} else {
			gpio_set_value(pdata->switch_gpio,
				!pdata->switch_flags);
			goto err_gpio_cfg;
		}
	}

	pdata->irq = gpio_to_irq(pdata->irq_gpio);

	ret = request_threaded_irq(pdata->irq, NULL, adv7533_irq,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "adv7533", pdata);
	if (ret) {
		pr_err("%s: Failed to enable ADV7533 interrupt\n",
			__func__);
		goto err_irq;
	}

	if (pdata->audio) {
		ret = adv7533_write_regs(pdata, I2S_cfg, ARRAY_SIZE(I2S_cfg));
		if (ret != 0) {
			pr_err("%s: I2S configuration fail = %d!\n",
				__func__, ret);
			goto err_dba_helper;
		}
	}

	dev_set_drvdata(&client->dev, &pdata->dev_info);
	ret = msm_dba_helper_sysfs_init(&client->dev);
	if (ret) {
		pr_err("%s: sysfs init failed\n", __func__);
		goto err_dba_helper;
	}

	pdata->workq = create_workqueue("adv7533_workq");
	if (!pdata->workq) {
		pr_err("%s: workqueue creation failed.\n", __func__);
		ret = -EPERM;
		goto err_workqueue;
	}

	INIT_DELAYED_WORK(&pdata->adv7533_intr_work_id, adv7533_intr_work);

	pm_runtime_enable(&client->dev);
	pm_runtime_set_active(&client->dev);

	return 0;

err_workqueue:
	msm_dba_helper_sysfs_remove(&client->dev);
err_dba_helper:
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);
err_irq:
	adv7533_gpio_configure(pdata, false);
err_gpio_cfg:
	adv7533_unregister_dba(pdata);
err_dba_reg:
err_dt_parse:
	devm_kfree(&client->dev, pdata);
	return ret;
}

static int adv7533_remove(struct i2c_client *client)
{
	int ret = 0;

	pm_runtime_disable(&client->dev);
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);

	ret = adv7533_gpio_configure(pdata, false);

	devm_kfree(&client->dev, pdata);
	return ret;
}

static struct i2c_driver adv7533_driver = {
	.driver = {
		.name = "adv7533",
		.owner = THIS_MODULE,
	},
	.probe = adv7533_probe,
	.remove = adv7533_remove,
	.id_table = adv7533_id,
};

static int __init adv7533_init(void)
{
	return i2c_add_driver(&adv7533_driver);
}

static void __exit adv7533_exit(void)
{
	i2c_del_driver(&adv7533_driver);
}

module_param_string(panel, mdss_mdp_panel, MDSS_MAX_PANEL_LEN, 0);

module_init(adv7533_init);
module_exit(adv7533_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("adv7533 driver");
