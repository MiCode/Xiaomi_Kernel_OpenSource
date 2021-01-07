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
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : nls.c                                                     */
/*  PURPOSE : sdFAT NLS Manager                                         */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/
#include <linux/string.h>
#include <linux/nls.h>

#include "sdfat.h"
#include "core.h"

/*----------------------------------------------------------------------*/
/*  Global Variable Definitions                                         */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*  Local Variable Definitions                                          */
/*----------------------------------------------------------------------*/

static u16 bad_dos_chars[] = {
	/* + , ; = [ ] */
	0x002B, 0x002C, 0x003B, 0x003D, 0x005B, 0x005D,
	0xFF0B, 0xFF0C, 0xFF1B, 0xFF1D, 0xFF3B, 0xFF3D,
	0
};

/*
 * Allow full-width illegal characters :
 * "MS windows 7" supports full-width-invalid-name-characters.
 * So we should check half-width-invalid-name-characters(ASCII) only
 * for compatibility.
 *
 * " * / : < > ? \ |
 *
 * patch 1.2.0
 */
static u16 bad_uni_chars[] = {
	0x0022,         0x002A, 0x002F, 0x003A,
	0x003C, 0x003E, 0x003F, 0x005C, 0x007C,
#if 0 /* allow full-width characters */
	0x201C, 0x201D, 0xFF0A, 0xFF0F, 0xFF1A,
	0xFF1C, 0xFF1E, 0xFF1F, 0xFF3C, 0xFF5C,
#endif
	0
};

/*----------------------------------------------------------------------*/
/*  Local Function Declarations                                         */
/*----------------------------------------------------------------------*/
static s32  convert_uni_to_ch(struct nls_table *nls, u16 uni, u8 *ch, s32 *lossy);
static s32  convert_ch_to_uni(struct nls_table *nls, u8 *ch, u16 *uni, s32 *lossy);

static u16 nls_upper(struct super_block *sb, u16 a)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	if (SDFAT_SB(sb)->options.casesensitive)
		return a;
	if ((fsi->vol_utbl)[get_col_index(a)] != NULL)
		return (fsi->vol_utbl)[get_col_index(a)][get_row_index(a)];
	else
		return a;
}
/*======================================================================*/
/*  Global Function Definitions                                         */
/*======================================================================*/
u16 *nls_wstrchr(u16 *str, u16 wchar)
{
	while (*str) {
		if (*(str++) == wchar)
			return str;
	}

	return 0;
}

s32 nls_cmp_sfn(struct super_block *sb, u8 *a, u8 *b)
{
	return strncmp((void *)a, (void *)b, DOS_NAME_LENGTH);
}

s32 nls_cmp_uniname(struct super_block *sb, u16 *a, u16 *b)
{
	s32 i;

	for (i = 0; i < MAX_NAME_LENGTH; i++, a++, b++) {
		if (nls_upper(sb, *a) != nls_upper(sb, *b))
			return 1;
		if (*a == 0x0)
			return 0;
	}
	return 0;
}

#define CASE_LOWER_BASE (0x08)	/* base is lower case */
#define CASE_LOWER_EXT  (0x10)	/* extension is lower case */

s32 nls_uni16s_to_sfn(struct super_block *sb, UNI_NAME_T *p_uniname, DOS_NAME_T *p_dosname, s32 *p_lossy)
{
	s32 i, j, len, lossy = NLS_NAME_NO_LOSSY;
	u8 buf[MAX_CHARSET_SIZE];
	u8 lower = 0, upper = 0;
	u8 *dosname = p_dosname->name;
	u16 *uniname = p_uniname->name;
	u16 *p, *last_period;
	struct nls_table *nls = SDFAT_SB(sb)->nls_disk;

	/* DOSNAME is filled with space */
	for (i = 0; i < DOS_NAME_LENGTH; i++)
		*(dosname+i) = ' ';

	/* DOT and DOTDOT are handled by VFS layer */

	/* search for the last embedded period */
	last_period = NULL;
	for (p = uniname; *p; p++) {
		if (*p == (u16) '.')
			last_period = p;
	}

	i = 0;
	while (i < DOS_NAME_LENGTH) {
		if (i == 8) {
			if (last_period == NULL)
				break;

			if (uniname <= last_period) {
				if (uniname < last_period)
					lossy |= NLS_NAME_OVERLEN;
				uniname = last_period + 1;
			}
		}

		if (*uniname == (u16) '\0') {
			break;
		} else if (*uniname == (u16) ' ') {
			lossy |= NLS_NAME_LOSSY;
		} else if (*uniname == (u16) '.') {
			if (uniname < last_period)
				lossy |= NLS_NAME_LOSSY;
			else
				i = 8;
		} else if (nls_wstrchr(bad_dos_chars, *uniname)) {
			lossy |= NLS_NAME_LOSSY;
			*(dosname+i) = '_';
			i++;
		} else {
			len = convert_uni_to_ch(nls, *uniname, buf, &lossy);

			if (len > 1) {
				if ((i >= 8) && ((i+len) > DOS_NAME_LENGTH))
					break;

				if ((i <  8) && ((i+len) > 8)) {
					i = 8;
					continue;
				}

				lower = 0xFF;

				for (j = 0; j < len; j++, i++)
					*(dosname+i) = *(buf+j);
			} else { /* len == 1 */
				if ((*buf >= 'a') && (*buf <= 'z')) {
					*(dosname+i) = *buf - ('a' - 'A');

					lower |= (i < 8) ?
						CASE_LOWER_BASE :
						CASE_LOWER_EXT;
				} else if ((*buf >= 'A') && (*buf <= 'Z')) {
					*(dosname+i) = *buf;

					upper |= (i < 8) ?
						CASE_LOWER_BASE :
						CASE_LOWER_EXT;
				} else {
					*(dosname+i) = *buf;
				}
				i++;
			}
		}

		uniname++;
	}

	if (*dosname == 0xE5)
		*dosname = 0x05;
	if (*uniname != 0x0)
		lossy |= NLS_NAME_OVERLEN;

	if (upper & lower)
		p_dosname->name_case = 0xFF;
	else
		p_dosname->name_case = lower;

	if (p_lossy)
		*p_lossy = lossy;
	return i;
}

