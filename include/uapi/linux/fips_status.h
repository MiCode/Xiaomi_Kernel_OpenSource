/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_FIPS_STATUS__H
#define _UAPI_FIPS_STATUS__H

#include <linux/types.h>
#include <linux/ioctl.h>

/**
 * fips_status: global FIPS140-2 status
 * @FIPS140_STATUS_NA:
 *					Not a FIPS140-2 compliant Build.
 *					The flag status won't
 *					change throughout
 *					the lifetime
 * @FIPS140_STATUS_PASS_CRYPTO:
 *					KAT self tests are passed.
 * @FIPS140_STATUS_QCRYPTO_ALLOWED:
 *					Integrity test is passed.
 * @FIPS140_STATUS_PASS:
 *					All tests are passed and build
 *					is in FIPS140-2 mode
 * @FIPS140_STATUS_FAIL:
 *					One of the test is failed.
 *					This will block all requests
 *					to crypto modules
 */
enum fips_status {
		FIPS140_STATUS_NA				= 0,
		FIPS140_STATUS_PASS_CRYPTO		= 1,
		FIPS140_STATUS_QCRYPTO_ALLOWED	= 2,
		FIPS140_STATUS_PASS				= 3,
		FIPS140_STATUS_FAIL				= 0xFF
};
#endif /* _UAPI_FIPS_STATUS__H */
