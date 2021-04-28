/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/pm_wakeup.h>
#include <linux/io.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_secure_api.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/pm_qos.h>

#include <linux/uaccess.h>
#include "scp_ipi_pin.h"
#include "scp_mbox_layout.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#include "mtk_pmic_info.h"
#include "mtk_pmic_api_buck.h"
#endif

#include <linux/pm_qos.h>
#include "helio-dvfsrc-opp.h"
#include "mtk_secure_api.h"
#include "clk-fmeter.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)	"[scp_dvfs]: " fmt

#define DRV_Reg32(addr)	readl(addr)
#define DRV_WriteReg32(addr, val) writel(val, addr)
#define DRV_SetReg32(addr, val)	DRV_WriteReg32(addr, DRV_Reg32(addr) | (val))
#define DRV_ClrReg32(addr, val)	DRV_WriteReg32(addr, DRV_Reg32(addr) & ~(val))

#define SCP_ATF_RESOURCE_REQUEST	1
#define SCP_VCORE_REQ_TO_DVFSRC		1

struct ipi_tx_data_t {
	unsigned int arg1;
	unsigned int arg2;
};

/* -1:SCP DVFS OFF, 1:SCP DVFS ON */
int scp_dvfs_flag = 1;

/*
 * -1: SCP Debug CMD: off,
 * 0-4: SCP DVFS Debug OPP.
 */
static int scp_dvfs_debug_flag = -1;

/*
 * -1: SCP Req init state,
 * 0: SCP Request Release,
 * 1-4: SCP Request source.
 */
static int scp_resrc_req_cmd = -1;
static int scp_resrc_current_req = -1;

static int pre_pll_sel = -1;
static struct mt_scp_pll_t *mt_scp_pll;
static struct wakeup_source scp_suspend_lock;
static int g_scp_dvfs_init_flag = -1;

static void __iomem *gpio_base;
#define ADR_GPIO_MODE_OF_SCP_VREQ	(gpio_base + 0x480)
#define BIT_GPIO_MODE_OF_SCP_VREQ	12
#define MSK_GPIO_MODE_OF_SCP_VREQ	0x7

#if SCP_VCORE_REQ_TO_DVFSRC
static struct pm_qos_request dvfsrc_scp_vcore_req;
#endif

unsigned int slp_ipi_ackdata0, slp_ipi_ackdata1;
int slp_ipi_init_done;

#ifdef ULPOSC_CALI_BY_AP
static void __iomem *ulposc_base;

#define ULPOSC2_CON0 (ulposc_base + 0x2C0)
#define RG_OSC_CALI_MSK		0x7F
#define RG_OSC_CALI_SHFT	0

#define ULPOSC2_CON1 (ulposc_base + 0x2C4)
#define ULPOSC2_CON2 (ulposc_base + 0x2C8)

#define CAL_MIN_VAL		0
#define CAL_MAX_VAL		RG_OSC_CALI_MSK

/* calibation miss rate, unit: 1% */
#define CAL_MIS_RATE	5

#define MAX_ULPOSC_CALI_NUM	3
struct ulposc_cali_t ulposc_cfg[MAX_ULPOSC_CALI_NUM] = {
	{
		.freq = CLK_OPP0,
		.ulposc_rg0 = 0x5aeb40,
		.ulposc_rg1 = 0x3002900,
		.ulposc_rg2 = 0x43,
		.fmeter_id = FREQ_METER_ABIST_AD_OSC_CK_2,
	},
	{
		.freq = CLK_OPP2,
		.ulposc_rg0 = 0x3ceb40,
		.ulposc_rg1 = 0x2900,
		.ulposc_rg2 = 0x43,
		.fmeter_id = FREQ_METER_ABIST_AD_OSC_CK_2,
	},
	{
		.freq = CLK_OPP3,
		.ulposc_rg0 = 0x52eb40,
		.ulposc_rg1 = 0x2900,
		.ulposc_rg2 = 0x43,
		.fmeter_id = FREQ_METER_ABIST_AD_OSC_CK_2,
	},
};
#endif /* ULPOSC_CALI_BY_AP */

void scp_slp_ipi_init(void)
{
	int ret;

	ret = mtk_ipi_register(&scp_ipidev, IPI_OUT_C_SLEEP_0,
			NULL, NULL, &slp_ipi_ackdata0);
	if (ret)
		pr_err("scp0 sleep ipi_register fail, ret %d\n", ret);

	ret = mtk_ipi_register(&scp_ipidev, IPI_OUT_C_SLEEP_1,
			NULL, NULL, &slp_ipi_ackdata1);
	if (ret)
		pr_err("scp1 sleep ipi_register fail, ret %d\n", ret);

	slp_ipi_init_done = 1;
}

static uint32_t _mt_scp_dvfs_set_test_freq(uint32_t sum)
{
	uint32_t added_freq = 0;

	if (scp_dvfs_debug_flag == -1)
		return 0;

	pr_info("manually set opp = %d\n", scp_dvfs_debug_flag);

	/*
	 * calculate test feature freq to meet fixed opp level.
	 */
	if (scp_dvfs_debug_flag == 0 && sum < CLK_OPP0)
		added_freq = CLK_OPP0 - sum;
	else if (scp_dvfs_debug_flag == 1 && sum < CLK_OPP1)
		added_freq = CLK_OPP1 - sum;
	else if (scp_dvfs_debug_flag == 2 && sum < CLK_OPP2)
		added_freq = CLK_OPP2 - sum;
	else if (scp_dvfs_debug_flag == 3 && sum < CLK_OPP3)
		added_freq = CLK_OPP3 - sum;
	else if (scp_dvfs_debug_flag == 4 && sum < CLK_OPP4)
		added_freq = CLK_OPP4 - sum;

	feature_table[VCORE_TEST_FEATURE_ID].freq =
		added_freq;
	pr_info("request freq: %d + %d = %d (MHz)\n",
			sum,
			added_freq,
			sum + added_freq);

	return added_freq;
}

