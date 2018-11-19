/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>

#include "mdss_panel.h"
#include "mdss_spi_panel.h"
#include "mdss_spi_client.h"

DEFINE_LED_TRIGGER(bl_led_trigger);
int mdss_spi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct spi_panel_data *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (WARN_ON(!pdata))
		return -EINVAL;

	ctrl_pdata = container_of(pdata, struct spi_panel_data,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return -EINVAL;
	}

	if (!gpio_is_valid(ctrl_pdata->disp_dc_gpio)) {
		pr_debug("%s:%d, dc line not configured\n",
			   __func__, __LINE__);
		return -EINVAL;
	}

	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
		if (rc) {
			pr_err("display reset gpio request failed\n");
			return rc;
		}

		rc = gpio_request(ctrl_pdata->disp_dc_gpio, "disp_dc");
		if (rc) {
			pr_err("display dc gpio request failed\n");
			if (gpio_is_valid(ctrl_pdata->rst_gpio))
				gpio_free(ctrl_pdata->rst_gpio);
			return rc;
		}

		if (!pinfo->cont_splash_enabled) {
			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_direction_output((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000,
					pinfo->rst_seq[i] * 1000);
			}
		}

		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
		}
	} else {
		gpio_direction_output((ctrl_pdata->rst_gpio), 0);
		gpio_free(ctrl_pdata->rst_gpio);

		gpio_direction_output(ctrl_pdata->disp_dc_gpio, 0);
		gpio_free(ctrl_pdata->disp_dc_gpio);
	}
	return 0;
}

int mdss_spi_panel_pinctrl_set_state(struct spi_panel_data *ctrl_pdata,
				bool active)
{
	struct pinctrl_state *pin_state;
	int rc = -EINVAL;

	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl))
		return -EINVAL;

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

static int mdss_spi_panel_pinctrl_init(struct platform_device *pdev)
{
	struct spi_panel_data *ctrl_pdata;

	ctrl_pdata = platform_get_drvdata(pdev);
	ctrl_pdata->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return -EINVAL;
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

static int mdss_spi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct spi_panel_data *ctrl_pdata = NULL;

	if (WARN_ON(!pdata))
		return -EINVAL;

	ctrl_pdata = container_of(pdata, struct spi_panel_data,
				panel_data);
	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: failed to enable vregs for PANEL_PM\n",
			__func__);
		return ret;
	}

	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled) {
		if (mdss_spi_panel_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_spi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

	return ret;
}

static int mdss_spi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct spi_panel_data *ctrl_pdata = NULL;

	if (WARN_ON(!pdata))
		return -EINVAL;

	ctrl_pdata = container_of(pdata, struct spi_panel_data,
				panel_data);

	ret = mdss_spi_panel_reset(pdata, 0);
	if (ret)
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);

	if (mdss_spi_panel_pinctrl_set_state(ctrl_pdata, false))
		pr_warn("reset disable: pinctrl not enabled\n");

	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for PANEL_PM\n",
			__func__);

	return ret;
}

