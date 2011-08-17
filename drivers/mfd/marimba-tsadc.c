/*
 * Marimba TSADC driver.
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/mfd/marimba.h>
#include <linux/mfd/marimba-tsadc.h>
#include <linux/pm.h>
#include <linux/slab.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

/* marimba configuration block: TS_CTL0 */
#define TS_CTL0			0xFF
#define TS_CTL0_RESET		BIT(0)
#define TS_CTL0_CLK_EN		BIT(1)
#define TS_CTL0_XO_EN		BIT(2)
#define TS_CTL0_EOC_EN		BIT(3)
#define TS_CTL0_PENIRQ_EN	BIT(4)

/* TSADC registers */
#define SSBI_PRESET		0x00
#define TSHK_DIG_CONFIG		0x4F
#define TSHK_INTF_CONFIG	0x50
#define TSHK_SETUP		0x51
	#define TSHK_SETUP_EN_ADC  BIT(0)
	#define TSHK_SETUP_EN_PIRQ BIT(7)
#define TSHK_PARAM		0x52
#define TSHK_DATA_RD		0x53
#define TSHK_STATUS		0x54
#define TSHK_SETUP2		0x55
#define TSHK_RSV1		0x56
	#define TSHK_RSV1_PRECHARGE_EN	BIT(0)
#define TSHK_COMMAND		0x57
#define TSHK_PARAM2		0x58
	#define TSHK_INPUT_CLK_MASK	0x3F
	#define TSHK_SAMPLE_PRD_MASK	0xC7
	#define TSHK_INPUT_CLK_SHIFT	0x6
	#define TSHK_SAMPLE_PRD_SHIFT	0x3
#define TSHK_PARAM3		0x59
	#define TSHK_PARAM3_MODE_MASK	0xFC
	#define TSHK_PARAM3_PRE_CHG_SHIFT (5)
	#define TSHK_PARAM3_STABIZ_SHIFT (2)
	#define TSHK_STABLE_TIME_MASK	0xE3
	#define TSHK_PRECHG_TIME_MASK	0x1F
#define TSHK_PARAM4		0x5A
#define TSHK_RSV2		0x5B
#define TSHK_RSV3		0x5C
#define TSHK_RSV4		0x5D
#define TSHK_RSV5		0x5E

struct marimba_tsadc_client {
	unsigned int is_ts;
	struct platform_device *pdev;
};

struct marimba_tsadc {
	struct marimba *marimba;
	struct device *dev;
	struct marimba_tsadc_platform_data *pdata;
	struct clk	*codec_ssbi;
	struct device *child_tssc;
	bool clk_enabled;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend		early_suspend;
#endif
};

static struct marimba_tsadc *tsadc_dev;

static int marimba_write_u8(struct marimba_tsadc *tsadc, u8 reg, u8 data)
{
	int rc;

	tsadc->marimba->mod_id = MARIMBA_SLAVE_ID_MARIMBA;
	rc = marimba_write(tsadc->marimba, reg, &data, 1);

	if (!rc)
		dev_warn(tsadc->dev, "Error writing marimba reg %X - ret %X\n",
				reg, data);
	return 0;
}

static int marimba_tsadc_write(struct marimba_tsadc *tsadc, u8 reg, u8 data)
{
	int rc;

	tsadc->marimba->mod_id = MARIMBA_ID_TSADC;

	rc = marimba_ssbi_write(tsadc->marimba, reg, &data, 1);
	if (!rc)
		dev_warn(tsadc->dev, "Error writing marimba reg %X - ret %X\n",
				reg, data);
	return rc;
}

static int marimba_tsadc_shutdown(struct marimba_tsadc *tsadc)
{
	u8 val;
	int rc;

	/* force reset */
	val = TS_CTL0_XO_EN | TS_CTL0_EOC_EN | TS_CTL0_PENIRQ_EN |
				TS_CTL0_CLK_EN;
	rc = marimba_write_u8(tsadc, TS_CTL0, val);
	if (rc < 0)
		return rc;

	/* disable xo, clock */
	val = TS_CTL0_PENIRQ_EN | TS_CTL0_EOC_EN;
	rc = marimba_write_u8(tsadc, TS_CTL0, val);
	if (rc < 0)
		return rc;

	/* de-vote S2 1.3v */
	if (tsadc->pdata->level_vote)
		/* REVISIT: Ignore error for level_vote(0) for now*/
		tsadc->pdata->level_vote(0);

	return 0;
}

