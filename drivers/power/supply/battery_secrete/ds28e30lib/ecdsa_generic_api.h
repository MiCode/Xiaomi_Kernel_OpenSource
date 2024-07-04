
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
*     Module Name: ECC
*     Description: ECC definition
*        Filename: ecdsa_generic_api.h
*          Author: LSL
*        Compiler: gcc
*
 *******************************************************************************
 */

/** @brief ECDSA signatures computation and verification
* 
* Long description of the module. This module is in charge of .... 
*  * Elliptic curves have been studied by mathematicians for more than a century.
 * A rich theory have been developed around them, and cryptography have taken
 * the advantage to find a way to use it in practice.
 * Elliptic curve public key cryptosystems were proposed independently by
 * Victor Miller and Neil Koblitz in the mid-eighties. After many attempts to
 * make a cryptographic protocol with an high level of confidence,
 * the first commercial implementation is appearing in the last ten years.
 * The security of the ECC is based on difficulty of the discret logarithm
 * problem.@n
 * @n
 * <b>Principle:</b>@n
 * @n
 * An elliptic curve can be defined as an equation of the form :@n
 * @f$ E ~:~ y^{2} + a_{1}xy + a_{3}y ~=~ x^{3} + a_{2}x^{2} + a_{4}x + a_{6} @f$ @n
 * For cryptographic purpose @a E is defined on GF(p) or GF(2^m).
 * @n
 * If the characteristic of the curve is different from 2 or 3, then an
 * admissible change of variable transforms the equation to :@n
 * @f$ E ~:~ y^{2}~=~x^3+a.x+b @f$ @n
 * If the characteristic of the curve is  2 then an
 * admissible change of variable transforms the equation to :@n
 * @f$ E ~:~ y^{2} + x.y ~=~x^3+ a.x^2 + b @f$ @n
 * @n
 * A point of the curve is a couple @f$ (x,y) @f$  verifying the equation @a E. @n
 * @n
 * It exists an addition on @a E such as @a E is a group.
 * If @a P is point of @a E, then we note @f$ Q = k.P @f$ the result of @p k 
 * successive additions. @n
 * The discrete logarithm problem is the problem which consists to find @p k 
 * from @p Q and @p P.
 * ECDSA is the implementation of the DSA in ECC
*/ 

#ifndef _UCL_ECDSA_GENERIC_API_NEW_H_
#define _UCL_ECDSA_GENERIC_API_NEW_H_
#include "ucl_config.h"
#include "bignum_ecdsa_generic_api.h"

/* ECDSA key lengths */
#define ECDSA_BLOCK_SIZE 32
#define WORD32
#ifdef WORD32
#define ECDSA_DIGITS 17
#endif

#define SECP192R1 0
#define SECP224R1 1
#define SECP256R1 2
#define SECP160R1 3
#define SECP384R1 4
#define SECP521R1 5
#define SM2FP192 6
#define SM2FP256 7
#define BP256R1 8
#define BP384R1 9
#define BP512R1 10
//VP for various
#define SM2VP256 11
#define UNKNOWN_CURVE 12
#define MAX_CURVE 12

#define SECP160R1_BYTESIZE 20
#define SECP192R1_BYTESIZE 24
#define SM2FP192_BYTESIZE 24
#define SECP224R1_BYTESIZE 28
#define SECP256R1_BYTESIZE 32
#define BP256R1_BYTESIZE 32
#define SM2FP256_BYTESIZE 32
#define SECP384R1_BYTESIZE 48
#define SECP521R1_BYTESIZE 66
#define BP384R1_BYTESIZE 48
#define BP512R1_BYTESIZE 64


#define SECP160R1_WORDSIZE 8
#define SECP192R1_WORDSIZE 6
#define SM2FP192_WORDSIZE 8
#define SECP224R1_WORDSIZE 8
#define SECP256R1_WORDSIZE 8
#define BP256R1_WORDSIZE 8
#define SM2FP256_WORDSIZE 8
#define SECP384R1_WORDSIZE 12
#define SECP521R1_WORDSIZE 17
#define BP384R1_WORDSIZE 12
#define BP512R1_WORDSIZE 16

//internal defines


#define P192
#define P256

#ifdef WORD32
static const u32 one[ECDSA_DIGITS]={0x00000001,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};
static const u32 two[ECDSA_DIGITS]={0x00000002,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};
static const u32 three[ECDSA_DIGITS]={0x00000003,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};
static const u32 four[ECDSA_DIGITS]={0x00000004,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};
#endif