int mdss_spi_panel_power_ctrl(struct mdss_panel_data *pdata, int power_state)
{
	int ret;
	struct mdss_panel_info *pinfo;

	if (WARN_ON(!pdata))
		return -EINVAL;

	pinfo = &pdata->panel_info;
	pr_debug("%s: cur_power_state=%d req_power_state=%d\n", __func__,
		pinfo->panel_power_state, power_state);

	if (pinfo->panel_power_state == power_state) {
		pr_debug("%s: no change needed\n", __func__);
		return 0;
	}

	switch (power_state) {
	case MDSS_PANEL_POWER_OFF:
		ret = mdss_spi_panel_power_off(pdata);
		break;
	case MDSS_PANEL_POWER_ON:
		ret = mdss_spi_panel_power_on(pdata);
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

void enable_spi_panel_te_irq(struct spi_panel_data *ctrl_pdata,
				bool enable)
{
	static int te_irq_count;

	if (!gpio_is_valid(ctrl_pdata->disp_te_gpio)) {
		pr_err("%s:%d,SPI panel TE GPIO not configured\n",
			   __func__, __LINE__);
		return;
	}

	mutex_lock(&ctrl_pdata->te_mutex);

	if (enable) {
		if (++te_irq_count == 1)
			enable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	} else {
		if (--te_irq_count == 0)
			disable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	}

	mutex_unlock(&ctrl_pdata->te_mutex);
}

void mdss_spi_tx_fb_complete(void *ctx)
{
	struct spi_panel_data *ctrl_pdata = ctx;

	if (atomic_add_unless(&ctrl_pdata->koff_cnt, -1, 0)) {
		if (atomic_read(&ctrl_pdata->koff_cnt)) {
			pr_err("%s: too many kickoffs=%d\n", __func__,
				atomic_read(&ctrl_pdata->koff_cnt));
		}
		wake_up_all(&ctrl_pdata->tx_done_waitq);
	}
}

static int mdss_spi_read_panel_data(struct mdss_panel_data *pdata,
		u8 reg_addr, u8 *data, u8 len)
{
	int rc = 0;
	struct spi_panel_data *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct spi_panel_data,
		panel_data);

	mutex_lock(&ctrl_pdata->spi_tx_mutex);
	gpio_direction_output(ctrl_pdata->disp_dc_gpio, 0);
	rc = mdss_spi_read_data(reg_addr, data, len);
	gpio_direction_output(ctrl_pdata->disp_dc_gpio, 1);
	mutex_unlock(&ctrl_pdata->spi_tx_mutex);

	return rc;
}

int mdss_spi_panel_on(struct mdss_panel_data *pdata)
{
	struct spi_panel_data *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	int i;

	if (WARN_ON(!pdata))
		return -EINVAL;

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct spi_panel_data,
				panel_data);

	for (i = 0; i < ctrl->on_cmds.cmd_cnt; i++) {
		/* pull down dc gpio indicate this is command */
		gpio_direction_output(ctrl->disp_dc_gpio, 0);
		mdss_spi_tx_command(ctrl->on_cmds.cmds[i].command);
		gpio_direction_output((ctrl->disp_dc_gpio), 1);

		if (ctrl->on_cmds.cmds[i].dchdr.dlen > 1) {
			mdss_spi_tx_parameter(ctrl->on_cmds.cmds[i].parameter,
					ctrl->on_cmds.cmds[i].dchdr.dlen - 1);
		}
		if (ctrl->on_cmds.cmds[i].dchdr.wait != 0)
			msleep(ctrl->on_cmds.cmds[i].dchdr.wait);
	}

	pr_debug("%s:-\n", __func__);
	return 0;
}

int mdss_spi_panel_off(struct mdss_panel_data *pdata)
{
	struct spi_panel_data *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	int i;

	if (WARN_ON(!pdata))
		return -EINVAL;

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct spi_panel_data,
				panel_data);

	for (i = 0; i < ctrl->off_cmds.cmd_cnt; i++) {
		/* pull down dc gpio indicate this is command */
		gpio_direction_output(ctrl->disp_dc_gpio, 0);
		mdss_spi_tx_command(ctrl->off_cmds.cmds[i].command);
		gpio_direction_output((ctrl->disp_dc_gpio), 1);

		if (ctrl->off_cmds.cmds[i].dchdr.dlen > 1) {
			mdss_spi_tx_parameter(ctrl->off_cmds.cmds[i].parameter,
					ctrl->off_cmds.cmds[i].dchdr.dlen-1);
		}

		if (ctrl->off_cmds.cmds[i].dchdr.wait != 0)
			msleep(ctrl->off_cmds.cmds[i].dchdr.wait);
	}

	pr_debug("%s:-\n", __func__);
	return 0;
}

static void mdss_spi_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
}

static int mdss_spi_get_panel_vreg_data(struct device *dev,
			struct dss_module_power *mp)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_node = NULL;
	struct device_node *supply_root_node = NULL;

	of_node = dev->of_node;

	mp->num_vreg = 0;

	supply_root_node = of_get_child_by_name(of_node,
				"qcom,panel-supply-entries");

	for_each_available_child_of_node(supply_root_node, supply_node)
		mp->num_vreg++;

	if (mp->num_vreg == 0) {
		pr_debug("%s: no vreg\n", __func__);
		goto novreg;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, mp->num_vreg);
	}

	mp->vreg_config = devm_kcalloc(dev, mp->num_vreg,
			sizeof(*(mp->vreg_config)), GFP_KERNEL);

	if (mp->vreg_config != NULL) {
		for_each_available_child_of_node(supply_root_node,
				supply_node) {
			const char *st = NULL;
			/* vreg-name */
			rc = of_property_read_string(supply_node,
				"qcom,supply-name", &st);
			if (rc) {
				pr_err("%s: error reading name. rc=%d\n",
					__func__, rc);
				goto error;
			}
			strlcpy(mp->vreg_config[i].vreg_name, st,
				sizeof(mp->vreg_config[i].vreg_name));

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
				pr_err("%s: error read enable load. rc=%d\n",
					__func__, rc);
				goto error;
			}
			mp->vreg_config[i].load[DSS_REG_MODE_ENABLE] = tmp;

			/* disable-load */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-disable-load", &tmp);
			if (rc) {
				pr_err("%s: error read disable load. rc=%d\n",
					__func__, rc);
				goto error;
			}
			mp->vreg_config[i].load[DSS_REG_MODE_DISABLE] = tmp;

			/* pre-sleep */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-pre-on-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error read pre on value\n",
						__func__);
				rc = 0;
			} else {
				mp->vreg_config[i].pre_on_sleep = tmp;
			}

			rc = of_property_read_u32(supply_node,
				"qcom,supply-pre-off-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error read pre off value\n",
						__func__);
				rc = 0;
			} else {
				mp->vreg_config[i].pre_off_sleep = tmp;
			}

			/* post-sleep */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-post-on-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error read post on value\n",
						__func__);
				rc = 0;
			} else {
				mp->vreg_config[i].post_on_sleep = tmp;
			}

			rc = of_property_read_u32(supply_node,
				"qcom,supply-post-off-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error read post off value\n",
						__func__);
				rc = 0;
			} else {
				mp->vreg_config[i].post_off_sleep = tmp;
			}

			++i;
		}
	}
	return rc;