int scp_resource_req(unsigned int req_type)
{
	unsigned long ret = 0;

#if SCP_ATF_RESOURCE_REQUEST
	if (req_type < 0 || req_type >= SCP_REQ_MAX)
		return ret;

	ret = mt_secure_call(MTK_SIP_KERNEL_SCP_DVFS_CTRL,
			req_type,
			0, 0, 0);
	if (!ret)
		scp_resrc_current_req = req_type;
#endif
	return ret;
}

#if 0
int __attribute__((weak))
get_vcore_uv_table(unsigned int vcore_opp)
{
	pr_err("ERROR: %s is not buildin by VCORE DVFS\n", __func__);
	return 0;
}
#endif

int scp_set_pmic_vcore(unsigned int cur_freq)
{
	int ret = 0;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	unsigned int ret_vc = 0;
	int get_vcore_val = 0;

#if !defined(CONFIG_MACH_MT6893)
	if (cur_freq == CLK_OPP0) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_3);
	} else if (cur_freq == CLK_OPP1) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_2);
	} else if (cur_freq == CLK_OPP2) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_1);
	}  else if (cur_freq == CLK_OPP3 || cur_freq == CLK_OPP4) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_0);
	}
#else
	if (cur_freq == CLK_OPP0) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_4);
	} else if (cur_freq == CLK_OPP1) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_3);
	} else if (cur_freq == CLK_OPP2) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_2);
	}  else if (cur_freq == CLK_OPP3 || cur_freq == CLK_OPP4) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_1);
	}
#endif
	else {
		ret = -2;
		pr_err("ERROR: %s: cur_freq=%d is not supported\n",
			__func__, cur_freq);
		WARN_ON(1);
	}

	if (get_vcore_val != 0) {
		pr_debug("get_vcore_val = %d\n", get_vcore_val);
		ret_vc = pmic_scp_set_vcore(get_vcore_val);
	} else {
		pr_err("ERROR: %s: get_vcore_uv_table(%d) fail\n",
			__func__, cur_freq);
		WARN_ON(1);
	}

	if (ret_vc) {
		ret = -1;
		pr_err("ERROR: %s: scp vcore setting error, (%d)\n",
					__func__, ret_vc);
		WARN_ON(1);
	}

#if SCP_VOW_LOW_POWER_MODE
	if (cur_freq == CLK_OPP0 || cur_freq == CLK_OPP1) {
		/* enable VOW low power mode */
		pmic_buck_vgpu11_lp(SRCLKEN11, 0, 1, HW_LP);
	} else {
		/* disable VOW low power mode */
		pmic_buck_vgpu11_lp(SRCLKEN11, 0, 1, HW_OFF);
	}
#endif

#endif /* CONFIG_FPGA_EARLY_PORTING */

	return ret;
}

uint32_t scp_get_freq(void)
{
	uint32_t i;
	uint32_t sum_core0 = 0;
	uint32_t sum_core1 = 0;
	uint32_t return_freq = 0;
	uint32_t sum = 0;

	/*
	 * calculate scp frequence
	 */
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (i != VCORE_TEST_FEATURE_ID &&
				feature_table[i].enable == 1) {
			if (feature_table[i].sys_id == SCPSYS_CORE0)
				sum_core0 += feature_table[i].freq;
			else
				sum_core1 += feature_table[i].freq;
		}
	}

	/*
	 * calculate scp sensor frequence
	 */
	for (i = 0; i < NUM_SENSOR_TYPE; i++) {
		if (sensor_type_table[i].enable == 1)
			sum += sensor_type_table[i].freq;
	}

	sum += sum_core0;
	feature_table[VCORE_TEST_FEATURE_ID].sys_id = SCPSYS_CORE0;
	if (sum_core1 > sum) {
		sum = sum_core1;
		feature_table[VCORE_TEST_FEATURE_ID].sys_id = SCPSYS_CORE1;
	}
	/*
	 * added up scp test cmd frequence
	 */
	sum += _mt_scp_dvfs_set_test_freq(sum);

	/*pr_debug("[SCP] needed freq sum:%d\n",sum);*/
	if (sum <= CLK_OPP0)
		return_freq = CLK_OPP0;
	else if (sum <= CLK_OPP1)
		return_freq = CLK_OPP1;
	else if (sum <= CLK_OPP2)
		return_freq = CLK_OPP2;
	else if (sum <= CLK_OPP3)
		return_freq = CLK_OPP3;
	else if (sum <= CLK_OPP4)
		return_freq = CLK_OPP4;
	else {
		return_freq = CLK_OPP4;
		pr_debug("warning: request freq %d > max opp %d\n",
				sum, CLK_OPP4);
	}

	return return_freq;
}

