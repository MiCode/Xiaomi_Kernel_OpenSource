/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */


#ifndef _DP_GPIO_HPD_H_
#define _DP_GPIO_HPD_H_

#include "dp_hpd.h"

/**
 * dp_gpio_hpd_get() - configure and get the DisplayPlot HPD module data
 *
 * @dev: device instance of the caller
 * return: pointer to allocated gpio hpd module data
 *
 * This function sets up the gpio hpd module
 */
struct dp_hpd *dp_gpio_hpd_get(struct device *dev,
	struct dp_hpd_cb *cb);

/**
 * dp_gpio_hpd_put()
 *
 * Cleans up dp_hpd instance
 *
 * @hpd: instance of gpio_hpd
 */
void dp_gpio_hpd_put(struct dp_hpd *hpd);

#endif /* _DP_GPIO_HPD_H_ */
