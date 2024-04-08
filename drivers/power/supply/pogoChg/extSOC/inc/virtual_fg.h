
#ifndef __VIRTUAL_FG_H
#define __VIRTUAL_FG_H

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/battmngr/battmngr_notifier.h>
#include <linux/battmngr/battmngr_voter.h>
#include <linux/battmngr/qti_use_pogo.h>
#include "linux/battmngr/platform_class.h"

enum print_reason {
	PR_INTERRUPT    = BIT(0),
	PR_REGISTER     = BIT(1),
	PR_OEM		= BIT(2),
	PR_DEBUG	= BIT(3),
};

#define PROBE_CNT_MAX	50

#define	INVALID_REG_ADDR	0xFF

#define FG_FLAGS_FD				BIT(4)
#define	FG_FLAGS_FC				BIT(5)
#define	FG_FLAGS_DSG				BIT(6)
#define FG_FLAGS_RCA				BIT(9)
#define FG_FLAGS_FASTCHAGE			BIT(5)

#define BATTERY_DIGEST_LEN 32

#define DEFUALT_FULL_DESIGN		5000

#define BQ_REPORT_FULL_SOC	9800
#define BQ_CHARGE_FULL_SOC	9750
#define BQ_RECHARGE_SOC		9800
#define BQ_DEFUALT_FULL_SOC		100

#define BATT_NORMAL_H_THRESHOLD		350

#define TI_TERM_CURRENT_L		1100
#define TI_TERM_CURRENT_H		1232

#define NFG_TERM_CURRENT_L		1237
#define NFG_TERM_CURRENT_H		1458

#define BQ27Z561_DEFUALT_TERM		-200
#define BQ27Z561_DEFUALT_FFC_TERM	-680
#define BQ27Z561_DEFUALT_RECHARGE_VOL	4380

#define NO_FFC_OVER_FV	(4464 * 1000)
#define STEP_FV	(16 * 1000)
#define BATT_WARM_THRESHOLD		480

#define PD_CHG_UPDATE_DELAY_US	20	/*20 sec*/
#define BQ_I2C_FAILED_SOC	-107
#define BQ_I2C_FAILED_TEMP	-307
#define DEBUG_CAPATICY		15
#define BMS_FG_VERIFY		"BMS_FG_VERIFY"
#define CC_CV_STEP		"CC_CV_STEP"
#define FCCMAIN_FG_CHARGE_FULL_VOTER		"FCCMAIN_FG_CHARGE_FULL_VOTER"
#define OVER_FV_VOTER	"OVER_FV_VOTER"
#define SMART_BATTERY_FV	"SMART_BATTERY_FV"

#define BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC			4490
#define BQ_MAXIUM_VOLTAGE_FOR_CELL			4480
#define BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC_SAFETY	4477
enum bq_fg_reg_idx {
	BQ_FG_REG_CTRL = 0,
	BQ_FG_REG_TEMP,		/* Battery Temperature */
	BQ_FG_REG_VOLT,		/* Battery Voltage */
	BQ_FG_REG_CN,		/* Current Now */
	BQ_FG_REG_AI,		/* Average Current */
	BQ_FG_REG_BATT_STATUS,	/* BatteryStatus */
	BQ_FG_REG_TTE,		/* Time to Empty */
	BQ_FG_REG_TTF,		/* Time to Full */
	BQ_FG_REG_FCC,		/* Full Charge Capacity */
	BQ_FG_REG_RM,		/* Remaining Capacity */
	BQ_FG_REG_CC,		/* Cycle Count */
	BQ_FG_REG_SOC,		/* Relative State of Charge */
	BQ_FG_REG_SOH,		/* State of Health */
	BQ_FG_REG_CHG_VOL,	/* Charging Voltage*/
	BQ_FG_REG_CHG_CUR,	/* Charging Current*/
	BQ_FG_REG_DC,		/* Design Capacity */
	BQ_FG_REG_ALT_MAC,	/* AltManufactureAccess*/
	BQ_FG_REG_MAC_DATA,	/* MACData*/
	BQ_FG_REG_MAC_CHKSUM,	/* MACChecksum */
	BQ_FG_REG_MAC_DATA_LEN,	/* MACDataLen */
	NUM_REGS,
};

