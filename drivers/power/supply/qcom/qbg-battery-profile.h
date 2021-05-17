/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef __QBG_BATTERY_PROFILE_H__
#define __QBG_BATTERY_PROFILE_H__

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

enum ttf_calc_mode {
	TTF_MODE_VBAT_STEP_CHG = 0,
	TTF_MODE_OCV_STEP_CHG,
	TTF_MODE_SOC_STEP_CHG,
};

struct battery_data_table0 {
	int	soc_length;
	int	ocv_length;
	int	*soc;
	int	*ocv;
};

/**
 * struct qbg_battery_data - Structure for QBG battery data
 * @dev_no:		Device number for QBG battery char device
 * @profile_node:	Pointer to devicetree node handle of profile
 * @battery_class:	Pointer to battery class
 * @battery_device:	Pointer to battery class device
 * @battery_cdev:	QBG battery char device
 * @bp:			QBG battery configuration
 * @bp_charge_tables:	Charge tables in QBG battery profile
 * @bp_discharge_tables:	Discharge tables in QBG battery profile
 * @table0:		Two tables for PON OCV to SOC mapping
 * @num_ctables:	Number of charge tables
 * @num_dtables:	Number of discharge tables
 */

struct qbg_battery_data {
	dev_t				dev_no;
	struct device_node		*profile_node;
	struct class			*battery_class;
	struct device			*battery_device;
	struct cdev			battery_cdev;
	struct battery_config		bp;
	struct battery_data_table	**bp_charge_tables;
	struct battery_data_table	**bp_discharge_tables;
	struct battery_data_table0	table0[2];
	int				num_ctables;
	int				num_dtables;
};

int qbg_batterydata_init(struct device_node *node,
	struct qbg_battery_data *battery);
void qbg_batterydata_exit(struct qbg_battery_data *battery);
int qbg_lookup_soc_ocv(struct qbg_battery_data *battery, int *pon_soc, int ocv, bool charging);
#endif /* __QBG_BATTERY_PROFILE_H__ */
