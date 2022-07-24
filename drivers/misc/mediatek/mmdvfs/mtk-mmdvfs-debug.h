/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef MTK_MMDVFS_DEBUG_H
#define MTK_MMDVFS_DEBUG_H

void mtk_mmdvfs_debug_release_step0(void);
void mtk_mmdvfs_debug_ulposc_enable(const bool enable);
bool mtk_is_mmdvfs_v3_debug_init_done(void);

#endif

