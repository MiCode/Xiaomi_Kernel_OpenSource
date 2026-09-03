// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Youxu.Zeng <Youxu.Zeng@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/power/sprd_fchg_extcon.h>

#if IS_ENABLED(CONFIG_FAST_CHARGER_SC27XX)
static int sprd_get_sfcp_fixed_current_max(struct sprd_fchg_info *info, u32 *max_cur)
{
	union power_supply_propval val;
	struct power_supply *psy_sfcp;
	int ret;

	psy_sfcp = power_supply_get_by_name(SPRD_FCHG_SFCP_NAME);
	if (!psy_sfcp) {
		dev_err(info->dev, "%s, psy_sfcp is NULL\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy_sfcp, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (ret) {
		dev_err(info->dev, "%s, failed to get current property, ret=%d\n", __func__, ret);
		return ret;
	}

	*max_cur = val.intval;
	return ret;
}

static int sprd_get_sfcp_fixed_voltage_max(struct sprd_fchg_info *info, u32 *max_vol)
{
	union power_supply_propval val;
	struct power_supply *psy_sfcp;
	int ret;

	psy_sfcp = power_supply_get_by_name(SPRD_FCHG_SFCP_NAME);
	if (!psy_sfcp) {
		dev_err(info->dev, "%s, psy_sfcp is NULL\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy_sfcp, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (ret) {
		dev_err(info->dev, "%s, failed to get vol property, ret=%d\n", __func__, ret);
		return ret;
	}

	*max_vol = val.intval;
	return ret;
}

static int sprd_sfcp_fixed_adjust_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	union power_supply_propval val;
	struct power_supply *psy_sfcp;
	int ret;

	psy_sfcp = power_supply_get_by_name(SPRD_FCHG_SFCP_NAME);
	if (!psy_sfcp) {
		dev_err(info->dev, "%s, psy_sfcp is NULL\n", __func__);
		return -ENODEV;
	}

	val.intval = input_vol;
	ret = power_supply_set_property(psy_sfcp, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (ret)
		dev_err(info->dev, "%s, failed to get vol property, ret=%d\n", __func__, ret);

	return ret;
}

static int sprd_enable_sfcp(struct sprd_fchg_info *info, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy_sfcp;
	static bool sfcp_active;
	int ret;

	if (!info->support_sfcp)
		return 0;

	psy_sfcp = power_supply_get_by_name(SPRD_FCHG_SFCP_NAME);
	if (!psy_sfcp) {
		dev_err(info->dev, "%s, psy_sfcp is NULL\n", __func__);
		return -ENODEV;
	}

	if (enable && !info->sfcp_enable) {
		val.intval = true;
		ret = power_supply_set_property(psy_sfcp, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to enable sfcp, ret=%d\n", __func__, ret);
			return ret;
		}

		sfcp_active = true;
	} else if (!enable && sfcp_active) {
		val.intval = false;
		ret = power_supply_set_property(psy_sfcp, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to disable sfcp, ret=%d\n", __func__, ret);
			return ret;
		}

		sfcp_active = false;
		info->sfcp_enable = false;
	}

	return 0;
}
#else
static int sprd_get_sfcp_fixed_current_max(struct sprd_fchg_info *info, u32 *max_cur)
{
	return 0;
}

static int sprd_get_sfcp_fixed_voltage_max(struct sprd_fchg_info *info, u32 *max_vol)
{
	return 0;
}

static int sprd_sfcp_fixed_adjust_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	return 0;
}

static int sprd_enable_sfcp(struct sprd_fchg_info *info, bool enable)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SPRD_TYPEC_TCPM)
static int sprd_get_pd_adapter_voltage_max(struct sprd_fchg_info *info, u32 *max_vol)
{
	struct power_supply *psy_tcpm;
	struct sprd_tcpm_port *port;
	enum sprd_pd_pdo_type pd_pdo_type = SPRD_PDO_TYPE_FIXED;
	int i, adptor_max_vbus_mv = 0;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	port = power_supply_get_drvdata(psy_tcpm);
	if (!port) {
		dev_err(info->dev, "%s, failed to get tcpm port\n", __func__);
		return -EINVAL;
	}

	sprd_tcpm_get_source_capabilities(port, &info->pd_source_cap);
	if (!info->pd_source_cap.nr_source_caps) {
		dev_err(info->dev, "%s, failed to obtain the PD power supply capacity\n", __func__);
		return -EINVAL;
	}

	if (info->pps_enable && !info->pps_active)
		pd_pdo_type = SPRD_PDO_TYPE_APDO;

	/*
	 * In the 10.2.2 Normative Voltages and Current section of the
	 * pd standard protocol requires that the power of non-5V fixed
	 * or dynamic voltage ranges is at least 15W. For detailed
	 * requirements, please refer to Table 10-7 Programmable Power
	 * Supply PDOs and APDOs based on the PDP.
	 */
	for (i = 0; i < info->pd_source_cap.nr_source_caps; i++) {
		if (info->pd_source_cap.type[i] == pd_pdo_type &&
		    adptor_max_vbus_mv < info->pd_source_cap.max_mv[i] &&
		    (info->pd_source_cap.max_mv[i] * 1000 <= SPRD_FCHG_5V_FIXED_PROG_MAX ||
		     (info->pd_source_cap.max_mv[i] * 1000 > SPRD_FCHG_5V_FIXED_PROG_MAX &&
		      info->pd_source_cap.max_mv[i] * info->pd_source_cap.ma[i] >
		      SPRD_FCHG_NON_5V_FIXED_PROG_MIN_UW)))
			adptor_max_vbus_mv = info->pd_source_cap.max_mv[i];
	}

	*max_vol = adptor_max_vbus_mv * 1000;

	return 0;
}

