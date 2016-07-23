/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-irq.h>
#include <linux/mfd/msm-cdc-supply.h>
#include <linux/mfd/msm-cdc-pinctrl.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-utils.h>

#define REG_BYTES 2
#define VAL_BYTES 1
/*
 * Page Register Address that APP Proc uses to
 * access WCD9335 Codec registers is identified
 * as 0x00
 */
#define PAGE_REG_ADDR 0x00

static enum wcd9xxx_intf_status wcd9xxx_intf = -1;

static struct mfd_cell tavil_devs[] = {
	{
		.name = "qcom-wcd-pinctrl",
		.of_compatible = "qcom,wcd-pinctrl",
	},
	{
		.name = "tavil_codec",
	},
};

static struct mfd_cell tasha_devs[] = {
	{
		.name = "tasha_codec",
	},
};

static struct mfd_cell tomtom_devs[] = {
	{
		.name = "tomtom_codec",
	},
};

static int wcd9xxx_read_of_property_u32(struct device *dev, const char *name,
					u32 *val)
{
	int rc = 0;

	rc = of_property_read_u32(dev->of_node, name, val);
	if (rc)
		dev_err(dev, "%s: Looking up %s property in node %s failed",
			__func__, name, dev->of_node->full_name);

	return rc;
}

static void wcd9xxx_dt_parse_micbias_info(struct device *dev,
					  struct wcd9xxx_micbias_setting *mb)
{
	u32 prop_val;
	int rc;

	if (of_find_property(dev->of_node, "qcom,cdc-micbias-ldoh-v", NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias-ldoh-v",
						  &prop_val);
		if (!rc)
			mb->ldoh_v  =  (u8)prop_val;
	}

	/* MB1 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias-cfilt1-mv",
			     NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias-cfilt1-mv",
						   &prop_val);
		if (!rc)
			mb->cfilt1_mv = prop_val;

		rc = wcd9xxx_read_of_property_u32(dev,
						"qcom,cdc-micbias1-cfilt-sel",
						&prop_val);
		if (!rc)
			mb->bias1_cfilt_sel = (u8)prop_val;

	} else if (of_find_property(dev->of_node, "qcom,cdc-micbias1-mv",
				    NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias1-mv",
						  &prop_val);
		if (!rc)
			mb->micb1_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias1 DT property not found\n",
			__func__);
	}

	/* MB2 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias-cfilt2-mv",
			     NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias-cfilt2-mv",
						   &prop_val);
		if (!rc)
			mb->cfilt2_mv = prop_val;

		rc = wcd9xxx_read_of_property_u32(dev,
						"qcom,cdc-micbias2-cfilt-sel",
						&prop_val);
		if (!rc)
			mb->bias2_cfilt_sel = (u8)prop_val;

	} else if (of_find_property(dev->of_node, "qcom,cdc-micbias2-mv",
				    NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias2-mv",
						  &prop_val);
		if (!rc)
			mb->micb2_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias2 DT property not found\n",
			__func__);
	}

	/* MB3 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias-cfilt3-mv",
			     NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias-cfilt3-mv",
						   &prop_val);
		if (!rc)
			mb->cfilt3_mv = prop_val;

		rc = wcd9xxx_read_of_property_u32(dev,
						"qcom,cdc-micbias3-cfilt-sel",
						&prop_val);
		if (!rc)
			mb->bias3_cfilt_sel = (u8)prop_val;

	} else if (of_find_property(dev->of_node, "qcom,cdc-micbias3-mv",
				    NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias3-mv",
						  &prop_val);
		if (!rc)
			mb->micb3_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias3 DT property not found\n",
			__func__);
	}

	/* MB4 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias4-cfilt-sel",
			     NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						"qcom,cdc-micbias4-cfilt-sel",
						&prop_val);
		if (!rc)
			mb->bias4_cfilt_sel = (u8)prop_val;

	} else if (of_find_property(dev->of_node, "qcom,cdc-micbias4-mv",
				    NULL)) {
		rc = wcd9xxx_read_of_property_u32(dev,
						  "qcom,cdc-micbias4-mv",
						  &prop_val);
		if (!rc)
			mb->micb4_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias4 DT property not found\n",
			__func__);
	}

	mb->bias1_cap_mode =
	   (of_property_read_bool(dev->of_node, "qcom,cdc-micbias1-ext-cap") ?
	 MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
	mb->bias2_cap_mode =
	   (of_property_read_bool(dev->of_node, "qcom,cdc-micbias2-ext-cap") ?
	    MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
	mb->bias3_cap_mode =
	   (of_property_read_bool(dev->of_node, "qcom,cdc-micbias3-ext-cap") ?
	    MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);
	mb->bias4_cap_mode =
	   (of_property_read_bool(dev->of_node, "qcom,cdc-micbias4-ext-cap") ?
	    MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);

	mb->bias2_is_headset_only =
		of_property_read_bool(dev->of_node,
				      "qcom,cdc-micbias2-headset-only");

	/* Print micbias info */
	dev_dbg(dev, "%s: ldoh_v  %u cfilt1_mv %u cfilt2_mv %u cfilt3_mv %u",
		__func__, (u32)mb->ldoh_v, (u32)mb->cfilt1_mv,
		(u32)mb->cfilt2_mv, (u32)mb->cfilt3_mv);

