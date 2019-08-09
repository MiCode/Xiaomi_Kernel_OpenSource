/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TRUSTZONE_TA_WIDEVINE__
#define __TRUSTZONE_TA_WIDEVINE__

#define TZ_TA_WIDEVINE_UUID   "ff33a6e0-8635-11e2-9e96-0800200c9a00"

/* Data Structure for Widevine TA
 * You should define data structure used both in REE/TEE here
 */
enum TEE_MTK_CryptoResult {
	TEE_MTK_Crypto_SUCCESS = 0,
	TEE_MTK_Crypto_ERROR_INIT_FAILED,
	TEE_MTK_Crypto_ERROR_TERMINATE_FAILED,
	TEE_MTK_Crypto_ERROR_ENTER_SECURE_PLAYBACK_FAILED,
	TEE_MTK_Crypto_ERROR_EXIT_SECURE_PLAYBACK_FAILED,
	TEE_MTK_Crypto_ERROR_SHORT_BUFFER,
	TEE_MTK_Crypto_ERROR_NO_DEVICE_KEY,
	TEE_MTK_Crypto_ERROR_NO_ASSET_KEY,
	TEE_MTK_Crypto_ERROR_KEYBOX_INVALID,
	TEE_MTK_Crypto_ERROR_NO_KEYDATA,
	TEE_MTK_Crypto_ERROR_NO_CW,
	TEE_MTK_Crypto_ERROR_DECRYPT_FAILED,
	TEE_MTK_Crypto_ERROR_WRITE_KEYBOX,
	TEE_MTK_Crypto_ERROR_WRAP_KEYBOX,
	TEE_MTK_Crypto_ERROR_BAD_MAGIC,
	TEE_MTK_Crypto_ERROR_BAD_CRC,
	TEE_MTK_Crypto_ERROR_NO_DEVICEID,
	TEE_MTK_Crypto_ERROR_RNG_FAILED,
	TEE_MTK_Crypto_ERROR_RNG_NOT_SUPPORTED,
	TEE_MTK_Crypto_ERROR_SETUP,
	TEE_MTK_Crypto_LEFT_NAL
};

/* Command for Widevine TA */

#define TZCMD_WIDEVINE_INIT                              1
#define TZCMD_WIDEVINE_TERMINATE                         2
#define TZCMD_WIDEVINE_SET_ENTITLEMENT_KEY               3
#define TZCMD_WIDEVINE_DEVICE_CONTROL                    4
#define TZCMD_WIDEVINE_DECRYPT_VIDEO                     5
#define TZCMD_WIDEVINE_DECRYPT_AUDIO                     6
#define TZCMD_WIDEVINE_INSTALL_KEYBOX                    7
#define TZCMD_WIDEVINE_IS_KEYBOX_VALID                   8
#define TZCMD_WIDEVINE_GET_DEVICE_ID                     9
#define TZCMD_WIDEVINE_GET_KEY_DATA                      10
#define TZCMD_WIDEVINE_RANDOM                            11
#define TZCMD_WIDEVINE_TEST                              12
#define TZCMD_WIDEVINE_READ_NAL                          13
/* Disable or enable debug level log in tee */
#define TZCMD_WIDEVINE_SET_DEBUG_LOG                     14

#endif				/* __TRUSTZONE_TA_WIDEVINE__ */
