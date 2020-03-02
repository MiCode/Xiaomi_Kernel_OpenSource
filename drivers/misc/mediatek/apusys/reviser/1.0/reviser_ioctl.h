/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_REVISER_IOCTL_H__
#define __APUSYS_REVISER_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>


struct reviser_boundary {
	int type;
	int index;
	uint8_t boundary;
};
struct reviser_context_ID {
	int type;
	int index;
	uint8_t ID;
};

struct reviser_remap_table {
	uint8_t valid;
	int index;
	uint8_t ID;
	uint8_t src_page;
	uint8_t dst_page;
};
/* reviser driver's share structure */
struct reviser_ioctl_info {
	struct reviser_boundary bound;
	struct reviser_context_ID contex;
	struct reviser_remap_table table;
};

#define APUSYS_REVISER_MAGICNO 'R'
#define REVISER_IOCTL_PRINT \
		_IOWR(APUSYS_REVISER_MAGICNO, 0, int)
#define REVISER_IOCTL_SET_BOUNDARY \
		_IOWR(APUSYS_REVISER_MAGICNO, 1, struct reviser_ioctl_info)
#define REVISER_IOCTL_SET_CONTEXT_ID \
	_IOWR(APUSYS_REVISER_MAGICNO, 2, struct reviser_ioctl_info)
#define REVISER_IOCTL_SET_REMAP_TABLE \
		_IOWR(APUSYS_REVISER_MAGICNO, 3, struct reviser_ioctl_info)

#endif
