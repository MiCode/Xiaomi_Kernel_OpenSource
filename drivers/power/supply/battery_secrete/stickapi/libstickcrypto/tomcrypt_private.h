/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

#include "tomcrypt.h"

/*
 * Internal Macros
 */

#define LTC_PAD_MASK       (0xF000U)

/*
 * Internal Enums
 */

enum ltc_oid_id {
   PKA_RSA,
   PKA_DSA,
   PKA_EC,
   PKA_EC_PRIMEF,
   PKA_X25519,
   PKA_ED25519,
};

/*
 * Internal Types
 */

typedef struct {
  int size;
  const char *name, *base, *prime;
} ltc_dh_set_type;


typedef int (*fn_kdf_t)(const unsigned char *password, unsigned long password_len,
                              const unsigned char *salt,     unsigned long salt_len,
                              int iteration_count,  int hash_idx,
                              unsigned char *out,   unsigned long *outlen);

typedef struct {
   /* KDF */
   fn_kdf_t kdf;
   /* Hash or HMAC */
   const char* h;
   /* cipher */
   const char* c;
   unsigned long keylen;
   /* not used for pbkdf2 */
   unsigned long blocklen;
} pbes_properties;

typedef struct
{
   pbes_properties type;
   const void *pwd;
   unsigned long pwdlen;
   ltc_asn1_list *enc_data;
   ltc_asn1_list *salt;
   ltc_asn1_list *iv;
   unsigned long iterations;
   /* only used for RC2 */
   unsigned long key_bits;
} pbes_arg;

/*
 * Internal functions
 */


/* tomcrypt_cipher.h */

void blowfish_enc(ulong32 *data, unsigned long blocks, const symmetric_key *skey);
int blowfish_expand(const unsigned char *key, int keylen,
                    const unsigned char *data, int datalen,
                    symmetric_key *skey);
int blowfish_setup_with_data(const unsigned char *key, int keylen,
                             const unsigned char *data, int datalen,
                             symmetric_key *skey);

/* tomcrypt_hash.h */

/* a simple macro for making hash "process" functions */
#define HASH_PROCESS(func_name, compress_name, state_var, block_size)                       \
int func_name (hash_state * md, const unsigned char *in, unsigned long inlen)               \
{                                                                                           \
    unsigned long n;                                                                        \
    int           err;                                                                      \
    LTC_ARGCHK(md != NULL);                                                                 \
    LTC_ARGCHK(in != NULL);                                                                 \
    if (md-> state_var .curlen > sizeof(md-> state_var .buf)) {                             \
       return CRYPT_INVALID_ARG;                                                            \
    }                                                                                       \
    if ((md-> state_var .length + inlen * 8) < md-> state_var .length) {                        \
      return CRYPT_HASH_OVERFLOW;                                                           \
    }                                                                                       \
    while (inlen > 0) {                                                                     \
        if (md-> state_var .curlen == 0 && inlen >= block_size) {                           \
           if ((err = compress_name (md, in)) != CRYPT_OK) {                                \
              return err;                                                                   \
           }                                                                                \
           md-> state_var .length += block_size * 8;                                        \
           in             += block_size;                                                    \
           inlen          -= block_size;                                                    \
        } else {                                                                            \
           n = MIN(inlen, (block_size - md-> state_var .curlen));                           \
           XMEMCPY(md-> state_var .buf + md-> state_var.curlen, in, (size_t)n);             \
           md-> state_var .curlen += n;                                                     \
           in             += n;                                                             \
           inlen          -= n;                                                             \
           if (md-> state_var .curlen == block_size) {                                      \
              if ((err = compress_name (md, md-> state_var .buf)) != CRYPT_OK) {            \
                 return err;                                                                \
              }                                                                             \
              md-> state_var .length += 8*block_size;                                       \
              md-> state_var .curlen = 0;                                                   \
           }                                                                                \
       }                                                                                    \
    }                                                                                       \
    return CRYPT_OK;                                                                        \
}


/* tomcrypt_mac.h */

int ocb3_int_ntz(unsigned long x);
void ocb3_int_xor_blocks(unsigned char *out, const unsigned char *block_a, const unsigned char *block_b, unsigned long block_len);


