/* LibTomCrypt, modular cryptographic library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* ---- NUMBER THEORY ---- */

enum public_key_type {
   /* Refers to the public key */
   PK_PUBLIC      = 0x0000,
   /* Refers to the private key */
   PK_PRIVATE     = 0x0001,

   /* Indicates standard output formats that can be read e.g. by OpenSSL or GnuTLS */
   PK_STD         = 0x1000,
   /* Indicates compressed public ECC key */
   PK_COMPRESSED  = 0x2000,
   /* Indicates ECC key with the curve specified by OID */
   PK_CURVEOID    = 0x4000
};

int rand_prime(void *N, long len, prng_state *prng, int wprng);

/* ---- RSA ---- */
#ifdef LTC_MRSA

/** RSA PKCS style key */
typedef struct Rsa_key {
    /** Type of key, PK_PRIVATE or PK_PUBLIC */
    int type;
    /** The public exponent */
    void *e;
    /** The private exponent */
    void *d;
    /** The modulus */
    void *N;
    /** The p factor of N */
    void *p;
    /** The q factor of N */
    void *q;
    /** The 1/q mod p CRT param */
    void *qP;
    /** The d mod (p - 1) CRT param */
    void *dP;
    /** The d mod (q - 1) CRT param */
    void *dQ;
} rsa_key;

int rsa_make_key(prng_state *prng, int wprng, int size, long e, rsa_key *key);

int rsa_get_size(const rsa_key *key);

int rsa_exptmod(const unsigned char *in,   unsigned long inlen,
                      unsigned char *out,  unsigned long *outlen, int which,
                const rsa_key *key);

void rsa_free(rsa_key *key);

/* These use PKCS #1 v2.0 padding */
#define rsa_encrypt_key(in, inlen, out, outlen, lparam, lparamlen, prng, prng_idx, hash_idx, key) \
  rsa_encrypt_key_ex(in, inlen, out, outlen, lparam, lparamlen, prng, prng_idx, hash_idx, LTC_PKCS_1_OAEP, key)

#define rsa_decrypt_key(in, inlen, out, outlen, lparam, lparamlen, hash_idx, stat, key) \
  rsa_decrypt_key_ex(in, inlen, out, outlen, lparam, lparamlen, hash_idx, LTC_PKCS_1_OAEP, stat, key)

#define rsa_sign_hash(in, inlen, out, outlen, prng, prng_idx, hash_idx, saltlen, key) \
  rsa_sign_hash_ex(in, inlen, out, outlen, LTC_PKCS_1_PSS, prng, prng_idx, hash_idx, saltlen, key)

#define rsa_verify_hash(sig, siglen, hash, hashlen, hash_idx, saltlen, stat, key) \
  rsa_verify_hash_ex(sig, siglen, hash, hashlen, LTC_PKCS_1_PSS, hash_idx, saltlen, stat, key)

#define rsa_sign_saltlen_get_max(hash_idx, key) \
  rsa_sign_saltlen_get_max_ex(LTC_PKCS_1_PSS, hash_idx, key)

/* These can be switched between PKCS #1 v2.x and PKCS #1 v1.5 paddings */
int rsa_encrypt_key_ex(const unsigned char *in,       unsigned long  inlen,
                             unsigned char *out,      unsigned long *outlen,
                       const unsigned char *lparam,   unsigned long  lparamlen,
                             prng_state    *prng,     int            prng_idx,
                             int            hash_idx, int            padding,
                       const rsa_key       *key);

int rsa_decrypt_key_ex(const unsigned char *in,             unsigned long  inlen,
                             unsigned char *out,            unsigned long *outlen,
                       const unsigned char *lparam,         unsigned long  lparamlen,
                             int            hash_idx,       int            padding,
                             int           *stat,     const rsa_key       *key);

int rsa_sign_hash_ex(const unsigned char *in,       unsigned long  inlen,
                           unsigned char *out,      unsigned long *outlen,
                           int            padding,
                           prng_state    *prng,     int            prng_idx,
                           int            hash_idx, unsigned long  saltlen,
                     const rsa_key       *key);

int rsa_verify_hash_ex(const unsigned char *sig,            unsigned long  siglen,
                       const unsigned char *hash,           unsigned long  hashlen,
                             int            padding,
                             int            hash_idx,       unsigned long  saltlen,
                             int           *stat,     const rsa_key       *key);

