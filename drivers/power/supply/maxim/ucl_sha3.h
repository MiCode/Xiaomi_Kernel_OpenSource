/* sha3.h */
#ifndef _UCL_SHA3_H
#define _UCL_SHA3_H

#ifdef __cplusplus
extern "C" {
#endif

#define UCL_SHA3_224_HASHSIZE  28
#define UCL_SHA3_256_HASHSIZE  32
#define UCL_SHA3_384_HASHSIZE  48
#define UCL_SHA3_512_HASHSIZE  64
#define UCL_SHA3_MAX_PERMSIZE 25
#define UCL_SHA3_MAXQRATE_QWORDS 24

#define UCL_SHA3 2
#define UCL_SHA3_224 9
#define UCL_SHA3_256 10
#define UCL_SHA3_384 11
#define UCL_SHA3_512 12
/**
* SHA3 Algorithm context.
*/
/** @file ucl_sha3.h
* @defgroup UCL_SHA3 SHA3
* Secure Hash Algorithm 3, from FIPS 202.
*
* @par Header:
* @link ucl_sha3.h ucl_sha3.h @endlink
*
* SHA-3 is a data-digest algorithm and a component of
* the Standard FIPS 202.
* The algorithm takes as input a data of arbitrary length and
* produces as output 224-bit, 256-bit, 384-bit or 512-bit "fingerprint" or "data digest"
* of the input.@n
* @n
*
* <b>SHA3 Descriptor:</b> @n
* @li Hash length : 224/256/384/512 bits
*
* @ingroup UCL_HASH
*/

/** <b>The SHA3 context</b>.
* This structure is associated to the 'step by step' process.
*
* @ingroup UCL_SHA3
*/

typedef struct sha3_ctx {
	/* 1600 bits algorithm hashing state */
	unsigned long long hash[UCL_SHA3_MAX_PERMSIZE];
	/* 1536-bit buffer for leftovers */
	unsigned long long message[UCL_SHA3_MAXQRATE_QWORDS];
	/* count of bytes in the message[] buffer */
	unsigned int rest;
	/* size of a message block processed at once */
	unsigned int block_size;
} sha3_ctx;

#define SHA3_SPONGE_WORDS (((1600)/8/*bits to byte*/)/sizeof(unsigned long long))

typedef struct sha3_context_ {
	unsigned long long saved;
	union {
		unsigned long long s[SHA3_SPONGE_WORDS];
		unsigned char sb[SHA3_SPONGE_WORDS * 8];
	};
	int byteIndex;         /* 0..7--the next byte after the set one
	 * (starts from 0; 0--none are buffered) */
	int wordIndex;         /* 0..24--the next word to integrate input
	 * (starts from 0) */
	int capacityWords;     /* the double size of the hash output in
	 * words (e.g. 16 for Keccak 512) */
} ucl_sha3_ctx_t;

// methods for calculating the hash function

/*============================================================================*/
/** <b>SHA3-224 Init</b>.
* The initialisation of SHA3-224.
*
* @param[in,out] context Pointer to the context
*
* @return Error code
*
* @retval #UCL_OK            if no error occurred
* @retval #UCL_INVALID_INPUT if @p context is #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_224_init(ucl_sha3_ctx_t *ctx);
/*============================================================================*/
/** <b>SHA3-256 Init</b>.
* The initialisation of SHA3-256.
*
* @param[in,out] context Pointer to the context
*
* @return Error code
*
* @retval #UCL_OK            if no error occurred
* @retval #UCL_INVALID_INPUT if @p context is #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_256_init(ucl_sha3_ctx_t *ctx);
/*============================================================================*/
/** <b>SHA3-384 Init</b>.
* The initialisation of SHA3-384.
*
* @param[in,out] context Pointer to the context
*
* @return Error code
*
* @retval #UCL_OK            if no error occurred
* @retval #UCL_INVALID_INPUT if @p context is #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_384_init(ucl_sha3_ctx_t *ctx);
/*============================================================================*/
/** <b>SHA3-512 Init</b>.
* The initialisation of SHA3-512.
*
* @param[in,out] context Pointer to the context
*
* @return Error code
*
* @retval #UCL_OK            if no error occurred
* @retval #UCL_INVALID_INPUT if @p context is #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_512_init(ucl_sha3_ctx_t *ctx);
/*============================================================================*/
/** <b>SHA3 Core</b>.
* The core of SHA3, common to all SHA3 hash functions.
*
* @param[in,out] context      Pointer to the context
* @param[in]     data         Pointer to the data
* @param[in]     data_byteLen Data byte length
*
* @warning #ucl_sha3-*_init must be processed before, and
*          #ucl_sha3_finish should be processed to get the final hash.
*
* @return Error code
*
* @retval #UCL_OK  if no error occurred
* @retval #UCL_NOP if @p data_byteLen = 0 or if @p data is the pointer #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_core(ucl_sha3_ctx_t *ctx, const unsigned char *bufIn, unsigned int len);
/*============================================================================*/
/** <b>SHA3 Finish</b>.
* Finish the process of SHA3, common to all SHA3 hash functions.
*
* @pre Hash byte length is equal to 28/32/48 or 64 bytes
*
* @param[out]    hash Pointer to the digest
* @param[in,out] context Pointer to the context
*
* @warning #ucl_sha3_*_init and #ucl_sha3_core must be processed before.
*
* @return Error code
*
* @retval #UCL_OK             if no error occurred
* @retval #UCL_INVALID_OUTPUT if @p hash is the pointer #NULL
* @retval #UCL_INVALID_INPUT  if @p context is the pointer #NULL
*
* @ingroup UCL_SHA3
*/

int ucl_sha3_finish(unsigned char *digest, ucl_sha3_ctx_t *ctx);

/*============================================================================*/
/** <b>SHA3-224</b>.
* The complete process of SHA3-224.
*
* @pre Hash byte length is equal to 28
*
* @param[out] hash         Pointer to the digest
* @param[in]  data         Pointer to the data
* @param[in]  data_byteLen Data byte length
*
* @return Error code
*
* @retval #UCL_OK             if no error occurred
* @retval #UCL_INVALID_OUTPUT if @p hash is #NULL
*
* @ingroup UCL_SHA3
*/

int ucl_sha3_224(unsigned char *digest, unsigned char *msg, unsigned int msgLen);

/*============================================================================*/
/** <b>SHA3-256</b>.
* The complete process of SHA3-256.
*
* @pre Hash byte length is equal to 32
*
* @param[out] hash         Pointer to the digest
* @param[in]  data         Pointer to the data
* @param[in]  data_byteLen Data byte length
*
* @return Error code
*
* @retval #UCL_OK             if no error occurred
* @retval #UCL_INVALID_OUTPUT if @p hash is #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_256(unsigned char *digest, unsigned char *msg, unsigned int msgLen);

/*============================================================================*/
/** <b>SHA3-384</b>.
* The complete process of SHA3-384.
*
* @pre Hash byte length is equal to 48
*
* @param[out] hash         Pointer to the digest
* @param[in]  data         Pointer to the data
* @param[in]  data_byteLen Data byte length
*
* @return Error code
*
* @retval #UCL_OK             if no error occurred
* @retval #UCL_INVALID_OUTPUT if @p hash is #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_384(unsigned char *digest, unsigned char *msg, unsigned int msgLen);

/*============================================================================*/
/** <b>SHA3-512</b>.
* The complete process of SHA3-512.
*
* @pre Hash byte length is equal to 64
*
* @param[out] hash         Pointer to the digest
* @param[in]  data         Pointer to the data
* @param[in]  data_byteLen Data byte length
*
* @return Error code
*
* @retval #UCL_OK             if no error occurred
* @retval #UCL_INVALID_OUTPUT if @p hash is #NULL
*
* @ingroup UCL_SHA3
*/
int ucl_sha3_512(unsigned char *digest, unsigned char *msg, unsigned int msgLen);
int ucl_shake128_init(ucl_sha3_ctx_t *ctx);
int ucl_shake256_init(ucl_sha3_ctx_t *ctx);
int ucl_shake_finish(unsigned char *hash, ucl_sha3_ctx_t *ctx);
int ucl_shake128(unsigned char *digest, unsigned char *msg, unsigned int msgLen);
int ucl_shake256(unsigned char *digest, unsigned char *msg, unsigned int msgLen);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _UCL_SHA3_H */
