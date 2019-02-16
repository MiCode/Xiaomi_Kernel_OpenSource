/* Copyright (c) 2014-2017, 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/regulator/qpnp-labibb-regulator.h>

#define QPNP_LABIBB_REGULATOR_DRIVER_NAME	"qcom,qpnp-labibb-regulator"

#define REG_REVISION_2			0x01
#define REG_PERPH_TYPE			0x04
#define REG_INT_RT_STS			0x10

#define QPNP_LAB_TYPE			0x24
#define QPNP_IBB_TYPE			0x20

/* Common register value for LAB/IBB */
#define REG_LAB_IBB_LCD_MODE		0x0
#define REG_LAB_IBB_AMOLED_MODE		BIT(7)
#define REG_LAB_IBB_SEC_ACCESS		0xD0
#define REG_LAB_IBB_SEC_UNLOCK_CODE	0xA5

/* LAB register offset definitions */
#define REG_LAB_STATUS1			0x08
#define REG_LAB_SWIRE_PGM_CTL		0x40
#define REG_LAB_VOLTAGE			0x41
#define REG_LAB_RING_SUPPRESSION_CTL	0x42
#define REG_LAB_LCD_AMOLED_SEL		0x44
#define REG_LAB_MODULE_RDY		0x45
#define REG_LAB_ENABLE_CTL		0x46
#define REG_LAB_PD_CTL			0x47
#define REG_LAB_CLK_DIV			0x48
#define REG_LAB_IBB_EN_RDY		0x49
#define REG_LAB_CURRENT_LIMIT		0x4B
#define REG_LAB_CURRENT_SENSE		0x4C
#define REG_LAB_PS_CTL			0x50
#define REG_LAB_RDSON_MNGMNT		0x53
#define REG_LAB_PRECHARGE_CTL		0x5E
#define REG_LAB_SOFT_START_CTL		0x5F
#define REG_LAB_SPARE_CTL		0x60
#define REG_LAB_MISC_CTL		0x60 /* PMI8998/PM660A */
#define REG_LAB_PFM_CTL			0x62

/* LAB registers for PM660A */
#define REG_LAB_VOUT_DEFAULT		0x44
#define REG_LAB_SW_HIGH_PSRR_CTL	0x70
#define REG_LAB_LDO_PD_CTL		0x78
#define REG_LAB_VPH_ENVELOP_CTL		0x7E

/* LAB register bits definitions */

/* REG_LAB_STATUS1 */
#define LAB_STATUS1_VREG_OK_BIT		BIT(7)
#define LAB_STATUS1_SC_DETECT_BIT	BIT(6)

/* REG_LAB_SWIRE_PGM_CTL */
#define LAB_EN_SWIRE_PGM_VOUT		BIT(7)
#define LAB_EN_SWIRE_PGM_PD		BIT(6)

/* REG_LAB_VOLTAGE */
#define LAB_VOLTAGE_OVERRIDE_EN		BIT(7)
#define LAB_VOLTAGE_SET_MASK		GENMASK(3, 0)

/* REG_LAB_RING_SUPPRESSION_CTL */
#define LAB_RING_SUPPRESSION_CTL_EN	BIT(7)

/* REG_LAB_MODULE_RDY */
#define LAB_MODULE_RDY_EN		BIT(7)

/* REG_LAB_ENABLE_CTL */
#define LAB_ENABLE_CTL_EN		BIT(7)

/* REG_LAB_PD_CTL */
#define LAB_PD_CTL_STRONG_PULL		BIT(0)
#define LAB_PD_CTL_STRENGTH_MASK	BIT(0)
#define LAB_PD_CTL_DISABLE_PD		BIT(1)
#define LAB_PD_CTL_EN_MASK		BIT(1)

/* REG_LAB_IBB_EN_RDY */
#define LAB_IBB_EN_RDY_EN		BIT(7)

/* REG_LAB_CURRENT_LIMIT */
#define LAB_CURRENT_LIMIT_MASK		GENMASK(2, 0)
#define LAB_CURRENT_LIMIT_EN_BIT	BIT(7)
#define LAB_OVERRIDE_CURRENT_MAX_BIT	BIT(3)

/* REG_LAB_CURRENT_SENSE */
#define LAB_CURRENT_SENSE_GAIN_MASK	GENMASK(1, 0)

/* REG_LAB_PS_CTL */
#define LAB_PS_THRESH_MASK		GENMASK(1, 0)
#define LAB_PS_CTL_EN			BIT(7)

/* REG_LAB_RDSON_MNGMNT */
#define LAB_RDSON_MNGMNT_NFET_SLEW_EN	BIT(5)
#define LAB_RDSON_MNGMNT_PFET_SLEW_EN	BIT(4)
#define LAB_RDSON_MNGMNT_NFET_MASK	GENMASK(3, 2)
#define LAB_RDSON_MNGMNT_NFET_SHIFT	2
#define LAB_RDSON_MNGMNT_PFET_MASK	GENMASK(1, 0)
#define LAB_RDSON_NFET_SW_SIZE_QUARTER	0x0
#define LAB_RDSON_PFET_SW_SIZE_QUARTER	0x0

/* REG_LAB_PRECHARGE_CTL */
#define LAB_FAST_PRECHARGE_CTL_EN	BIT(2)
#define LAB_MAX_PRECHARGE_TIME_MASK	GENMASK(1, 0)

/* REG_LAB_SOFT_START_CTL */
#define LAB_SOFT_START_CTL_MASK		GENMASK(1, 0)

/* REG_LAB_SPARE_CTL */
#define LAB_SPARE_TOUCH_WAKE_BIT	BIT(3)
#define LAB_SPARE_DISABLE_SCP_BIT	BIT(0)

/* REG_LAB_MISC_CTL */
#define LAB_AUTO_GM_BIT			BIT(4)

/* REG_LAB_PFM_CTL */
#define LAB_PFM_EN_BIT			BIT(7)

/* REG_LAB_SW_HIGH_PSRR_CTL */
#define LAB_EN_SW_HIGH_PSRR_MODE	BIT(7)
#define LAB_SW_HIGH_PSRR_REQ		BIT(0)

/* REG_LAB_VPH_ENVELOP_CTL */
#define LAB_VREF_HIGH_PSRR_SEL_MASK	GENMASK(7, 6)
#define LAB_SEL_HW_HIGH_PSRR_SRC_MASK	GENMASK(1, 0)
#define LAB_SEL_HW_HIGH_PSRR_SRC_SHIFT	6

/* IBB register offset definitions */
#define REG_IBB_REVISION4		0x03
#define REG_IBB_STATUS1			0x08
#define REG_IBB_VOLTAGE			0x41
#define REG_IBB_RING_SUPPRESSION_CTL	0x42
#define REG_IBB_LCD_AMOLED_SEL		0x44
#define REG_IBB_MODULE_RDY		0x45
#define REG_IBB_ENABLE_CTL		0x46
#define REG_IBB_PD_CTL			0x47
#define REG_IBB_CLK_DIV			0x48
#define REG_IBB_CURRENT_LIMIT		0x4B
#define REG_IBB_PS_CTL			0x50
#define REG_IBB_RDSON_MNGMNT		0x53
#define REG_IBB_NONOVERLAP_TIME_1	0x56
#define REG_IBB_NONOVERLAP_TIME_2	0x57
#define REG_IBB_PWRUP_PWRDN_CTL_1	0x58
#define REG_IBB_PWRUP_PWRDN_CTL_2	0x59
#define REG_IBB_SOFT_START_CTL		0x5F
#define REG_IBB_SWIRE_CTL		0x5A
#define REG_IBB_OUTPUT_SLEW_CTL		0x5D
#define REG_IBB_SPARE_CTL		0x60
#define REG_IBB_NLIMIT_DAC		0x61

/* IBB registers for PM660A */
#define REG_IBB_DEFAULT_VOLTAGE		0x40
#define REG_IBB_FLOAT_CTL		0x43
#define REG_IBB_VREG_OK_CTL		0x55
#define REG_IBB_VOUT_MIN_MAGNITUDE	0x5C
#define REG_IBB_PFM_CTL			0x62
#define REG_IBB_SMART_PS_CTL		0x65
#define REG_IBB_ADAPT_DEAD_TIME		0x67

/* IBB register bits definition */

/* REG_IBB_STATUS1 */
#define IBB_STATUS1_VREG_OK_BIT		BIT(7)
#define IBB_STATUS1_SC_DETECT_BIT	BIT(6)

/* REG_IBB_VOLTAGE */
#define IBB_VOLTAGE_OVERRIDE_EN		BIT(7)
#define IBB_VOLTAGE_SET_MASK		GENMASK(5, 0)

/* REG_IBB_CLK_DIV */
#define IBB_CLK_DIV_OVERRIDE_EN		BIT(7)
#define IBB_CLK_DIV_MASK		GENMASK(3, 0)

/* REG_IBB_RING_SUPPRESSION_CTL */
#define IBB_RING_SUPPRESSION_CTL_EN	BIT(7)

/* REG_IBB_FLOAT_CTL */
#define IBB_FLOAT_EN			BIT(0)
#define IBB_SMART_FLOAT_EN		BIT(7)

/* REG_IBB_MIN_MAGNITUDE */
#define IBB_MIN_VOLTAGE_0P8_V		BIT(3)

/* REG_IBB_MODULE_RDY */
#define IBB_MODULE_RDY_EN		BIT(7)

/* REG_IBB_ENABLE_CTL */
#define IBB_ENABLE_CTL_MASK		(BIT(7) | BIT(6))
#define IBB_ENABLE_CTL_SWIRE_RDY	BIT(6)
#define IBB_ENABLE_CTL_MODULE_EN	BIT(7)

/* REG_IBB_PD_CTL */
#define IBB_PD_CTL_HALF_STRENGTH	BIT(0)
#define IBB_PD_CTL_STRENGTH_MASK	BIT(0)
#define IBB_PD_CTL_EN			BIT(7)
#define IBB_SWIRE_PD_UPD		BIT(1)
#define IBB_PD_CTL_EN_MASK		BIT(7)

/* REG_IBB_CURRENT_LIMIT */
#define IBB_CURRENT_LIMIT_MASK		GENMASK(4, 0)
#define IBB_CURRENT_LIMIT_DEBOUNCE_SHIFT	5
#define IBB_CURRENT_LIMIT_DEBOUNCE_MASK	GENMASK(6, 5)
#define IBB_CURRENT_LIMIT_EN		BIT(7)
#define IBB_ILIMIT_COUNT_CYC8		0
#define IBB_CURRENT_MAX_500MA		0xA

/* REG_IBB_PS_CTL */
#define IBB_PS_CTL_EN			0x85

/* REG_IBB_SMART_PS_CTL */
#define IBB_SMART_PS_CTL_EN			BIT(7)
#define IBB_NUM_SWIRE_PULSE_WAIT		0x5

/* REG_IBB_OUTPUT_SLEW_CTL */
#define IBB_SLEW_CTL_EN				BIT(7)
#define IBB_SLEW_RATE_SPEED_FAST_EN		BIT(6)
#define IBB_SLEW_RATE_TRANS_TIME_FAST_SHIFT	3
#define IBB_SLEW_RATE_TRANS_TIME_FAST_MASK	GENMASK(5, 3)
#define IBB_SLEW_RATE_TRANS_TIME_SLOW_MASK	GENMASK(2, 0)

/* REG_IBB_VREG_OK_CTL */
#define IBB_VREG_OK_EN_OVERLOAD_BLANK		BIT(7)
#define IBB_VREG_OK_OVERLOAD_DEB_SHIFT		5
#define IBB_VREG_OK_OVERLOAD_DEB_MASK		GENMASK(6, 5)

/* REG_IBB_RDSON_MNGMNT */
#define IBB_NFET_SLEW_EN		BIT(7)
#define IBB_PFET_SLEW_EN		BIT(6)
#define IBB_OVERRIDE_NFET_SW_SIZE	BIT(5)
#define IBB_OVERRIDE_PFET_SW_SIZE	BIT(2)
#define IBB_NFET_SW_SIZE_MASK		GENMASK(3, 2)
#define IBB_PFET_SW_SIZE_MASK		GENMASK(1, 0)

/* REG_IBB_NONOVERLAP_TIME_1 */
#define IBB_OVERRIDE_NONOVERLAP		BIT(6)
#define IBB_NONOVERLAP_NFET_MASK	GENMASK(2, 0)
#define IBB_NFET_GATE_DELAY_2		0x3

/* REG_IBB_NONOVERLAP_TIME_2 */
#define IBB_N2P_MUX_SEL		BIT(0)

/* REG_IBB_SOFT_START_CTL */
#define IBB_SOFT_START_CHARGING_RESISTOR_16K	0x3

/* REG_IBB_SPARE_CTL */
#define IBB_BYPASS_PWRDN_DLY2_BIT	BIT(5)
#define IBB_POFF_CTL_MASK		BIT(4)
#define IBB_FASTER_PFET_OFF		BIT(4)
#define IBB_FAST_STARTUP		BIT(3)

/* REG_IBB_SWIRE_CTL */
#define IBB_SWIRE_VOUT_UPD_EN		BIT(6)
#define IBB_OUTPUT_VOLTAGE_AT_ONE_PULSE_MASK	GENMASK(5, 0)
#define MAX_OUTPUT_EDGE_VOLTAGE_MV	6300
#define MAX_OUTPUT_PULSE_VOLTAGE_MV	7700
#define MIN_OUTPUT_PULSE_VOLTAGE_MV	1400
#define OUTPUT_VOLTAGE_STEP_MV		100

/* REG_IBB_NLIMIT_DAC */
#define IBB_DEFAULT_NLIMIT_DAC		0x5

/* REG_IBB_PFM_CTL */
#define IBB_PFM_ENABLE			BIT(7)
#define IBB_PFM_PEAK_CURRENT_BIT_SHIFT	1
#define IBB_PFM_PEAK_CURRENT_MASK	GENMASK(3, 1)
#define IBB_PFM_HYSTERESIS_BIT_SHIFT	4
#define IBB_PFM_HYSTERESIS_MASK		GENMASK(5, 4)

/* REG_IBB_PWRUP_PWRDN_CTL_1 */
#define IBB_PWRUP_PWRDN_CTL_1_DLY1_BITS	2
#define IBB_PWRUP_PWRDN_CTL_1_DLY1_MASK	GENMASK(5, 4)
#define IBB_PWRUP_PWRDN_CTL_1_DLY1_SHIFT	4
#define IBB_PWRUP_PWRDN_CTL_1_EN_DLY2	BIT(3)
#define IBB_PWRUP_PWRDN_CTL_1_DLY2_MASK	GENMASK(1, 0)
#define IBB_PWRUP_PWRDN_CTL_1_LAB_VREG_OK	BIT(7)
#define IBB_PWRUP_PWRDN_CTL_1_EN_DLY1	BIT(6)
#define PWRUP_PWRDN_CTL_1_DISCHARGE_EN	BIT(2)

/* REG_IBB_PWRUP_PWRDN_CTL_2 */
#define IBB_DIS_DLY_MASK		GENMASK(1, 0)
#define IBB_WAIT_MBG_OK			BIT(2)

/* Constants */
#define SWIRE_DEFAULT_2ND_CMD_DLY_MS		20
#define SWIRE_DEFAULT_IBB_PS_ENABLE_DLY_MS	200
#define IBB_HW_DEFAULT_SLEW_RATE		12000

/**
 * enum qpnp_labibb_mode - working mode of LAB/IBB regulators
 * %QPNP_LABIBB_LCD_MODE:		configure LAB and IBB regulators
 * together to provide power supply for LCD
 * %QPNP_LABIBB_AMOLED_MODE:		configure LAB and IBB regulators
 * together to provide power supply for AMOLED
 * %QPNP_LABIBB_MAX_MODE		max number of configureable modes
 * supported by qpnp_labibb_regulator
 */
enum qpnp_labibb_mode {
	QPNP_LABIBB_LCD_MODE,
	QPNP_LABIBB_AMOLED_MODE,
	QPNP_LABIBB_MAX_MODE,
};

/**
 * IBB_SW_CONTROL_EN: Specifies IBB is enabled through software.
 * IBB_SW_CONTROL_DIS: Specifies IBB is disabled through software.
 * IBB_HW_CONTROL: Specifies IBB is controlled through SWIRE (hardware).
 */
enum ibb_mode {
	IBB_SW_CONTROL_EN,
	IBB_SW_CONTROL_DIS,
	IBB_HW_CONTROL,
	IBB_HW_SW_CONTROL,
};

static const int ibb_dischg_res_table[] = {
	300,
	64,
	32,
	16,
};

static const int ibb_pwrup_dly_table[] = {
	1000,
	2000,
	4000,
	8000,
};

static const int ibb_pwrdn_dly_table[] = {
	1000,
	2000,
	4000,
	8000,
};

static const int lab_clk_div_table[] = {
	3200,
	2740,
	2400,
	2130,
	1920,
	1750,
	1600,
	1480,
	1370,
	1280,
	1200,
	1130,
	1070,
	1010,
	960,
	910,
};

static const int ibb_clk_div_table[] = {
	3200,
	2740,
	2400,
	2130,
	1920,
	1750,
	1600,
	1480,
	1370,
	1280,
	1200,
	1130,
	1070,
	1010,
	960,
	910,
};

static const int lab_current_limit_table[] = {
	200,
	400,
	600,
	800,
	1000,
	1200,
	1400,
	1600,
};

static const char * const lab_current_sense_table[] = {
	"0.5x",
	"1x",
	"1.5x",
	"2x"
};

static const int ibb_current_limit_table[] = {
	0,
	50,
	100,
	150,
	200,
	250,
	300,
	350,
	400,
	450,
	500,
	550,
	600,
	650,
	700,
	750,
	800,
	850,
	900,
	950,
	1000,
	1050,
	1100,
	1150,
	1200,
	1250,
	1300,
	1350,
	1400,
	1450,
	1500,
	1550,
};

static const int ibb_output_slew_ctl_table[] = {
	100,
	200,
	500,
	1000,
	2000,
	10000,
	12000,
	15000
};