error:
	kfree(mp->vreg_config);
	mp->vreg_config = NULL;

novreg:
	mp->num_vreg = 0;

	return rc;

}

static int mdss_spi_panel_parse_cmds(struct device_node *np,
		struct spi_panel_cmds *pcmds, char *cmd_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct spi_ctrl_hdr *dchdr;
	int i, cnt;
	struct platform_device *mdss_pdev;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOENT;
	}

	mdss_pdev = of_find_device_by_node(np->parent);
	if (!mdss_pdev) {
		pr_err("Unable to find mdss for node: %s\n", np->full_name);
		return -ENOENT;
	}

	buf = devm_kcalloc(&mdss_pdev->dev, blen, sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf = kmemdup(data, blen, GFP_KERNEL);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len >= sizeof(*dchdr)) {
		dchdr = (struct spi_ctrl_hdr *)bp;
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi parse error, len=%d\n",
				__func__, dchdr->dlen);
			goto exit_free;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error\n",
				__func__, buf[0], len);
		goto exit_free;
	}

	pcmds->cmds = devm_kcalloc(&mdss_pdev->dev, cnt, sizeof(*(pcmds->cmds)),
						GFP_KERNEL);
	if (!pcmds->cmds)
		goto exit_free;

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct spi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].command = bp;
		pcmds->cmds[i].parameter = bp + sizeof(char);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}

	pr_debug("%s: dcs_cmd=%x, len=%d, cmd_cnt=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt);
	return 0;

exit_free:
	kfree(buf);
	return -ENOMEM;
}
static int mdss_spi_panel_parse_reset_seq(struct device_node *np,
		u32 rst_seq[MDSS_SPI_RST_SEQ_LEN], u32 *rst_len,
		const char *name)
{
	int num = 0, i;
	int rc;
	struct property *data;
	u32 tmp[MDSS_SPI_RST_SEQ_LEN];

	*rst_len = 0;
	data = of_find_property(np, name, &num);
	num /= sizeof(u32);
	if (!data || !num || num > MDSS_SPI_RST_SEQ_LEN || num % 2) {
		pr_err("%s:%d, error reading %s, length found = %d\n",
			__func__, __LINE__, name, num);
		return -EINVAL;
	}
	rc = of_property_read_u32_array(np, name, tmp, num);
	if (rc) {
		pr_err("%s:%d, error reading %s, rc = %d\n",
			__func__, __LINE__, name, rc);
		return rc;
	}

	for (i = 0; i < num; ++i)
		rst_seq[i] = tmp[i];
	*rst_len = num;

	return 0;
}

static bool mdss_send_panel_cmd_for_esd(struct spi_panel_data *ctrl_pdata)
{
	if (WARN_ON(!ctrl_pdata))
		return false;

	mutex_lock(&ctrl_pdata->spi_tx_mutex);
	mdss_spi_panel_on(&ctrl_pdata->panel_data);
	mutex_unlock(&ctrl_pdata->spi_tx_mutex);

	return true;
}

static bool mdss_spi_reg_status_check(struct spi_panel_data *ctrl_pdata)
{
	int ret = 0;
	int i = 0;

	if (WARN_ON(!ctrl_pdata))
		return false;

	pr_debug("%s: Checking Register status\n", __func__);

	ret = mdss_spi_read_panel_data(&ctrl_pdata->panel_data,
					ctrl_pdata->panel_status_reg,
					ctrl_pdata->act_status_value,
					ctrl_pdata->status_cmds_rlen);
	if (ret < 0) {
		pr_err("%s: Read status register returned error\n", __func__);
		return false;
	}

	for (i = 0; i < ctrl_pdata->status_cmds_rlen; i++) {
		pr_debug("act_value[%d] = %x, exp_value[%d] = %x\n",
				i, ctrl_pdata->act_status_value[i],
				i, ctrl_pdata->exp_status_value[i]);
		if (ctrl_pdata->act_status_value[i] !=
				ctrl_pdata->exp_status_value[i])
			return false;
	}

	return true;
}

