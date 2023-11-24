/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file ed25519_make_key.c
  Create an Ed25519 key, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
   Create an Ed25519 key
   @param prng     An active PRNG state
   @param wprng    The index of the PRNG desired
   @param key      [out] Destination of a newly created private key pair
   @return CRYPT_OK if successful
*/
int ed25519_make_key(prng_state *prng, int wprng, curve25519_key *key)
{
   int err;

   LTC_ARGCHK(prng != NULL);
   LTC_ARGCHK(key  != NULL);

   if ((err = tweetnacl_crypto_sign_keypair(prng, wprng, key->pub, key->priv)) != CRYPT_OK) {
      return err;
   }

   key->type = PK_PRIVATE;
   key->algo = PKA_ED25519;

   return err;
}

#endif
