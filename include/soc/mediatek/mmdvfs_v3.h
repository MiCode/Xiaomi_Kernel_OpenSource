/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MTK_MMDVFS_V3_H
#define MTK_MMDVFS_V3_H


enum ccu_pwr_usr {
	CCU_PWR_USR_MMDVFS_SET_RATE,
	CCU_PWR_USR_MMDVFS_CCU_TEST,
	CCU_PWR_USR_IMG,
	CCU_PWR_USR_NUM
};

enum vcp_pwr_usr {
	VCP_PWR_USR_VMM_CEIL,
	VCP_PWR_USR_MMDVFS_FORCE_STEP,
	VCP_PWR_USR_MMDVFS_VOTE_STEP,
	VCP_PWR_USR_MMDVFS_VMM_INIT,
	VCP_PWR_USR_MMDVFS_CCU_TEST,
	VCP_PWR_USR_MMDVFS_DUMP_SETTING,
	VCP_PWR_USR_MMDVFS_VCP_STRESS,
	VCP_PWR_USR_MMDVFS_GET_VCP_LOG,
	VCP_PWR_USR_MMDVFS_SET_VCP_LOG,
	VCP_PWR_USR_MMDVFS_VCP_INIT,
	VCP_PWR_USR_MMDVFS_CAM_NOTIFY,
	VCP_PWR_USR_MMDVFS_VDEC_NOTIFY,
	VCP_PWR_USR_MMQOS_HRT,
	VCP_PWR_USR_VFMT,
	VCP_PWR_USR_IMG,
	VCP_PWR_USR_CAM,
	VCP_PWR_USR_SENIF,
	VCP_PWR_USR_PDA,
	VCP_PWR_USR_VDEC,
	VCP_PWR_USR_NUM
};

enum avs_usr {
	AVS_USR_CAM,
	AVS_USR_IMG,
	AVS_USR_NUM
};

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
void *mtk_mmdvfs_vcp_get_base(phys_addr_t *pa);
int mtk_mmdvfs_camera_notify(bool genpd_update, bool enable);
int mtk_mmdvfs_camera_notify_from_mmqos(bool enable);
int mtk_mmdvfs_vdec_notify(bool enable);
int mtk_mmdvfs_set_avs(u16 usr_id, u32 aging_cnt, u32 fresh_cnt);
bool mtk_is_mmdvfs_init_done(void);
int mtk_mmdvfs_enable_vcp(bool enable, unsigned int usr_id);
int mtk_mmdvfs_enable_ccu(bool enable, unsigned int usr_id);
int mtk_mmdvfs_v3_set_force_step(u16 pwr_idx, s16 opp);
int mtk_mmdvfs_v3_set_vote_step(u16 pwr_idx, s16 opp);
void mmdvfs_set_lp_mode(bool lp_mode);
#else
static inline
void *mtk_mmdvfs_vcp_get_base(phys_addr_t *pa)
{
	*pa = 0;
	return NULL;
}

static inline
int mtk_mmdvfs_camera_notify_from_mmqos(bool enable)
{ return 0; }

static inline
int mtk_mmdvfs_camera_notify(bool genpd_update, bool enable)
{ return 0; }

static inline
int mtk_mmdvfs_vdec_notify(bool enable)
{ return 0; }

static inline
int mtk_mmdvfs_set_avs(u16 usr_id, u32 aging_cnt, u32 fresh_cnt)
{ return 0; }

static inline
bool mtk_is_mmdvfs_init_done(void)
{ return false; }

int mtk_mmdvfs_enable_vcp(bool enable, unsigned int usr_id)
{ return 0; }

int mtk_mmdvfs_enable_ccu(bool enable, unsigned int usr_id)
{ return 0; }

static inline
int mtk_mmdvfs_v3_set_force_step(u16 pwr_idx, s16 opp)
{ return 0; }

static inline
int mtk_mmdvfs_v3_set_vote_step(u16 pwr_idx, s16 opp)
{ return 0; }

static inline
void mmdvfs_set_lp_mode(bool lp_mode)
{ return; };
#endif

#endif /* MTK_MMDVFS_V3_H */
