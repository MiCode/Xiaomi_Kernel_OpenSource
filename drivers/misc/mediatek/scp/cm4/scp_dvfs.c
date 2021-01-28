// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/pm_wakeup.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */

#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "dvfsrc-opp.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)	"[scp_dvfs]: " fmt

#define DRV_Reg32(addr)	readl(addr)
#define DRV_WriteReg32(addr, val) writel(val, addr)
#define DRV_SetReg32(addr, val)	DRV_WriteReg32(addr, DRV_Reg32(addr) | (val))
#define DRV_ClrReg32(addr, val)	DRV_WriteReg32(addr, DRV_Reg32(addr) & ~(val))

/* -1:SCP DVFS OFF, 1:SCP DVFS ON */
static int scp_dvfs_flag = 1;

/*
 * 0: SCP Sleep: OFF,
 * 1: SCP Sleep: ON,
 * 2: SCP Sleep: sleep without wakeup,
 * 3: SCP Sleep: force to sleep
 */
static int scp_sleep_flag = -1;

static int pre_pll_sel = -1;
static struct mt_scp_pll_t *mt_scp_pll;
static struct wakeup_source *scp_suspend_lock;
static int g_scp_dvfs_init_flag = -1;

static struct regulator *dvfsrc_vscp_power;
static int dvfsrc_opp_uv[3];  /* index 0 means the highest opp */
static struct dvfs_data *dvfs;

static struct regulator *reg_vcore, *reg_vsram;

void scp_to_spm_resource_req(unsigned long cmd, unsigned long val)
{
	struct arm_smccc_res res;

	pr_notice("%s(0x%x, 0x%x)\n", __func__, (int)cmd, (int)val);

	arm_smccc_smc(MTK_SIP_KERNEL_SCP_DVFS_CTRL,
			cmd, val,
			0, 0, 0, 0, 0, &res);
	if (res.a0) {
		pr_err("%s: failed to request resource, ret0=0x%lx, ret1=0x%lx\n",
				__func__, res.a0, res.a1);
		WARN_ON(1);
	}
}

static struct subsys_data *sd;

const char *sub_feature_name[SUB_FEATURE_NUM] = {
	"gpio-mode",
	"pmic-vow-lp",
	"pmic-pmrc",
};

const char *subsys_name[SYS_NUM] = {
	"gpio",
	"pmic",
};

static int scp_get_sub_feature_idx(enum subsys_enum sys_e,
		enum sub_feature_enum comp_e)
{
	int i;

	for (i = 0; i < sd[sys_e].num; i++) {
		if (!strcmp(sd[sys_e].fd[i].name,
				sub_feature_name[comp_e]))
			break;
	}

	if (i == SUB_FEATURE_NUM) {
		pr_err("cannot find feature index\n");

		return -EINVAL;
	}

	return i;
}

static struct sub_feature_data *scp_get_sub_feature(enum subsys_enum sys_e,
		enum sub_feature_enum comp_e)
{
	int idx;

	idx = scp_get_sub_feature_idx(sys_e, comp_e);

	if (idx < 0)
		return ERR_PTR(idx);

	return &sd[sys_e].fd[idx];
}

static int scp_get_sub_feature_onoff(enum subsys_enum sys_e,
		enum sub_feature_enum comp_e)
{
	int idx;

	idx = scp_get_sub_feature_idx(sys_e, comp_e);

	if (idx < 0)
		return 0;

	return sd[sys_e].fd[idx].onoff;
}

static unsigned int *scp_get_sub_register_cfg(enum subsys_enum sys_e,
		enum sub_feature_enum comp_e)
{
	struct regmap *regmap = sd[sys_e].regmap;
	struct sub_feature_data *fd;
	unsigned int *ret;
	unsigned int val;
	int i;

	fd = scp_get_sub_feature(sys_e, comp_e);