static int sprd_get_pd_adapter_current(struct sprd_fchg_info *info,
				       u32 vbus_max_vol,
				       u32 *adapter_cur)
{
	struct power_supply *psy_tcpm;
	struct sprd_tcpm_port *port;
	enum sprd_pd_pdo_type pd_pdo_type = SPRD_PDO_TYPE_FIXED;
	int i, ret = 0, adapter_ibus = 0, max_mw = 0, src_mw = 0;

	vbus_max_vol = vbus_max_vol / 1000;
	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		ret = -ENODEV;
		goto done;
	}

	port = power_supply_get_drvdata(psy_tcpm);
	if (!port) {
		dev_err(info->dev, "%s, failed to get tcpm port\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	sprd_tcpm_get_source_capabilities(port, &info->pd_source_cap);
	if (!info->pd_source_cap.nr_source_caps) {
		dev_err(info->dev, "%s, failed to obtain the PD power supply capacity\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (info->pps_enable && !info->pps_active)
		pd_pdo_type = SPRD_PDO_TYPE_APDO;

	for (i = 0; i < info->pd_source_cap.nr_source_caps; i++) {
		if (info->pd_source_cap.type[i] == pd_pdo_type) {
			if (vbus_max_vol >= info->pd_source_cap.min_mv[i] &&
			    vbus_max_vol <= info->pd_source_cap.max_mv[i]) {
				src_mw = info->pd_source_cap.max_mv[i] / 1000 *
					 info->pd_source_cap.ma[i];
				if (src_mw >= max_mw) {
					max_mw = src_mw;
					adapter_ibus = info->pd_source_cap.ma[i];
				}
			}
		}
	}

done:
	*adapter_cur = adapter_ibus * 1000;

	return ret;
}

static int sprd_get_pps_voltage_max(struct sprd_fchg_info *info, u32 *max_vol)
{
	union power_supply_propval val;
	struct power_supply *psy_tcpm;
	int ret;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy_tcpm,
					POWER_SUPPLY_PROP_VOLTAGE_MAX,
					&val);
	if (ret) {
		dev_err(info->dev, "%s, failed to get online property, ret=%d\n", __func__, ret);
		return ret;
	}

	*max_vol = val.intval;

	return ret;
}

static int sprd_get_pps_current_max(struct sprd_fchg_info *info, u32 *max_cur)
{
	union power_supply_propval val;
	struct power_supply *psy_tcpm;
	int ret;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy_tcpm, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (ret) {
		dev_err(info->dev, "%s, failed to get pps current max, ret=%d\n", __func__, ret);
		return ret;
	}

	*max_cur = val.intval;

	return ret;
}

static int sprd_pd_fixed_adjust_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	struct sprd_tcpm_port *port;
	struct power_supply *psy_tcpm;
	int ret, i, index = -1, last_select_max_mv = 0;
	u32 pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int snk_uw;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	port = power_supply_get_drvdata(psy_tcpm);
	if (!port) {
		dev_err(info->dev, "%s, failed to get tcpm port\n", __func__);
		return -EINVAL;
	}

	sprd_tcpm_get_source_capabilities(port, &info->pd_source_cap);
	if (!info->pd_source_cap.nr_source_caps) {
		pdo[0] = SPRD_PDO_FIXED(5000, 2000, 0);
		snk_uw = SPRD_PD_DEFAULT_POWER_UW;
		index = 0;
		goto done;
	}

	for (i = 0; i < info->pd_source_cap.nr_source_caps; i++) {
		if (info->pd_source_cap.max_mv[i] <= input_vol / 1000 &&
		    info->pd_source_cap.type[i] == SPRD_PDO_TYPE_FIXED &&
		    last_select_max_mv < info->pd_source_cap.max_mv[i]) {
			last_select_max_mv = info->pd_source_cap.max_mv[i];
			index = i;
		}
	}

	/*
	 * Ensure that index is within a valid range to prevent arrays
	 * from crossing bounds.
	 */
	if (index < 0 || index >= info->pd_source_cap.nr_source_caps) {
		dev_err(info->dev, "%s, Index is invalid!!!\n", __func__);
		return -EINVAL;
	}

	snk_uw = info->pd_source_cap.max_mv[index] * info->pd_source_cap.ma[index];
	if (snk_uw > info->pd_fixed_max_uw)
		snk_uw = info->pd_fixed_max_uw;

	for (i = 0; i < index + 1; i++) {
		pdo[i] = SPRD_PDO_FIXED(info->pd_source_cap.max_mv[i],
					info->pd_source_cap.ma[i],
					0);
		if (info->pd_source_cap.max_mv[i] * info->pd_source_cap.ma[i] > snk_uw)
			pdo[i] = SPRD_PDO_FIXED(info->pd_source_cap.max_mv[i],
						snk_uw / info->pd_source_cap.max_mv[i],
						0);
	}

done:
	ret = sprd_tcpm_update_sink_capabilities(port, pdo, index + 1, snk_uw / 1000);
	if (ret) {
		dev_err(info->dev, "%s, failed to set pd, ret = %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int sprd_pd_update_source_capabilities(struct sprd_fchg_info *info,
					      const u32 *pdo,
					      unsigned int nr_pdo)
{
	struct sprd_tcpm_port *port;
	struct power_supply *psy_tcpm;
	int ret;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	port = power_supply_get_drvdata(psy_tcpm);
	if (!port) {
		dev_err(info->dev, "%s, failed to get tcpm port\n", __func__);
		return -EINVAL;
	}

	ret = sprd_tcpm_update_ext_source_capabilities(port, pdo, nr_pdo);
	if (ret) {
		dev_err(info->dev, "%s, failed to update src cap, ret = %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int sprd_pps_adjust_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	union power_supply_propval val;
	struct power_supply *psy_tcpm;
	int ret;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	if (!info->pps_active) {
		val.intval = SPRD_ENABLE_PPS;
		ret = power_supply_set_property(psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to set online property ret = %d\n",
				__func__, ret);
			return ret;
		}
		info->pps_active = true;
	}

	val.intval = input_vol;
	ret = power_supply_set_property(psy_tcpm, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (ret)
		dev_err(info->dev, "%s, failed to set vol property, ret=%d\n", __func__, ret);

	return ret;
}

static int sprd_pps_adjust_current(struct sprd_fchg_info *info, u32 input_current)
{
	union power_supply_propval val;
	struct power_supply *psy_tcpm;
	int ret;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	if (!info->pps_active) {
		val.intval = SPRD_ENABLE_PPS;
		ret = power_supply_set_property(psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to set online property, ret=%d\n",
				__func__, ret);
			return ret;
		}
		info->pps_active = true;
	}

	val.intval = input_current;
	ret = power_supply_set_property(psy_tcpm, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (ret)
		dev_err(info->dev, "%s, failed to set current property, ret=%d\n",
			__func__, ret);

	return ret;
}

static int sprd_enable_pps(struct sprd_fchg_info *info, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy_tcpm;
	int ret;

	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		return -ENODEV;
	}

	if (info->pps_active && !enable) {
		val.intval = SPRD_DISABLE_PPS;
		ret = power_supply_set_property(psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to disbale pps, ret = %d\n", __func__, ret);
			return ret;
		}
		info->pps_active = false;
	} else if (!info->pps_active && enable) {
		val.intval = SPRD_ENABLE_PPS;
		ret = power_supply_set_property(psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to enable pps, ret = %d\n", __func__, ret);
			return ret;
		}
		info->pps_active = true;
	}

	return 0;
}

static void sprd_detect_pd_online_work(struct work_struct *data)
{
	struct sprd_fchg_info *info = container_of(data, struct sprd_fchg_info, pd_online_work);
	union power_supply_propval val;
	struct power_supply *psy_tcpm;
	int ret = 0;

	mutex_lock(&info->lock);
	psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy_tcpm) {
		dev_err(info->dev, "%s, psy_tcpm is NULL\n", __func__);
		goto out;
	}

	ret = power_supply_get_property(psy_tcpm, POWER_SUPPLY_PROP_USB_TYPE, &val);
	if (ret) {
		dev_err(info->dev, "%s, failed to get usb type property, ret=%d\n",
			__func__, ret);
		goto out;
	}

	if (val.intval == POWER_SUPPLY_USB_TYPE_PD ||
	    (!info->support_pd_pps && val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS)) {
		info->pd_enable = true;
		info->pps_enable = false;
		info->pps_active = false;
		info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		mutex_unlock(&info->lock);
		cm_notify_event(info->psy, CM_EVENT_FAST_CHARGE, NULL);
		goto out1;
	} else if (val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS) {
		info->pps_enable = true;
		info->pd_enable = false;
		info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE;
		mutex_unlock(&info->lock);
		cm_notify_event(info->psy, CM_EVENT_FAST_CHARGE, NULL);
		goto out1;
	}

out:
	mutex_unlock(&info->lock);

out1:
	dev_info(info->dev, "%s, fchg type = %d\n", __func__, info->fchg_type);
}
#else
static int sprd_get_pd_adapter_voltage_max(struct sprd_fchg_info *info, u32 *max_vol)
{
	return 0;
}

static int sprd_get_pd_adapter_current(struct sprd_fchg_info *info,
				       u32 vbus_max_vol,
				       u32 *adapter_cur)
{
	return 0;
}

static int sprd_get_pps_voltage_max(struct sprd_fchg_info *info, u32 *max_vol)
{
	return 0;
}

static int sprd_get_pps_current_max(struct sprd_fchg_info *info, u32 *max_cur)
{
	return 0;
}

static int sprd_pd_fixed_adjust_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	return 0;
}

static int sprd_pd_update_source_capabilities(struct sprd_fchg_info *info,
					      const u32 *pdo,
					      unsigned int nr_pdo)
{
	return 0;
}

static int sprd_pps_adjust_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	return 0;
}

static int sprd_pps_adjust_current(struct sprd_fchg_info *info, u32 input_current)
{
	return 0;
}

static int sprd_enable_pps(struct sprd_fchg_info *info, bool enable)
{
	return 0;
}

static void sprd_detect_pd_online_work(struct work_struct *data)
{

}
#endif

static int sprd_get_customized_fchg_current_max(struct sprd_fchg_info *info, u32 *max_cur)
{
	union power_supply_propval val;
	struct power_supply *psy_customized_fchg;
	int ret;

	if (!info->customized_fchg_psy)
		return 0;

	psy_customized_fchg = power_supply_get_by_name(info->customized_fchg_psy);
	if (!psy_customized_fchg) {
		dev_err(info->dev, "%s, psy_customized_fchg is NULL\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy_customized_fchg, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (ret) {
		dev_err(info->dev, "%s, failed to get current property, ret=%d\n", __func__, ret);
		return ret;
	}

	*max_cur = val.intval;
	return ret;
}

static int sprd_get_customized_fchg_voltage_max(struct sprd_fchg_info *info, u32 *max_vol)
{
	union power_supply_propval val;
	struct power_supply *psy_customized_fchg;
	int ret;

	if (!info->customized_fchg_psy)
		return 0;

	psy_customized_fchg = power_supply_get_by_name(info->customized_fchg_psy);
	if (!psy_customized_fchg) {
		dev_err(info->dev, "%s, psy_customized_fchg is NULL\n", __func__);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy_customized_fchg, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (ret) {
		dev_err(info->dev, "%s, failed to get vol property, ret=%d\n", __func__, ret);
		return ret;
	}

	*max_vol = val.intval;
	return ret;
}

static int sprd_customized_fchg_adjust_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	union power_supply_propval val;
	struct power_supply *psy_customized_fchg;
	int ret;

	if (!info->customized_fchg_psy)
		return 0;

	psy_customized_fchg = power_supply_get_by_name(info->customized_fchg_psy);
	if (!psy_customized_fchg) {
		dev_err(info->dev, "%s, psy_customized_fchg is NULL\n", __func__);
		return -ENODEV;
	}

	val.intval = input_vol;
	ret = power_supply_set_property(psy_customized_fchg, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (ret)
		dev_err(info->dev, "%s, failed to get vol property, ret=%d\n", __func__, ret);

	return ret;
}

static int sprd_enable_customized_fchg(struct sprd_fchg_info *info, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy_customized_fchg;
	static bool customized_fchg_active;
	int ret;

	if (!info->customized_fchg_psy)
		return 0;

	psy_customized_fchg = power_supply_get_by_name(info->customized_fchg_psy);
	if (!psy_customized_fchg) {
		dev_err(info->dev, "%s, psy_customized_fchg is NULL\n", __func__);
		return -ENODEV;
	}

	if (enable && !info->customized_fchg_enable) {
		val.intval = true;
		ret = power_supply_set_property(psy_customized_fchg, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to enable customized fchg, ret=%d\n",
				__func__, ret);
			return ret;
		}

		customized_fchg_active = true;
	} else if (!enable && customized_fchg_active) {
		val.intval = false;
		ret = power_supply_set_property(psy_customized_fchg, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "%s, failed to disable customized fchg, ret=%d\n",
				__func__, ret);
			return ret;
		}

		customized_fchg_active = false;
		info->customized_fchg_enable = false;
	}

	return 0;
}

static void sprd_fchg_work(struct work_struct *data)
{
	struct sprd_fchg_info *info =
		container_of(data, struct sprd_fchg_info, fchg_work);
	union power_supply_propval val;
	int ret = 0;

	if (!info->psy_fchg) {
		dev_err(info->dev, "%s, psy_fchg is NULL!!!\n", __func__);
		return;
	}

	mutex_lock(&info->lock);
	if (info->pd_extcon)
		ret = power_supply_get_property(info->psy_fchg, POWER_SUPPLY_PROP_USB_TYPE, &val);
	else if (info->sfcp_extcon)
		ret = power_supply_get_property(info->psy_fchg, POWER_SUPPLY_PROP_CHARGE_TYPE,
						&val);
	else if (info->customized_fchg_extcon)
		ret = power_supply_get_property(info->psy_fchg, POWER_SUPPLY_PROP_CHARGE_TYPE,
						&val);

	if (ret) {
		dev_err(info->dev, "%s, failed to get fchg type\n", __func__);
		mutex_unlock(&info->lock);
		return;
	}

	if (info->pd_extcon && (val.intval == POWER_SUPPLY_USB_TYPE_PD ||
	    (!info->support_pd_pps && val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS))) {
		info->pd_enable = true;
		info->pps_enable = false;
		info->pps_active = false;
		info->sfcp_enable = false;
		info->customized_fchg_enable = false;
		info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		goto out;
	} else if (info->pd_extcon && val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS) {
		info->pps_enable = true;
		info->pd_enable = false;
		info->sfcp_enable = false;
		info->customized_fchg_enable = false;
		info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE;
		goto out;
	} else if (info->sfcp_extcon && val.intval == POWER_SUPPLY_CHARGE_TYPE_FAST) {
		info->sfcp_enable = true;
		info->pps_enable = false;
		info->pps_active = false;
		info->pd_enable = false;
		info->customized_fchg_enable = false;
		info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		goto out;
	} else if (info->customized_fchg_extcon && val.intval == POWER_SUPPLY_CHARGE_TYPE_FAST) {
		info->customized_fchg_enable = true;
		info->pps_enable = false;
		info->pps_active = false;
		info->pd_enable = false;
		info->sfcp_enable = false;
		info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		goto out;
	} else if (info->pd_extcon && val.intval == POWER_SUPPLY_USB_TYPE_C) {
		if (info->pd_enable)
			sprd_pd_fixed_adjust_voltage(info, SPRD_FCHG_VOLTAGE_5V);

		info->pd_enable = false;
		info->pps_enable = false;
		info->pps_active = false;
		if (!info->sfcp_enable && !info->customized_fchg_enable)
			info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

		mutex_unlock(&info->lock);
		dev_info(info->dev, "%s[%d]: extcon: %s, fchg_type: %d\n", __func__, __LINE__,
			 info->pd_extcon ? "pd" : (info->sfcp_extcon ? "sfcp" : "customized"),
			 info->fchg_type);
		return;
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "%s, extcon: %s, fchg_type: %d\n", __func__,
		 info->pd_extcon ? "pd" : (info->sfcp_extcon ? "sfcp" : "customized"),
		 info->fchg_type);
	cm_notify_event(info->psy, CM_EVENT_FAST_CHARGE, NULL);
}

static int sprd_fchg_change(struct notifier_block *nb,
			    unsigned long event, void *data)
{
	struct sprd_fchg_info *info =
		container_of(nb, struct sprd_fchg_info, fchg_notify);
	struct power_supply *psy = data;

	if (info->shutdown_flag)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, SPRD_FCHG_TCPM_PD_NAME) == 0 &&
	    event == PSY_EVENT_PROP_CHANGED) {
		dev_info(info->dev, "%s, pps or pd extcon\n", __func__);
		info->pd_extcon = true;
		info->sfcp_extcon = false;
		info->customized_fchg_extcon = false;
		info->psy_fchg = data;
		schedule_work(&info->fchg_work);
		goto out;
	}

	if (strcmp(psy->desc->name, SPRD_FCHG_SFCP_NAME) == 0 &&
	    event == PSY_EVENT_PROP_CHANGED) {
		dev_info(info->dev, "%s, sfcp extcon\n", __func__);
		info->sfcp_extcon = true;
		info->pd_extcon = false;
		info->customized_fchg_extcon = false;
		info->psy_fchg = data;
		schedule_work(&info->fchg_work);
		goto out;
	}

	if (info->customized_fchg_psy &&
	    strcmp(psy->desc->name, info->customized_fchg_psy) == 0 &&
	    event == PSY_EVENT_PROP_CHANGED) {
		dev_info(info->dev, "%s, customized fchg extcon\n", __func__);
		info->customized_fchg_extcon = true;
		info->sfcp_extcon = false;
		info->pd_extcon = false;
		info->psy_fchg = data;
		schedule_work(&info->fchg_work);
		goto out;
	}