s32 nls_sfn_to_uni16s(struct super_block *sb, DOS_NAME_T *p_dosname, UNI_NAME_T *p_uniname)
{
	s32 i = 0, j, n = 0;
	u8 buf[MAX_DOSNAME_BUF_SIZE];
	u8 *dosname = p_dosname->name;
	u16 *uniname = p_uniname->name;
	struct nls_table *nls = SDFAT_SB(sb)->nls_disk;

	if (*dosname == 0x05) {
		*buf = 0xE5;
		i++;
		n++;
	}

	for ( ; i < 8; i++, n++) {
		if (*(dosname+i) == ' ')
			break;

		if ((*(dosname+i) >= 'A') && (*(dosname+i) <= 'Z') &&
				(p_dosname->name_case & CASE_LOWER_BASE))
			*(buf+n) = *(dosname+i) + ('a' - 'A');
		else
			*(buf+n) = *(dosname+i);
	}
	if (*(dosname+8) != ' ') {
		*(buf+n) = '.';
		n++;
	}

	for (i = 8; i < DOS_NAME_LENGTH; i++, n++) {
		if (*(dosname+i) == ' ')
			break;

		if ((*(dosname+i) >= 'A') && (*(dosname+i) <= 'Z') &&
			       (p_dosname->name_case & CASE_LOWER_EXT))
			*(buf+n) = *(dosname+i) + ('a' - 'A');
		else
			*(buf+n) = *(dosname+i);
	}
	*(buf+n) = '\0';

	i = j = 0;
	while (j < MAX_NAME_LENGTH) {
		if (*(buf+i) == '\0')
			break;

		i += convert_ch_to_uni(nls, (buf+i), uniname, NULL);

		uniname++;
		j++;
	}

	*uniname = (u16) '\0';
	return j;
}

static s32 __nls_utf16s_to_vfsname(struct super_block *sb, UNI_NAME_T *p_uniname, u8 *p_cstring, s32 buflen)
{
	s32 len;
	const u16 *uniname = p_uniname->name;

	/* always len >= 0 */
	len = utf16s_to_utf8s(uniname, MAX_NAME_LENGTH, UTF16_HOST_ENDIAN,
		p_cstring, buflen);
	p_cstring[len] = '\0';
	return len;
}

static s32 __nls_vfsname_to_utf16s(struct super_block *sb, const u8 *p_cstring,
		const s32 len, UNI_NAME_T *p_uniname, s32 *p_lossy)
{
	s32 i, unilen, lossy = NLS_NAME_NO_LOSSY;
	u16 upname[MAX_NAME_LENGTH+1];
	u16 *uniname = p_uniname->name;

	BUG_ON(!len);

	unilen = utf8s_to_utf16s(p_cstring, len, UTF16_HOST_ENDIAN,
			(wchar_t *)uniname, MAX_NAME_LENGTH+2);
	if (unilen < 0) {
		MMSG("%s: failed to vfsname_to_utf16(err:%d) "
			"vfsnamelen:%d", __func__, unilen, len);
		return unilen;
	}

	if (unilen > MAX_NAME_LENGTH) {
		MMSG("%s: failed to vfsname_to_utf16(estr:ENAMETOOLONG) "
			"vfsnamelen:%d, unilen:%d>%d",
			__func__, len, unilen, MAX_NAME_LENGTH);
		return -ENAMETOOLONG;
	}

	p_uniname->name_len = (u8)(unilen & 0xFF);

	for (i = 0; i < unilen; i++) {
		if ((*uniname < 0x0020) || nls_wstrchr(bad_uni_chars, *uniname))
			lossy |= NLS_NAME_LOSSY;

		*(upname+i) = nls_upper(sb, *uniname);
		uniname++;
	}

	*uniname = (u16)'\0';
	p_uniname->name_len = unilen;
	p_uniname->name_hash = calc_chksum_2byte((void *) upname,
				unilen << 1, 0, CS_DEFAULT);

	if (p_lossy)
		*p_lossy = lossy;

	return unilen;
}