	dev_dbg(dev, "%s: micb1_mv %u micb2_mv %u micb3_mv %u micb4_mv %u",
		__func__, mb->micb1_mv, mb->micb2_mv,
		mb->micb3_mv, mb->micb4_mv);

	dev_dbg(dev, "%s: bias1_cfilt_sel %u bias2_cfilt_sel %u\n",
		__func__, (u32)mb->bias1_cfilt_sel, (u32)mb->bias2_cfilt_sel);

	dev_dbg(dev, "%s: bias3_cfilt_sel %u bias4_cfilt_sel %u\n",
		__func__, (u32)mb->bias3_cfilt_sel, (u32)mb->bias4_cfilt_sel);

	dev_dbg(dev, "%s: bias1_ext_cap %d bias2_ext_cap %d\n",
		__func__, mb->bias1_cap_mode, mb->bias2_cap_mode);

	dev_dbg(dev, "%s: bias3_ext_cap %d bias4_ext_cap %d\n",
		__func__, mb->bias3_cap_mode, mb->bias4_cap_mode);

	dev_dbg(dev, "%s: bias2_is_headset_only %d\n",
		__func__, mb->bias2_is_headset_only);
}

/*
 * wcd9xxx_validate_dmic_sample_rate:
 *	Given the dmic_sample_rate and mclk rate, validate the
 *	dmic_sample_rate. If dmic rate is found to be invalid,
 *	assign the dmic rate as undefined, so individual codec
 *	drivers can use their own defaults
 * @dev: the device for which the dmic is to be configured
 * @dmic_sample_rate: The input dmic_sample_rate
 * @mclk_rate: The input codec mclk rate
 * @dmic_rate_type: String to indicate the type of dmic sample
 *		    rate, used for debug/error logging.
 */
static u32 wcd9xxx_validate_dmic_sample_rate(struct device *dev,
		u32 dmic_sample_rate, u32 mclk_rate,
		const char *dmic_rate_type)
{
	u32 div_factor;

	if (dmic_sample_rate == WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED ||
	    mclk_rate % dmic_sample_rate != 0)
		goto undefined_rate;

	div_factor = mclk_rate / dmic_sample_rate;

	switch (div_factor) {
	case 2:
	case 3:
	case 4:
	case 8:
	case 16:
		/* Valid dmic DIV factors */
		dev_dbg(dev, "%s: DMIC_DIV = %u, mclk_rate = %u\n",
			__func__, div_factor, mclk_rate);
		break;
	case 6:
		/* DIV 6 is valid only for 12.288 MCLK */
		if (mclk_rate != WCD9XXX_MCLK_CLK_12P288MHZ)
			goto undefined_rate;
		break;
	default:
		/* Any other DIV factor is invalid */
		goto undefined_rate;
	}

	return dmic_sample_rate;

undefined_rate:
	dev_info(dev, "%s: Invalid %s = %d, for mclk %d\n",
		 __func__, dmic_rate_type, dmic_sample_rate, mclk_rate);
	dmic_sample_rate = WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED;

	return dmic_sample_rate;
}

/*
 * wcd9xxx_populate_dt_data:
 *	Parse device tree properties for the given codec device
 *
 * @dev: pointer to codec device
 *
 * Returns pointer to the platform data resulting from parsing
 * device tree.
 */
struct wcd9xxx_pdata *wcd9xxx_populate_dt_data(struct device *dev)
{
	struct wcd9xxx_pdata *pdata;
	u32 dmic_sample_rate = WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED;
	u32 mad_dmic_sample_rate = WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED;
	u32 dmic_clk_drive = WCD9XXX_DMIC_CLK_DRIVE_UNDEFINED;
	u32 prop_val;

	if (!dev || !dev->of_node)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(struct wcd9xxx_pdata),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	/* Parse power supplies */
	msm_cdc_get_power_supplies(dev, &pdata->regulator,
				   &pdata->num_supplies);
	if (!pdata->regulator || (pdata->num_supplies <= 0)) {
		dev_err(dev, "%s: no power supplies defined for codec\n",
			__func__);
		goto err_power_sup;
	}

