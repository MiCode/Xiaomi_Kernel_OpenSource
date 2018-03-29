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

#ifndef __FM_TYPEDEF_H__
#define __FM_TYPEDEF_H__

typedef signed char fm_s8;
typedef signed short fm_s16;
typedef signed int fm_s32;
typedef signed long long fm_s64;
typedef unsigned char fm_u8;
typedef unsigned short fm_u16;
typedef unsigned int fm_u32;
typedef unsigned long long fm_u64;

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef NULL
#define NULL  (0)
#endif

#ifndef BOOL
typedef unsigned char BOOL;
#endif

typedef enum fm_bool {
	fm_false = 0,
	fm_true = 1
} fm_bool;

#endif /* __FM_TYPEDEF_H__ */