/** <b>ECC Curve Structure</b>.
 *
 * @ingroup UCL_ECDSA
 */
typedef struct _t_curve
{
  const u32 *a;/**< curve equation a parameter.                                         */
  const u32 *b;/**< curve equation b parameter.                                         */
  const u32 *p;/**< curve equation p paramter.                                         */
  const u32 *n;/**< curve order.                                         */
  const u32 *xg;/**< curve base point x coordinate.                                         */
  const u32 *yg;/**< curve base point y coordinate.                                         */
  u32 *invp2;/**< curve field inversion of 2.                                         */
  u32 *psquare;/**< curve p parameter square.                                         */
  void *px;/**< curve precomputed x values (may be NULL) .                                         */
  void *py;/**< curve precomputed y values (may be NULL).                                         */
  u32 curve_wsize;/**< curve word size.                                         */
  u32 curve_bsize;/**< curve byte size.                                         */
  int curve;/**< curve identifier.                                         */
} ucl_type_curve;

/** <b>ECC word Jacobian point coordinates</b>.
 * this structure is used within ECC routines
 * @ingroup UCL_ECDSA
 */
typedef struct _t_jacobian_point
{
  u32 *x;/**< jacobian x coordinate.                                         */
  u32 *y;/**< jacobian y coordinate.                                         */
  u32 *z;/**< jacobian z coordinate.                                         */
} ucl_type_ecc_jacobian_point;

/** <b>ECC byte affine point coordinates</b>.
 * this structure is used at application level
 * @ingroup UCL_ECDSA
 */
typedef struct _t_u8_affine_point
{
  u8 *x;/**< affine x coordinate.                                         */
  u8 *y;/**< affine y coordinate.                                         */
} ucl_type_ecc_u8_affine_point;

/** <b>ECC word affine point coordinates</b>.
 * this structure is used within ECC routines
 *
 * @ingroup UCL_ECDSA
 */
typedef struct _t_digit_affine_point
{
  u32 *x;/**< affine x coordinate.                                         */
  u32 *y;/**< affine y coordinate.                                         */
} ucl_type_ecc_digit_affine_point;

/** <b>ECC byte signature structure</b>.
 * this structure is used at application level
 *
 * @ingroup UCL_ECDSA
 */
typedef struct _t_ecdsa_signature
{
  u8 *r;/**<signature r  value */
  u8 *s;/**<signature s value */
} ucl_type_ecdsa_signature;

#ifdef P192
  static const u32 local_inv2_p192r1[SECP192R1_WORDSIZE+2]={0x00000000,0x80000000,0xffffffff,0xffffffff,0xffffffff,0x7fffffff,0x00000000,0x00000000};
  static const u32 local_psquare_p192r1[]={0x00000001,0x00000000,0x00000002,0x00000000,0x00000001,0x00000000,0xfffffffe,0xffffffff,0xfffffffd,0xffffffff,0xffffffff,0x0fffffff,0x00000000,0x00000000,0x00000000,0x00000000};
  static const u32 local_xg_p192r1[SECP192R1_WORDSIZE+2]={0x82ff1012,0xf4ff0afd,0x43a18800,0x7cbf20eb,0xb03090f6,0x188da80e,0x00000000,0x00000000};
  static const u32 local_yg_p192r1[SECP192R1_WORDSIZE+2]={0x1e794811,0x73f977a1,0x6b24cdd5,0x631011ed,0xffc8da78,0x07192b95,0x00000000,0x00000000};
  static const u32 local_a_p192r1[SECP192R1_WORDSIZE+2]={0xfffffffc,0xffffffff,0xfffffffe,0xffffffff,0xffffffff,0xffffffff,0x00000000,0x00000000};
static const u32 local_b_p192r1[SECP192R1_WORDSIZE+2]={0xc146b9b1,0xfeb8deec,0x72243049,0x0fa7e9ab,0xe59c80e7,0x64210519,0x00000000,0x00000000};
  static const u32 local_p_p192r1[SECP192R1_WORDSIZE+2]={0xffffffff,0xffffffff,0xfffffffe,0xffffffff,0xffffffff,0xffffffff,0x00000000,0x00000000};
  static const u32 local_n_p192r1[SECP192R1_WORDSIZE+2]={0xb4d22831,0x146bc9b1,0x99def836,0xffffffff,0xffffffff,0xffffffff,0x00000000,0x00000000};