	/* Parse micbias info */
	wcd9xxx_dt_parse_micbias_info(dev, &pdata->micbias);

	pdata->wcd_rst_np = of_parse_phandle(dev->of_node,
					     "qcom,wcd-rst-gpio-node", 0);
	if (!pdata->wcd_rst_np) {
		dev_err(dev, "%s: Looking up %s property in node %s failed\n",
			__func__, "qcom,wcd-rst-gpio-node",
			dev->of_node->full_name);
		goto err_parse_dt_prop;
	}

	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-mclk-clk-rate",
					   &prop_val)))
		pdata->mclk_rate = prop_val;

	if (pdata->mclk_rate != WCD9XXX_MCLK_CLK_9P6HZ &&
	    pdata->mclk_rate != WCD9XXX_MCLK_CLK_12P288MHZ) {
		dev_err(dev, "%s: Invalid mclk_rate = %u\n", __func__,
			pdata->mclk_rate);
		goto err_parse_dt_prop;
	}

	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-dmic-sample-rate",
					   &prop_val)))
		dmic_sample_rate = prop_val;

	pdata->dmic_sample_rate = wcd9xxx_validate_dmic_sample_rate(dev,
							dmic_sample_rate,
							pdata->mclk_rate,
							"audio_dmic_rate");
	if (!(wcd9xxx_read_of_property_u32(dev, "qcom,cdc-mad-dmic-rate",
					   &prop_val)))
		mad_dmic_sample_rate = prop_val;

	pdata->mad_dmic_sample_rate = wcd9xxx_validate_dmic_sample_rate(dev,
							mad_dmic_sample_rate,
							pdata->mclk_rate,
							"mad_dmic_rate");

	if (!(of_property_read_u32(dev->of_node,
				   "qcom,cdc-dmic-clk-drv-strength",
				   &prop_val)))
		dmic_clk_drive = prop_val;

	if (dmic_clk_drive != 2 && dmic_clk_drive != 4 &&
	    dmic_clk_drive != 8 && dmic_clk_drive != 16)
		dev_err(dev, "Invalid cdc-dmic-clk-drv-strength %d\n",
			dmic_clk_drive);

	pdata->dmic_clk_drv = dmic_clk_drive;

	return pdata;

err_parse_dt_prop:
	devm_kfree(dev, pdata->regulator);
	pdata->regulator = NULL;
	pdata->num_supplies = 0;
err_power_sup:
	devm_kfree(dev, pdata);
	return NULL;
}
EXPORT_SYMBOL(wcd9xxx_populate_dt_data);

static bool is_wcd9xxx_reg_power_down(struct wcd9xxx *wcd9xxx, u16 rreg)
{
	bool ret = false;
	int i;
	struct wcd9xxx_power_region *wcd9xxx_pwr;

	if (!wcd9xxx)
		return ret;

	for (i = 0; i < WCD9XXX_MAX_PWR_REGIONS; i++) {
		wcd9xxx_pwr = wcd9xxx->wcd9xxx_pwr[i];
		if (!wcd9xxx_pwr)
			continue;
		if (((wcd9xxx_pwr->pwr_collapse_reg_min == 0) &&
		     (wcd9xxx_pwr->pwr_collapse_reg_max == 0)) ||
		    (wcd9xxx_pwr->power_state ==
		     WCD_REGION_POWER_COLLAPSE_REMOVE))
			ret = false;
		else if (((wcd9xxx_pwr->power_state ==
			   WCD_REGION_POWER_DOWN) ||
			  (wcd9xxx_pwr->power_state ==
			   WCD_REGION_POWER_COLLAPSE_BEGIN)) &&
			 (rreg >= wcd9xxx_pwr->pwr_collapse_reg_min) &&
			 (rreg <= wcd9xxx_pwr->pwr_collapse_reg_max))
			ret = true;
	}
	return ret;
}

/*
 * wcd9xxx_page_write:
 *	Retrieve page number from register and
 *	write that page number to the page address.
 *	Called under io_lock acquisition.
 *
 * @wcd9xxx: pointer to wcd9xxx
 * @reg: Register address from which page number is retrieved
 *
 * Returns 0 for success and negative error code for failure.
 */
