/*
 * mtk-afe-debug.h  --  Mediatek audio debug function
 *
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __MT_AFE_DEBUG_H__
#define __MT_AFE_DEBUG_H__

struct mtk_afe;


void mtk_afe_init_debugfs(struct mtk_afe *afe);

void mtk_afe_cleanup_debugfs(struct mtk_afe *afe);

#endif
