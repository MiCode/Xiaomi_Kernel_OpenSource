/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MTK_IDLE_INTERNAL_H__
#define __MTK_IDLE_INTERNAL_H__

#include "mtk_lp_dts_def.h"

/********************************************************************
 * Change idle condition check order (1:SO3,DP,SO, 0:DP,SO3,SO)
 *******************************************************************/
#define MTK_IDLE_ADJUST_CHECK_ORDER     (1)


/********************************************************************
 * Enable/Disable all idle related trace_tag
 *******************************************************************/
#define MTK_IDLE_TRACE_TAG_ENABLE       (1)


/********************************************************************
 * mtk idle related definitions
 *******************************************************************/
#define MTK_IDLE_OPT_VCORE_LP_MODE  (1 << 0)
#define MTK_IDLE_OPT_XO_UFS_ON_OFF  (1 << 1)
#define MTK_IDLE_OPT_CLKBUF_BBLPM   (1 << 2)
#define MTK_IDLE_OPT_VCORE_ULPOSC_OFF  (1 << 6)
#define MTK_IDLE_OPT_SLEEP_DPIDLE   (1 << 16)


/********************************************************************
 * mtk idle log flag
 *******************************************************************/
#define MTK_IDLE_LOG_REDUCE         (1 << 0)
#define MTK_IDLE_LOG_RESOURCE_USAGE (1 << 1)
#define MTK_IDLE_LOG_DISABLE        (1 << 2)
#define MTK_IDLE_LOG_DUMP_LP_GS     (1 << 4)


/* CPUIDLE_STATE is used to represent CPUidle C States */
enum {
	CPUIDLE_STATE_RG = 0,
	CPUIDLE_STATE_SO,
	CPUIDLE_STATE_DP,
	CPUIDLE_STATE_SO3,
	NR_CPUIDLE_STATE
};

/* Reason why cpu can enter dp/so/so3 */
enum {
	BY_FRM = 0, /* SPM FW is not loaded */
	BY_SRR,		/* Blocked by SPM Resource Request */
	BY_UFS,		/* Blocked by UFS */
	BY_TEE,		/* TEEI not ready */
	BY_DCS,		/* Blocked by DCS */
	BY_CLK,		/* CG check fail */
	BY_PWM,		/* Display pwm not ready (SODI3) */
	BY_PLL,		/* PLL not off (SODI3) */
	NR_REASONS,
};

enum _MTK_IDLE_PLAT_STATUS_ {
	MTK_IDLE_PLAT_BOOT_BLOCKED,
	MTK_IDLE_PLAT_READY
};


/*mtk idle initial data*/
struct mtk_idle_init_data {
	int dts_state;
	int dts_value;
};
#define IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, _f)\
			(p->dts_state & (1<<_f))
#define GET_MTK_LP_DTS_VALUE(p, _f)\
	({int ret = 0;\
		ret = (p->dts_value & (1<<_f))?1:0;\
	ret; })

/*Check the mtk idle node which in dts exist or not*/
#define IS_MTK_LP_DTS_AVAILABLE_SODI(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_SODI)
#define IS_MTK_LP_DTS_AVAILABLE_SODI3(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_SODI3)
#define IS_MTK_LP_DTS_AVAILABLE_DP(p)\
	IS_MTK_LP_DTS_FEATURE_AVAILABLE(p, MTK_LP_FEATURE_DTS_DP)

/*Get the mtk idle dts node status value*/
#define GET_MTK_LP_DTS_VALUE_SODI(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_SODI)
#define GET_MTK_LP_DTS_VALUE_SODI3(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_SODI3)
#define GET_MTK_LP_DTS_VALUE_DP(p)\
	GET_MTK_LP_DTS_VALUE(p, MTK_LP_FEATURE_DTS_DP)

/* Check the dts status result and set it to mtk_idle_init_data*/
#define MTK_IDLE_FEATURE_DTS_STATE_CHECK(_f, _s, _d) {\
	if (_s & MTK_OF_PROPERTY_STATUS_FOUND) {\
		_d.dts_state |= (1<<_f);\
		if (_s & MTK_OF_PROPERTY_VALUE_ENABLE)\
			_d.dts_value |= (1<<_f);\
		else\
			_d.dts_value &= ~(1<<_f);\
	} else {\
		_d.dts_state &= ~(1<<_f);\
		_d.dts_value &= ~(1<<_f); } }

const char*
	mtk_idle_block_reason_name(int reason);

/********************************************************************
 * mtk_dpidle.c / mtk_sodi3.c / mtk_sodi.c
 *******************************************************************/

#include <linux/debugfs.h>	/* struct dentry */

extern unsigned long dp_cnt[NR_CPUS];
extern unsigned long so_cnt[NR_CPUS];
extern unsigned long so3_cnt[NR_CPUS];
extern bool mtk_idle_screen_off_sodi3;