static const int ibb_debounce_table[] = {
	8,
	16,
	32,
	64,
};

static const int ibb_overload_debounce_table[] = {
	1,
	2,
	4,
	8
};

static const int ibb_vreg_ok_deb_table[] = {
	4,
	8,
	16,
	32
};

static const int lab_ps_thresh_table_v1[] = {
	20,
	30,
	40,
	50,
};

static const int lab_ps_thresh_table_v2[] = {
	50,
	60,
	70,
	80,
};

static const int lab_soft_start_table[] = {
	200,
	400,
	600,
	800,
};

static const int lab_rdson_nfet_table[] = {
	25,
	50,
	75,
	100,
};

static const int lab_rdson_pfet_table[] = {
	25,
	50,
	75,
	100,
};

static const int lab_max_precharge_table[] = {
	200,
	300,
	400,
	500,
};

static const int ibb_pfm_peak_curr_table[] = {
	150,
	200,
	250,
	300,
	350,
	400,
	450,
	500
};

static const int ibb_pfm_hysteresis_table[] = {
	0,
	25,
	50,
	0
};

static const int lab_vref_high_psrr_table[] = {
	350,
	400,
	450,
	500
};

struct lab_regulator {
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	struct mutex			lab_mutex;

	int				lab_vreg_ok_irq;
	int				lab_sc_irq;

	int				curr_volt;
	int				min_volt;

	int				step_size;
	int				slew_rate;
	int				soft_start;
	int				sc_wait_time_ms;

	int				vreg_enabled;
};

struct ibb_regulator {
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	struct mutex			ibb_mutex;

	int				ibb_sc_irq;

	int				curr_volt;
	int				min_volt;

	int				step_size;
	int				slew_rate;
	int				soft_start;

	u32				pwrup_dly;
	u32				pwrdn_dly;

	int				vreg_enabled;
	int				num_swire_trans;
};

struct qpnp_labibb {
	struct device			*dev;
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct class			labibb_class;
	struct pmic_revid_data		*pmic_rev_id;
	u16				lab_base;
	u16				ibb_base;
	u8				lab_dig_major;
	u8				ibb_dig_major;
	struct lab_regulator		lab_vreg;
	struct ibb_regulator		ibb_vreg;
	const struct ibb_ver_ops	*ibb_ver_ops;
	const struct lab_ver_ops	*lab_ver_ops;
	struct mutex			bus_mutex;
	enum qpnp_labibb_mode		mode;
	struct work_struct		lab_vreg_ok_work;
	struct delayed_work		sc_err_recovery_work;
	struct hrtimer			sc_err_check_timer;
	int				sc_err_count;
	bool				standalone;
	bool				ttw_en;
	bool				in_ttw_mode;
	bool				ibb_settings_saved;
	bool				swire_control;
	bool				pbs_control;
	bool				ttw_force_lab_on;
	bool				skip_2nd_swire_cmd;
	bool				pfm_enable;
	bool				notify_lab_vreg_ok_sts;
	bool				detect_lab_sc;
	bool				sc_detected;
	 /* Tracks the secure UI mode entry/exit */
	bool				secure_mode;
	u32				swire_2nd_cmd_delay;
	u32				swire_ibb_ps_enable_delay;
};

static RAW_NOTIFIER_HEAD(labibb_notifier);

struct ibb_ver_ops {
	int (*set_default_voltage)(struct qpnp_labibb *labibb,
			bool use_default);
	int (*set_voltage)(struct qpnp_labibb *labibb, int min_uV, int max_uV);
	int (*sel_mode)(struct qpnp_labibb *labibb, bool is_ibb);
	int (*get_mode)(struct qpnp_labibb *labibb);
	int (*set_clk_div)(struct qpnp_labibb *labibb, u8 val);
	int (*smart_ps_config)(struct qpnp_labibb *labibb, bool enable,
				int num_swire_trans, int neg_curr_limit);
	int (*soft_start_ctl)(struct qpnp_labibb *labibb,
				 struct device_node *of_node);
	int (*voltage_at_one_pulse)(struct qpnp_labibb *labibb, u32 volt);
};

struct lab_ver_ops {
	const char *ver_str;
	int (*set_default_voltage)(struct qpnp_labibb *labibb,
					bool default_pres);
	int (*ps_ctl)(struct qpnp_labibb *labibb,
				u32 thresh, bool enable);
};

enum ibb_settings_index {
	IBB_PD_CTL = 0,
	IBB_CURRENT_LIMIT,
	IBB_RDSON_MNGMNT,
	IBB_PWRUP_PWRDN_CTL_1,
	IBB_PWRUP_PWRDN_CTL_2,
	IBB_NLIMIT_DAC,
	IBB_PS_CTL,
	IBB_SOFT_START_CTL,
	IBB_SETTINGS_MAX,
};

enum lab_settings_index {
	LAB_SOFT_START_CTL = 0,
	LAB_PS_CTL,
	LAB_RDSON_MNGMNT,
	LAB_SETTINGS_MAX,
};

struct settings {
	u16	address;
	u8	value;
	bool	sec_access;
};

#define SETTING(_id, _sec_access)		\
	[_id] = {				\
		.address = REG_##_id,		\
		.sec_access = _sec_access,	\
	}

static struct settings ibb_settings[IBB_SETTINGS_MAX] = {
	SETTING(IBB_PD_CTL, false),
	SETTING(IBB_CURRENT_LIMIT, true),
	SETTING(IBB_RDSON_MNGMNT, false),
	SETTING(IBB_PWRUP_PWRDN_CTL_1, true),
	SETTING(IBB_PWRUP_PWRDN_CTL_2, true),
	SETTING(IBB_NLIMIT_DAC, false),
	SETTING(IBB_PS_CTL, false),
	SETTING(IBB_SOFT_START_CTL, false),
};

static struct settings lab_settings[LAB_SETTINGS_MAX] = {
	SETTING(LAB_SOFT_START_CTL, false),
	SETTING(LAB_PS_CTL, false),
	SETTING(LAB_RDSON_MNGMNT, false),
};

static int
qpnp_labibb_read(struct qpnp_labibb *labibb, u16 address,
			u8 *val, int count)
{
	int rc = 0;
	struct platform_device *pdev = labibb->pdev;

	mutex_lock(&(labibb->bus_mutex));
	rc = regmap_bulk_read(labibb->regmap, address, val, count);
	if (rc < 0)
		pr_err("SPMI read failed address=0x%02x sid=0x%02x rc=%d\n",
			address, to_spmi_device(pdev->dev.parent)->usid, rc);

	mutex_unlock(&(labibb->bus_mutex));
	return rc;
}

static int
qpnp_labibb_write(struct qpnp_labibb *labibb, u16 address,
			u8 *val, int count)
{
	int rc = 0;
	struct platform_device *pdev = labibb->pdev;

	mutex_lock(&(labibb->bus_mutex));
	if (address == 0) {
		pr_err("address cannot be zero address=0x%02x sid=0x%02x rc=%d\n",
			address, to_spmi_device(pdev->dev.parent)->usid, rc);
		rc = -EINVAL;
		goto error;
	}

	rc = regmap_bulk_write(labibb->regmap, address, val, count);
	if (rc < 0)
		pr_err("write failed address=0x%02x sid=0x%02x rc=%d\n",
			address, to_spmi_device(pdev->dev.parent)->usid, rc);

error:
	mutex_unlock(&(labibb->bus_mutex));
	return rc;
}

static int
qpnp_labibb_masked_write(struct qpnp_labibb *labibb, u16 address,
						u8 mask, u8 val)
{
	int rc = 0;
	struct platform_device *pdev = labibb->pdev;

	mutex_lock(&(labibb->bus_mutex));
	if (address == 0) {
		pr_err("address cannot be zero address=0x%02x sid=0x%02x\n",
			address, to_spmi_device(pdev->dev.parent)->usid);
		rc = -EINVAL;
		goto error;
	}

	rc = regmap_update_bits(labibb->regmap, address, mask, val);
	if (rc < 0)
		pr_err("spmi write failed: addr=%03X, rc=%d\n", address, rc);

error:
	mutex_unlock(&(labibb->bus_mutex));
	return rc;
}

static int qpnp_labibb_sec_write(struct qpnp_labibb *labibb, u16 base,
					u8 offset, u8 val)
{
	int rc = 0;
	u8 sec_val = REG_LAB_IBB_SEC_UNLOCK_CODE;
	struct platform_device *pdev = labibb->pdev;

	mutex_lock(&(labibb->bus_mutex));
	if (base == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x\n",
			base, to_spmi_device(pdev->dev.parent)->usid);
		rc = -EINVAL;
		goto error;
	}

	rc = regmap_write(labibb->regmap, base + REG_LAB_IBB_SEC_ACCESS,
				sec_val);
	if (rc < 0) {
		pr_err("register %x failed rc = %d\n",
			base + REG_LAB_IBB_SEC_ACCESS, rc);
		goto error;
	}

	rc = regmap_write(labibb->regmap, base + offset, val);
	if (rc < 0)
		pr_err("failed: addr=%03X, rc=%d\n",
			base + offset, rc);

error:
	mutex_unlock(&(labibb->bus_mutex));
	return rc;
}

static int qpnp_labibb_sec_masked_write(struct qpnp_labibb *labibb, u16 base,
					u8 offset, u8 mask, u8 val)
{
	int rc = 0;
	u8 sec_val = REG_LAB_IBB_SEC_UNLOCK_CODE;
	struct platform_device *pdev = labibb->pdev;

	mutex_lock(&(labibb->bus_mutex));
	if (base == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x\n",
			base, to_spmi_device(pdev->dev.parent)->usid);
		rc = -EINVAL;
		goto error;
	}

	rc = regmap_write(labibb->regmap, base + REG_LAB_IBB_SEC_ACCESS,
				sec_val);
	if (rc < 0) {
		pr_err("register %x failed rc = %d\n",
			base + REG_LAB_IBB_SEC_ACCESS, rc);
		goto error;
	}

	rc = regmap_update_bits(labibb->regmap, base + offset, mask, val);
	if (rc < 0)
		pr_err("spmi write failed: addr=%03X, rc=%d\n", base, rc);

error:
	mutex_unlock(&(labibb->bus_mutex));
	return rc;
}

static int qpnp_ibb_smart_ps_config_v1(struct qpnp_labibb *labibb, bool enable,
					int num_swire_trans, int neg_curr_limit)
{
	return 0;
}

static int qpnp_ibb_smart_ps_config_v2(struct qpnp_labibb *labibb, bool enable,
					int num_swire_trans, int neg_curr_limit)
{
	u8 val;
	int rc = 0;

	if (enable) {
		val = IBB_NUM_SWIRE_PULSE_WAIT;
		rc = qpnp_labibb_write(labibb,
			labibb->ibb_base + REG_IBB_PS_CTL, &val, 1);
		if (rc < 0) {
			pr_err("write register %x failed rc = %d\n",
						REG_IBB_PS_CTL, rc);
			return rc;
		}
	}

	val = enable ? IBB_SMART_PS_CTL_EN : IBB_NUM_SWIRE_PULSE_WAIT;
	if (num_swire_trans)
		val |= num_swire_trans;
	else
		val |= IBB_NUM_SWIRE_PULSE_WAIT;

	rc = qpnp_labibb_write(labibb,
		labibb->ibb_base + REG_IBB_SMART_PS_CTL, &val, 1);
	if (rc < 0) {
		pr_err("write register %x failed rc = %d\n",
					REG_IBB_SMART_PS_CTL, rc);
		return rc;
	}

	val = enable ? (neg_curr_limit ? neg_curr_limit :
		IBB_DEFAULT_NLIMIT_DAC) : IBB_DEFAULT_NLIMIT_DAC;

	rc = qpnp_labibb_write(labibb,
		labibb->ibb_base + REG_IBB_NLIMIT_DAC, &val, 1);
	if (rc < 0)
		pr_err("write register %x failed rc = %d\n",
					REG_IBB_NLIMIT_DAC, rc);

	return rc;
}

static int qpnp_labibb_sel_mode_v1(struct qpnp_labibb *labibb, bool is_ibb)
{
	int rc = 0;
	u8 val;
	u16 base;

	val = (labibb->mode == QPNP_LABIBB_LCD_MODE) ? REG_LAB_IBB_LCD_MODE :
				 REG_LAB_IBB_AMOLED_MODE;

	base = is_ibb ? labibb->ibb_base : labibb->lab_base;

	rc = qpnp_labibb_sec_write(labibb, base, REG_LAB_LCD_AMOLED_SEL,
					val);
	if (rc < 0)
		pr_err("register %x failed rc = %d\n",
			REG_LAB_LCD_AMOLED_SEL, rc);

	return rc;
}

static int qpnp_labibb_sel_mode_v2(struct qpnp_labibb *labibb, bool is_ibb)
{
	return 0;
}

static int qpnp_ibb_get_mode_v1(struct qpnp_labibb *labibb)
{
	int rc = 0;
	u8 val;

	rc = qpnp_labibb_read(labibb, labibb->ibb_base + REG_IBB_LCD_AMOLED_SEL,
				&val, 1);
	if (rc < 0)
		return rc;

	if (val == REG_LAB_IBB_AMOLED_MODE)
		labibb->mode = QPNP_LABIBB_AMOLED_MODE;
	else
		labibb->mode = QPNP_LABIBB_LCD_MODE;

	return 0;
}

static int qpnp_ibb_get_mode_v2(struct qpnp_labibb *labibb)
{
	labibb->mode = QPNP_LABIBB_AMOLED_MODE;

	return 0;
}

static int qpnp_ibb_set_clk_div_v1(struct qpnp_labibb *labibb, u8 val)
{
	int rc = 0;

	rc = qpnp_labibb_write(labibb, labibb->ibb_base + REG_IBB_CLK_DIV,
				&val, 1);

	return rc;
}

static int qpnp_ibb_set_clk_div_v2(struct qpnp_labibb *labibb, u8 val)
{
	int rc = 0;

	val |= IBB_CLK_DIV_OVERRIDE_EN;
	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
				REG_IBB_CLK_DIV, IBB_CLK_DIV_MASK |
				IBB_CLK_DIV_OVERRIDE_EN, val);

	return rc;
}

static int qpnp_ibb_soft_start_ctl_v1(struct qpnp_labibb *labibb,
					struct device_node *of_node)
{
	int rc = 0;
	u8 val;
	u32 tmp;

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-soft-start",
					&(labibb->ibb_vreg.soft_start));
	if (rc < 0) {
		pr_err("qcom,qpnp-ibb-soft-start is missing, rc = %d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-discharge-resistor",
			&tmp);
	if (!rc) {
		for (val = 0; val < ARRAY_SIZE(ibb_dischg_res_table); val++) {
			if (ibb_dischg_res_table[val] == tmp)
				break;
		}

		if (val == ARRAY_SIZE(ibb_dischg_res_table)) {
			pr_err("Invalid value in qcom,qpnp-ibb-discharge-resistor\n");
			return -EINVAL;
		}

		rc = qpnp_labibb_write(labibb, labibb->ibb_base +
				REG_IBB_SOFT_START_CTL, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_IBB_SOFT_START_CTL,	rc);
			return rc;
		}
	}

	return 0;
}

static int qpnp_ibb_soft_start_ctl_v2(struct qpnp_labibb *labibb,
			 struct device_node *of_node)
{
	return 0;
}

static int qpnp_ibb_vreg_ok_ctl(struct qpnp_labibb *labibb,
			struct device_node *of_node)
{
	u8 val = 0;
	int rc = 0, i = 0;
	u32 tmp;

	if (labibb->pmic_rev_id->pmic_subtype != PM660L_SUBTYPE)
		return rc;

	val |= IBB_VREG_OK_EN_OVERLOAD_BLANK;

	rc = of_property_read_u32(of_node,
				"qcom,qpnp-ibb-overload-debounce", &tmp);
	if (rc < 0) {
		pr_err("failed to read qcom,qpnp-ibb-overload-debounce rc=%d\n",
								rc);
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(ibb_overload_debounce_table); i++)
		if (ibb_overload_debounce_table[i] == tmp)
			break;

	if (i == ARRAY_SIZE(ibb_overload_debounce_table)) {
		pr_err("Invalid value in qcom,qpnp-ibb-overload-debounce\n");
		return -EINVAL;
	}
	val |= i << IBB_VREG_OK_OVERLOAD_DEB_SHIFT;

	rc = of_property_read_u32(of_node,
				"qcom,qpnp-ibb-vreg-ok-debounce", &tmp);
	if (rc < 0) {
		pr_err("failed to read qcom,qpnp-ibb-vreg-ok-debounce rc=%d\n",
								rc);
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(ibb_vreg_ok_deb_table); i++)
		if (ibb_vreg_ok_deb_table[i] == tmp)
			break;

	if (i == ARRAY_SIZE(ibb_vreg_ok_deb_table)) {
		pr_err("Invalid value in qcom,qpnp-ibb-vreg-ok-debounce\n");
		return -EINVAL;
	}
	val |= i;

	rc = qpnp_labibb_write(labibb, labibb->ibb_base +
				REG_IBB_VREG_OK_CTL,
				&val, 1);
	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n",
		 REG_IBB_VREG_OK_CTL, rc);

	return rc;
}