static int marimba_tsadc_startup(struct marimba_tsadc *tsadc)
{
	u8 val;
	int rc = 0;

	/* vote for S2 1.3v */
	if (tsadc->pdata->level_vote) {
		rc = tsadc->pdata->level_vote(1);
		if (rc < 0)
			return rc;
	}

	/* disable XO, clock and output enables */
	rc = marimba_write_u8(tsadc, TS_CTL0, 0x00);
	if (rc < 0)
		goto fail_marimba_write;

	/* Enable output enables */
	val = TS_CTL0_XO_EN | TS_CTL0_EOC_EN | TS_CTL0_PENIRQ_EN;
	rc = marimba_write_u8(tsadc, TS_CTL0, val);
	if (rc < 0)
		goto fail_marimba_write;

	/* Enable clock */
	val = val | TS_CTL0_CLK_EN;
	rc = marimba_write_u8(tsadc, TS_CTL0, val);
	if (rc < 0)
		goto fail_marimba_write;

	/* remove reset */
	val = val | TS_CTL0_RESET;
	rc = marimba_write_u8(tsadc, TS_CTL0, val);
	if (rc < 0)
		goto fail_marimba_write;

	return 0;

fail_marimba_write:
	if (tsadc->pdata->level_vote)
		/* REVISIT: Ignore error for level_vote(0) for now*/
		tsadc->pdata->level_vote(0);
	return rc;
}


static int marimba_tsadc_configure(struct marimba_tsadc *tsadc)
{
	u8 rsv1 = 0,  setup = 0, i, count = 0;
	u8 param2 = 0,  param3 = 0;
	unsigned long val;
	int rc;

	rc = marimba_tsadc_write(tsadc, SSBI_PRESET, 0x00);
	if (rc < 0)
		return rc;

	if (!tsadc->pdata)
		return -EINVAL;

	/* Configure RSV1 register*/
	if (tsadc->pdata->tsadc_prechg_en == true)
		rsv1 |= TSHK_RSV1_PRECHARGE_EN;
	else
		rsv1 &= ~TSHK_RSV1_PRECHARGE_EN;

	/*  Set RSV1 register*/
	rc = marimba_tsadc_write(tsadc, TSHK_RSV1, rsv1);
	if (rc < 0)
		return rc;

	/* Configure PARAM2 register */
	/* Input clk */
	val = tsadc->pdata->params2.input_clk_khz;
	param2 &= TSHK_INPUT_CLK_MASK;
	val /= 600;
	if (val >= 1 && val <= 8 && !(val & (val - 1))) {
		/* Input clk can be .6, 1.2, 2.4, 4.8Mhz */
		if (val % 4 != 0)
			param2 = (4 - (val % 4)) << TSHK_INPUT_CLK_SHIFT;
		else
			param2 = ((val / 4) - 1) << TSHK_INPUT_CLK_SHIFT;
	} else /* Configure the default clk 2.4Mhz */
		param2 = 0x00 << TSHK_INPUT_CLK_SHIFT;

	/* Sample period */
	param2 &= TSHK_SAMPLE_PRD_MASK;
	param2 |=  tsadc->pdata->params2.sample_prd << TSHK_SAMPLE_PRD_SHIFT;

	/* Write PARAM2 register */
	rc = marimba_tsadc_write(tsadc, TSHK_PARAM2, param2);
	if (rc < 0)
		return rc;

	/* REVISIT: If Precharge time, stabilization time  > 409.6us */
	/* Configure PARAM3 register */
	val = tsadc->pdata->params3.prechg_time_nsecs;
	param3 &= TSHK_PRECHG_TIME_MASK;
	val /= 6400;
	if (val >= 1 && val <= 64  && !(val & (val - 1))) {
		count = 0;
		while ((val = val >> 1) != 0)
			count++;
		param3 |= count << TSHK_PARAM3_PRE_CHG_SHIFT;
	} else	/* Set default value if the input is wrong */
		param3 |= 0x00 << TSHK_PARAM3_PRE_CHG_SHIFT;

	val = tsadc->pdata->params3.stable_time_nsecs;
	param3 &= TSHK_STABLE_TIME_MASK;
	val /= 6400;
	if (val >= 1 && val <= 64 && !(val & (val - 1))) {
		count = 0;
		while ((val = val >> 1) != 0)
			count++;
		param3 |= count << TSHK_PARAM3_STABIZ_SHIFT;
	} else /* Set default value if the input is wrong */
		param3 |=  0x00 << TSHK_PARAM3_STABIZ_SHIFT;

	/* Get TSADC mode */
	val = tsadc->pdata->params3.tsadc_test_mode;
	param3 &= TSHK_PARAM3_MODE_MASK;
	if (val == 0)
		param3 |= 0x00;
	else
		for (i = 0; i < 3 ; i++) {
			if (((val + i) % 39322) == 0) {
				param3 |= (i + 1);
				break;
			}
		}
	if (i == 3) /* Set to normal mode if input is wrong */
		param3 |= 0x00;

	rc = marimba_tsadc_write(tsadc, TSHK_PARAM3, param3);
	if (rc < 0)
		return rc;

	/* Configure TSHK SETUP Register */
	if (tsadc->pdata->setup.pen_irq_en == true)
		setup |= TSHK_SETUP_EN_PIRQ;
	else
		setup &= ~TSHK_SETUP_EN_PIRQ;

	if (tsadc->pdata->setup.tsadc_en == true)
		setup |= TSHK_SETUP_EN_ADC;
	else
		setup &= ~TSHK_SETUP_EN_ADC;

	/* Enable signals to ADC, pen irq assertion */
	rc = marimba_tsadc_write(tsadc, TSHK_SETUP, setup);
	if (rc < 0)
		return rc;

	return 0;
}

