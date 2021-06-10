/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __FP_VENDOR_H__
#define __FP_VENDOR_H__

enum {
	FP_VENDOR_INVALID = 0,
	FPC_VENDOR,
	GOODIX_VENDOR,
};

int get_fp_vendor(void);

#endif  /*__FP_VENDOR_H__*/
