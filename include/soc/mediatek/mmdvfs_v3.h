/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MTK_MMDVFS_V3_H
#define MTK_MMDVFS_V3_H

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
void *mtk_mmdvfs_vcp_get_base(void);
int mtk_mmdvfs_camera_notify(bool enable);
int mtk_mmdvfs_camera_notify_from_mmqos(bool enable);
bool mtk_is_mmdvfs_init_done(void);
int mtk_mmdvfs_enable_vcp(bool enable);
int mtk_mmdvfs_enable_ccu(bool enable);
int mtk_mmdvfs_v3_set_force_step(u16 pwr_idx, s16 opp);
int mtk_mmdvfs_v3_set_vote_step(u16 pwr_idx, s16 opp);
#else
static inline
void *mtk_mmdvfs_vcp_get_base(void)
{ return NULL; }

static inline
int mtk_mmdvfs_camera_notify_from_mmqos(bool enable)
{ return 0; }

static inline
int mtk_mmdvfs_camera_notify(bool enable)
{ return 0; }

static inline
bool mtk_is_mmdvfs_init_done(void)
{ return false; }

int mtk_mmdvfs_enable_vcp(bool enable)
{ return 0; }

int mtk_mmdvfs_enable_ccu(bool enable)
{ return 0; }

static inline
int mtk_mmdvfs_v3_set_force_step(u16 pwr_idx, s16 opp)
{ return 0; }

static inline
int mtk_mmdvfs_v3_set_vote_step(u16 pwr_idx, s16 opp)
{ return 0; }
#endif

#endif /* MTK_MMDVFS_V3_H */
