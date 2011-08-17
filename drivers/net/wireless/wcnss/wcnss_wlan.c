/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/parser.h>
#include <linux/wcnss_wlan.h>
#include <mach/peripheral-loader.h>
#include "wcnss_riva.h"

#define DEVICE "wcnss_wlan"
#define VERSION "1.01"
#define WCNSS_PIL_DEVICE "wcnss"
#define WCNSS_NV_NAME "wlan/prima/WCNSS_qcom_cfg.ini"

/* By default assume 48MHz XO is populated */
#define CONFIG_USE_48MHZ_XO_DEFAULT 1

static struct {
	struct platform_device *pdev;
	void		*pil;
	struct resource	*mmio_res;
	struct resource	*tx_irq_res;
	struct resource	*rx_irq_res;
	const struct dev_pm_ops *pm_ops;
	int             smd_channel_ready;
	struct wcnss_wlan_config wlan_config;
} *penv = NULL;

enum {
	nv_none = -1,
	nv_use_48mhz_xo,
	nv_end,
};

static const match_table_t nv_tokens = {
	{nv_use_48mhz_xo, "gUse48MHzXO=%d"},
	{nv_end, "END"},
	{nv_none, NULL}
};

static void wcnss_init_config(void)
{
	penv->wlan_config.use_48mhz_xo = CONFIG_USE_48MHZ_XO_DEFAULT;
}

static void wcnss_parse_nv(char *nvp)
{
	substring_t args[MAX_OPT_ARGS];
	char *cur;
	char *tok;
	int token;
	int intval;

	cur = nvp;
	while (cur != NULL) {
		if ('#' == *cur) {
			/* comment, consume remainder of line */
			tok = strsep(&cur, "\r\n");
			continue;
		}

		tok = strsep(&cur, " \t\r\n,");
		if (!*tok)
			continue;

		token = match_token(tok, nv_tokens, args);
		switch (token) {
		case nv_use_48mhz_xo:
			if (match_int(&args[0], &intval)) {
				dev_err(&penv->pdev->dev,
					"Invalid value for gUse48MHzXO: %s\n",
					args[0].from);
				continue;
			}
			if ((0 > intval) || (1 < intval)) {
				dev_err(&penv->pdev->dev,
					"Invalid value for gUse48MHzXO: %d\n",
					intval);
				continue;
			}
			penv->wlan_config.use_48mhz_xo = intval;
			dev_info(&penv->pdev->dev,
					"gUse48MHzXO set to %d\n", intval);
			break;
		case nv_end:
			/* end of options so we are done */
			return;
		default:
			/* silently ignore unknown settings */
			break;
		}
	}
}

static int __devinit
wcnss_wlan_ctrl_probe(struct platform_device *pdev)
{
	if (penv)
		penv->smd_channel_ready = 1;

	pr_info("%s: SMD ctrl channel up\n", __func__);

	return 0;
}

static int __devexit
wcnss_wlan_ctrl_remove(struct platform_device *pdev)
{
	if (penv)
		penv->smd_channel_ready = 0;

	pr_info("%s: SMD ctrl channel down\n", __func__);

	return 0;
}


static struct platform_driver wcnss_wlan_ctrl_driver = {
	.driver = {
		.name	= "WLAN_CTRL",
		.owner	= THIS_MODULE,
	},
	.probe	= wcnss_wlan_ctrl_probe,
	.remove	= __devexit_p(wcnss_wlan_ctrl_remove),
};

struct device *wcnss_wlan_get_device(void)
{
	if (penv && penv->pdev && penv->smd_channel_ready)
		return &penv->pdev->dev;
	return NULL;
}
EXPORT_SYMBOL(wcnss_wlan_get_device);

struct resource *wcnss_wlan_get_memory_map(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) && penv->smd_channel_ready)
		return penv->mmio_res;
	return NULL;
}
EXPORT_SYMBOL(wcnss_wlan_get_memory_map);

int wcnss_wlan_get_dxe_tx_irq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
				penv->tx_irq_res && penv->smd_channel_ready)
		return penv->tx_irq_res->start;
	return WCNSS_WLAN_IRQ_INVALID;
}
EXPORT_SYMBOL(wcnss_wlan_get_dxe_tx_irq);

int wcnss_wlan_get_dxe_rx_irq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
				penv->rx_irq_res && penv->smd_channel_ready)
		return penv->rx_irq_res->start;
	return WCNSS_WLAN_IRQ_INVALID;
}
EXPORT_SYMBOL(wcnss_wlan_get_dxe_rx_irq);