	if (!fd) {
		pr_err("fd is NULL\n");

		return ERR_PTR(-EINVAL);
	}

	/* alloc memory for return cfg value */
	ret = kcalloc(fd->num, sizeof(unsigned int), GFP_KERNEL);
	if (!ret)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < fd->num; i++) {
		regmap_read(regmap, fd->reg[i].ofs, &val);
		ret[i] = (val >> fd->reg[i].bit) & fd->reg[i].msk;
	}

	return ret;
}

static unsigned int scp_set_sub_register_cfg_internal(
		struct regmap *regmap,
		struct reg_info *reg,
		struct reg_cfg *cfg,
		bool on)
{
	unsigned int val, mask;
	int ret = 0;

	/* get mask */
	mask = reg->msk << reg->bit;
	/* get new cfg setting according to feature on/off */
	val = on ? cfg->on : cfg->off;
	val = (val & reg->msk) << reg->bit;

	if (reg->setclr) {
		/* clear cfg setting first */
		ret = regmap_write(regmap, reg->ofs + 4, mask);
		/* set cfg with new setting */
		ret = regmap_write(regmap, reg->ofs + 2, val);
	} else
		/* directly write cfg by new setting */
		ret = regmap_update_bits(regmap, reg->ofs, mask, val);

	return ret;
}

static int scp_set_sub_register_cfg(enum subsys_enum sys_e,
		enum sub_feature_enum comp_e, bool on)
{
	struct regmap *regmap = sd[sys_e].regmap;
	struct sub_feature_data *fd;
	int ret = 0;
	int i;

	fd = scp_get_sub_feature(sys_e, comp_e);
	if (!fd) {
		pr_err("fd is NULL\n");

		return -EINVAL;
	}

	for (i = 0; i < fd->num; i++) {
		ret = scp_set_sub_register_cfg_internal(regmap,
				&fd->reg[i], &fd->cfg[i], on);
		if (ret) {
			pr_err("fail to set sub feature cfg[%d] : %d\n",
					i, ret);
			goto fail;
		}
	}
fail:
	return ret;
}

static int scp_get_freq_idx(unsigned int clk_opp)
{
	int i;

	for (i = 0; i < dvfs->scp_opp_num; i++)
		if (clk_opp == dvfs->opp[i].freq)
			break;

	if (i == dvfs->scp_opp_num) {
		pr_err("no available opp\n");
		return -EINVAL;
	}

	return i;
}

int scp_set_pmic_vcore(unsigned int cur_freq)
{
	int ret = 0;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	int max_vcore = dvfs->opp[dvfs->scp_opp_num - 1].vcore + 100000;
	int max_vsram = dvfs->opp[dvfs->scp_opp_num - 1].vsram + 100000;
	int idx = scp_get_freq_idx(cur_freq);
	unsigned int ret_vc = 0, ret_vs = 0;

	/* if vcore/vsram define as 0xff, means no pmic op during dvfs */
	if (dvfs->opp[dvfs->scp_opp_num - 1].vcore == 0xff
			&& dvfs->opp[dvfs->scp_opp_num - 1].vsram == 0xff)
		return ret;

	if (idx >= 0 && idx < dvfs->scp_opp_num) {
		ret_vc = regulator_set_voltage(reg_vcore, dvfs->opp[idx].vcore,
				max_vcore);
		ret_vs = regulator_set_voltage(reg_vsram, dvfs->opp[idx].vsram,
				max_vsram);
	} else {
		ret = -2;
		pr_err("cur_freq=%d is not supported\n", cur_freq);
				WARN_ON(1);
	}

	if (ret_vc != 0 || ret_vs != 0) {
		ret = -1;
		pr_err("ERROR: %s: scp vcore/vsram setting error, (%d, %d)\n",
				__func__, ret_vc, ret_vs);
		WARN_ON(1);
	}

	if (scp_get_sub_feature_onoff(SYS_PMIC, PMIC_VOW_LP)) {
		/* vcore > 0.6v cannot hold pmic/vcore in lp mode */
		if (idx < 2)
			/* enable VOW low power mode */
			ret = scp_set_sub_register_cfg(SYS_PMIC,
					PMIC_VOW_LP, true);
		else
			/* disable VOW low power mode */
			ret = scp_set_sub_register_cfg(SYS_PMIC,
					PMIC_VOW_LP, false);
	}
#endif /* CONFIG_FPGA_EARLY_PORTING */

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
	for (i = 0; i < dvfs->scp_opp_num; i++) {
		if (sum <= dvfs->opp[i].freq) {
			return_freq = dvfs->opp[i].freq;
			break;
		}
	}

	if (i == dvfs->scp_opp_num) {
		return_freq = dvfs->opp[dvfs->scp_opp_num - 1].freq;
		pr_notice("warning: request freq %d > max opp %d\n",
				sum, return_freq);
	}

	return return_freq;
}

