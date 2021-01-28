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
#include "scp_ipi.h"
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
#include "mtk_spm_vcore_dvfs.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)	"[scp_dvfs]: " fmt

#define DRV_Reg32(addr)	readl(addr)
#define DRV_WriteReg32(addr, val) writel(val, addr)
#define DRV_SetReg32(addr, val)	DRV_WriteReg32(addr, DRV_Reg32(addr) | (val))
#define DRV_ClrReg32(addr, val)	DRV_WriteReg32(addr, DRV_Reg32(addr) & ~(val))

void __attribute__((weak)) dvfsrc_set_scp_vcore_request(unsigned int level);

/***************************
 * Operate Point Definition
 ****************************/
#if 0
static struct pinctrl *scp_pctrl; /* static pinctrl instance */

/* DTS state */
enum SCP_DTS_GPIO_STATE {
	SCP_DTS_GPIO_STATE_DEFAULT = 0,
	SCP_DTS_VREQ_OFF,
	SCP_DTS_VREQ_ON,
	SCP_DTS_GPIO_STATE_MAX,	/* for array size */
};

/* DTS state mapping name */
static const char *scp_state_name[SCP_DTS_GPIO_STATE_MAX] = {
	"default",
	"scp_gpio_off",
	"scp_gpio_on"
};
#endif

/* -1:SCP DVFS OFF, 1:SCP DVFS ON */
static int scp_dvfs_flag = 1;

/*
 * 0: SCP Sleep: OFF,
 * 1: SCP Sleep: ON,
 * 2: SCP Sleep: sleep without wakeup,
 * 3: SCP Sleep: force to sleep
 */
static int scp_sleep_flag = -1;

static int mt_scp_dvfs_debug = -1;
static int scp_cur_volt = -1;
static int pre_pll_sel = -1;
static struct mt_scp_pll_t *mt_scp_pll;
static struct wakeup_source scp_suspend_lock;
static int g_scp_dvfs_init_flag = -1;

static unsigned int pre_feature_req = 0xff;

static void __iomem *gpio_base;
#define RG_GPIO154_MODE		(gpio_base + 0x430)
#define GPIO154_BIT			8
#define GPIO154_MASK		0x7

unsigned int scp_get_dvfs_opp(void)
{
	return (unsigned int)scp_cur_volt;
}

int scp_set_pmic_vcore(unsigned int cur_freq)
{
	int ret = 0;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	unsigned int ret_vc = 0, ret_vs = 0;

	if (cur_freq == CLK_OPP0) {
		ret_vc = pmic_scp_set_vcore(600000);
		ret_vs = pmic_scp_set_vsram_vcore(850000);
		scp_cur_volt = 2;
	} else if (cur_freq == CLK_OPP1) {
		ret_vc = pmic_scp_set_vcore(700000);
		ret_vs = pmic_scp_set_vsram_vcore(900000);
		scp_cur_volt = 1;
	} else if (cur_freq == CLK_OPP2) {
		ret_vc = pmic_scp_set_vcore(800000);
		ret_vs = pmic_scp_set_vsram_vcore(900000);
		scp_cur_volt = 0;
	} else {
		ret = -2;
		pr_err("ERROR: %s: cur_freq=%d is not supported\n",
		__func__, cur_freq);
		WARN_ON(1);
	}

	if (ret_vc != 0 || ret_vs != 0) {
		ret = -1;
		pr_err("ERROR: %s: scp vcore/vsram setting error, (%d, %d)\n",
					__func__, ret_vc, ret_vs);
		WARN_ON(1);
	}
#endif

	return ret;
}

uint32_t scp_get_freq(void)
{
	uint32_t i;

	uint32_t sum = 0;
	uint32_t return_freq = 0;

	/*
	 * calculate scp frequence
	 */
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].enable == 1)
			sum += feature_table[i].freq;
	}
	/*
	 * calculate scp sensor frequence
	 */
	for (i = 0; i < NUM_SENSOR_TYPE; i++) {
		if (sensor_type_table[i].enable == 1)
			sum += sensor_type_table[i].freq;
	}

	/*pr_debug("[SCP] needed freq sum:%d\n",sum);*/
	if (sum <= CLK_OPP0)
		return_freq = CLK_OPP0;
	else if (sum <= CLK_OPP1)
		return_freq = CLK_OPP1;
	else if (sum <= CLK_OPP2)
		return_freq = CLK_OPP2;
	else {
		return_freq = CLK_OPP2;
		pr_debug("warning: request freq %d > max opp %d\n",
				sum, CLK_OPP2);
	}

	return return_freq;
}

