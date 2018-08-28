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

#ifndef __MSM_FIPS_SELFTEST_H__
#define __MSM_FIPS_SELFTEST_H__

struct ctr_debg_test_inputs_s {
	char *entropy_string;		/* must by 16 bytes */
	char *nonce_string;		/* must be 8 bytes */
	char *reseed_entropy_string;	/* must be 16 bytes */
	char *observed_string;		/* length is defined
						in observed_string_len */
	int  observed_string_len;
};

int fips_ctraes128_df_known_answer_test(struct ctr_debg_test_inputs_s *tcase);

int fips_self_test(void);

#endif  /* __MSM_FIPS_SELFTEST_H__ */
