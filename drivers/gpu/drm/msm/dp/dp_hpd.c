// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>

#include "dp_hpd.h"
#include "dp_usbpd.h"
#include "dp_gpio_hpd.h"

struct dp_hpd *dp_hpd_get(struct device *dev,
	struct dp_parser *parser, struct dp_hpd_cb *cb)
{
	struct dp_hpd *dp_hpd;

	if (parser->no_aux_switch) {
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
	default:
		pr_err("unknown hpd type %d\n", dp_hpd->type);
		break;
	}
}
