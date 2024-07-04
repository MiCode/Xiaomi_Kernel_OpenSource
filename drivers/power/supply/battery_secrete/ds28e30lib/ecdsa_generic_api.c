
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
 *     Description: performs ECC computations
 *        Filename: ecdsa_generic_api.c
 *          Author: LSL
 *        Compiler: gcc
 *
 *******************************************************************************
 */

#define MAJVER 1
#define MINVER 0
#define ZVER 0
//1.0.0: initial release from UCL file 1.9.0
#include "bignum_ecdsa_generic_api.h"
#include "ecdsa_generic_api.h"


#include "ucl_config.h"
#include "ucl_sys.h"
#include "ucl_defs.h"
#include "ucl_retdefs.h"
#include "ucl_rng.h"
#include "ucl_hash.h"
#ifdef HASH_SHA256
#include "ucl_sha256.h"
#endif

//default modular reduction
//not efficient for special NIST primes, as not using their structure
void ecc_mod(u32 *b,u32 *c,u32 cDigits,u32 *p,u32 pDigits)
{
  bignum_mod(b,c,cDigits,p,pDigits);
}

#ifdef P192
//default modular reduction
//not efficient for special NIST primes, as not using their structure
void ecc_mod192r1(u32 *b,u32 *c,u32 cDigits,u32 *p,u32 pDigits)
{
  bignum_mod(b,c,cDigits,p,pDigits);
}
#endif//P192
#ifdef P256
//default modular reduction
//not efficient for special NIST primes, as not using their structure
void ecc_mod256r1(u32 *b,u32 *c,u32 cDigits,u32 *p,u32 pDigits)
{
  bignum_mod(b,c,cDigits,p,pDigits);
}
#endif//P256

int point_less_than_psquare(u32 *c,u32 cDigits,u32 *psquare,u32 pDigits)
{
  if(cDigits<pDigits)
    return(UCL_TRUE);
  else
    if(cDigits>pDigits)
      return(UCL_FALSE);
    else
      if(bignum_cmp(c,psquare,cDigits)>=0)
	return(UCL_FALSE);
      else
	return(UCL_TRUE);
}

void ecc_modcurve(u32 *b,u32 *c,u32 cDigits,ucl_type_curve *curve_params)
{
  switch(curve_params->curve)
    {
#ifdef P192
    case SECP192R1:
	ecc_mod192r1(b,c,cDigits,(u32*)(curve_params->p),curve_params->curve_wsize);
      break;
#endif//P192
#ifdef P256
    case SECP256R1:
	ecc_mod256r1(b,c,cDigits,(u32*)(curve_params->p),curve_params->curve_wsize);
      break;
#endif//P256
    default:
      ecc_mod(b,c,cDigits,(u32*)(curve_params->p),curve_params->curve_wsize);
      break;
    }
}

int ecc_modsub(u32 *p_result, u32 *p_left, u32 *p_right,ucl_type_curve *curve_params)
{
  u32 carry;
  carry= bignum_sub(p_result, p_left, p_right,curve_params->curve_wsize);
  if(carry)
    bignum_add(p_result, p_result, (u32*)(curve_params->p),curve_params->curve_wsize);
  return(UCL_OK);
}

int ecc_modadd(u32 *r,u32 *a,u32 *b,ucl_type_curve *curve_params)
{
  u32 resu[1+ECDSA_DIGITS];
  bignum_add(resu,a,b,curve_params->curve_wsize);
  ecc_modcurve(r,resu,1+curve_params->curve_wsize,curve_params);
  return(UCL_OK);
}

int ecc_modleftshift(u32 *a,u32 *b,u32 c,u32 digits,ucl_type_curve *curve_params)
{
  u32 tmp[ECDSA_DIGITS+1];
  tmp[digits]=bignum_leftshift(tmp,b,c,digits);
  ecc_modcurve(a,tmp,digits+1,curve_params);
  return(UCL_OK);
}

