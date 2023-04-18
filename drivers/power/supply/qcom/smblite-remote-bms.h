/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __SMBLITE_REMOTE_BMS_H
#define __SMBLITE_REMOTE_BMS_H

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

#define SEB_MAX_RX_PARAMS		25
#define SEB_BUF_HEADER_SIZE		2
#define SEB_EACH_OPCODE_SIZE		1
#define SEB_EACH_DATA_SIZE		4
#define SEB_EACH_PARAM_SIZE		(SEB_EACH_OPCODE_SIZE + SEB_EACH_DATA_SIZE)
/* Allocate more memory to parse extra patameters from remote-fg */
#define SEB_RX_BUF_SIZE			\
		(SEB_BUF_HEADER_SIZE + (SEB_MAX_RX_PARAMS * SEB_EACH_PARAM_SIZE))
#define BMS_WRITE			1
#define BMS_READ			0

#define BMS_READ_INTERVAL_MS		30000

#define REMOTE_FG_VOTER			"REMOTE_FG_VOTER"
#define REMOTE_FG_DEBUG_BATT_SOC	67
#define REMOTE_FG_FAKE_BATT_SOC		50

enum bms_src {
	BMS_SDAM = 1,
	BMS_GLINK,
};

enum bms_tx_param {
	CHARGE_STATUS = 0,
	CHARGE_TYPE,
	CHARGER_PRESENT,
	TX_MAX,
	REQUEST_DATA = TX_MAX,
};

enum bms_rx_param {
	CAPACITY = 0,
	CURRENT_NOW,
	VOLTAGE_OCV,
	CYCLE_COUNT,
	CHARGE_COUNTER,
	CHARGE_FULL,
	CHARGE_FULL_DESIGN,
	TIME_TO_FULL,
	TIME_TO_EMPTY,
	SOH,
	RECHARGE_TRIGGER,
	RECHARGE_FV,
	RECHARGE_ITERM,
	REQUEST_CHG_DATA,
	RX_MAX,
};

struct bms_params {
	enum bms_rx_param offset;
	int sdam_offset;
	unsigned int data;
};

struct smblite_remote_bms {
	struct device			*dev;
	struct mutex			data_lock;
	struct mutex			tx_lock;
	struct mutex			rx_lock;
	struct delayed_work		periodic_fg_work;
	struct work_struct		psy_status_change_work;
	struct work_struct		rx_data_work;
	int				read_interval_ms;
	struct power_supply		*batt_psy;
	struct power_supply		*usb_psy;
	struct nvmem_device		*nvmem;
	struct bms_params		rx_params[RX_MAX];
	struct seb_notif_info		*seb_handle;
	struct notifier_block		seb_nb;
	struct notifier_block		psy_nb;
	struct iio_channel		*batt_id_chan;
	struct iio_channel		*batt_temp_chan;
	struct iio_channel		*batt_volt_chan;
	struct device_node		*batt_node;
	struct votable			*awake_votable;
	const char			*batt_profile_name;
	char				rx_buf[SEB_RX_BUF_SIZE];
	int				batt_id_ohm;
	int				float_volt_uv;
	int				fastchg_curr_ma;
	int				default_iterm_ma;
	int				force_recharge;
	int				recharge_float_voltage;
	int				recharge_iterm;
	int				charge_status;
	int				charge_type;
	int				charger_present;
	bool				is_seb_up;
	bool				received_first_data;
	int (*iio_read)(struct device *dev, int iio_chan, int *val);
	int (*iio_write)(struct device *dev, int iio_chan, int val);
};

int remote_bms_init(struct smblite_remote_bms *bms);
int remote_bms_deinit(void);
int remote_bms_get_prop(int channel, int *val, int src);
int remote_bms_suspend(void);
int remote_bms_resume(void);
#endif /* __SMBLITE_REMOTE_BMS_H */
