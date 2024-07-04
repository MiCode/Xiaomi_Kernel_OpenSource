
/******************************************************************************* 
* Copyright (C) 2015 Maxim Integrated Products, Inc., All rights Reserved.
* * This software is protected by copyright laws of the United States and
* of foreign countries. This material may also be protected by patent laws
* and technology transfer regulations of the United States and of foreign
* countries. This software is furnished under a license agreement and/or a
* nondisclosure agreement and may only be used or reproduced in accordance
* with the terms of those agreements. Dissemination of this information to
* any party or parties not specified in the license agreement and/or
* nondisclosure agreement is expressly prohibited.
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
* 
* Except as contained in this notice, the name of Maxim Integrated 
* Products, Inc. shall not be used except as stated in the Maxim Integrated 
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all 
* ownership rights.
*     Module Name: SHA256
*     Description: sha256 definition
*        Filename: ucl_sha256.h
*          Author: LSL
*        Compiler: gcc
*
 *******************************************************************************
 */

#define __API__ 

#ifndef _UCL_SHA256_H_
#define _UCL_SHA256_H_
#include "ucl_hash.h"
#ifdef HASH_SHA256

#ifdef __cplusplus
extern "C" {
#endif /* _ cplusplus  */

/** @file ucl_sha256.h
 * @defgroup UCL_SHA256 SHA256
 * Secure Hash Algorithm 256, from FIPS 180-2.
 *
 * @par Header:
 * @link ucl_sha256.h ucl_sha256.h @endlink
 *
 * SHA-256 is a data-digest algorithm and a component of
 * the Standard FIPS 180-2.
 * The algorithm takes as input a data of arbitrary length and
 * produces as output a 256-bit "fingerprint" or "data digest"
 * of the input.@n
 * @n
 * Given a data of any length, a single "1" bit is appended at the end of the
 * data. This bit must always be added. Then, a set of "0" bits are also
 * appended so that the data length is 448 modulo 512. Finally, the length
 * of the data is appended. @n
 *@n
 *
 * <b>SHA256 Descriptor:</b> @n
 * @li Block length : 512 bits
 * @li Hash length : 256 bits
 *
 * @ingroup UCL_HASH
 */


/** <b>The SHA256 context</b>.
 * This structure is associated to the 'step by step' process.
 *
 * @ingroup UCL_SHA256
 */

struct ucl_sha256_ctx
{
    /** Intermediate and then final hash. */
    u32 state[8];
    /** Counter in bits. */
    u32 count[2];
    /** Buffer. */
    u8 buffer[64];
};

/** <b>The SHA256 context</b>.
 * @ingroup UCL_SHA256
 */

typedef struct ucl_sha256_ctx ucl_sha256_ctx_t;


/** <b>Core block size</b>.
 * Byte size of a SHA256 core block.
 *
 * @ingroup UCL_SHA256
 */
#define UCL_SHA256_BLOCKSIZE 64
/** <b>Hash size</b>.
 * Byte size of the output of SHA256.
 *
 * @ingroup UCL_SHA256
 */
#define UCL_SHA256 1
#define UCL_SHA256_HASHSIZE 32
/** <b>Hash size</b>.
 * 32-bits word size of the output of SHA256.
 *
 * @ingroup UCL_SHA256
 */
#define UCL_SHA256_HASHW32SIZE 8


/** <b>SHA256</b>.
 * The complete process of SHA256.
 *
 * @pre Hash byte length is equal to 32
 *
 * @param[out] hash         Pointer to the digest
 * @param[in]  data         Pointer to the data
 * @param[in]  data_byteLen Data byte length
 *
 * @return Error code
 *
 * @retval #UCL_OK             if no error occurred
 * @retval #UCL_INVALID_OUTPUT if @p hash is #NULL
 *
 * @ingroup UCL_SHA256
 */
int __API__ ucl_sha256(u8 *hash, u8 *data, u32 data_byteLen);


/** <b>SHA256 Init</b>.
 * The initialisation of SHA256.
 *
 * @param[in,out] context Pointer to the context
 *
 * @return Error code
 *
 * @retval #UCL_OK            if no error occurred
 * @retval #UCL_INVALID_INPUT if @p context is #NULL
 *
 * @ingroup UCL_SHA256
 */
int __API__ ucl_sha256_init(ucl_sha256_ctx_t *context);

/** <b>SHA256 Core</b>.
 * The core of SHA256.
 *
 * @param[in,out] context      Pointer to the context
 * @param[in]     data         Pointer to the data
 * @param[in]     data_byteLen Data byte length
 *
 * @warning #ucl_sha256_init must be processed before, and
 *          #ucl_sha256_finish should be processed to get the final hash.
 *
 * @return Error code
 *
 * @retval #UCL_OK  if no error occurred
 * @retval #UCL_NOP if @p data_byteLen = 0 or if @p data is the pointer #NULL
 *
 * @ingroup UCL_SHA256
 */
int __API__ ucl_sha256_core(ucl_sha256_ctx_t *context, u8 *data,
                    u32 data_byteLen);

/** <b>SHA256 Finish</b>.
 * Finish the process of SHA256.
 *
 * @pre Hash byte length is equal to 32
 *
 * @param[out]    hash Pointer to the digest
 * @param[in,out] context Pointer to the context
 *
 * @warning #ucl_sha256_init and #ucl_sha256_core must be processed before.
 *
 * @return Error code
 *
 * @retval #UCL_OK             if no error occurred
 * @retval #UCL_INVALID_OUTPUT if @p hash is the pointer #NULL
 * @retval #UCL_INVALID_INPUT  if @p context is the pointer #NULL
 *
 * @ingroup UCL_SHA256
 */
int __API__ ucl_sha256_finish(u8 *hash, ucl_sha256_ctx_t *context);


#ifdef __cplusplus
}
#endif /* _ cplusplus  */
#endif//HASH_SHA256
#endif /* _UCL_SHA256_H_ */
