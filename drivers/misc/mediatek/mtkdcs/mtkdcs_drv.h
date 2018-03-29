/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MTKDCS_DRV_H_
#define _MTKDCS_DRV_H_

/* enum should be aligned with tinysys middleware */

enum {
	IPI_DCS_MIGRATION,
	IPI_DCS_GET_MODE,
	IPI_DCS_SET_DUMMY_WRITE,
	IPI_DCS_SET_MD_NOTIFY,
	IPI_DCS_DUMP_REG,
	NR_DCS_IPI,
};

enum migrate_dir {
	NORMAL,
	LOWPWR,
};

enum initial_status {
	CASCADE_NORMAL_INTERLEAVE_NORMAL,
	CASCADE_LOWPWR_INTERLEAVE_NORMAL,
	CASCADE_NORMAL_INTERLEAVE_LOWPWR,
	CASCADE_LOWPWR_INTERLEAVE_LOWPWR,
};

#endif /* _MTKDCS_DRV_H_ */
