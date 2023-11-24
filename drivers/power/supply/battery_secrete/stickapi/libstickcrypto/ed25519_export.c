/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file ed25519_export.c
  Export an Ed25519 key to a binary packet, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
   Export an Ed25519 key to a binary packet
   @param out    [out] The destination for the key
   @param outlen [in/out] The max size and resulting size of the Ed25519 key
   @param type   Which type of key (PK_PRIVATE, PK_PUBLIC|PK_STD or PK_PUBLIC)
   @param key    The key you wish to export
   @return CRYPT_OK if successful
*/
int ed25519_export(       unsigned char *out, unsigned long *outlen,
                                    int  which,
                   const curve25519_key *key)
{
   LTC_ARGCHK(key != NULL);

   if (key->algo != PKA_ED25519) return CRYPT_PK_INVALID_TYPE;

   return ec25519_export(out, outlen, which, key);
}

#endif