int ecc_modmult(u32 *r,u32 *a,u32 *b,ucl_type_curve *curve_params)
{
  u32 mult[2*ECDSA_DIGITS];
  bignum_mult(mult,a,b,curve_params->curve_wsize);
  ecc_modcurve(r,mult,2*curve_params->curve_wsize,curve_params);
  return(UCL_OK);
}

void ecc_modmultscalar(u32 *r,u32 a,u32 *b,ucl_type_curve *curve_params)
{
  u32 mult[2*ECDSA_DIGITS];
  bignum_multscalar(mult,a,b,curve_params->curve_wsize);
  ecc_modcurve(r,mult,2*curve_params->curve_wsize,curve_params);
}

int ecc_modsquare(u32 *r,u32 *a,ucl_type_curve *curve_params)
{
  u32 mult[2*ECDSA_DIGITS];
  bignum_square(mult,a,curve_params->curve_wsize);
  ecc_modcurve(r,mult,2*curve_params->curve_wsize,curve_params);
  return(UCL_OK);
}

int  ecc_infinite_affine(ucl_type_ecc_digit_affine_point Q,ucl_type_curve *curve_params)
{
  if(bignum_isnul(Q.x,(u32)(curve_params->curve_wsize)) && bignum_isnul(Q.y,(u32)(curve_params->curve_wsize)))
    return(UCL_TRUE);
  return(UCL_ERROR);
}

int ecc_infinite_jacobian(ucl_type_ecc_jacobian_point Q,ucl_type_curve *curve_params)
{
  int i;
  if( (Q.x[0]!=1) || (Q.y[0]!=1))
    return(UCL_ERROR);
  if(!bignum_isnul(Q.z,curve_params->curve_wsize))
    return(UCL_ERROR);

  for(i=1;i<(int)curve_params->curve_wsize;i++)
    if((Q.x[i]!=0) || (Q.y[i]!=0))
      return(UCL_ERROR);
  return(UCL_TRUE);
}

int ecc_double_jacobian(ucl_type_ecc_jacobian_point Q3,ucl_type_ecc_jacobian_point Q1,ucl_type_curve *curve_params)
{
  u32 t1[ECDSA_DIGITS];
  u32 t2[ECDSA_DIGITS];
  u32 t3[ECDSA_DIGITS+1];
  int digits;
  digits=curve_params->curve_wsize;

  //2.t1=z1^2
  if(ecc_infinite_jacobian(Q1,curve_params)==UCL_TRUE)
    {
      //return(x2:y2:1)
      bignum_copy(Q3.x,Q1.x,curve_params->curve_wsize);
      bignum_copy(Q3.y,Q1.y,curve_params->curve_wsize);
      bignum_copydigit(Q3.z,0,curve_params->curve_wsize);
      return(UCL_OK);
    }
  ecc_modsquare(t1,Q1.z,curve_params);
  //3.t2=x1-t1
  ecc_modsub(t2,Q1.x,t1,curve_params);
  //4.t1=x1+t1
  bignum_modadd(t1,t1,Q1.x,(u32*)curve_params->p,curve_params->curve_wsize);
  //5.t2=t2*t1
  ecc_modmult(t2,t2,t1,curve_params);
  //6.t2=3*t2
  ecc_modmultscalar(t2,3,t2,curve_params);
  //7.y3=2*y1
  ecc_modleftshift(Q3.y,Q1.y,1,digits,curve_params);
  //8.z3=y3*z1
  ecc_modmult(Q3.z,Q1.z,Q3.y,curve_params);
  //9.y3^2
  ecc_modsquare(Q3.y,Q3.y,curve_params);
  //10.t3=y3.x1
  ecc_modmult(t3,Q1.x,Q3.y,curve_params);
  //11.y3=y3^2
  ecc_modsquare(Q3.y,Q3.y,curve_params);
  //12.y3=y3/2 equiv. to y3=y3*(2^-1)
  ecc_modmult(Q3.y,Q3.y,curve_params->invp2,curve_params);
  //13.x3=t2^2
  ecc_modsquare(Q3.x,t2,curve_params);
  //14.t1=2*t3
  ecc_modleftshift(t1,t3,1,digits,curve_params);
  //15.x3=x3-t1
  ecc_modsub(Q3.x,Q3.x,t1,curve_params);
  //16.t1=t3-x3
  ecc_modsub(t1,t3,Q3.x,curve_params);
  //17.t1=t1*t2
  ecc_modmult(t1,t1,t2,curve_params);
  //18.y3=t1-y3
  ecc_modsub(Q3.y,t1,Q3.y,curve_params);
  //result in x3,y3,z3
  return(UCL_OK);
}

