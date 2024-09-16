/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "gps_dl_lib_misc.h"
#include "gps_dl_log.h"

bool gps_dl_hal_comp_buf_match(unsigned char *data_buf, unsigned int data_len,
	unsigned char *golden_buf, unsigned int golden_len, unsigned int data_shift) {
	bool is_match = true;

	int i;

	if (data_len < golden_len + data_shift) {
		GDL_LOGD("not match len: %d, %d, %d", data_len, golden_len, data_shift);
		is_match = false;
	}

	if (is_match) {
		for (i = 0; i < data_shift; i++) {
			if (data_buf[i] != 0) {
				GDL_LOGD("not fill 0 on start %d: %x", i, data_buf[i]);
				is_match = false;
				break;
			}
		}
	}

	if (is_match) {
		for (; i < data_shift + golden_len; i++) {
			if (data_buf[i] != golden_buf[i - data_shift]) {
				GDL_LOGD("not match on data[%d] -> %x, gold[%d] -> %x",
					i, data_buf[i], i - data_shift, golden_buf[i - data_shift]);
				is_match = false;
				break;
			}
		}
	}

	if (is_match) {
		for (; i < data_len; i++) {
			if (data_buf[i] != 0) {
				GDL_LOGD("not fill 0 on end %d: %x", i, data_buf[i]);
				is_match = false;
				break;
			}
		}
	}

	GDL_LOGD("match = %d, data_len = %d, golden_len = %d, data_shift = %d",
		is_match, data_len, golden_len, data_shift);

	if (!is_match) {
		gps_dl_hal_show_buf("data", data_buf, data_len);
		gps_dl_hal_show_buf("golden", golden_buf, golden_len);
	}

	return is_match;
}

#define SHOW_BUF_MAX_LINE 2
void gps_dl_hal_show_buf(unsigned char *tag,
	unsigned char *buf, unsigned int len)
{
	int base = 0, line_idx = 0;
	int line_len = 8;
	int left_len = len;
#define SHOW_BUF_FMT0 "[%s] len = %u"
#define SHOW_BUF_FMT1 SHOW_BUF_FMT0", data = %02x"
#define SHOW_BUF_FMT2 SHOW_BUF_FMT1" %02x"
#define SHOW_BUF_FMT3 SHOW_BUF_FMT2" %02x"
#define SHOW_BUF_FMT4 SHOW_BUF_FMT3" %02x"
#define SHOW_BUF_FMT5 SHOW_BUF_FMT4", %02x"
#define SHOW_BUF_FMT6 SHOW_BUF_FMT5" %02x"
#define SHOW_BUF_FMT7 SHOW_BUF_FMT6" %02x"
#define SHOW_BUF_FMT8 SHOW_BUF_FMT7" %02x"

#define SHOW_BUF_ARG0 do {GDL_LOGI_DRW(SHOW_BUF_FMT0, tag, len); } while (0)

#define SHOW_BUF_ARG1 do {GDL_LOGI_DRW(SHOW_BUF_FMT1, tag, len, buf[base+0]); } while (0)

#define SHOW_BUF_ARG2 do {GDL_LOGI_DRW(SHOW_BUF_FMT2, tag, len, buf[base+0], buf[base+1]); } while (0)

#define SHOW_BUF_ARG3 do {GDL_LOGI_DRW(SHOW_BUF_FMT3, tag, len, buf[base+0], buf[base+1], buf[base+2]) \
	; } while (0)

#define SHOW_BUF_ARG4 do {GDL_LOGI_DRW(SHOW_BUF_FMT4, tag, len, buf[base+0], buf[base+1], buf[base+2], \
	buf[base+3]); } while (0)

#define SHOW_BUF_ARG5 do {GDL_LOGI_DRW(SHOW_BUF_FMT5, tag, len, buf[base+0], buf[base+1], buf[base+2], \
	buf[base+3], buf[base+4]); } while (0)

#define SHOW_BUF_ARG6 do {GDL_LOGI_DRW(SHOW_BUF_FMT6, tag, len, buf[base+0], buf[base+1], buf[base+2], \
	buf[base+3], buf[base+4], buf[base+5]); } while (0)

#define SHOW_BUF_ARG7 do {GDL_LOGI_DRW(SHOW_BUF_FMT7, tag, len, buf[base+0], buf[base+1], buf[base+2], \
	buf[base+3], buf[base+4], buf[base+5], buf[base+6]); } while (0)

#define SHOW_BUF_ARG8 do {GDL_LOGI_DRW(SHOW_BUF_FMT8, tag, len, buf[base+0], buf[base+1], buf[base+2], \
	buf[base+3], buf[base+4], buf[base+5], buf[base+6], buf[base+7]); } while (0)

	for (left_len = len, base = 0, line_idx = 0;
		left_len > 0 && line_idx < SHOW_BUF_MAX_LINE;
		left_len -= 8, base += 8, line_idx++) {

		if (left_len > 8)
			line_len = 8;
		else
			line_len = left_len;

		switch (line_len) {
#if 0
		/* case 0 is impossible */
		case 0:
			SHOW_BUF_ARG0; break;
#endif
		case 1:
			SHOW_BUF_ARG1; break;
		case 2:
			SHOW_BUF_ARG2; break;
		case 3:
			SHOW_BUF_ARG3; break;
		case 4:
			SHOW_BUF_ARG4; break;
		case 5:
			SHOW_BUF_ARG5; break;
		case 6:
			SHOW_BUF_ARG6; break;
		case 7:
			SHOW_BUF_ARG7; break;
		default:
			SHOW_BUF_ARG8; break;
		}
	}
}

void GDL_VOIDF(void) {}

