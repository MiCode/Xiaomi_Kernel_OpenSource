/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/c2port.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_data/tegra_c2port_platform_data.h>
#include <asm/system.h>

struct tegra_c2port_device {
	struct c2port_device *p_c2dev;
	/* configurations */
	u8 GPIO_C2CK;
	u8 GPIO_C2D;
};

static struct tegra_c2port_device *g_c2port_device;

/* Indicating the status of C2 interface */
static bool g_gpio_required;

/* Enable gpios */
/*
 * We need Hall sensor's gpio usage to be diabled
 * for requesting it as C2D pin.
 * After finishing using C2 interface,  we will reset
 * it.
 */
static void enable_hall_sensor_gpio(int gpio_c2d)
{
	gpio_request(gpio_c2d, "Hall Effect Sensor");
	gpio_direction_input(gpio_c2d);
	enable_irq(gpio_to_irq(gpio_c2d));
}

static void disable_hall_sensor_gpio(int gpio_c2d)
{
	disable_irq(gpio_to_irq(gpio_c2d));
	gpio_free(gpio_c2d);
}

/* if GPIO_C2D is occupied, then need disable Hall Sensor */
static void acquire_gpio(void)
{
	int ret;
	struct tegra_c2port_device *pc2dev = g_c2port_device;
	struct device *dev = pc2dev->p_c2dev->dev;

	if (g_gpio_required) {
		dev_info(dev, "Kernel GPIO alread been reuqired\n");
		return;
	}
	/* C2CK request should not be failed */
	ret = gpio_request(pc2dev->GPIO_C2CK, "GPIO_C2CK");
	if (ret < 0) {
		dev_err(dev, "GPIO_C2CK request failed %d\n", ret);
		return;
	}
	gpio_direction_output(pc2dev->GPIO_C2CK, 1);
	ret = gpio_request(pc2dev->GPIO_C2D, "GPIO_C2D");
	if (ret < 0) {
		/* Free GPIO and IRQ occupied by Hall Sensor */
		disable_hall_sensor_gpio(pc2dev->GPIO_C2D);
		ret = gpio_request(pc2dev->GPIO_C2D, "GPIO_C2D");
	}
	if (ret < 0) {
		dev_err(dev,
			"GPIO_C2D reqest failed,need disable HallSensor!\n");
		return;
	}
	gpio_direction_input(pc2dev->GPIO_C2D);
	g_gpio_required = true;
}

static void release_gpio(void)
{
	struct tegra_c2port_device *pc2dev = g_c2port_device;
	if (g_gpio_required == false)
		return;
	gpio_free(pc2dev->GPIO_C2CK);
	gpio_free(pc2dev->GPIO_C2D);
	enable_hall_sensor_gpio(pc2dev->GPIO_C2D);
	g_gpio_required = false;
}

/* operations to manipulate C2 interfaces */
/* enable c2 port access, status = 1 means set them as output */
static void tegra_c2port_access(struct c2port_device *dev, int status)
{
	if (status) {
		/* enable */
		acquire_gpio();
		if (!g_gpio_required)
			dev_err(dev->dev, "GPIO can't be acquired!\n");
	} else {
		/* disable */
		release_gpio();
		if (g_gpio_required)
			dev_err(dev->dev, "GPIO can't be released\n");
	}
}

/* set c2d direction, 0 = output, 1 = input */
static void tegra_c2port_c2d_dir(struct c2port_device *dev, int direction)
{
	struct tegra_c2port_device *pc2dev = g_c2port_device;
	if (!g_gpio_required)
		dev_err(dev->dev, "GPIO hasn't be acquired!\n");
	if (!direction)
		gpio_direction_output(pc2dev->GPIO_C2D, 1);
	else
		gpio_direction_input(pc2dev->GPIO_C2D);
}

