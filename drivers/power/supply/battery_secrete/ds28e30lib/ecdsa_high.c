
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
*     Module Name: ECC test application
*     Description: performs ECDSA computations
*        Filename: ecdsa_high.c
*          Author: LSL
*        Compiler: gcc
*
*******************************************************************************
*/

//1.0.0: first release, with sign and verify functions, taken from UCL
//1.0.1: some cleaning in the code
#include "bignum_ecdsa_generic_api.h"
#include "ecdsa_generic_api.h"
#include <linux/string.h>

#include "ucl_config.h"
#include "ucl_sys.h"
#include "ucl_defs.h"
#include "ucl_retdefs.h"
#include "ucl_rng.h"
#include "ucl_hash.h"
#ifdef HASH_SHA256
#include "ucl_sha256.h"
#endif
#ifdef HASH_SIA256
#include "ucl_sia256.h"
#endif

extern int hash_size[MAX_HASH_FUNCTIONS];
extern int sha_debug; 

//curves domain parameters
ucl_type_curve secp256r1={(u32*)local_a_p256r1,(u32*)local_b_p256r1,(u32*)local_p_p256r1,(u32*)local_n_p256r1,(u32*)local_xg_p256r1,(u32*)local_yg_p256r1,(u32*)local_inv2_p256r1,(u32*)local_psquare_p256r1,NULL,NULL,SECP256R1_WORDSIZE,SECP256R1_BYTESIZE,SECP256R1};
ucl_type_curve secp192r1={(u32*)local_a_p192r1,(u32*)local_b_p192r1,(u32*)local_p_p192r1,(u32*)local_n_p192r1,(u32*)local_xg_p192r1,(u32*)local_yg_p192r1,(u32*)local_inv2_p192r1,(u32*)local_psquare_p192r1,NULL,NULL,SECP192R1_WORDSIZE,SECP192R1_BYTESIZE,SECP192R1};


