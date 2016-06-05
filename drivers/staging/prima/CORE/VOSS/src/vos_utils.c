/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*============================================================================
  FILE:         vos_utils.c

  OVERVIEW:     This source file contains definitions for vOS crypto APIs
                The four APIs mentioned in this file are used for
                initializing, and de-initializing a crypto context, and
                obtaining truly random data (for keys), as well as
                SHA1 HMAC, and AES encrypt and decrypt routines.

                The routines include:
                vos_crypto_init() - Initializes Crypto module
                vos_crypto_deinit() - De-initializes Crypto module
                vos_rand_get_bytes() - Generates random byte
                vos_sha1_hmac_str() - Generate the HMAC-SHA1 of a string given a key
                vos_encrypt_AES() - Generate AES Encrypted byte stream
                vos_decrypt_AES() - Decrypts an AES Encrypted byte stream

  DEPENDENCIES:

============================================================================*/

/*============================================================================
  EDIT HISTORY FOR MODULE

============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

#include "vos_trace.h"
#include "vos_utils.h"
#include "vos_memory.h"
#include <linux/wcnss_wlan.h>

#include <linux/err.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/completion.h>
#include <linux/vmalloc.h>
#include <crypto/hash.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/
extern struct crypto_ahash *wcnss_wlan_crypto_alloc_ahash(const char *alg_name,
                                                          unsigned int type,
                                                          unsigned int mask);

extern int wcnss_wlan_crypto_ahash_digest(struct ahash_request *req);
extern void wcnss_wlan_crypto_free_ahash(struct crypto_ahash *tfm);
extern int wcnss_wlan_crypto_ahash_setkey(struct crypto_ahash *tfm, const u8 *key,
                                          unsigned int keylen);
extern struct crypto_ablkcipher *wcnss_wlan_crypto_alloc_ablkcipher(const char *alg_name,
                                                                    u32 type, u32 mask);
extern void wcnss_wlan_ablkcipher_request_free(struct ablkcipher_request *req);
extern void wcnss_wlan_crypto_free_ablkcipher(struct crypto_ablkcipher *tfm);

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
   Function Definitions and Documentation
 * -------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------

  \brief vos_crypto_init() - Initializes Crypto module

  The vos_crypto_init() function initializes Crypto module.

  \param phCryptProv - pointer to the Crypt handle

  \return VOS_STATUS_SUCCESS - Successfully generated random memory.

          VOS_STATUS_E_FAULT  - pbBuf is an invalid pointer.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons

       ***VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable
  \sa

    ( *** return value not considered yet )
  --------------------------------------------------------------------------*/
VOS_STATUS vos_crypto_init( v_U32_t *phCryptProv )
{
    VOS_STATUS uResult = VOS_STATUS_E_FAILURE;

    // This implementation doesn't require a crypto context
    *phCryptProv  = 0;
    uResult = VOS_STATUS_SUCCESS;
    return ( uResult );
}

VOS_STATUS vos_crypto_deinit( v_U32_t hCryptProv )
{
    VOS_STATUS uResult = VOS_STATUS_E_FAILURE;

    // CryptReleaseContext succeeded
    uResult = VOS_STATUS_SUCCESS;

    return ( uResult );
}

/*--------------------------------------------------------------------------

  \brief vos_rand_get_bytes() - Generates random byte

  The vos_rand_get_bytes() function generate random bytes.

  Buffer should be allocated before calling vos_rand_get_bytes().

  Attempting to initialize an already initialized lock results in
  a failure.

  \param lock - pointer to the opaque lock object to initialize

  \return VOS_STATUS_SUCCESS - Successfully generated random memory.

          VOS_STATUS_E_FAULT  - pbBuf is an invalid pointer.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons

       ***VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable
  \sa

    ( *** return value not considered yet )
  --------------------------------------------------------------------------*/
VOS_STATUS vos_rand_get_bytes( v_U32_t cryptHandle, v_U8_t *pbBuf, v_U32_t numBytes )
{
   VOS_STATUS uResult = VOS_STATUS_E_FAILURE;
   //v_UINT_t uCode;
//   HCRYPTPROV hCryptProv = (HCRYPTPROV) cryptHandle;

   //check for invalid pointer
   if ( NULL == pbBuf )
   {
      uResult = VOS_STATUS_E_FAULT;
      return ( uResult );
   }

//#if 0
   // get_random_bytes() is a void procedure
   get_random_bytes( pbBuf, numBytes);
   // "Random sequence generated."
   uResult = VOS_STATUS_SUCCESS;
//#endif

   return ( uResult );
}


/**
 * vos_sha1_hmac_str
 *
 * FUNCTION:
 * Generate the HMAC-SHA1 of a string given a key.
 *
 * LOGIC:
 * Standard HMAC processing from RFC 2104. The code is provided in the
 * appendix of the RFC.
 *
 * ASSUMPTIONS:
 * The RFC is correct.
 *
 * @param text text to be hashed
 * @param textLen length of text
 * @param key key to use for HMAC
 * @param keyLen length of key
 * @param digest holds resultant SHA1 HMAC (20B)
 *
 * @return VOS_STATUS_SUCCSS if the operation succeeds
 *
 */

struct hmac_sha1_result {
    struct completion completion;
    int err;
};

static void hmac_sha1_complete(struct crypto_async_request *req, int err)
{
    struct hmac_sha1_result *r = req->data;
    if (err == -EINPROGRESS)
        return;
    r->err = err;
    complete(&r->completion);
}