int rsa_sign_saltlen_get_max_ex(int padding, int hash_idx, const rsa_key *key);

/* PKCS #1 import/export */
int rsa_export(unsigned char *out, unsigned long *outlen, int type, const rsa_key *key);
int rsa_import(const unsigned char *in, unsigned long inlen, rsa_key *key);

int rsa_import_x509(const unsigned char *in, unsigned long inlen, rsa_key *key);
int rsa_import_pkcs8(const unsigned char *in, unsigned long inlen,
                     const void *passwd, unsigned long passwdlen, rsa_key *key);

int rsa_set_key(const unsigned char *N,  unsigned long Nlen,
                const unsigned char *e,  unsigned long elen,
                const unsigned char *d,  unsigned long dlen,
                rsa_key *key);
int rsa_set_factors(const unsigned char *p,  unsigned long plen,
                    const unsigned char *q,  unsigned long qlen,
                    rsa_key *key);
int rsa_set_crt_params(const unsigned char *dP, unsigned long dPlen,
                       const unsigned char *dQ, unsigned long dQlen,
                       const unsigned char *qP, unsigned long qPlen,
                       rsa_key *key);
#endif

/* ---- DH Routines ---- */
#ifdef LTC_MDH

typedef struct {
    int type;
    void *x;
    void *y;
    void *base;
    void *prime;
} dh_key;

int dh_get_groupsize(const dh_key *key);

int dh_export(unsigned char *out, unsigned long *outlen, int type, const dh_key *key);
int dh_import(const unsigned char *in, unsigned long inlen, dh_key *key);

int dh_set_pg(const unsigned char *p, unsigned long plen,
              const unsigned char *g, unsigned long glen,
              dh_key *key);
int dh_set_pg_dhparam(const unsigned char *dhparam, unsigned long dhparamlen, dh_key *key);
int dh_set_pg_groupsize(int groupsize, dh_key *key);

int dh_set_key(const unsigned char *in, unsigned long inlen, int type, dh_key *key);
int dh_generate_key(prng_state *prng, int wprng, dh_key *key);

int dh_shared_secret(const dh_key  *private_key, const dh_key  *public_key,
                     unsigned char *out,         unsigned long *outlen);

void dh_free(dh_key *key);

int dh_export_key(void *out, unsigned long *outlen, int type, const dh_key *key);
#endif /* LTC_MDH */


/* ---- ECC Routines ---- */
#ifdef LTC_MECC

/* size of our temp buffers for exported keys */
#define ECC_BUF_SIZE 256

/* max private key size */
#define ECC_MAXSIZE  66

/** Structure defines a GF(p) curve */
typedef struct {
   /** The prime that defines the field the curve is in (encoded in hex) */
   const char *prime;

   /** The fields A param (hex) */
   const char *A;

   /** The fields B param (hex) */
   const char *B;

   /** The order of the curve (hex) */
   const char *order;

   /** The x co-ordinate of the base point on the curve (hex) */
   const char *Gx;

   /** The y co-ordinate of the base point on the curve (hex) */
   const char *Gy;

   /** The co-factor */
   unsigned long cofactor;

   /** The OID */
   const char *OID;
} ltc_ecc_curve;

/** A point on a ECC curve, stored in Jacbobian format such that (x,y,z) => (x/z^2, y/z^3, 1) when interpretted as affine */
typedef struct {
    /** The x co-ordinate */
    void *x;

    /** The y co-ordinate */
    void *y;

    /** The z co-ordinate */
    void *z;
} ecc_point;

/** ECC key's domain parameters */
typedef struct {
   /** The size of the curve in octets */
   int size;
   /** The prime that defines the field the curve is in */
   void *prime;
   /** The fields A param */
   void *A;
   /** The fields B param */
   void *B;
   /** The order of the curve */
   void *order;
   /** The base point G on the curve */
   ecc_point base;
   /** The co-factor */
   unsigned long cofactor;
   /** The OID */
   unsigned long oid[16];
   unsigned long oidlen;
} ltc_ecc_dp;

