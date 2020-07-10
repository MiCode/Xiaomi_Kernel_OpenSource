// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2018, 2020, The Linux Foundation. All rights reserved. */

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
#include <linux/mdss_io_util.h>

#define ADV7533_REG_CHIP_REVISION (0x00)
#define ADV7533_DSI_CEC_I2C_ADDR_REG (0xE1)
#define ADV7533_RESET_DELAY (10)

#define PINCTRL_STATE_ACTIVE    "pmx_adv7533_active"
#define PINCTRL_STATE_SUSPEND   "pmx_adv7533_suspend"

#define MDSS_MAX_PANEL_LEN      256
#define EDID_SEG_SIZE 0x100
/* size of audio and speaker info Block */
#define AUDIO_DATA_SIZE 32

/* 0x94 interrupts */
#define HPD_INT_ENABLE           BIT(7)
#define MONITOR_SENSE_INT_ENABLE BIT(6)
#define ACTIVE_VSYNC_EDGE        BIT(5)
#define AUDIO_FIFO_FULL          BIT(4)
#define EDID_READY_INT_ENABLE    BIT(2)
#define HDCP_AUTHENTICATED       BIT(1)
#define HDCP_RI_READY            BIT(0)

#define MAX_WAIT_TIME (100)
#define MAX_RW_TRIES (3)

/* 0x95 interrupts */
#define HDCP_ERROR               BIT(7)
#define HDCP_BKSV_FLAG           BIT(6)
#define CEC_TX_READY             BIT(5)
#define CEC_TX_ARB_LOST          BIT(4)
#define CEC_TX_RETRY_TIMEOUT     BIT(3)
#define CEC_TX_RX_BUF3_READY     BIT(2)
#define CEC_TX_RX_BUF2_READY     BIT(1)
#define CEC_TX_RX_BUF1_READY     BIT(0)

#define HPD_INTERRUPTS           (HPD_INT_ENABLE | \
					MONITOR_SENSE_INT_ENABLE)
#define EDID_INTERRUPTS          EDID_READY_INT_ENABLE
#define HDCP_INTERRUPTS1         HDCP_AUTHENTICATED
#define HDCP_INTERRUPTS2         (HDCP_BKSV_FLAG | \
					HDCP_ERROR)
#define CEC_INTERRUPTS           (CEC_TX_READY | \
					CEC_TX_ARB_LOST | \
					CEC_TX_RETRY_TIMEOUT | \
					CEC_TX_RX_BUF3_READY | \
					CEC_TX_RX_BUF2_READY | \
					CEC_TX_RX_BUF1_READY)

#define CFG_HPD_INTERRUPTS       BIT(0)
#define CFG_EDID_INTERRUPTS      BIT(1)
#define CFG_HDCP_INTERRUPTS      BIT(2)
#define CFG_CEC_INTERRUPTS       BIT(3)

#define MAX_OPERAND_SIZE	14
#define CEC_MSG_SIZE            (MAX_OPERAND_SIZE + 2)

enum adv7533_i2c_addr {
	I2C_ADDR_MAIN = 0x3D,
	I2C_ADDR_CEC_DSI = 0x3C,
};

enum adv7533_cec_buf {
	ADV7533_CEC_BUF1,
	ADV7533_CEC_BUF2,
	ADV7533_CEC_BUF3,
	ADV7533_CEC_BUF_MAX,
};

struct adv7533_reg_cfg {
	u8 i2c_addr;
	u8 reg;
	u8 val;
	int sleep_in_ms;
};

struct adv7533_cec_msg {
	u8 buf[CEC_MSG_SIZE];
	u8 timestamp;
	bool pending;
};

struct adv7533 {
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
	struct dss_module_power power_data;
	bool hdcp_enabled;
	bool cec_enabled;
	bool is_power_on;
	void *edid_data;
	u8 edid_buf[EDID_SEG_SIZE];
	u8 audio_spkr_data[AUDIO_DATA_SIZE];
	struct workqueue_struct *workq;
	struct delayed_work adv7533_intr_work_id;
	struct msm_dba_device_info dev_info;
	struct adv7533_cec_msg cec_msg[ADV7533_CEC_BUF_MAX];
	struct i2c_client *i2c_client;
	struct mutex ops_mutex;
};

static char mdss_mdp_panel[MDSS_MAX_PANEL_LEN];

static struct adv7533_reg_cfg adv7533_init_setup[] = {
	/* power down */
	{I2C_ADDR_MAIN, 0x41, 0x50, 5},
	/* HPD override */
	{I2C_ADDR_MAIN, 0xD6, 0x48, 5},
	/* color space */
	{I2C_ADDR_MAIN, 0x16, 0x20, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0x9A, 0xE0, 0},
	/* HDCP */
	{I2C_ADDR_MAIN, 0xBA, 0x70, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0xDE, 0x82, 0},
	/* V1P2 */
	{I2C_ADDR_MAIN, 0xE4, 0x40, 0},
	/* Fixed */
	{I2C_ADDR_MAIN, 0xE5, 0x80, 0},
	/* Fixed */
	{I2C_ADDR_CEC_DSI, 0x15, 0xD0, 0},
	/* Fixed */
	{I2C_ADDR_CEC_DSI, 0x17, 0xD0, 0},
	/* Fixed */
	{I2C_ADDR_CEC_DSI, 0x24, 0x20, 0},
	/* Fixed */
	{I2C_ADDR_CEC_DSI, 0x57, 0x11, 0},
};

static struct adv7533_reg_cfg adv7533_video_en[] = {
	 /* Timing Generator Enable */
	{I2C_ADDR_CEC_DSI, 0x27, 0xCB, 0},
	{I2C_ADDR_CEC_DSI, 0x27, 0x8B, 0},
	{I2C_ADDR_CEC_DSI, 0x27, 0xCB, 0},
	/* power up */
	{I2C_ADDR_MAIN, 0x41, 0x10, 0},
	/* hdmi enable */
	{I2C_ADDR_CEC_DSI, 0x03, 0x89, 0},
	/* color depth */
	{I2C_ADDR_MAIN, 0x4C, 0x04, 0},
	/* down dither */
	{I2C_ADDR_MAIN, 0x49, 0x02, 0},
	/* Audio and CEC clock gate */
	{I2C_ADDR_CEC_DSI, 0x05, 0xC8, 0},
	/* GC packet enable */
	{I2C_ADDR_MAIN, 0x40, 0x80, 0},
};

static struct adv7533_reg_cfg adv7533_cec_en[] = {
	/* Fixed, clock gate disable */
	{I2C_ADDR_CEC_DSI, 0x05, 0xC8, 0},
	/* read divider(7:2) from calc */
	{I2C_ADDR_CEC_DSI, 0xBE, 0x01, 0},
};

