/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#include "tomcrypt_private.h"

/**
  @file crypt_cipher_is_valid.c
  Determine if cipher is valid, Tom St Denis
*/

/*
   Test if a cipher index is valid
   @param idx   The index of the cipher to search for
   @return CRYPT_OK if valid
*/
int cipher_is_valid(int idx)
{
   LTC_MUTEX_LOCK(&ltc_cipher_mutex);
   if (idx < 0 || idx >= TAB_SIZE || cipher_descriptor[idx].name == NULL) {
      LTC_MUTEX_UNLOCK(&ltc_cipher_mutex);
      return CRYPT_INVALID_CIPHER;
   }
   LTC_MUTEX_UNLOCK(&ltc_cipher_mutex);
   return CRYPT_OK;
}
