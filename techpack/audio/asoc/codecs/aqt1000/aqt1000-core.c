// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/mfd/core.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <asoc/msm-cdc-pinctrl.h>
#include <asoc/msm-cdc-supply.h>
#include "aqt1000-registers.h"
#include "aqt1000-internal.h"
#include "aqt1000.h"
#include "aqt1000-utils.h"
#include "aqt1000-irq.h"

static int aqt1000_bringup(struct aqt1000 *aqt)
{
	struct aqt1000_pdata *pdata;
	u8 clk_div = 0, mclk = 1;

	if (!aqt->regmap) {
		dev_err(aqt->dev, "%s: aqt regmap is NULL\n", __func__);
		return -EINVAL;
	}

	/* Bringup register write sequence */
	regmap_update_bits(aqt->regmap, AQT1000_BUCK_5V_CTRL_CCL_1, 0xF0, 0xF0);
	regmap_update_bits(aqt->regmap, AQT1000_BIAS_CCOMP_FINE_ADJ,
			   0xF0, 0x90);
	regmap_update_bits(aqt->regmap, AQT1000_ANA_BIAS, 0x80, 0x80);
	regmap_update_bits(aqt->regmap, AQT1000_ANA_BIAS, 0x40, 0x40);

	/* Added 1msec sleep as per HW requirement */
	usleep_range(1000, 1010);

	regmap_update_bits(aqt->regmap, AQT1000_ANA_BIAS, 0x40, 0x00);

	clk_div = 0x04; /* Assumption is CLK DIV 2 */
	pdata = dev_get_platdata(aqt->dev);
	if (pdata) {
		if (pdata->mclk_rate == AQT1000_CLK_12P288MHZ)
			mclk = 0;
		clk_div = (((pdata->ext_clk_rate / pdata->mclk_rate) >> 1)
				<< 2);
	}
	regmap_update_bits(aqt->regmap, AQT1000_CHIP_CFG0_CLK_CFG_MCLK,
			   0x03, mclk);

	regmap_update_bits(aqt->regmap, AQT1000_CLK_SYS_MCLK1_PRG,
			   0x0C, clk_div);

	/* Source clock enable */
	regmap_update_bits(aqt->regmap, AQT1000_CLK_SYS_MCLK1_PRG, 0x02, 0x02);

	/* Ungate the source clock */
	regmap_update_bits(aqt->regmap, AQT1000_CLK_SYS_MCLK1_PRG, 0x10, 0x10);

	/* Set the I2S_HS_CLK reference to CLK DIV 2 */
	regmap_update_bits(aqt->regmap, AQT1000_CLK_SYS_MCLK2_I2S_HS_CLK_PRG,
			   0x60, 0x20);

	/* Set the PLL preset to CLK9P6M_IN_12P288M_OUT */
	regmap_update_bits(aqt->regmap, AQT1000_CLK_SYS_PLL_PRESET, 0x0F, 0x02);

	/* Enable clock PLL */
	regmap_update_bits(aqt->regmap, AQT1000_CLK_SYS_PLL_ENABLES,
			   0x01, 0x01);

	/* Add 100usec delay as per HW requirement */
	usleep_range(100, 110);

	/* Set AQT to I2S Master */
	regmap_update_bits(aqt->regmap, AQT1000_I2S_I2S_0_CTL, 0x02, 0x02);

	/* Enable I2S HS clock */
	regmap_update_bits(aqt->regmap, AQT1000_CLK_SYS_MCLK2_I2S_HS_CLK_PRG,
			   0x01, 0x01);

	regmap_update_bits(aqt->regmap, AQT1000_CHIP_CFG0_CLK_CFG_MCLK,
			   0x04, 0x00);

	/* Add 100usec delay as per HW requirement */
	usleep_range(100, 110);
	regmap_update_bits(aqt->regmap, AQT1000_CDC_CLK_RST_CTRL_MCLK_CONTROL,
			   0x01, 0x01);
	regmap_update_bits(aqt->regmap, AQT1000_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
			   0x01, 0x01);
	regmap_update_bits(aqt->regmap, AQT1000_CHIP_CFG0_CLK_CTL_CDC_DIG,
			   0x01, 0x01);

	/* Codec digital reset */
	regmap_update_bits(aqt->regmap, AQT1000_CHIP_CFG0_RST_CTL, 0x01, 0x01);
	/* Add 100usec delay as per HW requirement */
	usleep_range(100, 110);

	return 0;
}