void scp_vcore_request(unsigned int clk_opp)
{
	pr_debug("%s(%d)\n", __func__, clk_opp);

	/* Set PMIC */
	scp_set_pmic_vcore(clk_opp);

#if SCP_VCORE_REQ_TO_DVFSRC
	/* DVFSRC_VCORE_REQUEST [31:30]
	 * 2'b00: scp request 0.575v
	 * 2'b01: scp request 0.6v
	 * 2'b10: scp request 0.65v
	 * 2'b11: scp request 0.725v
	 */
	if (clk_opp == CLK_OPP0)
		pm_qos_update_request(&dvfsrc_scp_vcore_req, 0x0);
	else if (clk_opp == CLK_OPP1)
		pm_qos_update_request(&dvfsrc_scp_vcore_req, 0x1);
	else if (clk_opp == CLK_OPP2)
		pm_qos_update_request(&dvfsrc_scp_vcore_req, 0x2);
	else
		pm_qos_update_request(&dvfsrc_scp_vcore_req, 0x3);
#endif

#if !defined(CONFIG_MACH_MT6893)
	if (clk_opp == CLK_OPP0)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0008);
	else if (clk_opp == CLK_OPP1)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0104);
	else if (clk_opp == CLK_OPP2)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0202);
	else
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0301);
#else
	if (clk_opp == CLK_OPP0)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0010);
	else if (clk_opp == CLK_OPP1)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0108);
	else if (clk_opp == CLK_OPP2)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0204);
	else
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x0302);
#endif
}

/* scp_request_freq
 * return :-1 means the scp request freq. error
 * return :0  means the request freq. finished
 */
int scp_request_freq(void)
{
	int value = 0;
	int timeout = 250;
	int ret = 0;
	unsigned long spin_flags;
	int is_increasing_freq = 0;

	pr_debug("%s()\n", __func__);

	if (scp_dvfs_flag != 1) {
		pr_debug("warning: SCP DVFS is OFF\n");
		return 0;
	}

	/* because we are waiting for scp to update register:scp_current_freq
	 * use wake lock to prevent AP from entering suspend state
	 */
	__pm_stay_awake(&scp_suspend_lock);

	if (scp_current_freq != scp_expected_freq) {
		/* keep scp alive before raise vcore up */
		scp_awake_lock((void *)SCP_A_ID);

		/* do DVS before DFS if increasing frequency */
		if (scp_current_freq < scp_expected_freq
				|| scp_current_freq == CLK_UNINIT) {
			scp_vcore_request(scp_expected_freq);
			is_increasing_freq = 1;
		}

		/* Request SPM not to turn off mainpll/26M/infra */
		/* because SCP may park in it during DFS process */
		scp_resource_req(SCP_REQ_26M | SCP_REQ_IFR | SCP_REQ_SYSPLL1);

		/*  turn on PLL if necessary */
		scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);
		value = scp_expected_freq;

		do {
			ret = mtk_ipi_send(&scp_ipidev,
					IPI_OUT_DVFS_SET_FREQ_0,
					IPI_SEND_WAIT, &value,
					PIN_OUT_SIZE_DVFS_SET_FREQ_0, 0);
			if (ret != IPI_ACTION_DONE)
				pr_debug("SCP send IPI fail - %d\n", ret);

			mdelay(2);
			timeout -= 1; /*try 50 times, total about 100ms*/
			if (timeout <= 0) {
				pr_err("set freq fail, current(%d) != expect(%d)\n",
					scp_current_freq, scp_expected_freq);
				goto fail;
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

		/* release scp to sleep after ap freq drop request */
		scp_awake_unlock((void *)SCP_A_ID);

		if (scp_expected_freq == (unsigned int)CLK_OPP4)
			/* request SPM not to turn off 26M/infra */
			scp_resource_req(SCP_REQ_26M | SCP_REQ_IFR);
		else
			scp_resource_req(SCP_REQ_RELEASE);
	}

	__pm_relax(&scp_suspend_lock);
	pr_debug("[SCP] succeed to set freq, expect=%d, cur=%d\n",
			scp_expected_freq, scp_current_freq);
	return 0;

fail:
	/* release scp to sleep after ap freq drop request */
	scp_awake_unlock((void *)SCP_A_ID);

	/* relax AP to allow entering suspend after ap freq drop request */
	__pm_relax(&scp_suspend_lock);

	WARN_ON(1);

	return -1;
}

void wait_scp_dvfs_init_done(void)
{
	int count = 0;

	while (g_scp_dvfs_init_flag != 1) {
		mdelay(1);
		count++;
		if (count > 3000) {
			pr_err("SCP dvfs driver init fail\n");
			WARN_ON(1);
		}
	}
}

void scp_pll_mux_set(unsigned int pll_ctrl_flag)
{
	int ret = 0;

	pr_debug("%s(%d)\n\n", __func__, pll_ctrl_flag);

	if (pll_ctrl_flag == PLL_ENABLE) {
		ret = clk_prepare_enable(mt_scp_pll->clk_mux);
		if (ret) {
			pr_err("scp dvfs cannot enable clk mux, %d\n", ret);
			WARN_ON(1);
		}
	} else
		clk_disable_unprepare(mt_scp_pll->clk_mux);
}

int scp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel)
{
	int ret = 0;

	pr_debug("%s(%d, %d)\n", __func__, pll_ctrl_flag, pll_sel);

	if (pll_ctrl_flag == PLL_ENABLE) {
		if (pre_pll_sel != CLK_OPP4) {
			ret = clk_prepare_enable(mt_scp_pll->clk_mux);
			if (ret) {
				pr_err("clk_prepare_enable() failed\n");
				WARN_ON(1);
			}
		} else {
			pr_debug("no need to do clk_prepare_enable()\n");
		}

		switch (pll_sel) {
		case CLK_26M:		/* 26 MHz */
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll0);
			break;
		case CLK_OPP0:		/* 182, MAINPLL */
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll2);
			break;
		case CLK_OPP1:		/* 273M, MAINPLL */
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll5);
			break;
		case CLK_OPP2:		/* 312 MHz, MAINPLL */
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll7);
			break;
		case CLK_OPP3:		/* 364 MHz, MAINPLL */
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll3);
			break;
		case CLK_OPP4:		/* 416 MHz, UNIVPLL */
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll4);
			break;
		default:
			pr_err("not support opp freq %d\n", pll_sel);
			WARN_ON(1);
			break;
		}

		if (ret) {
			pr_debug("clk_set_parent() failed, opp=%d\n",
					pll_sel);
			WARN_ON(1);
		}

		if (pre_pll_sel != pll_sel)
			pre_pll_sel = pll_sel;

	} else if (pll_ctrl_flag == PLL_DISABLE
				&& pll_sel != CLK_OPP4) {
		clk_disable_unprepare(mt_scp_pll->clk_mux);
		pr_debug("clk_disable_unprepare()\n");
	} else {
		pr_debug("no need to do clk_disable_unprepare\n");
	}

	return ret;
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
	unsigned int scp_state;

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
	return 0;
}