int wcd9xxx_page_write(struct wcd9xxx *wcd9xxx, unsigned short *reg)
{
	int ret = 0;
	unsigned short c_reg, reg_addr;
	u8 pg_num, prev_pg_num;

	if (wcd9xxx->type != WCD9335 && wcd9xxx->type != WCD934X)
		return ret;

	c_reg = *reg;
	pg_num = c_reg >> 8;
	reg_addr = c_reg & 0xff;
	if (wcd9xxx->prev_pg_valid) {
		prev_pg_num = wcd9xxx->prev_pg;
		if (prev_pg_num != pg_num) {
			ret = wcd9xxx->write_dev(
					wcd9xxx, PAGE_REG_ADDR, 1,
					(void *) &pg_num, false);
			if (ret < 0)
				pr_err("page write error, pg_num: 0x%x\n",
					pg_num);
			else {
				wcd9xxx->prev_pg = pg_num;
				dev_dbg(wcd9xxx->dev, "%s: Page 0x%x Write to 0x00\n",
					__func__, pg_num);
			}
		}
	} else {
		ret = wcd9xxx->write_dev(
				wcd9xxx, PAGE_REG_ADDR, 1, (void *) &pg_num,
				false);
		if (ret < 0)
			pr_err("page write error, pg_num: 0x%x\n", pg_num);
		else {
			wcd9xxx->prev_pg = pg_num;
			wcd9xxx->prev_pg_valid = true;
			dev_dbg(wcd9xxx->dev, "%s: Page 0x%x Write to 0x00\n",
				__func__, pg_num);
		}
	}
	*reg = reg_addr;
	return ret;
}
EXPORT_SYMBOL(wcd9xxx_page_write);

static int regmap_bus_read(void *context, const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	struct device *dev = context;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(dev);
	unsigned short c_reg, rreg;
	int ret, i;

	if (!wcd9xxx) {
		dev_err(dev, "%s: wcd9xxx is NULL\n", __func__);
		return -EINVAL;
	}
	if (!reg || !val) {
		dev_err(dev, "%s: reg or val is NULL\n", __func__);
		return -EINVAL;
	}

	if (reg_size != REG_BYTES) {
		dev_err(dev, "%s: register size %zd bytes, not supported\n",
			__func__, reg_size);
		return -EINVAL;
	}

	mutex_lock(&wcd9xxx->io_lock);
	c_reg = *(u16 *)reg;
	rreg = c_reg;

	if (is_wcd9xxx_reg_power_down(wcd9xxx, rreg)) {
		ret = 0;
		for (i = 0; i < val_size; i++)
			((u8 *)val)[i] = 0;
		goto err;
	}
	ret = wcd9xxx_page_write(wcd9xxx, &c_reg);
	if (ret)
		goto err;
	ret = wcd9xxx->read_dev(wcd9xxx, c_reg, val_size, val, false);
	if (ret < 0)
		dev_err(dev, "%s: Codec read failed (%d), reg: 0x%x, size:%zd\n",
			__func__, ret, rreg, val_size);
	else {
		for (i = 0; i < val_size; i++)
			dev_dbg(dev, "%s: Read 0x%02x from 0x%x\n",
				__func__, ((u8 *)val)[i], rreg + i);
	}
err:
	mutex_unlock(&wcd9xxx->io_lock);

	return ret;
}

static int regmap_bus_gather_write(void *context,
				   const void *reg, size_t reg_size,
				   const void *val, size_t val_size)
{
	struct device *dev = context;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(dev);
	unsigned short c_reg, rreg;
	int ret, i;

	if (!wcd9xxx) {
		dev_err(dev, "%s: wcd9xxx is NULL\n", __func__);
		return -EINVAL;
	}
	if (!reg || !val) {
		dev_err(dev, "%s: reg or val is NULL\n", __func__);
		return -EINVAL;
	}
	if (reg_size != REG_BYTES) {
		dev_err(dev, "%s: register size %zd bytes, not supported\n",
			__func__, reg_size);
		return -EINVAL;
	}
	mutex_lock(&wcd9xxx->io_lock);
	c_reg = *(u16 *)reg;
	rreg = c_reg;

	if (is_wcd9xxx_reg_power_down(wcd9xxx, rreg)) {
		ret = 0;
		goto err;
	}
	ret = wcd9xxx_page_write(wcd9xxx, &c_reg);
	if (ret)
		goto err;

	for (i = 0; i < val_size; i++)
		dev_dbg(dev, "Write %02x to 0x%x\n", ((u8 *)val)[i],
			rreg + i);

	ret = wcd9xxx->write_dev(wcd9xxx, c_reg, val_size, (void *) val,
				 false);
	if (ret < 0)
		dev_err(dev, "%s: Codec write failed (%d), reg:0x%x, size:%zd\n",
			__func__, ret, rreg, val_size);

err:
	mutex_unlock(&wcd9xxx->io_lock);
	return ret;
}

