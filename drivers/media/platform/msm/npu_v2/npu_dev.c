/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/sizes.h>
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
#define DDR_MAPPED_START_ADDR   0x00000000
#define DDR_MAPPED_SIZE         (SZ_1G * 4ULL)

#define MBOX_OP_TIMEOUTMS 1000

/* -------------------------------------------------------------------------
 * File Scope Prototypes
 * -------------------------------------------------------------------------
 */
static int npu_enable_regulators(struct npu_device *npu_dev);
static void npu_disable_regulators(struct npu_device *npu_dev);
static int npu_enable_clocks(struct npu_device *npu_dev, bool post_pil);
static void npu_disable_clocks(struct npu_device *npu_dev, bool post_pil);
static int npu_enable_core_clocks(struct npu_device *npu_dev);
static void npu_disable_core_clocks(struct npu_device *npu_dev);
static uint32_t npu_calc_power_level(struct npu_device *npu_dev);
static ssize_t caps_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t pwr_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t pwr_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t perf_mode_override_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t perf_mode_override_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t dcvs_mode_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t dcvs_mode_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t boot_store(struct device *dev,
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
static int npu_get_info(struct npu_client *client, unsigned long arg);
static int npu_map_buf(struct npu_client *client, unsigned long arg);
static int npu_unmap_buf(struct npu_client *client,
	unsigned long arg);
static int npu_load_network_v2(struct npu_client *client,
	unsigned long arg);
static int npu_unload_network(struct npu_client *client,
	unsigned long arg);
static int npu_exec_network_v2(struct npu_client *client,
	unsigned long arg);
static int npu_receive_event(struct npu_client *client,
	unsigned long arg);
static int npu_set_fw_state(struct npu_client *client, uint32_t enable);
static int npu_set_property(struct npu_client *client,
	unsigned long arg);
static int npu_get_property(struct npu_client *client,
	unsigned long arg);
static long npu_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg);
static unsigned int npu_poll(struct file *filp, struct poll_table_struct *p);
static int npu_parse_dt_clock(struct npu_device *npu_dev);
static int npu_parse_dt_regulator(struct npu_device *npu_dev);
static int npu_parse_dt_bw(struct npu_device *npu_dev);
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
static const char * const npu_post_clocks[] = {
};

static const char * const npu_exclude_rate_clocks[] = {
	"xo_clk",
	"bwmon_clk",
	"bto_core_clk",
	"llm_xo_clk",
	"dpm_xo_clk",
	"rsc_xo_clk",
	"dsp_bwmon_clk",
	"dl_dpm_clk",
	"dpm_temp_clk",
	"dsp_bwmon_ahb_clk",
	"cal_hm0_perf_cnt_clk",
	"cal_hm1_perf_cnt_clk",
	"dsp_ahbs_clk",
	"axi_clk",
	"ahb_clk",
	"dma_clk",
	"atb_clk",
	"s2p_clk",
};

static const char * const npu_require_reset_clocks[] = {
	"dpm_temp_clk",
	"llm_temp_clk",
	"llm_curr_clk",
};

static const struct npu_irq npu_irq_info[] = {
	{"ipc_irq", 0, IRQF_TRIGGER_RISING | IRQF_ONESHOT, npu_ipc_intr_hdlr},
	{"general_irq", 0,  IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		npu_general_intr_hdlr},
	{"error_irq", 0,  IRQF_TRIGGER_HIGH | IRQF_ONESHOT, npu_err_intr_hdlr},
	{"wdg_bite_irq", 0,  IRQF_TRIGGER_RISING | IRQF_ONESHOT,
		npu_wdg_intr_hdlr}
};

static struct npu_device *g_npu_dev;

/* -------------------------------------------------------------------------
 * Entry Points for Probe
 * -------------------------------------------------------------------------
 */
/* Sys FS */
static DEVICE_ATTR_RO(caps);
static DEVICE_ATTR_RW(pwr);
static DEVICE_ATTR_RW(perf_mode_override);
static DEVICE_ATTR_WO(boot);
static DEVICE_ATTR_RW(dcvs_mode);

static struct attribute *npu_fs_attrs[] = {
	&dev_attr_caps.attr,
	&dev_attr_pwr.attr,
	&dev_attr_perf_mode_override.attr,
	&dev_attr_dcvs_mode.attr,
	&dev_attr_boot.attr,
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
		.of_match_table = npu_dt_match,
		.pm = NULL,
	},
};

static const struct file_operations npu_fops = {
	.owner = THIS_MODULE,
	.open = npu_open,
	.release = npu_close,
	.unlocked_ioctl = npu_ioctl,
#ifdef CONFIG_COMPAT
	 .compat_ioctl = npu_ioctl,
#endif
	.poll = npu_poll,
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
static ssize_t caps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t ret = 0;
	struct npu_device *npu_dev = dev_get_drvdata(dev);

	if (!npu_enable_core_power(npu_dev)) {
		if (scnprintf(buf, PAGE_SIZE, "hw_version :0x%X",
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
static ssize_t pwr_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			(pwr->pwr_vote_num > 0) ? "on" : "off");
}

static ssize_t pwr_store(struct device *dev,
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
 * SysFS - Power State
 * -------------------------------------------------------------------------
 */
static ssize_t perf_mode_override_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pwr->perf_mode_override);
}

static ssize_t perf_mode_override_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct npu_client client;
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	uint32_t val;
	int rc;

	rc = kstrtou32(buf, 10, &val);
	if (rc) {
		NPU_ERR("Invalid input for perf mode setting\n");
		return -EINVAL;
	}

	val = min(val, npu_dev->pwrctrl.num_pwrlevels);
	NPU_INFO("setting perf mode to %d\n", val);
	client.npu_dev = npu_dev;
	npu_host_set_perf_mode(&client, 0, val);

	return count;
}

static ssize_t dcvs_mode_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pwr->dcvs_mode);
}

static ssize_t dcvs_mode_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	struct msm_npu_property prop;
	uint32_t val;
	int ret = 0;

	ret = kstrtou32(buf, 10, &val);
	if (ret) {
		NPU_ERR("Invalid input for dcvs mode setting\n");
		return -EINVAL;
	}

	val = min(val, (uint32_t)(npu_dev->pwrctrl.num_pwrlevels - 1));
	NPU_DBG("sysfs: setting dcvs_mode to %d\n", val);

	prop.prop_id = MSM_NPU_PROP_ID_DCVS_MODE;
	prop.num_of_params = 1;
	prop.network_hdl = 0;
	prop.prop_param[0] = val;

	ret = npu_host_set_fw_property(npu_dev, &prop);
	if (ret) {
		NPU_ERR("npu_host_set_fw_property failed %d\n", ret);
		return ret;
	}

	npu_dev->pwrctrl.dcvs_mode = val;

	return count;
}
/* -------------------------------------------------------------------------
 * SysFS - npu_boot
 * -------------------------------------------------------------------------
 */
static ssize_t boot_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct npu_device *npu_dev = dev_get_drvdata(dev);
	bool enable = false;
	int rc;

	if (strtobool(buf, &enable) < 0)
		return -EINVAL;

	if (enable) {
		NPU_DBG("%s: load fw\n", __func__);
		rc = load_fw(npu_dev);
		if (rc) {
			NPU_ERR("fw init failed\n");
			return rc;
		}
	} else {
		NPU_DBG("%s: unload fw\n", __func__);
		unload_fw(npu_dev);
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

	mutex_lock(&npu_dev->dev_lock);
	NPU_DBG("Enable core power %d\n", pwr->pwr_vote_num);
	if (!pwr->pwr_vote_num) {
		ret = npu_set_bw(npu_dev, 100, 100);
		if (ret)
			goto fail;

		ret = npu_enable_regulators(npu_dev);
		if (ret) {
			npu_set_bw(npu_dev, 0, 0);
			goto fail;
		}

		ret = npu_enable_core_clocks(npu_dev);
		if (ret) {
			npu_disable_regulators(npu_dev);
			npu_set_bw(npu_dev, 0, 0);
			goto fail;
		}
		npu_resume_devbw(npu_dev);
	}
	pwr->pwr_vote_num++;
fail:
	mutex_unlock(&npu_dev->dev_lock);

	return ret;
}

