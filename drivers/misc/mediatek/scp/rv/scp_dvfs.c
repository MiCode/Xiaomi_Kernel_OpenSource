// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/bits.h>
#include <linux/build_bug.h>
#include <clk-fmeter.h>

#if IS_ENABLED(CONFIG_MFD_MT6397)
#include <linux/mfd/mt6397/core.h>
#endif /* IS_ENABLED(CONFIG_MFD_MT6397) */

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif /* IS_ENABLED(CONFIG_OF) */

#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "scp.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)			"[scp_dvfs]: " fmt

#define DRV_Reg32(addr)			readl(addr)
#define DRV_WriteReg32(addr, val)	writel(val, addr)
#define DRV_SetReg32(addr, val) DRV_WriteReg32(addr, DRV_Reg32(addr) | (val))
#define DRV_ClrReg32(addr, val) DRV_WriteReg32(addr, DRV_Reg32(addr) & ~(val))

#if IS_ENABLED(CONFIG_MFD_MT6397)
#define pmic_main_chip			mt6397_chip
#endif

#define PMIC_PHANDLE_NAME		"pmic"
#define GPIO_PHANDLE_NAME		"gpio-base"
#define SCP_CLK_CTRL_PHANDLE_NAME	"scp_clk_ctrl"
#define FM_CLK_PHANDLE_NAME		"fmeter_clksys"
#define ULPOSC_CLK_PHANDLE_NAME		"ulposc_clksys"

#define FM_CNT2FREQ(cnt)	(cnt * 26 / CALI_DIV_VAL)
#define FM_FREQ2CNT(freq)	(freq * CALI_DIV_VAL / 26)

unsigned int scp_ipi_ackdata0, scp_ipi_ackdata1;
struct ipi_tx_data_t {
	unsigned int arg1;
	unsigned int arg2;
};

/*
 * -1  : SCP Debug CMD: off,
 * >=0 : SCP DVFS Debug OPP.
 */
static int scp_dvfs_debug_flag = -1;

/* -1:SCP DVFS OFF, 1:SCP DVFS ON */
static int scp_dvfs_flag = 1;

/*
 * 0: SCP Sleep: OFF,
 * 1: SCP Sleep: ON,
 */
static int scp_resrc_current_req = -1;

static struct mt_scp_pll_t mt_scp_pll;
static struct wakeup_source *scp_suspend_lock;
static int g_scp_dvfs_init_flag = -1;
static int g_scp_dvfs_enable; /* feature enabled */

static struct scp_dvfs_hw dvfs;

static struct regulator *dvfsrc_vscp_power;
static struct regulator *reg_vcore;
static struct regulator *reg_vsram;

const char *ulposc_ver[MAX_ULPOSC_VERSION] __initconst = {
	[ULPOSC_VER_1] = "v1",
	[ULPOSC_VER_2] = "v2",
};

const char *clk_dbg_ver[MAX_CLK_DBG_VERSION] __initconst = {
	[CLK_DBG_VER_1] = "v1",
	[CLK_DBG_VER_2] = "v2",
};

const char *scp_clk_ver[MAX_SCP_CLK_VERSION] __initconst = {
	[SCP_CLK_VER_1] = "v1",
};

const char *scp_dvfs_hw_chip_ver[MAX_SCP_DVFS_CHIP_HW] __initconst = {
	[MT6853] = "mediatek,mt6853",
	[MT6873] = "mediatek,mt6873",
	[MT6893] = "mediatek,mt6893",
	[MT6833] = "mediatek,mt6833",
};

struct ulposc_cali_regs cali_regs[MAX_ULPOSC_VERSION] __initdata = {
	[ULPOSC_VER_1] = {
		REG_DEFINE(con0, 0x2C0, REG_MAX_MASK, 0)
		REG_DEFINE(cali, 0x2C0, GENMASK(CAL_BITS, 0), 0)
		REG_DEFINE(con1, 0x2C4, REG_MAX_MASK, 0)
		REG_DEFINE(con2, 0x2C8, REG_MAX_MASK, 0)
	},
	[ULPOSC_VER_2] = { /* Suppose VLP_CKSYS is from 0x1C013000 */
		REG_DEFINE(con0, 0x210, REG_MAX_MASK, 0)
		REG_DEFINE(cali_ext, 0x210, GENMASK(CAL_EXT_BITS, 0), 7)
		REG_DEFINE_WITH_INIT(cali, 0x210, GENMASK(CAL_BITS, 0), 0, 0x40, 0)
		REG_DEFINE(con1, 0x214, REG_MAX_MASK, 0)
		REG_DEFINE(con2, 0x218, REG_MAX_MASK, 0)
	},
};

struct clk_cali_regs clk_dbg_reg[MAX_CLK_DBG_VERSION] __initdata = {
	[CLK_DBG_VER_1] = {
		REG_DEFINE(clk_misc_cfg0, 0x140, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(meter_div, 0x140, 0xFF, 24, 0, 0)

		REG_DEFINE(clk_dbg_cfg, 0x17C, 0x3, 0)
		REG_DEFINE_WITH_INIT(fmeter_ck_sel, 0x17C, 0x3F, 16, 36, 0)
		REG_DEFINE_WITH_INIT(abist_clk, 0x17C, 0x3, 0, 0, 0)

		REG_DEFINE(clk26cali_0, 0x220, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(fmeter_en, 0x220, 0x1, 12, 1, 0)
		REG_DEFINE_WITH_INIT(trigger_cal, 0x220, 0x1, 4, 1, 0)

		REG_DEFINE(clk26cali_1, 0x224, REG_MAX_MASK, 0)
		REG_DEFINE(cal_cnt, 0x224, 0xFFFF, 0)
		REG_DEFINE_WITH_INIT(load_cnt, 0x224, 0x3FF, 16, 0x1FF, 0)
	},
	[CLK_DBG_VER_2] = {/* Suppose VLP_CKSYS is from 0x1C013000 */
		REG_DEFINE(clk26cali_0, 0x230, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(fmeter_ck_sel, 0x230, 0x1F, 16, 0x16, 0)
		REG_DEFINE_WITH_INIT(fmeter_rst, 0x230, 0x1, 15, 0x1, 0)
		REG_DEFINE_WITH_INIT(fmeter_en, 0x230, 0x1, 12, 0x1, 0)
		REG_DEFINE_WITH_INIT(trigger_cal, 0x230, 0x1, 4, 1, 0)

		REG_DEFINE(clk26cali_1, 0x234, REG_MAX_MASK, 0)
		REG_DEFINE(cal_cnt, 0x234, 0xFFFF, 0)
		REG_DEFINE_WITH_INIT(load_cnt, 0x234, 0x3FF, 16, 0x1FF, 0)
	},
};

struct scp_clk_hw scp_clk_hw_regs[MAX_SCP_CLK_VERSION] = {
	[SCP_CLK_VER_1] = {
		REG_DEFINE(clk_high_en, 0x4, 0x1, 1)
		REG_DEFINE(ulposc2_en, 0x6C, 0x1, 5)
		REG_DEFINE(ulposc2_cg, 0x5C, 0x1, 1)
		REG_DEFINE(sel_clk, 0x0, 0xF, 8)
	}
};

struct scp_pmic_regs scp_pmic_hw_regs[MAX_SCP_DVFS_CHIP_HW] = {
	[MT6853] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x1520, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1514, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x151a, 0x1, 11, 1, 1)
	},
	[MT6873] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x15a0, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1594, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x159a, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_buck_en, 0x15aa, 0x1, 0, 1, 0)
		REG_DEFINE_WITH_INIT(sshub_ldo_en, 0x1f28, 0x1, 0, 0, 0)
		REG_DEFINE_WITH_INIT(pmrc_en, 0x1ac, 0x1, 2, 0, 1)
	},
	[MT6893] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x15a0, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1594, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x159a, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_buck_en, 0x15aa, 0x1, 0, 1, 0)
		REG_DEFINE_WITH_INIT(sshub_ldo_en, 0x1f28, 0x1, 0, 0, 0)
		REG_DEFINE_WITH_INIT(pmrc_en, 0x1ac, 0x1, 2, 0, 1)
	},
	[MT6833] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x1520, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1514, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x151a, 0x1, 11, 1, 1)
	},
};

static void slp_ipi_init(void)
{
	int ret;

	ret = mtk_ipi_register(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		NULL, NULL, &scp_ipi_ackdata0);
	if (ret) {
		pr_notice("scp0 slp ipi register failed\n");
		WARN_ON(1);
	}

	if (dvfs.core_nums == 2) {
		ret = mtk_ipi_register(&scp_ipidev, IPI_OUT_C_SLEEP_1,
			NULL, NULL, &scp_ipi_ackdata1);
		if (ret)
			pr_notice("scp1 slp ipi register failed\n");
	}
	if (!ret)
		dvfs.sleep_init_done = true;
}

static int scp_get_vcore_table(unsigned int gear)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, VCORE_ACQUIRE,
		gear, 0, 0, 0, 0, 0, &res);
	if (!(res.a0))
		return res.a1;

	pr_notice("[%s]: should not end here\n", __func__);
	return -1;
}

int scp_resource_req(unsigned int req_type)
{
	struct arm_smccc_res res;

	if (req_type >= SCP_REQ_MAX)
		return 0;

	pr_notice("%s(0x%x)\n", __func__, req_type);

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, RESOURCE_REQ,
		req_type, 0, 0, 0, 0, 0, &res);
	if (!res.a0)
		scp_resrc_current_req = req_type;
	else
		pr_notice("[%s]: resource request failed, req: %u\n",
			__func__, req_type);

	return res.a0;
}

static int scp_reg_update(struct regmap *regmap, struct reg_info *reg, u32 val)
{
	u32 mask;
	int ret = 0;

	if (!reg->msk) {
		pr_notice("[%s]: reg not support\n", __func__);
		return -ESCP_REG_NOT_SUPPORTED;
	}
	mask = reg->msk << reg->bit;
	val = (val & reg->msk) << reg->bit;

	if (reg->setclr) {
		ret = regmap_write(regmap, reg->ofs + 4, mask);
		ret = regmap_write(regmap, reg->ofs + 2, val);
	} else {
		ret = regmap_update_bits(regmap, reg->ofs, mask, val);
	}

	return ret;
}

static int scp_reg_read(struct regmap *regmap, struct reg_info *reg, u32 *val)
{
	int ret = 0;

	if (!reg->msk) {
		pr_notice("[%s]: reg not support\n", __func__);
		return -ESCP_REG_NOT_SUPPORTED;
	}

	ret = regmap_read(regmap, reg->ofs, val);
	if (!ret)
		*val = (*val >> reg->bit) & reg->msk;
	return ret;
}

static inline int scp_reg_init(struct regmap *regmap, struct reg_info *reg)
{
	return scp_reg_update(regmap, reg, reg->init_config);
}

static int scp_get_freq_idx(unsigned int clk_opp)
{
	int i;

	for (i = 0; i < dvfs.scp_opp_nums; i++)
		if (clk_opp == dvfs.opp[i].freq)
			break;

	if (i == dvfs.scp_opp_nums) {
		pr_notice("no available opp, freq: %u\n", clk_opp);
		return -EINVAL;
	}

	return i;
}

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static int scp_update_pmic_vow_lp_mode(bool on)
{
	int ret = 0;

	if (dvfs.vlp_support || dvfs.bypass_pmic_rg_access) {
		pr_notice("[%s]: VCORE DVS is not supported\n", __func__);
		WARN_ON(1);
		return -ESCP_DVFS_DVS_SHOULD_BE_BYPASSED;
	}

	if (on)
		ret = scp_reg_update(dvfs.pmic_regmap,
			&dvfs.pmic_regs->_sshub_op_cfg, 1);
	else
		ret = scp_reg_update(dvfs.pmic_regmap,
			&dvfs.pmic_regs->_sshub_op_cfg, 0);

	return ret;
}
#endif /* CONFIG_FPGA_EARLY_PORTING */