/** An ECC key */
typedef struct {
    /** Type of key, PK_PRIVATE or PK_PUBLIC */
    int type;

    /** Structure with domain parameters */
    ltc_ecc_dp dp;

    /** Structure with the public key */
    ecc_point pubkey;

    /** The private key */
    void *k;
} ecc_key;

/** Formats of ECC signatures */
typedef enum ecc_signature_type_ {
   /* ASN.1 encoded, ANSI X9.62 */
   LTC_ECCSIG_ANSIX962   = 0x0,
   /* raw R, S values */
   LTC_ECCSIG_RFC7518    = 0x1,
   /* raw R, S, V (+27) values */
   LTC_ECCSIG_ETH27      = 0x2,
   /* SSH + ECDSA signature format defined by RFC5656 */
   LTC_ECCSIG_RFC5656    = 0x3,
} ecc_signature_type;

/** the ECC params provided */
extern const ltc_ecc_curve ltc_ecc_curves[];

void ecc_sizes(int *low, int *high);
int  ecc_get_size(const ecc_key *key);

int  ecc_find_curve(const char* name_or_oid, const ltc_ecc_curve** cu);
int  ecc_set_curve(const ltc_ecc_curve *cu, ecc_key *key);
int  ecc_generate_key(prng_state *prng, int wprng, ecc_key *key);
int  ecc_set_key(const unsigned char *in, unsigned long inlen, int type, ecc_key *key);
int  ecc_get_key(unsigned char *out, unsigned long *outlen, int type, const ecc_key *key);
int  ecc_get_oid_str(char *out, unsigned long *outlen, const ecc_key *key);

int  ecc_make_key(prng_state *prng, int wprng, int keysize, ecc_key *key);
int  ecc_make_key_ex(prng_state *prng, int wprng, ecc_key *key, const ltc_ecc_curve *cu);
void ecc_free(ecc_key *key);

int  ecc_export(unsigned char *out, unsigned long *outlen, int type, const ecc_key *key);
int  ecc_import(const unsigned char *in, unsigned long inlen, ecc_key *key);
int  ecc_import_ex(const unsigned char *in, unsigned long inlen, ecc_key *key, const ltc_ecc_curve *cu);

int ecc_ansi_x963_export(const ecc_key *key, unsigned char *out, unsigned long *outlen);
int ecc_ansi_x963_import(const unsigned char *in, unsigned long inlen, ecc_key *key);
int ecc_ansi_x963_import_ex(const unsigned char *in, unsigned long inlen, ecc_key *key, const ltc_ecc_curve *cu);

int ecc_export_openssl(unsigned char *out, unsigned long *outlen, int type, const ecc_key *key);
int ecc_import_openssl(const unsigned char *in, unsigned long inlen, ecc_key *key);
int ecc_import_pkcs8(const unsigned char *in, unsigned long inlen, const void *pwd, unsigned long pwdlen, ecc_key *key);
int ecc_import_x509(const unsigned char *in, unsigned long inlen, ecc_key *key);

int  ecc_shared_secret(const ecc_key *private_key, const ecc_key *public_key,
                       unsigned char *out, unsigned long *outlen);

int  ecc_encrypt_key(const unsigned char *in,   unsigned long inlen,
                           unsigned char *out,  unsigned long *outlen,
                           prng_state *prng, int wprng, int hash,
                           const ecc_key *key);

int  ecc_decrypt_key(const unsigned char *in,  unsigned long  inlen,
                           unsigned char *out, unsigned long *outlen,
                           const ecc_key *key);

#define ecc_sign_hash_rfc7518(in_, inlen_, out_, outlen_, prng_, wprng_, key_) \
   ecc_sign_hash_ex(in_, inlen_, out_, outlen_, prng_, wprng_, LTC_ECCSIG_RFC7518, NULL, key_)

#define ecc_sign_hash(in_, inlen_, out_, outlen_, prng_, wprng_, key_) \
   ecc_sign_hash_ex(in_, inlen_, out_, outlen_, prng_, wprng_, LTC_ECCSIG_ANSIX962, NULL, key_)

#define ecc_verify_hash_rfc7518(sig_, siglen_, hash_, hashlen_, stat_, key_) \
   ecc_verify_hash_ex(sig_, siglen_, hash_, hashlen_, LTC_ECCSIG_RFC7518, stat_, key_)

