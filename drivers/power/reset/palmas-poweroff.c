/*
 * palmas-poweroff.c : Power off and reset for Palma device.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/palmas.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/power/reset/system-pmic.h>

struct palmas_pm {
	struct device *dev;
	struct palmas *palmas;
	struct system_pmic_dev *system_pmic_dev;
	int num_int_mask_regs;
	int int_mask_reg_add[PALMAS_MAX_INTERRUPT_MASK_REG];
	int int_status_reg_add[PALMAS_MAX_INTERRUPT_MASK_REG];
	int int_mask_val[PALMAS_MAX_INTERRUPT_MASK_REG];
	bool need_rtc_power_on;
	bool need_usb_event_power_on;
};

static void palmas_auto_power_on(struct palmas_pm *palmas_pm)
{
	struct palmas *palmas = palmas_pm->palmas;
	int ret;

	dev_info(palmas_pm->dev, "Resetting Palmas through RTC\n");

	ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
			PALMAS_SWOFF_COLDRST, PALMAS_SWOFF_COLDRST_SW_RST, 0x0);
	if (ret < 0) {
		dev_err(palmas_pm->dev, "SWOFF_COLDRST update failed: %d\n",
			ret);
		goto scrub;
	}

	ret = palmas_update_bits(palmas, PALMAS_INTERRUPT_BASE,
			PALMAS_INT2_MASK, PALMAS_INT2_MASK_RTC_TIMER, 0);
	if (ret < 0) {
		dev_err(palmas_pm->dev, "INT2_MASK update failed: %d\n", ret);
		goto scrub;
	}

	if (palmas_pm->need_usb_event_power_on) {
		ret = palmas_update_bits(palmas, PALMAS_INTERRUPT_BASE,
			PALMAS_INT3_MASK,
			PALMAS_INT3_MASK_VBUS | PALMAS_INT3_MASK_VBUS_OTG, 0);
		if (ret < 0) {
			dev_err(palmas_pm->dev,
				"INT3_MASK update failed: %d\n", ret);
			goto scrub;
		}
	}

	ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
			PALMAS_DEV_CTRL, PALMAS_DEV_CTRL_SW_RST,
			PALMAS_DEV_CTRL_SW_RST);
	if (ret < 0) {
		dev_err(palmas_pm->dev, "DEV_CTRL update failed: %d\n", ret);
		goto scrub;
	}

	while (1)
		dev_err(palmas_pm->dev, "Device should not be here\n");
scrub:
	return;
}

static void palmas_power_off(void *drv_data)
{
	struct palmas_pm *palmas_pm = drv_data;
	struct palmas *palmas = palmas_pm->palmas;
	unsigned int val;
	int i;
	int ret;

	palmas_allow_atomic_xfer(palmas);

	if (palmas_pm->need_rtc_power_on)
		palmas_auto_power_on(palmas_pm);

	for (i = 0; i < palmas_pm->num_int_mask_regs; ++i) {
		ret = palmas_write(palmas, PALMAS_INTERRUPT_BASE,
				palmas_pm->int_mask_reg_add[i],
				palmas_pm->int_mask_val[i]);
		if (ret < 0)
			dev_err(palmas_pm->dev,
				"register 0x%02x write failed: %d\n",
				palmas_pm->int_mask_reg_add[i], ret);

		ret = palmas_read(palmas, PALMAS_INTERRUPT_BASE,
					palmas_pm->int_status_reg_add[i], &val);
		if (ret < 0)
			dev_err(palmas_pm->dev,
				"register 0x%02x read failed: %d\n",
				palmas_pm->int_status_reg_add[i], ret);
	}

	if (palmas_pm->need_usb_event_power_on) {
		ret = palmas_update_bits(palmas, PALMAS_INTERRUPT_BASE,
			PALMAS_INT3_MASK,
			PALMAS_INT3_MASK_VBUS | PALMAS_INT3_MASK_VBUS_OTG, 0);
		if (ret < 0)
			dev_err(palmas_pm->dev,
				"INT3_MASK update failed: %d\n", ret);
	}

	/* Mask all COLD RST condition */
	palmas_write(palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_SWOFF_COLDRST, 0x0);

	dev_info(palmas_pm->dev, "Powering off the device\n");

	/* Power off the device */
	palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_DEV_CTRL, 1, 0);
}

