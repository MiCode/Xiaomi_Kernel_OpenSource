// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 LC Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_LX_FG_CLASS_H__
#define __LINUX_LX_FG_CLASS_H__

struct fuel_gauge_dev;
struct fuel_gauge_ops {
	int (*get_avg_ibat)(struct fuel_gauge_dev *);
	int (*get_soc_decimal)(struct fuel_gauge_dev *);
	int (*get_soc_decimal_rate)(struct fuel_gauge_dev *);
	bool (*get_chip_ok)(struct fuel_gauge_dev *);
	int (*get_resistance_id)(struct fuel_gauge_dev *);
	int (*get_fg_battery_id)(struct fuel_gauge_dev *);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int (*check_i2c_function)(struct fuel_gauge_dev *);
#endif
	int (*set_fastcharge_mode)(struct fuel_gauge_dev *, bool);
	int (*fg_set_charger_to_full)(struct fuel_gauge_dev *);
	int (*get_fastcharge_mode)(struct fuel_gauge_dev *);
	int (*get_input_suspend)(struct fuel_gauge_dev *);
	int (*get_lpd_charging)(struct fuel_gauge_dev *);
	int (*get_mtbf_current)(struct fuel_gauge_dev *);
	int (*fg_write_block)(struct fuel_gauge_dev *, u16 , u8 *, u8 );
	int (*fg_read_block)(struct fuel_gauge_dev *, u16 , u8 *, u8 );
	int (*fg_reg_write_block)(struct fuel_gauge_dev *, u8 , u8 *, u8);
	int (*fg_reg_read_block)(struct fuel_gauge_dev *, u8 , u8 *, u8);
	int (*fg_set_partition_prop)(struct fuel_gauge_dev *, int, int);
	int (*fg_get_partition_prop)(struct fuel_gauge_dev *, int, int *);
	void (*fg_update)(struct fuel_gauge_dev *);
};

struct fuel_gauge_dev {
	struct device dev;
	char *name;
	void *private;
	struct fuel_gauge_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
};

struct fuel_gauge_dev *fuel_gauge_find_dev_by_name(const char *name);
struct fuel_gauge_dev *fuel_gauge_register(char *name, struct device *parent,
			struct fuel_gauge_ops *ops, void *private);

void *fuel_gauge_get_private(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_unregister(struct fuel_gauge_dev *fuel_gauge);

int fuel_gauge_get_avg_ibat(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge);
bool fuel_gauge_get_chip_ok(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_resistance_id(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_battery_id(struct fuel_gauge_dev *fuel_gauge);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
int fuel_gauge_check_i2c_function(struct fuel_gauge_dev *fuel_gauge);
#endif
int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool);
int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_charger_to_full(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_input_suspend(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_lpd_charging(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_mtbf_current(struct fuel_gauge_dev *fuel_gauge);
int fuel_guage_mac_read_block(struct fuel_gauge_dev *fuel_gauge, u16 cmd, u8 *data, u8 len);
int fuel_guage_mac_write_block(struct fuel_gauge_dev *fuel_gauge, u16 cmd, u8 *data, u8 len);
int fuel_guage_write_reg_block(struct fuel_gauge_dev *fuel_gauge, u8 reg, u8 *data, u8 len);
int fuel_guage_read_reg_block(struct fuel_gauge_dev *fuel_gauge, u8 reg, u8 *data, u8 len);
int charger_partition1_set_prop(struct fuel_gauge_dev *fuel_gauge, int type, int val);
int charger_partition1_get_prop(struct fuel_gauge_dev *fuel_gauge, int type, int *val);
void fuel_gauge_status_update(struct fuel_gauge_dev *fuel_gauge);
#endif
