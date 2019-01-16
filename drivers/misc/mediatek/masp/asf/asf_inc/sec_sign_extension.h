#ifndef _SEC_SIGN_EXTENSION_H
#define _SEC_SIGN_EXTENSION_H

#define MAX_VERITY_COUNT            32
#define SEC_EXTENSION_MAGIC         (0x7A797A79)
#define SEC_EXTENSION_MAGIC_V4      (0x7B797B79)
#define SEC_EXTENSION_HEADER_MAGIC  (0x45454545)

#define CRYPTO_SIZE_UNKNOWN 0

typedef enum
{
    SEC_EXT_HDR_UNKNOWN = 0,
    SEC_EXT_HDR_CRYPTO = 1,
    SEC_EXT_HDR_FRAG_CFG = 2,
    SEC_EXT_HDR_HASH_ONLY = 3,
    SEC_EXT_HDR_HASH_SIG = 4,
    SEC_EXT_HDR_SPARSE = 5,
    SEC_EXT_HDR_HASH_ONLY_64 = 6,
    
    SEC_EXT_HDR_END_MARK = 0xFFFFFFFF
} SEC_EXT_HEADER_TYPE;

typedef enum
{
    SEC_CRYPTO_HASH_UNKNOWN = 0,
    SEC_CRYPTO_HASH_MD5 = 1,
    SEC_CRYPTO_HASH_SHA1 = 2,
    SEC_CRYPTO_HASH_SHA256 = 3,
    SEC_CRYPTO_HASH_SHA512 = 4,
    
} SEC_CRYPTO_HASH_TYPE;

typedef enum
{
    SEC_CRYPTO_SIG_UNKNOWN = 0,
    SEC_CRYPTO_SIG_RSA512 = 1,
    SEC_CRYPTO_SIG_RSA1024 = 2,
    SEC_CRYPTO_SIG_RSA2048 = 3,
    
} SEC_CRYPTO_SIGNATURE_TYPE;

typedef enum
{
    SEC_CRYPTO_ENC_UNKNOWN = 0,
    SEC_CRYPTO_ENC_RC4 = 1,
    SEC_CRYPTO_ENC_AES128 = 2,
    SEC_CRYPTO_ENC_AES192 = 3,    
    SEC_CRYPTO_ENC_AES256 = 4,

} SEC_CRYPTO_ENCRYPTION_TYPE;

typedef enum
{
    SEC_SIZE_HASH_MD5 = 16,
    SEC_SIZE_HASH_SHA1 = 20,
    SEC_SIZE_HASH_SHA256 = 32,
    SEC_SIZE_HASH_SHA512 = 64,
    
} SEC_CRYPTO_HASH_SIZE_BYTES;

typedef enum
{
    SEC_SIZE_SIG_RSA512 = 64,
    SEC_SIZE_SIG_RSA1024 = 128,
    SEC_SIZE_SIG_RSA2048 = 256,
    
} SEC_CRYPTO_SIGNATURE_SIZE_BYTES;


typedef enum
{
    SEC_CHUNK_SIZE_ZERO = 0,
    SEC_CHUNK_SIZE_UNKNOWN = 0x00100000,
    SEC_CHUNK_SIZE_1M = 0x00100000,
    SEC_CHUNK_SIZE_2M = 0x00200000,
    SEC_CHUNK_SIZE_4M = 0x00400000,
    SEC_CHUNK_SIZE_8M = 0x00800000,
    SEC_CHUNK_SIZE_16M = 0x01000000,
    SEC_CHUNK_SIZE_32M = 0x02000000,
    
} SEC_FRAG_CHUNK_SIZE_BYTES;


typedef struct _SEC_EXTENSTION_CRYPTO
{
    unsigned int magic;
    unsigned int ext_type;
    unsigned char hash_type;
    unsigned char sig_type;
    unsigned char enc_type;
    unsigned char reserved;
} SEC_EXTENSTION_CRYPTO;

typedef struct _SEC_FRAGMENT_CFG
{
    unsigned int magic;
    unsigned int ext_type;
    unsigned int chunk_size;
    unsigned int frag_count;
} SEC_FRAGMENT_CFG;

typedef struct _SEC_EXTENSTION_HASH_ONLY
{
    unsigned int magic;
    unsigned int ext_type;
    unsigned int sub_type;  /* hash type */
    unsigned int hash_offset;
    unsigned int hash_len;
    unsigned char hash_data[];
} SEC_EXTENSTION_HASH_ONLY;


typedef struct _SEC_EXTENSTION_HASH_ONLY_64
{
    unsigned int magic;
    unsigned int ext_type;
    unsigned int sub_type;  /* hash type */
    unsigned int padding;
    unsigned long long hash_offset_64;
    unsigned long long hash_len_64;
    unsigned char hash_data[];
} SEC_EXTENSTION_HASH_ONLY_64;

typedef struct _SEC_EXTENSTION_HASH_SIG
{
    unsigned int magic;
    unsigned int ext_type;
    unsigned int sig_type;  /* sig type */
    unsigned int hash_type; /* hash type */
    unsigned int auth_offset;
    unsigned int auth_len;
    unsigned char auth_data[];  /* sig + hash */
} SEC_EXTENSTION_HASH_SIG;

typedef struct _SEC_EXTENSTION_END_MARK
{
    unsigned int magic;
    unsigned int ext_type;
} SEC_EXTENSTION_END_MARK;

typedef struct _SEC_IMG_EXTENSTION_SET
{
    SEC_EXTENSTION_CRYPTO *crypto;
    SEC_FRAGMENT_CFG *frag;
    SEC_EXTENSTION_END_MARK *end;
    SEC_EXTENSTION_HASH_ONLY **hash_only;
    SEC_EXTENSTION_HASH_ONLY_64 **hash_only_64;
} SEC_IMG_EXTENSTION_SET;

#endif

