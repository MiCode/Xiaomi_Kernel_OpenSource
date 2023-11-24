/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file ed25519_import_pkcs8.c
  Import an Ed25519 key in PKCS#8 format, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
  Import an Ed25519 private key in PKCS#8 format
  @param in        The DER-encoded PKCS#8-formatted private key
  @param inlen     The length of the input data
  @param passwd    The password to decrypt the private key
  @param passwdlen Password's length (octets)
  @param key       [out] Where to import the key to
  @return CRYPT_OK if successful, on error all allocated memory is freed automatically
*/
int ed25519_import_pkcs8(const unsigned char *in, unsigned long inlen,
                                  const void *pwd, unsigned long pwdlen,
                              curve25519_key *key)
{
   return ec25519_import_pkcs8(in, inlen, pwd, pwdlen, PKA_ED25519, tweetnacl_crypto_sk_to_pk, key);
}

#endif
