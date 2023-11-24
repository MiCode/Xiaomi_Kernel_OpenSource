/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file ed25519_verify.c
  Verify an Ed25519 signature, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
   Verify an Ed25519 signature.
   @param private_key     The private Ed25519 key in the pair
   @param public_key      The public Ed25519 key in the pair
   @param out             [out] The destination of the shared data
   @param outlen          [in/out] The max size and resulting size of the shared data.
   @param stat            [out] The result of the signature verification, 1==valid, 0==invalid
   @return CRYPT_OK if successful
*/
int ed25519_verify(const  unsigned char *msg, unsigned long msglen,
                   const  unsigned char *sig, unsigned long siglen,
                   int *stat, const curve25519_key *public_key)
{
   unsigned char* m;
   unsigned long long mlen;
   int err;

   LTC_ARGCHK(msg        != NULL);
   LTC_ARGCHK(sig        != NULL);
   LTC_ARGCHK(stat       != NULL);
   LTC_ARGCHK(public_key != NULL);

   *stat = 0;

   if (siglen != 64uL) return CRYPT_INVALID_ARG;
   if (public_key->algo != PKA_ED25519) return CRYPT_PK_INVALID_TYPE;

   mlen = msglen + siglen;
   if ((mlen < msglen) || (mlen < siglen)) return CRYPT_OVERFLOW;

   m = XMALLOC(mlen);
   if (m == NULL) return CRYPT_MEM;

   XMEMCPY(m, sig, siglen);
   XMEMCPY(m + siglen, msg, msglen);

   err = tweetnacl_crypto_sign_open(stat,
                                    m, &mlen,
                                    m, mlen,
                                    public_key->pub);

#ifdef LTC_CLEAN_STACK
   zeromem(m, mlen);
#endif
   XFREE(m);

   return err;
}

#endif
