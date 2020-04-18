/* Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/msm-bus.h>
#include <linux/pm_qos.h>
#include <linux/dma-buf.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdss_debug.h"
#include "mdss_dsi_phy.h"
#include "mdss_dba_utils.h"

#define CMDLINE_DSI_CTL_NUM_STRING_LEN 2

/* Master structure to hold all the information about the DSI/panel */
static struct mdss_dsi_data *mdss_dsi_res;

#define DSI_DISABLE_PC_LATENCY 100
#define DSI_ENABLE_PC_LATENCY PM_QOS_DEFAULT_VALUE

static struct pm_qos_request mdss_dsi_pm_qos_request;

void mdss_dump_dsi_debug_bus(u32 bus_dump_flag,
	u32 **dump_mem)
{
	struct mdss_dsi_data *sdata = mdss_dsi_res;
	struct mdss_dsi_ctrl_pdata *m_ctrl, *s_ctrl;
	bool in_log, in_mem;
	u32 *dump_addr = NULL;
	u32 status0 = 0, status1 = 0;
	phys_addr_t phys = 0;
	int list_size = 0;
	int i;
	bool dsi0_active = false, dsi1_active = false;

	if (!sdata || !sdata->dbg_bus || !sdata->dbg_bus_size)
		return;

	m_ctrl = sdata->ctrl_pdata[0];
	s_ctrl = sdata->ctrl_pdata[1];

	if (!m_ctrl)
		return;

	if (m_ctrl && m_ctrl->shared_data->dsi0_active)
		dsi0_active = true;
	if (s_ctrl && s_ctrl->shared_data->dsi1_active)
		dsi1_active = true;

	list_size = (sdata->dbg_bus_size * sizeof(sdata->dbg_bus[0]) * 4);

	in_log = (bus_dump_flag & MDSS_DBG_DUMP_IN_LOG);
	in_mem = (bus_dump_flag & MDSS_DBG_DUMP_IN_MEM);

	if (in_mem) {
		if (!(*dump_mem))
			*dump_mem = dma_alloc_coherent(&sdata->pdev->dev,
				list_size, &phys, GFP_KERNEL);

		if (*dump_mem) {
			dump_addr = *dump_mem;
			pr_info("%s: start_addr:0x%pK end_addr:0x%pK\n",
				__func__, dump_addr, dump_addr + list_size);
		} else {
			in_mem = false;
			pr_err("dump_mem: allocation fails\n");
		}
	}

	pr_info("========= Start DSI Debug Bus =========\n");

	mdss_dsi_clk_ctrl(m_ctrl, m_ctrl->dsi_clk_handle,
			  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_ON);

	for (i = 0; i < sdata->dbg_bus_size; i++) {
		if (dsi0_active) {
			writel_relaxed(sdata->dbg_bus[i],
					m_ctrl->ctrl_base + 0x124);
			wmb(); /* ensure register is committed */
		}
		if (dsi1_active) {
			writel_relaxed(sdata->dbg_bus[i],
					s_ctrl->ctrl_base + 0x124);
			wmb(); /* ensure register is committed */
		}

		if (dsi0_active) {
			status0 = readl_relaxed(m_ctrl->ctrl_base + 0x128);
			if (in_log)
				pr_err("CTRL:0 bus_ctrl: 0x%x status: 0x%x\n",
					sdata->dbg_bus[i], status0);
		}
		if (dsi1_active) {
			status1 = readl_relaxed(s_ctrl->ctrl_base + 0x128);
			if (in_log)
				pr_err("CTRL:1 bus_ctrl: 0x%x status: 0x%x\n",
					sdata->dbg_bus[i], status1);
		}

		if (dump_addr && in_mem) {
			dump_addr[i*4]     = sdata->dbg_bus[i];
			dump_addr[i*4 + 1] = status0;
			dump_addr[i*4 + 2] = status1;
			dump_addr[i*4 + 3] = 0x0;
		}
	}

	mdss_dsi_clk_ctrl(m_ctrl, m_ctrl->dsi_clk_handle,
			  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_OFF);

	pr_info("========End DSI Debug Bus=========\n");
}

static void mdss_dsi_pm_qos_add_request(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct irq_info *irq_info;

	if (!ctrl_pdata || !ctrl_pdata->shared_data)
		return;

	irq_info = ctrl_pdata->dsi_hw->irq_info;

	if (!irq_info)
		return;

	mutex_lock(&ctrl_pdata->shared_data->pm_qos_lock);
	if (!ctrl_pdata->shared_data->pm_qos_req_cnt) {
		pr_debug("%s: add request irq\n", __func__);

		mdss_dsi_pm_qos_request.type = PM_QOS_REQ_AFFINE_IRQ;
		mdss_dsi_pm_qos_request.irq = irq_info->irq;
		pm_qos_add_request(&mdss_dsi_pm_qos_request,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	}
	ctrl_pdata->shared_data->pm_qos_req_cnt++;
	mutex_unlock(&ctrl_pdata->shared_data->pm_qos_lock);
}

static void mdss_dsi_pm_qos_remove_request(struct dsi_shared_data *sdata)
{
	if (!sdata)
		return;

	mutex_lock(&sdata->pm_qos_lock);
	if (sdata->pm_qos_req_cnt) {
		sdata->pm_qos_req_cnt--;
		if (!sdata->pm_qos_req_cnt) {
			pr_debug("%s: remove request", __func__);
			pm_qos_remove_request(&mdss_dsi_pm_qos_request);
		}
	} else {
		pr_warn("%s: unbalanced pm_qos ref count\n", __func__);
	}
	mutex_unlock(&sdata->pm_qos_lock);
}

static void mdss_dsi_pm_qos_update_request(int val)
{
	pr_debug("%s: update request %d", __func__, val);
	pm_qos_update_request(&mdss_dsi_pm_qos_request, val);
}

static int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
					bool active);

static struct mdss_dsi_ctrl_pdata *mdss_dsi_get_ctrl(u32 ctrl_id)
{
	if (ctrl_id >= DSI_CTRL_MAX || !mdss_dsi_res)
		return NULL;

	return mdss_dsi_res->ctrl_pdata[ctrl_id];
}

static void mdss_dsi_config_clk_src(struct platform_device *pdev)
{
	struct mdss_dsi_data *dsi_res = platform_get_drvdata(pdev);
	struct dsi_shared_data *sdata = dsi_res->shared_data;

	if (!sdata->ext_byte0_clk || !sdata->ext_pixel0_clk) {
		pr_debug("%s: DSI-0 ext. clocks not present\n", __func__);
		return;
	}

	if (mdss_dsi_is_pll_src_default(sdata)) {
		/*
		 * Default Mapping:
		 * 1. dual-dsi/single-dsi:
		 *     DSI0 <--> PLL0
		 *     DSI1 <--> PLL1
		 * 2. split-dsi:
		 *     DSI0 <--> PLL0
		 *     DSI1 <--> PLL0
		 */
		sdata->byte0_parent = sdata->ext_byte0_clk;
		sdata->pixel0_parent = sdata->ext_pixel0_clk;

		if (mdss_dsi_is_hw_config_split(sdata)) {
			sdata->byte1_parent = sdata->byte0_parent;
			sdata->pixel1_parent = sdata->pixel0_parent;
		} else if (sdata->ext_byte1_clk && sdata->ext_pixel1_clk) {
			sdata->byte1_parent = sdata->ext_byte1_clk;
			sdata->pixel1_parent = sdata->ext_pixel1_clk;
		} else {
			pr_debug("%s: DSI-1 external clocks not present\n",
				__func__);
			return;
		}

		pr_debug("%s: default: DSI0 <--> PLL0, DSI1 <--> %s", __func__,
			mdss_dsi_is_hw_config_split(sdata) ? "PLL0" : "PLL1");
	} else {
		/*
		 * For split-dsi and single-dsi use cases, map the PLL source
		 * based on the pll source configuration. It is possible that
		 * for split-dsi case, the only supported config is to source
		 * the clocks from PLL0. This is not explicitly checked here as
		 * it should have been already enforced when validating the
		 * board configuration.
		 */
		if (mdss_dsi_is_pll_src_pll0(sdata)) {
			pr_debug("%s: single source: PLL0", __func__);
			sdata->byte0_parent = sdata->ext_byte0_clk;
			sdata->pixel0_parent = sdata->ext_pixel0_clk;
		} else if (mdss_dsi_is_pll_src_pll1(sdata)) {
			if (sdata->ext_byte1_clk && sdata->ext_pixel1_clk) {
				pr_debug("%s: single source: PLL1", __func__);
				sdata->byte0_parent = sdata->ext_byte1_clk;
				sdata->pixel0_parent = sdata->ext_pixel1_clk;
			} else {
				pr_err("%s: DSI-1 external clocks not present\n",
					__func__);
				return;
			}
		}
		sdata->byte1_parent = sdata->byte0_parent;
		sdata->pixel1_parent = sdata->pixel0_parent;
	}

	return;
}

static char const *mdss_dsi_get_clk_src(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dsi_shared_data *sdata;

	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return "????";
	}

	sdata = ctrl->shared_data;

	if (mdss_dsi_is_left_ctrl(ctrl)) {
		if (sdata->byte0_parent == sdata->ext_byte0_clk)
			return "PLL0";
		else
			return "PLL1";
	} else {
		if (sdata->byte1_parent == sdata->ext_byte0_clk)
			return "PLL0";
		else
			return "PLL1";
	}
}

static int mdss_dsi_set_clk_src(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	struct dsi_shared_data *sdata;
	struct clk *byte_parent, *pixel_parent;

	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	sdata = ctrl->shared_data;

	if (!ctrl->byte_clk_rcg || !ctrl->pixel_clk_rcg) {
		pr_debug("%s: set_clk_src not needed\n", __func__);
		return 0;
	}

	if (mdss_dsi_is_left_ctrl(ctrl)) {
		byte_parent = sdata->byte0_parent;
		pixel_parent = sdata->pixel0_parent;
	} else {
		byte_parent = sdata->byte1_parent;
		pixel_parent = sdata->pixel1_parent;
	}

	rc = clk_set_parent(ctrl->byte_clk_rcg, byte_parent);
	if (rc) {
		pr_err("%s: failed to set parent for byte clk for ctrl%d. rc=%d\n",
			__func__, ctrl->ndx, rc);
		goto error;
	}

	rc = clk_set_parent(ctrl->pixel_clk_rcg, pixel_parent);
	if (rc) {
		pr_err("%s: failed to set parent for pixel clk for ctrl%d. rc=%d\n",
			__func__, ctrl->ndx, rc);
		goto error;
	}

	pr_debug("%s: ctrl%d clock source set to %s", __func__, ctrl->ndx,
		mdss_dsi_get_clk_src(ctrl));

error:
	return rc;
}

static int mdss_dsi_regulator_init(struct platform_device *pdev,
		struct dsi_shared_data *sdata)
{
	int rc = 0, i = 0, j = 0;

	if (!pdev || !sdata) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	for (i = DSI_CORE_PM; !rc && (i < DSI_MAX_PM); i++) {
		rc = msm_dss_config_vreg(&pdev->dev,
			sdata->power_data[i].vreg_config,
			sdata->power_data[i].num_vreg, 1);
		if (rc) {
			pr_err("%s: failed to init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
			for (j = i-1; j >= DSI_CORE_PM; j--) {
				msm_dss_config_vreg(&pdev->dev,
				sdata->power_data[j].vreg_config,
				sdata->power_data[j].num_vreg, 0);
			}
		}
	}

	return rc;
}

static int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	ret = mdss_dsi_panel_reset(pdata, 0);
	if (ret) {
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

end:
	return ret;
}

static int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: failed to enable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
		return ret;
	}

	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

	return ret;
}

static int mdss_dsi_panel_power_lp(struct mdss_panel_data *pdata, int enable)
{
	/* Panel power control when entering/exiting lp mode */
	return 0;
}

static int mdss_dsi_panel_power_ctrl(struct mdss_panel_data *pdata,
	int power_state)
{
	int ret = 0;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	pr_debug("%s: cur_power_state=%d req_power_state=%d\n", __func__,
		pinfo->panel_power_state, power_state);

	if (pinfo->panel_power_state == power_state) {
		pr_debug("%s: no change needed\n", __func__);
		return 0;
	}

	/*
	 * If a dynamic mode switch is pending, the regulators should not
	 * be turned off or on.
	 */
	if (pdata->panel_info.dynamic_switch_pending)
		return 0;

	switch (power_state) {
	case MDSS_PANEL_POWER_OFF:
	case MDSS_PANEL_POWER_LCD_DISABLED:
		/* if LCD has not been disabled, then disable it now */
		if ((pinfo->panel_power_state != MDSS_PANEL_POWER_LCD_DISABLED)
		     && (pinfo->panel_power_state != MDSS_PANEL_POWER_OFF))
			ret = mdss_dsi_panel_power_off(pdata);
		break;
	case MDSS_PANEL_POWER_ON:
		if (mdss_dsi_is_panel_on_lp(pdata))
			ret = mdss_dsi_panel_power_lp(pdata, false);
		else
			ret = mdss_dsi_panel_power_on(pdata);
		break;
	case MDSS_PANEL_POWER_LP1:
	case MDSS_PANEL_POWER_LP2:
		ret = mdss_dsi_panel_power_lp(pdata, true);
		break;
	default:
		pr_err("%s: unknown panel power state requested (%d)\n",
			__func__, power_state);
		ret = -EINVAL;
	}

	if (!ret)
		pinfo->panel_power_state = power_state;

	return ret;
}

void mdss_dsi_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
}

int mdss_dsi_get_dt_vreg_data(struct device *dev,
	struct device_node *of_node, struct dss_module_power *mp,
	enum dsi_pm_type module)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *supply_node = NULL;
	const char *pm_supply_name = NULL;
	struct device_node *supply_root_node = NULL;

	if (!dev || !mp) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	mp->num_vreg = 0;
	pm_supply_name = __mdss_dsi_pm_supply_node_name(module);
	supply_root_node = of_get_child_by_name(of_node, pm_supply_name);
	if (!supply_root_node) {
		/*
		 * Try to get the root node for panel power supply using
		 * of_parse_phandle() API if of_get_child_by_name() API fails.
		 */
		supply_root_node = of_parse_phandle(of_node, pm_supply_name, 0);
		if (!supply_root_node) {
			pr_err("no supply entry present: %s\n", pm_supply_name);
			goto novreg;
		}
	}


	for_each_child_of_node(supply_root_node, supply_node) {
		mp->num_vreg++;
	}

	if (mp->num_vreg == 0) {
		pr_debug("%s: no vreg\n", __func__);
		goto novreg;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, mp->num_vreg);
	}

	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
		mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		rc = -ENOMEM;
		goto error;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string(supply_node,
			"qcom,supply-name", &st);
		if (rc) {
			pr_err("%s: error reading name. rc=%d\n",
				__func__, rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name,
			ARRAY_SIZE((mp->vreg_config[i].vreg_name)), "%s", st);
		/* vreg-min-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading min volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = tmp;

		/* vreg-max-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading max volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = tmp;

		/* enable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading enable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].load[DSS_REG_MODE_ENABLE] = tmp;

		/* disable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading disable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].load[DSS_REG_MODE_DISABLE] = tmp;

		/* pre-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_off_sleep = tmp;
		}

		/* post-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_off_sleep = tmp;
		}

		mp->vreg_config[i].lp_disable_allowed =
			of_property_read_bool(supply_node,
			"qcom,supply-lp-mode-disable-allowed");

		pr_debug("%s: %s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d lp_disable_allowed=%d\n",
			__func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].load[DSS_REG_MODE_ENABLE],
			mp->vreg_config[i].load[DSS_REG_MODE_DISABLE],
			mp->vreg_config[i].pre_on_sleep,
			mp->vreg_config[i].post_on_sleep,
			mp->vreg_config[i].pre_off_sleep,
			mp->vreg_config[i].post_off_sleep,
			mp->vreg_config[i].lp_disable_allowed);
		++i;
	}

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
novreg:
	mp->num_vreg = 0;

	return rc;
}

static int mdss_dsi_get_panel_cfg(char *panel_cfg,
				struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	struct mdss_panel_cfg *pan_cfg = NULL;

	if (!panel_cfg)
		return MDSS_PANEL_INTF_INVALID;

	pan_cfg = ctrl->mdss_util->panel_intf_type(MDSS_PANEL_INTF_DSI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (!pan_cfg) {
		panel_cfg[0] = 0;
		return 0;
	}

	pr_debug("%s:%d: cfg:[%s]\n", __func__, __LINE__,
		 pan_cfg->arg_cfg);
	rc = strlcpy(panel_cfg, pan_cfg->arg_cfg,
		     sizeof(pan_cfg->arg_cfg));
	return rc;
}

struct buf_data {
	char *buf; /* cmd buf */
	int blen; /* cmd buf length */
	char *string_buf; /* cmd buf as string, 3 bytes per number */
	int sblen; /* string buffer length */
	int sync_flag;
	struct mutex dbg_mutex; /* mutex to synchronize read/write/flush */
};

struct mdss_dsi_debugfs_info {
	struct dentry *root;
	struct mdss_dsi_ctrl_pdata ctrl_pdata;
	struct buf_data on_cmd;
	struct buf_data off_cmd;
	u32 override_flag;
};

static int mdss_dsi_cmd_state_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t mdss_dsi_cmd_state_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	int *link_state = file->private_data;
	char buffer[32];
	int blen = 0;

	if (*ppos)
		return 0;

	if ((*link_state) == DSI_HS_MODE)
		blen = snprintf(buffer, sizeof(buffer), "dsi_hs_mode\n");
	else
		blen = snprintf(buffer, sizeof(buffer), "dsi_lp_mode\n");

	if (blen < 0)
		return 0;

	if (copy_to_user(buf, buffer, min(count, (size_t)blen+1)))
		return -EFAULT;

	*ppos += blen;
	return blen;
}

static ssize_t mdss_dsi_cmd_state_write(struct file *file,
			const char __user *p, size_t count, loff_t *ppos)
{
	int *link_state = file->private_data;
	char *input;

	if (!count) {
		pr_err("%s: Zero bytes to be written\n", __func__);
		return -EINVAL;
	}

	input = kmalloc(count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, p, count)) {
		kfree(input);
		return -EFAULT;
	}
	input[count-1] = '\0';

	if (strnstr(input, "dsi_hs_mode", strlen("dsi_hs_mode")))
		*link_state = DSI_HS_MODE;
	else
		*link_state = DSI_LP_MODE;

	kfree(input);
	return count;
}