int ecc_add_jacobian_affine(ucl_type_ecc_jacobian_point Q3,ucl_type_ecc_jacobian_point Q1,ucl_type_ecc_digit_affine_point Q2,ucl_type_curve *curve_params)
{
  u32 t1[ECDSA_DIGITS];
  u32 t2[ECDSA_DIGITS];
  u32 t3[ECDSA_DIGITS];
  u32 t4[ECDSA_DIGITS];
  u32 scalar[ECDSA_DIGITS];
  ucl_type_ecc_jacobian_point Q2tmp;
  int digits;
  digits=curve_params->curve_wsize;
  if(ecc_infinite_affine(Q2,curve_params)==UCL_TRUE)
    {
      bignum_copy(Q3.x,Q1.x,curve_params->curve_wsize);
      bignum_copy(Q3.y,Q1.y,curve_params->curve_wsize);
      bignum_copy(Q3.z,Q1.z,curve_params->curve_wsize);
      return(UCL_OK);
    }

  if(ecc_infinite_jacobian(Q1,curve_params)==UCL_TRUE)
    {
      //return(x2:y2:1)
      bignum_copy(Q3.x,Q2.x,curve_params->curve_wsize);
      bignum_copy(Q3.y,Q2.y,curve_params->curve_wsize);
      bignum_copydigit(Q3.z,1,curve_params->curve_wsize);
      return(UCL_OK);
    }
  //3.t1=z1^2
  ecc_modsquare(t1,Q1.z,curve_params);
  //4.t2=t1*z1
  ecc_modmult(t2,t1,Q1.z,curve_params);
  //6.t2=t2*y2
  ecc_modmult(t2,t2,Q2.y,curve_params);
  //5.t1=t1*x2
  ecc_modmult(t1,t1,Q2.x,curve_params);
  //7.t1=t1-x1
  ecc_modsub(t1,t1,Q1.x,curve_params);
  //8.t2=t2-y1
  ecc_modsub(t2,t2,Q1.y,curve_params);
  //9.
  if(bignum_isnul(t1,curve_params->curve_wsize))
    {
      bignum_copyzero(scalar,curve_params->curve_wsize);
      //9.1
      if(bignum_isnul(t2,curve_params->curve_wsize))
	{
	  //double (x2:y2:1)
	  scalar[0]=1;
	  Q2tmp.x=Q2.x;
	  Q2tmp.y=Q2.y;
	  Q2tmp.z=scalar;
	  ecc_double_jacobian(Q3,Q2tmp,curve_params);
	  return(UCL_OK);
	}
      //9.2
      else
	{
	  //return infinite
	  bignum_copy(Q3.x,scalar,curve_params->curve_wsize);
	  bignum_copy(Q3.y,scalar,curve_params->curve_wsize);
	  bignum_copyzero(Q3.z,curve_params->curve_wsize);
	  return(UCL_OK);
	}
    }
  //10.z3=z1*t1
  ecc_modmult(Q3.z,Q1.z,t1,curve_params);
  //11.t3=t1^2
  ecc_modsquare(t3,t1,curve_params);
  //12.t4=t3*t1
  ecc_modmult(t4,t3,t1,curve_params);
  //13.t3=t3*x1
  ecc_modmult(t3,t3,Q1.x,curve_params);
  //14.t1=2*t3
  ecc_modleftshift(t1,t3,1,digits,curve_params);
  //15.x3=t2^2
  ecc_modsquare(Q3.x,t2,curve_params);
  //16.x3=Q3.x-t1
  ecc_modsub(Q3.x,Q3.x,t1,curve_params);
  //17.x3=x3-t4
  ecc_modsub(Q3.x,Q3.x,t4,curve_params);
  //18.t3=t3-x3
  ecc_modsub(t3,t3,Q3.x,curve_params);
  //19.t3=t3*t2
  ecc_modmult(t3,t3,t2,curve_params);
  //20.t4=t4*y1
  ecc_modmult(t4,t4,Q1.y,curve_params);
  //21.y3=t3-t4
  ecc_modsub(Q3.y,t3,t4,curve_params);
  //result in x3,y3,z3
  return(UCL_OK);
}