static struct adv7533_reg_cfg adv7533_cec_tg_init[] = {
	/* TG programming for 19.2MHz, divider 25 */
	{I2C_ADDR_CEC_DSI, 0xBE, 0x61, 0},
	{I2C_ADDR_CEC_DSI, 0xC1, 0x0D, 0},
	{I2C_ADDR_CEC_DSI, 0xC2, 0x80, 0},
	{I2C_ADDR_CEC_DSI, 0xC3, 0x0C, 0},
	{I2C_ADDR_CEC_DSI, 0xC4, 0x9A, 0},
	{I2C_ADDR_CEC_DSI, 0xC5, 0x0E, 0},
	{I2C_ADDR_CEC_DSI, 0xC6, 0x66, 0},
	{I2C_ADDR_CEC_DSI, 0xC7, 0x0B, 0},
	{I2C_ADDR_CEC_DSI, 0xC8, 0x1A, 0},
	{I2C_ADDR_CEC_DSI, 0xC9, 0x0A, 0},
	{I2C_ADDR_CEC_DSI, 0xCA, 0x33, 0},
	{I2C_ADDR_CEC_DSI, 0xCB, 0x0C, 0},
	{I2C_ADDR_CEC_DSI, 0xCC, 0x00, 0},
	{I2C_ADDR_CEC_DSI, 0xCD, 0x07, 0},
	{I2C_ADDR_CEC_DSI, 0xCE, 0x33, 0},
	{I2C_ADDR_CEC_DSI, 0xCF, 0x05, 0},
	{I2C_ADDR_CEC_DSI, 0xD0, 0xDA, 0},
	{I2C_ADDR_CEC_DSI, 0xD1, 0x08, 0},
	{I2C_ADDR_CEC_DSI, 0xD2, 0x8D, 0},
	{I2C_ADDR_CEC_DSI, 0xD3, 0x01, 0},
	{I2C_ADDR_CEC_DSI, 0xD4, 0xCD, 0},
	{I2C_ADDR_CEC_DSI, 0xD5, 0x04, 0},
	{I2C_ADDR_CEC_DSI, 0xD6, 0x80, 0},
	{I2C_ADDR_CEC_DSI, 0xD7, 0x05, 0},
	{I2C_ADDR_CEC_DSI, 0xD8, 0x66, 0},
	{I2C_ADDR_CEC_DSI, 0xD9, 0x03, 0},
	{I2C_ADDR_CEC_DSI, 0xDA, 0x26, 0},
	{I2C_ADDR_CEC_DSI, 0xDB, 0x0A, 0},
	{I2C_ADDR_CEC_DSI, 0xDC, 0xCD, 0},
	{I2C_ADDR_CEC_DSI, 0xDE, 0x00, 0},
	{I2C_ADDR_CEC_DSI, 0xDF, 0xC0, 0},
	{I2C_ADDR_CEC_DSI, 0xE1, 0x00, 0},
	{I2C_ADDR_CEC_DSI, 0xE2, 0xE6, 0},
	{I2C_ADDR_CEC_DSI, 0xE3, 0x02, 0},
	{I2C_ADDR_CEC_DSI, 0xE4, 0xB3, 0},
	{I2C_ADDR_CEC_DSI, 0xE5, 0x03, 0},
	{I2C_ADDR_CEC_DSI, 0xE6, 0x9A, 0},
};

static struct adv7533_reg_cfg adv7533_cec_power[] = {
	/* cec power up */
	{I2C_ADDR_MAIN, 0xE2, 0x00, 0},
	/* hpd override */
	{I2C_ADDR_MAIN, 0xD6, 0x48, 0},
	/* edid reread */
	{I2C_ADDR_MAIN, 0xC9, 0x13, 0},
	/* read all CEC Rx Buffers */
	{I2C_ADDR_CEC_DSI, 0xBA, 0x08, 0},
	/* logical address0 0x04 */
	{I2C_ADDR_CEC_DSI, 0xBC, 0x04, 0},
	/* select logical address0 */
	{I2C_ADDR_CEC_DSI, 0xBB, 0x10, 0},
};

static struct adv7533_reg_cfg I2S_cfg[] = {
	{I2C_ADDR_MAIN, 0x0D, 0x18, 0},	/* Bit width = 16Bits*/
	{I2C_ADDR_MAIN, 0x15, 0x20, 0},	/* Sampling Frequency = 48kHz*/
	{I2C_ADDR_MAIN, 0x02, 0x18, 0},	/* N value 6144 --> 0x1800*/
	{I2C_ADDR_MAIN, 0x14, 0x02, 0},	/* Word Length = 16Bits*/
	{I2C_ADDR_MAIN, 0x73, 0x01, 0},	/* Channel Count = 2 channels */
};

static int adv7533_write(struct adv7533 *pdata, u8 offset, u8 reg, u8 val)
{
	u8 addr = 0;
	int ret = 0;

	if (!pdata) {
		pr_debug("%s: Invalid argument\n", __func__);
		return -EINVAL;
	}

	if (offset == I2C_ADDR_MAIN)
		addr = pdata->main_i2c_addr;
	else if (offset == I2C_ADDR_CEC_DSI)
		addr = pdata->cec_dsi_i2c_addr;
	else
		addr = offset;

	ret = msm_dba_helper_i2c_write_byte(pdata->i2c_client, addr, reg, val);
	if (ret)
		pr_err_ratelimited("%s: wr err: addr 0x%x, reg 0x%x, val 0x%x\n",
				__func__, addr, reg, val);
	return ret;
}

static int adv7533_read(struct adv7533 *pdata, u8 offset,
		u8 reg, char *buf, u32 size)
{
	u8 addr = 0;
	int ret = 0;

	if (!pdata) {
		pr_debug("%s: Invalid argument\n", __func__);
		return -EINVAL;
	}

	if (offset == I2C_ADDR_MAIN)
		addr = pdata->main_i2c_addr;
	else if (offset == I2C_ADDR_CEC_DSI)
		addr = pdata->cec_dsi_i2c_addr;
	else
		addr = offset;

	ret = msm_dba_helper_i2c_read(pdata->i2c_client, addr, reg, buf, size);
	if (ret)
		pr_err_ratelimited("%s: read err: addr 0x%x, reg 0x%x, size 0x%x\n",
				__func__, addr, reg, size);
	return ret;
}

