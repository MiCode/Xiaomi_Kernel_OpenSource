/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SCP_DVFS_H__
#define __SCP_DVFS_H__

#define PLL_ENABLE			(1)
#define PLL_DISABLE			(0)

#define CLK_26M				(26)

#define GPIO_ELEM_CNT			(3)
#define CALI_CONFIG_ELEM_CNT		(3)
#define OPP_ELEM_CNT			(7)

#define REG_MAX_MASK			(0xFFFFFFFF)
#define SCP_ULPOSC_SEL_CORE		(0x4)
#define SCP_ULPOSC_SEL_PERI		(0x8)

#define CAL_EXT_BITS		(2)
#define CAL_MIN_VAL_EXT		(0)
#define CAL_MAX_VAL_EXT		(0x2)
#define CAL_BITS			(7)
#define CAL_MIN_VAL			(0)
#define CAL_MAX_VAL			(0x7F)
#define CALI_MIS_RATE			(40)
#define CALI_DIV_VAL			(512)

#define MAX_SUPPORTED_PLL_NUM 9

#define REG_DEFINE_WITH_INIT(reg, offset, mask, shift, init, set_clr)	\
	._##reg = {							\
		.ofs = offset,						\
		.msk = mask,						\
		.bit = shift,						\
		.setclr = set_clr,					\
		.init_config = init					\
	},

#define REG_DEFINE(reg, ofs, msk, bit)					\
	REG_DEFINE_WITH_INIT(reg, ofs, msk, bit, 0, 0)

enum scp_core_enum {
	SCP_CORE_0,
	SCP_CORE_1,
	SCP_MAX_CORE_NUM,
};

enum scp_dvfs_err_enum {
	ESCP_REG_NOT_SUPPORTED = 1000,
	ESCP_DVFS_OPP_OUT_OF_BOUND,
	ESCP_DVFS_PMIC_REGULATOR_FAILED,
	ESCP_DVFS_IPI_FAILED,
	ESCP_DVFS_CALI_FAILED,
	ESCP_DVFS_GPIO_CONFIG_FAILED,
	ESCP_DVFS_NO_CALI_CONFIG_FOUND,
	ESCP_DVFS_NO_CALI_HW_FOUND,
	ESCP_DVFS_DATA_RE_INIT,
	ESCP_DVFS_NO_PMIC_REG_FOUND,
	ESCP_DVFS_REGMAP_INIT_FAILED,
	ESCP_DVFS_INIT_FAILED,
	ESCP_DVFS_DBG_INVALID_CMD,
	ESCP_DVFS_DVS_SHOULD_BE_BYPASSED,
};

enum scp_cmd_type {
	VCORE_ACQUIRE,
	RESOURCE_REQ,
	ULPOSC2_TURN_ON,
	ULPOSC2_TURN_OFF,
};

enum scp_req_r {
	SCP_REQ_RELEASE = 0,
	SCP_REQ_26M = 1 << 0,
	SCP_REQ_INFRA = 1 << 1,
	SCP_REQ_SYSPLL = 1 << 2,
	SCP_REQ_MAX = 1 << 3,
};

enum scp_state_enum {
	IN_DEBUG_IDLE = 1 << 0,
	ENTERING_SLEEP = 1 << 1,
	IN_SLEEP = 1 << 2,
	ENTERING_ACTIVE = 1 << 3,
	IN_ACTIVE = 1 << 4,
};

enum scp_sleep_config {
	R_CPU_OFF = 1 << 1, /* cpu-off config: 1:enable, 0: disable */
};

enum scp_power_status {
	POW_ON = 1 << 1, /* 1: cpu-on, 0: cpu-off */
};

enum scp_ipi_cmd {
	SCP_SLEEP_OFF,
	SCP_SLEEP_ON,
	SCP_SLEEP_NO_WAKEUP,
	SCP_SLEEP_NO_CONDITION,
	SCP_SLEEP_GET_DBG_FLAG,
	SCP_SLEEP_GET_COUNT,
	SCP_SLEEP_RESET,
	SCP_SYNC_ULPOSC_CALI,
	SCP_SLEEP_BLOCK_BY_TIMER_CNI,
	SCP_SLEEP_BLOCK_BY_COMPILER_CNT,
	SCP_SLEEP_BLOCK_BY_SEMAPHORE_CNT,
	SCP_SLEEP_BLOCK_BY_WAKELOCK_CNT,
	SCP_SLEEP_BLOCK_BY_IPI_BUSY_CNT,
	SCP_SLEEP_BLOCK_BY_PENDING_IRQ_CNT,
	SCP_SLEEP_BLOCK_BY_SLP_DISABLED_CNT,
	SCP_SLEEP_BLOCK_BY_SLP_BUSY_CNT,
	SCP_SLEEP_BLOCK_BY_HARD1_BUSY_CNT,
	SCP_SLEEP_CMD_MAX,
};

enum ulposc_ver_enum {
	ULPOSC_VER_1, /* APMIXED_SYS */
	ULPOSC_VER_2, /* VLP_CKSYS */
	MAX_ULPOSC_VERSION,
};

