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

/**  @file deep_cover_coproc.c 
 *   @brief Coprocessor functions to support DS28C36/DS2476
 *     implemented in software. 
 */

#define DEEP_COVER_COPROC
#include "deep_cover_coproc.h"
#include "ecdsa_generic_api.h"
#include "ucl_defs.h"
#include "ucl_sha256.h"
#include "ucl_sys.h"
#include "sha256_hmac.h"

// Software compute functions
int deep_cover_verifyECDSASignature(char *message, int msg_len, char *pubkey_x, char *pubkey_y, char *sig_r, char *sig_s);
int deep_cover_verifyHMAC(char *message, int msg_len, char *secret, char *hmac);
int deep_cover_computeHMAC(char *message, int msg_len, char *secret, char *hmac);
int deep_cover_computeECDSASignature(char *message, int msg_len, char *priv_key, char *sig_r, char *sig_s);
int deep_cover_createECDSACertificate(char *sig_r, char *sig_s, 
                                      char *pub_x, char *pub_y,
                                      char *custom_cert_fields, int cert_len, 
                                      char *priv_key);
int deep_cover_verifyECDSACertificate(char *sig_r, char *sig_s, 
                                      char *pub_x, char *pub_y,
                                      char *custom_cert_fields, int cert_len, 
                                      char *ver_pubkey_x, char *ver_pubkey_y);
int deep_cover_compute_ECDH(char *pubkey_x, char *pubkey_y, char *private_key, char *custom_ecdh_fields, int ecdh_len, char *shared_secret);
int deep_cover_computeUniqueSecret(char *master_secret, char *unique_secret, int pg, int anon, char *binding, char *partial, char *manid, char *romid);
int deep_cover_coproc_setup(int master_secret, int ecdsa_signing_key, int ecdh_key, int ecdsa_verify_key);


// flag to enable debug
extern int ecdsa_debug;


//---------------------------------------------------------------------------
//-------- Deep Cover coprocessor functions (software implementation)
//---------------------------------------------------------------------------

int deep_cover_coproc_setup(int master_secret, int ecdsa_signing_key, int ecdh_key, int ecdsa_verify_key)
{
   // initialize the FCL library
   ucl_init();

   return TRUE;
}

//--------------------------------------------------------------------------
/// High level 'Compute and Lock SHA2 Secret' equivalent software sequence.
///
/// @param[in] master_secret (not used, master secret is Secret A or Secret B of DS2476)
/// pointer to buffer that contains the master secret (source)
/// @param[out] unique_secret (not used, unique secret will be saved to Secret S)
/// pointer to buffer that contains the resulting unique secret (destination)
/// @param[in] pg
/// Page to use to compute new secret
/// @param[in] anon
/// flag to indicate in anonymous mode (1) or not anonymous (0)
/// @param[in] binding
/// 32-byte buffer containing the binding data (from page), required to compute local copy of secret
/// @param[in] partial
/// 32-byte buffer containing the partial secret (for Buffer)
/// @param[in] manid
/// 2-byte buffer containing the MANID
/// @param[in] romid
/// 8-byte buffer containing the ROMID
///
/// @return
/// TRUE - command successful @n
/// FALSE - command failed
///
int deep_cover_computeUniqueSecret(char *master_secret, char *unique_secret, int pg, int anon, char *binding, char *partial, char *manid, char *romid)
{
   char message[256];
   int msg_len;

   // Compute and udpate the destination secret buffer
   msg_len = 0;
   // ROM_NO
   if (anon)
      memset(&message[msg_len],0xFF,8);
   else
      memcpy(&message[msg_len],romid,8);
   msg_len += 8;
   // Binding data
   memcpy(&message[msg_len],binding,32);
   msg_len += 32;
   // partial secret
   memcpy(&message[msg_len],partial,32);
   msg_len += 32;
   // Page#
   message[msg_len++] = pg; 
   // MANID
   memcpy(&message[msg_len], manid, 2);
   msg_len += 2;

   // Compute HMAC 
   return deep_cover_computeHMAC(message, msg_len, master_secret, unique_secret);
}