static int adv7533_dump_debug_info(struct msm_dba_device_info *dev, u32 flags)
{
	int rc = 0;
	u8 byte_val = 0;
	u16 addr = 0;
	struct adv7533 *pdata = NULL;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = container_of(dev, struct adv7533, dev_info);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	/* dump main addr*/
	pr_err("========Main I2C=0x%02x Start==========\n",
		pdata->main_i2c_addr);
	for (addr = 0; addr <= 0xFF; addr++) {
		rc = adv7533_read(pdata, I2C_ADDR_MAIN,
			(u8)addr, &byte_val, 1);
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
		rc = adv7533_read(pdata, I2C_ADDR_CEC_DSI,
			(u8)addr, &byte_val, 1);
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

static int adv7533_write_array(struct adv7533 *pdata,
	struct adv7533_reg_cfg *cfg, int size)
{
	int ret = 0;
	int i;

	size = size / sizeof(struct adv7533_reg_cfg);
	for (i = 0; i < size; i++) {
		switch (cfg[i].i2c_addr) {
		case I2C_ADDR_MAIN:
			ret = adv7533_write(pdata, I2C_ADDR_MAIN,
				cfg[i].reg, cfg[i].val);
			if (ret != 0)
				pr_err("%s: adv7533_write_byte returned %d\n",
					__func__, ret);
			break;
		case I2C_ADDR_CEC_DSI:
			ret = adv7533_write(pdata, I2C_ADDR_CEC_DSI,
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
			pr_err("%s: adv7533 reg writes failed.\n", __func__);
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

static int adv7533_read_device_rev(struct adv7533 *pdata)
{
	u8 rev = 0;
	int ret;

	ret = adv7533_read(pdata, I2C_ADDR_MAIN, ADV7533_REG_CHIP_REVISION,
							&rev, 1);

	return ret;
}

static int adv7533_program_i2c_addr(struct adv7533 *pdata)
{
	u8 i2c_8bits = pdata->cec_dsi_i2c_addr << 1;
	int ret = 0;

	if (pdata->cec_dsi_i2c_addr != I2C_ADDR_CEC_DSI) {
		ret = adv7533_write(pdata, I2C_ADDR_MAIN,
					ADV7533_DSI_CEC_I2C_ADDR_REG,
					i2c_8bits);

		if (ret)
			pr_err("%s: write err CEC_ADDR[0x%02x] main_addr=0x%02x\n",
				__func__, ADV7533_DSI_CEC_I2C_ADDR_REG,
				pdata->main_i2c_addr);
	}

	return ret;
}

static void adv7533_parse_vreg_dt(struct device *dev,
				struct dss_module_power *mp)
{
	int i, rc = 0;
	int dt_vreg_total = 0;
	struct device_node *of_node = NULL;
	u32 *val_array = NULL;

	of_node = dev->of_node;

	dt_vreg_total = of_property_count_strings(of_node, "qcom,supply-names");
	if (dt_vreg_total <= 0) {
		pr_warn("%s: vreg not found. rc=%d\n", __func__,
					dt_vreg_total);
		goto end;
	}
	mp->num_vreg = dt_vreg_total;
	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
			dt_vreg_total, GFP_KERNEL);
	if (!mp->vreg_config)
		goto end;

	val_array = devm_kzalloc(dev, sizeof(u32) * dt_vreg_total, GFP_KERNEL);
	if (!val_array)
		goto end;

	for (i = 0; i < dt_vreg_total; i++) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string_index(of_node,
				"qcom,supply-names", i, &st);
		if (rc) {
			pr_warn("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto end;
		}
		snprintf(mp->vreg_config[i].vreg_name, 32, "%s", st);

		/* vreg-min-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,min-voltage-level", val_array,
					dt_vreg_total);
		if (rc) {
			pr_warn("%s: error read min volt. rc=%d\n",
						__func__, rc);
			goto end;
		}
		mp->vreg_config[i].min_voltage = val_array[i];

		/* vreg-max-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
				"qcom,max-voltage-level", val_array,
						dt_vreg_total);
		if (rc) {
			pr_warn("%s: error read max volt. rc=%d\n",
					__func__, rc);
			goto end;
		}
		mp->vreg_config[i].max_voltage = val_array[i];

		/* vreg-op-mode */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
				"qcom,enable-load", val_array,
					dt_vreg_total);
		if (rc) {
			pr_warn("%s: error read enable load. rc=%d\n",
				__func__, rc);
			goto end;
		}
		mp->vreg_config[i].enable_load = val_array[i];

		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,disable-load", val_array,
			dt_vreg_total);
		if (rc) {
			pr_warn("%s: error read disable load. rc=%d\n",
				__func__, rc);
			goto end;
		}
		mp->vreg_config[i].disable_load = val_array[i];

		/* post-on-sleep */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
				"qcom,post-on-sleep", val_array,
						dt_vreg_total);
		if (rc)
			pr_warn("%s: error read post on sleep. rc=%d\n",
					__func__, rc);
		else
			mp->vreg_config[i].post_on_sleep = val_array[i];

		pr_debug("%s: %s min=%d, max=%d, enable=%d disable=%d post-on-sleep=%d\n",
			__func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].enable_load,
			mp->vreg_config[i].disable_load,
			mp->vreg_config[i].post_on_sleep);
	}

	return;

end:
	mp->num_vreg = 0;

}

static int adv7533_parse_dt(struct device *dev,
	struct adv7533 *pdata)
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

	adv7533_parse_vreg_dt(dev, &pdata->power_data);

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

static int adv7533_gpio_configure(struct adv7533 *pdata, bool on)
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

		if (gpio_is_valid(pdata->hpd_irq_gpio)) {
			ret = gpio_request(pdata->hpd_irq_gpio,
				"adv7533_hpd_irq_gpio");
			if (ret) {
				pr_err("unable to request gpio [%d]\n",
					pdata->hpd_irq_gpio);
				goto err_irq_gpio;
			}
			ret = gpio_direction_input(pdata->hpd_irq_gpio);
			if (ret) {
				pr_err("unable to set dir for gpio[%d]\n",
					pdata->hpd_irq_gpio);
				goto err_hpd_irq_gpio;
			}
		} else {
			pr_warn("hpd irq gpio not provided\n");
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

		return 0;
	}
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	if (gpio_is_valid(pdata->hpd_irq_gpio))
		gpio_free(pdata->hpd_irq_gpio);
	if (gpio_is_valid(pdata->switch_gpio))
		gpio_free(pdata->switch_gpio);

	return 0;
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

static void adv7533_notify_clients(struct msm_dba_device_info *dev,
		enum msm_dba_callback_event event)
{
	struct msm_dba_client_info *c;
	struct list_head *pos = NULL;

	if (!dev) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	list_for_each(pos, &dev->client_list) {
		c = list_entry(pos, struct msm_dba_client_info, list);

		pr_debug("%s: notifying event %d to client %s\n", __func__,
			event, c->client_name);

		if (c && c->cb)
			c->cb(c->cb_data, event);
	}
}

u32 adv7533_read_edid(struct adv7533 *pdata, u32 size, char *edid_buf)
{
	u32 ret = 0, read_size = size / 2;
	u8 edid_addr = 0;
	int ndx;

	if (!pdata || !edid_buf)
		return 0;

	pr_debug("%s: size %d\n", __func__, size);

	adv7533_read(pdata, I2C_ADDR_MAIN, 0x43, &edid_addr, 1);

	pr_debug("%s: edid address 0x%x\n", __func__, edid_addr);

	adv7533_read(pdata, edid_addr >> 1, 0x00, edid_buf, read_size);

	adv7533_read(pdata, edid_addr >> 1, read_size,
		edid_buf + read_size, read_size);

	for (ndx = 0; ndx < size; ndx += 4)
		pr_debug("%s: EDID[%02x-%02x] %02x %02x %02x %02x\n",
			__func__, ndx, ndx + 3,
			edid_buf[ndx + 0], edid_buf[ndx + 1],
			edid_buf[ndx + 2], edid_buf[ndx + 3]);

	return ret;
}

static int adv7533_cec_prepare_msg(struct adv7533 *pdata, u8 *msg, u32 size)
{
	int i;
	int op_sz;

	if (!pdata || !msg) {
		pr_err("%s: invalid input\n", __func__);
		goto end;
	}

	if (size <= 0 || size > CEC_MSG_SIZE) {
		pr_err("%s: ERROR: invalid msg size\n", __func__);
		goto end;
	}

	/* operand size = total size - header size - opcode size */
	op_sz = size - 2;

	/* write header */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x70, msg[0]);

	/* write opcode */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x71, msg[1]);

	/* write operands */
	for (i = 0; i < op_sz && i < MAX_OPERAND_SIZE; i++) {
		pr_debug("%s: writing operands\n", __func__);
		adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x72 + i, msg[i + 2]);
	}

	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x80, size);

end:
	return -EINVAL;
}

static int adv7533_rd_cec_msg(struct adv7533 *pdata, u8 *cec_buf, int msg_num)
{
	u8 reg = 0;

	if (!pdata || !cec_buf) {
		pr_err("%s: Invalid input\n", __func__);
		goto end;
	}

	if (msg_num == ADV7533_CEC_BUF1)
		reg = 0x85;
	else if (msg_num == ADV7533_CEC_BUF2)
		reg = 0x97;
	else if (msg_num == ADV7533_CEC_BUF3)
		reg = 0xA8;
	else
		pr_err("%s: Invalid msg_num %d\n", __func__, msg_num);

	if (!reg)
		goto end;

	adv7533_read(pdata, I2C_ADDR_CEC_DSI, reg, cec_buf, CEC_MSG_SIZE);
end:
	return -EINVAL;
}