void scp_vcore_request(unsigned int clk_opp)
{
	int idx;

	pr_debug("%s(%d)\n", __func__, clk_opp);

	/* Set PMIC */
	scp_set_pmic_vcore(clk_opp);

	/* DVFSRC_VCORE_REQUEST [31:30]
	 * 2'b00: scp request 0.575v/0.6v/0.625v
	 * 2'b01: scp request 0.7v
	 * 2'b10: scp request 0.8v
	 */
	idx = scp_get_freq_idx(clk_opp);
	if (idx < 0)
		return;
	/* idx == 0xff, means scp opp[idx] not supported in dvfsrc opp table */
	if (idx != 0xff)
		/* vcore MAX_uV set to highest opp + 100mV */
		regulator_set_voltage(dvfsrc_vscp_power, dvfsrc_opp_uv[idx],
				dvfsrc_opp_uv[0] + 100000);

	/* SCP to SPM voltage level 0x100066C4 (scp reg 0xC0094)
	 * 0x0: scp request 0.575v/0.6v
	 * 0x1: scp request 0.625v
	 * 0x12: scp request 0.7v
	 * 0x28: scp request 0.8v
	 */
	DRV_WriteReg32(SCP_SCP2SPM_VOL_LV, dvfs->opp[idx].spm_opp);
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
	__pm_stay_awake(scp_suspend_lock);

	if (scp_current_freq != scp_expected_freq) {

		/* do DVS before DFS if increasing frequency */
		if (scp_current_freq < scp_expected_freq) {
			scp_vcore_request(scp_expected_freq);
			is_increasing_freq = 1;
		}

		/* Request SPM not to turn off mainpll/26M/infra */
		/* because SCP may park in it during DFS process */
		scp_to_spm_resource_req(SCP_DVFS_SMC_RESOURCE_REQ,
				SCP_REQ_RESOURCE_26M |
				SCP_REQ_RESOURCE_INFRA |
				SCP_REQ_RESOURCE_SYSPLL);

		/*  turn on PLL if necessary */
		scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);

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
				pr_err("set freq fail, current(%d) != expect(%d)\n",
					scp_current_freq, scp_expected_freq);
				__pm_relax(scp_suspend_lock);
				WARN_ON(1);
				return -1;
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

		if (scp_expected_freq == dvfs->opp[dvfs->scp_opp_num - 1].freq)
			/* request SPM not to turn off 26M/infra */
			scp_to_spm_resource_req(SCP_DVFS_SMC_RESOURCE_REQ,
					SCP_REQ_RESOURCE_26M |
					SCP_REQ_RESOURCE_INFRA);
		else
			scp_to_spm_resource_req(SCP_DVFS_SMC_RESOURCE_REL, 0);
	}

	__pm_relax(scp_suspend_lock);
	pr_debug("[SCP] succeed to set freq, expect=%d, cur=%d\n",
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
			pr_err("SCP dvfs driver init fail\n");
			WARN_ON(1);
		}
	}
}