static const struct file_operations mdss_dsi_cmd_state_fop = {
	.open = mdss_dsi_cmd_state_open,
	.read = mdss_dsi_cmd_state_read,
	.write = mdss_dsi_cmd_state_write,
};

static int mdss_dsi_cmd_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t mdss_dsi_cmd_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct buf_data *pcmds = file->private_data;
	char *bp;
	ssize_t ret = 0;

	mutex_lock(&pcmds->dbg_mutex);
	if (*ppos == 0) {
		kfree(pcmds->string_buf);
		pcmds->string_buf = NULL;
		pcmds->sblen = 0;
	}

	if (!pcmds->string_buf) {
		/*
		 * Buffer size is the sum of cmd length (3 bytes per number)
		 * with NULL terminater
		 */
		int bsize = ((pcmds->blen)*3 + 1);
		int blen = 0;
		char *buffer;

		buffer = kmalloc(bsize, GFP_KERNEL);
		if (!buffer) {
			mutex_unlock(&pcmds->dbg_mutex);
			return -ENOMEM;
		}

		bp = pcmds->buf;
		while ((blen < (bsize-1)) &&
		       (bp < ((pcmds->buf) + (pcmds->blen)))) {
			struct dsi_ctrl_hdr dchdr =
					*((struct dsi_ctrl_hdr *)bp);
			int dhrlen = sizeof(dchdr), dlen;
			char *tmp = (char *)(&dchdr);
			dlen = dchdr.dlen;
			dchdr.dlen = htons(dchdr.dlen);
			while (dhrlen--)
				blen += snprintf(buffer+blen, bsize-blen,
						 "%02x ", (*tmp++));

			bp += sizeof(dchdr);
			while (dlen--)
				blen += snprintf(buffer+blen, bsize-blen,
						 "%02x ", (*bp++));
			buffer[blen-1] = '\n';
		}
		buffer[blen] = '\0';
		pcmds->string_buf = buffer;
		pcmds->sblen = blen;
	}

	/*
	 * The max value of count is PAGE_SIZE(4096).
	 * It may need multiple times of reading if string buf is too large
	 */
	if (*ppos >= (pcmds->sblen)) {
		kfree(pcmds->string_buf);
		pcmds->string_buf = NULL;
		pcmds->sblen = 0;
		mutex_unlock(&pcmds->dbg_mutex);
		return 0; /* the end */
	}
	ret = simple_read_from_buffer(buf, count, ppos, pcmds->string_buf,
				      pcmds->sblen);
	mutex_unlock(&pcmds->dbg_mutex);
	return ret;
}

static ssize_t mdss_dsi_cmd_write(struct file *file, const char __user *p,
				  size_t count, loff_t *ppos)
{
	struct buf_data *pcmds = file->private_data;
	ssize_t ret = 0;
	unsigned int blen = 0;
	char *string_buf;

	mutex_lock(&pcmds->dbg_mutex);
	if (*ppos == 0) {
		kfree(pcmds->string_buf);
		pcmds->string_buf = NULL;
		pcmds->sblen = 0;
	}

	/* Allocate memory for the received string */
	blen = count + (pcmds->sblen);
	if (blen > U32_MAX - 1) {
		mutex_unlock(&pcmds->dbg_mutex);
		return -EINVAL;
	}

	string_buf = krealloc(pcmds->string_buf, blen + 1, GFP_KERNEL);
	if (!string_buf) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		mutex_unlock(&pcmds->dbg_mutex);
		return -ENOMEM;
	}

	pcmds->string_buf = string_buf;
	/* Writing in batches is possible */
	ret = simple_write_to_buffer(string_buf, blen, ppos, p, count);
	if (ret < 0) {
		pr_err("%s: Failed to copy data\n", __func__);
		mutex_unlock(&pcmds->dbg_mutex);
		return -EINVAL;
	}

	string_buf[ret] = '\0';
	pcmds->sblen = count;
	mutex_unlock(&pcmds->dbg_mutex);
	return ret;
}

static int mdss_dsi_cmd_flush(struct file *file, fl_owner_t id)
{
	struct buf_data *pcmds = file->private_data;
	unsigned int len;
	int blen, i;
	char *buf, *bufp, *bp;
	struct dsi_ctrl_hdr *dchdr;

	mutex_lock(&pcmds->dbg_mutex);

	if (!pcmds->string_buf) {
		mutex_unlock(&pcmds->dbg_mutex);
		return 0;
	}

	/*
	 * Allocate memory for command buffer
	 * 3 bytes per number, and 2 bytes for the last one
	 */
	blen = ((pcmds->sblen) + 2) / 3;
	buf = kzalloc(blen, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		kfree(pcmds->string_buf);
		pcmds->string_buf = NULL;
		pcmds->sblen = 0;
		mutex_unlock(&pcmds->dbg_mutex);
		return -ENOMEM;
	}

	/* Translate the input string to command array */
	bufp = pcmds->string_buf;
	for (i = 0; i < blen; i++) {
		uint32_t value = 0;
		int step = 0;
		if (sscanf(bufp, "%02x%n", &value, &step) > 0) {
			*(buf+i) = (char)value;
			bufp += step;
		}
	}

	/* Scan dcs commands */
	bp = buf;
	len = blen;
	while (len >= sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > (len - sizeof(*dchdr)) || dchdr->dlen < 0) {
			pr_err("%s: dtsi cmd=%x error, len=%d\n",
				__func__, dchdr->dtype, dchdr->dlen);
			kfree(buf);
			mutex_unlock(&pcmds->dbg_mutex);
			return -EINVAL;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}
	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!\n", __func__,
				bp[0], len);
		kfree(buf);
		mutex_unlock(&pcmds->dbg_mutex);
		return -EINVAL;
	}

	if (pcmds->sync_flag) {
		pcmds->buf = buf;
		pcmds->blen = blen;
		pcmds->sync_flag = 0;
	} else {
		kfree(pcmds->buf);
		pcmds->buf = buf;
		pcmds->blen = blen;
	}
	mutex_unlock(&pcmds->dbg_mutex);
	return 0;
}

static const struct file_operations mdss_dsi_cmd_fop = {
	.open = mdss_dsi_cmd_open,
	.read = mdss_dsi_cmd_read,
	.write = mdss_dsi_cmd_write,
	.flush = mdss_dsi_cmd_flush,
};

struct dentry *dsi_debugfs_create_dcs_cmd(const char *name, umode_t mode,
				struct dentry *parent, struct buf_data *cmd,
				struct dsi_panel_cmds ctrl_cmds)
{
	mutex_init(&cmd->dbg_mutex);
	cmd->buf = ctrl_cmds.buf;
	cmd->blen = ctrl_cmds.blen;
	cmd->string_buf = NULL;
	cmd->sblen = 0;
	cmd->sync_flag = 1;

	return debugfs_create_file(name, mode, parent,
				   cmd, &mdss_dsi_cmd_fop);
}

#define DEBUGFS_CREATE_DCS_CMD(name, node, cmd, ctrl_cmd) \
	dsi_debugfs_create_dcs_cmd(name, 0644, node, cmd, ctrl_cmd)

static int mdss_dsi_debugfs_setup(struct mdss_panel_data *pdata,
			struct dentry *parent)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata, *dfs_ctrl;
	struct mdss_dsi_debugfs_info *dfs;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	dfs = kzalloc(sizeof(*dfs), GFP_KERNEL);
	if (!dfs)
		return -ENOMEM;

	dfs->root = debugfs_create_dir("dsi_ctrl_pdata", parent);
	if (IS_ERR_OR_NULL(dfs->root)) {
		pr_err("%s: debugfs_create_dir dsi fail, error %ld\n",
			__func__, PTR_ERR(dfs->root));
		kfree(dfs);
		return -ENODEV;
	}

	dfs_ctrl = &dfs->ctrl_pdata;
	debugfs_create_u32("override_flag", 0644, dfs->root,
			   &dfs->override_flag);

	debugfs_create_bool("cmd_sync_wait_broadcast", 0644, dfs->root,
			    &dfs_ctrl->cmd_sync_wait_broadcast);
	debugfs_create_bool("cmd_sync_wait_trigger", 0644, dfs->root,
			    &dfs_ctrl->cmd_sync_wait_trigger);

	debugfs_create_file("dsi_on_cmd_state", 0644, dfs->root,
		&dfs_ctrl->on_cmds.link_state, &mdss_dsi_cmd_state_fop);
	debugfs_create_file("dsi_off_cmd_state", 0644, dfs->root,
		&dfs_ctrl->off_cmds.link_state, &mdss_dsi_cmd_state_fop);

	DEBUGFS_CREATE_DCS_CMD("dsi_on_cmd", dfs->root, &dfs->on_cmd,
				ctrl_pdata->on_cmds);
	DEBUGFS_CREATE_DCS_CMD("dsi_off_cmd", dfs->root, &dfs->off_cmd,
				ctrl_pdata->off_cmds);

	debugfs_create_u32("dsi_err_counter", 0644, dfs->root,
			   &dfs_ctrl->err_cont.max_err_index);
	debugfs_create_u32("dsi_err_time_delta", 0644, dfs->root,
			   &dfs_ctrl->err_cont.err_time_delta);

	dfs->override_flag = 0;
	dfs->ctrl_pdata = *ctrl_pdata;
	ctrl_pdata->debugfs_info = dfs;
	return 0;
}

static int mdss_dsi_debugfs_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info panel_info;

	if (!ctrl_pdata) {
		pr_warn_once("%s: Invalid pdata!\n", __func__);
		return -EINVAL;
	}

	pdata = &ctrl_pdata->panel_data;
	if (!pdata)
		return -EINVAL;

	panel_info = pdata->panel_info;
	rc = mdss_dsi_debugfs_setup(pdata, panel_info.debugfs_info->root);
	if (rc) {
		pr_err("%s: Error in initilizing dsi ctrl debugfs\n",
				__func__);
		return rc;
	}

	pr_debug("%s: Init complete\n", __func__);
	return 0;
}

static void mdss_dsi_debugfs_cleanup(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mdss_panel_data *pdata = &ctrl_pdata->panel_data;

	do {
		struct mdss_dsi_ctrl_pdata *ctrl = container_of(pdata,
			struct mdss_dsi_ctrl_pdata, panel_data);
		struct mdss_dsi_debugfs_info *dfs = ctrl->debugfs_info;
		if (dfs && dfs->root)
			debugfs_remove_recursive(dfs->root);
		kfree(dfs);
		pdata = pdata->next;
	} while (pdata);
	pr_debug("%s: Cleaned up mdss_dsi_debugfs_info\n", __func__);
}

static int _mdss_dsi_refresh_cmd(struct buf_data *new_cmds,
	struct dsi_panel_cmds *original_pcmds)
{
	char *bp;
	int len, cnt, i;
	struct dsi_ctrl_hdr *dchdr;
	struct dsi_cmd_desc *cmds;

	if (new_cmds->sync_flag)
		return 0;

	bp = new_cmds->buf;
	len = new_cmds->blen;
	cnt = 0;
	/* Scan dcs commands and get dcs command count */
	while (len >= sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d\n",
				__func__, dchdr->dtype, dchdr->dlen);
			return -EINVAL;
		}
		bp += sizeof(*dchdr) + dchdr->dlen;
		len -= sizeof(*dchdr) + dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!\n", __func__,
				bp[0], len);
		return -EINVAL;
	}

	/* Reallocate space for dcs commands */
	cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
	if (!cmds)
		return -ENOMEM;
	kfree(original_pcmds->buf);
	kfree(original_pcmds->cmds);
	original_pcmds->cmd_cnt = cnt;
	original_pcmds->cmds = cmds;
	original_pcmds->buf = new_cmds->buf;
	original_pcmds->blen = new_cmds->blen;

	bp = original_pcmds->buf;
	len = original_pcmds->blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		original_pcmds->cmds[i].dchdr = *dchdr;
		original_pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}

	new_cmds->sync_flag = 1;
	return 0;
}

static void mdss_dsi_debugfsinfo_to_dsictrl_info(
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mdss_dsi_debugfs_info *dfs = ctrl_pdata->debugfs_info;
	struct dsi_err_container *dfs_err_cont = &dfs->ctrl_pdata.err_cont;
	struct dsi_err_container *err_cont = &ctrl_pdata->err_cont;

	ctrl_pdata->cmd_sync_wait_broadcast =
			dfs->ctrl_pdata.cmd_sync_wait_broadcast;
	ctrl_pdata->cmd_sync_wait_trigger =
			dfs->ctrl_pdata.cmd_sync_wait_trigger;

	_mdss_dsi_refresh_cmd(&dfs->on_cmd, &ctrl_pdata->on_cmds);
	_mdss_dsi_refresh_cmd(&dfs->off_cmd, &ctrl_pdata->off_cmds);

	ctrl_pdata->on_cmds.link_state =
			dfs->ctrl_pdata.on_cmds.link_state;
	ctrl_pdata->off_cmds.link_state =
			dfs->ctrl_pdata.off_cmds.link_state;

	/* keep error counter between 2 to 10 */
	if (dfs_err_cont->max_err_index >= 2 &&
		dfs_err_cont->max_err_index <= MAX_ERR_INDEX) {
		err_cont->max_err_index = dfs_err_cont->max_err_index;
	} else {
		dfs_err_cont->max_err_index = err_cont->max_err_index;
		pr_warn("resetting the dsi error counter to %d\n",
			err_cont->max_err_index);
	}

	/* keep error duration between 16 ms to 100 seconds */
	if (dfs_err_cont->err_time_delta >= 16 &&
		dfs_err_cont->err_time_delta <= 100000) {
		err_cont->err_time_delta = dfs_err_cont->err_time_delta;
	} else {
		dfs_err_cont->err_time_delta = err_cont->err_time_delta;
		pr_warn("resetting the dsi error time delta to %d ms\n",
			err_cont->err_time_delta);
	}
}

static void mdss_dsi_validate_debugfs_info(
		struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mdss_dsi_debugfs_info *dfs = ctrl_pdata->debugfs_info;

	if (dfs->override_flag) {
		pr_debug("%s: Overriding dsi ctrl_pdata with debugfs data\n",
			__func__);
		dfs->override_flag = 0;
		mdss_dsi_debugfsinfo_to_dsictrl_info(ctrl_pdata);
	}
}

/**
 * mdss_dsi_clamp_phy_reset_config() - configure DSI phy reset mask
 * @ctrl: pointer to DSI controller structure
 * @enable: true to mask the reset signal, false to unmask
 *
 * Configure the register to mask/unmask the propagation of the mdss ahb
 * clock reset signal to the DSI PHY. This would be necessary when the MDSS
 * core is idle power collapsed with the DSI panel on. This function assumes
 * that the mmss_misc_ahb clock is already on.
 */
static int mdss_dsi_clamp_phy_reset_config(struct mdss_dsi_ctrl_pdata *ctrl,
	bool enable)
{
	u32 regval;

	if (!ctrl) {
		pr_warn_ratelimited("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!ctrl->mmss_misc_io.base) {
		pr_warn_ratelimited("%s: mmss_misc_io not mapped\n", __func__);
		return -EINVAL;
	}

	if ((ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_104) &&
		(MDSS_GET_STEP(ctrl->shared_data->hw_rev) !=
		MDSS_DSI_HW_REV_STEP_2)) {
		u32 clamp_reg_off = ctrl->shared_data->ulps_clamp_ctrl_off;

		regval = MIPI_INP(ctrl->mmss_misc_io.base + clamp_reg_off);
		if (enable)
			regval = regval | BIT(30);
		else
			regval = regval & ~BIT(30);
		MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off, regval);
	} else {
		u32 phyrst_reg_off = ctrl->shared_data->ulps_phyrst_ctrl_off;

		if (enable)
			regval = BIT(0);
		else
			regval = 0;
		MIPI_OUTP(ctrl->mmss_misc_io.base + phyrst_reg_off, regval);
	}

	/* make sure that clamp ctrl is updated */
	wmb();

	return 0;
}

static int mdss_dsi_off(struct mdss_panel_data *pdata, int power_state)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *panel_info = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	panel_info = &ctrl_pdata->panel_data.panel_info;

	pr_debug("%s+: ctrl=%pK ndx=%d power_state=%d\n",
		__func__, ctrl_pdata, ctrl_pdata->ndx, power_state);

	if (power_state == panel_info->panel_power_state) {
		pr_debug("%s: No change in power state %d -> %d\n", __func__,
			panel_info->panel_power_state, power_state);
		goto end;
	}

	if (mdss_panel_is_power_on(power_state)) {
		pr_debug("%s: dsi_off with panel always on\n", __func__);
		goto panel_power_ctrl;
	}

	/*
	 * Link clocks should be turned off before PHY can be disabled.
	 * For command mode panels, all clocks are turned off prior to reaching
	 * here, so core clocks should be turned on before accessing hardware
	 * registers. For video mode panel, turn off link clocks and then
	 * disable PHY
	 */
	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
					MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_ON);
	else
		mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
				  MDSS_DSI_LINK_CLK, MDSS_DSI_CLK_OFF);

	if (!pdata->panel_info.ulps_suspend_enabled) {
		/* disable DSI controller */
		mdss_dsi_controller_cfg(0, pdata);

		/* disable DSI phy */
		mdss_dsi_phy_disable(ctrl_pdata);
	}
	ctrl_pdata->ctrl_state &= ~CTRL_STATE_DSI_ACTIVE;
	mdss_dsi_clamp_phy_reset_config(ctrl_pdata, false);
	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_OFF);

panel_power_ctrl:
	ret = mdss_dsi_panel_power_ctrl(pdata, power_state);
	if (ret) {
		pr_err("%s: Panel power off failed\n", __func__);
		goto end;
	}

	if (panel_info->dynamic_fps
	    && (panel_info->dfps_update == DFPS_SUSPEND_RESUME_MODE)
	    && (panel_info->new_fps != panel_info->mipi.frame_rate))
		panel_info->mipi.frame_rate = panel_info->new_fps;

	/* Initialize Max Packet size for DCS reads */
	ctrl_pdata->cur_max_pkt_size = 0;
end:
	pr_debug("%s-:\n", __func__);

	return ret;
}