static void adv7533_handle_hdcp_intr(struct adv7533 *pdata, u8 hdcp_status)
{
	u8 ddc_status = 0;

	if (!pdata) {
		pr_err("%s: Invalid input\n", __func__);
		goto end;
	}

	/* HDCP ready for read */
	if (hdcp_status & BIT(6))
		pr_debug("%s: BKSV FLAG\n", __func__);

	/* check for HDCP error */
	if (hdcp_status & BIT(7)) {
		pr_err("%s: HDCP ERROR\n", __func__);

		/* get error details */
		adv7533_read(pdata, I2C_ADDR_MAIN, 0xC8, &ddc_status, 1);

		switch (ddc_status & 0xF0 >> 4) {
		case 0:
			pr_debug("%s: DDC: NO ERROR\n", __func__);
			break;
		case 1:
			pr_err("%s: DDC: BAD RX BKSV\n", __func__);
			break;
		case 2:
			pr_err("%s: DDC: Ri MISMATCH\n", __func__);
			break;
		case 3:
			pr_err("%s: DDC: Pj MISMATCH\n", __func__);
			break;
		case 4:
			pr_err("%s: DDC: I2C ERROR\n", __func__);
			break;
		case 5:
			pr_err("%s: DDC: TIMED OUT DS DONE\n", __func__);
			break;
		case 6:
			pr_err("%s: DDC: MAX CAS EXC\n", __func__);
			break;
		default:
			pr_debug("%s: DDC: UNKNOWN ERROR\n", __func__);
		}
	}
end:
	return;
}

static void adv7533_handle_cec_intr(struct adv7533 *pdata, u8 cec_status)
{
	u8 cec_int_clear = 0x08;
	bool cec_rx_intr = false;
	u8 cec_rx_ready = 0;
	u8 cec_rx_timestamp = 0;

	if (!pdata) {
		pr_err("%s: Invalid input\n", __func__);
		goto end;
	}

	if (cec_status & 0x07) {
		cec_rx_intr = true;
		adv7533_read(pdata, I2C_ADDR_CEC_DSI, 0xBA, &cec_int_clear, 1);
	}

	if (cec_status & BIT(5))
		pr_debug("%s: CEC TX READY\n", __func__);

	if (cec_status & BIT(4))
		pr_debug("%s: CEC TX Arbitration lost\n", __func__);

	if (cec_status & BIT(3))
		pr_debug("%s: CEC TX retry timeout\n", __func__);

	if (!cec_rx_intr)
		return;


	adv7533_read(pdata, I2C_ADDR_CEC_DSI, 0xB9, &cec_rx_ready, 1);

	adv7533_read(pdata, I2C_ADDR_CEC_DSI, 0x96, &cec_rx_timestamp, 1);

	if (cec_rx_ready & BIT(0)) {
		pr_debug("%s: CEC Rx buffer 1 ready\n", __func__);
		adv7533_rd_cec_msg(pdata,
			pdata->cec_msg[ADV7533_CEC_BUF1].buf,
			ADV7533_CEC_BUF1);

		pdata->cec_msg[ADV7533_CEC_BUF1].pending = true;

		pdata->cec_msg[ADV7533_CEC_BUF1].timestamp =
			cec_rx_timestamp & (BIT(0) | BIT(1));

		adv7533_notify_clients(&pdata->dev_info,
			MSM_DBA_CB_CEC_READ_PENDING);
	}

	if (cec_rx_ready & BIT(1)) {
		pr_debug("%s: CEC Rx buffer 2 ready\n", __func__);
		adv7533_rd_cec_msg(pdata,
			pdata->cec_msg[ADV7533_CEC_BUF2].buf,
			ADV7533_CEC_BUF2);

		pdata->cec_msg[ADV7533_CEC_BUF2].pending = true;

		pdata->cec_msg[ADV7533_CEC_BUF2].timestamp =
			cec_rx_timestamp & (BIT(2) | BIT(3));

		adv7533_notify_clients(&pdata->dev_info,
			MSM_DBA_CB_CEC_READ_PENDING);
	}

	if (cec_rx_ready & BIT(2)) {
		pr_debug("%s: CEC Rx buffer 3 ready\n", __func__);
		adv7533_rd_cec_msg(pdata,
			pdata->cec_msg[ADV7533_CEC_BUF3].buf,
			ADV7533_CEC_BUF3);

		pdata->cec_msg[ADV7533_CEC_BUF3].pending = true;

		pdata->cec_msg[ADV7533_CEC_BUF3].timestamp =
			cec_rx_timestamp & (BIT(4) | BIT(5));

		adv7533_notify_clients(&pdata->dev_info,
			MSM_DBA_CB_CEC_READ_PENDING);
	}

	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0xBA,
		cec_int_clear | (cec_status & 0x07));

	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0xBA, cec_int_clear & ~0x07);

end:
	return;
}

static int adv7533_edid_read_init(struct adv7533 *pdata)
{
	if (!pdata) {
		pr_err("%s: invalid pdata\n", __func__);
		goto end;
	}

	/* initiate edid read in adv7533 */
	adv7533_write(pdata, I2C_ADDR_MAIN, 0x41, 0x10);
	adv7533_write(pdata, I2C_ADDR_MAIN, 0xC9, 0x13);

end:
	return -EINVAL;
}

static void *adv7533_handle_hpd_intr(struct adv7533 *pdata)
{
	int ret = 0;
	u8 hpd_state;
	u8 connected = 0, disconnected = 0;

	if (!pdata) {
		pr_err("%s: invalid pdata\n", __func__);
		goto end;
	}

	adv7533_read(pdata, I2C_ADDR_MAIN, 0x42, &hpd_state, 1);

	connected = (hpd_state & BIT(5)) && (hpd_state & BIT(6));
	disconnected = !(hpd_state & (BIT(5) | BIT(6)));

	if (connected) {
		pr_debug("%s: Rx CONNECTED\n", __func__);
	} else if (disconnected) {
		pr_debug("%s: Rx DISCONNECTED\n", __func__);

		adv7533_notify_clients(&pdata->dev_info,
			MSM_DBA_CB_HPD_DISCONNECT);
	} else {
		pr_debug("%s: HPD Intermediate state\n", __func__);
	}

	ret = connected ? 1 : 0;
end:
	return ERR_PTR(ret);
}

static int adv7533_enable_interrupts(struct adv7533 *pdata, int interrupts)
{
	u8 reg_val, init_reg_val;

	if (!pdata) {
		pr_err("%s: invalid input\n", __func__);
		goto end;
	}

	adv7533_read(pdata, I2C_ADDR_MAIN, 0x94, &reg_val, 1);

	init_reg_val = reg_val;

	if (interrupts & CFG_HPD_INTERRUPTS)
		reg_val |= HPD_INTERRUPTS;

	if (interrupts & CFG_EDID_INTERRUPTS)
		reg_val |= EDID_INTERRUPTS;

	if (interrupts & CFG_HDCP_INTERRUPTS)
		reg_val |= HDCP_INTERRUPTS1;

	if (reg_val != init_reg_val) {
		pr_debug("%s: enabling 0x94 interrupts\n", __func__);
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x94, reg_val);
	}

	adv7533_read(pdata, I2C_ADDR_MAIN, 0x95, &reg_val, 1);

	init_reg_val = reg_val;

	if (interrupts & CFG_HDCP_INTERRUPTS)
		reg_val |= HDCP_INTERRUPTS2;

	if (interrupts & CFG_CEC_INTERRUPTS)
		reg_val |= CEC_INTERRUPTS;

	if (reg_val != init_reg_val) {
		pr_debug("%s: enabling 0x95 interrupts\n", __func__);
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x95, reg_val);
	}