void npu_disable_core_power(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	mutex_lock(&npu_dev->dev_lock);
	NPU_DBG("Disable core power %d\n", pwr->pwr_vote_num);
	if (!pwr->pwr_vote_num) {
		mutex_unlock(&npu_dev->dev_lock);
		return;
	}

	pwr->pwr_vote_num--;
	if (!pwr->pwr_vote_num) {
		npu_suspend_devbw(npu_dev);
		npu_disable_core_clocks(npu_dev);
		npu_disable_regulators(npu_dev);
		npu_set_bw(npu_dev, 0, 0);
		pwr->active_pwrlevel = pwr->default_pwrlevel;
		pwr->uc_pwrlevel = pwr->max_pwrlevel;
		pwr->cdsprm_pwrlevel = pwr->max_pwrlevel;
		pwr->cur_dcvs_activity = pwr->num_pwrlevels;
		NPU_DBG("setting back to power level=%d\n",
			pwr->active_pwrlevel);
	}
	mutex_unlock(&npu_dev->dev_lock);
}

static int npu_enable_core_clocks(struct npu_device *npu_dev)
{
	return npu_enable_clocks(npu_dev, false);
}

static void npu_disable_core_clocks(struct npu_device *npu_dev)
{
	return npu_disable_clocks(npu_dev, false);
}

int npu_enable_post_pil_clocks(struct npu_device *npu_dev)
{
	return npu_enable_clocks(npu_dev, true);
}

void npu_disable_post_pil_clocks(struct npu_device *npu_dev)
{
	npu_disable_clocks(npu_dev, true);
}

static uint32_t npu_power_level_from_index(struct npu_device *npu_dev,
	uint32_t index)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	if (index >= pwr->num_pwrlevels)
		index = pwr->num_pwrlevels - 1;

	return pwr->pwrlevels[index].pwr_level;
}

static uint32_t npu_power_level_to_index(struct npu_device *npu_dev,
	uint32_t pwr_lvl)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	int i;

	for (i = 0; i < pwr->num_pwrlevels; i++) {
		if (pwr->pwrlevels[i].pwr_level > pwr_lvl)
			break;
	}


	return i == 0 ? 0 : i - 1;
}

static uint32_t npu_calc_power_level(struct npu_device *npu_dev)
{
	uint32_t ret_level;
	uint32_t therm_pwr_level = npu_dev->thermalctrl.pwr_level;
	uint32_t active_pwr_level = npu_dev->pwrctrl.active_pwrlevel;
	uint32_t uc_pwr_level = npu_dev->pwrctrl.uc_pwrlevel;

	/*
	 * pick the lowese power level between thermal power and usecase power
	 * settings
	 */
	ret_level = min(therm_pwr_level, uc_pwr_level);
	NPU_DBG("therm=%d active=%d uc=%d set level=%d\n",
		therm_pwr_level, active_pwr_level, uc_pwr_level,
		ret_level);

	return ret_level;
}

int npu_set_power_level(struct npu_device *npu_dev, bool notify_cxlimit)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct npu_pwrlevel *pwrlevel;
	int i, ret = 0;
	uint32_t pwr_level_to_set, pwr_level_to_cdsprm, pwr_level_idx;

	/* get power level to set */
	pwr_level_to_set = npu_calc_power_level(npu_dev);
	pwr_level_to_cdsprm = pwr_level_to_set;

	if (!pwr->pwr_vote_num) {
		NPU_DBG("power is not enabled during set request\n");
		pwr->active_pwrlevel = min(pwr_level_to_set,
			npu_dev->pwrctrl.cdsprm_pwrlevel);
		return 0;
	}

	pwr_level_to_set = min(pwr_level_to_set,
		npu_dev->pwrctrl.cdsprm_pwrlevel);

	/* if the same as current, dont do anything */
	if (pwr_level_to_set == pwr->active_pwrlevel) {
		NPU_DBG("power level %d doesn't change\n", pwr_level_to_set);
		return 0;
	}

	NPU_DBG("setting power level to [%d]\n", pwr_level_to_set);
	pwr_level_idx = npu_power_level_to_index(npu_dev, pwr_level_to_set);
	pwrlevel = &npu_dev->pwrctrl.pwrlevels[pwr_level_idx];

	ret = npu_host_notify_fw_pwr_state(npu_dev, pwr_level_to_set, false);
	/*
	 * if new power level is lower than current power level,
	 * ignore fw notification failure, and apply the new power level.
	 * otherwise remain the current power level.
	 */

	if (ret) {
		NPU_WARN("notify fw new power level [%d] failed\n",
			pwr_level_to_set);
		if (pwr->active_pwrlevel < pwr_level_to_set) {
			NPU_WARN("remain current power level [%d]\n",
				pwr->active_pwrlevel);
			return 0;
		}

		ret = 0;
	}

	for (i = 0; i < npu_dev->core_clk_num; i++) {
		if (npu_is_exclude_rate_clock(
			npu_dev->core_clks[i].clk_name))
			continue;

		if (npu_dev->host_ctx.fw_state != FW_ENABLED) {
			if (npu_is_post_clock(
				npu_dev->core_clks[i].clk_name))
				continue;
		}

		NPU_DBG("requested rate of clock [%s] to [%ld]\n",
			npu_dev->core_clks[i].clk_name, pwrlevel->clk_freq[i]);

		ret = clk_set_rate(npu_dev->core_clks[i].clk,
			pwrlevel->clk_freq[i]);
		if (ret) {
			NPU_DBG("clk_set_rate %s to %ld failed with %d\n",
				npu_dev->core_clks[i].clk_name,
				pwrlevel->clk_freq[i], ret);
			break;
		}
	}

	if (!ret) {
		ret = npu_host_notify_fw_pwr_state(npu_dev,
			pwr_level_to_set, true);
		if (ret)
			NPU_WARN("notify fw new power level [%d] failed\n",
				pwr_level_to_set);

		ret = 0;
	}

	pwr->active_pwrlevel = pwr_level_to_set;
	return ret;
}

int npu_set_uc_power_level(struct npu_device *npu_dev,
	uint32_t perf_mode)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	uint32_t uc_pwrlevel_to_set;

	uc_pwrlevel_to_set = npu_power_level_from_index(npu_dev,
		perf_mode - 1);

	if (uc_pwrlevel_to_set > pwr->max_pwrlevel)
		uc_pwrlevel_to_set = pwr->max_pwrlevel;

	pwr->uc_pwrlevel = uc_pwrlevel_to_set;
	return npu_set_power_level(npu_dev, true);
}

/* -------------------------------------------------------------------------
 * Bandwidth Monitor Related
 * -------------------------------------------------------------------------
 */
static void npu_suspend_devbw(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	int ret, i;

	if (pwr->bwmon_enabled && (pwr->devbw_num > 0)) {
		for (i = 0; i < pwr->devbw_num; i++) {
			ret = devfreq_suspend_devbw(pwr->devbw[i]);
			if (ret)
				NPU_ERR("devfreq_suspend_devbw failed rc:%d\n",
					ret);
		}
		pwr->bwmon_enabled = 0;
	}
}

