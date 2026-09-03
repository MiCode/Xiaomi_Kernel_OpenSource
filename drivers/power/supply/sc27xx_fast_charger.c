// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/extcon-provider.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define FCHG1_TIME1				0x0
#define FCHG1_TIME2				0x4
#define FCHG1_DELAY				0x8
#define FCHG2_DET_HIGH				0xc
#define FCHG2_DET_LOW				0x10
#define FCHG2_DET_LOW_CV			0x14
#define FCHG2_DET_HIGH_CV			0x18
#define FCHG2_DET_LOW_CC			0x1c
#define FCHG2_ADJ_TIME1				0x20
#define FCHG2_ADJ_TIME2				0x24
#define FCHG2_ADJ_TIME3				0x28
#define FCHG2_ADJ_TIME4				0x2c
#define FCHG_CTRL				0x30
#define FCHG_ADJ_CTRL				0x34
#define FCHG_INT_EN				0x38
#define FCHG_INT_CLR				0x3c
#define FCHG_INT_STS				0x40
#define FCHG_INT_STS0				0x44
#define FCHG_ERR_STS				0x48

#define SC2721_MODULE_EN0			0xC08
#define SC2721_CLK_EN0				0xC10
#define SC2721_IB_CTRL				0xEA4
#define SC2730_MODULE_EN0			0x1808
#define SC2730_CLK_EN0				0x1810
#define SC2730_IB_CTRL				0x1b84
#define UMP9620_MODULE_EN0			0x2008
#define UMP9620_CLK_EN0				0x2010
#define UMP9620_IB_CTRL				0x2384

#define ANA_REG_IB_TRIM_MASK			GENMASK(6, 0)
#define ANA_REG_IB_TRIM_SHIFT			2
#define ANA_REG_IB_TRIM_MAX			0x7f
#define ANA_REG_IB_TRIM_EM_SEL_BIT		BIT(1)

#define FAST_CHARGE_MODULE_EN0_BIT		BIT(11)
#define FAST_CHARGE_RTC_CLK_EN0_BIT		BIT(4)

#define FCHG_ENABLE_BIT				BIT(0)
#define FCHG_INT_EN_BIT				BIT(1)
#define FCHG_INT_CLR_MASK			BIT(1)
#define FCHG_TIME1_MASK				GENMASK(10, 0)
#define FCHG_TIME2_MASK				GENMASK(11, 0)
#define FCHG_DET_VOL_MASK			GENMASK(1, 0)
#define FCHG_DET_VOL_SHIFT			3
#define FCHG_DET_VOL_EXIT_SFCP			3
#define FCHG_CALI_MASK				GENMASK(15, 9)
#define FCHG_CALI_SHIFT				9

#define FCHG_ERR0_BIT				BIT(1)
#define FCHG_ERR1_BIT				BIT(2)
#define FCHG_ERR2_BIT				BIT(3)
#define FCHG_OUT_OK_BIT				BIT(0)

#define FCHG_INT_STS_DETDONE			BIT(5)

/* FCHG1_TIME1_VALUE is used for detect the time of V > VT1 */
#define FCHG1_TIME1_VALUE			0x514
/* FCHG1_TIME2_VALUE is used for detect the time of V > VT2 */
#define FCHG1_TIME2_VALUE			0x9c4

#define FCHG_VOLTAGE_5V				5000000
#define FCHG_VOLTAGE_9V				9000000
#define FCHG_VOLTAGE_12V			12000000
#define FCHG_VOLTAGE_20V			20000000

#define FCHG_CURRENT_2A				2000000

#define SC27XX_FCHG_TIMEOUT			msecs_to_jiffies(5000)

struct sc27xx_fast_chg_data {
	u32 module_en;
	u32 clk_en;
	u32 ib_ctrl;
};

static const struct sc27xx_fast_chg_data sc2721_info = {
	.module_en = SC2721_MODULE_EN0,
	.clk_en = SC2721_CLK_EN0,
	.ib_ctrl = SC2721_IB_CTRL,
};

static const struct sc27xx_fast_chg_data sc2730_info = {
	.module_en = SC2730_MODULE_EN0,
	.clk_en = SC2730_CLK_EN0,
	.ib_ctrl = SC2730_IB_CTRL,
};

static const struct sc27xx_fast_chg_data ump9620_info = {
	.module_en = UMP9620_MODULE_EN0,
	.clk_en = UMP9620_CLK_EN0,
	.ib_ctrl = UMP9620_IB_CTRL,
};

struct sc27xx_fchg_info {
	struct device *dev;
	struct regmap *regmap;
	struct power_supply *psy_usb;
	struct delayed_work work;
	struct mutex lock;
	struct mutex sfcp_handshake_lock;
	struct completion completion;
	u32 state;
	u32 base;
	int input_vol;
	int ext_dev_comp_cur_code;
	u32 charger_online;
	bool detected;
	bool shutdown_flag;
	const struct sc27xx_fast_chg_data *pdata;
};