static int scp_set_pmic_vcore(unsigned int cur_freq)
{
	int ret = 0;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int idx = 0;
	unsigned int ret_vc = 0, ret_vs = 0;

	if (dvfs.vlp_support) {
		pr_notice("[%s]: VCORE DVS is not supported\n", __func__);
		WARN_ON(1);
		return -ESCP_DVFS_DVS_SHOULD_BE_BYPASSED;
	}

	idx = scp_get_freq_idx(cur_freq);
	if (idx >= 0 && idx < dvfs.scp_opp_nums) {
		ret = scp_get_vcore_table(dvfs.opp[idx].dvfsrc_opp);
		if (ret > 0)
			dvfs.opp[idx].tuned_vcore = ret;

		ret_vc = regulator_set_voltage(reg_vcore,
				dvfs.opp[idx].tuned_vcore,
				dvfs.opp[dvfs.scp_opp_nums - 1].vcore + 100000);

		ret_vs = regulator_set_voltage(reg_vsram,
				dvfs.opp[idx].vsram,
				dvfs.opp[dvfs.scp_opp_nums - 1].vsram + 100000);
	} else {
		ret = -ESCP_DVFS_OPP_OUT_OF_BOUND;
		pr_notice("[%s]: cur_freq=%d is not supported\n",
			__func__, cur_freq);
		WARN_ON(1);
	}

	if (ret_vc != 0 || ret_vs != 0) {
		ret = -ESCP_DVFS_PMIC_REGULATOR_FAILED;
		pr_notice("[%s]: ERROR: scp vcore/vsram setting error, (%d, %d)\n",
				__func__, ret_vc, ret_vs);
		WARN_ON(1);
	}

	if (dvfs.vow_lp_en_gear != -1) {
		/* vcore > 0.6v cannot hold pmic/vcore in lp mode */
		if (idx < dvfs.vow_lp_en_gear)
			/* enable VOW low power mode */
			scp_update_pmic_vow_lp_mode(true);
		else
			/* disable VOW low power mode */
			scp_update_pmic_vow_lp_mode(false);
	}
#endif /* CONFIG_FPGA_EARLY_PORTING */

	return ret;
}

static uint32_t sum_required_freq(uint32_t core_id)
{
	uint32_t i = 0;
	uint32_t sum = 0;

	if (core_id >= dvfs.core_nums) {
		pr_notice("[%s]: ERROR: core_id is invalid: %u\n",
				__func__, core_id);
		WARN_ON(1);
		core_id = SCPSYS_CORE0;
	}

	/*
	 * calculate scp frequence for core_id
	 */
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (i != VCORE_TEST_FEATURE_ID &&
			feature_table[i].enable == 1 &&
			feature_table[i].sys_id == core_id)
			sum += feature_table[i].freq;
	}

	/*
	 * calculate scp sensor frequence (core0 only)
	 */
	if (core_id == SCPSYS_CORE0)
		for (i = 0; i < NUM_SENSOR_TYPE; i++)
			if (sensor_type_table[i].enable == 1)
				sum += sensor_type_table[i].freq;

	return sum;
}

static uint32_t _mt_scp_dvfs_set_test_freq(uint32_t sum)
{
	uint32_t freq = 0, added_freq = 0, i = 0;

	if (scp_dvfs_debug_flag == -1)
		return 0;

	pr_info("manually set opp = %d\n", scp_dvfs_debug_flag);

	for (i = 0; i < dvfs.scp_opp_nums; i++) {
		freq = dvfs.opp[i].freq;

		if (scp_dvfs_debug_flag == i && sum < freq) {
			added_freq = freq - sum;
			break;
		}
	}
	feature_table[VCORE_TEST_FEATURE_ID].freq = added_freq;
	pr_notice("[%s]test freq: %d + %d = %d (MHz)\n",
			__func__,
			sum,
			added_freq,
			sum + added_freq);

	return added_freq;
}

uint32_t scp_get_freq(void)
{
	uint32_t i;
	uint32_t sum_core0 = 0;
	uint32_t sum_core1 = 0;
	uint32_t sum = 0;
	uint32_t return_freq = 0;

	/*
	 * calculate scp frequence requirement
	 */
	sum_core0 += sum_required_freq(SCPSYS_CORE0);
	if (dvfs.core_nums > SCPSYS_CORE1)
		sum_core1 = sum_required_freq(SCPSYS_CORE1);

	if (sum_core0 > sum_core1) {
		sum = sum_core0;
		feature_table[VCORE_TEST_FEATURE_ID].sys_id = SCPSYS_CORE0;
	} else {
		sum = sum_core1;
		feature_table[VCORE_TEST_FEATURE_ID].sys_id = SCPSYS_CORE1;
	}

	/*
	 * add scp test cmd frequence
	 */
	sum += _mt_scp_dvfs_set_test_freq(sum);

	for (i = 0; i < dvfs.scp_opp_nums; i++) {
		if (sum <= dvfs.opp[i].freq) {
			return_freq = dvfs.opp[i].freq;
			break;
		}
	}

	if (i == dvfs.scp_opp_nums) {
		return_freq = dvfs.opp[dvfs.scp_opp_nums - 1].freq;
		pr_notice("warning: request freq %d > max opp %d\n",
				sum, return_freq);
	}

	return return_freq;
}

static void scp_vcore_request(unsigned int clk_opp)
{
	int idx;
	int ret = 0;

	pr_debug("[%s]: opp(%d)\n", __func__, clk_opp);

	if (dvfs.vlp_support)
		return;

	/* SCP vcore request to PMIC */
	if (dvfs.pmic_sshub_en)
		ret = scp_set_pmic_vcore(clk_opp);

	idx = scp_get_freq_idx(clk_opp);
	if (idx < 0) {
		pr_notice("[%s]: invalid clk_opp %d\n", __func__, clk_opp);
		WARN_ON(1);
		return;
	}

	/* SCP vcore request to DVFSRC
	 * min & max set to requested Vcore value
	 * DVFSRC SW will find corresponding idx to process
	 * if opp[idx].dvfsrc_opp == 0xff, means that
	 * opp[idx] is not supported by DVFSRC
	 */
	if (dvfs.opp[idx].dvfsrc_opp != 0xff)
		ret = regulator_set_voltage(dvfsrc_vscp_power,
			dvfs.opp[idx].vcore,
			dvfs.opp[idx].vcore);
	if (ret) {
		pr_notice("[%s]: dvfsrc vcore update error, opp: %d\n",
			__func__, idx);
		WARN_ON(1);
	}

	/* SCP vcore request to SPM */
	DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, dvfs.opp[idx].spm_opp);
}

void scp_init_vcore_request(void)
{
	if (scp_dvfs_flag != 1)
		scp_vcore_request(dvfs.opp[0].freq);
}

int scp_request_freq_vcore(void)
{
	int timeout = 50;
	int ret = 0;
	unsigned long spin_flags;
	int is_increasing_freq = 0;
	int opp_idx;

	if (scp_dvfs_flag != 1) {
		pr_debug("[%s]: warning: SCP DVFS is OFF\n", __func__);
		return 0;
	}

	/* because we are waiting for scp to update register:scp_current_freq
	 * use wake lock to prevent AP from entering suspend state
	 */
	__pm_stay_awake(scp_suspend_lock);

	if (scp_current_freq != scp_expected_freq) {

		scp_awake_lock((void *)SCP_A_ID);

		/* do DVS before DFS if increasing frequency */
		if (scp_current_freq < scp_expected_freq) {
			scp_vcore_request(scp_expected_freq);
			is_increasing_freq = 1;
		}

		/* Request SPM not to turn off mainpll/26M/infra */
		/* because SCP may park in it during DFS process */
		scp_resource_req(SCP_REQ_26M |
				SCP_REQ_INFRA |
				SCP_REQ_SYSPLL);

		/*  turn on PLL if necessary */
		scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);

		do {
			ret = mtk_ipi_send(&scp_ipidev,
				IPI_OUT_DVFS_SET_FREQ_0,
				IPI_SEND_WAIT, &scp_expected_freq,
				PIN_OUT_SIZE_DVFS_SET_FREQ_0, 0);
			if (ret != IPI_ACTION_DONE)
				pr_notice("SCP send IPI fail - %d, %d\n", ret, timeout);

			mdelay(2);
			timeout -= 1; /*try 50 times, total about 100ms*/
			if (timeout <= 0) {
				pr_notice("set freq fail, current(%d) != expect(%d)\n",
					scp_current_freq, scp_expected_freq);
				scp_resource_req(SCP_REQ_RELEASE);
				__pm_relax(scp_suspend_lock);
				WARN_ON(1);
				return -ESCP_DVFS_IPI_FAILED;
			}

			/* read scp_current_freq again */
			spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
			scp_current_freq = readl(CURRENT_FREQ_REG);
			spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

		} while (scp_current_freq != scp_expected_freq);

		/* turn off PLL if necessary */
		scp_pll_ctrl_set(PLL_DISABLE, scp_expected_freq);

		/* do DVS after DFS if decreasing frequency */
		if (is_increasing_freq == 0)
			scp_vcore_request(scp_expected_freq);

		scp_awake_unlock((void *)SCP_A_ID);

		opp_idx = scp_get_freq_idx(scp_current_freq);
		if (dvfs.opp[opp_idx].resource_req)
			scp_resource_req(dvfs.opp[opp_idx].resource_req);
		else
			scp_resource_req(SCP_REQ_RELEASE);
	}

	__pm_relax(scp_suspend_lock);
	pr_debug("[SCP] succeed to set freq, expect=%d, cur=%d\n",
			scp_expected_freq, scp_current_freq);
	return 0;
}

int scp_request_freq_vlp(void)
{
	int timeout = 50;
	int ret = 0;
	unsigned long spin_flags;
	int opp_idx;

	if (scp_dvfs_flag != 1) {
		pr_debug("[%s]: warning: SCP DVFS is OFF\n", __func__);
		return 0;
	}

	if (!dvfs.vlp_support) {
		pr_notice("[%s]: should not end here: vlp not supported!\n", __func__);
		return 0;
	}

	/*
	 * In order to prevent sending the same freq request from kernel repeatedly,
	 * we used last_scp_expected_freq to record last freq request.
	 */
	if (last_scp_expected_freq == scp_expected_freq) {
		pr_debug("[SCP] Skip DFS: resending the same freq request: %dMhz\n",
			last_scp_expected_freq);
		return 0;
	}

	/* because we are waiting for scp to update register:scp_current_freq
	 * use wake lock to prevent AP from entering suspend state
	 */
	__pm_stay_awake(scp_suspend_lock);

	if (scp_current_freq != scp_expected_freq) {

		scp_awake_lock((void *)SCP_A_ID);

		/* Request SPM not to turn off mainpll/26M/infra */
		/* because SCP may park in it during DFS process */
		scp_resource_req(SCP_REQ_26M |
				SCP_REQ_INFRA |
				SCP_REQ_SYSPLL);

		/*  turn on PLL if necessary */
		scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);

		do {
			ret = mtk_ipi_send(&scp_ipidev,
				IPI_OUT_DVFS_SET_FREQ_0,
				IPI_SEND_WAIT, &scp_expected_freq,
				PIN_OUT_SIZE_DVFS_SET_FREQ_0, 0);
			mdelay(2);
			if (ret != IPI_ACTION_DONE) {
				pr_notice("SCP send IPI fail - %d\n", ret);
			} else {
				last_scp_expected_freq = scp_expected_freq;
				break;
			}

			timeout -= 1; /*try 50 times, total about 100ms*/
		} while (timeout > 0);

		/* if ipi send fail 50(=timeout) times */
		if (timeout <= 0) {
			/* to check scp_current_freq */
			spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
			scp_current_freq = readl(CURRENT_FREQ_REG);
			spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);
			pr_notice("set expected_freq(%d) fail, current is %d\n",
				scp_expected_freq, scp_current_freq);
			__pm_relax(scp_suspend_lock);
			WARN_ON(1);
			return -ESCP_DVFS_IPI_FAILED;
		}

		/* turn off PLL if necessary */
		scp_pll_ctrl_set(PLL_DISABLE, scp_expected_freq);

		scp_awake_unlock((void *)SCP_A_ID);

		opp_idx = scp_get_freq_idx(scp_expected_freq);
		if (dvfs.opp[opp_idx].resource_req)
			scp_resource_req(dvfs.opp[opp_idx].resource_req);
		else
			scp_resource_req(SCP_REQ_RELEASE);
	}

	__pm_relax(scp_suspend_lock);
	pr_debug("[SCP] succeed to set freq, expect=%d, cur=%d\n",
			scp_expected_freq, scp_current_freq);
	return 0;
}

