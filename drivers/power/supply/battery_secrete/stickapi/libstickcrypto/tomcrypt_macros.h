/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* ---- HELPER MACROS ---- */
#ifdef ENDIAN_NEUTRAL

#define STORE32L(x, y)                                                                     \
  do { (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); } while(0)

#define LOAD32L(x, y)                            \
  do { x = ((ulong32)((y)[3] & 255)<<24) | \
           ((ulong32)((y)[2] & 255)<<16) | \
           ((ulong32)((y)[1] & 255)<<8)  | \
           ((ulong32)((y)[0] & 255)); } while(0)

#define STORE64L(x, y)                                                                     \
  do { (y)[7] = (unsigned char)(((x)>>56)&255); (y)[6] = (unsigned char)(((x)>>48)&255);   \
       (y)[5] = (unsigned char)(((x)>>40)&255); (y)[4] = (unsigned char)(((x)>>32)&255);   \
       (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); } while(0)

#define LOAD64L(x, y)                                                       \
  do { x = (((ulong64)((y)[7] & 255))<<56)|(((ulong64)((y)[6] & 255))<<48)| \
           (((ulong64)((y)[5] & 255))<<40)|(((ulong64)((y)[4] & 255))<<32)| \
           (((ulong64)((y)[3] & 255))<<24)|(((ulong64)((y)[2] & 255))<<16)| \
           (((ulong64)((y)[1] & 255))<<8)|(((ulong64)((y)[0] & 255))); } while(0)

#define STORE32H(x, y)                                                                     \
  do { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); } while(0)

#define LOAD32H(x, y)                            \
  do { x = ((ulong32)((y)[0] & 255)<<24) | \
           ((ulong32)((y)[1] & 255)<<16) | \
           ((ulong32)((y)[2] & 255)<<8)  | \
           ((ulong32)((y)[3] & 255)); } while(0)

#define STORE64H(x, y)                                                                     \
do { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); } while(0)

#define LOAD64H(x, y)                                                      \
do { x = (((ulong64)((y)[0] & 255))<<56)|(((ulong64)((y)[1] & 255))<<48) | \
         (((ulong64)((y)[2] & 255))<<40)|(((ulong64)((y)[3] & 255))<<32) | \
         (((ulong64)((y)[4] & 255))<<24)|(((ulong64)((y)[5] & 255))<<16) | \
         (((ulong64)((y)[6] & 255))<<8)|(((ulong64)((y)[7] & 255))); } while(0)


#elif defined(ENDIAN_LITTLE)

#ifdef LTC_HAVE_BSWAP_BUILTIN

#define STORE32H(x, y)                          \
do { ulong32 ttt = __builtin_bswap32 ((x));     \
      XMEMCPY ((y), &ttt, 4); } while(0)

#define LOAD32H(x, y)                           \
do { XMEMCPY (&(x), (y), 4);                    \
      (x) = __builtin_bswap32 ((x)); } while(0)

#elif !defined(LTC_NO_BSWAP) && (defined(INTEL_CC) || (defined(__GNUC__) && (defined(__DJGPP__) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__i386__) || defined(__x86_64__))))

#define STORE32H(x, y)           \
asm __volatile__ (               \
   "bswapl %0     \n\t"          \
   "movl   %0,(%1)\n\t"          \
   "bswapl %0     \n\t"          \
      ::"r"(x), "r"(y): "memory");

#define LOAD32H(x, y)          \
asm __volatile__ (             \
   "movl (%1),%0\n\t"          \
   "bswapl %0\n\t"             \
   :"=r"(x): "r"(y): "memory");

#else

#define STORE32H(x, y)                                                                     \
  do { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); } while(0)

#define LOAD32H(x, y)                            \
  do { x = ((ulong32)((y)[0] & 255)<<24) | \
           ((ulong32)((y)[1] & 255)<<16) | \
           ((ulong32)((y)[2] & 255)<<8)  | \
           ((ulong32)((y)[3] & 255)); } while(0)

#endif

#ifdef LTC_HAVE_BSWAP_BUILTIN

#define STORE64H(x, y)                          \
do { ulong64 ttt = __builtin_bswap64 ((x));     \
      XMEMCPY ((y), &ttt, 8); } while(0)