static int regmap_bus_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(dev);

	if (!wcd9xxx)
		return -EINVAL;

	WARN_ON(count < REG_BYTES);

	if (count > (REG_BYTES + VAL_BYTES)) {
		if (wcd9xxx->multi_reg_write)
			return wcd9xxx->multi_reg_write(wcd9xxx,
							data, count);
	} else
		return regmap_bus_gather_write(context, data, REG_BYTES,
					       data + REG_BYTES,
					       count - REG_BYTES);

	dev_err(dev, "%s: bus multi reg write failure\n", __func__);

	return -EINVAL;
}

static struct regmap_bus regmap_bus_config = {
	.write = regmap_bus_write,
	.gather_write = regmap_bus_gather_write,
	.read = regmap_bus_read,
	.reg_format_endian_default = REGMAP_ENDIAN_NATIVE,
	.val_format_endian_default = REGMAP_ENDIAN_NATIVE,
};

/*
 * wcd9xxx_regmap_init:
 *	Initialize wcd9xxx register map
 *
 * @dev: pointer to wcd device
 * @config: pointer to register map config
 *
 * Returns pointer to regmap structure for success
 * or NULL in case of failure.
 */
struct regmap *wcd9xxx_regmap_init(struct device *dev,
				   const struct regmap_config *config)
{
	return devm_regmap_init(dev, &regmap_bus_config, dev, config);
}
EXPORT_SYMBOL(wcd9xxx_regmap_init);

/*
 * wcd9xxx_reset:
 *	Reset wcd9xxx codec
 *
 * @dev: pointer to wcd device
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_reset(struct device *dev)
{
	struct wcd9xxx *wcd9xxx;
	int rc;
	int value;

	if (!dev)
		return -ENODEV;

	wcd9xxx = dev_get_drvdata(dev);
	if (!wcd9xxx)
		return -EINVAL;

	if (!wcd9xxx->wcd_rst_np) {
		dev_err(dev, "%s: reset gpio device node not specified\n",
			__func__);
		return -EINVAL;
	}

	value = msm_cdc_get_gpio_state(wcd9xxx->wcd_rst_np);
	if (value > 0) {
		wcd9xxx->avoid_cdc_rstlow = 1;
		return 0;
	}

	rc = msm_cdc_pinctrl_select_sleep_state(wcd9xxx->wcd_rst_np);
	if (rc) {
		dev_err(dev, "%s: wcd sleep state request fail!\n",
			__func__);
		return rc;
	}

	/* 20ms sleep required after pulling the reset gpio to LOW */
	msleep(20);

	rc = msm_cdc_pinctrl_select_active_state(wcd9xxx->wcd_rst_np);
	if (rc) {
		dev_err(dev, "%s: wcd active state request fail!\n",
			__func__);
		return rc;
	}
	msleep(20);

	return rc;
}
EXPORT_SYMBOL(wcd9xxx_reset);