/* scp_request_freq
 * return :<0 means the scp request freq. error
 * return :0  means the request freq. finished
 */
int scp_request_freq(void)
{
	if (dvfs.vlp_support)
		return scp_request_freq_vlp();
	else
		return scp_request_freq_vcore();
}

void wait_scp_dvfs_init_done(void)
{
	int count = 0;

	while (g_scp_dvfs_init_flag != 1) {
		mdelay(1);
		count++;
		if (count > 3000) {
			pr_notice("SCP dvfs driver init fail\n");
			WARN_ON(1);
		}
	}
}

static int set_scp_clk_mux(unsigned int  pll_ctrl_flag)
{
	int ret = 0;

	if (pll_ctrl_flag == PLL_ENABLE) {
		if (!dvfs.pre_mux_en) {
			ret = clk_prepare_enable(mt_scp_pll.clk_mux); /* should not enable twice */
			if (ret) {
				pr_notice("[%s]: clk_prepare_enable failed\n",
					__func__);
				WARN_ON(1);
				return -1;
			}
			dvfs.pre_mux_en = true;
		}
	} else if (pll_ctrl_flag == PLL_DISABLE) {
		clk_disable_unprepare(mt_scp_pll.clk_mux); /* should not disable twice */
		dvfs.pre_mux_en = false;
	}

	return 0;
}

static int __scp_pll_sel_26M(unsigned int pll_ctrl_flag, unsigned int pll_sel)
{
	int ret = 0;

	if (pll_sel != CLK_26M)
		return -EINVAL;

	ret = set_scp_clk_mux(pll_ctrl_flag);
	if (ret)
		return ret;

	if (pll_ctrl_flag == PLL_ENABLE) {
		ret = clk_set_parent(mt_scp_pll.clk_mux, mt_scp_pll.clk_pll[0]);
		if (ret) {
			pr_notice("[%s]: clk_set_parent() failed for 26M\n", __func__);
			WARN_ON(1);
		}
	}

	return ret;
}

int scp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel)
{
	int idx;
	int mux_idx = 0;
	int ret = 0;

	pr_debug("[%s]: (%d, %d)\n", __func__, pll_ctrl_flag, pll_sel);

	if (pll_sel == CLK_26M)
		return __scp_pll_sel_26M(pll_ctrl_flag, pll_sel);

	idx = scp_get_freq_idx(pll_sel);
	if (idx < 0) {
		pr_notice("invalid idx %d\n", idx);
		WARN_ON(1);
		return -EINVAL;
	}

	mux_idx = dvfs.opp[idx].clk_mux;

	if (mux_idx < 0) {
		pr_notice("invalid mux_idx %d\n", mux_idx);
		WARN_ON(1);
		return -EINVAL;
	}

	if (pll_ctrl_flag == PLL_ENABLE) {
		ret = set_scp_clk_mux(pll_ctrl_flag);
		if (ret)
			return ret;
		if (idx >= 0 && idx < dvfs.scp_opp_nums
				&& mux_idx < mt_scp_pll.pll_num)
			ret = clk_set_parent(mt_scp_pll.clk_mux,
					mt_scp_pll.clk_pll[mux_idx]);
		else {
			pr_notice("[%s]: not support opp freq %d\n",
				__func__, pll_sel);
			WARN_ON(1);
		}

		if (ret) {
			pr_notice("[%s]: clk_set_parent() failed, opp=%d\n",
				__func__, pll_sel);
			WARN_ON(1);
		}
	} else if ((pll_ctrl_flag == PLL_DISABLE) &&
			(dvfs.opp[idx].resource_req == 0)) {
		/* if resource_req != 0, it means it may need resource to keep pll on. */
		set_scp_clk_mux(pll_ctrl_flag);
	}
	return ret;
}

void mt_scp_dvfs_state_dump(void)
{
	unsigned int scp_state, slp_pwr_ctrl, power_status;
	char *scp_status = 0;

	scp_state = readl(SCP_A_SLEEP_DEBUG_REG);
	scp_status = ((scp_state & IN_DEBUG_IDLE) == IN_DEBUG_IDLE) ? "idle mode"
			: ((scp_state & ENTERING_SLEEP) == ENTERING_SLEEP) ?
				"enter sleep"
			: ((scp_state & IN_SLEEP) == IN_SLEEP) ?
				"sleep mode"
			: ((scp_state & ENTERING_ACTIVE) == ENTERING_ACTIVE) ?
				"enter active"
			: ((scp_state & IN_ACTIVE) == IN_ACTIVE) ?
				"active mode" : "none of state";

	if (dvfs.vlp_support) {
		slp_pwr_ctrl = readl(SCP_SLP_PWR_CTRL);
		power_status = readl(SCP_POWER_STATUS);
		pr_info("scp status: %s, cpu-off config: %s, power status: %s\n",
			scp_status,
			((slp_pwr_ctrl & R_CPU_OFF) == R_CPU_OFF) ? "enable" : "disable",
			((power_status & POW_ON) == POW_ON) ? "on" : "off");
	} else {
		pr_info("scp status: %s\n", scp_status);
	}
}

#ifdef CONFIG_PROC_FS
/*
 * PROC
 */

/****************************
 * show SCP state
 *****************************/
static int mt_scp_dvfs_state_proc_show(struct seq_file *m, void *v)
{
	unsigned int scp_state, slp_pwr_ctrl, power_status;

	scp_state = readl(SCP_A_SLEEP_DEBUG_REG);
	seq_printf(m, "scp status: %s\n",
		((scp_state & IN_DEBUG_IDLE) == IN_DEBUG_IDLE) ? "idle mode"
		: ((scp_state & ENTERING_SLEEP) == ENTERING_SLEEP) ?
			"enter sleep"
		: ((scp_state & IN_SLEEP) == IN_SLEEP) ?
			"sleep mode"
		: ((scp_state & ENTERING_ACTIVE) == ENTERING_ACTIVE) ?
			"enter active"
		: ((scp_state & IN_ACTIVE) == IN_ACTIVE) ?
			"active mode" : "none of state");
	seq_printf(m, "current debug scp core: %d\n",
		dvfs.cur_dbg_core);

	if (dvfs.vlp_support) {
		slp_pwr_ctrl = readl(SCP_SLP_PWR_CTRL);
		power_status = readl(SCP_POWER_STATUS);
		seq_printf(m, "cpu-off config: %s, power status: %s\n",
			((slp_pwr_ctrl & R_CPU_OFF) == R_CPU_OFF) ? "enable" : "disable",
			((power_status & POW_ON) == POW_ON) ? "on" : "off");
	}
	return 0;
}

/****************************
 * show SCP count
 *****************************/
static int mt_scp_dvfs_sleep_cnt_proc_show(struct seq_file *m, void *v)
{
	struct ipi_tx_data_t ipi_data;
	unsigned int *scp_ack_data = NULL;
	int ret;

	if (!dvfs.sleep_init_done)
		slp_ipi_init();

	ipi_data.arg1 = SCP_SLEEP_GET_COUNT;
	if (dvfs.cur_dbg_core == SCP_CORE_0) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
		scp_ack_data = &scp_ipi_ackdata0;
	} else if (dvfs.cur_dbg_core == SCP_CORE_1) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
		scp_ack_data = &scp_ipi_ackdata1;
	} else {
		pr_notice("[%s]: invalid scp core num: %d\n",
			__func__, dvfs.cur_dbg_core);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[%s] ipi send failed with error: %d\n",
			__func__, ret);
		return -ESCP_DVFS_IPI_FAILED;
	}
	seq_printf(m, "scp core%d sleep count: %d\n",
		dvfs.cur_dbg_core, *scp_ack_data);
	pr_notice("[%s]: scp core%d sleep count :%d\n",
		__func__, dvfs.cur_dbg_core, *scp_ack_data);


	return 0;
}

/**********************************
 * write scp dvfs sleep
 ***********************************/
static ssize_t mt_scp_dvfs_sleep_cnt_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], cmd[32];
	unsigned int len = 0;
	unsigned int ipi_cmd = -1;
	unsigned int ipi_cmd_size = -1;
	int ret = 0;
	int n = 0;
	struct ipi_tx_data_t ipi_data;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!dvfs.sleep_init_done)
		slp_ipi_init();

	n = sscanf(desc, "%31s", cmd);
	if (n != 1) {
		pr_notice("[%s]: invalid command", __func__);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}

	if (!strcmp(cmd, "reset")) {
		ipi_data.arg1 = SCP_SLEEP_RESET;
		if (dvfs.cur_dbg_core == SCP_CORE_0) {
			ipi_cmd = IPI_OUT_C_SLEEP_0;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_0;
		} else if (dvfs.cur_dbg_core == SCP_CORE_1) {
			ipi_cmd = IPI_OUT_C_SLEEP_1;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_1;
		} else {
			pr_notice("[%s]: invalid core index: %d\n",
				__func__, dvfs.cur_dbg_core);
			return -ESCP_DVFS_DBG_INVALID_CMD;
		}
		ret = mtk_ipi_send(&scp_ipidev, ipi_cmd, IPI_SEND_WAIT,
			&ipi_data, ipi_cmd_size, 500);
		if (ret != SCP_IPI_DONE) {
			pr_info("[%s]: SCP send IPI failed - %d\n",
				__func__, ret);
			return -ESCP_DVFS_IPI_FAILED;
		}
	} else {
		pr_notice("[%s]: invalid command: %s\n", __func__, cmd);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}

	return count;
}

/****************************
 * show scp dvfs sleep
 *****************************/
static int mt_scp_dvfs_sleep_proc_show(struct seq_file *m, void *v)
{
	struct ipi_tx_data_t ipi_data;
	unsigned int *scp_ack_data = NULL;
	int ret = 0;

	if (!dvfs.sleep_init_done)
		slp_ipi_init();

	ipi_data.arg1 = SCP_SLEEP_GET_DBG_FLAG;

	if (dvfs.cur_dbg_core == SCP_CORE_0) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
		scp_ack_data = &scp_ipi_ackdata0;
	} else if (dvfs.cur_dbg_core == SCP_CORE_1) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
		scp_ack_data = &scp_ipi_ackdata1;
	} else {
		pr_notice("[%s]: invalid scp core index: %d\n",
			__func__, dvfs.cur_dbg_core);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[%s] ipi send failed with error: %d\n",
			__func__, ret);
	} else {
		if (*scp_ack_data <= SCP_SLEEP_ON)
			seq_printf(m, "scp sleep flag: %d\n",
				*scp_ack_data);
		else
			seq_printf(m, "invalid sleep flag: %d\n",
				*scp_ack_data);
	}

	return 0;
}