int scp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel)
{
	int max_freq = dvfs->opp[dvfs->scp_opp_num - 1].freq;
	int idx = scp_get_freq_idx(pll_sel);
	int mux_idx = dvfs->opp[idx].clk_mux;
	int ret = 0;

	pr_debug("%s(%d, %d)\n", __func__, pll_ctrl_flag, pll_sel);

	if (pll_ctrl_flag == PLL_ENABLE) {
		if (pre_pll_sel != max_freq) {
			ret = clk_prepare_enable(mt_scp_pll->clk_mux);
			if (ret) {
				pr_err("clk_prepare_enable() failed\n");
				WARN_ON(1);
			}
		} else {
			pr_debug("no need to do clk_prepare_enable()\n");
		}

		if (pll_sel == CLK_26M)
			/* default boot-up clk : 26 MHz */
			ret = clk_set_parent(mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll[0]);
		else if (idx >= 0 && idx < dvfs->scp_opp_num
				&& idx < mt_scp_pll->pll_num)
			ret = clk_set_parent(mt_scp_pll->clk_mux,
					mt_scp_pll->clk_pll[mux_idx]);
		else {
			pr_err("not support opp freq %d\n", pll_sel);
			WARN_ON(1);
		}

		if (ret) {
			pr_debug("clk_set_parent() failed, opp=%d\n",
					pll_sel);
			WARN_ON(1);
		}

		if (pre_pll_sel != pll_sel)
			pre_pll_sel = pll_sel;
	} else if (pll_ctrl_flag == PLL_DISABLE
			&& (pll_sel != max_freq)) {
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

	scp_pll_ctrl_set(*pll_ctrl_flag, *pll_sel);
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
	int len = 0;
	int dvfs_opp;
	int freq;
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
				scp_deregister_feature(
						VCORE_TEST_FEATURE_ID);
			} else if (dvfs_opp >= 0 &&
					dvfs_opp < dvfs->scp_opp_num) {
				uint32_t i;
				uint32_t sum = 0, added_freq = 0;

				pr_info("manually set opp = %d\n", dvfs_opp);

				/*
				 * calculate scp frequence
				 */
				for (i = 0; i < NUM_FEATURE_ID; i++) {
					if (i != VCORE_TEST_FEATURE_ID &&
						feature_table[i].enable == 1)
						sum += feature_table[i].freq;
				}

				/*
				 * calculate scp sensor frequence
				 */
				for (i = 0; i < NUM_SENSOR_TYPE; i++) {
					if (sensor_type_table[i].enable == 1)
						sum +=
						sensor_type_table[i].freq;
				}

				for (i = 0; i < dvfs->scp_opp_num; i++) {
					freq = dvfs->opp[i].freq;

					if (dvfs_opp == i && sum < freq)
						added_freq = freq - sum;
				}

				feature_table[VCORE_TEST_FEATURE_ID].freq =
						added_freq;

				pr_debug("request freq: %d + %d = %d (MHz)\n",
						sum,
						added_freq,
						sum + added_freq);

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

static int __init mt_scp_regmap_init(struct platform_device *pdev,
		struct device_node *node)
{
	struct platform_device *pmic_pdev;
	struct device_node *pmic_node;
	struct mt6397_chip *chip;
	struct regmap *regmap;

	/* get GPIO regmap */
	regmap = syscon_regmap_lookup_by_phandle(node, subsys_name[SYS_GPIO]);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev,
			"Get gpio regmap fail: %ld\n",
			PTR_ERR(regmap));
		goto fail;
	}

	sd[SYS_GPIO].regmap = regmap;

	/* get PMIC regmap */
	pmic_node = of_parse_phandle(node,
			subsys_name[SYS_PMIC], 0);
	if (!pmic_node) {
		dev_notice(&pdev->dev, "fail to find pmic node\n");
		goto fail;
	}

	pmic_pdev = of_find_device_by_node(pmic_node);
	if (!pmic_pdev) {
		dev_err(&pdev->dev, "fail to find pmic device\n");
		goto fail;
	}

	chip = dev_get_drvdata(&(pmic_pdev->dev));
	if (!chip) {
		dev_err(&pdev->dev, "fail to find pmic drv data\n");
		goto fail;
	}

	regmap =  chip->regmap;
	if (IS_ERR_VALUE(regmap)) {
		sd[SYS_PMIC].regmap = NULL;
		dev_err(&pdev->dev, "get pmic regmap fail\n");
		goto fail;
	}

	sd[SYS_PMIC].regmap = regmap;

	return 0;
fail:
	WARN_ON(1);
	return -1;
}