/****************************
 * show scp dvfs ctrl
 *****************************/
static int mt_scp_dvfs_ctrl_proc_show(struct seq_file *m, void *v)
{
	unsigned long spin_flags;
	int i;

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_current_freq = readl(CURRENT_FREQ_REG);
	scp_expected_freq = readl(EXPECTED_FREQ_REG);
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);
	seq_printf(m, "SCP DVFS: %s\n", (scp_dvfs_flag == 1)?"ON":"OFF");
	seq_printf(m, "SCP frequency: cur=%dMHz, expect=%dMHz\n",
				scp_current_freq, scp_expected_freq);

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
				pr_info("remove the opp setting of command\n");

				feature_table[VCORE_TEST_FEATURE_ID].freq = 0;

				scp_deregister_feature(VCORE_TEST_FEATURE_ID);

				scp_dvfs_debug_flag = dvfs_opp;
			} else if (dvfs_opp >= 0 && dvfs_opp <= 4) {
				scp_dvfs_debug_flag = dvfs_opp;

				scp_register_feature(VCORE_TEST_FEATURE_ID);
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

/****************************
 * show scp sleep ctrl0
 *****************************/
static int mt_scp_sleep_ctrl0_proc_show(struct seq_file *m, void *v)
{
	int ret;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	ipi_data.arg1 = SLP_DBG_CMD_GET_FLAG;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	if (ret != IPI_ACTION_DONE)
		seq_printf(m, "ipi fail, ret = %d\n", ret);
	else {
		if (slp_ipi_ackdata0 >= SCP_SLEEP_OFF &&
			slp_ipi_ackdata0 <= SLP_DBG_CMD_SET_NO_CONDITION)
			seq_printf(m, "SCP Sleep flag = %d\n",
				slp_ipi_ackdata0);
		else
			seq_printf(m, "invalid SCP Sleep flag = %d\n",
				slp_ipi_ackdata0);
	}

	return 0;
}

/**********************************
 * write scp sleep ctrl0
 ***********************************/
static ssize_t mt_scp_sleep_ctrl0_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64];
	unsigned int val = 0;
	unsigned int len = 0;
	int ret = 0;
	struct ipi_tx_data_t ipi_data;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	if (kstrtouint(desc, 10, &val) == 0) {
		if (val >= SCP_SLEEP_OFF &&
			val <= SCP_SLEEP_NO_CONDITION) {
			ipi_data.arg1 = val;
			ret = mtk_ipi_send_compl(&scp_ipidev,
						IPI_OUT_C_SLEEP_0,
						IPI_SEND_WAIT,
						&ipi_data,
						PIN_OUT_C_SIZE_SLEEP_0,
						500);
			if (ret)
				pr_err("%s: mtk_ipi_send_compl fail, ret=%d\n",
					__func__, ret);
		} else {
			pr_info("Warning: invalid input value %d\n", val);
		}
	} else {
		pr_info("Warning: invalid input command, val=%d\n", val);
	}

	return count;
}

/****************************
 * show scp sleep ctrl1
 *****************************/
static int mt_scp_sleep_ctrl1_proc_show(struct seq_file *m, void *v)
{
	int ret;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	ipi_data.arg1 = SLP_DBG_CMD_GET_FLAG;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
	if (ret != IPI_ACTION_DONE)
		seq_printf(m, "ipi fail, ret = %d\n", ret);
	else {
		if (slp_ipi_ackdata1 >= SCP_SLEEP_OFF &&
			slp_ipi_ackdata1 <= SLP_DBG_CMD_SET_NO_CONDITION)
			seq_printf(m, "SCP Sleep flag = %d\n",
				slp_ipi_ackdata1);
		else
			seq_printf(m, "invalid SCP Sleep flag = %d\n",
				slp_ipi_ackdata1);
	}

	return 0;
}

/**********************************
 * write scp sleep ctrl1
 ***********************************/
static ssize_t mt_scp_sleep_ctrl1_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64];
	unsigned int val = 0;
	unsigned int len = 0;
	int ret = 0;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 10, &val) == 0) {
		if (val >= SCP_SLEEP_OFF &&
			val <= SCP_SLEEP_NO_CONDITION) {
			ipi_data.arg1 = val;
			ret = mtk_ipi_send_compl(&scp_ipidev,
						IPI_OUT_C_SLEEP_1,
						IPI_SEND_WAIT,
						&ipi_data,
						PIN_OUT_C_SIZE_SLEEP_1,
						500);
			if (ret != IPI_ACTION_DONE)
				pr_err("%s: mtk_ipi_send_compl fail, ret=%d\n",
					__func__, ret);
		} else {
			pr_info("Warning: invalid input value %d\n", val);
		}
	} else {
		pr_info("Warning: invalid input command, val=%d\n", val);
	}

	return count;
}

/****************************
 * show scp sleep cnt0
 *****************************/
