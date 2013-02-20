/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_TSPP_H_
#define _MSM_TSPP_H_

struct msm_tspp_platform_data {
	int num_gpios;
	const struct msm_gpio *gpios;
	const char *tsif_pclk;
	const char *tsif_ref_clk;
};

#endif /* _MSM_TSPP_H_ */

