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
	FIPS140_STATUS_NA		= 0x0,
	FIPS140_STATUS_PASS_CRYPTO	= 0x1,
	FIPS140_STATUS_QCRYPTO_ALLOWED	= 0x2,
	FIPS140_STATUS_PASS		= 0x3,
	FIPS140_STATUS_FAIL		= 0xFF,
	FIPS140_CMD_OK			= 0x100,
	FIPS140_CMD_FAIL		= 0xFF00
};
#endif /* _UAPI_FIPS_STATUS__H */
