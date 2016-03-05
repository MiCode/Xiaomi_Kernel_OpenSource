/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

                Copyright (c) 2007 QUALCOMM Incorporated.
                All Rights Reserved.
                Qualcomm Confidential and Proprietary
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

#include <linux/err.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/completion.h>
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
    *phCryptProv  = (v_U32_t)NULL;
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
    memcpy(hash_buff, plaintext, psize);
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
    memcpy(hash_buff, plaintext, psize);
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
