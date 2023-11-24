/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* ---- HASH FUNCTIONS ---- */
#if defined(LTC_SHA3) || defined(LTC_KECCAK)
struct sha3_state {
    ulong64 saved;                  /* the portion of the input message that we didn't consume yet */
    ulong64 s[25];
    unsigned char sb[25 * 8];       /* used for storing `ulong64 s[25]` as little-endian bytes */
    unsigned short byte_index;      /* 0..7--the next byte after the set one (starts from 0; 0--none are buffered) */
    unsigned short word_index;      /* 0..24--the next word to integrate input (starts from 0) */
    unsigned short capacity_words;  /* the double size of the hash output in words (e.g. 16 for Keccak 512) */
    unsigned short xof_flag;
};
#endif

#ifdef LTC_SHA512
struct sha512_state {
    ulong64  length, state[8];
    unsigned long curlen;
    unsigned char buf[128];
};
#endif

#ifdef LTC_SHA256
struct sha256_state {
    ulong64 length;
    ulong32 state[8], curlen;
    unsigned char buf[64];
};
#endif

#ifdef LTC_SHA1
struct sha1_state {
    ulong64 length;
    ulong32 state[5], curlen;
    unsigned char buf[64];
};
#endif

#ifdef LTC_MD5
struct md5_state {
    ulong64 length;
    ulong32 state[4], curlen;
    unsigned char buf[64];
};
#endif

#ifdef LTC_MD4
struct md4_state {
    ulong64 length;
    ulong32 state[4], curlen;
    unsigned char buf[64];
};
#endif

#ifdef LTC_TIGER
struct tiger_state {
    ulong64 state[3], length;
    unsigned long curlen;
    unsigned char buf[64];
};
#endif

#ifdef LTC_MD2
struct md2_state {
    unsigned char chksum[16], X[48], buf[16];
    unsigned long curlen;
};
#endif

#ifdef LTC_RIPEMD128
struct rmd128_state {
    ulong64 length;
    unsigned char buf[64];
    ulong32 curlen, state[4];
};
#endif

#ifdef LTC_RIPEMD160
struct rmd160_state {
    ulong64 length;
    unsigned char buf[64];
    ulong32 curlen, state[5];
};
#endif

#ifdef LTC_RIPEMD256
struct rmd256_state {
    ulong64 length;
    unsigned char buf[64];
    ulong32 curlen, state[8];
};
#endif

#ifdef LTC_RIPEMD320
struct rmd320_state {
    ulong64 length;
    unsigned char buf[64];
    ulong32 curlen, state[10];
};
#endif

#ifdef LTC_WHIRLPOOL
struct whirlpool_state {
    ulong64 length, state[8];
    unsigned char buf[64];
    ulong32 curlen;
};
#endif

#ifdef LTC_CHC_HASH
struct chc_state {
    ulong64 length;
    unsigned char state[MAXBLOCKSIZE], buf[MAXBLOCKSIZE];
    ulong32 curlen;
};
#endif

#ifdef LTC_BLAKE2S
struct blake2s_state {
    ulong32 h[8];
    ulong32 t[2];
    ulong32 f[2];
    unsigned char buf[64];
    unsigned long curlen;
    unsigned long outlen;
    unsigned char last_node;
};
#endif

#ifdef LTC_BLAKE2B
struct blake2b_state {
    ulong64 h[8];
    ulong64 t[2];
    ulong64 f[2];
    unsigned char buf[128];
    unsigned long curlen;
    unsigned long outlen;
    unsigned char last_node;
};
#endif

