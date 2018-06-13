/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/thermal.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <soc/qcom/devfreq_devbw.h>

#include "npu_common.h"
#include "npu_hw.h"

/* -------------------------------------------------------------------------
 * Defines
 * -------------------------------------------------------------------------
 */
#define CLASS_NAME              "npu"
#define DRIVER_NAME             "msm_npu"
#define DDR_MAPPED_START_ADDR   0x80000000
#define DDR_MAPPED_SIZE         0x60000000

#define PERF_MODE_DEFAULT 0

#define POWER_LEVEL_MIN_SVS 0
#define POWER_LEVEL_LOW_SVS 1
#define POWER_LEVEL_NOMINAL 4

/* -------------------------------------------------------------------------
 * File Scope Prototypes
 * -------------------------------------------------------------------------
 */
static int npu_enable_regulators(struct npu_device *npu_dev);
static void npu_disable_regulators(struct npu_device *npu_dev);
static int npu_enable_core_clocks(struct npu_device *npu_dev, bool post_pil);
static void npu_disable_core_clocks(struct npu_device *npu_dev);
static uint32_t npu_calc_power_level(struct npu_device *npu_dev);
static ssize_t npu_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_show_pwr_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t npu_store_pwr_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static void npu_suspend_devbw(struct npu_device *npu_dev);
static void npu_resume_devbw(struct npu_device *npu_dev);
static bool npu_is_post_clock(const char *clk_name);
static bool npu_is_exclude_rate_clock(const char *clk_name);
static int npu_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state);
static int npu_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state);
static int npu_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state);
static int npu_open(struct inode *inode, struct file *file);
static int npu_close(struct inode *inode, struct file *file);
static int npu_get_info(struct npu_device *npu_dev, unsigned long arg);
static int npu_map_buf(struct npu_device *npu_dev, unsigned long arg);
static int npu_unmap_buf(struct npu_device *npu_dev, unsigned long arg);
static int npu_load_network(struct npu_device *npu_dev, unsigned long arg);
static int npu_load_network_v2(struct npu_device *npu_dev, unsigned long arg);
static int npu_unload_network(struct npu_device *npu_dev, unsigned long arg);
static int npu_exec_network(struct npu_device *npu_dev, unsigned long arg);
static int npu_exec_network_v2(struct npu_device *npu_dev, unsigned long arg);
static long npu_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg);
static int npu_parse_dt_clock(struct npu_device *npu_dev);
static int npu_parse_dt_regulator(struct npu_device *npu_dev);
static int npu_of_parse_pwrlevels(struct npu_device *npu_dev,
		struct device_node *node);
static int npu_pwrctrl_init(struct npu_device *npu_dev);
static int npu_probe(struct platform_device *pdev);
static int npu_remove(struct platform_device *pdev);
static int npu_suspend(struct platform_device *dev, pm_message_t state);
static int npu_resume(struct platform_device *dev);
static int __init npu_init(void);
static void __exit npu_exit(void);

/* -------------------------------------------------------------------------
 * File Scope Variables
 * -------------------------------------------------------------------------
 */
static const char * const npu_clock_order[] = {
	"qdss_clk",
	"at_clk",
	"trig_clk",
	"armwic_core_clk",
	"cal_dp_clk",
	"cal_dp_cdc_clk",
	"conf_noc_ahb_clk",
	"comp_noc_axi_clk",
	"npu_core_clk",
	"npu_core_cti_clk",
	"npu_core_apb_clk",
	"npu_core_atb_clk",
	"npu_cpc_clk",
	"npu_cpc_timer_clk",
	"qtimer_core_clk",
	"sleep_clk",
	"bwmon_clk",
	"perf_cnt_clk",
	"bto_core_clk",
	"xo_clk"
};

static const char * const npu_post_clocks[] = {
	"npu_cpc_clk",
	"npu_cpc_timer_clk"
};

static const char * const npu_exclude_rate_clocks[] = {
	"qdss_clk",
	"at_clk",
	"trig_clk",
	"sleep_clk",
	"xo_clk",
	"conf_noc_ahb_clk",
	"comp_noc_axi_clk",
	"npu_core_cti_clk",
	"npu_core_apb_clk",
	"npu_core_atb_clk",
	"npu_cpc_timer_clk",
	"qtimer_core_clk",
	"bwmon_clk",
	"bto_core_clk"
};

static struct npu_reg npu_saved_bw_registers[] = {
	{ BWMON2_SAMPLING_WINDOW, 0, false },
	{ BWMON2_BYTE_COUNT_THRESHOLD_HIGH, 0, false },
	{ BWMON2_BYTE_COUNT_THRESHOLD_MEDIUM, 0, false },
	{ BWMON2_BYTE_COUNT_THRESHOLD_LOW, 0, false },
	{ BWMON2_ZONE_ACTIONS, 0, false },
	{ BWMON2_ZONE_COUNT_THRESHOLD, 0, false },
};

static const struct npu_irq npu_irq_info[NPU_MAX_IRQ] = {
	{"ipc_irq", 0},
	{"error_irq", 0},
	{"wdg_bite_irq", 0},
};

/* -------------------------------------------------------------------------
 * Entry Points for Probe
 * -------------------------------------------------------------------------
 */
/* Sys FS */
static DEVICE_ATTR(caps, 0444, npu_show_capabilities, NULL);
static DEVICE_ATTR(pwr, 0644, npu_show_pwr_state, npu_store_pwr_state);

