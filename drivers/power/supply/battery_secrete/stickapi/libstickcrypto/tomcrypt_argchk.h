/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* Defines the LTC_ARGCHK macro used within the library */
/* ARGTYPE is defined in tomcrypt_cfg.h */

/* ARGTYPE is per default defined to 0  */
#if ARGTYPE == 0

#include <signal.h>

LTC_NORETURN void crypt_argchk(const char *v, const char *s, int d);
#define LTC_ARGCHK(x) do { if (!(x)) { crypt_argchk(#x, __FILE__, __LINE__); } }while(0)
#define LTC_ARGCHKVD(x) do { if (!(x)) { crypt_argchk(#x, __FILE__, __LINE__); } }while(0)

#elif ARGTYPE == 1

/* fatal type of error */
#define LTC_ARGCHK(x) do { if (!(x)) { WARN_ON(1); } }while(0)
#define LTC_ARGCHKVD(x) LTC_ARGCHK(x)

#elif ARGTYPE == 2

#define LTC_ARGCHK(x) if (!(x)) { fprintf(stderr, "\nwarning: ARGCHK failed at %s:%d\n", __FILE__, __LINE__); }
#define LTC_ARGCHKVD(x) LTC_ARGCHK(x)

#elif ARGTYPE == 3

#define LTC_ARGCHK(x) LTC_UNUSED_PARAM(x)
#define LTC_ARGCHKVD(x) LTC_ARGCHK(x)

#elif ARGTYPE == 4

#define LTC_ARGCHK(x)   if (!(x)) return CRYPT_INVALID_ARG;
#define LTC_ARGCHKVD(x) if (!(x)) return;

#endif

