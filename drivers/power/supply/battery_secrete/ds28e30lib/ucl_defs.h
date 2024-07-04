
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
*     Module Name: UCL
*     Description: UCL usual definition
*        Filename: ucl_defs.h
*          Author: LSL
*        Compiler: gcc
*
 *******************************************************************************
 */
#ifndef _UCL_DEFS_H_
#define _UCL_DEFS_H_

/** @defgroup UCL_DEFINITIONS Definitions
 *
 */


/** @defgroup UCL_DEFINES Defines
 *
 * @ingroup UCL_DEFINITIONS
 */

#ifndef NULL
#define NULL 0x0
#endif


#include <linux/memory.h>
/*
 * Algorithm masks and types.
 */
#define UCL_ALG_TYPE_MASK   0x000000ff
#define UCL_ALG_TYPE_CIPHER   0x00000001
#define UCL_ALG_TYPE_HASH   0x00000002
#define UCL_ALG_TYPE_COMPRESS  0x00000004
#define UCL_ALG_TYPE_MAC   0x00000008

/*
 * Transform masks and values.
 */
#define UCL_FLAG_MODE_MASK   0x000000ff
#define UCL_FLAG_REQ_MASK   0x000fff00
#define UCL_FLAG_RES_MASK   0xfff00000

#define UCL_FLAG_MODE_ECB   0x00000001
#define UCL_FLAG_MODE_CBC   0x00000002
#define UCL_FLAG_MODE_CFB   0x00000004
#define UCL_FLAG_MODE_CTR   0x00000008
#define UCL_FLAG_MODE_OFB   0x00000010

#define UCL_FLAG_REQ_WEAK_KEY  0x00000100

#define UCL_FLAG_RES_WEAK_KEY  0x00100000
#define UCL_FLAG_RES_BAD_KEYLEN    0x00200000
#define UCL_FLAG_RES_BAD_KEYSCHED  0x00400000
#define UCL_FLAG_RES_BAD_BLOCKLEN  0x00800000
#define UCL_FLAG_RES_BAD_FLAGS   0x01000000

/*==============================================================================
 * CIPHER
 *============================================================================*/
/** <b>Encryption Mode</b>.
 * Cipher in Encryption Mode.
 *
 * @ingroup UCL_BLOCK_CIPHER
 */
#define UCL_CIPHER_ENCRYPT 0x0
/** <b>Decryption Mode</b>.
 * Cipher in Decryption Mode.
 *
 * @ingroup UCL_BLOCK_CIPHER
 */
#define UCL_CIPHER_DECRYPT 0x1

/** <b>EEE Encryption Mode</b>.
 * Cipher in Encryption Mode for EEE mode.
 *
 * @ingroup UCL_BLOCK_CIPHER
 */
#define UCL_CIPHER_ENCRYPT_EEE 0x2
/** <b>DDD Decryption Mode</b>.
 * Cipher in Decryption Mode for EEE mode
 *
 * @ingroup UCL_BLOCK_CIPHER
 */
#define UCL_CIPHER_DECRYPT_EEE 0x3

#define UCL_CIPHER_MODE_LAST	UCL_CIPHER_DECRYPT_EEE

/*==============================================================================
 * ASN1
 *============================================================================*/
#define UCL_ASN1_ID_MD5_SIZE 18
#define UCL_ASN1_ID_SHA256_SIZE 19
#define UCL_ASN1_ID_SHA1_SIZE 15
#define UCL_ASN1_ID_RIPEMD160_SIZE 18


/*==============================================================================
 * Big Number
 *============================================================================*/
/* <b>Sign</b>.
 * The value of an negative integer last word.
 * For complement representation of big integer.
 *
 * @ingroup UCL_DEFINES
 */
#define SIGNED 0xFFFFFFFF

/** The precision.
 * @ingroup UCL_FPA
 */
#define UCL_FPA_PRECISION 65
/** The precision for unsigned large integer.
 * @ingroup UCL_FPA
 */
#define UCL_FPA_UPRECISION (UCL_FPA_PRECISION - 1)
/** Double precision.
 * @ingroup UCL_FPA
 */
#define UCL_FPA_DB_PRECISION (2*UCL_FPA_PRECISION)
/** Half precision.
 * @ingroup UCL_FPA
 */
#define UCL_FPA_HF_PRECISION (UCL_FPA_PRECISION / 2)
/** Maximum precision.
 * @ingroup UCL_FPA
 */
#define UCL_FPA_MAX_PRECISION (UCL_FPA_DB_PRECISION + 2)

/*
1    1         0
5432109876543210
             HHH
*/
#define UCL_NO_INPUT 0
#define UCL_HASH_INPUT 1
#define UCL_MSG_INPUT 2
#define UCL_NO_PRECOMP 0
#define UCL_R_PRECOMP 1
#define UCL_PRECOMP_R 2
#define UCL_NO_PRECOMP_TRICK 0
#define UCL_PRECOMP_TRICK 1
#define UCL_PRECOMP_FULL_TRICK 2
#define UCL_PRECOMP_MASK 7//so 3 bits: 9,10,11
#define UCL_PRECOMP_TRICK_MASK 7// so 3 bits: 12,13,14
#define UCL_INPUT_MASK 3//so 2 bits: 7,8
#define UCL_CURVE_MASK 15//so 4 bits: 3,4,5,6
#define UCL_HASH_MASK 7//so 3 bits: 0,1,2
#define UCL_HASH_SHIFT 0
#define UCL_CURVE_SHIFT 3
#define UCL_INPUT_SHIFT 7
#define UCL_PRECOMP_SHIFT 9
#define UCL_PRECOMP_TRICK_SHIFT 12

#endif /*_UCL_DEFS_H_*/
