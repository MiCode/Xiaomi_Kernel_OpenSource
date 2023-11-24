/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file x25519_import.c
  Import a X25519 key from a SubjectPublicKeyInfo, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
  Import a X25519 key
  @param in     The packet to read
  @param inlen  The length of the input packet
  @param key    [out] Where to import the key to
  @return CRYPT_OK if successful, on error all allocated memory is freed automatically
*/
int x25519_import(const unsigned char *in, unsigned long inlen, curve25519_key *key)
{
   int err;
   unsigned long key_len;

   LTC_ARGCHK(in  != NULL);
   LTC_ARGCHK(key != NULL);

   key_len = sizeof(key->pub);
   if ((err = x509_decode_subject_public_key_info(in, inlen, PKA_X25519, key->pub, &key_len, LTC_ASN1_EOL, NULL, 0uL)) == CRYPT_OK) {
      key->type = PK_PUBLIC;
      key->algo = PKA_X25519;
   }
   return err;
}

#endif
