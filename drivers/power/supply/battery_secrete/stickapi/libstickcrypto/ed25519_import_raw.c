/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file ed25519_import_raw.c
  Set the parameters of an Ed25519 key, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
   Set the parameters of an Ed25519 key

   @param in       The key
   @param inlen    The length of the key
   @param which    Which type of key (PK_PRIVATE or PK_PUBLIC)
   @param key      [out] Destination of the key
   @return CRYPT_OK if successful
*/
int ed25519_import_raw(const unsigned char *in, unsigned long inlen, int which, curve25519_key *key)
{
   LTC_ARGCHK(in   != NULL);
   LTC_ARGCHK(inlen == 32uL);
   LTC_ARGCHK(key  != NULL);

   if (which == PK_PRIVATE) {
      XMEMCPY(key->priv, in, sizeof(key->priv));
      tweetnacl_crypto_sk_to_pk(key->pub, key->priv);
   } else if (which == PK_PUBLIC) {
      XMEMCPY(key->pub, in, sizeof(key->pub));
   } else {
      return CRYPT_INVALID_ARG;
   }
   key->algo = PKA_ED25519;
   key->type = which;

   return CRYPT_OK;
}

#endif