static int qpnp_ibb_set_default_voltage_v1(struct qpnp_labibb *labibb,
						 bool use_default)
{
	u8 val;
	int rc = 0;

	if (!use_default) {
		if (labibb->ibb_vreg.curr_volt < labibb->ibb_vreg.min_volt) {
			pr_err("qcom,qpnp-ibb-init-voltage %d is less than the the minimum voltage %d",
			 labibb->ibb_vreg.curr_volt, labibb->ibb_vreg.min_volt);
				return -EINVAL;
		}

		val = DIV_ROUND_UP(labibb->ibb_vreg.curr_volt -
				labibb->ibb_vreg.min_volt,
				labibb->ibb_vreg.step_size);
		if (val > IBB_VOLTAGE_SET_MASK) {
			pr_err("qcom,qpnp-lab-init-voltage %d is larger than the max supported voltage %ld",
				labibb->ibb_vreg.curr_volt,
				labibb->ibb_vreg.min_volt +
				labibb->ibb_vreg.step_size *
				IBB_VOLTAGE_SET_MASK);
			return -EINVAL;
		}

		labibb->ibb_vreg.curr_volt = val * labibb->ibb_vreg.step_size +
				labibb->ibb_vreg.min_volt;
		val |= IBB_VOLTAGE_OVERRIDE_EN;
	} else {
		val = 0;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
			REG_IBB_VOLTAGE, IBB_VOLTAGE_SET_MASK |
			IBB_VOLTAGE_OVERRIDE_EN, val);
	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n", REG_IBB_VOLTAGE,
			rc);

	return rc;
}

static int qpnp_ibb_set_default_voltage_v2(struct qpnp_labibb *labibb,
						bool use_default)
{
	int rc = 0;
	u8 val;

	val = DIV_ROUND_UP(labibb->ibb_vreg.curr_volt,
			labibb->ibb_vreg.step_size);
	if (val > IBB_VOLTAGE_SET_MASK) {
		pr_err("Invalid qcom,qpnp-ibb-init-voltage property %d",
			labibb->ibb_vreg.curr_volt);
		return -EINVAL;
	}

	labibb->ibb_vreg.curr_volt = val * labibb->ibb_vreg.step_size;

	rc = qpnp_labibb_write(labibb, labibb->ibb_base +
				REG_IBB_DEFAULT_VOLTAGE, &val, 1);
	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n",
			 REG_IBB_DEFAULT_VOLTAGE, rc);

	return rc;
}

static int qpnp_ibb_set_voltage_v1(struct qpnp_labibb *labibb,
				 int min_uV, int max_uV)
{
	int rc, new_uV;
	u8 val;

	if (min_uV < labibb->ibb_vreg.min_volt) {
		pr_err("min_uV %d is less than min_volt %d", min_uV,
			labibb->ibb_vreg.min_volt);
		return -EINVAL;
	}

	val = DIV_ROUND_UP(min_uV - labibb->ibb_vreg.min_volt,
				labibb->ibb_vreg.step_size);
	new_uV = val * labibb->ibb_vreg.step_size + labibb->ibb_vreg.min_volt;

	if (new_uV > max_uV) {
		pr_err("unable to set voltage %d (min:%d max:%d)\n", new_uV,
			min_uV, max_uV);
		return -EINVAL;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
				REG_IBB_VOLTAGE,
				IBB_VOLTAGE_SET_MASK |
				IBB_VOLTAGE_OVERRIDE_EN,
				val | IBB_VOLTAGE_OVERRIDE_EN);

	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n", REG_IBB_VOLTAGE,
			rc);
		return rc;
	}

	if (new_uV > labibb->ibb_vreg.curr_volt) {
		val = DIV_ROUND_UP(new_uV - labibb->ibb_vreg.curr_volt,
				labibb->ibb_vreg.step_size);
		udelay(val * labibb->ibb_vreg.slew_rate);
	}
	labibb->ibb_vreg.curr_volt = new_uV;

	return 0;
}

static int qpnp_ibb_set_voltage_v2(struct qpnp_labibb *labibb,
				int min_uV, int max_uV)
{
	int rc, new_uV;
	u8 val;

	val = DIV_ROUND_UP(min_uV, labibb->ibb_vreg.step_size);
	new_uV = val * labibb->ibb_vreg.step_size;

	if (new_uV > max_uV) {
		pr_err("unable to set voltage %d (min:%d max:%d)\n", new_uV,
			min_uV, max_uV);
		return -EINVAL;
	}

	rc = qpnp_labibb_write(labibb, labibb->ibb_base +
				REG_IBB_VOLTAGE, &val, 1);
	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n", REG_IBB_VOLTAGE,
			rc);
		return rc;
	}

	if (new_uV > labibb->ibb_vreg.curr_volt) {
		val = DIV_ROUND_UP(new_uV - labibb->ibb_vreg.curr_volt,
				labibb->ibb_vreg.step_size);
		udelay(val * labibb->ibb_vreg.slew_rate);
	}
	labibb->ibb_vreg.curr_volt = new_uV;

	return 0;
}

static int qpnp_ibb_output_voltage_at_one_pulse_v1(struct qpnp_labibb *labibb,
						u32 volt)
{
	int rc = 0;
	u8 val;

	/*
	 * Set the output voltage 100mV lower as the IBB HW module
	 * counts one pulse less in SWIRE mode.
	 */
	val = DIV_ROUND_UP((volt - MIN_OUTPUT_PULSE_VOLTAGE_MV),
				OUTPUT_VOLTAGE_STEP_MV) - 1;
	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
			REG_IBB_SWIRE_CTL,
			IBB_OUTPUT_VOLTAGE_AT_ONE_PULSE_MASK,
			val);
	if (rc < 0)
		pr_err("write register %x failed rc = %d\n",
			REG_IBB_SWIRE_CTL, rc);

	return rc;
}

static int qpnp_ibb_output_voltage_at_one_pulse_v2(struct qpnp_labibb *labibb,
						u32 volt)
{
	int rc = 0;
	u8 val;

	val = DIV_ROUND_UP(volt, OUTPUT_VOLTAGE_STEP_MV);

	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
			REG_IBB_SWIRE_CTL,
			IBB_OUTPUT_VOLTAGE_AT_ONE_PULSE_MASK,
			val);
	if (rc < 0)
		pr_err("qpnp_labiibb_write register %x failed rc = %d\n",
			REG_IBB_SWIRE_CTL, rc);

	return rc;
}

/* For PMI8998 and earlier PMICs */
static const struct ibb_ver_ops ibb_ops_v1 = {
	.set_default_voltage	= qpnp_ibb_set_default_voltage_v1,
	.set_voltage		= qpnp_ibb_set_voltage_v1,
	.sel_mode		= qpnp_labibb_sel_mode_v1,
	.get_mode		= qpnp_ibb_get_mode_v1,
	.set_clk_div		= qpnp_ibb_set_clk_div_v1,
	.smart_ps_config	= qpnp_ibb_smart_ps_config_v1,
	.soft_start_ctl		= qpnp_ibb_soft_start_ctl_v1,
	.voltage_at_one_pulse	= qpnp_ibb_output_voltage_at_one_pulse_v1,
};

/* For PM660A and later PMICs */
static const struct ibb_ver_ops ibb_ops_v2 = {
	.set_default_voltage	= qpnp_ibb_set_default_voltage_v2,
	.set_voltage		= qpnp_ibb_set_voltage_v2,
	.sel_mode		= qpnp_labibb_sel_mode_v2,
	.get_mode		= qpnp_ibb_get_mode_v2,
	.set_clk_div		= qpnp_ibb_set_clk_div_v2,
	.smart_ps_config	= qpnp_ibb_smart_ps_config_v2,
	.soft_start_ctl		= qpnp_ibb_soft_start_ctl_v2,
	.voltage_at_one_pulse	= qpnp_ibb_output_voltage_at_one_pulse_v2,
};

static int qpnp_lab_set_default_voltage_v1(struct qpnp_labibb *labibb,
						 bool default_pres)
{
	u8 val;
	int rc = 0;

	if (!default_pres) {
		if (labibb->lab_vreg.curr_volt < labibb->lab_vreg.min_volt) {
			pr_err("qcom,qpnp-lab-init-voltage %d is less than the the minimum voltage %d",
				labibb->lab_vreg.curr_volt,
				labibb->lab_vreg.min_volt);
			return -EINVAL;
		}

		val = DIV_ROUND_UP(labibb->lab_vreg.curr_volt -
				labibb->lab_vreg.min_volt,
				labibb->lab_vreg.step_size);
		if (val > LAB_VOLTAGE_SET_MASK) {
			pr_err("qcom,qpnp-lab-init-voltage %d is larger than the max supported voltage %ld",
				labibb->lab_vreg.curr_volt,
				labibb->lab_vreg.min_volt +
				labibb->lab_vreg.step_size *
				LAB_VOLTAGE_SET_MASK);
			return -EINVAL;
		}

		labibb->lab_vreg.curr_volt = val * labibb->lab_vreg.step_size +
				labibb->lab_vreg.min_volt;
		val |= LAB_VOLTAGE_OVERRIDE_EN;

	} else {
		val = 0;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_VOLTAGE, LAB_VOLTAGE_SET_MASK |
				LAB_VOLTAGE_OVERRIDE_EN, val);

	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n", REG_LAB_VOLTAGE,
			rc);

	return rc;
}

static int qpnp_lab_set_default_voltage_v2(struct qpnp_labibb *labibb,
						 bool default_pres)
{
	int rc = 0;
	u8 val;

	val = DIV_ROUND_UP((labibb->lab_vreg.curr_volt
		 - labibb->lab_vreg.min_volt), labibb->lab_vreg.step_size);

	rc = qpnp_labibb_write(labibb, labibb->lab_base +
				REG_LAB_VOUT_DEFAULT, &val, 1);
	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n",
			 REG_LAB_VOUT_DEFAULT, rc);

	return rc;
}

static int qpnp_lab_ps_ctl_v1(struct qpnp_labibb *labibb,
					u32 thresh, bool enable)
{
	int rc = 0;
	u8 val;

	if (enable) {
		for (val = 0; val < ARRAY_SIZE(lab_ps_thresh_table_v1); val++)
			if (lab_ps_thresh_table_v1[val] == thresh)
				break;

		if (val == ARRAY_SIZE(lab_ps_thresh_table_v1)) {
			pr_err("Invalid value in qcom,qpnp-lab-ps-threshold\n");
			return -EINVAL;
		}

		val |= LAB_PS_CTL_EN;
	} else {
		val = 0;
	}

	rc = qpnp_labibb_write(labibb, labibb->lab_base +
			 REG_LAB_PS_CTL, &val, 1);

	if (rc < 0)
		pr_err("write register %x failed rc = %d\n",
				REG_LAB_PS_CTL, rc);

	return rc;
}

static int qpnp_lab_ps_ctl_v2(struct qpnp_labibb *labibb,
				u32 thresh, bool enable)
{
	int rc = 0;
	u8 val, mask;

	mask = LAB_PS_CTL_EN;
	if (enable) {
		for (val = 0; val < ARRAY_SIZE(lab_ps_thresh_table_v2); val++)
			if (lab_ps_thresh_table_v2[val] == thresh)
				break;

		if (val == ARRAY_SIZE(lab_ps_thresh_table_v2)) {
			pr_err("Invalid value in qcom,qpnp-lab-ps-threshold\n");
			return -EINVAL;
		}

		val |= LAB_PS_CTL_EN;
		mask |= LAB_PS_THRESH_MASK;
	} else {
		val = 0;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
			 REG_LAB_PS_CTL, mask, val);
	if (rc < 0)
		pr_err("write register %x failed rc = %d\n",
				REG_LAB_PS_CTL, rc);

	return rc;
}

/* For PMI8996 and earlier PMICs */
static const struct lab_ver_ops lab_ops_v1 = {
	.set_default_voltage	= qpnp_lab_set_default_voltage_v1,
	.ps_ctl			= qpnp_lab_ps_ctl_v1,
};

static const struct lab_ver_ops pmi8998_lab_ops = {
	.set_default_voltage	= qpnp_lab_set_default_voltage_v1,
	.ps_ctl			= qpnp_lab_ps_ctl_v2,
};

static const struct lab_ver_ops pm660_lab_ops = {
	.set_default_voltage	= qpnp_lab_set_default_voltage_v2,
	.ps_ctl			= qpnp_lab_ps_ctl_v2,
};

static int qpnp_labibb_get_matching_idx(const char *val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lab_current_sense_table); i++)
		if (!strcmp(lab_current_sense_table[i], val))
			return i;

	return -EINVAL;
}

static int qpnp_ibb_set_mode(struct qpnp_labibb *labibb, enum ibb_mode mode)
{
	int rc;
	u8 val;

	if (mode == IBB_SW_CONTROL_EN)
		val = IBB_ENABLE_CTL_MODULE_EN;
	else if (mode == IBB_HW_CONTROL)
		val = IBB_ENABLE_CTL_SWIRE_RDY;
	else if (mode == IBB_HW_SW_CONTROL)
		val = IBB_ENABLE_CTL_MODULE_EN | IBB_ENABLE_CTL_SWIRE_RDY;
	else if (mode == IBB_SW_CONTROL_DIS)
		val = 0;
	else
		return -EINVAL;

	rc = qpnp_labibb_masked_write(labibb,
		labibb->ibb_base + REG_IBB_ENABLE_CTL,
		IBB_ENABLE_CTL_MASK, val);
	if (rc < 0)
		pr_err("Unable to configure IBB_ENABLE_CTL rc=%d\n", rc);

	return rc;
}

static int qpnp_ibb_ps_config(struct qpnp_labibb *labibb, bool enable)
{
	u8 val;
	int rc;

	val = enable ? IBB_PS_CTL_EN : IBB_NUM_SWIRE_PULSE_WAIT;
	rc = qpnp_labibb_write(labibb, labibb->ibb_base + REG_IBB_PS_CTL,
							&val, 1);
	if (rc < 0) {
		pr_err("write register %x failed rc = %d\n",
					REG_IBB_PS_CTL, rc);
		return rc;
	}

	val = enable ? 0 : IBB_DEFAULT_NLIMIT_DAC;
	rc = qpnp_labibb_write(labibb, labibb->ibb_base + REG_IBB_NLIMIT_DAC,
							&val, 1);
	if (rc < 0)
		pr_err("write register %x failed rc = %d\n",
					REG_IBB_NLIMIT_DAC, rc);
	return rc;
}

static int qpnp_lab_dt_init(struct qpnp_labibb *labibb,
				struct device_node *of_node)
{
	int rc = 0;
	u8 i, val, mask;
	u32 tmp;

	/*
	 * Do not configure LCD_AMOLED_SEL for pmi8998 as it will be done by
	 * GPIO selector.
	 */
	if (labibb->pmic_rev_id->pmic_subtype != PMI8998_SUBTYPE) {
		rc = labibb->ibb_ver_ops->sel_mode(labibb, 0);
		if (rc < 0)
			return rc;
	}

	val = 0;
	if (of_property_read_bool(of_node, "qcom,qpnp-lab-full-pull-down"))
		val |= LAB_PD_CTL_STRONG_PULL;

	if (!of_property_read_bool(of_node, "qcom,qpnp-lab-pull-down-enable"))
		val |= LAB_PD_CTL_DISABLE_PD;