#define ecc_verify_hash(sig_, siglen_, hash_, hashlen_, stat_, key_) \
   ecc_verify_hash_ex(sig_, siglen_, hash_, hashlen_, LTC_ECCSIG_ANSIX962, stat_, key_)

int  ecc_sign_hash_ex(const unsigned char *in,  unsigned long inlen,
                            unsigned char *out, unsigned long *outlen,
                            prng_state *prng, int wprng, ecc_signature_type sigformat,
                            int *recid, const ecc_key *key);

int  ecc_verify_hash_ex(const unsigned char *sig,  unsigned long siglen,
                        const unsigned char *hash, unsigned long hashlen,
                        ecc_signature_type sigformat, int *stat, const ecc_key *key);

int  ecc_recover_key(const unsigned char *sig,  unsigned long siglen,
                     const unsigned char *hash, unsigned long hashlen,
                     int recid, ecc_signature_type sigformat, ecc_key *key);

#endif

#ifdef LTC_CURVE25519

typedef struct {
   /** The key type, PK_PRIVATE or PK_PUBLIC */
   enum public_key_type type;

   /** The PK-algorithm, PKA_ED25519 or PKA_X25519 */
   /** This was supposed to be:
    * enum public_key_algorithms algo;
    * but that enum is now in tomcrypt_private.h
    */
   int algo;

   /** The private key */
   unsigned char priv[32];

   /** The public key */
   unsigned char pub[32];
} curve25519_key;


/** Ed25519 Signature API */
int ed25519_make_key(prng_state *prng, int wprng, curve25519_key *key);

int ed25519_export(       unsigned char *out, unsigned long *outlen,
                                    int  which,
                   const curve25519_key *key);

int ed25519_import(const unsigned char *in, unsigned long inlen, curve25519_key *key);
int ed25519_import_raw(const unsigned char *in, unsigned long inlen, int which, curve25519_key *key);
int ed25519_import_x509(const unsigned char *in, unsigned long inlen, curve25519_key *key);
int ed25519_import_pkcs8(const unsigned char *in, unsigned long inlen,
                                  const void *pwd, unsigned long pwdlen,
                              curve25519_key *key);

int ed25519_sign(const unsigned char  *msg, unsigned long msglen,
                       unsigned char  *sig, unsigned long *siglen,
                 const curve25519_key *private_key);

int ed25519_verify(const  unsigned char *msg, unsigned long msglen,
                   const  unsigned char *sig, unsigned long siglen,
                   int *stat, const curve25519_key *public_key);

/** X25519 Key-Exchange API */
int x25519_make_key(prng_state *prng, int wprng, curve25519_key *key);

int x25519_export(       unsigned char *out, unsigned long *outlen,
                                   int  which,
                  const curve25519_key *key);

int x25519_import(const unsigned char *in, unsigned long inlen, curve25519_key *key);
int x25519_import_raw(const unsigned char *in, unsigned long inlen, int which, curve25519_key *key);
int x25519_import_x509(const unsigned char *in, unsigned long inlen, curve25519_key *key);
int x25519_import_pkcs8(const unsigned char *in, unsigned long inlen,
                                 const void *pwd, unsigned long pwdlen,
                             curve25519_key *key);

int x25519_shared_secret(const curve25519_key *private_key,
                         const curve25519_key *public_key,
                                unsigned char *out, unsigned long *outlen);

#endif /* LTC_CURVE25519 */

#ifdef LTC_MDSA

/* Max diff between group and modulus size in bytes (max case: L=8192bits, N=256bits) */
#define LTC_MDSA_DELTA 992

/* Max DSA group size in bytes */
#define LTC_MDSA_MAX_GROUP 64

/* Max DSA modulus size in bytes (the actual DSA size, max 8192 bits) */
#define LTC_MDSA_MAX_MODULUS 1024

/** DSA key structure */
typedef struct {
   /** The key type, PK_PRIVATE or PK_PUBLIC */
   int type;

   /** The order of the sub-group used in octets */
   int qord;

   /** The generator  */
   void *g;

   /** The prime used to generate the sub-group */
   void *q;

   /** The large prime that generats the field the contains the sub-group */
   void *p;

   /** The private key */
   void *x;

   /** The public key */
   void *y;
} dsa_key;