static  int __init mt_scp_sub_feature_init_internal(struct device_node *node,
		struct sub_feature_data *fd)
{
	char *buf = kzalloc(sizeof(char) * 25, GFP_KERNEL);
	struct reg_info *reg;
	struct reg_cfg *cfg;
	unsigned int cfg_num;
	int ret = 0;
	int i;

	if (!buf)
		return -ENOMEM;

	snprintf(buf, 25, "%s-reg", fd->name);

	fd->num = of_property_count_u32_elems(node, buf) / 4;
	if (fd->num <= 0)
		goto pass;

	reg = kcalloc(fd->num, sizeof(struct reg_info), GFP_KERNEL);

	if (!reg) {
		ret = -ENOMEM;
		goto fail_1;
	}

	for (i = 0; i < fd->num; i++) {
		ret = of_property_read_u32_index(node, buf, i * 4,
			&reg[i].ofs);
		if (ret) {
			pr_err("Cannot get property offset(%d)\n", ret);
			goto fail_2;
		}
		ret = of_property_read_u32_index(node, buf, (i * 4) + 1,
			&reg[i].msk);
		if (ret) {
			pr_err("Cannot get property mask(%d)\n", ret);
			goto fail_2;
		}
		ret = of_property_read_u32_index(node, buf, (i * 4) + 2,
			&reg[i].bit);
		if (ret) {
			pr_err("Cannot get property bit shift(%d)\n", ret);
			goto fail_2;
		}
		ret = of_property_read_u32_index(node, buf, (i * 4) + 3,
			&reg[i].setclr);
		if (ret) {
			pr_err("Cannot get property set/clr type(%d)\n", ret);
			goto fail_2;
		}
	}

	fd->reg = reg;

	snprintf(buf, 25, "%s-cfg", fd->name);
	cfg_num = of_property_count_u32_elems(node, buf) / 2;
	if (cfg_num != fd->num) {
		pr_notice("cfg number is not matched(%d)\n", cfg_num);
		goto pass;
	}

	cfg = kcalloc(cfg_num, sizeof(struct reg_cfg), GFP_KERNEL);

	if (!cfg) {
		ret = -ENOMEM;
		goto fail_2;
	}

	for (i = 0; i < fd->num; i++) {
		ret = of_property_read_u32_index(node, buf, i * 2,
				&cfg[i].on);
		if (ret) {
			pr_err("Cannot get property cfg on(%d)\n", ret);
			goto fail_3;
		}
		ret = of_property_read_u32_index(node, buf, (i * 2) + 1,
			&cfg[i].off);
		if (ret) {
			pr_err("Cannot get property cfg off(%d)\n", ret);
			goto fail_3;
		}
	}

	fd->cfg = cfg;
pass:
	kfree(buf);

	return 0;
fail_3:
	kfree(cfg);
fail_2:
	kfree(reg);
fail_1:
	kfree(buf);

	WARN_ON(1);

	return ret;
}

