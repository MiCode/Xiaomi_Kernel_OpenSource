// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_REVISER_EXPORT_H__
#define __APUSYS_REVISER_EXPORT_H__
#include <linux/types.h>


extern int reviser_get_vlm(uint32_t request_size, bool force,
		unsigned long *id, uint32_t *tcm_size);
extern int reviser_free_vlm(uint32_t ctxid);
extern int reviser_set_context(int type,
		int index, uint8_t ctxid);
extern int reviser_get_resource_vlm(uint32_t *addr, uint32_t *size);
#endif