static void mdss_spi_parse_esd_params(struct device_node *np,
		struct spi_panel_data *ctrl)
{
	u32 tmp;
	int rc;
	struct property *data;
	const char *string;
	struct mdss_panel_info *pinfo = &ctrl->panel_data.panel_info;
	struct platform_device *mdss_pdev;

	mdss_pdev = of_find_device_by_node(np->parent);
	if (!mdss_pdev) {
		pr_err("Unable to find mdss for node: %s\n", np->full_name);
		return;
	}

	pinfo->esd_check_enabled = of_property_read_bool(np,
		"qcom,esd-check-enabled");

	if (!pinfo->esd_check_enabled)
		return;

	ctrl->status_mode = SPI_ESD_MAX;

	rc = of_property_read_string(np,
			"qcom,mdss-spi-panel-status-check-mode", &string);
	if (!rc) {
		if (!strcmp(string, "reg_read")) {
			ctrl->status_mode = SPI_ESD_REG;
			ctrl->check_status =
				mdss_spi_reg_status_check;
		} else if (!strcmp(string, "send_init_command")) {
			ctrl->status_mode = SPI_SEND_PANEL_COMMAND;
			ctrl->check_status =
				mdss_send_panel_cmd_for_esd;
				return;
		} else {
			pr_err("No valid panel-status-check-mode string\n");
			pinfo->esd_check_enabled = false;
			return;
		}
	}

	rc = of_property_read_u8(np, "qcom,mdss-spi-panel-status-reg",
				&ctrl->panel_status_reg);
	if (rc) {
		pr_warn("%s:%d, Read status reg failed, disable ESD check\n",
				__func__, __LINE__);
		pinfo->esd_check_enabled = false;
		return;
	}

	rc = of_property_read_u32(np, "qcom,mdss-spi-panel-status-read-length",
					&tmp);
	if (rc) {
		pr_warn("%s:%d, Read reg length failed, disable ESD check\n",
				__func__, __LINE__);
		pinfo->esd_check_enabled = false;
		return;
	}

	ctrl->status_cmds_rlen = (!rc ? tmp : 1);

	ctrl->exp_status_value = devm_kzalloc(&mdss_pdev->dev, sizeof(u8) *
				 (ctrl->status_cmds_rlen + 1), GFP_KERNEL);
	ctrl->act_status_value = devm_kzalloc(&mdss_pdev->dev, sizeof(u8) *
				(ctrl->status_cmds_rlen + 1), GFP_KERNEL);

	if (!ctrl->exp_status_value || !ctrl->act_status_value) {
		pinfo->esd_check_enabled = false;
		return;
	}

	data = of_find_property(np, "qcom,mdss-spi-panel-status-value", &tmp);
	tmp /= sizeof(u8);
	if (!data || (tmp != ctrl->status_cmds_rlen)) {
		pr_err("%s: Panel status values not found\n", __func__);
		pinfo->esd_check_enabled = false;
		memset(ctrl->exp_status_value, 0, ctrl->status_cmds_rlen);
	} else {
		rc = of_property_read_u8_array(np,
			"qcom,mdss-spi-panel-status-value",
			ctrl->exp_status_value, tmp);
		if (rc) {
			pr_err("%s: Error reading panel status values\n",
				__func__);
			pinfo->esd_check_enabled = false;
			kfree(ctrl->exp_status_value);
			kfree(ctrl->act_status_value);
		}
	}
}

static int mdss_spi_panel_parse_dt(struct device_node *np,
		struct spi_panel_data	*ctrl_pdata)
{
	u32 tmp;
	int rc;
	const char *data;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	pinfo->cont_splash_enabled = of_property_read_bool(np,
					"qcom,cont-splash-enabled");

	rc = of_property_read_u32(np, "qcom,mdss-spi-panel-width", &tmp);
	if (rc) {
		pr_err("%s: panel width not specified\n", __func__);
		return -EINVAL;
	}
	pinfo->xres = tmp;