static int __init mt_scp_sub_feature_init(struct device_node *node,
		struct subsys_data *sys,
		const char *str)
{
	struct sub_feature_data *fd;
	char *buf;
	int ret;
	int i, j;

	/* init  feature data struct */
	sys->num = of_property_count_strings(node, str);
	if (sys->num <= 0) {
		kfree(fd);
		goto pass;
	}
	/* init feature data structure */
	fd = kcalloc(sys->num, sizeof(*fd), GFP_KERNEL);
	buf = kzalloc(sizeof(char) * 25, GFP_KERNEL);
	if (!fd || !buf) {
		ret = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < sys->num; i++) {
		const char *name = kzalloc(sizeof(char) * 20, GFP_KERNEL);

		if (!name) {
			ret = -ENOMEM;
			goto fail_2;
		}

		ret = of_property_read_string_index(node, str, i,
				&name);
		if (ret) {
			pr_err("Cannot get property string(%d)\n", ret);
			kfree(name);
			ret = -EINVAL;
			goto fail_2;
		}

		fd[i].name = name;
		ret = mt_scp_sub_feature_init_internal(node, &fd[i]);
		if (ret) {
			kfree(name);
			goto fail_2;
		}

		/* init feature cfg */
		snprintf(buf, 25, "%s-cfg", str);
		ret = of_property_read_u32_index(node, buf, i,
				&fd[i].onoff);
		if (ret) {
			kfree(name);
			goto fail_2;
		}
	}

	sys->fd = fd;
pass:
	kfree(buf);

	return 0;
fail_2:
	for (j = i - 1; j >= 0; j--)
		kfree(fd[j].name);
fail:
	kfree(buf);
	kfree(fd);
	WARN_ON(1);

	return ret;
}

static void __init mt_pmic_sshub_init(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	int max_vcore = dvfs->opp[dvfs->scp_opp_num - 1].vcore;
	int max_vsram = dvfs->opp[dvfs->scp_opp_num - 1].vsram;

	/* if vcore/vsram define as 0xff, means no pmic op during dvfs */
	if (max_vcore == 0xff && max_vsram == 0xff)
		return;

	/* set SCP VCORE voltage */
	if (regulator_set_voltage(reg_vcore, dvfs->opp[0].vcore,
			max_vcore) != 0)
		pr_notice("Set wrong vcore voltage\n");

	/* set SCP VSRAM voltage */
	if (regulator_set_voltage(reg_vsram, dvfs->opp[0].vsram,
			max_vsram) != 0)
		pr_notice("Set wrong vsram voltage\n");

	if (scp_get_sub_feature_onoff(SYS_PMIC, PMIC_VOW_LP))
		/* enable VOW low power mode */
		scp_set_sub_register_cfg(SYS_PMIC, PMIC_VOW_LP, true);
	else
		/* disable VOW low power mode */
		scp_set_sub_register_cfg(SYS_PMIC, PMIC_VOW_LP, false);

	/* pmrc_mode: OFF */
	if (scp_get_sub_feature_onoff(SYS_PMIC, PMIC_PMRC))
		scp_set_sub_register_cfg(SYS_PMIC, PMIC_PMRC, true);
	else
		scp_set_sub_register_cfg(SYS_PMIC, PMIC_PMRC, false);

	/* BUCK_VCORE_SSHUB_EN: ON */
	/* LDO_VSRAM_OTHERS_SSHUB_EN: ON */
	if (regulator_enable(reg_vcore) != 0)
		pr_notice("Enable vcore failed!!!\n");
	if (regulator_enable(reg_vsram) != 0)
		pr_notice("Enable vsram failed!!!\n");
#endif
}

