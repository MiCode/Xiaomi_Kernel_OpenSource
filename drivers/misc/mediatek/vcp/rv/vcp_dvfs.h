/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_DVFS_H__
#define __VCP_DVFS_H__

#define PLL_ENABLE			(1)
#define PLL_DISABLE			(0)

#define CLK_26M				(26)

#define GPIO_ELEM_CNT			(3)
#define CALI_CONFIG_ELEM_CNT		(3)
#define OPP_ELEM_CNT			(7)

#define REG_MAX_MASK			(0xFFFFFFFF)
#define VCP_ULPOSC_SEL_CORE		(0x4)
#define VCP_ULPOSC_SEL_PERI		(0x8)

#define CAL_MIN_VAL			(0)
#define CAL_MAX_VAL			(0x7F)
#define CALI_MIS_RATE			(40)
#define CALI_DIV_VAL			(512)

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

enum vcp_core_enum {
	VCP_CORE_0,
	VCP_CORE_1,
	VCP_MAX_CORE_NUM,
};

enum vcp_dvfs_err_enum {
	EVCP_REG_NOT_SUPPORTED = 1000,
	EVCP_DVFS_OPP_OUT_OF_BOUND,
	EVCP_DVFS_PMIC_REGULATOR_FAILED,
	EVCP_DVFS_IPI_FAILED,
	EVCP_DVFS_CALI_FAILED,
	EVCP_DVFS_GPIO_CONFIG_FAILED,
	EVCP_DVFS_NO_CALI_CONFIG_FOUND,
	EVCP_DVFS_NO_CALI_HW_FOUND,
	EVCP_DVFS_DATA_RE_INIT,
	EVCP_DVFS_NO_PMIC_REG_FOUND,
	EVCP_DVFS_REGMAP_INIT_FAILED,
	EVCP_DVFS_INIT_FAILED,
	EVCP_DVFS_DBG_INVALID_CMD,
};

enum vcp_cmd_type {
	VCORE_ACQUIRE,
	RESOURCE_REQ,
};

enum vcp_req_r {
	VCP_REQ_RELEASE = 0,
	VCP_REQ_26M = 1 << 0,
	VCP_REQ_INFRA = 1 << 1,
	VCP_REQ_SYSPLL = 1 << 2,
	VCP_REQ_MAX = 1 << 3,
};

enum vcp_state_enum {
	IN_DEBUG_IDLE = 1 << 0,
	ENTERING_SLEEP = 1 << 1,
	IN_SLEEP = 1 << 2,
	ENTERING_ACTIVE = 1 << 3,
	IN_ACTIVE = 1 << 4,
};

enum vcp_ipi_cmd {
	VCP_SLEEP_OFF,
	VCP_SLEEP_ON,
	VCP_SLEEP_NO_WAKEUP,
	VCP_SLEEP_NO_CONDITION,
	VCP_SLEEP_GET_DBG_FLAG,
	VCP_SLEEP_GET_COUNT,
	VCP_SLEEP_RESET,
	VCP_SYNC_ULPOSC_CALI,
	VCP_SLEEP_BLOCK_BY_TIMER_CNI,
	VCP_SLEEP_BLOCK_BY_COMPILER_CNT,
	VCP_SLEEP_BLOCK_BY_SEMAPHORE_CNT,
	VCP_SLEEP_BLOCK_BY_WAKELOCK_CNT,
	VCP_SLEEP_BLOCK_BY_IPI_BUSY_CNT,
	VCP_SLEEP_BLOCK_BY_PENDING_IRQ_CNT,
	VCP_SLEEP_BLOCK_BY_SLP_DISABLED_CNT,
	VCP_SLEEP_BLOCK_BY_SLP_BUSY_CNT,
	VCP_SLEEP_BLOCK_BY_HARD1_BUSY_CNT,
	VCP_SLEEP_CMD_MAX,
};

enum ulposc_ver_enum {
	ULPOSC_VER_1,
	MAX_ULPOSC_VERSION,
};

enum vcp_dvfs_chip_hw_enum {
	MT6853,
	MT6873,
	MT6893,
	MAX_VCP_DVFS_CHIP_HW,
};

enum clk_dbg_ver_enum {
	CLK_DBG_VER_1,
	MAX_CLK_DBG_VERSION,
};

enum vcp_clk_ver_enum {
	VCP_CLK_VER_1,
	MAX_VCP_CLK_VERSION,
};

enum ulposc_onoff_enum {
	ULPOSC_OFF,
	ULPOSC_ON,
};

struct mt_vcp_pll_t {
	struct clk *clk_mux;
	struct clk *clk_pll[8];
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
	struct reg_info _cali;
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
	struct reg_info _fmeter_en;
	struct reg_info _trigger_cal;

	struct reg_info _clk26cali_1;
	struct reg_info _cal_cnt;
	struct reg_info _load_cnt;
};

struct ulposc_cali_hw {
	struct regmap *topck_regmap;
	struct regmap *apmixed_regmap;
	struct ulposc_cali_regs *ulposc_regs;
	struct ulposc_cali_config *cali_configs;
	struct clk_cali_regs *clkdbg_regs;
	unsigned int cali_nums;
	unsigned short *cali_val;
	unsigned short *cali_freq;
	bool do_ulposc_cali;
	bool cali_failed;
};

struct vcp_clk_hw {
	struct regmap *vcp_clk_regmap;
	struct reg_info _clk_high_en;
	struct reg_info _ulposc2_en;
	struct reg_info _ulposc2_cg;
	struct reg_info _sel_clk;
};

struct vcp_pmic_regs {
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

struct vcp_dvfs_hw {
	struct dvfs_opp *opp;
	struct regmap *pmic_regmap;
	struct regmap *gpio_regmap;
	struct vcp_pmic_regs *pmic_regs;
	struct ulposc_cali_hw ulposc_hw;
	struct vcp_clk_hw *clk_hw;
	bool pmic_sshub_en;
	bool sleep_init_done;
	bool pre_mux_en;
	int vcp_opp_nums;
	int vow_lp_en_gear;
	int cur_dbg_core;
	u32 core_nums;
};

extern int vcp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel);
extern int vcp_request_freq(void);
extern int vcp_resource_req(unsigned int req);
extern uint32_t vcp_get_freq(void);
extern unsigned int vcp_get_dvfs_opp(void);
extern void vcp_init_vcore_request(void);
extern void vcp_pll_mux_set(unsigned int pll_ctrl_flag);
extern void wait_vcp_dvfs_init_done(void);
extern void sync_ulposc_cali_data_to_vcp(void);
extern int __init vcp_dvfs_init(void);
extern void __exit vcp_dvfs_exit(void);

/* vcp dvfs variable*/
extern unsigned int vcp_expected_freq;
extern unsigned int vcp_current_freq;
extern spinlock_t vcp_awake_spinlock;

#endif  /* __VCP_DVFS_H__ */