	rc = of_property_read_u32(np, "qcom,mdss-spi-panel-height", &tmp);
	if (rc) {
		pr_err("%s:panel height not specified\n", __func__);
		return -EINVAL;
	}
	pinfo->yres = tmp;

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-width-dimension", &tmp);
	pinfo->physical_width = (!rc ? tmp : 0);
	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-height-dimension", &tmp);
	pinfo->physical_height = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-spi-panel-framerate", &tmp);
	pinfo->spi.frame_rate = (!rc ? tmp : 30);

	/*
	 * Due to SPI clock limit, frame rate of SPI display can olny reach to
	 * ~30fps with resolution is 240*240 and format is rgb565.
	 * VSYNC frequency should be match frame rate for avoid flicker issue.
	 * but some panels can't support that TE frequency lower than 30Hz, so
	 * we need to set double the TE frequency in this case.
	 */
	rc = of_property_read_u32(np, "qcom,mdss-spi-panel-te-per-vsync", &tmp);
	ctrl_pdata->vsync_per_te = (!rc ? tmp : 2);

	rc = of_property_read_u32(np, "qcom,mdss-spi-bpp", &tmp);
	if (rc) {
		pr_err("%s: bpp not specified\n", __func__);
		return -EINVAL;
	}
	pinfo->bpp = tmp;

	pinfo->pdest = DISPLAY_1;

	ctrl_pdata->bklt_ctrl = SPI_UNKNOWN_CTRL;
	data = of_get_property(np, "qcom,mdss-spi-bl-pmic-control-type", NULL);
	if (data) {
		if (!strcmp(data, "bl_ctrl_wled")) {
			led_trigger_register_simple("bkl-trigger",
				&bl_led_trigger);
			pr_debug("%s: SUCCESS-> WLED TRIGGER register\n",
				__func__);
			ctrl_pdata->bklt_ctrl = SPI_BL_WLED;
		} else if (!strcmp(data, "bl_ctrl_pwm")) {
			ctrl_pdata->bklt_ctrl = SPI_BL_PWM;
			ctrl_pdata->pwm_pmi = of_property_read_bool(np,
					"qcom,mdss-spi-bl-pwm-pmi");
			rc = of_property_read_u32(np,
				"qcom,mdss-spi-bl-pmic-pwm-frequency", &tmp);
			if (rc) {
				pr_err("%s: Error, panel pwm_period\n",
					 __func__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_period = tmp;
			if (ctrl_pdata->pwm_pmi) {
				ctrl_pdata->pwm_bl = of_pwm_get(np, NULL);
				if (IS_ERR(ctrl_pdata->pwm_bl)) {
					pr_err("%s: Error, pwm device\n",
							__func__);
					ctrl_pdata->pwm_bl = NULL;
					return -EINVAL;
				}
			} else {
				rc = of_property_read_u32(np,
					"qcom,mdss-spi-bl-pmic-bank-select",
								 &tmp);
				if (rc) {
					pr_err("%s: Error, lpg channel\n",
						__func__);
					return -EINVAL;
				}
				ctrl_pdata->pwm_lpg_chan = tmp;
				tmp = of_get_named_gpio(np,
					"qcom,mdss-spi-pwm-gpio", 0);
				ctrl_pdata->pwm_pmic_gpio = tmp;
				pr_debug("%s: Configured PWM bklt ctrl\n",
								 __func__);
			}
		}
	}
	rc = of_property_read_u32(np, "qcom,mdss-brightness-max-level", &tmp);
	pinfo->brightness_max = (!rc ? tmp : MDSS_MAX_BL_BRIGHTNESS);
	rc = of_property_read_u32(np, "qcom,mdss-spi-bl-min-level", &tmp);
	pinfo->bl_min = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-spi-bl-max-level", &tmp);
	pinfo->bl_max = (!rc ? tmp : 255);
	ctrl_pdata->bklt_max = pinfo->bl_max;


	mdss_spi_panel_parse_reset_seq(np, pinfo->rst_seq,
					&(pinfo->rst_seq_len),
					"qcom,mdss-spi-reset-sequence");

	mdss_spi_panel_parse_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-spi-on-command");

	mdss_spi_panel_parse_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-spi-off-command");

	mdss_spi_parse_esd_params(np, ctrl_pdata);

	return 0;
}

static void mdss_spi_panel_bklt_pwm(struct spi_panel_data *ctrl, int level)
{
	int ret;
	u32 duty;
	u32 period_ns;

	if (WARN_ON(!ctrl->pwm_bl))
		return;

	if (level == 0) {
		if (ctrl->pwm_enabled) {
			ret = pwm_config(ctrl->pwm_bl, level,
				ctrl->pwm_period * NSEC_PER_USEC);
			if (ret)
				pr_err("%s: pwm_config() failed err=%d.\n",
						__func__, ret);
			pwm_disable(ctrl->pwm_bl);
		}
		ctrl->pwm_enabled = 0;
		return;
	}

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	period_ns = ctrl->pwm_period * NSEC_PER_USEC;
	ret = pwm_config(ctrl->pwm_bl,
			level * period_ns / ctrl->bklt_max, period_ns);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d\n", __func__, ret);
		return;
	}

	if (!ctrl->pwm_enabled) {
		ret = pwm_enable(ctrl->pwm_bl);
		if (ret)
			pr_err("%s: pwm_enable() failed err=%d\n", __func__,
				ret);
		ctrl->pwm_enabled = 1;
	}
}

void mdss_spi_panel_bl_ctrl_update(struct mdss_panel_data *pdata,
				u32 bl_level)
{
	struct spi_panel_data *ctrl_pdata = NULL;

	if (WARN_ON(!pdata))
		return;

	ctrl_pdata = container_of(pdata, struct spi_panel_data,
				panel_data);

	if ((bl_level < pdata->panel_info.bl_min) && (bl_level != 0))
		bl_level = pdata->panel_info.bl_min;

	switch (ctrl_pdata->bklt_ctrl) {
	case SPI_BL_WLED:
		led_trigger_event(bl_led_trigger, bl_level);
		break;
	case SPI_BL_PWM:
		mdss_spi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration %d\n",
			__func__, ctrl_pdata->bklt_ctrl);
		break;
	}
}