static int sc27xx_fchg_efuse_read(struct sc27xx_fchg_info *info,
				  const char *cell_id, u32 *val)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(info->dev, cell_id);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(val, buf, min(len, sizeof(u32)));
	kfree(buf);
	buf = NULL;

	return 0;
}

static int sc27xx_fchg_internal_cur_calibration(struct sc27xx_fchg_info *info)
{
	u32 calib_data = 0, calib_current = 0;
	int ret = 0;
	const struct sc27xx_fast_chg_data *pdata = info->pdata;

	ret = sc27xx_fchg_efuse_read(info, "fchg_cur_calib", &calib_data);
	if (ret) {
		dev_err(info->dev, "%s, failed to get efuse data, ret = %d\n", __func__, ret);
		return ret;
	}

	/*
	 * In the handshake protocol behavior of sfcp, the current source
	 * of the fast charge internal module is small, we improve it
	 * by set the register ANA_REG_IB_CTRL. Now we add 30 level compensation.
	 */
	calib_current = (calib_data & FCHG_CALI_MASK) >> FCHG_CALI_SHIFT;
	calib_current += info->ext_dev_comp_cur_code;

	if (calib_current > ANA_REG_IB_TRIM_MAX) {
		dev_info(info->dev, "The compensated calib_current exceeds the range of IB_TRIM,"
			 " calib_current=%d\n", calib_current);
		calib_current = (calib_data & FCHG_CALI_MASK) >> FCHG_CALI_SHIFT;
	}

	ret = regmap_update_bits(info->regmap,
				 pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_MASK << ANA_REG_IB_TRIM_SHIFT,
				 (calib_current & ANA_REG_IB_TRIM_MASK) << ANA_REG_IB_TRIM_SHIFT);
	if (ret) {
		dev_err(info->dev, "failed to calibrate fast charger current.\n");
		return ret;
	}

	/*
	 * Fast charge dm current source calibration mode, enable soft calibration mode.
	 */
	ret = regmap_update_bits(info->regmap, pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_EM_SEL_BIT,
				 0);
	if (ret) {
		dev_err(info->dev, "failed to select ib trim mode.\n");
		return ret;
	}

	return 0;
}

static irqreturn_t sc27xx_fchg_interrupt(int irq, void *dev_id)
{
	struct sc27xx_fchg_info *info = dev_id;
	u32 int_sts, int_sts0;
	int ret;

	ret = regmap_read(info->regmap, info->base + FCHG_INT_STS, &int_sts);
	if (ret)
		return IRQ_RETVAL(ret);

	ret = regmap_read(info->regmap, info->base + FCHG_INT_STS0, &int_sts0);
	if (ret)
		return IRQ_RETVAL(ret);

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_EN,
				 FCHG_INT_EN_BIT, 0);
	if (ret) {
		dev_err(info->dev, "failed to disable fast charger irq.\n");
		return IRQ_RETVAL(ret);
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_CLR,
				 FCHG_INT_CLR_MASK, FCHG_INT_CLR_MASK);
	if (ret) {
		dev_err(info->dev, "failed to clear fast charger interrupts\n");
		return IRQ_RETVAL(ret);
	}

	if ((int_sts & FCHG_INT_STS_DETDONE) && !(int_sts0 & FCHG_OUT_OK_BIT))
		dev_warn(info->dev,
			 "met some errors, now status = 0x%x, status0 = 0x%x\n",
			 int_sts, int_sts0);

	if ((int_sts & FCHG_INT_STS_DETDONE) && (int_sts0 & FCHG_OUT_OK_BIT)) {
		info->state = POWER_SUPPLY_CHARGE_TYPE_FAST;
		dev_info(info->dev, "setting sfcp 1.0 to fast type\n");
	} else {
		info->state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	complete(&info->completion);

	return IRQ_HANDLED;
}