enum scp_dvfs_chip_hw_enum {
	MT6853,
	MT6873,
	MT6893,
	MT6833,
	MAX_SCP_DVFS_CHIP_HW,
};

enum clk_dbg_ver_enum {
	CLK_DBG_VER_1,
	CLK_DBG_VER_2,
	MAX_CLK_DBG_VERSION,
};

enum scp_clk_ver_enum {
	SCP_CLK_VER_1,
	MAX_SCP_CLK_VERSION,
};

enum ulposc_onoff_enum {
	ULPOSC_OFF,
	ULPOSC_ON,
};

struct mt_scp_pll_t {
	struct clk *clk_mux;
	struct clk *clk_pll[MAX_SUPPORTED_PLL_NUM];
	unsigned int pll_num;
};

struct reg_info {
	unsigned int ofs;
	unsigned int msk;
	unsigned int bit;
	unsigned int setclr;
	unsigned int init_config;
};

struct ulposc_cali_regs {
	struct reg_info _con0;
	struct reg_info _cali_ext;	/* turning factor 1, maybe unused,  */
	struct reg_info _cali;		/* turning factor 2 */
	struct reg_info _con1;
	struct reg_info _con2;
};

struct ulposc_cali_config {
	unsigned int con0_val;
	unsigned int con1_val;
	unsigned int con2_val;
};

struct clk_cali_regs {
	struct reg_info _clk_misc_cfg0;
	struct reg_info _meter_div;

	struct reg_info _clk_dbg_cfg;
	struct reg_info _fmeter_ck_sel;
	struct reg_info _abist_clk;

	struct reg_info _clk26cali_0;
	struct reg_info _fmeter_rst; /* using carefully, if set to 0, fmeter will reset */
	struct reg_info _fmeter_en;
	struct reg_info _trigger_cal;

	struct reg_info _clk26cali_1;
	struct reg_info _cal_cnt;
	struct reg_info _load_cnt;
};

struct ulposc_cali_hw {
	struct regmap *fmeter_regmap;
	struct regmap *ulposc_regmap;
	struct ulposc_cali_regs *ulposc_regs;
	struct ulposc_cali_config *cali_configs;
	struct clk_cali_regs *clkdbg_regs;
	unsigned int cali_nums;
	unsigned short *cali_val_ext;
	unsigned short *cali_val;
	unsigned short *cali_freq;
	bool do_ulposc_cali;
	bool cali_failed;
};

struct scp_clk_hw {
	struct regmap *scp_clk_regmap;
	struct reg_info _clk_high_en;
	struct reg_info _ulposc2_en;
	struct reg_info _ulposc2_cg;
	struct reg_info _sel_clk;
};

struct scp_pmic_regs {
	struct reg_info _sshub_op_mode;
	struct reg_info _sshub_op_en;
	struct reg_info _sshub_op_cfg;
	struct reg_info _sshub_buck_en;
	struct reg_info _sshub_ldo_en;
	struct reg_info _pmrc_en;
};

struct dvfs_opp {
	unsigned int vcore;
	unsigned int vsram;
	unsigned int tuned_vcore;
	unsigned int dvfsrc_opp;
	unsigned int spm_opp;
	unsigned int freq;
	unsigned int clk_mux;
	unsigned int resource_req;
};

struct scp_dvfs_hw {
	struct dvfs_opp *opp;
	struct regmap *pmic_regmap;
	struct regmap *gpio_regmap;
	struct scp_pmic_regs *pmic_regs;
	struct ulposc_cali_hw ulposc_hw;
	struct scp_clk_hw *clk_hw;
	bool ccf_fmeter_support; /* Has CCF provided fmeter api to use? */
	int ccf_fmeter_id;
	int ccf_fmeter_type;
	bool vlpck_support; /* Using 2-phase calibration if vlpck_bypass_phase1 not set */
	bool vlpck_bypass_phase1;
	bool vlp_support; /* Moving regulator & PMIC setting into SCP side */
	bool pmic_sshub_en;
	bool sleep_init_done;
	bool pre_mux_en;
	u32 scp_opp_nums;
	int vow_lp_en_gear;
	int cur_dbg_core;
	u32 core_nums;
	unsigned int secure_access_scp;
	bool bypass_pmic_rg_access;
};

extern int scp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel);
extern int scp_request_freq(void);
extern int scp_resource_req(unsigned int req);
extern uint32_t scp_get_freq(void);
extern void scp_init_vcore_request(void);
extern void wait_scp_dvfs_init_done(void);
extern bool sync_ulposc_cali_data_to_scp(void);
extern int __init scp_dvfs_init(void);
extern void scp_dvfs_exit(void);
extern int scp_dvfs_feature_enable(void);

/* scp dvfs variable*/
extern unsigned int last_scp_expected_freq;
extern unsigned int scp_expected_freq;
extern unsigned int scp_current_freq;
extern spinlock_t scp_awake_spinlock;

#endif  /* __SCP_DVFS_H__ */