#define LOAD64H(x, y)                           \
do { XMEMCPY (&(x), (y), 8);                    \
      (x) = __builtin_bswap64 ((x)); } while(0)

/* x86_64 processor */
#elif !defined(LTC_NO_BSWAP) && (defined(__GNUC__) && defined(__x86_64__))

#define STORE64H(x, y)           \
asm __volatile__ (               \
   "bswapq %0     \n\t"          \
   "movq   %0,(%1)\n\t"          \
   "bswapq %0     \n\t"          \
   ::"r"(x), "r"(y): "memory");

#define LOAD64H(x, y)          \
asm __volatile__ (             \
   "movq (%1),%0\n\t"          \
   "bswapq %0\n\t"             \
   :"=r"(x): "r"(y): "memory");

#else

#define STORE64H(x, y)                                                                     \
do { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); } while(0)

#define LOAD64H(x, y)                                                      \
do { x = (((ulong64)((y)[0] & 255))<<56)|(((ulong64)((y)[1] & 255))<<48) | \
         (((ulong64)((y)[2] & 255))<<40)|(((ulong64)((y)[3] & 255))<<32) | \
         (((ulong64)((y)[4] & 255))<<24)|(((ulong64)((y)[5] & 255))<<16) | \
         (((ulong64)((y)[6] & 255))<<8)|(((ulong64)((y)[7] & 255))); } while(0)

#endif

#ifdef ENDIAN_32BITWORD

#define STORE32L(x, y)        \
  do { ulong32  ttt = (x); XMEMCPY(y, &ttt, 4); } while(0)

#define LOAD32L(x, y)         \
  do { XMEMCPY(&(x), y, 4); } while(0)

#define STORE64L(x, y)                                                                     \
  do { (y)[7] = (unsigned char)(((x)>>56)&255); (y)[6] = (unsigned char)(((x)>>48)&255);   \
       (y)[5] = (unsigned char)(((x)>>40)&255); (y)[4] = (unsigned char)(((x)>>32)&255);   \
       (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); } while(0)

#define LOAD64L(x, y)                                                       \
  do { x = (((ulong64)((y)[7] & 255))<<56)|(((ulong64)((y)[6] & 255))<<48)| \
           (((ulong64)((y)[5] & 255))<<40)|(((ulong64)((y)[4] & 255))<<32)| \
           (((ulong64)((y)[3] & 255))<<24)|(((ulong64)((y)[2] & 255))<<16)| \
           (((ulong64)((y)[1] & 255))<<8)|(((ulong64)((y)[0] & 255))); } while(0)

#else /* 64-bit words then  */

#define STORE32L(x, y)        \
  do { ulong32 ttt = (x); XMEMCPY(y, &ttt, 4); } while(0)

#define LOAD32L(x, y)         \
  do { XMEMCPY(&(x), y, 4); x &= 0xFFFFFFFF; } while(0)

#define STORE64L(x, y)        \
  do { ulong64 ttt = (x); XMEMCPY(y, &ttt, 8); } while(0)

#define LOAD64L(x, y)         \
  do { XMEMCPY(&(x), y, 8); } while(0)

#endif /* ENDIAN_64BITWORD */

#elif defined(ENDIAN_BIG)

#define STORE32L(x, y)                                                                     \
  do { (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); } while(0)

#define LOAD32L(x, y)                            \
  do { x = ((ulong32)((y)[3] & 255)<<24) | \
           ((ulong32)((y)[2] & 255)<<16) | \
           ((ulong32)((y)[1] & 255)<<8)  | \
           ((ulong32)((y)[0] & 255)); } while(0)

#define STORE64L(x, y)                                                                     \
do { (y)[7] = (unsigned char)(((x)>>56)&255); (y)[6] = (unsigned char)(((x)>>48)&255);     \
     (y)[5] = (unsigned char)(((x)>>40)&255); (y)[4] = (unsigned char)(((x)>>32)&255);     \
     (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);     \
     (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); } while(0)

#define LOAD64L(x, y)                                                      \
do { x = (((ulong64)((y)[7] & 255))<<56)|(((ulong64)((y)[6] & 255))<<48) | \
         (((ulong64)((y)[5] & 255))<<40)|(((ulong64)((y)[4] & 255))<<32) | \
         (((ulong64)((y)[3] & 255))<<24)|(((ulong64)((y)[2] & 255))<<16) | \
         (((ulong64)((y)[1] & 255))<<8)|(((ulong64)((y)[0] & 255))); } while(0)

