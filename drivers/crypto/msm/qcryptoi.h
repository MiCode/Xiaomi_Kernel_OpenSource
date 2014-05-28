/* QTI Crypto driver
 *
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __CRYPTO_MSM_QCRYPTOI_H
#define __CRYPTO_MSM_QCRYPTOI_H

/* FIPS global status variable */
extern enum fips_status g_fips140_status;

/* The structure to hold data
 * that selftests require
 */
struct fips_selftest_data {

	char algo_prefix[10];
	unsigned int ce_device;
	bool prefix_ahash_algo;
	bool prefix_hmac_algo;
	bool prefix_aes_xts_algo;
	bool prefix_aes_cbc_ecb_ctr_algo;
	bool prefix_aead_algo;
};

#ifdef CONFIG_FIPS_ENABLE
/*
 * Sha/HMAC self tests
 */
int _fips_qcrypto_sha_selftest(struct fips_selftest_data *selftest_d);

/*
* Cipher algorithm self tests
*/
int _fips_qcrypto_cipher_selftest(struct fips_selftest_data *selftest_d);

/*
 * AEAD algorithm self tests
 */
int _fips_qcrypto_aead_selftest(struct fips_selftest_data *selftest_d);

#else

static inline
int _fips_qcrypto_sha_selftest(struct fips_selftest_data *selftest_d)
{
	return 0;
}

static inline
int _fips_qcrypto_cipher_selftest(struct fips_selftest_data *selftest_d)
{
	return 0;
}

static
inline int _fips_qcrypto_aead_selftest(struct fips_selftest_data *selftest_d)
{
	return 0;
}

#endif  /* CONFIG_FIPS_ENABLE*/

#endif  /* __CRYPTO_MSM_QCRYPTOI_H */