int mdss_dsi_switch_mode(struct mdss_panel_data *pdata, int mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_panel_info *pinfo;
	bool dsi_ctrl_setup_needed = false;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s, start\n", __func__);

	pinfo = &pdata->panel_info.mipi;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
		panel_data);

	if ((pinfo->dms_mode != DYNAMIC_MODE_RESOLUTION_SWITCH_IMMEDIATE) &&
			(pinfo->dms_mode != DYNAMIC_MODE_SWITCH_IMMEDIATE)) {
		pr_debug("%s: Dynamic mode switch not enabled.\n", __func__);
		return -EPERM;
	}

	if (mode == MIPI_VIDEO_PANEL) {
		mode = SWITCH_TO_VIDEO_MODE;
	} else if (mode == MIPI_CMD_PANEL) {
		mode = SWITCH_TO_CMD_MODE;
	} else if (mode == SWITCH_RESOLUTION) {
		dsi_ctrl_setup_needed = true;
		pr_debug("Resolution switch mode selected\n");
	} else {
		pr_err("Invalid mode selected, mode=%d\n", mode);
		return -EINVAL;
	}

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
	if (dsi_ctrl_setup_needed)
		mdss_dsi_ctrl_setup(ctrl_pdata);

	ATRACE_BEGIN("switch_cmds");
	ctrl_pdata->switch_mode(pdata, mode);
	ATRACE_END("switch_cmds");

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);

	pr_debug("%s, end\n", __func__);
	return 0;
}

static int mdss_dsi_reconfig(struct mdss_panel_data *pdata, int mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_panel_info *pinfo;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s, start\n", __func__);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
		panel_data);
	pinfo = &pdata->panel_info.mipi;

	if (pinfo->dms_mode == DYNAMIC_MODE_SWITCH_IMMEDIATE) {
		/* reset DSI */
		mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
		mdss_dsi_sw_reset(ctrl_pdata, true);
		mdss_dsi_ctrl_setup(ctrl_pdata);
		mdss_dsi_controller_cfg(true, pdata);
		mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	}

	pr_debug("%s, end\n", __func__);
	return 0;
}
static int mdss_dsi_update_panel_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
				int mode)
{
	int ret = 0;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (mode == DSI_CMD_MODE) {
		pinfo->mipi.mode = DSI_CMD_MODE;
		pinfo->type = MIPI_CMD_PANEL;
		pinfo->mipi.vsync_enable = 1;
		pinfo->mipi.hw_vsync_mode = 1;
		pinfo->partial_update_enabled = pinfo->partial_update_supported;
	} else {	/*video mode*/
		pinfo->mipi.mode = DSI_VIDEO_MODE;
		pinfo->type = MIPI_VIDEO_PANEL;
		pinfo->mipi.vsync_enable = 0;
		pinfo->mipi.hw_vsync_mode = 0;
		pinfo->partial_update_enabled = 0;
	}

	ctrl_pdata->panel_mode = pinfo->mipi.mode;
	mdss_panel_get_dst_fmt(pinfo->bpp, pinfo->mipi.mode,
			pinfo->mipi.pixel_packing, &(pinfo->mipi.dst_format));
	return ret;
}

int mdss_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int cur_power_state;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (ctrl_pdata->debugfs_info)
		mdss_dsi_validate_debugfs_info(ctrl_pdata);

	cur_power_state = pdata->panel_info.panel_power_state;
	pr_debug("%s+: ctrl=%pK ndx=%d cur_power_state=%d\n", __func__,
		ctrl_pdata, ctrl_pdata->ndx, cur_power_state);

	pinfo = &pdata->panel_info;
	mipi = &pdata->panel_info.mipi;

	if (mdss_dsi_is_panel_on_interactive(pdata)) {
		/*
		 * all interrupts are disabled at LK
		 * for cont_splash case, intr mask bits need
		 * to be restored to allow dcs command be
		 * sent to panel
		 */
		mdss_dsi_restore_intr_mask(ctrl_pdata);
		pr_debug("%s: panel already on\n", __func__);
		goto end;
	}

	ret = mdss_dsi_panel_power_ctrl(pdata, MDSS_PANEL_POWER_ON);
	if (ret) {
		pr_err("%s:Panel power on failed. rc=%d\n", __func__, ret);
		goto end;
	}

	if (mdss_panel_is_power_on(cur_power_state)) {
		pr_debug("%s: dsi_on from panel low power state\n", __func__);
		goto end;
	}

	ret = mdss_dsi_set_clk_src(ctrl_pdata);
	if (ret) {
		pr_err("%s: failed to set clk src. rc=%d\n", __func__, ret);
		goto end;
	}

	/*
	 * Enable DSI core clocks prior to resetting and initializing DSI
	 * Phy. Phy and ctrl setup need to be done before enabling the link
	 * clocks.
	 */
	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_ON);

	/*
	 * If ULPS during suspend feature is enabled, then DSI PHY was
	 * left on during suspend. In this case, we do not need to reset/init
	 * PHY. This would have already been done when the CORE clocks are
	 * turned on. However, if cont splash is disabled, the first time DSI
	 * is powered on, phy init needs to be done unconditionally.
	 */
	if (!pdata->panel_info.ulps_suspend_enabled || !ctrl_pdata->ulps) {
		mdss_dsi_phy_sw_reset(ctrl_pdata);
		mdss_dsi_phy_init(ctrl_pdata);
		mdss_dsi_ctrl_setup(ctrl_pdata);
	}
	ctrl_pdata->ctrl_state |= CTRL_STATE_DSI_ACTIVE;

	mdss_dsi_clamp_phy_reset_config(ctrl_pdata, true);

	/* DSI link clocks need to be on prior to ctrl sw reset */
	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_LINK_CLK, MDSS_DSI_CLK_ON);
	mdss_dsi_sw_reset(ctrl_pdata, true);

	/*
	 * Issue hardware reset line after enabling the DSI clocks and data
	 * data lanes for LP11 init
	 */
	if (mipi->lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");
		mdss_dsi_panel_reset(pdata, 1);
	}

	if (mipi->init_delay)
		usleep_range(mipi->init_delay, mipi->init_delay);

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);

end:
	pr_debug("%s-:\n", __func__);
	return ret;
}

static int mdss_dsi_pinctrl_set_state(
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool active)
{
	struct pinctrl_state *pin_state;
	struct mdss_panel_info *pinfo = NULL;
	int rc = -EFAULT;

	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl))
		return PTR_ERR(ctrl_pdata->pin_res.pinctrl);

	pinfo = &ctrl_pdata->panel_data.panel_info;
	if ((mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) ||
			pinfo->is_dba_panel) {
		pr_debug("%s:%d, right ctrl pinctrl config not needed\n",
			__func__, __LINE__);
		return 0;
	}

	pin_state = active ? ctrl_pdata->pin_res.gpio_state_active
				: ctrl_pdata->pin_res.gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(ctrl_pdata->pin_res.pinctrl,
				pin_state);
		if (rc)
			pr_err("%s: can not set %s pins\n", __func__,
			       active ? MDSS_PINCTRL_STATE_DEFAULT
			       : MDSS_PINCTRL_STATE_SLEEP);
	} else {
		pr_err("%s: invalid '%s' pinstate\n", __func__,
		       active ? MDSS_PINCTRL_STATE_DEFAULT
		       : MDSS_PINCTRL_STATE_SLEEP);
	}
	return rc;
}

static int mdss_dsi_pinctrl_init(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	ctrl_pdata = platform_get_drvdata(pdev);
	ctrl_pdata->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(ctrl_pdata->pin_res.pinctrl);
	}

	ctrl_pdata->pin_res.gpio_state_active
		= pinctrl_lookup_state(ctrl_pdata->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.gpio_state_active))
		pr_warn("%s: can not get default pinstate\n", __func__);

	ctrl_pdata->pin_res.gpio_state_suspend
		= pinctrl_lookup_state(ctrl_pdata->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.gpio_state_suspend))
		pr_warn("%s: can not get sleep pinstate\n", __func__);

	return 0;
}

static int mdss_dsi_unblank(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *sctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s+: ctrl=%pK ndx=%d cur_power_state=%d ctrl_state=%x\n",
			__func__, ctrl_pdata, ctrl_pdata->ndx,
		pdata->panel_info.panel_power_state, ctrl_pdata->ctrl_state);

	mdss_dsi_pm_qos_update_request(DSI_DISABLE_PC_LATENCY);

	if (mdss_dsi_is_ctrl_clk_master(ctrl_pdata))
		sctrl = mdss_dsi_get_ctrl_clk_slave();

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
	if (sctrl)
		mdss_dsi_clk_ctrl(sctrl, sctrl->dsi_clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);

	if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_LP) {
		pr_debug("%s: dsi_unblank with panel always on\n", __func__);
		if (ctrl_pdata->low_power_config)
			ret = ctrl_pdata->low_power_config(pdata, false);
		if (!ret)
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_LP;
		goto error;
	}

	if (!(ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT)) {
		if (!pdata->panel_info.dynamic_switch_pending) {
			ATRACE_BEGIN("dsi_panel_on");
			ret = ctrl_pdata->on(pdata);
			if (ret) {
				pr_err("%s: unable to initialize the panel\n",
							__func__);
				goto error;
			}
			ATRACE_END("dsi_panel_on");
		}
	}

	if ((pdata->panel_info.type == MIPI_CMD_PANEL) &&
		mipi->vsync_enable && mipi->hw_vsync_mode) {
		mdss_dsi_set_tear_on(ctrl_pdata);
		if (mdss_dsi_is_te_based_esd(ctrl_pdata))
			enable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	}

	ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_INIT;

error:
	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	if (sctrl)
		mdss_dsi_clk_ctrl(sctrl, sctrl->dsi_clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);

	mdss_dsi_pm_qos_update_request(DSI_ENABLE_PC_LATENCY);

	pr_debug("%s-:\n", __func__);

	return ret;
}

static int mdss_dsi_blank(struct mdss_panel_data *pdata, int power_state)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi = &pdata->panel_info.mipi;

	pr_debug("%s+: ctrl=%pK ndx=%d power_state=%d\n",
		__func__, ctrl_pdata, ctrl_pdata->ndx, power_state);

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);

	if (mdss_panel_is_power_on_lp(power_state)) {
		pr_debug("%s: low power state requested\n", __func__);
		if (ctrl_pdata->low_power_config)
			ret = ctrl_pdata->low_power_config(pdata, true);
		if (!ret)
			ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_LP;
		goto error;
	}

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL &&
			ctrl_pdata->off_cmds.link_state == DSI_LP_MODE) {
		mdss_dsi_sw_reset(ctrl_pdata, false);
		mdss_dsi_host_init(pdata);
	}

	mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);

	if (pdata->panel_info.dynamic_switch_pending) {
		pr_info("%s: switching to %s mode\n", __func__,
			(pdata->panel_info.mipi.mode ? "video" : "command"));
		if (pdata->panel_info.type == MIPI_CMD_PANEL) {
			ctrl_pdata->switch_mode(pdata, SWITCH_TO_VIDEO_MODE);
		} else if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
			ctrl_pdata->switch_mode(pdata, SWITCH_TO_CMD_MODE);
			mdss_dsi_set_tear_off(ctrl_pdata);
		}
	}

	if ((pdata->panel_info.type == MIPI_CMD_PANEL) &&
		mipi->vsync_enable && mipi->hw_vsync_mode) {
		if (mdss_dsi_is_te_based_esd(ctrl_pdata)) {
			disable_irq(gpio_to_irq(
				ctrl_pdata->disp_te_gpio));
			atomic_dec(&ctrl_pdata->te_irq_ready);
		}
		mdss_dsi_set_tear_off(ctrl_pdata);
	}

	if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
		if (!pdata->panel_info.dynamic_switch_pending) {
			ATRACE_BEGIN("dsi_panel_off");
			ret = ctrl_pdata->off(pdata);
			if (ret) {
				pr_err("%s: Panel OFF failed\n", __func__);
				goto error;
			}
			ATRACE_END("dsi_panel_off");
		}
		ctrl_pdata->ctrl_state &= ~(CTRL_STATE_PANEL_INIT |
			CTRL_STATE_PANEL_LP);
	}

error:
	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	pr_debug("%s-:End\n", __func__);
	return ret;
}

static int mdss_dsi_post_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%pK ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);

	if (ctrl_pdata->post_panel_on)
		ctrl_pdata->post_panel_on(pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	pr_debug("%s-:\n", __func__);

	return 0;
}

static irqreturn_t test_hw_vsync_handler(int irq, void *data)
{
	struct mdss_panel_data *pdata = (struct mdss_panel_data *)data;

	pr_debug("HW VSYNC\n");
	MDSS_XLOG(0xaaa, irq);
	complete_all(&pdata->te_done);
	if (pdata->next)
		complete_all(&pdata->next->te_done);
	return IRQ_HANDLED;
}

int mdss_dsi_cont_splash_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_info("%s:%d DSI on for continuous splash.\n", __func__, __LINE__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	mipi = &pdata->panel_info.mipi;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%pK ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	WARN((ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT),
		"Incorrect Ctrl state=0x%x\n", ctrl_pdata->ctrl_state);

	mdss_dsi_ctrl_setup(ctrl_pdata);
	mdss_dsi_sw_reset(ctrl_pdata, true);
	pr_debug("%s-:End\n", __func__);
	return ret;
}

static void __mdss_dsi_mask_dfps_errors(struct mdss_dsi_ctrl_pdata *ctrl,
		bool mask)
{
	u32 data = 0;

	/*
	 * Assumption is that the DSI clocks will be enabled
	 * when this API is called from dfps thread
	 */
	if (mask) {
		/* mask FIFO underflow and PLL unlock bits */
		mdss_dsi_set_reg(ctrl, 0x10c, 0x7c000000, 0x7c000000);
	} else {
		data = MIPI_INP((ctrl->ctrl_base) + 0x0120);
		if (data & BIT(16)) {
			pr_debug("pll unlocked: 0x%x\n", data);
			/* clear PLL unlock bit */
			MIPI_OUTP((ctrl->ctrl_base) + 0x120, BIT(16));
		}

		data = MIPI_INP((ctrl->ctrl_base) + 0x00c);
		if (data & 0x88880000) {
			pr_debug("dsi fifo underflow: 0x%x\n", data);
			/* clear DSI FIFO underflow and empty */
			MIPI_OUTP((ctrl->ctrl_base) + 0x00c, 0x99990000);
		}

		/* restore FIFO underflow and PLL unlock bits */
		mdss_dsi_set_reg(ctrl, 0x10c, 0x7c000000, 0x0);
	}
}

static void __mdss_dsi_update_video_mode_total(struct mdss_panel_data *pdata,
		int new_fps)
{
	u32 hsync_period, vsync_period;
	u32 new_dsi_v_total, current_dsi_v_total;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s Invalid ctrl_pdata\n", __func__);
		return;
	}

	if (ctrl_pdata->timing_db_mode)
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x1e8, 0x1);

	vsync_period =
		mdss_panel_get_vtotal(&pdata->panel_info);
	hsync_period =
		mdss_panel_get_htotal(&pdata->panel_info, true);
	current_dsi_v_total =
		MIPI_INP((ctrl_pdata->ctrl_base) + 0x2C);
	new_dsi_v_total =
		((vsync_period - 1) << 16) | (hsync_period - 1);

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C, new_dsi_v_total);

	if (ctrl_pdata->timing_db_mode)
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x1e4, 0x1);

	pr_debug("%s new_fps:%d new_vtotal:0x%X cur_vtotal:0x%X frame_rate:%d\n",
			__func__, new_fps, new_dsi_v_total, current_dsi_v_total,
			ctrl_pdata->panel_data.panel_info.mipi.frame_rate);

	ctrl_pdata->panel_data.panel_info.current_fps = new_fps;
	MDSS_XLOG(current_dsi_v_total, new_dsi_v_total, new_fps,
		ctrl_pdata->timing_db_mode);

}

static void __mdss_dsi_dyn_refresh_config(
		struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int reg_data = 0;
	u32 phy_rev = ctrl_pdata->shared_data->phy_rev;

	/* configure only for master control in split display */
	if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
			mdss_dsi_is_ctrl_clk_slave(ctrl_pdata))
		return;

	switch (phy_rev) {
	case DSI_PHY_REV_10:
		reg_data = MIPI_INP((ctrl_pdata->ctrl_base) +
				DSI_DYNAMIC_REFRESH_CTRL);
		reg_data &= ~BIT(12);
		MIPI_OUTP((ctrl_pdata->ctrl_base)
				+ DSI_DYNAMIC_REFRESH_CTRL, reg_data);
		break;
	case DSI_PHY_REV_20:
		reg_data = BIT(13);
		MIPI_OUTP((ctrl_pdata->ctrl_base)
				+ DSI_DYNAMIC_REFRESH_CTRL, reg_data);
		break;
	default:
		pr_err("Phy rev %d unsupported\n", phy_rev);
		break;
	}

	pr_debug("Dynamic fps ctrl = 0x%x\n", reg_data);
}

static void __mdss_dsi_calc_dfps_delay(struct mdss_panel_data *pdata)
{
	u32 esc_clk_rate_hz;
	u32 pipe_delay, pipe_delay2 = 0, pll_delay;
	u32 hsync_period = 0;
	u32 pclk_to_esc_ratio, byte_to_esc_ratio, hr_bit_to_esc_ratio;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_phy_ctrl *pd = NULL;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s Invalid ctrl_pdata\n", __func__);
		return;
	}

	if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
		mdss_dsi_is_ctrl_clk_slave(ctrl_pdata))
		return;

	pinfo = &pdata->panel_info;
	pd = &(pinfo->mipi.dsi_phy_db);

	esc_clk_rate_hz = ctrl_pdata->esc_clk_rate_hz;
	pclk_to_esc_ratio = (ctrl_pdata->pclk_rate / esc_clk_rate_hz);
	byte_to_esc_ratio = (ctrl_pdata->byte_clk_rate / esc_clk_rate_hz);
	hr_bit_to_esc_ratio = ((ctrl_pdata->byte_clk_rate * 4) /
					esc_clk_rate_hz);

	hsync_period = mdss_panel_get_htotal(pinfo, true);
	pipe_delay = (hsync_period + 1) / pclk_to_esc_ratio;
	if (pinfo->mipi.eof_bllp_power_stop == 0)
		pipe_delay += (17 / pclk_to_esc_ratio) +
			((21 + (pinfo->mipi.t_clk_pre + 1) +
				(pinfo->mipi.t_clk_post + 1)) /
				byte_to_esc_ratio) +
			((((pd->timing[8] >> 1) + 1) +
			((pd->timing[6] >> 1) + 1) +
			((pd->timing[3] * 4) + (pd->timing[5] >> 1) + 1) +
			((pd->timing[7] >> 1) + 1) +
			((pd->timing[1] >> 1) + 1) +
			((pd->timing[4] >> 1) + 1)) / hr_bit_to_esc_ratio);

	if (pinfo->mipi.force_clk_lane_hs)
		pipe_delay2 = (6 / byte_to_esc_ratio) +
			((((pd->timing[1] >> 1) + 1) +
			((pd->timing[4] >> 1) + 1)) / hr_bit_to_esc_ratio);

	/* 130 us pll delay recommended by h/w doc */
	pll_delay = ((130 * esc_clk_rate_hz) / 1000000) * 2;

	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_PIPE_DELAY,
						pipe_delay);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_PIPE_DELAY2,
						pipe_delay2);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_PLL_DELAY,
						pll_delay);
}

