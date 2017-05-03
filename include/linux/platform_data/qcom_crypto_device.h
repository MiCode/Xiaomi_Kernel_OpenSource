/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#ifndef __QCOM_CRYPTO_DEVICE__H
#define __QCOM_CRYPTO_DEVICE__H

struct msm_ce_hw_support {
	uint32_t ce_shared;
	uint32_t shared_ce_resource;
	uint32_t hw_key_support;
	uint32_t sha_hmac;
	void *bus_scale_table;
};

#endif /* __QCOM_CRYPTO_DEVICE__H */
