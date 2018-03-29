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

#ifndef __FM_PATCH_H__
#define __FM_PATCH_H__

enum {
	FM_ROM_V1 = 0,
	FM_ROM_V2 = 1,
	FM_ROM_V3 = 2,
	FM_ROM_V4 = 3,
	FM_ROM_V5 = 4,
	FM_ROM_MAX
};

struct fm_patch_tbl {
	fm_s32 idx;
	fm_s8 *patch;
	fm_s8 *coeff;
	fm_s8 *rom;
	fm_s8 *hwcoeff;
};

extern fm_s32 fm_file_read(const fm_s8 *filename, fm_u8 *dst, fm_s32 len, fm_s32 position);

extern fm_s32 fm_file_write(const fm_s8 *filename, fm_u8 *dst, fm_s32 len, fm_s32 *ppos);

#endif /* __FM_PATCH_H__ */