static s32 __nls_uni16s_to_vfsname(struct super_block *sb, UNI_NAME_T *p_uniname, u8 *p_cstring, s32 buflen)
{
	s32 i, j, len, out_len = 0;
	u8 buf[MAX_CHARSET_SIZE];
	const u16 *uniname = p_uniname->name;
	struct nls_table *nls = SDFAT_SB(sb)->nls_io;

	i = 0;
	while ((i < MAX_NAME_LENGTH) && (out_len < (buflen-1))) {
		if (*uniname == (u16)'\0')
			break;

		len = convert_uni_to_ch(nls, *uniname, buf, NULL);

		if (out_len + len >= buflen)
			len = (buflen - 1) - out_len;

		out_len += len;

		if (len > 1) {
			for (j = 0; j < len; j++)
				*p_cstring++ = (s8) *(buf+j);
		} else { /* len == 1 */
			*p_cstring++ = (s8) *buf;
		}

		uniname++;
		i++;
	}

	*p_cstring = '\0';
	return out_len;
}

static s32 __nls_vfsname_to_uni16s(struct super_block *sb, const u8 *p_cstring,
		const s32 len, UNI_NAME_T *p_uniname, s32 *p_lossy)
{
	s32 i, unilen, lossy = NLS_NAME_NO_LOSSY;
	u16 upname[MAX_NAME_LENGTH+1];
	u16 *uniname = p_uniname->name;
	struct nls_table *nls = SDFAT_SB(sb)->nls_io;

	BUG_ON(!len);

	i = unilen = 0;
	while ((unilen < MAX_NAME_LENGTH) && (i < len)) {
		i += convert_ch_to_uni(nls, (u8 *)(p_cstring+i), uniname, &lossy);

		if ((*uniname < 0x0020) || nls_wstrchr(bad_uni_chars, *uniname))
			lossy |= NLS_NAME_LOSSY;

		*(upname+unilen) = nls_upper(sb, *uniname);

		uniname++;
		unilen++;
	}

	if (*(p_cstring+i) != '\0')
		lossy |= NLS_NAME_OVERLEN;

	*uniname = (u16)'\0';
	p_uniname->name_len = unilen;
	p_uniname->name_hash =
		calc_chksum_2byte((void *) upname, unilen<<1, 0, CS_DEFAULT);

	if (p_lossy)
		*p_lossy = lossy;

	return unilen;
}

s32 nls_uni16s_to_vfsname(struct super_block *sb, UNI_NAME_T *uniname, u8 *p_cstring, s32 buflen)
{
	if (SDFAT_SB(sb)->options.utf8)
		return __nls_utf16s_to_vfsname(sb, uniname, p_cstring, buflen);

	return __nls_uni16s_to_vfsname(sb, uniname, p_cstring, buflen);
}

s32 nls_vfsname_to_uni16s(struct super_block *sb, const u8 *p_cstring, const s32 len, UNI_NAME_T *uniname, s32 *p_lossy)
{
	if (SDFAT_SB(sb)->options.utf8)
		return __nls_vfsname_to_utf16s(sb, p_cstring, len, uniname, p_lossy);
	return __nls_vfsname_to_uni16s(sb, p_cstring, len, uniname, p_lossy);
}

/*======================================================================*/
/*  Local Function Definitions                                          */
/*======================================================================*/

static s32 convert_ch_to_uni(struct nls_table *nls, u8 *ch, u16 *uni, s32 *lossy)
{
	int len;

	*uni = 0x0;

	if (ch[0] < 0x80) {
		*uni = (u16) ch[0];
		return 1;
	}

	len = nls->char2uni(ch, MAX_CHARSET_SIZE, uni);
	if (len < 0) {
		/* conversion failed */
		DMSG("%s: fail to use nls\n", __func__);
		if (lossy != NULL)
			*lossy |= NLS_NAME_LOSSY;
		*uni = (u16) '_';
		if (!strcmp(nls->charset, "utf8"))
			return 1;
		return 2;
	}

	return len;
} /* end of convert_ch_to_uni */

static s32 convert_uni_to_ch(struct nls_table *nls, u16 uni, u8 *ch, s32 *lossy)
{
	int len;

	ch[0] = 0x0;

	if (uni < 0x0080) {
		ch[0] = (u8) uni;
		return 1;
	}

	len = nls->uni2char(uni, ch, MAX_CHARSET_SIZE);
	if (len < 0) {
		/* conversion failed */
		DMSG("%s: fail to use nls\n", __func__);
		if (lossy != NULL)
			*lossy |= NLS_NAME_LOSSY;
		ch[0] = '_';
		return 1;
	}

	return len;

} /* end of convert_uni_to_ch */

/* end of nls.c */