int  ucl_ecdsa_signature(ucl_type_ecdsa_signature signature,u8 *d,int(*ucl_hash)(u8*,u8*,u32),u8 *input, u32 inputlength, ucl_type_curve *curve_params,u32 configuration)
{
  //the larger size to cover any hash function
  u8 e[64];
  int resu,i;
  u8 *ptr;
  u32 R[SECP521R1_WORDSIZE],E[SECP521R1_WORDSIZE],S[SECP521R1_WORDSIZE],U2[SECP521R1_WORDSIZE+1];    //SECP521R1_WORDSIZE=17
  u32 X1[SECP521R1_WORDSIZE],Y1[SECP521R1_WORDSIZE],K[SECP521R1_WORDSIZE],W[SECP521R1_WORDSIZE],D[SECP521R1_WORDSIZE];
  ucl_type_ecc_digit_affine_point Q;
  ucl_type_ecc_digit_affine_point P;
  int hash,input_format;
  int curve_wsize,curve_bsize,hashsize;
  //check parameters
  if(NULL==input)
    return(UCL_INVALID_INPUT);
  if(NULL==d)
    return(UCL_INVALID_INPUT);


  //retrieve configuration
  hash=(configuration>>UCL_HASH_SHIFT)&UCL_HASH_MASK;
  input_format=(configuration>>UCL_INPUT_SHIFT)&UCL_INPUT_MASK;
  //if no-input, the function call is only for r & k-1 precomputation
  if(UCL_NO_INPUT==input_format)
    return(UCL_INVALID_INPUT);

  //hash computation only if input format is UCL_MSG_INPUT
  hashsize=hash_size[hash];
//1. e=SHA(m)
  if(UCL_MSG_INPUT==input_format)
    ucl_hash(e,input,inputlength);
  else
    if(UCL_NO_INPUT!=input_format)
      {
	//here, the hash is provided as input
	if(inputlength!=UCL_SHA256_HASHSIZE)
	  if(inputlength!=UCL_SIA256_HASHSIZE)
	    return(UCL_INVALID_INPUT);
	hashsize=(int)inputlength;
	memcpy(e,input,inputlength);
      }
  curve_wsize=(int)(curve_params->curve_wsize);    //curve_wsize=8
  curve_bsize=(int)(curve_params->curve_bsize);    //curve_bsize=32
  //2 generate k for Q computation
  //this has to be really random, otherwise the key is exposed
//  do
//    ucl_rng_read((u8*)K,(u32)curve_bsize);
//  while(bignum_cmp(K,(u32*)(curve_params->n),curve_wsize)>=0);

	 do
    ucl_rng_read((u8*)K,(u32)curve_bsize);
  while(bignum_cmp(K,(u32*)(curve_params->n),curve_wsize)>=0);


  //????????????????????????????????????????????
  //???Force Random value
  //???55AA55AA55AA55AA
  //???55AA55AA55AA55AA
  //???55AA55AA55AA55AA
  //???55AA55AA55AA55AA
  //???????????????????

#ifdef ECDSA_FIXED_RANDOM
  ptr = (u8*)&K[0];
  for (i = 0; i < 32; i+=2)
  {
     ptr[i] = 0x55;
     ptr[i+1] = 0xAA;
  }
  //??????????????????
#endif

  if (sha_debug)
  {
      ptr = (u8*)&K[0];
      printk("\nRandom input (length 32): (k) [LSB first] \n");
      for (i = 0; i < 32; i++)
      {
         printk("%02x",ptr[i]);
         if (((i + 1) % 4) == 0)
            printk(" ");
      }
      printk("\n");
  }


  //3 compute r=x1(mod n) where (x1,y1)=k.G
  //compute k.G
  Q.x=X1;
  Q.y=Y1;
  P.x=(u32*)curve_params->xg;
  P.y=(u32*)curve_params->yg;
  resu=ecc_mult_jacobian(Q,K,P,curve_params);
  if(UCL_OK!=resu)
    return(resu);
  bignum_d2us(signature.r,(u32)curve_bsize,X1,(u32)curve_wsize);
  //r=x1 mod n
  bignum_mod(R,X1,(u32)curve_wsize,(u32*)curve_params->n,(u32)curve_wsize);
  //store R in r
  bignum_d2us(signature.r,(u32)curve_bsize,R,(u32)curve_wsize);
  //4 compute s=k_inv.(z+r.d)mod n
  bignum_modinv(W,K,(u32*)curve_params->n,(u32)curve_wsize);
  //parameter check
  //u2=r.d
  bignum_us2d(D,(u32)curve_wsize,d,(u32)curve_bsize);
  bignum_modmult(U2,R,D,(u32*)curve_params->n,(u32)curve_wsize);
  //z+r.d where z is e
  bignum_us2d(E,(u32)curve_wsize,e,(u32)min(curve_bsize,hashsize));
  //sm2.A5 r=(e+x1) mod n
  bignum_modadd(U2,E,U2,(u32*)curve_params->n,(u32)curve_wsize);
  //k_inv . (z+r.d)
  bignum_modmult(S,W,U2,(u32*)curve_params->n,(u32)curve_wsize);
  bignum_d2us(signature.s,(u32)curve_bsize,S,(u32)curve_wsize);
  //6 result
  return(UCL_OK);
}