/** <b>ECC Curve structure variable for SEC-P192r1</b>.
 *
 * @ingroup UCL_ECDSA
 */
extern ucl_type_curve secp192r1;
#endif//P192

#ifdef P256
  static const u32 local_inv2_p256r1[SECP256R1_WORDSIZE]={0x00000000,0x00000000,0x80000000,0x00000000,0x00000000,0x80000000,0x80000000,0x7fffffff};
  static const u32 local_psquare_p256r1[]={0x00000001,0x00000000,0x00000000,0xfffffffe,0xffffffff,0xffffffff,0xfffffffe,0x00000001,0xfffffffe,0x00000001,0xfffffffe,0x00000001,0x00000001,0xfffffffe,0x00000002,0xfffffffe};
  static const u32 local_xg_p256r1[SECP256R1_WORDSIZE]={0xd898c296,0xf4a13945,0x2deb33a0,0x77037d81,0x63a440f2,0xf8bce6e5,0xe12c4247,0x6b17d1f2};
  static const u32 local_yg_p256r1[SECP256R1_WORDSIZE]={0x37bf51f5,0xcbb64068,0x6b315ece,0x2bce3357,0x7c0f9e16,0x8ee7eb4a,0xfe1a7f9b,0x4fe342e2};
  static const u32 local_a_p256r1[SECP256R1_WORDSIZE]={0xfffffffc,0xffffffff,0xffffffff,0x00000000,0x00000000,0x00000000,0x00000001,0xffffffff};
static const u32 local_b_p256r1[SECP256R1_WORDSIZE]={0x27d2604b,0x3bce3c3e,0xcc53b0f6,0x651d06b0,0x769886bc,0xb3ebbd55,0xaa3a93e7,0x5ac635d8};
  static const u32 local_p_p256r1[SECP256R1_WORDSIZE]={0xffffffff,0xffffffff,0xffffffff,0x00000000,0x00000000,0x00000000,0x00000001,0xffffffff};
  static const u32 local_n_p256r1[SECP256R1_WORDSIZE]={0xfc632551,0xf3b9cac2,0xa7179e84,0xbce6faad,0xffffffff,0xffffffff,0x00000000,0xffffffff};
/** <b>ECC Curve structure variable for SEC-P256r1</b>.
 *
 * @ingroup UCL_ECDSA
 */
extern ucl_type_curve secp256r1;
#endif//P256

/** <b>ECDSA signature</b>.
 * Compute a ECDSA signature, using curve domain parameters
 *
* @param[out] signature: pointer to a ucl_type_ecdsa_signature structure, containing the signature (r,s) values
* @param[in] *d: input, the secret key
* @param[in] *ucl_hash: input, the pointer to the hash function (see hash functions documentation for already available ones)
* @param[in] *input: input, the message or the hash digest to be signed,
* @param[in] inputlength: input, the input length, in bytes
* @param[in] *curve_params: the pointer to a ucl_type_curve structure, containing the curve domain parameters (already existing ones are described in the documentation) 
* @param[in] configuration (combination of any of these lines) 
     - UCL_R_PRECOMP or UCL_PRECOMP_R:
     - UCL_R_PRECOMP: using precomputed r to finish the signature computation,
     - UCL_PRECOMP_R: to only pre-compute the r value,
     - UCL_MSG_INPUT or UCL_HASH_INPUT or UCL_NO_INPUT
     - UCL_MSG_INPUT: the message will be hashed first,
     - UCL_HASH_INPUT: the message is already the hash digest,
     - UCL_NO_INPUT: for r precomputation (UCL_PRECOMP_R)
     - Examples are:
       - UCL_HASH_INPUT<<UCL_INPUT_SHIFT: signing a hash digest, r not precomputed
       - UCL_HASH_INPUT<<UCL_INPUT_SHIFT: signing a hash digest, r not precomputed
       - UCL_PRECOMP_R<<UCL_PRECOMP_SHIFT+UCL_NO_INPUT<<UCL_INPUT_SHIFT: pre-compute r 
       - UCL_R_PRECOMP<<UCL_PRECOMP_SHIFT+ UCL_MSG_INPUT<<UCL_INPUT_SHIFT:signing a message, using pre-computed r

 * @return Error code
 *
 * @retval #UCL_OK in case of correct computation
 * @retval #UCL_INVALID_INPUT or #UCL_INVALID OUTPUT in case of wrong parameters configuration (e.g. NULL pointers)
 *
 * @ingroup UCL_ECDSA */