static int mt_scp_sleep_cnt0_proc_show(struct seq_file *m, void *v)
{
	int ret;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	ipi_data.arg1 = SLP_DBG_CMD_GET_CNT;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	if (ret != IPI_ACTION_DONE)
		seq_printf(m, "ipi fail, ret = %d\n", ret);
	else
		seq_printf(m, "scp_sleep_cnt = %d\n", slp_ipi_ackdata0);

	return 0;
}

/**********************************
 * write scp sleep cnt0
 ***********************************/
static ssize_t mt_scp_sleep_cnt0_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64];
	unsigned int val = 0;
	unsigned int len = 0;
	int ret = 0;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 10, &val) == 0) {
		ipi_data.arg1 = SLP_DBG_CMD_RESET;
		ret = mtk_ipi_send_compl(&scp_ipidev,
					IPI_OUT_C_SLEEP_0,
					IPI_SEND_WAIT,
					&ipi_data,
					PIN_OUT_C_SIZE_SLEEP_0,
					500);
		if (ret != IPI_ACTION_DONE)
			pr_err("%s: mtk_ipi_send_compl fail, ret=%d\n",
				__func__, ret);
	} else {
		pr_info("Warning: invalid input command, val=%d\n", val);
	}

	return count;
}

/****************************
 * show scp sleep cnt1
 *****************************/
static int mt_scp_sleep_cnt1_proc_show(struct seq_file *m, void *v)
{
	int ret;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	ipi_data.arg1 = SLP_DBG_CMD_GET_CNT;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
	if (ret != IPI_ACTION_DONE)
		seq_printf(m, "ipi fail, ret = %d\n", ret);
	else
		seq_printf(m, "scp_sleep_cnt = %d\n", slp_ipi_ackdata1);

	return 0;
}

/**********************************
 * write scp sleep cnt1
 ***********************************/
static ssize_t mt_scp_sleep_cnt1_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64];
	unsigned int val = 0;
	unsigned int len = 0;
	int ret = 0;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 10, &val) == 0) {
		ipi_data.arg1 = SLP_DBG_CMD_RESET;
		ret = mtk_ipi_send_compl(&scp_ipidev,
					IPI_OUT_C_SLEEP_1,
					IPI_SEND_WAIT,
					&ipi_data,
					PIN_OUT_C_SIZE_SLEEP_1,
					500);
		if (ret != IPI_ACTION_DONE)
			pr_err("%s: mtk_ipi_send_compl fail, ret=%d\n",
				__func__, ret);
	} else {
		pr_info("Warning: invalid input command, val=%d\n", val);
	}

	return count;
}

/****************************
 * show scp dvfs request
 *****************************/
static int mt_scp_resrc_req_proc_show(struct seq_file *m, void *v)
{
	if (scp_resrc_req_cmd == -1)
		seq_puts(m, "SCP Req CMD is not configured yet.\n");
	else if (scp_resrc_req_cmd == SCP_REQ_RELEASE)
		seq_puts(m, "SCP Req CMD release\n");
	else {
		if ((scp_resrc_req_cmd & SCP_REQ_26M) == SCP_REQ_26M)
			seq_puts(m, "SCP Req CMD: 26M on\n");
		if ((scp_resrc_req_cmd & SCP_REQ_IFR) == SCP_REQ_IFR)
			seq_puts(m, "SCP Req CMD: infra bus on\n");
		if ((scp_resrc_req_cmd & SCP_REQ_SYSPLL1) == SCP_REQ_SYSPLL1)
			seq_puts(m, "SCP Req CMD: univpll on\n");
	}

	seq_printf(m, "scp current req: %d\n", scp_resrc_current_req);

	return 0;
}

/**********************************
 * write scp dvfs request
 ***********************************/
static ssize_t mt_scp_resrc_req_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], cmd[8];
	int req_opp = 0;
	int len = 0;
	int ret = 0;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%7s %d", cmd, &req_opp) == 2) {
		if (strcmp(cmd, "req_on")) {
			pr_info("invalid command %s\n", cmd);
			return count;
		}

		if (req_opp >= 0  && req_opp < SCP_REQ_MAX) {
			if (req_opp != scp_resrc_req_cmd) {
				pr_info("scp_resrc_req_cmd = %d\n",
						req_opp);

				ret = scp_resource_req(req_opp);
				if (ret)
					pr_err("%s: SCP send req fail - %d\n",
						__func__, ret);
				else
					scp_resrc_req_cmd = req_opp;
			} else
				pr_info("SCP Req CMD  is not changed\n");
		} else {
			pr_info("Warning: invalid input value %d\n", req_opp);
		}
	} else {
		pr_info("Warning: invalid input number\n");
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
static const struct file_operations \
	mt_ ## name ## _proc_fops = {\
	.owner		= THIS_MODULE, \
	.open		= mt_ ## name ## _proc_open, \
	.read		= seq_read, \
	.llseek		= seq_lseek, \
	.release	= single_release, \
	.write		= mt_ ## name ## _proc_write, \
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
static const struct file_operations mt_ ## name ## _proc_fops = {\
	.owner		= THIS_MODULE,\
	.open		= mt_ ## name ## _proc_open,\
	.read		= seq_read,\
	.llseek		= seq_lseek,\
	.release	= single_release,\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RO(scp_dvfs_state);
PROC_FOPS_RW(scp_dvfs_ctrl);
PROC_FOPS_RW(scp_sleep_ctrl0);
PROC_FOPS_RW(scp_sleep_ctrl1);
PROC_FOPS_RW(scp_sleep_cnt0);
PROC_FOPS_RW(scp_sleep_cnt1);
PROC_FOPS_RW(scp_resrc_req);

static int mt_scp_dvfs_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(scp_dvfs_state),
		PROC_ENTRY(scp_dvfs_ctrl),
		PROC_ENTRY(scp_sleep_ctrl0),
		PROC_ENTRY(scp_sleep_ctrl1),
		PROC_ENTRY(scp_sleep_cnt0),
		PROC_ENTRY(scp_sleep_cnt1),
		PROC_ENTRY(scp_resrc_req),
	};

	dir = proc_mkdir("scp_dvfs", NULL);
	if (!dir) {
		pr_err("fail to create /proc/scp_dvfs @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
						0664,
						dir,
						entries[i].fops)) {
			pr_err("ERROR: %s: create /proc/scp_dvfs/%s failed\n",
						__func__, entries[i].name);
			ret = -ENOMEM;
		}
	}

	return ret;
}
#endif /* CONFIG_PROC_FS */