	mask = LAB_PD_CTL_EN_MASK | LAB_PD_CTL_STRENGTH_MASK;
	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base + REG_LAB_PD_CTL,
					mask, val);

	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n",
				REG_LAB_PD_CTL, rc);
		return rc;
	}

	rc = of_property_read_u32(of_node,
		"qcom,qpnp-lab-switching-clock-frequency", &tmp);
	if (!rc) {
		for (val = 0; val < ARRAY_SIZE(lab_clk_div_table); val++)
			if (lab_clk_div_table[val] == tmp)
				break;

		if (val == ARRAY_SIZE(lab_clk_div_table)) {
			pr_err("Invalid value in qpnp-lab-switching-clock-frequency\n");
			return -EINVAL;
		}

		rc = qpnp_labibb_write(labibb, labibb->lab_base +
				REG_LAB_CLK_DIV, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_CLK_DIV, rc);
			return rc;
		}
	}

	if (of_property_read_bool(of_node,
		"qcom,qpnp-lab-limit-max-current-enable")) {
		val = LAB_CURRENT_LIMIT_EN_BIT;

		rc = of_property_read_u32(of_node,
			"qcom,qpnp-lab-limit-maximum-current", &tmp);

		if (rc < 0) {
			pr_err("get qcom,qpnp-lab-limit-maximum-current failed rc = %d\n",
				rc);
			return rc;
		}

		for (i = 0; i < ARRAY_SIZE(lab_current_limit_table); i++)
			if (lab_current_limit_table[i] == tmp)
				break;

		if (i == ARRAY_SIZE(lab_current_limit_table)) {
			pr_err("Invalid value in qcom,qpnp-lab-limit-maximum-current\n");
			return -EINVAL;
		}

		val |= i;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_CURRENT_LIMIT, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
					REG_LAB_CURRENT_LIMIT, rc);
			return rc;
		}
	}

	if (of_property_read_bool(of_node,
		"qcom,qpnp-lab-ring-suppression-enable")) {
		val = LAB_RING_SUPPRESSION_CTL_EN;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_RING_SUPPRESSION_CTL, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_RING_SUPPRESSION_CTL, rc);
			return rc;
		}
	}

	if (of_property_read_bool(of_node, "qcom,qpnp-lab-ps-enable")) {

		rc = of_property_read_u32(of_node,
				 "qcom,qpnp-lab-ps-threshold", &tmp);

		if (rc < 0) {
			pr_err("get qcom,qpnp-lab-ps-threshold failed rc = %d\n",
				rc);
			return rc;
		}
		rc = labibb->lab_ver_ops->ps_ctl(labibb, tmp, true);
		if (rc < 0)
			return rc;
	} else {
		rc = labibb->lab_ver_ops->ps_ctl(labibb, tmp, false);
		if (rc < 0)
			return rc;
	}

	val = 0;
	mask = 0;
	rc = of_property_read_u32(of_node, "qcom,qpnp-lab-pfet-size", &tmp);
	if (!rc) {
		for (val = 0; val < ARRAY_SIZE(lab_rdson_pfet_table); val++)
			if (tmp == lab_rdson_pfet_table[val])
				break;

		if (val == ARRAY_SIZE(lab_rdson_pfet_table)) {
			pr_err("Invalid value in qcom,qpnp-lab-pfet-size\n");
			return -EINVAL;
		}
		val |= LAB_RDSON_MNGMNT_PFET_SLEW_EN;
		mask |= LAB_RDSON_MNGMNT_PFET_MASK |
				LAB_RDSON_MNGMNT_PFET_SLEW_EN;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-lab-nfet-size",
				 &tmp);
	if (!rc) {
		for (i = 0; i < ARRAY_SIZE(lab_rdson_nfet_table); i++)
			if (tmp == lab_rdson_nfet_table[i])
				break;

		if (i == ARRAY_SIZE(lab_rdson_nfet_table)) {
			pr_err("Invalid value in qcom,qpnp-lab-nfet-size\n");
			return -EINVAL;
		}

		val |= i << LAB_RDSON_MNGMNT_NFET_SHIFT;
		val |= LAB_RDSON_MNGMNT_NFET_SLEW_EN;
		mask |= LAB_RDSON_MNGMNT_NFET_MASK |
				LAB_RDSON_MNGMNT_NFET_SLEW_EN;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_RDSON_MNGMNT, mask, val);
	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n",
			REG_LAB_RDSON_MNGMNT, rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-lab-init-voltage",
				&(labibb->lab_vreg.curr_volt));
	if (rc < 0) {
		pr_err("get qcom,qpnp-lab-init-voltage failed, rc = %d\n",
				 rc);
		return rc;
	}

	if (of_property_read_bool(of_node,
			"qcom,qpnp-lab-use-default-voltage"))
		rc = labibb->lab_ver_ops->set_default_voltage(labibb, true);
	else
		rc = labibb->lab_ver_ops->set_default_voltage(labibb, false);

	if (rc < 0)
		return rc;

	if (of_property_read_bool(of_node,
		"qcom,qpnp-lab-enable-sw-high-psrr")) {
		val = LAB_EN_SW_HIGH_PSRR_MODE;

		rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_SW_HIGH_PSRR_CTL, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_SW_HIGH_PSRR_CTL, rc);
			return rc;
		}
	}

	rc = of_property_read_u32(of_node,
		"qcom,qpnp-lab-ldo-pulldown-enable", (u32 *)&val);
	if (!rc) {
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
			REG_LAB_LDO_PD_CTL, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_LDO_PD_CTL, rc);
			return rc;
		}
	}

	rc = of_property_read_u32(of_node,
		"qcom,qpnp-lab-high-psrr-src-select", &tmp);
	if (!rc) {
		val = tmp;

		rc = of_property_read_u32(of_node,
			"qcom,qpnp-lab-vref-high-psrr-select", &tmp);
		if (rc < 0) {
			pr_err("get qcom,qpnp-lab-vref-high-psrr-select failed rc = %d\n",
				rc);
			return rc;
		}

		for (i = 0; i < ARRAY_SIZE(lab_vref_high_psrr_table); i++)
			if (lab_vref_high_psrr_table[i] == tmp)
				break;

		if (i == ARRAY_SIZE(lab_vref_high_psrr_table)) {
			pr_err("Invalid value in qpnp-lab-vref-high-psrr-selct\n");
			return -EINVAL;
		}
		val |= (i << LAB_SEL_HW_HIGH_PSRR_SRC_SHIFT);

		rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_VPH_ENVELOP_CTL,
				LAB_VREF_HIGH_PSRR_SEL_MASK |
				LAB_SEL_HW_HIGH_PSRR_SRC_MASK,
				val);

		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_VPH_ENVELOP_CTL, rc);
			return rc;
		}
	}

	if (labibb->swire_control) {
		rc = qpnp_ibb_set_mode(labibb, IBB_HW_CONTROL);
		if (rc < 0) {
			pr_err("Unable to set SWIRE_RDY rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

#define LAB_CURRENT_MAX_1600MA	0x7
#define LAB_CURRENT_MAX_400MA	0x1
static int qpnp_lab_pfm_disable(struct qpnp_labibb *labibb)
{
	int rc = 0;
	u8 val, mask;

	mutex_lock(&(labibb->lab_vreg.lab_mutex));
	if (!labibb->pfm_enable) {
		pr_debug("PFM already disabled\n");
		goto out;
	}

	val = 0;
	mask = LAB_PFM_EN_BIT;
	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_PFM_CTL, mask, val);
	if (rc < 0) {
		pr_err("Write register %x failed rc = %d\n",
			REG_LAB_PFM_CTL, rc);
		goto out;
	}

	val = LAB_CURRENT_MAX_1600MA;
	mask = LAB_OVERRIDE_CURRENT_MAX_BIT | LAB_CURRENT_LIMIT_MASK;
	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_CURRENT_LIMIT, mask, val);
	if (rc < 0) {
		pr_err("Write register %x failed rc = %d\n",
			REG_LAB_CURRENT_LIMIT, rc);
		goto out;
	}

	labibb->pfm_enable = false;
out:
	mutex_unlock(&(labibb->lab_vreg.lab_mutex));
	return rc;
}

static int qpnp_lab_pfm_enable(struct qpnp_labibb *labibb)
{
	int rc = 0;
	u8 val, mask;

	mutex_lock(&(labibb->lab_vreg.lab_mutex));
	if (labibb->pfm_enable) {
		pr_debug("PFM already enabled\n");
		goto out;
	}

	/* Wait for ~100uS */
	usleep_range(100, 105);

	val = LAB_OVERRIDE_CURRENT_MAX_BIT | LAB_CURRENT_MAX_400MA;
	mask = LAB_OVERRIDE_CURRENT_MAX_BIT | LAB_CURRENT_LIMIT_MASK;
	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_CURRENT_LIMIT, mask, val);
	if (rc < 0) {
		pr_err("Write register %x failed rc = %d\n",
			REG_LAB_CURRENT_LIMIT, rc);
		goto out;
	}

	/* Wait for ~100uS */
	usleep_range(100, 105);

	val = LAB_PFM_EN_BIT;
	mask = LAB_PFM_EN_BIT;
	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_PFM_CTL, mask, val);
	if (rc < 0) {
		pr_err("Write register %x failed rc = %d\n",
			REG_LAB_PFM_CTL, rc);
		goto out;
	}

	labibb->pfm_enable = true;
out:
	mutex_unlock(&(labibb->lab_vreg.lab_mutex));
	return rc;
}

static int qpnp_labibb_restore_settings(struct qpnp_labibb *labibb)
{
	int rc, i;

	for (i = 0; i < ARRAY_SIZE(ibb_settings); i++) {
		if (ibb_settings[i].sec_access)
			rc = qpnp_labibb_sec_write(labibb, labibb->ibb_base,
					ibb_settings[i].address,
					ibb_settings[i].value);
		else
			rc = qpnp_labibb_write(labibb, labibb->ibb_base +
					ibb_settings[i].address,
					&ibb_settings[i].value, 1);

		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				ibb_settings[i].address, rc);
			return rc;
		}
	}

	for (i = 0; i < ARRAY_SIZE(lab_settings); i++) {
		if (lab_settings[i].sec_access)
			rc = qpnp_labibb_sec_write(labibb, labibb->lab_base,
					lab_settings[i].address,
					lab_settings[i].value);
		else
			rc = qpnp_labibb_write(labibb, labibb->lab_base +
					lab_settings[i].address,
					&lab_settings[i].value, 1);

		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				lab_settings[i].address, rc);
			return rc;
		}
	}

	return 0;
}

static int qpnp_labibb_save_settings(struct qpnp_labibb *labibb)
{
	int rc, i;

	for (i = 0; i < ARRAY_SIZE(ibb_settings); i++) {
		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
			 ibb_settings[i].address, &ibb_settings[i].value, 1);
		if (rc < 0) {
			pr_err("read register %x failed rc = %d\n",
				ibb_settings[i].address, rc);
			return rc;
		}
	}

	for (i = 0; i < ARRAY_SIZE(lab_settings); i++) {
		rc = qpnp_labibb_read(labibb, labibb->lab_base +
			lab_settings[i].address, &lab_settings[i].value, 1);
		if (rc < 0) {
			pr_err("read register %x failed rc = %d\n",
				lab_settings[i].address, rc);
			return rc;
		}
	}

	return 0;
}

static int qpnp_labibb_ttw_enter_ibb_common(struct qpnp_labibb *labibb)
{
	int rc = 0;
	u8 val, mask;

	val = 0;
	rc = qpnp_labibb_write(labibb, labibb->ibb_base + REG_IBB_PD_CTL,
				&val, 1);
	if (rc < 0) {
		pr_err("read register %x failed rc = %d\n",
			REG_IBB_PD_CTL, rc);
		return rc;
	}

	val = 0;
	rc = qpnp_labibb_sec_write(labibb, labibb->ibb_base,
				REG_IBB_PWRUP_PWRDN_CTL_1, val);
	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n",
			REG_IBB_PWRUP_PWRDN_CTL_1, rc);
		return rc;
	}

	if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE) {
		val = 0;
		mask = IBB_DIS_DLY_MASK;
	} else {
		val = IBB_WAIT_MBG_OK;
		mask = IBB_DIS_DLY_MASK | IBB_WAIT_MBG_OK;
	}

	rc = qpnp_labibb_sec_masked_write(labibb, labibb->ibb_base,
				REG_IBB_PWRUP_PWRDN_CTL_2, mask, val);
	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n",
			REG_IBB_PWRUP_PWRDN_CTL_2, rc);
		return rc;
	}

	val = IBB_NFET_SLEW_EN | IBB_PFET_SLEW_EN | IBB_OVERRIDE_NFET_SW_SIZE |
		IBB_OVERRIDE_PFET_SW_SIZE;
	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
				REG_IBB_RDSON_MNGMNT, 0xFF, val);
	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n",
			REG_IBB_RDSON_MNGMNT, rc);
		return rc;
	}

	val = IBB_CURRENT_LIMIT_EN | IBB_CURRENT_MAX_500MA |
		(IBB_ILIMIT_COUNT_CYC8 << IBB_CURRENT_LIMIT_DEBOUNCE_SHIFT);
	rc = qpnp_labibb_sec_write(labibb, labibb->ibb_base,
				REG_IBB_CURRENT_LIMIT, val);
	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n",
			REG_IBB_CURRENT_LIMIT, rc);

	return rc;
}

static int qpnp_labibb_ttw_enter_ibb_pmi8996(struct qpnp_labibb *labibb)
{
	int rc;
	u8 val;

	val = IBB_BYPASS_PWRDN_DLY2_BIT | IBB_FAST_STARTUP;
	rc = qpnp_labibb_write(labibb, labibb->ibb_base + REG_IBB_SPARE_CTL,
				&val, 1);
	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n",
			REG_IBB_SPARE_CTL, rc);

	return rc;
}

static int qpnp_labibb_ttw_enter_ibb_pmi8950(struct qpnp_labibb *labibb)
{
	int rc;
	u8 val;

	rc = qpnp_ibb_ps_config(labibb, true);
	if (rc < 0) {
		pr_err("Failed to enable ibb_ps_config rc=%d\n", rc);
		return rc;
	}

	val = IBB_SOFT_START_CHARGING_RESISTOR_16K;
	rc = qpnp_labibb_write(labibb, labibb->ibb_base +
				REG_IBB_SOFT_START_CTL, &val, 1);
	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n",
			REG_IBB_SOFT_START_CTL, rc);
		return rc;
	}

	val = IBB_MODULE_RDY_EN;
	rc = qpnp_labibb_write(labibb, labibb->lab_base +
				REG_IBB_MODULE_RDY, &val, 1);
	if (rc < 0)
		pr_err("write to register %x failed rc = %d\n",
				REG_IBB_MODULE_RDY, rc);

	return rc;
}

static int qpnp_labibb_regulator_ttw_mode_enter(struct qpnp_labibb *labibb)
{
	int rc = 0;
	u8 val, reg;

	/* Save the IBB settings before they get modified for TTW mode */
	if (!labibb->ibb_settings_saved) {
		rc = qpnp_labibb_save_settings(labibb);
		if (rc) {
			pr_err("Error in storing IBB setttings, rc=%d\n", rc);
			return rc;
		}
		labibb->ibb_settings_saved = true;
	}

	if (labibb->ttw_force_lab_on) {
		val = LAB_MODULE_RDY_EN;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_MODULE_RDY, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_MODULE_RDY, rc);
			return rc;
		}

		/* Prevents LAB being turned off by IBB */
		val = LAB_ENABLE_CTL_EN;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_ENABLE_CTL, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_ENABLE_CTL, rc);
			return rc;
		}

		val = LAB_RDSON_MNGMNT_NFET_SLEW_EN |
			LAB_RDSON_MNGMNT_PFET_SLEW_EN |
			LAB_RDSON_NFET_SW_SIZE_QUARTER |
			LAB_RDSON_PFET_SW_SIZE_QUARTER;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
				REG_LAB_RDSON_MNGMNT, &val, 1);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				REG_LAB_RDSON_MNGMNT, rc);
			return rc;
		}

		rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_PS_CTL, LAB_PS_CTL_EN, LAB_PS_CTL_EN);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
				REG_LAB_PS_CTL, rc);
			return rc;
		}
	} else {
		val = LAB_PD_CTL_DISABLE_PD;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
				REG_LAB_PD_CTL, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
				REG_LAB_PD_CTL, rc);
			return rc;
		}

		val = LAB_SPARE_DISABLE_SCP_BIT;

		if (labibb->pmic_rev_id->pmic_subtype != PMI8950_SUBTYPE)
			val |= LAB_SPARE_TOUCH_WAKE_BIT;

		if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE) {
			reg = REG_LAB_MISC_CTL;
			val |= LAB_AUTO_GM_BIT;
		} else {
			reg = REG_LAB_SPARE_CTL;
		}
		rc = qpnp_labibb_write(labibb, labibb->lab_base + reg, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
				REG_LAB_SPARE_CTL, rc);
			return rc;
		}

		val = 0;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
				REG_LAB_SOFT_START_CTL, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
				REG_LAB_SOFT_START_CTL, rc);
			return rc;
		}
	}

	rc = qpnp_labibb_ttw_enter_ibb_common(labibb);
	if (rc) {
		pr_err("Failed to apply TTW ibb common settings rc=%d\n", rc);
		return rc;
	}

	switch (labibb->pmic_rev_id->pmic_subtype) {
	case PMI8996_SUBTYPE:
		rc = qpnp_labibb_ttw_enter_ibb_pmi8996(labibb);
		break;
	case PMI8950_SUBTYPE:
		rc = qpnp_labibb_ttw_enter_ibb_pmi8950(labibb);
		break;
	case PMI8998_SUBTYPE:
		rc = labibb->lab_ver_ops->ps_ctl(labibb, 70, true);
		if (rc < 0)
			break;

		rc = qpnp_ibb_ps_config(labibb, true);
		break;
	}

	if (rc < 0) {
		pr_err("Failed to configure TTW-enter for IBB rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_ibb_set_mode(labibb, IBB_HW_CONTROL);
	if (rc < 0) {
		pr_err("Unable to set SWIRE_RDY rc = %d\n", rc);
		return rc;
	}
	labibb->in_ttw_mode = true;
	return 0;
}

static int qpnp_labibb_ttw_exit_ibb_common(struct qpnp_labibb *labibb)
{
	int rc;
	u8 val;

	val = IBB_FASTER_PFET_OFF;
	rc = qpnp_labibb_write(labibb, labibb->ibb_base + REG_IBB_SPARE_CTL,
			&val, 1);
	if (rc < 0)
		pr_err("qpnp_labibb_write register %x failed rc = %d\n",
			REG_IBB_SPARE_CTL, rc);

	return rc;
}

static int qpnp_labibb_regulator_ttw_mode_exit(struct qpnp_labibb *labibb)
{
	int rc = 0;
	u8 val, reg;

	if (!labibb->ibb_settings_saved) {
		pr_err("IBB settings are not saved!\n");
		return -EINVAL;
	}

	/* Restore the IBB settings back to switch back to normal mode */
	rc = qpnp_labibb_restore_settings(labibb);
	if (rc < 0) {
		pr_err("Error in restoring IBB setttings, rc=%d\n", rc);
		return rc;
	}

	if (labibb->ttw_force_lab_on) {
		val = 0;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_ENABLE_CTL, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
				REG_LAB_ENABLE_CTL, rc);
			return rc;
		}
	} else {
		val = LAB_PD_CTL_STRONG_PULL;
		rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_PD_CTL,	&val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
						REG_LAB_PD_CTL, rc);
			return rc;
		}

		val = 0;
		if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE) {
			reg = REG_LAB_MISC_CTL;
			val |= LAB_AUTO_GM_BIT;
		} else {
			reg = REG_LAB_SPARE_CTL;
		}

		rc = qpnp_labibb_write(labibb, labibb->lab_base + reg, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
					REG_LAB_SPARE_CTL, rc);
			return rc;
		}
	}

	switch (labibb->pmic_rev_id->pmic_subtype) {
	case PMI8996_SUBTYPE:
	case PMI8994_SUBTYPE:
	case PMI8950_SUBTYPE:
		rc = qpnp_labibb_ttw_exit_ibb_common(labibb);
		break;
	}
	if (rc < 0) {
		pr_err("Failed to configure TTW-exit for IBB rc=%d\n", rc);
		return rc;
	}

	labibb->in_ttw_mode = false;
	return rc;
}

static void qpnp_lab_vreg_notifier_work(struct work_struct *work)
{
	int rc = 0;
	u16 retries = 1000, dly = 5000;
	u8 val;
	struct qpnp_labibb *labibb  = container_of(work, struct qpnp_labibb,
							lab_vreg_ok_work);
	if (labibb->lab_vreg.sc_wait_time_ms != -EINVAL)
		retries = labibb->lab_vreg.sc_wait_time_ms / 5;

	while (retries) {
		rc = qpnp_labibb_read(labibb, labibb->lab_base +
					REG_LAB_STATUS1, &val, 1);
		if (rc < 0) {
			pr_err("read register %x failed rc = %d\n",
				REG_LAB_STATUS1, rc);
			return;
		}

		if (val & LAB_STATUS1_VREG_OK_BIT) {
			raw_notifier_call_chain(&labibb_notifier,
						LAB_VREG_OK, NULL);
			break;
		}

		usleep_range(dly, dly + 100);
		retries--;
	}

	if (!retries) {
		if (labibb->detect_lab_sc) {
			pr_crit("short circuit detected on LAB rail.. disabling the LAB/IBB/OLEDB modules\n");
			/* Disable LAB module */
			val = 0;
			rc = qpnp_labibb_write(labibb, labibb->lab_base +
					REG_LAB_MODULE_RDY, &val, 1);
			if (rc < 0) {
				pr_err("write register %x failed rc = %d\n",
					REG_LAB_MODULE_RDY, rc);
				return;
			}
			raw_notifier_call_chain(&labibb_notifier,
						LAB_VREG_NOT_OK, NULL);
			labibb->sc_detected = true;
			labibb->lab_vreg.vreg_enabled = 0;
			labibb->ibb_vreg.vreg_enabled = 0;
		} else {
			pr_err("LAB_VREG_OK not set, failed to notify\n");
		}
	}
}