static void palmas_power_reset(void *drv_data)
{
	struct palmas_pm *palmas_pm = drv_data;
	struct palmas *palmas = palmas_pm->palmas;
	unsigned int val;
	int i;
	int ret;

	palmas_allow_atomic_xfer(palmas);

	for (i = 0; i < palmas_pm->num_int_mask_regs; ++i) {
		ret = palmas_write(palmas, PALMAS_INTERRUPT_BASE,
				palmas_pm->int_mask_reg_add[i],
				palmas_pm->int_mask_val[i]);
		if (ret < 0)
			dev_err(palmas_pm->dev,
				"register 0x%02x write failed: %d\n",
				palmas_pm->int_mask_reg_add[i], ret);

		ret = palmas_read(palmas, PALMAS_INTERRUPT_BASE,
					palmas_pm->int_status_reg_add[i], &val);
		if (ret < 0)
			dev_err(palmas_pm->dev,
				"register 0x%02x read failed: %d\n",
				palmas_pm->int_status_reg_add[i], ret);
	}

	/* SW-WAR for ES Version 2.1, 2.0 and 1.0 */
	if (palmas_is_es_version_or_less(palmas, 2, 1)) {
		dev_info(palmas_pm->dev, "Resetting Palmas through RTC\n");
		ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_DEV_CTRL, PALMAS_DEV_CTRL_SW_RST, 0);
		if (ret < 0) {
			dev_err(palmas_pm->dev,
				"DEV_CTRL update failed: %d\n", ret);
			goto reset_direct;
		}

		ret = palmas_update_bits(palmas, PALMAS_RTC_BASE,
				PALMAS_RTC_INTERRUPTS_REG,
				PALMAS_RTC_INTERRUPTS_REG_IT_TIMER,
				PALMAS_RTC_INTERRUPTS_REG_IT_TIMER);
		if (ret < 0) {
			dev_err(palmas_pm->dev,
				"RTC_INTERRUPTS update failed: %d\n", ret);
			goto reset_direct;
		}

		ret = palmas_update_bits(palmas, PALMAS_RTC_BASE,
			PALMAS_RTC_CTRL_REG, PALMAS_RTC_CTRL_REG_STOP_RTC,
			PALMAS_RTC_CTRL_REG_STOP_RTC);
		if (ret < 0) {
			dev_err(palmas_pm->dev,
				"RTC_CTRL_REG update failed: %d\n", ret);
			goto reset_direct;
		}

		ret = palmas_update_bits(palmas, PALMAS_INTERRUPT_BASE,
			PALMAS_INT2_MASK, PALMAS_INT2_MASK_RTC_TIMER, 0);
		if (ret < 0) {
			dev_err(palmas_pm->dev,
				"INT2_MASK update failed: %d\n", ret);
			goto reset_direct;
		}

		ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_DEV_CTRL, PALMAS_DEV_CTRL_SW_RST,
				PALMAS_DEV_CTRL_SW_RST);
		if (ret < 0) {
			dev_err(palmas_pm->dev,
				"DEV_CTRL update failed: %d\n", ret);
			goto reset_direct;
		}
		return;
	}

reset_direct:
	dev_info(palmas_pm->dev, "Power reset the device\n");
	palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_DEV_CTRL, 0x2, 0x2);
}

static int palmas_configure_power_on(void *drv_data,
	enum system_pmic_power_on_event event, void *event_data)
{
	struct palmas_pm *palmas_pm = drv_data;

	switch (event) {
	case SYSTEM_PMIC_RTC_ALARM:
		dev_info(palmas_pm->dev, "Resetting Palmas through RTC\n");
		palmas_pm->need_rtc_power_on = true;
		break;

	case SYSTEM_PMIC_USB_VBUS_INSERTION:
		palmas_pm->need_usb_event_power_on = true;
		break;

	default:
		dev_err(palmas_pm->dev, "power on event does not support\n");
		break;
	}
	return 0;
}

