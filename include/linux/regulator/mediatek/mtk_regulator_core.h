/*
 * mtk_regulator_core.h -- MTK Regulator driver support.
 * Copyright (C) 2017 MediaTek Inc.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef __LINUX_REGULATOR_MTK_REGULATOR_CORE_H_
#define __LINUX_REGULATOR_MTK_REGULATOR_CORE_H_
#include <linux/regulator/driver.h>
#include <linux/regulator/mediatek/mtk_regulator.h>
#include <linux/mutex.h>

#define MTK_SIMPLE_REGULATOR_VERSION "0.0.4_MTK"

struct mtk_simple_regulator_desc;

struct mtk_simple_regulator_control_ops {
	/* Necessary */
	int (*register_read)(void *client, uint32_t reg, uint32_t *data);
	int (*register_write)(void *client, uint32_t reg, uint32_t data);
	int (*register_update_bits)(void *client, uint32_t reg, uint32_t mask,
		uint32_t data);
};


struct mtk_simple_regulator_ext_ops {
	int (*set_voltage_sel)(struct mtk_simple_regulator_desc *mreg_desc,
				unsigned int selector);
	int (*get_voltage_sel)(struct mtk_simple_regulator_desc *mreg_desc);

	/* enable/disable regulator */
	int (*enable)(struct mtk_simple_regulator_desc *mreg_desc);
	int (*disable)(struct mtk_simple_regulator_desc *mreg_desc);
	int (*is_enabled)(struct mtk_simple_regulator_desc *mreg_desc);
	int (*set_mode)(struct mtk_simple_regulator_desc *mreg_desc,
			unsigned int mode);
	unsigned int (*get_mode)(struct mtk_simple_regulator_desc *mreg_desc);
	int (*get_status)(struct mtk_simple_regulator_desc *mreg_desc);

	/* set regulator suspend voltage */
	int (*set_suspend_voltage)(struct mtk_simple_regulator_desc *mreg_desc,
				   int uV);

	/* enable/disable regulator in suspend state */
	int (*set_suspend_enable)(struct mtk_simple_regulator_desc *mreg_desc);
	int (*set_suspend_disable)(struct mtk_simple_regulator_desc *mreg_desc);
};

struct mtk_simple_regulator_adv_ops {
	int (*set_property)(struct mtk_simple_regulator_desc *mreg_desc,
		enum mtk_simple_regulator_property prop,
		union mtk_simple_regulator_propval *val);
	int (*get_property)(struct mtk_simple_regulator_desc *mreg_desc,
		enum mtk_simple_regulator_property prop,
		union mtk_simple_regulator_propval *val);
};

struct mtk_simple_regulator_desc {
	struct regulator_desc rdesc;
	struct regulator_ops rops;
	struct regulator_dev *rdev;
	struct regulator_init_data *def_init_data;

	struct mtk_simple_regulator_control_ops *mreg_ctrl_ops;
	const struct mtk_simple_regulator_ext_ops *mreg_ext_ops;
	const struct mtk_simple_regulator_adv_ops *mreg_adv_ops;
	struct mtk_simple_regulator_device *mreg_dev;

	void *client;
	void *prv_data;
	uint32_t min_uV;
	uint32_t max_uV;
	const uint32_t  *output_list;
	int (*list_voltage)(struct mtk_simple_regulator_desc *mreg_desc,
		unsigned int selector);
	uint8_t vol_reg;
	uint8_t vol_mask;
	uint8_t enable_reg;
	uint8_t enable_bit;
	uint8_t vol_shift;
};

extern int mtk_simple_regulator_register(
	struct mtk_simple_regulator_desc *mreg_desc,
	struct device *dev,
	const struct mtk_simple_regulator_ext_ops *mreg_ext_ops,
	struct mtk_simple_regulator_adv_ops *mreg_adv_ops);
extern int mtk_simple_regulator_unregister(
				struct mtk_simple_regulator_desc *mreg_desc);

static inline void mtk_simple_regulator_mutex_init(struct mutex *lock)
{
	mutex_init(lock);
}

static inline void mtk_simple_regulator_mutex_lock(struct mutex *lock)
{
	mutex_lock(lock);
}

static inline void mtk_simple_regulator_mutex_unlock(struct mutex *lock)
{
	mutex_unlock(lock);
}

static inline void mtk_simple_regulator_mutex_destroy(struct mutex *lock)
{
	mutex_destroy(lock);
}

#define mreg_decl_vol(_name, _output_list, ctrl_ops) \
{ \
	.rdesc = { \
		.name = #_name, \
		.id = _name##_id, \
		.type = REGULATOR_VOLTAGE, \
		.n_voltages = ARRAY_SIZE(_output_list), \
		.owner = THIS_MODULE, \
	}, \
	.min_uV = _name##_min_uV, \
	.max_uV = _name##_max_uV, \
	.vol_reg = _name##_vol_reg, \
	.vol_mask = _name##_vol_mask, \
	.enable_reg = _name##_enable_reg, \
	.enable_bit = _name##_enable_bit, \
	.vol_shift = _name##_vol_shift, \
	.output_list = _output_list, \
	.mreg_ctrl_ops = ctrl_ops, \
}

#define mreg_decl(_name, _list_voltage, _n_voltages, ctrl_ops) \
{ \
	.rdesc = { \
		.name = #_name, \
		.id = _name##_id, \
		.type = REGULATOR_VOLTAGE, \
		.n_voltages = _n_voltages, \
		.owner = THIS_MODULE, \
	}, \
	.min_uV = _name##_min_uV, \
	.max_uV = _name##_max_uV, \
	.vol_reg = _name##_vol_reg, \
	.vol_mask = _name##_vol_mask, \
	.enable_reg = _name##_enable_reg, \
	.enable_bit = _name##_enable_bit, \
	.vol_shift = _name##_vol_shift, \
	.list_voltage = _list_voltage, \
	.mreg_ctrl_ops = ctrl_ops, \
}

#endif /* __LINUX_REGULATOR_MTK_REGULATOR_CORE_H_ */


/*
 * Version Info
 * 0.0.4_MTK
 * (1) Modify architecture of advance ops,
 * let driver owner can have mtk_simple_regulator_desc as parameter
 *
 * 0.0.3_MTK
 * (1) Fix KE problem of mtk_regulator_get
 * (2) Use constraint->name to register mtk regulator class
 * (3) Also get adv ops in devm_get and exclusive_get
 *
 * 0.0.2_MTK
 * (1) Modify some variables name
 * (2) Let user can assign their init config directly
 *
 * 0.0.1_MTK
 * Initial release
 */

