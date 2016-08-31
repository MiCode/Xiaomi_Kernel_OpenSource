/*
 *  i2c-pca-gmi.c driver for PCA9564 on ISA boards
 *	Copyright (C) 2013 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * drivers/i2c/busses/i2c-pca-gmi.c
 *
 * I2C GMI Bus driver for for PCA9564 on ISA boards
*/

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-pca.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <linux/platform_data/tegra_nor.h>
#include <linux/gpio.h>
#include <linux/tegra_snor.h>

#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
#include <linux/tegra_gmi_access.h>
#endif

#include "../../../arch/arm/mach-tegra/iomap.h"

#define DRV_NAME "i2c-pca-gmi"

static int irq = -1;

/* Data sheet recommends 59kHz for 100kHz operation due to variation
 * in the actual clock rate */
static int clock  = 1265800;
static struct i2c_adapter pca_gmi_ops;
static wait_queue_head_t pca_wait;

struct tegra_gmi_pca_info {
	struct tegra_nor_chip_parms *plat;
	struct device *dev;
	void __iomem *base;
	u32 init_config;
	u32 timing0_default, timing1_default;
	u32 timing0_read, timing1_read;
	int gpio_num;
#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
	u32 gmiFmLockHandle;
#endif

};

static struct tegra_gmi_pca_info *info;

static void pca_gmi_request_access(void *pd)
{
#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
	request_gmi_access(info->gmiFmLockHandle);
#endif
}

static void pca_gmi_release_access(void *pd)
{
#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
	release_gmi_access();
#endif
}
static inline unsigned long snor_tegra_readl(struct tegra_gmi_pca_info *tsnor,
						unsigned long reg)
{
	return readl(tsnor->base + reg);
}

static inline void snor_tegra_writel(struct tegra_gmi_pca_info *tsnor,
					unsigned long val, unsigned long reg)
{
	writel(val, tsnor->base + reg);
}


static void pca_gmi_writebyte(void *pd, int reg, int val)
{
	struct tegra_nor_chip_parms *chip_parm = info->plat;
	struct cs_info *csinfo = &chip_parm->csinfo;
	unsigned int *ptr = csinfo->virt;
	struct gpio_state *state = &csinfo->gpio_cs;

	snor_tegra_writel(info, info->init_config, TEGRA_SNOR_CONFIG_REG);
	snor_tegra_writel(info, info->timing1_read, TEGRA_SNOR_TIMING1_REG);
	snor_tegra_writel(info, info->timing0_read, TEGRA_SNOR_TIMING0_REG);

	gpio_set_value(state[0].gpio_num, state[0].value);
	__raw_writeb(val, ptr + reg);
}
static int pca_gmi_readbyte(void *pd, int reg)
{
	int res;
	struct tegra_nor_chip_parms *chip_parm = info->plat;
	struct cs_info *csinfo = &chip_parm->csinfo;
	unsigned int *ptr = csinfo->virt;
	struct gpio_state *state = &csinfo->gpio_cs;

	snor_tegra_writel(info, info->init_config, TEGRA_SNOR_CONFIG_REG);
	snor_tegra_writel(info, info->timing1_read, TEGRA_SNOR_TIMING1_REG);
	snor_tegra_writel(info, info->timing0_read, TEGRA_SNOR_TIMING0_REG);
	gpio_set_value(state[0].gpio_num, state[0].value);

	res = __raw_readb(ptr + reg);
	return res;
}

static int pca_gmi_waitforcompletion(void *pd)
{
	unsigned long timeout;
	long ret;

	if (irq > -1) {
		ret = wait_event_timeout(pca_wait,
				pca_gmi_readbyte(pd, I2C_PCA_CON)
				& I2C_PCA_CON_SI, pca_gmi_ops.timeout);
	} else {
		/* Do polling */
		timeout = jiffies + pca_gmi_ops.timeout;
		do {
			ret = time_before(jiffies, timeout);
			if (pca_gmi_readbyte(pd, I2C_PCA_CON)
					& I2C_PCA_CON_SI)
				break;
			udelay(100);
		} while (ret);
	}
	return ret > 0;
}

static void pca_gmi_resetchip(void *pd)
{
/* apparently only an external reset will do it. not a lot can be done */
	printk(KERN_WARNING "DRIVER: Haven't figured out how" \
						"to do a reset yet\n");
}

static irqreturn_t pca_handler(int this_irq, void *dev_id)
{
	static int gpio_state;

	if (gpio_state == 0)
		gpio_state = 1;
	else
		gpio_state = 0;

	gpio_set_value(info->gpio_num, gpio_state);

	wake_up(&pca_wait);
	return IRQ_HANDLED;
}

static struct i2c_algo_pca_data pca_gmi_data = {
	/* .data intentionally left NULL, not needed with ISA */
	.write_byte		= pca_gmi_writebyte,
	.read_byte		= pca_gmi_readbyte,
	.wait_for_completion	= pca_gmi_waitforcompletion,
	.reset_chip		= pca_gmi_resetchip,
	.request_access		= pca_gmi_request_access,
	.release_access		= pca_gmi_release_access,
};