static int qpnp_lab_enable_standalone(struct qpnp_labibb *labibb)
{
	int rc;
	u8 val;

	val = LAB_ENABLE_CTL_EN;
	rc = qpnp_labibb_write(labibb,
		labibb->lab_base + REG_LAB_ENABLE_CTL, &val, 1);
	if (rc < 0) {
		pr_err("Write register %x failed rc = %d\n",
					REG_LAB_ENABLE_CTL, rc);
		return rc;
	}

	udelay(labibb->lab_vreg.soft_start);

	rc = qpnp_labibb_read(labibb, labibb->lab_base +
				REG_LAB_STATUS1, &val, 1);
	if (rc < 0) {
		pr_err("Read register %x failed rc = %d\n",
					REG_LAB_STATUS1, rc);
		return rc;
	}

	if (!(val & LAB_STATUS1_VREG_OK_BIT)) {
		pr_err("Can't enable LAB standalone\n");
		return -EINVAL;
	}

	return 0;
}

static int qpnp_ibb_enable_standalone(struct qpnp_labibb *labibb)
{
	int rc, delay, retries = 10;
	u8 val;

	rc = qpnp_ibb_set_mode(labibb, IBB_SW_CONTROL_EN);
	if (rc < 0) {
		pr_err("Unable to set IBB_MODULE_EN rc = %d\n", rc);
		return rc;
	}

	delay = labibb->ibb_vreg.soft_start;
	while (retries--) {
		/* Wait for a small period before reading IBB_STATUS1 */
		usleep_range(delay, delay + 100);

		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
				REG_IBB_STATUS1, &val, 1);
		if (rc < 0) {
			pr_err("Read register %x failed rc = %d\n",
				REG_IBB_STATUS1, rc);
			return rc;
		}

		if (val & IBB_STATUS1_VREG_OK_BIT)
			break;
	}

	if (!(val & IBB_STATUS1_VREG_OK_BIT)) {
		pr_err("Can't enable IBB standalone\n");
		return -EINVAL;
	}

	return 0;
}

static int qpnp_labibb_regulator_enable(struct qpnp_labibb *labibb)
{
	int rc;
	u8 val;
	int dly;
	int retries;
	bool enabled = false;

	if (labibb->ttw_en && !labibb->ibb_vreg.vreg_enabled &&
		labibb->in_ttw_mode) {
		rc = qpnp_labibb_regulator_ttw_mode_exit(labibb);
		if (rc) {
			pr_err("Error in exiting TTW mode rc = %d\n", rc);
			return rc;
		}
	}

	rc = qpnp_ibb_set_mode(labibb, IBB_SW_CONTROL_EN);
	if (rc) {
		pr_err("Unable to set IBB_MODULE_EN rc = %d\n", rc);
		return rc;
	}

	/* total delay time */
	dly = labibb->lab_vreg.soft_start + labibb->ibb_vreg.soft_start
				+ labibb->ibb_vreg.pwrup_dly;
	usleep_range(dly, dly + 100);

	/* after this delay, lab should be enabled */
	rc = qpnp_labibb_read(labibb, labibb->lab_base + REG_LAB_STATUS1,
			&val, 1);
	if (rc < 0) {
		pr_err("read register %x failed rc = %d\n",
			REG_LAB_STATUS1, rc);
		goto err_out;
	}

	pr_debug("soft=%d %d up=%d dly=%d\n",
		labibb->lab_vreg.soft_start, labibb->ibb_vreg.soft_start,
				labibb->ibb_vreg.pwrup_dly, dly);

	if (!(val & LAB_STATUS1_VREG_OK_BIT)) {
		pr_err("failed for LAB %x\n", val);
		goto err_out;
	}

	/* poll IBB_STATUS to make sure ibb had been enabled */
	dly = labibb->ibb_vreg.soft_start + labibb->ibb_vreg.pwrup_dly;
	retries = 10;
	while (retries--) {
		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
					REG_IBB_STATUS1, &val, 1);
		if (rc < 0) {
			pr_err("read register %x failed rc = %d\n",
				REG_IBB_STATUS1, rc);
			goto err_out;
		}

		if (val & IBB_STATUS1_VREG_OK_BIT) {
			enabled = true;
			break;
		}
		usleep_range(dly, dly + 100);
	}

	if (!enabled) {
		pr_err("failed for IBB %x\n", val);
		goto err_out;
	}

	labibb->lab_vreg.vreg_enabled = 1;
	labibb->ibb_vreg.vreg_enabled = 1;

	return 0;
err_out:
	rc = qpnp_ibb_set_mode(labibb, IBB_SW_CONTROL_DIS);
	if (rc < 0) {
		pr_err("Unable to set IBB_MODULE_EN rc = %d\n", rc);
		return rc;
	}
	return -EINVAL;
}

static int qpnp_labibb_regulator_disable(struct qpnp_labibb *labibb)
{
	int rc;
	u8 val;
	int dly;
	int retries;
	bool disabled = false;

	/*
	 * When TTW mode is enabled and LABIBB regulators are disabled, it is
	 * recommended not to disable IBB through IBB_ENABLE_CTL when switching
	 * to SWIRE control on entering TTW mode. Hence, just enter TTW mode
	 * and mark the regulators disabled. When we exit TTW mode, normal
	 * mode settings will be restored anyways and regulators will be
	 * enabled as before.
	 */
	if (labibb->ttw_en && !labibb->in_ttw_mode) {
		rc = qpnp_labibb_regulator_ttw_mode_enter(labibb);
		if (rc < 0) {
			pr_err("Error in entering TTW mode rc = %d\n", rc);
			return rc;
		}
		labibb->lab_vreg.vreg_enabled = 0;
		labibb->ibb_vreg.vreg_enabled = 0;
		return 0;
	}

	rc = qpnp_ibb_set_mode(labibb, IBB_SW_CONTROL_DIS);
	if (rc < 0) {
		pr_err("Unable to set IBB_MODULE_EN rc = %d\n", rc);
		return rc;
	}

	/* poll IBB_STATUS to make sure ibb had been disabled */
	dly = labibb->ibb_vreg.pwrdn_dly;
	retries = 2;
	while (retries--) {
		usleep_range(dly, dly + 100);
		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
				REG_IBB_STATUS1, &val, 1);
		if (rc < 0) {
			pr_err("read register %x failed rc = %d\n",
				REG_IBB_STATUS1, rc);
			return rc;
		}

		if (!(val & IBB_STATUS1_VREG_OK_BIT)) {
			disabled = true;
			break;
		}
	}

	if (!disabled) {
		pr_err("failed for IBB %x\n", val);
		return -EINVAL;
	}

	if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE &&
		labibb->mode == QPNP_LABIBB_LCD_MODE) {
		rc = qpnp_lab_pfm_disable(labibb);
		if (rc < 0) {
			pr_err("Error in disabling PFM, rc=%d\n", rc);
			return rc;
		}
	}

	labibb->lab_vreg.vreg_enabled = 0;
	labibb->ibb_vreg.vreg_enabled = 0;

	return 0;
}

static int qpnp_lab_regulator_enable(struct regulator_dev *rdev)
{
	int rc;
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->secure_mode)
		return 0;

	if (labibb->sc_detected) {
		pr_info("Short circuit detected: disabled LAB/IBB rails\n");
		return 0;
	}

	if (labibb->skip_2nd_swire_cmd) {
		rc = qpnp_ibb_ps_config(labibb, false);
		if (rc < 0) {
			pr_err("Failed to disable IBB PS rc=%d\n", rc);
			return rc;
		}
	}

	if (!labibb->lab_vreg.vreg_enabled && !labibb->swire_control) {
		if (!labibb->standalone)
			return qpnp_labibb_regulator_enable(labibb);

		rc = qpnp_lab_enable_standalone(labibb);
		if (rc) {
			pr_err("enable lab standalone failed, rc=%d\n", rc);
			return rc;
		}
		labibb->lab_vreg.vreg_enabled = 1;
	}

	if (labibb->notify_lab_vreg_ok_sts || labibb->detect_lab_sc)
		schedule_work(&labibb->lab_vreg_ok_work);

	return 0;
}

static int qpnp_lab_regulator_disable(struct regulator_dev *rdev)
{
	int rc;
	u8 val;
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->secure_mode)
		return 0;

	if (labibb->lab_vreg.vreg_enabled && !labibb->swire_control) {

		if (!labibb->standalone)
			return qpnp_labibb_regulator_disable(labibb);

		val = 0;
		rc = qpnp_labibb_write(labibb,
			labibb->lab_base + REG_LAB_ENABLE_CTL, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_lab_regulator_enable write register %x failed rc = %d\n",
				REG_LAB_ENABLE_CTL, rc);
			return rc;
		}

		labibb->lab_vreg.vreg_enabled = 0;
	}
	return 0;
}

static int qpnp_lab_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->swire_control)
		return 0;

	return labibb->lab_vreg.vreg_enabled;
}

static int qpnp_labibb_force_enable(struct qpnp_labibb *labibb)
{
	int rc;

	if (labibb->skip_2nd_swire_cmd) {
		rc = qpnp_ibb_ps_config(labibb, false);
		if (rc < 0) {
			pr_err("Failed to disable IBB PS rc=%d\n", rc);
			return rc;
		}
	}

	if (!labibb->swire_control) {
		if (!labibb->standalone)
			return qpnp_labibb_regulator_enable(labibb);

		rc = qpnp_ibb_enable_standalone(labibb);
		if (rc < 0) {
			pr_err("enable ibb standalone failed, rc=%d\n", rc);
			return rc;
		}
		labibb->ibb_vreg.vreg_enabled = 1;

		rc = qpnp_lab_enable_standalone(labibb);
		if (rc < 0) {
			pr_err("enable lab standalone failed, rc=%d\n", rc);
			return rc;
		}
		labibb->lab_vreg.vreg_enabled = 1;
	}

	return 0;
}

#define SC_ERR_RECOVERY_DELAY_MS	250
#define SC_ERR_COUNT_INTERVAL_SEC	1
#define POLLING_SCP_DONE_COUNT		2
#define POLLING_SCP_DONE_INTERVAL_MS	5
static irqreturn_t labibb_sc_err_handler(int irq, void *_labibb)
{
	int rc;
	u16 reg;
	u8 sc_err_mask, val;
	char *str;
	struct qpnp_labibb *labibb = (struct qpnp_labibb *)_labibb;
	bool in_sc_err, lab_en, ibb_en, scp_done = false;
	int count;

	if (irq == labibb->lab_vreg.lab_sc_irq) {
		reg = labibb->lab_base + REG_LAB_STATUS1;
		sc_err_mask = LAB_STATUS1_SC_DETECT_BIT;
		str = "LAB";
	} else if (irq == labibb->ibb_vreg.ibb_sc_irq) {
		reg = labibb->ibb_base + REG_IBB_STATUS1;
		sc_err_mask = IBB_STATUS1_SC_DETECT_BIT;
		str = "IBB";
	} else {
		return IRQ_HANDLED;
	}

	rc = qpnp_labibb_read(labibb, reg, &val, 1);
	if (rc < 0) {
		pr_err("Read 0x%x failed, rc=%d\n", reg, rc);
		return IRQ_HANDLED;
	}
	pr_debug("%s SC error triggered! %s_STATUS1 = %d\n", str, str, val);

	in_sc_err = !!(val & sc_err_mask);

	/*
	 * The SC fault would trigger PBS to disable regulators
	 * for protection. This would cause the SC_DETECT status being
	 * cleared so that it's not able to get the SC fault status.
	 * Check if LAB/IBB regulators are enabled in the driver but
	 * disabled in hardware, this means a SC fault had happened
	 * and SCP handling is completed by PBS.
	 */
	if (!in_sc_err) {
		count = POLLING_SCP_DONE_COUNT;
		do {
			reg = labibb->lab_base + REG_LAB_ENABLE_CTL;
			rc = qpnp_labibb_read(labibb, reg, &val, 1);
			if (rc < 0) {
				pr_err("Read 0x%x failed, rc=%d\n", reg, rc);
				return IRQ_HANDLED;
			}
			lab_en = !!(val & LAB_ENABLE_CTL_EN);

			reg = labibb->ibb_base + REG_IBB_ENABLE_CTL;
			rc = qpnp_labibb_read(labibb, reg, &val, 1);
			if (rc < 0) {
				pr_err("Read 0x%x failed, rc=%d\n", reg, rc);
				return IRQ_HANDLED;
			}
			ibb_en = !!(val & IBB_ENABLE_CTL_MODULE_EN);
			if (lab_en || ibb_en)
				msleep(POLLING_SCP_DONE_INTERVAL_MS);
			else
				break;
		} while ((lab_en || ibb_en) && count--);

		if (labibb->lab_vreg.vreg_enabled
				&& labibb->ibb_vreg.vreg_enabled
				&& !lab_en && !ibb_en) {
			pr_debug("LAB/IBB has been disabled by SCP\n");
			scp_done = true;
		}
	}

	if (in_sc_err || scp_done) {
		if (hrtimer_active(&labibb->sc_err_check_timer) ||
			hrtimer_callback_running(&labibb->sc_err_check_timer)) {
			labibb->sc_err_count++;
		} else {
			labibb->sc_err_count = 1;
			hrtimer_start(&labibb->sc_err_check_timer,
					ktime_set(SC_ERR_COUNT_INTERVAL_SEC, 0),
					HRTIMER_MODE_REL);
		}
		schedule_delayed_work(&labibb->sc_err_recovery_work,
				msecs_to_jiffies(SC_ERR_RECOVERY_DELAY_MS));
	}

	return IRQ_HANDLED;
}

#define SC_FAULT_COUNT_MAX		4
static enum hrtimer_restart labibb_check_sc_err_count(struct hrtimer *timer)
{
	struct qpnp_labibb *labibb = container_of(timer,
			struct qpnp_labibb, sc_err_check_timer);
	/*
	 * if SC fault triggers more than 4 times in 1 second,
	 * then disable the IRQs and leave as it.
	 */
	if (labibb->sc_err_count > SC_FAULT_COUNT_MAX) {
		disable_irq(labibb->lab_vreg.lab_sc_irq);
		disable_irq(labibb->ibb_vreg.ibb_sc_irq);
	}

	return HRTIMER_NORESTART;
}

static void labibb_sc_err_recovery_work(struct work_struct *work)
{
	struct qpnp_labibb *labibb = container_of(work, struct qpnp_labibb,
					sc_err_recovery_work.work);
	int rc;

	labibb->ibb_vreg.vreg_enabled = 0;
	labibb->lab_vreg.vreg_enabled = 0;
	rc = qpnp_labibb_force_enable(labibb);
	if (rc < 0)
		pr_err("force enable labibb failed, rc=%d\n", rc);

}

static int qpnp_lab_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	int rc, new_uV;
	u8 val;
	struct qpnp_labibb *labibb = rdev_get_drvdata(rdev);

	if (labibb->swire_control || labibb->secure_mode)
		return 0;

	if (min_uV < labibb->lab_vreg.min_volt) {
		pr_err("min_uV %d is less than min_volt %d", min_uV,
			labibb->lab_vreg.min_volt);
		return -EINVAL;
	}

	val = DIV_ROUND_UP(min_uV - labibb->lab_vreg.min_volt,
				labibb->lab_vreg.step_size);
	new_uV = val * labibb->lab_vreg.step_size + labibb->lab_vreg.min_volt;

	if (new_uV > max_uV) {
		pr_err("unable to set voltage %d (min:%d max:%d)\n", new_uV,
			min_uV, max_uV);
		return -EINVAL;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_VOLTAGE,
				LAB_VOLTAGE_SET_MASK |
				LAB_VOLTAGE_OVERRIDE_EN,
				val | LAB_VOLTAGE_OVERRIDE_EN);

	if (rc < 0) {
		pr_err("write to register %x failed rc = %d\n", REG_LAB_VOLTAGE,
			rc);
		return rc;
	}

	if (new_uV > labibb->lab_vreg.curr_volt) {
		val = DIV_ROUND_UP(new_uV - labibb->lab_vreg.curr_volt,
				labibb->lab_vreg.step_size);
		udelay(val * labibb->lab_vreg.slew_rate);
	}
	labibb->lab_vreg.curr_volt = new_uV;

	return 0;
}

static int qpnp_skip_swire_command(struct qpnp_labibb *labibb)
{
	int rc = 0, retry = 50, dly;
	u8 reg;

	do {
		/* poll for ibb vreg_ok */
		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
					REG_IBB_STATUS1, &reg, 1);
		if (rc < 0) {
			pr_err("Failed to read ibb_status1 reg rc=%d\n", rc);
			return rc;
		}
		if (reg & IBB_STATUS1_VREG_OK_BIT)
			break;

		/* poll delay */
		usleep_range(500, 600);

	} while (--retry);

	if (!retry) {
		pr_err("ibb vreg_ok failed to turn-on\n");
		return -EBUSY;
	}

	/* move to SW control */
	rc = qpnp_ibb_set_mode(labibb, IBB_SW_CONTROL_EN);
	if (rc < 0) {
		pr_err("Failed switch to IBB_SW_CONTROL rc=%d\n", rc);
		return rc;
	}

	/* delay to skip the second swire command */
	dly = labibb->swire_2nd_cmd_delay * 1000;
	while (dly / 20000) {
		usleep_range(20000, 20010);
		dly -= 20000;
	}
	if (dly)
		usleep_range(dly, dly + 10);

	rc = qpnp_ibb_set_mode(labibb, IBB_HW_SW_CONTROL);
	if (rc < 0) {
		pr_err("Failed switch to IBB_HW_SW_CONTROL rc=%d\n", rc);
		return rc;
	}

	/* delay for SPMI to SWIRE transition */
	usleep_range(1000, 1100);

	/* Move back to SWIRE control */
	rc = qpnp_ibb_set_mode(labibb, IBB_HW_CONTROL);
	if (rc < 0)
		pr_err("Failed switch to IBB_HW_CONTROL rc=%d\n", rc);

	/* delay before enabling the PS mode */
	msleep(labibb->swire_ibb_ps_enable_delay);
	rc = qpnp_ibb_ps_config(labibb, true);
	if (rc < 0)
		pr_err("Unable to enable IBB PS rc=%d\n", rc);

	return rc;
}