static int __mdss_dsi_dfps_calc_clks(struct mdss_panel_data *pdata,
		u64 new_clk_rate)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	u32 phy_rev;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s Invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	phy_rev = ctrl_pdata->shared_data->phy_rev;

	pinfo->clk_rate = new_clk_rate;
	pinfo->mipi.dsi_pclk_rate = mdss_dsi_get_pclk_rate(pinfo,
		new_clk_rate);
	__mdss_dsi_dyn_refresh_config(ctrl_pdata);

	if (phy_rev == DSI_PHY_REV_20)
		mdss_dsi_dfps_config_8996(ctrl_pdata);

	__mdss_dsi_calc_dfps_delay(pdata);

	/* take a backup of current clk rates */
	ctrl_pdata->pclk_rate_bkp = ctrl_pdata->pclk_rate;
	ctrl_pdata->byte_clk_rate_bkp = ctrl_pdata->byte_clk_rate;

	ctrl_pdata->pclk_rate = pinfo->mipi.dsi_pclk_rate;
	do_div(new_clk_rate, 8U);
	ctrl_pdata->byte_clk_rate = (u32) new_clk_rate;

	pr_debug("byte_rate=%i\n", ctrl_pdata->byte_clk_rate);
	pr_debug("pclk_rate=%i\n", ctrl_pdata->pclk_rate);

	return rc;
}

static int __mdss_dsi_dfps_update_clks(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *sctrl_pdata = NULL;
	struct mdss_panel_info *pinfo, *spinfo = NULL;
	int rc = 0;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (IS_ERR_OR_NULL(ctrl_pdata)) {
		pr_err("Invalid sctrl_pdata = %lu\n", PTR_ERR(ctrl_pdata));
		return PTR_ERR(ctrl_pdata);
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;

	/*
	 * In split display case, configure and enable dynamic refresh
	 * register only after both the ctrl data is programmed. So,
	 * ignore enabling dynamic refresh for the master control and
	 * configure only when it is slave control.
	 */
	if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
			mdss_dsi_is_ctrl_clk_master(ctrl_pdata))
		return 0;

	if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
			mdss_dsi_is_ctrl_clk_slave(ctrl_pdata)) {
		sctrl_pdata = ctrl_pdata;
		spinfo = pinfo;
		ctrl_pdata = mdss_dsi_get_ctrl_clk_master();
		if (IS_ERR_OR_NULL(ctrl_pdata)) {
			pr_err("Invalid ctrl_pdata = %lu\n",
					PTR_ERR(ctrl_pdata));
			return PTR_ERR(ctrl_pdata);
		}

		pinfo = &ctrl_pdata->panel_data.panel_info;
	}

	/*
	 * For programming dynamic refresh registers, we need to change
	 * the parent to shadow clocks for the software byte and pixel mux.
	 * After switching to shadow clocks, if there is no ref count on
	 * main byte and pixel clocks, clock driver may shutdown those
	 * unreferenced  byte and pixel clocks. Hence add an extra reference
	 * count to avoid shutting down the main byte and pixel clocks.
	 */
	rc = clk_prepare_enable(ctrl_pdata->pll_byte_clk);
	if (rc) {
		pr_err("Unable to add extra refcnt for byte clock\n");
		goto error_byte;
	}

	rc = clk_prepare_enable(ctrl_pdata->pll_pixel_clk);
	if (rc) {
		pr_err("Unable to add extra refcnt for pixel clock\n");
		goto error_pixel;
	}

	/* change the parent to shadow clocks*/
	rc = clk_set_parent(ctrl_pdata->mux_byte_clk,
			ctrl_pdata->shadow_byte_clk);
	if (rc) {
		pr_err("Unable to set parent to shadow byte clock\n");
		goto error_shadow_byte;
	}

	rc = clk_set_parent(ctrl_pdata->mux_pixel_clk,
			ctrl_pdata->shadow_pixel_clk);
	if (rc) {
		pr_err("Unable to set parent to shadow pixel clock\n");
		goto error_shadow_pixel;
	}

	rc = mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
			MDSS_DSI_LINK_BYTE_CLK, ctrl_pdata->byte_clk_rate, 0);
	if (rc) {
		pr_err("%s: dsi_byte_clk - clk_set_rate failed\n",
				__func__);
		goto error_byte_link;
	}

	rc = mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
			MDSS_DSI_LINK_PIX_CLK, ctrl_pdata->pclk_rate, 0);
	if (rc) {
		pr_err("%s: dsi_pixel_clk - clk_set_rate failed\n",
				__func__);
		goto error_pixel_link;
	}

	if (sctrl_pdata) {
		rc = mdss_dsi_clk_set_link_rate(sctrl_pdata->dsi_clk_handle,
			MDSS_DSI_LINK_BYTE_CLK, sctrl_pdata->byte_clk_rate, 0);
		if (rc) {
			pr_err("%s: slv dsi_byte_clk - clk_set_rate failed\n",
					__func__);
			goto error_sbyte_link;
		}

		rc = mdss_dsi_clk_set_link_rate(sctrl_pdata->dsi_clk_handle,
			MDSS_DSI_LINK_PIX_CLK, sctrl_pdata->pclk_rate, 0);
		if (rc) {
			pr_err("%s: slv dsi_pixel_clk - clk_set_rate failed\n",
					__func__);
			goto error_spixel_link;
		}
	}

	rc = mdss_dsi_en_wait4dynamic_done(ctrl_pdata);
	if (rc < 0) {
		pr_err("Unsuccessful dynamic fps change");
		goto dfps_timeout;
	}

	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_CTRL, 0x00);
	if (sctrl_pdata)
		MIPI_OUTP((sctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_CTRL,
				0x00);

	rc = mdss_dsi_phy_pll_reset_status(ctrl_pdata);
	if (rc) {
		pr_err("%s: pll cannot be locked reset core ready failed %d\n",
			__func__, rc);
		goto dfps_timeout;
	}

	__mdss_dsi_mask_dfps_errors(ctrl_pdata, false);
	if (sctrl_pdata)
		__mdss_dsi_mask_dfps_errors(sctrl_pdata, false);

	/* Move the mux clocks to main byte and pixel clocks */
	rc = clk_set_parent(ctrl_pdata->mux_byte_clk,
			ctrl_pdata->pll_byte_clk);
	if (rc)
		pr_err("Unable to set parent back to main byte clock\n");

	rc = clk_set_parent(ctrl_pdata->mux_pixel_clk,
			ctrl_pdata->pll_pixel_clk);
	if (rc)
		pr_err("Unable to set parent back to main pixel clock\n");

	/* Remove extra ref count on parent clocks */
	clk_disable_unprepare(ctrl_pdata->pll_byte_clk);
	clk_disable_unprepare(ctrl_pdata->pll_pixel_clk);

	return rc;

dfps_timeout:
	if (sctrl_pdata)
		mdss_dsi_clk_set_link_rate(sctrl_pdata->dsi_clk_handle,
				MDSS_DSI_LINK_PIX_CLK,
				sctrl_pdata->pclk_rate_bkp, 0);
error_spixel_link:
	if (sctrl_pdata)
		mdss_dsi_clk_set_link_rate(sctrl_pdata->dsi_clk_handle,
				MDSS_DSI_LINK_BYTE_CLK,
				sctrl_pdata->byte_clk_rate_bkp, 0);
error_sbyte_link:
	mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
		MDSS_DSI_LINK_PIX_CLK, ctrl_pdata->pclk_rate_bkp, 0);
error_pixel_link:
	mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
		MDSS_DSI_LINK_BYTE_CLK, ctrl_pdata->byte_clk_rate_bkp, 0);
error_byte_link:
	clk_set_parent(ctrl_pdata->mux_pixel_clk, ctrl_pdata->pll_pixel_clk);
error_shadow_pixel:
	clk_set_parent(ctrl_pdata->mux_byte_clk, ctrl_pdata->pll_byte_clk);
error_shadow_byte:
	clk_disable_unprepare(ctrl_pdata->pll_pixel_clk);
error_pixel:
	clk_disable_unprepare(ctrl_pdata->pll_byte_clk);
error_byte:
	return rc;
}

static int mdss_dsi_check_params(struct mdss_dsi_ctrl_pdata *ctrl, void *arg)
{
	struct mdss_panel_info *var_pinfo, *pinfo;
	int rc = 0;

	if (!ctrl || !arg)
		return 0;

	pinfo = &ctrl->panel_data.panel_info;
	if (!pinfo->is_pluggable)
		return 0;

	var_pinfo = (struct mdss_panel_info *)arg;

	pr_debug("%s: reconfig xres: %d yres: %d, current xres: %d yres: %d\n",
			__func__, var_pinfo->xres, var_pinfo->yres,
					pinfo->xres, pinfo->yres);
	if ((var_pinfo->xres != pinfo->xres) ||
		(var_pinfo->yres != pinfo->yres) ||
		(var_pinfo->lcdc.h_back_porch != pinfo->lcdc.h_back_porch) ||
		(var_pinfo->lcdc.h_front_porch != pinfo->lcdc.h_front_porch) ||
		(var_pinfo->lcdc.h_pulse_width != pinfo->lcdc.h_pulse_width) ||
		(var_pinfo->lcdc.v_back_porch != pinfo->lcdc.v_back_porch) ||
		(var_pinfo->lcdc.v_front_porch != pinfo->lcdc.v_front_porch) ||
		(var_pinfo->lcdc.v_pulse_width != pinfo->lcdc.v_pulse_width)
		)
		rc = 1;

	return rc;
}

static void mdss_dsi_avr_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		int enabled)
{
	u32 data = MIPI_INP((ctrl_pdata->ctrl_base) + 0x10);

	/* DSI_VIDEO_MODE_CTRL */
	if (enabled)
		data |= BIT(29); /* AVR_SUPPORT_ENABLED */
	else
		data &= ~BIT(29);

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x10, data);
	MDSS_XLOG(ctrl_pdata->ndx, enabled, data);
}

static int __mdss_dsi_dynamic_clock_switch(struct mdss_panel_data *pdata,
	u64 new_clk_rate)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	u32 phy_rev;
	u64 clk_rate_bkp;

	pr_debug("%s+:\n", __func__);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	phy_rev = ctrl_pdata->shared_data->phy_rev;
	pinfo = &pdata->panel_info;

	/* get the fps configured in HW */
	clk_rate_bkp = pinfo->clk_rate;

	__mdss_dsi_mask_dfps_errors(ctrl_pdata, true);

	if (phy_rev == DSI_PHY_REV_20) {
		rc = mdss_dsi_phy_calc_timing_param(pinfo, phy_rev,
				new_clk_rate);
		if (rc) {
			pr_err("PHY calculations failed-%lld\n", new_clk_rate);
			goto end_update;
		}
	}

	rc = __mdss_dsi_dfps_calc_clks(pdata, new_clk_rate);
	if (rc) {
		pr_err("error calculating clocks for %lld\n", new_clk_rate);
		goto error_clks;
	}

	rc = __mdss_dsi_dfps_update_clks(pdata);
	if (rc) {
		pr_err("Dynamic refresh failed-%lld\n", new_clk_rate);
		goto error_dfps;
	}
	return rc;
error_dfps:
	if (__mdss_dsi_dfps_calc_clks(pdata, clk_rate_bkp))
		pr_err("error reverting clock calculations for %lld\n",
				clk_rate_bkp);
error_clks:
	if (mdss_dsi_phy_calc_timing_param(pinfo, phy_rev, clk_rate_bkp))
		pr_err("Unable to revert phy timing-%lld\n", clk_rate_bkp);
end_update:
	return rc;
}

static int mdss_dsi_dynamic_bitclk_config(struct mdss_panel_data *pdata)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	if (!ctrl_pdata->panel_data.panel_info.dynamic_bitclk) {
		pr_err("Dynamic bitclk not enabled for this panel\n");
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;

	if (!pinfo->new_clk_rate || (pinfo->clk_rate == pinfo->new_clk_rate)) {
		pr_debug("Bit clock update is not needed\n");
		return 0;
	}

	rc = __mdss_dsi_dynamic_clock_switch(&ctrl_pdata->panel_data,
		pinfo->new_clk_rate);
	if (!rc && mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
		struct mdss_dsi_ctrl_pdata *octrl =
			mdss_dsi_get_other_ctrl(ctrl_pdata);
		rc = __mdss_dsi_dynamic_clock_switch(&octrl->panel_data,
			pinfo->new_clk_rate);
		if (rc)
			pr_err("failed to switch DSI bitclk for sctrl\n");
	} else if (rc) {
		pr_err("failed to switch DSI bitclk\n");
	}
	return rc;
}

static int mdss_dsi_dfps_config(struct mdss_panel_data *pdata, int new_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	if (!ctrl_pdata->panel_data.panel_info.dynamic_fps) {
		pr_err("Dynamic fps not enabled for this panel\n");
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;

	if (new_fps == pinfo->current_fps) {
		/*
		 * This is unlikely as mdss driver checks for previously
		 * configured frame rate.
		 */
		pr_debug("Panel is already at this FPS\n");
		goto end_update;
	}

	if (pinfo->dfps_update == DFPS_IMMEDIATE_PORCH_UPDATE_MODE_HFP ||
		pinfo->dfps_update == DFPS_IMMEDIATE_PORCH_UPDATE_MODE_VFP) {
		/* Porch method */
		__mdss_dsi_update_video_mode_total(pdata, new_fps);
	} else if (pinfo->dfps_update == DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
		/* Clock update method */
		u64 new_clk_rate = mdss_dsi_calc_bitclk
			(&ctrl_pdata->panel_data.panel_info, new_fps);
		if (!new_clk_rate) {
			pr_err("%s: unable to get the new bit clock rate\n",
					__func__);
			rc = -EINVAL;
			goto end_update;
		}

		rc = __mdss_dsi_dynamic_clock_switch(pdata, new_clk_rate);
		if (!rc) {
			struct mdss_dsi_ctrl_pdata *mctrl_pdata = NULL;
			struct mdss_panel_info *mpinfo = NULL;

			if (mdss_dsi_is_hw_config_split
				(ctrl_pdata->shared_data) &&
				mdss_dsi_is_ctrl_clk_master(ctrl_pdata))
				goto end_update;

			if (mdss_dsi_is_hw_config_split
				(ctrl_pdata->shared_data) &&
				mdss_dsi_is_ctrl_clk_slave(ctrl_pdata)) {
				mctrl_pdata = mdss_dsi_get_ctrl_clk_master();
				if (IS_ERR_OR_NULL(mctrl_pdata)) {
					pr_err("Invalid mctrl_pdata\n");
					goto end_update;
				}

				mpinfo = &mctrl_pdata->panel_data.panel_info;
			}
			/*
			 * update new fps that at this point is already
			 * updated in hw
			 */
			pinfo->current_fps = new_fps;
			if (mctrl_pdata && mpinfo)
				mpinfo->current_fps = new_fps;
		}
	}
end_update:
	return rc;
}

static int mdss_dsi_ctl_partial_roi(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int rc = -EINVAL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (!pdata->panel_info.partial_update_enabled)
		return 0;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (ctrl_pdata->set_col_page_addr)
		rc = ctrl_pdata->set_col_page_addr(pdata, false);

	return rc;
}

static int mdss_dsi_set_stream_size(struct mdss_panel_data *pdata)
{
	u32 stream_ctrl, stream_total, idle;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct dsc_desc *dsc = NULL;
	struct mdss_rect *roi;
	struct panel_horizontal_idle *pidle;
	int i;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;

	if (!pinfo->partial_update_supported)
		return -EINVAL;

	if (pinfo->compression_mode == COMPRESSION_DSC)
		dsc = &pinfo->dsc;

	roi = &pinfo->roi;

	/* DSI_COMMAND_MODE_MDP_STREAM_CTRL */
	if (dsc) {
		u16 byte_num =  dsc->bytes_per_pkt;

		if (pinfo->mipi.insert_dcs_cmd)
			byte_num++;

		stream_ctrl = (byte_num << 16) | (pinfo->mipi.vc << 8) |
				DTYPE_DCS_LWRITE;
		stream_total = dsc->pic_height << 16 | dsc->pclk_per_line;
	} else  {

		stream_ctrl = (((roi->w * 3) + 1) << 16) |
			(pdata->panel_info.mipi.vc << 8) | DTYPE_DCS_LWRITE;
		stream_total = roi->h << 16 | roi->w;
	}
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x60, stream_ctrl);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x58, stream_ctrl);

	/* DSI_COMMAND_MODE_MDP_STREAM_TOTAL */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x64, stream_total);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x5C, stream_total);

	/* set idle control -- dsi clk cycle */
	idle = 0;
	pidle = ctrl_pdata->line_idle;
	for (i = 0; i < ctrl_pdata->horizontal_idle_cnt; i++) {
		if (roi->w > pidle->min && roi->w <= pidle->max) {
			idle = pidle->idle;
			pr_debug("%s: ndx=%d w=%d range=%d-%d idle=%d\n",
				__func__, ctrl_pdata->ndx, roi->w,
				pidle->min, pidle->max, pidle->idle);
			break;
		}
		pidle++;
	}

	if (idle)
		idle |= BIT(12);	/* enable */

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x194, idle);

	if (dsc)
		mdss_dsi_dsc_config(ctrl_pdata, dsc);

	return 0;
}