/* tomcrypt_math.h */

#if !defined(DESC_DEF_ONLY)

#define MP_DIGIT_BIT                 ltc_mp.bits_per_digit

/* some handy macros */
#define mp_init(a)                   ltc_mp.init(a)
#define mp_init_multi                ltc_init_multi
#define mp_clear(a)                  ltc_mp.deinit(a)
#define mp_clear_multi               ltc_deinit_multi
#define mp_cleanup_multi             ltc_cleanup_multi
#define mp_init_copy(a, b)           ltc_mp.init_copy(a, b)

#define mp_neg(a, b)                 ltc_mp.neg(a, b)
#define mp_copy(a, b)                ltc_mp.copy(a, b)

#define mp_set(a, b)                 ltc_mp.set_int(a, b)
#define mp_set_int(a, b)             ltc_mp.set_int(a, b)
#define mp_get_int(a)                ltc_mp.get_int(a)
#define mp_get_digit(a, n)           ltc_mp.get_digit(a, n)
#define mp_get_digit_count(a)        ltc_mp.get_digit_count(a)
#define mp_cmp(a, b)                 ltc_mp.compare(a, b)
#define mp_cmp_d(a, b)               ltc_mp.compare_d(a, b)
#define mp_count_bits(a)             ltc_mp.count_bits(a)
#define mp_cnt_lsb(a)                ltc_mp.count_lsb_bits(a)
#define mp_2expt(a, b)               ltc_mp.twoexpt(a, b)

#define mp_read_radix(a, b, c)       ltc_mp.read_radix(a, b, c)
#define mp_toradix(a, b, c)          ltc_mp.write_radix(a, b, c)
#define mp_unsigned_bin_size(a)      ltc_mp.unsigned_size(a)
#define mp_to_unsigned_bin(a, b)     ltc_mp.unsigned_write(a, b)
#define mp_read_unsigned_bin(a, b, c) ltc_mp.unsigned_read(a, b, c)

#define mp_add(a, b, c)              ltc_mp.add(a, b, c)
#define mp_add_d(a, b, c)            ltc_mp.addi(a, b, c)
#define mp_sub(a, b, c)              ltc_mp.sub(a, b, c)
#define mp_sub_d(a, b, c)            ltc_mp.subi(a, b, c)
#define mp_mul(a, b, c)              ltc_mp.mul(a, b, c)
#define mp_mul_d(a, b, c)            ltc_mp.muli(a, b, c)
#define mp_sqr(a, b)                 ltc_mp.sqr(a, b)
#define mp_sqrtmod_prime(a, b, c)    ltc_mp.sqrtmod_prime(a, b, c)
#define mp_div(a, b, c, d)           ltc_mp.mpdiv(a, b, c, d)
#define mp_div_2(a, b)               ltc_mp.div_2(a, b)
#define mp_mod(a, b, c)              ltc_mp.mpdiv(a, b, NULL, c)
#define mp_mod_d(a, b, c)            ltc_mp.modi(a, b, c)
#define mp_gcd(a, b, c)              ltc_mp.gcd(a, b, c)
#define mp_lcm(a, b, c)              ltc_mp.lcm(a, b, c)

#define mp_addmod(a, b, c, d)        ltc_mp.addmod(a, b, c, d)
#define mp_submod(a, b, c, d)        ltc_mp.submod(a, b, c, d)
#define mp_mulmod(a, b, c, d)        ltc_mp.mulmod(a, b, c, d)
#define mp_sqrmod(a, b, c)           ltc_mp.sqrmod(a, b, c)
#define mp_invmod(a, b, c)           ltc_mp.invmod(a, b, c)

#define mp_montgomery_setup(a, b)    ltc_mp.montgomery_setup(a, b)
#define mp_montgomery_normalization(a, b) ltc_mp.montgomery_normalization(a, b)
#define mp_montgomery_reduce(a, b, c)   ltc_mp.montgomery_reduce(a, b, c)
#define mp_montgomery_free(a)        ltc_mp.montgomery_deinit(a)