int ucl_ecdsa_verification(ucl_type_ecc_u8_affine_point Q,ucl_type_ecdsa_signature signature,int(*ucl_hash)(u8*,u8*,u32),u8 *input,u32 inputlength,ucl_type_curve *curve_params,u32 configuration)
{
  u32 S[SECP521R1_WORDSIZE+1],R[SECP521R1_WORDSIZE],W[SECP521R1_WORDSIZE],E[SECP521R1_WORDSIZE],U1[SECP521R1_WORDSIZE],U2[SECP521R1_WORDSIZE];
  u32 X1[SECP521R1_WORDSIZE],Y1[SECP521R1_WORDSIZE],X2[SECP521R1_WORDSIZE],Y2[SECP521R1_WORDSIZE];
  u32 X[SECP521R1_WORDSIZE],Y[SECP521R1_WORDSIZE];
  u32 XQ[SECP521R1_WORDSIZE],YQ[SECP521R1_WORDSIZE];
  ucl_type_ecc_digit_affine_point pQ;
  ucl_type_ecc_digit_affine_point pP;
  ucl_type_ecc_digit_affine_point pR;

  //the hash digest has the largest size, to fit any hash function
  u8 e[64];
  int hash,input_format;
  int curve_wsize,curve_bsize,hashsize;
  //check parameters
  if(NULL==input)
    return(UCL_INVALID_INPUT);
  //retrieve configuration
  hash=(configuration>>UCL_HASH_SHIFT)&UCL_HASH_MASK;
  input_format=(configuration>>UCL_INPUT_SHIFT)&UCL_INPUT_MASK;
  //no input is non sense for verify
  if(UCL_NO_INPUT==input_format)
    return(UCL_INVALID_INPUT);
  
  //hash computation only if input format is UCL_MSG_INPUT
    //1. e=SHA(m)
  hashsize=hash_size[hash];
  if(UCL_MSG_INPUT==input_format)
    ucl_hash(e,input,inputlength);
  else
    {
  //or here, the hash is provided as input
      if(inputlength!=UCL_SHA256_HASHSIZE)
	return(UCL_INVALID_INPUT);
      hashsize=(int)inputlength;
      memcpy(e,input,inputlength);
    }
  curve_wsize=curve_params->curve_wsize;
  curve_bsize=curve_params->curve_bsize;

  //2. Verification of the r/s intervals (shall be <n)
  bignum_us2d(S,(u32)curve_wsize, signature.s, (u32)curve_bsize);
  bignum_us2d(R,(u32)curve_wsize, signature.r, (u32)curve_bsize);
  if((bignum_cmp(S,(u32*)curve_params->n,(u32)curve_wsize)>=0)||(bignum_cmp(R,(u32*)curve_params->n,(u32)curve_wsize)>=0))
    return(UCL_ERROR);
  //3. w=s^-1
  bignum_modinv(W,S,(u32*)curve_params->n,(u32)curve_wsize);
  //4. U1=e.w mod n and U2=r.w mod n
  bignum_us2d(E,(u32)curve_wsize,e,(u32)min(hashsize,curve_bsize));
  //U1=E*W mod n
  bignum_modmult(U1,E,W,(u32*)curve_params->n,(u32)curve_wsize);
  bignum_modmult(U2,R,W,(u32*)curve_params->n,(u32)curve_wsize);
  // 5. (x1,y1)=u1*G+u2*Q
  // u1*G
  pP.x=(u32*)curve_params->xg;
  pP.y=(u32*)curve_params->yg;
  pQ.x=X1;
  pQ.y=Y1;
  ecc_mult_jacobian(pQ,U1,pP,curve_params);

  // u2*Q
  bignum_us2d(XQ,(u32)curve_wsize, Q.x, (u32)curve_bsize);
  bignum_us2d(YQ,(u32)curve_wsize, Q.y, (u32)curve_bsize);
  pP.x=XQ;
  pP.y=YQ;
  pQ.x=X2;
  pQ.y=Y2;
  ecc_mult_jacobian(pQ,U2,pP,curve_params);

  // u1*G+u2*Q
  if(bignum_cmp(X1,X2,(u32)curve_wsize)!=0 || bignum_cmp(X1,X2,(u32)curve_wsize)!=0)
    {
      pP.x=X1;
      pP.y=Y1;
      pQ.x=X2;
      pQ.y=Y2;
      pR.x=X;
      pR.y=Y;
      ecc_add(pR,pP,pQ,curve_params);
    }
  else
    {
      pP.x=X1;
      pP.y=Y1;
      pR.x=X;
      pR.y=Y;
      ecc_double(pR,pP,curve_params);
    }
  //5.4.4 2. v=x1 mod n
  bignum_mod(Y,X,(u32)curve_wsize,(u32*)curve_params->n,(u32)curve_wsize);
  // 3. if r==v) ok
  if(bignum_cmp(R,Y,(u32)curve_wsize)==0)
    return(UCL_OK);
  else
    return(UCL_ERROR);
}