void scp_vcore_request(unsigned int clk_opp)
{
	/* Set PMIC */
	scp_set_pmic_vcore(clk_opp);

	/* set DVFSRC_VCORE_REQUEST [31:30] */
	if (scp_expected_freq == CLK_OPP2)
		dvfsrc_set_scp_vcore_request(0x1);
	else
		dvfsrc_set_scp_vcore_request(0x0);

	/* Modify scp reg: 0xC0094 [7:0] */
	DRV_ClrReg32(SCP_SCP2SPM_VOL_LV, 0xff);
	if (clk_opp == CLK_OPP0)
		DRV_SetReg32(SCP_SCP2SPM_VOL_LV, 0);
	else if (clk_opp == CLK_OPP1)
		DRV_SetReg32(SCP_SCP2SPM_VOL_LV, 1);
	else
		DRV_SetReg32(SCP_SCP2SPM_VOL_LV, 2);
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

	if (scp_dvfs_flag != 1) {
		pr_debug("warning: SCP DVFS is OFF\n");
		return 0;
	}

	/* because we are waiting for scp to update register:scp_current_freq
	 * use wake lock to prevent AP from entering suspend state
	 */
	__pm_stay_awake(&scp_suspend_lock);

	if (scp_current_freq != scp_expected_freq) {

		/* do DVS before DFS if increasing frequency */
		if (scp_current_freq < scp_expected_freq) {
			scp_vcore_request(scp_expected_freq);
			is_increasing_freq = 1;
		}

		#if SCP_DVFS_USE_PLL
		/*  turn on PLL */
		scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);
		#endif

		do {
			ret = scp_ipi_send(IPI_DVFS_SET_FREQ,
								(void *)&value,
								sizeof(value),
								0,
								SCP_A_ID);
			if (ret != SCP_IPI_DONE)
				pr_debug("SCP send IPI fail - %d\n", ret);

			mdelay(2);
			timeout -= 1; /*try 50 times, total about 100ms*/
			if (timeout <= 0) {
				pr_err(
				"%s: set freq fail, current(%d)!= expect(%d)\n",
				__func__, scp_current_freq, scp_expected_freq);
				__pm_relax(&scp_suspend_lock);
				WARN_ON(1);
				return -1;
			}

			/* read scp_current_freq again */
			spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
			scp_current_freq = readl(CURRENT_FREQ_REG);
			spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

		} while (scp_current_freq != scp_expected_freq);

		#if SCP_DVFS_USE_PLL
		/* turn off PLL */
		scp_pll_ctrl_set(PLL_DISABLE, scp_expected_freq);
		#endif

		/* do DVS after DFS if decreasing frequency */
		if (is_increasing_freq == 0)
			scp_vcore_request(scp_expected_freq);
	}

	__pm_relax(&scp_suspend_lock);
	pr_debug("[SCP] set freq OK, %d == %d\n",
			scp_expected_freq, scp_current_freq);
	return 0;
}

void wait_scp_dvfs_init_done(void)
{
	int count = 0;

	while (g_scp_dvfs_init_flag != 1) {
		mdelay(1);
		count++;
		if (count > 3000) {
			pr_err("ERROR: %s: SCP dvfs driver init fail\n",
			__func__);
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
			pr_err("EEROR: %s: scp dvfs cannot enable clk mux, %d\n",
			__func__, ret);
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
		if (pre_pll_sel != CLK_OPP2) {
			ret = clk_prepare_enable(mt_scp_pll->clk_mux);
			if (ret) {
				pr_err("ERROR: %s: clk_prepare_enable() failed, %d\n",
					__func__, ret);
				WARN_ON(1);
			}
		} else {
			pr_debug("no need to do clk_prepare_enable()\n");
		}

		switch (pll_sel) {
		case CLK_26M:
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll0);
			break;
		case CLK_OPP0:
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll1);
			break;
		case CLK_OPP1:
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll5);
			break;
		case CLK_OPP2:
			ret = clk_set_parent(
					mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll6);
			break;
		default:
			pr_err("ERROR: %s: not support opp freq %d\n",
			__func__, pll_sel);
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
				&& pll_sel != CLK_OPP2) {
		clk_disable_unprepare(mt_scp_pll->clk_mux);
		pr_debug("clk_disable_unprepare()\n");
	} else {
		pr_debug("no need to do clk_disable_unprepare\n");
	}

	return ret;
}

void scp_pll_ctrl_handler(int id, void *data, unsigned int len)
{
	unsigned int *pll_ctrl_flag = (unsigned int *)data;
	unsigned int *pll_sel =  (unsigned int *) (data + 1);
	int ret = 0;

	ret = scp_pll_ctrl_set(*pll_ctrl_flag, *pll_sel);
}

