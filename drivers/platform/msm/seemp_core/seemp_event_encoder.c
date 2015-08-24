/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define PROVIDE_PARAM_ID

#include "seemp_logk.h"
#include <linux/seemp_param_id.h>
#include "seemp_event_encoder.h"

static char *scan_id(char *s);
static void encode_seemp_section(char *section_start, char *section_eq,
				char *section_end, bool param, bool numeric,
				int id, __s32 numeric_value);

static void check_param_range(char *section_eq, bool param,
	bool *numeric, int val_len, __s32 *numeric_value)
{
	long long_value = 0;

	if (param && *numeric) {
		/*check if 2 bytes & in[-99999,999999]*/
		*numeric = (val_len >= 2) && (val_len <= 6);
		if (*numeric) {
			if (kstrtol(section_eq + 1, 10, &long_value)
			!= 0) {
				*numeric = false;
			} else {
				*numeric_value = (__s32)long_value;
				/* We are checking whether the value
				*  lies within 16bits
				*/
				*numeric = (long_value >= -32768) &&
					(long_value <= 32767);
			}
		}
	}
}

void encode_seemp_params(struct seemp_logk_blk *blk)
{
	char *s = blk->payload.msg + 1;

	blk->payload.msg[BLK_MAX_MSG_SZ - 1] = 0; /* zero-terminate */

	while (true) {
		char *section_start = s;
		char *section_eq    = scan_id(s);
		bool  param         = (section_eq - section_start >= 2) &&
			(*section_eq == '=') && (section_eq[1] != ' ');
		bool  numeric       = false;
		int   id            = -1;
		__s32 numeric_value = 0;
		int id_len;
		int val_len;
		char ch;

		if (param) {
			id = param_id_index(section_start, section_eq);

			if (id < 0)
				param = false;
		}

		if (!param) {
			s = section_eq;
			while ((*s != 0) && (*s != ','))
				s++;
		} else {
			s = section_eq + 1; /* equal sign */
			numeric = (*s == '-') || ((*s >= '0') && (*s <= '9'));

			if (numeric)
				s++; /* first char of number */

			while ((*s != 0) && (*s != ',')) {
				if (*s == '=')
					param   = false;
				else if (!((*s >= '0') && (*s <= '9')))
					numeric = false;

				s++;
			}

			if (param) {
				id_len  = section_eq - section_start;
				val_len = s - (section_eq + 1);
				param = (id_len >= 2) && (id_len <= 31)
							&& (val_len <= 31);
				ch = *s;
				*s = 0;

				check_param_range(section_eq, param,
					&numeric, val_len, &numeric_value);
				*s = ch;
			}
		}

		encode_seemp_section(section_start, section_eq, s, param,
					numeric, id, numeric_value);

		if (*s == 0)
			break;

		s++;
	}

	blk->len = s - blk->payload.msg;
}

static char *scan_id(char *s)
{
	while ((*s == '_') ||
		((*s >= 'A') && (*s <= 'Z')) ||
		((*s >= 'a') && (*s <= 'z'))) {
		s++;
	}

	return s;
}

static void encode_seemp_section(char *section_start, char *section_eq,
				char *section_end, bool param, bool numeric,
				int id, __s32 numeric_value) {
	param = param && (section_eq + 1 < section_end);

	if (!param) {
		/* Encode skip section */
		int  skip_len	= section_end - section_start;
		char skip_len_hi = skip_len & 0xE0;
		char skip_len_lo = skip_len & 0x1F;

		if (skip_len < 32) {
			section_start[-1] = 0xC0 | skip_len_lo;
							/* [1:1:0:0 0000] */
		} else {
			section_start[-1] = 0xE0 | skip_len_lo;
							/* [1:1:1:0 0000] */

			if (skip_len_hi & 0x20)
				section_start[0] |= 0x80;

			if (skip_len_hi & 0x40)
				section_start[1] |= 0x80;

			if (skip_len_hi & 0x80)
				section_start[2] |= 0x80;
		}
	} else {
		/* Encode ID=VALUE section */
		char id_len            = section_eq  - section_start;
		char value_len         = section_end - (section_eq + 1);

		section_start[-1]      = 0x00 | id_len;
		*(__s16 *)section_start = id;
		section_eq[0]          = (!numeric ? 0x80 : 0x00) | value_len;

		if (numeric)
			*(__s16 *)(section_eq + 1) = numeric_value;
	}
}
