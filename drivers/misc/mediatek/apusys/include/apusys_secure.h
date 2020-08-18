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

#ifndef __APUSYS_SECURE_H__
#define __APUSYS_SECURE_H__

#include <mt-plat/mtk_secure_api.h>


enum MTK_APUSYS_KERNEL_OP {
	MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY = 0,
	MTK_APUSYS_KERNEL_OP_SET_AO_DBG_SEL,
	MTK_APUSYS_KERNEL_OP_NUM
};


#endif
