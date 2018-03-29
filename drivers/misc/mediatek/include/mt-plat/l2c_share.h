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

#ifndef _L2_SHARE_H
#define _L2_SHARE_H

enum mt_l2c_options {
	BORROW_L2,
	RETURN_L2,
	BORROW_NONE
};

int switch_L2(enum mt_l2c_options option);

#endif