int ecc_convert_affine_to_jacobian(ucl_type_ecc_jacobian_point Q,ucl_type_ecc_digit_affine_point X1,ucl_type_curve *curve_params)
{
  //conversion from x:y to x*z^2:y*z^3:z; direct and simple for z=1
  bignum_copy(Q.x,X1.x,(u32)curve_params->curve_wsize);
  bignum_copy(Q.y,X1.y,(u32)curve_params->curve_wsize);
  bignum_copydigit(Q.z,1,(u32)curve_params->curve_wsize);
  return(UCL_OK);
}

int ecc_convert_jacobian_to_affine(u32 *x,u32 *y,u32 *xq,u32 *yq,u32 *zq,ucl_type_curve *curve_params)
{
  u32 tmp[ECDSA_DIGITS];
  u32 tmp1[ECDSA_DIGITS];
  int digits;
  digits=curve_params->curve_wsize;
  //x:y:z corresponds to x/z^2:y/z^3
  //z^2
  ecc_modsquare(tmp,zq,curve_params);
  //z^-2
  bignum_modinv(tmp1,tmp,(u32*)curve_params->p,digits);
  ecc_modmult(x,xq,tmp1,curve_params);
  //z^3
  ecc_modmult(tmp,tmp,zq,curve_params);
  //z^-3
  bignum_modinv(tmp1,tmp,(u32*)curve_params->p,digits);
  ecc_modmult(y,yq,tmp1,curve_params);
  return(UCL_OK);
}

int ecc_mult_jacobian(ucl_type_ecc_digit_affine_point Q, u32 *m, ucl_type_ecc_digit_affine_point X1,ucl_type_curve *curve_params)
{
  int i;
  int j;
  u32 zq[ECDSA_DIGITS];
  int size;
  ucl_type_ecc_jacobian_point T;

  u32 mask=(u32)0x80000000;
  u8 first;

  if(NULL==m)
    return(UCL_INVALID_INPUT);
  bignum_copyzero(Q.x,curve_params->curve_wsize);
  bignum_copyzero(Q.y,curve_params->curve_wsize);
  bignum_copyzero(zq,curve_params->curve_wsize);
  size=(int)curve_params->curve_wsize;
  mask=(u32)0x80000000;
  T.x=Q.x;
  T.y=Q.y;
  T.z=zq;
  first=1;
  for(i=(int)(size-1);i>=0;i--)
    {
      for(j=0;j<(int)DIGIT_BITS;j++)
	{
	  if(!first)
	    ecc_double_jacobian(T,T,curve_params);
	  if((m[i]&(mask>>j))!=0)
	    {
	      if(first)
		{
		  ecc_convert_affine_to_jacobian(T,X1,curve_params);
		  first=0;
		}
	      else
		ecc_add_jacobian_affine(T,T,X1,curve_params);
	    }
	}
    }
  ecc_convert_jacobian_to_affine(Q.x,Q.y,T.x,T.y,T.z,curve_params);
  return(UCL_OK);
}

