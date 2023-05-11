/*
 * max77729.h
 *
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * This driver is based on max8997.h
 *
 * MAX77729 has Flash LED, SVC LED, Haptic, MUIC devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __MAX77729_H__
#define __MAX77729_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MFD_DEV_NAME "max77729"
#define M2SH(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) : ((m) & 0x04 ? 2 : 3)) : \
		((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) : ((m) & 0x40 ? 6 : 7)))

struct max77729_vibrator_pdata {
	int gpio;
	char *regulator_name;
	struct pwm_device *pwm;
	const char *motor_type;

	int freq;
	/* for multi-frequency */
	int freq_nums;
	u32 *freq_array;
	u32 *ratio_array; /* not used now */
	int normal_ratio;
	int overdrive_ratio;
	int high_temp_ratio;
	int high_temp_ref;
#if defined(CONFIG_SEC_VIBRATOR)
	bool calibration;
	int steps;
	int *intensities;
	int *haptic_intensities;
#endif
};

struct max77729_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct max77729_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;
	bool blocking_waterevent;
	bool extra_fw_enable;
	int wpc_en;
	int fw_product_id;
	struct muic_platform_data *muic_pdata;

	int num_regulators;
	struct max77729_regulator_data *regulators;
	struct max77729_vibrator_pdata *vibrator_data;
	struct mfd_cell *sub_devices;
	int num_subdevs;
	bool support_audio;
	char *wireless_charger_name;
};

struct max77729 {
	struct regmap *regmap;
};

#endif /* __MAX77729_H__ */