#ifdef ENDIAN_32BITWORD

#define STORE32H(x, y)        \
  do { ulong32 ttt = (x); XMEMCPY(y, &ttt, 4); } while(0)

#define LOAD32H(x, y)         \
  do { XMEMCPY(&(x), y, 4); } while(0)

#define STORE64H(x, y)                                                                     \
  do { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);   \
       (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);   \
       (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);   \
       (y)[6] = (unsigned char)(((x)>>8)&255);  (y)[7] = (unsigned char)((x)&255); } while(0)

#define LOAD64H(x, y)                                                       \
  do { x = (((ulong64)((y)[0] & 255))<<56)|(((ulong64)((y)[1] & 255))<<48)| \
           (((ulong64)((y)[2] & 255))<<40)|(((ulong64)((y)[3] & 255))<<32)| \
           (((ulong64)((y)[4] & 255))<<24)|(((ulong64)((y)[5] & 255))<<16)| \
           (((ulong64)((y)[6] & 255))<<8)| (((ulong64)((y)[7] & 255))); } while(0)

#else /* 64-bit words then  */

#define STORE32H(x, y)        \
  do { ulong32 ttt = (x); XMEMCPY(y, &ttt, 4); } while(0)

#define LOAD32H(x, y)         \
  do { XMEMCPY(&(x), y, 4); x &= 0xFFFFFFFF; } while(0)

#define STORE64H(x, y)        \
  do { ulong64 ttt = (x); XMEMCPY(y, &ttt, 8); } while(0)

#define LOAD64H(x, y)         \
  do { XMEMCPY(&(x), y, 8); } while(0)

#endif /* ENDIAN_64BITWORD */
#endif /* ENDIAN_BIG */

#define BSWAP(x)  ( ((x>>24)&0x000000FFUL) | ((x<<24)&0xFF000000UL)  | \
                    ((x>>8)&0x0000FF00UL)  | ((x<<8)&0x00FF0000UL) )


/* 32-bit Rotates */
#if defined(_MSC_VER)
#define LTC_ROx_BUILTIN

/* instrinsic rotate */
#include <stdlib.h>
#pragma intrinsic(_rotr,_rotl)
#define ROR(x,n) _rotr(x,n)
#define ROL(x,n) _rotl(x,n)
#define RORc(x,n) ROR(x,n)
#define ROLc(x,n) ROL(x,n)

#elif defined(LTC_HAVE_ROTATE_BUILTIN)
#define LTC_ROx_BUILTIN

#define ROR(x,n) __builtin_rotateright32(x,n)
#define ROL(x,n) __builtin_rotateleft32(x,n)
#define ROLc(x,n) ROL(x,n)
#define RORc(x,n) ROR(x,n)

#elif !defined(__STRICT_ANSI__) && defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(INTEL_CC) && !defined(LTC_NO_ASM)
#define LTC_ROx_ASM

static inline ulong32 ROL(ulong32 word, int i)
{
   asm ("roll %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

static inline ulong32 ROR(ulong32 word, int i)
{
   asm ("rorl %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

#ifndef LTC_NO_ROLC

#define ROLc(word,i) ({ \
   ulong32 ROLc_tmp = (word); \
   __asm__ ("roll %2, %0" : \
            "=r" (ROLc_tmp) : \
            "0" (ROLc_tmp), \
            "I" (i)); \
            ROLc_tmp; \
   })
#define RORc(word,i) ({ \
   ulong32 RORc_tmp = (word); \
   __asm__ ("rorl %2, %0" : \
            "=r" (RORc_tmp) : \
            "0" (RORc_tmp), \
            "I" (i)); \
            RORc_tmp; \
   })

#else

#define ROLc ROL
#define RORc ROR

#endif

#elif !defined(__STRICT_ANSI__) && defined(LTC_PPC32)
#define LTC_ROx_ASM

static inline ulong32 ROL(ulong32 word, int i)
{
   asm ("rotlw %0,%0,%2"
      :"=r" (word)
      :"0" (word),"r" (i));
   return word;
}

static inline ulong32 ROR(ulong32 word, int i)
{
   asm ("rotlw %0,%0,%2"
      :"=r" (word)
      :"0" (word),"r" (32-i));
   return word;
}

