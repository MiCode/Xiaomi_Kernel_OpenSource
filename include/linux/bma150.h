/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
#ifndef LINUX_BMA150_MODULE_H
#define LINUX_BMA150_MODULE_H

/**
 * struct bma150_platform_data - data to set up bma150 driver
 *
 * @setup: optional callback to activate the driver.
 * @teardown: optional callback to invalidate the driver.
 *
**/

struct bma150_platform_data {
	int (*setup)(struct device *);
	void (*teardown)(struct device *);
	int (*power_on)(void);
	void (*power_off)(void);
};

#endif /* LINUX_BMA150_MODULE_H */
