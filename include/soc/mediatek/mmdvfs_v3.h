/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MTK_MMDVFS_V3_H
#define MTK_MMDVFS_V3_H


enum {
	CCU_PWR_USR_MMDVFS,
	CCU_PWR_USR_IMG,
	CCU_PWR_USR_NUM
};

enum {
	VCP_PWR_USR_MMDVFS_INIT,
	VCP_PWR_USR_MMDVFS_GENPD,
	VCP_PWR_USR_MMDVFS_FORCE,
	VCP_PWR_USR_MMDVFS_VOTE,
	VCP_PWR_USR_MMDVFS_CCU,
	VCP_PWR_USR_MMQOS,
	VCP_PWR_USR_CAM,
	VCP_PWR_USR_IMG,
	VCP_PWR_USR_PDA,
	VCP_PWR_USR_SENIF,
	VCP_PWR_USR_VDEC,
	VCP_PWR_USR_VFMT,
	VCP_PWR_USR_SMI,
	VCP_PWR_USR_NUM
};

enum {
	VMM_USR_CAM,
	VMM_USR_IMG,
	VMM_USR_VDE = 1,
	VMM_USR_NUM
};

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
bool mtk_is_mmdvfs_init_done(void);
int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx);
int mtk_mmdvfs_enable_ccu(const bool enable, const u8 idx);

int mtk_mmdvfs_camera_notify_from_mmqos(const bool enable);
int mtk_mmdvfs_genpd_notify(const u8 idx, const bool enable, const bool genpd_update);
int mtk_mmdvfs_set_avs(const u8 idx, const u32 aging, const u32 fresh);

int mtk_mmdvfs_v3_set_force_step(const u16 pwr_idx, const s16 opp);
int mtk_mmdvfs_v3_set_vote_step(const u16 pwr_idx, const s16 opp);

void mmdvfs_set_lp_mode(bool lp_mode);
#else
static inline bool mtk_is_mmdvfs_init_done(void) { return false; }
static inline int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx) { return 0; }
static inline int mtk_mmdvfs_enable_ccu(const bool enable, const u8 idx) { return 0; }

static inline int mtk_mmdvfs_camera_notify_from_mmqos(const bool enable) { return 0; }
static inline
int mtk_mmdvfs_genpd_notify(const u8 idx, const bool enable, const bool genpd_update) { return 0; }
static inline int mtk_mmdvfs_set_avs(const u8 idx, const u32 aging, const u32 fresh) { return 0; }

static inline int mtk_mmdvfs_v3_set_force_step(const u16 pwr_idx, const s16 opp) { return 0; }
static inline int mtk_mmdvfs_v3_set_vote_step(const u16 pwr_idx, const s16 opp) { return 0; }

static inline void mmdvfs_set_lp_mode(bool lp_mode) { return; }
#endif

#endif /* MTK_MMDVFS_V3_H */