int hmac_sha1(v_U8_t *key, v_U8_t ksize, char *plaintext, v_U8_t psize,
              v_U8_t *output, v_U8_t outlen)
{
    int ret = 0;
    struct crypto_ahash *tfm;
    struct scatterlist sg;
    struct ahash_request *req;
    struct hmac_sha1_result tresult;
    void *hash_buff = NULL;

    unsigned char hash_result[64];
    int i;

    memset(output, 0, outlen);

    init_completion(&tresult.completion);

    tfm = wcnss_wlan_crypto_alloc_ahash("hmac(sha1)", CRYPTO_ALG_TYPE_AHASH,
                                        CRYPTO_ALG_TYPE_AHASH_MASK);
    if (IS_ERR(tfm)) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_alloc_ahash failed");
        ret = PTR_ERR(tfm);
        goto err_tfm;
    }

    req = ahash_request_alloc(tfm, GFP_KERNEL);
    if (!req) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "failed to allocate request for hmac(sha1)");
        ret = -ENOMEM;
        goto err_req;
    }

    ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
                               hmac_sha1_complete, &tresult);

    hash_buff = kzalloc(psize, GFP_KERNEL);
    if (!hash_buff) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "failed to kzalloc hash_buff");
        ret = -ENOMEM;
        goto err_hash_buf;
    }

    memset(hash_result, 0, 64);
    vos_mem_copy(hash_buff, plaintext, psize);
    sg_init_one(&sg, hash_buff, psize);

    if (ksize) {
        crypto_ahash_clear_flags(tfm, ~0);
        ret = wcnss_wlan_crypto_ahash_setkey(tfm, key, ksize);

        if (ret) {
            VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_ahash_setkey failed");
            goto err_setkey;
        }
    }

    ahash_request_set_crypt(req, &sg, hash_result, psize);
    ret = wcnss_wlan_crypto_ahash_digest(req);

    VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "ret 0x%x", ret);

    switch (ret) {
    case 0:
        for (i=0; i< outlen; i++)
            output[i] = hash_result[i];
        break;
    case -EINPROGRESS:
    case -EBUSY:
        ret = wait_for_completion_interruptible(&tresult.completion);
        if (!ret && !tresult.err) {
            for (i=0; i< outlen; i++)
                output[i] = hash_result[i];
            INIT_COMPLETION(tresult.completion);
            break;
        } else {
            VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "wait_for_completion_interruptible failed");
            if (!ret)
                ret = tresult.err;
            goto out;
        }
    default:
        goto out;
    }

out:
err_setkey:
    kfree(hash_buff);
err_hash_buf:
    ahash_request_free(req);
err_req:
    wcnss_wlan_crypto_free_ahash(tfm);
err_tfm:
    return ret;
}

VOS_STATUS vos_sha1_hmac_str(v_U32_t cryptHandle, /* Handle */
           v_U8_t *pText, /* pointer to data stream */
           v_U32_t textLen, /* length of data stream */
           v_U8_t *pKey, /* pointer to authentication key */
           v_U32_t keyLen, /* length of authentication key */
           v_U8_t digest[VOS_DIGEST_SHA1_SIZE])/* caller digest to be filled in */
{
    int ret = 0;

    ret = hmac_sha1(
            pKey,                   //v_U8_t *key,
            (v_U8_t) keyLen,        //v_U8_t ksize,
            (char *)pText,          //char *plaintext,
            (v_U8_t) textLen,       //v_U8_t psize,
            digest,                 //v_U8_t *output,
            VOS_DIGEST_SHA1_SIZE    //v_U8_t outlen
            );

    if (ret != 0) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR,"hmac_sha1() call failed");
        return VOS_STATUS_E_FAULT;
    }

    return VOS_STATUS_SUCCESS;
}

/**
 * vos_md5_hmac_str
 *
 * FUNCTION:
 * Generate the HMAC-MD5 of a string given a key.
 *
 * LOGIC:
 * Standard HMAC processing from RFC 2104. The code is provided in the
 * appendix of the RFC.
 *
 * ASSUMPTIONS:
 * The RFC is correct.
 *
 * @param text text to be hashed
 * @param textLen length of text
 * @param key key to use for HMAC
 * @param keyLen length of key
 * @param digest holds resultant MD5 HMAC (20B)
 *
 * @return VOS_STATUS_SUCCSS if the operation succeeds
 *
 */
struct hmac_md5_result {
    struct completion completion;
    int err;
};

static void hmac_md5_complete(struct crypto_async_request *req, int err)
{
    struct hmac_md5_result *r = req->data;
    if (err == -EINPROGRESS)
            return;
    r->err = err;
    complete(&r->completion);
}

int hmac_md5(v_U8_t *key, v_U8_t ksize, char *plaintext, v_U8_t psize,
                v_U8_t *output, v_U8_t outlen)
{
    int ret = 0;
    struct crypto_ahash *tfm;
    struct scatterlist sg;
    struct ahash_request *req;
    struct hmac_md5_result tresult = {.err = 0};
    void *hash_buff = NULL;

    unsigned char hash_result[64];
    int i;

    memset(output, 0, outlen);

    init_completion(&tresult.completion);

    tfm = wcnss_wlan_crypto_alloc_ahash("hmac(md5)", CRYPTO_ALG_TYPE_AHASH,
                                        CRYPTO_ALG_TYPE_AHASH_MASK);
    if (IS_ERR(tfm)) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_alloc_ahash failed");
                ret = PTR_ERR(tfm);
                goto err_tfm;
    }

    req = ahash_request_alloc(tfm, GFP_KERNEL);
    if (!req) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "failed to allocate request for hmac(md5)");
        ret = -ENOMEM;
        goto err_req;
    }

    ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
                                        hmac_md5_complete, &tresult);

    hash_buff = kzalloc(psize, GFP_KERNEL);
    if (!hash_buff) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "failed to kzalloc hash_buff");
        ret = -ENOMEM;
        goto err_hash_buf;
    }

    memset(hash_result, 0, 64);
    vos_mem_copy(hash_buff, plaintext, psize);
    sg_init_one(&sg, hash_buff, psize);

    if (ksize) {
        crypto_ahash_clear_flags(tfm, ~0);
        ret = wcnss_wlan_crypto_ahash_setkey(tfm, key, ksize);

        if (ret) {
            VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_ahash_setkey failed");
            goto err_setkey;
        }
    }

    ahash_request_set_crypt(req, &sg, hash_result, psize);
    ret = wcnss_wlan_crypto_ahash_digest(req);

    VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "ret 0x%x", ret);

    switch (ret) {
        case 0:
            for (i=0; i< outlen; i++)
                    output[i] = hash_result[i];
            break;
        case -EINPROGRESS:
        case -EBUSY:
             ret = wait_for_completion_interruptible(&tresult.completion);
             if (!ret && !tresult.err) {
                 for (i=0; i< outlen; i++)
                     output[i] = hash_result[i];
                 INIT_COMPLETION(tresult.completion);
                 break;
             } else {
                 VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "wait_for_completion_interruptible failed");
                 if (!ret)
                     ret = tresult.err;
                 goto out;
             }
        default:
              goto out;
        }

out:
err_setkey:
        kfree(hash_buff);
err_hash_buf:
        ahash_request_free(req);
err_req:
        wcnss_wlan_crypto_free_ahash(tfm);