static int mdss_spi_panel_init(struct device_node *node,
	struct spi_panel_data *ctrl_pdata)
{
	int rc = 0;
	static const char *panel_name;
	struct mdss_panel_info *pinfo;

	pinfo = &ctrl_pdata->panel_data.panel_info;

	pr_debug("%s:%d\n", __func__, __LINE__);
	pinfo->panel_name[0] = '\0';
	panel_name = of_get_property(node, "qcom,mdss-spi-panel-name", NULL);
	if (!panel_name) {
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	} else {
		pr_debug("%s: Panel Name = %s\n", __func__, panel_name);
		strlcpy(&pinfo->panel_name[0], panel_name, MDSS_MAX_PANEL_LEN);
	}
	rc = mdss_spi_panel_parse_dt(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt parse failed\n", __func__, __LINE__);
		return rc;
	}

	ctrl_pdata->panel_data.panel_info.is_prim_panel = true;
	ctrl_pdata->byte_per_frame = pinfo->xres * pinfo->yres * pinfo->bpp/8;

	ctrl_pdata->front_buf = kzalloc(ctrl_pdata->byte_per_frame, GFP_KERNEL);
	ctrl_pdata->back_buf = kzalloc(ctrl_pdata->byte_per_frame, GFP_KERNEL);

	pinfo->cont_splash_enabled = false;

	pr_info("%s: Continuous splash %s\n", __func__,
		pinfo->cont_splash_enabled ? "enabled" : "disabled");

	pinfo->dynamic_switch_pending = false;
	pinfo->is_lpm_mode = false;
	pinfo->esd_rdy = false;

	ctrl_pdata->panel_data.set_backlight = mdss_spi_panel_bl_ctrl_update;

	return 0;
}

static void mdss_spi_display_handle_vsync(struct spi_panel_data *ctrl_pdata,
					ktime_t t)
{
	ctrl_pdata->vsync_time = t;
	sysfs_notify_dirent(ctrl_pdata->vsync_event_sd);
}