static irqreturn_t lab_vreg_ok_handler(int irq, void *_labibb)
{
	struct qpnp_labibb *labibb = _labibb;
	int rc;

	if (labibb->skip_2nd_swire_cmd && labibb->lab_dig_major < 2) {
		rc = qpnp_skip_swire_command(labibb);
		if (rc < 0)
			pr_err("Failed in 'qpnp_skip_swire_command' rc=%d\n",
				rc);
	} else if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE &&
		labibb->mode == QPNP_LABIBB_LCD_MODE) {
		rc = qpnp_lab_pfm_enable(labibb);
		if (rc < 0)
			pr_err("Failed to config PFM, rc=%d\n", rc);
	}

	return IRQ_HANDLED;
}

static int qpnp_lab_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->swire_control)
		return 0;

	return labibb->lab_vreg.curr_volt;
}

static bool is_lab_vreg_ok_irq_available(struct qpnp_labibb *labibb)
{
	/*
	 * LAB VREG_OK interrupt is used only to skip 2nd SWIRE command in
	 * dig_major < 2 targets. For pmi8998, it is used to enable PFM in
	 * LCD mode.
	 */
	if (labibb->skip_2nd_swire_cmd && labibb->lab_dig_major < 2)
		return true;

	if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE &&
		labibb->mode == QPNP_LABIBB_LCD_MODE) {
		if (labibb->ttw_en)
			return false;
		return true;
	}

	return false;
}

static struct regulator_ops qpnp_lab_ops = {
	.enable			= qpnp_lab_regulator_enable,
	.disable		= qpnp_lab_regulator_disable,
	.is_enabled		= qpnp_lab_regulator_is_enabled,
	.set_voltage		= qpnp_lab_regulator_set_voltage,
	.get_voltage		= qpnp_lab_regulator_get_voltage,
};

static int register_qpnp_lab_regulator(struct qpnp_labibb *labibb,
					struct device_node *of_node)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_desc *rdesc = &labibb->lab_vreg.rdesc;
	struct regulator_config cfg = {};
	u8 val, mask;
	const char *current_sense_str;
	bool config_current_sense = false;
	u32 tmp;

	if (!of_node) {
		dev_err(labibb->dev, "qpnp lab regulator device tree node is missing\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(labibb->dev, of_node, rdesc);
	if (!init_data) {
		pr_err("unable to get regulator init data for qpnp lab regulator\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-lab-min-voltage",
					&(labibb->lab_vreg.min_volt));
	if (rc < 0) {
		pr_err("qcom,qpnp-lab-min-voltage is missing, rc = %d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-lab-step-size",
					&(labibb->lab_vreg.step_size));
	if (rc < 0) {
		pr_err("qcom,qpnp-lab-step-size is missing, rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-lab-slew-rate",
					&(labibb->lab_vreg.slew_rate));
	if (rc < 0) {
		pr_err("qcom,qpnp-lab-slew-rate is missing, rc = %d\n",
			rc);
		return rc;
	}

	labibb->notify_lab_vreg_ok_sts = of_property_read_bool(of_node,
					"qcom,notify-lab-vreg-ok-sts");

	labibb->lab_vreg.sc_wait_time_ms = -EINVAL;
	if (labibb->pmic_rev_id->pmic_subtype == PM660L_SUBTYPE &&
					labibb->detect_lab_sc)
		of_property_read_u32(of_node, "qcom,qpnp-lab-sc-wait-time-ms",
					&labibb->lab_vreg.sc_wait_time_ms);

	rc = of_property_read_u32(of_node, "qcom,qpnp-lab-soft-start",
					&(labibb->lab_vreg.soft_start));
	if (!rc) {
		for (val = 0; val < ARRAY_SIZE(lab_soft_start_table); val++)
			if (lab_soft_start_table[val] ==
					labibb->lab_vreg.soft_start)
				break;

		if (val == ARRAY_SIZE(lab_soft_start_table))
			val = ARRAY_SIZE(lab_soft_start_table) - 1;

		rc = qpnp_labibb_write(labibb, labibb->lab_base +
				REG_LAB_SOFT_START_CTL, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
				REG_LAB_SOFT_START_CTL, rc);
			return rc;
		}

		labibb->lab_vreg.soft_start = lab_soft_start_table
				[val & LAB_SOFT_START_CTL_MASK];
	}

	val = 0;
	mask = 0;
	rc = of_property_read_u32(of_node,
		"qcom,qpnp-lab-max-precharge-time", &tmp);
	if (!rc) {
		for (val = 0; val < ARRAY_SIZE(lab_max_precharge_table); val++)
			if (lab_max_precharge_table[val] == tmp)
				break;

		if (val == ARRAY_SIZE(lab_max_precharge_table)) {
			pr_err("Invalid value in qcom,qpnp-lab-max-precharge-time\n");
			return -EINVAL;
		}

		mask = LAB_MAX_PRECHARGE_TIME_MASK;
	}

	if (of_property_read_bool(of_node,
			"qcom,qpnp-lab-max-precharge-enable")) {
		val |= LAB_FAST_PRECHARGE_CTL_EN;
		mask |= LAB_FAST_PRECHARGE_CTL_EN;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_PRECHARGE_CTL, mask, val);
	if (rc < 0) {
		pr_err("qpnp_lab_dt_init write register %x failed rc = %d\n",
			REG_LAB_PRECHARGE_CTL, rc);
		return rc;
	}

	if (labibb->mode == QPNP_LABIBB_AMOLED_MODE &&
		labibb->pmic_rev_id->pmic_subtype != PM660L_SUBTYPE) {
		/*
		 * default to 1.5 times current gain if
		 * user doesn't specify the current-sense
		 * dt parameter
		 */
		current_sense_str = "1.5x";
		val = qpnp_labibb_get_matching_idx(current_sense_str);
		config_current_sense = true;
	}

	if (of_find_property(of_node,
		"qcom,qpnp-lab-current-sense", NULL)) {
		config_current_sense = true;
		rc = of_property_read_string(of_node,
			"qcom,qpnp-lab-current-sense",
			&current_sense_str);
		if (!rc) {
			val = qpnp_labibb_get_matching_idx(
					current_sense_str);
		} else {
			pr_err("qcom,qpnp-lab-current-sense configured incorrectly rc = %d\n",
				rc);
			return rc;
		}
	}

	if (config_current_sense) {
		rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
			REG_LAB_CURRENT_SENSE,
			LAB_CURRENT_SENSE_GAIN_MASK,
			val);
		if (rc < 0) {
			pr_err("qpnp_labibb_write register %x failed rc = %d\n",
				REG_LAB_CURRENT_SENSE, rc);
			return rc;
		}
	}

	val = (labibb->standalone) ? 0 : LAB_IBB_EN_RDY_EN;
	rc = qpnp_labibb_sec_write(labibb, labibb->lab_base,
				REG_LAB_IBB_EN_RDY, val);

	if (rc < 0) {
		pr_err("qpnp_lab_sec_write register %x failed rc = %d\n",
			REG_LAB_IBB_EN_RDY, rc);
		return rc;
	}

	rc = qpnp_labibb_read(labibb, labibb->ibb_base + REG_IBB_ENABLE_CTL,
				&val, 1);
	if (rc < 0) {
		pr_err("qpnp_labibb_read register %x failed rc = %d\n",
			REG_IBB_ENABLE_CTL, rc);
		return rc;
	}

	if (!(val & (IBB_ENABLE_CTL_SWIRE_RDY | IBB_ENABLE_CTL_MODULE_EN))) {
		/* SWIRE_RDY and IBB_MODULE_EN not enabled */
		rc = qpnp_lab_dt_init(labibb, of_node);
		if (rc < 0) {
			pr_err("qpnp-lab: wrong DT parameter specified: rc = %d\n",
				rc);
			return rc;
		}
	} else {
		rc = labibb->ibb_ver_ops->get_mode(labibb);

		rc = qpnp_labibb_read(labibb, labibb->lab_base +
					REG_LAB_VOLTAGE, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_lab_read read register %x failed rc = %d\n",
				REG_LAB_VOLTAGE, rc);
			return rc;
		}

		labibb->lab_vreg.curr_volt =
					(val &
					LAB_VOLTAGE_SET_MASK) *
					labibb->lab_vreg.step_size +
					labibb->lab_vreg.min_volt;
		if (labibb->mode == QPNP_LABIBB_LCD_MODE) {
			rc = of_property_read_u32(of_node,
				"qcom,qpnp-lab-init-lcd-voltage",
				&(labibb->lab_vreg.curr_volt));
			if (rc < 0) {
				pr_err("get qcom,qpnp-lab-init-lcd-voltage failed, rc = %d\n",
					rc);
				return rc;
			}
		} else if (!(val & LAB_VOLTAGE_OVERRIDE_EN)) {
			rc = of_property_read_u32(of_node,
				"qcom,qpnp-lab-init-amoled-voltage",
				&(labibb->lab_vreg.curr_volt));
			if (rc < 0) {
				pr_err("get qcom,qpnp-lab-init-amoled-voltage failed, rc = %d\n",
					rc);
				return rc;
			}
		}

		labibb->lab_vreg.vreg_enabled = 1;
	}

	if (is_lab_vreg_ok_irq_available(labibb)) {
		irq_set_status_flags(labibb->lab_vreg.lab_vreg_ok_irq,
				     IRQ_DISABLE_UNLAZY);
		rc = devm_request_threaded_irq(labibb->dev,
				labibb->lab_vreg.lab_vreg_ok_irq, NULL,
				lab_vreg_ok_handler,
				IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				"lab-vreg-ok", labibb);
		if (rc) {
			pr_err("Failed to register 'lab-vreg-ok' irq rc=%d\n",
						rc);
			return rc;
		}
	}

	if (labibb->lab_vreg.lab_sc_irq != -EINVAL) {
		irq_set_status_flags(labibb->lab_vreg.lab_sc_irq,
				     IRQ_DISABLE_UNLAZY);
		rc = devm_request_threaded_irq(labibb->dev,
				labibb->lab_vreg.lab_sc_irq, NULL,
				labibb_sc_err_handler,
				IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				"lab-sc-err", labibb);
		if (rc) {
			pr_err("Failed to register 'lab-sc-err' irq rc=%d\n",
						rc);
			return rc;
		}
	}
	rc = qpnp_labibb_read(labibb, labibb->lab_base + REG_LAB_MODULE_RDY,
				&val, 1);
	if (rc < 0) {
		pr_err("qpnp_lab_read read register %x failed rc = %d\n",
			REG_LAB_MODULE_RDY, rc);
		return rc;
	}

	if (!(val & LAB_MODULE_RDY_EN)) {
		val = LAB_MODULE_RDY_EN;

		rc = qpnp_labibb_write(labibb, labibb->lab_base +
			REG_LAB_MODULE_RDY, &val, 1);

		if (rc < 0) {
			pr_err("qpnp_lab_dt_init write register %x failed rc = %d\n",
				REG_LAB_MODULE_RDY, rc);
			return rc;
		}
	}

	if (init_data->constraints.name) {
		rdesc->owner		= THIS_MODULE;
		rdesc->type		= REGULATOR_VOLTAGE;
		rdesc->ops		= &qpnp_lab_ops;
		rdesc->name		= init_data->constraints.name;

		cfg.dev = labibb->dev;
		cfg.init_data = init_data;
		cfg.driver_data = labibb;
		cfg.of_node = of_node;

		if (of_get_property(labibb->dev->of_node, "parent-supply",
				NULL))
			init_data->supply_regulator = "parent";

		init_data->constraints.valid_ops_mask
				|= REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS;

		labibb->lab_vreg.rdev = regulator_register(rdesc, &cfg);
		if (IS_ERR(labibb->lab_vreg.rdev)) {
			rc = PTR_ERR(labibb->lab_vreg.rdev);
			labibb->lab_vreg.rdev = NULL;
			pr_err("unable to get regulator init data for qpnp lab regulator, rc = %d\n",
				rc);

			return rc;
		}
	} else {
		dev_err(labibb->dev, "qpnp lab regulator name missing\n");
		return -EINVAL;
	}

	return 0;
}

static int qpnp_ibb_pfm_mode_enable(struct qpnp_labibb *labibb,
			struct device_node *of_node)
{
	int rc = 0;
	u32 i, tmp = 0;
	u8 val = IBB_PFM_ENABLE;

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-pfm-peak-curr",
				&tmp);
	if (rc < 0) {
		pr_err("qcom,qpnp-ibb-pfm-peak-curr is missing, rc = %d\n",
			rc);
		return rc;
	}
	for (i = 0; i < ARRAY_SIZE(ibb_pfm_peak_curr_table); i++)
		if (ibb_pfm_peak_curr_table[i] == tmp)
			break;

	if (i == ARRAY_SIZE(ibb_pfm_peak_curr_table)) {
		pr_err("Invalid value in qcom,qpnp-ibb-pfm-peak-curr\n");
		return -EINVAL;
	}

	val |= (i << IBB_PFM_PEAK_CURRENT_BIT_SHIFT);

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-pfm-hysteresis",
				&tmp);
	if (rc < 0) {
		pr_err("qcom,qpnp-ibb-pfm-hysteresis is missing, rc = %d\n",
			rc);
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(ibb_pfm_hysteresis_table); i++)
		if (ibb_pfm_hysteresis_table[i] == tmp)
			break;

	if (i == ARRAY_SIZE(ibb_pfm_hysteresis_table)) {
		pr_err("Invalid value in qcom,qpnp-ibb-pfm-hysteresis\n");
		return -EINVAL;
	}

	val |= (i << IBB_PFM_HYSTERESIS_BIT_SHIFT);

	rc = qpnp_labibb_write(labibb, labibb->ibb_base +
				REG_IBB_PFM_CTL, &val, 1);
	if (rc < 0)
		pr_err("qpnp_ibb_pfm_ctl write register %x failed rc = %d\n",
					REG_IBB_PFM_CTL, rc);

	return rc;
}

static int qpnp_labibb_pbs_mode_enable(struct qpnp_labibb *labibb,
			struct device_node *of_node)
{
	int rc = 0;

	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
				REG_IBB_SWIRE_CTL,
				IBB_SWIRE_VOUT_UPD_EN, 0);
	if (rc < 0) {
		pr_err("qpnp_ibb_swire_ctl write register %x failed rc = %d\n",
					REG_IBB_SWIRE_CTL, rc);
		return rc;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
				REG_IBB_PD_CTL, IBB_SWIRE_PD_UPD, 0);
	if (rc < 0) {
		pr_err("qpnp_ibb_pd_ctl write register %x failed rc = %d\n",
					REG_IBB_PD_CTL, rc);
		return rc;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->lab_base +
				REG_LAB_SWIRE_PGM_CTL, LAB_EN_SWIRE_PGM_VOUT |
				LAB_EN_SWIRE_PGM_PD, 0);
	if (rc < 0)
		pr_err("qpnp_lab_swire_pgm_ctl write register %x failed rc = %d\n",
					REG_LAB_SWIRE_PGM_CTL, rc);

	return rc;
}

static int qpnp_ibb_slew_rate_config(struct qpnp_labibb *labibb,
			struct device_node *of_node)
{
	int rc = 0;
	u32 i, tmp = 0;
	u8 val = 0, mask = 0;

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-fast-slew-rate",
						&tmp);
	if (!rc) {
		for (i = 0; i < ARRAY_SIZE(ibb_output_slew_ctl_table); i++)
			if (ibb_output_slew_ctl_table[i] == tmp)
				break;

		if (i == ARRAY_SIZE(ibb_output_slew_ctl_table)) {
			pr_err("Invalid value in qcom,qpnp-ibb-fast-slew-rate\n");
			return -EINVAL;
		}

		labibb->ibb_vreg.slew_rate = tmp;
		val |= (i << IBB_SLEW_RATE_TRANS_TIME_FAST_SHIFT) |
				IBB_SLEW_RATE_SPEED_FAST_EN | IBB_SLEW_CTL_EN;

		mask = IBB_SLEW_RATE_SPEED_FAST_EN |
			IBB_SLEW_RATE_TRANS_TIME_FAST_MASK | IBB_SLEW_CTL_EN;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-slow-slew-rate",
				&tmp);
	if (!rc) {
		for (i = 0; i < ARRAY_SIZE(ibb_output_slew_ctl_table); i++)
			if (ibb_output_slew_ctl_table[i] == tmp)
				break;

		if (i == ARRAY_SIZE(ibb_output_slew_ctl_table)) {
			pr_err("Invalid value in qcom,qpnp-ibb-slow-slew-rate\n");
			return -EINVAL;
		}

		labibb->ibb_vreg.slew_rate = tmp;
		val |= (i | IBB_SLEW_CTL_EN);

		mask |= IBB_SLEW_RATE_SPEED_FAST_EN |
			IBB_SLEW_RATE_TRANS_TIME_SLOW_MASK | IBB_SLEW_CTL_EN;
	}

	rc = qpnp_labibb_masked_write(labibb, labibb->ibb_base +
				REG_IBB_OUTPUT_SLEW_CTL,
				mask, val);
	if (rc < 0)
		pr_err("qpnp_labibb_write register %x failed rc = %d\n",
			REG_IBB_OUTPUT_SLEW_CTL, rc);

	return rc;
}