err_tfm:
        return ret;
}

VOS_STATUS vos_md5_hmac_str(v_U32_t cryptHandle, /* Handle */
           v_U8_t *pText, /* pointer to data stream */
           v_U32_t textLen, /* length of data stream */
           v_U8_t *pKey, /* pointer to authentication key */
           v_U32_t keyLen, /* length of authentication key */
           v_U8_t digest[VOS_DIGEST_MD5_SIZE])/* caller digest to be filled in */
{
    int ret = 0;

    ret = hmac_md5(
            pKey,                   //v_U8_t *key,
            (v_U8_t) keyLen,        //v_U8_t ksize,
            (char *)pText,          //char *plaintext,
            (v_U8_t) textLen,       //v_U8_t psize,
            digest,                 //v_U8_t *output,
            VOS_DIGEST_MD5_SIZE     //v_U8_t outlen
            );

    if (ret != 0) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR,"hmac_md5() call failed");
        return VOS_STATUS_E_FAULT;
    }

    return VOS_STATUS_SUCCESS;
}


struct ecb_aes_result {
    struct completion completion;
    int err;
};

static void ecb_aes_complete(struct crypto_async_request *req, int err)
{
    struct ecb_aes_result *r = req->data;
    if (err == -EINPROGRESS)
        return;
    r->err = err;
    complete(&r->completion);
}


/*--------------------------------------------------------------------------

  \brief vos_encrypt_AES() - Generate AES Encrypted byte stream

  The vos_encrypt_AES() function generates the encrypted byte stream for given text.

  Buffer should be allocated before calling vos_rand_get_bytes().

  Attempting to initialize an already initialized lock results in
  a failure.

  \param lock - pointer to the opaque lock object to initialize

  \return VOS_STATUS_SUCCESS - Successfully generated random memory.

          VOS_STATUS_E_FAULT  - pbBuf is an invalid pointer.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons

       ***VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable
  \sa

    ( *** return value not considered yet )
  --------------------------------------------------------------------------*/

#define IV_SIZE_AES_128 16
#define KEY_SIZE_AES_128 16
#define AES_BLOCK_SIZE 16

VOS_STATUS vos_encrypt_AES(v_U32_t cryptHandle, /* Handle */
                           v_U8_t *pPlainText, /* pointer to data stream */
                           v_U8_t *pCiphertext,
                           v_U8_t *pKey) /* pointer to authentication key */
{
//    VOS_STATUS uResult = VOS_STATUS_E_FAILURE;
    struct ecb_aes_result result;
    struct ablkcipher_request *req;
    struct crypto_ablkcipher *tfm;
    int ret = 0;
    char iv[IV_SIZE_AES_128];
    struct scatterlist sg_in;
    struct scatterlist sg_out;

    init_completion(&result.completion);

    tfm =  wcnss_wlan_crypto_alloc_ablkcipher( "cbc(aes)", 0, 0);
    if (IS_ERR(tfm)) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_alloc_ablkcipher failed");
        ret = PTR_ERR(tfm);
        goto err_tfm;
    }

    req = ablkcipher_request_alloc(tfm, GFP_KERNEL);
    if (!req) {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "Failed to allocate request for cbc(aes)");
        ret = -ENOMEM;
        goto err_req;
    }

    ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
                                    ecb_aes_complete, &result);


    crypto_ablkcipher_clear_flags(tfm, ~0);

    ret = crypto_ablkcipher_setkey(tfm, pKey, KEY_SIZE_AES_128);
    if (ret) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_cipher_setkey failed");
        goto err_setkey;
    }

    memset(iv, 0, IV_SIZE_AES_128);

    sg_init_one(&sg_in, pPlainText, AES_BLOCK_SIZE);

    sg_init_one(&sg_out, pCiphertext, AES_BLOCK_SIZE);

    ablkcipher_request_set_crypt(req, &sg_in, &sg_out, AES_BLOCK_SIZE, iv);

    crypto_ablkcipher_encrypt(req);



// -------------------------------------
err_setkey:
    wcnss_wlan_ablkcipher_request_free(req);
err_req:
    wcnss_wlan_crypto_free_ablkcipher(tfm);
err_tfm:
    //return ret;
    if (ret != 0) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR,"%s() call failed", __func__);
        return VOS_STATUS_E_FAULT;
   }

    return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_decrypt_AES() - Decrypts an AES Encrypted byte stream

  The vos_decrypt_AES() function decrypts the encrypted byte stream.

  Buffer should be allocated before calling vos_rand_get_bytes().

  Attempting to initialize an already initialized lock results in
  a failure.

  \param lock - pointer to the opaque lock object to initialize

  \return VOS_STATUS_SUCCESS - Successfully generated random memory.

          VOS_STATUS_E_FAULT  - pbBuf is an invalid pointer.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons

       ***VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable
  \sa

    ( *** return value not considered yet )
  --------------------------------------------------------------------------*/

VOS_STATUS vos_decrypt_AES(v_U32_t cryptHandle, /* Handle */
                           v_U8_t *pText, /* pointer to data stream */
                           v_U8_t *pDecrypted,
                           v_U8_t *pKey) /* pointer to authentication key */
{
//    VOS_STATUS uResult = VOS_STATUS_E_FAILURE;
    struct ecb_aes_result result;
    struct ablkcipher_request *req;
    struct crypto_ablkcipher *tfm;
    int ret = 0;
    char iv[IV_SIZE_AES_128];
    struct scatterlist sg_in;
    struct scatterlist sg_out;

    init_completion(&result.completion);

    tfm =  wcnss_wlan_crypto_alloc_ablkcipher( "cbc(aes)", 0, 0);
    if (IS_ERR(tfm)) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_alloc_ablkcipher failed");
        ret = PTR_ERR(tfm);
        goto err_tfm;
    }

    req = ablkcipher_request_alloc(tfm, GFP_KERNEL);
    if (!req) {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "Failed to allocate request for cbc(aes)");
        ret = -ENOMEM;
        goto err_req;
    }

    ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
                                    ecb_aes_complete, &result);


    crypto_ablkcipher_clear_flags(tfm, ~0);

    ret = crypto_ablkcipher_setkey(tfm, pKey, KEY_SIZE_AES_128);
    if (ret) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR, "crypto_cipher_setkey failed");
        goto err_setkey;
       }

    memset(iv, 0, IV_SIZE_AES_128);

    sg_init_one(&sg_in, pText, AES_BLOCK_SIZE);

    sg_init_one(&sg_out, pDecrypted, AES_BLOCK_SIZE);

    ablkcipher_request_set_crypt(req, &sg_in, &sg_out, AES_BLOCK_SIZE, iv);

    crypto_ablkcipher_decrypt(req);



