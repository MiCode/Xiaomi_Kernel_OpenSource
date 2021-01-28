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