static bool qpnp_ibb_poff_ctl_required(struct qpnp_labibb *labibb)
{
	if (labibb->pmic_rev_id->pmic_subtype == PM660L_SUBTYPE)
		return false;

	return true;
}

static int qpnp_ibb_dt_init(struct qpnp_labibb *labibb,
				struct device_node *of_node)
{
	int rc = 0;
	u32 i = 0, tmp = 0;
	u8 val, mask;

	/*
	 * Do not configure LCD_AMOLED_SEL for pmi8998 as it will be done by
	 * GPIO selector. Override the labibb->mode with what was configured
	 * by the bootloader.
	 */
	if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE) {
		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
				REG_IBB_LCD_AMOLED_SEL, &val, 1);
		if (rc) {
			pr_err("qpnp_labibb_read register %x failed rc = %d\n",
						REG_IBB_LCD_AMOLED_SEL, rc);
			return rc;
		}
		if (val == REG_LAB_IBB_AMOLED_MODE)
			labibb->mode = QPNP_LABIBB_AMOLED_MODE;
		else
			labibb->mode = QPNP_LABIBB_LCD_MODE;
	} else {
		rc = labibb->ibb_ver_ops->sel_mode(labibb, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_sec_write register %x failed rc = %d\n",
				REG_IBB_LCD_AMOLED_SEL, rc);
			return rc;
		}
	}

	val = 0;
	mask = 0;
	rc = of_property_read_u32(of_node,
		"qcom,qpnp-ibb-lab-pwrdn-delay", &tmp);
	if (!rc) {
		if (tmp > 0) {
			for (i = 0; i < ARRAY_SIZE(ibb_pwrdn_dly_table); i++) {
				if (ibb_pwrdn_dly_table[i] == tmp)
					break;
			}

			if (i == ARRAY_SIZE(ibb_pwrdn_dly_table)) {
				pr_err("Invalid value in qcom,qpnp-ibb-lab-pwrdn-delay\n");
				return -EINVAL;
			}
		}

		labibb->ibb_vreg.pwrdn_dly = tmp;

		if (tmp > 0)
			val = i | IBB_PWRUP_PWRDN_CTL_1_EN_DLY2;

		mask |= IBB_PWRUP_PWRDN_CTL_1_EN_DLY2;
	}

	rc = of_property_read_u32(of_node,
			"qcom,qpnp-ibb-lab-pwrup-delay", &tmp);
	if (!rc) {
		if (tmp > 0) {
			for (i = 0; i < ARRAY_SIZE(ibb_pwrup_dly_table); i++) {
				if (ibb_pwrup_dly_table[i] == tmp)
					break;
			}

			if (i == ARRAY_SIZE(ibb_pwrup_dly_table)) {
				pr_err("Invalid value in qcom,qpnp-ibb-lab-pwrup-delay\n");
				return -EINVAL;
			}
		}

		labibb->ibb_vreg.pwrup_dly = tmp;

		if (tmp > 0)
			val |= IBB_PWRUP_PWRDN_CTL_1_EN_DLY1;

		val |= (i << IBB_PWRUP_PWRDN_CTL_1_DLY1_SHIFT);
		val |= IBB_PWRUP_PWRDN_CTL_1_LAB_VREG_OK;
		mask |= (IBB_PWRUP_PWRDN_CTL_1_EN_DLY1 |
			IBB_PWRUP_PWRDN_CTL_1_DLY1_MASK |
			IBB_PWRUP_PWRDN_CTL_1_LAB_VREG_OK);
	}

	if (of_property_read_bool(of_node,
				"qcom,qpnp-ibb-en-discharge")) {
		val |= PWRUP_PWRDN_CTL_1_DISCHARGE_EN;
		mask |= PWRUP_PWRDN_CTL_1_DISCHARGE_EN;
	}

	rc = qpnp_labibb_sec_masked_write(labibb, labibb->ibb_base,
				REG_IBB_PWRUP_PWRDN_CTL_1, mask, val);
	if (rc < 0) {
		pr_err("qpnp_labibb_sec_write register %x failed rc = %d\n",
			REG_IBB_PWRUP_PWRDN_CTL_1, rc);
		return rc;
	}

	if (of_property_read_bool(of_node, "qcom,qpnp-ibb-slew-rate-config")) {

		rc = qpnp_ibb_slew_rate_config(labibb, of_node);
		if (rc < 0)
			return rc;
	}

	val = 0;
	if (!of_property_read_bool(of_node, "qcom,qpnp-ibb-full-pull-down"))
		val = IBB_PD_CTL_HALF_STRENGTH;

	if (of_property_read_bool(of_node, "qcom,qpnp-ibb-pull-down-enable"))
		val |= IBB_PD_CTL_EN;

	mask = IBB_PD_CTL_STRENGTH_MASK | IBB_PD_CTL_EN;
	rc = qpnp_labibb_masked_write(labibb,
			labibb->ibb_base + REG_IBB_PD_CTL, mask, val);

	if (rc < 0) {
		pr_err("qpnp_lab_dt_init write register %x failed rc = %d\n",
				REG_IBB_PD_CTL, rc);
		return rc;
	}

	rc = of_property_read_u32(of_node,
		"qcom,qpnp-ibb-switching-clock-frequency", &tmp);
	if (!rc) {
		for (val = 0; val < ARRAY_SIZE(ibb_clk_div_table); val++)
			if (ibb_clk_div_table[val] == tmp)
				break;

		if (val == ARRAY_SIZE(ibb_clk_div_table)) {
			pr_err("Invalid value in qpnp-ibb-switching-clock-frequency\n");
			return -EINVAL;
		}
		rc = labibb->ibb_ver_ops->set_clk_div(labibb, val);
		if (rc < 0) {
			pr_err("qpnp_ibb_dt_init write register %x failed rc = %d\n",
				REG_IBB_CLK_DIV, rc);
			return rc;
		}
	}

	val = 0;
	mask = 0;
	rc = of_property_read_u32(of_node,
		"qcom,qpnp-ibb-limit-maximum-current", &tmp);
	if (!rc) {
		for (val = 0; val < ARRAY_SIZE(ibb_current_limit_table); val++)
			if (ibb_current_limit_table[val] == tmp)
				break;

		if (val == ARRAY_SIZE(ibb_current_limit_table)) {
			pr_err("Invalid value in qcom,qpnp-ibb-limit-maximum-current\n");
			return -EINVAL;
		}

		mask = IBB_CURRENT_LIMIT_MASK;
	}

	rc = of_property_read_u32(of_node,
		"qcom,qpnp-ibb-debounce-cycle", &tmp);
	if (!rc) {
		for (i = 0; i < ARRAY_SIZE(ibb_debounce_table); i++)
			if (ibb_debounce_table[i] == tmp)
				break;

		if (i == ARRAY_SIZE(ibb_debounce_table)) {
			pr_err("Invalid value in qcom,qpnp-ibb-debounce-cycle\n");
			return -EINVAL;
		}

		val |= (i << IBB_CURRENT_LIMIT_DEBOUNCE_SHIFT);
		mask |= IBB_CURRENT_LIMIT_DEBOUNCE_MASK;
	}

	if (of_property_read_bool(of_node,
		"qcom,qpnp-ibb-limit-max-current-enable")) {
		val |= IBB_CURRENT_LIMIT_EN;
		mask |= IBB_CURRENT_LIMIT_EN;
	}

	rc = qpnp_labibb_sec_masked_write(labibb, labibb->ibb_base,
				REG_IBB_CURRENT_LIMIT, mask, val);
	if (rc < 0) {
		pr_err("qpnp_labibb_sec_write register %x failed rc = %d\n",
				REG_IBB_CURRENT_LIMIT, rc);
		return rc;
	}

	if (of_property_read_bool(of_node,
		"qcom,qpnp-ibb-ring-suppression-enable")) {
		val = IBB_RING_SUPPRESSION_CTL_EN;
		rc = qpnp_labibb_write(labibb, labibb->ibb_base +
					REG_IBB_RING_SUPPRESSION_CTL,
					&val,
					1);
		if (rc < 0) {
			pr_err("qpnp_ibb_dt_init write register %x failed rc = %d\n",
				REG_IBB_RING_SUPPRESSION_CTL, rc);
			return rc;
		}
	}

	if (of_property_read_bool(of_node, "qcom,qpnp-ibb-ps-enable")) {
		rc = qpnp_ibb_ps_config(labibb, true);
		if (rc < 0) {
			pr_err("qpnp_ibb_dt_init PS enable failed rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = qpnp_ibb_ps_config(labibb, false);
		if (rc < 0) {
			pr_err("qpnp_ibb_dt_init PS disable failed rc=%d\n",
									rc);
			return rc;
		}
	}

	if (of_property_read_bool(of_node,
				 "qcom,qpnp-ibb-smart-ps-enable")){
		of_property_read_u32(of_node, "qcom,qpnp-ibb-num-swire-trans",
					&labibb->ibb_vreg.num_swire_trans);

		of_property_read_u32(of_node,
				"qcom,qpnp-ibb-neg-curr-limit", &tmp);

		rc = labibb->ibb_ver_ops->smart_ps_config(labibb, true,
					labibb->ibb_vreg.num_swire_trans, tmp);
		if (rc < 0) {
			pr_err("qpnp_ibb_dt_init smart PS enable failed rc=%d\n",
					 rc);
			return rc;
		}

	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-init-voltage",
					&(labibb->ibb_vreg.curr_volt));
	if (rc < 0) {
		pr_err("get qcom,qpnp-ibb-init-voltage failed, rc = %d\n", rc);
		return rc;
	}

	if (of_property_read_bool(of_node,
			"qcom,qpnp-ibb-use-default-voltage"))
		rc = labibb->ibb_ver_ops->set_default_voltage(labibb, true);
	else
		rc = labibb->ibb_ver_ops->set_default_voltage(labibb, false);

	if (rc < 0)
		return rc;

	if (of_property_read_bool(of_node, "qcom,qpnp-ibb-overload-blank")) {
		rc = qpnp_ibb_vreg_ok_ctl(labibb, of_node);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int qpnp_ibb_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->secure_mode)
		return 0;

	if (labibb->sc_detected) {
		pr_info("Short circuit detected: disabled LAB/IBB rails\n");
		return 0;
	}

	if (!labibb->ibb_vreg.vreg_enabled && !labibb->swire_control) {
		if (!labibb->standalone)
			return qpnp_labibb_regulator_enable(labibb);

		rc = qpnp_ibb_enable_standalone(labibb);
		if (rc < 0) {
			pr_err("enable ibb standalone failed, rc=%d\n", rc);
			return rc;
		}
		labibb->ibb_vreg.vreg_enabled = 1;
	}

	return 0;
}

static int qpnp_ibb_regulator_disable(struct regulator_dev *rdev)
{
	int rc;
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->secure_mode)
		return 0;

	if (labibb->ibb_vreg.vreg_enabled && !labibb->swire_control) {

		if (!labibb->standalone)
			return qpnp_labibb_regulator_disable(labibb);

		rc = qpnp_ibb_set_mode(labibb, IBB_SW_CONTROL_DIS);
		if (rc < 0) {
			pr_err("Unable to set IBB_MODULE_EN rc = %d\n", rc);
			return rc;
		}

		labibb->ibb_vreg.vreg_enabled = 0;
	}
	return 0;
}

static int qpnp_ibb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->swire_control)
		return 0;

	return labibb->ibb_vreg.vreg_enabled;
}

static int qpnp_ibb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	int rc = 0;

	struct qpnp_labibb *labibb = rdev_get_drvdata(rdev);

	if (labibb->swire_control || labibb->secure_mode)
		return 0;

	rc = labibb->ibb_ver_ops->set_voltage(labibb, min_uV, max_uV);
	return rc;
}

static int qpnp_ibb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_labibb *labibb  = rdev_get_drvdata(rdev);

	if (labibb->swire_control)
		return 0;

	return labibb->ibb_vreg.curr_volt;
}

static struct regulator_ops qpnp_ibb_ops = {
	.enable			= qpnp_ibb_regulator_enable,
	.disable		= qpnp_ibb_regulator_disable,
	.is_enabled		= qpnp_ibb_regulator_is_enabled,
	.set_voltage		= qpnp_ibb_regulator_set_voltage,
	.get_voltage		= qpnp_ibb_regulator_get_voltage,
};

static int register_qpnp_ibb_regulator(struct qpnp_labibb *labibb,
					struct device_node *of_node)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_desc *rdesc = &labibb->ibb_vreg.rdesc;
	struct regulator_config cfg = {};
	u8 val, ibb_enable_ctl, index;
	u32 tmp;

	if (!of_node) {
		dev_err(labibb->dev, "qpnp ibb regulator device tree node is missing\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(labibb->dev, of_node, rdesc);
	if (!init_data) {
		pr_err("unable to get regulator init data for qpnp ibb regulator\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-min-voltage",
					&(labibb->ibb_vreg.min_volt));
	if (rc < 0) {
		pr_err("qcom,qpnp-ibb-min-voltage is missing, rc = %d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-step-size",
					&(labibb->ibb_vreg.step_size));
	if (rc < 0) {
		pr_err("qcom,qpnp-ibb-step-size is missing, rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(of_node, "qcom,qpnp-ibb-slew-rate",
					&(labibb->ibb_vreg.slew_rate));
	if (rc < 0)
		labibb->ibb_vreg.slew_rate = IBB_HW_DEFAULT_SLEW_RATE;

	rc = labibb->ibb_ver_ops->soft_start_ctl(labibb, of_node);
	if (rc < 0) {
		pr_err("qpnp_labibb_write register %x failed rc = %d\n",
			REG_IBB_SOFT_START_CTL, rc);
		return rc;
	}

	if (of_find_property(of_node, "qcom,output-voltage-one-pulse", NULL)) {
		if (!labibb->swire_control) {
			pr_err("output-voltage-one-pulse valid for SWIRE only\n");
			return -EINVAL;
		}
		rc = of_property_read_u32(of_node,
				"qcom,output-voltage-one-pulse", &tmp);
		if (rc < 0) {
			pr_err("failed to read qcom,output-voltage-one-pulse rc=%d\n",
									rc);
			return rc;
		}
		if (tmp > MAX_OUTPUT_PULSE_VOLTAGE_MV ||
				tmp < MIN_OUTPUT_PULSE_VOLTAGE_MV) {
			pr_err("Invalid one-pulse voltage range %d\n", tmp);
			return -EINVAL;
		}
		rc = labibb->ibb_ver_ops->voltage_at_one_pulse(labibb, tmp);
		if (rc < 0)
			return rc;
	}

	rc = qpnp_labibb_read(labibb, labibb->ibb_base + REG_IBB_ENABLE_CTL,
				&ibb_enable_ctl, 1);
	if (rc < 0) {
		pr_err("qpnp_ibb_read register %x failed rc = %d\n",
			REG_IBB_ENABLE_CTL, rc);
		return rc;
	}

	/*
	 * For pmi8998, override swire_control with what was configured
	 * before by the bootloader.
	 */
	if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE)
		labibb->swire_control = ibb_enable_ctl &
						IBB_ENABLE_CTL_SWIRE_RDY;

	if (ibb_enable_ctl &
		(IBB_ENABLE_CTL_SWIRE_RDY | IBB_ENABLE_CTL_MODULE_EN)) {

		rc = labibb->ibb_ver_ops->get_mode(labibb);
		if (rc < 0) {
			pr_err("qpnp_labibb_read register %x failed rc = %d\n",
				REG_IBB_LCD_AMOLED_SEL, rc);
			return rc;
		}
		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
					REG_IBB_VOLTAGE, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_read read register %x failed rc = %d\n",
				REG_IBB_VOLTAGE, rc);
			return rc;
		}

		labibb->ibb_vreg.curr_volt =
			(val & IBB_VOLTAGE_SET_MASK) *
			labibb->ibb_vreg.step_size +
			labibb->ibb_vreg.min_volt;

		if (labibb->mode == QPNP_LABIBB_LCD_MODE) {
			rc = of_property_read_u32(of_node,
				"qcom,qpnp-ibb-init-lcd-voltage",
				&(labibb->ibb_vreg.curr_volt));
			if (rc < 0) {
				pr_err("get qcom,qpnp-ibb-init-lcd-voltage failed, rc = %d\n",
					rc);
				return rc;
			}
		} else if (!(val & IBB_VOLTAGE_OVERRIDE_EN)) {
			rc = of_property_read_u32(of_node,
				"qcom,qpnp-ibb-init-amoled-voltage",
				&(labibb->ibb_vreg.curr_volt));
			if (rc < 0) {
				pr_err("get qcom,qpnp-ibb-init-amoled-voltage failed, rc = %d\n",
					rc);
				return rc;
			}

		}

		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
				REG_IBB_PWRUP_PWRDN_CTL_1, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_config_init read register %x failed rc = %d\n",
				REG_IBB_PWRUP_PWRDN_CTL_1, rc);
			return rc;
		}

		index = (val & IBB_PWRUP_PWRDN_CTL_1_DLY1_MASK) >>
				IBB_PWRUP_PWRDN_CTL_1_DLY1_SHIFT;
		labibb->ibb_vreg.pwrup_dly = ibb_pwrup_dly_table[index];
		index = val & IBB_PWRUP_PWRDN_CTL_1_DLY2_MASK;
		labibb->ibb_vreg.pwrdn_dly =  ibb_pwrdn_dly_table[index];

		labibb->ibb_vreg.vreg_enabled = 1;
	} else {
		/* SWIRE_RDY and IBB_MODULE_EN not enabled */
		rc = qpnp_ibb_dt_init(labibb, of_node);
		if (rc < 0) {
			pr_err("qpnp-ibb: wrong DT parameter specified: rc = %d\n",
				rc);
			return rc;
		}
	}

	if (labibb->mode == QPNP_LABIBB_AMOLED_MODE &&
			qpnp_ibb_poff_ctl_required(labibb)) {

		val = IBB_OVERRIDE_NONOVERLAP | IBB_NFET_GATE_DELAY_2;
		rc = qpnp_labibb_sec_masked_write(labibb, labibb->ibb_base,
			REG_IBB_NONOVERLAP_TIME_1,
			IBB_OVERRIDE_NONOVERLAP | IBB_NONOVERLAP_NFET_MASK,
			val);

		if (rc < 0) {
			pr_err("qpnp_labibb_sec_masked_write register %x failed rc = %d\n",
				REG_IBB_NONOVERLAP_TIME_1, rc);
			return rc;
		}

		val = IBB_N2P_MUX_SEL;
		rc = qpnp_labibb_sec_write(labibb, labibb->ibb_base,
			REG_IBB_NONOVERLAP_TIME_2, val);

		if (rc < 0) {
			pr_err("qpnp_labibb_sec_write register %x failed rc = %d\n",
				REG_IBB_NONOVERLAP_TIME_2, rc);
			return rc;
		}

		val = IBB_FASTER_PFET_OFF;
		rc = qpnp_labibb_masked_write(labibb,
				labibb->ibb_base + REG_IBB_SPARE_CTL,
				IBB_POFF_CTL_MASK, val);
		if (rc < 0) {
			pr_err("write to register %x failed rc = %d\n",
				 REG_IBB_SPARE_CTL, rc);
			return rc;
		}
	}

	if (labibb->standalone) {
		val = 0;
		rc = qpnp_labibb_sec_write(labibb, labibb->ibb_base,
				REG_IBB_PWRUP_PWRDN_CTL_1, val);
		if (rc < 0) {
			pr_err("qpnp_labibb_sec_write register %x failed rc = %d\n",
				REG_IBB_PWRUP_PWRDN_CTL_1, rc);
			return rc;
		}
		labibb->ibb_vreg.pwrup_dly = 0;
		labibb->ibb_vreg.pwrdn_dly = 0;
	}

	if (labibb->ibb_vreg.ibb_sc_irq != -EINVAL) {
		irq_set_status_flags(labibb->ibb_vreg.ibb_sc_irq,
				     IRQ_DISABLE_UNLAZY);
		rc = devm_request_threaded_irq(labibb->dev,
				labibb->ibb_vreg.ibb_sc_irq, NULL,
				labibb_sc_err_handler,
				IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				"ibb-sc-err", labibb);
		if (rc) {
			pr_err("Failed to register 'ibb-sc-err' irq rc=%d\n",
						rc);
			return rc;
		}
	}

	rc = qpnp_labibb_read(labibb, labibb->ibb_base + REG_IBB_MODULE_RDY,
				&val, 1);
	if (rc < 0) {
		pr_err("qpnp_ibb_read read register %x failed rc = %d\n",
			REG_IBB_MODULE_RDY, rc);
		return rc;
	}

	if (!(val & IBB_MODULE_RDY_EN)) {
		val = IBB_MODULE_RDY_EN;

		rc = qpnp_labibb_write(labibb, labibb->ibb_base +
			REG_IBB_MODULE_RDY, &val, 1);

		if (rc < 0) {
			pr_err("qpnp_ibb_dt_init write register %x failed rc = %d\n",
				REG_IBB_MODULE_RDY, rc);
			return rc;
		}
	}

	if (of_property_read_bool(of_node,
			"qcom,qpnp-ibb-enable-pfm-mode")) {
		rc = qpnp_ibb_pfm_mode_enable(labibb, of_node);
		if (rc < 0)
			return rc;
	}

	if (labibb->pbs_control) {
		rc = qpnp_labibb_pbs_mode_enable(labibb, of_node);
		if (rc < 0)
			return rc;
	}

	if (init_data->constraints.name) {
		rdesc->owner		= THIS_MODULE;
		rdesc->type		= REGULATOR_VOLTAGE;
		rdesc->ops		= &qpnp_ibb_ops;
		rdesc->name		= init_data->constraints.name;

		cfg.dev = labibb->dev;
		cfg.init_data = init_data;
		cfg.driver_data = labibb;
		cfg.of_node = of_node;

		if (of_get_property(labibb->dev->of_node, "parent-supply",
				 NULL))
			init_data->supply_regulator = "parent";

		init_data->constraints.valid_ops_mask
				|= REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS;

		labibb->ibb_vreg.rdev = regulator_register(rdesc, &cfg);
		if (IS_ERR(labibb->ibb_vreg.rdev)) {
			rc = PTR_ERR(labibb->ibb_vreg.rdev);
			labibb->ibb_vreg.rdev = NULL;
			pr_err("unable to get regulator init data for qpnp ibb regulator, rc = %d\n",
				rc);

			return rc;
		}
	} else {
		dev_err(labibb->dev, "qpnp ibb regulator name missing\n");
		return -EINVAL;
	}

	return 0;
}