#ifdef CONFIG_PROC_FS
/*
 * PROC
 */

/***************************
 * show current debug status
 ****************************/
static int mt_scp_dvfs_debug_proc_show(struct seq_file *m, void *v)
{
	if (mt_scp_dvfs_debug == -1)
		seq_puts(m, "mt_scp_dvfs_debug has not been set\n");
	else
		seq_printf(m, "mt_scp_dvfs_debug = %d\n", mt_scp_dvfs_debug);

	return 0;
}

/***********************
 * enable debug message
 ************************/
static ssize_t mt_scp_dvfs_debug_proc_write(
						struct file *file,
						const char __user *buffer,
						size_t count,
						loff_t *data)
{
	char desc[64];
	unsigned int debug = 0;
	unsigned int len = 0;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 10, &debug) == 0) {
		if (debug == 0)
			mt_scp_dvfs_debug = 0;
		else if (debug == 1)
			mt_scp_dvfs_debug = 1;
		else
			pr_info("bad argument %d\n", debug);
	} else {
		pr_info("invalid command!\n");
	}

	scp_ipi_send(IPI_DVFS_DEBUG, (void *)&mt_scp_dvfs_debug,
				sizeof(mt_scp_dvfs_debug), 0, SCP_A_ID);

	return count;
}

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
	unsigned int len = 0;
	int ret = 0;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 10, &val) == 0) {
		if (val <= 3) {
			if (val != scp_sleep_flag) {
				scp_sleep_flag = val;
				pr_info("scp_sleep_flag = %d\n",
						scp_sleep_flag);
				ret = scp_ipi_send(IPI_DVFS_SLEEP,
							(void *)&scp_sleep_flag,
							sizeof(scp_sleep_flag),
							0, SCP_A_ID);
				if (ret != SCP_IPI_DONE)
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
	unsigned int len = 0;
	int req;
	int n;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	n = sscanf(desc, "%31s %d", cmd, &req);
	if (n == 1 || n == 2) {
		if (!strcmp(cmd, "on")) {
			scp_dvfs_flag = 1;
			pr_info("SCP DVFS: ON\n");
		} else if (!strcmp(cmd, "off")) {
			scp_dvfs_flag = -1;
			pr_info("SCP DVFS: OFF\n");
		}
#if SCP_VCORE_TEST_ENABLE
		else if (!strcmp(cmd, "req")) {
			if (req >= 0 && req <= 5) {
				if (pre_feature_req == 1)
					scp_deregister_feature(
						VCORE_TEST_FEATURE_ID);
				else if (pre_feature_req == 2)
					scp_deregister_feature(
						VCORE_TEST2_FEATURE_ID);
				else if (pre_feature_req == 3)
					scp_deregister_feature(
						VCORE_TEST3_FEATURE_ID);
				else if (pre_feature_req == 4)
					scp_deregister_feature(
						VCORE_TEST4_FEATURE_ID);
				else if (pre_feature_req == 5)
					scp_deregister_feature(
						VCORE_TEST5_FEATURE_ID);

				if (req == 1)
					scp_register_feature(
						VCORE_TEST_FEATURE_ID);
				else if (req == 2)
					scp_register_feature(
						VCORE_TEST2_FEATURE_ID);
				else if (req == 3)
					scp_register_feature(
						VCORE_TEST3_FEATURE_ID);
				else if (req == 4)
					scp_register_feature(
						VCORE_TEST4_FEATURE_ID);
				else if (req == 5)
					scp_register_feature(
						VCORE_TEST5_FEATURE_ID);

				pre_feature_req = req;
				pr_info("[SCP] set freq: %d => %d\n",
					scp_current_freq, scp_expected_freq);
			} else {
				pr_info("invalid req value %d\n", req);
			}
#endif
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

PROC_FOPS_RW(scp_dvfs_debug);
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
		PROC_ENTRY(scp_dvfs_debug),
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
						0x0664,
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

#if 0
/* pinctrl implementation */
static long _set_state(const char *name)
{
	long ret = 0;
	struct pinctrl_state *pState = 0;

	WARN_ON(!scp_pctrl);

	pState = pinctrl_lookup_state(scp_pctrl, name);
	if (IS_ERR(pState)) {
		pr_err("lookup state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(scp_pctrl, pState);

exit:
	return ret; /* Good! */
}
#endif

static const struct of_device_id scpdvfs_of_ids[] = {
	{.compatible = "mediatek,scp_dvfs",},
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

	pr_debug("%s()\n", __func__);

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
#if 0
	scp_pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(scp_pctrl)) {
		dev_notice(&pdev->dev, "Cannot find scp pinctrl!\n");
	    WARN_ON(1);
		return PTR_ERR(scp_pctrl);
	}

	_set_state(scp_state_name[SCP_DTS_VREQ_ON]);
#endif

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

	/* check if v_req pin is configured correctly  */
	gpio_mode = (DRV_Reg32(RG_GPIO154_MODE)>>GPIO154_BIT)&GPIO154_MASK;
	if (gpio_mode == 1)
		pr_debug("v_req muxpin setting is correct\n");
	else {
		pr_err("error: V_REQ muxpin setting is wrong - %d\n"
		, gpio_mode);
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

struct platform_device mt_scp_dvfs_pdev = {
	.name = "mt-scpdvfs",
	.id = -1,
};

static struct platform_driver mt_scp_dvfs_pdrv = {
	.probe = mt_scp_dvfs_pdrv_probe,
	.remove = mt_scp_dvfs_pdrv_remove,
	.driver = {
		.name = "scpdvfs",
		.pm = &mt_scp_dvfs_pm_ops,
		.owner = THIS_MODULE,
		.of_match_table = scpdvfs_of_ids,
		},
};

/**********************************
 * mediatek scp dvfs initialization
 ***********************************/
void mt_pmic_sshub_init(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	unsigned int val[8];

	val[0] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_EN);
	val[1] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_VOSEL);
	val[2] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_SLEEP_VOSEL_EN);
	val[3] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_VOSEL_SLEEP);
	val[4] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_EN);
	val[5] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL);
	val[6] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_SLEEP_VOSEL_EN);
	val[7] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SLEEP);
	pr_debug(
	"Before: vcore=(0x%x,0x%x,0x%x,0x%x), vsram=(0x%x,0x%x,0x%x,0x%x)\n",
	val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);

	pmic_scp_set_vcore(600000);
	pmic_scp_set_vcore_sleep(600000);
	pmic_set_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_EN, 1);
	pmic_set_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_SLEEP_VOSEL_EN, 0);
	pmic_scp_set_vsram_vcore(850000);
	pmic_scp_set_vsram_vcore_sleep(850000);
	pmic_set_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_EN, 1);
	pmic_set_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_SLEEP_VOSEL_EN, 0);

	val[0] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_EN);
	val[1] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_VOSEL);
	val[2] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_SLEEP_VOSEL_EN);
	val[3] = pmic_get_register_value(
		PMIC_RG_BUCK_VCORE_SSHUB_VOSEL_SLEEP);
	val[4] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_EN);
	val[5] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL);
	val[6] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_SLEEP_VOSEL_EN);
	val[7] = pmic_get_register_value(
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SLEEP);
	pr_debug(
	"After: vcore=(0x%x,0x%x,0x%x,0x%x), vsram=(0x%x,0x%x,0x%x,0x%x)\n",
	val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);

	/*  Workaround once force BUCK in NML mode */
	pmic_set_register_value(PMIC_RG_SRCVOLTEN_LP_EN, 1);