static u8 bq27z561_regs[NUM_REGS] = {
	0x00,	/* CONTROL */
	0x06,	/* TEMP */
	0x08,	/* VOLT */
	0x0C,	/* CURRENT NOW */
	0x14,	/* AVG CURRENT */
	0x0A,	/* FLAGS */
	0x16,	/* Time to empty */
	0x18,	/* Time to full */
	0x12,	/* Full charge capacity */
	0x10,	/* Remaining Capacity */
	0x2A,	/* CycleCount */
	0x2C,	/* State of Charge */
	0x2E,	/* State of Health */
	0x30,	/* Charging Voltage*/
	0x32,	/* Charging Current*/
	0x3C,	/* Design Capacity */
	0x3E,	/* AltManufacturerAccess*/
	0x40,	/* MACData*/
	0x60,	/* MACChecksum */
	0x61,	/* MACDataLen */
};

enum bq_fg_mac_cmd {
	FG_MAC_CMD_CTRL_STATUS	= 0x0000,
	FG_MAC_CMD_DEV_TYPE	= 0x0001,
	FG_MAC_CMD_FW_VER	= 0x0002,
	FG_MAC_CMD_HW_VER	= 0x0003,
	FG_MAC_CMD_IF_SIG	= 0x0004,
	FG_MAC_CMD_CHEM_ID	= 0x0006,
	FG_MAC_CMD_GAUGING	= 0x0021,
	FG_MAC_CMD_SEAL		= 0x0030,
	FG_MAC_CMD_FASTCHARGE_EN = 0x003E,
	FG_MAC_CMD_FASTCHARGE_DIS = 0x003F,
	FG_MAC_CMD_DEV_RESET	= 0x0041,
	FG_MAC_CMD_CHEM_NAME	= 0x004B,
	FG_MAC_CMD_MANU_NAME	= 0x004C,
	FG_MAC_CMD_CHARGING_STATUS = 0x0055,
	FG_MAC_CMD_GAGUE_STATUS = 0x0056,
	FG_MAC_CMD_LIFETIME1	= 0x0060,
	FG_MAC_CMD_LIFETIME3	= 0x0062,
	FG_MAC_CMD_ITSTATUS1	= 0x0073,
	FG_MAC_CMD_QMAX		= 0x0075,
	FG_MAC_CMD_FCC_SOH	= 0x0077,
	FG_MAC_CMD_RA_TABLE	= 0x40C0,
	FG_MAC_CMD_TERM_CURRENTS = 0x456E,
};

enum {
	SEAL_STATE_RSVED,
	SEAL_STATE_UNSEALED,
	SEAL_STATE_SEALED,
	SEAL_STATE_FA,
};

enum bq_fg_device {
	BQ27Z561 = 0,
	BQ28Z610,
};

static const unsigned char *device2str[] = {
	"bq27z561",
	"bq28z610",
};

struct cold_thermal {
	int index;
	int temp_l;
	int temp_h;
	int curr_th;
};

#define STEP_TABLE_MAX 3
struct step_config {
	int volt_lim;
	int curr_lim;
};

/* for test
struct step_config cc_cv_step_config[STEP_TABLE_MAX] = {
	{4150-2,    5500},
	{4190-1,    4200},
	{4225-1,    3000},
};*/

static struct step_config cc_cv_step_config[STEP_TABLE_MAX] = {
	{4200-3,    8820},
	{4300-3,    7350},
	{4400-3,    5880},
};

enum bq_fg_part_no {
	TIBQ27Z561 = 0,
	NFG1000B,
};

enum manu_macro {
	TERMINATION = 0,
	RECHARGE_VOL,
	FFC_TERMINATION,
	MANU_NAME,
	MANU_DATA_LEN,
};

#define TERMINATION_BYTE	6
#define TERMINATION_BASE	30
#define TERMINATION_STEP	5

#define RECHARGE_VOL_BYTE	7
#define RECHARGE_VOL_BASE	4200
#define RECHARGE_VOL_STEP	5

#define FFC_TERMINATION_BYTE	8
#define FFC_TERMINATION_BASE	400
#define FFC_TERMINATION_STEP	20