static const struct of_device_id scpdvfs_of_ids[] = {
	{.compatible = "mediatek,scp_dvfs"},
	{}
};

static int mt_scp_dvfs_suspend(struct device *dev)
{
	return 0;
}

static int mt_scp_dvfs_resume(struct device *dev)
{
	return 0;
}

static int mt_scp_dvfs_pm_restore_early(struct device *dev)
{
	return 0;
}

#ifdef ULPOSC_CALI_BY_AP
static void turn_onoff_clk_high(int id, int is_on)
{
	pr_debug("%s(%d, %d)\n", __func__, id, is_on);

	if (is_on) {
		/* turn on ulposc */
		DRV_SetReg32(CLK_ENABLE, (1 << CLK_HIGH_EN_BIT));
		if (id == ULPOSC_2)
			DRV_ClrReg32(CLK_ON_CTRL, (1 << HIGH_CORE_DIS_SUB_BIT));

		/* wait settle time */
		udelay(150);

		/* turn on CG */
		if (id == ULPOSC_2)
			DRV_SetReg32(CLK_HIGH_CORE, (1 << HIGH_CORE_CG_BIT));
		else
			DRV_SetReg32(CLK_ENABLE, (1 << CLK_HIGH_CG_BIT));
	} else {
		/* turn off CG */
		if (id == ULPOSC_2)
			DRV_ClrReg32(CLK_HIGH_CORE, (1 << HIGH_CORE_CG_BIT));
		else
			DRV_ClrReg32(CLK_ENABLE, (1 << CLK_HIGH_CG_BIT));

		udelay(50);

		/* turn off ULPOSC */
		if (id == ULPOSC_2)
			DRV_SetReg32(CLK_ON_CTRL, (1 << HIGH_CORE_DIS_SUB_BIT));
		else
			DRV_ClrReg32(CLK_ENABLE, (1 << CLK_HIGH_EN_BIT));
	}

	udelay(50);
}

static void set_ulposc_cali_value(unsigned int cali_val)
{
	unsigned int val;

	val = DRV_Reg32(ULPOSC2_CON0) & ~(RG_OSC_CALI_MSK << RG_OSC_CALI_SHFT);
	val = (val | ((cali_val & RG_OSC_CALI_MSK) << RG_OSC_CALI_SHFT));
	DRV_WriteReg32(ULPOSC2_CON0, val);

	udelay(50);
}

static unsigned int ulposc_cali_process(unsigned int idx)
{
	unsigned int target_val = 0, current_val = 0;
	unsigned int min = CAL_MIN_VAL, max = CAL_MAX_VAL, middle;
	unsigned int diff_by_min = 0, diff_by_max = 0xffff;
	unsigned int cal_result = 0;

	target_val = ulposc_cfg[idx].freq * 1000;

	do {
		middle = (min + max) / 2;
		if (middle == min) {
			pr_debug("middle(%d) == min(%d)\n", middle, min);
			break;
		}

		set_ulposc_cali_value(middle);
		current_val = mt_get_abist_freq(ulposc_cfg[idx].fmeter_id);

		if (current_val > target_val)
			max = middle;
		else
			min = middle;
	} while (min <= max);

	set_ulposc_cali_value(min);
	current_val = mt_get_abist_freq(ulposc_cfg[idx].fmeter_id);
	if (current_val > target_val)
		diff_by_min = current_val - target_val;
	else
		diff_by_min = target_val - current_val;

	set_ulposc_cali_value(max);
	current_val = mt_get_abist_freq(ulposc_cfg[idx].fmeter_id);
	if (current_val > target_val)
		diff_by_max = current_val - target_val;
	else
		diff_by_max = target_val - current_val;

	if (diff_by_min < diff_by_max)
		cal_result = min;
	else
		cal_result = max;

	set_ulposc_cali_value(cal_result);
	current_val = mt_get_abist_freq(ulposc_cfg[idx].fmeter_id);

	/* check if calibrated value is in the range of target value +- 4% */
	if ((current_val < (target_val * (100 - CAL_MIS_RATE) / 100)) ||
		(current_val > (target_val * (100 + CAL_MIS_RATE) / 100))) {
		pr_err("calibration fail, target=%dMHz, calibrated=%dMHz\n",
				target_val/1000, current_val/1000);
		return 0;
	}

	pr_info("calibration done, target=%dMHz, calibrated=%dMHz\n",
				target_val/1000, current_val/1000);

	return cal_result;
}

