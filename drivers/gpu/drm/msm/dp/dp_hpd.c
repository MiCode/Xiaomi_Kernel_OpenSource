/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>

#include "dp_hpd.h"
#include "dp_usbpd.h"
#include "dp_gpio_hpd.h"
#include "dp_lphw_hpd.h"

static void dp_hpd_host_init(struct dp_hpd *dp_hpd,
		struct dp_catalog_hpd *catalog)
{
	if (!catalog) {
		pr_err("invalid input");
		return;
	}
	catalog->config_hpd(catalog, true);
}

static void dp_hpd_host_deinit(struct dp_hpd *dp_hpd,
		struct dp_catalog_hpd *catalog)
{
	if (!catalog) {
		pr_err("invalid input");
		return;
	}
	catalog->config_hpd(catalog, false);
}

static void dp_hpd_isr(struct dp_hpd *dp_hpd)
{
}

struct dp_hpd *dp_hpd_get(struct device *dev, struct dp_parser *parser,
		struct dp_catalog_hpd *catalog, struct dp_hpd_cb *cb)
{
	struct dp_hpd *dp_hpd;

	if (parser->no_aux_switch && parser->lphw_hpd) {
		dp_hpd = dp_lphw_hpd_get(dev, parser, catalog, cb);
		if (!dp_hpd) {
			pr_err("failed to get lphw hpd\n");
			return dp_hpd;
		}
		dp_hpd->type = DP_HPD_LPHW;
	} else if (parser->no_aux_switch) {
		dp_hpd = dp_gpio_hpd_get(dev, cb);
		if (!dp_hpd) {
			pr_err("failed to get gpio hpd\n");
			goto out;
		}
		dp_hpd->type = DP_HPD_GPIO;
	} else {
		dp_hpd = dp_usbpd_get(dev, cb);
		if (!dp_hpd) {
			pr_err("failed to get usbpd\n");
			goto out;
		}
		dp_hpd->type = DP_HPD_USBPD;
	}

	if (!dp_hpd->host_init)
		dp_hpd->host_init	= dp_hpd_host_init;
	if (!dp_hpd->host_deinit)
		dp_hpd->host_deinit	= dp_hpd_host_deinit;
	if (!dp_hpd->isr)
		dp_hpd->isr		= dp_hpd_isr;

out:
	return dp_hpd;
}

void dp_hpd_put(struct dp_hpd *dp_hpd)
{
	if (!dp_hpd)
		return;

	switch (dp_hpd->type) {
	case DP_HPD_USBPD:
		dp_usbpd_put(dp_hpd);
		break;
	case DP_HPD_GPIO:
		dp_gpio_hpd_put(dp_hpd);
		break;
	case DP_HPD_LPHW:
		dp_lphw_hpd_put(dp_hpd);
		break;
	default:
		pr_err("unknown hpd type %d\n", dp_hpd->type);
		break;
	}
}