out:
	return NOTIFY_OK;
}

static void sprd_fixed_fchg_handshake_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_fchg_info *info = container_of(dwork,
						   struct sprd_fchg_info,
						   fixed_fchg_handshake_work);
	int ret;

	mutex_lock(&info->lock);
	if (!info->chg_online) {
		if (!info->pps_enable && !info->pd_enable)
			info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

		info->detected = false;
		ret = sprd_enable_sfcp(info, false);
		if (ret)
			dev_err(info->dev, "%s, failed to disable sfcp, ret=%d\n",
				__func__, ret);

		ret = sprd_enable_customized_fchg(info, false);
		if (ret)
			dev_err(info->dev, "%s, failed to disable customized fchg, ret=%d\n",
				__func__, ret);
	} else if (!info->detected) {
		info->detected = true;
		if (info->pd_enable || info->pps_enable) {
			ret = sprd_enable_sfcp(info, false);
			if (ret)
				dev_err(info->dev, "%s, failed to disable sfcp, ret=%d\n",
					__func__, ret);

			ret = sprd_enable_customized_fchg(info, false);
			if (ret)
				dev_err(info->dev, "%s, failed to disable customized fchg, ret=%d\n",
					__func__, ret);
		} else {
			ret = sprd_enable_sfcp(info, true);
			if (ret)
				dev_err(info->dev, "%s, failed to enable sfcp, ret=%d\n",
					__func__, ret);

			ret = sprd_enable_customized_fchg(info, true);
			if (ret)
				dev_err(info->dev, "%s, failed to enable customized fchg, ret=%d\n",
					__func__, ret);
		}
	}

	mutex_unlock(&info->lock);
}

