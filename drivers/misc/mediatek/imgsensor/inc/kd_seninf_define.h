/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef _KD_SENINF_DEFINE_H_
#define _KD_SENINF_DEFINE_H_

/*************************************************
 *
 **************************************************/

struct KD_SENINF_MMAP {
	MUINT32 map_addr;
	MUINT32 map_length;
};

struct KD_SENINF_REG {
	struct KD_SENINF_MMAP seninf;
	struct KD_SENINF_MMAP ana;
	struct KD_SENINF_MMAP gpio;
};

#endif