static int aqt1000_device_init(struct aqt1000 *aqt)
{
	int ret = 0;

	mutex_init(&aqt->io_lock);
	mutex_init(&aqt->xfer_lock);
	mutex_init(&aqt->cdc_bg_clk_lock);
	mutex_init(&aqt->master_bias_lock);

	ret = aqt1000_bringup(aqt);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto done;
	}

	ret = aqt_irq_init(aqt);
	if (ret)
		goto done;

	return ret;
done:
	mutex_destroy(&aqt->io_lock);
	mutex_destroy(&aqt->xfer_lock);
	mutex_destroy(&aqt->cdc_bg_clk_lock);
	mutex_destroy(&aqt->master_bias_lock);
	return ret;
}

static int aqt1000_i2c_write(struct aqt1000 *aqt1000, unsigned short reg,
			     void *val, int bytes)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	u8 data[bytes + 1];
	struct aqt1000_i2c *aqt1000_i2c;
	u8 *value = (u8 *)val;

	aqt1000_i2c = &aqt1000->i2c_dev;
	if (aqt1000_i2c == NULL || aqt1000_i2c->client == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}
	reg_addr = (u8)reg;
	msg = &aqt1000_i2c->xfer_msg[0];
	msg->addr = aqt1000_i2c->client->addr;
	msg->len = bytes + 1;
	msg->flags = 0;
	data[0] = reg;
	data[1] = *value;
	msg->buf = data;
	ret = i2c_transfer(aqt1000_i2c->client->adapter,
			   aqt1000_i2c->xfer_msg, 1);
	/* Try again if the write fails */
	if (ret != 1) {
		ret = i2c_transfer(aqt1000_i2c->client->adapter,
						aqt1000_i2c->xfer_msg, 1);
		if (ret != 1) {
			dev_err(aqt1000->dev,
				"%s: I2C write failed, reg: 0x%x ret: %d\n",
				__func__, reg, ret);
			return ret;
		}
	}
	dev_dbg(aqt1000->dev, "%s: write success register = %x val = %x\n",
		__func__, reg, data[1]);
	return 0;
}

static int aqt1000_i2c_read(struct aqt1000 *aqt1000, unsigned short reg,
				  void *dst, int bytes)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	struct aqt1000_i2c *aqt1000_i2c;
	u8 i = 0;
	unsigned char *dest = (unsigned char *)dst;

	aqt1000_i2c = &aqt1000->i2c_dev;
	if (aqt1000_i2c == NULL || aqt1000_i2c->client == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}
	for (i = 0; i < bytes; i++) {
		reg_addr = (u8)reg++;
		msg = &aqt1000_i2c->xfer_msg[0];
		msg->addr = aqt1000_i2c->client->addr;
		msg->len = 1;
		msg->flags = 0;
		msg->buf = &reg_addr;

		msg = &aqt1000_i2c->xfer_msg[1];
		msg->addr = aqt1000_i2c->client->addr;
		msg->len = 1;
		msg->flags = I2C_M_RD;
		msg->buf = dest++;
		ret = i2c_transfer(aqt1000_i2c->client->adapter,
				aqt1000_i2c->xfer_msg, 2);

		/* Try again if read fails first time */
		if (ret != 2) {
			ret = i2c_transfer(aqt1000_i2c->client->adapter,
					   aqt1000_i2c->xfer_msg, 2);
			if (ret != 2) {
				dev_err(aqt1000->dev,
					"%s: I2C read failed, reg: 0x%x\n",
					__func__, reg);
				return ret;
			}
		}
	}
	return 0;
}