static void sprd_enable_fixed_fchg(struct sprd_fchg_info *info, bool enable)
{
	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->support_fchg)
		return;

	info->chg_online = enable;
	if (!info->chg_online) {
		cancel_delayed_work(&info->fixed_fchg_handshake_work);
		schedule_delayed_work(&info->fixed_fchg_handshake_work, 0);
	} else {
		/*
		 * There is a confilt between charger detection and fast charger
		 * detection, and BC1.2 detection time consumption is <300ms,
		 * so we delay fast charger detection to avoid this issue.
		 */
		schedule_delayed_work(&info->fixed_fchg_handshake_work,
				      SPRD_FIXED_FCHG_DETECT_MS);
	}
}

static int sprd_enable_dynamic_fchg(struct sprd_fchg_info *info, bool enable)
{
	int ret = 0;

	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!info->support_fchg)
		return -ENOMEM;

	mutex_lock(&info->lock);
	if (info->pps_enable) {
		ret = sprd_enable_pps(info, enable);
		if (ret)
			dev_err(info->dev, "%s, failed to %s pps, ret = %d\n",
				__func__, enable ? "enable" : "disable", ret);
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sprd_adjust_fchg_voltage(struct sprd_fchg_info *info, u32 input_vol)
{
	int ret = 0;

	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!info->support_fchg)
		return -ENOMEM;

	mutex_lock(&info->lock);
	if (info->pps_enable) {
		ret = sprd_pps_adjust_voltage(info, input_vol);
		if (ret)
			dev_err(info->dev, "%s, failed to adjust pps voltage, ret=%d\n",
				__func__, ret);
	} else if (info->pd_enable) {
		if (sprd_enable_pps(info, false))
			dev_err(info->dev, "%s, failed to disable pps\n", __func__);

		ret = sprd_pd_fixed_adjust_voltage(info, input_vol);
		if (ret)
			dev_err(info->dev, "%s, failed to adjust pd fixed voltage, ret=%d\n",
				__func__, ret);
	} else if (info->sfcp_enable) {
		ret = sprd_sfcp_fixed_adjust_voltage(info, input_vol);
		if (ret)
			dev_err(info->dev, "%s, failed to adjust sfcp fixed voltage, ret=%d\n",
				__func__, ret);
	} else if (info->customized_fchg_enable) {
		ret = sprd_customized_fchg_adjust_voltage(info, input_vol);
		if (ret)
			dev_err(info->dev, "%s, failed to adjust customized fchg voltage, ret=%d\n",
				__func__, ret);
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sprd_adjust_fchg_current(struct sprd_fchg_info *info, u32 input_current)
{
	int ret = 0;

	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!info->support_fchg)
		return -ENOMEM;

	mutex_lock(&info->lock);
	if (info->pps_enable) {
		ret = sprd_pps_adjust_current(info, input_current);
		if (ret)
			dev_err(info->dev, "%s, failed to adjust pps current, ret=%d\n",
				__func__, ret);
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sprd_update_source_capabilities(struct sprd_fchg_info *info, const u32 *pdo,
				       unsigned int nr_pdo)
{
	int ret = 0;

	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!info->support_fchg)
		return -EINVAL;

	mutex_lock(&info->lock);
	dev_info(info->dev, "%s:%d, pd_extcon = %d\n", __func__, __LINE__, info->pd_extcon);
	ret = sprd_pd_update_source_capabilities(info, pdo, nr_pdo);
	if (ret)
		dev_err(info->dev, "%s, failed to update src cap, ret=%d\n", __func__, ret);
	mutex_unlock(&info->lock);

	return ret;
}

static void sprd_force_set_fixed_fchg_type(struct sprd_fchg_info *info)
{
	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->support_fchg)
		return;

	dev_dbg(info->dev, "%s, fchg type = %d, pps_enable = %d\n",
		__func__, info->fchg_type, info->pps_enable);
	mutex_lock(&info->lock);
	if (info->fchg_type == POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE) {
		info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;

		if (info->pps_enable) {
			info->pd_enable = true;
			info->pps_enable = false;
			info->pps_active = false;
			info->sfcp_enable = false;
			info->customized_fchg_enable = false;
			info->support_pd_pps = false;
		}
	}

	mutex_unlock(&info->lock);

	dev_info(info->dev, "%s, fchg_type = %d, pps: %d, pd: %d, sfcp: %d, customized: %d\n",
		 __func__, info->fchg_type, info->pps_enable, info->pd_enable, info->sfcp_enable,
		 info->customized_fchg_enable);
}

static int sprd_get_fchg_type(struct sprd_fchg_info *info, u32 *type)
{
	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!info->support_fchg)
		return -ENOMEM;

	*type = info->fchg_type;

	return 0;
}