static void mdss_dsi_dba_work(struct work_struct *work)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct delayed_work *dw = to_delayed_work(work);
	struct mdss_dba_utils_init_data utils_init_data;
	struct mdss_panel_info *pinfo;

	ctrl_pdata = container_of(dw, struct mdss_dsi_ctrl_pdata, dba_work);
	if (!ctrl_pdata) {
		pr_err("%s: invalid ctrl data\n", __func__);
		return;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;
	if (!pinfo) {
		pr_err("%s: invalid ctrl data\n", __func__);
		return;
	}

	memset(&utils_init_data, 0, sizeof(utils_init_data));

	utils_init_data.chip_name = ctrl_pdata->bridge_name;
	utils_init_data.client_name = "dsi";
	utils_init_data.instance_id = ctrl_pdata->bridge_index;
	utils_init_data.fb_node = ctrl_pdata->fb_node;
	utils_init_data.kobj = ctrl_pdata->kobj;
	utils_init_data.pinfo = pinfo;
	if (ctrl_pdata->mdss_util)
		utils_init_data.cont_splash_enabled =
			ctrl_pdata->mdss_util->panel_intf_status(
			ctrl_pdata->panel_data.panel_info.pdest,
			MDSS_PANEL_INTF_DSI) ? true : false;
	else
		utils_init_data.cont_splash_enabled = false;

	pinfo->dba_data = mdss_dba_utils_init(&utils_init_data);

	if (!IS_ERR_OR_NULL(pinfo->dba_data)) {
		ctrl_pdata->ds_registered = true;
	} else {
		pr_debug("%s: dba device not ready, queue again\n", __func__);
		queue_delayed_work(ctrl_pdata->workq,
				&ctrl_pdata->dba_work, HZ);
	}
}

static int mdss_dsi_reset_write_ptr(struct mdss_panel_data *pdata)
{

	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	int rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	pinfo = &ctrl_pdata->panel_data.panel_info;
	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
	/* Need to reset the DSI core since the pixel stream was stopped. */
	mdss_dsi_sw_reset(ctrl_pdata, true);

	/*
	 * Reset the partial update co-ordinates to the panel height and
	 * width
	 */
	if (pinfo->dcs_cmd_by_left && (ctrl_pdata->ndx == 1))
		goto skip_cmd_send;

	pinfo->roi.x = 0;
	pinfo->roi.y = 0;
	pinfo->roi.w = pinfo->xres;
	if (pinfo->dcs_cmd_by_left)
		pinfo->roi.w = pinfo->xres;
	if (pdata->next)
		pinfo->roi.w += pdata->next->panel_info.xres;
	pinfo->roi.h = pinfo->yres;

	mdss_dsi_set_stream_size(pdata);

	if (ctrl_pdata->set_col_page_addr)
		rc = ctrl_pdata->set_col_page_addr(pdata, true);

skip_cmd_send:
	mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);

	pr_debug("%s: DSI%d write ptr reset finished\n", __func__,
			ctrl_pdata->ndx);

	return rc;
}

int mdss_dsi_register_recovery_handler(struct mdss_dsi_ctrl_pdata *ctrl,
	struct mdss_intf_recovery *recovery)
{
	mutex_lock(&ctrl->mutex);
	ctrl->recovery = recovery;
	mutex_unlock(&ctrl->mutex);
	return 0;
}

static int mdss_dsi_register_mdp_callback(struct mdss_dsi_ctrl_pdata *ctrl,
	struct mdss_intf_recovery *mdp_callback)
{
	mutex_lock(&ctrl->mutex);
	ctrl->mdp_callback = mdp_callback;
	mutex_unlock(&ctrl->mutex);
	return 0;
}

static int mdss_dsi_register_clamp_handler(struct mdss_dsi_ctrl_pdata *ctrl,
	struct mdss_intf_ulp_clamp *clamp_handler)
{
	mutex_lock(&ctrl->mutex);
	ctrl->clamp_handler = clamp_handler;
	mutex_unlock(&ctrl->mutex);
	return 0;
}

static struct device_node *mdss_dsi_get_fb_node_cb(struct platform_device *pdev)
{
	struct device_node *fb_node;
	struct platform_device *dsi_dev;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	if (pdev == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return NULL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	dsi_dev = of_find_device_by_node(pdev->dev.of_node->parent);
	if (!dsi_dev) {
		pr_err("Unable to find dsi master device: %s\n",
			pdev->dev.of_node->full_name);
		return NULL;
	}

	fb_node = of_parse_phandle(dsi_dev->dev.of_node,
			mdss_dsi_get_fb_name(ctrl_pdata), 0);
	if (!fb_node) {
		pr_err("Unable to find fb node for device: %s\n", pdev->name);
		return NULL;
	}

	return fb_node;
}

static void mdss_dsi_timing_db_ctrl(struct mdss_dsi_ctrl_pdata *ctrl,
						int enable)
{
	if (!ctrl || !ctrl->timing_db_mode ||
		(ctrl->shared_data->hw_rev < MDSS_DSI_HW_REV_201))
		return;

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
		  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_ON);
	MIPI_OUTP((ctrl->ctrl_base + 0x1e8), enable);
	wmb(); /* ensure timing db is disabled */
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
		  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_OFF);
}

static struct mdss_dsi_ctrl_pdata *mdss_dsi_get_drvdata(struct device *dev)
{
	struct msm_fb_data_type *mfd;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct fb_info *fbi = dev_get_drvdata(dev);

	if (fbi) {
		mfd = (struct msm_fb_data_type *)fbi->par;
		pdata = dev_get_platdata(&mfd->pdev->dev);

		ctrl_pdata = container_of(pdata,
			struct mdss_dsi_ctrl_pdata, panel_data);
	}

	return ctrl_pdata;
}

static ssize_t supp_bitclk_list_sysfs_rda(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = mdss_dsi_get_drvdata(dev);
	struct mdss_panel_info *pinfo = NULL;

	if (!ctrl_pdata) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;
	if (!pinfo) {
		pr_err("no panel connected\n");
		return -ENODEV;
	}

	if (!pinfo->dynamic_bitclk) {
		pr_err_once("%s: Dynamic bitclk not enabled for this panel\n",
				__func__);
		return -EINVAL;
	}

	buf[0] = 0;
	for (i = 0; i < pinfo->supp_bitclk_len; i++) {
		if (ret > 0)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				",%d", pinfo->supp_bitclks[i]);
		else
			ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				"%d", pinfo->supp_bitclks[i]);
	}

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	return ret;
}

static ssize_t dynamic_bitclk_sysfs_wta(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0, i = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = mdss_dsi_get_drvdata(dev);
	struct mdss_panel_info *pinfo = NULL;
	int clk_rate = 0;

	if (!ctrl_pdata) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;
	if (!pinfo) {
		pr_err("no panel connected\n");
		return -ENODEV;
	}

	if (!pinfo->dynamic_bitclk) {
		pr_err_once("%s: Dynamic bitclk not enabled for this panel\n",
				__func__);
		return -EINVAL;
	}

	if (mdss_panel_is_power_off(pinfo->panel_power_state)) {
		pr_err_once("%s: Panel powered off!\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &clk_rate);
	if (rc) {
		pr_err("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	for (i = 0; i < pinfo->supp_bitclk_len; i++) {
		if (pinfo->supp_bitclks[i] == clk_rate)
			break;
	}
	if (i == pinfo->supp_bitclk_len) {
		pr_err("Requested bitclk: %d not supported\n", clk_rate);
		return -EINVAL;
	}

	pinfo->new_clk_rate = clk_rate;
	if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
		struct mdss_dsi_ctrl_pdata *octrl =
			mdss_dsi_get_other_ctrl(ctrl_pdata);
		struct mdss_panel_info *opinfo = &octrl->panel_data.panel_info;

		opinfo->new_clk_rate = clk_rate;
	}
	return count;
} /* dynamic_bitclk_sysfs_wta */

static ssize_t dynamic_bitclk_sysfs_rda(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = mdss_dsi_get_drvdata(dev);
	struct mdss_panel_info *pinfo = NULL;

	if (!ctrl_pdata) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;
	if (!pinfo) {
		pr_err("no panel connected\n");
		return -ENODEV;
	}

	ret = snprintf(buf, PAGE_SIZE, "%llu\n", pinfo->clk_rate);
	pr_debug("%s: '%llu'\n", __func__, pinfo->clk_rate);

	return ret;
} /* dynamic_bitclk_sysfs_rda */

static DEVICE_ATTR(dynamic_bitclk, 0664,
	dynamic_bitclk_sysfs_rda, dynamic_bitclk_sysfs_wta);
static DEVICE_ATTR(supported_bitclk, 0444, supp_bitclk_list_sysfs_rda, NULL);

static struct attribute *dynamic_bitclk_fs_attrs[] = {
	&dev_attr_dynamic_bitclk.attr,
	&dev_attr_supported_bitclk.attr,
	NULL,
};

static struct attribute_group mdss_dsi_fs_attrs_group = {
	.attrs = dynamic_bitclk_fs_attrs,
};

static int mdss_dsi_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct fb_info *fbi;
	int power_state;
	u32 mode;
	struct mdss_panel_info *pinfo;
	int ret;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	pinfo = &pdata->panel_info;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_debug("%s+: ctrl=%d event=%d\n", __func__, ctrl_pdata->ndx, event);

	MDSS_XLOG(event, arg, ctrl_pdata->ndx, 0x3333);

	switch (event) {
	case MDSS_EVENT_CHECK_PARAMS:
		pr_debug("%s:Entered Case MDSS_EVENT_CHECK_PARAMS\n", __func__);
		if (mdss_dsi_check_params(ctrl_pdata, arg)) {
			ctrl_pdata->update_phy_timing = true;
			/*
			 * Call to MDSS_EVENT_CHECK_PARAMS expects
			 * the return value of 1, if there is a change
			 * in panel timing parameters.
			 */
			rc = 1;
		}
		ctrl_pdata->refresh_clk_rate = true;
		break;
	case MDSS_EVENT_LINK_READY:
		if (ctrl_pdata->refresh_clk_rate)
			rc = mdss_dsi_clk_refresh(pdata,
				ctrl_pdata->update_phy_timing);

		rc = mdss_dsi_on(pdata);
		break;
	case MDSS_EVENT_UNBLANK:
		if (ctrl_pdata->on_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_unblank(pdata);
		break;
	case MDSS_EVENT_POST_PANEL_ON:
		rc = mdss_dsi_post_panel_on(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		ctrl_pdata->ctrl_state |= CTRL_STATE_MDP_ACTIVE;
		if (ctrl_pdata->on_cmds.link_state == DSI_HS_MODE)
			rc = mdss_dsi_unblank(pdata);
		pdata->panel_info.esd_rdy = true;
		break;
	case MDSS_EVENT_BLANK:
		power_state = (int) (unsigned long) arg;
		if (ctrl_pdata->off_cmds.link_state == DSI_HS_MODE)
			rc = mdss_dsi_blank(pdata, power_state);
		break;
	case MDSS_EVENT_PANEL_OFF:
		power_state = (int) (unsigned long) arg;
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		if (ctrl_pdata->off_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_blank(pdata, power_state);
		rc = mdss_dsi_off(pdata, power_state);
		break;
	case MDSS_EVENT_DISABLE_PANEL:
		/* disable esd thread */
		disable_esd_thread();

		/* disable backlight */
		ctrl_pdata->panel_data.set_backlight(pdata, 0);

		/* send the off commands */
		ctrl_pdata->off(pdata);

		/* disable panel power */
		ret = mdss_dsi_panel_power_ctrl(pdata,
			MDSS_PANEL_POWER_LCD_DISABLED);
		break;
	case MDSS_EVENT_CONT_SPLASH_FINISH:
		if (ctrl_pdata->off_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_blank(pdata, MDSS_PANEL_POWER_OFF);
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		rc = mdss_dsi_cont_splash_on(pdata);
		break;
	case MDSS_EVENT_PANEL_CLK_CTRL:
		mdss_dsi_clk_req(ctrl_pdata,
			(struct dsi_panel_clk_ctrl *) arg);
		break;
	case MDSS_EVENT_DSI_CMDLIST_KOFF:
		mdss_dsi_cmdlist_commit(ctrl_pdata, 1);
		break;
	case MDSS_EVENT_PANEL_UPDATE_FPS:
		if (arg != NULL) {
			rc = mdss_dsi_dfps_config(pdata,
					 (int) (unsigned long) arg);
			if (rc)
				pr_err("unable to change fps-%d, error-%d\n",
						(int) (unsigned long) arg, rc);
			else
				pr_debug("panel frame rate changed to %d\n",
						(int) (unsigned long) arg);
		}
		break;
	case MDSS_EVENT_CONT_SPLASH_BEGIN:
		if (ctrl_pdata->off_cmds.link_state == DSI_HS_MODE) {
			/* Panel is Enabled in Bootloader */
			rc = mdss_dsi_blank(pdata, MDSS_PANEL_POWER_OFF);
		}
		break;
	case MDSS_EVENT_DSC_PPS_SEND:
		if (pinfo->compression_mode == COMPRESSION_DSC)
			mdss_dsi_panel_dsc_pps_send(ctrl_pdata, pinfo);
		break;
	case MDSS_EVENT_ENABLE_PARTIAL_ROI:
		rc = mdss_dsi_ctl_partial_roi(pdata);
		break;
	case MDSS_EVENT_DSI_RESET_WRITE_PTR:
		rc = mdss_dsi_reset_write_ptr(pdata);
		break;
	case MDSS_EVENT_DSI_STREAM_SIZE:
		rc = mdss_dsi_set_stream_size(pdata);
		break;
	case MDSS_EVENT_DSI_UPDATE_PANEL_DATA:
		rc = mdss_dsi_update_panel_config(ctrl_pdata,
					(int)(unsigned long) arg);
		break;
	case MDSS_EVENT_REGISTER_RECOVERY_HANDLER:
		rc = mdss_dsi_register_recovery_handler(ctrl_pdata,
			(struct mdss_intf_recovery *)arg);
		break;
	case MDSS_EVENT_REGISTER_MDP_CALLBACK:
		rc = mdss_dsi_register_mdp_callback(ctrl_pdata,
			(struct mdss_intf_recovery *)arg);
		break;
	case MDSS_EVENT_REGISTER_CLAMP_HANDLER:
		rc = mdss_dsi_register_clamp_handler(ctrl_pdata,
			(struct mdss_intf_ulp_clamp *)arg);
		break;
	case MDSS_EVENT_DSI_DYNAMIC_SWITCH:
		mode = (u32)(unsigned long) arg;
		mdss_dsi_switch_mode(pdata, mode);
		break;
	case MDSS_EVENT_DSI_RECONFIG_CMD:
		mode = (u32)(unsigned long) arg;
		rc = mdss_dsi_reconfig(pdata, mode);
		break;
	case MDSS_EVENT_DSI_PANEL_STATUS:
		rc = mdss_dsi_check_panel_status(ctrl_pdata, arg);
		break;
	case MDSS_EVENT_PANEL_TIMING_SWITCH:
		rc = mdss_dsi_panel_timing_switch(ctrl_pdata, arg);
		break;
	case MDSS_EVENT_FB_REGISTERED:
		mdss_dsi_debugfs_init(ctrl_pdata);

		fbi = (struct fb_info *)arg;
		if (!fbi || !fbi->dev)
			break;

		ctrl_pdata->kobj = &fbi->dev->kobj;
		ctrl_pdata->fb_node = fbi->node;

		if (!mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) ||
			(mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
			mdss_dsi_is_ctrl_clk_master(ctrl_pdata))) {
			if (sysfs_create_group(&fbi->dev->kobj,
				&mdss_dsi_fs_attrs_group))
				pr_err("failed to create DSI sysfs group\n");
		}

		if (IS_ENABLED(CONFIG_MSM_DBA) &&
			pdata->panel_info.is_dba_panel) {
			queue_delayed_work(ctrl_pdata->workq,
				&ctrl_pdata->dba_work, HZ);
		}
		break;
	case MDSS_EVENT_DSI_TIMING_DB_CTRL:
		mdss_dsi_timing_db_ctrl(ctrl_pdata, (int)(unsigned long)arg);
		break;
	case MDSS_EVENT_AVR_MODE:
		mdss_dsi_avr_config(ctrl_pdata, (int)(unsigned long) arg);
		break;
	case MDSS_EVENT_DSI_DYNAMIC_BITCLK:
		if (ctrl_pdata->panel_data.panel_info.dynamic_bitclk) {
			rc = mdss_dsi_dynamic_bitclk_config(pdata);
			if (rc)
				pr_err("unable to change bitclk error-%d\n",
					rc);
		}
		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	pr_debug("%s-:event=%d, rc=%d\n", __func__, event, rc);
	return rc;
}

static int mdss_dsi_set_override_cfg(char *override_cfg,
		struct mdss_dsi_ctrl_pdata *ctrl_pdata, char *panel_cfg)
{
	struct mdss_panel_info *pinfo = &ctrl_pdata->panel_data.panel_info;
	char *token = NULL;

	pr_debug("%s: override config:%s\n", __func__, override_cfg);
	while ((token = strsep(&override_cfg, ":"))) {
		if (!strcmp(token, OVERRIDE_CFG)) {
			continue;
		} else if (!strcmp(token, SIM_HW_TE_PANEL)) {
			pinfo->sim_panel_mode = SIM_HW_TE_MODE;
		} else if (!strcmp(token, SIM_SW_TE_PANEL)) {
			pinfo->sim_panel_mode = SIM_SW_TE_MODE;
		} else if (!strcmp(token, SIM_PANEL)) {
			pinfo->sim_panel_mode = SIM_MODE;
		} else {
			pr_err("%s: invalid override_cfg token: %s\n",
					__func__, token);
			return -EINVAL;
		}
	}
	pr_debug("%s:sim_panel_mode:%d\n", __func__, pinfo->sim_panel_mode);

	return 0;
}

static struct device_node *mdss_dsi_pref_prim_panel(
		struct platform_device *pdev)
{
	struct device_node *dsi_pan_node = NULL;

	pr_debug("%s:%d: Select primary panel from dt\n",
					__func__, __LINE__);
	dsi_pan_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,dsi-pref-prim-pan", 0);
	if (!dsi_pan_node)
		pr_err("%s:can't find panel phandle\n", __func__);

	return dsi_pan_node;
}

/**
 * mdss_dsi_find_panel_of_node(): find device node of dsi panel
 * @pdev: platform_device of the dsi ctrl node
 * @panel_cfg: string containing intf specific config data
 *
 * Function finds the panel device node using the interface
 * specific configuration data. This configuration data is
 * could be derived from the result of bootloader's GCDB
 * panel detection mechanism. If such config data doesn't
 * exist then this panel returns the default panel configured
 * in the device tree.
 *
 * returns pointer to panel node on success, NULL on error.
 */
static struct device_node *mdss_dsi_find_panel_of_node(
		struct platform_device *pdev, char *panel_cfg)
{
	int len, i = 0;
	int ctrl_id = pdev->id - 1;
	char panel_name[MDSS_MAX_PANEL_LEN] = "";
	char ctrl_id_stream[3] =  "0:";
	char *str1 = NULL, *str2 = NULL, *override_cfg = NULL;
	char cfg_np_name[MDSS_MAX_PANEL_LEN] = "";
	struct device_node *dsi_pan_node = NULL, *mdss_node = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	struct mdss_panel_info *pinfo = &ctrl_pdata->panel_data.panel_info;

	len = strlen(panel_cfg);
	ctrl_pdata->panel_data.dsc_cfg_np_name[0] = '\0';
	if (!len) {
		/* no panel cfg chg, parse dt */
		pr_debug("%s:%d: no cmd line cfg present\n",
			 __func__, __LINE__);
		goto end;
	} else {
		/* check if any override parameters are set */
		pinfo->sim_panel_mode = 0;
		override_cfg = strnstr(panel_cfg, "#" OVERRIDE_CFG, len);
		if (override_cfg) {
			*override_cfg = '\0';
			if (mdss_dsi_set_override_cfg(override_cfg + 1,
					ctrl_pdata, panel_cfg))
				return NULL;
			len = strlen(panel_cfg);
		}

		if (ctrl_id == 1)
			strlcpy(ctrl_id_stream, "1:", 3);

		/* get controller number */
		str1 = strnstr(panel_cfg, ctrl_id_stream, len);
		if (!str1) {
			pr_err("%s: controller %s is not present in %s\n",
				__func__, ctrl_id_stream, panel_cfg);
			goto end;
		}
		if ((str1 != panel_cfg) && (*(str1-1) != ':')) {
			str1 += CMDLINE_DSI_CTL_NUM_STRING_LEN;
			pr_debug("false match with config node name in \"%s\". search again in \"%s\"\n",
				panel_cfg, str1);
			str1 = strnstr(str1, ctrl_id_stream, len);
			if (!str1) {
				pr_err("%s: 2. controller %s is not present in %s\n",
					__func__, ctrl_id_stream, str1);
				goto end;
			}
		}
		str1 += CMDLINE_DSI_CTL_NUM_STRING_LEN;

		/* get panel name */
		str2 = strnchr(str1, strlen(str1), ':');
		if (!str2) {
			strlcpy(panel_name, str1, MDSS_MAX_PANEL_LEN);
		} else {
			for (i = 0; (str1 + i) < str2; i++)
				panel_name[i] = *(str1 + i);
			panel_name[i] = 0;
		}
		pr_info("%s: cmdline:%s panel_name:%s\n",
			__func__, panel_cfg, panel_name);
		if (!strcmp(panel_name, NONE_PANEL))
			goto exit;

		mdss_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,mdss-mdp", 0);
		if (!mdss_node) {
			pr_err("%s: %d: mdss_node null\n",
			       __func__, __LINE__);
			return NULL;
		}
		dsi_pan_node = of_find_node_by_name(mdss_node, panel_name);
		if (!dsi_pan_node) {
			pr_err("%s: invalid pan node \"%s\"\n",
			       __func__, panel_name);
			goto end;
		} else {
			/* extract config node name if present */
			str1 += i;
			str2 = strnstr(str1, "config", strlen(str1));
			if (str2) {
				str1 = strnchr(str2, strlen(str2), ':');
				if (str1) {
					for (i = 0; ((str2 + i) < str1) &&
					     i < (MDSS_MAX_PANEL_LEN - 1); i++)
						cfg_np_name[i] = *(str2 + i);
					if ((i >= 0)
						&& (i < MDSS_MAX_PANEL_LEN))
						cfg_np_name[i] = 0;
				} else {
					strlcpy(cfg_np_name, str2,
						MDSS_MAX_PANEL_LEN);
				}
				strlcpy(ctrl_pdata->panel_data.dsc_cfg_np_name,
					cfg_np_name, MDSS_MAX_PANEL_LEN);
			}
		}

		return dsi_pan_node;
	}
end:
	if (strcmp(panel_name, NONE_PANEL))
		dsi_pan_node = mdss_dsi_pref_prim_panel(pdev);
exit:
	return dsi_pan_node;
}

static struct device_node *mdss_dsi_config_panel(struct platform_device *pdev,
	int ndx)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	char panel_cfg[MDSS_MAX_PANEL_LEN];
	struct device_node *dsi_pan_node = NULL;
	int rc = 0;

	if (!ctrl_pdata) {
		pr_err("%s: Unable to get the ctrl_pdata\n", __func__);
		return NULL;
	}

	/* DSI panels can be different between controllers */
	rc = mdss_dsi_get_panel_cfg(panel_cfg, ctrl_pdata);
	if (!rc)
		/* dsi panel cfg not present */
		pr_warn("%s:%d:dsi specific cfg not present\n",
			__func__, __LINE__);

	/* find panel device node */
	dsi_pan_node = mdss_dsi_find_panel_of_node(pdev, panel_cfg);
	if (!dsi_pan_node) {
		pr_err("%s: can't find panel node %s\n", __func__, panel_cfg);
		of_node_put(dsi_pan_node);
		return NULL;
	}

	rc = mdss_dsi_panel_init(dsi_pan_node, ctrl_pdata, ndx);
	if (rc) {
		pr_err("%s: dsi panel init failed\n", __func__);
		of_node_put(dsi_pan_node);
		return NULL;
	}

	return dsi_pan_node;
}

static int mdss_dsi_ctrl_clock_init(struct platform_device *ctrl_pdev,
				    struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;
	struct mdss_dsi_clk_info info;
	struct mdss_dsi_clk_client client1 = {"dsi_clk_client"};
	struct mdss_dsi_clk_client client2 = {"mdp_event_client"};
	void *handle;

	if (mdss_dsi_link_clk_init(ctrl_pdev, ctrl_pdata)) {
		pr_err("%s: unable to initialize Dsi ctrl clks\n", __func__);
		return -EPERM;
	}

	memset(&info, 0x0, sizeof(info));

	info.core_clks.mdp_core_clk = ctrl_pdata->shared_data->mdp_core_clk;
	info.core_clks.mnoc_clk = ctrl_pdata->shared_data->mnoc_clk;
	info.core_clks.ahb_clk = ctrl_pdata->shared_data->ahb_clk;
	info.core_clks.axi_clk = ctrl_pdata->shared_data->axi_clk;
	info.core_clks.mmss_misc_ahb_clk =
		ctrl_pdata->shared_data->mmss_misc_ahb_clk;

	info.link_clks.esc_clk = ctrl_pdata->esc_clk;
	info.link_clks.byte_clk = ctrl_pdata->byte_clk;
	info.link_clks.pixel_clk = ctrl_pdata->pixel_clk;
	info.link_clks.byte_intf_clk = ctrl_pdata->byte_intf_clk;

	info.pre_clkoff_cb = mdss_dsi_pre_clkoff_cb;
	info.post_clkon_cb = mdss_dsi_post_clkon_cb;
	info.pre_clkon_cb = mdss_dsi_pre_clkon_cb;
	info.post_clkoff_cb = mdss_dsi_post_clkoff_cb;
	info.priv_data = ctrl_pdata;
	snprintf(info.name, DSI_CLK_NAME_LEN, "DSI%d", ctrl_pdata->ndx);
	ctrl_pdata->clk_mngr = mdss_dsi_clk_init(&info);
	if (IS_ERR_OR_NULL(ctrl_pdata->clk_mngr)) {
		rc = PTR_ERR(ctrl_pdata->clk_mngr);
		ctrl_pdata->clk_mngr = NULL;
		pr_err("dsi clock registration failed, rc = %d\n", rc);
		goto error_link_clk_deinit;
	}

	/*
	 * There are two clients that control dsi clocks. MDP driver controls
	 * the clock through MDSS_PANEL_EVENT_CLK_CTRL event and dsi driver
	 * through clock interface. To differentiate between the votes from the
	 * two clients, dsi driver will use two different handles to vote for
	 * clock states from dsi and mdp driver.
	 */
	handle = mdss_dsi_clk_register(ctrl_pdata->clk_mngr, &client1);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
		       client1.client_name, rc);
		goto error_clk_deinit;
	} else {
		ctrl_pdata->dsi_clk_handle = handle;
	}

	handle = mdss_dsi_clk_register(ctrl_pdata->clk_mngr, &client2);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
		       client2.client_name, rc);
		goto error_clk_client_deregister;
	} else {
		ctrl_pdata->mdp_clk_handle = handle;
	}

	return rc;
error_clk_client_deregister:
	mdss_dsi_clk_deregister(ctrl_pdata->dsi_clk_handle);
error_clk_deinit:
	mdss_dsi_clk_deinit(ctrl_pdata->clk_mngr);
error_link_clk_deinit:
	mdss_dsi_link_clk_deinit(&ctrl_pdev->dev, ctrl_pdata);
	return rc;
}