#endif
}

void mt_scp_dvfs_ipi_init(void)
{
	scp_ipi_registration(IPI_SCP_PLL_CTRL,
						scp_pll_ctrl_handler,
						"IPI_SCP_PLL_CTRL");
}

int __init scp_dvfs_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

#ifdef CONFIG_PROC_FS
	/* init proc */
	if (mt_scp_dvfs_create_procfs()) {
		pr_err("mt_scp_dvfs_create_procfs fail..\n");
		WARN_ON(1);
		return -1;
	}
#endif /* CONFIG_PROC_FS */

	/* register platform device/driver */
	ret = platform_device_register(&mt_scp_dvfs_pdev);
	if (ret) {
		pr_err("fail to register scp dvfs device @ %s()\n", __func__);
		WARN_ON(1);
		return -1;
	}

	ret = platform_driver_register(&mt_scp_dvfs_pdrv);
	if (ret) {
		pr_err("fail to register scp dvfs driver @ %s()\n", __func__);
		platform_device_unregister(&mt_scp_dvfs_pdev);
		WARN_ON(1);
		return -1;
	}

	wakeup_source_init(&scp_suspend_lock, "scp wakelock");

	mt_scp_dvfs_ipi_init();
	mt_pmic_sshub_init();

	return ret;
}

void __exit scp_dvfs_exit(void)
{
	platform_driver_unregister(&mt_scp_dvfs_pdrv);
	platform_device_unregister(&mt_scp_dvfs_pdev);
}

