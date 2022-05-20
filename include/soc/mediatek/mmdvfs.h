/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MTK_MMDVFS_H
#define MTK_MMDVFS_H

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
int mtk_mmdvfs_set_camera_notify(bool enable);
bool mtk_is_mmdvfs_init_done(void);
#else
static inline
int mtk_mmdvfs_set_camera_notify(bool enable)
{ return 0; }

static inline
bool mtk_is_mmdvfs_init_done(void)
{ return false; }
#endif

#endif /* MTK_MMDVFS_H */
