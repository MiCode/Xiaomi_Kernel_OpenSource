/*
 * MAX77665 charger Driver
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LINUX_INPUT_MAX77665_CHARGER_H
#define _LINUX_INPUT_MAX77665_CHARGER_H

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/machine.h>
#include <linux/extcon.h>
#include <linux/mfd/max77665.h>

#define MAX77665_CHG_INT        0xb0
#define MAX77665_CHG_INT_MASK   0xb1
#define MAX77665_CHG_INT_OK     0xb2
/* the bit def for above three registers is same per Maxim design */
#define BYP_BIT			BIT(0)
#define DETBAT_BIT		BIT(2)
#define BAT_BIT			BIT(3)
#define CHG_BIT			BIT(4)
#define CHGIN_BIT		BIT(6)

#define MAX77665_CHG_DTLS_00    0xb3
#define CHGIN_DTLS_MASK(val)	((val & 0x60) >> 5)
#define CHGIN_DTLS_VALID	0x3

#define MAX77665_CHG_DTLS_01    0xb4
#define CHG_DTLS_MASK(val)	((val) & 0xF)
#define CHG_DTLS_PREQUALIFICATION_MODE		0x0
#define CHG_DTLS_FAST_CONSTANT_CURRENT_MODE	0x1
#define CHG_DTLS_FAST_CONSTANT_VOLTAGE_MODE	0x2
#define CHG_DTLS_TOP_OFF_MODE			0x3
#define CHG_DTLS_DONE_MODE			0x4
#define CHG_DTLS_HIGH_TEMP_CHG_MODE		0x5
#define CHG_DTLS_TIME_FAULT_MODE		0x6
#define CHG_DTLS_THERMISTOR_SUSPEND_FAULT_MODE	0x7
#define CHG_DTLS_CHG_OFF_INPUT_INVALID		0x8
#define CHG_DTLS_CHG_OFF_JUNCT_TEMP		0xA
#define CHG_DTLS_CHG_OFF_WDT_EXPIRE		0xB
#define BAT_DTLS_MASK(val)	(((val) & 0x7F) >> 4)
#define BAT_DTLS_NO_BATTERY	0x0
#define BAT_DTLS_OVERCURRENT	0x6

#define MAX77665_CHG_DTLS_02    0xb5
#define BYP_DTLS_MASK(val)	((val) & 0xF)
#define BYP_DTLS_VALID		0x0
#define MAX77665_CHG_DTLS_03    0xb6
#define MAX77665_CHG_CNFG_00    0xb7
#define CHARGER_OFF_OTG_OFF_BUCK_ON_BOOST_OFF 0x04
#define CHARGER_ON_OTG_OFF_BUCK_ON_BOOST_OFF 0x05
#define CHARGER_OFF_OTG_ON_BUCK_OFF_BOOST_ON 0x2A
#define WDTEN	(1 << 4)

#define MAX77665_CHG_CNFG_01			0xb8
#define FAST_CHARGE_DURATION_DISABLE		(0x0 << 0)
#define FAST_CHARGE_DURATION_4HR		(0x1 << 0)
#define FAST_CHARGE_DURATION_6HR		(0x2 << 0)
#define FAST_CHARGE_DURATION_8HR		(0x3 << 0)
#define FAST_CHARGE_DURATION_10HR		(0x4 << 0)
#define FAST_CHARGE_DURATION_12HR		(0x5 << 0)
#define FAST_CHARGE_DURATION_14HR		(0x6 << 0)
#define FAST_CHARGE_DURATION_16HR		(0x7 << 0)
#define CHARGER_RESTART_THRESHOLD_100mV		(0x0 << 4)
#define CHARGER_RESTART_THRESHOLD_150mV		(0x1 << 4)
#define CHARGER_RESTART_THRESHOLD_200mV		(0x2 << 4)
#define CHARGER_RESTART_THRESHOLD_DISABLE	(0x3 << 4)
#define LOW_BATTERY_PREQ_DISABLE		(0x0 << 7)
#define LOW_BATTERY_PREQ_ENABLE			(0x1 << 7)