// -------------------------------------
err_setkey:
    wcnss_wlan_ablkcipher_request_free(req);
err_req:
    wcnss_wlan_crypto_free_ablkcipher(tfm);
err_tfm:
    //return ret;
    if (ret != 0) {
        VOS_TRACE(VOS_MODULE_ID_VOSS,VOS_TRACE_LEVEL_ERROR,"%s() call failed", __func__);
        return VOS_STATUS_E_FAULT;
      }

    return VOS_STATUS_SUCCESS;
}

v_U8_t vos_chan_to_band(v_U32_t chan)
{
    if (chan <= VOS_24_GHZ_CHANNEL_14)
        return VOS_BAND_2GHZ;

    return VOS_BAND_5GHZ;
}

void vos_get_wlan_unsafe_channel(v_U16_t *unsafeChannelList,
                           v_U16_t buffer_size, v_U16_t *unsafeChannelCount)
{
    /* Get unsafe channel list from cached location */
    wcnss_get_wlan_unsafe_channel(unsafeChannelList, buffer_size,
                                  unsafeChannelCount);
}


#include <linux/skbuff.h>
#include "vos_timer.h"
#include "i_vos_packet.h"

#define MAC_ADDR_ARRAY(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MAC_ADDRESS_STR "%02x:%02x:%02x:%02x:%02x:%02x"
#define DXE_DATA_MAGIC_NO 0x010
#define DXE_MGMT_MAGIC_NO 0x011

//Define gRoamDelayMetaInfo
tRoamDelayMetaInfo gRoamDelayMetaInfo = {0};
tRoamDelayMetaInfo *gpRoamDelayTable = NULL;
v_BOOL_t gRoamDelayCurrentIndex = 0;

#define VOS_ETHERTYPE_802_1_X                           ( 0x888E )
#define VOS_ETHERTYPE_802_1_X_SIZE                      ( 2 )
//802.3 frame header have SRC MAC (6), DST(6),next will PROTOCOL type
#define VOS_ETHERTYPE_802_1_X_FRAME_OFFSET_IN_802_3_PKT ( 12 )

//802.11 header wil have 26 byte (Inculding QoS Info)
//8Byte LLC / SNAP header in which last two byte will be PROTOCOL type
//So offset will 32 if it is QoS data pkt else it will be 30
#define VOS_ETHERTYPE_802_1_X_FRAME_OFFSET_IN_802_11_PKT ( 32 )
#define VOS_QOS_DATA_VALUE                              ( 0x88 )
#define VOS_NON_QOS_DATA_VALUE                          ( 0x80 )


// Frame Type definitions
#define VOS_MAC_MGMT_FRAME    0x0
#define VOS_MAC_CTRL_FRAME    0x1
#define VOS_MAC_DATA_FRAME    0x2

#define MONITOR_STOP            0x0
#define MONITOR_START           0x1
#define MONITOR_EAPOL_DONE      0x2
#define MONITOR_FIRST_DATA_DONE 0x4

v_BOOL_t vos_skb_is_eapol(struct sk_buff *skb,
                          v_SIZE_t pktOffset,
                          v_SIZE_t numBytes)
{
    void       *pBuffer   = NULL;
    v_BOOL_t   fEAPOL     = VOS_FALSE;
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "enter vos_skb_is_eapol");
    //vos_trace_hex_dump( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, &skb->data[0], skb->len);
    // Validate the skb
    if (unlikely(NULL == skb))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "vos_skb_is_eapol [%d]: NULL skb", __LINE__);
        return VOS_STATUS_E_INVAL;
        VOS_ASSERT(0);
    }
    // check for overflow
    if (unlikely((pktOffset + numBytes) > skb->len))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "vos_skb_is_eapol [%d]: Packet overflow, offset %d size %d len %d",
                  __LINE__, pktOffset, numBytes, skb->len);
        return VOS_STATUS_E_INVAL;
    }
    //check for the Qos Data, if Offset length is more 12.
    //it means it will 802.11 header skb
    if((pktOffset > VOS_ETHERTYPE_802_1_X_FRAME_OFFSET_IN_802_3_PKT)
       && (skb->data[0] == VOS_NON_QOS_DATA_VALUE))
    {
        // reduced 2 byte of Qos ctrl field in DOT11 header
        pktOffset = pktOffset - 2;
    }
    pBuffer = &skb->data[pktOffset];
    if (pBuffer && vos_be16_to_cpu( *(unsigned short*)pBuffer ) == VOS_ETHERTYPE_802_1_X )
    {
      fEAPOL = VOS_TRUE;
    }
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "exit vos_skb_is_eapol fEAPOL = %d", fEAPOL);
    return fEAPOL;
}

v_BOOL_t vos_roam_delay_stats_init(void)
{
    if (gpRoamDelayTable == NULL)
    {
        gpRoamDelayTable = vmalloc(sizeof(tRoamDelayMetaInfo) * ROAM_DELAY_TABLE_SIZE);
        if (gpRoamDelayTable == NULL)
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "Memory allocation failed");
            return VOS_FALSE;
        }
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "Memory is already allocated");
        return VOS_FALSE;
    }

    return VOS_TRUE;
}


v_BOOL_t vos_roam_delay_stats_deinit(void)
{
    if (gpRoamDelayTable != NULL)
    {
        vfree(gpRoamDelayTable);
        gpRoamDelayTable = NULL;
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "Memory is already freed");
        return VOS_FALSE;
    }

    return VOS_TRUE;
}

