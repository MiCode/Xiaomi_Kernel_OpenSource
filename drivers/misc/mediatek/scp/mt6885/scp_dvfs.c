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

/* -1:SCP DVFS OFF, 1:SCP DVFS ON */
int scp_dvfs_flag = 1;

/*
 * 0: SCP Sleep: OFF,
 * 1: SCP Sleep: ON,
 * 2: SCP Sleep: sleep without wakeup,
 * 3: SCP Sleep: force to sleep
 */
static int scp_sleep_flag = 1;

/*
 * -1: SCP Debug CMD: off,
 * 0-4: SCP DVFS Debug OPP.
 */
static int scp_dvfs_debug_flag = -1;

static int pre_pll_sel = -1;
static struct mt_scp_pll_t *mt_scp_pll;
static struct wakeup_source scp_suspend_lock;
static int g_scp_dvfs_init_flag = -1;

static void __iomem *gpio_base;
#define ADR_GPIO_MODE_OF_SCP_VREQ	(gpio_base + 0x410)
#define BIT_GPIO_MODE_OF_SCP_VREQ	24
#define MSK_GPIO_MODE_OF_SCP_VREQ	0x7

#if SCP_VCORE_REQ_TO_DVFSRC
static struct pm_qos_request dvfsrc_scp_vcore_req;
#endif

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
	ret = mt_secure_call(MTK_SIP_KERNEL_SCP_DVFS_CTRL,
			req_type,
			0, 0, 0);
#endif
	return ret;
}

int __attribute__((weak))
get_vcore_uv_table(int vcore_opp)
{
	pr_err("ERROR: %s is not buildin by VCORE DVFS\n", __func__);
	return 0;
}


int scp_set_pmic_vcore(unsigned int cur_freq)
{
	int ret = 0;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	unsigned int ret_vc = 0;
	int get_vcore_val = 0;

	if (cur_freq == CLK_OPP0) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_3);
		pr_notice("get_vcore_val = %d\n", get_vcore_val);
		if (!get_vcore_val)
			ret_vc = pmic_scp_set_vcore(575000);
		else
			ret_vc = pmic_scp_set_vcore(get_vcore_val);
	} else if (cur_freq == CLK_OPP1) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_2);
		pr_notice("get_vcore_val = %d\n", get_vcore_val);
		if (!get_vcore_val)
			ret_vc = pmic_scp_set_vcore(600000);
		else
			ret_vc = pmic_scp_set_vcore(get_vcore_val);
	} else if (cur_freq == CLK_OPP2) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_1);
		pr_notice("get_vcore_val = %d\n", get_vcore_val);
		if (!get_vcore_val)
			ret_vc = pmic_scp_set_vcore(650000);
		else
			ret_vc = pmic_scp_set_vcore(get_vcore_val);
	}  else if (cur_freq == CLK_OPP3 || cur_freq == CLK_OPP4) {
		get_vcore_val = get_vcore_uv_table(VCORE_OPP_0);
		pr_notice("get_vcore_val = %d\n", get_vcore_val);
		if (!get_vcore_val)
			ret_vc = pmic_scp_set_vcore(725000);
		else
			ret_vc = pmic_scp_set_vcore(get_vcore_val);
	} else {
		ret = -2;
		pr_err("ERROR: %s: cur_freq=%d is not supported\n",
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
	uint32_t return_freq = 0;
	uint32_t sum_core0 = 0;
	uint32_t sum_core1 = 0;
	uint32_t sum = 0;
	uint32_t i;

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

/* TBD */
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

	/* SCP to SPM voltage level 0x100066C4 (scp reg 0xC0094)
	 * 2'b0000_0000_1000: scp request 0.575v
	 * 2'b0001_0000_0100: scp request 0.6v
	 * 2'b0010_0000_0010: scp request 0.65v
	 * 2'b0011_0000_0001: scp request 0.725v
	 */
	if (clk_opp == CLK_OPP0)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x8);
	else if (clk_opp == CLK_OPP1)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x104);
	else if (clk_opp == CLK_OPP2)
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x202);
	else
		DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, 0x301);
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
		scp_resource_req(SCP_REQ_26M | SCP_REQ_IFR);

		/*  turn on PLL if necessary */
		scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);

		do {
			ret = mtk_ipi_send(&scp_ipidev,
					IPI_OUT_DVFS_SET_FREQ_0,
					0, &value,
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
		case CLK_OPP0:		/* 218M, MAINPLL */
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll6);
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
		case CLK_OPP3:		/* 360 MHz, MAINPLL */
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
 * show scp dvfs sleep
 *****************************/
static int mt_scp_dvfs_sleep_proc_show(struct seq_file *m, void *v)
{
	if (scp_sleep_flag == -1)
		seq_puts(m, "SCP sleep is not configured by command yet.\n");
	else if (scp_sleep_flag == 0)
		seq_puts(m, "SCP Sleep: OFF\n");
	else if (scp_sleep_flag == 1)
		seq_puts(m, "SCP Sleep: ON\n");
	else if (scp_sleep_flag == 2)
		seq_puts(m, "SCP Sleep: sleep without wakeup\n");
	else if (scp_sleep_flag == 3)
		seq_puts(m, "SCP Sleep: force to sleep\n");
	else
		seq_puts(m, "Warning: invalid SCP Sleep configure\n");

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
	char desc[64];
	unsigned int val = 0;
	int len = 0;
	int ret = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 10, &val) == 0) {
		if (val >= 0  && val <= 3) {
			if (val != scp_sleep_flag) {
				scp_sleep_flag = val;
				pr_info("scp_sleep_flag = %d\n",
						scp_sleep_flag);

				ret = mtk_ipi_send(&scp_ipidev,
						IPI_OUT_DVFS_SLEEP_0,
						0,
						&scp_sleep_flag,
						PIN_OUT_SIZE_DVFS_SLEEP_0,
						0);
				if (ret != IPI_ACTION_DONE)
					pr_info("%s: SCP send IPI fail - %d\n",
						__func__, ret);
			} else
				pr_info("SCP sleep flag is not changed\n");
		} else {
			pr_info("Warning: invalid input value %d\n", val);
		}
	} else {
		pr_info("Warning: invalid input command, val=%d\n", val);
	}

	return count;
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
	int len = 0;
	int dvfs_opp;
	int n;

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
PROC_FOPS_RW(scp_dvfs_sleep);
PROC_FOPS_RW(scp_dvfs_ctrl);

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
		PROC_ENTRY(scp_dvfs_sleep),
		PROC_ENTRY(scp_dvfs_ctrl)
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

	return ret;
}

void __exit scp_dvfs_exit(void)
{
	platform_driver_unregister(&mt_scp_dvfs_pdrv);
}

