/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_EXPORT_H__
#define __APUSYS_REVISER_EXPORT_H__
#include <linux/types.h>

extern struct reviser_dev_info *g_rdv;

int reviser_get_vlm(uint32_t request_size, bool force,
		unsigned long *ctx, uint32_t *tcm_size);
int reviser_free_vlm(uint32_t ctx);
int reviser_set_context(int type,
		int index, uint8_t ctx);

#endif
