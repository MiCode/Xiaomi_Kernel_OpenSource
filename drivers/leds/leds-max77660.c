/*
 * leds-max77660.c -- MAXIM MAX77660 led driver.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/mfd/max77660/max77660-core.h>

struct max77660_leds {
	struct device		*dev;
	struct device		*parent;
};

static int max77660_disable_leds(struct max77660_leds *leds)
{
	int ret;

	/* Disable LED driver by default */
	ret = max77660_reg_write(leds->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LEDEN, 0x80);
	if (ret < 0) {
		dev_err(leds->dev, "LED write failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_write(leds->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LEDBLNK, 0x0);
	if (ret < 0) {
		dev_err(leds->dev, "LEDBLNK write failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int max77660_leds_probe(struct platform_device *pdev)
{
	struct max77660_leds *leds;
	struct max77660_platform_data *pdata;
	int ret = 0;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata) {
		dev_err(&pdev->dev, "No Platform data\n");
		return -EINVAL;
	}

	leds = devm_kzalloc(&pdev->dev, sizeof(*leds), GFP_KERNEL);
	if (!leds) {
		dev_err(&pdev->dev, "Memory allocation failed for leds\n");
		return -ENOMEM;
	}

	leds->dev = &pdev->dev;
	leds->parent = pdev->dev.parent;
	dev_set_drvdata(&pdev->dev, leds);

	if (pdata->led_disable)
		ret = max77660_disable_leds(leds);
	return ret;
}

static struct platform_driver max77660_leds_driver = {
	.probe = max77660_leds_probe,
	.driver = {
		.name = "max77660-leds",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(max77660_leds_driver);

MODULE_DESCRIPTION("max77660 LEDs driver");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:max77660-leds");
MODULE_LICENSE("GPL v2");
