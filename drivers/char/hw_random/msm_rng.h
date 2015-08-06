/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#ifndef __MSM_RNG_HEADER__
#define __MSM_RNG_HEADER__

#include <linux/semaphore.h>
#include <linux/qcedev.h>

struct _fips_drbg_ctx;

#define FIPS140_DRBG_ENABLED  (1)
#define FIPS140_DRBG_DISABLED (0)

#define Q_HW_DRBG_BLOCK_BYTES (32)

extern void fips_reg_drbg_callback(void *src);

struct msm_rng_device {
	struct platform_device *pdev;
	void __iomem *base;
	struct clk *prng_clk;
	uint32_t qrng_perf_client;
	struct mutex rng_lock;
	struct fips_drbg_ctx_s *drbg_ctx;
	int    fips140_drbg_enabled;
};

/*
 *
 *  This function calls hardware random bit generator
 *  directory and retuns it back to caller.
 *
 */
int msm_rng_direct_read(struct msm_rng_device *msm_rng_dev,
				void *data, size_t max);
#endif