//---------------------------------------------------------------------------
/// Helper function to verify ECDSA signature using the DS2476 public. 
///
/// @param[in] message
/// Messge to hash for signature verification
/// @param[in] msg_len
/// Length of message in bytes
/// @param[in] pubkey_x
/// 32-byte buffer container the public key x value
/// @param[in] pubkey_y
/// 32-byte buffer container the public key y value
/// @param[in] sig_r
/// Signature r to verify
/// @param[in] sig_s
/// Signature s to verify
///
/// @return
/// TRUE - signature verified @n
/// FALSE - signature not verified
///
int deep_cover_verifyECDSASignature(char *message, int msg_len, char *pubkey_x, char *pubkey_y, char *sig_r, char *sig_s)
{
   int configuration;
   ucl_type_ecdsa_signature signature;
   ucl_type_ecc_u8_affine_point public_key;
   int rslt;   

   // Hook structure to r/s
   signature.r = sig_r;
   signature.s = sig_s;

   // Hook structure to x/y
   public_key.x = pubkey_x;
   public_key.y = pubkey_y;

   // construct configuration
   configuration=(SECP256R1<<UCL_CURVE_SHIFT)^(UCL_MSG_INPUT<<UCL_INPUT_SHIFT)^(UCL_SHA256<<UCL_HASH_SHIFT);

/*   if (ecdsa_debug)
   {
      dprintf("\n\nPublic Key X (length 32): (Q(x)) [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",public_key.x[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }

      dprintf("\n\nPublic Key Y (length 32): (Q(y)) [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",public_key.y[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }

      dprintf("\n\nMessage (length %d): [LSB first] \n",msg_len);
      for (i = 0; i < msg_len; i++)
      {
         dprintf("%02x",message[i]);
         if (((i + 1) % 8) == 0)
            dprintf("\n");
      }

      dprintf("\n\nSignature R (length 32): [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",signature.r[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }

      dprintf("\n\nSignature S (length 32): [MSB first] \n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",signature.s[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }
      dprintf("\n\n");
   }        */

   rslt = ucl_ecdsa_verification(public_key, signature, ucl_sha256, message, msg_len, &secp256r1, configuration); 
   
//   if (ecdsa_debug)
//      dprintf("verification result %d\n",(rslt == 0)); 

   return (rslt == 0);
}

//---------------------------------------------------------------------------
/// Helper function to verify HMAC using the DS2476 secret. 
///
/// @param[in] message
/// Messge to hash for signature verification
/// @param[in] msg_len
/// Length of message in bytes
/// @param[in] secret (not used, Secret S is used as the secret)
/// 32-byte buffer container the secret used to verify HMAC
/// @param[in] hmac
/// buffer for hmac to check, 32 bytes
///
/// @return
/// TRUE - signature verified @n
/// FALSE - signature not verified
///
int deep_cover_verifyHMAC(char *message, int msg_len, char *secret, char *hmac)
{
   char compute_hmac[32];

   // compute the HMAC
   if (!deep_cover_computeHMAC(message, msg_len, secret, compute_hmac))
      return FALSE;

   return (memcmp(hmac,compute_hmac,32) == 0);
}

//---------------------------------------------------------------------------
/// Helper function to compute HMAC using the DS2476 secret. 
///
/// @param[in] message
/// Messge to hash for signature verification
/// @param[in] msg_len
/// Length of message in bytes
/// @param[in] secret (not used, secret used is Secret S)
/// 32-byte buffer container secret to use
/// @param[out] hmac
/// buffer for hmac output, 32 bytes
///
/// @return
/// TRUE - computation complete @n
/// FALSE - failure to compute HMAC
///
int deep_cover_computeHMAC(char *message, int msg_len, char *secret, char *hmac)
{
   // compute the hmac
   return sha256_hmac(secret, 32, message, msg_len, hmac);
}

//---------------------------------------------------------------------------
/// Helper function to compute Signature using the specified private key. 
///
/// @param[in] message
/// Messge to hash for signature verification
/// @param[in] msg_len
/// Length of message in bytes
/// @param[in] priv_key (not used, private key must be either Private Key A, Private Key B, or Private Key C)
/// 32-byte buffer container the private key to use to compute signature
/// @param[out] sig_r
/// signature portion r
/// @param[out] sig_s
/// signature portion s
///
/// @return
/// TRUE - signature verified @n
/// FALSE - signature not verified
///
int deep_cover_computeECDSASignature(char *message, int msg_len, char *priv_key, char *sig_r, char *sig_s)
{
   int configuration;
   ucl_type_ecdsa_signature signature;

   // hook up r/s to the signature structure
   signature.r = sig_r;
   signature.s = sig_s;


   // construct configuration
   configuration=(SECP256R1<<UCL_CURVE_SHIFT)^(UCL_MSG_INPUT<<UCL_INPUT_SHIFT)^(UCL_SHA256<<UCL_HASH_SHIFT);

   // create signature and return result
   return (ucl_ecdsa_signature(signature, priv_key, ucl_sha256, message, msg_len, &secp256r1, configuration) == 0);
}

