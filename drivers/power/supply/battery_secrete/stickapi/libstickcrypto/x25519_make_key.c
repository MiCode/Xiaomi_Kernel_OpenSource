/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file x25519_make_key.c
  Create a X25519 key, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
   Create a X25519 key
   @param prng     An active PRNG state
   @param wprng    The index of the PRNG desired
   @param key      [out] Destination of a newly created private key pair
   @return CRYPT_OK if successful
*/
int x25519_make_key(prng_state *prng, int wprng, curve25519_key *key)
{
   int err;

   LTC_ARGCHK(prng != NULL);
   LTC_ARGCHK(key  != NULL);

   if ((err = prng_is_valid(wprng)) != CRYPT_OK) {
      return err;
   }

   if (prng_descriptor[wprng].read(key->priv, sizeof(key->priv), prng) != sizeof(key->priv)) {
      return CRYPT_ERROR_READPRNG;
   }

   tweetnacl_crypto_scalarmult_base(key->pub, key->priv);

   key->type = PK_PRIVATE;
   key->algo = PKA_X25519;

   return err;
}

#endif
