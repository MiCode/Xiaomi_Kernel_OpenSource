/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SCP_DVFS_H__
#define __SCP_DVFS_H__

#define PLL_ENABLE				(1)
#define PLL_DISABLE				(0)

#define DVFS_STATUS_OK			(0)
#define DVFS_STATUS_BUSY		(-1)
#define DVFS_REQUEST_SAME_CLOCK	(-2)
#define DVFS_STATUS_ERR			(-3)
#define DVFS_STATUS_TIMEOUT		(-4)
#define DVFS_CLK_ERROR			(-5)
#define DVFS_STATUS_CMD_FIX		(-6)
#define DVFS_STATUS_CMD_LIMITED	(-7)
#define DVFS_STATUS_CMD_DISABLE	(-8)

#define CLK_26M					(26)
#define MAINPLL_273M			(273)
#define UNIVPLL_416M			(416)

enum scp_state_enum {
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

#if (defined(CONFIG_MACH_MT6781) || defined(CONFIG_MACH_MT6785))
#include <linux/arm-smccc.h>
enum scp_req_r {
	SCP_REQ_RELEASE = 0,
	SCP_REQ_26M = 1 << 0,
	SCP_REQ_IFR = 1 << 1,
	SCP_REQ_SYSPLL1 = 1 << 2,
	SCP_REQ_MAX	= 1 << 3,
};
extern int scp_resource_req(unsigned int req_type);


#endif

enum clk_opp_enum {
   #if defined(CONFIG_MACH_MT6781)
   CLK_OPP0 = 125,
   CLK_OPP1 = 250,
   CLK_OPP2 = 273,
   CLK_OPP3 = 330,
   CLK_OPP4 = 416,
   CLK_MAX_OPP = CLK_OPP4,
   CLK_MAINPLL = CLK_OPP2,
   CLK_UNIVPLL = CLK_OPP4,
 #else
   CLK_OPP0 = 110,
   CLK_OPP1 = 130,
   CLK_OPP2 = 165,
   CLK_OPP3 = 218,
   CLK_OPP4 = 330,
   CLK_OPP5 = 416,
   CLK_MAX_OPP = CLK_OPP5,
   CLK_MAINPLL = CLK_OPP3,
   CLK_UNIVPLL = CLK_OPP5,
 #endif
   CLK_UNINIT = 0xffff,
 };





enum clk_div_enum {
	CLK_DIV_1 = 0,
	CLK_DIV_2 = 1,
	CLK_DIV_4 = 2,
	CLK_DIV_3 = 3,
};

enum subsys_enum {
	SYS_GPIO = 0,
	SYS_PMIC,
	SYS_NUM,
};

enum sub_feature_enum {
	GPIO_MODE = 0,
	PMIC_VOW_LP,
	PMIC_PMRC,
	SUB_FEATURE_NUM,
};

enum scp_dvfs_smc_cmd {
	SCP_DVFS_SMC_RESOURCE_REQ = 1,
	SCP_DVFS_SMC_RESOURCE_REL,
	SCP_DVFS_SMC_WRITE_SPM,
	SCP_DVFS_SMC_READ_SPM,
};

enum scp_request_resources {
	SCP_REQ_RESOURCE_26M = (1 << 1L),
	SCP_REQ_RESOURCE_INFRA = (1 << 2L),
	SCP_REQ_RESOURCE_SYSPLL = (1 << 3L),
	SCP_REQ_RESOURCE_DRAM = (1 << 4L),
	SCP_REQ_RESOURCE_ALL = (0xFFFFFFFF),
};

struct mt_scp_pll_t {
	struct clk *clk_mux;
	struct clk *clk_pll[8];
	unsigned int pll_num;
};

struct reg_info {
	unsigned int ofs;
	unsigned int msk;
	unsigned int bit;
	unsigned int setclr;
};

struct reg_cfg {
	unsigned int on;
	unsigned int off;
};

struct sub_feature_data {
	const char *name;
	struct reg_info *reg;
	struct reg_cfg *cfg;
	unsigned int onoff;
	int num;
};

struct subsys_data {
	struct regmap *regmap;
	struct sub_feature_data *fd;
	int num;
};

struct dvfs_opp {
	unsigned int vcore;
	unsigned int vsram;
	unsigned int uv_idx;
	unsigned int dvfsrc_opp;
	unsigned int spm_opp;
	unsigned int freq;
	unsigned int clk_mux;
};

struct dvfs_data {
	struct dvfs_opp *opp;
	int scp_opp_num;
	int dvfsrc_opp_num;
};

extern int scp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel);
extern int scp_set_pmic_vcore(unsigned int cur_freq);
extern unsigned int scp_get_dvfs_opp(void);
extern uint32_t scp_get_freq(void);
extern int scp_request_freq(void);
extern void scp_pll_mux_set(unsigned int pll_ctrl_flag);
extern void wait_scp_dvfs_init_done(void);
extern int __init scp_dvfs_init(void);
extern void __exit scp_dvfs_exit(void);

/* scp dvfs variable*/
extern unsigned int scp_expected_freq;
extern unsigned int scp_current_freq;
extern spinlock_t scp_awake_spinlock;

#endif  /* __SCP_DVFS_H__ */