/**********************************
 * write scp dvfs sleep
 ***********************************/
static ssize_t mt_scp_dvfs_sleep_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], cmd[32];
	unsigned int len = 0;
	unsigned int ipi_cmd = -1;
	unsigned int ipi_cmd_size = -1;
	int ret = 0;
	int n = 0;
	int dbg_core = -1, slp_cmd = -1;
	struct ipi_tx_data_t ipi_data;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!dvfs.sleep_init_done)
		slp_ipi_init();

	n = sscanf(desc, "%31s %d", cmd, &slp_cmd);
	if (n != 2) {
		pr_notice("[%s]: invalid command", __func__);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}
	if (!strcmp(cmd, "sleep")) {
		if (slp_cmd < SCP_SLEEP_OFF
				|| slp_cmd > SCP_SLEEP_ON) {
			pr_info("[%s]: invalid slp cmd: %d\n",
				__func__, slp_cmd);
			return -ESCP_DVFS_DBG_INVALID_CMD;
		}
		ipi_data.arg1 = slp_cmd;
		if (dvfs.cur_dbg_core == SCP_CORE_0) {
			ipi_cmd = IPI_OUT_C_SLEEP_0;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_0;
		} else if (dvfs.cur_dbg_core == SCP_CORE_1) {
			ipi_cmd = IPI_OUT_C_SLEEP_1;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_1;
		} else {
			pr_notice("[%s]: invalid debug core: %d\n",
				__func__, dvfs.cur_dbg_core);
			return -ESCP_DVFS_DBG_INVALID_CMD;
		}
		ret = mtk_ipi_send(&scp_ipidev, ipi_cmd, IPI_SEND_WAIT,
			&ipi_data, ipi_cmd_size, 500);
		if (ret != SCP_IPI_DONE) {
			pr_info("%s: SCP send IPI fail - %d\n",
				__func__, ret);
			return -ESCP_DVFS_IPI_FAILED;
		}
	} else if (!strcmp(cmd, "dbg_core")) {
		dbg_core = (enum scp_core_enum) slp_cmd;
		if (dbg_core < SCP_MAX_CORE_NUM)
			dvfs.cur_dbg_core = dbg_core;
	} else {
		pr_notice("[%s]: invalid command: %s\n", __func__, cmd);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}

	return count;
}

/****************************
 * show scp dvfs ctrl
 *****************************/
static int mt_scp_dvfs_ctrl_proc_show(struct seq_file *m, void *v)
{
	unsigned long spin_flags;
	unsigned int scp_expected_freq_reg;
	int i;

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_current_freq = readl(CURRENT_FREQ_REG);
	scp_expected_freq_reg = readl(EXPECTED_FREQ_REG);
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);
	seq_printf(m, "SCP DVFS: %s\n", (scp_dvfs_flag == 1)?"ON":"OFF");
	seq_printf(m, "SCP frequency: cur=%dMHz, expect=%dMHz, kernel=%dMHz\n",
				scp_current_freq, scp_expected_freq_reg, scp_expected_freq);

	for (i = 0; i < NUM_FEATURE_ID; i++)
		seq_printf(m, "feature=%d, freq=%d, enable=%d\n",
			feature_table[i].feature, feature_table[i].freq,
			feature_table[i].enable);

	for (i = 0; i < NUM_SENSOR_TYPE; i++)
		seq_printf(m, "sensor id=%d, freq=%d, enable=%d\n",
			sensor_type_table[i].feature, sensor_type_table[i].freq,
			sensor_type_table[i].enable);

	return 0;
}

/**********************************
 * write scp dvfs ctrl
 ***********************************/
static ssize_t mt_scp_dvfs_ctrl_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], cmd[32];
	unsigned int len = 0;
	int dvfs_opp;
	int n;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	n = sscanf(desc, "%31s %d", cmd, &dvfs_opp);
	if (n == 1 || n == 2) {
		if (!strcmp(cmd, "on")) {
			scp_dvfs_flag = 1;
			pr_info("SCP DVFS: ON\n");
		} else if (!strcmp(cmd, "off")) {
			scp_dvfs_flag = -1;
			pr_info("SCP DVFS: OFF\n");
		} else if (!strcmp(cmd, "opp")) {
			if (dvfs_opp == -1) {
				/* deregister dvfs debug feature */
				pr_info("remove the opp setting of command\n");
				feature_table[VCORE_TEST_FEATURE_ID].freq = 0;
				scp_deregister_feature(
						VCORE_TEST_FEATURE_ID);
				scp_dvfs_debug_flag = dvfs_opp;
			} else if (dvfs_opp >= 0 &&
					dvfs_opp < dvfs.scp_opp_nums) {
				/* register dvfs debug feature */
				scp_dvfs_debug_flag = dvfs_opp;
				scp_register_feature(
						VCORE_TEST_FEATURE_ID);
			} else {
				pr_info("invalid opp value %d\n", dvfs_opp);
			}
		} else {
			pr_info("invalid command %s\n", cmd);
		}
	} else {
		pr_info("invalid length %d\n", n);
	}

	return count;
}

#define PROC_FOPS_RW(name) \
static int mt_ ## name ## _proc_open(\
					struct inode *inode, \
					struct file *file) \
{ \
	return single_open(file, \
					mt_ ## name ## _proc_show, \
					PDE_DATA(inode)); \
} \
static const struct proc_ops \
	mt_ ## name ## _proc_fops = {\
	.proc_open		= mt_ ## name ## _proc_open, \
	.proc_read		= seq_read, \
	.proc_lseek		= seq_lseek, \
	.proc_release	= single_release, \
	.proc_write		= mt_ ## name ## _proc_write, \
}

#define PROC_FOPS_RO(name) \
static int mt_ ## name ## _proc_open(\
				struct inode *inode,\
				struct file *file)\
{\
	return single_open(file, \
						mt_ ## name ## _proc_show, \
						PDE_DATA(inode)); \
} \
static const struct proc_ops mt_ ## name ## _proc_fops = {\
	.proc_open		= mt_ ## name ## _proc_open,\
	.proc_read		= seq_read,\
	.proc_lseek		= seq_lseek,\
	.proc_release	= single_release,\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RO(scp_dvfs_state);
PROC_FOPS_RW(scp_dvfs_sleep_cnt);
PROC_FOPS_RW(scp_dvfs_sleep);
PROC_FOPS_RW(scp_dvfs_ctrl);

static int mt_scp_dvfs_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(scp_dvfs_state),
		PROC_ENTRY(scp_dvfs_sleep_cnt),
		PROC_ENTRY(scp_dvfs_sleep),
		PROC_ENTRY(scp_dvfs_ctrl)
	};

	dir = proc_mkdir("scp_dvfs", NULL);
	if (!dir) {
		pr_notice("fail to create /proc/scp_dvfs @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
						0664,
						dir,
						entries[i].fops)) {
			pr_notice("ERROR: %s: create /proc/scp_dvfs/%s failed\n",
						__func__, entries[i].name);
			ret = -ENOMEM;
		}
	}

	return ret;
}
#endif /* CONFIG_PROC_FS */

static const struct of_device_id scpdvfs_of_ids[] = {
	{.compatible = "mediatek,scp_dvfs",},
	{}
};

static void __init mt_pmic_sshub_init(void)
{
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int max_vcore = dvfs.opp[dvfs.scp_opp_nums - 1].tuned_vcore + 100000;
	int max_vsram = dvfs.opp[dvfs.scp_opp_nums - 1].vsram + 100000;

	if (dvfs.pmic_sshub_en) {
		/* set SCP VCORE voltage */
		if (regulator_set_voltage(reg_vcore, dvfs.opp[0].tuned_vcore,
				max_vcore) != 0)
			pr_notice("Set wrong vcore voltage\n");

		/* set SCP VSRAM voltage */
		if (regulator_set_voltage(reg_vsram, dvfs.opp[0].vsram,
				max_vsram) != 0)
			pr_notice("Set wrong vsram voltage\n");
	}

	if (!dvfs.bypass_pmic_rg_access) {
		scp_reg_init(dvfs.pmic_regmap, &dvfs.pmic_regs->_sshub_op_mode);
		scp_reg_init(dvfs.pmic_regmap, &dvfs.pmic_regs->_sshub_op_en);
		scp_reg_init(dvfs.pmic_regmap, &dvfs.pmic_regs->_sshub_op_cfg);
	}

	if (dvfs.pmic_sshub_en) {
		if (!dvfs.bypass_pmic_rg_access) {
			/* BUCK_VCORE_SSHUB_EN: ON */
			/* LDO_VSRAM_OTHERS_SSHUB_EN: ON */
			/* PMRC mode: OFF */
			scp_reg_init(dvfs.pmic_regmap,
				&dvfs.pmic_regs->_sshub_buck_en);
			scp_reg_init(dvfs.pmic_regmap,
				&dvfs.pmic_regs->_sshub_ldo_en);
			scp_reg_init(dvfs.pmic_regmap,
				&dvfs.pmic_regs->_pmrc_en);
		}

		if (regulator_enable(reg_vcore) != 0)
			pr_notice("Enable vcore failed!!!\n");
		if (regulator_enable(reg_vsram) != 0)
			pr_notice("Enable vsram failed!!!\n");
	}
#endif
}

static_assert(CAL_BITS + CAL_EXT_BITS <= 8 * sizeof(unsigned short),
"error: there are only 16bits available in IPI\n");
bool sync_ulposc_cali_data_to_scp(void)
{
	unsigned int sel_clk = 0;
	unsigned int ipi_data[2];
	unsigned short *p = (unsigned short *)&ipi_data[1];
	int i, ret;
	bool cali_ok = true;

	if (!dvfs.ulposc_hw.do_ulposc_cali) {
		pr_notice("[%s]: ulposc2 calibration is not done by AP\n",
			__func__);
		/* u2 is usable, return true */
		return true;
	}

	if (dvfs.ulposc_hw.cali_failed) {
		pr_notice("[%s]: ulposc2 calibration failed\n", __func__);
		return false;
	}

	if (!dvfs.sleep_init_done)
		slp_ipi_init();

	ipi_data[0] = SCP_SYNC_ULPOSC_CALI;
	for (i = 0; i < dvfs.ulposc_hw.cali_nums; i++) {
		*p = dvfs.ulposc_hw.cali_freq[i];
		if ((!dvfs.vlpck_support) || dvfs.vlpck_bypass_phase1)
			*(p + 1) = dvfs.ulposc_hw.cali_val[i];
		else
			*(p + 1) = dvfs.ulposc_hw.cali_val[i] |
				(dvfs.ulposc_hw.cali_val_ext[i] << CAL_BITS);

		pr_notice("[%s]: ipi to scp: freq=%d, cali_val=0x%x\n",
			__func__,
			dvfs.ulposc_hw.cali_freq[i],
			*(p + 1));

		ret = mtk_ipi_send_compl(&scp_ipidev,
					IPI_OUT_C_SLEEP_0,
					IPI_SEND_WAIT,
					&ipi_data[0],
					PIN_OUT_C_SIZE_SLEEP_0,
					500);
		if (ret != IPI_ACTION_DONE) {
			pr_notice("[%s]: ipi send ulposc cali val(%d, 0x%x) fail\n",
				__func__,
				dvfs.ulposc_hw.cali_freq[i],
				*(p + 1));
			WARN_ON(1);
			cali_ok = false;
		}
	}

	scp_reg_read(dvfs.clk_hw->scp_clk_regmap,
		&dvfs.clk_hw->_sel_clk, &sel_clk);
	if ((sel_clk & (SCP_ULPOSC_SEL_CORE | SCP_ULPOSC_SEL_PERI)) == 0) {
		pr_notice("[%s]:ERROR scp is not switched to ULPOSC, CLK_SW_SEL=0x%x\n",
			__func__, sel_clk);
		WARN_ON(1);
		cali_ok = false;
	} else {
		/*
		 * After syncing, scp will be changed to default freq, which is not set by kernel.
		 * Reset last_scp_expected_freq to prevent not updating freq in scp reset flow.
		 */
		last_scp_expected_freq = 0;
	}
	return cali_ok;
}