void vos_record_roam_event(enum e_roaming_event roam_event, void *pBuff, v_ULONG_t buff_len)
{
    if (gpRoamDelayTable == NULL)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "Roam delay table is not initialized\n");
        return;
    }
    switch(roam_event)
    {
        case e_HDD_DISABLE_TX_QUEUE:
             gRoamDelayMetaInfo.hdd_monitor_tx = MONITOR_STOP;
             gRoamDelayMetaInfo.disable_tx_queues_time = vos_timer_get_system_time();
             break;
        case e_SME_PREAUTH_REASSOC_START:
             gRoamDelayMetaInfo.preauth_reassoc_start_time = vos_timer_get_system_time();
             break;
        case e_SME_PREAUTH_CALLBACK_HIT:
             gRoamDelayMetaInfo.preauth_cb_time = vos_timer_get_system_time();
             break;
        case e_SME_ISSUE_REASSOC_REQ:
             gRoamDelayMetaInfo.issue_reassoc_req_time = vos_timer_get_system_time();
             //HACK buff len will carry the AuthType
             gRoamDelayMetaInfo.hdd_auth_type = buff_len;
             break;
        case e_LIM_SEND_REASSOC_REQ:
             gRoamDelayMetaInfo.send_reassoc_req_time = vos_timer_get_system_time();
             //we can enable the rx eapol monitoring ASAP we send the REASSOC REQ Because
             //there is very less delay in between REASSOC RSP and M1 Sent by the AP
             gRoamDelayMetaInfo.hdd_monitor_rx = MONITOR_START;
             gRoamDelayMetaInfo.dxe_monitor_rx = MONITOR_START;
             break;
        case e_CACHE_ROAM_PEER_MAC:
             vos_mem_copy(&gRoamDelayMetaInfo.peer_mac_addr, pBuff, buff_len);
             break;
        case e_HDD_SEND_REASSOC_RSP:
             gRoamDelayMetaInfo.hdd_sendassoc_rsp_time = vos_timer_get_system_time();
             break;
        case e_SME_DISASSOC_ISSUE:
             gRoamDelayMetaInfo.disassoc_issue_time = vos_timer_get_system_time();
             break;
        case e_SME_DISASSOC_COMPLETE:
             gRoamDelayMetaInfo.disassoc_comp_time = vos_timer_get_system_time();
             break;
        case e_LIM_ADD_BS_REQ:
             gRoamDelayMetaInfo.lim_add_bss_req_time = vos_timer_get_system_time();
             break;
        case e_LIM_ADD_BS_RSP:
             gRoamDelayMetaInfo.lim_add_bss_rsp_time = vos_timer_get_system_time();
             break;
        case e_HDD_ENABLE_TX_QUEUE:
             gRoamDelayMetaInfo.hdd_monitor_tx = MONITOR_START;
             gRoamDelayMetaInfo.enable_tx_queues_reassoc_time = vos_timer_get_system_time();
             break;
        case e_HDD_SET_PTK_REQ:
             gRoamDelayMetaInfo.set_ptk_roam_key_time = vos_timer_get_system_time();
             break;
        case e_HDD_SET_GTK_REQ:
             gRoamDelayMetaInfo.set_gtk_roam_key_time = vos_timer_get_system_time();
             break;
        case e_HDD_SET_PTK_RSP:
             gRoamDelayMetaInfo.complete_ptk_roam_key_time = vos_timer_get_system_time();
             //vos_mem_copy(&gRoamDelayMetaInfo.peer_mac_addr, pBuff, buff_len);
             break;
        case e_HDD_SET_GTK_RSP:
             gRoamDelayMetaInfo.complete_gtk_roam_key_time = vos_timer_get_system_time();
             break;
        case e_TL_FIRST_XMIT_TIME:
             if(gRoamDelayMetaInfo.log_tl)
             {
                 gRoamDelayMetaInfo.tl_fetch_pkt_time = vos_timer_get_system_time();
                 gRoamDelayMetaInfo.log_tl = VOS_FALSE;
             }
             break;
        case e_HDD_FIRST_XMIT_TIME:
             if(gRoamDelayMetaInfo.hdd_monitor_tx != MONITOR_STOP)
             {
                 struct sk_buff *skb = (struct sk_buff *)pBuff;
                 if(!skb)
                 {
                     VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                "event e_HDD_FIRST_XMIT_TIME skb is null");
                     return;
                 }
                 if((gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_RSN_PSK) ||
                    (gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_WPA_PSK))
                 {
                     //Hdd xmit will have only 802.3 pkt so offset will pass as accordingly
                     if(vos_skb_is_eapol(skb, VOS_ETHERTYPE_802_1_X_FRAME_OFFSET_IN_802_3_PKT,
                            VOS_ETHERTYPE_802_1_X_SIZE) == VOS_TRUE)
                     {
                          if(gRoamDelayMetaInfo.hdd_eapol_m2 == 0)
                          {
                              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"HDD XMIT m2");
                              gRoamDelayMetaInfo.hdd_eapol_m2 = vos_timer_get_system_time();
                              gRoamDelayMetaInfo.dxe_monitor_tx = MONITOR_START;
                          }
                          else if((gRoamDelayMetaInfo.hdd_eapol_m2) && (gRoamDelayMetaInfo.hdd_eapol_m4 == 0))
                         {
                              gRoamDelayMetaInfo.hdd_eapol_m4 = vos_timer_get_system_time();
                              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"HDD XMIT m4");
                              gRoamDelayMetaInfo.hdd_monitor_tx = MONITOR_EAPOL_DONE;
                              //We should return from here so can cache the time for first data pkt
                              return;
                         }
                     }
                 }
                 else
                 {
                    gRoamDelayMetaInfo.hdd_monitor_tx = MONITOR_EAPOL_DONE;
                    gRoamDelayMetaInfo.dxe_monitor_tx = MONITOR_START;
                 }
                 //Eapol is done it must be first data frame capture it
                 if(gRoamDelayMetaInfo.hdd_monitor_tx == MONITOR_EAPOL_DONE)
                 {
                     gRoamDelayMetaInfo.hdd_first_pkt_len = 50;
                     gRoamDelayMetaInfo.hdd_first_xmit_time = vos_timer_get_system_time();
                     gRoamDelayMetaInfo.log_tl = VOS_TRUE;
                     gRoamDelayMetaInfo.hdd_monitor_tx = MONITOR_STOP;
                     VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                "HDD %s XMIT first data frame after roaming", __func__);
                     if(skb->len < gRoamDelayMetaInfo.hdd_first_pkt_len)
                         gRoamDelayMetaInfo.hdd_first_pkt_len = skb->len;
                     vos_mem_copy(&gRoamDelayMetaInfo.hdd_first_pkt_data,
                                  skb->data,gRoamDelayMetaInfo.hdd_first_pkt_len);
                 }
             }
             break;
        case e_HDD_RX_PKT_CBK_TIME:
             if(gRoamDelayMetaInfo.hdd_monitor_rx != MONITOR_STOP)
             {
                 struct sk_buff *skb = (struct sk_buff *)pBuff;
                 if(!skb)
                 {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                               "event e_HDD_RX_PKT_CBK_TIME skb is null");
                    return;
                 }
                 if((gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_RSN_PSK) ||
                    (gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_WPA_PSK))
                 {
                     if(vos_skb_is_eapol(skb, VOS_ETHERTYPE_802_1_X_FRAME_OFFSET_IN_802_3_PKT,
                            VOS_ETHERTYPE_802_1_X_SIZE) == VOS_TRUE)
                     {
                         if(gRoamDelayMetaInfo.hdd_eapol_m1 == 0)
                         {
                             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"HDD recv m1");
                             gRoamDelayMetaInfo.hdd_eapol_m1 = vos_timer_get_system_time();
                         }
                         else if((gRoamDelayMetaInfo.hdd_eapol_m1) && (gRoamDelayMetaInfo.hdd_eapol_m3 == 0))
                         {
                             gRoamDelayMetaInfo.hdd_eapol_m3 = vos_timer_get_system_time();
                             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"HDD recv m3");
                             gRoamDelayMetaInfo.hdd_monitor_rx = MONITOR_EAPOL_DONE;
                         }
                     }
                 }
                 else
                 {
                     gRoamDelayMetaInfo.hdd_monitor_rx = MONITOR_EAPOL_DONE;
                 }
                 if(gRoamDelayMetaInfo.hdd_monitor_rx == MONITOR_EAPOL_DONE)
                 {
                     gRoamDelayMetaInfo.hdd_monitor_rx = MONITOR_STOP;
                 }
             }
             break;
        case e_DXE_RX_PKT_TIME:
             if(gRoamDelayMetaInfo.dxe_monitor_rx != MONITOR_STOP)
             {
                 vos_pkt_t *vos_pkt = NULL;
                 struct sk_buff *skb = NULL;
                 vos_pkt = (vos_pkt_t *)pBuff;
                 if(!vos_pkt)
                 {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                               "event e_DXE_RX_PKT_TIME vos_pkt is null");
                    return;
                 }
                 skb = vos_pkt->pSkb;
                 if(!skb)
                 {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                               "event e_DXE_RX_PKT_TIME skb is null");
                    return;
                 }
                 //DXE can RECV MGMT and DATA frame, we are interetsed in only DATA frame
                 if(buff_len & VOS_MAC_DATA_FRAME)
                 {
                     if((gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_RSN_PSK) ||
                        (gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_WPA_PSK))
                     {
                         if(vos_skb_is_eapol(skb, VOS_ETHERTYPE_802_1_X_FRAME_OFFSET_IN_802_11_PKT,
                                VOS_ETHERTYPE_802_1_X_SIZE) == VOS_TRUE)
                         {
                             if(gRoamDelayMetaInfo.dxe_eapol_m1 == 0)
                             {
                                 VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"DXE recv m1");
                                 gRoamDelayMetaInfo.dxe_eapol_m1 = vos_timer_get_system_time();
                             }
                             else if((gRoamDelayMetaInfo.dxe_eapol_m1) && (gRoamDelayMetaInfo.dxe_eapol_m3 == 0))
                             {
                                 gRoamDelayMetaInfo.dxe_eapol_m3 = vos_timer_get_system_time();
                                 VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"DXE recv m3");
                                 gRoamDelayMetaInfo.dxe_monitor_rx = MONITOR_EAPOL_DONE;
                             }
                         }
                     }
                     else
                     {
                         gRoamDelayMetaInfo.dxe_monitor_rx = MONITOR_EAPOL_DONE;
                     }
                     if(gRoamDelayMetaInfo.dxe_monitor_rx == MONITOR_EAPOL_DONE)
                     {
                         gRoamDelayMetaInfo.dxe_monitor_rx = MONITOR_STOP;
                     }
                 }
                 /*
                 else
                 {
                     printk("e_DXE_RX_PKT_TIME dump mgmt frames");
                     vos_trace_hex_dump( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, &skb->data[0], skb->len);
                 }
                 */
             }
             break;
        case e_DXE_FIRST_XMIT_TIME:
             if(gRoamDelayMetaInfo.dxe_monitor_tx != MONITOR_STOP)
             {
                 vos_pkt_t *vos_pkt = NULL;
                 struct sk_buff *skb = NULL;
                 vos_pkt = (vos_pkt_t *)pBuff;
                 if(!vos_pkt)
                 {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                               "event e_DXE_FIRST_XMIT_TIME vos_pkt is null");
                    return;
                 }
                 skb = vos_pkt->pSkb;
                 if(!skb)
                 {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                               "event e_DXE_FIRST_XMIT_TIME skb is null");
                    return;
                 }
                 //DXE can Txmit MGMT and DATA frame, we are interetsed in only DATA frame
                 if(buff_len & VOS_MAC_DATA_FRAME)
                 {
                    if((gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_RSN_PSK) ||
                       (gRoamDelayMetaInfo.hdd_auth_type == eVOS_AUTH_TYPE_WPA_PSK))
                    {
                        if(vos_skb_is_eapol(skb, VOS_ETHERTYPE_802_1_X_FRAME_OFFSET_IN_802_11_PKT,
                               VOS_ETHERTYPE_802_1_X_SIZE) == VOS_TRUE)
                        {
                             if(gRoamDelayMetaInfo.dxe_eapol_m2 == 0)
                             {
                                 VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"DXE XMIT m2");
                                 gRoamDelayMetaInfo.dxe_eapol_m2 = vos_timer_get_system_time();
                             }
                             else if((gRoamDelayMetaInfo.dxe_eapol_m2) && (gRoamDelayMetaInfo.dxe_eapol_m4 == 0))
                            {
                                 gRoamDelayMetaInfo.dxe_eapol_m4 = vos_timer_get_system_time();
                                 VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"DXE XMIT m4");
                                 gRoamDelayMetaInfo.dxe_monitor_tx = MONITOR_EAPOL_DONE;
                                 //We should return from here so can cache the time for first data pkt
                                 return;
                            }
                        }
                    }
                    else
                    {
                        gRoamDelayMetaInfo.dxe_monitor_tx = MONITOR_EAPOL_DONE;
                    }
                    //HACK buff len is getting used as FRAME TYPE
                    if(gRoamDelayMetaInfo.dxe_monitor_tx == MONITOR_EAPOL_DONE)
                    {
                        gRoamDelayMetaInfo.dxe_first_tx_time = vos_timer_get_system_time();
                        gRoamDelayMetaInfo.dxe_monitor_tx = MONITOR_STOP;
                        gRoamDelayMetaInfo.dxe_first_pkt_len = 75;
                        if(skb->len < gRoamDelayMetaInfo.dxe_first_pkt_len)
                            gRoamDelayMetaInfo.dxe_first_pkt_len = skb->len;
                        vos_mem_copy(&gRoamDelayMetaInfo.dxe_first_pkt_data, skb->data,
                                     gRoamDelayMetaInfo.dxe_first_pkt_len);
                        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                   "DXE %s XMIT first data frame after roaming", __func__);
                    }
                 }
             }
             break;
        case e_SME_VO_ADDTS_REQ:
             gRoamDelayMetaInfo.hdd_addts_vo_req_time = vos_timer_get_system_time();
             break;
        case e_SME_VO_ADDTS_RSP:
             gRoamDelayMetaInfo.hdd_addts_vo_rsp_time = vos_timer_get_system_time();
             break;
        case e_SME_VI_ADDTS_REQ:
             gRoamDelayMetaInfo.hdd_addts_vi_req_time = vos_timer_get_system_time();
             break;
        case e_SME_VI_ADDTS_RSP:
             gRoamDelayMetaInfo.hdd_addts_vi_rsp_time = vos_timer_get_system_time();
             break;
        case e_CACHE_ROAM_DELAY_DATA:
             //Let us copy roam meta info
             if(gRoamDelayCurrentIndex > ROAM_DELAY_TABLE_SIZE)
                 gRoamDelayCurrentIndex = 0;
             vos_mem_copy(&gpRoamDelayTable[gRoamDelayCurrentIndex++],
                          &gRoamDelayMetaInfo, sizeof(gRoamDelayMetaInfo));
             vos_mem_set(&gRoamDelayMetaInfo, sizeof(gRoamDelayMetaInfo), 0);
             break;
        default:
             VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                       "%s Invalid roam_event = %d received ", __func__, roam_event);
             break;
    }
}

