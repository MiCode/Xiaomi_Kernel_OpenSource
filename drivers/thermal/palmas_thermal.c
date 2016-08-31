/*
 * palmas_thermal.c -- TI PALMAS THERMAL.
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * Author: Pradeep Goudagunta <pgoudagunta@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/thermal.h>
#include <linux/mfd/palmas.h>

#define PALMAS_NORMAL_OPERATING_TEMP 100000
#define PALMAS_CRITICAL_DEFUALT_TEMP 108000

struct palmas_therm_zone {
	struct device			*dev;
	struct palmas			*palmas;
	struct thermal_zone_device	*tz_device;
	int				irq;
	int				is_crit_temp;
};

struct palmas_trip_point {
	unsigned long temp;
	 enum thermal_trip_type type;
};

static struct palmas_trip_point palmas_tpoint = {
		.temp = PALMAS_CRITICAL_DEFUALT_TEMP,
		.type = THERMAL_TRIP_CRITICAL,
};

static int palmas_thermal_get_crit_temp(struct thermal_zone_device *tz_device,
			unsigned long *temp)
{
	*temp = palmas_tpoint.temp;

	return 0;
}

static int palmas_thermal_get_temp(struct thermal_zone_device *tz_device,
			unsigned long *temp)
{
	struct palmas_therm_zone *ptherm_zone = tz_device->devdata;

	if (ptherm_zone->is_crit_temp) {
		/*
		 * Set temperature greater than Critical trip
		 * temp to trigger orderly power down sequence
		 */
		palmas_thermal_get_crit_temp(tz_device, temp);
		*temp += 1;
		return 0;
	}

	*temp = PALMAS_NORMAL_OPERATING_TEMP;
	return 0;
}

static int palmas_thermal_get_trip_type(struct thermal_zone_device *tz_device,
			int trip, enum thermal_trip_type *type)
{
	if (trip >= 1)
		return -EINVAL;

	*type = palmas_tpoint.type;
	return 0;
}

static int palmas_thermal_get_trip_temp(struct thermal_zone_device *tz_device,
			int trip, unsigned long *temp)
{
	if (trip >= 1)
		return -EINVAL;

	*temp = palmas_tpoint.temp;
	return 0;
}

static struct thermal_zone_device_ops palmas_tz_ops = {
	.get_temp = palmas_thermal_get_temp,
	.get_crit_temp = palmas_thermal_get_crit_temp,
	.get_trip_type = palmas_thermal_get_trip_type,
	.get_trip_temp = palmas_thermal_get_trip_temp,
};

static irqreturn_t palmas_thermal_irq(int irq, void *data)
{
	struct palmas_therm_zone *ptherm_zone = data;
	unsigned int val;
	int ret;

	/*
	 * Necessary action can be taken here
	 * e.g: thermal_zone_device_update(pz->tz_device);
	 * will trigger orderly power down sequence.
	 */
	ret = palmas_read(ptherm_zone->palmas, PALMAS_INTERRUPT_BASE,
			PALMAS_INT1_LINE_STATE, &val);
	if (ret < 0) {
		dev_err(ptherm_zone->dev,
			"%s: Failed to read INT1_LINE_STATE, %d\n",
			__func__, ret);
	} else {
		ptherm_zone->is_crit_temp =
				val & PALMAS_INT1_STATUS_HOTDIE ? 1 : 0;
		dev_info(ptherm_zone->dev, "%s: HOTDIE line is equal to %d\n",
			__func__, ptherm_zone->is_crit_temp);
	}

	return IRQ_HANDLED;
}

static int palmas_thermal_probe(struct platform_device *pdev)
{
	struct palmas_therm_zone *ptherm_zone;
	struct palmas_platform_data *pdata;
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	char *default_tz_name = "palmas-junc-tz";
	int ret;
	u8 val;

	pdata = dev_get_platdata(pdev->dev.parent);

	if (!pdata || !(pdata->hd_threshold_temp)) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODEV;
	}

	ptherm_zone = devm_kzalloc(&pdev->dev, sizeof(*ptherm_zone),
			GFP_KERNEL);
	if (!ptherm_zone) {
		dev_err(&pdev->dev, "No available free memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ptherm_zone);
	ptherm_zone->dev = &pdev->dev;
	ptherm_zone->palmas = palmas;
	if (!(pdata->tz_name))
		pdata->tz_name = default_tz_name;

	ptherm_zone->tz_device = thermal_zone_device_register(pdata->tz_name,
					1, 0, ptherm_zone, &palmas_tz_ops,
					NULL, 0, 0);
	if (IS_ERR_OR_NULL(ptherm_zone->tz_device)) {
		dev_err(ptherm_zone->dev,
			"Register thermal zone device failed.\n");
		return PTR_ERR(ptherm_zone->tz_device);
	}

	palmas_tpoint.temp = pdata->hd_threshold_temp;

	ptherm_zone->irq = platform_get_irq(pdev, 0);
	ret = request_threaded_irq(ptherm_zone->irq, NULL,
			palmas_thermal_irq,
			IRQF_ONESHOT, dev_name(&pdev->dev),
			ptherm_zone);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"request irq %d failed: %dn", ptherm_zone->irq, ret);
		goto int_req_failed;
	}

	switch (palmas_tpoint.temp) {
	case 108000:
		val = 0;
		break;
	case 112000:
		val = 1;
		break;
	case 116000:
		val = 2;
		break;
	case 120000:
		val = 3;
		break;
	default:
		dev_err(&pdev->dev, "%ld threshold is not supported",
				palmas_tpoint.temp);
		ret = -EINVAL;
		goto error;
	}

	val <<= PALMAS_OSC_THERM_CTRL_THERM_HD_SEL_SHIFT;
	ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
			PALMAS_OSC_THERM_CTRL,
			PALMAS_OSC_THERM_CTRL_THERM_HD_SEL_MASK, val);
	if (ret < 0) {
		dev_err(&pdev->dev, "osc_therm_ctrl reg update failed.\n");
		goto error;
	}

	return 0;

error:
	free_irq(ptherm_zone->irq, ptherm_zone);
int_req_failed:
	thermal_zone_device_unregister(ptherm_zone->tz_device);
	return ret;
}

static int palmas_thermal_remove(struct platform_device *pdev)
{
	struct palmas_therm_zone *ptherm_zone = platform_get_drvdata(pdev);

	thermal_zone_device_unregister(ptherm_zone->tz_device);
	free_irq(ptherm_zone->irq, ptherm_zone);
	kfree(ptherm_zone);
	return 0;
}

static struct platform_driver palmas_thermal_driver = {
	.probe = palmas_thermal_probe,
	.remove = palmas_thermal_remove,
	.driver = {
		.name = "palmas-thermal",
		.owner = THIS_MODULE,
	},
};

static int __init palmas_thermal_init(void)
{
	return platform_driver_register(&palmas_thermal_driver);
}
module_init(palmas_thermal_init);

static void __exit palmas_thermal_exit(void)
{
	platform_driver_unregister(&palmas_thermal_driver);
}
module_exit(palmas_thermal_exit);

MODULE_DESCRIPTION("Palmas Thermal driver");
MODULE_AUTHOR("Pradeep Goudagunta<pgoudagunta@nvidia.com>");
MODULE_ALIAS("platform:palmas-thermal");
MODULE_LICENSE("GPL v2");