static int aqt1000_reset(struct device *dev)
{
	struct aqt1000 *aqt1000;
	int rc = 0;

	if (!dev)
		return -ENODEV;

	aqt1000 = dev_get_drvdata(dev);
	if (!aqt1000)
		return -EINVAL;

	if (!aqt1000->aqt_rst_np) {
		dev_err(dev, "%s: reset gpio device node not specified\n",
			__func__);
		return -EINVAL;
	}

	if (!msm_cdc_pinctrl_get_state(aqt1000->aqt_rst_np)) {
		rc = msm_cdc_pinctrl_select_sleep_state(aqt1000->aqt_rst_np);
		if (rc) {
			dev_err(dev, "%s: aqt sleep state request fail!\n",
				__func__);
			return rc;
		}

		/* 20ms sleep required after pulling the reset gpio to LOW */
		msleep(20);

		rc = msm_cdc_pinctrl_select_active_state(aqt1000->aqt_rst_np);
		if (rc) {
			dev_err(dev,
				"%s: aqt active state request fail, ret: %d\n",
				__func__, rc);
			return rc;
		}
		/* 20ms sleep required after pulling the reset gpio to HIGH */
		msleep(20);
	}

	return rc;
}

static int aqt1000_read_of_property_u32(struct device *dev, const char *name,
					u32 *val)
{
	int rc = 0;

	rc = of_property_read_u32(dev->of_node, name, val);
	if (rc)
		dev_err(dev, "%s: Looking up %s property in node %s failed",
			__func__, name, dev->of_node->full_name);

	return rc;
}

static void aqt1000_dt_parse_micbias_info(struct device *dev,
					  struct aqt1000_micbias_setting *mb)
{
	u32 prop_val;
	int rc;

	if (of_find_property(dev->of_node, "qcom,cdc-micbias-ldoh-v", NULL)) {
		rc = aqt1000_read_of_property_u32(dev,
						  "qcom,cdc-micbias-ldoh-v",
						  &prop_val);
		if (!rc)
			mb->ldoh_v  =  (u8)prop_val;
	}

	/* MB1 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias-cfilt1-mv",
			     NULL)) {
		rc = aqt1000_read_of_property_u32(dev,
						  "qcom,cdc-micbias-cfilt1-mv",
						   &prop_val);
		if (!rc)
			mb->cfilt1_mv = prop_val;

		rc = aqt1000_read_of_property_u32(dev,
						"qcom,cdc-micbias1-cfilt-sel",
						&prop_val);
		if (!rc)
			mb->bias1_cfilt_sel = (u8)prop_val;

	} else if (of_find_property(dev->of_node, "qcom,cdc-micbias1-mv",
				    NULL)) {
		rc = aqt1000_read_of_property_u32(dev,
						  "qcom,cdc-micbias1-mv",
						  &prop_val);
		if (!rc)
			mb->micb1_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias1 DT property not found\n",
			__func__);
	}

	/* Print micbias info */
	dev_dbg(dev, "%s: ldoh_v %u cfilt1_mv %u micb1_mv %u \n", __func__,
		(u32)mb->ldoh_v, (u32)mb->cfilt1_mv, (u32)mb->micb1_mv);
}

static struct aqt1000_pdata *aqt1000_populate_dt_data(struct device *dev)
{
	struct aqt1000_pdata *pdata;
	u32 prop_val;

	if (!dev || !dev->of_node)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(struct aqt1000_pdata),
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
	aqt1000_dt_parse_micbias_info(dev, &pdata->micbias);

	pdata->aqt_rst_np = of_parse_phandle(dev->of_node,
					     "qcom,aqt-rst-gpio-node", 0);
	if (!pdata->aqt_rst_np) {
		dev_err(dev, "%s: Looking up %s property in node %s failed\n",
			__func__, "qcom,aqt-rst-gpio-node",
			dev->of_node->full_name);
		goto err_parse_dt_prop;
	}

	if (!(aqt1000_read_of_property_u32(dev, "qcom,cdc-ext-clk-rate",
					   &prop_val)))
		pdata->ext_clk_rate = prop_val;
	if (pdata->ext_clk_rate != AQT1000_CLK_24P576MHZ &&
	    pdata->ext_clk_rate != AQT1000_CLK_19P2MHZ &&
	    pdata->ext_clk_rate != AQT1000_CLK_12P288MHZ) {
		/* Use the default ext_clk_rate if the DT value is wrong */
		pdata->ext_clk_rate = AQT1000_CLK_9P6MHZ;
	}

