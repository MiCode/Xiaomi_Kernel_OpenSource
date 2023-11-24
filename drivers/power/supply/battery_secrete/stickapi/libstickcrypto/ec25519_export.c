/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file ec25519_export.c
  Generic export of a Curve/Ed25519 key to a binary packet, Steffen Jaeckel
*/

#ifdef LTC_CURVE25519

/**
   Generic export of a Curve/Ed25519 key to a binary packet
   @param out    [out] The destination for the key
   @param outlen [in/out] The max size and resulting size of the Ed25519 key
   @param type   Which type of key (PK_PRIVATE, PK_PUBLIC|PK_STD or PK_PUBLIC)
   @param key    The key you wish to export
   @return CRYPT_OK if successful
*/
int ec25519_export(       unsigned char *out, unsigned long *outlen,
                                    int  which,
                   const curve25519_key *key)
{
   int err, std;
#ifndef LTC_STICK
   const char* OID;
   unsigned long oid[16], oidlen;
   ltc_asn1_list alg_id[1];
   unsigned char private_key[34];
   unsigned long version, private_key_len = sizeof(private_key);
#endif // LTC_STICK

   LTC_ARGCHK(out       != NULL);
   LTC_ARGCHK(outlen    != NULL);
   LTC_ARGCHK(key       != NULL);

   std = which & PK_STD;
   which &= ~PK_STD;

   if (which == PK_PRIVATE) {
      if(key->type != PK_PRIVATE) return CRYPT_PK_INVALID_TYPE;

#ifndef LTC_STICK
      if (std == PK_STD) {
         if ((err = pk_get_oid(key->algo, &OID)) != CRYPT_OK) {
            return err;
         }
         oidlen = sizeof(oid)/sizeof(oid[0]);
         if ((err = pk_oid_str_to_num(OID, oid, &oidlen)) != CRYPT_OK) {
            return err;
         }

         LTC_SET_ASN1(alg_id, 0, LTC_ASN1_OBJECT_IDENTIFIER, oid, oidlen);

         /* encode private key as PKCS#8 */
         if ((err = der_encode_octet_string(key->priv, 32uL, private_key, &private_key_len)) != CRYPT_OK) {
            return err;
         }

         version = 0;
         err = der_encode_sequence_multi(out, outlen,
                                   LTC_ASN1_SHORT_INTEGER,            1uL, &version,
                                   LTC_ASN1_SEQUENCE,                 1uL, alg_id,
                                   LTC_ASN1_OCTET_STRING, private_key_len, private_key,
                                   LTC_ASN1_EOL,                      0uL, NULL);
      } else {
#endif // LTC_STICK
         if (*outlen < sizeof(key->priv)) {
            err = CRYPT_BUFFER_OVERFLOW;
         } else {
            XMEMCPY(out, key->priv, sizeof(key->priv));
            err = CRYPT_OK;
         }
         *outlen = sizeof(key->priv);
#ifndef LTC_STICK
      }
#endif // LTC_STICK
   } else {
#ifndef LTC_STICK
     if (std == PK_STD) {
         /* encode public key as SubjectPublicKeyInfo */
         err = x509_encode_subject_public_key_info(out, outlen, key->algo, key->pub, 32uL, LTC_ASN1_EOL, NULL, 0);
      } else {
#endif // LTC_STICK
         if (*outlen < sizeof(key->pub)) {
            err = CRYPT_BUFFER_OVERFLOW;
         } else {
            XMEMCPY(out, key->pub, sizeof(key->pub));
            err = CRYPT_OK;
         }
         *outlen = sizeof(key->pub);
#ifndef LTC_STICK
      }
#endif // LTC_STICK
   }

   return err;
}

#endif
