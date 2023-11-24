/*******************************************************************************
* Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
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
//  ECC Generate Key - Generate Public key from provided private key for ECC secp256r1 



#define GEN_ECC_KEY
#include "ecc_generate_key.h"

int deep_cover_generatePublicKey(char *private_key, char *pubkey_x, char *pubkey_y);


//--------------------------------------------------------------------------
/// Generate ECC secp256r1 from provided Private Key
///
/// @param[in] private_key 
/// pointer to buffer that contains the private key 
/// (minumum 32-byte buffer, SECP256R1_BYTESIZE) 
/// @param[out] pubkey_x
/// 32-byte buffer container the public key x value
/// @param[out] pubkey_y
/// 32-byte buffer container the public key y value
///
/// @return
/// TRUE - command successful @n
/// FALSE - command failed
///
int deep_cover_generatePublicKey(char *private_key, char *pubkey_x, char *pubkey_y)
{
   ucl_type_ecc_digit_affine_point G_point, public_key;
   u32 privateKeyWords[SECP256R1_WORDSIZE];
   u32 Gx[SECP256R1_WORDSIZE];
   u32 Gy[SECP256R1_WORDSIZE];
   u32 publicKeyXWords[SECP256R1_WORDSIZE];
   u32 publicKeyYWords[SECP256R1_WORDSIZE];
   int i,rslt;

   // Convert bytes to words.
   bignum_us2d(privateKeyWords, SECP256R1_WORDSIZE, private_key, SECP256R1_BYTESIZE);

   // Copy multiplication constants.
   for (i = 0; i < SECP256R1_WORDSIZE; i++)
   {
      Gx[i] = local_xg_p256r1[i];
      Gy[i] = local_yg_p256r1[i];
   }

   // Generate public key.
   public_key.x = publicKeyXWords;
   public_key.y = publicKeyYWords;
   G_point.x = Gx;
   G_point.y = Gy; 
   rslt = ecc_mult_jacobian(public_key,
                     privateKeyWords,
                     G_point,
                     &secp256r1);

   // Convert words to bytes.
   bignum_d2us(pubkey_x, SECP256R1_BYTESIZE, publicKeyXWords, SECP256R1_WORDSIZE);
   bignum_d2us(pubkey_y, SECP256R1_BYTESIZE, publicKeyYWords, SECP256R1_WORDSIZE);

   return (rslt == 0);
}