int dsa_make_key(prng_state *prng, int wprng, int group_size, int modulus_size, dsa_key *key);

int dsa_set_pqg(const unsigned char *p,  unsigned long plen,
                const unsigned char *q,  unsigned long qlen,
                const unsigned char *g,  unsigned long glen,
                dsa_key *key);
int dsa_set_pqg_dsaparam(const unsigned char *dsaparam, unsigned long dsaparamlen, dsa_key *key);
int dsa_generate_pqg(prng_state *prng, int wprng, int group_size, int modulus_size, dsa_key *key);

int dsa_set_key(const unsigned char *in, unsigned long inlen, int type, dsa_key *key);
int dsa_generate_key(prng_state *prng, int wprng, dsa_key *key);

void dsa_free(dsa_key *key);

int dsa_sign_hash_raw(const unsigned char *in,  unsigned long inlen,
                                   void *r,   void *s,
                               prng_state *prng, int wprng, const dsa_key *key);

int dsa_sign_hash(const unsigned char *in,  unsigned long inlen,
                        unsigned char *out, unsigned long *outlen,
                        prng_state *prng, int wprng, const dsa_key *key);

int dsa_verify_hash_raw(         void *r,          void *s,
                    const unsigned char *hash, unsigned long hashlen,
                                    int *stat, const dsa_key *key);

int dsa_verify_hash(const unsigned char *sig,        unsigned long  siglen,
                    const unsigned char *hash,       unsigned long  hashlen,
                          int           *stat, const dsa_key       *key);

int dsa_encrypt_key(const unsigned char *in,   unsigned long inlen,
                          unsigned char *out,  unsigned long *outlen,
                          prng_state    *prng, int wprng, int hash,
                    const dsa_key       *key);

int dsa_decrypt_key(const unsigned char *in,  unsigned long  inlen,
                          unsigned char *out, unsigned long *outlen,
                    const dsa_key       *key);

int dsa_import(const unsigned char *in, unsigned long inlen, dsa_key *key);
int dsa_export(unsigned char *out, unsigned long *outlen, int type, const dsa_key *key);
int dsa_verify_key(const dsa_key *key, int *stat);
int dsa_shared_secret(void          *private_key, void *base,
                      const dsa_key *public_key,
                      unsigned char *out,         unsigned long *outlen);
#endif /* LTC_MDSA */

#ifdef LTC_DER
/* DER handling */

typedef enum ltc_asn1_type_ {
 /*  0 */
 LTC_ASN1_EOL,
 LTC_ASN1_BOOLEAN,
 LTC_ASN1_INTEGER,
 LTC_ASN1_SHORT_INTEGER,
 LTC_ASN1_BIT_STRING,
 /*  5 */
 LTC_ASN1_OCTET_STRING,
 LTC_ASN1_NULL,
 LTC_ASN1_OBJECT_IDENTIFIER,
 LTC_ASN1_IA5_STRING,
 LTC_ASN1_PRINTABLE_STRING,
 /* 10 */
 LTC_ASN1_UTF8_STRING,
 LTC_ASN1_UTCTIME,
 LTC_ASN1_CHOICE,
 LTC_ASN1_SEQUENCE,
 LTC_ASN1_SET,
 /* 15 */
 LTC_ASN1_SETOF,
 LTC_ASN1_RAW_BIT_STRING,
 LTC_ASN1_TELETEX_STRING,
 LTC_ASN1_GENERALIZEDTIME,
 LTC_ASN1_CUSTOM_TYPE,
} ltc_asn1_type;

typedef enum {
   LTC_ASN1_CL_UNIVERSAL = 0x0,
   LTC_ASN1_CL_APPLICATION = 0x1,
   LTC_ASN1_CL_CONTEXT_SPECIFIC = 0x2,
   LTC_ASN1_CL_PRIVATE = 0x3,
} ltc_asn1_class;

typedef enum {
   LTC_ASN1_PC_PRIMITIVE = 0x0,
   LTC_ASN1_PC_CONSTRUCTED = 0x1,
} ltc_asn1_pc;

