/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MMDVFS_MGR_H__
#define __MMDVFS_MGR_H__

#include <aee.h>
#include <mt_smi.h>

#define MMDVFS_LOG_TAG	"MMDVFS"

#define MMDVFSMSG(string, args...) pr_debug("[pid=%d]"string, current->tgid, ##args)
#define MMDVFSMSG2(string, args...) pr_debug(string, ##args)
#define MMDVFSTMP(string, args...) pr_debug("[pid=%d]"string, current->tgid, ##args)
#define MMDVFSERR(string, args...) \
		do {\
			pr_debug("error: "string, ##args); \
			aee_kernel_warning(MMDVFS_LOG_TAG, "error: "string, ##args);  \
		} while (0)

#define _BIT_(_bit_) (unsigned)(1 << (_bit_))
#define _BITS_(_bits_, _val_) ((((unsigned) -1 >> (31 - ((1) ? _bits_))) \
				& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_) (((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_) (((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/* MMDVFS extern APIs */
extern void mmdvfs_init(MTK_SMI_BWC_MM_INFO *info);
extern void mmdvfs_handle_cmd(MTK_MMDVFS_CMD *cmd);
extern void mmdvfs_notify_scenario_enter(MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_exit(MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_concurrency(unsigned int u4Concurrency);
extern void mmdvfs_mhl_enable(int enable);
extern void mmdvfs_mjc_enable(int enable);

/* screen size */
extern unsigned int DISP_GetScreenWidth(void);
extern unsigned int DISP_GetScreenHeight(void);


#define MMSYS_CLK_LOW (0)
#define MMSYS_CLK_HIGH (1)
#define MMSYS_CLK_MEDIUM (2)

#define MMDVFS_EVENT_OVL_SINGLE_LAYER_ENTER 0
#define MMDVFS_EVENT_OVL_SINGLE_LAYER_EXIT 1
#define MMDVFS_EVENT_UI_IDLE_ENTER 2
#define MMDVFS_EVENT_UI_IDLE_EXIT 3

#define MMDVFS_CLIENT_ID_ISP 0

typedef int (*clk_switch_cb)(int ori_mmsys_clk_mode, int update_mmsys_clk_mode);
typedef int (*vdec_ctrl_cb)(void);

/* MMDVFS V2 only APIs */
extern int mmdvfs_notify_mmclk_switch_request(int event);
extern int mmdvfs_raise_mmsys_by_mux(void);
extern int mmdvfs_lower_mmsys_by_mux(void);
extern int register_mmclk_switch_cb(clk_switch_cb notify_cb,
clk_switch_cb notify_cb_nolock);
extern int mmdvfs_register_mmclk_switch_cb(clk_switch_cb notify_cb, int mmdvfs_client_id);
extern void dump_mmdvfs_info(void);


/* Extern from other module */
extern MTK_SMI_BWC_SCEN smi_get_current_profile(void);
extern int is_mmdvfs_freq_hopping_disabled(void);
extern int is_mmdvfs_freq_mux_disabled(void);
extern int is_force_max_mmsys_clk(void);
extern int is_force_camera_hpm(void);
extern int is_mmdvfs_disabled(void);
extern void mmdvfs_enable(int enable);
extern int mmdvfs_get_stable_isp_clk(void);


#ifdef MMDVFS_STANDALONE
#define vcorefs_request_dvfs_opp(scen, mode) do { \
	MMDVFSMSG("vcorefs_request_dvfs_opp"); \
	MMDVFSMSG("MMDVFS_STANDALONE mode enabled\n"); \
} while (0)

#define fliper_set_bw(BW_THRESHOLD_HIGH) do { \
	MMDVFSMSG("MMDVFS_STANDALONE mode enabled\n"); \
	MMDVFSMSG("fliper_set_bw");\
} while (0)

#define fliper_restore_bw() do {\
	MMDVFSMSG("MMDVFS_STANDALONE mode enabled\n"); \
	MMDVFSMSG("fliper_restore_bw(): fliper normal\n"); \
} while (0)

#endif /* MMDVFS_STANDALONE */

#ifdef MMDVFS_WQHD_1_0V
#include "disp_session.h"
extern int primary_display_switch_mode_for_mmdvfs(int sess_mode, unsigned int session, int blocking);
#endif

/* D2 plus only */
#if defined(SMI_D2)
extern void mt_set_vencpll_con1(int val);
extern int clkmux_sel(int id, unsigned int clksrc, char *name);
#endif

/* D1 plus implementation only */
extern u32 get_devinfo_with_index(u32 index);

#define MMDVFS_PROFILE_UNKNOWN (0)
#define MMDVFS_PROFILE_R1 (1)
#define MMDVFS_PROFILE_J1 (2)
#define MMDVFS_PROFILE_D1 (3)
#define MMDVFS_PROFILE_D1_PLUS (4)
#define MMDVFS_PROFILE_D2 (5)
#define MMDVFS_PROFILE_D2_M_PLUS (6)
#define MMDVFS_PROFILE_D2_P_PLUS (7)
#define MMDVFS_PROFILE_D3 (8)
#define MMDVFS_PROFILE_E1 (9)


enum {
	MMDVFS_CAM_MON_SCEN = SMI_BWC_SCEN_CNT, MMDVFS_SCEN_MHL, MMDVFS_SCEN_MJC, MMDVFS_SCEN_DISP,
	MMDVFS_SCEN_ISP,	MMDVFS_SCEN_VP_HIGH_RESOLUTION , MMDVFS_SCEN_COUNT
};

/* Backward compatible */
#define SMI_BWC_SCEN_120HZ MMDVFS_SCEN_DISP


#ifndef CONFIG_MTK_SMI_EXT
#define mmdvfs_set_step(scenario, step)
#else
int mmdvfs_set_step(MTK_SMI_BWC_SCEN scenario, mmdvfs_voltage_enum step);
#endif /* CONFIG_MTK_SMI_EXT */

extern int mmdvfs_get_mmdvfs_profile(void);
extern int is_mmdvfs_supported(void);
extern int mmdvfs_set_mmsys_clk(MTK_SMI_BWC_SCEN scenario, int mmsys_clk_mode);

#endif /* __MMDVFS_MGR_H__ */