static struct attribute *npu_fs_attrs[] = {
	&dev_attr_caps.attr,
	&dev_attr_pwr.attr,
	NULL
};

static struct attribute_group npu_fs_attr_group = {
	.attrs = npu_fs_attrs
};

static const struct of_device_id npu_dt_match[] = {
	{ .compatible = "qcom,msm-npu",},
	{}
};

static struct platform_driver npu_driver = {
	.probe = npu_probe,
	.remove = npu_remove,
#if defined(CONFIG_PM)
	.suspend = npu_suspend,
	.resume = npu_resume,
#endif
	.driver = {
		.name = "msm_npu",
		.owner = THIS_MODULE,
		.of_match_table = npu_dt_match,
		.pm = NULL,
	},
};

static const struct file_operations npu_fops = {
	.owner = THIS_MODULE,
	.open = npu_open,
	.release = npu_close,
	.unlocked_ioctl = npu_ioctl,
};

static const struct thermal_cooling_device_ops npu_cooling_ops = {
	.get_max_state = npu_get_max_state,
	.get_cur_state = npu_get_cur_state,
	.set_cur_state = npu_set_cur_state,
};

/* -------------------------------------------------------------------------
 * SysFS - Capabilities
 * -------------------------------------------------------------------------
 */
static ssize_t npu_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t ret = 0;
	struct npu_device *npu_dev = dev_get_drvdata(dev);

	if (!npu_enable_core_power(npu_dev)) {
		if (snprintf(buf, PAGE_SIZE, "hw_version :0x%X",
			REGR(npu_dev, NPU_HW_VERSION)) < 0)
			ret = -EINVAL;
		npu_disable_core_power(npu_dev);
	} else
		ret = -EPERM;

	return ret;
}

/* -------------------------------------------------------------------------
 * SysFS - Power State
 * -------------------------------------------------------------------------
 */
static ssize_t npu_show_pwr_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(pwr->pwr_vote_num > 0) ? "on" : "off");
}

static ssize_t npu_store_pwr_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	bool pwr_on = false;

	if (strtobool(buf, &pwr_on) < 0)
		return -EINVAL;

	if (pwr_on) {
		if (npu_enable_core_power(npu_dev))
			return -EPERM;
	} else {
		npu_disable_core_power(npu_dev);
	}

	return count;
}

/* -------------------------------------------------------------------------
 * Power Related
 * -------------------------------------------------------------------------
 */
int npu_enable_core_power(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	int ret = 0;

	if (!pwr->pwr_vote_num) {
		ret = npu_enable_regulators(npu_dev);
		if (ret)
			return ret;

		ret = npu_enable_core_clocks(npu_dev, false);
		if (ret) {
			npu_disable_regulators(npu_dev);
			pwr->pwr_vote_num = 0;
			return ret;
		}
		npu_resume_devbw(npu_dev);
	}
	pwr->pwr_vote_num++;

	return ret;
}

void npu_disable_core_power(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	if (!pwr->pwr_vote_num)
		return;
	pwr->pwr_vote_num--;
	if (!pwr->pwr_vote_num) {
		npu_suspend_devbw(npu_dev);
		npu_disable_core_clocks(npu_dev);
		npu_disable_regulators(npu_dev);
	}
	/* init the power levels back to default */
	pwr->active_pwrlevel = pwr->default_pwrlevel;
	pwr->uc_pwrlevel = pwr->default_pwrlevel;
	pr_debug("setting back to default power level=%d\n",
		pwr->default_pwrlevel);
}

int npu_enable_post_pil_clocks(struct npu_device *npu_dev)
{
	return npu_enable_core_clocks(npu_dev, true);
}


static uint32_t npu_calc_power_level(struct npu_device *npu_dev)
{
	uint32_t ret_level;
	uint32_t therm_pwr_level = npu_dev->thermalctrl.pwr_level;
	uint32_t active_pwr_level = npu_dev->pwrctrl.active_pwrlevel;
	uint32_t uc_pwr_level = npu_dev->pwrctrl.uc_pwrlevel;

	/* if thermal power is higher than usecase power
	 * leave level at use case.  Otherwise go down to
	 * thermal power level
	 */
	if (therm_pwr_level > uc_pwr_level)
		ret_level = uc_pwr_level;
	else
		ret_level = therm_pwr_level;

	/* adjust the power level */
	/* force to lowsvs, minsvs not supported */
	if (ret_level == POWER_LEVEL_MIN_SVS)
		ret_level = POWER_LEVEL_LOW_SVS;

	pr_debug("%s therm=%d active=%d uc=%d set level=%d\n", __func__,
		therm_pwr_level, active_pwr_level, uc_pwr_level, ret_level);

	return ret_level;
}

