// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <linux/rtc.h>

/* PMIC global control registers definition */
#define SC27XX_MODULE_EN0		0xc08
#define SC27XX_CLK_EN0			0xc18
#define SC2730_MODULE_EN0		0x1808
#define SC2730_CLK_EN0			0x1810
#define UMP9620_MODULE_EN0		0x2008
#define UMP9620_CLK_EN0			0x2010
#define SC2720_MODULE_EN0		0xc08
#define SC2720_CLK_EN0			0xc10
#define SC27XX_FGU_EN			BIT(7)
#define SC27XX_FGU_RTC_EN		BIT(6)

/* FGU registers definition */
#define SC27XX_FGU_START		0x0
#define SC27XX_FGU_CONFIG		0x4
#define SC27XX_FGU_ADC_CONFIG		0x8
#define SC27XX_FGU_STATUS		0xc
#define SC27XX_FGU_INT_EN		0x10
#define SC27XX_FGU_INT_CLR		0x14
#define SC27XX_FGU_INT_STS		0x1c
#define SC27XX_FGU_VOLTAGE		0x20
#define SC27XX_FGU_OCV			0x24
#define SC27XX_FGU_POCV			0x28
#define SC27XX_FGU_CURRENT		0x2c
#define SC27XX_FGU_HIGH_OVERLOAD	0x30
#define SC27XX_FGU_LOW_OVERLOAD		0x34
#define SC27XX_FGU_CLBCNT_SETH		0x50
#define SC27XX_FGU_CLBCNT_SETL		0x54
#define SC27XX_FGU_CLBCNT_DELTH		0x58
#define SC27XX_FGU_CLBCNT_DELTL		0x5c
#define SC27XX_FGU_CLBCNT_VALH		0x68
#define SC27XX_FGU_CLBCNT_VALL		0x6c
#define SC27XX_FGU_CLBCNT_QMAXL		0x74
#define SC27XX_FGU_RELAX_CURT_THRE	0x80
#define SC27XX_FGU_RELAX_CNT_THRE	0x84
#define SC27XX_FGU_USER_AREA_SET	0xa0
#define SC27XX_FGU_USER_AREA_CLEAR	0xa4
#define SC27XX_FGU_USER_AREA_STATUS	0xa8
#define SC27XX_FGU_USER_AREA_SET1	0xc0
#define SC27XX_FGU_USER_AREA_CLEAR1	0xc4
#define SC27XX_FGU_USER_AREA_STATUS1	0xc8
#define SC27XX_FGU_VOLTAGE_BUF		0xd0
#define SC27XX_FGU_CURRENT_BUF		0xf0
#define SC27XX_FGU_REG_MAX		0x260

/* SC27XX_FGU_CONFIG */
#define SC27XX_FGU_LOW_POWER_MODE	BIT(1)
#define SC27XX_FGU_RELAX_CNT_MODE	0
#define SC27XX_FGU_DEEP_SLEEP_MODE	1

#define SC27XX_WRITE_SELCLB_EN		BIT(0)
#define SC27XX_FGU_CLBCNT_MASK		GENMASK(15, 0)
#define SC27XX_FGU_CLBCNT_SHIFT		16
#define SC27XX_FGU_HIGH_OVERLOAD_MASK	GENMASK(12, 0)
#define SC27XX_FGU_LOW_OVERLOAD_MASK	GENMASK(12, 0)

#define SC27XX_FGU_INT_MASK		GENMASK(9, 0)
#define SC27XX_FGU_LOW_OVERLOAD_INT	BIT(0)
#define SC27XX_FGU_HIGH_OVERLOAD_INT	BIT(1)
#define SC27XX_FGU_CLBCNT_DELTA_INT	BIT(2)
#define SC27XX_FGU_RELAX_CNT_INT	BIT(3)
#define SC27XX_FGU_RELAX_CNT_STS	BIT(3)
#define SC27XX_FGU_STATUS_INVALID_POCV  BIT(7)

#define SC27XX_FGU_MODE_AREA_MASK	GENMASK(15, 12)
#define SC27XX_FGU_CAP_AREA_MASK	GENMASK(11, 0)
#define SC27XX_FGU_MODE_AREA_SHIFT	12
#define SC27XX_FGU_CAP_INTEGER_MASK	GENMASK(7, 0)
#define SC27XX_FGU_CAP_DECIMAL_MASK	GENMASK(3, 0)
#define SC27XX_FGU_CAP_DECIMAL_SHIFT	8

#define SC27XX_FGU_FIRST_POWERON	GENMASK(3, 0)
#define SC27XX_FGU_DEFAULT_CAP		GENMASK(11, 0)
#define SC27XX_FGU_NORMAL_POWERON	0x5
#define SC27XX_FGU_RTC2_RESET_VALUE	0xA05

#define SC27XX_FGU_CUR_BASIC_ADC	8192
#define SC27XX_FGU_POCV_VOLT_THRESHOLD	3400
#define SC27XX_FGU_SAMPLE_HZ		2
#define SC27XX_FGU_TEMP_BUFF_CNT	10
#define SC27XX_FGU_LOW_TEMP_REGION	100

/* micro Ohms */
#define SC27XX_FGU_IDEAL_RESISTANCE	20000
#define SC27XX_FGU_LOW_VBAT_REGION	3400
#define SC27XX_FGU_LOW_VBAT_REC_REGION	3450
#define SC27XX_FGU_LOW_VBAT_UUSOC_STEP	7
#define SC27XX_FGU_RELAX_CNT_THRESHOLD	320
#define SC27XX_FGU_RELAX_CUR_THRESHOLD_MA	30
#define SC27XX_FGU_SLP_CAP_CALIB_SLP_TIME	300
#define SC27XX_FGU_CAP_CALIB_TEMP_LOW	100
#define SC27XX_FGU_CAP_CALIB_TEMP_HI	450
#define SC27XX_FGU_CAP_CALIB_ALARM_CAP	30

/* Efuse fgu calibration bit definition */
#define SC2720_FGU_CAL			GENMASK(8, 0)
#define SC2720_FGU_CAL_SHIFT		0
#define SC2730_FGU_CAL			GENMASK(8, 0)
#define SC2730_FGU_CAL_SHIFT		0
#define SC2731_FGU_CAL			GENMASK(8, 0)
#define SC2731_FGU_CAL_SHIFT		0
#define UMP9620_FGU_CAL			GENMASK(15, 7)
#define UMP9620_FGU_CAL_SHIFT		7

/* SC27XX_FGU_RELAX_CURT_THRE */
#define SC27XX_FGU_RELAX_CURT_THRE_MASK		GENMASK(13, 0)
#define SC27XX_FGU_RELAX_CURT_THRE_SHITF	0
/* SC27XX_FGU_RELAX_CNT_THRE */
#define SC27XX_FGU_RELAX_CNT_THRE_MASK		GENMASK(12, 0)
#define SC27XX_FGU_RELAX_CNT_THRE_SHITF		0

#define SC27XX_FGU_CURRENT_BUFF_CNT	8
#define SC27XX_FGU_DISCHG_CNT		4
#define SC27XX_FGU_VOLTAGE_BUFF_CNT	8
#define SC27XX_FGU_MAGIC_NUMBER		0x5a5aa5a5
#define SC27XX_FGU_DEBUG_EN_CMD		0x5a5aa5a5
#define SC27XX_FGU_DEBUG_DIS_CMD	0x5a5a5a5a
#define SC27XX_FGU_GOOD_HEALTH_CMD	0x7f7f7f7f
#define SC27XX_FGU_FCC_PERCENT		1000

#define SC27XX_FGU_TRACK_CAP_START_VOLTAGE		3650
#define SC27XX_FGU_TRACK_CAP_START_CURRENT		50
#define SC27XX_FGU_TRACK_HIGH_TEMP_THRESHOLD		450
#define SC27XX_FGU_TRACK_LOW_TEMP_THRESHOLD		150
#define SC27XX_FGU_TRACK_TIMEOUT_THRESHOLD		108000
#define SC27XX_FGU_TRACK_NEW_OCV_VALID_THRESHOLD	(SC27XX_FGU_TRACK_TIMEOUT_THRESHOLD / 60)
#define SC27XX_FGU_TRACK_START_CAP_HTHRESHOLD		200
#define SC27XX_FGU_TRACK_START_CAP_LTHRESHOLD		10
#define SC27XX_FGU_TRACK_START_CAP_SWOCV_HTHRESHOLD	100
#define SC27XX_FGU_TRACK_WAKE_UP_MS			16000
#define SC27XX_FGU_TRACK_UPDATING_WAKE_UP_MS		200
#define SC27XX_FGU_TRACK_DONE_WAKE_UP_MS		6000
#define SC27XX_FGU_TRACK_OCV_VALID_TIME			15

#define SC27XX_FGU_RESIST_ALG_REIST_CNT		40
#define SC27XX_FGU_RESIST_ALG_OCV_GAP_UV	20000
#define SC27XX_FGU_RESIST_ALG_OCV_CNT		10
#define SC27XX_FGU_RBAT_CMP_MOH			10

#define SC27XX_FGU_CAPACITY_TRACK_0S		0
#define SC27XX_FGU_CAPACITY_TRACK_3S		3
#define SC27XX_FGU_CAPACITY_TRACK_15S		15
#define SC27XX_FGU_CAPACITY_TRACK_100S		100
#define SC27XX_FGU_WORK_MS			msecs_to_jiffies(15000)

#define SC27XX_FGU_PROBE_TIMEOUT		msecs_to_jiffies(3000)

/* RTC OF 2021-08-06 15 : 44*/
#define SC27XX_FGU_MISCDATA_RTC_TIME		(1621355101)
#define SC27XX_FGU_SHUTDOWN_TIME		(15 * 60)

#define interpolate(x, x1, y1, x2, y2) \
	((y1) + ((((y2) - (y1)) * ((x) - (x1))) / ((x2) - (x1))))

struct power_supply_vol_temp_table {
	int vol;	/* microVolts */
	int temp;	/* celsius */
};

struct power_supply_capacity_temp_table {
	int temp;	/* celsius */
	int cap;	/* capacity percentage */
};

enum sc27xx_fgu_track_state {
	CAP_TRACK_INIT,
	CAP_TRACK_IDLE,
	CAP_TRACK_UPDATING,
	CAP_TRACK_DONE,
	CAP_TRACK_ERR,
};

enum sc27xx_fgu_track_mode {
	CAP_TRACK_MODE_UNKNOWN,
	CAP_TRACK_MODE_SW_OCV,
	CAP_TRACK_MODE_POCV,
	CAP_TRACK_MODE_LP_OCV,
};

struct sc27xx_fgu_ocv_info {
	s64 ocv_rtc_time;
	int ocv_uv;
	bool valid;
};

struct sc27xx_fgu_track_capacity {
	enum sc27xx_fgu_track_state state;
	bool clear_cap_flag;
	int start_clbcnt;
	int start_cap;
	int end_vol;
	int end_cur;
	s64 start_time;
	bool cap_tracking;
	int learned_mah;
	struct sc27xx_fgu_ocv_info lpocv_info;
	struct sc27xx_fgu_ocv_info pocv_info;
	density_ocv_table *dens_ocv_table;
	int dens_ocv_table_len;
	enum sc27xx_fgu_track_mode mode;
};

struct sc27xx_fgu_debug_info {
	bool temp_debug_en;
	bool vbat_now_debug_en;
	bool ocv_debug_en;
	bool cur_now_debug_en;
	bool batt_present_debug_en;
	bool chg_vol_debug_en;
	bool batt_health_debug_en;

	int debug_temp;
	int debug_vbat_now;
	int debug_ocv;
	int debug_cur_now;
	bool debug_batt_present;
	int debug_chg_vol;
	int debug_batt_health;

	int sel_reg_id;
};

struct sc27xx_fgu_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sc27xx_fgu_dump_info;
	struct device_attribute attr_sc27xx_fgu_sel_reg_id;
	struct device_attribute attr_sc27xx_fgu_reg_val;
	struct device_attribute attr_sc27xx_fgu_enable_sleep_calib;
	struct device_attribute attr_sc27xx_fgu_relax_cnt_th;
	struct device_attribute attr_sc27xx_fgu_relax_cur_th;
	struct attribute *attrs[7];

	struct sc27xx_fgu_data *data;
};

struct sc27xx_fgu_sleep_capacity_calibration {
	bool support_slp_calib;
	int suspend_ocv_uv;
	int resume_ocv_uv;
	int suspend_clbcnt;
	int resume_clbcnt;
	s64 suspend_time;
	s64 resume_time;
	int resume_ocv_cap;

	int relax_cnt_threshold;
	int relax_cur_threshold;

	bool relax_cnt_int_ocurred;
};

/*
 * struct sc27xx_fgu_data: describe the FGU device
 * @regmap: regmap for register access
 * @dev: platform device
 * @battery: battery power supply
 * @base: the base offset for the controller
 * @lock: protect the structure
 * @gpiod: GPIO for battery detection
 * @channel: IIO channel to get battery temperature
 * @charge_chan: IIO channel to get charge voltage
 * @internal_resist: the battery internal resistance in mOhm
 * @total_mah: the total capacity of the battery in mAh
 * @init_cap: the initial capacity of the battery in mAh
 * @alarm_cap: the alarm capacity
 * @normal_temp_cap: the normal temperature capacity
 * @init_clbcnt: the initial coulomb counter
 * @max_volt_uv: the maximum constant input voltage in millivolt
 * @min_volt_uv: the minimum drained battery voltage in microvolt
 * @boot_volt_uv: the voltage measured during boot in microvolt
 * @table_len: the capacity table length
 * @temp_table_len: temp_table length
 * @cap_table_len：the capacity temperature table length
 * @resist_table_len: the resistance table length
 * @cur_1000ma_adc: ADC value corresponding to 1000 mA
 * @vol_1000mv_adc: ADC value corresponding to 1000 mV
 * @calib_resist: the real resistance of coulomb counter chip in uOhm
 * @comp_resistance: the coulomb counter internal and the board ground resistance
 * @index: record temp_buff array index
 * @temp_buff: record the battery temperature for each measurement
 * @bat_temp: the battery temperature
 * @cap_table: capacity table with corresponding ocv
 * @temp_table: the NTC voltage table with corresponding battery temperature
 * @cap_temp_table: the capacity table with corresponding temperature
 * @resist_table: resistance percent table with corresponding temperature
 */
struct sc27xx_fgu_data {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *battery;
	u32 base;
	struct mutex lock;
	struct gpio_desc *gpiod;
	struct iio_channel *channel;
	struct iio_channel *charge_chan;
	bool bat_present;
	int internal_resist;
	int total_mah;
	int design_mah;
	int init_cap;
	int alarm_cap;
	int boot_cap;
	int normal_temp_cap;
	int init_clbcnt;
	int uusoc_mah;
	int init_mah;
	int cc_mah;
	int max_volt_uv;
	int min_volt_uv;
	int boot_volt_uv;
	int table_len;
	int temp_table_len;
	int cap_table_len;
	int resist_table_len;
	int cap_calib_dens_ocv_table_len;
	int cur_1000ma_adc;
	int vol_1000mv_adc;
	int calib_resist;
	int first_calib_volt;
	int first_calib_cap;
	int uusoc_vbat;
	unsigned int comp_resistance;
	int batt_ovp_threshold;
	int index;
	int ocv_uv;
	int batt_mv;
	int temp_buff[SC27XX_FGU_TEMP_BUFF_CNT];
	int cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT];
	bool dischg_trend[SC27XX_FGU_DISCHG_CNT];
	int last_clbcnt;
	int cur_clbcnt;
	int bat_temp;
	bool online;
	bool is_first_poweron;
	bool is_ovp;
	bool invalid_pocv;
	u32 chg_type;
	struct sc27xx_fgu_track_capacity track;
	struct power_supply_battery_ocv_table *cap_table;
	struct power_supply_vol_temp_table *temp_table;
	struct power_supply_capacity_temp_table *cap_temp_table;
	struct power_supply_resistance_temp_table *resist_table;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	int chg_sts;
	const struct sc27xx_fgu_variant_data *pdata;
	struct sc27xx_fgu_debug_info debug_info;
	struct sc27xx_fgu_sleep_capacity_calibration slp_cap_calib;
	density_ocv_table *cap_calib_dens_ocv_table;

	struct sc27xx_fgu_sysfs *sysfs;
	struct delayed_work fgu_work;
	struct delayed_work cap_track_work;

	/* typec extcon */
	struct extcon_dev *edev;
	struct notifier_block pd_swap_notify;
	struct notifier_block extcon_nb;
	struct work_struct typec_extcon_work;
	bool is_sink;
	bool use_typec_extcon;

	/* multi resistance */
	int *target_rbat_table;
	int **rbat_table;
	int rabat_table_len;
	int *rbat_temp_table;
	int rbat_temp_table_len;
	int *rbat_ocv_table;
	int rbat_ocv_table_len;
	bool support_multi_resistance;
	bool support_debug_log;

	/* boot capacity calibration */
	bool support_boot_calib;
	s64 shutdown_rtc_time;

	/* charge cycle */
	int charge_cycle;
	bool support_charge_cycle;

	/* basp */
	bool support_basp;
	int basp_volt_uv;
	struct sprd_battery_ocv_table **basp_ocv_table;
	int basp_ocv_table_len;
	int *basp_full_design_table;
	int basp_full_design_table_len;
	int *basp_voltage_max_table;
	int basp_voltage_max_table_len;

	struct completion probe_init;
	bool probe_initialized;
};

struct sc27xx_fgu_variant_data {
	u32 module_en;
	u32 clk_en;
	u32 fgu_cal;
	u32 fgu_cal_shift;
};

static const struct sc27xx_fgu_variant_data sc2731_info = {
	.module_en = SC27XX_MODULE_EN0,
	.clk_en = SC27XX_CLK_EN0,
	.fgu_cal = SC2731_FGU_CAL,
	.fgu_cal_shift = SC2731_FGU_CAL_SHIFT,
};

static const struct sc27xx_fgu_variant_data sc2730_info = {
	.module_en = SC2730_MODULE_EN0,
	.clk_en = SC2730_CLK_EN0,
	.fgu_cal = SC2730_FGU_CAL,
	.fgu_cal_shift = SC2730_FGU_CAL_SHIFT,
};

static const struct sc27xx_fgu_variant_data ump9620_info = {
	.module_en = UMP9620_MODULE_EN0,
	.clk_en = UMP9620_CLK_EN0,
	.fgu_cal = UMP9620_FGU_CAL,
	.fgu_cal_shift = UMP9620_FGU_CAL_SHIFT,
};

static const struct sc27xx_fgu_variant_data sc2720_info = {
	.module_en = SC2720_MODULE_EN0,
	.clk_en = SC2720_CLK_EN0,
	.fgu_cal = SC2720_FGU_CAL,
	.fgu_cal_shift = SC2720_FGU_CAL_SHIFT,
};

static bool is_charger_mode;
static bool allow_charger_enable;

static int get_boot_mode(struct sc27xx_fgu_data *data)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	char *match;
	char result[5] = {0};
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strncmp(cmd_line, "charger", strlen("charger")) == 0)
		is_charger_mode =  true;

	match = strstr(cmd_line, "sprdboot.mode=");
	if (match) {
		memcpy(result, (match + strlen("sprdboot.mode=")), sizeof(result) - 1);
		dev_info(data->dev, "result = %s\n", result);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			allow_charger_enable = true;
	}

	dev_info(data->dev, "cmd_line = %s，allow_charger_enable = %d\n", cmd_line,
		 allow_charger_enable);

	return 0;
}

static void sc27xx_fgu_capacity_calibration(struct sc27xx_fgu_data *data, bool int_mode);
static void sc27xx_fgu_discharging_calibration(struct sc27xx_fgu_data *data, int *cap);
static int sc27xx_fgu_resistance_algo(struct sc27xx_fgu_data *data, int cur_ua, int vol_uv);

static int sc27xx_fgu_set_basp_volt(struct sc27xx_fgu_data *data, int max_volt_uv)
{
	int i, index;

	if (!data->support_basp || max_volt_uv == -1 || !data->basp_voltage_max_table ||
	    !data->basp_full_design_table || !data->basp_ocv_table)
		return 0;

	for (i = 0; i < data->basp_voltage_max_table_len; i++) {
		if (max_volt_uv >= data->basp_voltage_max_table[i])
			break;
	}

	if (i == data->basp_voltage_max_table_len)
		index = i - 1;
	else
		index = i;

	data->basp_volt_uv = data->basp_voltage_max_table[index];
	data->total_mah = data->basp_full_design_table[index]  / 1000;
	data->design_mah = data->total_mah;

	data->table_len = data->basp_ocv_table_len;
	data->cap_table = (struct power_supply_battery_ocv_table *)
		(data->basp_ocv_table[index]);

	dev_info(data->dev, "%s, basp_volt_uv = %d, basp_index = %d, max_volt_uv= %d,"
		 "total_mah = %d\n",
		 __func__, data->basp_volt_uv, index, max_volt_uv, data->total_mah);

	return 0;
}

static int sc27xx_fgu_parse_cmdline_match(struct sc27xx_fgu_data *data, char *match_str,
					  char *result, int size)
{
	struct device_node *cmdline_node = NULL;
	const char *cmdline;
	char *match, *match_end;
	int len, match_str_len, ret;

	if (!result || !match_str)
		return -EINVAL;

	memset(result, '\0', size);
	match_str_len = strlen(match_str);

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		dev_warn(data->dev, "%s failed to read bootargs\n", __func__);
		return -EINVAL;
	}

	match = strstr(cmdline, match_str);
	if (!match) {
		dev_warn(data->dev, "Mmatch: %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	match_end = strstr((match + match_str_len), " ");
	if (!match_end) {
		dev_warn(data->dev, "Match end of : %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	len = match_end - (match + match_str_len);
	if (len < 0) {
		dev_warn(data->dev, "Match cmdline :%s fail, len = %d\n", match_str, len);
		return -EINVAL;
	}

	memcpy(result, (match + match_str_len), len);

	return 0;
}

static void sc27xx_fgu_parse_shutdown_rtc_time(struct sc27xx_fgu_data *data)
{
	char result[32] = {};
	int ret;
	char *str;

	str = "charge.shutdown_rtc_time=";
	data->shutdown_rtc_time = -1;
	ret = sc27xx_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoll(result, 10, &data->shutdown_rtc_time);
		if (ret) {
			data->shutdown_rtc_time = -1;
			dev_err(data->dev, "Covert shutdown_rtc_time fail, ret = %d, result = %s",
				ret, result);
		}
	}
}

static void sc27xx_fgu_parse_charge_cycle(struct sc27xx_fgu_data *data)
{
	char result[32] = {};
	int ret;
	char *str;

	str = "charge.charge_cycle=";
	data->charge_cycle = -1;
	ret = sc27xx_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &data->charge_cycle);
		if (ret) {
			data->charge_cycle = -1;
			dev_err(data->dev, "Covert charge_cycle fail, ret = %d, result = %s\n",
				ret, result);
		}
	}
}

static void sc27xx_fgu_parse_basp(struct sc27xx_fgu_data *data)
{
	char result[32] = {};
	int ret;
	char *str;

	str = "charge.basp=";
	data->basp_volt_uv = -1;
	ret = sc27xx_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &data->basp_volt_uv);
		if (ret) {
			data->basp_volt_uv = -1;
			dev_err(data->dev, "Covert basp fail, ret = %d, result = %s\n",
				ret, result);
		}

		ret = sc27xx_fgu_set_basp_volt(data, data->basp_volt_uv);
		if (ret)
			dev_err(data->dev, "Fail to set basp volt\n");
	}
}

static void sc27xx_fgu_parse_learned_mah(struct sc27xx_fgu_data *data)
{
	char result[32] = {};
	int ret;
	char *str;

	str = "charge.total_mah=";
	data->track.learned_mah = -1;
	ret = sc27xx_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &data->track.learned_mah);
		if (ret) {
			data->track.learned_mah = -1;
			dev_err(data->dev, "Covert learned_mah fail, ret = %d, result = %s\n",
				ret, result);
		}
	}
}

static void sc27xx_fgu_parse_cmdline(struct sc27xx_fgu_data *data)
{
	/* parse shutdown rtc time */
	if (data->support_boot_calib)
		sc27xx_fgu_parse_shutdown_rtc_time(data);

	/* parse charge cycle */
	if (data->support_charge_cycle)
		sc27xx_fgu_parse_charge_cycle(data);

	/* parse basp */
	if (data->support_basp)
		sc27xx_fgu_parse_basp(data);

	/* parse learned total mah */
	if (data->track.cap_tracking)
		sc27xx_fgu_parse_learned_mah(data);

	dev_info(data->dev, "shutdown_rtc_time = %lld, charge_cycle = %d, basp = %d, "
		 "learned_mah = %d\n",
		 data->shutdown_rtc_time, data->charge_cycle, data->basp_volt_uv,
		 data->track.learned_mah);
}

static int sc27xx_fgu_get_rtc_time(struct sc27xx_fgu_data *data, s64 *time)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int ret;

	rtc = alarmtimer_get_rtcdev();
	if (!rtc) {
		dev_err(data->dev, "NO RTC dev!!!\n");
		return -EINVAL;
	}

	ret = rtc_read_time(rtc, &tm);
	if (ret) {
		dev_err(data->dev, "failed to read rtc time, ret = %d\n", ret);
		return ret;
	}

	*time = rtc_tm_to_time64(&tm);

	return 0;
}

static inline int sc27xx_fgu_adc2current(struct sc27xx_fgu_data *data, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST(adc * 1000, data->cur_1000ma_adc);
}

static inline int sc27xx_fgu_current2adc(struct sc27xx_fgu_data *data, int cur_ma)
{
	return DIV_ROUND_CLOSEST(cur_ma * data->cur_1000ma_adc, 1000);
}

static inline int sc27xx_fgu_adc2voltage(struct sc27xx_fgu_data *data, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST(adc * 1000, data->vol_1000mv_adc);
}

static inline int sc27xx_fgu_voltage2adc(struct sc27xx_fgu_data *data, int vol_mv)
{
	return DIV_ROUND_CLOSEST(vol_mv * data->vol_1000mv_adc, 1000);
}

static inline int sc27xx_fgu_cap2mah(struct sc27xx_fgu_data *data, int cap)
{
	return DIV_ROUND_CLOSEST(data->total_mah * cap, SC27XX_FGU_FCC_PERCENT);
}

static int sc27xx_fgu_cap2clbcnt(struct sc27xx_fgu_data *data, int capacity)
{
	/*
	 * Get current capacity (mAh) = battery total capacity (mAh) *
	 * current capacity percent (capacity / 100).
	 */
	int cur_mah = sc27xx_fgu_cap2mah(data, capacity);

	/*
	 * Convert current capacity (mAh) to coulomb counter according to the
	 * formula: 1 mAh =3.6 coulomb.
	 */
	return DIV_ROUND_CLOSEST(cur_mah * 36 * data->cur_1000ma_adc * SC27XX_FGU_SAMPLE_HZ, 10);
}

static int sc27xx_fgu_clbcnt2mah(struct sc27xx_fgu_data *data, int clbcnt)
{
	/*
	 * Convert coulomb counter to delta capacity (mAh), and set multiplier
	 * as 10 to improve the precision.
	 * formula: 1 mAh =3.6 coulomb
	 */
	int mah = DIV_ROUND_CLOSEST(clbcnt * 10, 36 * SC27XX_FGU_SAMPLE_HZ);

	if (mah > 0)
		mah = mah + data->cur_1000ma_adc / 2;
	else
		mah = mah - data->cur_1000ma_adc / 2;

	mah = (int)div_s64(mah, data->cur_1000ma_adc);

	return mah;
}

static int sc27xx_fgu_ocv2cap(struct power_supply_battery_ocv_table *table,
			      int table_len, int ocv_uv)
{
	int i, cap;

	for (i = 0; i < table_len; i++) {
		if (ocv_uv > table[i].ocv)
			break;
	}

	if (i > 0 && i < table_len) {
		cap = interpolate(ocv_uv,
				  table[i].ocv,
				  table[i].capacity * 10,
				  table[i - 1].ocv,
				  table[i - 1].capacity * 10);
	} else if (i == 0) {
		cap = table[0].capacity * 10;
	} else {
		cap = table[table_len - 1].capacity * 10;
	}

	return cap;
}

static int sc27xx_fgu_cap2ocv(struct power_supply_battery_ocv_table *table,
			      int table_len, int cap)
{
	int i, ocv_uv;

	for (i = 0; i < table_len; i++) {
		if (cap > table[i].capacity * 10)
			break;
	}

	if (i > 0 && i < table_len) {
		ocv_uv = interpolate(cap,
				     table[i].capacity * 10,
				     table[i].ocv,
				     table[i - 1].capacity * 10,
				     table[i - 1].ocv);
	} else if (i == 0) {
		ocv_uv = table[0].ocv;
	} else {
		ocv_uv = table[table_len - 1].ocv;
	}

	return ocv_uv;
}

static int sc27xx_fgu_vol2temp(struct power_supply_vol_temp_table *table,
			       int table_len, int vol_uv)
{
	int i, temp;

	for (i = 0; i < table_len; i++) {
		if (vol_uv > table[i].vol)
			break;
	}

	if (i > 0 && i < table_len) {
		temp = interpolate(vol_uv,
				   table[i].vol,
				   table[i].temp,
				   table[i - 1].vol,
				   table[i - 1].temp);
	} else if (i == 0) {
		temp = table[0].temp;
	} else {
		temp = table[table_len - 1].temp;
	}

	return temp - 1000;
}

static int sc27xx_fgu_temp2resist_ratio(struct power_supply_resistance_temp_table *table,
					int table_len, int bat_temp)
{
	int i, scale_ratio;

	for (i = 0; i < table_len; i++) {
		if (bat_temp > table[i].temp * 10)
			break;
	}

	if (i > 0 && i < table_len) {
		scale_ratio = interpolate(bat_temp,
					  table[i].temp * 10,
					  table[i].resistance,
					  table[i - 1].temp * 10,
					  table[i - 1].resistance);
	} else if (i == 0) {
		scale_ratio = table[0].resistance;
	} else {
		scale_ratio = table[table_len - 1].resistance;
	}

	return scale_ratio;
}

static int sc27xx_fgu_temp2cap(struct power_supply_capacity_temp_table *table,
			       int table_len, int temp)
{
	int i, capacity;

	for (i = 0; i < table_len; i++) {
		if (temp > table[i].temp * 10)
			break;
	}

	if (i > 0 && i < table_len) {
		capacity = interpolate(temp,
				       table[i].temp * 10,
				       table[i].cap * 10,
				       table[i - 1].temp * 10,
				       table[i - 1].cap * 10);
	} else if (i == 0) {
		capacity = table[0].cap * 10;
	} else {
		capacity = table[table_len - 1].cap * 10;
	}

	return capacity;
}

/* @val: value of battery voltage in mV*/
static int sc27xx_fgu_get_vbat_now(struct sc27xx_fgu_data *data, int *val)
{
	int ret, vol_adc = 0;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_VOLTAGE, &vol_adc);
	if (ret)
		return ret;

	/*
	* It is ADC values reading from registers which need to convert to
	* corresponding voltage values.
	*/
	*val = sc27xx_fgu_adc2voltage(data, vol_adc);

	return 0;
}

static void sc27xx_fgu_capacity_loss_by_temperature(struct sc27xx_fgu_data *data, int *cap)
{
	int temp_cap, ret;

	if (data->cap_table_len > 0) {
		temp_cap = sc27xx_fgu_temp2cap(data->cap_temp_table,
					       data->cap_table_len,
					       data->bat_temp);
		/*
		 * Battery capacity at different temperatures, we think
		 * the change is linear, the follow the formula: y = ax + k
		 *
		 * for example: display 100% at 25 degrees need to display
		 * 100% at -10 degrees, display 10% at 25 degrees need to
		 * display 0% at -10 degrees, substituting the above special
		 * points will deduced follow formula.
		 * formula 1:
		 * Capacity_Delta = 100 - Capacity_Percentage(T1)
		 * formula 2:
		 * Capacity_temp = (Capacity_Percentage(current) -
		 * Capacity_Delta) * 100 /(100 - Capacity_Delta)
		 */
		*cap = DIV_ROUND_CLOSEST((*cap + temp_cap - 1000) * 1000, temp_cap);
		if (*cap < 0) {
			dev_info(data->dev, "%s Capacity_temp < 0, adjust !!!\n", __func__);
			*cap = 0;
		} else if (*cap > SC27XX_FGU_FCC_PERCENT) {
			dev_info(data->dev, "%s Capacity_temp > 1000, adjust !!!\n", __func__);
			*cap = SC27XX_FGU_FCC_PERCENT;
		}

		if (*cap <= 5) {
			ret =  sc27xx_fgu_get_vbat_now(data, &data->batt_mv);
			if (ret) {
				dev_err(data->dev, "get battery vol error.\n");
				return;
			}

			if (data->batt_mv > SC27XX_FGU_LOW_VBAT_REGION)
				*cap = 5;
		}
	}
}

/* @val: average value of battery current in mA */
static int sc27xx_fgu_get_current_avg(struct sc27xx_fgu_data *data, int *val)
{
	int ret, cur_adc = 0;
	u32 i;

	*val = 0;

	for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur_adc);
		if (ret)
			return ret;
		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding current values (unit mA).
		 */
		*val += sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);
	}

	*val /= 8;

	return 0;
}

static int sc27xx_fgu_set_clbcnt(struct sc27xx_fgu_data *data, int clbcnt)
{
	int ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_CLBCNT_SETL,
				 SC27XX_FGU_CLBCNT_MASK, clbcnt);
	if (ret)
		return ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_CLBCNT_SETH,
				 SC27XX_FGU_CLBCNT_MASK,
				 clbcnt >> SC27XX_FGU_CLBCNT_SHIFT);
	if (ret)
		return ret;

	return regmap_update_bits(data->regmap, data->base + SC27XX_FGU_START,
				  SC27XX_WRITE_SELCLB_EN, SC27XX_WRITE_SELCLB_EN);
}

static int sc27xx_fgu_get_clbcnt(struct sc27xx_fgu_data *data, int *clb_cnt)
{
	int ccl, cch, ret;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_VALL, &ccl);
	if (ret)
		return ret;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_VALH, &cch);
	if (ret)
		return ret;

	*clb_cnt = ccl & SC27XX_FGU_CLBCNT_MASK;
	*clb_cnt |= (cch & SC27XX_FGU_CLBCNT_MASK) << SC27XX_FGU_CLBCNT_SHIFT;

	return 0;
}

/* @val: average value of battery voltage in mV */
static int sc27xx_fgu_get_vbat_avg(struct sc27xx_fgu_data *data, int *val)
{
	int ret, i;
	u32 vol_adc = 0;

	*val = 0;
	for (i = 0; i < SC27XX_FGU_VOLTAGE_BUFF_CNT; i++) {
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_VOLTAGE_BUF + i * 4,
				  &vol_adc);
		if (ret)
			return ret;

		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding voltage values.
		 */
		*val += sc27xx_fgu_adc2voltage(data, vol_adc);
	}

	*val /= 8;

	return 0;
}

/* @val: value of battery current in mA*/
static int sc27xx_fgu_get_current_now(struct sc27xx_fgu_data *data, int *val)
{
	int ret;
	u32 cur_adc = 0;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CURRENT, &cur_adc);
	if (ret)
		return ret;

	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding current values (unit mA).
	 */
	*val = sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);

	return 0;
}

static void sc27xx_fgu_adjust_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret;

	dev_dbg(data->dev, "%s:line%d: cap = %d\n", __func__, __LINE__, cap);

	data->init_cap = cap;
	ret = sc27xx_fgu_get_clbcnt(data, &data->init_clbcnt);
	if (ret)
		dev_err(data->dev, "failed to get init clbcnt\n");
}

/* @val: value of battery ocv in mV*/
static int sc27xx_fgu_get_vbat_ocv(struct sc27xx_fgu_data *data, int *val)
{
	int vol_mv, cur_ma, resistance, scale_ratio, ret;

	ret = sc27xx_fgu_get_vbat_now(data, &vol_mv);
	if (ret)
		return ret;

	ret = sc27xx_fgu_get_current_now(data, &cur_ma);
	if (ret)
		return ret;

	if (data->support_multi_resistance) {
		resistance = sc27xx_fgu_resistance_algo(data, cur_ma * 1000, vol_mv * 1000);
		goto resistance_algo;
	}

	resistance = data->internal_resist;
	if (data->resist_table_len > 0) {
		scale_ratio = sc27xx_fgu_temp2resist_ratio(data->resist_table,
							   data->resist_table_len,
							   data->bat_temp);
		resistance = data->internal_resist * scale_ratio / 100;
	}

resistance_algo:
	*val = vol_mv - cur_ma * (resistance + SC27XX_FGU_RBAT_CMP_MOH) / 1000;

	return 0;
}

static int sc27xx_fgu_get_basp_volt(struct sc27xx_fgu_data *data, int *max_volt_uv)
{
	int ret = 0;

	*max_volt_uv = data->basp_volt_uv;

	return ret;
}

static void sc27xx_fgu_dump_battery_info(struct sc27xx_fgu_data *data, char *str)
{
	int i, j;

	dev_info(data->dev, "%s, ocv_table_len = %d, temp_table_len = %d, rabat_table_len = %d, "
		 "basp_ocv_table_len = %d, basp_full_design_table_len = %d, "
		 "basp_voltage_max_table_len = %d, track.end_vol = %d, track.end_cur = %d, "
		 "first_calib_volt = %d, first_calib_cap = %d, total_mah = %d, max_volt_uv = %d, "
		 "internal_resist = %d, min_volt_uv = %d\n",
		 str, data->rbat_ocv_table_len, data->rbat_temp_table_len,
		 data->rabat_table_len, data->basp_ocv_table_len,
		 data->basp_full_design_table_len, data->basp_voltage_max_table_len,
		 data->track.end_vol, data->track.end_cur, data->first_calib_volt,
		 data->first_calib_cap, data->total_mah, data->max_volt_uv,
		 data->internal_resist, data->min_volt_uv);

	if (data->rbat_temp_table_len > 0) {
		for (i = 0; i < data->rbat_temp_table_len; i++)
			dev_info(data->dev, "%s, internal_resistance_temp[%d] = %d\n",
				 str, i, data->rbat_temp_table[i]);
	}

	if (data->rbat_ocv_table_len > 0) {
		for (i = 0; i < data->rbat_ocv_table_len; i++)
			dev_info(data->dev, "%s, battery_internal_resistance_ocv_table[%d] = %d\n",
				 str, i, data->rbat_ocv_table[i]);
	}

	for (i = 0; i < data->rbat_temp_table_len; i++) {
		for (j = 0; j < data->rabat_table_len; j++)
			dev_info(data->dev, "%s, resistance_table[%d][%d] = %d\n",
				 str, i, j, data->rbat_table[i][j]);
	}

	if (data->target_rbat_table) {
		for (i = 0; i < data->rabat_table_len; i++)
			dev_info(data->dev, "%s, target_rbat_table[%d] = %d\n",
				 str, i, data->target_rbat_table[i]);
	}

	if (data->basp_full_design_table) {
		for (i = 0; i < data->basp_full_design_table_len; i++)
			dev_info(data->dev, "%s, basp_full_design_table[%d] = %d\n",
				 str, i, data->basp_full_design_table[i]);
	}

	if (data->basp_voltage_max_table) {
		for (i = 0; i < data->basp_voltage_max_table_len; i++)
			dev_info(data->dev, "%s, basp_voltage_max_table[%d] = %d\n",
				 str, i, data->basp_voltage_max_table[i]);
	}

	if (data->basp_ocv_table) {
		for (i = 0; i < data->basp_voltage_max_table_len; i++) {
			for (j = 0; j < data->basp_ocv_table_len; j++)
				dev_info(data->dev, "%s, basp_ocv_table[%d][%d] = (%d, %d)\n",
					 str, i, j, data->basp_ocv_table[i][j].ocv,
					 data->basp_ocv_table[i][j].capacity);
		}
	}

	if (data->cap_table) {
		for (i = 0; i < data->table_len; i++)
			dev_info(data->dev, "cap_table[%d].ocv = %d, cap_table[%d].cap = %d\n",
				 i, data->cap_table[i].ocv, i, data->cap_table[i].capacity);
	}

}

static void sc27xx_fgu_dump_info(struct sc27xx_fgu_data *data)
{
	dev_info(data->dev, "init_cap = %d, init_mah = %d, init_clbcnt = %d, cur_clbcnt = %d, "
		 "normal_cap = %d, data->cc_mah = %d, Tbat = %d, uusoc_vbat = %d, uusoc_mah = %d, "
		 "cur_1000ma_adc = %d, vol_1000mv_adc = %d, track_sts = %d\n",
		 data->init_cap, data->init_mah, data->init_clbcnt, data->cur_clbcnt,
		 data->normal_temp_cap, data->cc_mah, data->bat_temp, data->uusoc_vbat,
		 data->uusoc_mah,  data->cur_1000ma_adc, data->vol_1000mv_adc, data->track.state);
}

static bool sc27xx_fgu_is_first_poweron(struct sc27xx_fgu_data *data)
{
	int ret;
	u32 status = 0, cap, mode;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_USER_AREA_STATUS, &status);
	if (ret)
		return false;

	/*
	 * We use low 4 bits to save the last battery capacity and high 12 bits
	 * to save the system boot mode.
	 */
	mode = (status & SC27XX_FGU_MODE_AREA_MASK) >> SC27XX_FGU_MODE_AREA_SHIFT;
	cap = status & SC27XX_FGU_CAP_AREA_MASK;

	/*
	 * When FGU has been powered down, the user area registers became
	 * default value (0xffff), which can be used to valid if the system is
	 * first power on or not.
	 */
	if (mode == SC27XX_FGU_FIRST_POWERON || cap == SC27XX_FGU_DEFAULT_CAP)
		return true;

	return false;
}

static bool sc27xx_fgu_is_in_low_energy_dens(struct sc27xx_fgu_data *data, int ocv_uv,
					     density_ocv_table *table, int len)
{
	bool is_matched = false;
	int i;

	if (len == 0) {
		dev_warn(data->dev, "energy density ocv table len is 0 !!!!\n");
		return is_matched;
	}

	for (i = 0; i < len; i++) {
		if (ocv_uv > table[i].engy_dens_ocv_lo &&
		    ocv_uv < table[i].engy_dens_ocv_hi) {
			dev_info(data->dev, "low ernergy dens matched, vol = %d\n", ocv_uv);
			is_matched = true;
			break;
		}
	}

	if (!is_matched)
		dev_info(data->dev, "ocv_uv[%d] is out of dens range\n", ocv_uv);

	return is_matched;
}

static int sc27xx_fgu_save_boot_mode(struct sc27xx_fgu_data *data, int boot_mode)
{
	int ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_MODE_AREA_MASK,
				 SC27XX_FGU_MODE_AREA_MASK);
	if (ret) {
		dev_err(data->dev, "%d Failed to write mode user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET,
				 SC27XX_FGU_MODE_AREA_MASK,
				 boot_mode << SC27XX_FGU_MODE_AREA_SHIFT);
	if (ret) {
		dev_err(data->dev, "Failed to write mode user set, ret = %d\n", ret);
		return ret;
	};

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	/*
	 * According to the datasheet, we should set the USER_AREA_CLEAR to 0 to
	 * make the user area data available, otherwise we can not save the user
	 * area data.
	 */
	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_MODE_AREA_MASK, 0);
	if (ret) {
		dev_err(data->dev, "%d Failed to write mode user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	return ret;

}

static int sc27xx_fgu_save_last_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret;
	u32 value;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_CAP_AREA_MASK,
				 SC27XX_FGU_CAP_AREA_MASK);
	if (ret) {
		dev_err(data->dev, "%d Failed to write user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	value = (cap / 10) & SC27XX_FGU_CAP_INTEGER_MASK;
	value |= ((cap % 10) & SC27XX_FGU_CAP_DECIMAL_MASK) << SC27XX_FGU_CAP_DECIMAL_SHIFT;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET,
				 SC27XX_FGU_CAP_AREA_MASK, value);
	if (ret) {
		dev_err(data->dev, "Failed to write user set, ret = %d\n", ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	/*
	 * According to the datasheet, we should set the USER_AREA_CLEAR to 0 to
	 * make the user area data available, otherwise we can not save the user
	 * area data.
	 */
	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_CAP_AREA_MASK, 0);
	if (ret) {
		dev_err(data->dev, "%d Failed to write user clr, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	return ret;
}

/*
 * We get the percentage at the current temperature by multiplying
 * the percentage at normal temperature by the temperature conversion
 * factor, and save the percentage before conversion in the rtc register
 */
static int sc27xx_fgu_save_normal_temperature_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret = 0;
	u32 value;

	if (data->normal_temp_cap == SC27XX_FGU_MAGIC_NUMBER) {
		dev_info(data->dev, "normal_cap = %#x\n", data->normal_temp_cap);
		return ret;
	}

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR1,
				 SC27XX_FGU_CAP_AREA_MASK,
				 SC27XX_FGU_CAP_AREA_MASK);
	if (ret) {
		dev_err(data->dev, "%d Failed to write user clr1, ret = %d\n", __LINE__, ret);
		return ret;
	}
	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	value = (cap / 10) & SC27XX_FGU_CAP_INTEGER_MASK;
	value |= ((cap % 10) & SC27XX_FGU_CAP_DECIMAL_MASK) << SC27XX_FGU_CAP_DECIMAL_SHIFT;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET1,
				 SC27XX_FGU_CAP_AREA_MASK, value);
	if (ret) {
		dev_err(data->dev, "Failed to write user set1, ret = %d\n", ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR1,
				 SC27XX_FGU_CAP_AREA_MASK, 0);
	if (ret) {
		dev_err(data->dev, "%d Failed to write user clr1, ret = %d\n", __LINE__, ret);
		return ret;
	}

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	return ret;
}

static int sc27xx_fgu_read_normal_temperature_cap(struct sc27xx_fgu_data *data, int *cap)
{
	int ret;
	unsigned int value;

	ret = regmap_read(data->regmap,
			  data->base + SC27XX_FGU_USER_AREA_STATUS1, &value);
	if (ret)
		return ret;

	*cap = (value & SC27XX_FGU_CAP_INTEGER_MASK) * 10;
	*cap += (value >> SC27XX_FGU_CAP_DECIMAL_SHIFT) & SC27XX_FGU_CAP_DECIMAL_MASK;

	return 0;
}

static int sc27xx_fgu_read_last_cap(struct sc27xx_fgu_data *data, int *cap)
{
	int ret;
	unsigned int value = 0;

	ret = regmap_read(data->regmap,
			  data->base + SC27XX_FGU_USER_AREA_STATUS, &value);
	if (ret)
		return ret;

	*cap = (value & SC27XX_FGU_CAP_INTEGER_MASK) * 10;
	*cap += (value >> SC27XX_FGU_CAP_DECIMAL_SHIFT) & SC27XX_FGU_CAP_DECIMAL_MASK;

	return 0;
}

static void sc27xx_fgu_calc_charge_cycle(struct sc27xx_fgu_data *data, int cap, int *fgu_cap)
{
	int delta_cap;

	if (!data->support_charge_cycle)
		return;

	if (*fgu_cap == SC27XX_FGU_MAGIC_NUMBER)
		*fgu_cap = cap;

	delta_cap = cap - *fgu_cap;

	if (data->support_debug_log)
		dev_info(data->dev, "%s: delta_cap = %d, fgu_cap = %d, cap = %d\n",
			 __func__, delta_cap, *fgu_cap, cap);

	*fgu_cap = cap;

	/*
	 * Formula:
	 * charge_cycle(0.001 cycle) = accumulate_cap  * 1000 /  SC27XX_FGU_FCC_PERCENT
	 */
	if (delta_cap > 0)
		data->charge_cycle += delta_cap * 1000 / SC27XX_FGU_FCC_PERCENT;
}

static int sc27xx_fgu_get_boot_voltage(struct sc27xx_fgu_data *data, int *pocv_uv)
{
	int vol_adc, cur_adc, oci_ma, ret, ocv_mv, fgu_sts, vol_mv;

	/*
	 * After system booting on, the SC27XX_FGU_CLBCNT_QMAXL register saved
	 * the first sampled open circuit current.
	 */
	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_QMAXL, &cur_adc);
	if (ret) {
		dev_err(data->dev, "Failed to read CLBCNT_QMAXL, ret = %d\n", ret);
		return ret;
	}

	cur_adc <<= 1;
	oci_ma = sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);

	/*
	 * Should get the OCV from SC27XX_FGU_POCV register at the system
	 * beginning. It is ADC values reading from registers which need to
	 * convert the corresponding voltage.
	 */
	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_POCV, &vol_adc);
	if (ret) {
		dev_err(data->dev, "Failed to read FGU_POCV, ret = %d\n", ret);
		return ret;
	}

	vol_mv = sc27xx_fgu_adc2voltage(data, vol_adc);
	*pocv_uv = vol_mv * 1000 - oci_ma * data->internal_resist;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_STATUS, &fgu_sts);
	if (ret) {
		dev_err(data->dev, "Failed to read FGU_STATUS, ret = %d\n", ret);
		return ret;
	}

	data->invalid_pocv = !!(fgu_sts & SC27XX_FGU_STATUS_INVALID_POCV);
	if (vol_mv < SC27XX_FGU_POCV_VOLT_THRESHOLD || data->invalid_pocv) {
		dev_info(data->dev, "pocv is %s\n", data->invalid_pocv ? "invalid" : "valid");
		ret = sc27xx_fgu_get_vbat_ocv(data, &ocv_mv);
		if (ret) {
			dev_err(data->dev, "Failed to read volt, ret = %d\n", ret);
			return ret;
		}
		*pocv_uv = ocv_mv * 1000;
	}
	dev_info(data->dev, "oci_ma = %d, vol_mv = %d, pocv = %d\n", oci_ma, vol_mv, *pocv_uv);

	return 0;
}

static void sc27xx_fgu_boot_cap_calibration(struct sc27xx_fgu_data *data,
					    int pocv_cap, int pocv_uv, int *cap)
{
	s64 cur_time, shutdown_time;
	int ret;

	if (!data->support_boot_calib) {
		dev_warn(data->dev, "Boot calib: not support boot calibration !!!!\n");
		return;
	}

	if (data->shutdown_rtc_time == 0 || data->shutdown_rtc_time == -1 ||
	    data->shutdown_rtc_time < SC27XX_FGU_MISCDATA_RTC_TIME) {
		dev_err(data->dev, "Boot calib: shutdown_rtc_time = %lld not meet\n",
			data->shutdown_rtc_time);
		return;
	}

	if (!sc27xx_fgu_is_in_low_energy_dens(data, pocv_uv, data->cap_calib_dens_ocv_table,
					      data->cap_calib_dens_ocv_table_len)) {
		dev_warn(data->dev, "Boot calib: pocv_uv is not in low energy dens !!!!\n");
		return;
	}

	if (data->bat_temp < SC27XX_FGU_CAP_CALIB_TEMP_LOW ||
		data->bat_temp > SC27XX_FGU_CAP_CALIB_TEMP_HI) {
		dev_err(data->dev, "Boot calib: temp = %d out range\n", data->bat_temp);
		return;
	}

	ret = sc27xx_fgu_get_rtc_time(data, &cur_time);
	if (ret)
		return;

	if (cur_time < SC27XX_FGU_MISCDATA_RTC_TIME) {
		dev_err(data->dev, "Boot calib: current rtc time = %lld less than %d\n",
			cur_time, SC27XX_FGU_MISCDATA_RTC_TIME);
		return;
	}

	shutdown_time = cur_time - data->shutdown_rtc_time;
	if (shutdown_time < SC27XX_FGU_SHUTDOWN_TIME) {
		dev_err(data->dev, "Boot calib: shutdown time = %lld not meet\n", shutdown_time);
		return;
	}

	data->track.pocv_info.valid = true;
	data->track.pocv_info.ocv_uv = pocv_uv;
	data->track.pocv_info.ocv_rtc_time = cur_time;


	dev_info(data->dev, "Boot calib: pocv_cap = %d, *cap = %d\n", pocv_cap, *cap);

	if (pocv_cap > *cap + 30)
		*cap += (pocv_cap - *cap - 30);
	else if (pocv_cap < *cap - 30)
		*cap -= (*cap - pocv_cap - 30);
}

/*
 * When system boots on, we can not read battery capacity from coulomb
 * registers, since now the coulomb registers are invalid. So we should
 * calculate the battery open circuit voltage, and get current battery
 * capacity according to the capacity table.
 */
static int sc27xx_fgu_get_boot_capacity(struct sc27xx_fgu_data *data, int *cap)
{
	int pocv_uv, ret, pocv_cap;
	bool is_first_poweron = sc27xx_fgu_is_first_poweron(data);

	ret = sc27xx_fgu_get_boot_voltage(data, &pocv_uv);
	if (ret) {
		dev_err(data->dev, "Failed to get boot voltage, ret = %d\n", ret);
		return ret;
	}
	data->boot_volt_uv = pocv_uv;

	/*
	 * Parse the capacity table to look up the correct capacity percent
	 * according to current battery's corresponding OCV values.
	 */
	pocv_cap = sc27xx_fgu_ocv2cap(data->cap_table, data->table_len, pocv_uv);

	/*
	 * If system is not the first power on, we should use the last saved
	 * battery capacity as the initial battery capacity. Otherwise we should
	 * re-calculate the initial battery capacity.
	 */
	if (!is_first_poweron) {
		ret = sc27xx_fgu_read_last_cap(data, cap);
		if (ret) {
			dev_err(data->dev, "Failed to read last cap, ret = %d\n", ret);
			return ret;
		}

		data->boot_cap = *cap;
		ret = sc27xx_fgu_read_normal_temperature_cap(data, cap);
		if (ret) {
			dev_err(data->dev, "Failed to read normal temperature cap, ret = %d\n", ret);
			sc27xx_fgu_boot_cap_calibration(data, pocv_cap, pocv_uv, cap);
			return ret;
		}

		if (*cap == SC27XX_FGU_DEFAULT_CAP || *cap == SC27XX_FGU_RTC2_RESET_VALUE) {
			*cap = data->boot_cap;
			sc27xx_fgu_boot_cap_calibration(data, pocv_cap, pocv_uv, cap);
			ret = sc27xx_fgu_save_normal_temperature_cap(data, *cap);
			if (ret < 0)
				dev_err(data->dev, "Failed to initialize fgu user area status1 register\n");
		} else {
			sc27xx_fgu_boot_cap_calibration(data, pocv_cap, pocv_uv, cap);
		}

		data->normal_temp_cap = *cap;
		ret = sc27xx_fgu_save_normal_temperature_cap(data, data->normal_temp_cap);
		if (ret) {
			dev_err(data->dev, "Failed to save normal temperature capacity, ret = %d\n", ret);
			return ret;
		}

		dev_info(data->dev, "init: boot_cap = %d, normal_cap = %d\n", data->boot_cap, *cap);

		return sc27xx_fgu_save_boot_mode(data, SC27XX_FGU_NORMAL_POWERON);
	}

	*cap = pocv_cap;
	sc27xx_fgu_capacity_loss_by_temperature(data, cap);
	data->boot_cap = *cap;
	ret = sc27xx_fgu_save_last_cap(data, *cap);
	if (ret) {
		dev_err(data->dev, "Failed to save last cap, ret = %d\n", ret);
		return ret;
	}

	data->normal_temp_cap = pocv_cap;
	ret = sc27xx_fgu_save_normal_temperature_cap(data, data->normal_temp_cap);
	if (ret) {
		dev_err(data->dev, "Failed to save normal temperature capacity, ret = %d\n", ret);
		return ret;
	}

	data->is_first_poweron = true;
	dev_info(data->dev, "First_poweron: pocv_uv = %d, pocv_cap = %d, "
		 "boot_cap = %d\n", pocv_uv, pocv_cap, *cap);
	*cap = pocv_cap;

	return sc27xx_fgu_save_boot_mode(data, SC27XX_FGU_NORMAL_POWERON);
}

static int sc27xx_fgu_uusoc_algo(struct sc27xx_fgu_data *data, int *uusoc_mah)
{
	int vol_mv, cur_ma, ret, cur_avg_ma;
	int resistance_moh = 0, ocv_pzero_uv;
	int ocv_pzero_cap, ocv_pzero_mah;

	ret = sc27xx_fgu_get_vbat_now(data, &vol_mv);
	if (ret) {
		dev_info(data->dev, "UUSOC fail to get vbat, ret = %d\n", ret);
		return ret;
	}

	ret = sc27xx_fgu_get_current_now(data, &cur_ma);
	if (ret) {
		dev_info(data->dev, "UUSOC fail to get cur_ma, ret = %d\n", ret);
		return ret;
	}

	ret = sc27xx_fgu_get_current_avg(data, &cur_avg_ma);
	if (ret) {
		dev_info(data->dev, "UUSOC fail to get cur_avg_ma, ret = %d\n", ret);
		return ret;
	}

	ocv_pzero_uv = data->cap_table[data->table_len - 1].ocv;
	if (cur_avg_ma < 0) {
		resistance_moh = sc27xx_fgu_resistance_algo(data, cur_ma * 1000,
							    vol_mv * 1000);
		ocv_pzero_uv -= cur_avg_ma * resistance_moh;
	}

	ocv_pzero_cap = sc27xx_fgu_ocv2cap(data->cap_table, data->table_len, ocv_pzero_uv);

	ocv_pzero_mah = sc27xx_fgu_cap2mah(data, ocv_pzero_cap);
	*uusoc_mah = ocv_pzero_mah;

	dev_info(data->dev, "UUSOC: cur_avg_ma = %d, resistance_moh = %d, ocv_pzero_uv = %d, "
		 "ocv_pzero_cap = %d, ocv_pzero_mah = %d\n",
		 cur_avg_ma, resistance_moh, ocv_pzero_uv, ocv_pzero_cap, ocv_pzero_mah);

	return 0;
}

static int sc27xx_fgu_get_capacity(struct sc27xx_fgu_data *data, int *cap)
{
	int ret, cur_clbcnt, delta_clbcnt, delta_cap;
	static int last_fgu_cap = SC27XX_FGU_MAGIC_NUMBER;

	/* Get current coulomb counters firstly */
	ret = sc27xx_fgu_get_clbcnt(data, &cur_clbcnt);
	if (ret)
		return ret;

	delta_clbcnt = cur_clbcnt - data->init_clbcnt;
	data->last_clbcnt = data->cur_clbcnt;
	data->cur_clbcnt = cur_clbcnt;

	data->cc_mah = sc27xx_fgu_clbcnt2mah(data, delta_clbcnt);

	/*
	 * Convert to capacity percent of the battery total capacity,
	 * and multiplier is 100 too.
	 */
	delta_cap = DIV_ROUND_CLOSEST(data->cc_mah * 1000, data->total_mah);
	*cap = delta_cap + data->init_cap;

	sc27xx_fgu_calc_charge_cycle(data, *cap, &last_fgu_cap);

	data->normal_temp_cap = *cap;
	if (data->normal_temp_cap < 0)
		data->normal_temp_cap = 0;
	else if (data->normal_temp_cap > 1000)
		data->normal_temp_cap = 1000;

	if (*cap < 0) {
		*cap = 0;
		dev_err(data->dev, "ERORR: normal_cap is < 0, adjust!!!\n");
		sc27xx_fgu_dump_info(data);
		data->uusoc_vbat = 0;
		sc27xx_fgu_adjust_cap(data, 0);
		return 0;
	} else if (*cap > SC27XX_FGU_FCC_PERCENT) {
		dev_info(data->dev, "normal_cap is > 1000, adjust !!!\n");
		sc27xx_fgu_dump_info(data);
		*cap = SC27XX_FGU_FCC_PERCENT;
		sc27xx_fgu_adjust_cap(data, SC27XX_FGU_FCC_PERCENT);
		return 0;
	}

	if (data->support_multi_resistance) {
		ret = sc27xx_fgu_uusoc_algo(data, &data->uusoc_mah);
		if (ret) {
			dev_info(data->dev, "Fail to get uusoc, ret = %d\n", ret);
			goto normal_cap_calc;
		}

		data->init_mah = sc27xx_fgu_cap2mah(data, data->init_cap);
		*cap = DIV_ROUND_CLOSEST((data->init_mah + data->cc_mah - data->uusoc_mah) * 1000,
					 (data->total_mah - data->uusoc_mah));

		if (*cap < 0) {
			dev_info(data->dev, "UUSOC *cap < 0, adjust !!!\n");
			sc27xx_fgu_dump_info(data);
			*cap = 0;
		} else if (*cap > SC27XX_FGU_FCC_PERCENT) {
			dev_info(data->dev, "UUSOC *cap > 1000, adjust !!!\n");
			sc27xx_fgu_dump_info(data);
			*cap = SC27XX_FGU_FCC_PERCENT;
		}

		goto capacity_calibration;
	}

normal_cap_calc:
	sc27xx_fgu_capacity_loss_by_temperature(data, cap);
capacity_calibration:
	sc27xx_fgu_capacity_calibration(data, false);

	*cap -= data->uusoc_vbat;
	if (*cap < 0) {
		dev_info(data->dev, "Capacity_temp < 0, adjust !!!\n");
		*cap = 0;
	} else if (*cap > SC27XX_FGU_FCC_PERCENT) {
		dev_info(data->dev, "Capacity_temp > 1000, adjust !!!\n");
		*cap = SC27XX_FGU_FCC_PERCENT;
	}

	sc27xx_fgu_discharging_calibration(data, cap);

	return 0;
}

static int sc27xx_fgu_resistance_algo_init_resistance(struct sc27xx_fgu_data *data,
						      int *rbat_table)
{
	int i, min;

	if (!rbat_table)
		return 0;

	min = rbat_table[0];
	for (i = 1; i < data->rabat_table_len; i++) {
		if (min >  rbat_table[i])
			min = rbat_table[i];
	}

	return min;
}

static void sc27xx_fgu_find_resistance_by_ocv(struct sc27xx_fgu_data *data,
					      int *rbat_table, int ocv, int *rbat)
{
	int i, delta_rbat;

	for (i = 0; i < data->rbat_ocv_table_len; i++) {
		if (ocv > data->rbat_ocv_table[i])
			break;
	}

	if (i == data->rbat_ocv_table_len) {
		*rbat = rbat_table[data->rbat_ocv_table_len - 1];
		return;
	} else if (i == 0) {
		*rbat = rbat_table[0];
		return;
	}

	delta_rbat =
		DIV_ROUND_CLOSEST((rbat_table[i] - rbat_table[i - 1]) *
				  (ocv - data->rbat_ocv_table[i - 1]),
				  (data->rbat_ocv_table[i] - data->rbat_ocv_table[i - 1]));
	*rbat =  rbat_table[i - 1] + delta_rbat;

	if (data->support_debug_log)
		dev_info(data->dev, "%s: i = %d, ocv = %d, rbat_table[%d]= %d, rbat_table[%d]= %d, "
			 "ocv_table[%d] = %d, cv_table[%d] = %d, delta_rbat = %d, rbat = %d\n",
			 __func__, i, ocv, i, rbat_table[i], i - 1, rbat_table[i - 1],
			 i, data->rbat_ocv_table[i], i - 1, data->rbat_ocv_table[i - 1],
			 delta_rbat, *rbat);

}

static bool sc27xx_fgu_is_resistance_valid(struct sc27xx_fgu_data *data, int *table, int len)
{
	int sum = 0, min = 0, max = 0, avg = 0, i;
	bool is_valid = true;

	if (!table)
		return false;

	min = max = table[0];
	for (i = 0; i < len; i++) {
		if (table[i] == 0)
			return false;

		if (min > table[i])
			min =  table[i];
		if (max < table[i])
			max = table[i];

		sum += table[i];
	}

	avg = DIV_ROUND_CLOSEST(sum, len);

	if (data->support_debug_log)
		dev_info(data->dev, "%s, avg = %d, min = %d, max = %d\n",
			 __func__, avg, min, max);

	if (max - min > SC27XX_FGU_RESIST_ALG_OCV_GAP_UV)
		return false;

	for (i = 0; i < len; i++) {
		if ((abs(avg - table[i]) > (SC27XX_FGU_RESIST_ALG_OCV_GAP_UV / 2))) {
			is_valid = false;
			break;
		}
	}

	return is_valid;
}

static int sc27xx_fgu_calc_ocv(int vol_uv, int cur_ua, int rbat_moh, int rbat_cmp_moh)
{
	return (vol_uv - (cur_ua / 1000) * (rbat_moh + rbat_cmp_moh));
}

static int sc27xx_fgu_resistance_algo(struct sc27xx_fgu_data *data, int cur_ua, int vol_uv)
{
	int ocv_uv = vol_uv;
	int resistance_moh, rbat_cmp_moh = SC27XX_FGU_RBAT_CMP_MOH;
	int *rbat_table;
	int i, j, sum = 0, cnt = 0;
	int resistance_moh_tab[SC27XX_FGU_RESIST_ALG_REIST_CNT] = {0};
	int ocv_tab[SC27XX_FGU_RESIST_ALG_OCV_CNT] = {0};
	bool resistance_valid = false;

	sprd_battery_find_resistance_table(data->battery,
					   data->rbat_table,
					   data->rabat_table_len,
					   data->rbat_temp_table,
					   data->rbat_temp_table_len,
					   data->bat_temp,
					   data->target_rbat_table);

	rbat_table = data->target_rbat_table;

	resistance_moh = sc27xx_fgu_resistance_algo_init_resistance(data, rbat_table);
	ocv_uv = sc27xx_fgu_calc_ocv(vol_uv, cur_ua, resistance_moh, rbat_cmp_moh);

	if (data->support_debug_log)
		dev_info(data->dev, "%s, vol_uv = %d, cur = %d, init ocv = %d, "
			 "init resistance_moh = %d, rcmp = %d\n",
			 __func__, vol_uv, cur_ua, ocv_uv, resistance_moh, rbat_cmp_moh);

	for (i = 0; i < SC27XX_FGU_RESIST_ALG_REIST_CNT; i++) {
		ocv_uv = sc27xx_fgu_calc_ocv(vol_uv, cur_ua, resistance_moh, rbat_cmp_moh);
		ocv_tab[(i % SC27XX_FGU_RESIST_ALG_OCV_CNT)] = ocv_uv;

		sc27xx_fgu_find_resistance_by_ocv(data, rbat_table, ocv_uv, &resistance_moh);
		resistance_moh_tab[i] = resistance_moh;

		resistance_valid = sc27xx_fgu_is_resistance_valid(data, ocv_tab,
								  SC27XX_FGU_RESIST_ALG_OCV_CNT);
		if (resistance_valid)
			break;
	}

	if (!resistance_valid)
		i = SC27XX_FGU_RESIST_ALG_REIST_CNT - 1;

	for (j = i; j >= 0; j--) {
		cnt++;
		sum += resistance_moh_tab[j];
		if (cnt >= SC27XX_FGU_RESIST_ALG_OCV_CNT)
			break;
	}

	resistance_moh = DIV_ROUND_CLOSEST(sum, SC27XX_FGU_RESIST_ALG_OCV_CNT);

	if (data->support_debug_log)
		dev_info(data->dev, "Get resistance_moh =  %d, resistance_valid = %d\n",
			 resistance_moh, resistance_valid);

	return resistance_moh;

}

static int sc27xx_fgu_get_charge_vol(struct sc27xx_fgu_data *data, int *val)
{
	int ret, vol_mv;

	ret = iio_read_channel_processed(data->charge_chan, &vol_mv);
	if (ret < 0)
		return ret;

	*val = vol_mv;
	return 0;
}

static int sc27xx_fgu_get_average_temp(struct sc27xx_fgu_data *data, int temp)
{
	int i, min, max;
	int sum = 0;

	if (data->temp_buff[0] == -500) {
		for (i = 0; i < SC27XX_FGU_TEMP_BUFF_CNT; i++)
			data->temp_buff[i] = temp;
	}

	if (data->index >= SC27XX_FGU_TEMP_BUFF_CNT)
		data->index = 0;

	data->temp_buff[data->index++] = temp;
	min = max = data->temp_buff[0];

	for (i = 0; i < SC27XX_FGU_TEMP_BUFF_CNT; i++) {
		if (data->temp_buff[i] > max)
			max = data->temp_buff[i];

		if (data->temp_buff[i] < min)
			min = data->temp_buff[i];

		sum += data->temp_buff[i];
	}

	sum = sum - max - min;

	return sum / (SC27XX_FGU_TEMP_BUFF_CNT - 2);
}

static int sc27xx_fgu_get_temp(struct sc27xx_fgu_data *data, int *temp)
{
	int vol_ntc_uv, vol_adc_mv, ret;

	ret = iio_read_channel_processed(data->channel, &vol_adc_mv);
	if (ret < 0)
		return ret;

	vol_ntc_uv = vol_adc_mv * 1000;
	if (data->comp_resistance) {
		int bat_current_ma, resistance_vol, calib_resistance_vol, temp_vol;

		ret = sc27xx_fgu_get_current_now(data, &bat_current_ma);
		if (ret) {
			dev_err(data->dev, "failed to get battery current\n");
			return ret;
		}

		resistance_vol = bat_current_ma * data->comp_resistance;
		resistance_vol = DIV_ROUND_CLOSEST(resistance_vol, 10);
		calib_resistance_vol = bat_current_ma * (data->calib_resist / 10);
		calib_resistance_vol =
			DIV_ROUND_CLOSEST(calib_resistance_vol, 1000) + resistance_vol;

		temp_vol = (vol_ntc_uv / 10 - resistance_vol) * calib_resistance_vol;
		temp_vol = DIV_ROUND_CLOSEST(temp_vol, (187500 - calib_resistance_vol));

		vol_ntc_uv = temp_vol * 10 + vol_ntc_uv - resistance_vol * 10;
		if (vol_ntc_uv < 0)
			vol_ntc_uv = 0;
	}

	if (data->temp_table_len > 0) {
		*temp = sc27xx_fgu_vol2temp(data->temp_table, data->temp_table_len, vol_ntc_uv);
		*temp = sc27xx_fgu_get_average_temp(data, *temp);
	} else {
		*temp = 200;
	}

	data->bat_temp = *temp;

	return 0;
}

static void sc27xx_fgu_get_health(struct sc27xx_fgu_data *data, int *health)
{
	if (data->is_ovp)
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		*health = POWER_SUPPLY_HEALTH_GOOD;
}

static int sc27xx_fgu_suspend_calib_check_temp(struct sc27xx_fgu_data *data)
{
	int ret, temp;

	ret = sc27xx_fgu_get_temp(data, &temp);
	if (ret) {
		dev_err(data->dev, "Suspend calib failed to temp, ret = %d\n", ret);
		return ret;
	}

	if (temp < SC27XX_FGU_CAP_CALIB_TEMP_LOW || temp > SC27XX_FGU_CAP_CALIB_TEMP_HI) {
		dev_err(data->dev, "Suspend calib  temp = %d out range\n", temp);
		ret = -EINVAL;
	}

	dev_info(data->dev, "%s, temp = %d\n", __func__, temp);

	return ret;
}

static int sc27xx_fgu_suspend_calib_check_relax_cnt_int(struct sc27xx_fgu_data *data)
{
	int ret = -EINVAL;
	u32 int_status;

	mutex_lock(&data->lock);
	if (data->slp_cap_calib.relax_cnt_int_ocurred) {
		data->slp_cap_calib.relax_cnt_int_ocurred = false;
		ret = 0;
		dev_info(data->dev, "RELAX_CNT_INT ocurred 1!!\n");
		goto no_relax_cnt_int;
	}

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_INT_STS, &int_status);
	if (ret) {
		dev_err(data->dev, "suspend_calib failed to get fgu interrupt status, ret = %d\n", ret);
		goto no_relax_cnt_int;
	}

	if (!(int_status & SC27XX_FGU_RELAX_CNT_STS)) {
		dev_info(data->dev, "no RELAX_CNT_INT ocurred!!\n");
		ret = -EINVAL;
		goto no_relax_cnt_int;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				SC27XX_FGU_RELAX_CNT_STS, SC27XX_FGU_RELAX_CNT_STS);
	if (ret)
		dev_err(data->dev, "failed to clear  RELAX_CNT_STS interrupt status, ret = %d\n", ret);

	dev_info(data->dev, "RELAX_CNT_INT ocurred!!\n");
	ret = 0;

no_relax_cnt_int:
	mutex_unlock(&data->lock);
	return ret;
}

static int sc27xx_fgu_suspend_calib_check_sleep_time(struct sc27xx_fgu_data *data)
{
	s64 cur_time;
	int ret;

	ret = sc27xx_fgu_get_rtc_time(data, &cur_time);
	if (ret)
		return -EINVAL;

	data->slp_cap_calib.resume_time =  cur_time;

	dev_info(data->dev, "%s, resume_time = %lld, suspend_time = %lld\n",
		 __func__, data->slp_cap_calib.resume_time, data->slp_cap_calib.suspend_time);

	if ((data->slp_cap_calib.resume_time - data->slp_cap_calib.suspend_time <
	    SC27XX_FGU_SLP_CAP_CALIB_SLP_TIME) ||
	    data->slp_cap_calib.suspend_time == 0) {
		dev_info(data->dev, "suspend time not meet: suspend_time = %lld, resume_time = %lld\n",
			 data->slp_cap_calib.suspend_time,
			 data->slp_cap_calib.resume_time);
		return -EINVAL;
	}

	return 0;
}

static int sc27xx_fgu_suspend_calib_check_sleep_cur(struct sc27xx_fgu_data *data)
{
	int mah, clbcnt, times, sleep_cur_ma = 0;

	sc27xx_fgu_get_clbcnt(data, &data->slp_cap_calib.resume_clbcnt);

	clbcnt = data->slp_cap_calib.suspend_clbcnt - data->slp_cap_calib.resume_clbcnt;
	times = (int)(data->slp_cap_calib.resume_time -  data->slp_cap_calib.suspend_time);

	mah = DIV_ROUND_CLOSEST(clbcnt * 10, 36 * SC27XX_FGU_SAMPLE_HZ);

	/* sleep_cur_ma = mah * 3600 / times, but mah may be some, and will be zero after div_s64.
	 * so we *3600 first.
	 */
	mah *= 3600;
	if (mah > 0)
		mah = mah + data->cur_1000ma_adc / 2;
	else
		mah = mah - data->cur_1000ma_adc / 2;

	mah = div_s64(mah, data->cur_1000ma_adc);

	sleep_cur_ma = mah / times;

	dev_info(data->dev, "%s, suspend_clbcnt = %d, resume_clbcnt = %d, clbcnt = %d\n",
		 __func__, data->slp_cap_calib.suspend_clbcnt,
		 data->slp_cap_calib.resume_clbcnt, clbcnt);
	dev_info(data->dev, "%s, sleep_cur_ma = %d, times = %d, clbcnt = %d, mah = %d, 1000ma_adc = %d\n",
		 __func__, sleep_cur_ma, times, clbcnt, mah, data->cur_1000ma_adc);

	if (abs(sleep_cur_ma) > data->slp_cap_calib.relax_cur_threshold) {
		dev_info(data->dev, "Sleep calib sleep current = %d, not meet conditions\n", sleep_cur_ma);
		return -EINVAL;
	}

	return 0;
}

static int sc27xx_fgu_suspend_calib_get_ocv(struct sc27xx_fgu_data *data)
{
	int ret, i, vol_mv = 0, cur_ma = 0, cur_adc = 0x7fffffff;
	u32 vol_adc = 0;

	for (i = SC27XX_FGU_VOLTAGE_BUFF_CNT - 1; i >= 0; i--) {
		vol_adc = 0;
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_VOLTAGE_BUF + i * 4,
				  &vol_adc);
		if (ret) {
			dev_info(data->dev, "Sleep calib fail to get vbat_buf[%d]\n", i);
			continue;
		}

		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding voltage values.
		 */
		vol_mv = sc27xx_fgu_adc2voltage(data, vol_adc);

		cur_adc = 0x7fffffff;
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur_adc);
		if (ret) {
			dev_info(data->dev, "Sleep calib fail to get cur_buf[%d]\n", i);
			continue;
		}

		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding current values.
		 */
		cur_ma = sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);
		if (abs(cur_ma) < data->slp_cap_calib.relax_cur_threshold) {
			dev_info(data->dev, "Sleep calib get cur[%d] = %d meet condition\n", i, cur_ma);
			break;
		}
	}

	if (vol_adc == 0 || cur_adc == 0x7fffffff) {
		dev_info(data->dev, "Sleep calib fail to get cur and vol: cur_adc = %d, vol_adc = %d\n",
			 cur_adc, vol_adc);
		return -EINVAL;
	}

	dev_info(data->dev, "Sleep calib vol = %d, cur = %d, i = %d\n", vol_mv, cur_ma, i);

	data->slp_cap_calib.resume_ocv_uv = vol_mv * 1000;

	return 0;
}

static void sc27xx_fgu_suspend_calib_cap_calib(struct sc27xx_fgu_data *data)
{
	data->slp_cap_calib.resume_ocv_cap =
		sc27xx_fgu_ocv2cap(data->cap_table, data->table_len, data->slp_cap_calib.resume_ocv_uv);

	dev_info(data->dev, "%s, resume_ocv_cap = %d, normal_temp_cap = %d, init_cap = %d\n",
		 __func__, data->slp_cap_calib.resume_ocv_cap,
		 data->normal_temp_cap, data->init_cap);

	if (data->slp_cap_calib.resume_ocv_cap > data->normal_temp_cap + 30)
		data->init_cap += (data->slp_cap_calib.resume_ocv_cap -
				   data->normal_temp_cap - 30);
	else if (data->slp_cap_calib.resume_ocv_cap < data->normal_temp_cap - 30)
		data->init_cap -= (data->normal_temp_cap -
				   data->slp_cap_calib.resume_ocv_cap - 30);

	data->track.lpocv_info.valid = true;
	data->track.lpocv_info.ocv_uv = data->slp_cap_calib.resume_ocv_uv;
	data->track.lpocv_info.ocv_rtc_time = data->slp_cap_calib.resume_time;
}

static void sc27xx_fgu_suspend_calib_check(struct sc27xx_fgu_data *data)
{
	int ret;

	if (!data->slp_cap_calib.support_slp_calib)
		return;

	if (data->chg_sts != POWER_SUPPLY_STATUS_NOT_CHARGING &&
	    data->chg_sts != POWER_SUPPLY_STATUS_DISCHARGING) {
		dev_info(data->dev, "Suspend calib charging status = %d, not meet conditions\n",
			 data->chg_sts);
		return;
	}

	ret = sc27xx_fgu_suspend_calib_check_relax_cnt_int(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_sleep_time(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_sleep_cur(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_temp(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_get_ocv(data);
	if (ret)
		return;

	if (!sc27xx_fgu_is_in_low_energy_dens(data, data->slp_cap_calib.resume_ocv_uv,
					      data->cap_calib_dens_ocv_table,
					      data->cap_calib_dens_ocv_table_len))
		return;

	sc27xx_fgu_suspend_calib_cap_calib(data);

	return;
}

static void sc27xx_fgu_enable_relax_cnt_int(struct sc27xx_fgu_data *data)
{
	int ret;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				 SC27XX_FGU_RELAX_CNT_STS, SC27XX_FGU_RELAX_CNT_STS);
	if (ret)
		dev_err(data->dev, "Sleep calib failed to clear  RELAX_CNT_INT_STS, ret = %d\n", ret);

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_RELAX_CNT_INT, SC27XX_FGU_RELAX_CNT_INT);
	if (ret)
		dev_err(data->dev, "Sleep calib Fail to enable RELAX_CNT_INT, re= %d\n", ret);
}

static int sc27xx_fgu_relax_mode_config(struct sc27xx_fgu_data *data)
{
	int ret, relax_cur_threshold_adc;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_RELAX_CNT_THRE,
				 SC27XX_FGU_RELAX_CNT_THRE_MASK,
				 data->slp_cap_calib.relax_cnt_threshold <<
				 SC27XX_FGU_RELAX_CNT_THRE_SHITF);
	if (ret) {
		dev_err(data->dev, "Sleep calib Fail to enable RELAX_CNT_THRE, re= %d\n", ret);
		return ret;
	}

	relax_cur_threshold_adc = sc27xx_fgu_current2adc(data, data->slp_cap_calib.relax_cur_threshold);
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_RELAX_CURT_THRE,
				 SC27XX_FGU_RELAX_CURT_THRE_MASK,
				 relax_cur_threshold_adc << SC27XX_FGU_RELAX_CURT_THRE_SHITF);
	if (ret)
		dev_err(data->dev, "Sleep calib Fail to enable RELAX_CURT_THRE, re= %d\n", ret);

	return ret;
}

static void sc27xx_fgu_suspend_calib_config(struct sc27xx_fgu_data *data)
{
	s64 cur_time;
	int ret;

	if (!data->slp_cap_calib.support_slp_calib)
		return;

	ret = sc27xx_fgu_get_rtc_time(data, &cur_time);
	if (ret)
		cur_time = 0;

	data->slp_cap_calib.suspend_time =  cur_time;
	sc27xx_fgu_get_clbcnt(data, &data->slp_cap_calib.suspend_clbcnt);
	ret = sc27xx_fgu_relax_mode_config(data);
	if (!ret)
		sc27xx_fgu_enable_relax_cnt_int(data);
}

static int sc27xx_fgu_batt_ovp_threshold_config(struct sc27xx_fgu_data *data)
{
	int ret = 0, overvoltage_adc;

	overvoltage_adc = sc27xx_fgu_voltage2adc(data, data->batt_ovp_threshold);
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_HIGH_OVERLOAD,
				 SC27XX_FGU_HIGH_OVERLOAD_MASK, overvoltage_adc);
	if (ret) {
		dev_err(data->dev, "failed to set fgu high overload\n");
		return ret;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_HIGH_OVERLOAD_INT, SC27XX_FGU_HIGH_OVERLOAD_INT);
	if (ret)
		return ret;

	data->is_ovp = false;
	dev_info(data->dev, "%s %d overload threshold config done!\n", __func__, __LINE__);

	return ret;
}

static bool sc27xx_fgu_probe_is_ready(struct sc27xx_fgu_data *data)
{
	unsigned long timeout;

	if (unlikely(!data->probe_initialized)) {
		timeout = wait_for_completion_timeout(&data->probe_init, SC27XX_FGU_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(data->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int sc27xx_fgu_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct sc27xx_fgu_data *data = power_supply_get_drvdata(psy);
	int ret = 0, value = 0;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!sc27xx_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&data->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		if (data->debug_info.batt_health_debug_en) {
			val->intval = data->debug_info.debug_batt_health;
			break;
		}

		sc27xx_fgu_get_health(data, &value);
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->bat_present;

		if (data->debug_info.batt_present_debug_en)
			val->intval = data->debug_info.debug_batt_present;

		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (data->debug_info.temp_debug_en)
			val->intval = data->debug_info.debug_temp;
		else if (data->temp_table_len <= 0 || (data->bat_present == 0 && allow_charger_enable))
			val->intval = 200;
		else {
			ret = sc27xx_fgu_get_temp(data, &value);
			if (ret < 0 && !data->debug_info.temp_debug_en)
				goto error;

			ret = 0;
			val->intval = value;
		}

		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == CM_BOOT_CAPACITY) {
			val->intval = data->boot_cap;
			break;
		}

		ret = sc27xx_fgu_get_capacity(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = sc27xx_fgu_get_vbat_avg(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (data->debug_info.vbat_now_debug_en) {
			val->intval = data->debug_info.debug_vbat_now;
			break;
		}

		ret = sc27xx_fgu_get_vbat_now(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		if (data->debug_info.ocv_debug_en) {
			val->intval = data->debug_info.debug_ocv;
			break;
		}
		ret = sc27xx_fgu_get_vbat_ocv(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (data->debug_info.chg_vol_debug_en) {
			val->intval = data->debug_info.debug_chg_vol;
			break;
		}

		ret = sc27xx_fgu_get_charge_vol(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = sc27xx_fgu_get_current_avg(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (data->debug_info.cur_now_debug_en) {
			val->intval = data->debug_info.debug_cur_now;
			break;
		}

		ret = sc27xx_fgu_get_current_now(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = data->total_mah * 1000;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = data->charge_cycle;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		ret = sc27xx_fgu_get_basp_volt(data, &val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
	}

error:
	mutex_unlock(&data->lock);
	return ret;
}

static int sc27xx_fgu_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	struct sc27xx_fgu_data *data = power_supply_get_drvdata(psy);
	int ret = 0, ui_cap, normal_cap;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!sc27xx_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&data->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		dev_dbg(data->dev, "%s:line%d cap = %d\n", __func__, __LINE__, val->intval);
		ret = sc27xx_fgu_save_last_cap(data, val->intval);
		if (ret < 0)
			dev_err(data->dev, "failed to save battery capacity\n");

		ret = sc27xx_fgu_save_normal_temperature_cap(data, data->normal_temp_cap);
		if (ret < 0)
			dev_err(data->dev, "failed to save normal temperature capacity\n");

		ret = sc27xx_fgu_read_last_cap(data, &ui_cap);
		if (ret < 0) {
			ui_cap = -1;
			dev_err(data->dev, "failed to read ui capacity\n");
		}

		if (val->intval != ui_cap) {
			dev_info(data->dev, "ui cap save failed, save it again!"
				 "save_cap = %d, read_cap = %d\n", val->intval, ui_cap);
			ret = sc27xx_fgu_save_last_cap(data, val->intval);
			if (ret < 0)
				dev_err(data->dev, "%d failed to save battery capacity\n", __LINE__);
		}

		ret = sc27xx_fgu_read_normal_temperature_cap(data, &normal_cap);
		if (ret < 0) {
			normal_cap = -1;
			dev_err(data->dev, "failed to read normal capacity\n");
		}

		if (data->normal_temp_cap != normal_cap) {
			dev_info(data->dev, "normal cap save failed, save it again!"
				 "save_cap = %d, read_cap = %d\n", data->normal_temp_cap, normal_cap);
			ret = sc27xx_fgu_save_normal_temperature_cap(data, data->normal_temp_cap);
			if (ret < 0)
				dev_err(data->dev, "%d failed to save normal temperature capacity\n", __LINE__);
		}
		break;

	case POWER_SUPPLY_PROP_STATUS:
		data->chg_sts = val->intval;
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		dev_dbg(data->dev, "%s:line%d calib cap = %d\n", __func__, __LINE__, val->intval);
		sc27xx_fgu_adjust_cap(data, val->intval);
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		dev_dbg(data->dev, "%s:line%d total_mah = %d\n", __func__, __LINE__, val->intval);
		data->total_mah = val->intval / 1000;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change battery temperature to debug mode\n");
			data->debug_info.temp_debug_en = true;
			data->debug_info.debug_temp = 200;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery battery temperature to normal mode\n");
			data->debug_info.temp_debug_en = false;
			break;
		} else if (!data->debug_info.temp_debug_en) {
			dev_info(data->dev, "Battery temperature not in debug mode\n");
			break;
		}

		data->debug_info.debug_temp = val->intval;
		dev_info(data->dev, "Battery debug temperature = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change battery present to debug mode\n");
			data->debug_info.debug_batt_present = true;
			data->debug_info.batt_present_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery battery present to normal mode\n");
			data->debug_info.batt_present_debug_en = false;
			break;
		} else if (!data->debug_info.batt_present_debug_en) {
			dev_info(data->dev, "Battery present not in debug mode\n");
			break;
		}

		data->debug_info.debug_batt_present = !!val->intval;
		mutex_unlock(&data->lock);
		cm_notify_event(data->battery, data->debug_info.debug_batt_present ?
				CM_EVENT_BATT_IN : CM_EVENT_BATT_OUT, NULL);
		dev_info(data->dev, "Battery debug present = %d\n", !!val->intval);
		return ret;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change voltage_now to debug mode\n");
			data->debug_info.debug_vbat_now = 4000000;
			data->debug_info.vbat_now_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery voltage_now to normal mode\n");
			data->debug_info.vbat_now_debug_en = false;
			data->debug_info.debug_vbat_now = 0;
			break;
		} else if (!data->debug_info.vbat_now_debug_en) {
			dev_info(data->dev, "Voltage_now not in debug mode\n");
			break;
		}

		data->debug_info.debug_vbat_now = val->intval;
		dev_info(data->dev, "Battery debug voltage_now = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change current_now to debug mode\n");
			data->debug_info.debug_cur_now = 1000000;
			data->debug_info.cur_now_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery current_now to normal mode\n");
			data->debug_info.cur_now_debug_en = false;
			data->debug_info.debug_cur_now = 0;
			break;
		} else if (!data->debug_info.cur_now_debug_en) {
			dev_info(data->dev, "Current_now not in debug mode\n");
			break;
		}

		data->debug_info.debug_cur_now = val->intval;
		dev_info(data->dev, "Battery debug current_now = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change charge voltage to debug mode\n");
			data->debug_info.debug_chg_vol = 5000000;
			data->debug_info.chg_vol_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery charge voltage to normal mode\n");
			data->debug_info.chg_vol_debug_en = false;
			data->debug_info.debug_chg_vol = 0;
			break;
		} else if (!data->debug_info.chg_vol_debug_en) {
			dev_info(data->dev, "Charge voltage not in debug mode\n");
			break;
		}

		data->debug_info.debug_chg_vol = val->intval;
		dev_info(data->dev, "Battery debug charge voltage = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change OCV voltage to debug mode\n");
			data->debug_info.debug_ocv = 4000000;
			data->debug_info.ocv_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery OCV voltage to normal mode\n");
			data->debug_info.ocv_debug_en = false;
			data->debug_info.debug_ocv = 0;
			break;
		} else if (!data->debug_info.ocv_debug_en) {
			dev_info(data->dev, "OCV voltage not in debug mode\n");
			break;
		}

		data->debug_info.debug_ocv = val->intval;
		dev_info(data->dev, "Battery debug OCV voltage = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == SC27XX_FGU_GOOD_HEALTH_CMD) {
			data->is_ovp = false;

			ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
						 SC27XX_FGU_HIGH_OVERLOAD_INT,
						 SC27XX_FGU_HIGH_OVERLOAD_INT);
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change Battery Health to debug mode\n");
			data->debug_info.batt_health_debug_en = true;
			data->debug_info.debug_batt_health = 1;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery  Battery Health to normal mode\n");
			data->debug_info.batt_health_debug_en = false;
			data->debug_info.debug_batt_health = 1;
			break;
		} else if (!data->debug_info.batt_health_debug_en) {
			dev_info(data->dev, "OCV  Battery Health not in debug mode\n");
			break;
		}

		data->debug_info.debug_batt_health = val->intval;
		dev_info(data->dev, "Battery debug  Battery Health = %#x\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		dev_dbg(data->dev, "%s:line%d max vol= %d\n", __func__, __LINE__, val->intval);
		ret = sc27xx_fgu_set_basp_volt(data, val->intval);
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&data->lock);
	return ret;
}

static int sc27xx_fgu_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_HEALTH:
		return 1;

	default:
		return 0;
	}
}

static enum power_supply_property sc27xx_fgu_props[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
};

static const struct power_supply_desc sc27xx_fgu_desc = {
	.name			= "sc27xx-fgu",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sc27xx_fgu_props,
	.num_properties		= ARRAY_SIZE(sc27xx_fgu_props),
	.get_property		= sc27xx_fgu_get_property,
	.set_property		= sc27xx_fgu_set_property,
	.external_power_changed	= power_supply_changed,
	.property_is_writeable	= sc27xx_fgu_property_is_writeable,
	.no_thermal		= true,
};

static void sc27xx_fgu_adjust_uusoc_vbat(struct sc27xx_fgu_data *data)
{
	if (data->batt_mv >= SC27XX_FGU_LOW_VBAT_REC_REGION) {
		data->uusoc_vbat = 0;
	} else if (data->batt_mv >= SC27XX_FGU_LOW_VBAT_REGION) {
		if (data->uusoc_vbat >= SC27XX_FGU_LOW_VBAT_UUSOC_STEP)
			data->uusoc_vbat -= SC27XX_FGU_LOW_VBAT_UUSOC_STEP;
	}
}

static void sc27xx_fgu_low_capacity_match_ocv(struct sc27xx_fgu_data *data)
{
	if (data->ocv_uv < data->min_volt_uv && data->normal_temp_cap > data->alarm_cap) {
		data->init_cap -= 5;
		if (data->init_cap < 0)
			data->init_cap = 0;
	} else if (data->ocv_uv > data->min_volt_uv && data->normal_temp_cap <= data->alarm_cap) {
		sc27xx_fgu_adjust_cap(data, data->alarm_cap);
	} else if (data->ocv_uv <= data->cap_table[data->table_len - 1].ocv) {
		sc27xx_fgu_adjust_cap(data, 0);
	} else if (data->first_calib_volt > 0 && data->first_calib_cap > 0 &&
		   data->ocv_uv <= data->first_calib_volt &&
		   data->normal_temp_cap > data->first_calib_cap) {
		data->init_cap -= 5;
		if (data->init_cap < 0)
			data->init_cap = 0;
	} else if (data->batt_mv < SC27XX_FGU_LOW_VBAT_REGION &&
		   data->normal_temp_cap > data->alarm_cap)
		data->uusoc_vbat += SC27XX_FGU_LOW_VBAT_UUSOC_STEP;

	sc27xx_fgu_adjust_uusoc_vbat(data);
}

static bool sc27xx_fgu_discharging_current_trend(struct sc27xx_fgu_data *data)
{
	int i, ret, cur_adc = 0;
	bool is_discharging = true;

	if (data->cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT - 1] == SC27XX_FGU_MAGIC_NUMBER) {
		is_discharging = false;
		for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
			ret = regmap_read(data->regmap,
					  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
					  &cur_adc);
			if (ret) {
				dev_err(data->dev, "fail to init cur_now_buff[%d]\n", i);
				return is_discharging;
			}

			data->cur_now_buff[i] =
			      sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);
		}

		return is_discharging;
	}

	for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
		if (data->cur_now_buff[i] > 0)
			is_discharging = false;
	}

	for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur_adc);
		if (ret) {
			dev_err(data->dev, "fail to get cur_now_buff[%d]\n", i);
			data->cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT - 1] =
				SC27XX_FGU_MAGIC_NUMBER;
			is_discharging = false;
			return is_discharging;
		}

		data->cur_now_buff[i] =
			sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);
		if (data->cur_now_buff[i] > 0)
			is_discharging = false;
	}

	return is_discharging;
}

static bool sc27xx_fgu_discharging_clbcnt_trend(struct sc27xx_fgu_data *data)
{
	if (data->last_clbcnt - data->cur_clbcnt > 0)
		return true;
	else
		return false;
}

static bool sc27xx_fgu_discharging_trend(struct sc27xx_fgu_data *data)
{
	bool discharging = true;
	static int dischg_cnt;
	int i;

	if (dischg_cnt >= SC27XX_FGU_DISCHG_CNT)
		dischg_cnt = 0;

	if (!sc27xx_fgu_discharging_current_trend(data)) {
		discharging =  false;
		goto charging;
	}

	if (!sc27xx_fgu_discharging_clbcnt_trend(data)) {
		discharging =  false;
		goto charging;
	}

	data->dischg_trend[dischg_cnt++] = true;

	for (i = 0; i < SC27XX_FGU_DISCHG_CNT; i++) {
		if (!data->dischg_trend[i]) {
			discharging =  false;
			return discharging;
		}
	}

	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING && discharging)
		dev_info(data->dev, "%s: discharging\n", __func__);

	return discharging;

charging:
	data->dischg_trend[dischg_cnt++] = false;
	return discharging;
}

static void sc27xx_fgu_discharging_calibration(struct sc27xx_fgu_data *data, int *cap)
{
	int ret, low_temp_ocv;
	int vol_mv, vbat_avg_mv, vol_uv, vbat_avg_uv;

	if (data->bat_temp <= SC27XX_FGU_LOW_TEMP_REGION) {
		dev_err(data->dev, "exceed temp range not need to calibrate.\n");
		return;
	}

	if (*cap > SC27XX_FGU_CAP_CALIB_ALARM_CAP)
		return;

	if (data->chg_sts != POWER_SUPPLY_STATUS_CHARGING ||
	    sc27xx_fgu_discharging_trend(data)) {
		low_temp_ocv = sc27xx_fgu_cap2ocv(data->cap_table,
						  data->table_len,
						  SC27XX_FGU_CAP_CALIB_ALARM_CAP);

		/* Get current battery voltage */
		ret = sc27xx_fgu_get_vbat_now(data, &vol_mv);
		if (ret) {
			dev_err(data->dev, "get current battery voltage error.\n");
			return;
		}

		/* Get average value of battery voltage */
		ret = sc27xx_fgu_get_vbat_avg(data, &vbat_avg_mv);
		if (ret) {
			dev_err(data->dev, "get average value of battery voltage error.\n");
			return;
		}

		vol_uv = vol_mv * 1000;
		vbat_avg_uv = vbat_avg_mv * 1000;
		dev_info(data->dev, "discharging_trend low_temp_ocv = %d, vbat = %d, vbat_avg = %d\n",
			 low_temp_ocv, vol_uv, vbat_avg_uv);
		if (vol_uv > low_temp_ocv && vbat_avg_uv > low_temp_ocv)
			*cap = SC27XX_FGU_CAP_CALIB_ALARM_CAP;
	}
}

static void sc27xx_fgu_capacity_calibration(struct sc27xx_fgu_data *data, bool int_mode)
{
	int ret, adc, ocv_mv;

	ret = sc27xx_fgu_get_vbat_ocv(data, &ocv_mv);
	if (ret) {
		dev_err(data->dev, "get battery ocv error.\n");
		return;
	}

	data->ocv_uv = ocv_mv * 1000;

	ret =  sc27xx_fgu_get_vbat_now(data, &data->batt_mv);
	if (ret) {
		dev_err(data->dev, "get battery vol error.\n");
		return;
	}

	/*
	 * If we are in charging mode or the battery temperature is
	 * 10 degrees or less, then we do not need to calibrate the
	 * lower capacity.
	 */
	if ((!sc27xx_fgu_discharging_trend(data) &&
	     data->chg_sts == POWER_SUPPLY_STATUS_CHARGING) ||
	    data->bat_temp <= SC27XX_FGU_LOW_TEMP_REGION) {
		sc27xx_fgu_adjust_uusoc_vbat(data);
		return;
	}

	if (!data->cap_table) {
		dev_info(data->dev, "%s: cap_table allocate not ready\n", __func__);
		return;
	}

	sc27xx_fgu_low_capacity_match_ocv(data);

	if (data->ocv_uv <= data->min_volt_uv) {
		if (!int_mode)
			return;

		/*
		 * After adjusting the battery capacity, we should set the
		 * lowest alarm voltage instead.
		 */
		data->min_volt_uv = data->cap_table[data->table_len - 1].ocv;
		data->alarm_cap = sc27xx_fgu_ocv2cap(data->cap_table, data->table_len, data->min_volt_uv);

		if (data->alarm_cap < 10)
			data->alarm_cap = 10;

		adc = sc27xx_fgu_voltage2adc(data, data->min_volt_uv / 1000);
		regmap_update_bits(data->regmap,
				   data->base + SC27XX_FGU_LOW_OVERLOAD,
				   SC27XX_FGU_LOW_OVERLOAD_MASK, adc);
	}
}

