/*******************************************************************************
* Copyright (C) 2015 Maxim Integrated Products, Inc., All rights Reserved.
* Copyright (C) 2020 XiaoMi, Inc.
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
*     Description: return definition
*        Filename: ucl_retdefs.h
*          Author: LSL
*        Compiler: gcc
*
 *******************************************************************************
 */
#ifndef UCL_RETDEFS_H_
#define UCL_RETDEFS_H_

/** @file ucl_retdefs.h
 * @defgroup UCL_RETURN Definitions of returns
 *
 * @par Header:
 *  @link ucl_retdefs.h ucl_retdefs.h @endlink
 *
 * @ingroup UCL_DEFINITIONS
 */


/** <b>Carry</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_CARRY   1

/** <b>True</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_TRUE    1
/** <b>False</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_FALSE   0

/* ========================================================================== */


/** <b>No error occured</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_OK                      0
/** <b>Generic Error</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_ERROR                   -1
/** <b>Not a failure but no operation was performed</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_NOP                     -2
/** <b>Invalid cipher specified</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_CIPHER          -3
/** <b>Invalid hash specified</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_HASH            -4
/** <b>Generic invalid argument</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_ARG             -5
/** <b>Invalid argument input</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_INPUT           -6
/** <b>Invalid argument output</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_OUTPUT          -7
/** <b>Invalid precision for Fixed-Precision Aritmetic</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_PRECISION       -8
/** <b>Invalid RSA public key</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_RSAPUBKEY       -9
/** <b>Invalid RSA private key</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_RSAPRIVKEY      -10
/** Invalid RSA CRT key.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_RSACRTKEY       -11
/** <b>Invalid RSA CRT alternative key</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_RSACRTALTKEY    -12
/** <b>Error during CRT recomposition</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_RSACRT_ERROR            -13
/** <b>Error, division by zero</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_DIVISION_BY_ZERO        -14
/** <b>Invalid chosen mode</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_MODE            -15
/** <b>Large number with invalid sign</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INVALID_SIGN            -16
/** <b>Invalid input for RSA</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_RSA_INVALID_INPUT       -17
/** <b>TRNG timeout</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_RNGTIMEOUT              -18
/** <b>RSA PKCS1-v1.5 decryption error</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_RSAPKCS1_DECRYPTERR     -19
/** <b>Overflow</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_OVERFLOW                -20
/** <b>Error in the case of the function is disabled</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_FUNCTION_DISABLED       -21
/** <b>(Big) Integer not odd</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_INTEGER_NOT_ODD         -22
/** <b>Invalid exponant of RSA key</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_RSA_INVALID_EXPONANT    -23
/** <b>UCL Stack overflow</b>.
 * Not enough memory.
 * @ingroup UCL_RETURN
 */
#define UCL_STACK_OVERFLOW         -24
/** <b>UCL Stack not init</b>.
 * @see ucl_stack_init
 * @ingroup UCL_RETURN
 */
#define UCL_STACK_NOT_INIT          -25
/** <b>Invalid UCL Stack free</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_STACK_INVALID_FREE      -26
/** <b>The UCL Stack is disabled</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_STACK_DEFAULT           -27
/** <b>Use default UCL stack</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_STACK_ERROR             -28
/** <b>General Warning</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_WARNING                 -29
/** <b>PKCS1V25 Error - Invalid Signature </b>.
 * @ingroup UCL_RETURN
 */
#define UCL_PKCS1_INVALID_SIGNATURE -30
/** <b>No Interface for USIP&reg; AES</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_NO_UAES_INTERFACE      -31
/** <b>USIP&reg; AES Corrupted</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_UAES_CORRUPTED         -32
/** <b>USIP&reg; AES Error</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_UAES_ERROR              -33
/** <b>USIP&reg; TRNG Error</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_TRNG_ERROR              -34
/** <b>No Interface for USIP&reg; TRNG</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_NO_TRNG_INTERFACE       -35
/** <b>No Interface for USIP&reg; TRNG</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_TRNG_CORRUPTED          -36
/** <b>USIP&reg; UCL Not Init</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_NOT_INIT                -37
/** <b>ECC key is invalid</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_ECC_INVALID_KEY         -38

/** <b>RNG Interface Error</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_RNG_ERROR               -39

/** <b>Functionality not implemented</b>.
 * @ingroup UCL_RETURN
 */
#define UCL_NOT_IMPLEMENTED         -99


#endif /* UCL_RETDEFS_H_ */