static inline bool __init is_ulposc_cali_pass(unsigned int cur,
		unsigned int target)
{
	if (cur > (target * (1000 - CALI_MIS_RATE) / 1000)
			&& cur < (target * (1000 + CALI_MIS_RATE) / 1000))
		return 1;

	/* calibrated failed here */
	pr_notice("[%s]: cur: %dMHz, target: %dMHz calibrate failed\n",
		__func__,
		FM_CNT2FREQ(cur),
		FM_CNT2FREQ(target));
	return 0;
}

static unsigned int __init _get_ulposc_clk_by_fmeter_wrapper(void)
{
	unsigned int result;

	result = mt_get_fmeter_freq(dvfs.ccf_fmeter_id, dvfs.ccf_fmeter_type);
	if (result == 0) {
		pr_notice("[%s]: mt_get_fmeter_freq() return %d, pls check CCF configs\n",
			__func__, result);
		WARN_ON(1);
	}
	return FM_FREQ2CNT(result) / 1000;
}

static unsigned int __init get_ulposc_clk_by_fmeter_vlp(void)
{
	unsigned int i = 0, result = 0;
	unsigned int vlp_fqmtr_con0_bk = 0, vlp_fqmtr_con1_bk = 0;
	unsigned int wait_for_measure = 0;
	int is_fmeter_timeout = 0;

	if (dvfs.ccf_fmeter_support)
		return _get_ulposc_clk_by_fmeter_wrapper();

	/* 0. backup regsiters */
	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_0, &vlp_fqmtr_con0_bk);
	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_1, &vlp_fqmtr_con1_bk);

	/* 1. set load cnt */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_load_cnt,
		dvfs.ulposc_hw.clkdbg_regs->_load_cnt.init_config);

	/*
	* 2. select clock source VLP_FQMTR_CON0[20:16] =
	* ULPOSC_1 (0x16), ULPOSC_2 (0x17)
	*/
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_fmeter_ck_sel,
		dvfs.ulposc_hw.clkdbg_regs->_fmeter_ck_sel.init_config);

	/*
	* Make sure @_fmeter_rst which is used to reset fmeter keeps 1.
	*     If it accidentally set to 0, fmeter would be reset. Maybe
	* someone would have initialized it to 1 previously, so you do
	* not need to set it by yourself.
	*/
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_fmeter_rst,
		dvfs.ulposc_hw.clkdbg_regs->_fmeter_rst.init_config);

	/* enable fmeter */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_fmeter_en,
		dvfs.ulposc_hw.clkdbg_regs->_fmeter_en.init_config);

	/* 3. trigger fmeter to start measure */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_trigger_cal,
		dvfs.ulposc_hw.clkdbg_regs->_trigger_cal.init_config);

	/* 4. wait for frequency measurement done */
	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_trigger_cal, &wait_for_measure);
	while (wait_for_measure) {
		i++;
		udelay(10);
		if (i > 40000) {
			is_fmeter_timeout = 1;
			break;
		}
		scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
			&dvfs.ulposc_hw.clkdbg_regs->_trigger_cal, &wait_for_measure);
	}

	/* disable fmeter */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_fmeter_en,
		!dvfs.ulposc_hw.clkdbg_regs->_fmeter_en.init_config);

	/* 5. get fmeter result */
	if (!is_fmeter_timeout) {
		scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
			&dvfs.ulposc_hw.clkdbg_regs->_cal_cnt,
			&result);
	} else {
		result = 0;
	}

	/* 0. restore freq meter registers */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_0, vlp_fqmtr_con0_bk);
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_1, vlp_fqmtr_con1_bk);

	return result;
}

static unsigned int __init get_ulposc_clk_by_fmeter(void)
{
	unsigned int i = 0, result = 0;
	unsigned int misc_org = 0, dbg_org = 0, cali0_org = 0, cali1_org = 0;
	unsigned int wait_for_measure = 0;
	int is_fmeter_timeout = 0;

	if (dvfs.ccf_fmeter_support)
		return _get_ulposc_clk_by_fmeter_wrapper();

	/* backup regsiters */
	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk_misc_cfg0, &misc_org);
	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk_dbg_cfg, &dbg_org);
	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_0, &cali0_org);
	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_1, &cali1_org);

	/* set load cnt */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_load_cnt,
		dvfs.ulposc_hw.clkdbg_regs->_load_cnt.init_config);

	/* select meter clock input CLK_DBG_CFG[1:0] = 0x0 */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_abist_clk,
		dvfs.ulposc_hw.clkdbg_regs->_abist_clk.init_config);

	/* select clock source CLK_DBG_CFG[21:16] */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_fmeter_ck_sel,
		dvfs.ulposc_hw.clkdbg_regs->_fmeter_ck_sel.init_config);

	/* select meter div CLK_MISC_CFG_0[31:24] = 0x3 for div 1 */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_meter_div,
		dvfs.ulposc_hw.clkdbg_regs->_meter_div.init_config);

	/* enable fmeter */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_fmeter_en,
		dvfs.ulposc_hw.clkdbg_regs->_fmeter_en.init_config);

	/* trigger fmeter to start measure */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_trigger_cal,
		dvfs.ulposc_hw.clkdbg_regs->_trigger_cal.init_config);

	scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_trigger_cal, &wait_for_measure);

	/* wait for frequency measurement done */
	while (wait_for_measure) {
		i++;
		udelay(10);
		if (i > 40000) {
			is_fmeter_timeout = 1;
			break;
		}
		scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
			&dvfs.ulposc_hw.clkdbg_regs->_trigger_cal,
			&wait_for_measure);
	}

	/* disable fmeter */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_fmeter_en,
		!dvfs.ulposc_hw.clkdbg_regs->_fmeter_en.init_config);

	/* get fmeter result */
	if (!is_fmeter_timeout) {
		scp_reg_read(dvfs.ulposc_hw.fmeter_regmap,
			&dvfs.ulposc_hw.clkdbg_regs->_cal_cnt,
			&result);
	} else {
		result = 0;
	}

	/* restore freq meter registers */
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk_misc_cfg0, misc_org);
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk_dbg_cfg, dbg_org);
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_0, cali0_org);
	scp_reg_update(dvfs.ulposc_hw.fmeter_regmap,
		&dvfs.ulposc_hw.clkdbg_regs->_clk26cali_1, cali1_org);

	return result;
}

static void __init set_ulposc_cali_value_ext(unsigned int cali_val)
{
	int ret = 0;

	ret = scp_reg_update(dvfs.ulposc_hw.ulposc_regmap,
		&dvfs.ulposc_hw.ulposc_regs->_cali_ext,
		cali_val);

	udelay(50);
}

static void __init set_ulposc_cali_value(unsigned int cali_val)
{
	int ret = 0;

	ret = scp_reg_update(dvfs.ulposc_hw.ulposc_regmap,
		&dvfs.ulposc_hw.ulposc_regs->_cali,
		cali_val);

	udelay(50);
}

/*
*	Since available frequencies are expanded when SCP using VLP_CKSYS,
*	we use 2 phases calibration to widen the searching range.
*		1. dvfs.ulposc_hw.ulposc_regs->_cali_ext
*		2. dvfs.ulposc_hw.ulposc_regs->_cali
*/
static int __init ulposc_cali_process_vlp(unsigned int cali_idx,
		unsigned short *cali_res1, unsigned short *cali_res2)
{
	unsigned int target_val = 0, current_val = 0;
	unsigned int min = CAL_MIN_VAL, max = CAL_MAX_VAL, mid;
	unsigned int diff_by_min = 0, diff_by_max = 0xffff;
	target_val = FM_FREQ2CNT(dvfs.ulposc_hw.cali_freq[cali_idx]);

	/* phase1 */
	if (!dvfs.vlpck_bypass_phase1) {
		/* fixed in phase1 */
		set_ulposc_cali_value(dvfs.ulposc_hw.ulposc_regs->_cali.init_config);
		min = CAL_MIN_VAL_EXT;
		max = CAL_MAX_VAL_EXT;
		do {
			mid = (min + max) / 2;
			if (mid == min) {
				pr_debug("turning_factor1 mid(%u) == min(%u)\n", mid, min);
				break;
			}

			set_ulposc_cali_value_ext(mid);
			current_val = get_ulposc_clk_by_fmeter_vlp();

			if (current_val > target_val)
				max = mid;
			else
				min = mid;
		} while (min <= max);

		set_ulposc_cali_value_ext(min);
		current_val = get_ulposc_clk_by_fmeter_vlp();
		diff_by_min = (current_val > target_val) ?
			(current_val - target_val):(target_val - current_val);

		set_ulposc_cali_value_ext(max);
		current_val = get_ulposc_clk_by_fmeter_vlp();
		diff_by_max = (current_val > target_val) ?
			(current_val - target_val):(target_val - current_val);

		*cali_res1 = (diff_by_min < diff_by_max) ? min : max;
		set_ulposc_cali_value_ext(*cali_res1);
	}

	/* phase2 */
	min = CAL_MIN_VAL;
	max = CAL_MAX_VAL;
	do {
		mid = (min + max) / 2;
		if (mid == min) {
			pr_debug("turning_factor2 mid(%u) == min(%u)\n", mid, min);
			break;
		}

		set_ulposc_cali_value(mid);
		current_val = get_ulposc_clk_by_fmeter_vlp();

		if (current_val > target_val)
			max = mid;
		else
			min = mid;
	} while (min <= max);

	set_ulposc_cali_value(min);
	current_val = get_ulposc_clk_by_fmeter_vlp();
	diff_by_min = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	set_ulposc_cali_value(max);
	current_val = get_ulposc_clk_by_fmeter_vlp();
	diff_by_max = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	*cali_res2 = (diff_by_min < diff_by_max) ? min : max;
	set_ulposc_cali_value(*cali_res2);

	/* Checking final calibration result */
	current_val = get_ulposc_clk_by_fmeter_vlp();
	if (!is_ulposc_cali_pass(current_val, target_val)) {
		pr_notice("[%s]: calibration failed for: %dMHz\n",
			__func__,
			dvfs.ulposc_hw.cali_freq[cali_idx]);
		*cali_res1 = 0;
		*cali_res2 = 0;
		WARN_ON(1);
		return -ESCP_DVFS_CALI_FAILED;
	}

	pr_notice("[%s]: target: %uMhz, calibrated = %uMHz\n",
		__func__,
		FM_CNT2FREQ(target_val),
		FM_CNT2FREQ(current_val));

	return 0;
}