/*
 * wcd9xxx_reset_low:
 *	Pull the wcd9xxx codec reset_n to low
 *
 * @dev: pointer to wcd device
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_reset_low(struct device *dev)
{
	struct wcd9xxx *wcd9xxx;
	int rc;

	if (!dev)
		return -ENODEV;

	wcd9xxx = dev_get_drvdata(dev);
	if (!wcd9xxx)
		return -EINVAL;

	if (!wcd9xxx->wcd_rst_np) {
		dev_err(dev, "%s: reset gpio device node not specified\n",
			__func__);
		return -EINVAL;
	}
	if (wcd9xxx->avoid_cdc_rstlow) {
		wcd9xxx->avoid_cdc_rstlow = 0;
		dev_dbg(dev, "%s: avoid pull down of reset GPIO\n", __func__);
		return 0;
	}

	rc = msm_cdc_pinctrl_select_sleep_state(wcd9xxx->wcd_rst_np);
	if (rc)
		dev_err(dev, "%s: wcd sleep state request fail!\n",
			__func__);

	return rc;
}
EXPORT_SYMBOL(wcd9xxx_reset_low);

/*
 * wcd9xxx_bringup:
 *	Toggle reset analog and digital cores of wcd9xxx codec
 *
 * @dev: pointer to wcd device
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_bringup(struct device *dev)
{
	struct wcd9xxx *wcd9xxx;
	int rc;
	codec_bringup_fn cdc_bup_fn;

	if (!dev)
		return -ENODEV;

	wcd9xxx = dev_get_drvdata(dev);
	if (!wcd9xxx)
		return -EINVAL;

	cdc_bup_fn = wcd9xxx_bringup_fn(wcd9xxx->type);
	if (!cdc_bup_fn) {
		dev_err(dev, "%s: Codec bringup fn NULL!\n",
			__func__);
		return -EINVAL;
	}
	rc = cdc_bup_fn(wcd9xxx);
	if (rc)
		dev_err(dev, "%s: Codec bringup error, rc: %d\n",
			__func__, rc);

	return rc;
}
EXPORT_SYMBOL(wcd9xxx_bringup);

/*
 * wcd9xxx_bringup:
 *	Set analog and digital cores of wcd9xxx codec in reset state
 *
 * @dev: pointer to wcd device
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_bringdown(struct device *dev)
{
	struct wcd9xxx *wcd9xxx;
	int rc;
	codec_bringdown_fn cdc_bdown_fn;

	if (!dev)
		return -ENODEV;

	wcd9xxx = dev_get_drvdata(dev);
	if (!wcd9xxx)
		return -EINVAL;

	cdc_bdown_fn = wcd9xxx_bringdown_fn(wcd9xxx->type);
	if (!cdc_bdown_fn) {
		dev_err(dev, "%s: Codec bring down fn NULL!\n",
			__func__);
		return -EINVAL;
	}
	rc = cdc_bdown_fn(wcd9xxx);
	if (rc)
		dev_err(dev, "%s: Codec bring down error, rc: %d\n",
			__func__, rc);

	return rc;
}
EXPORT_SYMBOL(wcd9xxx_bringdown);

/*
 * wcd9xxx_get_codec_info:
 *	Fill codec specific information like interrupts, version
 *
 * @dev: pointer to wcd device
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_get_codec_info(struct device *dev)
{
	struct wcd9xxx *wcd9xxx;
	int rc;
	codec_type_fn cdc_type_fn;
	struct wcd9xxx_codec_type *cinfo;

	if (!dev)
		return -ENODEV;

	wcd9xxx = dev_get_drvdata(dev);
	if (!wcd9xxx)
		return -EINVAL;

	cdc_type_fn = wcd9xxx_get_codec_info_fn(wcd9xxx->type);
	if (!cdc_type_fn) {
		dev_err(dev, "%s: Codec fill type fn NULL!\n",
			__func__);
		return -EINVAL;
	}

	cinfo = wcd9xxx->codec_type;
	if (!cinfo)
		return -EINVAL;

	rc = cdc_type_fn(wcd9xxx, cinfo);
	if (rc) {
		dev_err(dev, "%s: Codec type fill failed, rc:%d\n",
			__func__, rc);
		return rc;

	}

	switch (wcd9xxx->type) {
	case WCD934X:
		cinfo->dev = tavil_devs;
		cinfo->size = ARRAY_SIZE(tavil_devs);
		break;
	case WCD9335:
		cinfo->dev = tasha_devs;
		cinfo->size = ARRAY_SIZE(tasha_devs);
		break;
	case WCD9330:
		cinfo->dev = tomtom_devs;
		cinfo->size = ARRAY_SIZE(tomtom_devs);
		break;
	default:
		cinfo->dev = NULL;
		cinfo->size = 0;
		break;
	}

	return rc;
}
EXPORT_SYMBOL(wcd9xxx_get_codec_info);

/*
 * wcd9xxx_core_irq_init:
 *	Initialize wcd9xxx codec irq instance
 *
 * @wcd9xxx_core_res: pointer to wcd core resource
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_core_irq_init(
	struct wcd9xxx_core_resource *wcd9xxx_core_res)
{
	int ret = 0;

	if (!wcd9xxx_core_res)
		return -EINVAL;

	if (wcd9xxx_core_res->irq != 1) {
		ret = wcd9xxx_irq_init(wcd9xxx_core_res);
		if (ret)
			pr_err("IRQ initialization failed\n");
	}

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_core_irq_init);

/*
 * wcd9xxx_assign_irq:
 *	Assign irq and irq_base to wcd9xxx core resource
 *
 * @wcd9xxx_core_res: pointer to wcd core resource
 * @irq: irq number
 * @irq_base: base irq number
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_assign_irq(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	unsigned int irq,
	unsigned int irq_base)
{
	if (!wcd9xxx_core_res)
		return -EINVAL;

	wcd9xxx_core_res->irq = irq;
	wcd9xxx_core_res->irq_base = irq_base;

	return 0;
}
EXPORT_SYMBOL(wcd9xxx_assign_irq);

/*
 * wcd9xxx_core_res_init:
 *	Initialize wcd core resource instance
 *
 * @wcd9xxx_core_res: pointer to wcd core resource
 * @num_irqs: number of irqs for wcd9xxx core
 * @num_irq_regs: number of irq registers
 * @wcd_regmap: pointer to the wcd register map
 *
 * Returns 0 for success or negative error code in case of failure
 */