static int mdss_dsi_set_clk_rates(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	rc = mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
					MDSS_DSI_LINK_BYTE_CLK,
					ctrl_pdata->byte_clk_rate,
					MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc) {
		pr_err("%s: dsi_byte_clk - clk_set_rate failed\n",
				__func__);
		return rc;
	}

	rc = mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
					MDSS_DSI_LINK_PIX_CLK,
					ctrl_pdata->pclk_rate,
					MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc) {
		pr_err("%s: dsi_pixel_clk - clk_set_rate failed\n",
			__func__);
		return rc;
	}

	rc = mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
					MDSS_DSI_LINK_ESC_CLK,
					ctrl_pdata->esc_clk_rate_hz,
					MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc) {
		pr_err("%s: dsi_esc_clk - clk_set_rate failed\n",
			__func__);
		return rc;
	}

	return rc;
}

static int mdss_dsi_cont_splash_config(struct mdss_panel_info *pinfo,
				       struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	void *clk_handle;
	int rc = 0;

	if (pinfo->cont_splash_enabled) {
		rc = mdss_dsi_panel_power_ctrl(&(ctrl_pdata->panel_data),
			MDSS_PANEL_POWER_ON);
		if (rc) {
			pr_err("%s: Panel power on failed\n", __func__);
			return rc;
		}
		if (ctrl_pdata->bklt_ctrl == BL_PWM)
			mdss_dsi_panel_pwm_enable(ctrl_pdata);
		ctrl_pdata->ctrl_state |= (CTRL_STATE_PANEL_INIT |
			CTRL_STATE_MDP_ACTIVE | CTRL_STATE_DSI_ACTIVE);

		/*
		 * MDP client removes this extra vote during splash reconfigure
		 * for command mode panel from interface. DSI removes the vote
		 * during suspend-resume for video mode panel.
		 */
		if (ctrl_pdata->panel_data.panel_info.type == MIPI_CMD_PANEL)
			clk_handle = ctrl_pdata->mdp_clk_handle;
		else
			clk_handle = ctrl_pdata->dsi_clk_handle;

		mdss_dsi_clk_ctrl(ctrl_pdata, clk_handle,
				  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
		mdss_dsi_read_hw_revision(ctrl_pdata);
		mdss_dsi_read_phy_revision(ctrl_pdata);
		ctrl_pdata->is_phyreg_enabled = 1;
		if (pinfo->type == MIPI_CMD_PANEL)
			mdss_dsi_set_burst_mode(ctrl_pdata);
		mdss_dsi_clamp_phy_reset_config(ctrl_pdata, true);
	} else {
		/* Turn on the clocks to read the DSI and PHY revision */
		mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
				  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_ON);
		mdss_dsi_read_hw_revision(ctrl_pdata);
		mdss_dsi_read_phy_revision(ctrl_pdata);
		mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
				  MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_OFF);
		pinfo->panel_power_state = MDSS_PANEL_POWER_OFF;
	}

	return rc;
}

static int mdss_dsi_get_bridge_chip_params(struct mdss_panel_info *pinfo,
				       struct mdss_dsi_ctrl_pdata *ctrl_pdata,
				       struct platform_device *pdev)
{
	int rc = 0;
	u32 temp_val = 0;

	if (!ctrl_pdata || !pdev || !pinfo) {
		pr_err("%s: Invalid Params ctrl_pdata=%pK, pdev=%pK\n",
			__func__, ctrl_pdata, pdev);
		rc = -EINVAL;
		goto end;
	}

	if (pinfo->is_dba_panel) {
		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,bridge-index", &temp_val);
		if (rc) {
			pr_err("%s:%d Unable to read qcom,bridge-index, ret=%d\n",
				__func__, __LINE__, rc);
			goto end;
		}
		pr_debug("%s: DT property %s is %X\n", __func__,
			"qcom,bridge-index", temp_val);
		ctrl_pdata->bridge_index = temp_val;
	}
end:
	return rc;
}

static int mdss_dsi_ctrl_validate_config(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;

	if (!ctrl) {
		rc = -EINVAL;
		goto error;
	}

	/*
	 * check to make sure that the byte interface clock is specified for
	 * DSI ctrl version 2 and above.
	 */
	if ((ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_200) &&
		(!ctrl->byte_intf_clk)) {
		pr_err("%s: byte intf clk must be defined for hw rev 0x%08x\n",
			__func__, ctrl->shared_data->hw_rev);
		rc = -EINVAL;
	}

error:
	return rc;
}

static int mdss_dsi_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0;
	u32 index;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	struct device_node *dsi_pan_node = NULL;
	const char *ctrl_name;
	struct mdss_util_intf *util;
	static int te_irq_registered;
	struct mdss_panel_data *pdata;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("%s: pdev not found for DSI controller\n", __func__);
		return -ENODEV;
	}
	rc = of_property_read_u32(pdev->dev.of_node,
				  "cell-index", &index);
	if (rc) {
		dev_err(&pdev->dev, "%s: Cell-index not specified, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (index == 0)
		pdev->id = 1;
	else
		pdev->id = 2;

	ctrl_pdata = mdss_dsi_get_ctrl(index);
	if (!ctrl_pdata) {
		pr_err("%s: Unable to get the ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	platform_set_drvdata(pdev, ctrl_pdata);

	util = mdss_get_util_intf();
	if (util == NULL) {
		pr_err("Failed to get mdss utility functions\n");
		return -ENODEV;
	}

	ctrl_pdata->mdss_util = util;
	atomic_set(&ctrl_pdata->te_irq_ready, 0);

	ctrl_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!ctrl_name)
		pr_info("%s:%d, DSI Ctrl name not specified\n",
			__func__, __LINE__);
	else
		pr_info("%s: DSI Ctrl name = %s\n",
			__func__, ctrl_name);

	rc = mdss_dsi_pinctrl_init(pdev);
	if (rc)
		pr_warn("%s: failed to get pin resources\n", __func__);

	if (index == 0) {
		ctrl_pdata->panel_data.panel_info.pdest = DISPLAY_1;
		ctrl_pdata->ndx = DSI_CTRL_0;
	} else {
		ctrl_pdata->panel_data.panel_info.pdest = DISPLAY_2;
		ctrl_pdata->ndx = DSI_CTRL_1;
	}

	if (mdss_dsi_ctrl_clock_init(pdev, ctrl_pdata)) {
		pr_err("%s: unable to initialize dsi clk manager\n", __func__);
		return -EPERM;
	}

	dsi_pan_node = mdss_dsi_config_panel(pdev, index);
	if (!dsi_pan_node) {
		pr_err("%s: panel configuration failed\n", __func__);
		return -EINVAL;
	}

	if (!mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) ||
		(mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
		(ctrl_pdata->panel_data.panel_info.pdest == DISPLAY_1))) {
		rc = mdss_panel_parse_bl_settings(dsi_pan_node, ctrl_pdata);
		if (rc) {
			pr_warn("%s: dsi bl settings parse failed\n", __func__);
			/* Panels like AMOLED and dsi2hdmi chip
			 * does not need backlight control.
			 * So we should not fail probe here.
			 */
			ctrl_pdata->bklt_ctrl = UNKNOWN_CTRL;
		}
	} else {
		ctrl_pdata->bklt_ctrl = UNKNOWN_CTRL;
	}

	rc = dsi_panel_device_register(pdev, dsi_pan_node, ctrl_pdata);
	if (rc) {
		pr_err("%s: dsi panel dev reg failed\n", __func__);
		goto error_pan_node;
	}

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if (!(mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
		mdss_dsi_is_ctrl_clk_slave(ctrl_pdata)) &&
		(pinfo->dynamic_fps || pinfo->dynamic_bitclk)) {
		rc = mdss_dsi_shadow_clk_init(pdev, ctrl_pdata);

		if (rc) {
			pr_err("%s: unable to initialize shadow ctrl clks\n",
					__func__);
			rc = -EPERM;
		}
	}

	rc = mdss_dsi_set_clk_rates(ctrl_pdata);
	if (rc) {
		pr_err("%s: Failed to set dsi clk rates\n", __func__);
		return rc;
	}

	rc = mdss_dsi_cont_splash_config(pinfo, ctrl_pdata);
	if (rc) {
		pr_err("%s: Failed to set dsi splash config\n", __func__);
		return rc;
	}

	if (mdss_dsi_is_te_based_esd(ctrl_pdata)) {
		rc = devm_request_irq(&pdev->dev,
			gpio_to_irq(ctrl_pdata->disp_te_gpio),
			hw_vsync_handler, IRQF_TRIGGER_FALLING,
			"VSYNC_GPIO", ctrl_pdata);
		if (rc) {
			pr_err("%s: TE request_irq failed for ESD\n", __func__);
			goto error_shadow_clk_deinit;
		}
		te_irq_registered = 1;
		disable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	}

	pdata = &ctrl_pdata->panel_data;
	init_completion(&pdata->te_done);
	if (pdata->panel_info.type == MIPI_CMD_PANEL) {
		if (!te_irq_registered) {
			rc = devm_request_irq(&pdev->dev,
				gpio_to_irq(pdata->panel_te_gpio),
				test_hw_vsync_handler, IRQF_TRIGGER_FALLING,
				"VSYNC_GPIO", &ctrl_pdata->panel_data);
			if (rc) {
				pr_err("%s: TE request_irq failed\n", __func__);
				goto error_shadow_clk_deinit;
			}
			te_irq_registered = 1;
			disable_irq_nosync(gpio_to_irq(pdata->panel_te_gpio));
		}
	}

	rc = mdss_dsi_get_bridge_chip_params(pinfo, ctrl_pdata, pdev);
	if (rc) {
		pr_err("%s: Failed to get bridge params\n", __func__);
		goto error_shadow_clk_deinit;
	}

	ctrl_pdata->workq = create_workqueue("mdss_dsi_dba");
	if (!ctrl_pdata->workq) {
		pr_err("%s: Error creating workqueue\n", __func__);
		rc = -EPERM;
		goto error_pan_node;
	}

	INIT_DELAYED_WORK(&ctrl_pdata->dba_work, mdss_dsi_dba_work);

	rc = mdss_dsi_ctrl_validate_config(ctrl_pdata);
	if (rc) {
		pr_err("%s: invalid controller configuration\n", __func__);
		goto error_shadow_clk_deinit;
	}

	pr_info("%s: Dsi Ctrl->%d initialized, DSI rev:0x%x, PHY rev:0x%x\n",
		__func__, index, ctrl_pdata->shared_data->hw_rev,
		ctrl_pdata->shared_data->phy_rev);
	mdss_dsi_pm_qos_add_request(ctrl_pdata);

	if (index == 0)
		ctrl_pdata->shared_data->dsi0_active = true;
	else
		ctrl_pdata->shared_data->dsi1_active = true;

	mdss_dsi_debug_bus_init(mdss_dsi_res);

	return 0;

error_shadow_clk_deinit:
	mdss_dsi_shadow_clk_deinit(&pdev->dev, ctrl_pdata);
error_pan_node:
#ifndef CONFIG_BACKLIGHT_QCOM_SPMI_WLED
	mdss_dsi_unregister_bl_settings(ctrl_pdata);
#endif
	of_node_put(dsi_pan_node);
	return rc;
}

static int mdss_dsi_bus_scale_init(struct platform_device *pdev,
			    struct dsi_shared_data  *sdata)
{
	int rc = 0;

	sdata->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (IS_ERR_OR_NULL(sdata->bus_scale_table)) {
		rc = PTR_ERR(sdata->bus_scale_table);
		pr_err("%s: msm_bus_cl_get_pdata() failed, rc=%d\n", __func__,
								     rc);
		return rc;
		sdata->bus_scale_table = NULL;
	}

	sdata->bus_handle =
		msm_bus_scale_register_client(sdata->bus_scale_table);

	if (!sdata->bus_handle) {
		rc = -EINVAL;
		pr_err("%sbus_client register failed\n", __func__);
	}

	return rc;
}

static void mdss_dsi_bus_scale_deinit(struct dsi_shared_data *sdata)
{
	if (sdata->bus_handle) {
		if (sdata->bus_refcount)
			msm_bus_scale_client_update_request(sdata->bus_handle,
									0);

		sdata->bus_refcount = 0;
		msm_bus_scale_unregister_client(sdata->bus_handle);
		sdata->bus_handle = 0;
	}
}

static int mdss_dsi_parse_dt_params(struct platform_device *pdev,
		struct dsi_shared_data *sdata)
{
	int rc = 0;

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,mmss-ulp-clamp-ctrl-offset",
			&sdata->ulps_clamp_ctrl_off);
	if (!rc) {
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,mmss-phyreset-ctrl-offset",
				&sdata->ulps_phyrst_ctrl_off);
	}

	sdata->cmd_clk_ln_recovery_en =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,dsi-clk-ln-recovery");

	return 0;
}