void ulposc_cali_init(void)
{
	struct device_node *node;
	unsigned int i;

	pr_info("%s\n", __func__);

	/* get ULPOSC base address */
	node = of_find_compatible_node(NULL, NULL,
			"mediatek,apmixed");
	if (!node) {
		pr_err("error: can't find apmixedsys node\n");
		WARN_ON(1);
		return;
	}

	ulposc_base = of_iomap(node, 0);
	if (!ulposc_base) {
		pr_err("error: iomap fail for ulposc_base\n");
		WARN_ON(1);
		return;
	}

	for (i = 0; i < MAX_ULPOSC_CALI_NUM; i++) {
		/* turn off ULPOSC2 */
		turn_onoff_clk_high(ULPOSC_2, 0);

		/* init ULPOSC RGs */
		DRV_WriteReg32(ULPOSC2_CON0, ulposc_cfg[i].ulposc_rg0);
		DRV_WriteReg32(ULPOSC2_CON1, ulposc_cfg[i].ulposc_rg1);
		DRV_WriteReg32(ULPOSC2_CON2, ulposc_cfg[i].ulposc_rg2);

		/* turn on ULPOSC2 */
		turn_onoff_clk_high(ULPOSC_2, 1);

		pr_debug("ULPOSC2: CON0=0x%x, CON1=0x%x, CON2=0x%x\n",
			DRV_Reg32(ULPOSC2_CON0),
			DRV_Reg32(ULPOSC2_CON1),
			DRV_Reg32(ULPOSC2_CON2));

		ulposc_cfg[i].cali_val = (unsigned short)ulposc_cali_process(i);
		if (!ulposc_cfg[i].cali_val) {
			pr_err("Error: calibrate ULPOSC2 to %dM fail\n",
					ulposc_cfg[i].freq);
			break;
		}
	}

	/* turn off ULPOSC2 */
	turn_onoff_clk_high(ULPOSC_2, 0);
}

void sync_ulposc_cali_data_to_scp(void)
{
	int i, ret;
	unsigned int ipi_data[2];
	unsigned short *ptrTmp = (unsigned short *)&ipi_data[1];

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	ipi_data[0] = SLP_DBG_CMD_ULPOSC_CALI_VAL;

	for (i = 0; i < MAX_ULPOSC_CALI_NUM; i++) {
		*ptrTmp = ulposc_cfg[i].freq;
		*(ptrTmp+1) = ulposc_cfg[i].cali_val;

		pr_info("ipi to scp: freq=%d, cali_val=0x%x\n",
			ulposc_cfg[i].freq, ulposc_cfg[i].cali_val);

		ret = mtk_ipi_send_compl(&scp_ipidev,
					IPI_OUT_C_SLEEP_0,
					IPI_SEND_WAIT,
					&ipi_data[0],
					PIN_OUT_C_SIZE_SLEEP_0,
					500);
		if (ret != IPI_ACTION_DONE) {
			pr_err("mtk_ipi_send_compl ULPOSC2_CALI_VAL(%d,%d) fail\n",
					ulposc_cfg[i].freq,
					ulposc_cfg[i].cali_val);
			WARN_ON(1);
		}
	}

	/* check if SCP clock is switched to ULPOSC */
	if ((((DRV_Reg32(CLK_SW_SEL)>>CLK_SW_SEL_O_BIT) & CLK_SW_SEL_O_MASK) &
		 (CLK_SW_SEL_O_ULPOSC_CORE | CLK_SW_SEL_O_ULPOSC_PERI)) == 0) {
		pr_err("Error: SCP clock is not switched to ULPOSC, CLK_SW_SEL=0x%x\n",
			DRV_Reg32(CLK_SW_SEL));
		WARN_ON(1);
	}
}
#endif /* ULPOSC_CALI_BY_AP */