int wcd9xxx_core_res_init(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	int num_irqs, int num_irq_regs, struct regmap *wcd_regmap)
{
	if (!wcd9xxx_core_res || !wcd_regmap)
		return -EINVAL;

	mutex_init(&wcd9xxx_core_res->pm_lock);
	wcd9xxx_core_res->wlock_holders = 0;
	wcd9xxx_core_res->pm_state = WCD9XXX_PM_SLEEPABLE;
	init_waitqueue_head(&wcd9xxx_core_res->pm_wq);
	pm_qos_add_request(&wcd9xxx_core_res->pm_qos_req,
				PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	wcd9xxx_core_res->num_irqs = num_irqs;
	wcd9xxx_core_res->num_irq_regs = num_irq_regs;
	wcd9xxx_core_res->wcd_core_regmap = wcd_regmap;

	pr_info("%s: num_irqs = %d, num_irq_regs = %d\n",
			__func__, wcd9xxx_core_res->num_irqs,
			wcd9xxx_core_res->num_irq_regs);

	return 0;
}
EXPORT_SYMBOL(wcd9xxx_core_res_init);

/*
 * wcd9xxx_core_res_deinit:
 *	Deinit wcd core resource instance
 *
 * @wcd9xxx_core_res: pointer to wcd core resource
 */
void wcd9xxx_core_res_deinit(struct wcd9xxx_core_resource *wcd9xxx_core_res)
{
	if (!wcd9xxx_core_res)
		return;

	pm_qos_remove_request(&wcd9xxx_core_res->pm_qos_req);
	mutex_destroy(&wcd9xxx_core_res->pm_lock);
}
EXPORT_SYMBOL(wcd9xxx_core_res_deinit);

/*
 * wcd9xxx_pm_cmpxchg:
 *	Check old state and exchange with pm new state
 *	if old state matches with current state
 *
 * @wcd9xxx_core_res: pointer to wcd core resource
 * @o: pm old state
 * @n: pm new state
 *
 * Returns old state
 */
enum wcd9xxx_pm_state wcd9xxx_pm_cmpxchg(
		struct wcd9xxx_core_resource *wcd9xxx_core_res,
		enum wcd9xxx_pm_state o,
		enum wcd9xxx_pm_state n)
{
	enum wcd9xxx_pm_state old;

	if (!wcd9xxx_core_res)
		return o;

	mutex_lock(&wcd9xxx_core_res->pm_lock);
	old = wcd9xxx_core_res->pm_state;
	if (old == o)
		wcd9xxx_core_res->pm_state = n;
	mutex_unlock(&wcd9xxx_core_res->pm_lock);

	return old;
}
EXPORT_SYMBOL(wcd9xxx_pm_cmpxchg);

/*
 * wcd9xxx_core_res_suspend:
 *	Suspend callback function for wcd9xxx core
 *
 * @wcd9xxx_core_res: pointer to wcd core resource
 * @pm_message_t: pm message
 *
 * Returns 0 for success or negative error code for failure/busy
 */
int wcd9xxx_core_res_suspend(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	pm_message_t pmesg)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	/*
	 * pm_qos_update_request() can be called after this suspend chain call
	 * started. thus suspend can be called while lock is being held
	 */
	mutex_lock(&wcd9xxx_core_res->pm_lock);
	if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_SLEEPABLE) {
		pr_debug("%s: suspending system, state %d, wlock %d\n",
			 __func__, wcd9xxx_core_res->pm_state,
			 wcd9xxx_core_res->wlock_holders);
		wcd9xxx_core_res->pm_state = WCD9XXX_PM_ASLEEP;
	} else if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_AWAKE) {
		/*
		 * unlock to wait for pm_state == WCD9XXX_PM_SLEEPABLE
		 * then set to WCD9XXX_PM_ASLEEP
		 */
		pr_debug("%s: waiting to suspend system, state %d, wlock %d\n",
			 __func__, wcd9xxx_core_res->pm_state,
			 wcd9xxx_core_res->wlock_holders);
		mutex_unlock(&wcd9xxx_core_res->pm_lock);
		if (!(wait_event_timeout(wcd9xxx_core_res->pm_wq,
					 wcd9xxx_pm_cmpxchg(wcd9xxx_core_res,
						  WCD9XXX_PM_SLEEPABLE,
						  WCD9XXX_PM_ASLEEP) ==
							WCD9XXX_PM_SLEEPABLE,
					 HZ))) {
			pr_debug("%s: suspend failed state %d, wlock %d\n",
				 __func__, wcd9xxx_core_res->pm_state,
				 wcd9xxx_core_res->wlock_holders);
			ret = -EBUSY;
		} else {
			pr_debug("%s: done, state %d, wlock %d\n", __func__,
				 wcd9xxx_core_res->pm_state,
				 wcd9xxx_core_res->wlock_holders);
		}
		mutex_lock(&wcd9xxx_core_res->pm_lock);
	} else if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_ASLEEP) {
		pr_warn("%s: system is already suspended, state %d, wlock %dn",
			__func__, wcd9xxx_core_res->pm_state,
			wcd9xxx_core_res->wlock_holders);
	}
	mutex_unlock(&wcd9xxx_core_res->pm_lock);

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_core_res_suspend);