void vos_reset_roam_timer_log(void)
{
    if (gpRoamDelayTable != NULL)
    {
       //Set zero to whole gpRoamDelayTable
       vos_mem_set(gpRoamDelayTable, (sizeof(tRoamDelayMetaInfo) * ROAM_DELAY_TABLE_SIZE), 0);
    }
}

void vos_dump_roam_time_log_service(void)
{
    v_SLONG_t slA, slB, slC, slD, slE, slF, slG, slH, slI, slJ, slK, slL, slM, slRoamDelay;
    tRoamDelayMetaInfo currentRoamDelayInfo;
    v_ULONG_t index = 0,i=0;

    if (gpRoamDelayTable == NULL)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "Roam delay table is not initialized\n");
        return;
    }
    //Let us first copy the current gRoamDelayMetaInfo into gpRoamDelayTable
    if(gRoamDelayCurrentIndex > ROAM_DELAY_TABLE_SIZE)
        gRoamDelayCurrentIndex = 0;
    vos_mem_copy(&gpRoamDelayTable[gRoamDelayCurrentIndex++], &gRoamDelayMetaInfo, sizeof(gRoamDelayMetaInfo));

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "** RoamDelay = ( dxe_first_tx_time - disable_tx_queues_time)\n");

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "||========================"
         "===============|====== A ======|====== B ======|====== C ======|"
         "====== D ======|====== E ======|====== F ======|====== G ======|"
         "====== H ======|====== I ======|====== J ======|====== K ======|"
         "====== L ======|====== M ======||\n");

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "||Sl | Peer MAC address  |"
         " **RoamDelay** | PreAuth Timer | Disassoc Issue| Add BSS Req   |"
         " AddBssRsp to  | ReassocReq to | ReassocRsp to | Disable to    |"
         " M1-M2 DXE SW  | M1-M2 HDD SW  | M3-M4 DXE SW  | M3-M4 HDD SW  |"
         " ReassocRsp to | HDD to DXE    ||\n");

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "||No.|                   |"
         " ************* | to Roam Start | to Complete   | to Rsp time   |"
         " Reassoc Req   | ReassocRsp    | Enable Tx Que | Enable Tx Que |"
         "               |               |               |               |"
         " Set GTK       | 1st data frame||\n");

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "||========================"
         "================================================================"
         "================================================================"
         "================================================================"
         "===============================||\n");

    for (index = 0; index < gRoamDelayCurrentIndex; index++)
    {
        currentRoamDelayInfo = gpRoamDelayTable[index];
        /* PreAuth Timer to Roam Start */
        slA = (currentRoamDelayInfo.preauth_cb_time -
               currentRoamDelayInfo.preauth_reassoc_start_time);

        /* Disassoc Issue to Complete */
        slB = (currentRoamDelayInfo.disassoc_comp_time -
               currentRoamDelayInfo.disassoc_issue_time);

        /* Add BSS Req to Rsp time */
        slC = (currentRoamDelayInfo.lim_add_bss_rsp_time -
               currentRoamDelayInfo.lim_add_bss_req_time);

        /* AddBssRsp to Reassoc Req */
        slD = (currentRoamDelayInfo.send_reassoc_req_time -
               currentRoamDelayInfo.lim_add_bss_rsp_time);

        /* ReassocReq to ReassocRsp */
        slE = (currentRoamDelayInfo.hdd_sendassoc_rsp_time -
               currentRoamDelayInfo.send_reassoc_req_time);

        /* ReassocRsp to Enable Tx Que */
        slF = (currentRoamDelayInfo.enable_tx_queues_reassoc_time -
               currentRoamDelayInfo.hdd_sendassoc_rsp_time);

        /* Disable to Enable Tx Que */
        slG = (currentRoamDelayInfo.enable_tx_queues_reassoc_time -
               currentRoamDelayInfo.disable_tx_queues_time);

        /* M1-M2 DXE SW */
        slH = (currentRoamDelayInfo.dxe_eapol_m2 -
               currentRoamDelayInfo.dxe_eapol_m1);

        /* M1-M2 HDD SW */
        slI = (currentRoamDelayInfo.hdd_eapol_m2 -
               currentRoamDelayInfo.hdd_eapol_m1);

        /* M3-M4 DXE SW */
        slJ = (currentRoamDelayInfo.dxe_eapol_m4 -
               currentRoamDelayInfo.dxe_eapol_m3);

        /* M3-M4 HDD SW */
        slK = (currentRoamDelayInfo.hdd_eapol_m4 -
               currentRoamDelayInfo.hdd_eapol_m3);

        /* ReassocRsp to Set GTK */
        slL = (currentRoamDelayInfo.set_gtk_roam_key_time -
               currentRoamDelayInfo.hdd_sendassoc_rsp_time);

        /* HDD to DXE 1st data frame */
        slM = (currentRoamDelayInfo.dxe_first_tx_time -
               currentRoamDelayInfo.hdd_first_xmit_time);

        /* Calculate roam delay */
        slRoamDelay = (currentRoamDelayInfo.dxe_first_tx_time -
                       currentRoamDelayInfo.disable_tx_queues_time);

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||%2ld:|<"MAC_ADDRESS_STR">|"
                "%14ld |%14ld |%14ld |%14ld |"
                "%14ld |%14ld |%14ld |%14ld |"
                "%14ld |%14ld |%14ld |%14ld |"
                "%14ld |%14ld ||\n",
                (index+1), MAC_ADDR_ARRAY(currentRoamDelayInfo.peer_mac_addr),
                slRoamDelay, slA, slB, slC,
                slD, slE, slF, slG,
                slH, slI, slJ, slK,
                slL, slM );
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "||========================"
         "================================================================"
         "================================================================"
         "================================================================"
         "===============================||\n");

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "||== More Details ====================="
         "===============================||\n");

    for (index = 0; index < gRoamDelayCurrentIndex; index++)
    {

        currentRoamDelayInfo = gpRoamDelayTable[index];
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||%2ld: Peer Mac: <"MAC_ADDRESS_STR">\n",
                (index+1), MAC_ADDR_ARRAY(currentRoamDelayInfo.peer_mac_addr)
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||preauth_reassoc_start_time : %14ld\n",
                currentRoamDelayInfo.preauth_reassoc_start_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||preauth_cb_time            : %14ld\n",
                currentRoamDelayInfo.preauth_cb_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||disable_tx_queues_time     : %14ld\n",
                currentRoamDelayInfo.disable_tx_queues_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||disassoc_issue_time        : %14ld\n",
                currentRoamDelayInfo.disassoc_issue_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||lim_add_bss_req_time       : %14ld\n",
                currentRoamDelayInfo.lim_add_bss_req_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||lim_add_bss_rsp_time       : %14ld\n",
                currentRoamDelayInfo.lim_add_bss_rsp_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||disassoc_comp_time         : %14ld\n",
                currentRoamDelayInfo.disassoc_comp_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||send_reassoc_req_time      : %14ld\n",
                currentRoamDelayInfo.send_reassoc_req_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||hdd_sendassoc_rsp_time     : %14ld\n",
                currentRoamDelayInfo.hdd_sendassoc_rsp_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||enable_tx_queues_time      : %14ld\n",
                currentRoamDelayInfo.enable_tx_queues_reassoc_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||hdd_eapol_m1               : %14ld\n",
                currentRoamDelayInfo.hdd_eapol_m1
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||hdd_eapol_m2               : %14ld\n",
                currentRoamDelayInfo.hdd_eapol_m2
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||hdd_eapol_m3               : %14ld\n",
                currentRoamDelayInfo.hdd_eapol_m3
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||hdd_eapol_m4               : %14ld\n",
                currentRoamDelayInfo.hdd_eapol_m4
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||dxe_eapol_m1               : %14ld\n",
                currentRoamDelayInfo.dxe_eapol_m1
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||dxe_eapol_m2               : %14ld\n",
                currentRoamDelayInfo.dxe_eapol_m2
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||dxe_eapol_m3               : %14ld\n",
                currentRoamDelayInfo.dxe_eapol_m3
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||dxe_eapol_m4               : %14ld\n",
                currentRoamDelayInfo.dxe_eapol_m4
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||set_ptk_roam_key_time      : %14ld\n",
                currentRoamDelayInfo.set_ptk_roam_key_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||set_gtk_roam_key_time      : %14ld\n",
                currentRoamDelayInfo.set_gtk_roam_key_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||tl_fetch_pkt_time          : %14ld\n",
                currentRoamDelayInfo.tl_fetch_pkt_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||hdd_first_xmit_time        : %14ld\n",
                currentRoamDelayInfo.hdd_first_xmit_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||dxe_first_tx_time          : %14ld\n",
                currentRoamDelayInfo.dxe_first_tx_time
                );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||hdd_first_pkt_data         : \n"
                );


        for (i=0; i<currentRoamDelayInfo.hdd_first_pkt_len && i< (50-8); i+=8)
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "||%2X, %2X, %2X, %2X, %2X, %2X, %2X, %2X, ",
                    currentRoamDelayInfo.hdd_first_pkt_data[i],
                    currentRoamDelayInfo.hdd_first_pkt_data[i+1],
                    currentRoamDelayInfo.hdd_first_pkt_data[i+2],
                    currentRoamDelayInfo.hdd_first_pkt_data[i+3],
                    currentRoamDelayInfo.hdd_first_pkt_data[i+4],
                    currentRoamDelayInfo.hdd_first_pkt_data[i+5],
                    currentRoamDelayInfo.hdd_first_pkt_data[i+6],
                    currentRoamDelayInfo.hdd_first_pkt_data[i+7]
                    );

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "||dxe_first_pkt_data         : \n"
                );

        for (i=0; i<currentRoamDelayInfo.dxe_first_pkt_len && i < (75-8); i+=8)
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "||%2X, %2X, %2X, %2X, %2X, %2X, %2X, %2X, ",
                    currentRoamDelayInfo.dxe_first_pkt_data[i],
                    currentRoamDelayInfo.dxe_first_pkt_data[i+1],
                    currentRoamDelayInfo.dxe_first_pkt_data[i+2],
                    currentRoamDelayInfo.dxe_first_pkt_data[i+3],
                    currentRoamDelayInfo.dxe_first_pkt_data[i+4],
                    currentRoamDelayInfo.dxe_first_pkt_data[i+5],
                    currentRoamDelayInfo.dxe_first_pkt_data[i+6],
                    currentRoamDelayInfo.dxe_first_pkt_data[i+7]
                    );
    }
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
         "||== END ====================="
         "===============================||\n");
}