/** A LTC ASN.1 list type */
typedef struct ltc_asn1_list_ {
   /** The LTC ASN.1 enumerated type identifier */
   ltc_asn1_type type;
   /** The data to encode or place for decoding */
   void         *data;
   /** The size of the input or resulting output */
   unsigned long size;
   /** The used flag
    * 1. This is used by the CHOICE ASN.1 type to indicate which choice was made
    * 2. This is used by the ASN.1 decoder to indicate if an element is used
    * 3. This is used by the flexi-decoder to indicate the first byte of the identifier */
   int           used;
   /** Flag used to indicate optional items in ASN.1 sequences */
   int           optional;
   /** ASN.1 identifier */
   ltc_asn1_class klass;
   ltc_asn1_pc    pc;
   ulong64        tag;
   /** prev/next entry in the list */
   struct ltc_asn1_list_ *prev, *next, *child, *parent;
} ltc_asn1_list;

#define LTC_SET_ASN1(list, index, Type, Data, Size)  \
   do {                                              \
      int LTC_MACRO_temp            = (index);       \
      ltc_asn1_list *LTC_MACRO_list = (list);        \
      LTC_MACRO_list[LTC_MACRO_temp].type = (Type);  \
      LTC_MACRO_list[LTC_MACRO_temp].data = (void*)(Data);  \
      LTC_MACRO_list[LTC_MACRO_temp].size = (Size);  \
      LTC_MACRO_list[LTC_MACRO_temp].used = 0;       \
      LTC_MACRO_list[LTC_MACRO_temp].optional = 0;   \
      LTC_MACRO_list[LTC_MACRO_temp].klass = 0;      \
      LTC_MACRO_list[LTC_MACRO_temp].pc = 0;         \
      LTC_MACRO_list[LTC_MACRO_temp].tag = 0;        \
   } while (0)

#define LTC_SET_ASN1_IDENTIFIER(list, index, Class, Pc, Tag)      \
   do {                                                           \
      int LTC_MACRO_temp            = (index);                    \
      ltc_asn1_list *LTC_MACRO_list = (list);                     \
      LTC_MACRO_list[LTC_MACRO_temp].type = LTC_ASN1_CUSTOM_TYPE; \
      LTC_MACRO_list[LTC_MACRO_temp].klass = (Class);             \
      LTC_MACRO_list[LTC_MACRO_temp].pc = (Pc);                   \
      LTC_MACRO_list[LTC_MACRO_temp].tag = (Tag);                 \
   } while (0)

