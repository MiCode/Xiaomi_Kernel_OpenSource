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

/*
 * Qualcomm Inc, proprietary. All rights reserved.
 * Ref File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/ssm/include/aniSsmAesKeyWrap.h $
 *
 * Contains SSM-private declarations related to the AES key WRAP
 * algorithm described in RFC 3394.
 *
 * Author:      Arul V Raj
 * Date:        27-February-2009
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */

#ifndef _ANI_SSM_AES_KEY_WRAP_H_
#define _ANI_SSM_AES_KEY_WRAP_H_

#define ANI_SSM_AES_KEY_WRAP_BLOCK_SIZE 8 // Bytes
#define AES_BLOCK_SIZE 16 // Bytes

typedef union uAniU32ValAry{
    tANI_U32 val;
    char ary[sizeof(tANI_U32)];
} tAniU32ValAry;

/**
 * Implements the AES Key Wrap algorithm described in RFC 3394.
 * If n is the number of blocks in plainText, of size
 * ANI_SSM_AES_KEY_WRAP_BLOCK_SIZE, then the output value is (n+1)
 * blocks. The first block is the IV from section 2.2.3 o the
 * RFC. Note: It is the caller's responsibility to free the returned
 * value.
 * 
 * @param plainText the plaintext data to wrap
 * @param len the length of the plaintext, which must be a multiple of
 * ANI_SSM_AES_KEY_WRAP_BLOCK_SIZE.
 * @param keyEncKey the encryption key
 * @param keyEncKeyLen the length of keyEncKey
 * @param cipherTextPtr is set to a newly allocated array containing
 * the result if the operation succeeds. It is the caller's
 * responsibility to free this.
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniSsmAesKeyWrap(v_U32_t cryptHandle, tANI_U8 *plainText, tANI_U32 len,
                 tANI_U8 *keyEncKey, tANI_U32 keyEncKeyLen,
                 tANI_U8 **cipherTextPtr);

/**
 * Implements the AES Key Unwrap algorithm described in RFC 3394.
 * If (n+1) is the number of blocks in cipherText, of size
 * ANI_SSM_AES_KEY_WRAP_BLOCK_SIZE, then the output value is (n+1)
 * blocks. The actual plaintext consists of n blocks that start at the
 * second block. Note: It is the caller's responsibility to free the
 * returned value.
 *
 * @param cipherText the cipertext data to unwrap
 * @param len the length of the ciphertext, which must be a multiple of
 * ANI_SSM_AES_KEY_WRAP_BLOCK_SIZE.
 * @param keyEncKey the encryption key
 * @param keyEncKeyLen the length of keyEncKey
 * @param plainTextPtr is set to a newly allocated array containing
 * the result if the operation succeeds. It is the caller's
 * responsibility to free this.
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniSsmAesKeyUnwrap(v_U32_t cryptHandle, tANI_U8 *cipherText, tANI_U32 len,
                   tANI_U8 *keyEncKey, tANI_U32 keyEncKeyLen,
                   tANI_U8 **plainTextPtr);


#endif //_ANI_SSM_AES_KEY_WRAP_H_

