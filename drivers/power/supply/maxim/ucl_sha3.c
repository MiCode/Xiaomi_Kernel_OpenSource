//SHA3 implementation
//this implementation used the implementation from Markku-Juhani O.Saarinen
//as a starting point
/*******************************************************************************
* Copyright (C) 2017 Maxim Integrated Products, Inc., All rights Reserved.
*
* This software is protected by copyright laws of the United States and
* of foreign countries. This material may also be protected by patent laws
* and technology transfer regulations of the United States and of foreign
* countries. This software is furnished under a license agreement and/or a
* nondisclosure agreement and may only be used or reproduced in accordance
* with the terms of those agreements. Dissemination of this information to
* any party or parties not specified in the license agreement and/or
* nondisclosure agreement is expressly prohibited.
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************

The MIT License (MIT)

Copyright (c) 2015 Markku-Juhani O. Saarinen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "ucl_hash.h"
#ifdef HASH_SHA3

#include <linux/string.h>
#include "ucl_retdefs.h"
#include "ucl_sha3.h"

#define N_ROUNDS 24 // the specialization for  keccak-f from keccak-p
#define ROTL64(x, y) (((x) << (y)) | ((x) >> ((sizeof(unsigned long long)*8) - (y))))

const unsigned long long kcf_rc[24] = {
									0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
									0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
									0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
									0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
									0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
									0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
									0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
									0x8000000000008080, 0x0000000080000001, 0x8000000080008008};
static const unsigned char kcf_rho[24] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};
static const unsigned char kcf_pilane[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};

// generally called after SHA3_SPONGE_WORDS-ctx->capacityWords words
// are XORed into the state s
static void kcf(unsigned long long state[25])
{
	int i, j, round;
	unsigned long long t, c[5];

	//I(chi(Pi(ro(theta(
	for (round = 0; round < N_ROUNDS; round++) {
		// Theta
		for (i = 0; i < 5; i++)
			c[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];

		for (i = 0; i < 5; i++) {
			t = c[(i + 4) % 5] ^ (unsigned long long)ROTL64(c[(i + 1) % 5], 1);
			for (j = 0; j < 25; j += 5)
				state[j + i] ^= t;
		}

		// Rho Pi
		t = state[1];
		for (i = 0; i < 24; i++) {
			j = (int)kcf_pilane[i];
			c[0] = state[j];
			state[j] = (unsigned long long)ROTL64(t, kcf_rho[i]);
			t = c[0];
		}

		// Chi
		for (j = 0; j < 25; j += 5) {
			for (i = 0; i < 5; i++)
				c[i] = state[j + i];
			for (i = 0; i < 5; i++)
				state[j + i] ^= (~c[(i + 1) % 5]) & c[(i + 2) % 5];
		}
		// Iota
		state[0] ^= kcf_rc[round];
	}
}

int ucl_shake128_init(ucl_sha3_ctx_t *ctx)
{
	if (ctx == NULL)
		return UCL_INVALID_INPUT;
	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 2 * 128 / (8 * sizeof(unsigned long long));
	return UCL_OK;
}

int ucl_sha3_224_init(ucl_sha3_ctx_t *ctx)
{
	if (ctx == NULL)
		return UCL_INVALID_INPUT;
	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 448 / (8 * sizeof(unsigned long long));
	return UCL_OK;
}

int  ucl_sha3_256_init(ucl_sha3_ctx_t *ctx)
{
	if (ctx == NULL)
		return UCL_INVALID_INPUT;
	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 512 / (8 * sizeof(unsigned long long));
	return UCL_OK;
}

int ucl_shake256_init(ucl_sha3_ctx_t *ctx)
{
	return ucl_sha3_256_init(ctx);
}

int  ucl_sha3_384_init(ucl_sha3_ctx_t *ctx)
{
	if (ctx == NULL)
		return UCL_INVALID_INPUT;
	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 768 / (8 * sizeof(unsigned long long));
	return UCL_OK;
}

int  ucl_sha3_512_init(ucl_sha3_ctx_t *ctx)
{
	if (ctx == NULL)
		return UCL_INVALID_INPUT;
	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 1024 / (8 * sizeof(unsigned long long));
	return UCL_OK;
}

int ucl_sha3_core(ucl_sha3_ctx_t *ctx, const unsigned char *bufIn, unsigned int len)
{
	unsigned int old_tail = (8 - ctx->byteIndex) & 7;
	size_t words;
	int tail;
	size_t i;
	const unsigned char *buf = bufIn;

	if (ctx == NULL)
		return UCL_INVALID_INPUT;
	if (bufIn == NULL)
		return UCL_INVALID_INPUT;
	if (len < old_tail) {
		while (len--)
			ctx->saved |= (unsigned long long) (*(buf++)) << ((ctx->byteIndex++) * 8);
		return UCL_OK;
	}

	if (old_tail) {
		len -= old_tail;
		while (old_tail--)
			ctx->saved |= (unsigned long long) (*(buf++)) << ((ctx->byteIndex++) * 8);
		ctx->s[ctx->wordIndex] ^= ctx->saved;
		ctx->byteIndex = 0;
		ctx->saved = 0;
		if (++ctx->wordIndex == ((int)SHA3_SPONGE_WORDS - ctx->capacityWords)) {
			kcf(ctx->s);
			ctx->wordIndex = 0;
		}
	}

	words = len / sizeof(unsigned long long);
	tail = (int)(len - words * sizeof(unsigned long long));
	for (i = 0; i < words; i++, buf += sizeof(unsigned long long)) {
		const unsigned long long t = (unsigned long long) (buf[0]) |
		((unsigned long long) (buf[1]) << 8 * 1) |
		((unsigned long long) (buf[2]) << 8 * 2) |
		((unsigned long long) (buf[3]) << 8 * 3) |
		((unsigned long long) (buf[4]) << 8 * 4) |
		((unsigned long long) (buf[5]) << 8 * 5) |
		((unsigned long long) (buf[6]) << 8 * 6) |
		((unsigned long long) (buf[7]) << 8 * 7);
		ctx->s[ctx->wordIndex] ^= t;

		if (++ctx->wordIndex == ((int)SHA3_SPONGE_WORDS - ctx->capacityWords)) {
			kcf(ctx->s);
			ctx->wordIndex = 0;
		}
	}

	while (tail--)
		ctx->saved |= (unsigned long long) (*(buf++)) << ((ctx->byteIndex++) * 8);

	return UCL_OK;
}

int ucl_sha3_finish(unsigned char *digest, ucl_sha3_ctx_t *ctx)
{
	int i;

	// SHA3 version
	ctx->s[ctx->wordIndex] ^= (ctx->saved ^ ((unsigned long long) ((unsigned long long) (0x02 | (1 << 2)) << ((ctx->byteIndex) * 8))));
	ctx->s[(int)SHA3_SPONGE_WORDS - ctx->capacityWords - 1] ^= (unsigned long long)0x8000000000000000UL;
	kcf(ctx->s);

	if (digest == NULL)
		return UCL_INVALID_OUTPUT;
	if (ctx == NULL)
		return UCL_INVALID_INPUT;

	for (i = 0; i < (int)SHA3_SPONGE_WORDS; i++) {
		const unsigned int t1 = (unsigned int) ctx->s[i];
		const unsigned int t2 = (unsigned int) ((ctx->s[i] >> 16) >> 16);

		ctx->sb[i * 8 + 0] = (unsigned char) (t1);
		ctx->sb[i * 8 + 1] = (unsigned char) (t1 >> 8);
		ctx->sb[i * 8 + 2] = (unsigned char) (t1 >> 16);
		ctx->sb[i * 8 + 3] = (unsigned char) (t1 >> 24);
		ctx->sb[i * 8 + 4] = (unsigned char) (t2);
		ctx->sb[i * 8 + 5] = (unsigned char) (t2 >> 8);
		ctx->sb[i * 8 + 6] = (unsigned char) (t2 >> 16);
		ctx->sb[i * 8 + 7] = (unsigned char) (t2 >> 24);
	}

	for (i = 0; i < (int)SHA3_SPONGE_WORDS * 8; i++)
		digest[i] = ctx->sb[i];

	return UCL_OK;
}

int ucl_shake_finish(unsigned char *digest, ucl_sha3_ctx_t *ctx)
{
	int i;

	if (ctx == NULL)
		return UCL_INVALID_INPUT;
	if (digest == NULL)
		return UCL_INVALID_OUTPUT;

	ctx->s[ctx->wordIndex] ^= (ctx->saved ^ ((unsigned long long) (0x1F) << ((ctx->byteIndex) * 8)));
	i = (int)SHA3_SPONGE_WORDS - ctx->capacityWords - 1;
	ctx->s[(int)SHA3_SPONGE_WORDS - ctx->capacityWords - 1] ^= (unsigned long long)(0x8000000000000000UL);
	kcf(ctx->s);

	for (i = 0; i < (int)SHA3_SPONGE_WORDS; i++) {
		const unsigned int t1 = (unsigned int) ctx->s[i];
		const unsigned int t2 = (unsigned int) ((ctx->s[i] >> 16) >> 16);

		ctx->sb[i * 8 + 0] = (unsigned char) (t1);
		ctx->sb[i * 8 + 1] = (unsigned char) (t1 >> 8);
		ctx->sb[i * 8 + 2] = (unsigned char) (t1 >> 16);
		ctx->sb[i * 8 + 3] = (unsigned char) (t1 >> 24);
		ctx->sb[i * 8 + 4] = (unsigned char) (t2);
		ctx->sb[i * 8 + 5] = (unsigned char) (t2 >> 8);
		ctx->sb[i * 8 + 6] = (unsigned char) (t2 >> 16);
		ctx->sb[i * 8 + 7] = (unsigned char) (t2 >> 24);
	}

	for (i = 0; i < (int)SHA3_SPONGE_WORDS * 8; i++)
		digest[i] = ctx->sb[i];

	return UCL_OK;
}

int ucl_sha3_224(unsigned char *digest, unsigned char *msg, unsigned int msgLen)
{
	ucl_sha3_ctx_t ctx;

	if (msg == NULL)
		return UCL_INVALID_INPUT;
	if (digest == NULL)
		return UCL_INVALID_OUTPUT;
	if (ucl_sha3_224_init(&ctx) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_core(&ctx, msg, msgLen) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_finish(digest, &ctx) != UCL_OK)
		return(UCL_ERROR);

	return UCL_OK;
}

int ucl_sha3_256(unsigned char *digest, unsigned char *msg, unsigned int msgLen)
{
	ucl_sha3_ctx_t ctx;

	if (msg == NULL)
		return UCL_INVALID_INPUT;
	if (digest == NULL)
		return UCL_INVALID_OUTPUT;

	if (ucl_sha3_256_init(&ctx) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_core(&ctx, msg, msgLen) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_finish(digest, &ctx) != UCL_OK)
		return(UCL_ERROR);

	return UCL_OK;
}

int ucl_shake128(unsigned char *digest, unsigned char *msg, unsigned int msgLen)
{
	ucl_sha3_ctx_t ctx;

	if (msg == NULL)
		return UCL_INVALID_INPUT;
	if (digest == NULL)
		return UCL_INVALID_OUTPUT;

	if (ucl_shake128_init(&ctx) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_core(&ctx, msg, msgLen) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_shake_finish(digest, &ctx) != UCL_OK)
		return(UCL_ERROR);

	return UCL_OK;
}

int ucl_sha3_384(unsigned char *digest, unsigned char *msg, unsigned int msgLen)
{
	ucl_sha3_ctx_t ctx;

	if (msg == NULL)
		return UCL_INVALID_INPUT;
	if (digest == NULL)
		return UCL_INVALID_OUTPUT;

	if (ucl_sha3_384_init(&ctx) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_core(&ctx, msg, msgLen) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_finish(digest, &ctx) != UCL_OK)
		return(UCL_ERROR);

	return UCL_OK;
}

int ucl_sha3_512(unsigned char *digest, unsigned char *msg, unsigned int msgLen)
{
	ucl_sha3_ctx_t ctx;

	if (msg == NULL)
		return UCL_INVALID_INPUT;
	if (digest == NULL)
		return UCL_INVALID_OUTPUT;

	if (UCL_OK != ucl_sha3_512_init(&ctx))
		return(UCL_ERROR);
	if (ucl_sha3_core(&ctx, msg, msgLen) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_finish(digest, &ctx) != UCL_OK)
		return(UCL_ERROR);

	return UCL_OK;
}
int ucl_shake256(unsigned char *digest, unsigned char *msg, unsigned int msgLen)
{
	ucl_sha3_ctx_t ctx;

	if (msg == NULL)
		return UCL_INVALID_INPUT;
	if (digest == NULL)
		return UCL_INVALID_OUTPUT;

	if (ucl_shake256_init(&ctx) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_sha3_core(&ctx, msg, msgLen) != UCL_OK)
		return(UCL_ERROR);
	if (ucl_shake_finish(digest, &ctx) != UCL_OK)
		return(UCL_ERROR);

	return UCL_OK;
}
#endif//SHA3