static void mdss_dsi_res_deinit(struct platform_device *pdev)
{
	int i;
	struct mdss_dsi_data *dsi_res = platform_get_drvdata(pdev);
	struct dsi_shared_data *sdata;

	if (!dsi_res) {
		pr_err("%s: DSI root device drvdata not found\n", __func__);
		return;
	}

	for (i = 0; i < DSI_CTRL_MAX; i++) {
		if (dsi_res->ctrl_pdata[i]) {
			if (dsi_res->ctrl_pdata[i]->ds_registered) {
				struct mdss_panel_info *pinfo =
				&dsi_res->ctrl_pdata[i]->panel_data.panel_info;

				if (pinfo)
					mdss_dba_utils_deinit(pinfo->dba_data);
			}

			devm_kfree(&pdev->dev, dsi_res->ctrl_pdata[i]);
		}
	}

	sdata = dsi_res->shared_data;
	if (!sdata)
		goto res_release;

	for (i = (DSI_MAX_PM - 1); i >= DSI_CORE_PM; i--) {
		if (msm_dss_config_vreg(&pdev->dev,
				sdata->power_data[i].vreg_config,
				sdata->power_data[i].num_vreg, 1) < 0)
			pr_err("%s: failed to de-init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
		mdss_dsi_put_dt_vreg_data(&pdev->dev,
			&sdata->power_data[i]);
	}

	mdss_dsi_bus_scale_deinit(sdata);
	mdss_dsi_core_clk_deinit(&pdev->dev, sdata);

	if (sdata)
		devm_kfree(&pdev->dev, sdata);

res_release:
	if (dsi_res)
		devm_kfree(&pdev->dev, dsi_res);

	return;
}

static int mdss_dsi_res_init(struct platform_device *pdev)
{
	int rc = 0, i;
	struct dsi_shared_data *sdata;

	mdss_dsi_res = platform_get_drvdata(pdev);
	if (!mdss_dsi_res) {
		mdss_dsi_res = devm_kzalloc(&pdev->dev,
					  sizeof(struct mdss_dsi_data),
					  GFP_KERNEL);
		if (!mdss_dsi_res) {
			pr_err("%s: FAILED: cannot alloc dsi data\n",
			       __func__);
			rc = -ENOMEM;
			goto mem_fail;
		}

		mdss_dsi_res->shared_data = devm_kzalloc(&pdev->dev,
				sizeof(struct dsi_shared_data),
				GFP_KERNEL);
		pr_debug("%s Allocated shared_data=%pK\n", __func__,
				mdss_dsi_res->shared_data);
		if (!mdss_dsi_res->shared_data) {
			pr_err("%s Unable to alloc mem for shared_data\n",
					__func__);
			rc = -ENOMEM;
			goto mem_fail;
		}

		sdata = mdss_dsi_res->shared_data;

		rc = mdss_dsi_parse_dt_params(pdev, sdata);
		if (rc) {
			pr_err("%s: failed to parse mdss dsi DT params\n",
				__func__);
			goto mem_fail;
		}

		rc = mdss_dsi_core_clk_init(pdev, sdata);
		if (rc) {
			pr_err("%s: failed to initialize DSI core clocks\n",
				__func__);
			goto mem_fail;
		}

		/* Parse the regulator information */
		for (i = DSI_CORE_PM; i < DSI_MAX_PM; i++) {
			rc = mdss_dsi_get_dt_vreg_data(&pdev->dev,
				pdev->dev.of_node, &sdata->power_data[i], i);
			if (rc) {
				pr_err("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
					__func__, __mdss_dsi_pm_name(i), rc);
				i--;
				for (; i >= DSI_CORE_PM; i--)
					mdss_dsi_put_dt_vreg_data(&pdev->dev,
						&sdata->power_data[i]);
				goto mem_fail;
			}
		}
		rc = mdss_dsi_regulator_init(pdev, sdata);
		if (rc) {
			pr_err("%s: failed to init regulator, rc=%d\n",
							__func__, rc);
			goto mem_fail;
		}

		rc = mdss_dsi_bus_scale_init(pdev, sdata);
		if (rc) {
			pr_err("%s: failed to init bus scale settings, rc=%d\n",
							__func__, rc);
			goto mem_fail;
		}

		mutex_init(&sdata->phy_reg_lock);
		mutex_init(&sdata->pm_qos_lock);

		for (i = 0; i < DSI_CTRL_MAX; i++) {
			mdss_dsi_res->ctrl_pdata[i] = devm_kzalloc(&pdev->dev,
					sizeof(struct mdss_dsi_ctrl_pdata),
					GFP_KERNEL);
			if (!mdss_dsi_res->ctrl_pdata[i]) {
				pr_err("%s Unable to alloc mem for ctrl=%d\n",
						__func__, i);
				rc = -ENOMEM;
				goto mem_fail;
			}
			pr_debug("%s Allocated ctrl_pdata[%d]=%pK\n",
				__func__, i, mdss_dsi_res->ctrl_pdata[i]);
			mdss_dsi_res->ctrl_pdata[i]->shared_data =
				mdss_dsi_res->shared_data;
		}

		platform_set_drvdata(pdev, mdss_dsi_res);
	}

	mdss_dsi_res->pdev = pdev;
	pr_debug("%s: Setting up mdss_dsi_res=%pK\n", __func__, mdss_dsi_res);

	return 0;

mem_fail:
	mdss_dsi_res_deinit(pdev);
	return rc;
}

static int mdss_dsi_parse_hw_cfg(struct platform_device *pdev, char *pan_cfg)
{
	const char *data;
	struct mdss_dsi_data *dsi_res = platform_get_drvdata(pdev);
	struct dsi_shared_data *sdata;
	char dsi_cfg[20];
	char *cfg_prim = NULL, *cfg_sec = NULL, *ch = NULL;
	int i = 0;

	if (!dsi_res) {
		pr_err("%s: DSI root device drvdata not found\n", __func__);
		return -EINVAL;
	}

	sdata = mdss_dsi_res->shared_data;
	if (!sdata) {
		pr_err("%s: DSI shared data not found\n", __func__);
		return -EINVAL;
	}

	sdata->hw_config = SINGLE_DSI;

	if (pan_cfg)
		cfg_prim = strnstr(pan_cfg, "cfg:", strlen(pan_cfg));
	if (cfg_prim) {
		cfg_prim += 4;

		cfg_sec = strnchr(cfg_prim, strlen(cfg_prim), ':');
		if (!cfg_sec)
			cfg_sec = cfg_prim + strlen(cfg_prim);

		for (i = 0; ((cfg_prim + i) < cfg_sec) &&
		     (*(cfg_prim+i) != '#'); i++)
			dsi_cfg[i] = *(cfg_prim + i);

		dsi_cfg[i] = '\0';
		data = dsi_cfg;
	} else {
		data = of_get_property(pdev->dev.of_node,
			"hw-config", NULL);
	}

	if (data) {
		/*
		 * To handle the  override parameter (#override:sim)
		 * passed for simulator panels
		 */
		ch = strnstr(data, "#", strlen(data));
		ch ? *ch = '\0' : false;

		if (!strcmp(data, "dual_dsi"))
			sdata->hw_config = DUAL_DSI;
		else if (!strcmp(data, "split_dsi"))
			sdata->hw_config = SPLIT_DSI;
		else if (!strcmp(data, "single_dsi"))
			sdata->hw_config = SINGLE_DSI;
		else
			pr_err("%s: Incorrect string for DSI config:%s. Setting default as SINGLE_DSI\n",
				__func__, data);
	} else {
		pr_err("%s: Error: No DSI HW config found\n",
			__func__);
		return -EINVAL;
	}

	pr_debug("%s: DSI h/w configuration is %d\n", __func__,
		sdata->hw_config);

	return 0;
}

static void mdss_dsi_parse_pll_src_cfg(struct platform_device *pdev,
	char *pan_cfg)
{
	const char *data;
	char *pll_ptr, pll_cfg[10] = {'\0'};
	struct dsi_shared_data *sdata = mdss_dsi_res->shared_data;

	sdata->pll_src_config = PLL_SRC_DEFAULT;

	if (pan_cfg) {
		pll_ptr = strnstr(pan_cfg, ":pll0", strlen(pan_cfg));
		if (!pll_ptr) {
			pll_ptr = strnstr(pan_cfg, ":pll1", strlen(pan_cfg));
			if (pll_ptr)
				strlcpy(pll_cfg, "PLL1", strlen(pll_cfg));
		} else {
			strlcpy(pll_cfg, "PLL0", strlen(pll_cfg));
		}
	}
	data = pll_cfg;

	if (!data || !strcmp(data, ""))
		data = of_get_property(pdev->dev.of_node,
			"pll-src-config", NULL);
	if (data) {
		if (!strcmp(data, "PLL0"))
			sdata->pll_src_config = PLL_SRC_0;
		else if (!strcmp(data, "PLL1"))
			sdata->pll_src_config = PLL_SRC_1;
		else
			pr_err("%s: invalid pll src config %s\n",
				__func__, data);
	} else {
		pr_debug("%s: PLL src config not specified\n", __func__);
	}

	pr_debug("%s: pll_src_config = %d", __func__, sdata->pll_src_config);

	return;
}

static int mdss_dsi_validate_pll_src_config(struct dsi_shared_data *sdata)
{
	int rc = 0;

	/*
	 * DSI PLL1 can only drive DSI PHY1. As such:
	 *     - For split dsi config, only PLL0 is supported
	 *     - For dual dsi config, DSI0-PLL0 and DSI1-PLL1 is the only
	 *       possible configuration
	 */
	if (mdss_dsi_is_hw_config_split(sdata) &&
		mdss_dsi_is_pll_src_pll1(sdata)) {
		pr_err("%s: unsupported PLL config: using PLL1 for split-dsi\n",
			__func__);
		rc = -EINVAL;
		goto error;
	}

	if (mdss_dsi_is_hw_config_dual(sdata) &&
		!mdss_dsi_is_pll_src_default(sdata)) {
		pr_debug("%s: pll src config not applicable for dual-dsi\n",
			__func__);
		sdata->pll_src_config = PLL_SRC_DEFAULT;
	}

error:
	return rc;
}

static int mdss_dsi_validate_config(struct platform_device *pdev)
{
	struct dsi_shared_data *sdata = mdss_dsi_res->shared_data;

	return mdss_dsi_validate_pll_src_config(sdata);
}

static const struct of_device_id mdss_dsi_ctrl_dt_match[] = {
	{.compatible = "qcom,mdss-dsi-ctrl"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_dsi_ctrl_dt_match);

static int mdss_dsi_probe(struct platform_device *pdev)
{
	struct mdss_panel_cfg *pan_cfg = NULL;
	struct mdss_util_intf *util;
	char *panel_cfg;
	int rc = 0;

	util = mdss_get_util_intf();
	if (util == NULL) {
		pr_err("%s: Failed to get mdss utility functions\n", __func__);
		return -ENODEV;
	}

	if (!util->mdp_probe_done) {
		pr_err("%s: MDP not probed yet!\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!pdev || !pdev->dev.of_node) {
		pr_err("%s: DSI driver only supports device tree probe\n",
			__func__);
		return -ENOTSUPP;
	}

	pan_cfg = util->panel_intf_type(MDSS_PANEL_INTF_HDMI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (pan_cfg) {
		pr_debug("%s: HDMI is primary\n", __func__);
		return -ENODEV;
	}

	pan_cfg = util->panel_intf_type(MDSS_PANEL_INTF_DSI);
	if (IS_ERR_OR_NULL(pan_cfg)) {
		rc = PTR_ERR(pan_cfg);
		goto error;
	} else {
		panel_cfg = pan_cfg->arg_cfg;
	}

	rc = mdss_dsi_res_init(pdev);
	if (rc) {
		pr_err("%s Unable to set dsi res\n", __func__);
		return rc;
	}

	rc = mdss_dsi_parse_hw_cfg(pdev, panel_cfg);
	if (rc) {
		pr_err("%s Unable to parse dsi h/w config\n", __func__);
		mdss_dsi_res_deinit(pdev);
		return rc;
	}

	mdss_dsi_parse_pll_src_cfg(pdev, panel_cfg);

	of_platform_populate(pdev->dev.of_node, mdss_dsi_ctrl_dt_match,
				NULL, &pdev->dev);

	rc = mdss_dsi_validate_config(pdev);
	if (rc) {
		pr_err("%s: Invalid DSI hw configuration\n", __func__);
		goto error;
	}

	mdss_dsi_config_clk_src(pdev);

error:
	return rc;
}

static int mdss_dsi_remove(struct platform_device *pdev)
{
	mdss_dsi_res_deinit(pdev);
	return 0;
}

static int mdss_dsi_ctrl_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	mdss_dsi_pm_qos_remove_request(ctrl_pdata->shared_data);

	if (msm_dss_config_vreg(&pdev->dev,
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 1) < 0)
		pr_err("%s: failed to de-init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
	mdss_dsi_put_dt_vreg_data(&pdev->dev, &ctrl_pdata->panel_power_data);

	mfd = platform_get_drvdata(pdev);
	msm_dss_iounmap(&ctrl_pdata->mmss_misc_io);
	msm_dss_iounmap(&ctrl_pdata->phy_io);
	msm_dss_iounmap(&ctrl_pdata->ctrl_io);
	mdss_dsi_debugfs_cleanup(ctrl_pdata);

	if (ctrl_pdata->workq)
		destroy_workqueue(ctrl_pdata->workq);

	return 0;
}

struct device dsi_dev;

int mdss_dsi_retrieve_ctrl_resources(struct platform_device *pdev, int mode,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	u32 index;

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: Cell-index not specified, rc=%d\n",
						__func__, rc);
		return rc;
	}

	if (index == 0) {
		if (mode != DISPLAY_1) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong\n",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else if (index == 1) {
		if (mode != DISPLAY_2) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong\n",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else {
		pr_err("%s:%d Unknown Ctrl mapped to panel\n",
			       __func__, __LINE__);
		return -EPERM;
	}

	rc = msm_dss_ioremap_byname(pdev, &ctrl->ctrl_io, "dsi_ctrl");
	if (rc) {
		pr_err("%s:%d unable to remap dsi ctrl resources\n",
			       __func__, __LINE__);
		return rc;
	}

	ctrl->ctrl_base = ctrl->ctrl_io.base;
	ctrl->reg_size = ctrl->ctrl_io.len;

	rc = msm_dss_ioremap_byname(pdev, &ctrl->phy_io, "dsi_phy");
	if (rc) {
		pr_err("%s:%d unable to remap dsi phy resources\n",
			       __func__, __LINE__);
		return rc;
	}

	rc = msm_dss_ioremap_byname(pdev, &ctrl->phy_regulator_io,
			"dsi_phy_regulator");
	if (rc)
		pr_debug("%s:%d unable to remap dsi phy regulator resources\n",
			       __func__, __LINE__);
	else
		pr_info("%s: phy_regulator_base=%pK phy_regulator_size=%x\n",
			__func__, ctrl->phy_regulator_io.base,
			ctrl->phy_regulator_io.len);

	pr_info("%s: ctrl_base=%pK ctrl_size=%x phy_base=%pK phy_size=%x\n",
		__func__, ctrl->ctrl_base, ctrl->reg_size, ctrl->phy_io.base,
		ctrl->phy_io.len);

	rc = msm_dss_ioremap_byname(pdev, &ctrl->mmss_misc_io,
		"mmss_misc_phys");
	if (rc) {
		pr_debug("%s:%d mmss_misc IO remap failed\n",
			__func__, __LINE__);
	}

	return 0;
}

static int mdss_dsi_irq_init(struct device *dev, int irq_no,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	ret = devm_request_irq(dev, irq_no, mdss_dsi_isr,
				0x0, "DSI", ctrl);
	if (ret) {
		pr_err("msm_dsi_irq_init request_irq() failed!\n");
		return ret;
	}

	disable_irq(irq_no);
	ctrl->dsi_hw->irq_info = kzalloc(sizeof(struct irq_info), GFP_KERNEL);
	if (!ctrl->dsi_hw->irq_info)
		return -ENOMEM;
	ctrl->dsi_hw->irq_info->irq = irq_no;
	ctrl->dsi_hw->irq_info->irq_ena = false;

	return ret;
}

static void __set_lane_map(struct mdss_dsi_ctrl_pdata *ctrl,
	enum dsi_physical_lane_id lane0,
	enum dsi_physical_lane_id lane1,
	enum dsi_physical_lane_id lane2,
	enum dsi_physical_lane_id lane3)
{
	ctrl->lane_map[DSI_LOGICAL_LANE_0] = lane0;
	ctrl->lane_map[DSI_LOGICAL_LANE_1] = lane1;
	ctrl->lane_map[DSI_LOGICAL_LANE_2] = lane2;
	ctrl->lane_map[DSI_LOGICAL_LANE_3] = lane3;
}

static void mdss_dsi_parse_lane_swap(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	const char *data;
	u8 temp[DSI_LOGICAL_LANE_MAX];
	int i;

	/* First, check for the newer version of the binding */
	rc = of_property_read_u8_array(np, "qcom,lane-map-v2", temp,
		DSI_LOGICAL_LANE_MAX);
	if (!rc) {
		for (i = DSI_LOGICAL_LANE_0; i < DSI_LOGICAL_LANE_MAX; i++)
			ctrl->lane_map[i] = BIT(temp[i]);
		return;
	} else if (rc != -EINVAL) {
		pr_warn("%s: invalid lane map specfied. Defaulting to <0 1 2 3>\n",
			__func__);
		goto set_default;
	}

	/* Check if an older version of the binding is present */
	data = of_get_property(np, "qcom,lane-map", NULL);
	if (!data)
		goto set_default;

	if (!strcmp(data, "lane_map_3012")) {
		ctrl->dlane_swap = DSI_LANE_MAP_3012;
		__set_lane_map(ctrl,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0);
	} else if (!strcmp(data, "lane_map_2301")) {
		ctrl->dlane_swap = DSI_LANE_MAP_2301;
		__set_lane_map(ctrl,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_1);
	} else if (!strcmp(data, "lane_map_1230")) {
		ctrl->dlane_swap = DSI_LANE_MAP_1230;
		__set_lane_map(ctrl,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_2);
	} else if (!strcmp(data, "lane_map_0321")) {
		ctrl->dlane_swap = DSI_LANE_MAP_0321;
		__set_lane_map(ctrl,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1);
	} else if (!strcmp(data, "lane_map_1032")) {
		ctrl->dlane_swap = DSI_LANE_MAP_1032;
		__set_lane_map(ctrl,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2);
	} else if (!strcmp(data, "lane_map_2103")) {
		ctrl->dlane_swap = DSI_LANE_MAP_2103;
		__set_lane_map(ctrl,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0,
			DSI_PHYSICAL_LANE_3);
	} else if (!strcmp(data, "lane_map_3210")) {
		ctrl->dlane_swap = DSI_LANE_MAP_3210;
		__set_lane_map(ctrl,
			DSI_PHYSICAL_LANE_3,
			DSI_PHYSICAL_LANE_2,
			DSI_PHYSICAL_LANE_1,
			DSI_PHYSICAL_LANE_0);
	} else {
		pr_warn("%s: invalid lane map %s specified. defaulting to lane_map0123\n",
			__func__, data);
	}

	return;

set_default:
	/* default lane mapping */
	__set_lane_map(ctrl, DSI_PHYSICAL_LANE_0, DSI_PHYSICAL_LANE_1,
		DSI_PHYSICAL_LANE_2, DSI_PHYSICAL_LANE_3);
	ctrl->dlane_swap = DSI_LANE_MAP_0123;
}

static int mdss_dsi_parse_ctrl_params(struct platform_device *ctrl_pdev,
	struct device_node *pan_node, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int i, len;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	const char *data;

	ctrl_pdata->null_insert_enabled = of_property_read_bool(
		ctrl_pdev->dev.of_node, "qcom,null-insertion-enabled");

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-strength-ctrl", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings\n",
			__func__, __LINE__);
		return -EINVAL;
	} else {
		pinfo->mipi.dsi_phy_db.strength_len = len;
		for (i = 0; i < len; i++)
			pinfo->mipi.dsi_phy_db.strength[i] = data[i];
	}

	pinfo->mipi.dsi_phy_db.reg_ldo_mode = of_property_read_bool(
		ctrl_pdev->dev.of_node, "qcom,regulator-ldo-mode");

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-regulator-settings", &len);
	if (!data) {
		pr_debug("%s:%d, Unable to read Phy regulator settings\n",
			__func__, __LINE__);
		pinfo->mipi.dsi_phy_db.regulator_len = 0;
	} else {
		pinfo->mipi.dsi_phy_db.regulator_len = len;
		for (i = 0; i < len; i++)
			pinfo->mipi.dsi_phy_db.regulator[i] = data[i];
	}

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-bist-ctrl", &len);
	if ((!data) || (len != 6))
		pr_debug("%s:%d, Unable to read Phy Bist Ctrl settings\n",
			__func__, __LINE__);
	else
		for (i = 0; i < len; i++)
			pinfo->mipi.dsi_phy_db.bistctrl[i] = data[i];

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-lane-config", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read Phy lane configure settings\n",
			__func__, __LINE__);
		return -EINVAL;
	} else {
		pinfo->mipi.dsi_phy_db.lanecfg_len = len;
		for (i = 0; i < len; i++)
			pinfo->mipi.dsi_phy_db.lanecfg[i] = data[i];
	}

	ctrl_pdata->timing_db_mode = of_property_read_bool(
		ctrl_pdev->dev.of_node, "qcom,timing-db-mode");

	ctrl_pdata->cmd_sync_wait_broadcast = of_property_read_bool(
		pan_node, "qcom,cmd-sync-wait-broadcast");

	if (ctrl_pdata->cmd_sync_wait_broadcast &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) &&
		(pinfo->pdest == DISPLAY_2))
		ctrl_pdata->cmd_sync_wait_trigger = true;

	pr_debug("%s: cmd_sync_wait_enable=%d trigger=%d\n", __func__,
				ctrl_pdata->cmd_sync_wait_broadcast,
				ctrl_pdata->cmd_sync_wait_trigger);

	mdss_dsi_parse_lane_swap(ctrl_pdev->dev.of_node, ctrl_pdata);

	pinfo->is_pluggable = of_property_read_bool(ctrl_pdev->dev.of_node,
		"qcom,pluggable");

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,display-id", &len);
	if (!data || len <= 0)
		pr_err("%s:%d Unable to read qcom,display-id, data=%pK,len=%d\n",
			__func__, __LINE__, data, len);
	else
		snprintf(ctrl_pdata->panel_data.panel_info.display_id,
			MDSS_DISPLAY_ID_MAX_LEN, "%s", data);

	return 0;


}

