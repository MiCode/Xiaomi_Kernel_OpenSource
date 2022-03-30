/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2021 MediaTek Inc. */

#ifndef __IMGSENSOR_EXPORT_H__
#define __IMGSENSOR_EXPORT_H__

#include "adaptor-subdrv.h"

struct external_entry {
	struct subdrv_entry wrapper;
	struct subdrv_entry *target;
	struct list_head list;
};

int add_external_subdrv_entry(struct subdrv_entry *target);
struct external_entry *query_external_subdrv_entry(const char *name);

#endif