int ecc_add(ucl_type_ecc_digit_affine_point Q3,ucl_type_ecc_digit_affine_point Q1,ucl_type_ecc_digit_affine_point Q2,ucl_type_curve *curve_params)
{
  u32 lambda[ECDSA_DIGITS];
  u32 tmp1[ECDSA_DIGITS];
  u32 tmp2[ECDSA_DIGITS];
  //tmp1=(x2-x1)
  ecc_modsub(tmp1,Q2.x,Q1.x,curve_params);
  bignum_modinv(tmp2,tmp1,(u32*)(curve_params->p),curve_params->curve_wsize);
  //tmp1=(y2-y1)
  ecc_modsub(tmp1,Q2.y,Q1.y,curve_params);    
  //lambda=(y2-y1)*(x2-x1)^-1 mod p          
  ecc_modmult(lambda,tmp1,tmp2,curve_params); 
  //tmp1=lambda^2 mod p
  ecc_modsquare(tmp1,lambda,curve_params); 
  //tmp2=lambda^2 mod p -x1
  ecc_modsub(tmp2,tmp1,Q1.x,curve_params); 
  //x3  =lambda^2 mod p -x1 -x2
  ecc_modsub(Q3.x,tmp2,Q2.x,curve_params); 
  //tmp2=x1-x3
  ecc_modsub(tmp2,Q1.x,Q3.x,curve_params);
  //tmp1=lambda * (x1-x3)
  ecc_modmult(tmp1,lambda,tmp2,curve_params);  
  //y3=lambda * (x1-x3) -y1
  ecc_modsub(Q3.y,tmp1,Q1.y,curve_params); 
  return(UCL_OK);
}

int ecc_double(ucl_type_ecc_digit_affine_point Q3,ucl_type_ecc_digit_affine_point Q1, ucl_type_curve *curve_params)
{
  u32 lambda[ECDSA_DIGITS+1];
  u32 tmp1[ECDSA_DIGITS+1];
  u32 tmp2[ECDSA_DIGITS+1];
  u32 tmp3[ECDSA_DIGITS+1];
  u32 trois[ECDSA_DIGITS];

  bignum_copyzero(trois,curve_params->curve_wsize);
  trois[0]=3;
  //tmp1   = x1^2
  ecc_modsquare(tmp1,Q1.x,curve_params);		
  //lambda = 3*x1^2	
  ecc_modmult(lambda,trois,tmp1,curve_params);		
  //tmp1   = 3*x1^2+a
  tmp1[curve_params->curve_wsize]=bignum_add(tmp1,lambda,(u32*)(curve_params->a),curve_params->curve_wsize);			    
  ecc_modcurve(tmp1,tmp1,curve_params->curve_wsize+1,curve_params);
  //tmp2   = 2*y1
  tmp2[curve_params->curve_wsize]=bignum_leftshift(tmp2,Q1.y,1,curve_params->curve_wsize);					
  ecc_modcurve(tmp2,tmp2,curve_params->curve_wsize+1,curve_params);
  //tmp3   = 2*y1^-1 mod p
  bignum_modinv(tmp3,tmp2,(u32*)(curve_params->p),curve_params->curve_wsize);				
  //lambda = (3*x1^2+a)*(2*y)^-1 modp 
  ecc_modmult(lambda,tmp1,tmp3,curve_params);	 
  //tmp1=Lambda^2 mod p
  ecc_modsquare(tmp1,lambda,curve_params); 
  //tmp2=Lambda^2 mod p -x1
  ecc_modsub(tmp2,tmp1,Q1.x,curve_params);			 
  //x3  =Lambda^2 mod p -x1 -x2
  ecc_modsub(Q3.x,tmp2,Q1.x,curve_params);			 
  //tmp2=x1-x3
  ecc_modsub(tmp2,Q1.x,Q3.x,curve_params);
  //tmp1=Lambda * (x1-x3)				
  ecc_modmult(tmp1,lambda,tmp2,curve_params);  
  //y3=Lambda * (x1-x3) -y1
  ecc_modsub(Q3.y,tmp1,Q1.y,curve_params);			
  return(UCL_OK);
}

