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
#ifndef _GPS_DL_BASE_H
#define _GPS_DL_BASE_H

enum GDL_RET_STATUS {
	GDL_OKAY = 0,
	GDL_FAIL, /* general fail */
	GDL_FAIL_ASSERT,
	GDL_FAIL_BUSY,
	GDL_FAIL_NOSPACE,
	GDL_FAIL_NODATA,
	GDL_FAIL_STATE_MISMATCH,
	GDL_FAIL_SIGNALED,
	GDL_FAIL_TIMEOUT,
	GDL_FAIL_NOT_SUPPORT,
	GDL_FAIL_INVAL,
	GDL_FAIL_NOSPACE_PENDING_RX,
	GDL_FAIL_NOENTRY,
	GDL_FAIL_NOENTRY2,
	GDL_FAIL_CONN_NOT_OKAY,
	GDL_RET_NUM,
};

const char *gdl_ret_to_name(enum GDL_RET_STATUS gdl_ret);

#endif /* _GPS_DL_BASE_H */

