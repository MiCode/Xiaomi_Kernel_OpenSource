/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DEFS_H_
#define _SPRD_DEFS_H_

#include <linux/sprd_iommu.h>
#include <linux/string.h>
#include <linux/slab.h>

#define SPRD_DECLARE_HANDLE(module_hdl) \
	typedef struct{int dummy; } module_hdl##__;\
	typedef module_hdl##__ * module_hdl

/* General driver error definition */
enum {
	SPRD_NO_ERR = 0x100,
	SPRD_ERR_INVALID_PARAM,
	SPRD_ERR_INITIALIZED,
	SPRD_ERR_INVALID_HDL,
	SPRD_ERR_STATUS,
	SPRD_ERR_RESOURCE_BUSY,
	SPRD_ERR_ILLEGAL_PARAM,
	SPRD_ERR_MAX,
};

#endif  /*END OF : define  _SPRD_DEFS_H_ */
