/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#define AES_MAXNR 14

struct AES_KEY {
	unsigned int rd_key[4 * (AES_MAXNR + 1)];
	int rounds;
};

struct AES_CTX {
	struct AES_KEY enc_key;
	struct AES_KEY dec_key;
};

asmlinkage void AES_encrypt_ce(const u8 *in, u8 *out, struct AES_KEY *ctx);
asmlinkage void AES_decrypt_ce(const u8 *in, u8 *out, struct AES_KEY *ctx);
asmlinkage int private_AES_set_decrypt_key_ce(const unsigned char *userKey,
					   const int bits, struct AES_KEY *key);
asmlinkage int private_AES_set_encrypt_key_ce(const unsigned char *userKey,
					   const int bits, struct AES_KEY *key);
