
#include <linux/battmngr/xm_charger_core.h>

int xm_charger_parse_dt_therm(struct xm_charger *charger)
{
	struct device_node *node = charger->dev->of_node;
	int rc = 0, byte_len;

	if (of_find_property(node, "xm,thermal-mitigation-dcp", &byte_len)) {
		charger->dt.thermal_mitigation_dcp = devm_kzalloc(charger->dev, byte_len,
			GFP_KERNEL);

		if (charger->dt.thermal_mitigation_dcp == NULL)
			return -ENOMEM;

		charger->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"xm,thermal-mitigation-dcp",
				charger->dt.thermal_mitigation_dcp,
				charger->thermal_levels);
		if (rc < 0) {
			charger_err("%s: Couldn't read threm limits rc = %d\n",
					__func__, rc);
			return rc;
		}
	}

	if (of_find_property(node, "xm,thermal-mitigation-qc2", &byte_len)) {
		charger->dt.thermal_mitigation_qc2 = devm_kzalloc(charger->dev, byte_len,
			GFP_KERNEL);

		if (charger->dt.thermal_mitigation_qc2 == NULL)
			return -ENOMEM;

		charger->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"xm,thermal-mitigation-qc2",
				charger->dt.thermal_mitigation_qc2,
				charger->thermal_levels);
		if (rc < 0) {
			charger_err("%s: Couldn't read threm limits rc = %d\n",
					__func__, rc);
			return rc;
		}
	}

	if (of_find_property(node, "xm,thermal-mitigation-pd", &byte_len)) {
		charger->dt.thermal_mitigation_pd = devm_kzalloc(charger->dev, byte_len,
				GFP_KERNEL);

		if (charger->dt.thermal_mitigation_pd == NULL)
			return -ENOMEM;

		charger->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"xm,thermal-mitigation-pd",
				charger->dt.thermal_mitigation_pd,
				charger->thermal_levels);
		if (rc < 0) {
			charger_err("%s: Couldn't read threm limits rc = %d\n",
					__func__, rc);
			return rc;
		}
	}

	if (of_find_property(node, "xm,thermal-mitigation-pd-cp", &byte_len)) {
		charger->dt.thermal_mitigation_pd_cp = devm_kzalloc(charger->dev, byte_len,
				GFP_KERNEL);

		if (charger->dt.thermal_mitigation_pd_cp == NULL)
			return -ENOMEM;

		charger->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"xm,thermal-mitigation-pd-cp",
				charger->dt.thermal_mitigation_pd_cp,
				charger->thermal_levels);
		if (rc < 0) {
			charger_err("%s: Couldn't read threm limits rc = %d\n",
					__func__, rc);
			return rc;
		}
	}

	if (of_find_property(node, "xm,thermal-mitigation-icl", &byte_len)) {
		charger->dt.thermal_mitigation_icl = devm_kzalloc(charger->dev, byte_len,
			GFP_KERNEL);

		if (charger->dt.thermal_mitigation_icl == NULL)
				return -ENOMEM;

		charger->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"xm,thermal-mitigation-icl",
				charger->dt.thermal_mitigation_icl,
				charger->thermal_levels);
		if (rc < 0) {
			charger_err("%s: Couldn't read threm limits rc = %d\n",
					__func__, rc);
			return rc;
		}
	}

	return rc;
}

int xm_charger_thermal(struct xm_charger *charger)
{
	int thermal_icl_ua = 0;
	int thermal_fcc_ua = 0;
	int rc;

	if ((charger->system_temp_level >= MAX_TEMP_LEVEL) ||
			(charger->system_temp_level < 0))
		return -EINVAL;

	if (charger->pd_active) {
		if (charger->pd_active == QTI_POWER_SUPPLY_PD_PPS_ACTIVE) {
			thermal_fcc_ua = charger->dt.thermal_mitigation_pd_cp[charger->system_temp_level];
		} else {
			thermal_icl_ua = charger->dt.thermal_mitigation_icl[charger->system_temp_level];
			thermal_fcc_ua = charger->dt.thermal_mitigation_pd[charger->system_temp_level];
		}
	} else if (charger->bc12_type) {
		if (charger->bc12_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
			thermal_icl_ua = charger->dt.thermal_mitigation_icl[charger->system_temp_level];
			thermal_fcc_ua = charger->dt.thermal_mitigation_qc2[charger->system_temp_level];
		} else {
			thermal_icl_ua = charger->dt.thermal_mitigation_icl[charger->system_temp_level];
			thermal_fcc_ua = charger->dt.thermal_mitigation_pd[charger->system_temp_level];
		}
	} else {
		thermal_icl_ua = charger->dt.thermal_mitigation_icl[charger->system_temp_level];
		thermal_fcc_ua = charger->dt.thermal_mitigation_dcp[charger->system_temp_level];
	}

	charger_err("%s: thermal_icl_ua is %d, chg->system_temp_level: %d, thermal_fcc_ua is %d, pd_active = %d, bc12_type = %d\n",
		__func__, thermal_icl_ua, charger->system_temp_level, thermal_fcc_ua, charger->pd_active, charger->bc12_type);

	if (charger->system_temp_level == 0) {
		/* if therm_lvl_sel is 0, clear thermal voter */
		rc = vote(charger->icl_votable, THERMAL_DAEMON_VOTER, false, 0);
		if (rc < 0)
			charger_err("%s: Couldn't disable USB thermal ICL vote rc = %d\n",
					__func__, rc);
		rc = vote(charger->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
		if (rc < 0)
			charger_err("%s: Couldn't disable USB thermal FCC vote rc = %d\n",
					__func__, rc);
	} else {
		if (thermal_icl_ua > 0) {
			rc = vote(charger->icl_votable, THERMAL_DAEMON_VOTER, true, thermal_icl_ua);
			if (rc < 0)
				charger_err("%s: Couldn't enable USB thermal ICL vote rc = %d\n",
					__func__, rc);
		}
		if (thermal_fcc_ua > 0) {
			rc = vote(charger->fcc_votable, THERMAL_DAEMON_VOTER, true, thermal_fcc_ua);
			if (rc < 0)
				charger_err("%s: Couldn't enable USB thermal FCC vote rc = %d\n",
					__func__, rc);
		}
	}

	return rc;
}