static int __init ulposc_cali_process(unsigned int cali_idx,
		unsigned short *cali_res)
{
	unsigned int target_val = 0, current_val = 0;
	unsigned int min = CAL_MIN_VAL, max = CAL_MAX_VAL, mid;
	unsigned int diff_by_min = 0, diff_by_max = 0xffff;

	target_val = FM_FREQ2CNT(dvfs.ulposc_hw.cali_freq[cali_idx]);

	do {
		mid = (min + max) / 2;
		if (mid == min) {
			pr_debug("mid(%u) == min(%u)\n", mid, min);
			break;
		}

		set_ulposc_cali_value(mid);
		current_val = get_ulposc_clk_by_fmeter();

		if (current_val > target_val)
			max = mid;
		else
			min = mid;
	} while (min <= max);

	set_ulposc_cali_value(min);
	current_val = get_ulposc_clk_by_fmeter();
	diff_by_min = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	set_ulposc_cali_value(max);
	current_val = get_ulposc_clk_by_fmeter();
	diff_by_max = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	*cali_res = (diff_by_min < diff_by_max) ? min : max;

	set_ulposc_cali_value(*cali_res);
	current_val = get_ulposc_clk_by_fmeter();
	if (!is_ulposc_cali_pass(current_val, target_val)) {
		pr_notice("[%s]: calibration failed for: %dMHz\n",
			__func__,
			dvfs.ulposc_hw.cali_freq[cali_idx]);
		*cali_res = 0;
		WARN_ON(1);
		return -ESCP_DVFS_CALI_FAILED;
	}

	pr_notice("[%s]: target: %uMhz, calibrated = %uMHz\n",
		__func__,
		FM_CNT2FREQ(target_val),
		FM_CNT2FREQ(current_val));

	return 0;
}

