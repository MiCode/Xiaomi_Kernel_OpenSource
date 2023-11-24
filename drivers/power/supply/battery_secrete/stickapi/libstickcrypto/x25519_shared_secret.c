/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file x25519_shared_secret.c
  Create a X25519 shared secret, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
   Create a X25519 shared secret.
   @param private_key     The private X25519 key in the pair
   @param public_key      The public X25519 key in the pair
   @param out             [out] The destination of the shared data
   @param outlen          [in/out] The max size and resulting size of the shared data.
   @return CRYPT_OK if successful
*/
int x25519_shared_secret(const    curve25519_key *private_key,
                         const    curve25519_key *public_key,
                               unsigned char *out, unsigned long *outlen)
{
   LTC_ARGCHK(private_key        != NULL);
   LTC_ARGCHK(public_key         != NULL);
   LTC_ARGCHK(out                != NULL);
   LTC_ARGCHK(outlen             != NULL);

   if(private_key->type != PK_PRIVATE) return CRYPT_PK_INVALID_TYPE;

   if(*outlen < 32uL) {
      *outlen = 32uL;
      return CRYPT_BUFFER_OVERFLOW;
   }

   tweetnacl_crypto_scalarmult(out, private_key->priv, public_key->pub);
   *outlen = 32uL;

   return CRYPT_OK;
}

#endif