typedef union Hash_state {
    char dummy[1];
#ifdef LTC_CHC_HASH
    struct chc_state chc;
#endif
#ifdef LTC_WHIRLPOOL
    struct whirlpool_state whirlpool;
#endif
#if defined(LTC_SHA3) || defined(LTC_KECCAK)
    struct sha3_state sha3;
#endif
#ifdef LTC_SHA512
    struct sha512_state sha512;
#endif
#ifdef LTC_SHA256
    struct sha256_state sha256;
#endif
#ifdef LTC_SHA1
    struct sha1_state   sha1;
#endif
#ifdef LTC_MD5
    struct md5_state    md5;
#endif
#ifdef LTC_MD4
    struct md4_state    md4;
#endif
#ifdef LTC_MD2
    struct md2_state    md2;
#endif
#ifdef LTC_TIGER
    struct tiger_state  tiger;
#endif
#ifdef LTC_RIPEMD128
    struct rmd128_state rmd128;
#endif
#ifdef LTC_RIPEMD160
    struct rmd160_state rmd160;
#endif
#ifdef LTC_RIPEMD256
    struct rmd256_state rmd256;
#endif
#ifdef LTC_RIPEMD320
    struct rmd320_state rmd320;
#endif
#ifdef LTC_BLAKE2S
    struct blake2s_state blake2s;
#endif
#ifdef LTC_BLAKE2B
    struct blake2b_state blake2b;
#endif

    void *data;
} hash_state;

/** hash descriptor */
extern  struct ltc_hash_descriptor {
    /** name of hash */
    const char *name;
    /** internal ID */
    unsigned char ID;
    /** Size of digest in octets */
    unsigned long hashsize;
    /** Input block size in octets */
    unsigned long blocksize;
    /** ASN.1 OID */
    unsigned long OID[16];
    /** Length of DER encoding */
    unsigned long OIDlen;

    /** Init a hash state
      @param hash   The hash to initialize
      @return CRYPT_OK if successful
    */
    int (*init)(hash_state *hash);
    /** Process a block of data
      @param hash   The hash state
      @param in     The data to hash
      @param inlen  The length of the data (octets)
      @return CRYPT_OK if successful
    */
    int (*process)(hash_state *hash, const unsigned char *in, unsigned long inlen);
    /** Produce the digest and store it
      @param hash   The hash state
      @param out    [out] The destination of the digest
      @return CRYPT_OK if successful
    */
    int (*done)(hash_state *hash, unsigned char *out);
    /** Self-test
      @return CRYPT_OK if successful, CRYPT_NOP if self-tests have been disabled
    */
    int (*test)(void);

    /* accelerated hmac callback: if you need to-do multiple packets just use the generic hmac_memory and provide a hash callback */
    int  (*hmac_block)(const unsigned char *key, unsigned long  keylen,
                       const unsigned char *in,  unsigned long  inlen,
                             unsigned char *out, unsigned long *outlen);

} hash_descriptor[];

#ifdef LTC_CHC_HASH
int chc_register(int cipher);
int chc_init(hash_state * md);
int chc_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int chc_done(hash_state * md, unsigned char *out);
int chc_test(void);
extern const struct ltc_hash_descriptor chc_desc;
#endif

#ifdef LTC_WHIRLPOOL
int whirlpool_init(hash_state * md);
int whirlpool_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int whirlpool_done(hash_state * md, unsigned char *out);
int whirlpool_test(void);
extern const struct ltc_hash_descriptor whirlpool_desc;
#endif

#if defined(LTC_SHA3) || defined(LTC_KECCAK)
/* sha3_NNN_init are shared by SHA3 and KECCAK */
int sha3_512_init(hash_state * md);
int sha3_384_init(hash_state * md);
int sha3_256_init(hash_state * md);
int sha3_224_init(hash_state * md);
/* sha3_process is the same for all variants of SHA3 + KECCAK */
int sha3_process(hash_state * md, const unsigned char *in, unsigned long inlen);
#endif

#ifdef LTC_SHA3
int sha3_512_test(void);
extern const struct ltc_hash_descriptor sha3_512_desc;
int sha3_384_test(void);
extern const struct ltc_hash_descriptor sha3_384_desc;
int sha3_256_test(void);
extern const struct ltc_hash_descriptor sha3_256_desc;
int sha3_224_test(void);
extern const struct ltc_hash_descriptor sha3_224_desc;
int sha3_done(hash_state *md, unsigned char *out);
/* SHAKE128 + SHAKE256 */
int sha3_shake_init(hash_state *md, int num);
#define sha3_shake_process(a,b,c) sha3_process(a,b,c)
int sha3_shake_done(hash_state *md, unsigned char *out, unsigned long outlen);
int sha3_shake_test(void);
int sha3_shake_memory(int num, const unsigned char *in, unsigned long inlen, unsigned char *out, const unsigned long *outlen);
#endif

