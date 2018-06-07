/*
 * Copyright (c) 2014, 2018 The Linux Foundation. All rights reserved.
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

#ifndef __MSM_FIPS_DRBG_H__
#define __MSM_FIPS_DRBG_H__

#include "ctr_drbg.h"
#include "msm_rng.h"

#define FIPS140_PRNG_OK  (0)
#define FIPS140_PRNG_ERR (-1)

extern int _do_msm_fips_drbg_init(void *rng_dev);

typedef int (*get_entropy_callback_t)(void *ctx, void *buf);

struct fips_drbg_ctx_s {
	uint32_t magic;		/* for checking that ctx is likely valid */
	get_entropy_callback_t get_entropy_callback;
	void *get_entropy_callback_ctx;
	struct ctr_drbg_ctx_s ctr_drbg_ctx;
	uint8_t fips_drbg_started;
	uint8_t prev_hw_drbg_block[Q_HW_DRBG_BLOCK_BYTES];
};

/*
 * initialize *ctx, requesting automatic reseed after reseed_interval
 * calls to qpsi_rng_gen.  callback is a function to get entropy.
 * callback_ctx is a pointer to any context structure that function
 * may need.  (Pass NULL if no context structure is needed.) callback
 * must return zero or a positive number on success, and a
 * negative number on an error.
 */
int fips_drbg_init(struct msm_rng_device *msm_rng_ctx);

/* generated random data.  Returns 0 on success, -1 on failures */
int fips_drbg_gen(struct fips_drbg_ctx_s *ctx, void *tgt, size_t len);


/*
 * free resources and make zero state.
 * Failure to call fips_drbg_final is not a security issue, since
 * CTR_DRBG provides backtracking resistance by updating Key and V
 * immediately after the data has been generated but before the
 * generate function returns.  But it is a resource issue (except at
 * program termination), as it abandons a FILE structure and a file
 * descriptor.
 */
void fips_drbg_final(struct fips_drbg_ctx_s *ctx);

#endif /* __MSM_FIPS_DRBG_H__ */