end:
	return 0;
}

static int adv7533_disable_interrupts(struct adv7533 *pdata, int interrupts)
{
	u8 reg_val, init_reg_val;

	if (!pdata) {
		pr_err("%s: invalid input\n", __func__);
		goto end;
	}

	adv7533_read(pdata, I2C_ADDR_MAIN, 0x94, &reg_val, 1);

	init_reg_val = reg_val;

	if (interrupts & CFG_HPD_INTERRUPTS)
		reg_val &= ~HPD_INTERRUPTS;

	if (interrupts & CFG_EDID_INTERRUPTS)
		reg_val &= ~EDID_INTERRUPTS;

	if (interrupts & CFG_HDCP_INTERRUPTS)
		reg_val &= ~HDCP_INTERRUPTS1;

	if (reg_val != init_reg_val) {
		pr_debug("%s: disabling 0x94 interrupts\n", __func__);
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x94, reg_val);
	}

	adv7533_read(pdata, I2C_ADDR_MAIN, 0x95, &reg_val, 1);

	init_reg_val = reg_val;

	if (interrupts & CFG_HDCP_INTERRUPTS)
		reg_val &= ~HDCP_INTERRUPTS2;

	if (interrupts & CFG_CEC_INTERRUPTS)
		reg_val &= ~CEC_INTERRUPTS;

	if (reg_val != init_reg_val) {
		pr_debug("%s: disabling 0x95 interrupts\n", __func__);
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x95, reg_val);
	}
end:
	return 0;
}

static void adv7533_intr_work(struct work_struct *work)
{
	int ret;
	u8 int_status  = 0xFF;
	u8 hdcp_cec_status = 0xFF;
	u32 interrupts = 0;
	int connected = false;
	struct adv7533 *pdata;
	struct delayed_work *dw = to_delayed_work(work);

	pdata = container_of(dw, struct adv7533,
			adv7533_intr_work_id);
	if (!pdata) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	/* READ Interrupt registers */
	adv7533_read(pdata, I2C_ADDR_MAIN, 0x96, &int_status, 1);
	adv7533_read(pdata, I2C_ADDR_MAIN, 0x97, &hdcp_cec_status, 1);

	if (int_status & (BIT(6) | BIT(7))) {
		void *ptr_val = adv7533_handle_hpd_intr(pdata);

		ret = PTR_ERR(ptr_val);
		if (IS_ERR(ptr_val)) {
			pr_err("%s: error in hpd handing: %d\n",
				__func__, ret);
			goto reset;
		}
		connected = ret;
	}

	/* EDID ready for read */
	if ((int_status & BIT(2)) && pdata->is_power_on) {
		pr_debug("%s: EDID READY\n", __func__);

		ret = adv7533_read_edid(pdata, sizeof(pdata->edid_buf),
			pdata->edid_buf);
		if (ret)
			pr_err("%s: edid read failed\n", __func__);

		adv7533_notify_clients(&pdata->dev_info,
			MSM_DBA_CB_HPD_CONNECT);
	}

	if (pdata->hdcp_enabled)
		adv7533_handle_hdcp_intr(pdata, hdcp_cec_status);

	if (pdata->cec_enabled)
		adv7533_handle_cec_intr(pdata, hdcp_cec_status);
reset:
	/* Clear HPD/EDID interrupts */
	adv7533_write(pdata, I2C_ADDR_MAIN, 0x96, int_status);

	/* Clear HDCP/CEC interrupts */
	adv7533_write(pdata, I2C_ADDR_MAIN, 0x97, hdcp_cec_status);

	/* Re-enable HPD interrupts */
	interrupts |= CFG_HPD_INTERRUPTS;

	/* Re-enable EDID interrupts */
	interrupts |= CFG_EDID_INTERRUPTS;

	/* Re-enable HDCP interrupts */
	if (pdata->hdcp_enabled)
		interrupts |= CFG_HDCP_INTERRUPTS;

	/* Re-enable CEC interrupts */
	if (pdata->cec_enabled)
		interrupts |= CFG_CEC_INTERRUPTS;

	if (adv7533_enable_interrupts(pdata, interrupts))
		pr_err("%s: err enabling interrupts\n", __func__);

	/* initialize EDID read after cable connected */
	if (connected)
		adv7533_edid_read_init(pdata);
}

static irqreturn_t adv7533_irq(int irq, void *data)
{
	struct adv7533 *pdata = data;
	u32 interrupts = 0;

	if (!pdata) {
		pr_err("%s: invalid input\n", __func__);
		return IRQ_HANDLED;
	}

	/* disable HPD interrupts */
	interrupts |= CFG_HPD_INTERRUPTS;

	/* disable EDID interrupts */
	interrupts |= CFG_EDID_INTERRUPTS;

	/* disable HDCP interrupts */
	if (pdata->hdcp_enabled)
		interrupts |= CFG_HDCP_INTERRUPTS;

	/* disable CEC interrupts */
	if (pdata->cec_enabled)
		interrupts |= CFG_CEC_INTERRUPTS;

	if (adv7533_disable_interrupts(pdata, interrupts))
		pr_err("%s: err disabling interrupts\n", __func__);

	queue_delayed_work(pdata->workq, &pdata->adv7533_intr_work_id, 0);

	return IRQ_HANDLED;
}

static struct i2c_device_id adv7533_id[] = {
	{ "adv7533", 0},
	{}
};

static struct adv7533 *adv7533_get_platform_data(void *client)
{
	struct adv7533 *pdata = NULL;
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

	pdata = container_of(dev, struct adv7533, dev_info);
	if (!pdata)
		pr_err("%s: invalid platform data\n", __func__);

end:
	return pdata;
}

static int adv7533_cec_enable(void *client, bool cec_on, u32 flags)
{
	int ret = -EINVAL;
	struct adv7533 *pdata = adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	if (cec_on) {
		adv7533_write_array(pdata, adv7533_cec_en,
					sizeof(adv7533_cec_en));
		adv7533_write_array(pdata, adv7533_cec_tg_init,
					sizeof(adv7533_cec_tg_init));
		adv7533_write_array(pdata, adv7533_cec_power,
					sizeof(adv7533_cec_power));

		pdata->cec_enabled = true;

		ret = adv7533_enable_interrupts(pdata, CFG_CEC_INTERRUPTS);

	} else {
		pdata->cec_enabled = false;
		ret = adv7533_disable_interrupts(pdata, CFG_CEC_INTERRUPTS);
	}
end:
	return ret;
}
static void adv7533_set_audio_block(void *client, u32 size, void *buf)
{
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		return;
	}

	mutex_lock(&pdata->ops_mutex);

	size = min_t(u32, size, AUDIO_DATA_SIZE);

	memset(pdata->audio_spkr_data, 0, AUDIO_DATA_SIZE);
	memcpy(pdata->audio_spkr_data, buf, size);

	mutex_unlock(&pdata->ops_mutex);
}