static int npu_set_power_level(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct npu_pwrlevel *pwrlevel;
	int i, ret = 0;
	uint32_t pwr_level_to_set;

	if (!pwr->pwr_vote_num) {
		pr_err("power is not enabled during set request\n");
		return -EINVAL;
	}

	/* get power level to set */
	pwr_level_to_set = npu_calc_power_level(npu_dev);

	/* if the same as current, dont do anything */
	if (pwr_level_to_set == pwr->active_pwrlevel)
		return 0;

	pr_debug("setting power level to [%d]\n", pwr_level_to_set);

	pwr->active_pwrlevel = pwr_level_to_set;
	pwrlevel = &npu_dev->pwrctrl.pwrlevels[pwr->active_pwrlevel];

	for (i = 0; i < npu_dev->core_clk_num; i++) {
		if (npu_is_exclude_rate_clock(
			npu_dev->core_clks[i].clk_name))
			continue;

		if (npu_dev->host_ctx.fw_state == FW_DISABLED) {
			if (npu_is_post_clock(
				npu_dev->core_clks[i].clk_name))
				continue;
		}

		pr_debug("requested rate of clock [%s] to [%ld]\n",
			npu_dev->core_clks[i].clk_name, pwrlevel->clk_freq[i]);

		ret = clk_set_rate(npu_dev->core_clks[i].clk,
			pwrlevel->clk_freq[i]);
		if (ret) {
			pr_debug("clk_set_rate %s to %ld failed with %d\n",
				npu_dev->core_clks[i].clk_name,
				pwrlevel->clk_freq[i], ret);
			break;
		}
	}

	return ret;
}

int npu_set_uc_power_level(struct npu_device *npu_dev,
	uint32_t perf_mode)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	if (perf_mode == PERF_MODE_DEFAULT)
		pwr->uc_pwrlevel = POWER_LEVEL_NOMINAL;
	else
		pwr->uc_pwrlevel = perf_mode - 1;

	return npu_set_power_level(npu_dev);
}

/* -------------------------------------------------------------------------
 * Bandwidth Related
 * -------------------------------------------------------------------------
 */
static void npu_save_bw_registers(struct npu_device *npu_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npu_saved_bw_registers); i++) {
		npu_saved_bw_registers[i].val = REGR(npu_dev,
			npu_saved_bw_registers[i].off);
		npu_saved_bw_registers[i].valid = true;
	}
}

static void npu_restore_bw_registers(struct npu_device *npu_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npu_saved_bw_registers); i++) {
		if (npu_saved_bw_registers[i].valid) {
			REGW(npu_dev, npu_saved_bw_registers[i].off,
				npu_saved_bw_registers[i].val);
			npu_saved_bw_registers[i].valid = false;
		}
	}
}

static void npu_suspend_devbw(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	int ret;

	if (pwr->bwmon_enabled) {
		pwr->bwmon_enabled = 0;
		ret = devfreq_suspend_devbw(pwr->devbw);
		if (ret)
			pr_err("devfreq_suspend_devbw failed rc:%d\n",
				ret);
		npu_save_bw_registers(npu_dev);
	}
}

static void npu_resume_devbw(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	int ret;

	if (!pwr->bwmon_enabled) {
		pwr->bwmon_enabled = 1;
		npu_restore_bw_registers(npu_dev);
		ret = devfreq_resume_devbw(pwr->devbw);

		if (ret)
			pr_err("devfreq_resume_devbw failed rc:%d\n", ret);
	}
}

/* -------------------------------------------------------------------------
 * Clocks Related
 * -------------------------------------------------------------------------
 */
static bool npu_is_post_clock(const char *clk_name)
{
	int ret = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(npu_post_clocks); i++) {
		if (!strcmp(clk_name, npu_post_clocks[i])) {
			ret = true;
			break;
		}
	}
	return ret;
}

static bool npu_is_exclude_rate_clock(const char *clk_name)
{
	int ret = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(npu_exclude_rate_clocks); i++) {
		if (!strcmp(clk_name, npu_exclude_rate_clocks[i])) {
			ret = true;
			break;
		}
	}
	return ret;
}

static int npu_enable_core_clocks(struct npu_device *npu_dev, bool post_pil)
{
	int i, rc = 0;
	struct npu_clk *core_clks = npu_dev->core_clks;
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct npu_pwrlevel *pwrlevel =
		&npu_dev->pwrctrl.pwrlevels[pwr->active_pwrlevel];

	for (i = 0; i < npu_dev->core_clk_num; i++) {
		if (post_pil) {
			if (!npu_is_post_clock(core_clks[i].clk_name))
				continue;
		} else {
			if (npu_is_post_clock(core_clks[i].clk_name))
				continue;
		}

		pr_debug("enabling clock %s\n", core_clks[i].clk_name);

		rc = clk_prepare_enable(core_clks[i].clk);
		if (rc) {
			pr_err("%s enable failed\n",
				core_clks[i].clk_name);
			break;
		}

		if (npu_is_exclude_rate_clock(core_clks[i].clk_name))
			continue;

		pr_debug("setting rate of clock %s to %ld\n",
			core_clks[i].clk_name, pwrlevel->clk_freq[i]);

		rc = clk_set_rate(core_clks[i].clk,
			pwrlevel->clk_freq[i]);
		/* not fatal error, keep using previous clk rate */
		if (rc) {
			pr_err("clk_set_rate %s to %ld failed\n",
				core_clks[i].clk_name,
				pwrlevel->clk_freq[i]);
			rc = 0;
		}
	}

	if (rc) {
		for (i--; i >= 0; i--) {
			if (post_pil) {
				if (!npu_is_post_clock(core_clks[i].clk_name))
					continue;
			} else {
				if (npu_is_post_clock(core_clks[i].clk_name))
					continue;
			}
			pr_debug("disabling clock %s\n", core_clks[i].clk_name);
			clk_disable_unprepare(core_clks[i].clk);
		}
	}

	return rc;
}

static void npu_disable_core_clocks(struct npu_device *npu_dev)
{
	int i = 0;
	struct npu_clk *core_clks = npu_dev->core_clks;

	for (i = (npu_dev->core_clk_num)-1; i >= 0 ; i--) {
		if (npu_dev->host_ctx.fw_state == FW_DISABLED) {
			if (npu_is_post_clock(npu_dev->core_clks[i].clk_name))
				continue;
		}

		pr_debug("disabling clock %s\n", core_clks[i].clk_name);
		clk_disable_unprepare(core_clks[i].clk);
	}
}