static int mdss_spi_panel_regulator_init(struct platform_device *pdev)
{
	int rc = 0;

	struct spi_panel_data *ctrl_pdata = NULL;

	ctrl_pdata = platform_get_drvdata(pdev);
	if (WARN_ON(!ctrl_pdata))
		return -EINVAL;

	rc = msm_dss_config_vreg(&pdev->dev,
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (rc)
		pr_err("%s: failed to init vregs for PANEL_PM\n",
			__func__);

	return rc;
}

static irqreturn_t spi_panel_te_handler(int irq, void *data)
{
	struct spi_panel_data *ctrl_pdata = (struct spi_panel_data *)data;
	ktime_t vsync_time;
	static int te_count;
	complete(&ctrl_pdata->spi_panel_te);

	if (ctrl_pdata->vsync_enable && (++te_count ==
				ctrl_pdata->vsync_per_te)) {
		vsync_time = ktime_get();
		mdss_spi_display_handle_vsync(ctrl_pdata, vsync_time);
		te_count = 0;
	}

	return IRQ_HANDLED;
}

void mdss_spi_vsync_enable(struct mdss_panel_data *pdata, int enable)
{
	struct spi_panel_data *ctrl_pdata = NULL;

	if (WARN_ON(!pdata))
		return;

	ctrl_pdata = container_of(pdata, struct spi_panel_data,
				panel_data);
	if (enable) {
		if (ctrl_pdata->vsync_enable == false) {
			enable_spi_panel_te_irq(ctrl_pdata, true);
			ctrl_pdata->vsync_enable = true;
		}
	} else {
		if (ctrl_pdata->vsync_enable == true) {
			enable_spi_panel_te_irq(ctrl_pdata, false);
			ctrl_pdata->vsync_enable = false;
		}
	}
}

static struct device_node *mdss_spi_pref_prim_panel(
		struct platform_device *pdev)
{
	struct device_node *spi_pan_node = NULL;

	pr_debug("%s:%d: Select primary panel from dt\n",
					__func__, __LINE__);
	spi_pan_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,spi-pref-prim-pan", 0);
	if (!spi_pan_node)
		pr_err("%s:can't find panel phandle\n", __func__);

	return spi_pan_node;
}

static struct device_node *mdss_spi_get_fb_node_cb(struct platform_device *pdev)
{
	struct device_node *fb_node;
	struct platform_device *spi_dev;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	if (WARN_ON(!pdev))
		return NULL;

	ctrl_pdata = platform_get_drvdata(pdev);
	spi_dev = of_find_device_by_node(pdev->dev.of_node);
	if (!spi_dev) {
		pr_err("Unable to find dsi master device: %s\n",
			pdev->dev.of_node->full_name);
		return NULL;
	}

	fb_node = of_parse_phandle(spi_dev->dev.of_node,
				"qcom,mdss-fb-map-prim", 0);
	if (!fb_node) {
		pr_err("Unable to find fb node for device: %s\n", pdev->name);
		return NULL;
	}

	return fb_node;
}

static int spi_panel_device_register(struct device_node *pan_node,
				struct spi_panel_data *ctrl_pdata)
{
	int rc;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	struct device_node *spi_ctrl_np = NULL;
	struct platform_device *ctrl_pdev = NULL;

	pinfo->type = SPI_PANEL;

	spi_ctrl_np = of_parse_phandle(pan_node,
				"qcom,mdss-spi-panel-controller", 0);
	if (!spi_ctrl_np) {
		pr_err("%s: SPI controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	ctrl_pdev = of_find_device_by_node(spi_ctrl_np);
	if (!ctrl_pdev) {
		of_node_put(spi_ctrl_np);
		pr_err("%s: SPI controller node not find\n", __func__);
		return -EPROBE_DEFER;
	}

	rc = mdss_spi_panel_regulator_init(ctrl_pdev);
	if (rc) {
		pr_err("%s: failed to init regulator, rc=%d\n",
						__func__, rc);
		return rc;
	}

	pinfo->panel_max_fps = mdss_panel_get_framerate(pinfo,
		FPS_RESOLUTION_HZ);
	pinfo->panel_max_vtotal = mdss_panel_get_vtotal(pinfo);

	ctrl_pdata->disp_te_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-te-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_te_gpio))
		pr_err("%s:%d, TE gpio not specified\n",
						__func__, __LINE__);

	ctrl_pdata->disp_dc_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-spi-dc-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dc_gpio))
		pr_err("%s:%d, SPI DC gpio not specified\n",
						__func__, __LINE__);

	ctrl_pdata->rst_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			 "qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio))
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);

	if (ctrl_pdata->bklt_ctrl == SPI_BL_PWM) {
		if (ctrl_pdata->pwm_pmi)
			return -EINVAL;

		ctrl_pdata->pwm_bl = devm_of_pwm_get(&ctrl_pdev->dev,
					ctrl_pdev->dev.of_node, NULL);
		if (IS_ERR_OR_NULL(ctrl_pdata->pwm_bl))
			pr_err("%s: Error: devm_of_pwm_get failed",
				__func__);
		ctrl_pdata->pwm_enabled = 0;
	}

	ctrl_pdata->ctrl_state = CTRL_STATE_UNKNOWN;
	ctrl_pdata->panel_data.get_fb_node = mdss_spi_get_fb_node_cb;
	ctrl_pdata->panel_data.get_fb_node = NULL;
	if (pinfo->cont_splash_enabled) {
		rc = mdss_spi_panel_power_ctrl(&(ctrl_pdata->panel_data),
			MDSS_PANEL_POWER_ON);
		if (rc) {
			pr_err("%s: Panel power on failed\n", __func__);
			return rc;
		}
		if (ctrl_pdata->bklt_ctrl == SPI_BL_PWM)
			ctrl_pdata->pwm_enabled = 1;
		ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_INIT;
	} else {
		pinfo->panel_power_state = MDSS_PANEL_POWER_OFF;
	}

	rc = mdss_register_panel(ctrl_pdev, &(ctrl_pdata->panel_data));
	if (rc) {
		pr_err("%s: unable to register SPI panel\n", __func__);
		return rc;
	}

	pr_debug("%s: Panel data initialized\n", __func__);
	return 0;
}