#ifndef LTC_NO_ROLC

static inline ulong32 ROLc(ulong32 word, const int i)
{
   asm ("rotlwi %0,%0,%2"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

static inline ulong32 RORc(ulong32 word, const int i)
{
   asm ("rotrwi %0,%0,%2"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

#else

#define ROLc ROL
#define RORc ROR

#endif


#else

/* rotates the hard way */
#define ROL(x, y) ( (((ulong32)(x)<<(ulong32)((y)&31)) | (((ulong32)(x)&0xFFFFFFFFUL)>>(ulong32)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define ROR(x, y) ( ((((ulong32)(x)&0xFFFFFFFFUL)>>(ulong32)((y)&31)) | ((ulong32)(x)<<(ulong32)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define ROLc(x, y) ( (((ulong32)(x)<<(ulong32)((y)&31)) | (((ulong32)(x)&0xFFFFFFFFUL)>>(ulong32)((32-((y)&31))&31))) & 0xFFFFFFFFUL)
#define RORc(x, y) ( ((((ulong32)(x)&0xFFFFFFFFUL)>>(ulong32)((y)&31)) | ((ulong32)(x)<<(ulong32)((32-((y)&31))&31))) & 0xFFFFFFFFUL)

#endif


/* 64-bit Rotates */
#if defined(_MSC_VER)

/* instrinsic rotate */
#include <stdlib.h>
#pragma intrinsic(_rotr64,_rotr64)
#define ROR64(x,n) _rotr64(x,n)
#define ROL64(x,n) _rotl64(x,n)
#define ROR64c(x,n) ROR64(x,n)
#define ROL64c(x,n) ROL64(x,n)

#elif defined(LTC_HAVE_ROTATE_BUILTIN)

#define ROR64(x,n) __builtin_rotateright64(x,n)
#define ROL64(x,n) __builtin_rotateleft64(x,n)
#define ROR64c(x,n) ROR64(x,n)
#define ROL64c(x,n) ROL64(x,n)

#elif !defined(__STRICT_ANSI__) && defined(__GNUC__) && defined(__x86_64__) && !defined(INTEL_CC) && !defined(LTC_NO_ASM)

static inline ulong64 ROL64(ulong64 word, int i)
{
   asm("rolq %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

static inline ulong64 ROR64(ulong64 word, int i)
{
   asm("rorq %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

#ifndef LTC_NO_ROLC

#define ROL64c(word,i) ({ \
   ulong64 ROL64c_tmp = word; \
   __asm__ ("rolq %2, %0" : \
            "=r" (ROL64c_tmp) : \
            "0" (ROL64c_tmp), \
            "J" (i)); \
            ROL64c_tmp; \
   })
#define ROR64c(word,i) ({ \
   ulong64 ROR64c_tmp = word; \
   __asm__ ("rorq %2, %0" : \
            "=r" (ROR64c_tmp) : \
            "0" (ROR64c_tmp), \
            "J" (i)); \
            ROR64c_tmp; \
   })

#else /* LTC_NO_ROLC */

#define ROL64c ROL64
#define ROR64c ROR64

#endif

#else /* Not x86_64  */

#define ROL64(x, y) \
    ( (((x)<<((ulong64)(y)&63)) | \
      (((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>(((ulong64)64-((y)&63))&63))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define ROR64(x, y) \
    ( ((((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)(y)&CONST64(63))) | \
      ((x)<<(((ulong64)64-((y)&63))&63))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define ROL64c(x, y) \
    ( (((x)<<((ulong64)(y)&63)) | \
      (((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>(((ulong64)64-((y)&63))&63))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define ROR64c(x, y) \
    ( ((((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)(y)&CONST64(63))) | \
      ((x)<<(((ulong64)64-((y)&63))&63))) & CONST64(0xFFFFFFFFFFFFFFFF))

#endif

#ifndef MAX
   #define MAX(x, y) ( ((x)>(y))?(x):(y) )
#endif

#ifndef MIN
   #define MIN(x, y) ( ((x)<(y))?(x):(y) )
#endif

#ifndef LTC_UNUSED_PARAM
   #define LTC_UNUSED_PARAM(x) (void)(x)
#endif

/* there is no snprintf before Visual C++ 2015 */
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif
