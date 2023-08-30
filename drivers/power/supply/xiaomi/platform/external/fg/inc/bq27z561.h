
#ifndef __BQ27Z561_H
#define __BQ27Z561_H

#define pr_fmt(fmt)	"[bq27z561] %s: " fmt, __func__

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

struct bq_fg_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap    *regmap;

	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;
	struct iio_channel	**ext_iio_chans;
	struct platform_device *pdev;

	struct votable *fv_votable;
	struct votable *smart_batt_votable;
	struct votable *usb_icl_votable;
	struct votable *fcc_votable;

	struct mutex i2c_rw_lock;
	struct mutex data_lock;
	enum  bq_fg_part_no fg_device;

	int fw_ver;
	int df_ver;

	u8 chip;
	u8 regs[NUM_REGS];
	char *model_name;

	/* status tracking */

	bool batt_fc;
	bool batt_fd;	/* full depleted */

	bool batt_dsg;
	bool batt_rca;	/* remaining capacity alarm */

	bool batt_fc_1;
	bool batt_fd_1;	/* full depleted */
	bool batt_tc_1;
	bool batt_td_1;	/* full depleted */

	int seal_state; /* 0 - Full Access, 1 - Unsealed, 2 - Sealed */
	int batt_tte;
	int batt_soc;
	int batt_rsoc;
	int batt_soc_old;
	int batt_soc_flag;
	int batt_fcc;	/* Full charge capacity */
	int batt_rm;	/* Remaining capacity */
	int batt_dc;	/* Design Capacity */
	int batt_volt;
	int batt_temp;
	int old_batt_temp;
	int batt_curr;
	int old_batt_curr;
	int fcc_curr;
	int batt_resistance;
	int batt_cyclecnt;	/* cycle count */
	int batt_st;
	int raw_soc;
	int last_soc;
	int batt_id;
	int Qmax_old;
	int rm_adjust;
	int rm_adjust_max;
	bool rm_flag;

	/* debug */
	int skip_reads;
	int skip_writes;

	int fake_soc;
	int fake_temp;
	int fake_volt;
	int	fake_chip_ok;
	int retry_chip_ok;
	int count;
	int last_count;

	struct	delayed_work monitor_work;
	//struct power_supply *fg_psy;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	//struct power_supply_desc fg_psy_d;
	//struct timeval *suspend_time;
	ktime_t suspend_time;

	u8 digest[BATTERY_DIGEST_LEN];
	bool verify_digest_success;
	int constant_charge_current_max;

	bool charge_done;
	bool charge_full;
	int health;
	int batt_recharge_vol;
	bool recharge_done_flag;
	/* workaround for debug or other purpose */
	bool	ignore_digest_for_debug;
	bool	old_hw;

	int	*dec_rate_seq;
	int	dec_rate_len;

	struct cold_thermal *cold_thermal_seq;
	int	cold_thermal_len;
	bool	update_now;
	bool    resume_update;
	bool	fast_mode;
	int	optimiz_soc;
	bool	ffc_smooth;
	bool	shutdown_delay;
	bool	shutdown_delay_enable;

	int cell1_max;
	int max_charge_current;
	int max_discharge_current;
	int max_temp_cell;
	int min_temp_cell;
	int total_fw_runtime;
	int time_spent_in_lt;
	int time_spent_in_ht;
	int time_spent_in_ot;

	int cell_ov_check;
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

extern struct bq_fg_chip *g_bq27z561;

int fg_write_byte(struct bq_fg_chip *bq, u8 reg, u8 val);
int fg_read_word(struct bq_fg_chip *bq, u8 reg, u16 *val);
int fg_read_block(struct bq_fg_chip *bq, u8 reg, u8 *buf, u8 len);
int fg_write_block(struct bq_fg_chip *bq, u8 reg, u8 *data, u8 len);
u8 checksum(u8 *data, u8 len);
int fg_mac_read_block(struct bq_fg_chip *bq, u16 cmd, u8 *buf, u8 len);
int fg_mac_write_block(struct bq_fg_chip *bq, u16 cmd, u8 *data, u8 len);
int fg_check_battery_psy(struct bq_fg_chip *bq);
int fg_get_gague_mode(struct bq_fg_chip *bq);
int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable);
int fg_read_status(struct bq_fg_chip *bq);
int fg_get_manufacture_data(struct bq_fg_chip *bq);
int fg_get_chem_data(struct bq_fg_chip *bq);
int fg_read_rsoc(struct bq_fg_chip *bq);
int fg_read_system_soc(struct bq_fg_chip *bq);
int fg_read_temperature(struct bq_fg_chip *bq);
int fg_read_volt(struct bq_fg_chip *bq);
int fg_read_avg_current(struct bq_fg_chip *bq, int *curr);
int fg_read_current(struct bq_fg_chip *bq, int *curr);
int fg_read_fcc(struct bq_fg_chip *bq);
int fg_read_dc(struct bq_fg_chip *bq);
int fg_read_rm(struct bq_fg_chip *bq);
int fg_read_soh(struct bq_fg_chip *bq);
int fg_read_cyclecount(struct bq_fg_chip *bq);
int fg_read_tte(struct bq_fg_chip *bq);
int fg_read_charging_voltage(struct bq_fg_chip *bq);
int fg_get_temp_max_fac(struct bq_fg_chip *bq);
int fg_get_time_ot(struct bq_fg_chip *bq);
int fg_read_ttf(struct bq_fg_chip *bq);
int fg_get_chip_ok(struct bq_fg_chip *bq);
int fg_get_batt_status(struct bq_fg_chip *bq);
int fg_get_batt_capacity_level(struct bq_fg_chip *bq);
int fg_get_soc_decimal_rate(struct bq_fg_chip *bq);
int fg_get_soc_decimal(struct bq_fg_chip *bq);
int fg_dump_registers(struct bq_fg_chip *bq);
int fg_get_lifetime_data(struct bq_fg_chip *bq);
void fg_update_status(struct bq_fg_chip *bq);
int fg_get_raw_soc(struct bq_fg_chip *bq);
int fg_update_charge_full(struct bq_fg_chip *bq);
int calc_delta_time(ktime_t time_last, int *delta_time);

#endif /* __BQ27Z561_H */