static void npu_resume_devbw(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	int ret, i;

	if (!pwr->bwmon_enabled && (pwr->devbw_num > 0)) {
		for (i = 0; i < pwr->devbw_num; i++) {
			ret = devfreq_resume_devbw(pwr->devbw[i]);
			if (ret)
				NPU_ERR("devfreq_resume_devbw failed rc:%d\n",
					ret);
		}
		pwr->bwmon_enabled = 1;
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

static bool npu_clk_need_reset(const char *clk_name)
{
	int ret = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(npu_require_reset_clocks); i++) {
		if (!strcmp(clk_name, npu_require_reset_clocks[i])) {
			ret = true;
			break;
		}
	}

	return ret;
}

static int npu_enable_clocks(struct npu_device *npu_dev, bool post_pil)
{
	int i, rc = 0;
	struct npu_clk *core_clks = npu_dev->core_clks;
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct npu_pwrlevel *pwrlevel;
	uint32_t pwrlevel_to_set, pwrlevel_idx;

	pwrlevel_to_set = pwr->active_pwrlevel;
	pwrlevel_idx = npu_power_level_to_index(npu_dev, pwrlevel_to_set);
	pwrlevel = &pwr->pwrlevels[pwrlevel_idx];
	for (i = 0; i < npu_dev->core_clk_num; i++) {
		if (post_pil) {
			if (!npu_is_post_clock(core_clks[i].clk_name))
				continue;
		} else {
			if (npu_is_post_clock(core_clks[i].clk_name))
				continue;
		}

		if (core_clks[i].reset) {
			rc = reset_control_deassert(core_clks[i].reset);
			if (rc)
				NPU_WARN("deassert %s reset failed\n",
					core_clks[i].clk_name);
		}

		rc = clk_prepare_enable(core_clks[i].clk);
		if (rc) {
			NPU_ERR("%s enable failed\n",
				core_clks[i].clk_name);
			break;
		}

		if (npu_is_exclude_rate_clock(core_clks[i].clk_name))
			continue;

		rc = clk_set_rate(core_clks[i].clk,
			pwrlevel->clk_freq[i]);
		/* not fatal error, keep using previous clk rate */
		if (rc) {
			NPU_ERR("clk_set_rate %s to %ld failed\n",
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
			clk_disable_unprepare(core_clks[i].clk);

			if (core_clks[i].reset) {
				rc = reset_control_assert(core_clks[i].reset);
				if (rc)
					NPU_WARN("assert %s reset failed\n",
						core_clks[i].clk_name);
			}
		}
	}

	return rc;
}

static void npu_disable_clocks(struct npu_device *npu_dev, bool post_pil)
{
	int i, rc = 0;
	struct npu_clk *core_clks = npu_dev->core_clks;

	for (i = npu_dev->core_clk_num - 1; i >= 0 ; i--) {
		if (post_pil) {
			if (!npu_is_post_clock(core_clks[i].clk_name))
				continue;
		} else {
			if (npu_is_post_clock(core_clks[i].clk_name))
				continue;
		}

		/* set clock rate to 0 before disabling it */
		if (!npu_is_exclude_rate_clock(core_clks[i].clk_name)) {
			rc = clk_set_rate(core_clks[i].clk, 0);
			if (rc) {
				NPU_ERR("clk_set_rate %s to 0 failed\n",
					core_clks[i].clk_name);
			}
		}

		clk_disable_unprepare(core_clks[i].clk);

		if (core_clks[i].reset) {
			rc = reset_control_assert(core_clks[i].reset);
			if (rc)
				NPU_WARN("assert %s reset failed\n",
					core_clks[i].clk_name);
		}
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

	NPU_DBG("thermal max state=%lu\n", thermalctrl->max_state);

	*state = thermalctrl->max_state;

	return 0;
}

static int npu_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct npu_device *npu_dev = cdev->devdata;
	struct npu_thermalctrl *thermal = &npu_dev->thermalctrl;

	NPU_DBG("thermal current state=%lu\n", thermal->current_state);

	*state = thermal->current_state;

	return 0;
}

static int
npu_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct npu_device *npu_dev = cdev->devdata;
	struct npu_thermalctrl *thermal = &npu_dev->thermalctrl;

	NPU_DBG("request state=%lu\n", state);
	if (state > thermal->max_state)
		return -EINVAL;

	thermal->current_state = state;
	thermal->pwr_level =  npu_power_level_from_index(npu_dev,
		thermal->max_state - state);
	return npu_host_update_power(npu_dev);
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
				NPU_ERR("%s enable failed\n",
					regulators[i].regulator_name);
				break;
			}
		}
	}

	if (rc) {
		for (i--; i >= 0; i--)
			regulator_disable(regulators[i].regulator);
	} else {
		host_ctx->power_vote_num++;
	}
	return rc;
}

static void npu_disable_regulators(struct npu_device *npu_dev)
{
	int i = 0;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_regulator *regulators = npu_dev->regulators;

	if (host_ctx->power_vote_num > 0) {
		for (i = 0; i < npu_dev->regulator_num; i++)
			regulator_disable(regulators[i].regulator);

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
	uint32_t reg_val;

	if (npu_dev->irq_enabled) {
		NPU_WARN("Irq is enabled already\n");
		return 0;
	}

	/* setup general irq */
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_MASTERn_GENERAL_IRQ_CLEAR(0),
		RSC_SHUTDOWN_REQ_IRQ_ENABLE | RSC_BRINGUP_REQ_IRQ_ENABLE);
	reg_val = npu_cc_reg_read(npu_dev,
		NPU_CC_NPU_MASTERn_GENERAL_IRQ_OWNER(0));
	reg_val |= RSC_SHUTDOWN_REQ_IRQ_ENABLE | RSC_BRINGUP_REQ_IRQ_ENABLE;
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_MASTERn_GENERAL_IRQ_OWNER(0),
		reg_val);
	reg_val = npu_cc_reg_read(npu_dev,
		NPU_CC_NPU_MASTERn_GENERAL_IRQ_ENABLE(0));
	reg_val |= RSC_SHUTDOWN_REQ_IRQ_ENABLE | RSC_BRINGUP_REQ_IRQ_ENABLE;
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_MASTERn_GENERAL_IRQ_ENABLE(0),
		reg_val);
	for (i = 0; i < NPU_MAX_IRQ; i++)
		if (npu_dev->irq[i].irq != 0)
			enable_irq(npu_dev->irq[i].irq);

	npu_dev->irq_enabled = true;
	NPU_DBG("irq enabled\n");

	return 0;
}

void npu_disable_irq(struct npu_device *npu_dev)
{
	int i;
	uint32_t reg_val;

	if (!npu_dev->irq_enabled) {
		NPU_WARN("irq is not enabled\n");
		return;
	}

	for (i = 0; i < NPU_MAX_IRQ; i++)
		if (npu_dev->irq[i].irq != 0)
			disable_irq(npu_dev->irq[i].irq);

	reg_val = npu_cc_reg_read(npu_dev,
		NPU_CC_NPU_MASTERn_GENERAL_IRQ_OWNER(0));
	reg_val &= ~(RSC_SHUTDOWN_REQ_IRQ_ENABLE | RSC_BRINGUP_REQ_IRQ_ENABLE);
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_MASTERn_GENERAL_IRQ_OWNER(0),
		reg_val);
	reg_val = npu_cc_reg_read(npu_dev,
		NPU_CC_NPU_MASTERn_GENERAL_IRQ_ENABLE(0));
	reg_val &= ~(RSC_SHUTDOWN_REQ_IRQ_ENABLE | RSC_BRINGUP_REQ_IRQ_ENABLE);
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_MASTERn_GENERAL_IRQ_ENABLE(0),
		reg_val);
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_MASTERn_GENERAL_IRQ_CLEAR(0),
		RSC_SHUTDOWN_REQ_IRQ_ENABLE | RSC_BRINGUP_REQ_IRQ_ENABLE);
	npu_dev->irq_enabled = false;
	NPU_DBG("irq disabled\n");
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
		//npu_dev->sys_cache = llcc_slice_getd(LLCC_NPU);
		if (IS_ERR_OR_NULL(npu_dev->sys_cache)) {
			NPU_WARN("unable to init sys cache\n");
			npu_dev->sys_cache = NULL;
			npu_dev->host_ctx.sys_cache_disable = true;
			return 0;
		}

		/* set npu side regs - program SCID */
		reg_val = REGR(npu_dev, NPU_CACHEMAP0_ATTR_IDn(0));
		reg_val = (reg_val & ~NPU_CACHEMAP_SCID_MASK) | SYS_CACHE_SCID;

		REGW(npu_dev, NPU_CACHEMAP0_ATTR_IDn(0), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_IDn(1), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_IDn(2), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_IDn(3), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_IDn(4), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_METADATA_IDn(0), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_METADATA_IDn(1), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_METADATA_IDn(2), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_METADATA_IDn(3), reg_val);
		REGW(npu_dev, NPU_CACHEMAP0_ATTR_METADATA_IDn(4), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_IDn(0), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_IDn(1), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_IDn(2), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_IDn(3), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_IDn(4), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_METADATA_IDn(0), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_METADATA_IDn(1), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_METADATA_IDn(2), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_METADATA_IDn(3), reg_val);
		REGW(npu_dev, NPU_CACHEMAP1_ATTR_METADATA_IDn(4), reg_val);

		rc = llcc_slice_activate(npu_dev->sys_cache);
		if (rc) {
			NPU_ERR("failed to activate sys cache\n");
			llcc_slice_putd(npu_dev->sys_cache);
			npu_dev->sys_cache = NULL;
			rc = 0;
		}
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
				NPU_ERR("failed to deactivate sys cache\n");
				return;
			}
			NPU_DBG("sys cache deactivated\n");
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
	struct npu_client *client;

	client = kmalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->npu_dev = npu_dev;
	init_waitqueue_head(&client->wait);
	mutex_init(&client->list_lock);
	INIT_LIST_HEAD(&client->evt_list);
	INIT_LIST_HEAD(&(client->mapped_buffer_list));
	file->private_data = client;

	return 0;
}