void wcnss_wlan_register_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops)
{
	if (penv && dev && (dev == &penv->pdev->dev) && pm_ops)
		penv->pm_ops = pm_ops;
}
EXPORT_SYMBOL(wcnss_wlan_register_pm_ops);

static int wcnss_wlan_suspend(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->suspend)
		return penv->pm_ops->suspend(dev);
	return 0;
}

static int wcnss_wlan_resume(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->resume)
		return penv->pm_ops->resume(dev);
	return 0;
}

static int __devinit
wcnss_wlan_probe(struct platform_device *pdev)
{
	const struct firmware *nv;
	char *nvp;
	int ret;

	/* verify we haven't been called more than once */
	if (penv) {
		dev_err(&pdev->dev, "cannot handle multiple devices.\n");
		return -ENODEV;
	}

	/* create an environment to track the device */
	penv = kzalloc(sizeof(*penv), GFP_KERNEL);
	if (!penv) {
		dev_err(&pdev->dev, "cannot allocate device memory.\n");
		return -ENOMEM;
	}
	penv->pdev = pdev;

	/* initialize the WCNSS default configuration */
	wcnss_init_config();

	/* update the WCNSS configuration from NV if present */
	ret = request_firmware(&nv, WCNSS_NV_NAME, &pdev->dev);
	if (!ret) {
		/* firmware is read-only so make a NUL-terminated copy */
		nvp = kmalloc(nv->size+1, GFP_KERNEL);
		if (nvp) {
			memcpy(nvp, nv->data, nv->size);
			nvp[nv->size] = '\0';
			wcnss_parse_nv(nvp);
			kfree(nvp);
		} else {
			dev_err(&pdev->dev, "cannot parse NV.\n");
		}
		release_firmware(nv);
	} else {
		dev_err(&pdev->dev, "cannot read NV.\n");
	}

	/* power up the WCNSS */
	ret = wcnss_wlan_power(&pdev->dev, &penv->wlan_config,
					WCNSS_WLAN_SWITCH_ON);
	if (ret) {
		dev_err(&pdev->dev, "WCNSS Power-up failed.\n");
		goto fail_power;
	}

	/* trigger initialization of the WCNSS */
	penv->pil = pil_get(WCNSS_PIL_DEVICE);
	if (IS_ERR(penv->pil)) {
		dev_err(&pdev->dev, "Peripheral Loader failed on WCNSS.\n");
		ret = PTR_ERR(penv->pil);
		penv->pil = NULL;
		goto fail_pil;
	}

	/* allocate resources */
	penv->mmio_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"wcnss_mmio");
	penv->tx_irq_res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"wcnss_wlantx_irq");
	penv->rx_irq_res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"wcnss_wlanrx_irq");

	if (!(penv->mmio_res && penv->tx_irq_res && penv->rx_irq_res)) {
		dev_err(&pdev->dev, "insufficient resources\n");
		ret = -ENOENT;
		goto fail_res;
	}

	return 0;

fail_res:
	if (penv->pil)
		pil_put(penv->pil);
fail_pil:
	wcnss_wlan_power(&pdev->dev, &penv->wlan_config,
				WCNSS_WLAN_SWITCH_OFF);
fail_power:
	kfree(penv);
	penv = NULL;
	return ret;
}

static int __devexit
wcnss_wlan_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct dev_pm_ops wcnss_wlan_pm_ops = {
	.suspend	= wcnss_wlan_suspend,
	.resume		= wcnss_wlan_resume,
};

static struct platform_driver wcnss_wlan_driver = {
	.driver = {
		.name	= DEVICE,
		.owner	= THIS_MODULE,
		.pm	= &wcnss_wlan_pm_ops,
	},
	.probe	= wcnss_wlan_probe,
	.remove	= __devexit_p(wcnss_wlan_remove),
};

static int __init wcnss_wlan_init(void)
{
	platform_driver_register(&wcnss_wlan_driver);
	platform_driver_register(&wcnss_wlan_ctrl_driver);

	return 0;
}

static void __exit wcnss_wlan_exit(void)
{
	if (penv) {
		if (penv->pil)
			pil_put(penv->pil);

		wcnss_wlan_power(&penv->pdev->dev, &penv->wlan_config,
					WCNSS_WLAN_SWITCH_OFF);

		kfree(penv);
		penv = NULL;
	}

	platform_driver_unregister(&wcnss_wlan_ctrl_driver);
	platform_driver_unregister(&wcnss_wlan_driver);
}

module_init(wcnss_wlan_init);
module_exit(wcnss_wlan_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION(DEVICE "Driver");
