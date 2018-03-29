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

#ifndef SEC_META_H
#define SEC_META_H

/* used for META library */
#define NVRAM_CIPHER_LEN (16)

/******************************************************************************
 *  MODEM CONTEXT FOR BOTH USER SPACE PROGRAM AND KERNEL MODULE
 ******************************************************************************/
typedef struct {
	unsigned char data[NVRAM_CIPHER_LEN];
	unsigned int ret;
} META_CONTEXT;

/******************************************************************************
 *  EXPORT FUNCTIONS
 ******************************************************************************/
extern int sec_nvram_enc(META_CONTEXT *meta_ctx);
extern int sec_nvram_dec(META_CONTEXT *meta_ctx);

#endif				/* SEC_META_H */