#define MAX77665_CHG_CNFG_02    0xb9
#define MAX77665_CHG_CNFG_03    0xba
#define MAX77665_CHG_CNFG_04    0xbb
#define MAX77665_CHG_CNFG_05    0xbc

#define MAX77665_CHG_CNFG_06    0xbd
#define WDTCLR	(1 << 0)

#define MAX77665_CHG_CNFG_07    0xbe
#define MAX77665_CHG_CNFG_08    0xbf

#define MAX77665_CHG_CNFG_09    0xc0
#define MIN_CURRENT_LIMIT_mA	60
#define CURRENT_STEP_mA		20

#define MAX77665_CHG_CNFG_10    0xc1
#define MAX77665_CHG_CNFG_11    0xc2

#define MAX77665_CHG_CNFG_12    0xc3
#define BAT_TO_SYS_OVERCURRENT_MASK	0x7
#define BAT_TO_SYS_OVERCURRENT_3A00	(0x1 << 0)
#define BAT_TO_SYS_OVERCURRENT_3A25	(0x2 << 0)
#define BAT_TO_SYS_OVERCURRENT_3A50	(0x3 << 0)
#define BAT_TO_SYS_OVERCURRENT_3A75	(0x4 << 0)
#define BAT_TO_SYS_OVERCURRENT_4A00	(0x5 << 0)
#define BAT_TO_SYS_OVERCURRENT_4A25	(0x5 << 0)
#define BAT_TO_SYS_OVERCURRENT_4A75	(0x7 << 0)
#define	VCHGIN_REGULATION_4V3		(0x0 << 3)
#define	VCHGIN_REGULATION_4V7		(0x1 << 3)
#define	VCHGIN_REGULATION_4V8		(0x2 << 3)
#define	VCHGIN_REGULATION_4V9		(0x3 << 3)
#define	CHARGER_LOW_POWER_MODE_OFF	(0x0 << 7)
#define	CHARGER_LOW_POWER_MODE_ON	(0x1 << 7)

#define MAX77665_CHG_CNFG_13    0xc4

#define MAX77665_CHG_CNFG_14    0xc5

#define MAX77665_SAFEOUTCTRL    0xc6
#define ENSAFEOUT1		BIT(6)
#define ENSAFEOUT2		BIT(7)

#define MAX77665_WATCHDOG_TIMER_PERIOD_S 80

#define MAX_CABLES	6

enum max77665_mode {
	OFF,
	CHARGER,
	OTG,
};

struct max77665_muic_platform_data {
	const char *ext_conn_name;
};

struct max77665_charger_cable {
	const char *extcon_name;
	const char *name;
	struct notifier_block nb;
	struct max77665_charger *charger;
	struct extcon_specific_cable_nb *extcon_dev;
	struct delayed_work extcon_notifier_work;
	long int event;
};

struct max77665_charger_plat_data {
	uint32_t fast_chg_cc; /* fast charger current*/
	uint32_t term_volt; /* charger termination voltage */
	uint32_t curr_lim; /* input current limit */
	uint8_t num_cables;
	struct max77665_charger_cable *cables;
	char *extcon_name;
	struct regulator_consumer_supply *consumer_supplies;
	int num_consumer_supplies;
	bool is_battery_present;
};

static inline bool charging_is_on(int val)
{
	switch (CHG_DTLS_MASK(val)) {
	case CHG_DTLS_PREQUALIFICATION_MODE:
	case CHG_DTLS_FAST_CONSTANT_CURRENT_MODE:
	case CHG_DTLS_FAST_CONSTANT_VOLTAGE_MODE:
	case CHG_DTLS_TOP_OFF_MODE:
	case CHG_DTLS_HIGH_TEMP_CHG_MODE:
		return true;
	case CHG_DTLS_DONE_MODE:
		/*
		 * Fall through:
		 * charger is in done mode after 30mins(Tto) in top-off mode,
		 * charging current will be 0 in this state.
		 */
	default:
		return false;
	}
}
#endif