/* -------------------------------------------------------------------------
 * Thermal Functions
 * -------------------------------------------------------------------------
 */
static int npu_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct npu_device *npu_dev = cdev->devdata;
	struct npu_thermalctrl *thermalctrl = &npu_dev->thermalctrl;

	pr_debug("enter %s thermal max state=%lu\n", __func__,
		thermalctrl->max_state);

	*state = thermalctrl->max_state;

	return 0;
}

static int npu_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct npu_device *npu_dev = cdev->devdata;
	struct npu_thermalctrl *thermal = &npu_dev->thermalctrl;

	pr_debug("enter %s thermal current state=%lu\n", __func__,
		thermal->current_state);

	*state = thermal->current_state;

	return 0;
}

static int
npu_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct npu_device *npu_dev = cdev->devdata;
	struct npu_thermalctrl *thermal = &npu_dev->thermalctrl;
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	pr_debug("enter %s request state=%lu\n", __func__, state);
	if (state > thermal->max_state)
		return -EINVAL;

	thermal->current_state = state;
	thermal->pwr_level =  pwr->max_pwrlevel - state;

	return npu_set_power_level(npu_dev);
}

/* -------------------------------------------------------------------------
 * Regulator Related
 * -------------------------------------------------------------------------
 */
static int npu_enable_regulators(struct npu_device *npu_dev)
{
	int i = 0;
	int rc = 0;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_regulator *regulators = npu_dev->regulators;

	if (!host_ctx->power_vote_num) {
		for (i = 0; i < npu_dev->regulator_num; i++) {
			rc = regulator_enable(regulators[i].regulator);
			if (rc < 0) {
				pr_err("%s enable failed\n",
					regulators[i].regulator_name);
				break;
			}
			pr_debug("regulator %s enabled\n",
				regulators[i].regulator_name);
		}
	}
	host_ctx->power_vote_num++;
	return rc;
}

static void npu_disable_regulators(struct npu_device *npu_dev)
{
	int i = 0;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_regulator *regulators = npu_dev->regulators;

	if (host_ctx->power_vote_num > 0) {
		for (i = 0; i < npu_dev->regulator_num; i++) {
			regulator_disable(regulators[i].regulator);
			pr_debug("regulator %s disabled\n",
				regulators[i].regulator_name);
		}
		host_ctx->power_vote_num--;
	}
}

/* -------------------------------------------------------------------------
 * Interrupt Related
 * -------------------------------------------------------------------------
 */
int npu_enable_irq(struct npu_device *npu_dev)
{
	int i;

	/* clear pending irq state */
	REGW(npu_dev, NPU_MASTERn_IPC_IRQ_OUT(0), 0x0);
	REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_CLEAR(0), NPU_ERROR_IRQ_MASK);
	REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_ENABLE(0), NPU_ERROR_IRQ_MASK);

	for (i = 0; i < NPU_MAX_IRQ; i++) {
		if (npu_dev->irq[i].irq != 0) {
			enable_irq(npu_dev->irq[i].irq);
			pr_debug("enable irq %d\n", npu_dev->irq[i].irq);
		}
	}

	return 0;
}

void npu_disable_irq(struct npu_device *npu_dev)
{
	int i;

	for (i = 0; i < NPU_MAX_IRQ; i++) {
		if (npu_dev->irq[i].irq != 0) {
			disable_irq(npu_dev->irq[i].irq);
			pr_debug("disable irq %d\n", npu_dev->irq[i].irq);
		}
	}

	REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_ENABLE(0), 0);
	/* clear pending irq state */
	REGW(npu_dev, NPU_MASTERn_IPC_IRQ_OUT(0), 0x0);
	REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_CLEAR(0), NPU_ERROR_IRQ_MASK);
}

/* -------------------------------------------------------------------------
 * System Cache
 * -------------------------------------------------------------------------
 */
int npu_enable_sys_cache(struct npu_device *npu_dev)
{
	int rc = 0;
	uint32_t reg_val = 0;

	if (!npu_dev->host_ctx.sys_cache_disable) {
		npu_dev->sys_cache = llcc_slice_getd(&(npu_dev->pdev->dev),
			"npu");
		if (IS_ERR_OR_NULL(npu_dev->sys_cache)) {
			pr_debug("unable to init sys cache\n");
			npu_dev->sys_cache = NULL;
			return -ENODEV;
		}

		/* set npu side regs - program SCID */
		reg_val = NPU_CACHE_ATTR_IDn___POR | SYS_CACHE_SCID;

		REGW(npu_dev, NPU_CACHE_ATTR_IDn(0), reg_val);
		REGW(npu_dev, NPU_CACHE_ATTR_IDn(1), reg_val);
		REGW(npu_dev, NPU_CACHE_ATTR_IDn(2), reg_val);
		REGW(npu_dev, NPU_CACHE_ATTR_IDn(3), reg_val);
		REGW(npu_dev, NPU_CACHE_ATTR_IDn(4), reg_val);

		pr_debug("prior to activate sys cache\n");
		rc = llcc_slice_activate(npu_dev->sys_cache);
		if (rc)
			pr_err("failed to activate sys cache\n");
		else
			pr_debug("sys cache activated\n");
	}

	return rc;
}

