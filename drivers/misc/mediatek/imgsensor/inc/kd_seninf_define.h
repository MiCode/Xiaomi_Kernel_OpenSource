/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