static struct system_pmic_ops palmas_pm_ops = {
	.power_off = palmas_power_off,
	.power_reset = palmas_power_reset,
	.configure_power_on = palmas_configure_power_on,
};

static int palmas_pm_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_pm *palmas_pm = NULL;
	struct palmas_platform_data *palmas_pdata;
	struct palmas_pm_platform_data *pm_pdata = NULL;
	struct system_pmic_config config;
	struct device_node *node = pdev->dev.of_node;
	int i;

	palmas_pm = devm_kzalloc(&pdev->dev, sizeof(*palmas_pm),
			GFP_KERNEL);
	if (!palmas_pm) {
		dev_err(&pdev->dev, "Memory allocation failed.\n");
		return -ENOMEM;
	}

	palmas_pm->dev = &pdev->dev;
	palmas_pm->palmas = palmas;
	platform_set_drvdata(pdev, palmas_pm);

	palmas_pm->num_int_mask_regs = PALMAS_MAX_INTERRUPT_MASK_REG;
	if (palmas->id != TPS80036)
		palmas_pm->num_int_mask_regs = 4;

	palmas_pm->int_mask_reg_add[0] = PALMAS_INT1_MASK;
	palmas_pm->int_mask_reg_add[1] = PALMAS_INT2_MASK;
	palmas_pm->int_mask_reg_add[2] = PALMAS_INT3_MASK;
	palmas_pm->int_mask_reg_add[3] = PALMAS_INT4_MASK;
	palmas_pm->int_mask_reg_add[4] = PALMAS_INT5_MASK;
	palmas_pm->int_mask_reg_add[5] = PALMAS_INT6_MASK;
	palmas_pm->int_status_reg_add[0] = PALMAS_INT1_STATUS;
	palmas_pm->int_status_reg_add[1] = PALMAS_INT2_STATUS;
	palmas_pm->int_status_reg_add[2] = PALMAS_INT3_STATUS;
	palmas_pm->int_status_reg_add[3] = PALMAS_INT4_STATUS;
	palmas_pm->int_status_reg_add[4] = PALMAS_INT5_STATUS;
	palmas_pm->int_status_reg_add[5] = PALMAS_INT6_STATUS;
	for (i = 0; i < palmas_pm->num_int_mask_regs; ++i)
		palmas_pm->int_mask_val[i] = 0xFF;

	palmas_pdata = dev_get_platdata(pdev->dev.parent);
	if (palmas_pdata)
		pm_pdata = palmas_pdata->pm_pdata;

	if (pm_pdata) {
		config.allow_power_off = pm_pdata->use_power_off;
		config.allow_power_reset = pm_pdata->use_power_reset;
	} else {
		if (node) {
			config.allow_power_off = of_property_read_bool(node,
				"system-pmic-power-off");
			config.allow_power_reset = of_property_read_bool(node,
				"system-pmic-power-reset");
		} else {
			config.allow_power_off = true;
			config.allow_power_reset = false;
		}
	}

	palmas_pm->system_pmic_dev = system_pmic_register(&pdev->dev,
				&palmas_pm_ops, &config, palmas_pm);
	if (IS_ERR(palmas_pm->system_pmic_dev)) {
		int ret = PTR_ERR(palmas_pm->system_pmic_dev);
		dev_err(&pdev->dev, "System PMIC registartion failed: %d\n",
			ret);
		return ret;
	}
	return 0;
}

static int palmas_pm_remove(struct platform_device *pdev)
{
	struct palmas_pm *palmas_pm = platform_get_drvdata(pdev);

	system_pmic_unregister(palmas_pm->system_pmic_dev);
	return 0;
}

static struct of_device_id of_palmas_pm_match_tbl[] = {
	{ .compatible = "ti,palmas-pm", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_palmas_pm_match_tbl);

static struct platform_driver palmas_pm_driver = {
	.probe		= palmas_pm_probe,
	.remove		= palmas_pm_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "palmas-pm",
		.of_match_table = of_palmas_pm_match_tbl,
	},
};

module_platform_driver(palmas_pm_driver);
MODULE_ALIAS("platform:palmas-pm");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