/* get c2d value */
static int tegra_c2port_c2d_get(struct c2port_device *dev)
{
	int ret = 0;
	struct tegra_c2port_device *pc2dev = g_c2port_device;
	if (!g_gpio_required)
		dev_err(dev->dev, "GPIO hasn't be acquired!\n");
	ret = gpio_get_value(pc2dev->GPIO_C2D);
	return ret;
}

/* set c2d value */
static void tegra_c2port_c2d_set(struct c2port_device *dev, int value)
{
	struct tegra_c2port_device *pc2dev = g_c2port_device;
	if (unlikely(!g_gpio_required))
		dev_err(dev->dev, "GPIO hasn't be acquired!\n");
	gpio_set_value(pc2dev->GPIO_C2D, value);
}

/* set c2ck value */
static void tegra_c2port_c2ck_set(struct c2port_device *dev, int value)
{
	struct tegra_c2port_device *pc2dev = g_c2port_device;
	/*
	 * We don't complain when gpio not required, because in core,
	 * c2ck_set is called before access function in c2_core
	 */
	if ((!g_gpio_required))
		return;
	gpio_set_value(pc2dev->GPIO_C2CK, value);
}

static struct c2port_ops tegra_c2port_ops = {
	.block_size = 512,
	.blocks_num = 30,
	.ram_size = 0x100,
	/* although sfr_start should be 0x80 */
	/* set it to 0x00 doesn't really matter */
	.sfr_size = 0x100,
	.xram_size = 0x800,
	.access = tegra_c2port_access,
	.c2d_dir = tegra_c2port_c2d_dir,
	.c2d_get = tegra_c2port_c2d_get,
	.c2d_set = tegra_c2port_c2d_set,
	.c2ck_set = tegra_c2port_c2ck_set,
};

/* mcu_debugger driver functions */
static int tegra_c2port_probe(struct platform_device *pdev)
{
	struct tegra_c2port_platform_data *pdata =
	    (struct tegra_c2port_platform_data *)pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data\n");
		return -ENOENT;
	}

	if (g_c2port_device) {
		dev_err(&pdev->dev,
			"tegra_c2port probe more than one device!\n");
		return -1;
	}

	g_c2port_device = devm_kzalloc(&pdev->dev,
		sizeof(struct tegra_c2port_device), GFP_KERNEL);

	if (!g_c2port_device) {
		dev_err(&pdev->dev,
			"tegra_c2port_device allocated error!\n");
		return -ENOMEM;
	}
	g_c2port_device->GPIO_C2CK = pdata->gpio_c2ck;
	g_c2port_device->GPIO_C2D = pdata->gpio_c2d;

	/* register the device with c2 port core */
	g_c2port_device->p_c2dev = c2port_device_register("tegra_mcu_c2port",
							  &tegra_c2port_ops,
							  NULL);

	if (!g_c2port_device->p_c2dev) {
		dev_err(&pdev->dev,
			"tegra_port c2 device register failed!\n");
		devm_kfree(&pdev->dev, g_c2port_device);
		g_c2port_device = NULL;
		return -EBUSY;
	}
	return 0;
}

static int tegra_c2port_remove(struct platform_device *pdev)
{
	/* remove c2 port device */
	if (g_c2port_device) {
		c2port_device_unregister(g_c2port_device->p_c2dev);
		devm_kfree(&pdev->dev, g_c2port_device);
	}
	g_c2port_device = NULL;
	return 0;
}

static struct of_device_id tegra_c2port_of_match[] = {
	{.compatible = "tegra_c2port",},
	{},
};

static struct platform_driver tegra_c2port_driver = {
	.probe = tegra_c2port_probe,
	.remove = tegra_c2port_remove,
	.driver = {
		   .name = "tegra_c2port",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(tegra_c2port_of_match),
		}
};

module_platform_driver(tegra_c2port_driver);

MODULE_AUTHOR("Will Wu <willw@nvidia.com>");
MODULE_DESCRIPTION("C2 port driver for Loki");
MODULE_LICENSE("GPLv2");