static int npu_close(struct inode *inode, struct file *file)
{
	struct npu_client *client = file->private_data;
	struct npu_kevent *kevent;

	npu_host_cleanup_networks(client);

	while (!list_empty(&client->evt_list)) {
		kevent = list_first_entry(&client->evt_list,
			struct npu_kevent, list);
		list_del(&kevent->list);
		kfree(kevent);
	}

	mutex_destroy(&client->list_lock);
	kfree(client);
	return 0;
}

/* -------------------------------------------------------------------------
 * IOCTL Implementations
 * -------------------------------------------------------------------------
 */
static int npu_get_info(struct npu_client *client, unsigned long arg)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct msm_npu_get_info_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_get_info(npu_dev, &req);

	if (ret) {
		NPU_ERR("npu_host_get_info failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_map_buf(struct npu_client *client, unsigned long arg)
{
	struct msm_npu_map_buf_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_map_buf(client, &req);

	if (ret) {
		NPU_ERR("npu_host_map_buf failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_unmap_buf(struct npu_client *client, unsigned long arg)
{
	struct msm_npu_unmap_buf_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_unmap_buf(client, &req);

	if (ret) {
		NPU_ERR("npu_host_unmap_buf failed\n");
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}


static int npu_load_network_v2(struct npu_client *client,
	unsigned long arg)
{
	struct msm_npu_load_network_ioctl_v2 req;
	struct msm_npu_unload_network_ioctl unload_req;
	void __user *argp = (void __user *)arg;
	struct msm_npu_patch_info_v2 *patch_info = NULL;
	int ret;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	if ((req.patch_info_num > NPU_MAX_PATCH_NUM) ||
		(req.patch_info_num == 0)) {
		NPU_ERR("Invalid patch info num %d[max:%d]\n",
			req.patch_info_num, NPU_MAX_PATCH_NUM);
		return -EINVAL;
	}

	if (req.patch_info_num) {
		patch_info = kmalloc_array(req.patch_info_num,
			sizeof(*patch_info), GFP_KERNEL);
		if (!patch_info)
			return -ENOMEM;

		ret = copy_from_user(patch_info,
			(void __user *)req.patch_info,
			req.patch_info_num * sizeof(*patch_info));
		if (ret) {
			NPU_ERR("fail to copy patch info\n");
			kfree(patch_info);
			return -EFAULT;
		}
	}

	NPU_DBG("network load with perf request %d\n", req.perf_mode);

	ret = npu_host_load_network_v2(client, &req, patch_info);

	kfree(patch_info);
	if (ret) {
		NPU_ERR("npu_host_load_network_v2 failed %d\n", ret);
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		NPU_ERR("fail to copy to user\n");
		ret = -EFAULT;
		unload_req.network_hdl = req.network_hdl;
		npu_host_unload_network(client, &unload_req);
	}

	return ret;
}

static int npu_unload_network(struct npu_client *client,
	unsigned long arg)
{
	struct msm_npu_unload_network_ioctl req;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	ret = npu_host_unload_network(client, &req);

	if (ret) {
		NPU_ERR("npu_host_unload_network failed %d\n", ret);
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		NPU_ERR("fail to copy to user\n");
		return -EFAULT;
	}
	return 0;
}

static int npu_exec_network_v2(struct npu_client *client,
	unsigned long arg)
{
	struct msm_npu_exec_network_ioctl_v2 req;
	void __user *argp = (void __user *)arg;
	struct msm_npu_patch_buf_info *patch_buf_info = NULL;
	int ret;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	if ((req.patch_buf_info_num > NPU_MAX_PATCH_NUM) ||
		(req.patch_buf_info_num == 0)) {
		NPU_ERR("Invalid patch buf info num %d[max:%d]\n",
			req.patch_buf_info_num, NPU_MAX_PATCH_NUM);
		return -EINVAL;
	}

	if (req.stats_buf_size > NPU_MAX_STATS_BUF_SIZE) {
		NPU_ERR("Invalid stats buffer size %d max %d\n",
			req.stats_buf_size, NPU_MAX_STATS_BUF_SIZE);
		return -EINVAL;
	}

	if (req.patch_buf_info_num) {
		patch_buf_info = kmalloc_array(req.patch_buf_info_num,
			sizeof(*patch_buf_info), GFP_KERNEL);
		if (!patch_buf_info)
			return -ENOMEM;

		ret = copy_from_user(patch_buf_info,
			(void __user *)req.patch_buf_info,
			req.patch_buf_info_num * sizeof(*patch_buf_info));
		if (ret) {
			NPU_ERR("fail to copy patch buf info\n");
			kfree(patch_buf_info);
			return -EFAULT;
		}
	}

	ret = npu_host_exec_network_v2(client, &req, patch_buf_info);

	kfree(patch_buf_info);
	if (ret) {
		NPU_ERR("npu_host_exec_network_v2 failed %d\n", ret);
		return ret;
	}

	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		NPU_ERR("fail to copy to user\n");
		ret = -EFAULT;
	}

	return ret;
}

static int npu_receive_event(struct npu_client *client,
	unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct npu_kevent *kevt;
	int ret = 0;

	mutex_lock(&client->list_lock);
	if (list_empty(&client->evt_list)) {
		NPU_ERR("event list is empty\n");
		ret = -EINVAL;
	} else {
		kevt = list_first_entry(&client->evt_list,
			struct npu_kevent, list);
		list_del(&kevt->list);
		npu_process_kevent(client, kevt);
		ret = copy_to_user(argp, &kevt->evt,
			sizeof(struct msm_npu_event));
		if (ret) {
			NPU_ERR("fail to copy to user\n");
			ret = -EFAULT;
		}
		kfree(kevt);
	}
	mutex_unlock(&client->list_lock);

	return ret;
}

static int npu_set_fw_state(struct npu_client *client, uint32_t enable)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int rc = 0;

	if (host_ctx->network_num > 0) {
		NPU_ERR("Need to unload network first\n");
		mutex_unlock(&npu_dev->dev_lock);
		return -EINVAL;
	}

	if (enable) {
		NPU_DBG("enable fw\n");
		rc = enable_fw(npu_dev);
		if (rc) {
			NPU_ERR("enable fw failed\n");
		} else {
			host_ctx->npu_init_cnt++;
			NPU_DBG("npu_init_cnt %d\n",
				host_ctx->npu_init_cnt);
			/* set npu to lowest power level */
			if (npu_set_uc_power_level(npu_dev, 1))
				NPU_WARN("Failed to set uc power level\n");
		}
	} else if (host_ctx->npu_init_cnt > 0) {
		NPU_DBG("disable fw\n");
		disable_fw(npu_dev);
		host_ctx->npu_init_cnt--;
		NPU_DBG("npu_init_cnt %d\n", host_ctx->npu_init_cnt);
	} else {
		NPU_ERR("can't disable fw %d\n", host_ctx->npu_init_cnt);
	}

	return rc;
}

static int npu_set_property(struct npu_client *client,
	unsigned long arg)
{
	struct msm_npu_property prop;
	void __user *argp = (void __user *)arg;
	int ret = -EINVAL;

	ret = copy_from_user(&prop, argp, sizeof(prop));
	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	switch (prop.prop_id) {
	case MSM_NPU_PROP_ID_FW_STATE:
		ret = npu_set_fw_state(client,
			(uint32_t)prop.prop_param[0]);
		break;
	case MSM_NPU_PROP_ID_PERF_MODE:
		ret = npu_host_set_perf_mode(client,
			(uint32_t)prop.network_hdl,
			(uint32_t)prop.prop_param[0]);
		break;
	default:
		ret = npu_host_set_fw_property(client->npu_dev, &prop);
		if (ret)
			NPU_ERR("npu_host_set_fw_property failed\n");
		break;
	}

	return ret;
}

static int npu_get_property(struct npu_client *client,
	unsigned long arg)
{
	struct msm_npu_property prop;
	void __user *argp = (void __user *)arg;
	int ret = -EINVAL;
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	ret = copy_from_user(&prop, argp, sizeof(prop));
	if (ret) {
		NPU_ERR("fail to copy from user\n");
		return -EFAULT;
	}

	switch (prop.prop_id) {
	case MSM_NPU_PROP_ID_FW_STATE:
		prop.prop_param[0] = host_ctx->fw_state;
		break;
	case MSM_NPU_PROP_ID_PERF_MODE:
		prop.prop_param[0] = npu_host_get_perf_mode(client,
			(uint32_t)prop.network_hdl);
		break;
	case MSM_NPU_PROP_ID_PERF_MODE_MAX:
		prop.prop_param[0] = npu_dev->pwrctrl.num_pwrlevels;
		break;
	case MSM_NPU_PROP_ID_DRV_VERSION:
		prop.prop_param[0] = 0;
		break;
	case MSM_NPU_PROP_ID_HARDWARE_VERSION:
		prop.prop_param[0] = npu_dev->hw_version;
		break;
	case MSM_NPU_PROP_ID_IPC_QUEUE_INFO:
		ret = npu_host_get_ipc_queue_size(npu_dev,
			prop.prop_param[0]);
		if (ret < 0) {
			NPU_ERR("Can't get ipc queue %d size\n",
				prop.prop_param[0]);
			return ret;
		}

		prop.prop_param[1] = ret;
		break;
	case MSM_NPU_PROP_ID_DRV_FEATURE:
		prop.prop_param[0] = MSM_NPU_FEATURE_MULTI_EXECUTE |
			MSM_NPU_FEATURE_ASYNC_EXECUTE;
		break;
	default:
		ret = npu_host_get_fw_property(client->npu_dev, &prop);
		if (ret) {
			NPU_ERR("npu_host_set_fw_property failed\n");
			return ret;
		}
		break;
	}

	ret = copy_to_user(argp, &prop, sizeof(prop));
	if (ret) {
		NPU_ERR("fail to copy to user\n");
		return -EFAULT;
	}

	return ret;
}

static long npu_ioctl(struct file *file, unsigned int cmd,
						 unsigned long arg)
{
	int ret = -ENOIOCTLCMD;
	struct npu_client *client = file->private_data;

	switch (cmd) {
	case MSM_NPU_GET_INFO:
		ret = npu_get_info(client, arg);
		break;
	case MSM_NPU_MAP_BUF:
		ret = npu_map_buf(client, arg);
		break;
	case MSM_NPU_UNMAP_BUF:
		ret = npu_unmap_buf(client, arg);
		break;
	case MSM_NPU_LOAD_NETWORK:
		NPU_ERR("npu_load_network_v1 is no longer supported\n");
		ret = -ENOTTY;
		break;
	case MSM_NPU_LOAD_NETWORK_V2:
		ret = npu_load_network_v2(client, arg);
		break;
	case MSM_NPU_UNLOAD_NETWORK:
		ret = npu_unload_network(client, arg);
		break;
	case MSM_NPU_EXEC_NETWORK:
		NPU_ERR("npu_exec_network_v1 is no longer supported\n");
		ret = -ENOTTY;
		break;
	case MSM_NPU_EXEC_NETWORK_V2:
		ret = npu_exec_network_v2(client, arg);
		break;
	case MSM_NPU_RECEIVE_EVENT:
		ret = npu_receive_event(client, arg);
		break;
	case MSM_NPU_SET_PROP:
		ret = npu_set_property(client, arg);
		break;
	case MSM_NPU_GET_PROP:
		ret = npu_get_property(client, arg);
		break;
	default:
		NPU_ERR("unexpected IOCTL %x\n", cmd);
	}

	return ret;
}

static unsigned int npu_poll(struct file *filp, struct poll_table_struct *p)
{
	struct npu_client *client = filp->private_data;
	int rc = 0;

	poll_wait(filp, &client->wait, p);

	mutex_lock(&client->list_lock);
	if (!list_empty(&client->evt_list)) {
		NPU_DBG("poll cmd done\n");
		rc = POLLIN | POLLRDNORM;
	}
	mutex_unlock(&client->list_lock);

	return rc;
}

/* -------------------------------------------------------------------------
 * Device Tree Parsing
 * -------------------------------------------------------------------------
 */
static int npu_parse_dt_clock(struct npu_device *npu_dev)
{
	int rc = 0;
	uint32_t i;
	const char *clock_name;
	int num_clk;
	struct npu_clk *core_clks = npu_dev->core_clks;
	struct platform_device *pdev = npu_dev->pdev;
	struct reset_control *reset;

	num_clk = of_property_count_strings(pdev->dev.of_node,
			"clock-names");
	if (num_clk <= 0) {
		NPU_ERR("clocks are not defined\n");
		rc = -EINVAL;
		goto clk_err;
	} else if (num_clk > NUM_MAX_CLK_NUM) {
		NPU_ERR("number of clocks %d exceeds limit\n", num_clk);
		rc = -EINVAL;
		goto clk_err;
	}

	npu_dev->core_clk_num = num_clk;
	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(pdev->dev.of_node, "clock-names",
							i, &clock_name);
		strlcpy(core_clks[i].clk_name, clock_name,
			sizeof(core_clks[i].clk_name));
		core_clks[i].clk = devm_clk_get(&pdev->dev, clock_name);
		if (IS_ERR(core_clks[i].clk)) {
			if (PTR_ERR(core_clks[i].clk) != -EPROBE_DEFER)
				NPU_ERR("unable to get clk: %s\n", clock_name);
			rc = PTR_ERR(core_clks[i].clk);
			break;
		}

		if (npu_clk_need_reset(clock_name)) {
			reset = devm_reset_control_get(&pdev->dev, clock_name);
			if (IS_ERR(reset))
				NPU_WARN("no reset for %s %d\n", clock_name,
					PTR_ERR(reset));
			else
				core_clks[i].reset = reset;
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
		NPU_ERR("regulator not defined\n");
		goto regulator_err;
	}
	if (num > NPU_MAX_REGULATOR_NUM) {
		rc = -EINVAL;
		NPU_ERR("regulator number %d is over the limit %d\n", num,
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
			NPU_ERR("unable to get regulator: %s\n", name);
			rc = -EINVAL;
			break;
		}
	}

regulator_err:
	return rc;
}

static int npu_parse_dt_bw(struct npu_device *npu_dev)
{
	int ret, len;
	uint32_t ports[2];
	struct platform_device *pdev = npu_dev->pdev;
	struct npu_bwctrl *bwctrl = &npu_dev->bwctrl;

	if (of_find_property(pdev->dev.of_node, "qcom,src-dst-ports", &len)) {
		len /= sizeof(ports[0]);
		if (len != 2) {
			NPU_ERR("Unexpected number of ports\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,src-dst-ports", ports, len);
		if (ret) {
			NPU_ERR("Failed to read bw property\n");
			return ret;
		}
	} else {
		NPU_ERR("can't find bw property\n");
		return -EINVAL;
	}

	bwctrl->bw_levels[0].vectors = &bwctrl->vectors[0];
	bwctrl->bw_levels[1].vectors = &bwctrl->vectors[MAX_PATHS];
	bwctrl->bw_data.usecase = bwctrl->bw_levels;
	bwctrl->bw_data.num_usecases = ARRAY_SIZE(bwctrl->bw_levels);
	bwctrl->bw_data.name = dev_name(&pdev->dev);
	bwctrl->bw_data.active_only = false;

	bwctrl->bw_levels[0].vectors[0].src = ports[0];
	bwctrl->bw_levels[0].vectors[0].dst = ports[1];
	bwctrl->bw_levels[1].vectors[0].src = ports[0];
	bwctrl->bw_levels[1].vectors[0].dst = ports[1];
	bwctrl->bw_levels[0].num_paths = 1;
	bwctrl->bw_levels[1].num_paths = 1;
	bwctrl->num_paths = 1;

	bwctrl->bus_client = msm_bus_scale_register_client(&bwctrl->bw_data);
	if (!bwctrl->bus_client) {
		NPU_ERR("Unable to register bus client\n");
		return -ENODEV;
	}

	NPU_INFO("NPU BW client sets up successfully\n");

	return 0;
}

int npu_set_bw(struct npu_device *npu_dev, int new_ib, int new_ab)
{
	int i, ret;
	struct npu_bwctrl *bwctrl = &npu_dev->bwctrl;

	if (!bwctrl->bus_client) {
		NPU_DBG("bus client doesn't exist\n");
		return 0;
	}

	if (bwctrl->cur_ib == new_ib && bwctrl->cur_ab == new_ab)
		return 0;

	i = (bwctrl->cur_idx + 1) % DBL_BUF;

	bwctrl->bw_levels[i].vectors[0].ib = new_ib * MBYTE;
	bwctrl->bw_levels[i].vectors[0].ab = new_ab / bwctrl->num_paths * MBYTE;
	bwctrl->bw_levels[i].vectors[1].ib = new_ib * MBYTE;
	bwctrl->bw_levels[i].vectors[1].ab = new_ab / bwctrl->num_paths * MBYTE;

	ret = msm_bus_scale_client_update_request(bwctrl->bus_client, i);
	if (ret) {
		NPU_ERR("bandwidth request failed (%d)\n", ret);
	} else {
		bwctrl->cur_idx = i;
		bwctrl->cur_ib = new_ib;
		bwctrl->cur_ab = new_ab;
	}

	return ret;
}

static int npu_of_parse_pwrlevels(struct npu_device *npu_dev,
		struct device_node *node)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct device_node *child;
	uint32_t init_level_index = 0, init_power_level;
	uint32_t fmax, fmax_pwrlvl;

	pwr->num_pwrlevels = 0;
	pwr->min_pwrlevel = NPU_PWRLEVEL_TURBO_L1;
	pwr->max_pwrlevel = NPU_PWRLEVEL_MINSVS;

	for_each_available_child_of_node(node, child) {
		uint32_t i = 0;
		uint32_t index;
		uint32_t pwr_level;
		uint32_t clk_array_values[NUM_MAX_CLK_NUM];
		uint32_t clk_rate;
		struct npu_pwrlevel *level;

		if (of_property_read_u32(child, "reg", &index)) {
			NPU_ERR("Can't find reg property\n");
			return -EINVAL;
		}

		if (of_property_read_u32(child, "vreg", &pwr_level)) {
			NPU_ERR("Can't find vreg property\n");
			return -EINVAL;
		}

		if (index >= NPU_MAX_PWRLEVELS) {
			NPU_ERR("pwrlevel index %d is out of range\n",
				index);
			continue;
		}

		if (index >= pwr->num_pwrlevels)
			pwr->num_pwrlevels = index + 1;

		if (of_property_read_u32_array(child, "clk-freq",
			clk_array_values, npu_dev->core_clk_num)) {
			NPU_ERR("pwrlevel index %d read clk-freq failed %d\n",
				index, npu_dev->core_clk_num);
			return -EINVAL;
		}

		level = &pwr->pwrlevels[index];
		level->pwr_level = pwr_level;
		if (pwr->min_pwrlevel > pwr_level)
			pwr->min_pwrlevel = pwr_level;
		if (pwr->max_pwrlevel < pwr_level)
			pwr->max_pwrlevel = pwr_level;

		for (i = 0; i < npu_dev->core_clk_num; i++) {
			if (npu_is_exclude_rate_clock(
				npu_dev->core_clks[i].clk_name))
				continue;

			clk_rate = clk_round_rate(npu_dev->core_clks[i].clk,
				clk_array_values[i]);
			NPU_DBG("clk %s rate [%u]:[%u]\n",
				npu_dev->core_clks[i].clk_name,
				clk_array_values[i], clk_rate);
			level->clk_freq[i] = clk_rate;
		}
	}

	/* Read FMAX info if available */
	if (npu_dev->qfprom_io.base) {
		fmax = (npu_qfprom_reg_read(npu_dev,
			QFPROM_FMAX_REG_OFFSET) & QFPROM_FMAX_BITS_MASK) >>
			QFPROM_FMAX_BITS_SHIFT;
		NPU_DBG("fmax %x\n", fmax);

		switch (fmax) {
		case 1:
		case 2:
			fmax_pwrlvl = NPU_PWRLEVEL_NOM;
			break;
		case 3:
			fmax_pwrlvl = NPU_PWRLEVEL_SVS_L1;
			break;
		default:
			fmax_pwrlvl = pwr->max_pwrlevel;
			break;
		}

		if (fmax_pwrlvl < pwr->max_pwrlevel)
			pwr->max_pwrlevel = fmax_pwrlvl;
	}

	of_property_read_u32(node, "initial-pwrlevel", &init_level_index);
	NPU_DBG("initial-pwrlevel %d\n", init_level_index);

	if (init_level_index >= pwr->num_pwrlevels)
		init_level_index = pwr->num_pwrlevels - 1;

	init_power_level = npu_power_level_from_index(npu_dev,
		init_level_index);
	if (init_power_level > pwr->max_pwrlevel) {
		init_power_level = pwr->max_pwrlevel;
		NPU_DBG("Adjust init power level to %d\n", init_power_level);
	}

	NPU_DBG("init power level %d max %d min %d\n", init_power_level,
		pwr->max_pwrlevel, pwr->min_pwrlevel);
	pwr->active_pwrlevel = pwr->default_pwrlevel = init_power_level;
	pwr->uc_pwrlevel = pwr->max_pwrlevel;
	pwr->perf_mode_override = 0;
	pwr->cdsprm_pwrlevel = pwr->max_pwrlevel;
	pwr->cur_dcvs_activity = pwr->num_pwrlevels;

	return 0;
}

static int npu_pwrctrl_init(struct npu_device *npu_dev)
{
	struct platform_device *pdev = npu_dev->pdev;
	struct device_node *node;
	int ret = 0, i;
	struct platform_device *p2dev;
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	/* Power levels */
	node = of_find_node_by_name(pdev->dev.of_node, "qcom,npu-pwrlevels");

	if (!node) {
		NPU_ERR("unable to find 'qcom,npu-pwrlevels'\n");
		return -EINVAL;
	}

	ret = npu_of_parse_pwrlevels(npu_dev, node);
	if (ret)
		return ret;

	/* Parse Bandwidth Monitor */
	pwr->devbw_num = of_property_count_strings(pdev->dev.of_node,
			"qcom,npubw-dev-names");
	if (pwr->devbw_num <= 0) {
		NPU_INFO("npubw-dev-names are not defined\n");
		return 0;
	} else if (pwr->devbw_num > NPU_MAX_BW_DEVS) {
		NPU_ERR("number of devbw %d exceeds limit\n", pwr->devbw_num);
		return -EINVAL;
	}

	for (i = 0; i < pwr->devbw_num; i++) {
		node = of_parse_phandle(pdev->dev.of_node,
				"qcom,npubw-devs", i);

		if (node) {
			p2dev = of_find_device_by_node(node);
			of_node_put(node);
			if (p2dev) {
				pwr->devbw[i] = &p2dev->dev;
			} else {
				NPU_ERR("can't find devbw%d\n", i);
				ret = -EINVAL;
				break;
			}
		} else {
			NPU_ERR("can't find devbw node\n");
			ret = -EINVAL;
			break;
		}
	}

	if (ret) {
		/* Allow npu work without bwmon */
		pwr->devbw_num = 0;
		ret = 0;
	} else {
		/* Set to 1 initially - we assume bwmon is on */
		pwr->bwmon_enabled = 1;
	}

	return ret;
}

static int npu_thermalctrl_init(struct npu_device *npu_dev)
{
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct npu_thermalctrl *thermalctrl = &npu_dev->thermalctrl;

	thermalctrl->max_state = pwr->num_pwrlevels - 1;
	thermalctrl->current_state = 0;
	return 0;
}

static int npu_irq_init(struct npu_device *npu_dev)
{
	unsigned long irq_type;
	int ret = 0, i;

	memcpy(npu_dev->irq, npu_irq_info, sizeof(npu_irq_info));
	for (i = 0; i < ARRAY_SIZE(npu_irq_info); i++) {
		irq_type = npu_irq_info[i].irq_type;
		npu_dev->irq[i].irq = platform_get_irq_byname(
			npu_dev->pdev, npu_dev->irq[i].name);
		if (npu_dev->irq[i].irq < 0) {
			NPU_ERR("get_irq for %s failed\n\n",
				npu_dev->irq[i].name);
			ret = -EINVAL;
			break;
		}

		NPU_DBG("irq %s: %d\n", npu_dev->irq[i].name,
			npu_dev->irq[i].irq);
		irq_set_status_flags(npu_dev->irq[i].irq,
						IRQ_NOAUTOEN);
		ret = devm_request_irq(&npu_dev->pdev->dev,
				npu_dev->irq[i].irq, npu_dev->irq[i].handler,
				irq_type, npu_dev->irq[i].name,
				npu_dev);
		if (ret) {
			NPU_ERR("devm_request_irq(%s:%d) failed\n",
				npu_dev->irq[i].name,
				npu_dev->irq[i].irq);
			break;
		}
	}

	return ret;
}

/* -------------------------------------------------------------------------
 * Mailbox
 * -------------------------------------------------------------------------
 */
static int npu_ipcc_bridge_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct ipcc_mbox_chan *ipcc_mbox_chan = chan->con_priv;
	struct npu_device *npu_dev = ipcc_mbox_chan->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	unsigned long flags;

	NPU_DBG("Generating IRQ for signal_id: %u\n",
		ipcc_mbox_chan->signal_id);

	spin_lock_irqsave(&host_ctx->bridge_mbox_lock, flags);
	ipcc_mbox_chan->npu_mbox->send_data_pending = true;
	queue_work(host_ctx->wq, &host_ctx->bridge_mbox_work);
	spin_unlock_irqrestore(&host_ctx->bridge_mbox_lock, flags);

	return 0;
}

static void npu_ipcc_bridge_mbox_shutdown(struct mbox_chan *chan)
{
	struct ipcc_mbox_chan *ipcc_mbox_chan = chan->con_priv;

	chan->con_priv = NULL;
	kfree(ipcc_mbox_chan);
}

static struct mbox_chan *npu_ipcc_bridge_mbox_xlate(
	struct mbox_controller *mbox, const struct of_phandle_args *ph)
{
	int chan_id, i;
	struct npu_device *npu_dev;
	struct mbox_bridge_data *bridge_data;
	struct ipcc_mbox_chan *ipcc_mbox_chan;

	bridge_data = container_of(mbox, struct mbox_bridge_data, mbox);
	if (WARN_ON(!bridge_data))
		return ERR_PTR(-EINVAL);

	npu_dev = bridge_data->priv_data;

	if (ph->args_count != 1)
		return ERR_PTR(-EINVAL);

	for (chan_id = 0; chan_id < mbox->num_chans; chan_id++) {
		ipcc_mbox_chan = bridge_data->chans[chan_id].con_priv;

		if (!ipcc_mbox_chan)
			break;
		else if (ipcc_mbox_chan->signal_id == ph->args[0])
			return ERR_PTR(-EBUSY);
	}

	if (chan_id >= mbox->num_chans)
		return ERR_PTR(-EBUSY);

	/* search for target mailbox */
	for (i = 0; i < NPU_MAX_MBOX_NUM; i++) {
		if (npu_dev->mbox[i].chan &&
			(npu_dev->mbox[i].signal_id == ph->args[0])) {
			NPU_DBG("Find matched target mailbox %d\n", i);
			break;
		}
	}

	if (i == NPU_MAX_MBOX_NUM) {
		NPU_ERR("Can't find matched target mailbox %d\n",
			ph->args[0]);
		return ERR_PTR(-EINVAL);
	}

	ipcc_mbox_chan = kzalloc(sizeof(*ipcc_mbox_chan), GFP_KERNEL);
	if (!ipcc_mbox_chan)
		return ERR_PTR(-ENOMEM);

	ipcc_mbox_chan->signal_id = ph->args[0];
	ipcc_mbox_chan->chan = &bridge_data->chans[chan_id];
	ipcc_mbox_chan->npu_dev = npu_dev;
	ipcc_mbox_chan->chan->con_priv = ipcc_mbox_chan;
	ipcc_mbox_chan->npu_mbox = &npu_dev->mbox[i];

	NPU_DBG("New mailbox channel: %u for signal_id: %u\n",
		chan_id, ipcc_mbox_chan->signal_id);

	return ipcc_mbox_chan->chan;
}

static const struct mbox_chan_ops ipcc_mbox_chan_ops = {
	.send_data = npu_ipcc_bridge_mbox_send_data,
	.shutdown = npu_ipcc_bridge_mbox_shutdown
};

static int npu_setup_ipcc_bridge_mbox(struct npu_device *npu_dev)
{
	int i, j, ret;
	int num_chans = 0;
	struct mbox_controller *mbox;
	struct device_node *client_dn;
	struct of_phandle_args curr_ph;
	struct device *dev = &npu_dev->pdev->dev;
	struct device_node *controller_dn = dev->of_node;
	struct mbox_bridge_data *mbox_data = &npu_dev->mbox_bridge_data;

	NPU_DBG("Setup ipcc brige mbox\n");
	/*
	 * Find out the number of clients interested in this mailbox
	 * and create channels accordingly.
	 */
	for_each_node_with_property(client_dn, "mboxes") {
		if (!of_device_is_available(client_dn)) {
			NPU_DBG("No node available\n");
			continue;
		}
		i = of_count_phandle_with_args(client_dn,
						"mboxes", "#mbox-cells");
		for (j = 0; j < i; j++) {
			ret = of_parse_phandle_with_args(client_dn, "mboxes",
						"#mbox-cells", j, &curr_ph);
			of_node_put(curr_ph.np);
			if (!ret && curr_ph.np == controller_dn) {
				NPU_DBG("Found a client\n");
				num_chans++;
				break;
			}
		}
	}

	/* If no clients are found, skip registering as a mbox controller */
	if (!num_chans) {
		NPU_WARN("Can't find ipcc bridge mbox client\n");
		return 0;
	}

	mbox_data->chans = devm_kcalloc(dev, num_chans,
					sizeof(struct mbox_chan), GFP_KERNEL);
	if (!mbox_data->chans)
		return -ENOMEM;

	mbox_data->priv_data = npu_dev;
	mbox = &mbox_data->mbox;
	mbox->dev = dev;
	mbox->num_chans = num_chans;
	mbox->chans = mbox_data->chans;
	mbox->ops = &ipcc_mbox_chan_ops;
	mbox->of_xlate = npu_ipcc_bridge_mbox_xlate;
	mbox->txdone_irq = false;
	mbox->txdone_poll = false;

	return mbox_controller_register(mbox);
}

static int npu_mbox_init(struct npu_device *npu_dev)
{
	struct platform_device *pdev = npu_dev->pdev;
	struct npu_mbox *mbox = NULL;
	struct property *prop;
	const char *mbox_name;
	uint32_t index = 0;
	int ret = 0;
	struct of_phandle_args curr_ph;

	if (!of_get_property(pdev->dev.of_node, "mbox-names", NULL)  ||
		!of_find_property(pdev->dev.of_node, "mboxes", NULL)) {
		NPU_WARN("requires mbox-names and mboxes property\n");
		return 0;
	}

	of_property_for_each_string(pdev->dev.of_node,
		"mbox-names", prop, mbox_name) {
		NPU_DBG("setup mbox[%d] %s\n", index, mbox_name);
		mbox = &npu_dev->mbox[index];
		mbox->client.dev = &pdev->dev;
		mbox->client.knows_txdone = true;
		mbox->chan = mbox_request_channel(&mbox->client, index);
		if (IS_ERR(mbox->chan)) {
			NPU_WARN("mailbox %s is not available\n", mbox_name);
			mbox->chan = NULL;
		} else if (!strcmp(mbox_name, "aop")) {
			npu_dev->mbox_aop = mbox;
		} else {
			ret = of_parse_phandle_with_args(pdev->dev.of_node,
				"mboxes", "#mbox-cells", index, &curr_ph);
			of_node_put(curr_ph.np);
			if (ret) {
				NPU_WARN("can't get mailbox %s args\n",
					mbox_name);
			} else {
				mbox->signal_id = curr_ph.args[0];
				NPU_DBG("argument for mailbox %x is %x\n",
					mbox_name, curr_ph.args[0]);
			}
		}
		index++;
	}

	return npu_setup_ipcc_bridge_mbox(npu_dev);
}

static void npu_mbox_deinit(struct npu_device *npu_dev)
{
	int i;

	mbox_controller_unregister(&npu_dev->mbox_bridge_data.mbox);

	for (i = 0; i < NPU_MAX_MBOX_NUM; i++) {
		if (!npu_dev->mbox[i].chan)
			continue;

		mbox_free_channel(npu_dev->mbox[i].chan);
		npu_dev->mbox[i].chan = NULL;
	}
}

static int npu_hw_info_init(struct npu_device *npu_dev)
{
	int rc = 0;

	rc = npu_enable_core_power(npu_dev);
	if (rc) {
		NPU_ERR("Failed to enable power\n");
		return rc;
	}

	npu_dev->hw_version = REGR(npu_dev, NPU_HW_VERSION);
	NPU_DBG("NPU_HW_VERSION 0x%x\n", npu_dev->hw_version);
	npu_disable_core_power(npu_dev);

	return rc;
}

/* -------------------------------------------------------------------------
 * Probe/Remove
 * -------------------------------------------------------------------------
 */
static int npu_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res = NULL;
	struct npu_device *npu_dev = NULL;
	struct thermal_cooling_device *tcdev = NULL;

	npu_dev = devm_kzalloc(&pdev->dev,
		sizeof(struct npu_device), GFP_KERNEL);
	if (!npu_dev)
		return -EFAULT;

	npu_dev->pdev = pdev;
	mutex_init(&npu_dev->dev_lock);

	platform_set_drvdata(pdev, npu_dev);
	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "core");
	if (!res) {
		NPU_ERR("unable to get core resource\n");
		rc = -ENODEV;
		goto error_get_dev_num;
	}
	npu_dev->core_io.size = resource_size(res);
	npu_dev->core_io.phy_addr = res->start;
	npu_dev->core_io.base = devm_ioremap(&pdev->dev, res->start,
					npu_dev->core_io.size);
	if (unlikely(!npu_dev->core_io.base)) {
		NPU_ERR("unable to map core\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	NPU_DBG("core phy address=0x%llx virt=%pK\n",
		res->start, npu_dev->core_io.base);

	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "tcm");
	if (!res) {
		NPU_ERR("unable to get tcm resource\n");
		rc = -ENODEV;
		goto error_get_dev_num;
	}
	npu_dev->tcm_io.size = resource_size(res);
	npu_dev->tcm_io.phy_addr = res->start;
	npu_dev->tcm_io.base = devm_ioremap(&pdev->dev, res->start,
					npu_dev->tcm_io.size);
	if (unlikely(!npu_dev->tcm_io.base)) {
		NPU_ERR("unable to map tcm\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	NPU_DBG("tcm phy address=0x%llx virt=%pK\n",
		res->start, npu_dev->tcm_io.base);

	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "cc");
	if (!res) {
		NPU_ERR("unable to get cc resource\n");
		rc = -ENODEV;
		goto error_get_dev_num;
	}
	npu_dev->cc_io.size = resource_size(res);
	npu_dev->cc_io.phy_addr = res->start;
	npu_dev->cc_io.base = devm_ioremap(&pdev->dev, res->start,
					npu_dev->cc_io.size);
	if (unlikely(!npu_dev->cc_io.base)) {
		NPU_ERR("unable to map cc\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	NPU_DBG("cc_io phy address=0x%llx virt=%pK\n",
		res->start, npu_dev->cc_io.base);

	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "tcsr");
	if (!res) {
		NPU_ERR("unable to get tcsr_mutex resource\n");
		rc = -ENODEV;
		goto error_get_dev_num;
	}
	npu_dev->tcsr_io.size = resource_size(res);
	npu_dev->tcsr_io.phy_addr = res->start;
	npu_dev->tcsr_io.base = devm_ioremap(&pdev->dev, res->start,
					npu_dev->tcsr_io.size);
	if (unlikely(!npu_dev->tcsr_io.base)) {
		NPU_ERR("unable to map tcsr\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	NPU_DBG("tcsr phy address=0x%llx virt=%pK\n",
		res->start, npu_dev->tcsr_io.base);

	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "apss_shared");
	if (!res) {
		NPU_ERR("unable to get apss_shared resource\n");
		rc = -ENODEV;
		goto error_get_dev_num;
	}
	npu_dev->apss_shared_io.size = resource_size(res);
	npu_dev->apss_shared_io.phy_addr = res->start;
	npu_dev->apss_shared_io.base = devm_ioremap(&pdev->dev, res->start,
					npu_dev->apss_shared_io.size);
	if (unlikely(!npu_dev->apss_shared_io.base)) {
		NPU_ERR("unable to map apss_shared\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	NPU_DBG("apss_shared phy address=0x%llx virt=%pK\n",
		res->start, npu_dev->apss_shared_io.base);

	rc = npu_parse_dt_regulator(npu_dev);
	if (rc)
		goto error_get_dev_num;

	rc = npu_parse_dt_clock(npu_dev);
	if (rc)
		goto error_get_dev_num;

	rc = npu_parse_dt_bw(npu_dev);
	if (rc)
		NPU_WARN("Parse bw info failed\n");

	rc = npu_hw_info_init(npu_dev);
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

	rc = npu_mbox_init(npu_dev);
	if (rc)
		goto error_get_dev_num;

	/* character device might be optional */
	rc = alloc_chrdev_region(&npu_dev->dev_num, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		NPU_ERR("alloc_chrdev_region failed: %d\n", rc);
		goto error_get_dev_num;
	}

	npu_dev->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(npu_dev->class)) {
		rc = PTR_ERR(npu_dev->class);
		NPU_ERR("class_create failed: %d\n", rc);
		goto error_class_create;
	}

	npu_dev->device = device_create(npu_dev->class, NULL,
		npu_dev->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(npu_dev->device)) {
		rc = PTR_ERR(npu_dev->device);
		NPU_ERR("device_create failed: %d\n", rc);
		goto error_class_device_create;
	}

	cdev_init(&npu_dev->cdev, &npu_fops);
	rc = cdev_add(&npu_dev->cdev,
			MKDEV(MAJOR(npu_dev->dev_num), 0), 1);
	if (rc < 0) {
		NPU_ERR("cdev_add failed %d\n", rc);
		goto error_cdev_add;
	}
	dev_set_drvdata(npu_dev->device, npu_dev);
	NPU_DBG("drvdata %pK %pK\n", dev_get_drvdata(&pdev->dev),
		dev_get_drvdata(npu_dev->device));
	rc = sysfs_create_group(&npu_dev->device->kobj, &npu_fs_attr_group);
	if (rc) {
		NPU_ERR("unable to register npu sysfs nodes\n");
		goto error_res_init;
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

	rc = npu_host_init(npu_dev);
	if (rc) {
		NPU_ERR("unable to init host\n");
		goto error_driver_init;
	}

	if (IS_ENABLED(CONFIG_THERMAL)) {
		tcdev = thermal_of_cooling_device_register(pdev->dev.of_node,
							  "npu", npu_dev,
							  &npu_cooling_ops);
		if (IS_ERR(tcdev)) {
			dev_err(&pdev->dev,
				"npu: failed to register npu as cooling device\n");
			rc = PTR_ERR(tcdev);
			goto error_driver_init;
		}
		npu_dev->tcdev = tcdev;
		thermal_cdev_update(tcdev);
	}

	g_npu_dev = npu_dev;

	return rc;
error_driver_init:
	if (npu_dev->tcdev)
		thermal_cooling_device_unregister(npu_dev->tcdev);
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
	npu_mbox_deinit(npu_dev);
error_get_dev_num:
	return rc;
}

static int npu_remove(struct platform_device *pdev)
{
	struct npu_device *npu_dev;

	npu_dev = platform_get_drvdata(pdev);
	npu_host_deinit(npu_dev);
	npu_debugfs_deinit(npu_dev);
	if (npu_dev->tcdev)
		thermal_cooling_device_unregister(npu_dev->tcdev);
	sysfs_remove_group(&npu_dev->device->kobj, &npu_fs_attr_group);
	cdev_del(&npu_dev->cdev);
	device_destroy(npu_dev->class, npu_dev->dev_num);
	class_destroy(npu_dev->class);
	unregister_chrdev_region(npu_dev->dev_num, 1);
	platform_set_drvdata(pdev, NULL);
	npu_mbox_deinit(npu_dev);
	msm_bus_scale_unregister_client(npu_dev->bwctrl.bus_client);

	g_npu_dev = NULL;

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
		NPU_ERR("register failed %d\n", rc);
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
