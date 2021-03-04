/*
 *  Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : exfat_nls.h                                               */
/*  PURPOSE : Header File for exFAT NLS Manager                         */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  REVISION HISTORY (Ver 0.9)                                          */
/*                                                                      */
/*  - 2010.11.15 [Joosun Hahn] : first writing                          */
/*                                                                      */
/************************************************************************/

#ifndef _EXFAT_NLS_H
#define _EXFAT_NLS_H

#include <linux/types.h>
#include <linux/nls.h>

#include "exfat_config.h"
#include "exfat_api.h"

/*----------------------------------------------------------------------*/
/*  Constant & Macro Definitions                                        */
/*----------------------------------------------------------------------*/

#define NUM_UPCASE              2918

#define DOS_CUR_DIR_NAME        ".          "
#define DOS_PAR_DIR_NAME        "..         "

#ifdef __LITTLE_ENDIAN
#define UNI_CUR_DIR_NAME        ".\0"
#define UNI_PAR_DIR_NAME        ".\0.\0"
#else
#define UNI_CUR_DIR_NAME        "\0."
#define UNI_PAR_DIR_NAME        "\0.\0."
#endif

/*----------------------------------------------------------------------*/
/*  Type Definitions                                                    */
/*----------------------------------------------------------------------*/

/* DOS name stucture */
typedef struct {
	u8       name[DOS_NAME_LENGTH];
	u8       name_case;
} DOS_NAME_T;

/* unicode name stucture */
typedef struct {
	u16      name[MAX_NAME_LENGTH];
	u16      name_hash;
	u8       name_len;
} UNI_NAME_T;

/*----------------------------------------------------------------------*/
/*  External Function Declarations                                      */
/*----------------------------------------------------------------------*/

/* NLS management function */
u16 nls_upper(struct super_block *sb, u16 a);
s32  nls_dosname_cmp(struct super_block *sb, u8 *a, u8 *b);
s32  nls_uniname_cmp(struct super_block *sb, u16 *a, u16 *b);
void   nls_uniname_to_dosname(struct super_block *sb, DOS_NAME_T *p_dosname, UNI_NAME_T *p_uniname, s32 *p_lossy);
void   nls_dosname_to_uniname(struct super_block *sb, UNI_NAME_T *p_uniname, DOS_NAME_T *p_dosname);
void   nls_uniname_to_cstring(struct super_block *sb, u8 *p_cstring, UNI_NAME_T *p_uniname);
void   nls_cstring_to_uniname(struct super_block *sb, UNI_NAME_T *p_uniname, u8 *p_cstring, s32 *p_lossy);

#endif /* _EXFAT_NLS_H */