int mdss_spi_wait_tx_done(struct spi_panel_data *ctrl_pdata)
{
	int rc = 0;

	rc = wait_event_timeout(ctrl_pdata->tx_done_waitq,
			atomic_read(&ctrl_pdata->koff_cnt) == 0,
			KOFF_TIMEOUT);

	return rc;
}


static int mdss_spi_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct spi_panel_data	*ctrl_pdata;
	struct device_node *spi_pan_node = NULL;
	char panel_cfg[MDSS_MAX_PANEL_LEN];
	const char *ctrl_name;

	if (!pdev->dev.of_node) {
		pr_err("SPI driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		ctrl_pdata = devm_kzalloc(&pdev->dev,
					  sizeof(struct spi_panel_data),
					  GFP_KERNEL);
		if (!ctrl_pdata) {
			pr_err("%s: FAILED: cannot alloc spi panel\n",
			       __func__);
			return -ENOMEM;
		}
		platform_set_drvdata(pdev, ctrl_pdata);
	}

	ctrl_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!ctrl_name)
		pr_info("%s:%d, Ctrl name not specified\n",
			__func__, __LINE__);
	else
		pr_debug("%s: Ctrl name = %s\n",
			__func__, ctrl_name);


	rc = of_platform_populate(pdev->dev.of_node,
				  NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
		goto error_no_mem;
	}

	rc = mdss_spi_panel_pinctrl_init(pdev);
	if (rc)
		pr_warn("%s: failed to get pin resources\n", __func__);

	rc = mdss_spi_get_panel_vreg_data(&pdev->dev,
					&ctrl_pdata->panel_power_data);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: failed to get panel vreg data, rc=%d\n",
			__func__, rc);
		goto error_vreg;
	}

	/* find panel device node */
	spi_pan_node = mdss_spi_pref_prim_panel(pdev);
	if (!spi_pan_node) {
		pr_err("%s: can't find panel node %s\n", __func__, panel_cfg);
		goto error_pan_node;
	}

	rc = mdss_spi_panel_init(spi_pan_node, ctrl_pdata);
	if (rc) {
		pr_err("%s: spi panel init failed\n", __func__);
		goto error_pan_node;
	}

	rc = spi_panel_device_register(spi_pan_node, ctrl_pdata);
	if (rc) {
		pr_err("%s: spi panel dev reg failed\n", __func__);
		goto error_pan_node;
	}

	init_completion(&ctrl_pdata->spi_panel_te);
	mutex_init(&ctrl_pdata->spi_tx_mutex);
	mutex_init(&ctrl_pdata->te_mutex);
	init_waitqueue_head(&ctrl_pdata->tx_done_waitq);

	rc = devm_request_irq(&pdev->dev,
		gpio_to_irq(ctrl_pdata->disp_te_gpio),
		spi_panel_te_handler, IRQF_TRIGGER_RISING,
		"TE_GPIO", ctrl_pdata);
	if (rc) {
		pr_err("TE request_irq failed.\n");
		goto error_te_request;
	}
	disable_irq_nosync(gpio_to_irq(ctrl_pdata->disp_te_gpio));

	pr_debug("%s: spi panel  initialized\n", __func__);
	return 0;

error_te_request:
	mutex_destroy(&ctrl_pdata->spi_tx_mutex);
	mutex_destroy(&ctrl_pdata->te_mutex);
error_pan_node:
	of_node_put(spi_pan_node);
error_vreg:
	mdss_spi_put_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->panel_power_data);
error_no_mem:
	devm_kfree(&pdev->dev, ctrl_pdata);
	return rc;
}

static const struct of_device_id mdss_spi_panel_match[] = {
	{ .compatible = "qcom,mdss-spi-panel" },
	{},
};

static struct platform_driver this_driver = {
	.probe = mdss_spi_panel_probe,
	.driver = {
		.name = "spi_panel",
		.owner  = THIS_MODULE,
		.of_match_table = mdss_spi_panel_match,
	},
};

static int __init mdss_spi_display_panel_init(void)
{
	int ret;

	ret = platform_driver_register(&this_driver);
	return ret;
}
module_init(mdss_spi_display_panel_init);

MODULE_DEVICE_TABLE(of, mdss_spi_panel_match);
MODULE_LICENSE("GPL v2");