	prop_val = 0;
	if (!(aqt1000_read_of_property_u32(dev, "qcom,cdc-mclk-clk-rate",
					   &prop_val)))
		pdata->mclk_rate = prop_val;

	if (pdata->mclk_rate != AQT1000_CLK_9P6MHZ &&
	    pdata->mclk_rate != AQT1000_CLK_12P288MHZ) {
		dev_err(dev, "%s: Invalid mclk_rate = %u\n", __func__,
			pdata->mclk_rate);
		goto err_parse_dt_prop;
	}
	if (pdata->ext_clk_rate % pdata->mclk_rate) {
		dev_err(dev,
			"%s: Invalid clock group, ext_clk = %d mclk = %d\n",
			__func__, pdata->ext_clk_rate, pdata->mclk_rate);
		goto err_parse_dt_prop;
	}

	pdata->irq_gpio = of_get_named_gpio(dev->of_node,
					    "qcom,gpio-connect", 0);
	if (!gpio_is_valid(pdata->irq_gpio)) {
		dev_err(dev, "%s: TLMM connect gpio not found\n", __func__);
		goto err_parse_dt_prop;
	}

	return pdata;

err_parse_dt_prop:
	devm_kfree(dev, pdata->regulator);
	pdata->regulator = NULL;
	pdata->num_supplies = 0;
err_power_sup:
	devm_kfree(dev, pdata);
	return NULL;
}

static int aqt1000_bringdown(struct device *dev)
{
	/* No sequence for teardown */

	return 0;
}

static void aqt1000_device_exit(struct aqt1000 *aqt)
{
	aqt_irq_exit(aqt);
	aqt1000_bringdown(aqt->dev);
	mutex_destroy(&aqt->io_lock);
	mutex_destroy(&aqt->xfer_lock);
	mutex_destroy(&aqt->cdc_bg_clk_lock);
	mutex_destroy(&aqt->master_bias_lock);
}

static int aqt1000_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct aqt1000 *aqt1000 = NULL;
	struct aqt1000_pdata *pdata = NULL;
	int ret = 0;

	pdata = aqt1000_populate_dt_data(&client->dev);
	if (!pdata) {
		dev_err(&client->dev,
			"%s: Fail to obtain pdata from device tree\n",
			__func__);
		ret = -EINVAL;
		goto fail;
	}
	client->dev.platform_data = pdata;

	aqt1000 = devm_kzalloc(&client->dev, sizeof(struct aqt1000),
			       GFP_KERNEL);
	if (!aqt1000) {
		ret = -ENOMEM;
		goto fail;
	}

	aqt1000->regmap = aqt1000_regmap_init(&client->dev,
			&aqt1000_regmap_config);
	if (IS_ERR(aqt1000->regmap)) {
		ret = PTR_ERR(aqt1000->regmap);
		dev_err(&client->dev,
			"%s: Failed to init register map: %d\n",
			__func__, ret);
		goto fail;
	}
	aqt1000->aqt_rst_np = pdata->aqt_rst_np;
	if (!aqt1000->aqt_rst_np) {
		dev_err(&client->dev, "%s: pinctrl not used for rst_n\n",
			__func__);
		ret = -EINVAL;
		goto fail;
	}

	if (i2c_check_functionality(client->adapter,
				    I2C_FUNC_I2C) == 0) {
		dev_dbg(&client->dev, "%s: can't talk I2C?\n", __func__);
		ret = -EIO;
		goto fail;
	}
	dev_set_drvdata(&client->dev, aqt1000);
	aqt1000->dev = &client->dev;
	aqt1000->dev_up = true;
	aqt1000->mclk_rate = pdata->mclk_rate;
	aqt1000->irq = client->irq;

	aqt1000->num_of_supplies = pdata->num_supplies;
	ret = msm_cdc_init_supplies(aqt1000->dev, &aqt1000->supplies,
				    pdata->regulator,
				    pdata->num_supplies);
	if (!aqt1000->supplies) {
		dev_err(aqt1000->dev, "%s: Cannot init aqt supplies\n",
			__func__);
		goto err_codec;
	}
	ret = msm_cdc_enable_static_supplies(aqt1000->dev,
					     aqt1000->supplies,
					     pdata->regulator,
					     pdata->num_supplies);
	if (ret) {
		dev_err(aqt1000->dev, "%s: aqt static supply enable failed!\n",
			__func__);
		goto err_codec;
	}
	/* 5 usec sleep is needed as per HW requirement */
	usleep_range(5, 10);

	ret = aqt1000_reset(aqt1000->dev);
	if (ret) {
		dev_err(aqt1000->dev, "%s: Codec reset failed\n", __func__);
		goto err_supplies;
	}

	aqt1000->i2c_dev.client = client;
	aqt1000->read_dev = aqt1000_i2c_read;
	aqt1000->write_dev = aqt1000_i2c_write;

	ret = aqt1000_device_init(aqt1000);
	if (ret) {
		pr_err("%s: error, initializing device failed (%d)\n",
		       __func__, ret);
		goto err_supplies;
	}

	pm_runtime_set_active(aqt1000->dev);
	pm_runtime_enable(aqt1000->dev);

	ret = aqt_register_codec(&client->dev);
	if (ret) {
		dev_err(aqt1000->dev, "%s: Codec registration failed\n",
			 __func__);
		goto err_cdc_register;
	}

	return ret;