static int __init mt_scp_dvfs_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct dvfs_opp *opp;
	unsigned int *gpio_mode;
	char *buf;
	int i;
	int ret = 0;

	/* find device tree node of scp_dvfs */
	node = of_find_matching_node(NULL, scpdvfs_of_ids);
	if (!node) {
		dev_notice(&pdev->dev, "fail to find SCPDVFS node\n");
		return -ENODEV;
	}

	/* init subsys data struct */
	sd = kcalloc(SYS_NUM, sizeof(*sd), GFP_KERNEL);
	if (!sd)
		return -ENOMEM;

	/* init temp buf */
	buf = kzalloc(sizeof(char) * 15, GFP_KERNEL);
	if (!buf) {
		kfree(sd);
		return -ENOMEM;
	}

	/* init dvfs data structure */
	dvfs = kzalloc(sizeof(*dvfs), GFP_KERNEL);
	if (!dvfs) {
		kfree(buf);
		kfree(sd);
		return -ENOMEM;
	}

	/* get scp dvfs opp count */
	ret = of_property_count_u32_elems(node, "dvfs-opp") / 6;
	if (ret <= 0) {
		kfree(buf);
		kfree(sd);
		return ret;
	}

	dvfs->scp_opp_num = ret;
	/* init opp data structure */
	opp = kcalloc(dvfs->scp_opp_num, sizeof(*opp), GFP_KERNEL);
	if (!dvfs) {
		kfree(dvfs);
		kfree(buf);
		kfree(sd);
		return -ENOMEM;
	}
	/* init regmap */
	ret = mt_scp_regmap_init(pdev, node);
	if (ret)
		goto fail;

	/* init gpio/pmic feature data */
	for (i = 0; i < SYS_NUM; i++) {
		snprintf(buf, 15, "%s-feature", subsys_name[i]);
		ret = mt_scp_sub_feature_init(node, &sd[i], buf);
		if (ret)
			goto fail;
	}

	/* get scp_sel for clk-mux setting */
	mt_scp_pll = kzalloc(sizeof(struct mt_scp_pll_t), GFP_KERNEL);
	if (mt_scp_pll == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	mt_scp_pll->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(mt_scp_pll->clk_mux)) {
		dev_notice(&pdev->dev, "cannot get clock mux\n");
		WARN_ON(1);
		ret =  PTR_ERR(mt_scp_pll->clk_mux);
		goto fail;
	}
	/* scp_sel has most 8 member of clk source */
	for (i = 0; i < 8; i++) {
		snprintf(buf, 15, "clk_pll_%d", i);
		mt_scp_pll->clk_pll[i] = devm_clk_get(&pdev->dev, buf);
		if (IS_ERR(mt_scp_pll->clk_pll[i])) {
			dev_notice(&pdev->dev,
					"cannot get %dst clock parent\n",
					i);
			mt_scp_pll->pll_num = i;
			break;
		}
	}

	/* check if GPIO is configured correctly for SCP VREQ */
	if (scp_get_sub_feature_onoff(SYS_GPIO, GPIO_MODE)) {
		gpio_mode = kzalloc(sizeof(int), GFP_KERNEL);
		if (!gpio_mode) {
			ret = -ENOMEM;
			goto fail;
		}

		gpio_mode = scp_get_sub_register_cfg(SYS_GPIO, GPIO_MODE);

		if (*gpio_mode == 1)
			pr_debug("v_req muxpin setting is correct\n");
		else {
			pr_notice("wrong V_REQ muxpin setting - %d\n",
					*gpio_mode);
			WARN_ON(1);
		}
	}

	/* get each dvfs opp data from dts node */
	for (i = 0; i < dvfs->scp_opp_num; i++) {
		ret = of_property_read_u32_index(node, "dvfs-opp", i * 6,
				&opp[i].vcore);
		if (ret) {
			pr_err("Cannot get property vcore(%d)\n", ret);
			goto fail;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp", (i * 6) + 1,
				&opp[i].vsram);
		if (ret) {
			pr_err("Cannot get property vsram(%d)\n", ret);
			goto fail;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp", (i * 6) + 2,
				&opp[i].dvfsrc_opp);
		if (ret) {
			pr_err("Cannot get property dvfsrc opp(%d)\n", ret);
			goto fail;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp", (i * 6) + 3,
				&opp[i].spm_opp);
		if (ret) {
			pr_err("Cannot get property spm opp(%d)\n", ret);
			goto fail;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp", (i * 6) + 4,
				&opp[i].freq);

		if (ret) {
			pr_err("Cannot get property freq(%d)\n", ret);
			goto fail;
		}

		ret = of_property_read_u32_index(node, "dvfs-opp", (i * 6) + 5,
				&opp[i].clk_mux);
		if (ret) {
			pr_err("Cannot get property clk mux(%d)\n", ret);
			goto fail;
		}
	}

	dvfs->opp = opp;

	/* get dvfsrc table opp count */
	ret = of_property_read_u32(node, "dvfsrc-opp-num",
			&dvfs->dvfsrc_opp_num);
	if (ret) {
		pr_err("Cannot get property dvfsrc opp num(%d)\n", ret);
		goto fail;
	}

	if (dvfs->dvfsrc_opp_num == 0) {
		pr_notice("dvfsrc table has zero opp count\n");
		goto pmic_cfg;
	}

	/* get dvfsrc regulator */
	dvfsrc_vscp_power = regulator_get(&pdev->dev, "dvfsrc-vscp");
	for (i = 0; i < dvfs->dvfsrc_opp_num; i++) {
		dvfsrc_opp_uv[i] = mtk_dvfsrc_vcore_uv_table(i);
		if (dvfsrc_opp_uv[i] <= 0) {
			pr_err("Cannot get dvfsrc opp[%d] = (%d)\n",
					i, dvfsrc_opp_uv[i]);
			goto fail;
		}

		pr_notice("%s: dvfsrc opp[%d] = %duV\n", __func__,
				i, dvfsrc_opp_uv[i]);
	}