//---------------------------------------------------------------------------
/// Create certificate to authorize the provided Public Key for writes. 
///
/// @param[out] sig_r
/// Buffer for R portion of signature (MSByte first)
/// @param[out] sig_s
/// Buffer for S portion of signature (MSByte first)
/// @param[in] pub_x
/// Public Key x to create certificate
/// @param[in] pub_y
/// Public Key y to create certificate
/// @param[in] custom_cert_fields
/// Buffer for certificate customization fields (LSByte first)
/// @param[in] cert_len
/// Length of certificate customization field
/// @param[in] priv_key (not used, Private Key A, Private Key B, or Private Key C)
/// 32-byte buffer containing private key used to sign certificate
///
///  @return
///  TRUE - certificate created @n
///  FALSE - certificate not created
///
int deep_cover_createECDSACertificate(char *sig_r, char *sig_s,
                                      char *pub_x, char *pub_y,
                                      char *custom_cert_fields, int cert_len, 
                                      char *priv_key)
{
   char message[256];
   int  msg_len;

   // build message to verify signature
   // Public Key X | Public Key Y | Buffer (custom fields)
   
   // Public Key X
   msg_len = 0; 
   memcpy(&message[msg_len], pub_x, 32);
   msg_len += 32;
   // Public Key SY
   memcpy(&message[msg_len], pub_y, 32);
   msg_len += 32;
   // Customization cert fields
   memcpy(&message[msg_len], custom_cert_fields, cert_len);
   msg_len += cert_len;

   // Compute the certificate
   return deep_cover_computeECDSASignature(message, msg_len, priv_key, sig_r, sig_s);
}

//---------------------------------------------------------------------------
/// Verify certificate. 
///
/// @param[in] sig_r
/// Buffer for R portion of certificate signature (MSByte first)
/// @param[in] sig_s
/// Buffer for S portion of certificate signature (MSByte first)
/// @param[in] pub_x
/// Public Key x to verify
/// @param[in] pub_y
/// Public Key y to verify
/// @param[in] custom_cert_fields
/// Buffer for certificate customization fields (LSByte first)
/// @param[in] cert_len
/// Length of certificate customization field
/// @param[in] ver_pubkey_x
/// 32-byte buffer container the verify public key x
/// @param[in] ver_pubkey_y
/// 32-byte buffer container the verify public key y
///
///  @return
///  TRUE - certificate valid @n
///  FALSE - certificate not valid
///
int deep_cover_verifyECDSACertificate(char *sig_r, char *sig_s, 
                                      char *pub_x, char *pub_y,
                                      char *custom_cert_fields, int cert_len, 
                                      char *ver_pubkey_x, char *ver_pubkey_y)
{
   char message[256];
   int  msg_len;

   // build message to verify signature
   // Public Key X | Public Key Y | Buffer (custom fields)
   
   // Public Key X
   msg_len = 0; 
   memcpy(&message[msg_len], pub_x, 32);
   msg_len += 32;
   // Public Key SY
   memcpy(&message[msg_len], pub_y, 32);
   msg_len += 32;
   // Customization cert fields
   memcpy(&message[msg_len], custom_cert_fields, cert_len);
   msg_len += cert_len;

   // Compute the certificate
   return deep_cover_verifyECDSASignature(message, msg_len, ver_pubkey_x, ver_pubkey_y, sig_r, sig_s);
}

