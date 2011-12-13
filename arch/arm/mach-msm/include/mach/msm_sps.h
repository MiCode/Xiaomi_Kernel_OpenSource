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
#ifndef _MSM_SPS_H_
#define _MSM_SPS_H_

/**
 * struct msm_sps_platform_data - SPS Platform specific data.
 * @bamdma_restricted_pipes - Bitmask of pipes restricted from local use.
 *
 */
struct msm_sps_platform_data {
	u32 bamdma_restricted_pipes;
};

#endif /* _MSM_SPS_H_ */