#define MANU_NAME_BYTE		3
#define MANU_NAME_BASE		0x0C
#define MANU_NAME_STEP		1

struct manu_data {
	int byte;
	int base;
	int step;
	int data;
};

static struct manu_data manu_info[MANU_DATA_LEN] = {
	{TERMINATION_BYTE, TERMINATION_BASE, TERMINATION_STEP},
	{RECHARGE_VOL_BYTE, RECHARGE_VOL_BASE, RECHARGE_VOL_STEP},
	{FFC_TERMINATION_BYTE, FFC_TERMINATION_BASE, FFC_TERMINATION_STEP},
	{MANU_NAME, MANU_NAME_BASE, MANU_NAME_STEP},
};

#define SHUTDOWN_DELAY_VOL	3300
#define BQ_RESUME_UPDATE_TIME	600

static const u8 fg_dump_regs[] = {
	0x00, 0x02, 0x04, 0x06,
	0x08, 0x0A, 0x0C, 0x0E,
	0x10, 0x16, 0x18, 0x1A,
	0x1C, 0x1E, 0x20, 0x28,
	0x2A, 0x2C, 0x2E, 0x30,
	0x66, 0x68, 0x6C, 0x6E,
};


#ifndef abs
#define abs(x) ((x) >0? (x) : -(x))
#endif

struct vir_bq_fg_chip
{
	struct device *dev;
	char *model_name;
	struct mutex data_lock;

	int batt_st;
	int batt_temp;
	int batt_temp_m;
	int batt_temp_s;
	int batt_curr;
	int batt_curr_m;
	int batt_curr_s;
	int batt_volt;
	int batt_volt_m;
	int batt_volt_s;
	int term_curr;
	int ffc_term_curr;
	int recharge_volt;
	int batt_ttf;
	int batt_tte;
	int batt_fcc;
	int batt_soc;
	int batt_rm;
	int batt_rm_m;
	int batt_rm_s;
	int raw_soc;
	int raw_soc_m;
	int raw_soc_s;
	int last_soc;
	int batt_cyclecnt;
	int batt_resistance;
	int batt_dc;
	int batt_capacity_level;
	int batt_recharge_vol;
	int soc_decimal;
	int soc_decimal_rate;
	int constant_charge_current_max;
	bool verify_digest_success;
	bool fast_mode;
	int soh;
	int fcc_master;
	int fcc_slave;
	bool fg1_batt_ctl_enabled;
	bool fg2_batt_ctl_enabled;
	int fg_master_disable_gpio;
	int fg_slave_disable_gpio;

	int fake_volt;
	int fake_soc;
	int fake_chip_ok;
	int fake_temp;

	int *dec_rate_seq;
	int dec_rate_len;

	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct power_supply *fg_psy;
	struct power_supply_desc fg_psy_d;
	struct power_supply *fg_master_psy;
	struct power_supply *fg_slave_psy;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct delayed_work monitor_work;
	//const struct battmngr_ops *fg_ops;
	struct battmngr_device* battmg_dev;


	/* workaround for debug or other purpose */
	bool ignore_digest_for_debug;
	bool old_hw;

	bool shutdown_delay;
	bool shutdown_delay_enable;

	/* move update charge full states  */
	bool charge_done;
	bool charge_full;
	int health;

	/* counter for low temp soc smooth */
	int master_soc_smooth_cnt;
	int slave_soc_smooth_cnt;
};


//soc smooth

#define BATT_HIGH_AVG_CURRENT		1000000
#define NORMAL_TEMP_CHARGING_DELTA	10000
#define NORMAL_DISTEMP_CHARGING_DELTA	60000
#define LOW_TEMP_CHARGING_DELTA		20000
#define LOW_TEMP_DISCHARGING_DELTA	40000
#define FFC_SMOOTH_LEN			4
#define FG_RAW_SOC_FULL			10000
#define FG_REPORT_FULL_SOC		9400
#define FG_OPTIMIZ_FULL_TIME		64000

struct ffc_smooth {
	int curr_lim;
	int time;
};

static struct ffc_smooth ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    300000},
	{300,  150000},
	{600,   72000},
	{1000,  50000},
};

extern int battery_process_event_fg(struct battmngr_notify *noti_data);

#endif /* __VIRTUAL_FG_H */