static int mdss_dsi_parse_gpio_params(struct platform_device *ctrl_pdev,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	/*
	 * If disp_en_gpio has been set previously (disp_en_gpio > 0)
	 *  while parsing the panel node, then do not override it
	 */
	struct mdss_panel_data *pdata = &ctrl_pdata->panel_data;

	if (ctrl_pdata->disp_en_gpio <= 0) {
		ctrl_pdata->disp_en_gpio = of_get_named_gpio(
			ctrl_pdev->dev.of_node,
			"qcom,platform-enable-gpio", 0);

		if (!gpio_is_valid(ctrl_pdata->disp_en_gpio))
			pr_debug("%s:%d, Disp_en gpio not specified\n",
					__func__, __LINE__);
	}

	ctrl_pdata->disp_te_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-te-gpio", 0);

	if (!gpio_is_valid(ctrl_pdata->disp_te_gpio))
		pr_err("%s:%d, TE gpio not specified\n",
						__func__, __LINE__);
	pdata->panel_te_gpio = ctrl_pdata->disp_te_gpio;

	ctrl_pdata->bklt_en_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-bklight-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		pr_info("%s: bklt_en gpio not specified\n", __func__);

	ctrl_pdata->bklt_en_gpio_invert =
			of_property_read_bool(ctrl_pdev->dev.of_node,
				"qcom,platform-bklight-en-gpio-invert");

	ctrl_pdata->avdd_en_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-avdd-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->avdd_en_gpio))
		pr_info("%s: avdd_en gpio not specified\n", __func__);

	ctrl_pdata->avdd_en_gpio_invert =
			of_property_read_bool(ctrl_pdev->dev.of_node,
				"qcom,platform-avdd-en-gpio-invert");

	ctrl_pdata->rst_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			 "qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio))
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);

	ctrl_pdata->lcd_mode_sel_gpio = of_get_named_gpio(
			ctrl_pdev->dev.of_node, "qcom,panel-mode-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->lcd_mode_sel_gpio)) {
		pr_debug("%s:%d mode gpio not specified\n", __func__, __LINE__);
		ctrl_pdata->lcd_mode_sel_gpio = -EINVAL;
	}

	return 0;
}

static void mdss_dsi_set_prim_panel(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mdss_dsi_ctrl_pdata *octrl = NULL;
	struct mdss_panel_info *pinfo;

	pinfo = &ctrl_pdata->panel_data.panel_info;

	/*
	 * for Split and Single DSI case default is always primary
	 * and for Dual dsi case below assumptions are made.
	 *	1. DSI controller with bridge chip is always secondary
	 *	2. When there is no brigde chip, DSI1 is secondary
	 */
	pinfo->is_prim_panel = true;
	if (mdss_dsi_is_hw_config_dual(ctrl_pdata->shared_data)) {
		if (mdss_dsi_is_right_ctrl(ctrl_pdata)) {
			octrl = mdss_dsi_get_other_ctrl(ctrl_pdata);
			if (octrl && octrl->panel_data.panel_info.is_prim_panel)
				pinfo->is_prim_panel = false;
			else
				pinfo->is_prim_panel = true;
		}
	}
}

int dsi_panel_device_register(struct platform_device *ctrl_pdev,
	struct device_node *pan_node, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mipi_panel_info *mipi;
	int rc;
	struct dsi_shared_data *sdata;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	struct resource *res;
	u64 clk_rate;

	mipi  = &(pinfo->mipi);

	pinfo->type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	pinfo->clk_rate = mdss_dsi_calc_bitclk(pinfo, mipi->frame_rate);
	if (!pinfo->clk_rate) {
		pr_err("%s: unable to calculate the DSI bit clock\n", __func__);
		return -EINVAL;
	}

	pinfo->mipi.dsi_pclk_rate = mdss_dsi_get_pclk_rate(pinfo,
		pinfo->clk_rate);
	if (!pinfo->mipi.dsi_pclk_rate) {
		pr_err("%s: unable to calculate the DSI pclk\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata->pclk_rate = mipi->dsi_pclk_rate;
	clk_rate = pinfo->clk_rate;
	do_div(clk_rate, 8U);
	ctrl_pdata->byte_clk_rate = (u32)clk_rate;
	pr_debug("%s: pclk=%d, bclk=%d\n", __func__,
			ctrl_pdata->pclk_rate, ctrl_pdata->byte_clk_rate);

	ctrl_pdata->esc_clk_rate_hz = pinfo->esc_clk_rate_hz;
	pr_debug("%s: esc clk=%d\n", __func__,
			ctrl_pdata->esc_clk_rate_hz);

	rc = mdss_dsi_get_dt_vreg_data(&ctrl_pdev->dev, pan_node,
		&ctrl_pdata->panel_power_data, DSI_PANEL_PM);
	if (rc) {
		DEV_ERR("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM), rc);
		return rc;
	}

	rc = msm_dss_config_vreg(&ctrl_pdev->dev,
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (rc) {
		pr_err("%s: failed to init regulator, rc=%d\n",
						__func__, rc);
		return rc;
	}

	rc = mdss_dsi_parse_ctrl_params(ctrl_pdev, pan_node, ctrl_pdata);
	if (rc) {
		pr_err("%s: failed to parse ctrl settings, rc=%d\n",
						__func__, rc);
		return rc;
	}

	/* default state of gpio is false */
	ctrl_pdata->bklt_en_gpio_state = false;

	pinfo->panel_max_fps = mdss_panel_get_framerate(pinfo);
	pinfo->panel_max_vtotal = mdss_panel_get_vtotal(pinfo);

	rc = mdss_dsi_parse_gpio_params(ctrl_pdev, ctrl_pdata);
	if (rc) {
		pr_err("%s: failed to parse gpio params, rc=%d\n",
						__func__, rc);
		return rc;
	}

	if (mdss_dsi_retrieve_ctrl_resources(ctrl_pdev,
					     pinfo->pdest,
					     ctrl_pdata)) {
		pr_err("%s: unable to get Dsi controller res\n", __func__);
		return -EPERM;
	}

	ctrl_pdata->panel_data.event_handler = mdss_dsi_event_handler;
	ctrl_pdata->panel_data.get_fb_node = mdss_dsi_get_fb_node_cb;

	if (ctrl_pdata->status_mode == ESD_REG ||
			ctrl_pdata->status_mode == ESD_REG_NT35596)
		ctrl_pdata->check_status = mdss_dsi_reg_status_check;
	else if (ctrl_pdata->status_mode == ESD_BTA)
		ctrl_pdata->check_status = mdss_dsi_bta_status_check;

	if (ctrl_pdata->status_mode == ESD_MAX) {
		pr_err("%s: Using default BTA for ESD check\n", __func__);
		ctrl_pdata->check_status = mdss_dsi_bta_status_check;
	}
	if (ctrl_pdata->bklt_ctrl == BL_PWM)
		mdss_dsi_panel_pwm_cfg(ctrl_pdata);

	mdss_dsi_ctrl_init(&ctrl_pdev->dev, ctrl_pdata);
	mdss_dsi_set_prim_panel(ctrl_pdata);

	ctrl_pdata->dsi_irq_line = of_property_read_bool(
				ctrl_pdev->dev.of_node, "qcom,dsi-irq-line");

	if (ctrl_pdata->dsi_irq_line) {
		/* DSI has it's own irq line */
		res = platform_get_resource(ctrl_pdev, IORESOURCE_IRQ, 0);
		if (!res || res->start == 0) {
			pr_err("%s:%d unable to get the MDSS irq resources\n",
							__func__, __LINE__);
			return -ENODEV;
		}
		rc = mdss_dsi_irq_init(&ctrl_pdev->dev, res->start, ctrl_pdata);
		if (rc) {
			dev_err(&ctrl_pdev->dev, "%s: failed to init irq\n",
							__func__);
			return rc;
		}
	}
	ctrl_pdata->ctrl_state = CTRL_STATE_UNKNOWN;

	/*
	 * If ULPS during suspend is enabled, add an extra vote for the
	 * DSI CTRL power module. This keeps the regulator always enabled.
	 * This is needed for the DSI PHY to maintain ULPS state during
	 * suspend also.
	 */
	sdata = ctrl_pdata->shared_data;

	if (pinfo->ulps_suspend_enabled) {
		rc = msm_dss_enable_vreg(
			sdata->power_data[DSI_PHY_PM].vreg_config,
			sdata->power_data[DSI_PHY_PM].num_vreg, 1);
		if (rc) {
			pr_err("%s: failed to enable vregs for DSI_CTRL_PM\n",
				__func__);
			return rc;
		}
	}

	pinfo->cont_splash_enabled =
		ctrl_pdata->mdss_util->panel_intf_status(pinfo->pdest,
		MDSS_PANEL_INTF_DSI) ? true : false;

	pr_info("%s: Continuous splash %s\n", __func__,
		pinfo->cont_splash_enabled ? "enabled" : "disabled");

	rc = mdss_register_panel(ctrl_pdev, &(ctrl_pdata->panel_data));
	if (rc) {
		pr_err("%s: unable to register MIPI DSI panel\n", __func__);
		return rc;
	}

	if (pinfo->pdest == DISPLAY_1) {
		mdss_debug_register_io("dsi0_ctrl", &ctrl_pdata->ctrl_io, NULL);
		mdss_debug_register_io("dsi0_phy", &ctrl_pdata->phy_io, NULL);
		if (ctrl_pdata->phy_regulator_io.len)
			mdss_debug_register_io("dsi0_phy_regulator",
				&ctrl_pdata->phy_regulator_io, NULL);
	} else {
		mdss_debug_register_io("dsi1_ctrl", &ctrl_pdata->ctrl_io, NULL);
		mdss_debug_register_io("dsi1_phy", &ctrl_pdata->phy_io, NULL);
		if (ctrl_pdata->phy_regulator_io.len)
			mdss_debug_register_io("dsi1_phy_regulator",
				&ctrl_pdata->phy_regulator_io, NULL);
	}

	panel_debug_register_base("panel",
		ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);

	pr_debug("%s: Panel data initialized\n", __func__);
	return 0;
}

static const struct of_device_id mdss_dsi_dt_match[] = {
	{.compatible = "qcom,mdss-dsi"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_dsi_dt_match);

static struct platform_driver mdss_dsi_driver = {
	.probe = mdss_dsi_probe,
	.remove = mdss_dsi_remove,
	.shutdown = NULL,
	.driver = {
		.name = "mdss_dsi",
		.of_match_table = mdss_dsi_dt_match,
	},
};

static struct platform_driver mdss_dsi_ctrl_driver = {
	.probe = mdss_dsi_ctrl_probe,
	.remove = mdss_dsi_ctrl_remove,
	.shutdown = NULL,
	.driver = {
		.name = "mdss_dsi_ctrl",
		.of_match_table = mdss_dsi_ctrl_dt_match,
	},
};

static int mdss_dsi_register_driver(void)
{
	return platform_driver_register(&mdss_dsi_driver);
}

static int __init mdss_dsi_driver_init(void)
{
	int ret;

	ret = mdss_dsi_register_driver();
	if (ret) {
		pr_err("mdss_dsi_register_driver() failed!\n");
		return ret;
	}

	return ret;
}
module_init(mdss_dsi_driver_init);


static int mdss_dsi_ctrl_register_driver(void)
{
	return platform_driver_register(&mdss_dsi_ctrl_driver);
}

static int __init mdss_dsi_ctrl_driver_init(void)
{
	int ret;

	ret = mdss_dsi_ctrl_register_driver();
	if (ret) {
		pr_err("mdss_dsi_ctrl_register_driver() failed!\n");
		return ret;
	}

	return ret;
}
module_init(mdss_dsi_ctrl_driver_init);

static void __exit mdss_dsi_driver_cleanup(void)
{
	platform_driver_unregister(&mdss_dsi_ctrl_driver);
}
module_exit(mdss_dsi_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSI controller driver");