static int sprd_get_fchg_voltage_max(struct sprd_fchg_info *info, int *voltage_max)
{
	int ret = 0;

	*voltage_max = 0;
	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!info->support_fchg)
		return -ENOMEM;

	mutex_lock(&info->lock);
	if (info->pps_enable && info->pps_active) {
		ret = sprd_get_pps_voltage_max(info, voltage_max);
		if (ret)
			dev_err(info->dev, "%s, failed to get pps vol max, ret=%d\n",
				__func__, ret);
	} else if (info->pps_enable || info->pd_enable) {
		ret = sprd_get_pd_adapter_voltage_max(info, voltage_max);
		if (ret)
			dev_err(info->dev, "%s, failed to get pd adapter vol max, ret=%d\n",
				__func__, ret);
	} else if (info->sfcp_enable) {
		ret = sprd_get_sfcp_fixed_voltage_max(info, voltage_max);
		if (ret)
			dev_err(info->dev, "%s, failed to get sfcp fixed vol max, ret=%d\n",
				__func__, ret);
	} else if (info->customized_fchg_enable) {
		ret = sprd_get_customized_fchg_voltage_max(info, voltage_max);
		if (ret)
			dev_err(info->dev, "%s, failed to get customized fchg vol max, ret=%d\n",
				__func__, ret);
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sprd_get_fchg_current_max(struct sprd_fchg_info *info, int input_vol, int *current_max)
{
	int ret = 0;

	*current_max = 0;
	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!info->support_fchg)
		return -ENOMEM;

	mutex_lock(&info->lock);
	if (info->pps_enable && (!input_vol || info->pps_active)) {
		ret = sprd_get_pps_current_max(info, current_max);
		if (ret)
			dev_err(info->dev, "%s, failed to get pps current max, ret=%d\n",
				__func__, ret);
	} else if (info->pps_enable || info->pd_enable) {
		ret = sprd_get_pd_adapter_current(info, input_vol, current_max);
		if (ret)
			dev_err(info->dev, "%s, failed to get pd adapter current, ret=%d\n",
				__func__, ret);
	} else if (info->sfcp_enable) {
		ret = sprd_get_sfcp_fixed_current_max(info, current_max);
		if (ret)
			dev_err(info->dev, "%s, failed to get sfcp fixed current max, ret=%d\n",
				__func__, ret);
	} else if (info->customized_fchg_enable) {
		ret = sprd_get_customized_fchg_current_max(info, current_max);
		if (ret)
			dev_err(info->dev, "%s,failed to get customized fchg current max, ret=%d\n",
				__func__, ret);
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sprd_fchg_extcon_init(struct sprd_fchg_info *info,
				 struct power_supply *psy)
{
	int ret;

	if (!info) {
		pr_err("%s[%d], info is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!psy) {
		pr_err("%s[%d], psy is NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	info->psy = psy;
	info->fchg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	info->support_fchg = device_property_read_bool(info->dev, "sprd,support-fchg");
	if (!info->support_fchg)
		return 0;

	ret = device_property_read_u32(info->dev,
				       "sprd,pd-fixed-max-microwatt",
				       &info->pd_fixed_max_uw);
	if (ret) {
		dev_info(info->dev, "%s, failed to get pd fixed max uw\n", __func__);
		/* If this parameter is not defined in DTS, the default power is 10W */
		info->pd_fixed_max_uw = SPRD_PD_DEFAULT_POWER_UW;
	}

	info->support_sfcp = device_property_read_bool(info->dev, "sprd,support-sfcp");
	info->support_pd_pps = device_property_read_bool(info->dev, "sprd,support-pd-pps");

	ret = of_property_read_string(info->dev->of_node,
				      "sprd,customized-fchg-psy",
				      &info->customized_fchg_psy);
	if (!ret)
		dev_info(info->dev, "%s, support customized fchg psy: %s\n",
			__func__, info->customized_fchg_psy);

	mutex_init(&info->lock);
	INIT_DELAYED_WORK(&info->fixed_fchg_handshake_work, sprd_fixed_fchg_handshake_work);
	INIT_WORK(&info->pd_online_work, sprd_detect_pd_online_work);
	INIT_WORK(&info->fchg_work, sprd_fchg_work);

	info->fchg_notify.notifier_call = sprd_fchg_change;
	ret = power_supply_reg_notifier(&info->fchg_notify);
	if (ret)
		dev_err(info->dev, "%s, failed to register fchg notifier, ret = %d\n",
			__func__, ret);

	return ret;
}

static void sprd_fchg_detect(struct sprd_fchg_info *info)
{
	if (!info) {
		pr_err("%s[%d], info is NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->support_fchg)
		return;

	if (!info->pd_enable && !info->pps_enable)
		schedule_work(&info->pd_online_work);
}

static void sprd_fchg_suspend(struct sprd_fchg_info *info)
{
	if (!info) {
		pr_err("%s[%d], info is NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->support_fchg)
		return;
}

static void sprd_fchg_resume(struct sprd_fchg_info *info)
{
	if (!info) {
		pr_err("%s[%d], info is NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->support_fchg)
		return;
}

static void sprd_fchg_remove(struct sprd_fchg_info *info)
{
	if (!info) {
		pr_err("%s[%d], info is NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->support_fchg)
		return;

	power_supply_unreg_notifier(&info->fchg_notify);
	cancel_delayed_work_sync(&info->fixed_fchg_handshake_work);
	cancel_work_sync(&info->pd_online_work);
	cancel_work_sync(&info->fchg_work);
}

static void sprd_fchg_shutdown(struct sprd_fchg_info *info)
{
	if (!info) {
		pr_err("%s[%d], info is NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!info->support_fchg)
		return;

	info->shutdown_flag = true;
	power_supply_unreg_notifier(&info->fchg_notify);
	cancel_delayed_work_sync(&info->fixed_fchg_handshake_work);
	cancel_work_sync(&info->pd_online_work);
	cancel_work_sync(&info->fchg_work);
}

struct sprd_fchg_info *sprd_fchg_info_register(struct device *dev)
{
	struct sprd_fchg_info *info = NULL;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		pr_err("%s[%d]: info is NULL pointer!!!\n", __func__, __LINE__);
		return ERR_PTR(-ENOMEM);
	}

	info->ops = devm_kzalloc(dev, sizeof(*info->ops), GFP_KERNEL);
	if (!info->ops) {
		pr_err("%s[%d]: ops is NULL pointer!!!\n", __func__, __LINE__);
		return ERR_PTR(-ENOMEM);
	}

	info->dev = dev;

	info->ops->extcon_init = sprd_fchg_extcon_init;
	info->ops->fchg_detect = sprd_fchg_detect;
	info->ops->get_fchg_type = sprd_get_fchg_type;
	info->ops->force_set_fixed_fchg_type = sprd_force_set_fixed_fchg_type;
	info->ops->get_fchg_vol_max = sprd_get_fchg_voltage_max;
	info->ops->get_fchg_cur_max = sprd_get_fchg_current_max;
	info->ops->enable_fixed_fchg = sprd_enable_fixed_fchg;
	info->ops->enable_dynamic_fchg = sprd_enable_dynamic_fchg;
	info->ops->adj_fchg_vol = sprd_adjust_fchg_voltage;
	info->ops->adj_fchg_cur = sprd_adjust_fchg_current;
	info->ops->update_src_cap = sprd_update_source_capabilities;
	info->ops->suspend = sprd_fchg_suspend;
	info->ops->resume = sprd_fchg_resume;
	info->ops->remove = sprd_fchg_remove;
	info->ops->shutdown = sprd_fchg_shutdown;

	return info;
}
EXPORT_SYMBOL_GPL(sprd_fchg_info_register);