int marimba_tsadc_start(struct marimba_tsadc_client *client)
{
	int rc = 0;

	if (!client) {
		pr_err("%s: Not a valid client\n", __func__);
		return -ENODEV;
	}

	if (!tsadc_dev) {
		dev_err(&client->pdev->dev,
			"%s: No tsadc device available\n", __func__);
		return -ENODEV;
	}

	/* REVISIT - add locks */
	if (client->is_ts) {
		rc = marimba_tsadc_startup(tsadc_dev);
		if (rc < 0)
			goto fail_tsadc_startup;
		rc = marimba_tsadc_configure(tsadc_dev);
		if (rc < 0)
			goto fail_tsadc_conf;
	}

	return 0;
fail_tsadc_conf:
	marimba_tsadc_shutdown(tsadc_dev);
fail_tsadc_startup:
	return rc;
}
EXPORT_SYMBOL(marimba_tsadc_start);

struct marimba_tsadc_client *
marimba_tsadc_register(struct platform_device *pdev, unsigned int is_ts)
{
	struct marimba_tsadc_client *client;

	if (!pdev) {
		pr_err("%s: valid platform device pointer please\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!is_ts) {
		dev_err(&pdev->dev, "%s: only TS right now\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!tsadc_dev) {
		dev_err(&pdev->dev,
			"%s: No tsadc device available\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	client = kzalloc(sizeof *client, GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->pdev = pdev;
	client->is_ts = is_ts;

	return client;
}
EXPORT_SYMBOL(marimba_tsadc_register);

void marimba_tsadc_unregister(struct marimba_tsadc_client *client)
{
	if (client->is_ts)
		marimba_tsadc_shutdown(tsadc_dev);
	kfree(client);
}
EXPORT_SYMBOL(marimba_tsadc_unregister);

static struct resource resources_tssc[] = {
	{
		.start	= 0xAD300000,
		.end	= 0xAD300000 + SZ_4K - 1,
		.name	= "tssc",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= 55,
		.end	= 55,
		.name	= "tssc1",
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_RISING,
	},
	{
		.start	= 56,
		.end	= 56,
		.name	= "tssc2",
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_RISING,
	},
};

static struct device *
marimba_add_tssc_subdev(struct device *parent, const char *name, int num,
			 struct resource *resources, int num_resources,
			 void *pdata, int pdata_len)
{
	struct platform_device	*pdev;
	int			status;

	pdev = platform_device_alloc(name, num);
	if (!pdev) {
		dev_dbg(parent, "can't alloc dev\n");
		status = -ENOMEM;
		goto err;
	}

	pdev->dev.parent = parent;

	if (pdata) {
		status = platform_device_add_data(pdev, pdata, pdata_len);
		if (status < 0) {
			dev_dbg(&pdev->dev, "can't add platform_data\n");
			goto err;
		}
	}

	status = platform_device_add_resources(pdev, resources, num_resources);
	if (status < 0) {
		dev_dbg(&pdev->dev, "can't add resources\n");
		goto err;
	}

	status = platform_device_add(pdev);

err:
	if (status < 0) {
		platform_device_put(pdev);
		dev_err(parent, "can't add %s dev\n", name);
		return ERR_PTR(status);
	}
	return &pdev->dev;
}

#ifdef CONFIG_PM
static int
marimba_tsadc_suspend(struct device *dev)
{
	int rc = 0, ret = 0;
	struct marimba_tsadc *tsadc = dev_get_drvdata(dev);

	if (tsadc->clk_enabled == true) {
		clk_disable(tsadc->codec_ssbi);
		tsadc->clk_enabled = false;
	}

	if (!(device_may_wakeup(dev) &&
			device_may_wakeup(tsadc->child_tssc))) {
		rc = marimba_tsadc_shutdown(tsadc);
		if (rc < 0) {
			pr_err("%s: Unable to shutdown TSADC\n", __func__);
			goto fail_shutdown;
		}

		if (tsadc->pdata->marimba_tsadc_power) {
			rc = tsadc->pdata->marimba_tsadc_power(0);
			if (rc < 0)
				goto fail_tsadc_power;
		}
	}
	return rc;

fail_tsadc_power:
	marimba_tsadc_startup(tsadc_dev);
	marimba_tsadc_configure(tsadc_dev);
fail_shutdown:
	if (tsadc->clk_enabled == false) {
		ret = clk_enable(tsadc->codec_ssbi);
		if (ret == 0)
			tsadc->clk_enabled = true;
	}
	return rc;
}

static int marimba_tsadc_resume(struct device *dev)
{
	int rc = 0;
	struct marimba_tsadc *tsadc = dev_get_drvdata(dev);

	if (tsadc->clk_enabled == false) {
		rc = clk_enable(tsadc->codec_ssbi);
		if (rc != 0) {
			pr_err("%s: Clk enable failed\n", __func__);
			return rc;
		}
		tsadc->clk_enabled = true;
	}

	if (!(device_may_wakeup(dev) &&
			device_may_wakeup(tsadc->child_tssc))) {
		if (tsadc->pdata->marimba_tsadc_power) {
			rc = tsadc->pdata->marimba_tsadc_power(1);
			if (rc) {
				pr_err("%s: Unable to power on TSADC \n",
						__func__);
				goto fail_tsadc_power;
			}
		}

		rc = marimba_tsadc_startup(tsadc_dev);
		if (rc < 0) {
			pr_err("%s: Unable to startup TSADC\n", __func__);
			goto fail_tsadc_startup;
		}

		rc = marimba_tsadc_configure(tsadc_dev);
		if (rc < 0) {
			pr_err("%s: Unable to configure TSADC\n", __func__);
			goto fail_tsadc_configure;
		}
	}
	return rc;

fail_tsadc_configure:
	marimba_tsadc_shutdown(tsadc_dev);
fail_tsadc_startup:
	if (tsadc->pdata->marimba_tsadc_power)
		tsadc->pdata->marimba_tsadc_power(0);
fail_tsadc_power:
	if (tsadc->clk_enabled == true) {
		clk_disable(tsadc->codec_ssbi);
		tsadc->clk_enabled = false;
	}
	return rc;
}

static struct dev_pm_ops tsadc_pm_ops = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = marimba_tsadc_suspend,
	.resume = marimba_tsadc_resume,
#endif
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void marimba_tsadc_early_suspend(struct early_suspend *h)
{
	struct marimba_tsadc *tsadc = container_of(h, struct marimba_tsadc,
						 early_suspend);

	marimba_tsadc_suspend(tsadc->dev);
}

static void marimba_tsadc_late_resume(struct early_suspend *h)
{
	struct marimba_tsadc *tsadc = container_of(h, struct marimba_tsadc,
						 early_suspend);

	marimba_tsadc_resume(tsadc->dev);
}
#endif

static int __devinit marimba_tsadc_probe(struct platform_device *pdev)
{
	struct marimba *marimba = platform_get_drvdata(pdev);
	struct marimba_tsadc *tsadc;
	struct marimba_tsadc_platform_data *pdata = pdev->dev.platform_data;
	int rc = 0;
	struct device *child;

	printk("%s\n", __func__);

	if (!pdata) {
		dev_dbg(&pdev->dev, "no tsadc platform data?\n");
		return -EINVAL;
	}

	tsadc = kzalloc(sizeof *tsadc, GFP_KERNEL);
	if (!tsadc)
		return -ENOMEM;

	tsadc->marimba	= marimba;
	tsadc->dev	= &pdev->dev;
	tsadc->pdata	= pdata;

	platform_set_drvdata(pdev, tsadc);

	if (tsadc->pdata->init) {
		rc = tsadc->pdata->init();
		if (rc < 0)
			goto fail_tsadc_init;
	}

	if (tsadc->pdata->marimba_tsadc_power) {
		rc = tsadc->pdata->marimba_tsadc_power(1);
		if (rc) {
			pr_err("%s: Unable to power up TSADC \n", __func__);
			goto fail_tsadc_power;
		}
	}

	tsadc->codec_ssbi = clk_get(NULL, "codec_ssbi_clk");
	if (IS_ERR(tsadc->codec_ssbi)) {
		rc = PTR_ERR(tsadc->codec_ssbi);
		goto fail_clk_get;
	}
	rc = clk_enable(tsadc->codec_ssbi);
	if (rc != 0)
		goto fail_clk_enable;

	tsadc->clk_enabled = true;

	child = marimba_add_tssc_subdev(&pdev->dev, "msm_touchscreen", -1,
			 resources_tssc, ARRAY_SIZE(resources_tssc),
			 pdata->tssc_data, sizeof(*pdata->tssc_data));

	if (IS_ERR(child)) {
		rc = PTR_ERR(child);
		goto fail_add_subdev;
	}

	tsadc->child_tssc = child;
	platform_set_drvdata(pdev, tsadc);

#ifdef CONFIG_HAS_EARLYSUSPEND
	tsadc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
						 TSADC_SUSPEND_LEVEL;
	tsadc->early_suspend.suspend = marimba_tsadc_early_suspend;
	tsadc->early_suspend.resume = marimba_tsadc_late_resume;
	register_early_suspend(&tsadc->early_suspend);
#endif

	tsadc_dev = tsadc;
	device_init_wakeup(&pdev->dev, pdata->can_wakeup);

	return rc;

fail_add_subdev:
	clk_disable(tsadc->codec_ssbi);

fail_clk_enable:
	clk_put(tsadc->codec_ssbi);

fail_clk_get:
	if (tsadc->pdata->marimba_tsadc_power)
		rc = tsadc->pdata->marimba_tsadc_power(0);
fail_tsadc_power:
	if (tsadc->pdata->exit)
		rc = tsadc->pdata->exit();
fail_tsadc_init:
	kfree(tsadc);
	return rc;
}

static int __devexit marimba_tsadc_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct marimba_tsadc *tsadc = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);

	if (tsadc->clk_enabled == true)
		clk_disable(tsadc->codec_ssbi);

	clk_put(tsadc->codec_ssbi);

	if (tsadc->pdata->exit)
		rc = tsadc->pdata->exit();

	if (tsadc->pdata->marimba_tsadc_power)
		rc = tsadc->pdata->marimba_tsadc_power(0);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&tsadc->early_suspend);
#endif

	platform_set_drvdata(pdev, NULL);
	kfree(tsadc);
	return rc;
}

static struct platform_driver tsadc_driver = {
	.probe	= marimba_tsadc_probe,
	.remove	= __devexit_p(marimba_tsadc_remove),
	.driver	= {
		.name = "marimba_tsadc",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &tsadc_pm_ops,
#endif
	},
};

static int __init marimba_tsadc_init(void)
{
	return platform_driver_register(&tsadc_driver);
}
device_initcall(marimba_tsadc_init);

static void __exit marimba_tsadc_exit(void)
{
	return platform_driver_unregister(&tsadc_driver);
}
module_exit(marimba_tsadc_exit);

MODULE_DESCRIPTION("Marimba TSADC driver");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:marimba_tsadc");