#define mp_exptmod(a,b,c,d)          ltc_mp.exptmod(a,b,c,d)
#define mp_prime_is_prime(a, b, c)   ltc_mp.isprime(a, b, c)

#define mp_iszero(a)                 (mp_cmp_d(a, 0) == LTC_MP_EQ ? LTC_MP_YES : LTC_MP_NO)
#define mp_isodd(a)                  (mp_get_digit_count(a) > 0 ? (mp_get_digit(a, 0) & 1 ? LTC_MP_YES : LTC_MP_NO) : LTC_MP_NO)
#define mp_exch(a, b)                do { void *ABC__tmp = a; a = b; b = ABC__tmp; } while(0)

#define mp_tohex(a, b)               mp_toradix(a, b, 16)

#define mp_rand(a, b)                ltc_mp.rand(a, b)

#endif


/* tomcrypt_misc.h */

void copy_or_zeromem(const unsigned char* src, unsigned char* dest, unsigned long len, int coz);

int pbes_decrypt(const pbes_arg  *arg, unsigned char *dec_data, unsigned long *dec_size);

int pbes1_extract(const ltc_asn1_list *s, pbes_arg *res);
int pbes2_extract(const ltc_asn1_list *s, pbes_arg *res);


/* tomcrypt_pk.h */

int rand_bn_bits(void *N, int bits, prng_state *prng, int wprng);
int rand_bn_upto(void *N, void *limit, prng_state *prng, int wprng);

int pk_get_oid(enum ltc_oid_id id, const char **st);
int pk_oid_str_to_num(const char *OID, unsigned long *oid, unsigned long *oidlen);
int pk_oid_num_to_str(const unsigned long *oid, unsigned long oidlen, char *OID, unsigned long *outlen);

/* ---- DH Routines ---- */
#ifdef LTC_MRSA
int rsa_init(rsa_key *key);
void rsa_shrink_key(rsa_key *key);
#endif /* LTC_MRSA */

/* ---- DH Routines ---- */
#ifdef LTC_MDH
extern const ltc_dh_set_type ltc_dh_sets[];

int dh_check_pubkey(const dh_key *key);
#endif /* LTC_MDH */

/* ---- ECC Routines ---- */
#ifdef LTC_MECC
int ecc_set_curve_from_mpis(void *a, void *b, void *prime, void *order, void *gx, void *gy, unsigned long cofactor, ecc_key *key);
int ecc_copy_curve(const ecc_key *srckey, ecc_key *key);
int ecc_set_curve_by_size(int size, ecc_key *key);
int ecc_import_subject_public_key_info(const unsigned char *in, unsigned long inlen, ecc_key *key);

#ifdef LTC_SSH
int ecc_ssh_ecdsa_encode_name(char *buffer, unsigned long *buflen, const ecc_key *key);
#endif

/* low level functions */
ecc_point *ltc_ecc_new_point(void);
void       ltc_ecc_del_point(ecc_point *p);
int        ltc_ecc_set_point_xyz(ltc_mp_digit x, ltc_mp_digit y, ltc_mp_digit z, ecc_point *p);
int        ltc_ecc_copy_point(const ecc_point *src, ecc_point *dst);
int        ltc_ecc_is_point(const ltc_ecc_dp *dp, void *x, void *y);
int        ltc_ecc_is_point_at_infinity(const ecc_point *P, void *modulus, int *retval);
int        ltc_ecc_import_point(const unsigned char *in, unsigned long inlen, void *prime, void *a, void *b, void *x, void *y);
int        ltc_ecc_export_point(unsigned char *out, unsigned long *outlen, void *x, void *y, unsigned long size, int compressed);
int        ltc_ecc_verify_key(const ecc_key *key);

/* point ops (mp == montgomery digit) */
#if !defined(LTC_MECC_ACCEL) || defined(LTM_DESC) || defined(GMP_DESC)
/* R = 2P */
int ltc_ecc_projective_dbl_point(const ecc_point *P, ecc_point *R, void *ma, void *modulus, void *mp);

/* R = P + Q */
int ltc_ecc_projective_add_point(const ecc_point *P, const ecc_point *Q, ecc_point *R, void *ma, void *modulus, void *mp);
#endif