static int qpnp_lab_register_irq(struct device_node *child,
				struct qpnp_labibb *labibb)
{
	int rc = 0;

	if (is_lab_vreg_ok_irq_available(labibb)) {
		rc = of_irq_get_byname(child, "lab-vreg-ok");
		if (rc < 0) {
			pr_err("Invalid lab-vreg-ok irq\n");
			return rc;
		}
		labibb->lab_vreg.lab_vreg_ok_irq = rc;
	}

	labibb->lab_vreg.lab_sc_irq = -EINVAL;
	rc = of_irq_get_byname(child, "lab-sc-err");
	if (rc < 0)
		pr_debug("Unable to get lab-sc-err, rc = %d\n", rc);
	else
		labibb->lab_vreg.lab_sc_irq = rc;

	return 0;
}

static int qpnp_ibb_register_irq(struct device_node *child,
				struct qpnp_labibb *labibb)
{
	int rc;

	labibb->ibb_vreg.ibb_sc_irq = -EINVAL;
	rc = of_irq_get_byname(child, "ibb-sc-err");
	if (rc < 0)
		pr_debug("Unable to get ibb-sc-err, rc = %d\n", rc);
	else
		labibb->ibb_vreg.ibb_sc_irq = rc;

	return 0;
}

static int qpnp_labibb_check_ttw_supported(struct qpnp_labibb *labibb)
{
	int rc = 0;
	u8 val;

	switch (labibb->pmic_rev_id->pmic_subtype) {
	case PMI8996_SUBTYPE:
		rc = qpnp_labibb_read(labibb, labibb->ibb_base +
					REG_IBB_REVISION4, &val, 1);
		if (rc < 0) {
			pr_err("qpnp_labibb_read register %x failed rc = %d\n",
				REG_IBB_REVISION4, rc);
			return rc;
		}

		/* PMI8996 has revision 1 */
		if (val < 1) {
			pr_err("TTW feature cannot be enabled for revision %d\n",
									val);
			labibb->ttw_en = false;
		}
		/* FORCE_LAB_ON in TTW is not required for PMI8996 */
		labibb->ttw_force_lab_on = false;
		break;
	case PMI8950_SUBTYPE:
		/* TTW supported for all revisions */
		break;
	case PMI8998_SUBTYPE:
		/* TTW supported for all revisions */
		break;
	default:
		pr_info("TTW mode not supported for PMIC-subtype = %d\n",
					labibb->pmic_rev_id->pmic_subtype);
		labibb->ttw_en = false;
		break;

	}
	return rc;
}

static ssize_t qpnp_labibb_irq_control(struct class *c,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	struct qpnp_labibb *labibb = container_of(c, struct qpnp_labibb,
						  labibb_class);
	int val, rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val != 0 && val != 1)
		return count;

	/* Disable irqs */
	if (val == 1 && !labibb->secure_mode) {
		if (labibb->lab_vreg.lab_vreg_ok_irq > 0)
			disable_irq(labibb->lab_vreg.lab_vreg_ok_irq);
		if (labibb->lab_vreg.lab_sc_irq > 0)
			disable_irq(labibb->lab_vreg.lab_sc_irq);
		if (labibb->ibb_vreg.ibb_sc_irq > 0)
			disable_irq(labibb->ibb_vreg.ibb_sc_irq);
		labibb->secure_mode = true;
	} else if (val == 0 && labibb->secure_mode) {
		if (labibb->lab_vreg.lab_vreg_ok_irq > 0)
			enable_irq(labibb->lab_vreg.lab_vreg_ok_irq);
		if (labibb->lab_vreg.lab_sc_irq > 0)
			enable_irq(labibb->lab_vreg.lab_sc_irq);
		if (labibb->ibb_vreg.ibb_sc_irq > 0)
			enable_irq(labibb->ibb_vreg.ibb_sc_irq);
		labibb->secure_mode = false;
	}

	return count;
}

static struct class_attribute labibb_attributes[] = {
	[0] = __ATTR(secure_mode, 0664, NULL,
			 qpnp_labibb_irq_control),
	 __ATTR_NULL,
};

static int qpnp_labibb_regulator_probe(struct platform_device *pdev)
{
	struct qpnp_labibb *labibb;
	unsigned int base;
	struct device_node *child, *revid_dev_node;
	const char *mode_name;
	u8 type, revision;
	int rc = 0;

	labibb = devm_kzalloc(&pdev->dev, sizeof(*labibb), GFP_KERNEL);
	if (labibb == NULL)
		return -ENOMEM;

	labibb->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!labibb->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	labibb->dev = &(pdev->dev);
	labibb->pdev = pdev;

	mutex_init(&(labibb->lab_vreg.lab_mutex));
	mutex_init(&(labibb->ibb_vreg.ibb_mutex));
	mutex_init(&(labibb->bus_mutex));

	revid_dev_node = of_parse_phandle(labibb->dev->of_node,
					"qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	labibb->pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR(labibb->pmic_rev_id)) {
		pr_debug("Unable to get revid data\n");
		return -EPROBE_DEFER;
	}

	if (labibb->pmic_rev_id->pmic_subtype == PM660L_SUBTYPE) {
		labibb->ibb_ver_ops = &ibb_ops_v2;
		labibb->lab_ver_ops = &pm660_lab_ops;
	} else if (labibb->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE) {
		labibb->ibb_ver_ops = &ibb_ops_v1;
		labibb->lab_ver_ops = &pmi8998_lab_ops;
	} else {
		labibb->ibb_ver_ops = &ibb_ops_v1;
		labibb->lab_ver_ops = &lab_ops_v1;
	}

	if (labibb->pmic_rev_id->pmic_subtype == PM660L_SUBTYPE) {
		labibb->mode = QPNP_LABIBB_AMOLED_MODE;
		/* Enable polling for LAB short circuit detection for PM660A */
		labibb->detect_lab_sc = true;
	} else {
		rc = of_property_read_string(labibb->dev->of_node,
				"qcom,qpnp-labibb-mode", &mode_name);
		if (!rc) {
			if (strcmp("lcd", mode_name) == 0) {
				labibb->mode = QPNP_LABIBB_LCD_MODE;
			} else if (strcmp("amoled", mode_name) == 0) {
				labibb->mode = QPNP_LABIBB_AMOLED_MODE;
			} else {
				pr_err("Invalid device property in qcom,qpnp-labibb-mode: %s\n",
					mode_name);
				return -EINVAL;
			}
		} else {
			pr_err("qpnp_labibb: qcom,qpnp-labibb-mode is missing.\n");
			return rc;
		}
	}

	labibb->standalone = of_property_read_bool(labibb->dev->of_node,
				"qcom,labibb-standalone");

	labibb->ttw_en = of_property_read_bool(labibb->dev->of_node,
				"qcom,labibb-touch-to-wake-en");
	if (labibb->ttw_en && labibb->mode != QPNP_LABIBB_LCD_MODE) {
		pr_err("Invalid mode for TTW\n");
		return -EINVAL;
	}

	labibb->ttw_force_lab_on = of_property_read_bool(
		labibb->dev->of_node, "qcom,labibb-ttw-force-lab-on");

	labibb->swire_control = of_property_read_bool(labibb->dev->of_node,
							"qcom,swire-control");

	labibb->pbs_control = of_property_read_bool(labibb->dev->of_node,
							"qcom,pbs-control");
	if (labibb->swire_control && labibb->mode != QPNP_LABIBB_AMOLED_MODE) {
		pr_err("Invalid mode for SWIRE control\n");
		return -EINVAL;
	}

	if (labibb->swire_control) {
		labibb->skip_2nd_swire_cmd =
				of_property_read_bool(labibb->dev->of_node,
				"qcom,skip-2nd-swire-cmd");

		rc = of_property_read_u32(labibb->dev->of_node,
				"qcom,swire-2nd-cmd-delay",
				&labibb->swire_2nd_cmd_delay);
		if (rc < 0)
			labibb->swire_2nd_cmd_delay =
					SWIRE_DEFAULT_2ND_CMD_DLY_MS;

		rc = of_property_read_u32(labibb->dev->of_node,
				"qcom,swire-ibb-ps-enable-delay",
				&labibb->swire_ibb_ps_enable_delay);
		if (rc < 0)
			labibb->swire_ibb_ps_enable_delay =
					SWIRE_DEFAULT_IBB_PS_ENABLE_DLY_MS;
	}

	if (of_get_available_child_count(pdev->dev.of_node) == 0) {
		pr_err("no child nodes\n");
		return -ENXIO;
	}

	for_each_available_child_of_node(pdev->dev.of_node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Couldn't find reg in node = %s rc = %d\n",
				child->full_name, rc);
			return rc;
		}

		rc = qpnp_labibb_read(labibb, base + REG_REVISION_2,
					 &revision, 1);
		if (rc < 0) {
			pr_err("Reading REVISION_2 failed rc=%d\n", rc);
			goto fail_registration;
		}

		rc = qpnp_labibb_read(labibb, base + REG_PERPH_TYPE,
					&type, 1);
		if (rc < 0) {
			pr_err("Peripheral type read failed rc=%d\n", rc);
			goto fail_registration;
		}

		switch (type) {
		case QPNP_LAB_TYPE:
			labibb->lab_base = base;
			labibb->lab_dig_major = revision;
			rc = qpnp_lab_register_irq(child, labibb);
			if (rc) {
				pr_err("Failed to register LAB IRQ rc=%d\n",
							rc);
				goto fail_registration;
			}
			rc = register_qpnp_lab_regulator(labibb, child);
			if (rc < 0)
				goto fail_registration;
		break;

		case QPNP_IBB_TYPE:
			labibb->ibb_base = base;
			labibb->ibb_dig_major = revision;
			qpnp_ibb_register_irq(child, labibb);
			rc = register_qpnp_ibb_regulator(labibb, child);
			if (rc < 0)
				goto fail_registration;
		break;

		default:
			pr_err("qpnp_labibb: unknown peripheral type %x\n",
				type);
			rc = -EINVAL;
			goto fail_registration;
		}
	}

	if (labibb->ttw_en) {
		rc = qpnp_labibb_check_ttw_supported(labibb);
		if (rc < 0) {
			pr_err("pmic revision check failed for TTW rc=%d\n",
									rc);
			goto fail_registration;
		}
	}

	INIT_WORK(&labibb->lab_vreg_ok_work, qpnp_lab_vreg_notifier_work);
	INIT_DELAYED_WORK(&labibb->sc_err_recovery_work,
			labibb_sc_err_recovery_work);
	hrtimer_init(&labibb->sc_err_check_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	labibb->sc_err_check_timer.function = labibb_check_sc_err_count;
	dev_set_drvdata(&pdev->dev, labibb);

	labibb->labibb_class.name = "lcd_bias";
	labibb->labibb_class.owner = THIS_MODULE;
	labibb->labibb_class.class_attrs = labibb_attributes;

	rc = class_register(&labibb->labibb_class);
	if (rc < 0) {
		pr_err("Failed to register labibb class rc=%d\n", rc);
		return rc;
	}

	pr_info("LAB/IBB registered successfully, lab_vreg enable=%d ibb_vreg enable=%d swire_control=%d\n",
						labibb->lab_vreg.vreg_enabled,
						labibb->ibb_vreg.vreg_enabled,
						labibb->swire_control);

	return 0;

fail_registration:
	if (labibb->lab_vreg.rdev)
		regulator_unregister(labibb->lab_vreg.rdev);
	if (labibb->ibb_vreg.rdev)
		regulator_unregister(labibb->ibb_vreg.rdev);

	return rc;
}

int qpnp_labibb_notifier_register(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&labibb_notifier, nb);
}
EXPORT_SYMBOL(qpnp_labibb_notifier_register);

int qpnp_labibb_notifier_unregister(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&labibb_notifier, nb);
}
EXPORT_SYMBOL(qpnp_labibb_notifier_unregister);

static int qpnp_labibb_regulator_remove(struct platform_device *pdev)
{
	struct qpnp_labibb *labibb = dev_get_drvdata(&pdev->dev);

	if (labibb) {
		if (labibb->lab_vreg.rdev)
			regulator_unregister(labibb->lab_vreg.rdev);
		if (labibb->ibb_vreg.rdev)
			regulator_unregister(labibb->ibb_vreg.rdev);

		cancel_work_sync(&labibb->lab_vreg_ok_work);
	}
	return 0;
}

static const struct of_device_id spmi_match_table[] = {
	{ .compatible = QPNP_LABIBB_REGULATOR_DRIVER_NAME, },
	{ },
};

static struct platform_driver qpnp_labibb_regulator_driver = {
	.driver		= {
		.name		= QPNP_LABIBB_REGULATOR_DRIVER_NAME,
		.of_match_table	= spmi_match_table,
	},
	.probe		= qpnp_labibb_regulator_probe,
	.remove		= qpnp_labibb_regulator_remove,
};

static int __init qpnp_labibb_regulator_init(void)
{
	return platform_driver_register(&qpnp_labibb_regulator_driver);
}
arch_initcall(qpnp_labibb_regulator_init);

static void __exit qpnp_labibb_regulator_exit(void)
{
	platform_driver_unregister(&qpnp_labibb_regulator_driver);
}
module_exit(qpnp_labibb_regulator_exit);

MODULE_DESCRIPTION("QPNP labibb driver");
MODULE_LICENSE("GPL v2");
