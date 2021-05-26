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

#ifndef __APUSYS_REVISER_SECURE_H__
#define __APUSYS_REVISER_SECURE_H__

#include <mt-plat/mtk_secure_api.h>

#define APUSYS_SECURE 1
#define BOUNDARY_ALL_NO_CHANGE (0xFFFFFFFF)
#define BOUNDARY_BIT_MASK (0x0F)

#if APUSYS_SECURE
#define APUSYS_ATTR_USE __attribute__((unused))
#else
#define APUSYS_ATTR_USE
#endif

enum MTK_APUSYS_KERNEL_OP {
	MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY = 0,
	MTK_APUSYS_KERNEL_OP_SET_AO_DBG_SEL,
	MTK_APUSYS_KERNEL_OP_REVISER_CHK_VALUE,
	MTK_APUSYS_KERNEL_OP_REVISER_SET_DEFAULT_IOVA,
	MTK_APUSYS_KERNEL_OP_REVISER_GET_INTERRUPT_STATUS,
	MTK_APUSYS_KERNEL_OP_REVISER_SET_CONTEXT_ID,
	MTK_APUSYS_KERNEL_OP_REVISER_SET_REMAP_TABLE,
	MTK_APUSYS_KERNEL_OP_NUM
};


#endif