#ifdef LTC_KECCAK
#define keccak_512_init(a)    sha3_512_init(a)
#define keccak_384_init(a)    sha3_384_init(a)
#define keccak_256_init(a)    sha3_256_init(a)
#define keccak_224_init(a)    sha3_224_init(a)
#define keccak_process(a,b,c) sha3_process(a,b,c)
extern const struct ltc_hash_descriptor keccak_512_desc;
int keccak_512_test(void);
extern const struct ltc_hash_descriptor keccak_384_desc;
int keccak_384_test(void);
extern const struct ltc_hash_descriptor keccak_256_desc;
int keccak_256_test(void);
extern const struct ltc_hash_descriptor keccak_224_desc;
int keccak_224_test(void);
int keccak_done(hash_state *md, unsigned char *out);
#endif

#ifdef LTC_SHA512
int sha512_init(hash_state * md);
int sha512_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int sha512_done(hash_state * md, unsigned char *out);
int sha512_test(void);
extern const struct ltc_hash_descriptor sha512_desc;
#endif

#ifdef LTC_SHA384
#ifndef LTC_SHA512
   #error LTC_SHA512 is required for LTC_SHA384
#endif
int sha384_init(hash_state * md);
#define sha384_process sha512_process
int sha384_done(hash_state * md, unsigned char *out);
int sha384_test(void);
extern const struct ltc_hash_descriptor sha384_desc;
#endif

#ifdef LTC_SHA512_256
#ifndef LTC_SHA512
   #error LTC_SHA512 is required for LTC_SHA512_256
#endif
int sha512_256_init(hash_state * md);
#define sha512_256_process sha512_process
int sha512_256_done(hash_state * md, unsigned char *out);
int sha512_256_test(void);
extern const struct ltc_hash_descriptor sha512_256_desc;
#endif

#ifdef LTC_SHA512_224
#ifndef LTC_SHA512
   #error LTC_SHA512 is required for LTC_SHA512_224
#endif
int sha512_224_init(hash_state * md);
#define sha512_224_process sha512_process
int sha512_224_done(hash_state * md, unsigned char *out);
int sha512_224_test(void);
extern const struct ltc_hash_descriptor sha512_224_desc;
#endif

#ifdef LTC_SHA256
int sha256_init(hash_state * md);
int sha256_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int sha256_done(hash_state * md, unsigned char *out);
int sha256_test(void);
extern const struct ltc_hash_descriptor sha256_desc;

#ifdef LTC_SHA224
#ifndef LTC_SHA256
   #error LTC_SHA256 is required for LTC_SHA224
#endif
int sha224_init(hash_state * md);
#define sha224_process sha256_process
int sha224_done(hash_state * md, unsigned char *out);
int sha224_test(void);
extern const struct ltc_hash_descriptor sha224_desc;
#endif
#endif

#ifdef LTC_SHA1
int sha1_init(hash_state * md);
int sha1_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int sha1_done(hash_state * md, unsigned char *out);
int sha1_test(void);
extern const struct ltc_hash_descriptor sha1_desc;
#endif

#ifdef LTC_BLAKE2S
extern const struct ltc_hash_descriptor blake2s_256_desc;
int blake2s_256_init(hash_state * md);
int blake2s_256_test(void);

extern const struct ltc_hash_descriptor blake2s_224_desc;
int blake2s_224_init(hash_state * md);
int blake2s_224_test(void);

extern const struct ltc_hash_descriptor blake2s_160_desc;
int blake2s_160_init(hash_state * md);
int blake2s_160_test(void);

extern const struct ltc_hash_descriptor blake2s_128_desc;
int blake2s_128_init(hash_state * md);
int blake2s_128_test(void);

