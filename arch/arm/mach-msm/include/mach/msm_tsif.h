/* Copyright (c) 2009, 2011, The Linux Foundation. All rights reserved.
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
#ifndef _MSM_TSIF_H_
#define _MSM_TSIF_H_

struct msm_tsif_platform_data {
	int num_gpios;
	const struct msm_gpio *gpios;
	const char *tsif_clk;
	const char *tsif_pclk;
	const char *tsif_ref_clk;
	void (*init)(struct msm_tsif_platform_data *);
};

#endif /* _MSM_TSIF_H_ */