static void adv7533_get_audio_block(void *client, u32 size, void *buf)
{
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		return;
	}

	mutex_lock(&pdata->ops_mutex);

	size = min_t(u32, size, AUDIO_DATA_SIZE);

	memcpy(buf, pdata->audio_spkr_data, size);

	mutex_unlock(&pdata->ops_mutex);
}

static int adv7533_check_hpd(void *client, u32 flags)
{
	struct adv7533 *pdata = adv7533_get_platform_data(client);
	u8 reg_val = 0;
	u8 intr_status;
	int connected = 0;

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		return -EINVAL;
	}

	/* Check if cable is already connected.
	 * Since adv7533_irq line is edge triggered,
	 * if cable is already connected by this time
	 * it won't trigger HPD interrupt.
	 */
	mutex_lock(&pdata->ops_mutex);
	adv7533_read(pdata, I2C_ADDR_MAIN, 0x42, &reg_val, 1);

	connected  = (reg_val & BIT(6));
	if (connected) {
		pr_debug("%s: cable is connected\n", __func__);
		/* Clear the interrupts before initiating EDID read */
		adv7533_read(pdata, I2C_ADDR_MAIN, 0x96, &intr_status, 1);
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x96, intr_status);
		adv7533_enable_interrupts(pdata, (CFG_EDID_INTERRUPTS |
				CFG_HPD_INTERRUPTS));

		adv7533_edid_read_init(pdata);
	}
	mutex_unlock(&pdata->ops_mutex);

	return connected;
}

/* Device Operations */
static int adv7533_power_on(void *client, bool on, u32 flags)
{
	int ret = -EINVAL;
	struct adv7533 *pdata = adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	pr_debug("%s: %d\n", __func__, on);
	mutex_lock(&pdata->ops_mutex);

	if (on && !pdata->is_power_on) {
		adv7533_write_array(pdata, adv7533_init_setup,
					sizeof(adv7533_init_setup));

		ret = adv7533_enable_interrupts(pdata, CFG_HPD_INTERRUPTS);
		if (ret) {
			pr_err("%s: Failed: enable HPD intr %d\n",
				__func__, ret);
			goto end;
		}
		pdata->is_power_on = true;
	} else if (!on) {
		/* power down hdmi */
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x41, 0x50);
		pdata->is_power_on = false;

		adv7533_notify_clients(&pdata->dev_info,
			MSM_DBA_CB_HPD_DISCONNECT);
	}
end:
	mutex_unlock(&pdata->ops_mutex);
	return ret;
}

static void adv7533_video_setup(struct adv7533 *pdata,
	struct msm_dba_video_cfg *cfg)
{
	u32 h_total, hpw, hfp, hbp;
	u32 v_total, vpw, vfp, vbp;

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

	pr_debug("h_total 0x%x, h_active 0x%x, hfp 0x%x, hpw 0x%x, hbp 0x%x\n",
		h_total, cfg->h_active, cfg->h_front_porch,
		cfg->h_pulse_width, cfg->h_back_porch);

	pr_debug("v_total 0x%x, v_active 0x%x, vfp 0x%x, vpw 0x%x, vbp 0x%x\n",
		v_total, cfg->v_active, cfg->v_front_porch,
		cfg->v_pulse_width, cfg->v_back_porch);