int blake2s_init(hash_state * md, unsigned long outlen, const unsigned char *key, unsigned long keylen);
int blake2s_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int blake2s_done(hash_state * md, unsigned char *out);
#endif

#ifdef LTC_BLAKE2B
extern const struct ltc_hash_descriptor blake2b_512_desc;
int blake2b_512_init(hash_state * md);
int blake2b_512_test(void);

extern const struct ltc_hash_descriptor blake2b_384_desc;
int blake2b_384_init(hash_state * md);
int blake2b_384_test(void);

extern const struct ltc_hash_descriptor blake2b_256_desc;
int blake2b_256_init(hash_state * md);
int blake2b_256_test(void);

extern const struct ltc_hash_descriptor blake2b_160_desc;
int blake2b_160_init(hash_state * md);
int blake2b_160_test(void);

int blake2b_init(hash_state * md, unsigned long outlen, const unsigned char *key, unsigned long keylen);
int blake2b_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int blake2b_done(hash_state * md, unsigned char *out);
#endif

#ifdef LTC_MD5
int md5_init(hash_state * md);
int md5_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int md5_done(hash_state * md, unsigned char *out);
int md5_test(void);
extern const struct ltc_hash_descriptor md5_desc;
#endif

#ifdef LTC_MD4
int md4_init(hash_state * md);
int md4_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int md4_done(hash_state * md, unsigned char *out);
int md4_test(void);
extern const struct ltc_hash_descriptor md4_desc;
#endif

#ifdef LTC_MD2
int md2_init(hash_state * md);
int md2_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int md2_done(hash_state * md, unsigned char *out);
int md2_test(void);
extern const struct ltc_hash_descriptor md2_desc;
#endif

#ifdef LTC_TIGER
int tiger_init(hash_state * md);
int tiger_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int tiger_done(hash_state * md, unsigned char *out);
int tiger_test(void);
extern const struct ltc_hash_descriptor tiger_desc;
#endif

#ifdef LTC_RIPEMD128
int rmd128_init(hash_state * md);
int rmd128_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int rmd128_done(hash_state * md, unsigned char *out);
int rmd128_test(void);
extern const struct ltc_hash_descriptor rmd128_desc;
#endif

#ifdef LTC_RIPEMD160
int rmd160_init(hash_state * md);
int rmd160_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int rmd160_done(hash_state * md, unsigned char *out);
int rmd160_test(void);
extern const struct ltc_hash_descriptor rmd160_desc;
#endif

#ifdef LTC_RIPEMD256
int rmd256_init(hash_state * md);
int rmd256_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int rmd256_done(hash_state * md, unsigned char *out);
int rmd256_test(void);
extern const struct ltc_hash_descriptor rmd256_desc;
#endif

#ifdef LTC_RIPEMD320
int rmd320_init(hash_state * md);
int rmd320_process(hash_state * md, const unsigned char *in, unsigned long inlen);
int rmd320_done(hash_state * md, unsigned char *out);
int rmd320_test(void);
extern const struct ltc_hash_descriptor rmd320_desc;
#endif


int find_hash(const char *name);
int find_hash_id(unsigned char ID);
int find_hash_oid(const unsigned long *ID, unsigned long IDlen);
int find_hash_any(const char *name, int digestlen);
int register_hash(const struct ltc_hash_descriptor *hash);
int unregister_hash(const struct ltc_hash_descriptor *hash);
int register_all_hashes(void);
int hash_is_valid(int idx);

LTC_MUTEX_PROTO(ltc_hash_mutex)

int hash_memory(int hash,
                const unsigned char *in,  unsigned long inlen,
                      unsigned char *out, unsigned long *outlen);
int hash_memory_multi(int hash, unsigned char *out, unsigned long *outlen,
                      const unsigned char *in, unsigned long inlen, ...);

#ifndef LTC_NO_FILE
int hash_filehandle(int hash, FILE *in, unsigned char *out, unsigned long *outlen);
int hash_file(int hash, const char *fname, unsigned char *out, unsigned long *outlen);
#endif