static struct i2c_adapter pca_gmi_ops = {
	.owner          = THIS_MODULE,
	.algo_data	= &pca_gmi_data,
	.name		= "PCA9564/PCA9665 GMI Adapter",
	.timeout	= HZ,
};


static int tegra_gmi_controller_init(struct tegra_gmi_pca_info *info)
{
	struct tegra_nor_chip_parms *chip_parm = info->plat;
	struct cs_info *csinfo = &chip_parm->csinfo;

	u32 width = chip_parm->BusWidth;
	u32 config = 0;

	config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
	config |= TEGRA_SNOR_CONFIG_SNOR_CS(csinfo->cs);
	config &= ~TEGRA_SNOR_CONFIG_DEVICE_TYPE;
	config |= TEGRA_SNOR_CONFIG_WP; /* Enable writes */

	switch (width) {
	case 2:
		config &= ~TEGRA_SNOR_CONFIG_WORDWIDE;  /* 16 bit */
		break;
	case 4:
		config |= TEGRA_SNOR_CONFIG_WORDWIDE;   /* 32 bit */
		break;
	default:
		return -EINVAL;
	}

	switch (chip_parm->MuxMode) {
	case NorMuxMode_ADNonMux:
		config &= ~TEGRA_SNOR_CONFIG_MUX_MODE;
	break;
	case NorMuxMode_ADMux:
		config |= TEGRA_SNOR_CONFIG_MUX_MODE;
		break;
	default:
		return -EINVAL;
	}

	info->init_config = config;

	info->timing0_default = chip_parm->timing_default.timing0;
	info->timing0_read = chip_parm->timing_read.timing0;
	info->timing1_default = chip_parm->timing_default.timing1;
	info->timing1_read = chip_parm->timing_read.timing1;
	return 0;
}


static int tegra_gmi_pca_probe(struct platform_device *pdev)
{
	int err = 0, err_gpio = 0;
	int err_int = 0;
	struct tegra_nor_chip_parms *plat = pdev->dev.platform_data;
	struct cs_info *csinfo = &plat->csinfo;
	struct gpio_state *state = &csinfo->gpio_cs;
	struct device *dev = &pdev->dev;
	struct resource *res;

	if (!plat) {
		pr_err("%s: no platform device info\n", __func__);
		err = -EINVAL;
		goto fail;
	}
	info = devm_kzalloc(dev, sizeof(struct tegra_gmi_pca_info),
				GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
	goto fail;
	}

	info->base = ((void __iomem *)IO_APB_VIRT +
			(TEGRA_SNOR_BASE - IO_APB_PHYS));
	info->plat = plat;
	info->dev = dev;

	/* Intialise the SNOR controller before probe */
	err = tegra_gmi_controller_init(info);
	if (err) {
		dev_err(dev, "Error initializing controller\n");
		goto fail;
	}
	platform_set_drvdata(pdev, info);

	init_waitqueue_head(&pca_wait);

	err_gpio = gpio_request(state[0].gpio_num, state[0].label);
	gpio_direction_output(state[0].gpio_num, 0);

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!res) {
		dev_err(dev, "GPIO %d can't be used as interrupt\n",
							res->start);
		return -ENODEV;
	}

	err_int = gpio_request(res->start, NULL);
	gpio_direction_output(res->start, 0);
	irq = gpio_to_irq(res->start);
	info->gpio_num = res->start;

	if (irq <= 0) {
		printk(KERN_ALERT "GPIO %d can't be used as interrupt"
							, res->start);
		goto fail;
	}

	if (irq > -1) {
		if (request_irq(irq, pca_handler, IRQF_TRIGGER_FALLING,
				"i2c-pca-gmi", &pca_gmi_ops) < 0) {
			dev_err(dev, "Request irq%d failed\n", irq);
			goto fail;
		}
	}

#ifdef CONFIG_TEGRA_GMI_ACCESS_CONTROL
	info->gmiFmLockHandle = register_gmi_device("gmi-i2c-pca", 0);
#endif

	pca_gmi_data.i2c_clock = clock;
	pca_gmi_request_access(NULL);

	if (i2c_pca_add_bus(&pca_gmi_ops) < 0) {
		pca_gmi_release_access(NULL);
		dev_err(dev, "Failed to add i2c bus\n");
		goto out_irq;
	}

	pca_gmi_release_access(NULL);

	return 0;

out_irq:
	if (irq > -1)
		free_irq(irq, &pca_gmi_ops);

fail:
	pr_err("Tegra GMI PCA probe failed\n");
	return err;
}

static struct platform_driver __refdata tegra_gmi_pca_driver = {
	.probe = tegra_gmi_pca_probe,
	.driver = {
	.name = DRV_NAME,
	.owner = THIS_MODULE,
	},
};

module_platform_driver(tegra_gmi_pca_driver);

MODULE_DESCRIPTION("ISA base PCA9564/PCA9665 driver");
MODULE_LICENSE("GPL");

MODULE_ALIAS("platform:" DRV_NAME);