pmic_cfg:
	/* get Vcore/Vsram Regulator */
	reg_vcore = devm_regulator_get_optional(&pdev->dev, "sshub-vcore");
	if (IS_ERR(reg_vcore) || !reg_vcore) {
		pr_notice("regulator vcore sshub supply is not available\n");
		ret = PTR_ERR(reg_vcore);
		goto pass;
	}

	reg_vsram = devm_regulator_get_optional(&pdev->dev, "sshub-vsram");
	if (IS_ERR(reg_vsram) || !reg_vsram) {
		pr_notice("regulator vsram sshub supply is not available\n");
		ret = PTR_ERR(reg_vsram);
		goto pass;
	}

	mt_pmic_sshub_init();
pass:
	kfree(buf);

	g_scp_dvfs_init_flag = 1;

	return 0;
fail:
	kfree(dvfs);
	kfree(buf);
	kfree(opp);
	kfree(sd);
	WARN_ON(1);

	return -1;
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

static struct platform_driver mt_scp_dvfs_pdrv __refdata = {
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
void __init mt_scp_dvfs_ipi_init(void)
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
		goto fail;
	}
#endif /* CONFIG_PROC_FS */

	/* register platform device/driver */
	ret = platform_device_register(&mt_scp_dvfs_pdev);
	if (ret) {
		pr_err("fail to register scp dvfs device @ %s()\n", __func__);
		goto fail;
	}

	ret = platform_driver_register(&mt_scp_dvfs_pdrv);
	if (ret) {
		pr_err("fail to register scp dvfs driver @ %s()\n", __func__);
		platform_device_unregister(&mt_scp_dvfs_pdev);
		goto fail;
	}

	scp_suspend_lock = wakeup_source_register(NULL, "scp wakelock");

	mt_scp_dvfs_ipi_init();

	return 0;
fail:
	WARN_ON(1);
	return -1;
}

void __exit scp_dvfs_exit(void)
{
	platform_driver_unregister(&mt_scp_dvfs_pdrv);
	platform_device_unregister(&mt_scp_dvfs_pdev);
}

