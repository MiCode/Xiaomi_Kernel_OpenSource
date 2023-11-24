/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file ed25519_import_x509.c
  Import an Ed25519 key from a X.509 certificate, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

static int s_ed25519_decode(const unsigned char *in, unsigned long inlen, curve25519_key *key)
{
   if (inlen != sizeof(key->pub)) return CRYPT_PK_INVALID_SIZE;
   XMEMCPY(key->pub, in, sizeof(key->pub));
   return CRYPT_OK;
}

/**
  Import an Ed25519 public key from a X.509 certificate
  @param in     The DER encoded X.509 certificate
  @param inlen  The length of the certificate
  @param key    [out] Where to import the key to
  @return CRYPT_OK if successful, on error all allocated memory is freed automatically
*/
int ed25519_import_x509(const unsigned char *in, unsigned long inlen, curve25519_key *key)
{
   int err;

   LTC_ARGCHK(in  != NULL);
   LTC_ARGCHK(key != NULL);

   if ((err = x509_decode_public_key_from_certificate(in, inlen,
                                                      PKA_ED25519,
                                                      LTC_ASN1_EOL, NULL, NULL,
                                                      (public_key_decode_cb)s_ed25519_decode, key)) != CRYPT_OK) {
      return err;
   }
   key->type = PK_PUBLIC;
   key->algo = PKA_ED25519;

   return err;
}

#endif
