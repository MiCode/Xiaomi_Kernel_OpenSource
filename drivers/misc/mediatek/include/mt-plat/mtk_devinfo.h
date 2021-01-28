/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 */

#ifndef __MT_DEVINFO_H__
#define __MT_DEVINFO_H__

//#warning "mtk_devinfo is deprecated, please use nvmem driver instead"

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