#define LTC_SET_ASN1_CUSTOM_CONSTRUCTED(list, index, Class, Tag, Data)    \
   do {                                                           \
      int LTC_MACRO_temp##__LINE__ = (index);                     \
      LTC_SET_ASN1(list, LTC_MACRO_temp##__LINE__, LTC_ASN1_CUSTOM_TYPE, Data, 1);   \
      LTC_SET_ASN1_IDENTIFIER(list, LTC_MACRO_temp##__LINE__, Class, LTC_ASN1_PC_CONSTRUCTED, Tag);       \
   } while (0)

#define LTC_SET_ASN1_CUSTOM_PRIMITIVE(list, index, Class, Tag, Type, Data, Size)    \
   do {                                                           \
      int LTC_MACRO_temp##__LINE__ = (index);                     \
      LTC_SET_ASN1(list, LTC_MACRO_temp##__LINE__, LTC_ASN1_CUSTOM_TYPE, Data, Size);   \
      LTC_SET_ASN1_IDENTIFIER(list, LTC_MACRO_temp##__LINE__, Class, LTC_ASN1_PC_PRIMITIVE, Tag);       \
      list[LTC_MACRO_temp##__LINE__].used = (int)(Type);       \
   } while (0)

extern const char*          der_asn1_class_to_string_map[];
extern const unsigned long  der_asn1_class_to_string_map_sz;

extern const char*          der_asn1_pc_to_string_map[];
extern const unsigned long  der_asn1_pc_to_string_map_sz;

extern const char*          der_asn1_tag_to_string_map[];
extern const unsigned long  der_asn1_tag_to_string_map_sz;

/* SEQUENCE */
int der_encode_sequence_ex(const ltc_asn1_list *list, unsigned long inlen,
                           unsigned char *out,        unsigned long *outlen, int type_of);

#define der_encode_sequence(list, inlen, out, outlen) der_encode_sequence_ex(list, inlen, out, outlen, LTC_ASN1_SEQUENCE)

/** The supported bitmap for all the
 * decoders with a `flags` argument.
 */
enum ltc_der_seq {
   LTC_DER_SEQ_ZERO = 0x0u,

   /** Bit0  - [0]=Unordered (SET or SETOF)
    *          [1]=Ordered (SEQUENCE) */
   LTC_DER_SEQ_UNORDERED = LTC_DER_SEQ_ZERO,
   LTC_DER_SEQ_ORDERED = 0x1u,

   /** Bit1  - [0]=Relaxed
    *          [1]=Strict */
   LTC_DER_SEQ_RELAXED = LTC_DER_SEQ_ZERO,
   LTC_DER_SEQ_STRICT = 0x2u,

   /** Alternative naming */
   LTC_DER_SEQ_SET = LTC_DER_SEQ_UNORDERED,
   LTC_DER_SEQ_SEQUENCE = LTC_DER_SEQ_ORDERED,
};

int der_decode_sequence_ex(const unsigned char *in, unsigned long  inlen,
                           ltc_asn1_list *list,     unsigned long  outlen, unsigned int flags);

#define der_decode_sequence(in, inlen, list, outlen) der_decode_sequence_ex(in, inlen, list, outlen, LTC_DER_SEQ_SEQUENCE | LTC_DER_SEQ_RELAXED)
#define der_decode_sequence_strict(in, inlen, list, outlen) der_decode_sequence_ex(in, inlen, list, outlen, LTC_DER_SEQ_SEQUENCE | LTC_DER_SEQ_STRICT)

int der_length_sequence(const ltc_asn1_list *list, unsigned long inlen,
                        unsigned long *outlen);


/* Custom-types */
int der_encode_custom_type(const ltc_asn1_list *root,
                                 unsigned char *out, unsigned long *outlen);

int der_decode_custom_type(const unsigned char *in, unsigned long inlen,
                                 ltc_asn1_list *root);

int der_length_custom_type(const ltc_asn1_list *root,
                                 unsigned long *outlen,
                                 unsigned long *payloadlen);

/* SET */
#define der_decode_set(in, inlen, list, outlen) der_decode_sequence_ex(in, inlen, list, outlen, LTC_DER_SEQ_SET)
#define der_length_set der_length_sequence
int der_encode_set(const ltc_asn1_list *list, unsigned long inlen,
                   unsigned char *out,        unsigned long *outlen);

int der_encode_setof(const ltc_asn1_list *list, unsigned long inlen,
                     unsigned char *out,        unsigned long *outlen);

/* VA list handy helpers with triplets of <type, size, data> */
int der_encode_sequence_multi(unsigned char *out, unsigned long *outlen, ...);
int der_decode_sequence_multi(const unsigned char *in, unsigned long inlen, ...);

/* FLEXI DECODER handle unknown list decoder */
int  der_decode_sequence_flexi(const unsigned char *in, unsigned long *inlen, ltc_asn1_list **out);
#define der_free_sequence_flexi         der_sequence_free
void der_sequence_free(ltc_asn1_list *in);
void der_sequence_shrink(ltc_asn1_list *in);

/* BOOLEAN */
int der_length_boolean(unsigned long *outlen);
int der_encode_boolean(int in,
                       unsigned char *out, unsigned long *outlen);
int der_decode_boolean(const unsigned char *in, unsigned long inlen,
                                       int *out);
/* INTEGER */
int der_encode_integer(void *num, unsigned char *out, unsigned long *outlen);
int der_decode_integer(const unsigned char *in, unsigned long inlen, void *num);
int der_length_integer(void *num, unsigned long *outlen);

/* INTEGER -- handy for 0..2^32-1 values */
int der_decode_short_integer(const unsigned char *in, unsigned long inlen, unsigned long *num);
int der_encode_short_integer(unsigned long num, unsigned char *out, unsigned long *outlen);
int der_length_short_integer(unsigned long num, unsigned long *outlen);

/* BIT STRING */
int der_encode_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_encode_raw_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_raw_bit_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_bit_string(unsigned long nbits, unsigned long *outlen);

/* OCTET STRING */
int der_encode_octet_string(const unsigned char *in, unsigned long inlen,
                                  unsigned char *out, unsigned long *outlen);
int der_decode_octet_string(const unsigned char *in, unsigned long inlen,
                                  unsigned char *out, unsigned long *outlen);
int der_length_octet_string(unsigned long noctets, unsigned long *outlen);

/* OBJECT IDENTIFIER */
int der_encode_object_identifier(const unsigned long *words, unsigned long  nwords,
                                       unsigned char *out,   unsigned long *outlen);
int der_decode_object_identifier(const unsigned char *in,    unsigned long  inlen,
                                       unsigned long *words, unsigned long *outlen);
int der_length_object_identifier(const unsigned long *words, unsigned long nwords, unsigned long *outlen);
unsigned long der_object_identifier_bits(unsigned long x);

/* IA5 STRING */
int der_encode_ia5_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_ia5_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_ia5_string(const unsigned char *octets, unsigned long noctets, unsigned long *outlen);

int der_ia5_char_encode(int c);
int der_ia5_value_decode(int v);

/* TELETEX STRING */
int der_decode_teletex_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_teletex_string(const unsigned char *octets, unsigned long noctets, unsigned long *outlen);

/* PRINTABLE STRING */
int der_encode_printable_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_decode_printable_string(const unsigned char *in, unsigned long inlen,
                                unsigned char *out, unsigned long *outlen);
int der_length_printable_string(const unsigned char *octets, unsigned long noctets, unsigned long *outlen);

int der_printable_char_encode(int c);
int der_printable_value_decode(int v);

/* UTF-8 */
#if (defined(SIZE_MAX) || __STDC_VERSION__ >= 199901L || defined(WCHAR_MAX) || defined(__WCHAR_MAX__) || defined(_WCHAR_T) || defined(_WCHAR_T_DEFINED) || defined (__WCHAR_TYPE__)) && !defined(LTC_NO_WCHAR)
   #if defined(__WCHAR_MAX__)
      #define LTC_WCHAR_MAX __WCHAR_MAX__
   #else
      #include <wchar.h>
      #define LTC_WCHAR_MAX WCHAR_MAX
   #endif
/* please note that it might happen that LTC_WCHAR_MAX is undefined */
#else
   typedef ulong32 wchar_t;
   #define LTC_WCHAR_MAX 0xFFFFFFFF
#endif

int der_encode_utf8_string(const wchar_t *in,  unsigned long inlen,
                           unsigned char *out, unsigned long *outlen);

int der_decode_utf8_string(const unsigned char *in,  unsigned long inlen,
                                       wchar_t *out, unsigned long *outlen);
unsigned long der_utf8_charsize(const wchar_t c);
int der_length_utf8_string(const wchar_t *in, unsigned long noctets, unsigned long *outlen);


/* CHOICE */
int der_decode_choice(const unsigned char *in,   unsigned long *inlen,
                            ltc_asn1_list *list, unsigned long  outlen);

/* UTCTime */
typedef struct {
   unsigned YY, /* year */
            MM, /* month */
            DD, /* day */
            hh, /* hour */
            mm, /* minute */
            ss, /* second */
            off_dir, /* timezone offset direction 0 == +, 1 == - */
            off_hh, /* timezone offset hours */
            off_mm; /* timezone offset minutes */
} ltc_utctime;

int der_encode_utctime(const ltc_utctime   *utctime,
                             unsigned char *out,   unsigned long *outlen);

int der_decode_utctime(const unsigned char *in, unsigned long *inlen,
                             ltc_utctime   *out);

int der_length_utctime(const ltc_utctime *utctime, unsigned long *outlen);

/* GeneralizedTime */
typedef struct {
   unsigned YYYY, /* year */
            MM, /* month */
            DD, /* day */
            hh, /* hour */
            mm, /* minute */
            ss, /* second */
            fs, /* fractional seconds */
            off_dir, /* timezone offset direction 0 == +, 1 == - */
            off_hh, /* timezone offset hours */
            off_mm; /* timezone offset minutes */
} ltc_generalizedtime;

int der_encode_generalizedtime(const ltc_generalizedtime *gtime,
                                     unsigned char       *out, unsigned long *outlen);

int der_decode_generalizedtime(const unsigned char *in, unsigned long *inlen,
                               ltc_generalizedtime *out);

int der_length_generalizedtime(const ltc_generalizedtime *gtime, unsigned long *outlen);

#endif
