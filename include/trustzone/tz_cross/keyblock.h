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

#ifndef __KEY_BLOCK_H__
#define __KEY_BLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		WIDEVINE_ID = 0,
		MARLIN_ID,
		HDCP_1X_TX_ID,
		HDCP_2X_V1_TX_ID,
		HDCP_2X_V1_RX_ID,
		HDCP_2X_V2_TX_ID,
		HDCP_2X_V2_RX_ID,
		PLAYREADY_BGROUPCERT_ID,
		PLAYREADY_ZGPRIV_ID,
		PLAYREADY_KEYFILE_ID,
		DRM_KEY_MAX,
		DRM_SP_EKKB = 0xFFFF
	} DRMKeyID;

#define SZ_DRMKEY_ID 4
#define SZ_DRMKEY_TYPE 4
#define SZ_DRMKEY_SIZE 4
#define SZ_DRMKEY_ENC_SIZE 4
#define SZ_DRMKEY_CLEAR_IV 16
#define SZ_DRMKEY_RESERVED 64
#define SZ_DRMKEY_HEADER_SIZE \
	(SZ_DRMKEY_ID+SZ_DRMKEY_TYPE+SZ_DRMKEY_SIZE+SZ_DRMKEY_ENC_SIZE+\
	SZ_DRMKEY_CLEAR_IV+SZ_DRMKEY_RESERVED)
#define SZ_DRMKEY_SIG 16

/* begin of uree using */

/*
[in] keyID			  Enum DRMKeyID
[out] oneDrmkeyBlock  encrypt DRMBlock
[out] blockLeng	  encrypt DRMBlockLength

return	 0: OK,  others: FAIL
*/
int get_encrypt_drmkey(unsigned int keyID,
			unsigned char **oneDrmkeyBlock,
			unsigned int *blockLeng);

int get_clearDrmkey_size(unsigned int keyID, unsigned int *leng);

int free_encrypt_drmkey(unsigned char *oneEncDrmkeyBlock);

int write_kbo_drmkey(DRMKeyID id, unsigned char *enckey, unsigned int length);

int delete_kbo_drmkey(DRMKeyID id);

int install_KB_OTA_API(unsigned char *buff, unsigned int len);

int query_drmkey(unsigned int *count, unsigned int *keytype);

/* end of uree using */

/* begin for tee using */
int encrypt_drmkey(DRMKeyID id, unsigned char *clearKey, unsigned int inLength,
			   unsigned char **encKey, unsigned int *outLength);

int decrypt_drmkey(unsigned char *encDrmKeyBlock, unsigned int inLength,
		   unsigned char **DrmKey, unsigned int *outLength);

int free_drmkey(unsigned char *drmkey);

int free_drmkey_safe(unsigned char *drmkey, int size);

/* end for tee using */

#ifdef __cplusplus
}
#endif
#endif				/* __KEY_BLOCK_H__ */