/*
 * wcd9xxx_core_res_resume:
 *	Resume callback function for wcd9xxx core
 *
 * @wcd9xxx_core_res: pointer to wcd core resource
 *
 * Returns 0 for success or negative error code for failure/busy
 */
int wcd9xxx_core_res_resume(
	struct wcd9xxx_core_resource *wcd9xxx_core_res)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	mutex_lock(&wcd9xxx_core_res->pm_lock);
	if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_ASLEEP) {
		pr_debug("%s: resuming system, state %d, wlock %d\n", __func__,
				wcd9xxx_core_res->pm_state,
				wcd9xxx_core_res->wlock_holders);
		wcd9xxx_core_res->pm_state = WCD9XXX_PM_SLEEPABLE;
	} else {
		pr_warn("%s: system is already awake, state %d wlock %d\n",
				__func__, wcd9xxx_core_res->pm_state,
				wcd9xxx_core_res->wlock_holders);
	}
	mutex_unlock(&wcd9xxx_core_res->pm_lock);
	wake_up_all(&wcd9xxx_core_res->pm_wq);

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_core_res_resume);

/*
 * wcd9xxx_get_intf_type:
 *	Get interface type of wcd9xxx core
 *
 * Returns interface type
 */
enum wcd9xxx_intf_status wcd9xxx_get_intf_type(void)
{
	return wcd9xxx_intf;
}
EXPORT_SYMBOL(wcd9xxx_get_intf_type);

/*
 * wcd9xxx_set_intf_type:
 *	Set interface type of wcd9xxx core
 *
 */
void wcd9xxx_set_intf_type(enum wcd9xxx_intf_status intf_status)
{
	wcd9xxx_intf = intf_status;
}
EXPORT_SYMBOL(wcd9xxx_set_intf_type);

/*
 * wcd9xxx_set_power_state: set power state for the region
 * @wcd9xxx: handle to wcd core
 * @state: power state to be set
 * @region: region index
 *
 * Returns error code in case of failure or 0 for success
 */
int wcd9xxx_set_power_state(struct wcd9xxx *wcd9xxx,
			    enum codec_power_states state,
			    enum wcd_power_regions region)
{
	if (!wcd9xxx) {
		pr_err("%s: wcd9xxx is NULL\n", __func__);
		return -EINVAL;
	}

	if ((region < 0) || (region >= WCD9XXX_MAX_PWR_REGIONS)) {
		dev_err(wcd9xxx->dev, "%s: region index %d out of bounds\n",
			__func__, region);
		return -EINVAL;
	}
	if (!wcd9xxx->wcd9xxx_pwr[region]) {
		dev_err(wcd9xxx->dev, "%s: memory not created for region: %d\n",
			__func__, region);
		return -EINVAL;
	}
	mutex_lock(&wcd9xxx->io_lock);
	wcd9xxx->wcd9xxx_pwr[region]->power_state = state;
	mutex_unlock(&wcd9xxx->io_lock);

	return 0;
}
EXPORT_SYMBOL(wcd9xxx_set_power_state);

/*
 * wcd9xxx_get_current_power_state: Get power state of the region
 * @wcd9xxx: handle to wcd core
 * @region: region index
 *
 * Returns current power state of the region or error code for failure
 */
int wcd9xxx_get_current_power_state(struct wcd9xxx *wcd9xxx,
				    enum wcd_power_regions region)
{
	int state;

	if (!wcd9xxx) {
		pr_err("%s: wcd9xxx is NULL\n", __func__);
		return -EINVAL;
	}

	if ((region < 0) || (region >= WCD9XXX_MAX_PWR_REGIONS)) {
		dev_err(wcd9xxx->dev, "%s: region index %d out of bounds\n",
			__func__, region);
		return -EINVAL;
	}
	if (!wcd9xxx->wcd9xxx_pwr[region]) {
		dev_err(wcd9xxx->dev, "%s: memory not created for region: %d\n",
			__func__, region);
		return -EINVAL;
	}

	mutex_lock(&wcd9xxx->io_lock);
	state = wcd9xxx->wcd9xxx_pwr[region]->power_state;
	mutex_unlock(&wcd9xxx->io_lock);

	return state;
}
EXPORT_SYMBOL(wcd9xxx_get_current_power_state);