err_cdc_register:
	pm_runtime_disable(aqt1000->dev);
	aqt1000_device_exit(aqt1000);
err_supplies:
	msm_cdc_release_supplies(aqt1000->dev, aqt1000->supplies,
				 pdata->regulator,
				 pdata->num_supplies);
	pdata->regulator = NULL;
	pdata->num_supplies = 0;
err_codec:
	devm_kfree(&client->dev, aqt1000);
	dev_set_drvdata(&client->dev, NULL);
fail:
	return ret;
}

static int aqt1000_i2c_remove(struct i2c_client *client)
{
	struct aqt1000 *aqt;
	struct aqt1000_pdata *pdata = client->dev.platform_data;

	aqt = dev_get_drvdata(&client->dev);

	pm_runtime_disable(aqt->dev);
	msm_cdc_release_supplies(aqt->dev, aqt->supplies,
				 pdata->regulator,
				 pdata->num_supplies);
	aqt1000_device_exit(aqt);
	dev_set_drvdata(&client->dev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int aqt1000_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s system resume\n", __func__);

	return 0;
}

static int aqt1000_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s system suspend\n", __func__);

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int aqt1000_i2c_resume(struct device *dev)
{
	pr_debug("%s system resume\n", __func__);
	return 0;
}

static int aqt1000_i2c_suspend(struct device *dev)
{
	pr_debug("%s system suspend\n", __func__);
	return 0;
}
#endif

static struct i2c_device_id aqt1000_id_table[] = {
	{"aqt1000-i2c", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, aqt1000_id_table);

static const struct dev_pm_ops aqt1000_i2c_pm_ops = {
	SET_RUNTIME_PM_OPS(aqt1000_runtime_suspend,
			   aqt1000_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(aqt1000_i2c_suspend,
				aqt1000_i2c_resume)
};

static const struct of_device_id aqt_match_table[] = {
	{.compatible = "qcom,aqt1000-i2c-codec"},
	{}
};
MODULE_DEVICE_TABLE(of, aqt_match_table);

static struct i2c_driver aqt1000_i2c_driver = {
	.driver                 = {
		.owner          =       THIS_MODULE,
		.name           =       "aqt1000-i2c-codec",
#ifdef CONFIG_PM_SLEEP
		.pm             =       &aqt1000_i2c_pm_ops,
#endif
		.of_match_table =       aqt_match_table,
	},
	.id_table               =       aqt1000_id_table,
	.probe                  =       aqt1000_i2c_probe,
	.remove                 =       aqt1000_i2c_remove,
};

static int __init aqt1000_init(void)
{
	return i2c_add_driver(&aqt1000_i2c_driver);
}
module_init(aqt1000_init);

static void __exit aqt1000_exit(void)
{
	i2c_del_driver(&aqt1000_i2c_driver);
}
module_exit(aqt1000_exit);

MODULE_DESCRIPTION("AQT1000 Codec driver");
MODULE_LICENSE("GPL v2");
