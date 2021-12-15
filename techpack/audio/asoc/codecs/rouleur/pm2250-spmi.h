/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _PM2250_SPMI_H
#define _PM2250_SPMI_H

#ifdef CONFIG_PM2250_SPMI
int pm2250_spmi_write(struct device *dev, int reg, int value);
int pm2250_spmi_read(struct device *dev, int reg, int *value);
#else
int pm2250_spmi_write(struct device *dev, int reg, int value)
{
	return 0;
}
int pm2250_spmi_read(struct device *dev, int reg, int *value);
{
	return 0;
}
#endif	/* CONFIG_PM2250_SPMI */

#endif