static void sc27xx_fgu_batt_ovp_notfiy(struct sc27xx_fgu_data *data)
{
	int ret;

	data->is_ovp = true;
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_HIGH_OVERLOAD_INT, 0);
	if (ret) {
		dev_err(data->dev, "failed to disable high overload int\n");
	}

	cm_notify_event(data->battery, CM_EVENT_BATT_OVERVOLTAGE, NULL);
}

static irqreturn_t sc27xx_fgu_interrupt(int irq, void *dev_id)
{
	struct sc27xx_fgu_data *data = dev_id;
	int ret, cap;
	u32 status;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	if (!sc27xx_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&data->lock);

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_INT_STS,
			  &status);
	if (ret)
		goto out;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				 status, status);
	if (ret)
		goto out;

	if (status & SC27XX_FGU_RELAX_CNT_STS) {
		data->slp_cap_calib.relax_cnt_int_ocurred = true;
		dev_info(data->dev, "%s,  RELAX_CNT_INT ocurred!!\n", __func__);
	}

	if (status & SC27XX_FGU_HIGH_OVERLOAD_INT) {
		mutex_unlock(&data->lock);
		sc27xx_fgu_batt_ovp_notfiy(data);
		return IRQ_HANDLED;
	}

	/*
	 * When low overload voltage interrupt happens, we should calibrate the
	 * battery capacity in lower voltage stage.
	 */
	if (!(status & SC27XX_FGU_LOW_OVERLOAD_INT))
		goto out;

	ret = sc27xx_fgu_get_capacity(data, &cap);
	if (ret)
		goto out;

	sc27xx_fgu_capacity_calibration(data, true);

out:
	mutex_unlock(&data->lock);

	power_supply_changed(data->battery);
	return IRQ_HANDLED;
}

static irqreturn_t sc27xx_fgu_bat_detection(int irq, void *dev_id)
{
	struct sc27xx_fgu_data *data = dev_id;
	int state;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	if (!sc27xx_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&data->lock);

	state = gpiod_get_value_cansleep(data->gpiod);
	if (state < 0) {
		dev_err(data->dev, "failed to get gpio state\n");
		mutex_unlock(&data->lock);
		return IRQ_RETVAL(state);
	}

	data->bat_present = !!state;

	mutex_unlock(&data->lock);

	power_supply_changed(data->battery);

	cm_notify_event(data->battery,
			data->bat_present ? CM_EVENT_BATT_IN : CM_EVENT_BATT_OUT,
			NULL);

	return IRQ_HANDLED;
}

static void sc27xx_fgu_disable(void *_data)
{
	struct sc27xx_fgu_data *data = _data;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	regmap_update_bits(data->regmap, data->pdata->clk_en, SC27XX_FGU_RTC_EN, 0);
	regmap_update_bits(data->regmap, data->pdata->module_en, SC27XX_FGU_EN, 0);
}

static int sc27xx_fgu_calibration(struct sc27xx_fgu_data *data)
{
	struct nvmem_cell *cell;
	const struct sc27xx_fgu_variant_data *pdata = data->pdata;
	int calib_data, cal_4200mv;
	void *buf;
	size_t len;

	if (!pdata) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	cell = nvmem_cell_get(data->dev, "fgu_calib");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));

	/*
	 * Get the ADC value corresponding to 4200 mV from eFuse controller
	 * according to below formula. Then convert to ADC values corresponding
	 * to 1000 mV and 1000 mA.
	 */
	cal_4200mv = ((calib_data & pdata->fgu_cal) >> pdata->fgu_cal_shift)
			+ 6963 - 4096 - 256;
	data->vol_1000mv_adc = DIV_ROUND_CLOSEST(cal_4200mv * 10, 42);
	data->cur_1000ma_adc =
		DIV_ROUND_CLOSEST(data->vol_1000mv_adc * 4 * data->calib_resist,
				  SC27XX_FGU_IDEAL_RESISTANCE);

	kfree(buf);
	return 0;
}

static void sc27xx_fgu_typec_extcon_work(struct work_struct *data)
{
	struct sc27xx_fgu_data *fgu_data = container_of(data, struct sc27xx_fgu_data,
							typec_extcon_work);
	int retry_cnt = 12;

	if (fgu_data->use_typec_extcon && fgu_data->online) {
		/* if use typec extcon notify charger,
		 * wait for BC1.2 detect charger type.
		 */
		while (fgu_data->online && retry_cnt > 0) {
			if (fgu_data->usb_phy->chg_type != UNKNOWN_TYPE)
				break;
			retry_cnt--;
			msleep(50);
		}
		dev_info(fgu_data->dev, "retry_cnt = %d\n", retry_cnt);

	}

	switch (fgu_data->usb_phy->chg_type) {
	case SDP_TYPE:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;

	case DCP_TYPE:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	case CDP_TYPE:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;

	default:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	dev_info(fgu_data->dev, "charger type = %d\n", fgu_data->usb_phy->chg_type);
}

#define SC27XX_FGU_EXTCON_SINK		3

static int sc27xx_fgu_extcon_event(struct notifier_block *nb,
				   unsigned long event, void *param)
{
	struct sc27xx_fgu_data *data = container_of(nb, struct sc27xx_fgu_data, extcon_nb);
	int state = 0;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	state = extcon_get_state(data->edev, SC27XX_FGU_EXTCON_SINK);
	if (state < 0) {
		dev_err(data->dev, "failed to get extcon sink state（%d）\n", state);
		return NOTIFY_OK;
	}

	if (data->is_sink == state)
		return NOTIFY_OK;

	data->is_sink = state;

	if (data->is_sink)
		data->online = true;
	else
		data->online = false;

	schedule_work(&data->typec_extcon_work);
	return NOTIFY_OK;
}

static int sc27xx_fgu_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *info)
{
	u32 type;
	struct sc27xx_fgu_data *data =
		container_of(nb, struct sc27xx_fgu_data, usb_notify);

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	pm_stay_awake(data->dev);

	if (limit)
		data->online = true;
	else
		data->online = false;

	type = data->usb_phy->chg_type;

	switch (type) {
	case SDP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;

	case DCP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	case CDP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;

	default:
		data->chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	pm_relax(data->dev);

	return NOTIFY_OK;
}

static bool sc27xx_fgu_cap_track_is_ocv_valid(struct sc27xx_fgu_data *data, int *ocv_uv,
					      struct sc27xx_fgu_ocv_info *ocv_info)
{
	s64 cur_time;
	int ret;

	if (!ocv_info->valid)
		return false;

	ret = sc27xx_fgu_get_rtc_time(data, &cur_time);
	if (ret)
		return false;

	ocv_info->valid = false;

	if (cur_time - ocv_info->ocv_rtc_time > SC27XX_FGU_TRACK_OCV_VALID_TIME) {
		dev_info(data->dev, "capacity track ocv is invalid cur_time = %lld, rtc_time = %lld\n",
			 cur_time, ocv_info->ocv_rtc_time);
		return false;
	}

	if (!sc27xx_fgu_is_in_low_energy_dens(data, ocv_info->ocv_uv,
					      data->track.dens_ocv_table,
					      data->track.dens_ocv_table_len))
		return false;

	*ocv_uv = ocv_info->ocv_uv;


	return true;
}

static bool sc27xx_fgu_cap_track_is_sw_ocv_valid(struct sc27xx_fgu_data *data, int *ocv_uv)
{
	int i, ret, cap, vol_adc = 0, cur_adc = 0, cur_ma = 0, vol_mv = 0;
	int resistance, scale_ratio, ocv_mv;

	if (data->bat_temp > SC27XX_FGU_TRACK_HIGH_TEMP_THRESHOLD ||
	    data->bat_temp < SC27XX_FGU_TRACK_LOW_TEMP_THRESHOLD) {
		dev_err(data->dev, "exceed temp range, sw ocv is invalid\n");
		return false;
	}

	resistance = data->internal_resist;
	if (data->resist_table_len > 0) {
		scale_ratio = sc27xx_fgu_temp2resist_ratio(data->resist_table,
							   data->resist_table_len,
							   data->bat_temp);
		resistance = data->internal_resist * scale_ratio / 100;
	}

	for (i = 0; i < 8; i++) {
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur_adc);
		if (ret)
			return false;

		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_VOLTAGE_BUF + i * 4,
				  &vol_adc);
		if (ret)
			return false;

		cur_ma = sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);
		vol_mv = sc27xx_fgu_adc2voltage(data, vol_adc);


		if (abs(cur_ma) > SC27XX_FGU_TRACK_CAP_START_CURRENT)
			return false;

		ocv_mv = vol_mv - cur_ma * (resistance + SC27XX_FGU_RBAT_CMP_MOH) / 1000;
		if (ocv_mv > SC27XX_FGU_TRACK_CAP_START_VOLTAGE)
			return false;

		*ocv_uv += ocv_mv * 1000;
	}

	*ocv_uv /= 8;

	cap = sc27xx_fgu_ocv2cap(data->cap_table, data->table_len, *ocv_uv);

	if (cap > SC27XX_FGU_TRACK_START_CAP_SWOCV_HTHRESHOLD ||
	    cap < SC27XX_FGU_TRACK_START_CAP_LTHRESHOLD) {
		dev_info(data->dev, "start_cap = %d does not satisfy track start condition\n", cap);
		return false;
	}

	dev_info(data->dev, "sow ocv  cur_ma = %d, vol_mV = %d, ocv_uv = %d\n",
		 cur_ma, vol_mv, *ocv_uv);

	return true;
}

static bool sc27xx_fgu_is_meet_cap_track_start_conditon(struct sc27xx_fgu_data *data, int *ocv_uv)
{
	if (sc27xx_fgu_cap_track_is_ocv_valid(data, ocv_uv, &data->track.pocv_info)) {
		data->track.mode = CAP_TRACK_MODE_POCV;
		dev_info(data->dev, "capacity track pocv = %d meet start condition", *ocv_uv);
	} else if (sc27xx_fgu_cap_track_is_ocv_valid(data, ocv_uv, &data->track.lpocv_info)) {
		data->track.mode = CAP_TRACK_MODE_LP_OCV;
		dev_info(data->dev, "capacity track lpocv = %d meet start condition", *ocv_uv);
	} else if (sc27xx_fgu_cap_track_is_sw_ocv_valid(data, ocv_uv)) {
		data->track.mode = CAP_TRACK_MODE_SW_OCV;
		dev_info(data->dev, "capacity track sw ocv = %d  meet start condition", *ocv_uv);
	} else {
		return false;
	}

	return true;
}

static bool sc27xx_fgu_is_new_cap_track_start_conditon_meet(struct sc27xx_fgu_data *data)
{
	int ocv_uv, cap;

	if (!sc27xx_fgu_cap_track_is_ocv_valid(data, &ocv_uv, &data->track.lpocv_info))
		return false;

	cap = sc27xx_fgu_ocv2cap(data->cap_table, data->table_len, ocv_uv);
	if (cap > SC27XX_FGU_TRACK_START_CAP_HTHRESHOLD ||
	    cap < SC27XX_FGU_TRACK_START_CAP_LTHRESHOLD)
		return false;

	if ((data->track.mode == CAP_TRACK_MODE_LP_OCV) &&
	    ((ktime_divns(ktime_get_boottime(), NSEC_PER_SEC) -
	      data->track.start_time) < SC27XX_FGU_TRACK_NEW_OCV_VALID_THRESHOLD)) {
		return false;
	}

	dev_info(data->dev, "capacity track lpocv = %d new  start condition meet", ocv_uv);
	/*
	 * It need to set valid to true becase it will clear in
	 * sc27xx_fgu_cap_track_is_ocv_valid.
	 */
	data->track.lpocv_info.valid = true;

	return true;
}

static bool sc27xx_fgu_cap_track_is_meet_end_conditon(struct sc27xx_fgu_data *data)
{
	int i, ret, vol_adc = 0, cur_adc = 0, cur_now = 0, vol_now = 0;

	for (i = 0; i < 5; i++) {
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur_adc);
		if (ret)
			return false;

		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_VOLTAGE_BUF + i * 4,
				  &vol_adc);
		if (ret)
			return false;

		cur_now = sc27xx_fgu_adc2current(data, (s64)cur_adc - SC27XX_FGU_CUR_BASIC_ADC);
		vol_now = sc27xx_fgu_adc2voltage(data, vol_adc);


		if (cur_now <= 0 || cur_now > data->track.end_cur || vol_now < data->track.end_vol)
			return false;
	}

	return true;

}

static void sc27xx_fgu_cap_track_state_init(struct sc27xx_fgu_data *data, int *cycle)
{
	int design_mah, learned_mah;

	design_mah = data->design_mah;
	learned_mah = data->track.learned_mah;

	data->track.state = CAP_TRACK_IDLE;

	if (data->track.pocv_info.valid)
		*cycle = SC27XX_FGU_CAPACITY_TRACK_0S;

	if (learned_mah <= 0) {
		dev_err(data->dev, "[init] learned_mah is invalid.\n");
		return;
	}

	if (((learned_mah > design_mah) && ((learned_mah - design_mah) < design_mah / 10)) ||
	    ((design_mah > learned_mah) && ((design_mah - learned_mah) < design_mah / 2)))
		data->total_mah = learned_mah;
}

static void sc27xx_fgu_cap_track_state_idle(struct sc27xx_fgu_data *data, int *cycle)
{
	int ret, clbcnt, ocv_uv;

	if (!data->bat_present) {
		*cycle = SC27XX_FGU_CAPACITY_TRACK_100S;
		dev_dbg(data->dev, "[idle] battery is not present, monitor later.\n");
		return;
	}

	if (!sc27xx_fgu_is_meet_cap_track_start_conditon(data, &ocv_uv))
		return;

	data->track.start_cap = sc27xx_fgu_ocv2cap(data->cap_table,
						   data->table_len,
						   ocv_uv);

	/*
	 * When the capacity tracking start condition is met, the battery is almost empty,
	 * so we set a starting threshold, if it is greater than it will not enable
	 * the capacity tracking function, now we set the capacity tracking monitor
	 * initial percentage threshold to 20%.
	 */
	if (data->track.start_cap > SC27XX_FGU_TRACK_START_CAP_HTHRESHOLD ||
	    data->track.start_cap < SC27XX_FGU_TRACK_START_CAP_LTHRESHOLD) {
		dev_dbg(data->dev, "[idle] start_cap = %d does not satisfy the track start condition\n",
			data->track.start_cap);
		data->track.start_cap = 0;
		return;
	}

	ret = sc27xx_fgu_get_clbcnt(data, &clbcnt);
	if (ret) {
		dev_err(data->dev, "[idle] failed to get start clbcnt.\n");
		return;
	}

	data->track.start_time = ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
	data->track.start_clbcnt = clbcnt;
	data->track.state = CAP_TRACK_UPDATING;

	dev_info(data->dev, "[idle] start_time = %lld, start_clbcnt = %d, start_cap = %d\n",
		 data->track.start_time, clbcnt, data->track.start_cap);
}

static void sc27xx_fgu_cap_track_state_updating(struct sc27xx_fgu_data *data, int *cycle)
{
	int ibat_avg_ma, vbat_avg_mv, ibat_now_ma, ret;

	if (!data->bat_present) {
		*cycle = SC27XX_FGU_CAPACITY_TRACK_100S;
		data->track.state = CAP_TRACK_IDLE;
		dev_err(data->dev, "[updating] battery is not present, return to idle state.\n");
		return;
	}

	if (sc27xx_fgu_is_new_cap_track_start_conditon_meet(data)) {
		pm_wakeup_event(data->dev, SC27XX_FGU_TRACK_UPDATING_WAKE_UP_MS);
		*cycle = SC27XX_FGU_CAPACITY_TRACK_0S;
		data->track.state = CAP_TRACK_IDLE;
		return;
	}

	if (data->chg_sts != POWER_SUPPLY_STATUS_CHARGING)
		return;

	if (data->bat_temp > SC27XX_FGU_TRACK_HIGH_TEMP_THRESHOLD ||
	    data->bat_temp < SC27XX_FGU_TRACK_LOW_TEMP_THRESHOLD) {
		*cycle = SC27XX_FGU_CAPACITY_TRACK_100S;
		dev_dbg(data->dev, "[updating] exceed temp range, monitor capacity track later.\n");
		return;
	}

	if ((data->chg_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
	    || (data->chg_type == POWER_SUPPLY_USB_TYPE_SDP))
		return;

	if ((ktime_divns(ktime_get_boottime(), NSEC_PER_SEC) -
	     data->track.start_time) > SC27XX_FGU_TRACK_TIMEOUT_THRESHOLD) {
		data->track.state = CAP_TRACK_IDLE;
		dev_err(data->dev, "capacity tracktime out.\n");
		return;
	}

	ret = sc27xx_fgu_get_current_avg(data, &ibat_avg_ma);
	if (ret) {
		dev_err(data->dev, "failed to get ibat average current.\n");
		return;
	}

	ret = sc27xx_fgu_get_current_now(data, &ibat_now_ma);
	if (ret) {
		dev_err(data->dev, "failed to get ibat current now.\n");
		return;
	}

	ret = sc27xx_fgu_get_vbat_avg(data, &vbat_avg_mv);
	if (ret) {
		dev_err(data->dev, "failed to get battery voltage.\n");
		return;
	}

	if (vbat_avg_mv > data->track.end_vol &&
	    (ibat_avg_ma > 0 && ibat_avg_ma < data->track.end_cur) &&
	    (ibat_now_ma > 0 && ibat_now_ma < data->track.end_cur)) {
		dev_info(data->dev, "capacity track finish condition is met!!!\n");
		pm_wakeup_event(data->dev, SC27XX_FGU_TRACK_DONE_WAKE_UP_MS);
		data->track.state = CAP_TRACK_DONE;
		*cycle = SC27XX_FGU_CAPACITY_TRACK_3S;
	}
}

static void sc27xx_fgu_cap_track_state_done(struct sc27xx_fgu_data *data, int *cycle)
{
	int ret, ibat_avg_ma = 0, vbat_avg_mv = 0, ibat_now_ma = 0;
	int delta_mah, clbcnt, total_mah, design_mah, start_mah, end_mah;

	*cycle = SC27XX_FGU_CAPACITY_TRACK_3S;

	if (!data->bat_present) {
		*cycle = SC27XX_FGU_CAPACITY_TRACK_100S;
		data->track.state = CAP_TRACK_IDLE;
		dev_err(data->dev, "[done] battery is not present, return to idle state.\n");
		return;
	}

	if ((data->chg_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
	    || (data->chg_type == POWER_SUPPLY_USB_TYPE_SDP)) {
		data->track.state = CAP_TRACK_UPDATING;
		dev_err(data->dev, "[done] chg_type not support, return to updating state\n");
		return;
	}

	if (data->chg_sts != POWER_SUPPLY_STATUS_CHARGING) {
		*cycle = SC27XX_FGU_CAPACITY_TRACK_15S;
		data->track.state = CAP_TRACK_UPDATING;
		dev_err(data->dev, "[done] Not charging, Return to updating state\n");
		return;
	}

	if (data->bat_temp > SC27XX_FGU_TRACK_HIGH_TEMP_THRESHOLD ||
	    data->bat_temp < SC27XX_FGU_TRACK_LOW_TEMP_THRESHOLD) {
		data->track.state = CAP_TRACK_UPDATING;
		*cycle = SC27XX_FGU_CAPACITY_TRACK_15S;
		dev_err(data->dev, "[done] exceed temp range, return to updating state.\n");
		return;
	}

	ret = sc27xx_fgu_get_current_avg(data, &ibat_avg_ma);
	if (ret) {
		dev_err(data->dev, "failed to get battery current.\n");
		return;
	}

	ret = sc27xx_fgu_get_current_now(data, &ibat_now_ma);
	if (ret) {
		dev_err(data->dev, "failed to get now current.\n");
		return;
	}

	ret = sc27xx_fgu_get_vbat_avg(data, &vbat_avg_mv);
	if (ret) {
		dev_err(data->dev, "failed to get battery voltage.\n");
		return;
	}

	if (!sc27xx_fgu_cap_track_is_meet_end_conditon(data)) {
		if (vbat_avg_mv > data->track.end_vol &&
		    (ibat_avg_ma > 0 && ibat_avg_ma < data->track.end_cur) &&
		    (ibat_now_ma > 0 && ibat_now_ma < data->track.end_cur)) {
			*cycle = SC27XX_FGU_CAPACITY_TRACK_3S;
		} else {
			*cycle = SC27XX_FGU_CAPACITY_TRACK_15S;
			data->track.state = CAP_TRACK_UPDATING;
			dev_info(data->dev, "[done] does not meet end conditons, return to updating"
				 " status, vbat_avg_mv = %d, ibat_avg_ma = %d, ibat_now_ma = %d\n",
				 vbat_avg_mv, ibat_avg_ma, ibat_now_ma);
		}

		return;
	}

	ret = sc27xx_fgu_get_clbcnt(data, &clbcnt);
	if (ret) {
		dev_err(data->dev, "failed to get start clbcnt.\n");
		return;
	}

	total_mah = data->total_mah;
	design_mah = data->design_mah;
	/*
	 * Due to the capacity tracking function started, the coulomb amount corresponding
	 * to the initial percentage was not counted, so we need to compensate initial coulomb
	 * with following formula, we assume that coulomb and capacity are directly proportional.
	 *
	 * For example:
	 * if capacity tracking function started,  the battery percentage is 3%, we will count
	 * the capacity from 3% to 100%, it will discard capacity from 0% to 3%, so we use
	 * "total_mah * (start_cap / 100)" to compensate.
	 *
	 * formula:
	 * end_mah = total_mah * (start_cap / 100) + delta_mah
	 */
	delta_mah = sc27xx_fgu_clbcnt2mah(data, clbcnt - data->track.start_clbcnt);
	start_mah = (total_mah * data->track.start_cap) / 1000;
	end_mah = start_mah + delta_mah;

	dev_info(data->dev, "Capacity track end: clbcnt = %d, start_clbcnt = %d, delta_mah = %d, "
		 "total_mah = %d, design_mah = %d, start_mah = %d, end_mah = %d, ibat_avg_ma = %d, "
		 "ibat_now_ma = %d, vbat_avg_mv = %d\n",
		 clbcnt, data->track.start_clbcnt, delta_mah, total_mah, design_mah, start_mah,
		 end_mah, ibat_avg_ma, ibat_now_ma, vbat_avg_mv);


	data->track.state = CAP_TRACK_IDLE;
	if (((end_mah > design_mah) && ((end_mah - design_mah) < design_mah / 10)) ||
	    ((design_mah > end_mah) && ((design_mah - end_mah) < design_mah / 2))) {
		data->total_mah = end_mah;
		pm_wakeup_event(data->dev, SC27XX_FGU_TRACK_WAKE_UP_MS);
		dev_info(data->dev, "track capacity done: end_mah = %d, diff_mah = %d\n",
			 end_mah, (end_mah - total_mah));
	} else {
		dev_info(data->dev, "less than half standard capacity.\n");
	}
}

static int sc27xx_fgu_cap_track_state_machine(struct sc27xx_fgu_data *data)
{
	int cycle = SC27XX_FGU_CAPACITY_TRACK_15S;

	switch (data->track.state) {
	case CAP_TRACK_INIT:
		sc27xx_fgu_cap_track_state_init(data, &cycle);
		break;
	case CAP_TRACK_IDLE:
		sc27xx_fgu_cap_track_state_idle(data, &cycle);
		break;
	case CAP_TRACK_UPDATING:
		sc27xx_fgu_cap_track_state_updating(data, &cycle);
		break;
	case CAP_TRACK_DONE:
		sc27xx_fgu_cap_track_state_done(data, &cycle);
		break;
	case CAP_TRACK_ERR:
		dev_err(data->dev, "track status error\n");
		break;

	default:
		break;
	}

	return cycle;
}

static void sc27xx_fgu_cap_track_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc27xx_fgu_data *data = container_of(dwork,
			struct sc27xx_fgu_data, cap_track_work);
	int work_cycle = SC27XX_FGU_CAPACITY_TRACK_15S;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (!data->track.cap_tracking)
		return;

	work_cycle = sc27xx_fgu_cap_track_state_machine(data);

	schedule_delayed_work(&data->cap_track_work, msecs_to_jiffies(work_cycle * 1000));
}

static void sc27xx_fgu_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc27xx_fgu_data *data = container_of(dwork, struct sc27xx_fgu_data, fgu_work);

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	sc27xx_fgu_dump_info(data);

	schedule_delayed_work(&data->fgu_work, SC27XX_FGU_WORK_MS);
}

