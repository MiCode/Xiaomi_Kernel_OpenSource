/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm TSENS Header file
 *
 */

#ifndef __MSM_TSENS_H
#define __MSM_TSENS_H

enum platform_type {
	MSM_8660 = 0,
	MSM_8960,
	MSM_9615,
	MSM_TYPE
};

struct tsens_platform_data {
	int				slope;
	int				tsens_factor;
	uint32_t			tsens_num_sensor;
	enum platform_type		hw_type;
};

#endif /*MSM_TSENS_H */