//---------------------------------------------------------------------------
/// Compute ECDH and save in shared_secret
///
/// @param[in] pubkey_x
/// 32-byte buffer container the public key x to compute ECDH
/// @param[in] pubkey_y
/// 32-byte buffer container the public key y to compute ECDH
/// @param[in] private_key (not used, use Private Key A, Private Key B, or Private Key C)
/// 32-byte buffer container the private key to compute ECDH
/// @param[in] custom_ecdh_fields
/// Buffer for ECDH customization fields (LSByte first)
/// @param[in] ecdh_len
/// Length of ECDH customization field
/// @param[out] shared_secret (not used, use Secret S)
/// pointer to 32-byte buffer that contains the resulting shared secret
///
///  @return
///  TRUE - compute successful @n
///  FALSE - failure to compute
///
int deep_cover_compute_ECDH(char *pubkey_x, char *pubkey_y, char *private_key, char *custom_ecdh_fields, int ecdh_len, char *shared_secret)
{
   ucl_type_ecc_digit_affine_point ecdh_point, public_key;
   u32 private_key_u32[8];
   u32 public_key_x_u32[8], public_key_y_u32[8];
   u32 ecdh_x_u32[8], ecdh_y_u32[8]; 
   char ecdh_x[32];
   char message[256];
   int msg_len;

   // convert Private Key from byte array to u32 array
   bignum_us2d(private_key_u32,8,private_key,32);

   // convert Public Key from byte array to u32 array 
   bignum_us2d(public_key_x_u32,8,pubkey_x,32);
   bignum_us2d(public_key_y_u32,8,pubkey_y,32);

   // connect u32 arrays to result points
   public_key.x = public_key_x_u32;
   public_key.y = public_key_y_u32;
   ecdh_point.x = ecdh_x_u32;
   ecdh_point.y = ecdh_y_u32;

   // multiply device Public Key with "my" Private Key to get shared point (key)
   // the device will multiply the Public Key S with its private key to get teh same point
   if (ecc_mult_jacobian(ecdh_point, private_key_u32, public_key, &secp256r1))
      return FALSE;

   // convert result X from u32 array to byte array 
   bignum_d2us(ecdh_x,32,ecdh_x_u32,8);

   // construct message for hash
   // (ECDH X | ECDH custom fields)     
   memcpy(message,ecdh_x,32);
   msg_len = 32;
   memcpy(&message[msg_len],custom_ecdh_fields,ecdh_len);
   msg_len += ecdh_len;

   // Hash the result, store in SECRET_S
   ucl_sha256(shared_secret, message, msg_len);

   return TRUE;
}
//---------------------------------------------------------------------------
/// Compute ECDH_EX and get the shared points' HASH saved in shared_secret
///
/// @param[in] pubkey_x
/// 32-byte buffer container the public key x to compute ECDH_EX
/// @param[in] pubkey_y
/// 32-byte buffer container the public key y to compute ECDH_EX
/// @param[in] private_key 
/// 32-byte buffer container the private key to compute ECDH_EX
/// @param[out] shared_secret (can be used for other algorithms, such as AES-128/256)
/// pointer to 32-byte buffer that contains the resulting shared secret
///
///  @return
///  TRUE - compute successful @n
///  FALSE - failure to compute
///
int deep_cover_compute_ECDH_EX(char *pubkey_x, char *pubkey_y, char *private_key,  char *shared_secret)
{
   ucl_type_ecc_digit_affine_point ecdh_point, public_key;
   u32 private_key_u32[8];
   u32 public_key_x_u32[8], public_key_y_u32[8];
   u32 ecdh_x_u32[8], ecdh_y_u32[8]; 
   char ecdh_x[32],ecdh_y[32];
   char message[256];
   int msg_len;

   // convert Private Key from byte array to u32 array
   bignum_us2d(private_key_u32,8,private_key,32);

   // convert Public Key from byte array to u32 array 
   bignum_us2d(public_key_x_u32,8,pubkey_x,32);
   bignum_us2d(public_key_y_u32,8,pubkey_y,32);

   // connect u32 arrays to result points
   public_key.x = public_key_x_u32;
   public_key.y = public_key_y_u32;
   ecdh_point.x = ecdh_x_u32;
   ecdh_point.y = ecdh_y_u32;

   // multiply device Public Key with "my" Private Key to get shared point (key)
   // the device will multiply the Public Key S with its private key to get teh same point
   if (ecc_mult_jacobian(ecdh_point, private_key_u32, public_key, &secp256r1))
      return FALSE;

   // convert result X from u32 array to byte array 
   // convert result Y from u32 array to byte array    
   bignum_d2us(ecdh_x,32,ecdh_x_u32,8);
   bignum_d2us(ecdh_y,32,ecdh_y_u32,8);
   

   // construct message for hash
   memcpy(message,ecdh_x,32);
   msg_len = 32;
   memcpy(&message[msg_len],ecdh_y,32);
   msg_len += 32;

   // Hash the result, store in SECRET_S
   ucl_sha256(shared_secret, message, msg_len);

   return TRUE;
}