static ssize_t sc27xx_fgu_dump_info_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_dump_info);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	data->support_debug_log = !data->support_debug_log;

	sc27xx_fgu_dump_battery_info(data, "dump_info");

	return snprintf(buf, PAGE_SIZE, "[batt present:%d];\n[total_mah:%d];\n[init_cap:%d];\n"
			"[init_clbcnt:%d];\n[alarm_cap:%d];\n[boot_cap:%d];\n[normal_temp_cap:%d];\n"
			"[max_volt:%d];\n[min_volt:%d];\n[first_calib_volt:%d];\n[first_calib_cap:%d];\n"
			"[uusoc_vbat:%d];\n[boot_vol:%d];\n[last_clbcnt:%d];\n[cur_clbcnt:%d];\n"
			"[bat_temp:%d];\n[online:%d];\n[is_first_poweron:%d];\n[chg_type:%d]\n"
			"[support_debug_log:%d]\n",
			data->bat_present, data->total_mah, data->init_cap, data->init_clbcnt,
			data->alarm_cap, data->boot_cap, data->normal_temp_cap, data->max_volt_uv,
			data->min_volt_uv, data->first_calib_volt, data->first_calib_cap,
			data->uusoc_vbat, data->boot_volt_uv, data->last_clbcnt, data->cur_clbcnt,
			data->bat_temp, data->online, data->is_first_poweron, data->chg_type,
			data->support_debug_log);
}

static ssize_t sc27xx_fgu_sel_reg_id_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_sel_reg_id);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[sel_reg_id:0x%x]\n", data->debug_info.sel_reg_id);
}

static ssize_t sc27xx_fgu_sel_reg_id_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_sel_reg_id);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 16, &val);
	if (ret) {
		dev_err(data->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	if (val > SC27XX_FGU_REG_MAX) {
		dev_err(data->dev, "val = %d, out of SC27XX_FGU_REG_MAX\n", val);
		return count;
	}

	data->debug_info.sel_reg_id = val;

	return count;
}

static ssize_t sc27xx_fgu_reg_val_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_reg_val);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 reg_val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	ret = regmap_read(data->regmap,
			  data->base + data->debug_info.sel_reg_id,
			  &reg_val);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "Fail to read [REG_0x%x], ret = %d\n",
				data->debug_info.sel_reg_id, ret);

	return snprintf(buf, PAGE_SIZE, "[REG_0x%x][0x%x]\n",
			data->debug_info.sel_reg_id, reg_val);
}

static ssize_t sc27xx_fgu_reg_val_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_reg_val);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 reg_val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 16, &reg_val);
	if (ret) {
		dev_err(data->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	dev_info(data->dev, "Try to set [REG_0x%x][0x%x]\n", data->debug_info.sel_reg_id, reg_val);

	ret = regmap_write(data->regmap, data->base + data->debug_info.sel_reg_id, reg_val);
	if (ret)
		dev_err(data->dev, "fail to set [REG_0x%x][0x%x], ret = %d\n",
			data->debug_info.sel_reg_id, reg_val, ret);

	return count;
}

static ssize_t sc27xx_fgu_enable_sleep_calib_show(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_enable_sleep_calib);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "capacity sleep calibration function [%s]\n",
			data->slp_cap_calib.support_slp_calib ? "Enabled" : "Disabled");
}

static ssize_t sc27xx_fgu_enable_sleep_calib_store(struct device *dev,
						   struct device_attribute *attr,
						   const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_enable_sleep_calib);
	struct sc27xx_fgu_data *data = sysfs->data;
	bool enbale_slp_calib;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtobool(buf, &enbale_slp_calib);
	if (ret) {
		dev_err(data->dev, "fail to get sleep_calib info, ret = %d\n", ret);
		return count;
	}

	data->slp_cap_calib.support_slp_calib = enbale_slp_calib;

	dev_info(data->dev, "Try to [%s] capacity sleep calibration function\n",
		 data->slp_cap_calib.support_slp_calib ? "Enabled" : "Disabled");

	return count;
}

static ssize_t sc27xx_fgu_relax_cnt_th_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cnt_th);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[relax_cnt_th][%d]\n",
			data->slp_cap_calib.relax_cnt_threshold);
}

static ssize_t sc27xx_fgu_relax_cnt_th_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cnt_th);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 relax_cnt;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 10, &relax_cnt);
	if (ret) {
		dev_err(data->dev, "fail to get relax_cnt info, ret = %d\n", ret);
		return count;
	}

	data->slp_cap_calib.relax_cnt_threshold = relax_cnt;

	dev_info(data->dev, "Try to set [relax_cnt_th] to [%d]\n",
		 data->slp_cap_calib.relax_cnt_threshold);

	return count;
}

static ssize_t sc27xx_fgu_relax_cur_th_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cur_th);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[relax_cur_th][%d]\n",
			data->slp_cap_calib.relax_cur_threshold);
}

static ssize_t sc27xx_fgu_relax_cur_th_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cur_th);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 relax_cur;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 10, &relax_cur);
	if (ret) {
		dev_err(data->dev, "fail to get relax_cnt info, ret = %d\n", ret);
		return count;
	}

	data->slp_cap_calib.relax_cur_threshold = relax_cur;

	dev_info(data->dev, "Try to set [relax_cur_th] to [%d]\n",
		 data->slp_cap_calib.relax_cur_threshold);

	return count;
}

static int sc27xx_fgu_register_sysfs(struct sc27xx_fgu_data *data)
{
	struct sc27xx_fgu_sysfs *sysfs;
	int ret;

	sysfs = devm_kzalloc(data->dev, sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs)
		return -ENOMEM;

	data->sysfs = sysfs;
	sysfs->data = data;
	sysfs->name = "sc27xx_fgu_sysfs";
	sysfs->attrs[0] = &sysfs->attr_sc27xx_fgu_dump_info.attr;
	sysfs->attrs[1] = &sysfs->attr_sc27xx_fgu_sel_reg_id.attr;
	sysfs->attrs[2] = &sysfs->attr_sc27xx_fgu_reg_val.attr;
	sysfs->attrs[3] = &sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr;
	sysfs->attrs[4] = &sysfs->attr_sc27xx_fgu_relax_cnt_th.attr;
	sysfs->attrs[5] = &sysfs->attr_sc27xx_fgu_relax_cur_th.attr;
	sysfs->attrs[6] = NULL;
	sysfs->attr_g.name = "debug";
	sysfs->attr_g.attrs = sysfs->attrs;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_dump_info.attr);
	sysfs->attr_sc27xx_fgu_dump_info.attr.name = "dump_info";
	sysfs->attr_sc27xx_fgu_dump_info.attr.mode = 0444;
	sysfs->attr_sc27xx_fgu_dump_info.show = sc27xx_fgu_dump_info_show;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_sel_reg_id.attr);
	sysfs->attr_sc27xx_fgu_sel_reg_id.attr.name = "sel_reg_id";
	sysfs->attr_sc27xx_fgu_sel_reg_id.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_sel_reg_id.show = sc27xx_fgu_sel_reg_id_show;
	sysfs->attr_sc27xx_fgu_sel_reg_id.store = sc27xx_fgu_sel_reg_id_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_reg_val.attr);
	sysfs->attr_sc27xx_fgu_reg_val.attr.name = "reg_val";
	sysfs->attr_sc27xx_fgu_reg_val.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_reg_val.show = sc27xx_fgu_reg_val_show;
	sysfs->attr_sc27xx_fgu_reg_val.store = sc27xx_fgu_reg_val_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr);
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr.name = "enable_sleep_calib";
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.show = sc27xx_fgu_enable_sleep_calib_show;
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.store = sc27xx_fgu_enable_sleep_calib_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_relax_cnt_th.attr);
	sysfs->attr_sc27xx_fgu_relax_cnt_th.attr.name = "relax_cnt_th";
	sysfs->attr_sc27xx_fgu_relax_cnt_th.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_relax_cnt_th.show = sc27xx_fgu_relax_cnt_th_show;
	sysfs->attr_sc27xx_fgu_relax_cnt_th.store = sc27xx_fgu_relax_cnt_th_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_relax_cur_th.attr);
	sysfs->attr_sc27xx_fgu_relax_cur_th.attr.name = "relax_cur_th";
	sysfs->attr_sc27xx_fgu_relax_cur_th.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_relax_cur_th.show = sc27xx_fgu_relax_cur_th_show;
	sysfs->attr_sc27xx_fgu_relax_cur_th.store = sc27xx_fgu_relax_cur_th_store;

	ret = sysfs_create_group(&data->battery->dev.kobj, &sysfs->attr_g);
	if (ret < 0)
		dev_err(data->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int sc27xx_fgu_parse_sprd_battery_info(struct sc27xx_fgu_data *data,
					      struct sprd_battery_info *info)
{
	struct sprd_battery_ocv_table *table;
	int i;

	/*
	 * For SC27XX fuel gauge device, we only use one ocv-capacity
	 * table in normal temperature 20 Celsius.
	 */
	table = sprd_battery_find_ocv2cap_table(info, 20, &data->table_len);
	if (!table)
		return -EINVAL;

	data->cap_table = devm_kmemdup(data->dev, table,
				       data->table_len * sizeof(*table),
				       GFP_KERNEL);
	if (!data->cap_table)
		return -ENOMEM;

	/*
	 * We should give a initial temperature value of temp_buff.
	 */
	data->temp_buff[0] = -500;

	data->temp_table_len = info->battery_vol_temp_table_len;
	if (data->temp_table_len > 0) {
		data->temp_table = devm_kmemdup(data->dev, info->battery_vol_temp_table,
						data->temp_table_len *
						sizeof(struct sprd_battery_vol_temp_table),
						GFP_KERNEL);
		if (!data->temp_table)
			return -ENOMEM;
	}

	data->cap_table_len = info->battery_temp_cap_table_len;
	if (data->cap_table_len > 0) {
		data->cap_temp_table = devm_kmemdup(data->dev, info->battery_temp_cap_table,
						    data->cap_table_len *
						    sizeof(struct sprd_battery_temp_cap_table),
						    GFP_KERNEL);
		if (!data->cap_temp_table)
			return -ENOMEM;
	}

	data->resist_table_len = info->battery_temp_resist_table_len;
	if (data->resist_table_len > 0) {
		data->resist_table = devm_kmemdup(data->dev, info->battery_temp_resist_table,
						  data->resist_table_len *
						  sizeof(struct sprd_battery_resistance_temp_table),
						  GFP_KERNEL);
		if (!data->resist_table)
			return -ENOMEM;
	}

	data->rbat_temp_table_len = info->battery_internal_resistance_temp_table_len;
	if (data->rbat_temp_table_len > 0) {
		data->rbat_temp_table =
			devm_kmemdup(data->dev,
				     info->battery_internal_resistance_temp_table,
				     (u32)data->rbat_temp_table_len * sizeof(int), GFP_KERNEL);
		if (!data->rbat_temp_table)
			return -ENOMEM;
	}

	data->rbat_ocv_table_len = info->battery_internal_resistance_ocv_table_len;
	if (data->rbat_ocv_table_len > 0) {
		data->rbat_ocv_table =
			devm_kmemdup(data->dev,
				     info->battery_internal_resistance_ocv_table,
				     (u32)data->rbat_ocv_table_len * sizeof(int), GFP_KERNEL);
		if (!data->rbat_ocv_table)
			return -ENOMEM;
	}

	data->rabat_table_len = info->battery_internal_resistance_table_len[0];
	if (data->rabat_table_len > 0) {
		data->rbat_table = devm_kzalloc(data->dev,
						(u32)data->rbat_temp_table_len * sizeof(int *),
						GFP_KERNEL);
		if (!data->rbat_table) {
			dev_err(data->dev, "Fail to alloc rbat_table\n");
			return -ENOMEM;
		}
		for (i = 0; i < data->rbat_temp_table_len; i++) {
			data->rbat_table[i] =
				devm_kmemdup(data->dev,
					     info->battery_internal_resistance_table[i],
					     (u32)data->rabat_table_len * sizeof(int), GFP_KERNEL);
			if (!data->rbat_table[i]) {
				dev_err(data->dev, "data->rbat_table[%d]\n", i);
				return -ENOMEM;
			}
		}
	}

	if (data->rabat_table_len > 0) {
		data->target_rbat_table = devm_kzalloc(data->dev,
						       (u32)data->rabat_table_len * sizeof(int),
						       GFP_KERNEL);
		if (!data->target_rbat_table) {
			dev_err(data->dev, "Fail to alloc resist_table\n");
			return -ENOMEM;
		}
	}

	if (data->rbat_temp_table_len > 0 && data->rabat_table_len > 0 &&
	    data->rbat_ocv_table_len > 0 && data->rbat_temp_table &&
	    data->rbat_ocv_table && data->rbat_table && data->target_rbat_table)
		data->support_multi_resistance = true;

	data->cap_calib_dens_ocv_table_len = info->cap_calib_dens_ocv_table_len;
	if (data->cap_calib_dens_ocv_table_len > 0) {
		data->cap_calib_dens_ocv_table =
			devm_kmemdup(data->dev, info->cap_calib_dens_ocv_table,
				     (u32)data->cap_calib_dens_ocv_table_len *
				     sizeof(density_ocv_table),
				     GFP_KERNEL);
		if (!data->cap_calib_dens_ocv_table) {
			dev_err(data->dev, "data->cap_calib_dens_ocv_table is null\n");
			return -ENOMEM;
		}
	}

	data->track.dens_ocv_table_len = info->cap_track_dens_ocv_table_len;
	if (data->track.dens_ocv_table_len > 0) {
		data->track.dens_ocv_table =
			devm_kmemdup(data->dev, info->cap_track_dens_ocv_table,
				     (u32)data->track.dens_ocv_table_len * sizeof(density_ocv_table),
				     GFP_KERNEL);
		if (!data->track.dens_ocv_table) {
			dev_err(data->dev, "data->track.dens_ocv_table is null\n");
			return -ENOMEM;
		}
	}

	data->basp_full_design_table_len = info->basp_charge_full_design_uah_table_len;
	if (data->basp_full_design_table_len > 0) {
		data->basp_full_design_table =
			devm_kmemdup(data->dev, info->basp_charge_full_design_uah_table,
				     data->basp_full_design_table_len * sizeof(int), GFP_KERNEL);
		if (!data->basp_full_design_table) {
			dev_err(data->dev, "data->basp_full_design_table is null\n");
			return -ENOMEM;
		}
	}

	data->basp_voltage_max_table_len = info->basp_constant_charge_voltage_max_uv_table_len;
	if (data->basp_voltage_max_table_len > 0) {
		data->basp_voltage_max_table =
			devm_kmemdup(data->dev, info->basp_constant_charge_voltage_max_uv_table,
				     data->basp_voltage_max_table_len * sizeof(int), GFP_KERNEL);
		if (!data->basp_voltage_max_table) {
			dev_err(data->dev, "data->basp_voltage_max_table is null\n");
			return -ENOMEM;
		}
	}

	data->basp_ocv_table_len = info->basp_ocv_table_len[0];
	if (data->basp_ocv_table_len > 0) {
		data->basp_ocv_table =
			devm_kzalloc(data->dev, data->basp_voltage_max_table_len * sizeof(int *),
				     GFP_KERNEL);

		if (!data->basp_ocv_table) {
			dev_err(data->dev, "Fail to alloc basp_ocv_table\n");
			return -ENOMEM;
		}

		for (i = 0; i < data->basp_voltage_max_table_len; i++) {
			data->basp_ocv_table[i] =
				devm_kmemdup(data->dev, info->basp_ocv_table[i],
					     data->basp_ocv_table_len *
					     sizeof(struct sprd_battery_ocv_table),
					     GFP_KERNEL);
			if (!data->basp_ocv_table[i]) {
				dev_err(data->dev, "data->basp_ocv_table[%d]\n", i);
				return -ENOMEM;
			}
		}
	}

	if (info->fullbatt_track_end_voltage_uv > 0)
		data->track.end_vol = info->fullbatt_track_end_voltage_uv / 1000;
	else
		dev_warn(data->dev, "no fgu track.end_vol support\n");

	if (info->fullbatt_track_end_current_uA > 0)
		data->track.end_cur = info->fullbatt_track_end_current_uA / 1000;
	else
		dev_warn(data->dev, "no fgu track.end_cur support\n");

	if (info->first_capacity_calibration_voltage_uv > 0)
		data->first_calib_volt = info->first_capacity_calibration_voltage_uv;
	else
		dev_warn(data->dev, "no fgu first_calib_volt support\n");

	if (info->batt_ovp_threshold_uv > 0)
		data->batt_ovp_threshold = info->batt_ovp_threshold_uv / 1000;
	else
		dev_warn(data->dev, "no fgu battery ovp threshold support\n");

	if (info->first_capacity_calibration_capacity > 0)
		data->first_calib_cap = info->first_capacity_calibration_capacity;
	else
		dev_warn(data->dev, "no fgu first_calib_cap support\n");

	if (info->charge_full_design_uah > 0)
		data->total_mah = info->charge_full_design_uah / 1000;
	else
		dev_warn(data->dev, "no fgu charge_full_design_uah support\n");

	data->design_mah = data->total_mah;

	if (info->constant_charge_voltage_max_uv > 0)
		data->max_volt_uv = info->constant_charge_voltage_max_uv;
	else
		dev_warn(data->dev, "no fgu constant_charge_voltage_max_uv support\n");

	if (info->factory_internal_resistance_uohm > 0)
		data->internal_resist = info->factory_internal_resistance_uohm / 1000;
	else
		dev_warn(data->dev, "no fgu factory_internal_resistance_uohm support\n");

	if (info->voltage_min_design_uv > 0)
		data->min_volt_uv = info->voltage_min_design_uv;
	else
		dev_warn(data->dev, "no fgu voltage_min_design_uv support\n");

	if (data->support_debug_log)
		sc27xx_fgu_dump_battery_info(data, "parse_resistance_table");

	return 0;
}

static int sc27xx_fgu_hw_init(struct sc27xx_fgu_data *data,
			      const struct sc27xx_fgu_variant_data *pdata)
{
	int ret, delta_clbcnt, alarm_adc;
	struct sprd_battery_info info = {};

	data->cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT - 1] = SC27XX_FGU_MAGIC_NUMBER;

	ret = sprd_battery_get_battery_info(data->battery, &info);
	if (ret) {
		sprd_battery_put_battery_info(data->battery, &info);
		dev_err(data->dev, "failed to get sprd battery information\n");
		return ret;
	}

	ret = sc27xx_fgu_parse_sprd_battery_info(data, &info);
	sprd_battery_put_battery_info(data->battery, &info);
	if (ret) {
		dev_err(data->dev, "failed to parse battery information, ret = %d\n", ret);
		return ret;
	}

	sc27xx_fgu_parse_cmdline(data);

	data->alarm_cap = sc27xx_fgu_ocv2cap(data->cap_table, data->table_len, data->min_volt_uv);
	/*
	 * We must keep the alarm capacity is larger than 0%. When in monkey
	 * test, the precision power supply setting 4000mv, but the fake battery
	 * has been simulated into a real battery. Due to it has been discharging,
	 * the battery capacity has been decreasing, finally will reach 0%, so upper
	 * layer will issue a command to shutdown. we in order to prevent such problem,
	 * we determine if the ocv voltage is greater than data->min_volt_uv and cap is
	 * small alarm capacity. We will recalculate the battery capacity based on ocv voltage.
	 */
	if (data->alarm_cap < 10)
		data->alarm_cap = 10;

	ret = sc27xx_fgu_calibration(data);
	if (ret)
		return ret;

	/* Enable the FGU module */
	ret = regmap_update_bits(data->regmap, pdata->module_en,
				 SC27XX_FGU_EN, SC27XX_FGU_EN);
	if (ret) {
		dev_err(data->dev, "failed to enable fgu\n");
		return ret;
	}

	/* Enable the FGU RTC clock to make it work */
	ret = regmap_update_bits(data->regmap, pdata->clk_en,
				 SC27XX_FGU_RTC_EN, SC27XX_FGU_RTC_EN);
	if (ret) {
		dev_err(data->dev, "failed to enable fgu RTC clock\n");
		goto disable_fgu;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				 SC27XX_FGU_INT_MASK, SC27XX_FGU_INT_MASK);
	if (ret) {
		dev_err(data->dev, "failed to clear interrupt status\n");
		goto disable_clk;
	}

	/*
	 * Set the voltage low overload threshold, which means when the battery
	 * voltage is lower than this threshold, the controller will generate
	 * one interrupt to notify.
	 */
	alarm_adc = sc27xx_fgu_voltage2adc(data, data->min_volt_uv / 1000);
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_LOW_OVERLOAD,
				 SC27XX_FGU_LOW_OVERLOAD_MASK, alarm_adc);
	if (ret) {
		dev_err(data->dev, "failed to set fgu low overload\n");
		goto disable_clk;
	}

	/*
	 * Set the coulomb counter delta threshold, that means when the coulomb
	 * counter change is multiples of the delta threshold, the controller
	 * will generate one interrupt to notify the users to update the battery
	 * capacity. Now we set the delta threshold as a counter value of 1%
	 * capacity.
	 */
	delta_clbcnt = sc27xx_fgu_cap2clbcnt(data, 10);

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CLBCNT_DELTL,
				 SC27XX_FGU_CLBCNT_MASK, delta_clbcnt);
	if (ret) {
		dev_err(data->dev, "failed to set low delta coulomb counter\n");
		goto disable_clk;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CLBCNT_DELTH,
				 SC27XX_FGU_CLBCNT_MASK,
				 delta_clbcnt >> SC27XX_FGU_CLBCNT_SHIFT);
	if (ret) {
		dev_err(data->dev, "failed to set high delta coulomb counter\n");
		goto disable_clk;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CONFIG,
				 SC27XX_FGU_LOW_POWER_MODE, SC27XX_FGU_RELAX_CNT_MODE);
	if (ret) {
		dev_err(data->dev, "Fail to enable RELAX_CNT_MODE, re= %d\n", ret);
		goto disable_clk;
	}

	ret = sc27xx_fgu_get_temp(data, &data->bat_temp);
	if (ret) {
		dev_err(data->dev, "failed to get battery temperature\n");
		goto disable_clk;
	}

	/*
	 * Get the boot battery capacity when system powers on, which is used to
	 * initialize the coulomb counter. After that, we can read the coulomb
	 * counter to measure the battery capacity.
	 */
	ret = sc27xx_fgu_get_boot_capacity(data, &data->init_cap);
	if (ret) {
		dev_err(data->dev, "failed to get boot capacity\n");
		goto disable_clk;
	}
	if (data->batt_ovp_threshold) {
		ret = sc27xx_fgu_batt_ovp_threshold_config(data);
		if (ret) {
			dev_err(data->dev, "failed to set overload thershold config\n");
			goto disable_clk;
		}
	}
	/*
	 * Convert battery capacity to the corresponding initial coulomb counter
	 * and set into coulomb counter registers.
	 */
	data->init_clbcnt = sc27xx_fgu_cap2clbcnt(data, data->init_cap);
	data->last_clbcnt = data->cur_clbcnt = data->init_clbcnt;
	ret = sc27xx_fgu_set_clbcnt(data, data->init_clbcnt);
	if (ret) {
		dev_err(data->dev, "failed to initialize coulomb counter\n");
		goto disable_clk;
	}

	return 0;