#if defined(LTC_MECC_FP)
/* optimized point multiplication using fixed point cache (HAC algorithm 14.117) */
int ltc_ecc_fp_mulmod(void *k, ecc_point *G, ecc_point *R, void *a, void *modulus, int map);

/* functions for saving/loading/freeing/adding to fixed point cache */
int ltc_ecc_fp_save_state(unsigned char **out, unsigned long *outlen);
int ltc_ecc_fp_restore_state(unsigned char *in, unsigned long inlen);
void ltc_ecc_fp_free(void);
int ltc_ecc_fp_add_point(ecc_point *g, void *modulus, int lock);

/* lock/unlock all points currently in fixed point cache */
void ltc_ecc_fp_tablelock(int lock);
#endif

/* R = kG */
int ltc_ecc_mulmod(void *k, const ecc_point *G, ecc_point *R, void *a, void *modulus, int map);

#ifdef LTC_ECC_SHAMIR
/* kA*A + kB*B = C */
int ltc_ecc_mul2add(const ecc_point *A, void *kA,
                    const ecc_point *B, void *kB,
                          ecc_point *C,
                               void *ma,
                               void *modulus);

#ifdef LTC_MECC_FP
/* Shamir's trick with optimized point multiplication using fixed point cache */
int ltc_ecc_fp_mul2add(const ecc_point *A, void *kA,
                       const ecc_point *B, void *kB,
                             ecc_point *C,
                                  void *ma,
                                  void *modulus);
#endif

#endif


/* map P to affine from projective */
int ltc_ecc_map(ecc_point *P, void *modulus, void *mp);
#endif /* LTC_MECC */

#ifdef LTC_MDSA
int dsa_int_validate_xy(const dsa_key *key, int *stat);
int dsa_int_validate_pqg(const dsa_key *key, int *stat);
int dsa_int_validate_primes(const dsa_key *key, int *stat);
#endif /* LTC_MDSA */


#ifdef LTC_CURVE25519

int tweetnacl_crypto_sign(
  unsigned char *sm,unsigned long long *smlen,
  const unsigned char *m,unsigned long long mlen,
  const unsigned char *sk, const unsigned char *pk);
int tweetnacl_crypto_sign_open(
  int *stat,
  unsigned char *m,unsigned long long *mlen,
  const unsigned char *sm,unsigned long long smlen,
  const unsigned char *pk);
int tweetnacl_crypto_sign_keypair(prng_state *prng, int wprng, unsigned char *pk,unsigned char *sk);
int tweetnacl_crypto_sk_to_pk(unsigned char *pk, const unsigned char *sk);
int tweetnacl_crypto_scalarmult(unsigned char *q, const unsigned char *n, const unsigned char *p);
int tweetnacl_crypto_scalarmult_base(unsigned char *q,const unsigned char *n);

typedef int (*sk_to_pk)(unsigned char *pk ,const unsigned char *sk);
int ec25519_import_pkcs8(const unsigned char *in, unsigned long inlen,
                       const void *pwd, unsigned long pwdlen,
                       enum ltc_oid_id id, sk_to_pk fp,
                       curve25519_key *key);
int ec25519_export(       unsigned char *out, unsigned long *outlen,
                                    int  which,
                   const curve25519_key *key);
#endif /* LTC_CURVE25519 */

#ifdef LTC_DER

#define LTC_ASN1_IS_TYPE(e, t) (((e) != NULL) && ((e)->type == (t)))

/* DER handling */
int der_decode_custom_type_ex(const unsigned char *in, unsigned long  inlen,
                           ltc_asn1_list *root,
                           ltc_asn1_list *list,     unsigned long  outlen, unsigned int flags);

int der_encode_asn1_identifier(const ltc_asn1_list *id, unsigned char *out, unsigned long *outlen);
int der_decode_asn1_identifier(const unsigned char *in, unsigned long *inlen, ltc_asn1_list *id);
int der_length_asn1_identifier(const ltc_asn1_list *id, unsigned long *idlen);