	/* h_width */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x28, ((h_total & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x29, ((h_total & 0xF) << 4));

	/* hsync_width */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x2A, ((hpw & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x2B, ((hpw & 0xF) << 4));

	/* hfp */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x2C, ((hfp & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x2D, ((hfp & 0xF) << 4));

	/* hbp */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x2E, ((hbp & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x2F, ((hbp & 0xF) << 4));

	/* v_total */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x30, ((v_total & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x31, ((v_total & 0xF) << 4));

	/* vsync_width */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x32, ((vpw & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x33, ((vpw & 0xF) << 4));

	/* vfp */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x34, ((vfp & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x35, ((vfp & 0xF) << 4));

	/* vbp */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x36, ((vbp & 0xFF0) >> 4));
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x37, ((vbp & 0xF) << 4));
}

static int adv7533_config_vreg(struct adv7533 *pdata, int enable)
{
	int rc = 0;
	struct dss_module_power *power_data = NULL;

	if (!pdata) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	power_data = &pdata->power_data;
	if (!power_data || !power_data->num_vreg) {
		pr_warn("%s: Error: invalid power data\n", __func__);
		return 0;
	}

	if (enable) {
		rc = msm_dss_config_vreg(&pdata->i2c_client->dev,
					power_data->vreg_config,
					power_data->num_vreg, 1);
		if (rc) {
			pr_err("%s: Failed to config vreg. Err=%d\n",
				__func__, rc);
			goto exit;
		}
	} else {
		rc = msm_dss_config_vreg(&pdata->i2c_client->dev,
					power_data->vreg_config,
					power_data->num_vreg, 0);
		if (rc) {
			pr_err("%s: Failed to deconfig vreg. Err=%d\n",
				__func__, rc);
			goto exit;
		}
	}
exit:
	return rc;

}

static int adv7533_enable_vreg(struct adv7533 *pdata, int enable)
{
	int rc = 0;
	struct dss_module_power *power_data = NULL;

	if (!pdata) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	power_data = &pdata->power_data;
	if (!power_data || !power_data->num_vreg) {
		pr_warn("%s: Error: invalid power data\n", __func__);
		return 0;
	}

	if (enable) {
		rc = msm_dss_enable_vreg(power_data->vreg_config,
					power_data->num_vreg, 1);
		if (rc) {
			pr_err("%s: Failed to enable vreg. Err=%d\n",
				__func__, rc);
			goto exit;
		}
	} else {
		rc = msm_dss_enable_vreg(power_data->vreg_config,
					power_data->num_vreg, 0);
		if (rc) {
			pr_err("%s: Failed to disable vreg. Err=%d\n",
				__func__, rc);
			goto exit;
		}
	}
exit:
	return rc;

}

static int adv7533_video_on(void *client, bool on,
	struct msm_dba_video_cfg *cfg, u32 flags)
{
	int ret = 0;
	u8 lanes;
	u8 reg_val = 0;
	struct adv7533 *pdata = adv7533_get_platform_data(client);

	if (!pdata || !cfg) {
		pr_err("%s: invalid platform data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pdata->ops_mutex);

	/* DSI lane configuration */
	lanes = (cfg->num_of_input_lanes << 4);
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x1C, lanes);

	adv7533_video_setup(pdata, cfg);

	/* hdmi/dvi mode */
	if (cfg->hdmi_mode)
		adv7533_write(pdata, I2C_ADDR_MAIN, 0xAF, 0x06);
	else
		adv7533_write(pdata, I2C_ADDR_MAIN, 0xAF, 0x04);

	/* set scan info for AVI Infoframe*/
	if (cfg->scaninfo) {
		adv7533_read(pdata, I2C_ADDR_MAIN, 0x55, &reg_val, 1);
		reg_val |= cfg->scaninfo & (BIT(1) | BIT(0));
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x55, reg_val);
	}

	/*
	 * aspect ratio and sync polarity set up.
	 * Currently adv only supports 16:9 or 4:3 aspect ratio
	 * configuration.
	 */
	if (cfg->h_active * 3 - cfg->v_active * 4) {
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x17, 0x02);
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x56, 0x28);
	} else {
		/* 4:3 aspect ratio */
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x17, 0x00);
		adv7533_write(pdata, I2C_ADDR_MAIN, 0x56, 0x18);
	}

	adv7533_write_array(pdata, adv7533_video_en,
				sizeof(adv7533_video_en));

	mutex_unlock(&pdata->ops_mutex);
	return ret;
}

static int adv7533_hdcp_enable(void *client, bool hdcp_on,
	bool enc_on, u32 flags)
{
	u8 reg_val;
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	mutex_lock(&pdata->ops_mutex);

	adv7533_read(pdata, I2C_ADDR_MAIN, 0xAF, &reg_val, 1);

	if (hdcp_on)
		reg_val |= BIT(7);
	else
		reg_val &= ~BIT(7);

	if (enc_on)
		reg_val |= BIT(4);
	else
		reg_val &= ~BIT(4);

	adv7533_write(pdata, I2C_ADDR_MAIN, 0xAF, reg_val);

	pdata->hdcp_enabled = hdcp_on;

	if (pdata->hdcp_enabled)
		adv7533_enable_interrupts(pdata, CFG_HDCP_INTERRUPTS);
	else
		adv7533_disable_interrupts(pdata, CFG_HDCP_INTERRUPTS);

	mutex_unlock(&pdata->ops_mutex);
	return -EINVAL;
}

static int adv7533_configure_audio(void *client,
	struct msm_dba_audio_cfg *cfg, u32 flags)
{
	int sampling_rate = 0;
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);
	struct adv7533_reg_cfg reg_cfg[] = {
		{I2C_ADDR_MAIN, 0x12, 0x00, 0},
		{I2C_ADDR_MAIN, 0x13, 0x00, 0},
		{I2C_ADDR_MAIN, 0x14, 0x00, 0},
		{I2C_ADDR_MAIN, 0x15, 0x00, 0},
		{I2C_ADDR_MAIN, 0x0A, 0x00, 0},
		{I2C_ADDR_MAIN, 0x0C, 0x00, 0},
		{I2C_ADDR_MAIN, 0x0D, 0x00, 0},
		{I2C_ADDR_MAIN, 0x03, 0x00, 0},
		{I2C_ADDR_MAIN, 0x02, 0x00, 0},
		{I2C_ADDR_MAIN, 0x01, 0x00, 0},
		{I2C_ADDR_MAIN, 0x09, 0x00, 0},
		{I2C_ADDR_MAIN, 0x08, 0x00, 0},
		{I2C_ADDR_MAIN, 0x07, 0x00, 0},
		{I2C_ADDR_MAIN, 0x73, 0x00, 0},
		{I2C_ADDR_MAIN, 0x76, 0x00, 0},
	};

	if (!pdata || !cfg) {
		pr_err("%s: invalid data\n", __func__);
		return ret;
	}

	mutex_lock(&pdata->ops_mutex);

	if (cfg->copyright == MSM_DBA_AUDIO_COPYRIGHT_NOT_PROTECTED)
		reg_cfg[0].val |= BIT(5);

	if (cfg->pre_emphasis == MSM_DBA_AUDIO_PRE_EMPHASIS_50_15us)
		reg_cfg[0].val |= BIT(2);

	if (cfg->clock_accuracy == MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL1)
		reg_cfg[0].val |= BIT(0);
	else if (cfg->clock_accuracy == MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL3)
		reg_cfg[0].val |= BIT(1);

	reg_cfg[1].val = cfg->channel_status_category_code;

	reg_cfg[2].val = (cfg->channel_status_word_length & 0xF) << 0 |
		(cfg->channel_status_source_number & 0xF) << 4;

	if (cfg->sampling_rate == MSM_DBA_AUDIO_32KHZ)
		sampling_rate = 0x3;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_44P1KHZ)
		sampling_rate = 0x0;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_48KHZ)
		sampling_rate = 0x2;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_88P2KHZ)
		sampling_rate = 0x8;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_96KHZ)
		sampling_rate = 0xA;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_176P4KHZ)
		sampling_rate = 0xC;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_192KHZ)
		sampling_rate = 0xE;

	reg_cfg[3].val = (sampling_rate & 0xF) << 4;

	if (cfg->mode == MSM_DBA_AUDIO_MODE_MANUAL)
		reg_cfg[4].val |= BIT(7);

	if (cfg->interface == MSM_DBA_AUDIO_SPDIF_INTERFACE)
		reg_cfg[4].val |= BIT(4);

	if (cfg->interface == MSM_DBA_AUDIO_I2S_INTERFACE) {
		/* i2s enable */
		reg_cfg[5].val |= BIT(2);

		/* audio samp freq select */
		reg_cfg[5].val |= BIT(7);
	}

	/* format */
	reg_cfg[5].val |= cfg->i2s_fmt & 0x3;

	/* channel status override */
	reg_cfg[5].val |= (cfg->channel_status_source & 0x1) << 6;

	/* sample word lengths, default 24 */
	reg_cfg[6].val |= 0x18;

	/* endian order of incoming I2S data */
	if (cfg->word_endianness == MSM_DBA_AUDIO_WORD_LITTLE_ENDIAN)
		reg_cfg[6].val |= 0x1 << 7;

	/* compressed audio v - bit */
	reg_cfg[6].val |= (cfg->channel_status_v_bit & 0x1) << 5;

	/* ACR - N */
	reg_cfg[7].val |= (cfg->n & 0x000FF) >> 0;
	reg_cfg[8].val |= (cfg->n & 0x0FF00) >> 8;
	reg_cfg[9].val |= (cfg->n & 0xF0000) >> 16;

	/* ACR - CTS */
	reg_cfg[10].val |= (cfg->cts & 0x000FF) >> 0;
	reg_cfg[11].val |= (cfg->cts & 0x0FF00) >> 8;
	reg_cfg[12].val |= (cfg->cts & 0xF0000) >> 16;

	/* channel count */
	reg_cfg[13].val |= (cfg->channels & 0x3);

	/* CA */
	reg_cfg[14].val = cfg->channel_allocation;

	adv7533_write_array(pdata, reg_cfg, sizeof(reg_cfg));

	mutex_unlock(&pdata->ops_mutex);
	return -EINVAL;
}

static int adv7533_hdmi_cec_write(void *client, u32 size,
	char *buf, u32 flags)
{
	int ret = -EINVAL;
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	mutex_lock(&pdata->ops_mutex);

	ret = adv7533_cec_prepare_msg(pdata, buf, size);
	if (ret)
		goto end;

	/* Enable CEC msg tx with NACK 3 retries */
	adv7533_write(pdata, I2C_ADDR_CEC_DSI, 0x81, 0x07);
end:
	mutex_unlock(&pdata->ops_mutex);
	return ret;
}

static int adv7533_hdmi_cec_read(void *client, u32 *size, char *buf, u32 flags)
{
	int ret = -EINVAL;
	int i;
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	mutex_lock(&pdata->ops_mutex);

	for (i = 0; i < ADV7533_CEC_BUF_MAX; i++) {
		struct adv7533_cec_msg *msg = &pdata->cec_msg[i];

		if (msg->pending && msg->timestamp) {
			memcpy(buf, msg->buf, CEC_MSG_SIZE);
			msg->pending = false;
			break;
		}
	}

	if (i < ADV7533_CEC_BUF_MAX) {
		*size = CEC_MSG_SIZE;
		ret = 0;
	} else {
		pr_err("%s: no pending cec msg\n", __func__);
		*size = 0;
	}

	mutex_unlock(&pdata->ops_mutex);
	return ret;
}