static int smc_turn_on_ulposc2(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, ULPOSC2_TURN_ON,
		0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static int smc_turn_off_ulposc2(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, ULPOSC2_TURN_OFF,
		0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static void turn_onoff_ulposc2(enum ulposc_onoff_enum on)
{
	if (on) {
		/* turn on ulposc */
		if (dvfs.secure_access_scp) {
			// In case we can't directly access dvfs.clk_hw->scp_clk_regmap
			smc_turn_on_ulposc2();
		} else {
			scp_reg_update(dvfs.clk_hw->scp_clk_regmap,
				&dvfs.clk_hw->_clk_high_en, on);
			scp_reg_update(dvfs.clk_hw->scp_clk_regmap,
				&dvfs.clk_hw->_ulposc2_en, !on);

			/* wait settle time */
			udelay(150);

			scp_reg_update(dvfs.clk_hw->scp_clk_regmap,
				&dvfs.clk_hw->_ulposc2_cg, on);
		}
	} else {
		/* turn off ulposc */
		if (dvfs.secure_access_scp) {
			// In case we can't directly access dvfs.clk_hw->scp_clk_regmap
			smc_turn_off_ulposc2();
		} else {
			scp_reg_update(dvfs.clk_hw->scp_clk_regmap,
				&dvfs.clk_hw->_ulposc2_cg, on);
			udelay(50);
			scp_reg_update(dvfs.clk_hw->scp_clk_regmap,
				&dvfs.clk_hw->_ulposc2_en, !on);
		}
	}
	udelay(50);
}

static int __init mt_scp_dvfs_do_ulposc_cali_process(void)
{
	int ret = 0;
	unsigned int i;

	if (!dvfs.ulposc_hw.do_ulposc_cali) {
		pr_notice("[%s]: ulposc2 calibration is not done by AP\n",
			__func__);
		return 0;
	}

	for (i = 0; i < dvfs.ulposc_hw.cali_nums; i++) {
		turn_onoff_ulposc2(ULPOSC_OFF);

		ret += scp_reg_update(dvfs.ulposc_hw.ulposc_regmap,
			&dvfs.ulposc_hw.ulposc_regs->_con0,
			dvfs.ulposc_hw.cali_configs[i].con0_val);
		ret += scp_reg_update(dvfs.ulposc_hw.ulposc_regmap,
			&dvfs.ulposc_hw.ulposc_regs->_con1,
			dvfs.ulposc_hw.cali_configs[i].con1_val);
		ret += scp_reg_update(dvfs.ulposc_hw.ulposc_regmap,
			&dvfs.ulposc_hw.ulposc_regs->_con2,
			dvfs.ulposc_hw.cali_configs[i].con2_val);
		if (ret) {
			pr_notice("[%s]: config ulposc register failed\n",
				__func__);
			return ret;
		}

		turn_onoff_ulposc2(ULPOSC_ON);

		if (dvfs.vlpck_support)
			ret = ulposc_cali_process_vlp(i, &dvfs.ulposc_hw.cali_val_ext[i], &dvfs.ulposc_hw.cali_val[i]);
		else
			ret = ulposc_cali_process(i, &dvfs.ulposc_hw.cali_val[i]);
		if (ret) {
			pr_notice("[%s]: cali %uMHz ulposc failed\n",
				__func__, dvfs.ulposc_hw.cali_freq[i]);
			dvfs.ulposc_hw.cali_failed = true;
			return -ESCP_DVFS_CALI_FAILED;
		}
	}

	turn_onoff_ulposc2(ULPOSC_OFF);

	pr_notice("[%s]: ulposc calibration all done\n", __func__);

	return ret;
}

static int __init mt_scp_dts_gpio_check(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct reg_info vreq_gpio;
	u32 vreq_mode = 0;
	u32 val = 0;
	int ret = 0;

	ret = of_property_read_u32(node, "gpio-vreq-mode", &vreq_mode);
	if (ret)
		goto GET_GPIO_INFO_FAILED;

	ret = of_property_count_u32_elems(node, "gpio-vreq");
	if ((ret / GPIO_ELEM_CNT) <= 0 || (ret % GPIO_ELEM_CNT) != 0) {
		pr_notice("[%s]: get gpio info failed, count: %d\n",
			__func__, ret);
		return ret;
	}

	ret = of_property_read_u32_index(node, "gpio-vreq", 0, &vreq_gpio.ofs);
	if (ret)
		goto GET_GPIO_INFO_FAILED;

	ret = of_property_read_u32_index(node, "gpio-vreq", 1, &vreq_gpio.msk);
	if (ret)
		goto GET_GPIO_INFO_FAILED;

	ret = of_property_read_u32_index(node, "gpio-vreq", 2, &vreq_gpio.bit);
	if (ret)
		goto GET_GPIO_INFO_FAILED;

	ret = scp_reg_read(dvfs.gpio_regmap, &vreq_gpio, &val);

	if (val == vreq_mode) {
		pr_notice("v_req muxpin setting is correct\n");
	} else {
		pr_notice("WRONG v_req muxpin setting, func mode: %d\n",
			val);
		return -ESCP_DVFS_GPIO_CONFIG_FAILED;
		WARN_ON(1);
	}

	return ret;
GET_GPIO_INFO_FAILED:
	pr_notice("[%s]: get gpio info failed with err: %d\n",
		__func__, ret);
	return ret;
}

static int __init mt_scp_dts_get_cali_hw_setting(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	unsigned int i;
	int ret = 0;

	/* find hw calibration configuration data */
	/* update clk_dbg or ulposc_cali data if there is minor change by hw */
	ret = of_property_count_u32_elems(node, "ulposc-cali-config");
	if ((ret / CALI_CONFIG_ELEM_CNT) <= 0
		|| (ret % CALI_CONFIG_ELEM_CNT) != 0) {
		pr_notice("[%s]: cali config count does not equal to cali nums\n",
			__func__);
		return ret;
	}

	cali_hw->cali_configs = kcalloc(cali_hw->cali_nums,
				sizeof(struct ulposc_cali_config),
				GFP_KERNEL);
	if (!(cali_hw->cali_configs))
		return -ENOMEM;

	for (i = 0; i < cali_hw->cali_nums; i++) {
		ret = of_property_read_u32_index(node, "ulposc-cali-config",
			(i * CALI_CONFIG_ELEM_CNT),
			&(cali_hw->cali_configs[i].con0_val));
		if (ret) {
			pr_notice("[%s]: get con0 setting failed\n", __func__);
			goto CALI_DATA_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, "ulposc-cali-config",
			(i * CALI_CONFIG_ELEM_CNT + 1),
			&(cali_hw->cali_configs[i].con1_val));
		if (ret) {
			pr_notice("[%s]: get con1 setting failed\n", __func__);
			goto CALI_DATA_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, "ulposc-cali-config",
			(i * CALI_CONFIG_ELEM_CNT + 2),
			&(cali_hw->cali_configs[i].con2_val));
		if (ret) {
			pr_notice("[%s]: get con2 setting failed\n", __func__);
			goto CALI_DATA_INIT_FAILED;
		}
	}

	return 0;
CALI_DATA_INIT_FAILED:
	kfree(cali_hw->cali_configs);
	return ret;
}

static int __init mt_scp_dts_get_cali_target(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	int ret = 0;
	unsigned int i;
	unsigned int tmp = 0;

	/* find number of ulposc need to do calibration */
	ret = of_property_read_u32(node, "ulposc-cali-num",
		&cali_hw->cali_nums);
	if (ret) {
		pr_notice("[%s]: find ulposc calibration numbers failed\n",
			__func__);
		return ret;
	}

	ret = of_property_count_u32_elems(node, "ulposc-cali-target");
	if (ret != cali_hw->cali_nums) {
		pr_notice("[%s]: target nums does not equals to ulposc-cali-num\n",
			__func__);
		return ret;
	}

	if (dvfs.vlpck_support) {
		cali_hw->cali_val_ext = kcalloc(cali_hw->cali_nums, sizeof(unsigned short),
					GFP_KERNEL);
		if (!cali_hw->cali_val_ext)
			return -ENOMEM;
	} else {
		cali_hw->cali_val_ext = NULL;
	}

	cali_hw->cali_val = kcalloc(cali_hw->cali_nums, sizeof(unsigned short),
				GFP_KERNEL);
	if (!cali_hw->cali_val) {
		ret = -ENOMEM;
		goto CALI_EXT_TARGET_ALLOC_FAILED;
	}

	cali_hw->cali_freq = kcalloc(cali_hw->cali_nums, sizeof(unsigned short),
				GFP_KERNEL);
	if (!cali_hw->cali_freq) {
		ret = -ENOMEM;
		goto CALI_TARGET_ALLOC_FAILED;
	}

	for (i = 0; i < cali_hw->cali_nums; i++) {
		ret = of_property_read_u32_index(node, "ulposc-cali-target",
			i, &tmp);
		if (ret) {
			pr_notice("[%s]: find cali target failed, idx: %d\n",
				__func__, i);
			goto FIND_TARGET_FAILED;
		}
		cali_hw->cali_freq[i] = (unsigned short) tmp;
	}

	return ret;

FIND_TARGET_FAILED:
	kfree(cali_hw->cali_freq);
CALI_TARGET_ALLOC_FAILED:
	kfree(cali_hw->cali_val);
CALI_EXT_TARGET_ALLOC_FAILED:
	kfree(cali_hw->cali_val_ext);
	return ret;
}

static int __init mt_scp_dts_get_cali_hw_regs(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	const char *str = NULL;
	int ret = 0;
	int i;

	/* find ulposc register hw version */
	ret = of_property_read_string(node, "ulposc-cali-ver", &str);
	if (ret) {
		pr_notice("[%s]: find ulposc-cali-ver failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0; i < MAX_ULPOSC_VERSION; i++)
		if (!strcmp(ulposc_ver[i], str))
			cali_hw->ulposc_regs = &cali_regs[i];

	if (!cali_hw->ulposc_regs) {
		pr_notice("[%s]: no ulposc cali reg found\n", __func__);
		return -ESCP_DVFS_NO_CALI_HW_FOUND;
	}

	/* find clk dbg register hw version */
	if (!dvfs.ccf_fmeter_support) {
		ret = of_property_read_string(node, "clk-dbg-ver", &str);
		if (ret) {
			pr_notice("[%s]: find clk-dbg-ver failed with err: %d\n",
				__func__, ret);
			return ret;
		}

		for (i = 0; i < MAX_CLK_DBG_VERSION; i++)
			if (!strcmp(clk_dbg_ver[i], str))
				cali_hw->clkdbg_regs = &clk_dbg_reg[i];
		if (!cali_hw->clkdbg_regs) {
			pr_notice("[%s]: no clkfbg regs found\n",
				__func__);
			return -ESCP_DVFS_NO_CALI_HW_FOUND;
		}
	}

	return ret;
}

#if IS_ENABLED(CONFIG_PM)
static int mt_scp_dump_sleep_count(void)
{
	int ret = 0;
	struct ipi_tx_data_t ipi_data;

	if (!dvfs.sleep_init_done)
		slp_ipi_init();

	ipi_data.arg1 = SCP_SLEEP_GET_COUNT;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		goto FINISH;
	}

	if (dvfs.core_nums < 2) {
		pr_notice("[SCP] [%s:%d] - scp_sleep_cnt_0 = %d\n",
			__func__, __LINE__, scp_ipi_ackdata0);
		goto FINISH;
	}

	/* if there are 2 cores */
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		goto FINISH;
	}

	pr_notice("[SCP] [%s:%d] - scp_sleep_cnt_0 = %d, scp_sleep_cnt_1 = %d\n",
		__func__, __LINE__, scp_ipi_ackdata0, scp_ipi_ackdata1);

FINISH:
	return 0;
}

static int scp_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
	case PM_POST_SUSPEND:
		mt_scp_dvfs_state_dump();
		mt_scp_dump_sleep_count();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block scp_pm_notifier_func = {
	.notifier_call = scp_pm_event,
};
#endif /* IS_ENABLED(CONFIG_PM) */

static int __init mt_scp_dts_init_scp_clk_hw(struct device_node *node)
{
	const char *str = NULL;
	unsigned int i;
	int ret = 0;

	ret = of_property_read_string(node, "scp-clk-hw-ver", &str);
	if (ret) {
		pr_notice("[%s]: find scp-clk-hw-ver failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0; i < MAX_SCP_CLK_VERSION; i++) {
		if (!strcmp(scp_clk_ver[i], str)) {
			dvfs.clk_hw = &(scp_clk_hw_regs[i]);
			return 0;
		}
	}

	pr_notice("[%s]: no scp clk hw reg found\n", __func__);

	return -ESCP_DVFS_NO_CALI_HW_FOUND;
}

static int __init mt_scp_dts_init_cali_regmap(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	/* init regmap for calibration process */
	if (!dvfs.ccf_fmeter_support) {
		cali_hw->fmeter_regmap = syscon_regmap_lookup_by_phandle(node,
								FM_CLK_PHANDLE_NAME);
		if (IS_ERR(cali_hw->fmeter_regmap)) {
			pr_notice("fmeter regmap init failed: %ld\n",
				PTR_ERR(cali_hw->fmeter_regmap));
			return PTR_ERR(cali_hw->fmeter_regmap);
		}
	}

	cali_hw->ulposc_regmap = syscon_regmap_lookup_by_phandle(node,
							ULPOSC_CLK_PHANDLE_NAME);
	if (IS_ERR(cali_hw->ulposc_regmap)) {
		pr_notice("ulposc regmap init failed: %ld\n",
			PTR_ERR(cali_hw->ulposc_regmap));
		return PTR_ERR(cali_hw->ulposc_regmap);
	}

	return 0;
}

static int __init mt_scp_dts_ulposc_cali_init(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	int ret = 0;

	if (!cali_hw) {
		pr_notice("[%s]: ulposc calibration hw is NULL\n",
			__func__);
		WARN_ON(1);
		return -ESCP_DVFS_NO_CALI_HW_FOUND;
	}

	/* check if current platform need to do calibration */
	dvfs.ulposc_hw.do_ulposc_cali = of_property_read_bool(node,
							"do-ulposc-cali");
	if (!dvfs.ulposc_hw.do_ulposc_cali) {
		pr_notice("[%s]: skip ulposc calibration process\n", __func__);
		return 0;
	}

	ret = mt_scp_dts_init_cali_regmap(node, cali_hw);
	if (ret)
		return ret;

	ret = mt_scp_dts_init_scp_clk_hw(node);
	if (ret)
		return ret;

	ret = mt_scp_dts_get_cali_hw_regs(node, cali_hw);
	if (ret)
		return ret;

	dvfs.clk_hw->scp_clk_regmap = syscon_regmap_lookup_by_phandle(node,
						SCP_CLK_CTRL_PHANDLE_NAME);
	if (!dvfs.clk_hw->scp_clk_regmap) {
		pr_notice("[%s]: get scp clk regmap failed\n", __func__);
		return ret;
	}

	ret = mt_scp_dts_get_cali_target(node, cali_hw);
	if (ret)
		return ret;

	ret = mt_scp_dts_get_cali_hw_setting(node, cali_hw);
	if (ret)
		return ret;

	return ret;
}

static int __init mt_scp_dts_clk_init(struct platform_device *pdev)
{
	char buf[15];
	int ret = 0;
	int i;

	mt_scp_pll.clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(mt_scp_pll.clk_mux)) {
		dev_notice(&pdev->dev, "cannot get clock mux\n");
		WARN_ON(1);
		return PTR_ERR(mt_scp_pll.clk_mux);
	}

	/* scp_sel has most 9 member of clk source */
	mt_scp_pll.pll_num = MAX_SUPPORTED_PLL_NUM;
	for (i = 0; i < MAX_SUPPORTED_PLL_NUM; i++) {
		ret = snprintf(buf, 15, "clk_pll_%d", i);
		if (ret < 0 || ret >= 15) {
			pr_notice("[%s]: clk name buf len: %d\n",
				__func__, ret);
			return ret;
		}

		mt_scp_pll.clk_pll[i] = devm_clk_get(&pdev->dev, buf);
		if (IS_ERR(mt_scp_pll.clk_pll[i])) {
			dev_notice(&pdev->dev,
					"cannot get %dst clock parent\n",
					i);
			mt_scp_pll.pll_num = i;
			break;
		}
	}

	return 0;
}

static int __init mt_scp_dts_init_dvfs_data(struct device_node *node,
		struct dvfs_opp **opp)
{
	int ret = 0;
	int i;

	if (*opp) {
		pr_notice("[%s]: opp is initialized\n", __func__);
		return -ESCP_DVFS_DATA_RE_INIT;
	}

	/* get scp dvfs opp count */
	ret = of_property_count_u32_elems(node, "dvfs-opp");
	if ((ret / OPP_ELEM_CNT) <= 0 || (ret % OPP_ELEM_CNT) != 0) {
		pr_notice("[%s]: get dvfs opp count failed, count: %d\n",
			__func__, ret);
		return ret;
	}
	dvfs.scp_opp_nums = ret / 7;

	*opp = kcalloc(dvfs.scp_opp_nums, sizeof(struct dvfs_opp), GFP_KERNEL);
	if (!(*opp))
		return -ENOMEM;

	/* get each dvfs opp data from dts node */
	for (i = 0; i < dvfs.scp_opp_nums; i++) {
		ret = of_property_read_u32_index(node, "dvfs-opp",
				i * OPP_ELEM_CNT,
				&(*opp)[i].vcore);
		if (ret) {
			pr_notice("Cannot get property vcore(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp",
				(i * OPP_ELEM_CNT) + 1,
				&(*opp)[i].vsram);
		if (ret) {
			pr_notice("Cannot get property vsram(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp",
				(i * OPP_ELEM_CNT) + 2,
				&(*opp)[i].dvfsrc_opp);
		if (ret) {
			pr_notice("Cannot get property dvfsrc opp(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}
		if ((!dvfs.vlp_support) && ((*opp)[i].dvfsrc_opp != 0xff)) {
			ret = scp_get_vcore_table((*opp)[i].dvfsrc_opp);
			if (ret > 0) {
				(*opp)[i].tuned_vcore = ret;
			} else {
				/* As default value, if atf is not available */
				(*opp)[i].tuned_vcore = (*opp)[i].vcore;
			}
		} else {
			(*opp)[i].tuned_vcore = (*opp)[i].vcore;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp",
				(i * OPP_ELEM_CNT) + 3,
				&(*opp)[i].spm_opp);
		if (ret) {
			pr_notice("Cannot get property spm opp(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp",
				(i * OPP_ELEM_CNT) + 4,
				&(*opp)[i].freq);

		if (ret) {
			pr_notice("Cannot get property freq(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp",
				(i * OPP_ELEM_CNT) + 5,
				&(*opp)[i].clk_mux);
		if (ret) {
			pr_notice("Cannot get property clk mux(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp",
				(i * OPP_ELEM_CNT) + 6,
				&(*opp)[i].resource_req);
		if (ret) {
			pr_notice("Cannot get property opp resource(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}
	}
	return ret;

OPP_INIT_FAILED:
	kfree(*opp);
	return ret;
}

static int __init mt_scp_dts_init_pmic_data(void)
{
	unsigned int i;

	for (i = 0; i < MAX_SCP_DVFS_CHIP_HW; i++) {
		if (of_machine_is_compatible(scp_dvfs_hw_chip_ver[i])) {
			dvfs.pmic_regs = &scp_pmic_hw_regs[i];
			return 0;
		}
	}

	/* init pmic data failed here */
	pr_notice("[%s]: no scp pmic regs found\n", __func__);
	return -ESCP_DVFS_NO_PMIC_REG_FOUND;
}

static int __init mt_scp_dts_regmap_init(struct platform_device *pdev,
		struct device_node *node)
{
	struct platform_device *pmic_pdev;
	struct device_node *pmic_node;
	struct pmic_main_chip *chip;
	struct regmap *regmap;

	/* get GPIO regmap */
	regmap = syscon_regmap_lookup_by_phandle(node, GPIO_PHANDLE_NAME);
	if (IS_ERR(regmap)) {
		dev_notice(&pdev->dev,
			"Get gpio regmap fail: %ld\n",
			PTR_ERR(regmap));
		goto REGMAP_FIND_FAILED;
	}

	dvfs.gpio_regmap = regmap;

	/* get PMIC regmap */
	if (dvfs.vlp_support)
		goto BYPASS_PMIC;

	dvfs.bypass_pmic_rg_access = of_property_read_bool(node, "no-pmic-rg-access");
	pr_notice("bypass_pmic_rg_access: %s\n", dvfs.bypass_pmic_rg_access?"Yes":"No");
	if (dvfs.bypass_pmic_rg_access)
		goto BYPASS_PMIC;

	pmic_node = of_parse_phandle(node, PMIC_PHANDLE_NAME, 0);
	if (!pmic_node) {
		dev_notice(&pdev->dev, "fail to find pmic node\n");
		goto REGMAP_FIND_FAILED;
	}

	pmic_pdev = of_find_device_by_node(pmic_node);
	if (!pmic_pdev) {
		dev_notice(&pdev->dev, "fail to find pmic device\n");
		goto REGMAP_FIND_FAILED;
	}

	chip = dev_get_drvdata(&(pmic_pdev->dev));
	if (!chip) {
		dev_notice(&pdev->dev, "fail to find pmic drv data\n");
		goto REGMAP_FIND_FAILED;
	}

	regmap = chip->regmap;
	if (IS_ERR_VALUE(regmap)) {
		dvfs.pmic_regmap = NULL;
		dev_notice(&pdev->dev, "get pmic regmap fail\n");
		goto REGMAP_FIND_FAILED;
	}

	dvfs.pmic_regmap = regmap;

BYPASS_PMIC:
	return 0;
REGMAP_FIND_FAILED:
	WARN_ON(1);
	return -ESCP_DVFS_REGMAP_INIT_FAILED;
}

static int __init mt_scp_dts_init(struct platform_device *pdev)
{
	struct device_node *node;
	int ret = 0;
	bool is_scp_dvfs_disable;
	const char *str = NULL;

	/* find device tree node of scp_dvfs */
	node = pdev->dev.of_node;
	if (!node) {
		dev_notice(&pdev->dev, "fail to find SCPDVFS node\n");
		return -ENODEV;
	}

	/* used to replace 'SCP_DVFS_INIT_ENABLE' compile flag */
	is_scp_dvfs_disable = of_property_read_bool(node, "scp-dvfs-disable");
	if (is_scp_dvfs_disable) {
		g_scp_dvfs_enable = 0;
		pr_notice("SCP DVFS is disabled, so bypass its init\n");
		return 0;
	}
	g_scp_dvfs_enable = 1;

	/*
	* if set, no VCORE DVS is needed & PMIC setting should
	* be done in SCP side.
	*/
	dvfs.vlp_support = of_property_read_bool(node, "vlp-support");
	if (dvfs.vlp_support)
		pr_notice("[%s]: VCORE DVS sould be bypassed\n", __func__);

	if (dvfs.vlp_support) {
		dvfs.vow_lp_en_gear = -1;
	} else {
		ret = of_property_read_u32(node, "vow-lp-en-gear",
			&dvfs.vow_lp_en_gear);
		if (ret) {
			pr_notice("[%s]: no vow-lp-enable-gear property, set gear to -1\n",
				__func__);
			dvfs.vow_lp_en_gear = -1;
		}
	}

	dvfs.vlpck_support = of_property_read_bool(node, "vlpck-support");
	if (dvfs.vlpck_support) {
		dvfs.vlpck_bypass_phase1 = of_property_read_bool(node, "vlpck-bypass-phase1");
		pr_notice("[%s]: Use %d-phase VLP_CKSYS in calibration flow\n",
			__func__,
			dvfs.vlpck_bypass_phase1 ? 1:2);
	} else {
		dvfs.vlpck_bypass_phase1 = false;
	}

	ret = of_property_read_u32(node, "scp-cores",
		&dvfs.core_nums);
	if (ret || dvfs.core_nums > SCP_MAX_CORE_NUM) {
		pr_notice("[%s]: find invalid core numbers, set to 1\n",
			__func__);
		dvfs.core_nums = 1;
	}

	if (dvfs.vlp_support)
		dvfs.pmic_sshub_en = false;
	else
		dvfs.pmic_sshub_en = of_property_read_bool(node, "pmic-sshub-support");

	ret = mt_scp_dts_regmap_init(pdev, node);
	if (ret) {
		pr_notice("[%s]: scp regmap init failed with err: %d\n",
			__func__, ret);
		goto DTS_FAILED;
	}

	if (dvfs.vlp_support || dvfs.bypass_pmic_rg_access)
		pr_notice("bypass pmic rg init\n");
	else {
		ret = mt_scp_dts_init_pmic_data();
		if (ret)
			goto DTS_FAILED;
	}

	/*
	 * 1. If "ccf-fmeter-support" was set, it means common clock framework has provided API
	 * to use fmeter. And we should use mt_get_fmeter_freq(id, type) defined in clk-fmeter.h,
	 * instead of using get_ulposc_clk_by_fmeter*().
	 *
	 * 2. Only wehn mt_get_fmeter_freq havn't been provide, get_ulposc_clk_by_fmeter*() can
	 * be used temporarily.
	 */
	dvfs.ccf_fmeter_support = of_property_read_bool(node, "ccf-fmeter-support");
	if (!dvfs.ccf_fmeter_support) {
		pr_notice("[%s]: fmeter api havn't been provided, use legacy one\n", __func__);
	} else {
		/* enum FMETER_TYPE */
		if (dvfs.vlpck_support)
			dvfs.ccf_fmeter_type = VLPCK;
		else
			dvfs.ccf_fmeter_type = ABIST;

		/* enum FMETER_ID */
		ret = mt_get_fmeter_id(FID_ULPOSC2);
		dvfs.ccf_fmeter_id = ret;
		pr_notice("[%s]: init ccf fmeter api, id: %d, type: %d\n",
			__func__, dvfs.ccf_fmeter_id, dvfs.ccf_fmeter_type);
		if (ret < 0) {
			pr_notice("[%s]: failed to init ccf fmeter api, id: %d, type: %d\n",
				__func__, dvfs.ccf_fmeter_id, dvfs.ccf_fmeter_type);
			goto DTS_FAILED;
		}
	}

	/* init dvfs data */
	ret = mt_scp_dts_init_dvfs_data(node, &dvfs.opp);
	if (ret) {
		pr_notice("[%s]: scp dvfs opp data init failed with err: %d\n",
			__func__, ret);
		goto DTS_FAILED;
	}

	/* init clock mux/pll */
	ret = mt_scp_dts_clk_init(pdev);
	if (ret) {
		pr_notice("[%s]: init scp clk failed with err: %d\n",
			__func__, ret);
		goto DTS_FAILED_FREE_RES;
	}

	/* init ulposc cali dts data */
	ret = mt_scp_dts_ulposc_cali_init(node, &dvfs.ulposc_hw);
	if (ret) {
		pr_notice("[%s]: init scp ulposc cali data with err: %d\n",
			__func__, ret);
		goto DTS_FAILED_FREE_RES;
	}

	if (!dvfs.vlp_support) {
		/* get dvfsrc regulator */
		dvfsrc_vscp_power = regulator_get(&pdev->dev, "dvfsrc-vscp");
		if (IS_ERR(dvfsrc_vscp_power) || !dvfsrc_vscp_power) {
			pr_notice("regulator dvfsrc-vscp is not available\n");
			ret = PTR_ERR(dvfsrc_vscp_power);
			goto PASS;
		}

		/* get Vcore/Vsram Regulator */
		reg_vcore = devm_regulator_get_optional(&pdev->dev, "sshub-vcore");
		if (IS_ERR(reg_vcore) || !reg_vcore) {
			pr_notice("regulator vcore sshub supply is not available\n");
			ret = PTR_ERR(reg_vcore);
			goto PASS;
		}

		reg_vsram = devm_regulator_get_optional(&pdev->dev, "sshub-vsram");
		if (IS_ERR(reg_vsram) || !reg_vsram) {
			pr_notice("regulator vsram sshub supply is not available\n");
			ret = PTR_ERR(reg_vsram);
			goto PASS;
		}
	}

	/* get secure_access_scp node */
	if (dvfs.vlp_support)
		dvfs.secure_access_scp = 1;
	else {
		dvfs.secure_access_scp = 0;
		of_property_read_string(node, "secure_access", &str);
		if (str && strcmp(str, "enable") == 0)
			dvfs.secure_access_scp = 1;
	}
	pr_notice("secure_access_scp: %s\n", dvfs.secure_access_scp?"enable":"disable");

	/* get SCP DVFS enable/disable flag */
	of_property_read_string(node, "scp_dvfs_flag", &str);
	if (str && strcmp(str, "disable") == 0) {
		scp_dvfs_flag = 0;
		pr_notice("scp_dvfs_flag = 0\n");
	}

PASS:
	return 0;

DTS_FAILED_FREE_RES:
	kfree(dvfs.opp);
DTS_FAILED:
	return ret;
}

/* used to replace 'SCP_DVFS_INIT_ENABLE' compile flag */
int scp_dvfs_feature_enable(void)
{
	return g_scp_dvfs_enable;
}

static int __init mt_scp_dvfs_pdrv_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = mt_scp_dts_init(pdev);
	if (ret) {
		pr_notice("[%s]: dts init failed with err: %d\n",
			__func__, ret);
		goto DTS_INIT_FAILED;
	}

	if (!scp_dvfs_feature_enable()) {
		g_scp_dvfs_init_flag = 1;
		pr_notice("bypass scp dvfs init\n");
		return 0;
	}

	/* check gpio setting */
	ret = mt_scp_dts_gpio_check(pdev);
	if (ret)
		goto GPIO_CHECK_FAILED;

	/* init sshub */
	if (!dvfs.vlp_support)
		mt_pmic_sshub_init();

	/* do ulposc calibration */
	mt_scp_dvfs_do_ulposc_cali_process();
	kfree(dvfs.ulposc_hw.cali_configs);

	scp_suspend_lock = wakeup_source_register(NULL, "scp wakelock");

#if IS_ENABLED(CONFIG_PM)
	ret = register_pm_notifier(&scp_pm_notifier_func);
	if (ret) {
		pr_notice("[%s]: failed to register PM notifier.\n", __func__);
		WARN_ON(1);
	}
#endif /* IS_ENABLED(CONFIG_PM) */

#if IS_ENABLED(CONFIG_PROC_FS)
	/* init proc */
	if (mt_scp_dvfs_create_procfs()) {
		pr_notice("mt_scp_dvfs_create_procfs fail..\n");
		WARN_ON(1);
	}
#endif /* CONFIG_PROC_FS */

	g_scp_dvfs_init_flag = 1;
	pr_notice("[%s]: scp_dvfs probe done\n", __func__);

	return 0;

GPIO_CHECK_FAILED:
	kfree(dvfs.opp);
	kfree(dvfs.ulposc_hw.cali_configs);
DTS_INIT_FAILED:
	WARN_ON(1);

	return -ESCP_DVFS_INIT_FAILED;
}

/***************************************
 * this function should never be called
 ****************************************/
static int mt_scp_dvfs_pdrv_remove(struct platform_device *pdev)
{
	if (!scp_dvfs_feature_enable()) {
		pr_notice("bypass scp dvfs pdrv remove\n");
		return 0;
	}

	kfree(dvfs.opp);
	kfree(dvfs.ulposc_hw.cali_val_ext);
	kfree(dvfs.ulposc_hw.cali_val);
	kfree(dvfs.ulposc_hw.cali_freq);
	kfree(dvfs.ulposc_hw.cali_configs);

	return 0;
}

static struct platform_driver mt_scp_dvfs_pdrv __refdata = {
	.probe = mt_scp_dvfs_pdrv_probe,
	.remove = mt_scp_dvfs_pdrv_remove,
	.driver = {
		.name = "scp_dvfs",
		.owner = THIS_MODULE,
		.of_match_table = scpdvfs_of_ids,
	},
};

/**********************************
 * mediatek scp dvfs initialization
 ***********************************/
int __init scp_dvfs_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	ret = platform_driver_register(&mt_scp_dvfs_pdrv);
	if (ret) {
		pr_notice("fail to register scp dvfs driver @ %s()\n", __func__);
		goto fail;
	}

	pr_notice("[%s]: scp_dvfs init done\n", __func__);
	return 0;
fail:
	WARN_ON(1);
	return -ESCP_DVFS_INIT_FAILED;
}

void scp_dvfs_exit(void)
{
	platform_driver_unregister(&mt_scp_dvfs_pdrv);
}