int der_encode_asn1_length(unsigned long len, unsigned char* out, unsigned long* outlen);
int der_decode_asn1_length(const unsigned char *in, unsigned long *inlen, unsigned long *outlen);
int der_length_asn1_length(unsigned long len, unsigned long *outlen);

int der_length_sequence_ex(const ltc_asn1_list *list, unsigned long inlen,
                           unsigned long *outlen, unsigned long *payloadlen);

extern const ltc_asn1_type  der_asn1_tag_to_type_map[];
extern const unsigned long  der_asn1_tag_to_type_map_sz;

extern const int der_asn1_type_to_identifier_map[];
extern const unsigned long der_asn1_type_to_identifier_map_sz;

int der_decode_sequence_multi_ex(const unsigned char *in, unsigned long inlen, unsigned int flags, ...);

int der_teletex_char_encode(int c);
int der_teletex_value_decode(int v);

int der_utf8_valid_char(const wchar_t c);

typedef int (*public_key_decode_cb)(const unsigned char *in, unsigned long inlen, void *ctx);

int x509_decode_public_key_from_certificate(const unsigned char *in, unsigned long inlen,
                                            enum ltc_oid_id algorithm, ltc_asn1_type param_type,
                                            ltc_asn1_list* parameters, unsigned long *parameters_len,
                                            public_key_decode_cb callback, void *ctx);

/* SUBJECT PUBLIC KEY INFO */
int x509_encode_subject_public_key_info(unsigned char *out, unsigned long *outlen,
        unsigned int algorithm, const void* public_key, unsigned long public_key_len,
        ltc_asn1_type parameters_type, ltc_asn1_list* parameters, unsigned long parameters_len);

int x509_decode_subject_public_key_info(const unsigned char *in, unsigned long inlen,
        unsigned int algorithm, void* public_key, unsigned long* public_key_len,
        ltc_asn1_type parameters_type, ltc_asn1_list* parameters, unsigned long *parameters_len);

int pk_oid_cmp_with_ulong(const char *o1, const unsigned long *o2, unsigned long o2size);
int pk_oid_cmp_with_asn1(const char *o1, const ltc_asn1_list *o2);

#endif /* LTC_DER */

/* tomcrypt_pkcs.h */

#ifdef LTC_PKCS_8

int pkcs8_decode_flexi(const unsigned char  *in,  unsigned long inlen,
                                    const void  *pwd, unsigned long pwdlen,
                                 ltc_asn1_list **decoded_list);

#endif  /* LTC_PKCS_8 */


#ifdef LTC_PKCS_12

int pkcs12_utf8_to_utf16(const unsigned char *in,  unsigned long  inlen,
                               unsigned char *out, unsigned long *outlen);

int pkcs12_kdf(               int   hash_id,
               const unsigned char *pw,         unsigned long pwlen,
               const unsigned char *salt,       unsigned long saltlen,
                     unsigned int   iterations, unsigned char purpose,
                     unsigned char *out,        unsigned long outlen);

#endif  /* LTC_PKCS_12 */

/* tomcrypt_prng.h */

#define LTC_PRNG_EXPORT(which) \
int which ## _export(unsigned char *out, unsigned long *outlen, prng_state *prng)      \
{                                                                                      \
   unsigned long len = which ## _desc.export_size;                                     \
                                                                                       \
   LTC_ARGCHK(prng   != NULL);                                                         \
   LTC_ARGCHK(out    != NULL);                                                         \
   LTC_ARGCHK(outlen != NULL);                                                         \
                                                                                       \
   if (*outlen < len) {                                                                \
      *outlen = len;                                                                   \
      return CRYPT_BUFFER_OVERFLOW;                                                    \
   }                                                                                   \
                                                                                       \
   if (which ## _read(out, len, prng) != len) {                                        \
      return CRYPT_ERROR_READPRNG;                                                     \
   }                                                                                   \
                                                                                       \
   *outlen = len;                                                                      \
   return CRYPT_OK;                                                                    \
}

/* extract a byte portably */
#ifdef _MSC_VER
   #define LTC_BYTE(x, n) ((unsigned char)((x) >> (8 * (n))))
#else
   #define LTC_BYTE(x, n) (((x) >> (8 * (n))) & 255)
#endif
