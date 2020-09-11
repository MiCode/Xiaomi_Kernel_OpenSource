/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MT_DEVINFO_H__
#define __MT_DEVINFO_H__

#include <linux/types.h>

/*****************************************************************************
 * HRID RET CODE DEFINITION
 *****************************************************************************/
#define E_SUCCESS                 0x00000000
#define E_BUF_NOT_ENOUGH          0x10000000
#define E_BUF_ZERO_OR_NULL        0x20000000
#define E_BUF_SIZE_ZERO_OR_NULL   0x40000000

/*****************************************************************************
 * DEVINFO AND HRID APIS
 *****************************************************************************/
extern u32 get_devinfo_with_index(unsigned int index);
extern u32 devinfo_ready(void);
extern u32 devinfo_get_size(void);
extern u32 get_hrid_size(void);
extern u32 get_hrid(unsigned char *rid, unsigned char *rid_sz);

#endif /* __MT_DEVINFO_H__ */