int ucl_ecdsa_signature(ucl_type_ecdsa_signature signature,u8 *d,int(*ucl_hash)(u8*,u8*,u32),u8 *input, u32 inputlength, ucl_type_curve *curve_params,u32 configuration);

/** <b>ECDSA signature verification</b>.
 * Verify a ECDSA signature, using curve domain structure
*
* @param[in] public key: pointer to a ucl_type_ecc_u8_affine_point structure, containing the ECC public key, used for signature verification
* @param[in] signature: pointer to a ucl_type_ecdsa_signature structure, containing the signature (r,s) values
* @param[in] *ucl_hash: input, the pointer to the hash function (see hash functions documentation for already available ones)
* @param[in] *input: input, the message or the hash digest to be signed,
* @param[in] inputlength: input, the input length, in bytes
* @param[in] *curve_params: the pointer to a ucl_type_curve structure, containing the curve domain parameters (already existing ones are described in the documentation) 
* @param[in] configuration (combination of any of these lines) 
* @param[in] 
* @param[in] *ucl_hash: input, the already hashed digest of the message,
* @param[in] hashlength: input, the hash length, in bytes
 * @return Error code

 * @retval #UCL_OK if the signature is verified
 * @retval #UCL_ERROR if the signature is not verified
 * @retval #UCL_INVALID_INPUT in case of wrong parameters 
 *
 * @ingroup UCL_ECDSA */
int ucl_ecdsa_verification(ucl_type_ecc_u8_affine_point Q,ucl_type_ecdsa_signature signature,int(*ucl_hash)(u8*,u8*,u32),u8 *input,u32 inputlength,ucl_type_curve *curve_params,u32 configuration);

/** <b>ECC multiplication</b>.
 * multiply a scalar by a ECC point, using affine parameters, computation performed in Jacobian parameters
*
* @param[out] Q: pointer to a ucl_ecc_digit_affine_point structure, containing the ECC result point,
* @param[in] m: pointer to a word array containing the scalar,
* @param[in] X1: pointer to a ucl_ecc_digit_affine_point structure, containing the ECC point, to be multiplied by the scalar,
* @param[in] *curve_params: the pointer to a ucl_type_curve structure, containing the curve domain parameters (already existing ones are described in the documentation) 
* @return Error code

 * @retval #UCL_OK if the signature is verified
 * @retval #UCL_INVALID_INPUT in case of wrong parameters 
 *
 * @ingroup UCL_ECDSA */
int ecc_mult_jacobian(ucl_type_ecc_digit_affine_point Q, u32 *m, ucl_type_ecc_digit_affine_point X1,ucl_type_curve *curve_params);

/** <b>ECC addition</b>.
 * add two ECC points in affine coordinates
*
* @param[out] Q3: pointer to a ucl_ecc_digit_affine_point structure, containing the ECC result point,
* @param[in] Q1: pointer to a ucl_ecc_digit_affine_point structure, containing the ECC point, to be added with,
* @param[in] Q2: pointer to a ucl_ecc_digit_affine_point structure, containing the ECC point, to be added with,
* @param[in] *curve_params: the pointer to a ucl_type_curve structure, containing the curve domain parameters (already existing ones are described in the documentation) 
* @return Error code

 * @retval #UCL_OK if the signature is verified
 * @retval #UCL_INVALID_INPUT in case of wrong parameters 
 *
 * @ingroup UCL_ECDSA */
int ecc_add(ucl_type_ecc_digit_affine_point Q3,ucl_type_ecc_digit_affine_point Q1,ucl_type_ecc_digit_affine_point Q2,ucl_type_curve *curve_params);

/** <b>ECC doubling</b>.
 * doubling one ECC point in affine coordinates
*
* @param[out] Q3: pointer to a ucl_ecc_digit_affine_point structure, containing the ECC result point,
* @param[in] Q1: pointer to a ucl_ecc_digit_affine_point structure, containing the ECC point, to be doubled
* @param[in] *curve_params: the pointer to a ucl_type_curve structure, containing the curve domain parameters (already existing ones are described in the documentation) 
* @return Error code

 * @retval #UCL_OK if the signature is verified
 * @retval #UCL_INVALID_INPUT in case of wrong parameters 
 *
 * @ingroup UCL_ECDSA */
int ecc_double(ucl_type_ecc_digit_affine_point Q3,ucl_type_ecc_digit_affine_point Q1, ucl_type_curve *curve_params);


#endif//ECDSA_GENERIC_API