disable_clk:
	regmap_update_bits(data->regmap, pdata->clk_en, SC27XX_FGU_RTC_EN, 0);
disable_fgu:
	regmap_update_bits(data->regmap, pdata->module_en, SC27XX_FGU_EN, 0);

	return ret;
}

static int sc27xx_fgu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct power_supply_config fgu_cfg = { };
	struct sc27xx_fgu_data *data;
	int ret, irq;

	if (!dev) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	np = pdev->dev.of_node;
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->normal_temp_cap = SC27XX_FGU_MAGIC_NUMBER;
	data->chg_sts = POWER_SUPPLY_STATUS_DISCHARGING;

	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);

	data->pdata = of_device_get_match_data(&pdev->dev);
	if (!data->pdata) {
		dev_err(&pdev->dev, "no matching driver data found\n");
		return -EINVAL;
	}

	data->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}

	data->track.cap_tracking = device_property_read_bool(dev, "fgu-capacity-track");
	if (data->track.cap_tracking) {
		data->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
		if (IS_ERR(data->usb_phy)) {
			dev_err(dev, "failed to find USB phy, ret = %ld\n",
				PTR_ERR(data->usb_phy));
			return -EPROBE_DEFER;
		}
	}

	data->channel = devm_iio_channel_get(dev, "bat-temp");
	if (IS_ERR(data->channel)) {
		dev_err(dev, "failed to get IIO channel, ret = %ld\n", PTR_ERR(data->channel));
		return -ENXIO;
	}

	data->charge_chan = devm_iio_channel_get(dev, "charge-vol");
	if (IS_ERR(data->charge_chan)) {
		dev_err(dev, "failed to get charge IIO channel, ret = %ld\n",
			PTR_ERR(data->charge_chan));
		return -ENXIO;
	}

	ret = device_property_read_u32(dev, "reg", &data->base);
	if (ret) {
		dev_err(dev, "failed to get fgu address\n");
		return ret;
	}

	ret = device_property_read_u32(dev, "sprd,calib-resistance-micro-ohms",
				       &data->calib_resist);
	if (ret) {
		dev_err(dev, "failed to get fgu calibration resistance\n");
		return ret;
	}

	ret = device_property_read_u32(dev,
				       "sprd,comp-resistance-mohm",
				       &data->comp_resistance);
	if (ret)
		dev_warn(dev, "no fgu compensated resistance support\n");

	data->slp_cap_calib.support_slp_calib =
		device_property_read_bool(dev, "sprd,capacity-sleep-calibration");
	if (!data->slp_cap_calib.support_slp_calib) {
		dev_warn(&pdev->dev, "Do not support sleep calibration function\n");
	} else {
		ret = device_property_read_u32(dev, "sprd,relax-counter-threshold",
					       &data->slp_cap_calib.relax_cnt_threshold);
		if (ret)
			dev_warn(dev, "no relax-counter-threshold support\n");

		ret = device_property_read_u32(dev, "sprd,relax-current-threshold",
					       &data->slp_cap_calib.relax_cur_threshold);
		if (ret)
			dev_warn(dev, "no relax_current_threshold support\n");

		if (data->slp_cap_calib.relax_cnt_threshold < SC27XX_FGU_RELAX_CNT_THRESHOLD)
			data->slp_cap_calib.relax_cnt_threshold = SC27XX_FGU_RELAX_CNT_THRESHOLD;

		if (data->slp_cap_calib.relax_cur_threshold == 0)
			data->slp_cap_calib.relax_cur_threshold = SC27XX_FGU_RELAX_CUR_THRESHOLD_MA;
	}

	data->support_boot_calib =
		device_property_read_bool(&pdev->dev, "sprd,capacity-boot-calibration");
	if (!data->support_boot_calib)
		dev_info(&pdev->dev, "Do not support boot calibration function\n");

	ret = get_boot_mode(data);
	if (ret)
		dev_warn(dev, "get_boot_mode can't not parse bootargs property\n");

	data->support_charge_cycle =
		device_property_read_bool(&pdev->dev, "sprd,capacity-charge-cycle");
	if (!data->support_charge_cycle)
		dev_info(&pdev->dev, "Do not support charge cycle function\n");
	data->support_basp =
		device_property_read_bool(&pdev->dev, "sprd,basp");
	if (!data->support_basp)
		dev_info(&pdev->dev, "Do not support basp function\n");

	data->gpiod = devm_gpiod_get(&pdev->dev, "bat-detect", GPIOD_IN);
	if (IS_ERR(data->gpiod)) {
		dev_err(dev, "failed to get battery detection GPIO\n");
		return -ENXIO;
	}

	ret = gpiod_get_value_cansleep(data->gpiod);
	if (ret < 0) {
		dev_err(dev, "failed to get gpio state\n");
		return ret;
	}

	data->bat_present = !!ret;
	mutex_init(&data->lock);
	init_completion(&data->probe_init);

	fgu_cfg.drv_data = data;
	fgu_cfg.of_node = np;
	data->battery = devm_power_supply_register(dev, &sc27xx_fgu_desc, &fgu_cfg);
	if (IS_ERR(data->battery)) {
		dev_err(dev, "failed to register power supply");
		ret = -ENXIO;
		goto err;
	}

	ret = devm_add_action_or_reset(dev, sc27xx_fgu_disable, data);
	if (ret) {
		dev_err(dev, "failed to add fgu disable action\n");
		goto err;
	}

	ret = sc27xx_fgu_hw_init(data, data->pdata);
	if (ret) {
		dev_err(dev, "failed to initialize fgu hardware\n");
		goto err;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no irq resource specified\n");
		ret = irq;
		goto err;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,	sc27xx_fgu_interrupt,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					pdev->name, data);
	if (ret) {
		dev_err(dev, "failed to request fgu IRQ\n");
		goto err;
	}

	irq = gpiod_to_irq(data->gpiod);
	if (irq < 0) {
		dev_err(dev, "failed to translate GPIO to IRQ\n");
		ret = irq;
		goto err;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,
					sc27xx_fgu_bat_detection,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					pdev->name, data);
	if (ret) {
		dev_err(dev, "failed to request IRQ\n");
		goto err;
	}

	device_init_wakeup(dev, true);
	pm_wakeup_event(data->dev, SC27XX_FGU_TRACK_WAKE_UP_MS);

	if (!data->track.cap_tracking || !data->track.end_vol || !data->track.end_cur) {
		dev_warn(dev, "Not support fgu track. cap_tracking = %d, end_vol = %d, end_cur = %d\n",
			 data->track.cap_tracking, data->track.end_vol, data->track.end_cur);
		data->track.cap_tracking = false;
	}

	/* init capacity track function */
	if (data->track.cap_tracking) {
		if (data->use_typec_extcon) {
			data->edev = extcon_get_edev_by_phandle(data->dev, 0);
			if (IS_ERR(data->edev)) {
				ret = PTR_ERR(data->edev);
				dev_err(dev, "failed to find vbus extcon device, ret = %d.\n", ret);
				ret = -EPROBE_DEFER;
				goto err;
			}
			INIT_WORK(&data->typec_extcon_work, sc27xx_fgu_typec_extcon_work);
			data->extcon_nb.notifier_call = sc27xx_fgu_extcon_event;
			ret = devm_extcon_register_notifier_all(dev, data->edev, &data->extcon_nb);
			if (ret) {
				dev_err(dev, "Can't register extcon, ret = %d\n", ret);
				goto err;
			}
		} else {
			data->usb_notify.notifier_call = sc27xx_fgu_usb_change;
			ret = usb_register_notifier(data->usb_phy, &data->usb_notify);
			if (ret) {
				dev_err(dev, "failed to register notifier:%d\n", ret);
				goto err;
			}
		}

		data->track.state = CAP_TRACK_INIT;
		dev_info(data->dev, "end_vol = %d, end_cur = %d\n",
			 data->track.end_vol, data->track.end_cur);
	}

	INIT_DELAYED_WORK(&data->fgu_work, sc27xx_fgu_work);
	INIT_DELAYED_WORK(&data->cap_track_work, sc27xx_fgu_cap_track_work);
	schedule_delayed_work(&data->fgu_work, 0);
	schedule_delayed_work(&data->cap_track_work, 0);

	ret = sc27xx_fgu_register_sysfs(data);
	if (ret)
		dev_err(&pdev->dev, "register sysfs fail, ret = %d\n", ret);

	data->probe_initialized = true;
	complete_all(&data->probe_init);
	dev_info(dev, "use_typec_extcon = %d\n", data->use_typec_extcon);
	dev_info(dev, "%s:line%d probe successfully\n", __func__, __LINE__);
	return 0;

err:
	sc27xx_fgu_disable(data);
	mutex_destroy(&data->lock);
	return ret;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int sc27xx_fgu_resume(struct device *dev)
{
	struct sc27xx_fgu_data *data = dev_get_drvdata(dev);
	int ret;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	sc27xx_fgu_suspend_calib_check(data);

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_LOW_OVERLOAD_INT |
				 SC27XX_FGU_CLBCNT_DELTA_INT, 0);
	if (ret) {
		dev_err(data->dev, "failed to disable fgu interrupts\n");
		return ret;
	}

	schedule_delayed_work(&data->fgu_work, 0);
	schedule_delayed_work(&data->cap_track_work, 0);

	return 0;
}

static int sc27xx_fgu_suspend(struct device *dev)
{
	struct sc27xx_fgu_data *data = dev_get_drvdata(dev);
	int ret, ocv_uv;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	/*
	 * If we are charging, then no need to enable the FGU interrupts to
	 * adjust the battery capacity.
	 */
	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING ||
	    data->chg_sts == POWER_SUPPLY_STATUS_FULL)
		return 0;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_LOW_OVERLOAD_INT,
				 SC27XX_FGU_LOW_OVERLOAD_INT);
	if (ret) {
		dev_err(data->dev, "failed to enable low voltage interrupt\n");
		return ret;
	}

	ret = sc27xx_fgu_get_vbat_ocv(data, &ocv_uv);
	if (ret)
		goto disable_int;

	ocv_uv *= 1000;

	/*
	 * If current OCV is less than the minimum voltage, we should enable the
	 * coulomb counter threshold interrupt to notify events to adjust the
	 * battery capacity.
	 */
	if (ocv_uv < data->min_volt_uv) {
		ret = regmap_update_bits(data->regmap,
					 data->base + SC27XX_FGU_INT_EN,
					 SC27XX_FGU_CLBCNT_DELTA_INT,
					 SC27XX_FGU_CLBCNT_DELTA_INT);
		if (ret) {
			dev_err(data->dev, "failed to enable coulomb threshold int\n");
			goto disable_int;
		}
	}

	cancel_delayed_work_sync(&data->fgu_work);
	cancel_delayed_work_sync(&data->cap_track_work);

	sc27xx_fgu_suspend_calib_config(data);

	return 0;

disable_int:
	regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
			   SC27XX_FGU_LOW_OVERLOAD_INT, 0);
	return ret;
}
#endif

static void sc27xx_fgu_shutdown(struct platform_device *pdev)
{
	struct sc27xx_fgu_data *data = platform_get_drvdata(pdev);

	if (!data)
		return;

	cancel_delayed_work_sync(&data->fgu_work);
	cancel_delayed_work_sync(&data->cap_track_work);
}

static const struct dev_pm_ops sc27xx_fgu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc27xx_fgu_suspend, sc27xx_fgu_resume)
};

static const struct of_device_id sc27xx_fgu_of_match[] = {
	{ .compatible = "sprd,sc2731-fgu", .data = &sc2731_info},
	{ .compatible = "sprd,sc2730-fgu", .data = &sc2730_info},
	{ .compatible = "sprd,sc2720-fgu", .data = &sc2720_info},
	{ .compatible = "sprd,ump9620-fgu", .data = &ump9620_info},
	{ }
};
MODULE_DEVICE_TABLE(of, sc27xx_fgu_of_match);

static struct platform_driver sc27xx_fgu_driver = {
	.shutdown = sc27xx_fgu_shutdown,
	.probe = sc27xx_fgu_probe,
	.driver = {
		.name = "sc27xx-fgu",
		.of_match_table = sc27xx_fgu_of_match,
		.pm = &sc27xx_fgu_pm_ops,
	}
};

module_platform_driver(sc27xx_fgu_driver);

MODULE_DESCRIPTION("Spreadtrum SC27XX PMICs Fual Gauge Unit Driver");
MODULE_LICENSE("GPL v2");