void npu_disable_sys_cache(struct npu_device *npu_dev)
{
	int rc = 0;

	if (!npu_dev->host_ctx.sys_cache_disable) {
		if (npu_dev->sys_cache) {
			rc = llcc_slice_deactivate(npu_dev->sys_cache);
			if (rc) {
				pr_err("failed to deactivate sys cache\n");
				return;
			}
			pr_debug("sys cache deactivated\n");
			llcc_slice_putd(npu_dev->sys_cache);
			npu_dev->sys_cache = NULL;
		}
	}
}

/* -------------------------------------------------------------------------
 * Open/Close
 * -------------------------------------------------------------------------
 */
static int npu_open(struct inode *inode, struct file *file)
{
	struct npu_device *npu_dev = container_of(inode->i_cdev,
		struct npu_device, cdev);

	file->private_data = npu_dev;

	return 0;
}

static int npu_close(struct inode *inode, struct file *file)
{
	return 0;
}

/* -------------------------------------------------------------------------
 * IOCTL Implementations
 * -------------------------------------------------------------------------
 */
static int npu_get_info(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_get_info_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_get_info(npu_dev, &req);

	if (ret) {
		pr_err("npu_host_get_info failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		pr_err("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_map_buf(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_map_buf_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_map_buf(npu_dev, &req);

	if (ret) {
		pr_err("npu_host_map_buf failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		pr_err("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_unmap_buf(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_unmap_buf_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_unmap_buf(npu_dev, &req);

	if (ret) {
		pr_err("npu_host_unmap_buf failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		pr_err("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_load_network(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_load_network_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	pr_debug("network load with perf request %d\n", req.perf_mode);

	ret = npu_host_load_network(npu_dev, &req);
	if (ret) {
		pr_err("network load failed: %d\n", ret);
		return -EFAULT;
	}

	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		pr_err("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_load_network_v2(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_load_network_ioctl_v2 req;
	void __user *argp = (void __user *)arg;
	struct msm_npu_patch_info_v2 *patch_info = NULL;
	int ret;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	if (req.patch_info_num > MSM_NPU_MAX_PATCH_LAYER_NUM) {
		pr_err("Invalid patch info num %d[max:%d]\n",
			req.patch_info_num, MSM_NPU_MAX_PATCH_LAYER_NUM);
		return -EINVAL;
	}

	if (req.patch_info_num) {
		patch_info = kmalloc_array(req.patch_info_num,
			sizeof(*patch_info), GFP_KERNEL);
		if (!patch_info)
			return -ENOMEM;

		copy_from_user(patch_info,
			(void __user *)req.patch_info,
			req.patch_info_num * sizeof(*patch_info));
	}

	pr_debug("network load with perf request %d\n", req.perf_mode);

	ret = npu_host_load_network_v2(npu_dev, &req, patch_info);
	if (ret) {
		pr_err("network load failed: %d\n", ret);
	} else {
		ret = copy_to_user(argp, &req, sizeof(req));
		if (ret)
			pr_err("fail to copy to user\n");
	}

	kfree(patch_info);
	return ret;
}

static int npu_unload_network(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_unload_network_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_unload_network(npu_dev, &req);

	if (ret) {
		pr_err("npu_host_unload_network failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		pr_err("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_exec_network(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_exec_network_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_exec_network(npu_dev, &req);

	if (ret) {
		pr_err("npu_host_exec_network failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		pr_err("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_exec_network_v2(struct npu_device *npu_dev, unsigned long arg)
{
	struct msm_npu_exec_network_ioctl_v2 req;
	void __user *argp = (void __user *)arg;
	struct msm_npu_patch_buf_info *patch_buf_info = NULL;
	int ret;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("fail to copy from user\n");
		return -EFAULT;
	}

	if (req.patch_buf_info_num > MSM_NPU_MAX_PATCH_LAYER_NUM) {
		pr_err("Invalid patch buf info num %d[max:%d]\n",
			req.patch_buf_info_num, MSM_NPU_MAX_PATCH_LAYER_NUM);
		return -EINVAL;
	}

	if (req.stats_buf_size > MSM_NPU_MAX_STATS_BUF_SIZE) {
		pr_err("Invalid stats buffer size %d max %d\n",
			req.stats_buf_size, MSM_NPU_MAX_STATS_BUF_SIZE);
		return -EINVAL;
	}

	if (req.patch_buf_info_num) {
		patch_buf_info = kmalloc_array(req.patch_buf_info_num,
			sizeof(*patch_buf_info), GFP_KERNEL);
		if (!patch_buf_info)
			return -ENOMEM;

		copy_from_user(patch_buf_info,
			(void __user *)req.patch_buf_info,
			req.patch_buf_info_num * sizeof(*patch_buf_info));
	}

	ret = npu_host_exec_network_v2(npu_dev, &req, patch_buf_info);
	if (ret) {
		pr_err("npu_host_exec_network failed\n");
	} else {
		ret = copy_to_user(argp, &req, sizeof(req));
		if (ret)
			pr_err("fail to copy to user\n");
	}

	kfree(patch_buf_info);
	return ret;
}

static long npu_ioctl(struct file *file, unsigned int cmd,
						 unsigned long arg)
{
	int ret = -ENOIOCTLCMD;
	struct npu_device *npu_dev = file->private_data;

	switch (cmd) {
	case MSM_NPU_GET_INFO:
		ret = npu_get_info(npu_dev, arg);
		break;
	case MSM_NPU_MAP_BUF:
		ret = npu_map_buf(npu_dev, arg);
		break;
	case MSM_NPU_UNMAP_BUF:
		ret = npu_unmap_buf(npu_dev, arg);
		break;
	case MSM_NPU_LOAD_NETWORK:
		ret = npu_load_network(npu_dev, arg);
		break;
	case MSM_NPU_LOAD_NETWORK_V2:
		ret = npu_load_network_v2(npu_dev, arg);
		break;
	case MSM_NPU_UNLOAD_NETWORK:
		ret = npu_unload_network(npu_dev, arg);
		break;
	case MSM_NPU_EXEC_NETWORK:
		ret = npu_exec_network(npu_dev, arg);
		break;
	case MSM_NPU_EXEC_NETWORK_V2:
		ret = npu_exec_network_v2(npu_dev, arg);
		break;
	default:
		pr_err("unexpected IOCTL %x\n", cmd);
	}

	return ret;
}

/* -------------------------------------------------------------------------
 * Device Tree Parsing
 * -------------------------------------------------------------------------
 */
static int npu_parse_dt_clock(struct npu_device *npu_dev)
{
	int rc = 0;
	uint32_t i, j;
	const char *clock_name;
	int num_clk;
	struct npu_clk *core_clks = npu_dev->core_clks;
	struct platform_device *pdev = npu_dev->pdev;

	num_clk = of_property_count_strings(pdev->dev.of_node,
			"clock-names");
	if (num_clk <= 0) {
		pr_err("clocks are not defined\n");
		rc = -EINVAL;
		goto clk_err;
	}
	if (num_clk != NUM_TOTAL_CLKS) {
		pr_err("number of clocks is invalid [%d] should be [%d]\n",
			num_clk, NUM_TOTAL_CLKS);
		rc = -EINVAL;
		goto clk_err;
	}

	npu_dev->core_clk_num = num_clk;
	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(pdev->dev.of_node, "clock-names",
							i, &clock_name);
		for (j = 0; j < num_clk; j++) {
			if (!strcmp(npu_clock_order[j], clock_name))
				break;
		}
		if (j == num_clk) {
			pr_err("clock is not in ordered list\n");
			rc = -EINVAL;
			goto clk_err;
		}
		strlcpy(core_clks[j].clk_name, clock_name,
			sizeof(core_clks[j].clk_name));
		core_clks[j].clk = devm_clk_get(&pdev->dev, clock_name);
		if (IS_ERR(core_clks[j].clk)) {
			pr_err("unable to get clk: %s\n", clock_name);
			rc = -EINVAL;
			break;
		}
	}

clk_err:
	return rc;
}

static int npu_parse_dt_regulator(struct npu_device *npu_dev)
{
	int rc = 0;
	uint32_t i;
	const char *name;
	int num;
	struct npu_regulator *regulators = npu_dev->regulators;
	struct platform_device *pdev = npu_dev->pdev;

	num = of_property_count_strings(pdev->dev.of_node,
			"qcom,proxy-reg-names");
	if (num <= 0) {
		rc = -EINVAL;
		pr_err("regulator not defined\n");
		goto regulator_err;
	}
	if (num > NPU_MAX_REGULATOR_NUM) {
		rc = -EINVAL;
		pr_err("regulator number %d is over the limit %d\n", num,
			NPU_MAX_REGULATOR_NUM);
		num = NPU_MAX_REGULATOR_NUM;
	}

	npu_dev->regulator_num = num;
	for (i = 0; i < num; i++) {
		of_property_read_string_index(pdev->dev.of_node,
			"qcom,proxy-reg-names", i, &name);
		strlcpy(regulators[i].regulator_name, name,
				sizeof(regulators[i].regulator_name));
		regulators[i].regulator = devm_regulator_get(&pdev->dev, name);
		if (IS_ERR(regulators[i].regulator)) {
			pr_err("unable to get regulator: %s\n", name);
			rc = -EINVAL;
			break;
		}
	}

regulator_err:
	return rc;
}

static int npu_of_parse_pwrlevels(struct npu_device *npu_dev,
		struct device_node *node)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct device_node *child;
	uint32_t init_level = 0;
	const char *clock_name;
	struct platform_device *pdev = npu_dev->pdev;

	pwr->num_pwrlevels = 0;

	for_each_available_child_of_node(node, child) {
		uint32_t i = 0;
		uint32_t j = 0;
		uint32_t index;
		uint32_t clk_array_values[NUM_TOTAL_CLKS];
		uint32_t clk_rate;
		struct npu_pwrlevel *level;

		if (of_property_read_u32(child, "reg", &index))
			return -EINVAL;

		if (index >= NPU_MAX_PWRLEVELS) {
			pr_err("pwrlevel index %d is out of range\n",
				index);
			continue;
		}

		if (index >= pwr->num_pwrlevels)
			pwr->num_pwrlevels = index + 1;

		if (of_property_read_u32_array(child, "clk-freq",
			clk_array_values, npu_dev->core_clk_num)) {
			pr_err("pwrlevel index %d read clk-freq failed %d\n",
				index, npu_dev->core_clk_num);
			return -EINVAL;
		}

		/* sort */
		level = &pwr->pwrlevels[index];
		for (i = 0; i < npu_dev->core_clk_num; i++) {
			of_property_read_string_index(pdev->dev.of_node,
				"clock-names", i, &clock_name);

			if (npu_is_exclude_rate_clock(clock_name))
				continue;

			for (j = 0; j < npu_dev->core_clk_num; j++) {
				if (!strcmp(npu_clock_order[j],
					clock_name))
					break;
			}

			if (j == npu_dev->core_clk_num) {
				pr_err("pwrlevel clock is not in ordered list\n");
				return -EINVAL;
			}

			clk_rate = clk_round_rate(npu_dev->core_clks[j].clk,
				clk_array_values[i]);
			pr_debug("clk %s rate [%ld]:[%ld]\n", clock_name,
				clk_array_values[i], clk_rate);
			level->clk_freq[j] = clk_rate;
		}
	}

	of_property_read_u32(node, "initial-pwrlevel", &init_level);

	if (init_level >= pwr->num_pwrlevels)
		init_level = 0;

	pr_debug("init power level %d\n", init_level);
	pwr->active_pwrlevel = init_level;
	pwr->default_pwrlevel = init_level;
	pwr->uc_pwrlevel = init_level;
	pwr->min_pwrlevel = 0;
	pwr->max_pwrlevel = pwr->num_pwrlevels - 1;

	return 0;
}

static int npu_pwrctrl_init(struct npu_device *npu_dev)
{
	struct platform_device *pdev = npu_dev->pdev;
	struct device_node *node;
	int ret = 0;
	struct platform_device *p2dev;
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	/* Power levels */
	node = of_find_node_by_name(pdev->dev.of_node, "qcom,npu-pwrlevels");

	if (!node) {
		pr_err("unable to find 'qcom,npu-pwrlevels'\n");
		return -EINVAL;
	}

	ret = npu_of_parse_pwrlevels(npu_dev, node);
	if (ret)
		return ret;

	/* Parse Bandwidth */
	node = of_parse_phandle(pdev->dev.of_node,
				"qcom,npubw-dev", 0);

	if (node) {
		/* Set to 1 initially - we assume bwmon is on */
		pwr->bwmon_enabled = 1;
		p2dev = of_find_device_by_node(node);
		if (p2dev) {
			pwr->devbw = &p2dev->dev;
		} else {
			pr_err("parser power level failed\n");
			ret = -EINVAL;
			return ret;
		}
	} else {
		pr_err("bwdev is not defined in dts\n");
		pwr->devbw = NULL;
		ret = -EINVAL;
	}

	return ret;
}

static int npu_thermalctrl_init(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct npu_thermalctrl *thermalctrl = &npu_dev->thermalctrl;
	int ret = 0;

	thermalctrl->max_state = pwr->max_pwrlevel;
	thermalctrl->current_state = 0;
	return ret;
}

static int npu_irq_init(struct npu_device *npu_dev)
{
	unsigned long irq_type;
	int ret = 0, i;

	memcpy(npu_dev->irq, npu_irq_info, sizeof(npu_irq_info));
	for (i = 0; i < NPU_MAX_IRQ; i++) {
		irq_type = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
		npu_dev->irq[i].irq = platform_get_irq_byname(
			npu_dev->pdev, npu_dev->irq[i].name);
		if (npu_dev->irq[i].irq < 0) {
			pr_err("get_irq for %s failed\n\n",
				npu_dev->irq[i].name);
			ret = -EINVAL;
			break;
		}

		pr_debug("irq %s: %d\n", npu_dev->irq[i].name,
			npu_dev->irq[i].irq);
		irq_set_status_flags(npu_dev->irq[i].irq,
						IRQ_NOAUTOEN);
		ret = devm_request_irq(&npu_dev->pdev->dev,
				npu_dev->irq[i].irq, npu_intr_hdler,
				irq_type, npu_dev->irq[i].name,
				npu_dev);
		if (ret) {
			pr_err("devm_request_irq(%s:%d) failed\n",
				npu_dev->irq[i].name,
				npu_dev->irq[i].irq);
			break;
		}
	}

	return ret;
}

/* -------------------------------------------------------------------------
 * Probe/Remove
 * -------------------------------------------------------------------------
 */
static int npu_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res = 0;
	struct npu_device *npu_dev = 0;
	struct thermal_cooling_device *tcdev = 0;

	npu_dev = devm_kzalloc(&pdev->dev,
		sizeof(struct npu_device), GFP_KERNEL);
	if (!npu_dev)
		return -EFAULT;

	npu_dev->pdev = pdev;

	platform_set_drvdata(pdev, npu_dev);
	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "npu_base");
	if (!res) {
		pr_err("unable to get NPU reg base address\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	npu_dev->reg_size = resource_size(res);

	rc = npu_parse_dt_regulator(npu_dev);
	if (rc)
		goto error_get_dev_num;

	rc = npu_parse_dt_clock(npu_dev);
	if (rc)
		goto error_get_dev_num;

	rc = npu_pwrctrl_init(npu_dev);
	if (rc)
		goto error_get_dev_num;

	rc = npu_thermalctrl_init(npu_dev);
	if (rc)
		goto error_get_dev_num;

	rc = npu_irq_init(npu_dev);
	if (rc)
		goto error_get_dev_num;

	npu_dev->npu_base = devm_ioremap(&pdev->dev, res->start,
					npu_dev->reg_size);
	if (unlikely(!npu_dev->npu_base)) {
		pr_err("unable to map NPU base\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}

	npu_dev->npu_phys = res->start;
	pr_debug("hw base phy address=0x%x virt=%pK\n",
		npu_dev->npu_phys, npu_dev->npu_base);

	/* character device might be optional */
	rc = alloc_chrdev_region(&npu_dev->dev_num, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		pr_err("alloc_chrdev_region failed: %d\n", rc);
		goto error_get_dev_num;
	}

	npu_dev->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(npu_dev->class)) {
		rc = PTR_ERR(npu_dev->class);
		pr_err("class_create failed: %d\n", rc);
		goto error_class_create;
	}

	npu_dev->device = device_create(npu_dev->class, NULL,
		npu_dev->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(npu_dev->device)) {
		rc = PTR_ERR(npu_dev->device);
		pr_err("device_create failed: %d\n", rc);
		goto error_class_device_create;
	}

	cdev_init(&npu_dev->cdev, &npu_fops);
	rc = cdev_add(&npu_dev->cdev,
			MKDEV(MAJOR(npu_dev->dev_num), 0), 1);
	if (rc < 0) {
		pr_err("cdev_add failed %d\n", rc);
		goto error_cdev_add;
	}
	dev_set_drvdata(npu_dev->device, npu_dev);
	pr_debug("drvdata %pK %pK\n", dev_get_drvdata(&pdev->dev),
		dev_get_drvdata(npu_dev->device));
	rc = sysfs_create_group(&npu_dev->device->kobj, &npu_fs_attr_group);
	if (rc) {
		pr_err("unable to register npu sysfs nodes\n");
		goto error_res_init;
	}

	if (IS_ENABLED(CONFIG_THERMAL)) {
		tcdev = thermal_of_cooling_device_register(pdev->dev.of_node,
							  "npu", npu_dev,
							  &npu_cooling_ops);
		if (IS_ERR(tcdev)) {
			dev_err(&pdev->dev,
				"npu: failed to register npu as cooling device");
			rc = PTR_ERR(tcdev);
			goto error_driver_init;
		}
		npu_dev->tcdev = tcdev;
		thermal_cdev_update(tcdev);
	}

	rc = npu_debugfs_init(npu_dev);
	if (rc)
		goto error_driver_init;

	npu_dev->smmu_ctx.attach_cnt = 0;
	npu_dev->smmu_ctx.mmu_mapping = arm_iommu_create_mapping(
		pdev->dev.bus, DDR_MAPPED_START_ADDR, DDR_MAPPED_SIZE);
	if (IS_ERR(npu_dev->smmu_ctx.mmu_mapping)) {
		pr_err("iommu create mapping failed\n");
		rc = -ENOMEM;
		npu_dev->smmu_ctx.mmu_mapping = NULL;
		goto error_driver_init;
	}

	rc = arm_iommu_attach_device(&(npu_dev->pdev->dev),
			npu_dev->smmu_ctx.mmu_mapping);
	if (rc) {
		pr_err("arm_iommu_attach_device failed\n");
		goto error_driver_init;
	}

	INIT_LIST_HEAD(&(npu_dev->mapped_buffers.list));

	rc = npu_host_init(npu_dev);
	if (rc) {
		pr_err("unable to init host\n");
		goto error_driver_init;
	}

	return rc;
error_driver_init:
	sysfs_remove_group(&npu_dev->device->kobj, &npu_fs_attr_group);
	arm_iommu_detach_device(&(npu_dev->pdev->dev));
	if (!npu_dev->smmu_ctx.mmu_mapping)
		arm_iommu_release_mapping(npu_dev->smmu_ctx.mmu_mapping);
	sysfs_remove_group(&npu_dev->device->kobj, &npu_fs_attr_group);
error_res_init:
	cdev_del(&npu_dev->cdev);
error_cdev_add:
	device_destroy(npu_dev->class, npu_dev->dev_num);
error_class_device_create:
	class_destroy(npu_dev->class);
error_class_create:
	unregister_chrdev_region(npu_dev->dev_num, 1);
error_get_dev_num:
	return rc;
}

static int npu_remove(struct platform_device *pdev)
{
	struct npu_device *npu_dev;

	npu_dev = platform_get_drvdata(pdev);
	thermal_cooling_device_unregister(npu_dev->tcdev);
	npu_debugfs_deinit(npu_dev);
	npu_host_deinit(npu_dev);
	arm_iommu_detach_device(&(npu_dev->pdev->dev));
	arm_iommu_release_mapping(npu_dev->smmu_ctx.mmu_mapping);
	sysfs_remove_group(&npu_dev->device->kobj, &npu_fs_attr_group);
	cdev_del(&npu_dev->cdev);
	device_destroy(npu_dev->class, npu_dev->dev_num);
	class_destroy(npu_dev->class);
	unregister_chrdev_region(npu_dev->dev_num, 1);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

/* -------------------------------------------------------------------------
 * Suspend/Resume
 * -------------------------------------------------------------------------
 */
#if defined(CONFIG_PM)
static int npu_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int npu_resume(struct platform_device *dev)
{
	return 0;
}
#endif

/* -------------------------------------------------------------------------
 * Module Entry Points
 * -------------------------------------------------------------------------
 */
static int __init npu_init(void)
{
	int rc;

	rc = platform_driver_register(&npu_driver);
	if (rc)
		pr_err("register failed %d\n", rc);
	return rc;
}

static void __exit npu_exit(void)
{
	platform_driver_unregister(&npu_driver);
}

module_init(npu_init);
module_exit(npu_exit);

MODULE_DEVICE_TABLE(of, npu_dt_match);
MODULE_DESCRIPTION("MSM NPU driver");
MODULE_LICENSE("GPL v2");
MODULE_INFO(intree, "Y");