void mtk_dpidle_init(struct mtk_idle_init_data *pData);
void mtk_dpidle_disable(void);
bool mtk_dpidle_enabled(void);
bool dpidle_can_enter(int reason);

void mtk_sodi3_init(struct mtk_idle_init_data *pData);
void mtk_sodi3_disable(void);
bool mtk_sodi3_enabled(void);
bool sodi3_can_enter(int reason);

void mtk_sodi_init(struct mtk_idle_init_data *pData);
void mtk_sodi_disable(void);
bool mtk_sodi_enabled(void);
bool sodi_can_enter(int reason);

#define MTK_SYSFS_IDLE        "/sys/kernel/mtk_lpm/cpuidle/idle_state"
#define MTK_SYSFS_DPIDLE      "/sys/kernel/mtk_lpm/cpuidle/dpidle_state"
#define MTK_SYSFS_SODI3       "/sys/kernel/mtk_lpm/cpuidle/soidle3_state"
#define MTK_SYSFS_SODI        "/sys/kernel/mtk_lpm/cpuidle/soidle_state"
#define MTK_SYSFS_SUSPEND     "/sys/kernel/mtk_lpm/cpuidle/slp/suspend_state"
#define MTK_SYSFS_RESOURCE    "/sys/kernel/mtk_lpm/cpuidle/spm_resource_req"
/********************************************************************
 * mtk_idle_internal.c
 *******************************************************************/

int mtk_idle_enter(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag);


/********************************************************************
 * mtk_spm_resource_req.c
 *******************************************************************/

unsigned int spm_get_resource_usage(void);
unsigned int spm_get_resource_usage_by_user(unsigned int user);


/********************************************************************
 * mtk_idle_profile.c
 *******************************************************************/

/* idle ratio for internal use */
bool mtk_idle_get_ratio_status(void);
void mtk_idle_ratio_calc_start(int type, int cpu);
void mtk_idle_ratio_calc_stop(int type, int cpu);
void mtk_idle_disable_ratio_calc(void);
void mtk_idle_enable_ratio_calc(void);
void mtk_idle_dump_cnt_in_interval(void);

bool mtk_idle_select_state(int type, int reason);
void mtk_idle_block_setting(
	int type, unsigned long *cnt, unsigned long *block_cnt);

/* Latency profile for idle scenario */
enum {
	PIDX_SELECT_TO_ENTER,
	PIDX_ENTER_TOTAL,
	PIDX_LEAVE_TOTAL,
	PIDX_PRE_HANDLER,
	PIDX_PWR_PRE_WFI,
	PIDX_SPM_PRE_WFI,
	PIDX_PWR_PRE_WFI_WAIT,
	PIDX_PWR_POST_WFI,
	PIDX_SPM_POST_WFI,
	PIDX_POST_HANDLER,
	PIDX_PWR_POST_WFI_WAIT,
	NR_PIDX
};

void mtk_idle_latency_profile_enable(bool enable);
bool mtk_idle_latency_profile_is_on(void);
void mtk_idle_latency_profile(unsigned int idle_type, int idx);
void mtk_idle_latency_profile_result(unsigned int idle_type);

#define __profile_idle_start(idle_type, idx) \
	mtk_idle_latency_profile(idle_type, 2*idx)

#define __profile_idle_stop(idle_type, idx) \
	mtk_idle_latency_profile(idle_type, 2*idx+1)


/********************************************************************
 * mtk_idle_twam.c
 *******************************************************************/

struct mtk_idle_twam {
	u32 sel;
	u32 event;
	bool speed_mode;
	bool running;
};

struct mtk_idle_twam *mtk_idle_get_twam(void);

void mtk_idle_twam_enable(unsigned int event);

void mtk_idle_twam_disable(void);

#ifdef CONFIG_MTK_AEE_IPANIC
extern void aee_rr_rec_spm_suspend_val(u32 val);
extern void aee_rr_rec_deepidle_val(u32 val);
extern void aee_rr_rec_sodi3_val(u32 val);
extern void aee_rr_rec_sodi_val(u32 val);
extern u32 aee_rr_curr_spm_suspend_val(void);
extern u32 aee_rr_curr_deepidle_val(void);
extern u32 aee_rr_curr_sodi3_val(void);
extern u32 aee_rr_curr_sodi_val(void);
#endif /* CONFIG_MTK_AEE_IPANIC */

/* definition of spm resource request functions */
extern void spm_resource_req_block_dump(void);
extern void spm_resource_req_dump(void);
extern void dvfsrc_md_scenario_update(bool suspend);

/* call uart to sleep and wakeup*/
extern int mtk8250_request_to_sleep(void);
extern int mtk8250_request_to_wakeup(void);

#endif /* __MTK_IDLE_INTERNAL_H__ */
