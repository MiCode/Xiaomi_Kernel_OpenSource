/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_DVFS_H__
#define __ADSP_DVFS_H__

#include <adsp_ipi.h>
#include <adsp_clk.h>

#define ADSP_ITCM_MONITOR               (1)
#define ADSP_DTCM_MONITOR               (1)
#define ADSP_CFG_MONITOR                (0)
#define ADSP_DVFS_PROFILE               (1)
#define ADSP_FREQ_METER_ID              (43) //hf_fadsp_ck

#define ADSP_DVFS_USE_PLL               1

#define PLL_ENABLE                              (1)
#define PLL_DISABLE                             (0)

#define DVFS_STATUS_OK                  (0)
#define DVFS_STATUS_BUSY                (-1)
#define DVFS_REQUEST_SAME_CLOCK (-2)
#define DVFS_STATUS_ERR                 (-3)
#define DVFS_STATUS_TIMEOUT             (-4)
#define DVFS_CLK_ERROR                  (-5)
#define DVFS_STATUS_CMD_FIX             (-6)
#define DVFS_STATUS_CMD_LIMITED (-7)
#define DVFS_STATUS_CMD_DISABLE (-8)

enum adsp_cur_status_enum {
	ADSP_STATUS_RESET =   0x00,
	ADSP_STATUS_SUSPEND = 0x01,
	ADSP_STATUS_SLEEP =   0x10,
	ADSP_STATUS_ACTIVE =  0x11,
};

enum adsp_state_enum {
	IN_DEBUG_IDLE = 1,
	ENTERING_SLEEP = 2,
	IN_SLEEP = 4,
	ENTERING_ACTIVE = 8,
	IN_ACTIVE = 16,
};

enum {
	CLK_SYS_EN_BIT = 0,
	CLK_HIGH_EN_BIT = 1,
	CLK_HIGH_CG_BIT = 2,
	CLK_SYS_IRQ_EN_BIT = 16,
	CLK_HIGH_IRQ_EN_BIT = 17,
};
/*#ifdef CONFIG_PINCTRL_MT6797*/

enum clk_opp_enum {
	CLK_OPP0 = 125,
	CLK_OPP1 = 330,
	CLK_OPP2 = 416,
	CLK_INVALID_OPP,
};

enum clk_div_enum {
	CLK_DIV_1 = 0,
	CLK_DIV_2 = 1,
	CLK_DIV_4  = 2,
	CLK_DIV_8  = 3,
	CLK_DIV_UNKNOWN,
};

enum voltage_enum {
	SPM_VOLTAGE_800_D = 0,
	SPM_VOLTAGE_800,
	SPM_VOLTAGE_900,
	SPM_VOLTAGE_1000,
	SPM_VOLTAGE_TYPE_NUM,
};

struct mt_adsp_pll_t {
	/* main clock for mfg setting */
	struct clk *clk_mux;
	/* substitution clock for adsp transient parent setting */
	struct clk *clk_pll0;
	struct clk *clk_pll1;
	struct clk *clk_pll2;
	struct clk *clk_pll3;
	struct clk *clk_pll4;
	struct clk *clk_pll5;
	struct clk *clk_pll6;
	struct clk *clk_pll7;
};

extern int adsp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel);
extern int adsp_set_pmic_vcore(unsigned int cur_freq);
extern unsigned int adsp_get_dvfs_opp(void);
extern uint32_t adsp_get_freq(void);
extern int adsp_request_freq(void);
extern void adsp_pll_mux_set(unsigned int pll_ctrl_flag);
extern void wait_adsp_dvfs_init_done(void);
extern int __init adsp_dvfs_init(void);
extern void __exit adsp_dvfs_exit(void);

/* adsp dvfs variable*/
extern struct mutex adsp_feature_mutex;
extern struct mutex adsp_suspend_mutex;
extern struct completion adsp_suspend_cp;
extern struct completion adsp_resume_cp;
extern int adsp_is_suspend;

/* adsp new implement */
void adsp_A_send_spm_request(uint32_t enable);
extern void adsp_reset(void);
void adsp_sw_reset(enum adsp_core_id core_id);
extern void adsp_release_runstall(enum adsp_core_id, uint32_t release);
extern int adsp_suspend_init(void);
void adsp_start_suspend_timer(void);
void adsp_stop_suspend_timer(void);
int adsp_resume(void);
void adsp_suspend(enum adsp_core_id core_id);
/***************************/
#endif  /* __ADSP_DVFS_H__ */