static int sc27xx_fchg_get_detect_status(struct sc27xx_fchg_info *info)
{
	unsigned long timeout;
	int value, ret;
	const struct sc27xx_fast_chg_data *pdata = info->pdata;

	if (info->shutdown_flag)
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	reinit_completion(&info->completion);

	if (info->input_vol < FCHG_VOLTAGE_9V)
		value = 0;
	else if (info->input_vol < FCHG_VOLTAGE_12V)
		value = 1;
	else if (info->input_vol < FCHG_VOLTAGE_20V)
		value = 2;
	else
		value = 3;

	/*
	 * Due to the current source of the fast charge internal module is small
	 * we need to dynamically calibrate it through the software during the process
	 * of identifying fast charge. After fast charge recognition is completed, we
	 * disable soft calibration compensate function, in order to prevent the dm current
	 * source from deviating in accuracy when used in other modules.
	 */
	ret = sc27xx_fchg_internal_cur_calibration(info);
	if (ret) {
		dev_err(info->dev, "failed to set fast charger calibration.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, pdata->module_en,
				 FAST_CHARGE_MODULE_EN0_BIT,
				 FAST_CHARGE_MODULE_EN0_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, pdata->clk_en,
				 FAST_CHARGE_RTC_CLK_EN0_BIT,
				 FAST_CHARGE_RTC_CLK_EN0_BIT);
	if (ret) {
		dev_err(info->dev,
			"failed to enable fast charger clock.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG1_TIME1,
				 FCHG_TIME1_MASK, FCHG1_TIME1_VALUE);
	if (ret) {
		dev_err(info->dev, "failed to set fast charge time1\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG1_TIME2,
				 FCHG_TIME2_MASK, FCHG1_TIME2_VALUE);
	if (ret) {
		dev_err(info->dev, "failed to set fast charge time2\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
			FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
			(value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret) {
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_ENABLE_BIT, FCHG_ENABLE_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_EN,
				 FCHG_INT_EN_BIT, FCHG_INT_EN_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger irq.\n");
		return ret;
	}

	timeout = wait_for_completion_timeout(&info->completion, SC27XX_FCHG_TIMEOUT);
	if (!timeout) {
		dev_err(info->dev, "timeout to get fast charger status\n");
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	/*
	 * Fast charge dm current source calibration mode, select efuse calibration
	 * as default.
	 */
	ret = regmap_update_bits(info->regmap, pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_EM_SEL_BIT,
				 ANA_REG_IB_TRIM_EM_SEL_BIT);
	if (ret) {
		dev_err(info->dev, "failed to select ib trim mode.\n");
		return ret;
	}

	return info->state;
}

static void sc27xx_fchg_disable(struct sc27xx_fchg_info *info)
{
	const struct sc27xx_fast_chg_data *pdata = info->pdata;
	int ret;

	/*
	 * must exit SFCP mode, otherwise the next BC1.2
	 * recognition will be affected.
	 */
	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
				 (FCHG_DET_VOL_EXIT_SFCP & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret)
		dev_err(info->dev, "failed to set fast charger detect voltage.\n");

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_ENABLE_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable fast charger.\n");

	/*
	 * Adding delay is to make sure writing the control register
	 * successfully firstly, then disable the module and clock.
	 */
	msleep(100);

	ret = regmap_update_bits(info->regmap, pdata->module_en,
				 FAST_CHARGE_MODULE_EN0_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable fast charger module.\n");

	ret = regmap_update_bits(info->regmap, pdata->clk_en,
				 FAST_CHARGE_RTC_CLK_EN0_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable charger clock.\n");
}

static int sc27xx_fchg_sfcp_adjust_voltage(struct sc27xx_fchg_info *info, u32 input_vol)
{
	int ret, value;

	if (input_vol < FCHG_VOLTAGE_9V)
		value = 0;
	else if (input_vol < FCHG_VOLTAGE_12V)
		value = 1;
	else if (input_vol < FCHG_VOLTAGE_20V)
		value = 2;
	else
		value = 3;

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
				 (value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret) {
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
		return ret;
	}

	return 0;
}

static int sc27xx_fchg_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sc27xx_fchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = info->state;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = FCHG_VOLTAGE_9V;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = FCHG_CURRENT_2A;
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc27xx_fchg_usb_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct sc27xx_fchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == true) {
			info->charger_online = 1;
			schedule_delayed_work(&info->work, 0);
			break;
		} else if (val->intval == false) {
			info->charger_online = 0;
			complete(&info->completion);
			cancel_delayed_work(&info->work);
			schedule_delayed_work(&info->work, 0);
			break;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = sc27xx_fchg_sfcp_adjust_voltage(info, val->intval);
		if (ret)
			dev_err(info->dev, "failed to adjust sfcp vol\n");
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:

		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc27xx_fchg_property_is_writeable(struct power_supply *psy,
					     enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sc27xx_fchg_usb_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static const struct power_supply_desc sc27xx_fchg_desc = {
	.name			= "sc27xx_fast_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sc27xx_fchg_usb_props,
	.num_properties		= ARRAY_SIZE(sc27xx_fchg_usb_props),
	.get_property		= sc27xx_fchg_usb_get_property,
	.set_property		= sc27xx_fchg_usb_set_property,
	.property_is_writeable	= sc27xx_fchg_property_is_writeable,
};

static void sc27xx_fchg_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sc27xx_fchg_info *info = container_of(dwork, struct sc27xx_fchg_info, work);

	mutex_lock(&info->sfcp_handshake_lock);
	if (!info->charger_online) {
		info->state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		info->detected = false;
		sc27xx_fchg_disable(info);
	} else if (!info->detected && !info->shutdown_flag) {
		info->detected = true;

		if (sc27xx_fchg_get_detect_status(info) == POWER_SUPPLY_CHARGE_TYPE_FAST) {
			/*
			 * Must release info->sfcp_handshake_lock before send fast charge event
			 * to charger manager, otherwise it will cause deadlock.
			 */
			mutex_unlock(&info->sfcp_handshake_lock);
			power_supply_changed(info->psy_usb);
			dev_info(info->dev, "sfcp_enable\n");
			return;
		}

		sc27xx_fchg_disable(info);
	}

	mutex_unlock(&info->sfcp_handshake_lock);
}

static int sc27xx_fchg_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sc27xx_fchg_info *info;
	struct power_supply_config charger_cfg = { };
	int irq, ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	mutex_init(&info->sfcp_handshake_lock);
	info->dev = &pdev->dev;
	info->state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	info->pdata = of_device_get_match_data(info->dev);
	if (!info->pdata) {
		dev_err(info->dev, "no matching driver data found\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&info->work, sc27xx_fchg_work);
	init_completion(&info->completion);

	info->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!info->regmap) {
		dev_err(&pdev->dev, "failed to get charger regmap\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "reg", &info->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get register address\n");
		return -ENODEV;
	}

	ret = device_property_read_u32(&pdev->dev,
				       "sprd,input-voltage-microvolt",
				       &info->input_vol);
	if (ret) {
		dev_err(&pdev->dev, "failed to get fast charger voltage.\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev,
				       "sprd,ext-dev-comp-cur-code",
				       &info->ext_dev_comp_cur_code);
	if (ret) {
		dev_info(&pdev->dev, "Failed to get ext device compensation current code, ret=%d\n",
			 ret);
		/* If this parameter is not defined in DTS, the default value is 0 */
		info->ext_dev_comp_cur_code = 0;
	}

	dev_info(&pdev->dev, "%s, ext_dev_comp_cur_code = %d\n",
		 __func__, info->ext_dev_comp_cur_code);

	platform_set_drvdata(pdev, info);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource specified\n");
		return irq;
	}
	ret = devm_request_threaded_irq(info->dev, irq, NULL,
					sc27xx_fchg_interrupt,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq.\n");
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = np;

	info->psy_usb = devm_power_supply_register(&pdev->dev,
						   &sc27xx_fchg_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(&pdev->dev, "failed to register power supply\n");
		return PTR_ERR(info->psy_usb);
	}

	return 0;
}

static int sc27xx_fchg_remove(struct platform_device *pdev)
{
	struct sc27xx_fchg_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&info->work);
	return 0;
}

static void sc27xx_fchg_shutdown(struct platform_device *pdev)
{
	struct sc27xx_fchg_info *info = platform_get_drvdata(pdev);
	int ret;
	u32 value = FCHG_DET_VOL_EXIT_SFCP;
	const struct sc27xx_fast_chg_data *pdata = info->pdata;

	info->shutdown_flag = true;
	cancel_delayed_work_sync(&info->work);

	/*
	 * SFCP will handsharke failed from charging in shut down
	 * to charging in power up, because SFCP is not exit before
	 * shut down. Set bit3:4 to 2b'11 to exit SFCP.
	 */

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
				 (value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret)
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");

	/*
	 * Fast charge dm current source calibration mode, select efuse calibration
	 * as default.
	 */
	ret = regmap_update_bits(info->regmap, pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_EM_SEL_BIT,
				 ANA_REG_IB_TRIM_EM_SEL_BIT);
	if (ret)
		dev_err(info->dev, "%s, failed to select ib trim mode.\n", __func__);
}

static const struct of_device_id sc27xx_fchg_of_match[] = {
	{ .compatible = "sprd,sc2730-fast-charger", .data = &sc2730_info },
	{ .compatible = "sprd,ump9620-fast-chg", .data = &ump9620_info },
	{ .compatible = "sprd,sc2721-fast-charger", .data = &sc2721_info },
	{ }
};

static struct platform_driver sc27xx_fchg_driver = {
	.driver = {
		.name = "sc27xx-fast-charger",
		.of_match_table = sc27xx_fchg_of_match,
	},
	.probe = sc27xx_fchg_probe,
	.remove = sc27xx_fchg_remove,
	.shutdown = sc27xx_fchg_shutdown,
};

module_platform_driver(sc27xx_fchg_driver);

MODULE_DESCRIPTION("Spreadtrum SC27XX Fast Charger Driver");
MODULE_LICENSE("GPL v2");
