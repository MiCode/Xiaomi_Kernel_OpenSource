
/*******************************************************************************
* Copyright (C) 2016 Maxim Integrated Products, Inc., All rights Reserved.
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
*******************************************************************************
*/

/** @file deep_cover_coproc.h
*   @brief  Include file for coprocessor functions to support
*    DS28C36/DS2476.  Implementation could be either software or 
*    DS2476 hardware. 
*/

#ifndef __DEEP_COVER_COPROC_H__
#define __DEEP_COVER_COPROC_H__

// Keys
#define ECDSA_KEY_A              0x00
#define ECDSA_KEY_B              0x01
#define ECDSA_KEY_C              0x02
#define ECDSA_KEY_S              0x03

// Secrets
#define SECRET_A                 0x00
#define SECRET_B                 0x01
#define SECRET_S                 0x02

// misc constants
#define TRUE    1
#define FALSE   0

extern int deep_cover_verifyECDSASignature(char *message, int msg_len, char *pubkey_x, char *pubkey_y, char *sig_r, char *sig_s);
extern int deep_cover_verifyHMAC(char *message, int msg_len, char *secret, char *hmac);
extern int deep_cover_computeHMAC(char *message, int msg_len, char *secret, char *hmac);
extern int deep_cover_computeECDSASignature(char *message, int msg_len, char *priv_key, char *sig_r, char *sig_s);
extern int deep_cover_createECDSACertificate(char *sig_r, char *sig_s, 
                                      char *pub_x, char *pub_y,
                                      char *custom_cert_fields, int cert_len, 
                                      char *priv_key);
extern int deep_cover_verifyECDSACertificate(char *sig_r, char *sig_s, 
                                      char *pub_x, char *pub_y,
                                      char *custom_cert_fields, int cert_len, 
                                      char *ver_pubkey_x, char *ver_pubkey_y);
extern int deep_cover_compute_ECDH(char *pubkey_x, char *pubkey_y, char *private_key, char *custom_ecdh_fields, int ecdh_len, char *shared_secret);
extern int deep_cover_computeUniqueSecret(char *master_secret, char *unique_secret, int pg, int anon, char *binding, char *partial, char *manid, char *romid);
extern int deep_cover_coproc_setup(int write_master_secret, int coproc_ecdh_key, int coproc_pauth_key, int verify_auth_key);



#endif /* __DEEP_COVER_COPROC_H__ */
