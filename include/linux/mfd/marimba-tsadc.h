/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#ifndef _MARIMBA_TSADC_H_
#define _MARIMBA_TSADC_H_

struct marimba_tsadc_client;

#define	TSSC_SUSPEND_LEVEL  1
#define	TSADC_SUSPEND_LEVEL 2

int marimba_tsadc_start(struct marimba_tsadc_client *client);

struct marimba_tsadc_client *
marimba_tsadc_register(struct platform_device *pdev, unsigned int is_ts);

void marimba_tsadc_unregister(struct marimba_tsadc_client *client);

#endif /* _MARIMBA_TSADC_H_ */