static int mt_scp_dvfs_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int gpio_mode;

	pr_notice("%s()\n", __func__);

	node = of_find_matching_node(NULL, scpdvfs_of_ids);
	if (!node) {
		dev_notice(&pdev->dev, "fail to find SCPDVFS node\n");
		WARN_ON(1);
	}

	mt_scp_pll = kzalloc(sizeof(struct mt_scp_pll_t), GFP_KERNEL);
	if (mt_scp_pll == NULL)
		return -ENOMEM;

	mt_scp_pll->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(mt_scp_pll->clk_mux)) {
		dev_notice(&pdev->dev, "cannot get clock mux\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_mux);
	}

	mt_scp_pll->clk_pll0 = devm_clk_get(&pdev->dev, "clk_pll_0");
	if (IS_ERR(mt_scp_pll->clk_pll0)) {
		dev_notice(&pdev->dev, "cannot get 1st clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll0);
	}
	mt_scp_pll->clk_pll1 = devm_clk_get(&pdev->dev, "clk_pll_1");
	if (IS_ERR(mt_scp_pll->clk_pll1)) {
		dev_notice(&pdev->dev, "cannot get 2nd clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll1);
	}
	mt_scp_pll->clk_pll2 = devm_clk_get(&pdev->dev, "clk_pll_2");
	if (IS_ERR(mt_scp_pll->clk_pll2)) {
		dev_notice(&pdev->dev, "cannot get 3rd clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll2);
	}
	mt_scp_pll->clk_pll3 = devm_clk_get(&pdev->dev, "clk_pll_3");
	if (IS_ERR(mt_scp_pll->clk_pll3)) {
		dev_notice(&pdev->dev, "cannot get 4th clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll3);
	}
	mt_scp_pll->clk_pll4 = devm_clk_get(&pdev->dev, "clk_pll_4");
	if (IS_ERR(mt_scp_pll->clk_pll4)) {
		dev_notice(&pdev->dev, "cannot get 5th clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll4);
	}
	mt_scp_pll->clk_pll5 = devm_clk_get(&pdev->dev, "clk_pll_5");
	if (IS_ERR(mt_scp_pll->clk_pll5)) {
		dev_notice(&pdev->dev, "cannot get 6th clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll5);
	}
	mt_scp_pll->clk_pll6 = devm_clk_get(&pdev->dev, "clk_pll_6");
	if (IS_ERR(mt_scp_pll->clk_pll6)) {
		dev_notice(&pdev->dev, "cannot get 7th clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll6);
	}
	mt_scp_pll->clk_pll7 = devm_clk_get(&pdev->dev, "clk_pll_7");
	if (IS_ERR(mt_scp_pll->clk_pll7)) {
		dev_notice(&pdev->dev, "cannot get 8th clock parent\n");
	    WARN_ON(1);
		return PTR_ERR(mt_scp_pll->clk_pll7);
	}

	/* get GPIO base address */
	node = of_find_compatible_node(NULL, NULL, "mediatek,gpio");
	if (!node) {
		pr_err("error: can't find GPIO node\n");
		WARN_ON(1);
		return 0;
	}

	gpio_base = of_iomap(node, 0);
	if (!gpio_base) {
		pr_err("error: iomap fail for GPIO\n");
		WARN_ON(1);
		return 0;
	}

	/* check if GPIO is configured correctly for SCP VREQ */
	gpio_mode = (DRV_Reg32(ADR_GPIO_MODE_OF_SCP_VREQ) >>
				 BIT_GPIO_MODE_OF_SCP_VREQ) &
				 MSK_GPIO_MODE_OF_SCP_VREQ;
	if (gpio_mode == 1)
		pr_debug("v_req muxpin setting is correct\n");
	else {
		pr_err("wrong V_REQ muxpin setting - %d\n", gpio_mode);
	    WARN_ON(1);
	}

	g_scp_dvfs_init_flag = 1;

	return 0;
}

/***************************************
 * this function should never be called
 ****************************************/
static int mt_scp_dvfs_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct dev_pm_ops mt_scp_dvfs_pm_ops = {
	.suspend = mt_scp_dvfs_suspend,
	.resume = mt_scp_dvfs_resume,
	.restore_early = mt_scp_dvfs_pm_restore_early,
};

static struct platform_driver mt_scp_dvfs_pdrv = {
	.probe = mt_scp_dvfs_pdrv_probe,
	.remove = mt_scp_dvfs_pdrv_remove,
	.driver = {
		.name = "scp_dvfs",
		.pm = &mt_scp_dvfs_pm_ops,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = scpdvfs_of_ids,
#endif
		},
};

/**********************************
 * mediatek scp dvfs initialization
 ***********************************/
void mt_pmic_sshub_init(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)

	/* set SCP VCORE voltage */
	if (pmic_scp_set_vcore(575000) != 0)
		pr_notice("Set wrong vcore voltage\n");

#if SCP_VOW_LOW_POWER_MODE
	/* enable VOW low power mode */
	pmic_buck_vgpu11_lp(SRCLKEN11, 0, 1, HW_LP);
#else
	/* disable VOW low power mode */
	pmic_buck_vgpu11_lp(SRCLKEN11, 0, 1, HW_OFF);
#endif

	/* BUCK_VCORE_SSHUB_EN: ON */
	/* LDO_VSRAM_OTHERS_SSHUB_EN: OFF */
	/* pmrc_mode: OFF */
	pmic_scp_ctrl_enable(true, false, false);

#endif /* CONFIG_FPGA_EARLY_PORTING */
}

#ifdef CONFIG_PM
static int mt_scp_dump_sleep_count(void)
{
	int ret;
	struct ipi_tx_data_t ipi_data;

	if (!slp_ipi_init_done)
		scp_slp_ipi_init();

	ipi_data.arg1 = SLP_DBG_CMD_GET_CNT;

	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	if (ret != IPI_ACTION_DONE)
		printk_deferred("[name:scp&][%s:%d] - scp ipi fail, ret = %d\\n",
		__func__, __LINE__, ret);
	else
		printk_deferred("[name:scp&][%s:%d] - scp_sleep_cnt_0 = %d\n",
		__func__, __LINE__, slp_ipi_ackdata0);

	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
	if (ret != IPI_ACTION_DONE)
		printk_deferred("[name:scp&][%s:%d] - scp ipi fail, ret = %d\\n",
		__func__, __LINE__, ret);
	else
		printk_deferred("[name:scp&][%s:%d] - scp_sleep_cnt_1 = %d\n",
		__func__, __LINE__, slp_ipi_ackdata1);

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
		/* show scp sleep count */
		mt_scp_dump_sleep_count();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}


static struct notifier_block scp_pm_notifier_func = {
	.notifier_call = scp_pm_event,
};
#endif

int __init scp_dvfs_init(void)
{
	int ret = 0;

	pr_notice("%s\n", __func__);

#ifdef CONFIG_PROC_FS
	/* init proc */
	if (mt_scp_dvfs_create_procfs()) {
		pr_err("mt_scp_dvfs_create_procfs fail..\n");
		WARN_ON(1);
		return -1;
	}
#endif /* CONFIG_PROC_FS */

	/* register platform driver */
	ret = platform_driver_register(&mt_scp_dvfs_pdrv);
	if (ret) {
		pr_err("fail to register scp dvfs driver @ %s()\n", __func__);
		WARN_ON(1);
		return -1;
	}

	wakeup_source_init(&scp_suspend_lock, "scp wakelock");

	mt_pmic_sshub_init();

#if SCP_VCORE_REQ_TO_DVFSRC
	pm_qos_add_request(&dvfsrc_scp_vcore_req,
			PM_QOS_SCP_VCORE_REQUEST,
			PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE);
#endif

#ifdef CONFIG_PM
	ret = register_pm_notifier(&scp_pm_notifier_func);
	if (ret) {
		pr_debug("[name:scp&][SCP] Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */

	return ret;
}

void __exit scp_dvfs_exit(void)
{
	platform_driver_unregister(&mt_scp_dvfs_pdrv);
}

