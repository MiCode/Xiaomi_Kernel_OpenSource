/*
 * Copyright (C) 2013 MediaTek Inc.
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

#ifndef __TRUSTZONE_TA_MODULAR_DRM__
#define __TRUSTZONE_TA_MODULAR_DRM__

#define TZ_TA_MODULAR_DRM_UUID   "651d6d29-0cf5-4a0f-b31a-9e8e8cec83a5"

/* Data Structure for Modular DRM TA */
/* You should define data structure used both in REE/TEE here
   N/A for Modular DRM TA */

/* Command for Modular DRM TA */

#define TZCMD_MODULAR_DRM_Initialize               1
#define TZCMD_MODULAR_DRM_Terminate                2
#define TZCMD_MODULAR_DRM_InstallKeybox            3
#define TZCMD_MODULAR_DRM_GetKeyData               4
#define TZCMD_MODULAR_DRM_IsKeyboxValid            5
#define TZCMD_MODULAR_DRM_GetRandom                6
#define TZCMD_MODULAR_DRM_GetDeviceID              7
#define TZCMD_MODULAR_DRM_WrapKeybox               8
#define TZCMD_MODULAR_DRM_OpenSession              9
#define TZCMD_MODULAR_DRM_CloseSession             10
#define TZCMD_MODULAR_DRM_DecryptCTR               11
#define TZCMD_MODULAR_DRM_GenerateDerivedKeys      12
#define TZCMD_MODULAR_DRM_GenerateSignature        13
#define TZCMD_MODULAR_DRM_GenerateNonce            14
#define TZCMD_MODULAR_DRM_LoadKeys                 15
#define TZCMD_MODULAR_DRM_RefreshKeys              16
#define TZCMD_MODULAR_DRM_SelectKey                17
#define TZCMD_MODULAR_DRM_RewrapDeviceRSAKey       18
#define TZCMD_MODULAR_DRM_LoadDeviceRSAKey         19
#define TZCMD_MODULAR_DRM_GenerateRSASignature     20
#define TZCMD_MODULAR_DRM_DeriveKeysFromSessionKey 21
#define TZCMD_MODULAR_DRM_APIVersion               22
#define TZCMD_MODULAR_DRM_SecurityLevel            23
#define TZCMD_MODULAR_DRM_Generic_Encrypt          24
#define TZCMD_MODULAR_DRM_Generic_Decrypt          25
#define TZCMD_MODULAR_DRM_Generic_Sign             26
#define TZCMD_MODULAR_DRM_Generic_Verify           27
#define TZCMD_MODULAR_DRM_GET_RSA_KEY_SIZE         28






#define TZCMD_MODULAR_DRM_TEST                     29

#define TZCMD_MODULAR_DRM_SET_DEBUG_LOG            30


/*added by zhitao yan*/
#define TZCMD_MODULAR_DRM_UpdateUsageTable         31
#define TZCMD_MODULAR_DRM_DeactivateUsageEntry     32
#define TZCMD_MODULAR_DRM_ReportUsage              33
#define TZCMD_MODULAR_DRM_DeleteUsageEntry         34
#define TZCMD_MODULAR_DRM_DeleteUsageTable         35
#define TZCMD_MODULAR_DRM_GetHDCPCapability        36

/*#define TZCMD_MODULAR_DRM_UpdateUsageTable         31*/








enum TEE_MTK_MODULAR_DRM_Crypto_Result {
	TEE_MTK_MODULAR_DRM_Crypto_SUCCESS                              = 0,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_INIT_FAILED                    = 1,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_TERMINATE_FAILED               = 2,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_OPEN_FAILURE                   = 3,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_CLOSE_FAILURE                  = 4,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_ENTER_SECURE_PLAYBACK_FAILED   = 5,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_EXIT_SECURE_PLAYBACK_FAILED    = 6,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_SHORT_BUFFER                   = 7,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_NO_DEVICE_KEY                  = 8,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_NO_ASSET_KEY                   = 9,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_KEYBOX_INVALID                 = 10,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_NO_KEYDATA                     = 11,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_NO_CW                          = 12,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_DECRYPT_FAILED                 = 13,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_WRITE_KEYBOX                   = 14,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_WRAP_KEYBOX                    = 15,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_BAD_MAGIC                      = 16,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_BAD_CRC                        = 17,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_NO_DEVICEID                    = 18,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_RNG_FAILED                     = 19,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_RNG_NOT_SUPPORTED              = 20,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_SETUP                          = 21,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_OPEN_SESSION_FAILED            = 22,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_CLOSE_SESSION_FAILED           = 23,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_INVALID_SESSION                = 24,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_NOT_IMPLEMENTED                = 25,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_NO_CONTENT_KEY                 = 26,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_CONTROL_INVALID                = 27,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_UNKNOWN_FAILURE                = 28,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_INVALID_CONTEXT                = 29,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_SIGNATURE_FAILURE              = 30,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_TOO_MANY_SESSIONS              = 31,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_INVALID_NONCE                  = 32,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_TOO_MANY_KEYS                  = 33,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_DEVICE_NOT_RSA_PROVISIONED     = 34,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_INVALID_RSA_KEY                = 35,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_KEY_EXPIRED                    = 36,
	TEE_MTK_MODULAR_DRM_Crypto_ERROR_INSUFFICIENT_RESOURCES         = 37,
};


#endif /* __TRUSTZONE_TA_MODULAR_DRM__ */