static int adv7533_get_edid_size(void *client, u32 *size, u32 flags)
{
	int ret = 0;
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	mutex_lock(&pdata->ops_mutex);

	if (!size) {
		ret = -EINVAL;
		goto end;
	}

	*size = EDID_SEG_SIZE;
end:
	mutex_unlock(&pdata->ops_mutex);
	return ret;
}

static int adv7533_get_raw_edid(void *client,
	u32 size, char *buf, u32 flags)
{
	struct adv7533 *pdata =
		adv7533_get_platform_data(client);

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		goto end;
	}

	mutex_lock(&pdata->ops_mutex);

	size = min_t(u32, size, sizeof(pdata->edid_buf));

	memcpy(buf, pdata->edid_buf, size);
end:
	mutex_unlock(&pdata->ops_mutex);
	return 0;
}

static int adv7533_write_reg(struct msm_dba_device_info *dev,
		u32 reg, u32 val)
{
	struct adv7533 *pdata;
	u8 i2ca = 0;

	if (!dev)
		goto end;

	pdata = container_of(dev, struct adv7533, dev_info);
	if (!pdata)
		goto end;

	i2ca = ((reg & 0x100) ? pdata->cec_dsi_i2c_addr : pdata->main_i2c_addr);

	adv7533_write(pdata, i2ca, (u8)(reg & 0xFF), (u8)(val & 0xFF));
end:
	return -EINVAL;
}

static int adv7533_read_reg(struct msm_dba_device_info *dev,
		u32 reg, u32 *val)
{
	u8 byte_val = 0;
	u8 i2ca = 0;
	struct adv7533 *pdata;

	if (!dev)
		goto end;

	pdata = container_of(dev, struct adv7533, dev_info);
	if (!pdata)
		goto end;

	i2ca = ((reg & 0x100) ? pdata->cec_dsi_i2c_addr : pdata->main_i2c_addr);

	adv7533_read(pdata, i2ca, (u8)(reg & 0xFF), &byte_val, 1);

	*val = (u32)byte_val;

end:
	return 0;
}

static int adv7533_register_dba(struct adv7533 *pdata)
{
	struct msm_dba_ops *client_ops;
	struct msm_dba_device_ops *dev_ops;

	if (!pdata)
		return -EINVAL;

	client_ops = &pdata->dev_info.client_ops;
	dev_ops = &pdata->dev_info.dev_ops;

	client_ops->power_on        = adv7533_power_on;
	client_ops->video_on        = adv7533_video_on;
	client_ops->configure_audio = adv7533_configure_audio;
	client_ops->hdcp_enable     = adv7533_hdcp_enable;
	client_ops->hdmi_cec_on     = adv7533_cec_enable;
	client_ops->hdmi_cec_write  = adv7533_hdmi_cec_write;
	client_ops->hdmi_cec_read   = adv7533_hdmi_cec_read;
	client_ops->get_edid_size   = adv7533_get_edid_size;
	client_ops->get_raw_edid    = adv7533_get_raw_edid;
	client_ops->check_hpd	    = adv7533_check_hpd;
	client_ops->get_audio_block = adv7533_get_audio_block;
	client_ops->set_audio_block = adv7533_set_audio_block;

	dev_ops->write_reg = adv7533_write_reg;
	dev_ops->read_reg = adv7533_read_reg;
	dev_ops->dump_debug_info = adv7533_dump_debug_info;

	strlcpy(pdata->dev_info.chip_name, "adv7533",
		sizeof(pdata->dev_info.chip_name));

	mutex_init(&pdata->dev_info.dev_mutex);

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	return msm_dba_add_probed_device(&pdata->dev_info);
}

static void adv7533_unregister_dba(struct adv7533 *pdata)
{
	if (!pdata)
		return;

	msm_dba_remove_probed_device(&pdata->dev_info);
}


static int adv7533_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	static struct adv7533 *pdata;
	int ret = 0;

	if (!client || !client->dev.of_node) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pdata = devm_kzalloc(&client->dev,
		sizeof(struct adv7533), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = adv7533_parse_dt(&client->dev, pdata);
	if (ret) {
		pr_err("%s: Failed to parse DT\n", __func__);
		goto err_dt_parse;
	}
	pdata->i2c_client = client;

	ret = adv7533_config_vreg(pdata, 1);
	if (ret) {
		pr_err("%s: Failed to config vreg\n", __func__);
		return -EPROBE_DEFER;
	}
	adv7533_enable_vreg(pdata, 1);

	mutex_init(&pdata->ops_mutex);

	ret = adv7533_read_device_rev(pdata);
	if (ret) {
		pr_err("%s: Failed to read chip rev\n", __func__);
		goto err_i2c_prog;
	}

	ret = adv7533_program_i2c_addr(pdata);
	if (ret != 0) {
		pr_err("%s: Failed to program i2c addr\n", __func__);
		goto err_i2c_prog;
	}

	ret = adv7533_register_dba(pdata);
	if (ret) {
		pr_err("%s: Error registering with DBA %d\n",
			__func__, ret);
		goto err_dba_reg;
	}

	ret = pinctrl_select_state(pdata->ts_pinctrl,
		pdata->pinctrl_state_active);
	if (ret < 0)
		pr_err("%s: Failed to select %s pinstate %d\n",
			__func__, PINCTRL_STATE_ACTIVE, ret);

	ret = adv7533_gpio_configure(pdata, true);
	if (ret) {
		pr_err("%s: Failed to configure GPIOs\n", __func__);
		goto err_gpio_cfg;
	}

	if (gpio_is_valid(pdata->switch_gpio))
		gpio_set_value(pdata->switch_gpio, pdata->switch_flags);

	pdata->irq = gpio_to_irq(pdata->irq_gpio);

	ret = request_threaded_irq(pdata->irq, NULL, adv7533_irq,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "adv7533", pdata);
	if (ret) {
		pr_err("%s: Failed to enable ADV7533 interrupt\n",
			__func__);
		goto err_irq;
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

	if (pdata->audio) {
		pr_debug("%s: enabling default audio configs\n", __func__);
		if (adv7533_write_array(pdata, I2S_cfg, sizeof(I2S_cfg)))
			goto end;
	}

	INIT_DELAYED_WORK(&pdata->adv7533_intr_work_id, adv7533_intr_work);

	pm_runtime_enable(&client->dev);
	pm_runtime_set_active(&client->dev);

	return 0;
end:
	if (pdata->workq)
		destroy_workqueue(pdata->workq);
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
err_i2c_prog:
	adv7533_enable_vreg(pdata, 0);
	adv7533_config_vreg(pdata, 0);
err_dt_parse:
	return ret;
}

static int adv7533_remove(struct i2c_client *client)
{
	int ret = -EINVAL;
	struct msm_dba_device_info *dev;
	struct adv7533 *pdata;

	if (!client)
		goto end;

	dev = dev_get_drvdata(&client->dev);
	if (!dev)
		goto end;

	pdata = container_of(dev, struct adv7533, dev_info);
	if (!pdata)
		goto end;

	pm_runtime_disable(&client->dev);
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);

	adv7533_config_vreg(pdata, 0);
	ret = adv7533_gpio_configure(pdata, false);

	mutex_destroy(&pdata->ops_mutex);

end:
	return ret;
}

static struct i2c_driver adv7533_driver = {
	.driver = {
		.name = "adv7533",
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

module_param_string(panel, mdss_mdp_panel, MDSS_MAX_PANEL_LEN, 0000);

module_init(adv7533_init);
module_exit(adv7533_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("adv7533 driver");
