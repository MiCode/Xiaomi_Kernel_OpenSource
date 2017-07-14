/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"FG: %s: " fmt, __func__

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/rtc.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/spmi.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/power_supply.h>
#include <linux/of_batterydata.h>
#include <linux/spinlock.h>
#include <linux/string_helpers.h>
#include <linux/alarmtimer.h>
#include <linux/qpnp/qpnp-revid.h>

/* Register offsets */

/* Interrupt offsets */
#define INT_RT_STS(base)			(base + 0x10)
#define INT_EN_CLR(base)			(base + 0x16)

/* SPMI Register offsets */
#define SOC_MONOTONIC_SOC	0x09
#define SOC_BOOT_MOD		0x50
#define SOC_RESTART		0x51

#define REG_OFFSET_PERP_SUBTYPE	0x05

/* RAM register offsets */
#define RAM_OFFSET		0x400

/* Bit/Mask definitions */
#define FULL_PERCENT		0xFF
#define MAX_TRIES_SOC		5
#define MA_MV_BIT_RES		39
#define MSB_SIGN		BIT(7)
#define IBAT_VBAT_MASK		0x7F
#define NO_OTP_PROF_RELOAD	BIT(6)
#define REDO_FIRST_ESTIMATE	BIT(3)
#define RESTART_GO		BIT(0)
#define THERM_DELAY_MASK	0xE0

/* SUBTYPE definitions */
#define FG_SOC			0x9
#define FG_BATT			0xA
#define FG_ADC			0xB
#define FG_MEMIF		0xC

#define QPNP_FG_DEV_NAME "qcom,qpnp-fg"
#define MEM_IF_TIMEOUT_MS	5000
#define FG_CYCLE_MS		1500
#define BUCKET_COUNT		8
#define BUCKET_SOC_PCT		(256 / BUCKET_COUNT)

#define BCL_MA_TO_ADC(_current, _adc_val) {		\
	_adc_val = (u8)((_current) * 100 / 976);	\
}

/* Debug Flag Definitions */
enum {
	FG_SPMI_DEBUG_WRITES		= BIT(0), /* Show SPMI writes */
	FG_SPMI_DEBUG_READS		= BIT(1), /* Show SPMI reads */
	FG_IRQS				= BIT(2), /* Show interrupts */
	FG_MEM_DEBUG_WRITES		= BIT(3), /* Show SRAM writes */
	FG_MEM_DEBUG_READS		= BIT(4), /* Show SRAM reads */
	FG_POWER_SUPPLY			= BIT(5), /* Show POWER_SUPPLY */
	FG_STATUS			= BIT(6), /* Show FG status changes */
	FG_AGING			= BIT(7), /* Show FG aging algorithm */
};

/* PMIC REVISIONS */
#define REVID_RESERVED			0
#define REVID_VARIANT			1
#define REVID_ANA_MAJOR			2
#define REVID_DIG_MAJOR			3

enum dig_major {
	DIG_REV_1 = 0x1,
	DIG_REV_2 = 0x2,
	DIG_REV_3 = 0x3,
};

enum pmic_subtype {
	PMI8994		= 10,
	PMI8950		= 17,
	PMI8996		= 19,
	PMI8937		= 55,
	PMI8940		= 64,
};

enum wa_flags {
	IADC_GAIN_COMP_WA = BIT(0),
	USE_CC_SOC_REG = BIT(1),
	PULSE_REQUEST_WA = BIT(2),
	BCL_HI_POWER_FOR_CHGLED_WA = BIT(3)
};

enum current_sense_type {
	INTERNAL_CURRENT_SENSE,
	EXTERNAL_CURRENT_SENSE,
};

struct fg_mem_setting {
	u16	address;
	u8	offset;
	int	value;
};

struct fg_mem_data {
	u16	address;
	u8	offset;
	unsigned int len;
	int	value;
};

struct fg_learning_data {
	int64_t			cc_uah;
	int64_t			learned_cc_uah;
	int			init_cc_pc_val;
	bool			active;
	bool			feedback_on;
	struct mutex		learning_lock;
	ktime_t			time_stamp;
	/* configuration properties */
	int			max_start_soc;
	int			max_increment;
	int			max_decrement;
	int			min_temp;
	int			max_temp;
	int			vbat_est_thr_uv;
	int			max_cap_limit;
	int			min_cap_limit;
};

struct fg_rslow_data {
	u8			rslow_cfg;
	u8			rslow_thr;
	u8			rs_to_rslow[2];
	u8			rslow_comp[4];
	uint32_t		chg_rs_to_rslow;
	uint32_t		chg_rslow_comp_c1;
	uint32_t		chg_rslow_comp_c2;
	uint32_t		chg_rslow_comp_thr;
	bool			active;
	struct mutex		lock;
};

struct fg_cyc_ctr_data {
	bool			en;
	bool			started[BUCKET_COUNT];
	u16			count[BUCKET_COUNT];
	u8			last_soc[BUCKET_COUNT];
	int			id;
	struct mutex		lock;
};

struct fg_iadc_comp_data {
	u8			dfl_gain_reg[2];
	bool			gain_active;
	int64_t			dfl_gain;
};

struct fg_cc_soc_data {
	int	init_sys_soc;
	int	init_cc_soc;
	int	full_capacity;
	int	delta_soc;
};

/* FG_MEMIF setting index */
enum fg_mem_setting_index {
	FG_MEM_SOFT_COLD = 0,
	FG_MEM_SOFT_HOT,
	FG_MEM_HARD_COLD,
	FG_MEM_HARD_HOT,
	FG_MEM_RESUME_SOC,
	FG_MEM_BCL_LM_THRESHOLD,
	FG_MEM_BCL_MH_THRESHOLD,
	FG_MEM_TERM_CURRENT,
	FG_MEM_CHG_TERM_CURRENT,
	FG_MEM_IRQ_VOLT_EMPTY,
	FG_MEM_CUTOFF_VOLTAGE,
	FG_MEM_VBAT_EST_DIFF,
	FG_MEM_DELTA_SOC,
	FG_MEM_BATT_LOW,
	FG_MEM_THERM_DELAY,
	FG_MEM_SETTING_MAX,
};

/* FG_MEMIF data index */
enum fg_mem_data_index {
	FG_DATA_BATT_TEMP = 0,
	FG_DATA_OCV,
	FG_DATA_VOLTAGE,
	FG_DATA_CURRENT,
	FG_DATA_BATT_ESR,
	FG_DATA_BATT_ESR_COUNT,
	FG_DATA_BATT_SOC,
	FG_DATA_CC_CHARGE,
	FG_DATA_VINT_ERR,
	FG_DATA_CPRED_VOLTAGE,
	/* values below this only gets read once per profile reload */
	FG_DATA_BATT_ID,
	FG_DATA_BATT_ID_INFO,
	FG_DATA_MAX,
};

#define SETTING(_idx, _address, _offset, _value)	\
	[FG_MEM_##_idx] = {				\
		.address = _address,			\
		.offset = _offset,			\
		.value = _value,			\
	}						\

static struct fg_mem_setting settings[FG_MEM_SETTING_MAX] = {
	/*       ID                    Address, Offset, Value*/
	SETTING(SOFT_COLD,       0x454,   0,      100),
	SETTING(SOFT_HOT,        0x454,   1,      400),
	SETTING(HARD_COLD,       0x454,   2,      50),
	SETTING(HARD_HOT,        0x454,   3,      450),
	SETTING(RESUME_SOC,      0x45C,   1,      0),
	SETTING(BCL_LM_THRESHOLD, 0x47C,   2,      50),
	SETTING(BCL_MH_THRESHOLD, 0x47C,   3,      752),
	SETTING(TERM_CURRENT,	 0x40C,   2,      250),
	SETTING(CHG_TERM_CURRENT, 0x4F8,   2,      250),
	SETTING(IRQ_VOLT_EMPTY,	 0x458,   3,      3100),
	SETTING(CUTOFF_VOLTAGE,	 0x40C,   0,      3200),
	SETTING(VBAT_EST_DIFF,	 0x000,   0,      30),
	SETTING(DELTA_SOC,	 0x450,   3,      1),
	SETTING(BATT_LOW,	 0x458,   0,      4200),
	SETTING(THERM_DELAY,	 0x4AC,   3,      0),
};

#define DATA(_idx, _address, _offset, _length,  _value)	\
	[FG_DATA_##_idx] = {				\
		.address = _address,			\
		.offset = _offset,			\
		.len = _length,			\
		.value = _value,			\
	}						\

static struct fg_mem_data fg_data[FG_DATA_MAX] = {
	/*       ID           Address, Offset, Length, Value*/
	DATA(BATT_TEMP,       0x550,   2,      2,     -EINVAL),
	DATA(OCV,             0x588,   3,      2,     -EINVAL),
	DATA(VOLTAGE,         0x5CC,   1,      2,     -EINVAL),
	DATA(CURRENT,         0x5CC,   3,      2,     -EINVAL),
	DATA(BATT_ESR,        0x554,   2,      2,     -EINVAL),
	DATA(BATT_ESR_COUNT,  0x558,   2,      2,     -EINVAL),
	DATA(BATT_SOC,        0x56C,   1,      3,     -EINVAL),
	DATA(CC_CHARGE,       0x570,   0,      4,     -EINVAL),
	DATA(VINT_ERR,        0x560,   0,      4,     -EINVAL),
	DATA(CPRED_VOLTAGE,   0x540,   0,      2,     -EINVAL),
	DATA(BATT_ID,         0x594,   1,      1,     -EINVAL),
	DATA(BATT_ID_INFO,    0x594,   3,      1,     -EINVAL),
};

enum fg_mem_backup_index {
	FG_BACKUP_SOC = 0,
	FG_BACKUP_CYCLE_COUNT,
	FG_BACKUP_CC_SOC_COEFF,
	FG_BACKUP_IGAIN,
	FG_BACKUP_VCOR,
	FG_BACKUP_TEMP_COUNTER,
	FG_BACKUP_AGING_STORAGE,
	FG_BACKUP_MAH_TO_SOC,
	FG_BACKUP_MAX,
};

#define BACKUP(_idx, _address, _offset, _length,  _value)	\
	[FG_BACKUP_##_idx] = {				\
		.address = _address,			\
		.offset = _offset,			\
		.len = _length,			\
		.value = _value,			\
	}						\

static struct fg_mem_data fg_backup_regs[FG_BACKUP_MAX] = {
	/*       ID           Address, Offset, Length, Value*/
	BACKUP(SOC,		0x564,   0,      24,     -EINVAL),
	BACKUP(CYCLE_COUNT,	0x5E8,   0,      16,     -EINVAL),
	BACKUP(CC_SOC_COEFF,	0x5BC,   0,      8,     -EINVAL),
	BACKUP(IGAIN,		0x424,   0,      4,     -EINVAL),
	BACKUP(VCOR,		0x484,   0,      4,     -EINVAL),
	BACKUP(TEMP_COUNTER,	0x580,   0,      4,     -EINVAL),
	BACKUP(AGING_STORAGE,	0x5E4,   0,      4,     -EINVAL),
	BACKUP(MAH_TO_SOC,	0x4A0,   0,      4,     -EINVAL),
};

static int fg_debug_mask;
module_param_named(
	debug_mask, fg_debug_mask, int, S_IRUSR | S_IWUSR
);

static int fg_reset_on_lockup;

static int fg_sense_type = -EINVAL;
static int fg_restart;

static int fg_est_dump;
module_param_named(
	first_est_dump, fg_est_dump, int, S_IRUSR | S_IWUSR
);

static char *fg_batt_type;
module_param_named(
	battery_type, fg_batt_type, charp, S_IRUSR | S_IWUSR
);

static int fg_sram_update_period_ms = 30000;
module_param_named(
	sram_update_period_ms, fg_sram_update_period_ms, int, S_IRUSR | S_IWUSR
);

static bool fg_batt_valid_ocv;
module_param_named(batt_valid_ocv, fg_batt_valid_ocv, bool, S_IRUSR | S_IWUSR);

static int fg_batt_range_pct;
module_param_named(batt_range_pct, fg_batt_range_pct, int, S_IRUSR | S_IWUSR);

struct fg_irq {
	int			irq;
	bool			disabled;
	bool			wakeup;
};

enum fg_soc_irq {
	HIGH_SOC,
	LOW_SOC,
	FULL_SOC,
	EMPTY_SOC,
	DELTA_SOC,
	FIRST_EST_DONE,
	SW_FALLBK_OCV,
	SW_FALLBK_NEW_BATT,
	FG_SOC_IRQ_COUNT,
};

enum fg_batt_irq {
	JEITA_SOFT_COLD,
	JEITA_SOFT_HOT,
	VBATT_LOW,
	BATT_IDENTIFIED,
	BATT_ID_REQ,
	BATTERY_UNKNOWN,
	BATT_MISSING,
	BATT_MATCH,
	FG_BATT_IRQ_COUNT,
};

enum fg_mem_if_irq {
	FG_MEM_AVAIL,
	TA_RCVRY_SUG,
	FG_MEM_IF_IRQ_COUNT,
};

enum fg_batt_aging_mode {
	FG_AGING_NONE,
	FG_AGING_ESR,
	FG_AGING_CC,
};

enum register_type {
	MEM_INTF_CFG,
	MEM_INTF_CTL,
	MEM_INTF_ADDR_LSB,
	MEM_INTF_RD_DATA0,
	MEM_INTF_WR_DATA0,
	MAX_ADDRESS,
};

enum batt_info_params {
	BATT_INFO_NOTIFY = 0,
	BATT_INFO_SOC,
	BATT_INFO_RES_ID,
	BATT_INFO_VOLTAGE,
	BATT_INFO_TEMP,
	BATT_INFO_FCC,
	BATT_INFO_MAX,
};

struct register_offset {
	u16 address[MAX_ADDRESS];
};

static struct register_offset offset[] = {
	[0] = {
			 /* CFG   CTL   LSB   RD0   WD0 */
		.address = {0x40, 0x41, 0x42, 0x4C, 0x48},
	},
	[1] = {
			 /* CFG   CTL   LSB   RD0   WD0 */
		.address = {0x50, 0x51, 0x61, 0x67, 0x63},
	},
};

#define MEM_INTF_CFG(chip)	\
		((chip)->mem_base + (chip)->offset[MEM_INTF_CFG])
#define MEM_INTF_CTL(chip)	\
		((chip)->mem_base + (chip)->offset[MEM_INTF_CTL])
#define MEM_INTF_ADDR_LSB(chip) \
		((chip)->mem_base + (chip)->offset[MEM_INTF_ADDR_LSB])
#define MEM_INTF_RD_DATA0(chip) \
		((chip)->mem_base + (chip)->offset[MEM_INTF_RD_DATA0])
#define MEM_INTF_WR_DATA0(chip) \
		((chip)->mem_base + (chip)->offset[MEM_INTF_WR_DATA0])

struct fg_wakeup_source {
	struct wakeup_source	source;
	unsigned long		enabled;
};

static void fg_stay_awake(struct fg_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->enabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void fg_relax(struct fg_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->enabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

enum slope_limit_status {
	LOW_TEMP_CHARGE,
	HIGH_TEMP_CHARGE,
	LOW_TEMP_DISCHARGE,
	HIGH_TEMP_DISCHARGE,
	SLOPE_LIMIT_MAX,
};

#define VOLT_GAIN_MAX		3
struct dischg_gain_soc {
	bool			enable;
	u32			soc[VOLT_GAIN_MAX];
	u32			medc_gain[VOLT_GAIN_MAX];
	u32			highc_gain[VOLT_GAIN_MAX];
};

#define THERMAL_COEFF_N_BYTES		6
struct fg_chip {
	struct device		*dev;
	struct spmi_device	*spmi;
	u8			pmic_subtype;
	u8			pmic_revision[4];
	u8			revision[4];
	u16			soc_base;
	u16			batt_base;
	u16			mem_base;
	u16			vbat_adc_addr;
	u16			ibat_adc_addr;
	u16			tp_rev_addr;
	u32			wa_flag;
	atomic_t		memif_user_cnt;
	struct fg_irq		soc_irq[FG_SOC_IRQ_COUNT];
	struct fg_irq		batt_irq[FG_BATT_IRQ_COUNT];
	struct fg_irq		mem_irq[FG_MEM_IF_IRQ_COUNT];
	struct completion	sram_access_granted;
	struct completion	sram_access_revoked;
	struct completion	batt_id_avail;
	struct completion	first_soc_done;
	struct power_supply	bms_psy;
	spinlock_t		sec_access_lock;
	struct mutex		rw_lock;
	struct mutex		sysfs_restart_lock;
	struct delayed_work	batt_profile_init;
	struct work_struct	dump_sram;
	struct work_struct	status_change_work;
	struct work_struct	cycle_count_work;
	struct work_struct	battery_age_work;
	struct work_struct	update_esr_work;
	struct work_struct	set_resume_soc_work;
	struct work_struct	rslow_comp_work;
	struct work_struct	sysfs_restart_work;
	struct work_struct	init_work;
	struct work_struct	charge_full_work;
	struct work_struct	gain_comp_work;
	struct work_struct	bcl_hi_power_work;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct fg_wakeup_source	memif_wakeup_source;
	struct fg_wakeup_source	profile_wakeup_source;
	struct fg_wakeup_source	empty_check_wakeup_source;
	struct fg_wakeup_source	resume_soc_wakeup_source;
	struct fg_wakeup_source	gain_comp_wakeup_source;
	struct fg_wakeup_source	capacity_learning_wakeup_source;
	bool			first_profile_loaded;
	struct fg_wakeup_source	update_temp_wakeup_source;
	struct fg_wakeup_source	update_sram_wakeup_source;
	bool			fg_restarting;
	bool			profile_loaded;
	bool			soc_reporting_ready;
	bool			use_otp_profile;
	bool			battery_missing;
	bool			power_supply_registered;
	bool			sw_rbias_ctrl;
	bool			use_thermal_coefficients;
	bool			esr_strict_filter;
	bool			soc_empty;
	bool			charge_done;
	bool			resume_soc_lowered;
	bool			vbat_low_irq_enabled;
	bool			full_soc_irq_enabled;
	bool			charge_full;
	bool			hold_soc_while_full;
	bool			input_present;
	bool			otg_present;
	bool			safety_timer_expired;
	bool			bad_batt_detection_en;
	bool			bcl_lpm_disabled;
	bool			charging_disabled;
	bool			use_vbat_low_empty_soc;
	bool			fg_shutdown;
	bool			use_soft_jeita_irq;
	bool			allow_false_negative_isense;
	struct delayed_work	update_jeita_setting;
	struct delayed_work	update_sram_data;
	struct delayed_work	update_temp_work;
	struct delayed_work	check_empty_work;
	char			*batt_profile;
	u8			thermal_coefficients[THERMAL_COEFF_N_BYTES];
	u32			cc_cv_threshold_mv;
	unsigned int		batt_profile_len;
	unsigned int		batt_max_voltage_uv;
	const char		*batt_type;
	const char		*batt_psy_name;
	unsigned long		last_sram_update_time;
	unsigned long		last_temp_update_time;
	int64_t			ocv_coeffs[12];
	int64_t			cutoff_voltage;
	int			evaluation_current;
	int			ocv_junction_p1p2;
	int			ocv_junction_p2p3;
	int			nom_cap_uah;
	int			actual_cap_uah;
	int			status;
	int			prev_status;
	int			health;
	enum fg_batt_aging_mode	batt_aging_mode;
	struct alarm		hard_jeita_alarm;
	/* capacity learning */
	struct fg_learning_data	learning_data;
	struct alarm		fg_cap_learning_alarm;
	struct work_struct	fg_cap_learning_work;
	struct fg_cc_soc_data	sw_cc_soc_data;
	/* rslow compensation */
	struct fg_rslow_data	rslow_comp;
	int			rconn_mohm;
	/* cycle counter */
	struct fg_cyc_ctr_data	cyc_ctr;
	/* iadc compensation */
	struct fg_iadc_comp_data iadc_comp_data;
	/* interleaved memory access */
	u16			*offset;
	bool			ima_supported;
	bool			init_done;
	/* jeita hysteresis */
	bool			jeita_hysteresis_support;
	bool			batt_hot;
	bool			batt_cold;
	bool			batt_warm;
	bool			batt_cool;
	int			cold_hysteresis;
	int			hot_hysteresis;
	/* ESR pulse tuning */
	struct fg_wakeup_source	esr_extract_wakeup_source;
	struct work_struct	esr_extract_config_work;
	bool			esr_extract_disabled;
	bool			imptr_pulse_slow_en;
	bool			esr_pulse_tune_en;
	/* Slope limiter */
	struct work_struct	slope_limiter_work;
	struct fg_wakeup_source	slope_limit_wakeup_source;
	bool			soc_slope_limiter_en;
	enum slope_limit_status	slope_limit_sts;
	u32			slope_limit_temp;
	u32			slope_limit_coeffs[SLOPE_LIMIT_MAX];
	/* Discharge soc gain */
	struct work_struct	dischg_gain_work;
	struct fg_wakeup_source	dischg_gain_wakeup_source;
	struct dischg_gain_soc	dischg_gain;
	/* IMA error recovery */
	struct completion	fg_reset_done;
	struct work_struct	ima_error_recovery_work;
	struct fg_wakeup_source	fg_reset_wakeup_source;
	struct mutex		ima_recovery_lock;
	bool			ima_error_handling;
	bool			block_sram_access;
	bool			irqs_enabled;
	bool			use_last_soc;
	int			last_soc;
	/* Validating temperature */
	int			last_good_temp;
	int			batt_temp_low_limit;
	int			batt_temp_high_limit;
	/* Validating CC_SOC */
	struct work_struct	cc_soc_store_work;
	struct fg_wakeup_source	cc_soc_wakeup_source;
	int			cc_soc_limit_pct;
	bool			use_last_cc_soc;
	int64_t			last_cc_soc;
	/* Sanity check */
	struct delayed_work	check_sanity_work;
	struct fg_wakeup_source	sanity_wakeup_source;
	u8			last_beat_count;
	/* Batt_info restore */
	int			batt_info[BATT_INFO_MAX];
	int			batt_info_id;
	bool			batt_info_restore;
	bool			*batt_range_ocv;
	int			*batt_range_pct;
};

/* FG_MEMIF DEBUGFS structures */
#define ADDR_LEN	4	/* 3 byte address + 1 space character */
#define CHARS_PER_ITEM	3	/* Format is 'XX ' */
#define ITEMS_PER_LINE	4	/* 4 data items per line */
#define MAX_LINE_LENGTH  (ADDR_LEN + (ITEMS_PER_LINE * CHARS_PER_ITEM) + 1)
#define MAX_REG_PER_TRANSACTION	(8)

static const char *DFS_ROOT_NAME	= "fg_memif";
static const mode_t DFS_MODE = S_IRUSR | S_IWUSR;
static const char *default_batt_type	= "Unknown Battery";
static const char *loading_batt_type	= "Loading Battery Data";
static const char *missing_batt_type	= "Disconnected Battery";

/* Log buffer */
struct fg_log_buffer {
	size_t rpos;	/* Current 'read' position in buffer */
	size_t wpos;	/* Current 'write' position in buffer */
	size_t len;	/* Length of the buffer */
	char data[0];	/* Log buffer */
};

/* transaction parameters */
struct fg_trans {
	u32 cnt;	/* Number of bytes to read */
	u16 addr;	/* 12-bit address in SRAM */
	u32 offset;	/* Offset of last read data + byte offset */
	struct fg_chip *chip;
	struct fg_log_buffer *log; /* log buffer */
	u8 *data;	/* fg data that is read */
	struct mutex memif_dfs_lock; /* Prevent thread concurrency */
};

struct fg_dbgfs {
	u32 cnt;
	u32 addr;
	struct fg_chip *chip;
	struct dentry *root;
	struct mutex  lock;
	struct debugfs_blob_wrapper help_msg;
};

static struct fg_dbgfs dbgfs_data = {
	.lock = __MUTEX_INITIALIZER(dbgfs_data.lock),
	.help_msg = {
	.data =
"FG Debug-FS support\n"
"\n"
"Hierarchy schema:\n"
"/sys/kernel/debug/fg_memif\n"
"       /help            -- Static help text\n"
"       /address  -- Starting register address for reads or writes\n"
"       /count    -- Number of registers to read (only used for reads)\n"
"       /data     -- Initiates the SRAM read (formatted output)\n"
"\n",
	},
};

static const struct of_device_id fg_match_table[] = {
	{	.compatible = QPNP_FG_DEV_NAME, },
	{}
};

static char *fg_supplicants[] = {
	"battery",
	"bcl",
	"fg_adc"
};

#define DEBUG_PRINT_BUFFER_SIZE 64
static void fill_string(char *str, size_t str_len, u8 *buf, int buf_len)
{
	int pos = 0;
	int i;

	for (i = 0; i < buf_len; i++) {
		pos += scnprintf(str + pos, str_len - pos, "%02X", buf[i]);
		if (i < buf_len - 1)
			pos += scnprintf(str + pos, str_len - pos, " ");
	}
}

static int fg_write(struct fg_chip *chip, u8 *val, u16 addr, int len)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	if ((addr & 0xff00) == 0) {
		pr_err("addr cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, val, len);
	if (rc) {
		pr_err("write failed addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return rc;
	}

	if (!rc && (fg_debug_mask & FG_SPMI_DEBUG_WRITES)) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, val, len);
		pr_info("write(0x%04X), sid=%d, len=%d; %s\n",
			addr, spmi->sid, len, str);
	}

	return rc;
}

static int fg_read(struct fg_chip *chip, u8 *val, u16 addr, int len)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	if ((addr & 0xff00) == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, len);
	if (rc) {
		pr_err("SPMI read failed base=0x%02x sid=0x%02x rc=%d\n", addr,
				spmi->sid, rc);
		return rc;
	}

	if (!rc && (fg_debug_mask & FG_SPMI_DEBUG_READS)) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, val, len);
		pr_info("read(0x%04x), sid=%d, len=%d; %s\n",
			addr, spmi->sid, len, str);
	}

	return rc;
}

static int fg_masked_write_raw(struct fg_chip *chip, u16 addr,
		u8 mask, u8 val, int len)
{
	int rc;
	u8 reg;

	rc = fg_read(chip, &reg, addr, len);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}
	pr_debug("addr = 0x%x read 0x%x\n", addr, reg);

	reg &= ~mask;
	reg |= val & mask;

	pr_debug("Writing 0x%x\n", reg);

	rc = fg_write(chip, &reg, addr, len);
	if (rc)
		pr_err("spmi write failed: addr=%03X, rc=%d\n", addr, rc);

	return rc;
}

static int fg_masked_write(struct fg_chip *chip, u16 addr,
		u8 mask, u8 val, int len)
{
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&chip->sec_access_lock, flags);
	rc = fg_masked_write_raw(chip, addr, mask, val, len);
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);

	return rc;
}

#define SEC_ACCESS_OFFSET	0xD0
#define SEC_ACCESS_VALUE	0xA5
#define PERIPHERAL_MASK		0xFF
static int fg_sec_masked_write(struct fg_chip *chip, u16 addr, u8 mask, u8 val,
		int len)
{
	int rc;
	unsigned long flags;
	u8 temp;
	u16 base = addr & (~PERIPHERAL_MASK);

	spin_lock_irqsave(&chip->sec_access_lock, flags);
	temp = SEC_ACCESS_VALUE;
	rc = fg_write(chip, &temp, base + SEC_ACCESS_OFFSET, 1);
	if (rc) {
		pr_err("Unable to unlock sec_access: %d\n", rc);
		goto out;
	}

	rc = fg_masked_write_raw(chip, addr, mask, val, len);
	if (rc)
		pr_err("Unable to write securely to address 0x%x: %d", addr,
			rc);
out:
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);
	return rc;
}

#define RIF_MEM_ACCESS_REQ	BIT(7)
static int fg_check_rif_mem_access(struct fg_chip *chip, bool *status)
{
	int rc;
	u8 mem_if_sts;

	rc = fg_read(chip, &mem_if_sts, MEM_INTF_CFG(chip), 1);
	if (rc) {
		pr_err("failed to read rif_mem status rc=%d\n", rc);
		return rc;
	}

	*status = mem_if_sts & RIF_MEM_ACCESS_REQ;
	return 0;
}

static bool fg_check_sram_access(struct fg_chip *chip)
{
	int rc;
	u8 mem_if_sts;
	bool rif_mem_sts = false;

	rc = fg_read(chip, &mem_if_sts, INT_RT_STS(chip->mem_base), 1);
	if (rc) {
		pr_err("failed to read mem status rc=%d\n", rc);
		return false;
	}

	if ((mem_if_sts & BIT(FG_MEM_AVAIL)) == 0)
		return false;

	rc = fg_check_rif_mem_access(chip, &rif_mem_sts);
	if (rc)
		return false;

	return rif_mem_sts;
}

static inline int fg_assert_sram_access(struct fg_chip *chip)
{
	int rc;
	u8 mem_if_sts;

	rc = fg_read(chip, &mem_if_sts, INT_RT_STS(chip->mem_base), 1);
	if (rc) {
		pr_err("failed to read mem status rc=%d\n", rc);
		return rc;
	}

	if ((mem_if_sts & BIT(FG_MEM_AVAIL)) == 0) {
		pr_err("mem_avail not high: %02x\n", mem_if_sts);
		return -EINVAL;
	}

	rc = fg_read(chip, &mem_if_sts, MEM_INTF_CFG(chip), 1);
	if (rc) {
		pr_err("failed to read mem status rc=%d\n", rc);
		return rc;
	}

	if ((mem_if_sts & RIF_MEM_ACCESS_REQ) == 0) {
		pr_err("mem_avail not high: %02x\n", mem_if_sts);
		return -EINVAL;
	}

	return 0;
}

#define INTF_CTL_BURST		BIT(7)
#define INTF_CTL_WR_EN		BIT(6)
static int fg_config_access(struct fg_chip *chip, bool write,
		bool burst)
{
	int rc;
	u8 intf_ctl = 0;

	intf_ctl = (write ? INTF_CTL_WR_EN : 0) | (burst ? INTF_CTL_BURST : 0);

	rc = fg_write(chip, &intf_ctl, MEM_INTF_CTL(chip), 1);
	if (rc) {
		pr_err("failed to set mem access bit\n");
		return -EIO;
	}

	return rc;
}

static int fg_req_and_wait_access(struct fg_chip *chip, int timeout)
{
	int rc = 0, ret = 0;
	bool tried_again = false;

	if (!fg_check_sram_access(chip)) {
		rc = fg_masked_write(chip, MEM_INTF_CFG(chip),
			RIF_MEM_ACCESS_REQ, RIF_MEM_ACCESS_REQ, 1);
		if (rc) {
			pr_err("failed to set mem access bit\n");
			return -EIO;
		}
		fg_stay_awake(&chip->memif_wakeup_source);
	}

wait:
	/* Wait for MEM_AVAIL IRQ. */
	ret = wait_for_completion_interruptible_timeout(
			&chip->sram_access_granted,
			msecs_to_jiffies(timeout));
	/* If we were interrupted wait again one more time. */
	if (ret == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		goto wait;
	} else if (ret <= 0) {
		rc = -ETIMEDOUT;
		pr_err("transaction timed out rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int fg_release_access(struct fg_chip *chip)
{
	int rc;

	rc = fg_masked_write(chip, MEM_INTF_CFG(chip),
			RIF_MEM_ACCESS_REQ, 0, 1);
	fg_relax(&chip->memif_wakeup_source);
	reinit_completion(&chip->sram_access_granted);

	return rc;
}

static void fg_release_access_if_necessary(struct fg_chip *chip)
{
	mutex_lock(&chip->rw_lock);
	if (atomic_sub_return(1, &chip->memif_user_cnt) <= 0) {
		fg_release_access(chip);
	}
	mutex_unlock(&chip->rw_lock);
}

/*
 * fg_mem_lock disallows the fuel gauge to release access until it has been
 * released.
 *
 * an equal number of calls must be made to fg_mem_release for the fuel gauge
 * driver to release the sram access.
 */
static void fg_mem_lock(struct fg_chip *chip)
{
	mutex_lock(&chip->rw_lock);
	atomic_add_return(1, &chip->memif_user_cnt);
	mutex_unlock(&chip->rw_lock);
}

static void fg_mem_release(struct fg_chip *chip)
{
	fg_release_access_if_necessary(chip);
}

static int fg_set_ram_addr(struct fg_chip *chip, u16 *address)
{
	int rc;

	rc = fg_write(chip, (u8 *) address,
		chip->mem_base + chip->offset[MEM_INTF_ADDR_LSB], 2);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n",
			chip->mem_base + chip->offset[MEM_INTF_ADDR_LSB], rc);
		return rc;
	}

	return rc;
}

#define BUF_LEN		4
static int fg_sub_mem_read(struct fg_chip *chip, u8 *val, u16 address, int len,
		int offset)
{
	int rc, total_len;
	u8 *rd_data = val;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	rc = fg_config_access(chip, 0, (len > 4));
	if (rc)
		return rc;

	rc = fg_set_ram_addr(chip, &address);
	if (rc)
		return rc;

	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("length %d addr=%02X\n", len, address);

	total_len = len;
	while (len > 0) {
		if (!offset) {
			rc = fg_read(chip, rd_data, MEM_INTF_RD_DATA0(chip),
							min(len, BUF_LEN));
		} else {
			rc = fg_read(chip, rd_data,
				MEM_INTF_RD_DATA0(chip) + offset,
				min(len, BUF_LEN - offset));

			/* manually set address to allow continous reads */
			address += BUF_LEN;

			rc = fg_set_ram_addr(chip, &address);
			if (rc)
				return rc;
		}
		if (rc) {
			pr_err("spmi read failed: addr=%03x, rc=%d\n",
				MEM_INTF_RD_DATA0(chip) + offset, rc);
			return rc;
		}
		rd_data += (BUF_LEN - offset);
		len -= (BUF_LEN - offset);
		offset = 0;
	}

	if (fg_debug_mask & FG_MEM_DEBUG_READS) {
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, val, total_len);
		pr_info("data: %s\n", str);
	}
	return rc;
}

static int fg_conventional_mem_read(struct fg_chip *chip, u8 *val, u16 address,
		int len, int offset, bool keep_access)
{
	int rc = 0, user_cnt = 0, orig_address = address;

	if (offset > 3) {
		pr_err("offset too large %d\n", offset);
		return -EINVAL;
	}

	address = ((orig_address + offset) / 4) * 4;
	offset = (orig_address + offset) % 4;

	user_cnt = atomic_add_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("user_cnt %d\n", user_cnt);
	mutex_lock(&chip->rw_lock);
	if (!fg_check_sram_access(chip)) {
		rc = fg_req_and_wait_access(chip, MEM_IF_TIMEOUT_MS);
		if (rc)
			goto out;
	}

	rc = fg_sub_mem_read(chip, val, address, len, offset);

out:
	user_cnt = atomic_sub_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("user_cnt %d\n", user_cnt);

	fg_assert_sram_access(chip);

	if (!keep_access && (user_cnt == 0) && !rc) {
		rc = fg_release_access(chip);
		if (rc) {
			pr_err("failed to set mem access bit\n");
			rc = -EIO;
		}
	}

	mutex_unlock(&chip->rw_lock);
	return rc;
}

static int fg_conventional_mem_write(struct fg_chip *chip, u8 *val, u16 address,
		int len, int offset, bool keep_access)
{
	int rc = 0, user_cnt = 0, sublen;
	bool access_configured = false;
	u8 *wr_data = val, word[4];
	u16 orig_address = address;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	if (address < RAM_OFFSET)
		return -EINVAL;

	if (offset > 3)
		return -EINVAL;

	address = ((orig_address + offset) / 4) * 4;
	offset = (orig_address + offset) % 4;

	user_cnt = atomic_add_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_WRITES)
		pr_info("user_cnt %d\n", user_cnt);
	mutex_lock(&chip->rw_lock);
	if (!fg_check_sram_access(chip)) {
		rc = fg_req_and_wait_access(chip, MEM_IF_TIMEOUT_MS);
		if (rc)
			goto out;
	}

	if (fg_debug_mask & FG_MEM_DEBUG_WRITES) {
		pr_info("length %d addr=%02X offset=%d\n",
				len, address, offset);
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, wr_data, len);
		pr_info("writing: %s\n", str);
	}

	while (len > 0) {
		if (offset != 0) {
			sublen = min(4 - offset, len);
			rc = fg_sub_mem_read(chip, word, address, 4, 0);
			if (rc)
				goto out;
			memcpy(word + offset, wr_data, sublen);
			/* configure access as burst if more to write */
			rc = fg_config_access(chip, 1, (len - sublen) > 0);
			if (rc)
				goto out;
			rc = fg_set_ram_addr(chip, &address);
			if (rc)
				goto out;
			offset = 0;
			access_configured = true;
		} else if (len >= 4) {
			if (!access_configured) {
				rc = fg_config_access(chip, 1, len > 4);
				if (rc)
					goto out;
				rc = fg_set_ram_addr(chip, &address);
				if (rc)
					goto out;
				access_configured = true;
			}
			sublen = 4;
			memcpy(word, wr_data, 4);
		} else if (len > 0 && len < 4) {
			sublen = len;
			rc = fg_sub_mem_read(chip, word, address, 4, 0);
			if (rc)
				goto out;
			memcpy(word, wr_data, sublen);
			rc = fg_config_access(chip, 1, 0);
			if (rc)
				goto out;
			rc = fg_set_ram_addr(chip, &address);
			if (rc)
				goto out;
			access_configured = true;
		} else {
			pr_err("Invalid length: %d\n", len);
			break;
		}
		rc = fg_write(chip, word, MEM_INTF_WR_DATA0(chip), 4);
		if (rc) {
			pr_err("spmi write failed: addr=%03x, rc=%d\n",
					MEM_INTF_WR_DATA0(chip), rc);
			goto out;
		}
		len -= sublen;
		wr_data += sublen;
		address += 4;
	}

out:
	user_cnt = atomic_sub_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_WRITES)
		pr_info("user_cnt %d\n", user_cnt);

	fg_assert_sram_access(chip);

	if (!keep_access && (user_cnt == 0) && !rc) {
		rc = fg_release_access(chip);
		if (rc) {
			pr_err("failed to set mem access bit\n");
			rc = -EIO;
		}
	}

	mutex_unlock(&chip->rw_lock);
	return rc;
}

#define MEM_INTF_IMA_CFG		0x52
#define MEM_INTF_IMA_OPR_STS		0x54
#define MEM_INTF_IMA_ERR_STS		0x5F
#define MEM_INTF_IMA_EXP_STS		0x55
#define MEM_INTF_IMA_HW_STS		0x56
#define MEM_INTF_IMA_BYTE_EN		0x60
#define IMA_IACS_CLR			BIT(2)
#define IMA_IACS_RDY			BIT(1)
static int fg_run_iacs_clear_sequence(struct fg_chip *chip)
{
	int rc = 0;
	u8 temp;

	if (fg_debug_mask & FG_STATUS)
		pr_info("Running IACS clear sequence\n");

	/* clear the error */
	rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_IMA_CFG,
				IMA_IACS_CLR, IMA_IACS_CLR, 1);
	if (rc) {
		pr_err("Error writing to IMA_CFG, rc=%d\n", rc);
		return rc;
	}

	temp = 0x4;
	rc = fg_write(chip, &temp, MEM_INTF_ADDR_LSB(chip) + 1, 1);
	if (rc) {
		pr_err("Error writing to MEM_INTF_ADDR_MSB, rc=%d\n", rc);
		return rc;
	}

	temp = 0x0;
	rc = fg_write(chip, &temp, MEM_INTF_WR_DATA0(chip) + 3, 1);
	if (rc) {
		pr_err("Error writing to WR_DATA3, rc=%d\n", rc);
		return rc;
	}

	rc = fg_read(chip, &temp, MEM_INTF_RD_DATA0(chip) + 3, 1);
	if (rc) {
		pr_err("Error writing to RD_DATA3, rc=%d\n", rc);
		return rc;
	}

	rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_IMA_CFG,
				IMA_IACS_CLR, 0, 1);
	if (rc) {
		pr_err("Error writing to IMA_CFG, rc=%d\n", rc);
		return rc;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("IACS clear sequence complete!\n");
	return rc;
}

#define IACS_ERR_BIT		BIT(0)
#define XCT_ERR_BIT		BIT(1)
#define DATA_RD_ERR_BIT		BIT(3)
#define DATA_WR_ERR_BIT		BIT(4)
#define ADDR_BURST_WRAP_BIT	BIT(5)
#define ADDR_RNG_ERR_BIT	BIT(6)
#define ADDR_SRC_ERR_BIT	BIT(7)
static int fg_check_ima_exception(struct fg_chip *chip, bool check_hw_sts)
{
	int rc = 0, ret = 0;
	u8 err_sts = 0, exp_sts = 0, hw_sts = 0;
	bool run_err_clr_seq = false;

	rc = fg_read(chip, &err_sts,
			chip->mem_base + MEM_INTF_IMA_ERR_STS, 1);
	if (rc) {
		pr_err("failed to read IMA_ERR_STS, rc=%d\n", rc);
		return rc;
	}

	rc = fg_read(chip, &exp_sts,
			chip->mem_base + MEM_INTF_IMA_EXP_STS, 1);
	if (rc) {
		pr_err("Error in reading IMA_EXP_STS, rc=%d\n", rc);
		return rc;
	}

	rc = fg_read(chip, &hw_sts,
			chip->mem_base + MEM_INTF_IMA_HW_STS, 1);
	if (rc) {
		pr_err("Error in reading IMA_HW_STS, rc=%d\n", rc);
		return rc;
	}

	pr_info_once("Initial ima_err_sts=%x ima_exp_sts=%x ima_hw_sts=%x\n",
		err_sts, exp_sts, hw_sts);

	if (fg_debug_mask & (FG_MEM_DEBUG_READS | FG_MEM_DEBUG_WRITES))
		pr_info("ima_err_sts=%x ima_exp_sts=%x ima_hw_sts=%x\n",
			err_sts, exp_sts, hw_sts);

	if (check_hw_sts) {
		/*
		 * Lower nibble should be equal to upper nibble before SRAM
		 * transactions begins from SW side. If they are unequal, then
		 * the error clear sequence should be run irrespective of IMA
		 * exception errors.
		 */
		if ((hw_sts & 0x0F) != hw_sts >> 4) {
			pr_err("IMA HW not in correct state, hw_sts=%x\n",
				hw_sts);
			run_err_clr_seq = true;
		}
	}

	if (exp_sts & (IACS_ERR_BIT | XCT_ERR_BIT | DATA_RD_ERR_BIT |
		DATA_WR_ERR_BIT | ADDR_BURST_WRAP_BIT | ADDR_RNG_ERR_BIT |
		ADDR_SRC_ERR_BIT)) {
		pr_err("IMA exception bit set, exp_sts=%x\n", exp_sts);
		run_err_clr_seq = true;
	}

	if (run_err_clr_seq) {
		ret = fg_run_iacs_clear_sequence(chip);
		if (!ret)
			return -EAGAIN;
		else
			pr_err("Error clearing IMA exception ret=%d\n", ret);
	}

	return rc;
}

static void fg_enable_irqs(struct fg_chip *chip, bool enable)
{
	if (!(enable ^ chip->irqs_enabled))
		return;

	if (enable) {
		enable_irq(chip->soc_irq[DELTA_SOC].irq);
		enable_irq_wake(chip->soc_irq[DELTA_SOC].irq);
		if (!chip->full_soc_irq_enabled) {
			enable_irq(chip->soc_irq[FULL_SOC].irq);
			enable_irq_wake(chip->soc_irq[FULL_SOC].irq);
			chip->full_soc_irq_enabled = true;
		}
		enable_irq(chip->batt_irq[BATT_MISSING].irq);
		if (!chip->vbat_low_irq_enabled) {
			enable_irq(chip->batt_irq[VBATT_LOW].irq);
			enable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
			chip->vbat_low_irq_enabled = true;
		}
		if (!chip->use_vbat_low_empty_soc) {
			enable_irq(chip->soc_irq[EMPTY_SOC].irq);
			enable_irq_wake(chip->soc_irq[EMPTY_SOC].irq);
		}
		chip->irqs_enabled = true;
	} else {
		disable_irq_wake(chip->soc_irq[DELTA_SOC].irq);
		disable_irq_nosync(chip->soc_irq[DELTA_SOC].irq);
		if (chip->full_soc_irq_enabled) {
			disable_irq_wake(chip->soc_irq[FULL_SOC].irq);
			disable_irq_nosync(chip->soc_irq[FULL_SOC].irq);
			chip->full_soc_irq_enabled = false;
		}
		disable_irq(chip->batt_irq[BATT_MISSING].irq);
		if (chip->vbat_low_irq_enabled) {
			disable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
			disable_irq_nosync(chip->batt_irq[VBATT_LOW].irq);
			chip->vbat_low_irq_enabled = false;
		}
		if (!chip->use_vbat_low_empty_soc) {
			disable_irq_wake(chip->soc_irq[EMPTY_SOC].irq);
			disable_irq_nosync(chip->soc_irq[EMPTY_SOC].irq);
		}
		chip->irqs_enabled = false;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("FG interrupts are %sabled\n", enable ? "en" : "dis");
}

static void fg_check_ima_error_handling(struct fg_chip *chip)
{
	if (chip->ima_error_handling) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("IMA error is handled already!\n");
		return;
	}
	mutex_lock(&chip->ima_recovery_lock);
	fg_enable_irqs(chip, false);
	chip->use_last_cc_soc = true;
	chip->ima_error_handling = true;
	if (!work_pending(&chip->ima_error_recovery_work))
		schedule_work(&chip->ima_error_recovery_work);
	mutex_unlock(&chip->ima_recovery_lock);
}

#define SOC_ALG_ST		0xCF
#define FGXCT_PRD		BIT(7)
#define ALG_ST_CHECK_COUNT	20
static int fg_check_alg_status(struct fg_chip *chip)
{
	int rc = 0, timeout = ALG_ST_CHECK_COUNT, count = 0;
	u8 ima_opr_sts, alg_sts = 0, temp = 0;

	if (!fg_reset_on_lockup)  {
		pr_info("FG lockup detection cannot be run\n");
		return 0;
	}

	rc = fg_read(chip, &alg_sts, chip->soc_base + SOC_ALG_ST, 1);
	if (rc) {
		pr_err("Error in reading SOC_ALG_ST, rc=%d\n", rc);
		return rc;
	}

	while (1) {
		rc = fg_read(chip, &ima_opr_sts,
			chip->mem_base + MEM_INTF_IMA_OPR_STS, 1);
		if (!rc && !(ima_opr_sts & FGXCT_PRD))
			break;

		if (rc) {
			pr_err("Error in reading IMA_OPR_STS, rc=%d\n",
				rc);
			break;
		}

		rc = fg_read(chip, &temp, chip->soc_base + SOC_ALG_ST,
			1);
		if (rc) {
			pr_err("Error in reading SOC_ALG_ST, rc=%d\n",
				rc);
			break;
		}

		if ((ima_opr_sts & FGXCT_PRD) && (temp == alg_sts))
			count++;

		/* Wait for ~10ms while polling ALG_ST & IMA_OPR_STS */
		usleep_range(9000, 11000);

		if (!(--timeout))
			break;
	}

	if (fg_debug_mask & (FG_MEM_DEBUG_READS | FG_MEM_DEBUG_WRITES))
		pr_info("ima_opr_sts: %x  alg_sts: %x count=%d\n", ima_opr_sts,
			alg_sts, count);

	if (count == ALG_ST_CHECK_COUNT) {
		/* If we are here, that means FG ALG is stuck */
		pr_err("ALG is stuck\n");
		fg_check_ima_error_handling(chip);
		rc = -EBUSY;
	}
	return rc;
}

static int fg_check_iacs_ready(struct fg_chip *chip)
{
	int rc = 0, timeout = 250;
	u8 ima_opr_sts = 0;

	/*
	 * Additional delay to make sure IACS ready bit is set after
	 * Read/Write operation.
	 */

	usleep_range(30, 35);
	while (1) {
		rc = fg_read(chip, &ima_opr_sts,
			chip->mem_base + MEM_INTF_IMA_OPR_STS, 1);
		if (!rc && (ima_opr_sts & IMA_IACS_RDY)) {
			break;
		} else {
			if (!(--timeout) || rc)
				break;

			/* delay for iacs_ready to be asserted */
			usleep_range(5000, 7000);
		}
	}

	if (!timeout || rc) {
		pr_err("IACS_RDY not set, ima_opr_sts: %x\n", ima_opr_sts);
		rc = fg_check_alg_status(chip);
		if (rc && rc != -EBUSY)
			pr_err("Couldn't check FG ALG status, rc=%d\n",
				rc);
		/* perform IACS_CLR sequence */
		fg_check_ima_exception(chip, false);
		return -EBUSY;
	}

	return 0;
}

#define IACS_SLCT			BIT(5)
static int __fg_interleaved_mem_write(struct fg_chip *chip, u8 *val,
				u16 address, int offset, int len)
{
	int rc = 0, i;
	u8 *word = val, byte_enable = 0, num_bytes = 0;

	if (fg_debug_mask & FG_MEM_DEBUG_WRITES)
		pr_info("length %d addr=%02X offset=%d\n",
					len, address, offset);

	while (len > 0) {
			num_bytes = (offset + len) > BUF_LEN ?
				(BUF_LEN - offset) : len;
			/* write to byte_enable */
			for (i = offset; i < (offset + num_bytes); i++)
				byte_enable |= BIT(i);

			rc = fg_write(chip, &byte_enable,
				chip->mem_base + MEM_INTF_IMA_BYTE_EN, 1);
			if (rc) {
				pr_err("Unable to write to byte_en_reg rc=%d\n",
								rc);
				return rc;
			}
			/* write data */
		rc = fg_write(chip, word, MEM_INTF_WR_DATA0(chip) + offset,
				num_bytes);
		if (rc) {
			pr_err("spmi write failed: addr=%03x, rc=%d\n",
				MEM_INTF_WR_DATA0(chip) + offset, rc);
			return rc;
		}
		/*
		 * The last-byte WR_DATA3 starts the write transaction.
		 * Write a dummy value to WR_DATA3 if it does not have
		 * valid data. This dummy data is not written to the
		 * SRAM as byte_en for WR_DATA3 is not set.
		 */
		if (!(byte_enable & BIT(3))) {
			u8 dummy_byte = 0x0;
			rc = fg_write(chip, &dummy_byte,
				MEM_INTF_WR_DATA0(chip) + 3, 1);
			if (rc) {
				pr_err("Unable to write dummy-data to WR_DATA3 rc=%d\n",
									rc);
				return rc;
			}
		}

		rc = fg_check_iacs_ready(chip);
		if (rc) {
			pr_err("IACS_RDY failed post write to address %x offset %d rc=%d\n",
				address, offset, rc);
			return rc;
		}

		/* check for error condition */
		rc = fg_check_ima_exception(chip, false);
		if (rc) {
			pr_err("IMA transaction failed rc=%d", rc);
			return rc;
		}

		word += num_bytes;
		len -= num_bytes;
		offset = byte_enable = 0;
	}

	return rc;
}

static int __fg_interleaved_mem_read(struct fg_chip *chip, u8 *val, u16 address,
						int offset, int len)
{
	int rc = 0, total_len;
	u8 *rd_data = val, num_bytes;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("length %d addr=%02X\n", len, address);

	total_len = len;
	while (len > 0) {
		num_bytes = (offset + len) > BUF_LEN ? (BUF_LEN - offset) : len;
		rc = fg_read(chip, rd_data, MEM_INTF_RD_DATA0(chip) + offset,
								num_bytes);
		if (rc) {
			pr_err("spmi read failed: addr=%03x, rc=%d\n",
				MEM_INTF_RD_DATA0(chip) + offset, rc);
			return rc;
		}

		rd_data += num_bytes;
		len -= num_bytes;
		offset = 0;

		rc = fg_check_iacs_ready(chip);
		if (rc) {
			pr_err("IACS_RDY failed post read for address %x offset %d rc=%d\n",
				address, offset, rc);
			return rc;
		}

		/* check for error condition */
		rc = fg_check_ima_exception(chip, false);
		if (rc) {
			pr_err("IMA transaction failed rc=%d", rc);
			return rc;
		}

		if (len && (len + offset) < BUF_LEN) {
			/* move to single mode */
			u8 intr_ctl = 0;

			rc = fg_write(chip, &intr_ctl, MEM_INTF_CTL(chip), 1);
			if (rc) {
				pr_err("failed to move to single mode rc=%d\n",
									rc);
				return -EIO;
			}
		}
	}

	if (fg_debug_mask & FG_MEM_DEBUG_READS) {
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, val, total_len);
		pr_info("data: %s\n", str);
	}

	return rc;
}

#define IMA_REQ_ACCESS		(IACS_SLCT | RIF_MEM_ACCESS_REQ)
static int fg_interleaved_mem_config(struct fg_chip *chip, u8 *val,
		u16 address, int len, int offset, int op)
{
	int rc = 0;
	bool rif_mem_sts = true;
	int time_count = 0;

	while (1) {
		rc = fg_check_rif_mem_access(chip, &rif_mem_sts);
		if (rc)
			return rc;

		if (!rif_mem_sts)
			break;

		if (fg_debug_mask & (FG_MEM_DEBUG_READS | FG_MEM_DEBUG_WRITES))
			pr_info("RIF_MEM_ACCESS_REQ is not clear yet for IMA_%s\n",
				op ? "write" : "read");

		/*
		 * Try this no more than 4 times. If RIF_MEM_ACCESS_REQ is not
		 * clear, then return an error instead of waiting for it again.
		 */
		if  (time_count > 4) {
			pr_err("Waited for ~16ms polling RIF_MEM_ACCESS_REQ\n");
			return -ETIMEDOUT;
		}

		/* Wait for 4ms before reading RIF_MEM_ACCESS_REQ again */
		usleep_range(4000, 4100);
		time_count++;
	}

	/* configure for IMA access */
	rc = fg_masked_write(chip, MEM_INTF_CFG(chip),
				IMA_REQ_ACCESS, IMA_REQ_ACCESS, 1);
	if (rc) {
		pr_err("failed to set mem access bit rc = %d\n", rc);
		return rc;
	}

	/* configure for the read/write single/burst mode */
	rc = fg_config_access(chip, op, (offset + len) > 4);
	if (rc) {
		pr_err("failed to set configure memory access rc = %d\n", rc);
		return rc;
	}

	rc = fg_check_iacs_ready(chip);
	if (rc) {
		pr_err("IACS_RDY failed before setting address: %x offset: %d rc=%d\n",
			address, offset, rc);
		return rc;
	}

	/* write addresses to the register */
	rc = fg_set_ram_addr(chip, &address);
	if (rc) {
		pr_err("failed to set SRAM address rc = %d\n", rc);
		return rc;
	}

	rc = fg_check_iacs_ready(chip);
	if (rc)
		pr_err("IACS_RDY failed after setting address: %x offset: %d rc=%d\n",
			address, offset, rc);

	return rc;
}

#define MEM_INTF_FG_BEAT_COUNT		0x57
#define BEAT_COUNT_MASK			0x0F
#define RETRY_COUNT			3
static int fg_interleaved_mem_read(struct fg_chip *chip, u8 *val, u16 address,
						int len, int offset)
{
	int rc = 0, ret, orig_address = address;
	u8 start_beat_count, end_beat_count, count = 0;
	bool retry = false;

	if (chip->fg_shutdown)
		return -EINVAL;

	if (offset > 3) {
		pr_err("offset too large %d\n", offset);
		return -EINVAL;
	}

	fg_stay_awake(&chip->memif_wakeup_source);
	address = ((orig_address + offset) / 4) * 4;
	offset = (orig_address + offset) % 4;

	if (address < RAM_OFFSET) {
		/*
		 * OTP memory reads need a conventional memory access, do a
		 * conventional read when SRAM offset < RAM_OFFSET.
		 */
		rc = fg_conventional_mem_read(chip, val, address, len, offset,
						0);
		if (rc)
			pr_err("Failed to read OTP memory %d\n", rc);
		goto exit;
	}

	mutex_lock(&chip->rw_lock);
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("Read for %d bytes is attempted @ 0x%x[%d]\n",
			len, address, offset);

retry:
	if (count >= RETRY_COUNT) {
		pr_err("Retried reading 3 times\n");
		retry = false;
		goto out;
	}

	rc = fg_interleaved_mem_config(chip, val, address, offset, len, 0);
	if (rc) {
		pr_err("failed to configure SRAM for IMA rc = %d\n", rc);
		retry = true;
		count++;
		goto out;
	}

	/* read the start beat count */
	rc = fg_read(chip, &start_beat_count,
			chip->mem_base + MEM_INTF_FG_BEAT_COUNT, 1);
	if (rc) {
		pr_err("failed to read beat count rc=%d\n", rc);
		retry = true;
		count++;
		goto out;
	}

	/* read data */
	rc = __fg_interleaved_mem_read(chip, val, address, offset, len);
	if (rc) {
		count++;
		if ((rc == -EAGAIN) && (count < RETRY_COUNT)) {
			pr_err("IMA access failed retry_count = %d\n", count);
			goto retry;
		} else {
			pr_err("failed to read SRAM address rc = %d\n", rc);
			retry = true;
			goto out;
		}
	}

	/* read the end beat count */
	rc = fg_read(chip, &end_beat_count,
			chip->mem_base + MEM_INTF_FG_BEAT_COUNT, 1);
	if (rc) {
		pr_err("failed to read beat count rc=%d\n", rc);
		retry = true;
		count++;
		goto out;
	}

	start_beat_count &= BEAT_COUNT_MASK;
	end_beat_count &= BEAT_COUNT_MASK;
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("Start beat_count = %x End beat_count = %x\n",
				start_beat_count, end_beat_count);
	if (start_beat_count != end_beat_count) {
		if (fg_debug_mask & FG_MEM_DEBUG_READS)
			pr_info("Beat count do not match - retry transaction\n");
		retry = true;
		count++;
	}
out:
	/* Release IMA access */
	ret = fg_masked_write(chip, MEM_INTF_CFG(chip), IMA_REQ_ACCESS, 0, 1);
	if (ret)
		pr_err("failed to reset IMA access bit ret = %d\n", ret);

	if (retry) {
		retry = false;
		goto retry;
	}

	mutex_unlock(&chip->rw_lock);
exit:
	fg_relax(&chip->memif_wakeup_source);
	return rc;
}

static int fg_interleaved_mem_write(struct fg_chip *chip, u8 *val, u16 address,
							int len, int offset)
{
	int rc = 0, ret, orig_address = address;
	u8 count = 0;
	bool retry = false;

	if (chip->fg_shutdown)
		return -EINVAL;

	if (address < RAM_OFFSET)
		return -EINVAL;

	if (offset > 3) {
		pr_err("offset too large %d\n", offset);
		return -EINVAL;
	}

	fg_stay_awake(&chip->memif_wakeup_source);
	address = ((orig_address + offset) / 4) * 4;
	offset = (orig_address + offset) % 4;

	mutex_lock(&chip->rw_lock);
	if (fg_debug_mask & FG_MEM_DEBUG_WRITES)
		pr_info("Write for %d bytes is attempted @ 0x%x[%d]\n",
			len, address, offset);

retry:
	if (count >= RETRY_COUNT) {
		pr_err("Retried writing 3 times\n");
		retry = false;
		goto out;
	}

	rc = fg_interleaved_mem_config(chip, val, address, offset, len, 1);
	if (rc) {
		pr_err("failed to configure SRAM for IMA rc = %d\n", rc);
		retry = true;
		count++;
		goto out;
	}

	/* write data */
	rc = __fg_interleaved_mem_write(chip, val, address, offset, len);
	if (rc) {
		count++;
		if ((rc == -EAGAIN) && (count < RETRY_COUNT)) {
			pr_err("IMA access failed retry_count = %d\n", count);
			goto retry;
		} else {
			pr_err("failed to write SRAM address rc = %d\n", rc);
			retry = true;
			goto out;
		}
	}

out:
	/* Release IMA access */
	ret = fg_masked_write(chip, MEM_INTF_CFG(chip), IMA_REQ_ACCESS, 0, 1);
	if (ret)
		pr_err("failed to reset IMA access bit ret = %d\n", ret);

	if (retry) {
		retry = false;
		goto retry;
	}

	mutex_unlock(&chip->rw_lock);
	fg_relax(&chip->memif_wakeup_source);
	return rc;
}

static int fg_mem_read(struct fg_chip *chip, u8 *val, u16 address,
			int len, int offset, bool keep_access)
{
	if (chip->block_sram_access)
		return -EBUSY;

	if (chip->ima_supported)
		return fg_interleaved_mem_read(chip, val, address,
						len, offset);
	else
		return fg_conventional_mem_read(chip, val, address,
					len, offset, keep_access);
}

static int fg_mem_write(struct fg_chip *chip, u8 *val, u16 address,
		int len, int offset, bool keep_access)
{
	if (chip->block_sram_access)
		return -EBUSY;

	if (chip->ima_supported)
		return fg_interleaved_mem_write(chip, val, address,
						len, offset);
	else
		return fg_conventional_mem_write(chip, val, address,
					len, offset, keep_access);
}

static int fg_mem_masked_write(struct fg_chip *chip, u16 addr,
		u8 mask, u8 val, u8 offset)
{
	int rc = 0;
	u8 reg[4];
	char str[DEBUG_PRINT_BUFFER_SIZE];

	rc = fg_mem_read(chip, reg, addr, 4, 0, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}

	reg[offset] &= ~mask;
	reg[offset] |= val & mask;

	str[0] = '\0';
	fill_string(str, DEBUG_PRINT_BUFFER_SIZE, reg, 4);
	pr_debug("Writing %s address %03x, offset %d\n", str, addr, offset);

	rc = fg_mem_write(chip, reg, addr, 4, 0, 0);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}

	return rc;
}

static u8 sram_backup_buffer[100];
static int fg_backup_sram_registers(struct fg_chip *chip, bool save)
{
	int rc, i, len, offset;
	u16 address;
	u8 *ptr;

	if (fg_debug_mask & FG_STATUS)
		pr_info("%sing SRAM registers\n", save ? "Back" : "Restor");

	ptr = sram_backup_buffer;
	for (i = 0; i < FG_BACKUP_MAX; i++) {
		address = fg_backup_regs[i].address;
		offset = fg_backup_regs[i].offset;
		len = fg_backup_regs[i].len;
		if (save)
			rc = fg_interleaved_mem_read(chip, ptr, address,
					len, offset);
		else
			rc = fg_interleaved_mem_write(chip, ptr, address,
					len, offset);
		if (rc) {
			pr_err("Error in reading %d bytes from %x[%d], rc=%d\n",
				len, address, offset, rc);
			break;
		}
		ptr += len;
	}

	return rc;
}

#define SOC_FG_RESET	0xF3
#define RESET_MASK	(BIT(7) | BIT(5))
static int fg_reset(struct fg_chip *chip, bool reset)
{
	int rc;

	rc = fg_sec_masked_write(chip, chip->soc_base + SOC_FG_RESET,
		0xFF, reset ? RESET_MASK : 0, 1);
	if (rc)
		pr_err("Error in writing to 0x%x, rc=%d\n", SOC_FG_RESET, rc);

	return rc;
}

static void fg_handle_battery_insertion(struct fg_chip *chip)
{
	reinit_completion(&chip->batt_id_avail);
	reinit_completion(&chip->fg_reset_done);
	schedule_delayed_work(&chip->batt_profile_init, 0);
	cancel_delayed_work(&chip->update_sram_data);
	schedule_delayed_work(&chip->update_sram_data, msecs_to_jiffies(0));
}


static int soc_to_setpoint(int soc)
{
	return DIV_ROUND_CLOSEST(soc * 255, 100);
}

static void batt_to_setpoint_adc(int vbatt_mv, u8 *data)
{
	int val;
	/* Battery voltage is an offset from 0 V and LSB is 1/2^15. */
	val = DIV_ROUND_CLOSEST(vbatt_mv * 32768, 5000);
	data[0] = val & 0xFF;
	data[1] = val >> 8;
	return;
}

static u8 batt_to_setpoint_8b(int vbatt_mv)
{
	int val;
	/* Battery voltage is an offset from 2.5 V and LSB is 5/2^9. */
	val = (vbatt_mv - 2500) * 512 / 1000;
	return DIV_ROUND_CLOSEST(val, 5);
}

static u8 therm_delay_to_setpoint(u32 delay_us)
{
	u8 val;

	if (delay_us < 2560)
		val = 0;
	else if (delay_us > 163840)
		val = 7;
	else
		val = ilog2(delay_us / 10) - 7;
	return val << 5;
}

static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

close_time:
	rtc_class_close(rtc);
	return rc;
}

#define BATTERY_SOC_REG		0x56C
#define BATTERY_SOC_OFFSET	1
#define FULL_PERCENT_3B		0xFFFFFF
static int get_battery_soc_raw(struct fg_chip *chip)
{
	int rc;
	u8 buffer[3];

	rc = fg_mem_read(chip, buffer, BATTERY_SOC_REG, 3, 1, 0);
	if (rc) {
		pr_err("Unable to read battery soc: %d\n", rc);
		return 0;
	}
	return (int)(buffer[2] << 16 | buffer[1] << 8 | buffer[0]);
}

#define COUNTER_IMPTR_REG	0X558
#define COUNTER_PULSE_REG	0X55C
#define SOC_FULL_REG		0x564
#define COUNTER_IMPTR_OFFSET	2
#define COUNTER_PULSE_OFFSET	0
#define SOC_FULL_OFFSET		3
#define ESR_PULSE_RECONFIG_SOC	0xFFF971
static int fg_configure_soc(struct fg_chip *chip)
{
	u32 batt_soc;
	u8 cntr[2] = {0, 0};
	int rc = 0;

	mutex_lock(&chip->rw_lock);
	atomic_add_return(1, &chip->memif_user_cnt);
	mutex_unlock(&chip->rw_lock);

	/* Read Battery SOC */
	batt_soc = get_battery_soc_raw(chip);

	if (batt_soc > ESR_PULSE_RECONFIG_SOC) {
		if (fg_debug_mask & FG_POWER_SUPPLY)
			pr_info("Configuring soc registers batt_soc: %x\n",
				batt_soc);
		batt_soc = ESR_PULSE_RECONFIG_SOC;
		rc = fg_mem_write(chip, (u8 *)&batt_soc, BATTERY_SOC_REG, 3,
				BATTERY_SOC_OFFSET, 1);
		if (rc) {
			pr_err("failed to write BATT_SOC rc=%d\n", rc);
			goto out;
		}

		rc = fg_mem_write(chip, (u8 *)&batt_soc, SOC_FULL_REG, 3,
				SOC_FULL_OFFSET, 1);
		if (rc) {
			pr_err("failed to write SOC_FULL rc=%d\n", rc);
			goto out;
		}

		rc = fg_mem_write(chip, cntr, COUNTER_IMPTR_REG, 2,
				COUNTER_IMPTR_OFFSET, 1);
		if (rc) {
			pr_err("failed to write COUNTER_IMPTR rc=%d\n", rc);
			goto out;
		}

		rc = fg_mem_write(chip, cntr, COUNTER_PULSE_REG, 2,
				COUNTER_PULSE_OFFSET, 0);
		if (rc)
			pr_err("failed to write COUNTER_IMPTR rc=%d\n", rc);
	}
out:
	fg_release_access_if_necessary(chip);
	return rc;
}

#define VBATT_LOW_STS_BIT BIT(2)
static int fg_get_vbatt_status(struct fg_chip *chip, bool *vbatt_low_sts)
{
	int rc = 0;
	u8 fg_batt_sts;

	rc = fg_read(chip, &fg_batt_sts, INT_RT_STS(chip->batt_base), 1);
	if (rc)
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->batt_base), rc);
	else
		*vbatt_low_sts = !!(fg_batt_sts & VBATT_LOW_STS_BIT);

	return rc;
}

#define SOC_EMPTY	BIT(3)
static bool fg_is_batt_empty(struct fg_chip *chip)
{
	u8 fg_soc_sts;
	int rc;
	bool vbatt_low_sts;

	if (chip->use_vbat_low_empty_soc) {
		if (fg_get_vbatt_status(chip, &vbatt_low_sts))
			return false;

		return vbatt_low_sts;
	}

	rc = fg_read(chip, &fg_soc_sts, INT_RT_STS(chip->soc_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->soc_base), rc);
		return false;
	}

	return (fg_soc_sts & SOC_EMPTY) != 0;
}

static int get_monotonic_soc_raw(struct fg_chip *chip)
{
	u8 cap[2];
	int rc, tries = 0;

	while (tries < MAX_TRIES_SOC) {
		rc = fg_read(chip, cap,
				chip->soc_base + SOC_MONOTONIC_SOC, 2);
		if (rc) {
			pr_err("spmi read failed: addr=%03x, rc=%d\n",
				chip->soc_base + SOC_MONOTONIC_SOC, rc);
			return rc;
		}

		if (cap[0] == cap[1])
			break;

		tries++;
	}

	if (tries == MAX_TRIES_SOC) {
		pr_err("shadow registers do not match\n");
		return -EINVAL;
	}

	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info_ratelimited("raw: 0x%02x\n", cap[0]);
	return cap[0];
}

#define EMPTY_CAPACITY		0
#define DEFAULT_CAPACITY	50
#define MISSING_CAPACITY	100
#define FULL_CAPACITY		100
#define FULL_SOC_RAW		0xFF
static int get_prop_capacity(struct fg_chip *chip)
{
	int msoc, rc;
	bool vbatt_low_sts;

	if (chip->use_last_soc && chip->last_soc) {
		if (chip->last_soc == FULL_SOC_RAW)
			return FULL_CAPACITY;
		return DIV_ROUND_CLOSEST((chip->last_soc - 1) *
				(FULL_CAPACITY - 2),
				FULL_SOC_RAW - 2) + 1;
	}

	if (chip->battery_missing)
		return MISSING_CAPACITY;

	if (!chip->profile_loaded && !chip->use_otp_profile)
		return DEFAULT_CAPACITY;

	if (chip->charge_full)
		return FULL_CAPACITY;

	if (chip->soc_empty) {
		if (fg_debug_mask & FG_POWER_SUPPLY)
			pr_info_ratelimited("capacity: %d, EMPTY\n",
					EMPTY_CAPACITY);
		return EMPTY_CAPACITY;
	}

	msoc = get_monotonic_soc_raw(chip);
	if (msoc == 0) {
		if (fg_reset_on_lockup && chip->use_vbat_low_empty_soc) {
			rc = fg_get_vbatt_status(chip, &vbatt_low_sts);
			if (rc) {
				pr_err("Error in reading vbatt_status, rc=%d\n",
					rc);
				return EMPTY_CAPACITY;
			}

			if (!vbatt_low_sts)
				return DIV_ROUND_CLOSEST((chip->last_soc - 1) *
						(FULL_CAPACITY - 2),
						FULL_SOC_RAW - 2) + 1;
			else
				return EMPTY_CAPACITY;
		} else {
			return EMPTY_CAPACITY;
		}
	} else if (msoc == FULL_SOC_RAW) {
		return FULL_CAPACITY;
	}

	return DIV_ROUND_CLOSEST((msoc - 1) * (FULL_CAPACITY - 2),
			FULL_SOC_RAW - 2) + 1;
}

#define HIGH_BIAS	3
#define MED_BIAS	BIT(1)
#define LOW_BIAS	BIT(0)
static u8 bias_ua[] = {
	[HIGH_BIAS] = 150,
	[MED_BIAS] = 15,
	[LOW_BIAS] = 5,
};

static int64_t get_batt_id(unsigned int battery_id_uv, u8 bid_info)
{
	u64 battery_id_ohm;

	if ((bid_info & 0x3) == 0) {
		pr_err("can't determine battery id 0x%02x\n", bid_info);
		return -EINVAL;
	}

	battery_id_ohm = div_u64(battery_id_uv, bias_ua[bid_info & 0x3]);

	return battery_id_ohm;
}

#define DEFAULT_TEMP_DEGC	250
static int get_sram_prop_now(struct fg_chip *chip, unsigned int type)
{
	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("addr 0x%02X, offset %d value %d\n",
			fg_data[type].address, fg_data[type].offset,
			fg_data[type].value);

	if (type == FG_DATA_BATT_ID)
		return get_batt_id(fg_data[type].value,
				fg_data[FG_DATA_BATT_ID_INFO].value);

	return fg_data[type].value;
}

#define MIN_TEMP_DEGC	-300
#define MAX_TEMP_DEGC	970
static int get_prop_jeita_temp(struct fg_chip *chip, unsigned int type)
{
	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("addr 0x%02X, offset %d\n", settings[type].address,
			settings[type].offset);

	return settings[type].value;
}

static int set_prop_jeita_temp(struct fg_chip *chip,
				unsigned int type, int decidegc)
{
	int rc = 0;

	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("addr 0x%02X, offset %d temp%d\n",
			settings[type].address,
			settings[type].offset, decidegc);

	settings[type].value = decidegc;

	cancel_delayed_work_sync(
		&chip->update_jeita_setting);
	schedule_delayed_work(
		&chip->update_jeita_setting, 0);

	return rc;
}

#define EXTERNAL_SENSE_SELECT		0x4AC
#define EXTERNAL_SENSE_OFFSET		0x2
#define EXTERNAL_SENSE_BIT		BIT(2)
static int set_prop_sense_type(struct fg_chip *chip, int ext_sense_type)
{
	int rc;

	rc = fg_mem_masked_write(chip, EXTERNAL_SENSE_SELECT,
			EXTERNAL_SENSE_BIT,
			ext_sense_type ? EXTERNAL_SENSE_BIT : 0,
			EXTERNAL_SENSE_OFFSET);
	if (rc) {
		pr_err("failed to write profile rc=%d\n", rc);
		return rc;
	}

	return 0;
}

#define IGNORE_FALSE_NEGATIVE_ISENSE_BIT	BIT(3)
static int set_prop_ignore_false_negative_isense(struct fg_chip *chip,
							bool ignore)
{
	int rc;

	rc = fg_mem_masked_write(chip, EXTERNAL_SENSE_SELECT,
			IGNORE_FALSE_NEGATIVE_ISENSE_BIT,
			ignore ? IGNORE_FALSE_NEGATIVE_ISENSE_BIT : 0,
			EXTERNAL_SENSE_OFFSET);
	if (rc) {
		pr_err("failed to %s isense false negative ignore rc=%d\n",
				ignore ? "enable" : "disable", rc);
		return rc;
	}

	return 0;
}

#define EXPONENT_MASK		0xF800
#define MANTISSA_MASK		0x3FF
#define SIGN			BIT(10)
#define EXPONENT_SHIFT		11
#define MICRO_UNIT		1000000ULL
static int64_t float_decode(u16 reg)
{
	int64_t final_val, exponent_val, mantissa_val;
	int exponent, mantissa, n;
	bool sign;

	exponent = (reg & EXPONENT_MASK) >> EXPONENT_SHIFT;
	mantissa = (reg & MANTISSA_MASK);
	sign = !!(reg & SIGN);

	pr_debug("exponent=%d mantissa=%d sign=%d\n", exponent, mantissa, sign);

	mantissa_val = mantissa * MICRO_UNIT;

	n = exponent - 15;
	if (n < 0)
		exponent_val = MICRO_UNIT >> -n;
	else
		exponent_val = MICRO_UNIT << n;

	n = n - 10;
	if (n < 0)
		mantissa_val >>= -n;
	else
		mantissa_val <<= n;

	final_val = exponent_val + mantissa_val;

	if (sign)
		final_val *= -1;

	return final_val;
}

#define MIN_HALFFLOAT_EXP_N		-15
#define MAX_HALFFLOAT_EXP_N		 16
static int log2_floor(int64_t uval)
{
	int n = 0;
	int64_t i = MICRO_UNIT;

	if (uval > i) {
		while (uval > i && n > MIN_HALFFLOAT_EXP_N) {
			i <<= 1;
			n += 1;
		}
		if (uval < i)
			n -= 1;
	} else if (uval < i) {
		while (uval < i && n < MAX_HALFFLOAT_EXP_N) {
			i >>= 1;
			n -= 1;
		}
	}

	return n;
}

static int64_t exp2_int(int64_t n)
{
	int p = n - 1;

	if (p > 0)
		return (2 * MICRO_UNIT) << p;
	else
		return (2 * MICRO_UNIT) >> abs(p);
}

static u16 float_encode(int64_t uval)
{
	int sign = 0, n, exp, mantissa;
	u16 half = 0;

	if (uval < 0) {
		sign = 1;
		uval = abs(uval);
	}
	n = log2_floor(uval);
	exp = n + 15;
	mantissa = div_s64(div_s64((uval - exp2_int(n)) * exp2_int(10 - n),
				MICRO_UNIT) + MICRO_UNIT / 2, MICRO_UNIT);

	half = (mantissa & MANTISSA_MASK) | ((sign << 10) & SIGN)
		| ((exp << 11) & EXPONENT_MASK);

	if (fg_debug_mask & FG_STATUS)
		pr_info("uval = %lld, m = 0x%02x, sign = 0x%02x, exp = 0x%02x, half = 0x%04x\n",
				uval, mantissa, sign, exp, half);
	return half;
}

#define BATT_IDED	BIT(3)
static int fg_is_batt_id_valid(struct fg_chip *chip)
{
	u8 fg_batt_sts;
	int rc;

	rc = fg_read(chip, &fg_batt_sts,
				 INT_RT_STS(chip->batt_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->batt_base), rc);
		return rc;
	}

	pr_debug("fg batt sts 0x%x\n", fg_batt_sts);

	return (fg_batt_sts & BATT_IDED) ? 1 : 0;
}

static int64_t twos_compliment_extend(int64_t val, int nbytes)
{
	int i;
	int64_t mask;

	mask = 0x80LL << ((nbytes - 1) * 8);
	if (val & mask) {
		for (i = 8; i > nbytes; i--) {
			mask = 0xFFLL << ((i - 1) * 8);
			val |= mask;
		}
	}

	return val;
}

#define LSB_24B_NUMRTR		596046
#define LSB_24B_DENMTR		1000000
#define LSB_16B_NUMRTR		152587
#define LSB_16B_DENMTR		1000
#define LSB_8B		9800
#define TEMP_LSB_16B	625
#define DECIKELVIN	2730
#define SRAM_PERIOD_NO_ID_UPDATE_MS	100
#define FULL_PERCENT_28BIT		0xFFFFFFF
static int update_sram_data(struct fg_chip *chip, int *resched_ms)
{
	int i, j, rc = 0;
	u8 reg[4];
	int64_t temp;
	int battid_valid = fg_is_batt_id_valid(chip);

	fg_stay_awake(&chip->update_sram_wakeup_source);
	if (chip->fg_restarting)
		goto resched;

	fg_mem_lock(chip);
	for (i = 1; i < FG_DATA_MAX; i++) {
		if (chip->profile_loaded && i >= FG_DATA_BATT_ID)
			continue;
		rc = fg_mem_read(chip, reg, fg_data[i].address,
			fg_data[i].len, fg_data[i].offset, 0);
		if (rc) {
			pr_err("Failed to update sram data\n");
			break;
		}

		temp = 0;
		for (j = 0; j < fg_data[i].len; j++)
			temp |= reg[j] << (8 * j);

		switch (i) {
		case FG_DATA_OCV:
		case FG_DATA_VOLTAGE:
		case FG_DATA_CPRED_VOLTAGE:
			fg_data[i].value = div_u64(
					(u64)(u16)temp * LSB_16B_NUMRTR,
					LSB_16B_DENMTR);
			break;
		case FG_DATA_CURRENT:
			temp = twos_compliment_extend(temp, fg_data[i].len);
			fg_data[i].value = div_s64(
					(s64)temp * LSB_16B_NUMRTR,
					LSB_16B_DENMTR);
			break;
		case FG_DATA_BATT_ESR:
			fg_data[i].value = float_decode((u16) temp);
			break;
		case FG_DATA_BATT_ESR_COUNT:
			fg_data[i].value = (u16)temp;
			break;
		case FG_DATA_BATT_ID:
			if (battid_valid)
				fg_data[i].value = reg[0] * LSB_8B;
			break;
		case FG_DATA_BATT_ID_INFO:
			if (battid_valid)
				fg_data[i].value = reg[0];
			break;
		case FG_DATA_BATT_SOC:
			fg_data[i].value = div64_s64((temp * 10000),
							FULL_PERCENT_3B);
			break;
		case FG_DATA_CC_CHARGE:
			temp = twos_compliment_extend(temp, fg_data[i].len);
			fg_data[i].value = div64_s64(
					temp * (int64_t)chip->nom_cap_uah,
					FULL_PERCENT_28BIT);
			break;
		case FG_DATA_VINT_ERR:
			temp = twos_compliment_extend(temp, fg_data[i].len);
			fg_data[i].value = div64_s64(temp * chip->nom_cap_uah,
					FULL_PERCENT_3B);
			break;
		};

		if (fg_debug_mask & FG_MEM_DEBUG_READS)
			pr_info("%d %lld %d\n", i, temp, fg_data[i].value);
	}
	fg_mem_release(chip);

	/* Backup the registers whenever no error happens during update */
	if (fg_reset_on_lockup && !chip->ima_error_handling) {
		if (!rc) {
			if (fg_debug_mask & FG_STATUS)
				pr_info("backing up SRAM registers\n");
			rc = fg_backup_sram_registers(chip, true);
			if (rc) {
				pr_err("Couldn't save sram registers\n");
				goto out;
			}
			if (!chip->use_last_soc) {
				chip->last_soc = get_monotonic_soc_raw(chip);
				chip->last_cc_soc = div64_s64(
					(int64_t)chip->last_soc *
					FULL_PERCENT_28BIT, FULL_SOC_RAW);
			}
			if (fg_debug_mask & FG_STATUS)
				pr_info("last_soc: %d last_cc_soc: %lld\n",
					chip->last_soc, chip->last_cc_soc);
		} else {
			pr_err("update_sram failed\n");
			goto out;
		}
	}

	if (!rc)
		get_current_time(&chip->last_sram_update_time);

resched:
	if (battid_valid) {
		complete_all(&chip->batt_id_avail);
		*resched_ms = fg_sram_update_period_ms;
	} else {
		*resched_ms = SRAM_PERIOD_NO_ID_UPDATE_MS;
	}
out:
	fg_relax(&chip->update_sram_wakeup_source);
	return rc;
}

#define SANITY_CHECK_PERIOD_MS	5000
static void check_sanity_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				check_sanity_work.work);
	int rc = 0;
	u8 beat_count;
	bool tried_once = false;

	fg_stay_awake(&chip->sanity_wakeup_source);

try_again:
	rc = fg_read(chip, &beat_count,
			chip->mem_base + MEM_INTF_FG_BEAT_COUNT, 1);
	if (rc) {
		pr_err("failed to read beat count rc=%d\n", rc);
		goto resched;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("current: %d, prev: %d\n", beat_count,
			chip->last_beat_count);

	if (chip->last_beat_count == beat_count) {
		if (!tried_once) {
			/* Wait for 1 FG cycle and read it once again */
			msleep(1500);
			tried_once = true;
			goto try_again;
		} else {
			pr_err("Beat count not updating\n");
			fg_check_ima_error_handling(chip);
			goto out;
		}
	} else {
		chip->last_beat_count = beat_count;
	}
resched:
	schedule_delayed_work(
		&chip->check_sanity_work,
		msecs_to_jiffies(SANITY_CHECK_PERIOD_MS));
out:
	fg_relax(&chip->sanity_wakeup_source);
}

#define SRAM_TIMEOUT_MS			3000
static void update_sram_data_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				update_sram_data.work);
	int resched_ms, ret;
	bool tried_again = false;
	int rc = 0;

wait:
	/* Wait for MEMIF access revoked */
	ret = wait_for_completion_interruptible_timeout(
			&chip->sram_access_revoked,
			msecs_to_jiffies(SRAM_TIMEOUT_MS));

	/* If we were interrupted wait again one more time. */
	if (ret == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		goto wait;
	} else if (ret <= 0) {
		pr_err("transaction timed out ret=%d\n", ret);
		if (fg_is_batt_id_valid(chip))
			resched_ms = fg_sram_update_period_ms;
		else
			resched_ms = SRAM_PERIOD_NO_ID_UPDATE_MS;
		goto out;
	}
	rc = update_sram_data(chip, &resched_ms);

out:
	if (!rc)
		schedule_delayed_work(
			&chip->update_sram_data,
			msecs_to_jiffies(resched_ms));
}

#define BATT_TEMP_OFFSET	3
#define BATT_TEMP_CNTRL_MASK	0x17
#define DISABLE_THERM_BIT	BIT(0)
#define TEMP_SENSE_ALWAYS_BIT	BIT(1)
#define TEMP_SENSE_CHARGE_BIT	BIT(2)
#define FORCE_RBIAS_ON_BIT	BIT(4)
#define BATT_TEMP_OFF		DISABLE_THERM_BIT
#define BATT_TEMP_ON		(FORCE_RBIAS_ON_BIT | TEMP_SENSE_ALWAYS_BIT | \
				TEMP_SENSE_CHARGE_BIT)
#define TEMP_PERIOD_UPDATE_MS		10000
#define TEMP_PERIOD_TIMEOUT_MS		3000
#define BATT_TEMP_LOW_LIMIT		-600
#define BATT_TEMP_HIGH_LIMIT		1500
static void update_temp_data(struct work_struct *work)
{
	s16 temp;
	u8 reg[2];
	bool tried_again = false;
	int rc, ret, timeout = TEMP_PERIOD_TIMEOUT_MS;
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				update_temp_work.work);

	if (chip->fg_restarting)
		goto resched;

	fg_stay_awake(&chip->update_temp_wakeup_source);
	if (chip->sw_rbias_ctrl) {
		rc = fg_mem_masked_write(chip, EXTERNAL_SENSE_SELECT,
				BATT_TEMP_CNTRL_MASK,
				BATT_TEMP_ON,
				BATT_TEMP_OFFSET);
		if (rc) {
			pr_err("failed to write BATT_TEMP_ON rc=%d\n", rc);
			goto out;
		}

wait:
		/* Wait for MEMIF access revoked */
		ret = wait_for_completion_interruptible_timeout(
				&chip->sram_access_revoked,
				msecs_to_jiffies(timeout));

		/* If we were interrupted wait again one more time. */
		if (ret == -ERESTARTSYS && !tried_again) {
			tried_again = true;
			goto wait;
		} else if (ret <= 0) {
			rc = -ETIMEDOUT;
			pr_err("transaction timed out ret=%d\n", ret);
			goto out;
		}
	}

	/* Read FG_DATA_BATT_TEMP now */
	rc = fg_mem_read(chip, reg, fg_data[0].address,
		fg_data[0].len, fg_data[0].offset,
		chip->sw_rbias_ctrl ? 1 : 0);
	if (rc) {
		pr_err("Failed to update temp data\n");
		goto out;
	}

	temp = reg[0] | (reg[1] << 8);
	temp = (temp * TEMP_LSB_16B / 1000) - DECIKELVIN;

	/*
	 * If temperature is within the specified range (e.g. -60C and 150C),
	 * update it to the userspace. Otherwise, use the last read good
	 * temperature.
	 */
	if (temp > chip->batt_temp_low_limit &&
			temp < chip->batt_temp_high_limit) {
		chip->last_good_temp = temp;
		fg_data[0].value = temp;
	} else {
		fg_data[0].value = chip->last_good_temp;

		/*
		 * If the temperature is read before and seems to be in valid
		 * range, then a bad temperature reading could be because of
		 * FG lockup. Trigger the FG reset sequence in such cases.
		 */
		if (chip->last_temp_update_time && fg_reset_on_lockup &&
			(chip->last_good_temp > chip->batt_temp_low_limit &&
			chip->last_good_temp < chip->batt_temp_high_limit)) {
			pr_err("Batt_temp is %d !, triggering FG reset\n",
				temp);
			fg_check_ima_error_handling(chip);
		}
	}

	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("BATT_TEMP %d %d\n", temp, fg_data[0].value);

	get_current_time(&chip->last_temp_update_time);

	if (chip->soc_slope_limiter_en) {
		fg_stay_awake(&chip->slope_limit_wakeup_source);
		schedule_work(&chip->slope_limiter_work);
	}

out:
	if (chip->sw_rbias_ctrl) {
		rc = fg_mem_masked_write(chip, EXTERNAL_SENSE_SELECT,
				BATT_TEMP_CNTRL_MASK,
				BATT_TEMP_OFF,
				BATT_TEMP_OFFSET);
		if (rc)
			pr_err("failed to write BATT_TEMP_OFF rc=%d\n", rc);
	}
	fg_relax(&chip->update_temp_wakeup_source);

resched:
	schedule_delayed_work(
		&chip->update_temp_work,
		msecs_to_jiffies(TEMP_PERIOD_UPDATE_MS));
}

static void update_jeita_setting(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				update_jeita_setting.work);
	u8 reg[4];
	int i, rc;

	for (i = 0; i < 4; i++)
		reg[i] = (settings[FG_MEM_SOFT_COLD + i].value / 10) + 30;

	rc = fg_mem_write(chip, reg, settings[FG_MEM_SOFT_COLD].address,
			4, settings[FG_MEM_SOFT_COLD].offset, 0);
	if (rc)
		pr_err("failed to update JEITA setting rc=%d\n", rc);
}

static int fg_set_resume_soc(struct fg_chip *chip, u8 threshold)
{
	u16 address;
	int offset, rc;

	address = settings[FG_MEM_RESUME_SOC].address;
	offset = settings[FG_MEM_RESUME_SOC].offset;

	rc = fg_mem_masked_write(chip, address, 0xFF, threshold, offset);

	if (rc)
		pr_err("write failed rc=%d\n", rc);
	else
		pr_debug("setting resume-soc to %x\n", threshold);

	return rc;
}

#define BATT_CYCLE_NUMBER_REG		0x5E8
#define BATT_CYCLE_OFFSET		0
static void restore_cycle_counter(struct fg_chip *chip)
{
	int rc = 0, i, address;
	u8 data[2];

	fg_mem_lock(chip);
	for (i = 0; i < BUCKET_COUNT; i++) {
		address = BATT_CYCLE_NUMBER_REG + i * 2;
		rc = fg_mem_read(chip, (u8 *)&data, address, 2,
				BATT_CYCLE_OFFSET, 0);
		if (rc)
			pr_err("Failed to read BATT_CYCLE_NUMBER[%d] rc: %d\n",
				i, rc);
		else
			chip->cyc_ctr.count[i] = data[0] | data[1] << 8;
	}
	fg_mem_release(chip);
}

static void clear_cycle_counter(struct fg_chip *chip)
{
	int rc = 0, len, i;

	if (!chip->cyc_ctr.en)
		return;

	len = sizeof(chip->cyc_ctr.count);
	memset(chip->cyc_ctr.count, 0, len);
	for (i = 0; i < BUCKET_COUNT; i++) {
		chip->cyc_ctr.started[i] = false;
		chip->cyc_ctr.last_soc[i] = 0;
	}
	rc = fg_mem_write(chip, (u8 *)&chip->cyc_ctr.count,
			BATT_CYCLE_NUMBER_REG, len,
			BATT_CYCLE_OFFSET, 0);
	if (rc)
		pr_err("failed to write BATT_CYCLE_NUMBER rc=%d\n", rc);
}

static int fg_inc_store_cycle_ctr(struct fg_chip *chip, int bucket)
{
	int rc = 0, address;
	u16 cyc_count;
	u8 data[2];

	if (bucket < 0 || (bucket > BUCKET_COUNT - 1))
		return 0;

	cyc_count = chip->cyc_ctr.count[bucket];
	cyc_count++;
	data[0] = cyc_count & 0xFF;
	data[1] = cyc_count >> 8;

	address = BATT_CYCLE_NUMBER_REG + bucket * 2;

	rc = fg_mem_write(chip, data, address, 2, BATT_CYCLE_OFFSET, 0);
	if (rc)
		pr_err("failed to write BATT_CYCLE_NUMBER[%d] rc=%d\n",
			bucket, rc);
	else
		chip->cyc_ctr.count[bucket] = cyc_count;

	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("Stored bucket %d cyc_count: %d\n", bucket, cyc_count);
	return rc;
}

static void update_cycle_count(struct work_struct *work)
{
	int rc = 0, bucket, i;
	u8 reg[3], batt_soc;
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				cycle_count_work);

	mutex_lock(&chip->cyc_ctr.lock);
	rc = fg_mem_read(chip, reg, BATTERY_SOC_REG, 3,
			BATTERY_SOC_OFFSET, 0);
	if (rc) {
		pr_err("Failed to read battery soc rc: %d\n", rc);
		goto out;
	}
	batt_soc = reg[2];

	if (chip->status == POWER_SUPPLY_STATUS_CHARGING) {
		/* Find out which bucket the SOC falls in */
		bucket = batt_soc / BUCKET_SOC_PCT;

		if (fg_debug_mask & FG_STATUS)
			pr_info("batt_soc: %x bucket: %d\n", reg[2], bucket);

		/*
		 * If we've started counting for the previous bucket,
		 * then store the counter for that bucket if the
		 * counter for current bucket is getting started.
		 */
		if (bucket > 0 && chip->cyc_ctr.started[bucket - 1] &&
			!chip->cyc_ctr.started[bucket]) {
			rc = fg_inc_store_cycle_ctr(chip, bucket - 1);
			if (rc) {
				pr_err("Error in storing cycle_ctr rc: %d\n",
					rc);
				goto out;
			} else {
				chip->cyc_ctr.started[bucket - 1] = false;
				chip->cyc_ctr.last_soc[bucket - 1] = 0;
			}
		}
		if (!chip->cyc_ctr.started[bucket]) {
			chip->cyc_ctr.started[bucket] = true;
			chip->cyc_ctr.last_soc[bucket] = batt_soc;
		}
	} else {
		for (i = 0; i < BUCKET_COUNT; i++) {
			if (chip->cyc_ctr.started[i] &&
				batt_soc > chip->cyc_ctr.last_soc[i]) {
				rc = fg_inc_store_cycle_ctr(chip, i);
				if (rc)
					pr_err("Error in storing cycle_ctr rc: %d\n",
						rc);
				chip->cyc_ctr.last_soc[i] = 0;
			}
			chip->cyc_ctr.started[i] = false;
		}
	}
out:
	mutex_unlock(&chip->cyc_ctr.lock);
}

static int fg_get_cycle_count(struct fg_chip *chip)
{
	int count;

	if (!chip->cyc_ctr.en)
		return 0;

	if ((chip->cyc_ctr.id <= 0) || (chip->cyc_ctr.id > BUCKET_COUNT))
		return -EINVAL;

	mutex_lock(&chip->cyc_ctr.lock);
	count = chip->cyc_ctr.count[chip->cyc_ctr.id - 1];
	mutex_unlock(&chip->cyc_ctr.lock);
	return count;
}

static void half_float_to_buffer(int64_t uval, u8 *buffer)
{
	u16 raw;

	raw = float_encode(uval);
	buffer[0] = (u8)(raw & 0xFF);
	buffer[1] = (u8)((raw >> 8) & 0xFF);
}

static int64_t half_float(u8 *buffer)
{
	u16 val;

	val = buffer[1] << 8 | buffer[0];
	return float_decode(val);
}

static int voltage_2b(u8 *buffer)
{
	u16 val;

	val = buffer[1] << 8 | buffer[0];
	/* the range of voltage 2b is [-5V, 5V], so it will fit in an int */
	return (int)div_u64(((u64)val) * LSB_16B_NUMRTR, LSB_16B_DENMTR);
}

static int bcap_uah_2b(u8 *buffer)
{
	u16 val;

	val = buffer[1] << 8 | buffer[0];
	return ((int)val) * 1000;
}

#define SLOPE_LIMITER_COEFF_REG		0x430
#define SLOPE_LIMITER_COEFF_OFFSET	3
#define SLOPE_LIMIT_TEMP_THRESHOLD	100
#define SLOPE_LIMIT_LOW_TEMP_CHG	45
#define SLOPE_LIMIT_HIGH_TEMP_CHG	2
#define SLOPE_LIMIT_LOW_TEMP_DISCHG	45
#define SLOPE_LIMIT_HIGH_TEMP_DISCHG	2
static void slope_limiter_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work, struct fg_chip,
				slope_limiter_work);
	enum slope_limit_status status;
	int batt_temp, rc;
	u8 buf[2];
	int64_t val;

	batt_temp = get_sram_prop_now(chip, FG_DATA_BATT_TEMP);

	if (chip->status == POWER_SUPPLY_STATUS_CHARGING ||
			chip->status == POWER_SUPPLY_STATUS_FULL) {
		if (batt_temp < chip->slope_limit_temp)
			status = LOW_TEMP_CHARGE;
		else
			status = HIGH_TEMP_CHARGE;
	} else if (chip->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		if (batt_temp < chip->slope_limit_temp)
			status = LOW_TEMP_DISCHARGE;
		else
			status = HIGH_TEMP_DISCHARGE;
	} else {
		goto out;
	}

	if (status == chip->slope_limit_sts)
		goto out;

	val = chip->slope_limit_coeffs[status];
	val *= MICRO_UNIT;
	half_float_to_buffer(val, buf);
	rc = fg_mem_write(chip, buf,
			SLOPE_LIMITER_COEFF_REG, 2,
			SLOPE_LIMITER_COEFF_OFFSET, 0);
	if (rc) {
		pr_err("Couldn't write to slope_limiter_coeff_reg, rc=%d\n",
			rc);
		goto out;
	}

	chip->slope_limit_sts = status;
	if (fg_debug_mask & FG_STATUS)
		pr_info("Slope limit sts: %d val: %lld buf[%x %x] written\n",
			status, val, buf[0], buf[1]);
out:
	fg_relax(&chip->slope_limit_wakeup_source);
}

static int lookup_ocv_for_soc(struct fg_chip *chip, int soc)
{
	int64_t *coeffs;

	if (soc > chip->ocv_junction_p1p2 * 10)
		coeffs = chip->ocv_coeffs;
	else if (soc > chip->ocv_junction_p2p3 * 10)
		coeffs = chip->ocv_coeffs + 4;
	else
		coeffs = chip->ocv_coeffs + 8;
	/* the range of ocv will fit in a 32 bit int */
	return (int)(coeffs[0]
		+ div_s64(coeffs[1] * soc, 1000LL)
		+ div_s64(coeffs[2] * soc * soc, 1000000LL)
		+ div_s64(coeffs[3] * soc * soc * soc, 1000000000LL));
}

static int lookup_soc_for_ocv(struct fg_chip *chip, int ocv)
{
	int64_t val;
	int soc = -EINVAL;
	/*
	 * binary search variables representing the valid start and end
	 * percentages to search
	 */
	int start = 0, end = 1000, mid;

	if (fg_debug_mask & FG_AGING)
		pr_info("target_ocv = %d\n", ocv);
	/* do a binary search for the closest soc to match the ocv */
	while (end - start > 1) {
		mid = (start + end) / 2;
		val = lookup_ocv_for_soc(chip, mid);
		if (fg_debug_mask & FG_AGING)
			pr_info("start = %d, mid = %d, end = %d, ocv = %lld\n",
					start, mid, end, val);
		if (ocv < val) {
			end = mid;
		} else if (ocv > val) {
			start = mid;
		} else {
			soc = mid;
			break;
		}
	}
	/*
	 * if the exact soc was not found and there are two or less values
	 * remaining, just compare them and see which one is closest to the ocv
	 */
	if (soc == -EINVAL) {
		if (abs(ocv - lookup_ocv_for_soc(chip, start))
				> abs(ocv - lookup_ocv_for_soc(chip, end)))
			soc = end;
		else
			soc = start;
	}
	if (fg_debug_mask & FG_AGING)
		pr_info("closest = %d, target_ocv = %d, ocv_found = %d\n",
				soc, ocv, lookup_ocv_for_soc(chip, soc));
	return soc;
}

#define ESR_ACTUAL_REG		0x554
#define BATTERY_ESR_REG		0x4F4
#define TEMP_RS_TO_RSLOW_REG	0x514
#define ESR_OFFSET		2
static int estimate_battery_age(struct fg_chip *chip, int *actual_capacity)
{
	int64_t ocv_cutoff_new, ocv_cutoff_aged, temp_rs_to_rslow;
	int64_t esr_actual, battery_esr, val;
	int soc_cutoff_aged, soc_cutoff_new, rc;
	int battery_soc, unusable_soc, batt_temp;
	u8 buffer[3];

	if (chip->batt_aging_mode != FG_AGING_ESR)
		return 0;

	if (chip->nom_cap_uah == 0) {
		if (fg_debug_mask & FG_AGING)
			pr_info("ocv coefficients not loaded, aborting\n");
		return 0;
	}
	fg_mem_lock(chip);

	batt_temp = get_sram_prop_now(chip, FG_DATA_BATT_TEMP);
	if (batt_temp < 150 || batt_temp > 400) {
		if (fg_debug_mask & FG_AGING)
			pr_info("Battery temp (%d) out of range, aborting\n",
					(int)batt_temp);
		rc = 0;
		goto done;
	}

	battery_soc = get_battery_soc_raw(chip) * 100 / FULL_PERCENT_3B;
	if (rc) {
		goto error_done;
	} else if (battery_soc < 25 || battery_soc > 75) {
		if (fg_debug_mask & FG_AGING)
			pr_info("Battery SoC (%d) out of range, aborting\n",
					(int)battery_soc);
		rc = 0;
		goto done;
	}

	rc = fg_mem_read(chip, buffer, ESR_ACTUAL_REG, 2, 2, 0);
	esr_actual = half_float(buffer);
	rc |= fg_mem_read(chip, buffer, BATTERY_ESR_REG, 2, ESR_OFFSET, 0);
	battery_esr = half_float(buffer);

	if (rc) {
		goto error_done;
	} else if (esr_actual < battery_esr) {
		if (fg_debug_mask & FG_AGING)
			pr_info("Batt ESR lower than ESR actual, aborting\n");
		rc = 0;
		goto done;
	}
	rc = fg_mem_read(chip, buffer, TEMP_RS_TO_RSLOW_REG, 2, 0, 0);
	temp_rs_to_rslow = half_float(buffer);

	if (rc)
		goto error_done;

	fg_mem_release(chip);

	if (fg_debug_mask & FG_AGING) {
		pr_info("batt_soc = %d, cutoff_voltage = %lld, eval current = %d\n",
				battery_soc, chip->cutoff_voltage,
				chip->evaluation_current);
		pr_info("temp_rs_to_rslow = %lld, batt_esr = %lld, esr_actual = %lld\n",
				temp_rs_to_rslow, battery_esr, esr_actual);
	}

	/* calculate soc_cutoff_new */
	val = (1000000LL + temp_rs_to_rslow) * battery_esr;
	do_div(val, 1000000);
	ocv_cutoff_new = div64_s64(chip->evaluation_current * val, 1000)
		+ chip->cutoff_voltage;

	/* calculate soc_cutoff_aged */
	val = (1000000LL + temp_rs_to_rslow) * esr_actual;
	do_div(val, 1000000);
	ocv_cutoff_aged = div64_s64(chip->evaluation_current * val, 1000)
		+ chip->cutoff_voltage;

	if (fg_debug_mask & FG_AGING)
		pr_info("ocv_cutoff_new = %lld, ocv_cutoff_aged = %lld\n",
				ocv_cutoff_new, ocv_cutoff_aged);

	soc_cutoff_new = lookup_soc_for_ocv(chip, ocv_cutoff_new);
	soc_cutoff_aged = lookup_soc_for_ocv(chip, ocv_cutoff_aged);

	if (fg_debug_mask & FG_AGING)
		pr_info("aged soc = %d, new soc = %d\n",
				soc_cutoff_aged, soc_cutoff_new);
	unusable_soc = soc_cutoff_aged - soc_cutoff_new;

	*actual_capacity = div64_s64(((int64_t)chip->nom_cap_uah)
				* (1000 - unusable_soc), 1000);
	if (fg_debug_mask & FG_AGING)
		pr_info("nom cap = %d, actual cap = %d\n",
				chip->nom_cap_uah, *actual_capacity);

	return rc;

error_done:
	pr_err("some register reads failed: %d\n", rc);
done:
	fg_mem_release(chip);
	return rc;
}

static void battery_age_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				battery_age_work);

	estimate_battery_age(chip, &chip->actual_cap_uah);
}

static int correction_times[] = {
	1470,
	2940,
	4410,
	5880,
	7350,
	8820,
	10290,
	11760,
	13230,
	14700,
	16170,
	17640,
	19110,
	20580,
	22050,
	23520,
	24990,
	26460,
	27930,
	29400,
	30870,
	32340,
	33810,
	35280,
	36750,
	38220,
	39690,
	41160,
	42630,
	44100,
	45570,
	47040,
};

static int correction_factors[] = {
	1000000,
	1007874,
	1015789,
	1023745,
	1031742,
	1039780,
	1047859,
	1055979,
	1064140,
	1072342,
	1080584,
	1088868,
	1097193,
	1105558,
	1113964,
	1122411,
	1130899,
	1139427,
	1147996,
	1156606,
	1165256,
	1173947,
	1182678,
	1191450,
	1200263,
	1209115,
	1218008,
	1226942,
	1235915,
	1244929,
	1253983,
	1263076,
};

#define FG_CONVERSION_FACTOR	(64198531LL)
static int iavg_3b_to_uah(u8 *buffer, int delta_ms)
{
	int64_t val, i_filtered;
	int i, correction_factor;

	for (i = 0; i < ARRAY_SIZE(correction_times); i++) {
		if (correction_times[i] > delta_ms)
			break;
	}
	if (i >= ARRAY_SIZE(correction_times)) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("fuel gauge took more than 32 cycles\n");
		i = ARRAY_SIZE(correction_times) - 1;
	}
	correction_factor = correction_factors[i];
	if (fg_debug_mask & FG_STATUS)
		pr_info("delta_ms = %d, cycles = %d, correction = %d\n",
				delta_ms, i, correction_factor);
	val = buffer[2] << 16 | buffer[1] << 8 | buffer[0];
	/* convert val from signed 24b to signed 64b */
	i_filtered = (val << 40) >> 40;
	val = i_filtered * correction_factor;
	val = div64_s64(val + FG_CONVERSION_FACTOR / 2, FG_CONVERSION_FACTOR);
	if (fg_debug_mask & FG_STATUS)
		pr_info("i_filtered = 0x%llx/%lld, cc_uah = %lld\n",
				i_filtered, i_filtered, val);

	return val;
}

static bool fg_is_temperature_ok_for_learning(struct fg_chip *chip)
{
	int batt_temp = get_sram_prop_now(chip, FG_DATA_BATT_TEMP);

	if (batt_temp > chip->learning_data.max_temp
			|| batt_temp < chip->learning_data.min_temp) {
		if (fg_debug_mask & FG_AGING)
			pr_info("temp (%d) out of range [%d, %d], aborting\n",
					batt_temp,
					chip->learning_data.min_temp,
					chip->learning_data.max_temp);
		return false;
	}
	return true;
}

static void fg_cap_learning_stop(struct fg_chip *chip)
{
	chip->learning_data.cc_uah = 0;
	chip->learning_data.active = false;
}

#define I_FILTERED_REG			0x584
static void fg_cap_learning_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				fg_cap_learning_work);
	u8 i_filtered[3], data[3];
	int rc, cc_uah, delta_ms;
	ktime_t now_kt, delta_kt;

	mutex_lock(&chip->learning_data.learning_lock);
	if (!chip->learning_data.active)
		goto fail;
	if (!fg_is_temperature_ok_for_learning(chip)) {
		fg_cap_learning_stop(chip);
		goto fail;
	}

	if (chip->wa_flag & USE_CC_SOC_REG)
		goto fail;

	fg_mem_lock(chip);

	rc = fg_mem_read(chip, i_filtered, I_FILTERED_REG, 3, 0, 0);
	if (rc) {
		pr_err("Failed to read i_filtered: %d\n", rc);
		fg_mem_release(chip);
		goto fail;
	}
	memset(data, 0, 3);
	rc = fg_mem_write(chip, data, I_FILTERED_REG, 3, 0, 0);
	if (rc) {
		pr_err("Failed to clear i_filtered: %d\n", rc);
		fg_mem_release(chip);
		goto fail;
	}
	fg_mem_release(chip);

	now_kt = ktime_get_boottime();
	delta_kt = ktime_sub(now_kt, chip->learning_data.time_stamp);
	chip->learning_data.time_stamp = now_kt;

	delta_ms = (int)div64_s64(ktime_to_ns(delta_kt), 1000000);

	cc_uah = iavg_3b_to_uah(i_filtered, delta_ms);
	chip->learning_data.cc_uah -= cc_uah;
	if (fg_debug_mask & FG_AGING)
		pr_info("total_cc_uah = %lld\n", chip->learning_data.cc_uah);

fail:
	if (chip->wa_flag & USE_CC_SOC_REG)
		fg_relax(&chip->capacity_learning_wakeup_source);
	mutex_unlock(&chip->learning_data.learning_lock);
	return;

}

#define CC_SOC_BASE_REG		0x5BC
#define CC_SOC_OFFSET		3
static int fg_get_cc_soc(struct fg_chip *chip, int *cc_soc)
{
	int rc;
	u8 reg[4];
	int temp;

	rc = fg_mem_read(chip, reg, CC_SOC_BASE_REG, 4, CC_SOC_OFFSET, 0);
	if (rc) {
		pr_err("Failed to read CC_SOC_REG rc=%d\n", rc);
		return rc;
	}

	temp = reg[3] << 24 | reg[2] << 16 | reg[1] << 8 | reg[0];
	*cc_soc = sign_extend32(temp, 29);
	return 0;
}

static int fg_get_current_cc(struct fg_chip *chip)
{
	int cc_soc, rc;
	int64_t current_capacity;

	if (!(chip->wa_flag & USE_CC_SOC_REG))
		return chip->learning_data.cc_uah;

	if (!chip->learning_data.learned_cc_uah)
		return -EINVAL;

	rc = fg_get_cc_soc(chip, &cc_soc);
	if (rc < 0) {
		pr_err("Failed to get cc_soc, rc=%d\n", rc);
		return rc;
	}

	current_capacity = cc_soc * chip->learning_data.learned_cc_uah;
	do_div(current_capacity, FULL_PERCENT_28BIT);
	return current_capacity;
}

#define BATT_MISSING_STS BIT(6)
static bool is_battery_missing(struct fg_chip *chip)
{
	int rc;
	u8 fg_batt_sts;

	rc = fg_read(chip, &fg_batt_sts,
				 INT_RT_STS(chip->batt_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->batt_base), rc);
		return false;
	}

	return (fg_batt_sts & BATT_MISSING_STS) ? true : false;
}

static int fg_cap_learning_process_full_data(struct fg_chip *chip)
{
	int cc_pc_val, rc = -EINVAL;
	unsigned int cc_soc_delta_pc;
	int64_t delta_cc_uah;
	uint64_t temp;
	bool batt_missing = is_battery_missing(chip);

	if (batt_missing) {
		pr_err("Battery is missing!\n");
		goto fail;
	}

	if (!chip->learning_data.active)
		goto fail;

	if (!fg_is_temperature_ok_for_learning(chip)) {
		fg_cap_learning_stop(chip);
		goto fail;
	}

	rc = fg_get_cc_soc(chip, &cc_pc_val);
	if (rc) {
		pr_err("failed to get CC_SOC, stopping capacity learning\n");
		fg_cap_learning_stop(chip);
		goto fail;
	}

	temp = abs(cc_pc_val - chip->learning_data.init_cc_pc_val);
	cc_soc_delta_pc = DIV_ROUND_CLOSEST_ULL(temp * 100, FULL_PERCENT_28BIT);

	delta_cc_uah = div64_s64(
			chip->learning_data.learned_cc_uah * cc_soc_delta_pc,
			100);
	chip->learning_data.cc_uah = delta_cc_uah + chip->learning_data.cc_uah;

	if (fg_debug_mask & FG_AGING)
		pr_info("current cc_soc=%d cc_soc_pc=%d init_cc_pc_val=%d delta_cc_uah=%lld learned_cc_uah=%lld total_cc_uah = %lld\n",
				cc_pc_val, cc_soc_delta_pc,
				chip->learning_data.init_cc_pc_val,
				delta_cc_uah,
				chip->learning_data.learned_cc_uah,
				chip->learning_data.cc_uah);

	return 0;

fail:
	return rc;
}

#define FG_CAP_LEARNING_INTERVAL_NS	30000000000
static enum alarmtimer_restart fg_cap_learning_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct fg_chip *chip = container_of(alarm, struct fg_chip,
					fg_cap_learning_alarm);

	if (chip->learning_data.active) {
		if (fg_debug_mask & FG_AGING)
			pr_info("alarm fired\n");
		schedule_work(&chip->fg_cap_learning_work);
		alarm_forward_now(alarm,
				ns_to_ktime(FG_CAP_LEARNING_INTERVAL_NS));
		return ALARMTIMER_RESTART;
	}
	if (fg_debug_mask & FG_AGING)
		pr_info("alarm misfired\n");
	return ALARMTIMER_NORESTART;
}

#define FG_AGING_STORAGE_REG		0x5E4
#define ACTUAL_CAPACITY_REG		0x578
#define MAH_TO_SOC_CONV_REG		0x4A0
#define CC_SOC_COEFF_OFFSET		0
#define ACTUAL_CAPACITY_OFFSET		2
#define MAH_TO_SOC_CONV_CS_OFFSET	0
static int fg_calc_and_store_cc_soc_coeff(struct fg_chip *chip, int16_t cc_mah)
{
	int rc;
	int64_t cc_to_soc_coeff, mah_to_soc;
	u8 data[2];

	rc = fg_mem_write(chip, (u8 *)&cc_mah, ACTUAL_CAPACITY_REG, 2,
			ACTUAL_CAPACITY_OFFSET, 0);
	if (rc) {
		pr_err("Failed to store actual capacity: %d\n", rc);
		return rc;
	}

	rc = fg_mem_read(chip, (u8 *)&data, MAH_TO_SOC_CONV_REG, 2,
			MAH_TO_SOC_CONV_CS_OFFSET, 0);
	if (rc) {
		pr_err("Failed to read mah_to_soc_conv_cs: %d\n", rc);
	} else {
		mah_to_soc = data[1] << 8 | data[0];
		mah_to_soc *= MICRO_UNIT;
		cc_to_soc_coeff = div64_s64(mah_to_soc, cc_mah);
		half_float_to_buffer(cc_to_soc_coeff, data);
		rc = fg_mem_write(chip, (u8 *)data,
				ACTUAL_CAPACITY_REG, 2,
				CC_SOC_COEFF_OFFSET, 0);
		if (rc)
			pr_err("Failed to write cc_soc_coeff_offset: %d\n",
				rc);
		else if (fg_debug_mask & FG_AGING)
			pr_info("new cc_soc_coeff %lld [%x %x] saved to sram\n",
				cc_to_soc_coeff, data[0], data[1]);
	}
	return rc;
}

static void fg_cap_learning_load_data(struct fg_chip *chip)
{
	int16_t cc_mah;
	int64_t old_cap = chip->learning_data.learned_cc_uah;
	int rc;

	rc = fg_mem_read(chip, (u8 *)&cc_mah, FG_AGING_STORAGE_REG, 2, 0, 0);
	if (rc) {
		pr_err("Failed to load aged capacity: %d\n", rc);
	} else {
		chip->learning_data.learned_cc_uah = cc_mah * 1000;
		if (fg_debug_mask & FG_AGING)
			pr_info("learned capacity %lld-> %lld/%x uah\n",
					old_cap,
					chip->learning_data.learned_cc_uah,
					cc_mah);
	}
}

static void fg_cap_learning_save_data(struct fg_chip *chip)
{
	int16_t cc_mah;
	int rc;
	bool batt_missing = is_battery_missing(chip);

	if (batt_missing) {
		pr_err("Battery is missing!\n");
		return;
	}

	cc_mah = div64_s64(chip->learning_data.learned_cc_uah, 1000);

	rc = fg_mem_write(chip, (u8 *)&cc_mah, FG_AGING_STORAGE_REG, 2, 0, 0);
	if (rc)
		pr_err("Failed to store aged capacity: %d\n", rc);
	else if (fg_debug_mask & FG_AGING)
		pr_info("learned capacity %lld uah (%d/0x%x uah) saved to sram\n",
				chip->learning_data.learned_cc_uah,
				cc_mah, cc_mah);

	if (chip->learning_data.feedback_on) {
		rc = fg_calc_and_store_cc_soc_coeff(chip, cc_mah);
		if (rc)
			pr_err("Error in storing cc_soc_coeff, rc:%d\n", rc);
	}
}

static void fg_cap_learning_post_process(struct fg_chip *chip)
{
	int64_t max_inc_val, min_dec_val, old_cap;
	bool batt_missing = is_battery_missing(chip);

	if (batt_missing) {
		pr_err("Battery is missing!\n");
		return;
	}

	max_inc_val = chip->learning_data.learned_cc_uah
			* (1000 + chip->learning_data.max_increment);
	do_div(max_inc_val, 1000);

	min_dec_val = chip->learning_data.learned_cc_uah
			* (1000 - chip->learning_data.max_decrement);
	do_div(min_dec_val, 1000);

	old_cap = chip->learning_data.learned_cc_uah;
	if (chip->learning_data.cc_uah > max_inc_val)
		chip->learning_data.learned_cc_uah = max_inc_val;
	else if (chip->learning_data.cc_uah < min_dec_val)
		chip->learning_data.learned_cc_uah = min_dec_val;
	else
		chip->learning_data.learned_cc_uah =
			chip->learning_data.cc_uah;

	if (chip->learning_data.max_cap_limit) {
		max_inc_val = (int64_t)chip->nom_cap_uah * (1000 +
				chip->learning_data.max_cap_limit);
		do_div(max_inc_val, 1000);
		if (chip->learning_data.cc_uah > max_inc_val) {
			if (fg_debug_mask & FG_AGING)
				pr_info("learning capacity %lld goes above max limit %lld\n",
					chip->learning_data.cc_uah,
					max_inc_val);
			chip->learning_data.learned_cc_uah = max_inc_val;
		}
	}

	if (chip->learning_data.min_cap_limit) {
		min_dec_val = (int64_t)chip->nom_cap_uah * (1000 -
				chip->learning_data.min_cap_limit);
		do_div(min_dec_val, 1000);
		if (chip->learning_data.cc_uah < min_dec_val) {
			if (fg_debug_mask & FG_AGING)
				pr_info("learning capacity %lld goes below min limit %lld\n",
					chip->learning_data.cc_uah,
					min_dec_val);
			chip->learning_data.learned_cc_uah = min_dec_val;
		}
	}

	fg_cap_learning_save_data(chip);
	if (fg_debug_mask & FG_AGING)
		pr_info("final cc_uah = %lld, learned capacity %lld -> %lld uah\n",
				chip->learning_data.cc_uah,
				old_cap, chip->learning_data.learned_cc_uah);
}

static int get_vbat_est_diff(struct fg_chip *chip)
{
	return abs(fg_data[FG_DATA_VOLTAGE].value
				- fg_data[FG_DATA_CPRED_VOLTAGE].value);
}

#define CBITS_INPUT_FILTER_REG		0x4B4
#define IBATTF_TAU_MASK			0x38
#define IBATTF_TAU_99_S			0x30
static int fg_cap_learning_check(struct fg_chip *chip)
{
	u8 data[4];
	int rc = 0, battery_soc, cc_pc_val;
	int vbat_est_diff, vbat_est_thr_uv;
	unsigned int cc_pc_100 = FULL_PERCENT_28BIT;

	mutex_lock(&chip->learning_data.learning_lock);
	if (chip->status == POWER_SUPPLY_STATUS_CHARGING
				&& !chip->learning_data.active
				&& chip->batt_aging_mode == FG_AGING_CC) {
		if (chip->learning_data.learned_cc_uah == 0) {
			if (fg_debug_mask & FG_AGING)
				pr_info("no capacity, aborting\n");
			goto fail;
		}

		if (!fg_is_temperature_ok_for_learning(chip))
			goto fail;

		fg_mem_lock(chip);
		if (!chip->learning_data.feedback_on) {
			vbat_est_diff = get_vbat_est_diff(chip);
			vbat_est_thr_uv = chip->learning_data.vbat_est_thr_uv;
			if (vbat_est_diff >= vbat_est_thr_uv &&
					vbat_est_thr_uv > 0) {
				if (fg_debug_mask & FG_AGING)
					pr_info("vbat_est_diff (%d) < threshold (%d)\n",
						vbat_est_diff, vbat_est_thr_uv);
				fg_mem_release(chip);
				fg_cap_learning_stop(chip);
				goto fail;
			}
		}
		battery_soc = get_battery_soc_raw(chip);
		if (fg_debug_mask & FG_AGING)
			pr_info("checking battery soc (%d vs %d)\n",
				battery_soc * 100 / FULL_PERCENT_3B,
				chip->learning_data.max_start_soc);
		/* check if the battery is low enough to start soc learning */
		if (battery_soc * 100 / FULL_PERCENT_3B
				> chip->learning_data.max_start_soc) {
			if (fg_debug_mask & FG_AGING)
				pr_info("battery soc too high (%d > %d), aborting\n",
					battery_soc * 100 / FULL_PERCENT_3B,
					chip->learning_data.max_start_soc);
			fg_mem_release(chip);
			fg_cap_learning_stop(chip);
			goto fail;
		}

		/* set the coulomb counter to a percentage of the capacity */
		chip->learning_data.cc_uah = div64_s64(
			(chip->learning_data.learned_cc_uah * battery_soc),
				FULL_PERCENT_3B);

		/* Use CC_SOC_REG based capacity learning */
		if (chip->wa_flag & USE_CC_SOC_REG) {
			fg_mem_release(chip);
			/* SW_CC_SOC based capacity learning */
			if (fg_get_cc_soc(chip, &cc_pc_val)) {
				pr_err("failed to get CC_SOC, stop capacity learning\n");
				fg_cap_learning_stop(chip);
				goto fail;
			}

			chip->learning_data.init_cc_pc_val = cc_pc_val;
			chip->learning_data.active = true;
			if (fg_debug_mask & FG_AGING)
				pr_info("SW_CC_SOC based learning init_CC_SOC=%d\n",
					chip->learning_data.init_cc_pc_val);
		} else {
			rc = fg_mem_masked_write(chip, CBITS_INPUT_FILTER_REG,
					IBATTF_TAU_MASK, IBATTF_TAU_99_S, 0);
			if (rc) {
				pr_err("Failed to write IF IBAT Tau: %d\n",
								rc);
				fg_mem_release(chip);
				fg_cap_learning_stop(chip);
				goto fail;
			}

			/* clear the i_filtered register */
			memset(data, 0, 4);
			rc = fg_mem_write(chip, data, I_FILTERED_REG, 3, 0, 0);
			if (rc) {
				pr_err("Failed to clear i_filtered: %d\n", rc);
				fg_mem_release(chip);
				fg_cap_learning_stop(chip);
				goto fail;
			}
			fg_mem_release(chip);
			chip->learning_data.time_stamp = ktime_get_boottime();
			chip->learning_data.active = true;

			if (fg_debug_mask & FG_AGING)
				pr_info("cap learning started, soc = %d cc_uah = %lld\n",
					battery_soc * 100 / FULL_PERCENT_3B,
					chip->learning_data.cc_uah);
			rc = alarm_start_relative(&chip->fg_cap_learning_alarm,
				ns_to_ktime(FG_CAP_LEARNING_INTERVAL_NS));
			if (rc) {
				pr_err("Failed to start alarm: %d\n", rc);
				fg_cap_learning_stop(chip);
				goto fail;
			}
		}
	} else if ((chip->status != POWER_SUPPLY_STATUS_CHARGING)
				&& chip->learning_data.active) {
		if (fg_debug_mask & FG_AGING)
			pr_info("capacity learning stopped\n");
		if (!(chip->wa_flag & USE_CC_SOC_REG))
			alarm_try_to_cancel(&chip->fg_cap_learning_alarm);

		if (chip->status == POWER_SUPPLY_STATUS_FULL) {
			if (chip->wa_flag & USE_CC_SOC_REG) {
				rc = fg_cap_learning_process_full_data(chip);
				if (rc) {
					fg_cap_learning_stop(chip);
					goto fail;
				}
				/* reset SW_CC_SOC register to 100% */
				rc = fg_mem_write(chip, (u8 *)&cc_pc_100,
					CC_SOC_BASE_REG, 4, CC_SOC_OFFSET, 0);
				if (rc)
					pr_err("Failed to reset CC_SOC_REG rc=%d\n",
									rc);
			}
			fg_cap_learning_post_process(chip);
		}

		fg_cap_learning_stop(chip);
	} else if (chip->status == POWER_SUPPLY_STATUS_FULL) {
		if (chip->wa_flag & USE_CC_SOC_REG) {
			/* reset SW_CC_SOC register to 100% upon charge_full */
			rc = fg_mem_write(chip, (u8 *)&cc_pc_100,
				CC_SOC_BASE_REG, 4, CC_SOC_OFFSET, 0);
			if (rc)
				pr_err("Failed to reset CC_SOC_REG rc=%d\n",
								rc);
			else if (fg_debug_mask & FG_STATUS)
				pr_info("Reset SW_CC_SOC to full value\n");
		}
	}

fail:
	mutex_unlock(&chip->learning_data.learning_lock);
	return rc;
}

static bool is_usb_present(struct fg_chip *chip)
{
	union power_supply_propval prop = {0,};
	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (chip->usb_psy)
		chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
	return prop.intval != 0;
}

static bool is_dc_present(struct fg_chip *chip)
{
	union power_supply_propval prop = {0,};
	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");

	if (chip->dc_psy)
		chip->dc_psy->get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
	return prop.intval != 0;
}

static bool is_input_present(struct fg_chip *chip)
{
	return is_usb_present(chip) || is_dc_present(chip);
}

static bool is_otg_present(struct fg_chip *chip)
{
	union power_supply_propval prop = {0,};

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (chip->usb_psy)
		chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_USB_OTG, &prop);
	return prop.intval != 0;
}

static bool is_charger_available(struct fg_chip *chip)
{
	if (!chip->batt_psy_name)
		return false;

	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name(chip->batt_psy_name);

	if (!chip->batt_psy)
		return false;

	return true;
}

static int set_prop_enable_charging(struct fg_chip *chip, bool enable)
{
	int rc = 0;
	union power_supply_propval ret = {enable, };

	if (!is_charger_available(chip)) {
		pr_err("Charger not available yet!\n");
		return -EINVAL;
	}

	rc = chip->batt_psy->set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
			&ret);
	if (rc) {
		pr_err("couldn't configure batt chg %d\n", rc);
		return rc;
	}

	chip->charging_disabled = !enable;
	if (fg_debug_mask & FG_STATUS)
		pr_info("%sabling charging\n", enable ? "en" : "dis");

	return rc;
}

#define MAX_BATTERY_CC_SOC_CAPACITY		150
static void status_change_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				status_change_work);
	unsigned long current_time = 0;
	int cc_soc, batt_soc, rc, capacity = get_prop_capacity(chip);
	bool batt_missing = is_battery_missing(chip);

	if (batt_missing) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("Battery is missing\n");
		return;
	}

	if (chip->esr_pulse_tune_en) {
		fg_stay_awake(&chip->esr_extract_wakeup_source);
		schedule_work(&chip->esr_extract_config_work);
	}

	if (chip->status == POWER_SUPPLY_STATUS_FULL) {
		if (capacity >= 99 && chip->hold_soc_while_full
				&& chip->health == POWER_SUPPLY_HEALTH_GOOD) {
			if (fg_debug_mask & FG_STATUS)
				pr_info("holding soc at 100\n");
			chip->charge_full = true;
		} else if (fg_debug_mask & FG_STATUS) {
			pr_info("terminated charging at %d/0x%02x\n",
					capacity, get_monotonic_soc_raw(chip));
		}
	}
	if (chip->status == POWER_SUPPLY_STATUS_FULL ||
			chip->status == POWER_SUPPLY_STATUS_CHARGING) {
		if (!chip->vbat_low_irq_enabled &&
				!chip->use_vbat_low_empty_soc) {
			enable_irq(chip->batt_irq[VBATT_LOW].irq);
			enable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
			chip->vbat_low_irq_enabled = true;
		}

		if (!chip->full_soc_irq_enabled) {
			enable_irq(chip->soc_irq[FULL_SOC].irq);
			enable_irq_wake(chip->soc_irq[FULL_SOC].irq);
			chip->full_soc_irq_enabled = true;
		}

		if (!!(chip->wa_flag & PULSE_REQUEST_WA) && capacity == 100)
			fg_configure_soc(chip);
	} else if (chip->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		if (chip->vbat_low_irq_enabled &&
				!chip->use_vbat_low_empty_soc) {
			disable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
			disable_irq_nosync(chip->batt_irq[VBATT_LOW].irq);
			chip->vbat_low_irq_enabled = false;
		}

		if (chip->full_soc_irq_enabled) {
			disable_irq_wake(chip->soc_irq[FULL_SOC].irq);
			disable_irq_nosync(chip->soc_irq[FULL_SOC].irq);
			chip->full_soc_irq_enabled = false;
		}
	}
	fg_cap_learning_check(chip);
	schedule_work(&chip->update_esr_work);

	if (chip->wa_flag & USE_CC_SOC_REG) {
		if (fg_get_cc_soc(chip, &cc_soc)) {
			pr_err("failed to get CC_SOC\n");
			return;
		}
	}

	if (chip->prev_status != chip->status && chip->last_sram_update_time) {
		/*
		 * Reset SW_CC_SOC to a value based off battery SOC when
		 * the device is discharging.
		 */
		if (chip->status == POWER_SUPPLY_STATUS_DISCHARGING) {
			batt_soc = get_battery_soc_raw(chip);
			if (!batt_soc)
				return;

			batt_soc = div64_s64((int64_t)batt_soc *
					FULL_PERCENT_28BIT, FULL_PERCENT_3B);
			rc = fg_mem_write(chip, (u8 *)&batt_soc,
				CC_SOC_BASE_REG, 4, CC_SOC_OFFSET, 0);
			if (rc)
				pr_err("Failed to reset CC_SOC_REG rc=%d\n",
									rc);
			else if (fg_debug_mask & FG_STATUS)
				pr_info("Reset SW_CC_SOC to %x\n", batt_soc);
		}

		/*
		 * Schedule the update_temp_work whenever there is a status
		 * change. This is essential for applying the slope limiter
		 * coefficients when that feature is enabled.
		 */
		if (chip->last_temp_update_time && chip->soc_slope_limiter_en) {
			cancel_delayed_work_sync(&chip->update_temp_work);
			schedule_delayed_work(&chip->update_temp_work,
				msecs_to_jiffies(0));
		}

		if (chip->dischg_gain.enable) {
			fg_stay_awake(&chip->dischg_gain_wakeup_source);
			schedule_work(&chip->dischg_gain_work);
		}

		get_current_time(&current_time);
		/*
		 * When charging status changes, update SRAM parameters if it
		 * was not updated before 5 seconds from now.
		 */
		if (chip->last_sram_update_time + 5 < current_time) {
			cancel_delayed_work(&chip->update_sram_data);
			schedule_delayed_work(&chip->update_sram_data,
				msecs_to_jiffies(0));
		}

		if (chip->cyc_ctr.en)
			schedule_work(&chip->cycle_count_work);

		if ((chip->wa_flag & USE_CC_SOC_REG) &&
				chip->bad_batt_detection_en &&
				chip->status == POWER_SUPPLY_STATUS_CHARGING) {
			chip->sw_cc_soc_data.init_sys_soc = capacity;
			chip->sw_cc_soc_data.init_cc_soc = cc_soc;
			if (fg_debug_mask & FG_STATUS)
				pr_info(" Init_sys_soc %d init_cc_soc %d\n",
					chip->sw_cc_soc_data.init_sys_soc,
					chip->sw_cc_soc_data.init_cc_soc);
		}
	}

	if ((chip->wa_flag & USE_CC_SOC_REG) && chip->bad_batt_detection_en
			&& chip->safety_timer_expired) {
		uint64_t delta_cc_soc = abs(cc_soc -
					chip->sw_cc_soc_data.init_cc_soc);
		chip->sw_cc_soc_data.delta_soc = DIV_ROUND_CLOSEST_ULL(
				delta_cc_soc * 100, FULL_PERCENT_28BIT);
		chip->sw_cc_soc_data.full_capacity =
			chip->sw_cc_soc_data.delta_soc +
			chip->sw_cc_soc_data.init_sys_soc;
		pr_info("Init_sys_soc %d init_cc_soc %d cc_soc %d delta_soc %d full_capacity %d\n",
				chip->sw_cc_soc_data.init_sys_soc,
				chip->sw_cc_soc_data.init_cc_soc, cc_soc,
				chip->sw_cc_soc_data.delta_soc,
				chip->sw_cc_soc_data.full_capacity);
		/*
		 * If sw_cc_soc capacity greater than 150, then it's a bad
		 * battery. else, reset timer and restart charging.
		 */
		if (chip->sw_cc_soc_data.full_capacity >
				MAX_BATTERY_CC_SOC_CAPACITY) {
			pr_info("Battery possibly damaged, do not restart charging\n");
		} else {
			pr_info("Reset safety-timer and restart charging\n");
			rc = set_prop_enable_charging(chip, false);
			if (rc) {
				pr_err("failed to disable charging %d\n", rc);
				return;
			}

			chip->safety_timer_expired = false;
			msleep(200);

			rc = set_prop_enable_charging(chip, true);
			if (rc) {
				pr_err("failed to enable charging %d\n", rc);
				return;
			}
		}
	}
}

/*
 * Check for change in the status of input or OTG and schedule
 * IADC gain compensation work.
 */
static void check_gain_compensation(struct fg_chip *chip)
{
	bool input_present = is_input_present(chip);
	bool otg_present = is_otg_present(chip);

	if ((chip->wa_flag & IADC_GAIN_COMP_WA)
		&& ((chip->input_present ^ input_present)
			|| (chip->otg_present ^ otg_present))) {
		fg_stay_awake(&chip->gain_comp_wakeup_source);
		chip->input_present = input_present;
		chip->otg_present = otg_present;
		cancel_work_sync(&chip->gain_comp_work);
		schedule_work(&chip->gain_comp_work);
	}
}

static void fg_hysteresis_config(struct fg_chip *chip)
{
	int hard_hot = 0, hard_cold = 0;

	hard_hot = get_prop_jeita_temp(chip, FG_MEM_HARD_HOT);
	hard_cold = get_prop_jeita_temp(chip, FG_MEM_HARD_COLD);
	if (chip->health == POWER_SUPPLY_HEALTH_OVERHEAT && !chip->batt_hot) {
		/* turn down the hard hot threshold */
		chip->batt_hot = true;
		set_prop_jeita_temp(chip, FG_MEM_HARD_HOT,
			hard_hot - chip->hot_hysteresis);
		if (fg_debug_mask & FG_STATUS)
			pr_info("hard hot hysteresis: old hot=%d, new hot=%d\n",
				hard_hot, hard_hot - chip->hot_hysteresis);
	} else if (chip->health == POWER_SUPPLY_HEALTH_COLD &&
		!chip->batt_cold) {
		/* turn up the hard cold threshold */
		chip->batt_cold = true;
		set_prop_jeita_temp(chip, FG_MEM_HARD_COLD,
			hard_cold + chip->cold_hysteresis);
		if (fg_debug_mask & FG_STATUS)
			pr_info("hard cold hysteresis: old cold=%d, new cold=%d\n",
				hard_cold, hard_cold + chip->hot_hysteresis);
	} else if (chip->health != POWER_SUPPLY_HEALTH_OVERHEAT &&
		chip->batt_hot) {
		/* restore the hard hot threshold */
		set_prop_jeita_temp(chip, FG_MEM_HARD_HOT,
			hard_hot + chip->hot_hysteresis);
		chip->batt_hot = !chip->batt_hot;
		if (fg_debug_mask & FG_STATUS)
			pr_info("restore hard hot threshold: old hot=%d, new hot=%d\n",
				hard_hot,
				hard_hot + chip->hot_hysteresis);
	} else if (chip->health != POWER_SUPPLY_HEALTH_COLD &&
		chip->batt_cold) {
		/* restore the hard cold threshold */
		set_prop_jeita_temp(chip, FG_MEM_HARD_COLD,
			hard_cold - chip->cold_hysteresis);
		chip->batt_cold = !chip->batt_cold;
		if (fg_debug_mask & FG_STATUS)
			pr_info("restore hard cold threshold: old cold=%d, new cold=%d\n",
				hard_cold,
				hard_cold - chip->cold_hysteresis);
	}
}

#define BATT_INFO_STS(base)	(base + 0x09)
#define JEITA_HARD_HOT_RT_STS	BIT(6)
#define JEITA_HARD_COLD_RT_STS	BIT(5)
static int fg_init_batt_temp_state(struct fg_chip *chip)
{
	int rc = 0;
	u8 batt_info_sts;
	int hard_hot = 0, hard_cold = 0;

	/*
	 * read the batt_info_sts register to parse battery's
	 * initial status and do hysteresis config accordingly.
	 */
	rc = fg_read(chip, &batt_info_sts,
		BATT_INFO_STS(chip->batt_base), 1);
	if (rc) {
		pr_err("failed to read batt info sts, rc=%d\n", rc);
		return rc;
	}

	hard_hot = get_prop_jeita_temp(chip, FG_MEM_HARD_HOT);
	hard_cold = get_prop_jeita_temp(chip, FG_MEM_HARD_COLD);
	chip->batt_hot =
		(batt_info_sts & JEITA_HARD_HOT_RT_STS) ? true : false;
	chip->batt_cold =
		(batt_info_sts & JEITA_HARD_COLD_RT_STS) ? true : false;
	if (chip->batt_hot || chip->batt_cold) {
		if (chip->batt_hot) {
			chip->health = POWER_SUPPLY_HEALTH_OVERHEAT;
			set_prop_jeita_temp(chip, FG_MEM_HARD_HOT,
				hard_hot - chip->hot_hysteresis);
		} else {
			chip->health = POWER_SUPPLY_HEALTH_COLD;
			set_prop_jeita_temp(chip, FG_MEM_HARD_COLD,
				hard_cold + chip->cold_hysteresis);
		}
	}

	return rc;
}

static int fg_restore_cc_soc(struct fg_chip *chip)
{
	int rc;

	if (!chip->use_last_cc_soc || !chip->last_cc_soc)
		return 0;

	if (fg_debug_mask & FG_STATUS)
		pr_info("Restoring cc_soc: %lld\n", chip->last_cc_soc);

	rc = fg_mem_write(chip, (u8 *)&chip->last_cc_soc,
			fg_data[FG_DATA_CC_CHARGE].address, 4,
			fg_data[FG_DATA_CC_CHARGE].offset, 0);
	if (rc)
		pr_err("failed to update CC_SOC rc=%d\n", rc);
	else
		chip->use_last_cc_soc = false;

	return rc;
}

#define SRAM_MONOTONIC_SOC_REG		0x574
#define SRAM_MONOTONIC_SOC_OFFSET	2
static int fg_restore_soc(struct fg_chip *chip)
{
	int rc;
	u16 msoc;

	if (chip->use_last_soc && chip->last_soc)
		msoc = DIV_ROUND_CLOSEST(chip->last_soc * 0xFFFF,
				FULL_SOC_RAW);
	else
		return 0;

	if (fg_debug_mask & FG_STATUS)
		pr_info("Restored soc: %d\n", msoc);

	rc = fg_mem_write(chip, (u8 *)&msoc, SRAM_MONOTONIC_SOC_REG, 2,
			SRAM_MONOTONIC_SOC_OFFSET, 0);
	if (rc)
		pr_err("failed to write M_SOC_REG rc=%d\n", rc);

	return rc;
}

#define NOM_CAP_REG			0x4F4
#define CAPACITY_DELTA_DECIPCT		500
static int load_battery_aging_data(struct fg_chip *chip)
{
	int rc = 0;
	u8 buffer[2];
	int16_t cc_mah;
	int64_t delta_cc_uah, pct_nom_cap_uah;

	rc = fg_mem_read(chip, buffer, NOM_CAP_REG, 2, 0, 0);
	if (rc) {
		pr_err("Failed to read nominal capacitance: %d\n", rc);
		goto out;
	}

	chip->nom_cap_uah = bcap_uah_2b(buffer);
	chip->actual_cap_uah = chip->nom_cap_uah;

	if (chip->learning_data.learned_cc_uah == 0) {
		chip->learning_data.learned_cc_uah = chip->nom_cap_uah;
		fg_cap_learning_save_data(chip);
	} else if (chip->learning_data.feedback_on) {
		delta_cc_uah = abs(chip->learning_data.learned_cc_uah -
					chip->nom_cap_uah);
		pct_nom_cap_uah = div64_s64((int64_t)chip->nom_cap_uah *
				CAPACITY_DELTA_DECIPCT, 1000);
		/*
		 * If the learned capacity is out of range, say by 50%
		 * from the nominal capacity, then overwrite the learned
		 * capacity with the nominal capacity.
		 */
		if (chip->nom_cap_uah && delta_cc_uah > pct_nom_cap_uah) {
			if (fg_debug_mask & FG_AGING) {
				pr_info("learned_cc_uah: %lld is higher than expected\n",
					chip->learning_data.learned_cc_uah);
				pr_info("Capping it to nominal:%d\n",
					chip->nom_cap_uah);
			}
			chip->learning_data.learned_cc_uah = chip->nom_cap_uah;
			fg_cap_learning_save_data(chip);
		} else {
			cc_mah = div64_s64(chip->learning_data.learned_cc_uah,
					1000);
			rc = fg_calc_and_store_cc_soc_coeff(chip, cc_mah);
			if (rc)
				pr_err("Error in restoring cc_soc_coeff, rc:%d\n",
					rc);
		}
	}
out:
	return rc;
}

static void fg_restore_battery_info(struct fg_chip *chip)
{
	int rc;
	char buf[4] = {0, 0, 0, 0};

	chip->last_soc = DIV_ROUND_CLOSEST(chip->batt_info[BATT_INFO_SOC] *
				FULL_SOC_RAW, FULL_CAPACITY);
	chip->last_cc_soc = div64_s64((int64_t)chip->last_soc *
				FULL_PERCENT_28BIT, FULL_SOC_RAW);
	chip->use_last_soc = true;
	chip->use_last_cc_soc = true;
	rc = fg_restore_soc(chip);
	if (rc) {
		pr_err("Error in restoring soc, rc=%d\n", rc);
		goto out;
	}

	rc = fg_restore_cc_soc(chip);
	if (rc) {
		pr_err("Error in restoring cc_soc, rc=%d\n", rc);
		goto out;
	}

	rc = fg_mem_write(chip, buf,
			fg_data[FG_DATA_VINT_ERR].address,
			fg_data[FG_DATA_VINT_ERR].len,
			fg_data[FG_DATA_VINT_ERR].offset, 0);
	if (rc) {
		pr_err("Failed to write to VINT_ERR, rc=%d\n", rc);
		goto out;
	}

	chip->learning_data.learned_cc_uah = chip->batt_info[BATT_INFO_FCC];
	rc = load_battery_aging_data(chip);
	if (rc) {
		pr_err("Failed to load battery aging data, rc:%d\n", rc);
		goto out;
	}

	if (chip->power_supply_registered)
		power_supply_changed(&chip->bms_psy);

	if (fg_debug_mask & FG_STATUS)
		pr_info("Restored battery info!\n");

out:
	return;
}

#define DELTA_BATT_TEMP		30
static bool fg_validate_battery_info(struct fg_chip *chip)
{
	int i, delta_pct, batt_id_kohm, batt_temp, batt_volt_mv, batt_soc;

	for (i = 1; i < BATT_INFO_MAX; i++) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("batt_info[%d]: %d\n", i, chip->batt_info[i]);

		if ((chip->batt_info[i] == 0 && i != BATT_INFO_TEMP) ||
			chip->batt_info[i] == INT_MAX) {
			if (fg_debug_mask & FG_STATUS)
				pr_info("batt_info[%d]:%d is invalid\n", i,
					chip->batt_info[i]);
			return false;
		}
	}

	batt_id_kohm = get_sram_prop_now(chip, FG_DATA_BATT_ID) / 1000;
	if (batt_id_kohm != chip->batt_info[BATT_INFO_RES_ID]) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("batt_id(%dK) does not match the stored batt_id(%dK)\n",
				batt_id_kohm,
				chip->batt_info[BATT_INFO_RES_ID]);
		return false;
	}

	batt_temp = get_sram_prop_now(chip, FG_DATA_BATT_TEMP);
	if (abs(chip->batt_info[BATT_INFO_TEMP] - batt_temp) >
			DELTA_BATT_TEMP) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("batt_temp(%d) is higher/lower than stored batt_temp(%d)\n",
				batt_temp, chip->batt_info[BATT_INFO_TEMP]);
		return false;
	}

	if (chip->batt_info[BATT_INFO_FCC] < 0) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("batt_fcc cannot be %d\n",
				chip->batt_info[BATT_INFO_FCC]);
		return false;
	}

	batt_volt_mv = get_sram_prop_now(chip, FG_DATA_VOLTAGE) / 1000;
	batt_soc = get_monotonic_soc_raw(chip);
	if (batt_soc != 0 && batt_soc != FULL_SOC_RAW)
		batt_soc = DIV_ROUND_CLOSEST((batt_soc - 1) *
				(FULL_CAPACITY - 2), FULL_SOC_RAW - 2) + 1;

	if (*chip->batt_range_ocv && chip->batt_max_voltage_uv > 1000)
		delta_pct =  DIV_ROUND_CLOSEST(abs(batt_volt_mv -
				chip->batt_info[BATT_INFO_VOLTAGE]) * 100,
				chip->batt_max_voltage_uv / 1000);
	else
		delta_pct = abs(batt_soc - chip->batt_info[BATT_INFO_SOC]);

	if (fg_debug_mask & FG_STATUS)
		pr_info("Validating by %s batt_voltage:%d capacity:%d delta_pct:%d\n",
			*chip->batt_range_ocv ? "OCV" : "SOC", batt_volt_mv,
			batt_soc, delta_pct);

	if (*chip->batt_range_pct && delta_pct > *chip->batt_range_pct) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("delta_pct(%d) is higher than batt_range_pct(%d)\n",
				delta_pct, *chip->batt_range_pct);
		return false;
	}

	return true;
}

static int fg_set_battery_info(struct fg_chip *chip, int val)
{
	if (chip->batt_info_id < 0 ||
			chip->batt_info_id >= BATT_INFO_MAX) {
		pr_err("Invalid batt_info_id %d\n", chip->batt_info_id);
		chip->batt_info_id = 0;
		return -EINVAL;
	}

	if (chip->batt_info_id == BATT_INFO_NOTIFY && val == INT_MAX - 1) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("Notified from userspace\n");
		if (chip->batt_info_restore && !chip->ima_error_handling) {
			if (!fg_validate_battery_info(chip)) {
				if (fg_debug_mask & FG_STATUS)
					pr_info("Validating battery info failed\n");
			} else {
				fg_restore_battery_info(chip);
			}
		}
	}

	chip->batt_info[chip->batt_info_id] = val;
	return 0;
}

static enum power_supply_property fg_power_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_NOW_RAW,
	POWER_SUPPLY_PROP_CHARGE_NOW_ERROR,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_COOL_TEMP,
	POWER_SUPPLY_PROP_WARM_TEMP,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_UPDATE_NOW,
	POWER_SUPPLY_PROP_ESR_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CYCLE_COUNT_ID,
	POWER_SUPPLY_PROP_HI_POWER,
	POWER_SUPPLY_PROP_SOC_REPORTING_READY,
	POWER_SUPPLY_PROP_IGNORE_FALSE_NEGATIVE_ISENSE,
	POWER_SUPPLY_PROP_ENABLE_JEITA_DETECTION,
	POWER_SUPPLY_PROP_BATTERY_INFO,
	POWER_SUPPLY_PROP_BATTERY_INFO_ID,
};

static int fg_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct fg_chip *chip = container_of(psy, struct fg_chip, bms_psy);
	bool vbatt_low_sts;

	switch (psp) {
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		if (chip->battery_missing)
			val->strval = missing_batt_type;
		else if (chip->fg_restarting)
			val->strval = loading_batt_type;
		else
			val->strval = chip->batt_type;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		val->intval = get_sram_prop_now(chip, FG_DATA_BATT_SOC);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW_ERROR:
		val->intval = get_sram_prop_now(chip, FG_DATA_VINT_ERR);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_sram_prop_now(chip, FG_DATA_CURRENT);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_sram_prop_now(chip, FG_DATA_VOLTAGE);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = get_sram_prop_now(chip, FG_DATA_OCV);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->batt_max_voltage_uv;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_sram_prop_now(chip, FG_DATA_BATT_TEMP);
		break;
	case POWER_SUPPLY_PROP_COOL_TEMP:
		val->intval = get_prop_jeita_temp(chip, FG_MEM_SOFT_COLD);
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		val->intval = get_prop_jeita_temp(chip, FG_MEM_SOFT_HOT);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = get_sram_prop_now(chip, FG_DATA_BATT_ESR);
		break;
	case POWER_SUPPLY_PROP_ESR_COUNT:
		val->intval = get_sram_prop_now(chip, FG_DATA_BATT_ESR_COUNT);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = fg_get_cycle_count(chip);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT_ID:
		val->intval = chip->cyc_ctr.id;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		val->intval = get_sram_prop_now(chip, FG_DATA_BATT_ID);
		break;
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		if (!fg_get_vbatt_status(chip, &vbatt_low_sts))
			val->intval = (int)vbatt_low_sts;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->nom_cap_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = chip->learning_data.learned_cc_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = chip->learning_data.cc_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW_RAW:
		val->intval = get_sram_prop_now(chip, FG_DATA_CC_CHARGE);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fg_get_current_cc(chip);
		break;
	case POWER_SUPPLY_PROP_HI_POWER:
		val->intval = !!chip->bcl_lpm_disabled;
		break;
	case POWER_SUPPLY_PROP_SOC_REPORTING_READY:
		val->intval = !!chip->soc_reporting_ready;
		break;
	case POWER_SUPPLY_PROP_IGNORE_FALSE_NEGATIVE_ISENSE:
		val->intval = !chip->allow_false_negative_isense;
		break;
	case POWER_SUPPLY_PROP_ENABLE_JEITA_DETECTION:
		val->intval = chip->use_soft_jeita_irq;
		break;
	case POWER_SUPPLY_PROP_BATTERY_INFO:
		if (chip->batt_info_id < 0 ||
				chip->batt_info_id >= BATT_INFO_MAX)
			return -EINVAL;
		val->intval = chip->batt_info[chip->batt_info_id];
		break;
	case POWER_SUPPLY_PROP_BATTERY_INFO_ID:
		val->intval = chip->batt_info_id;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct fg_chip *chip = container_of(psy, struct fg_chip, bms_psy);
	int rc = 0, unused;

	switch (psp) {
	case POWER_SUPPLY_PROP_COOL_TEMP:
		rc = set_prop_jeita_temp(chip, FG_MEM_SOFT_COLD, val->intval);
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		rc = set_prop_jeita_temp(chip, FG_MEM_SOFT_HOT, val->intval);
		break;
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		if (val->intval)
			update_sram_data(chip, &unused);
		break;
	case POWER_SUPPLY_PROP_IGNORE_FALSE_NEGATIVE_ISENSE:
		rc = set_prop_ignore_false_negative_isense(chip, !!val->intval);
		if (rc)
			pr_err("set_prop_ignore_false_negative_isense failed, rc=%d\n",
							rc);
		else
			chip->allow_false_negative_isense = !val->intval;
		break;
	case POWER_SUPPLY_PROP_ENABLE_JEITA_DETECTION:
		if (chip->use_soft_jeita_irq == !!val->intval) {
			pr_debug("JEITA irq %s, ignore!\n",
				chip->use_soft_jeita_irq ?
				"enabled" : "disabled");
			break;
		}
		chip->use_soft_jeita_irq = !!val->intval;
		if (chip->use_soft_jeita_irq) {
			if (chip->batt_irq[JEITA_SOFT_COLD].disabled) {
				enable_irq(
					chip->batt_irq[JEITA_SOFT_COLD].irq);
				chip->batt_irq[JEITA_SOFT_COLD].disabled =
								false;
			}
			if (!chip->batt_irq[JEITA_SOFT_COLD].wakeup) {
				enable_irq_wake(
					chip->batt_irq[JEITA_SOFT_COLD].irq);
				chip->batt_irq[JEITA_SOFT_COLD].wakeup = true;
			}
			if (chip->batt_irq[JEITA_SOFT_HOT].disabled) {
				enable_irq(
					chip->batt_irq[JEITA_SOFT_HOT].irq);
				chip->batt_irq[JEITA_SOFT_HOT].disabled = false;
			}
			if (!chip->batt_irq[JEITA_SOFT_HOT].wakeup) {
				enable_irq_wake(
					chip->batt_irq[JEITA_SOFT_HOT].irq);
				chip->batt_irq[JEITA_SOFT_HOT].wakeup = true;
			}
		} else {
			if (chip->batt_irq[JEITA_SOFT_COLD].wakeup) {
				disable_irq_wake(
					chip->batt_irq[JEITA_SOFT_COLD].irq);
				chip->batt_irq[JEITA_SOFT_COLD].wakeup = false;
			}
			if (!chip->batt_irq[JEITA_SOFT_COLD].disabled) {
				disable_irq_nosync(
					chip->batt_irq[JEITA_SOFT_COLD].irq);
				chip->batt_irq[JEITA_SOFT_COLD].disabled = true;
			}
			if (chip->batt_irq[JEITA_SOFT_HOT].wakeup) {
				disable_irq_wake(
					chip->batt_irq[JEITA_SOFT_HOT].irq);
				chip->batt_irq[JEITA_SOFT_HOT].wakeup = false;
			}
			if (!chip->batt_irq[JEITA_SOFT_HOT].disabled) {
				disable_irq_nosync(
					chip->batt_irq[JEITA_SOFT_HOT].irq);
				chip->batt_irq[JEITA_SOFT_HOT].disabled = true;
			}
		}
		break;
	case POWER_SUPPLY_PROP_STATUS:
		chip->prev_status = chip->status;
		chip->status = val->intval;
		schedule_work(&chip->status_change_work);
		check_gain_compensation(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		chip->health = val->intval;
		if (chip->health == POWER_SUPPLY_HEALTH_GOOD) {
			fg_stay_awake(&chip->resume_soc_wakeup_source);
			schedule_work(&chip->set_resume_soc_work);
		}

		if (chip->jeita_hysteresis_support)
			fg_hysteresis_config(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		chip->charge_done = val->intval;
		if (!chip->resume_soc_lowered) {
			fg_stay_awake(&chip->resume_soc_wakeup_source);
			schedule_work(&chip->set_resume_soc_work);
		}
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT_ID:
		if ((val->intval > 0) && (val->intval <= BUCKET_COUNT)) {
			chip->cyc_ctr.id = val->intval;
		} else {
			pr_err("rejecting invalid cycle_count_id = %d\n",
								val->intval);
			rc = -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_SAFETY_TIMER_EXPIRED:
		chip->safety_timer_expired = val->intval;
		schedule_work(&chip->status_change_work);
		break;
	case POWER_SUPPLY_PROP_HI_POWER:
		if (chip->wa_flag & BCL_HI_POWER_FOR_CHGLED_WA) {
			chip->bcl_lpm_disabled = !!val->intval;
			schedule_work(&chip->bcl_hi_power_work);
		}
		break;
	case POWER_SUPPLY_PROP_BATTERY_INFO:
		rc = fg_set_battery_info(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_BATTERY_INFO_ID:
		chip->batt_info_id = val->intval;
		break;
	default:
		return -EINVAL;
	};

	return rc;
};

static int fg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_COOL_TEMP:
	case POWER_SUPPLY_PROP_WARM_TEMP:
	case POWER_SUPPLY_PROP_CYCLE_COUNT_ID:
	case POWER_SUPPLY_PROP_BATTERY_INFO:
	case POWER_SUPPLY_PROP_BATTERY_INFO_ID:
		return 1;
	default:
		break;
	}

	return 0;
}

#define SRAM_DUMP_START		0x400
#define SRAM_DUMP_LEN		0x200
static void dump_sram(struct work_struct *work)
{
	int i, rc;
	u8 *buffer, rt_sts;
	char str[16];
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				dump_sram);

	buffer = devm_kzalloc(chip->dev, SRAM_DUMP_LEN, GFP_KERNEL);
	if (buffer == NULL) {
		pr_err("Can't allocate buffer\n");
		return;
	}

	rc = fg_read(chip, &rt_sts, INT_RT_STS(chip->soc_base), 1);
	if (rc)
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->soc_base), rc);
	else
		pr_info("soc rt_sts: 0x%x\n", rt_sts);

	rc = fg_read(chip, &rt_sts, INT_RT_STS(chip->batt_base), 1);
	if (rc)
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->batt_base), rc);
	else
		pr_info("batt rt_sts: 0x%x\n", rt_sts);

	rc = fg_read(chip, &rt_sts, INT_RT_STS(chip->mem_base), 1);
	if (rc)
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->mem_base), rc);
	else
		pr_info("memif rt_sts: 0x%x\n", rt_sts);

	rc = fg_mem_read(chip, buffer, SRAM_DUMP_START, SRAM_DUMP_LEN, 0, 0);
	if (rc) {
		pr_err("dump failed: rc = %d\n", rc);
		return;
	}

	for (i = 0; i < SRAM_DUMP_LEN; i += 4) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, buffer + i, 4);
		pr_info("%03X %s\n", SRAM_DUMP_START + i, str);
	}
	devm_kfree(chip->dev, buffer);
}

#define MAXRSCHANGE_REG		0x434
#define ESR_VALUE_OFFSET	1
#define ESR_STRICT_VALUE	0x4120391F391F3019
#define ESR_DEFAULT_VALUE	0x58CD4A6761C34A67
static void update_esr_value(struct work_struct *work)
{
	union power_supply_propval prop = {0, };
	u64 esr_value;
	int rc = 0;
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				update_esr_work);

	if (!is_charger_available(chip))
		return;

	chip->batt_psy->get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);

	if (!chip->esr_strict_filter) {
		if ((prop.intval == POWER_SUPPLY_CHARGE_TYPE_TAPER &&
				chip->status == POWER_SUPPLY_STATUS_CHARGING) ||
			(chip->status == POWER_SUPPLY_STATUS_FULL)) {
			esr_value = ESR_STRICT_VALUE;
			rc = fg_mem_write(chip, (u8 *)&esr_value,
					MAXRSCHANGE_REG, 8,
					ESR_VALUE_OFFSET, 0);
			if (rc)
				pr_err("failed to write strict ESR value rc=%d\n",
					rc);
			else
				chip->esr_strict_filter = true;
		}
	} else if ((prop.intval != POWER_SUPPLY_CHARGE_TYPE_TAPER &&
				chip->status == POWER_SUPPLY_STATUS_CHARGING) ||
			(chip->status == POWER_SUPPLY_STATUS_DISCHARGING)) {
		esr_value = ESR_DEFAULT_VALUE;
		rc = fg_mem_write(chip, (u8 *)&esr_value, MAXRSCHANGE_REG, 8,
				ESR_VALUE_OFFSET, 0);
		if (rc)
			pr_err("failed to write default ESR value rc=%d\n", rc);
		else
			chip->esr_strict_filter = false;
	}
}

#define TEMP_COUNTER_REG	0x580
#define VBAT_FILTERED_OFFSET	1
#define GAIN_REG		0x424
#define GAIN_OFFSET		1
#define K_VCOR_REG		0x484
#define DEF_GAIN_OFFSET		2
#define PICO_UNIT		0xE8D4A51000LL
#define ATTO_UNIT		0xDE0B6B3A7640000LL
#define VBAT_REF		3800000

/*
 * IADC Gain compensation steps:
 * If Input/OTG absent:
 *	- read VBAT_FILTERED, KVCOR, GAIN
 *	- calculate the gain compensation using following formula:
 *	  gain = (1 + gain) * (1 + kvcor * (vbat_filtered - 3800000)) - 1;
 * else
 *	- reset to the default gain compensation
 */
static void iadc_gain_comp_work(struct work_struct *work)
{
	u8 reg[4];
	int rc;
	uint64_t vbat_filtered;
	int64_t gain, kvcor, temp, numerator;
	struct fg_chip *chip = container_of(work, struct fg_chip,
							gain_comp_work);
	bool input_present = is_input_present(chip);
	bool otg_present = is_otg_present(chip);

	if (!chip->init_done)
		goto done;

	if (!input_present && !otg_present) {
		/* read VBAT_FILTERED */
		rc = fg_mem_read(chip, reg, TEMP_COUNTER_REG, 3,
						VBAT_FILTERED_OFFSET, 0);
		if (rc) {
			pr_err("Failed to read VBAT: rc=%d\n", rc);
			goto done;
		}
		temp = (reg[2] << 16) | (reg[1] << 8) | reg[0];
		vbat_filtered = div_u64((u64)temp * LSB_24B_NUMRTR,
						LSB_24B_DENMTR);

		/* read K_VCOR */
		rc = fg_mem_read(chip, reg, K_VCOR_REG, 2, 0, 0);
		if (rc) {
			pr_err("Failed to KVCOR rc=%d\n", rc);
			goto done;
		}
		kvcor = half_float(reg);

		/* calculate gain */
		numerator = (MICRO_UNIT + chip->iadc_comp_data.dfl_gain)
			* (PICO_UNIT + kvcor * (vbat_filtered - VBAT_REF))
			- ATTO_UNIT;
		gain = div64_s64(numerator, PICO_UNIT);

		/* write back gain */
		half_float_to_buffer(gain, reg);
		rc = fg_mem_write(chip, reg, GAIN_REG, 2, GAIN_OFFSET, 0);
		if (rc) {
			pr_err("Failed to write gain reg rc=%d\n", rc);
			goto done;
		}

		if (fg_debug_mask & FG_STATUS)
			pr_info("IADC gain update [%x %x]\n", reg[1], reg[0]);
		chip->iadc_comp_data.gain_active = true;
	} else {
		/* reset gain register */
		rc = fg_mem_write(chip, chip->iadc_comp_data.dfl_gain_reg,
						GAIN_REG, 2, GAIN_OFFSET, 0);
		if (rc) {
			pr_err("unable to write gain comp: %d\n", rc);
			goto done;
		}

		if (fg_debug_mask & FG_STATUS)
			pr_info("IADC gain reset [%x %x]\n",
					chip->iadc_comp_data.dfl_gain_reg[1],
					chip->iadc_comp_data.dfl_gain_reg[0]);
		chip->iadc_comp_data.gain_active = false;
	}

done:
	fg_relax(&chip->gain_comp_wakeup_source);
}

static void cc_soc_store_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work, struct fg_chip,
					cc_soc_store_work);
	int cc_soc_pct;

	if (!chip->nom_cap_uah) {
		pr_err("nom_cap_uah zero!\n");
		fg_relax(&chip->cc_soc_wakeup_source);
		return;
	}

	cc_soc_pct = get_sram_prop_now(chip, FG_DATA_CC_CHARGE);
	cc_soc_pct = div64_s64(cc_soc_pct * 100,
				chip->nom_cap_uah);
	chip->last_cc_soc = div64_s64((int64_t)chip->last_soc *
				FULL_PERCENT_28BIT, FULL_SOC_RAW);

	if (fg_debug_mask & FG_STATUS)
		pr_info("cc_soc_pct: %d last_cc_soc: %lld\n", cc_soc_pct,
			chip->last_cc_soc);

	if (fg_reset_on_lockup && (chip->cc_soc_limit_pct > 0 &&
			cc_soc_pct >= chip->cc_soc_limit_pct)) {
		pr_err("CC_SOC out of range\n");
		fg_check_ima_error_handling(chip);
	}

	fg_relax(&chip->cc_soc_wakeup_source);
}

#define HARD_JEITA_ALARM_CHECK_NS	10000000000
static enum alarmtimer_restart fg_hard_jeita_alarm_cb(struct alarm *alarm,
						ktime_t now)
{
	struct fg_chip *chip = container_of(alarm,
			struct fg_chip, hard_jeita_alarm);
	int rc, health = POWER_SUPPLY_HEALTH_UNKNOWN;
	u8 regval;
	bool batt_hot, batt_cold;
	union power_supply_propval val = {0, };

	if (!is_usb_present(chip)) {
		pr_debug("USB plugged out, stop the timer!\n");
		return ALARMTIMER_NORESTART;
	}

	rc = fg_read(chip, &regval, BATT_INFO_STS(chip->batt_base), 1);
	if (rc) {
		pr_err("read batt_sts failed, rc=%d\n", rc);
		goto recheck;
	}

	batt_hot = !!(regval & JEITA_HARD_HOT_RT_STS);
	batt_cold = !!(regval & JEITA_HARD_COLD_RT_STS);
	if (batt_hot && batt_cold) {
		pr_debug("Hot && cold can't co-exist\n");
		goto recheck;
	}

	if ((batt_hot == chip->batt_hot) && (batt_cold == chip->batt_cold)) {
		pr_debug("battery JEITA state not changed, ignore\n");
		goto recheck;
	}

	if (batt_cold != chip->batt_cold) {
		/* cool --> cold */
		if (chip->batt_cool) {
			chip->batt_cool = false;
			chip->batt_cold = true;
			health = POWER_SUPPLY_HEALTH_COLD;
		} else if (chip->batt_cold) { /* cold --> cool */
			chip->batt_cool = true;
			chip->batt_cold = false;
			health = POWER_SUPPLY_HEALTH_COOL;
		}
	}

	if (batt_hot != chip->batt_hot) {
		/* warm --> hot */
		if (chip->batt_warm) {
			chip->batt_warm = false;
			chip->batt_hot = true;
			health = POWER_SUPPLY_HEALTH_OVERHEAT;
		} else if (chip->batt_hot) { /* hot --> warm */
			chip->batt_hot = false;
			chip->batt_warm = true;
			health = POWER_SUPPLY_HEALTH_WARM;
		}
	}

	if (health != POWER_SUPPLY_HEALTH_UNKNOWN) {
		pr_debug("FG report battery health: %d\n", health);
		val.intval = health;
		rc = chip->batt_psy->set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_HEALTH, &val);
		if (rc)
			pr_err("Set batt_psy health: %d failed\n", health);
	}

recheck:
	alarm_forward_now(alarm, ns_to_ktime(HARD_JEITA_ALARM_CHECK_NS));
	return ALARMTIMER_RESTART;
}

#define BATT_SOFT_COLD_STS	BIT(0)
#define BATT_SOFT_HOT_STS	BIT(1)
static irqreturn_t fg_jeita_soft_hot_irq_handler(int irq, void *_chip)
{
	int rc;
	struct fg_chip *chip = _chip;
	u8 regval;
	bool batt_warm;
	union power_supply_propval val = {0, };

	if (!is_charger_available(chip))
		return IRQ_HANDLED;

	rc = fg_read(chip, &regval, INT_RT_STS(chip->batt_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->batt_base), rc);
		return IRQ_HANDLED;
	}

	batt_warm = !!(regval & BATT_SOFT_HOT_STS);
	if (chip->batt_warm == batt_warm) {
		pr_debug("warm state not change, ignore!\n");
		return IRQ_HANDLED;
	}

	chip->batt_warm = batt_warm;
	if (batt_warm) {
		val.intval = POWER_SUPPLY_HEALTH_WARM;
		chip->batt_psy->set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &val);
		/* kick the alarm timer for hard hot polling */
		rc = alarm_start_relative(&chip->hard_jeita_alarm,
				ns_to_ktime(HARD_JEITA_ALARM_CHECK_NS));
		if (rc)
			pr_err("start alarm for hard HOT detection failed, rc=%d\n",
									rc);
	} else {
		val.intval = POWER_SUPPLY_HEALTH_GOOD;
		chip->batt_psy->set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &val);
		/* cancel the alarm timer */
		alarm_try_to_cancel(&chip->hard_jeita_alarm);
	}

	return IRQ_HANDLED;
}

static irqreturn_t fg_jeita_soft_cold_irq_handler(int irq, void *_chip)
{
	int rc;
	struct fg_chip *chip = _chip;
	u8 regval;
	bool batt_cool;
	union power_supply_propval val = {0, };

	if (!is_charger_available(chip))
		return IRQ_HANDLED;

	rc = fg_read(chip, &regval, INT_RT_STS(chip->batt_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->batt_base), rc);
		return IRQ_HANDLED;
	}

	batt_cool = !!(regval & BATT_SOFT_COLD_STS);
	if (chip->batt_cool == batt_cool) {
		pr_debug("cool state not change, ignore\n");
		return IRQ_HANDLED;
	}

	chip->batt_cool = batt_cool;
	if (batt_cool) {
		val.intval = POWER_SUPPLY_HEALTH_COOL;
		chip->batt_psy->set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &val);
		/* kick the alarm timer for hard cold polling */
		rc = alarm_start_relative(&chip->hard_jeita_alarm,
				ns_to_ktime(HARD_JEITA_ALARM_CHECK_NS));
		if (rc)
			pr_err("start alarm for hard COLD detection failed, rc=%d\n",
									rc);
	} else {
		val.intval = POWER_SUPPLY_HEALTH_GOOD;
		chip->batt_psy->set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &val);
		/* cancel the alarm timer */
		alarm_try_to_cancel(&chip->hard_jeita_alarm);
	}

	return IRQ_HANDLED;
}

#define SOC_FIRST_EST_DONE	BIT(5)
static bool is_first_est_done(struct fg_chip *chip)
{
	int rc;
	u8 fg_soc_sts;

	rc = fg_read(chip, &fg_soc_sts,
				 INT_RT_STS(chip->soc_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->soc_base), rc);
		return false;
	}

	return (fg_soc_sts & SOC_FIRST_EST_DONE) ? true : false;
}

#define FG_EMPTY_DEBOUNCE_MS	1500
static irqreturn_t fg_vbatt_low_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;
	bool vbatt_low_sts;

	if (fg_debug_mask & FG_IRQS)
		pr_info("vbatt-low triggered\n");

	/* handle empty soc based on vbatt-low interrupt */
	if (chip->use_vbat_low_empty_soc) {
		if (fg_get_vbatt_status(chip, &vbatt_low_sts))
			goto out;

		if (vbatt_low_sts) {
			if (fg_debug_mask & FG_IRQS)
				pr_info("Vbatt is low\n");
			disable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
			disable_irq_nosync(chip->batt_irq[VBATT_LOW].irq);
			chip->vbat_low_irq_enabled = false;
			fg_stay_awake(&chip->empty_check_wakeup_source);
			schedule_delayed_work(&chip->check_empty_work,
				msecs_to_jiffies(FG_EMPTY_DEBOUNCE_MS));
		} else {
			if (fg_debug_mask & FG_IRQS)
				pr_info("Vbatt is high\n");
			chip->soc_empty = false;
		}
		goto out;
	}

	if (chip->status == POWER_SUPPLY_STATUS_CHARGING) {
		if (fg_get_vbatt_status(chip, &vbatt_low_sts))
			goto out;
		if (!vbatt_low_sts && chip->vbat_low_irq_enabled) {
			if (fg_debug_mask & FG_IRQS)
				pr_info("disabling vbatt_low irq\n");
			disable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
			disable_irq_nosync(chip->batt_irq[VBATT_LOW].irq);
			chip->vbat_low_irq_enabled = false;
		}
	}
	if (chip->power_supply_registered)
		power_supply_changed(&chip->bms_psy);
out:
	return IRQ_HANDLED;
}

static irqreturn_t fg_batt_missing_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;
	bool batt_missing = is_battery_missing(chip);

	if (batt_missing) {
		fg_cap_learning_stop(chip);
		chip->battery_missing = true;
		chip->profile_loaded = false;
		chip->soc_reporting_ready = false;
		chip->batt_type = default_batt_type;
		mutex_lock(&chip->cyc_ctr.lock);
		if (fg_debug_mask & FG_IRQS)
			pr_info("battery missing, clearing cycle counters\n");
		clear_cycle_counter(chip);
		mutex_unlock(&chip->cyc_ctr.lock);
	} else {
		if (!chip->use_otp_profile)
			fg_handle_battery_insertion(chip);
		else
			chip->battery_missing = false;
	}

	if (fg_debug_mask & FG_IRQS)
		pr_info("batt-missing triggered: %s\n",
				batt_missing ? "missing" : "present");

	if (chip->power_supply_registered)
		power_supply_changed(&chip->bms_psy);
	return IRQ_HANDLED;
}

static irqreturn_t fg_mem_avail_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;
	u8 mem_if_sts;
	int rc;

	rc = fg_read(chip, &mem_if_sts, INT_RT_STS(chip->mem_base), 1);
	if (rc) {
		pr_err("failed to read mem status rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	if (fg_check_sram_access(chip)) {
		if ((fg_debug_mask & FG_IRQS)
				& (FG_MEM_DEBUG_READS | FG_MEM_DEBUG_WRITES))
			pr_info("sram access granted\n");
		reinit_completion(&chip->sram_access_revoked);
		complete_all(&chip->sram_access_granted);
	} else {
		if ((fg_debug_mask & FG_IRQS)
				& (FG_MEM_DEBUG_READS | FG_MEM_DEBUG_WRITES))
			pr_info("sram access revoked\n");
		complete_all(&chip->sram_access_revoked);
	}

	if (!rc && (fg_debug_mask & FG_IRQS)
			& (FG_MEM_DEBUG_READS | FG_MEM_DEBUG_WRITES))
		pr_info("mem_if sts 0x%02x\n", mem_if_sts);

	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;
	u8 soc_rt_sts;
	int rc, msoc;

	rc = fg_read(chip, &soc_rt_sts, INT_RT_STS(chip->soc_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->soc_base), rc);
	}

	if (fg_debug_mask & FG_IRQS)
		pr_info("triggered 0x%x\n", soc_rt_sts);

	if (chip->dischg_gain.enable) {
		fg_stay_awake(&chip->dischg_gain_wakeup_source);
		schedule_work(&chip->dischg_gain_work);
	}

	if (chip->soc_slope_limiter_en) {
		fg_stay_awake(&chip->slope_limit_wakeup_source);
		schedule_work(&chip->slope_limiter_work);
	}

	/* Backup last soc every delta soc interrupt */
	chip->use_last_soc = false;
	if (fg_reset_on_lockup) {
		if (!chip->ima_error_handling)
			chip->last_soc = get_monotonic_soc_raw(chip);
		if (fg_debug_mask & FG_STATUS)
			pr_info("last_soc: %d\n", chip->last_soc);

		fg_stay_awake(&chip->cc_soc_wakeup_source);
		schedule_work(&chip->cc_soc_store_work);
	}

	if (chip->use_vbat_low_empty_soc) {
		msoc = get_monotonic_soc_raw(chip);
		if (msoc == 0 || chip->soc_empty) {
			fg_stay_awake(&chip->empty_check_wakeup_source);
			schedule_delayed_work(&chip->check_empty_work,
				msecs_to_jiffies(FG_EMPTY_DEBOUNCE_MS));
		}
	}

	schedule_work(&chip->battery_age_work);

	if (chip->power_supply_registered)
		power_supply_changed(&chip->bms_psy);

	if (chip->rslow_comp.chg_rs_to_rslow > 0 &&
			chip->rslow_comp.chg_rslow_comp_c1 > 0 &&
			chip->rslow_comp.chg_rslow_comp_c2 > 0)
		schedule_work(&chip->rslow_comp_work);

	if (chip->cyc_ctr.en)
		schedule_work(&chip->cycle_count_work);

	schedule_work(&chip->update_esr_work);

	if (chip->charge_full)
		schedule_work(&chip->charge_full_work);

	if (chip->wa_flag & IADC_GAIN_COMP_WA
			&& chip->iadc_comp_data.gain_active) {
		fg_stay_awake(&chip->gain_comp_wakeup_source);
		schedule_work(&chip->gain_comp_work);
	}

	if (chip->wa_flag & USE_CC_SOC_REG
			&& chip->learning_data.active) {
		fg_stay_awake(&chip->capacity_learning_wakeup_source);
		schedule_work(&chip->fg_cap_learning_work);
	}

	if (chip->esr_pulse_tune_en) {
		fg_stay_awake(&chip->esr_extract_wakeup_source);
		schedule_work(&chip->esr_extract_config_work);
	}

	return IRQ_HANDLED;
}

static irqreturn_t fg_empty_soc_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;
	u8 soc_rt_sts;
	int rc;

	rc = fg_read(chip, &soc_rt_sts, INT_RT_STS(chip->soc_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->soc_base), rc);
		goto done;
	}

	if (fg_debug_mask & FG_IRQS)
		pr_info("triggered 0x%x\n", soc_rt_sts);
	if (fg_is_batt_empty(chip)) {
		fg_stay_awake(&chip->empty_check_wakeup_source);
		schedule_delayed_work(&chip->check_empty_work,
			msecs_to_jiffies(FG_EMPTY_DEBOUNCE_MS));
	} else {
		chip->soc_empty = false;
	}

done:
	return IRQ_HANDLED;
}

static irqreturn_t fg_first_soc_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;

	if (fg_debug_mask & FG_IRQS)
		pr_info("triggered\n");

	if (fg_est_dump)
		schedule_work(&chip->dump_sram);

	if (chip->power_supply_registered)
		power_supply_changed(&chip->bms_psy);

	complete_all(&chip->first_soc_done);

	return IRQ_HANDLED;
}

static void fg_external_power_changed(struct power_supply *psy)
{
	struct fg_chip *chip = container_of(psy, struct fg_chip, bms_psy);
	bool input_present = is_input_present(chip);

	if (input_present ^ chip->rslow_comp.active &&
			chip->rslow_comp.chg_rs_to_rslow > 0 &&
			chip->rslow_comp.chg_rslow_comp_c1 > 0 &&
			chip->rslow_comp.chg_rslow_comp_c2 > 0)
		schedule_work(&chip->rslow_comp_work);
	if (!input_present && chip->resume_soc_lowered) {
		fg_stay_awake(&chip->resume_soc_wakeup_source);
		schedule_work(&chip->set_resume_soc_work);
	}
	if (!input_present && chip->charge_full)
		schedule_work(&chip->charge_full_work);
}

static void set_resume_soc_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				set_resume_soc_work);
	int rc, resume_soc_raw;

	if (is_input_present(chip) && !chip->resume_soc_lowered) {
		if (!chip->charge_done)
			goto done;
		resume_soc_raw = get_monotonic_soc_raw(chip)
			- (0xFF - settings[FG_MEM_RESUME_SOC].value);
		if (resume_soc_raw > 0 && resume_soc_raw < FULL_SOC_RAW) {
			rc = fg_set_resume_soc(chip, resume_soc_raw);
			if (rc) {
				pr_err("Couldn't set resume SOC for FG\n");
				goto done;
			}
			if (fg_debug_mask & FG_STATUS) {
				pr_info("resume soc lowered to 0x%02x\n",
						resume_soc_raw);
			}
		} else if (settings[FG_MEM_RESUME_SOC].value > 0) {
			pr_err("bad resume soc 0x%02x\n", resume_soc_raw);
		}
		chip->charge_done = false;
		chip->resume_soc_lowered = true;
	} else if (chip->resume_soc_lowered && (!is_input_present(chip)
				|| chip->health == POWER_SUPPLY_HEALTH_GOOD)) {
		resume_soc_raw = settings[FG_MEM_RESUME_SOC].value;
		if (resume_soc_raw > 0 && resume_soc_raw < FULL_SOC_RAW) {
			rc = fg_set_resume_soc(chip, resume_soc_raw);
			if (rc) {
				pr_err("Couldn't set resume SOC for FG\n");
				goto done;
			}
			if (fg_debug_mask & FG_STATUS) {
				pr_info("resume soc set to 0x%02x\n",
						resume_soc_raw);
			}
		} else if (settings[FG_MEM_RESUME_SOC].value > 0) {
			pr_err("bad resume soc 0x%02x\n", resume_soc_raw);
		}
		chip->resume_soc_lowered = false;
	}
done:
	fg_relax(&chip->resume_soc_wakeup_source);
}

#define OCV_COEFFS_START_REG		0x4C0
#define OCV_JUNCTION_REG		0x4D8
#define CUTOFF_VOLTAGE_REG		0x40C
#define RSLOW_CFG_REG			0x538
#define RSLOW_CFG_OFFSET		2
#define RSLOW_THRESH_REG		0x52C
#define RSLOW_THRESH_OFFSET		0
#define RS_TO_RSLOW_CHG_OFFSET		2
#define RS_TO_RSLOW_DISCHG_OFFSET	0
#define RSLOW_COMP_REG			0x528
#define RSLOW_COMP_C1_OFFSET		0
#define RSLOW_COMP_C2_OFFSET		2
#define BATT_PROFILE_OFFSET		0x4C0
static void get_default_rslow_comp_settings(struct fg_chip *chip)
{
	int offset;

	offset = RSLOW_CFG_REG + RSLOW_CFG_OFFSET - BATT_PROFILE_OFFSET;
	memcpy(&chip->rslow_comp.rslow_cfg, chip->batt_profile + offset, 1);

	offset = RSLOW_THRESH_REG + RSLOW_THRESH_OFFSET - BATT_PROFILE_OFFSET;
	memcpy(&chip->rslow_comp.rslow_thr, chip->batt_profile + offset, 1);

	offset = TEMP_RS_TO_RSLOW_REG + RS_TO_RSLOW_CHG_OFFSET -
		BATT_PROFILE_OFFSET;
	memcpy(&chip->rslow_comp.rs_to_rslow, chip->batt_profile + offset, 2);

	offset = RSLOW_COMP_REG + RSLOW_COMP_C1_OFFSET - BATT_PROFILE_OFFSET;
	memcpy(&chip->rslow_comp.rslow_comp, chip->batt_profile + offset, 4);
}

static int populate_system_data(struct fg_chip *chip)
{
	u8 buffer[24];
	int rc, i;

	fg_mem_lock(chip);
	rc = fg_mem_read(chip, buffer, OCV_COEFFS_START_REG, 24, 0, 0);
	if (rc) {
		pr_err("Failed to read ocv coefficients: %d\n", rc);
		goto done;
	}
	for (i = 0; i < 12; i += 1)
		chip->ocv_coeffs[i] = half_float(buffer + (i * 2));
	if (fg_debug_mask & FG_AGING) {
		pr_info("coeffs1 = %lld %lld %lld %lld\n",
				chip->ocv_coeffs[0], chip->ocv_coeffs[1],
				chip->ocv_coeffs[2], chip->ocv_coeffs[3]);
		pr_info("coeffs2 = %lld %lld %lld %lld\n",
				chip->ocv_coeffs[4], chip->ocv_coeffs[5],
				chip->ocv_coeffs[6], chip->ocv_coeffs[7]);
		pr_info("coeffs3 = %lld %lld %lld %lld\n",
				chip->ocv_coeffs[8], chip->ocv_coeffs[9],
				chip->ocv_coeffs[10], chip->ocv_coeffs[11]);
	}
	rc = fg_mem_read(chip, buffer, OCV_JUNCTION_REG, 2, 0, 0);
	if (rc) {
		pr_err("Failed to read ocv junctions: %d\n", rc);
		goto done;
	}

	chip->ocv_junction_p1p2 = buffer[0] * 100 / 255;
	chip->ocv_junction_p2p3 = buffer[1] * 100 / 255;

	rc = load_battery_aging_data(chip);
	if (rc) {
		pr_err("Failed to load battery aging data, rc:%d\n", rc);
		goto done;
	}

	rc = fg_mem_read(chip, buffer, CUTOFF_VOLTAGE_REG, 2, 0, 0);
	if (rc) {
		pr_err("Failed to read cutoff voltage: %d\n", rc);
		goto done;
	}

	chip->cutoff_voltage = voltage_2b(buffer);
	if (fg_debug_mask & FG_AGING)
		pr_info("cutoff_voltage = %lld, nom_cap_uah = %d p1p2 = %d, p2p3 = %d\n",
				chip->cutoff_voltage, chip->nom_cap_uah,
				chip->ocv_junction_p1p2,
				chip->ocv_junction_p2p3);

	get_default_rslow_comp_settings(chip);
done:
	fg_mem_release(chip);
	return rc;
}

static int fg_update_batt_rslow_settings(struct fg_chip *chip)
{
	int64_t rs_to_rslow_chg, rs_to_rslow_dischg, batt_esr, rconn_uohm;
	u8 buffer[2];
	int rc;

	rc = fg_mem_read(chip, buffer, BATTERY_ESR_REG, 2, ESR_OFFSET, 0);
	if (rc) {
		pr_err("unable to read battery_esr: %d\n", rc);
		goto done;
	}
	batt_esr = half_float(buffer);

	rc = fg_mem_read(chip, buffer, TEMP_RS_TO_RSLOW_REG, 2,
			RS_TO_RSLOW_DISCHG_OFFSET, 0);
	if (rc) {
		pr_err("unable to read rs to rslow dischg: %d\n", rc);
		goto done;
	}
	rs_to_rslow_dischg = half_float(buffer);

	rc = fg_mem_read(chip, buffer, TEMP_RS_TO_RSLOW_REG, 2,
			RS_TO_RSLOW_CHG_OFFSET, 0);
	if (rc) {
		pr_err("unable to read rs to rslow chg: %d\n", rc);
		goto done;
	}
	rs_to_rslow_chg = half_float(buffer);

	if (fg_debug_mask & FG_STATUS)
		pr_info("rs_rslow_chg: %lld, rs_rslow_dischg: %lld, esr: %lld\n",
			rs_to_rslow_chg, rs_to_rslow_dischg, batt_esr);

	rconn_uohm = chip->rconn_mohm * 1000;
	rs_to_rslow_dischg = div64_s64(rs_to_rslow_dischg * batt_esr,
					batt_esr + rconn_uohm);
	rs_to_rslow_chg = div64_s64(rs_to_rslow_chg * batt_esr,
					batt_esr + rconn_uohm);

	half_float_to_buffer(rs_to_rslow_chg, buffer);
	rc = fg_mem_write(chip, buffer, TEMP_RS_TO_RSLOW_REG, 2,
			RS_TO_RSLOW_CHG_OFFSET, 0);
	if (rc) {
		pr_err("unable to write rs_to_rslow_chg: %d\n", rc);
		goto done;
	}

	half_float_to_buffer(rs_to_rslow_dischg, buffer);
	rc = fg_mem_write(chip, buffer, TEMP_RS_TO_RSLOW_REG, 2,
			RS_TO_RSLOW_DISCHG_OFFSET, 0);
	if (rc) {
		pr_err("unable to write rs_to_rslow_dischg: %d\n", rc);
		goto done;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("Modified rs_rslow_chg: %lld, rs_rslow_dischg: %lld\n",
			rs_to_rslow_chg, rs_to_rslow_dischg);
done:
	return rc;
}

#define RSLOW_CFG_MASK		(BIT(2) | BIT(3) | BIT(4) | BIT(5))
#define RSLOW_CFG_ON_VAL	(BIT(2) | BIT(3))
#define RSLOW_THRESH_FULL_VAL	0xFF
static int fg_rslow_charge_comp_set(struct fg_chip *chip)
{
	int rc;
	u8 buffer[2];

	mutex_lock(&chip->rslow_comp.lock);
	fg_mem_lock(chip);

	rc = fg_mem_masked_write(chip, RSLOW_CFG_REG,
			RSLOW_CFG_MASK, RSLOW_CFG_ON_VAL, RSLOW_CFG_OFFSET);
	if (rc) {
		pr_err("unable to write rslow cfg: %d\n", rc);
		goto done;
	}
	rc = fg_mem_masked_write(chip, RSLOW_THRESH_REG,
			0xFF, RSLOW_THRESH_FULL_VAL, RSLOW_THRESH_OFFSET);
	if (rc) {
		pr_err("unable to write rslow thresh: %d\n", rc);
		goto done;
	}

	half_float_to_buffer(chip->rslow_comp.chg_rs_to_rslow, buffer);
	rc = fg_mem_write(chip, buffer,
			TEMP_RS_TO_RSLOW_REG, 2, RS_TO_RSLOW_CHG_OFFSET, 0);
	if (rc) {
		pr_err("unable to write rs to rslow: %d\n", rc);
		goto done;
	}
	half_float_to_buffer(chip->rslow_comp.chg_rslow_comp_c1, buffer);
	rc = fg_mem_write(chip, buffer,
			RSLOW_COMP_REG, 2, RSLOW_COMP_C1_OFFSET, 0);
	if (rc) {
		pr_err("unable to write rslow comp: %d\n", rc);
		goto done;
	}
	half_float_to_buffer(chip->rslow_comp.chg_rslow_comp_c2, buffer);
	rc = fg_mem_write(chip, buffer,
			RSLOW_COMP_REG, 2, RSLOW_COMP_C2_OFFSET, 0);
	if (rc) {
		pr_err("unable to write rslow comp: %d\n", rc);
		goto done;
	}
	chip->rslow_comp.active = true;
	if (fg_debug_mask & FG_STATUS)
		pr_info("Activated rslow charge comp values\n");

done:
	fg_mem_release(chip);
	mutex_unlock(&chip->rslow_comp.lock);
	return rc;
}

#define RSLOW_CFG_ORIG_MASK	(BIT(4) | BIT(5))
static int fg_rslow_charge_comp_clear(struct fg_chip *chip)
{
	u8 reg;
	int rc;

	mutex_lock(&chip->rslow_comp.lock);
	fg_mem_lock(chip);

	reg = chip->rslow_comp.rslow_cfg & RSLOW_CFG_ORIG_MASK;
	rc = fg_mem_masked_write(chip, RSLOW_CFG_REG,
			RSLOW_CFG_MASK, reg, RSLOW_CFG_OFFSET);
	if (rc) {
		pr_err("unable to write rslow cfg: %d\n", rc);
		goto done;
	}
	rc = fg_mem_masked_write(chip, RSLOW_THRESH_REG,
			0xFF, chip->rslow_comp.rslow_thr, RSLOW_THRESH_OFFSET);
	if (rc) {
		pr_err("unable to write rslow thresh: %d\n", rc);
		goto done;
	}

	rc = fg_mem_write(chip, chip->rslow_comp.rs_to_rslow,
			TEMP_RS_TO_RSLOW_REG, 2, RS_TO_RSLOW_CHG_OFFSET, 0);
	if (rc) {
		pr_err("unable to write rs to rslow: %d\n", rc);
		goto done;
	}
	rc = fg_mem_write(chip, chip->rslow_comp.rslow_comp,
			RSLOW_COMP_REG, 4, RSLOW_COMP_C1_OFFSET, 0);
	if (rc) {
		pr_err("unable to write rslow comp: %d\n", rc);
		goto done;
	}
	chip->rslow_comp.active = false;
	if (fg_debug_mask & FG_STATUS)
		pr_info("Cleared rslow charge comp values\n");

done:
	fg_mem_release(chip);
	mutex_unlock(&chip->rslow_comp.lock);
	return rc;
}

static void rslow_comp_work(struct work_struct *work)
{
	int battery_soc_1b;
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				rslow_comp_work);

	battery_soc_1b = get_battery_soc_raw(chip) >> 16;
	if (battery_soc_1b > chip->rslow_comp.chg_rslow_comp_thr
			&& chip->status == POWER_SUPPLY_STATUS_CHARGING) {
		if (!chip->rslow_comp.active)
			fg_rslow_charge_comp_set(chip);
	} else {
		if (chip->rslow_comp.active)
			fg_rslow_charge_comp_clear(chip);
	}
}

#define MICROUNITS_TO_ADC_RAW(units)	\
			div64_s64(units * LSB_16B_DENMTR, LSB_16B_NUMRTR)
static int update_chg_iterm(struct fg_chip *chip)
{
	u8 data[2];
	u16 converted_current_raw;
	s64 current_ma = -settings[FG_MEM_CHG_TERM_CURRENT].value;

	converted_current_raw = (s16)MICROUNITS_TO_ADC_RAW(current_ma * 1000);
	data[0] = cpu_to_le16(converted_current_raw) & 0xFF;
	data[1] = cpu_to_le16(converted_current_raw) >> 8;

	if (fg_debug_mask & FG_STATUS)
		pr_info("current = %lld, converted_raw = %04x, data = %02x %02x\n",
			current_ma, converted_current_raw, data[0], data[1]);
	return fg_mem_write(chip, data,
			settings[FG_MEM_CHG_TERM_CURRENT].address,
			2, settings[FG_MEM_CHG_TERM_CURRENT].offset, 0);
}

#define CC_CV_SETPOINT_REG	0x4F8
#define CC_CV_SETPOINT_OFFSET	0
static void update_cc_cv_setpoint(struct fg_chip *chip)
{
	int rc;
	u8 tmp[2];

	if (!chip->cc_cv_threshold_mv)
		return;
	batt_to_setpoint_adc(chip->cc_cv_threshold_mv, tmp);
	rc = fg_mem_write(chip, tmp, CC_CV_SETPOINT_REG, 2,
				CC_CV_SETPOINT_OFFSET, 0);
	if (rc) {
		pr_err("failed to write CC_CV_VOLT rc=%d\n", rc);
		return;
	}
	if (fg_debug_mask & FG_STATUS)
		pr_info("Wrote %x %x to address %x for CC_CV setpoint\n",
			tmp[0], tmp[1], CC_CV_SETPOINT_REG);
}

#define CBITS_INPUT_FILTER_REG		0x4B4
#define CBITS_RMEAS1_OFFSET		1
#define CBITS_RMEAS2_OFFSET		2
#define CBITS_RMEAS1_DEFAULT_VAL	0x65
#define CBITS_RMEAS2_DEFAULT_VAL	0x65
#define IMPTR_FAST_TIME_SHIFT		1
#define IMPTR_LONG_TIME_SHIFT		(1 << 4)
#define IMPTR_PULSE_CTR_CHG		1
#define IMPTR_PULSE_CTR_DISCHG		(1 << 4)
static int fg_config_imptr_pulse(struct fg_chip *chip, bool slow)
{
	int rc;
	u8 cntr[2] = {0, 0};
	u8 val;

	if (slow == chip->imptr_pulse_slow_en) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("imptr_pulse_slow is %sabled already\n",
				slow ? "en" : "dis");
		return 0;
	}

	fg_mem_lock(chip);

	val = slow ? (IMPTR_FAST_TIME_SHIFT | IMPTR_LONG_TIME_SHIFT) :
		CBITS_RMEAS1_DEFAULT_VAL;
	rc = fg_mem_write(chip, &val, CBITS_INPUT_FILTER_REG, 1,
			CBITS_RMEAS1_OFFSET, 0);
	if (rc) {
		pr_err("unable to write cbits_rmeas1_offset rc=%d\n", rc);
		goto done;
	}

	val = slow ? (IMPTR_PULSE_CTR_CHG | IMPTR_PULSE_CTR_DISCHG) :
		CBITS_RMEAS2_DEFAULT_VAL;
	rc = fg_mem_write(chip, &val, CBITS_INPUT_FILTER_REG, 1,
			CBITS_RMEAS2_OFFSET, 0);
	if (rc) {
		pr_err("unable to write cbits_rmeas2_offset rc=%d\n", rc);
		goto done;
	}

	if (slow) {
		rc = fg_mem_write(chip, cntr, COUNTER_IMPTR_REG, 4,
				COUNTER_IMPTR_OFFSET, 0);
		if (rc) {
			pr_err("failed to write COUNTER_IMPTR rc=%d\n", rc);
			goto done;
		}

		rc = fg_mem_write(chip, cntr, COUNTER_PULSE_REG, 2,
				COUNTER_PULSE_OFFSET, 0);
		if (rc) {
			pr_err("failed to write COUNTER_IMPTR rc=%d\n", rc);
			goto done;
		}
	}

	chip->imptr_pulse_slow_en = slow;
	if (fg_debug_mask & FG_STATUS)
		pr_info("imptr_pulse_slow is %sabled\n", slow ? "en" : "dis");
done:
	fg_mem_release(chip);
	return rc;
}

#define CURRENT_DELTA_MIN_REG		0x42C
#define CURRENT_DELTA_MIN_OFFSET	1
#define SYS_CFG_1_REG			0x4AC
#define SYS_CFG_1_OFFSET		0
#define CURRENT_DELTA_MIN_DEFAULT	0x16
#define CURRENT_DELTA_MIN_500MA		0xCD
#define RSLOW_CFG_USE_FIX_RSER_VAL	BIT(7)
#define ENABLE_ESR_PULSE_VAL		BIT(3)
static int fg_config_esr_extract(struct fg_chip *chip, bool disable)
{
	int rc;
	u8 val;

	if (disable == chip->esr_extract_disabled) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("ESR extract already %sabled\n",
				disable ? "dis" : "en");
		return 0;
	}

	fg_mem_lock(chip);

	val = disable ? CURRENT_DELTA_MIN_500MA :
				CURRENT_DELTA_MIN_DEFAULT;
	rc = fg_mem_write(chip, &val, CURRENT_DELTA_MIN_REG, 1,
			CURRENT_DELTA_MIN_OFFSET, 0);
	if (rc) {
		pr_err("unable to write curr_delta_min rc=%d\n", rc);
		goto done;
	}

	val = disable ? RSLOW_CFG_USE_FIX_RSER_VAL : 0;
	rc = fg_mem_masked_write(chip, RSLOW_CFG_REG,
			RSLOW_CFG_USE_FIX_RSER_VAL, val, RSLOW_CFG_OFFSET);
	if (rc) {
		pr_err("unable to write rslow cfg rc= %d\n", rc);
		goto done;
	}

	val = disable ? 0 : ENABLE_ESR_PULSE_VAL;
	rc = fg_mem_masked_write(chip, SYS_CFG_1_REG,
			ENABLE_ESR_PULSE_VAL, val, SYS_CFG_1_OFFSET);
	if (rc) {
		pr_err("unable to write sys_cfg_1 rc= %d\n", rc);
		goto done;
	}

	chip->esr_extract_disabled = disable;
	if (fg_debug_mask & FG_STATUS)
		pr_info("ESR extract is %sabled\n", disable ? "dis" : "en");
done:
	fg_mem_release(chip);
	return rc;
}

#define ESR_EXTRACT_STOP_SOC		2
#define IMPTR_PULSE_CONFIG_SOC		5
static void esr_extract_config_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work, struct fg_chip,
						esr_extract_config_work);
	bool input_present = is_input_present(chip);
	int capacity = get_prop_capacity(chip);

	if (input_present && capacity <= ESR_EXTRACT_STOP_SOC) {
		fg_config_esr_extract(chip, true);
	} else if (capacity > ESR_EXTRACT_STOP_SOC) {
		fg_config_esr_extract(chip, false);

		if (capacity <= IMPTR_PULSE_CONFIG_SOC)
			fg_config_imptr_pulse(chip, true);
		else
			fg_config_imptr_pulse(chip, false);
	}

	fg_relax(&chip->esr_extract_wakeup_source);
}

#define KI_COEFF_MEDC_REG		0x400
#define KI_COEFF_MEDC_OFFSET		0
#define KI_COEFF_HIGHC_REG		0x404
#define KI_COEFF_HIGHC_OFFSET		0
#define DEFAULT_MEDC_VOLTAGE_GAIN	3
#define DEFAULT_HIGHC_VOLTAGE_GAIN	2
static void discharge_gain_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work, struct fg_chip,
						dischg_gain_work);
	u8 buf[2];
	int capacity, rc, i;
	int64_t medc_val = DEFAULT_MEDC_VOLTAGE_GAIN;
	int64_t highc_val = DEFAULT_HIGHC_VOLTAGE_GAIN;

	capacity = get_prop_capacity(chip);
	if (chip->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		for (i = VOLT_GAIN_MAX - 1; i >= 0; i--) {
			if (capacity <= chip->dischg_gain.soc[i]) {
				medc_val = chip->dischg_gain.medc_gain[i];
				highc_val = chip->dischg_gain.highc_gain[i];
			}
		}
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("Capacity: %d, medc_gain: %lld highc_gain: %lld\n",
			capacity, medc_val, highc_val);

	medc_val *= MICRO_UNIT;
	half_float_to_buffer(medc_val, buf);
	rc = fg_mem_write(chip, buf, KI_COEFF_MEDC_REG, 2,
				KI_COEFF_MEDC_OFFSET, 0);
	if (rc)
		pr_err("Couldn't write to ki_coeff_medc_reg, rc=%d\n", rc);
	else if (fg_debug_mask & FG_STATUS)
		pr_info("Value [%x %x] written to ki_coeff_medc\n", buf[0],
			buf[1]);

	highc_val *= MICRO_UNIT;
	half_float_to_buffer(highc_val, buf);
	rc = fg_mem_write(chip, buf, KI_COEFF_HIGHC_REG, 2,
				KI_COEFF_HIGHC_OFFSET, 0);
	if (rc)
		pr_err("Couldn't write to ki_coeff_highc_reg, rc=%d\n", rc);
	else if (fg_debug_mask & FG_STATUS)
		pr_info("Value [%x %x] written to ki_coeff_highc\n", buf[0],
			buf[1]);

	fg_relax(&chip->dischg_gain_wakeup_source);
}

#define LOW_LATENCY			BIT(6)
#define PROFILE_INTEGRITY_REG		0x53C
#define PROFILE_INTEGRITY_BIT		BIT(0)
#define FIRST_EST_DONE_BIT		BIT(5)
#define MAX_TRIES_FIRST_EST		3
#define FIRST_EST_WAIT_MS		2000
#define PROFILE_LOAD_TIMEOUT_MS		5000
static int fg_do_restart(struct fg_chip *chip, bool write_profile)
{
	int rc, ibat_ua;
	u8 reg = 0;
	u8 buf[2];
	bool tried_once = false;

	if (fg_debug_mask & FG_STATUS)
		pr_info("restarting fuel gauge...\n");

try_again:
	if (write_profile && !chip->ima_error_handling) {
		if (!chip->charging_disabled) {
			pr_err("Charging not yet disabled!\n");
			return -EINVAL;
		}

		ibat_ua = get_sram_prop_now(chip, FG_DATA_CURRENT);
		if (ibat_ua == -EINVAL) {
			pr_err("SRAM not updated yet!\n");
			return ibat_ua;
		}

		if (ibat_ua < 0) {
			pr_warn("Charging enabled?, ibat_ua: %d\n", ibat_ua);

			if (!tried_once) {
				cancel_delayed_work(&chip->update_sram_data);
				schedule_delayed_work(&chip->update_sram_data,
					msecs_to_jiffies(0));
				msleep(1000);
				tried_once = true;
				goto try_again;
			}
		}
	}

	chip->fg_restarting = true;
	/*
	 * save the temperature if the sw rbias control is active so that there
	 * is no gap of time when there is no valid temperature read after the
	 * restart
	 */
	if (chip->sw_rbias_ctrl) {
		rc = fg_mem_read(chip, buf,
				fg_data[FG_DATA_BATT_TEMP].address,
				fg_data[FG_DATA_BATT_TEMP].len,
				fg_data[FG_DATA_BATT_TEMP].offset, 0);
		if (rc) {
			pr_err("failed to read batt temp rc=%d\n", rc);
			goto sub_and_fail;
		}
	}
	/*
	 * release the sram access and configure the correct settings
	 * before re-requesting access.
	 */
	mutex_lock(&chip->rw_lock);
	fg_release_access(chip);

	rc = fg_masked_write(chip, chip->soc_base + SOC_BOOT_MOD,
			NO_OTP_PROF_RELOAD, 0, 1);
	if (rc) {
		pr_err("failed to set no otp reload bit\n");
		goto unlock_and_fail;
	}

	/* unset the restart bits so the fg doesn't continuously restart */
	reg = REDO_FIRST_ESTIMATE | RESTART_GO;
	rc = fg_masked_write(chip, chip->soc_base + SOC_RESTART,
			reg, 0, 1);
	if (rc) {
		pr_err("failed to unset fg restart: %d\n", rc);
		goto unlock_and_fail;
	}

	rc = fg_masked_write(chip, MEM_INTF_CFG(chip),
			LOW_LATENCY, LOW_LATENCY, 1);
	if (rc) {
		pr_err("failed to set low latency access bit\n");
		goto unlock_and_fail;
	}
	mutex_unlock(&chip->rw_lock);

	/* read once to get a fg cycle in */
	rc = fg_mem_read(chip, &reg, PROFILE_INTEGRITY_REG, 1, 0, 0);
	if (rc) {
		pr_err("failed to read profile integrity rc=%d\n", rc);
		goto fail;
	}

	/*
	 * If this is not the first time a profile has been loaded, sleep for
	 * 3 seconds to make sure the NO_OTP_RELOAD is cleared in memory
	 */
	if (chip->first_profile_loaded)
		msleep(3000);

	mutex_lock(&chip->rw_lock);
	fg_release_access(chip);
	rc = fg_masked_write(chip, MEM_INTF_CFG(chip), LOW_LATENCY, 0, 1);
	if (rc) {
		pr_err("failed to set low latency access bit\n");
		goto unlock_and_fail;
	}

	atomic_add_return(1, &chip->memif_user_cnt);
	mutex_unlock(&chip->rw_lock);

	if (write_profile) {
		/* write the battery profile */
		rc = fg_mem_write(chip, chip->batt_profile, BATT_PROFILE_OFFSET,
				chip->batt_profile_len, 0, 1);
		if (rc) {
			pr_err("failed to write profile rc=%d\n", rc);
			goto sub_and_fail;
		}
		/* write the integrity bits and release access */
		rc = fg_mem_masked_write(chip, PROFILE_INTEGRITY_REG,
				PROFILE_INTEGRITY_BIT,
				PROFILE_INTEGRITY_BIT, 0);
		if (rc) {
			pr_err("failed to write profile rc=%d\n", rc);
			goto sub_and_fail;
		}
	}

	/* decrement the user count so that memory access can be released */
	fg_release_access_if_necessary(chip);

	/*
	 * make sure that the first estimate has completed
	 * in case of a hotswap
	 */
	rc = wait_for_completion_interruptible_timeout(&chip->first_soc_done,
			msecs_to_jiffies(PROFILE_LOAD_TIMEOUT_MS));
	if (rc <= 0) {
		pr_err("transaction timed out rc=%d\n", rc);
		rc = -ETIMEDOUT;
		goto fail;
	}

	/*
	 * reinitialize the completion so that the driver knows when the restart
	 * finishes
	 */
	reinit_completion(&chip->first_soc_done);

	if (chip->esr_pulse_tune_en) {
		fg_stay_awake(&chip->esr_extract_wakeup_source);
		schedule_work(&chip->esr_extract_config_work);
	}

	/*
	 * set the restart bits so that the next fg cycle will not reload
	 * the profile
	 */
	rc = fg_masked_write(chip, chip->soc_base + SOC_BOOT_MOD,
			NO_OTP_PROF_RELOAD, NO_OTP_PROF_RELOAD, 1);
	if (rc) {
		pr_err("failed to set no otp reload bit\n");
		goto fail;
	}

	reg = REDO_FIRST_ESTIMATE | RESTART_GO;
	rc = fg_masked_write(chip, chip->soc_base + SOC_RESTART,
			reg, reg, 1);
	if (rc) {
		pr_err("failed to set fg restart: %d\n", rc);
		goto fail;
	}

	/* wait for the first estimate to complete */
	rc = wait_for_completion_interruptible_timeout(&chip->first_soc_done,
			msecs_to_jiffies(PROFILE_LOAD_TIMEOUT_MS));
	if (rc <= 0) {
		pr_err("transaction timed out rc=%d\n", rc);
		rc = -ETIMEDOUT;
		goto fail;
	}
	rc = fg_read(chip, &reg, INT_RT_STS(chip->soc_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->soc_base), rc);
		goto fail;
	}
	if ((reg & FIRST_EST_DONE_BIT) == 0)
		pr_err("Battery profile reloading failed, no first estimate\n");

	rc = fg_masked_write(chip, chip->soc_base + SOC_BOOT_MOD,
			NO_OTP_PROF_RELOAD, 0, 1);
	if (rc) {
		pr_err("failed to set no otp reload bit\n");
		goto fail;
	}
	/* unset the restart bits so the fg doesn't continuously restart */
	reg = REDO_FIRST_ESTIMATE | RESTART_GO;
	rc = fg_masked_write(chip, chip->soc_base + SOC_RESTART,
			reg, 0, 1);
	if (rc) {
		pr_err("failed to unset fg restart: %d\n", rc);
		goto fail;
	}

	/* restore the battery temperature reading here */
	if (chip->sw_rbias_ctrl) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("reloaded 0x%02x%02x into batt temp",
					buf[0], buf[1]);
		rc = fg_mem_write(chip, buf,
				fg_data[FG_DATA_BATT_TEMP].address,
				fg_data[FG_DATA_BATT_TEMP].len,
				fg_data[FG_DATA_BATT_TEMP].offset, 0);
		if (rc) {
			pr_err("failed to write batt temp rc=%d\n", rc);
			goto fail;
		}
	}

	/* Enable charging now as the first estimate is done now */
	if (chip->charging_disabled) {
		rc = set_prop_enable_charging(chip, true);
		if (rc)
			pr_err("Failed to enable charging, rc=%d\n", rc);
		else
			chip->charging_disabled = false;
	}

	chip->fg_restarting = false;

	if (fg_debug_mask & FG_STATUS)
		pr_info("done!\n");
	return 0;

unlock_and_fail:
	mutex_unlock(&chip->rw_lock);
	goto fail;
sub_and_fail:
	fg_release_access_if_necessary(chip);
	goto fail;
fail:
	chip->fg_restarting = false;
	return -EINVAL;
}

#define FG_PROFILE_LEN			128
#define PROFILE_COMPARE_LEN		32
#define THERMAL_COEFF_ADDR		0x444
#define THERMAL_COEFF_OFFSET		0x2
#define BATTERY_PSY_WAIT_MS		2000
static int fg_batt_profile_init(struct fg_chip *chip)
{
	int rc = 0, ret;
	int len;
	struct device_node *node = chip->spmi->dev.of_node;
	struct device_node *batt_node, *profile_node;
	const char *data, *batt_type_str;
	bool tried_again = false, vbat_in_range, profiles_same;
	u8 reg = 0;

wait:
	fg_stay_awake(&chip->profile_wakeup_source);
	ret = wait_for_completion_interruptible_timeout(&chip->batt_id_avail,
			msecs_to_jiffies(PROFILE_LOAD_TIMEOUT_MS));
	/* If we were interrupted wait again one more time. */
	if (ret == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		pr_debug("interrupted, waiting again\n");
		goto wait;
	} else if (ret <= 0) {
		rc = -ETIMEDOUT;
		pr_err("profile loading timed out rc=%d\n", rc);
		goto no_profile;
	}

	/* Check whether the charger is ready */
	if (!is_charger_available(chip))
		goto reschedule;

	/* Disable charging for a FG cycle before calculating vbat_in_range */
	if (!chip->charging_disabled) {
		rc = set_prop_enable_charging(chip, false);
		if (rc)
			pr_err("Failed to disable charging, rc=%d\n", rc);

		goto update;
	}

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		pr_warn("No available batterydata, using OTP defaults\n");
		rc = 0;
		goto no_profile;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("battery id = %d\n",
				get_sram_prop_now(chip, FG_DATA_BATT_ID));
	profile_node = of_batterydata_get_best_profile(batt_node, "bms",
							fg_batt_type);
	if (IS_ERR_OR_NULL(profile_node)) {
		rc = PTR_ERR(profile_node);
		if (rc == -EPROBE_DEFER) {
			goto reschedule;
		} else {
			pr_err("couldn't find profile handle rc=%d\n", rc);
			goto no_profile;
		}
	}

	/* read rslow compensation values if they're available */
	rc = of_property_read_u32(profile_node, "qcom,chg-rs-to-rslow",
					&chip->rslow_comp.chg_rs_to_rslow);
	if (rc) {
		chip->rslow_comp.chg_rs_to_rslow = -EINVAL;
		if (rc != -EINVAL)
			pr_err("Could not read rs to rslow: %d\n", rc);
	}
	rc = of_property_read_u32(profile_node, "qcom,chg-rslow-comp-c1",
					&chip->rslow_comp.chg_rslow_comp_c1);
	if (rc) {
		chip->rslow_comp.chg_rslow_comp_c1 = -EINVAL;
		if (rc != -EINVAL)
			pr_err("Could not read rslow comp c1: %d\n", rc);
	}
	rc = of_property_read_u32(profile_node, "qcom,chg-rslow-comp-c2",
					&chip->rslow_comp.chg_rslow_comp_c2);
	if (rc) {
		chip->rslow_comp.chg_rslow_comp_c2 = -EINVAL;
		if (rc != -EINVAL)
			pr_err("Could not read rslow comp c2: %d\n", rc);
	}
	rc = of_property_read_u32(profile_node, "qcom,chg-rslow-comp-thr",
					&chip->rslow_comp.chg_rslow_comp_thr);
	if (rc) {
		chip->rslow_comp.chg_rslow_comp_thr = -EINVAL;
		if (rc != -EINVAL)
			pr_err("Could not read rslow comp thr: %d\n", rc);
	}

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
					&chip->batt_max_voltage_uv);

	if (rc)
		pr_warn("couldn't find battery max voltage\n");

	/*
	 * Only configure from profile if fg-cc-cv-threshold-mv is not
	 * defined in the charger device node.
	 */
	if (!of_find_property(chip->spmi->dev.of_node,
				"qcom,fg-cc-cv-threshold-mv", NULL)) {
		of_property_read_u32(profile_node,
				"qcom,fg-cc-cv-threshold-mv",
				&chip->cc_cv_threshold_mv);
	}

	data = of_get_property(profile_node, "qcom,fg-profile-data", &len);
	if (!data) {
		pr_err("no battery profile loaded\n");
		rc = 0;
		goto no_profile;
	}

	if (len != FG_PROFILE_LEN) {
		pr_err("battery profile incorrect size: %d\n", len);
		rc = -EINVAL;
		goto no_profile;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
					&batt_type_str);
	if (rc) {
		pr_err("Could not find battery data type: %d\n", rc);
		rc = 0;
		goto no_profile;
	}

	if (!chip->batt_profile)
		chip->batt_profile = devm_kzalloc(chip->dev,
				sizeof(char) * len, GFP_KERNEL);

	if (!chip->batt_profile) {
		pr_err("out of memory\n");
		rc = -ENOMEM;
		goto no_profile;
	}

	rc = fg_mem_read(chip, &reg, PROFILE_INTEGRITY_REG, 1, 0, 1);
	if (rc) {
		pr_err("failed to read profile integrity rc=%d\n", rc);
		goto no_profile;
	}

	rc = fg_mem_read(chip, chip->batt_profile, BATT_PROFILE_OFFSET,
			len, 0, 1);
	if (rc) {
		pr_err("failed to read profile rc=%d\n", rc);
		goto no_profile;
	}


	vbat_in_range = get_vbat_est_diff(chip)
			< settings[FG_MEM_VBAT_EST_DIFF].value * 1000;
	profiles_same = memcmp(chip->batt_profile, data,
					PROFILE_COMPARE_LEN) == 0;
	if (reg & PROFILE_INTEGRITY_BIT) {
		fg_cap_learning_load_data(chip);
		if (vbat_in_range && !fg_is_batt_empty(chip) && profiles_same) {
			if (fg_debug_mask & FG_STATUS)
				pr_info("Battery profiles same, using default\n");
			if (fg_est_dump)
				schedule_work(&chip->dump_sram);
			/*
			 * Copy the profile read from device tree for
			 * getting profile parameters later.
			 */
			memcpy(chip->batt_profile, data, len);
			chip->batt_profile_len = len;
			goto done;
		}
	} else {
		pr_info("Battery profile not same, clearing data\n");
		clear_cycle_counter(chip);
		chip->learning_data.learned_cc_uah = 0;
	}

	if (fg_est_dump)
		dump_sram(&chip->dump_sram);

	if ((fg_debug_mask & FG_STATUS) && !vbat_in_range)
		pr_info("Vbat out of range: v_current_pred: %d, v:%d\n",
				fg_data[FG_DATA_CPRED_VOLTAGE].value,
				fg_data[FG_DATA_VOLTAGE].value);

	if ((fg_debug_mask & FG_STATUS) && fg_is_batt_empty(chip))
		pr_info("battery empty\n");

	if ((fg_debug_mask & FG_STATUS) && !profiles_same)
		pr_info("profiles differ\n");

	if (fg_debug_mask & FG_STATUS) {
		pr_info("Using new profile\n");
		print_hex_dump(KERN_INFO, "FG: loaded profile: ",
				DUMP_PREFIX_NONE, 16, 1,
				chip->batt_profile, len, false);
	}

	memcpy(chip->batt_profile, data, len);
	chip->batt_profile_len = len;

	if (fg_debug_mask & FG_STATUS)
		print_hex_dump(KERN_INFO, "FG: new profile: ",
				DUMP_PREFIX_NONE, 16, 1, chip->batt_profile,
				chip->batt_profile_len, false);

	rc = fg_do_restart(chip, true);
	if (rc) {
		pr_err("restart failed: %d\n", rc);
		goto no_profile;
	}

	/*
	 * Only configure from profile if thermal-coefficients is not
	 * defined in the FG device node.
	 */
	if (!of_find_property(chip->spmi->dev.of_node,
				"qcom,thermal-coefficients", NULL)) {
		data = of_get_property(profile_node,
				"qcom,thermal-coefficients", &len);
		if (data && len == THERMAL_COEFF_N_BYTES) {
			memcpy(chip->thermal_coefficients, data, len);
			rc = fg_mem_write(chip, chip->thermal_coefficients,
				THERMAL_COEFF_ADDR, THERMAL_COEFF_N_BYTES,
				THERMAL_COEFF_OFFSET, 0);
			if (rc)
				pr_err("spmi write failed addr:%03x, ret:%d\n",
						THERMAL_COEFF_ADDR, rc);
			else if (fg_debug_mask & FG_STATUS)
				pr_info("Battery thermal coefficients changed\n");
		}
	}

	if (chip->rconn_mohm > 0) {
		rc = fg_update_batt_rslow_settings(chip);
		if (rc)
			pr_err("Error in updating ESR, rc=%d\n", rc);
	}
done:
	if (chip->charging_disabled) {
		rc = set_prop_enable_charging(chip, true);
		if (rc)
			pr_err("Failed to enable charging, rc=%d\n", rc);
		else
			chip->charging_disabled = false;
	}

	if (fg_batt_type)
		chip->batt_type = fg_batt_type;
	else
		chip->batt_type = batt_type_str;

	if (chip->first_profile_loaded && fg_reset_on_lockup) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("restoring SRAM registers\n");
		rc = fg_backup_sram_registers(chip, false);
		if (rc)
			pr_err("Couldn't restore sram registers\n");

		/* Read the cycle counter back from FG SRAM */
		if (chip->cyc_ctr.en)
			restore_cycle_counter(chip);
	}

	chip->first_profile_loaded = true;
	chip->profile_loaded = true;
	chip->soc_reporting_ready = true;
	chip->battery_missing = is_battery_missing(chip);
	update_chg_iterm(chip);
	update_cc_cv_setpoint(chip);
	rc = populate_system_data(chip);
	if (rc) {
		pr_err("failed to read ocv properties=%d\n", rc);
		return rc;
	}
	estimate_battery_age(chip, &chip->actual_cap_uah);
	schedule_work(&chip->status_change_work);
	if (chip->power_supply_registered)
		power_supply_changed(&chip->bms_psy);
	fg_relax(&chip->profile_wakeup_source);
	pr_info("Battery SOC: %d, V: %duV\n", get_prop_capacity(chip),
		fg_data[FG_DATA_VOLTAGE].value);
	complete_all(&chip->fg_reset_done);
	return rc;
no_profile:
	chip->soc_reporting_ready = true;
	if (chip->charging_disabled) {
		rc = set_prop_enable_charging(chip, true);
		if (rc)
			pr_err("Failed to enable charging, rc=%d\n", rc);
		else
			chip->charging_disabled = false;
	}

	if (chip->power_supply_registered)
		power_supply_changed(&chip->bms_psy);
	fg_relax(&chip->profile_wakeup_source);
	return rc;
update:
	cancel_delayed_work(&chip->update_sram_data);
	schedule_delayed_work(
		&chip->update_sram_data,
		msecs_to_jiffies(0));
reschedule:
	schedule_delayed_work(
		&chip->batt_profile_init,
		msecs_to_jiffies(BATTERY_PSY_WAIT_MS));
	fg_relax(&chip->profile_wakeup_source);
	return 0;
}

static void check_empty_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				check_empty_work.work);
	bool vbatt_low_sts;
	int msoc;

	/* handle empty soc based on vbatt-low interrupt */
	if (chip->use_vbat_low_empty_soc) {
		if (fg_get_vbatt_status(chip, &vbatt_low_sts))
			goto out;

		msoc = get_monotonic_soc_raw(chip);

		if (fg_debug_mask & FG_STATUS)
			pr_info("Vbatt_low: %d, msoc: %d\n", vbatt_low_sts,
				msoc);
		if (vbatt_low_sts || (msoc == 0))
			chip->soc_empty = true;
		else
			chip->soc_empty = false;

		if (chip->power_supply_registered)
			power_supply_changed(&chip->bms_psy);

		if (!chip->vbat_low_irq_enabled) {
			enable_irq(chip->batt_irq[VBATT_LOW].irq);
			enable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
			chip->vbat_low_irq_enabled = true;
		}
	} else if (fg_is_batt_empty(chip)) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("EMPTY SOC high\n");
		chip->soc_empty = true;
		if (chip->power_supply_registered)
			power_supply_changed(&chip->bms_psy);
	}

out:
	fg_relax(&chip->empty_check_wakeup_source);
}

static void batt_profile_init(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				batt_profile_init.work);

	if (fg_batt_profile_init(chip))
		pr_err("failed to initialize profile\n");
}

static void sysfs_restart_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				sysfs_restart_work);
	int rc;

	rc = fg_do_restart(chip, false);
	if (rc)
		pr_err("fg restart failed: %d\n", rc);
	mutex_lock(&chip->sysfs_restart_lock);
	fg_restart = 0;
	mutex_unlock(&chip->sysfs_restart_lock);
}

#define SRAM_RELEASE_TIMEOUT_MS		500
static void charge_full_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				charge_full_work);
	int rc;
	u8 buffer[3];
	int bsoc;
	int resume_soc_raw = settings[FG_MEM_RESUME_SOC].value;
	bool disable = false;
	u8 reg;

	if (chip->status != POWER_SUPPLY_STATUS_FULL) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("battery not full: %d\n", chip->status);
		disable = true;
	}

	fg_mem_lock(chip);
	rc = fg_mem_read(chip, buffer, BATTERY_SOC_REG, 3, 1, 0);
	if (rc) {
		pr_err("Unable to read battery soc: %d\n", rc);
		goto out;
	}
	if (buffer[2] <= resume_soc_raw) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("bsoc = 0x%02x <= resume = 0x%02x\n",
					buffer[2], resume_soc_raw);
		disable = true;
	}
	if (!disable)
		goto out;

	rc = fg_mem_write(chip, buffer, SOC_FULL_REG, 3,
			SOC_FULL_OFFSET, 0);
	if (rc) {
		pr_err("failed to write SOC_FULL rc=%d\n", rc);
		goto out;
	}
	/* force a full soc value into the monotonic in order to display 100 */
	buffer[0] = 0xFF;
	buffer[1] = 0xFF;
	rc = fg_mem_write(chip, buffer, SRAM_MONOTONIC_SOC_REG, 2,
			SRAM_MONOTONIC_SOC_OFFSET, 0);
	if (rc) {
		pr_err("failed to write SOC_FULL rc=%d\n", rc);
		goto out;
	}
	if (fg_debug_mask & FG_STATUS) {
		bsoc = buffer[0] | buffer[1] << 8 | buffer[2] << 16;
		pr_info("wrote %06x into soc full\n", bsoc);
	}
	fg_mem_release(chip);
	/*
	 * wait one cycle to make sure the soc is updated before clearing
	 * the soc mask bit
	 */
	fg_mem_lock(chip);
	fg_mem_read(chip, &reg, PROFILE_INTEGRITY_REG, 1, 0, 0);
out:
	fg_mem_release(chip);
	if (disable)
		chip->charge_full = false;
}

static void update_bcl_thresholds(struct fg_chip *chip)
{
	u8 data[4];
	u8 mh_offset = 0, lm_offset = 0;
	u16 address = 0;
	int ret = 0;

	address = settings[FG_MEM_BCL_MH_THRESHOLD].address;
	mh_offset = settings[FG_MEM_BCL_MH_THRESHOLD].offset;
	lm_offset = settings[FG_MEM_BCL_LM_THRESHOLD].offset;
	ret = fg_mem_read(chip, data, address, 4, 0, 1);
	if (ret)
		pr_err("Error reading BCL LM & MH threshold rc:%d\n", ret);
	else
		pr_debug("Old BCL LM threshold:%x MH threshold:%x\n",
			data[lm_offset], data[mh_offset]);
	BCL_MA_TO_ADC(settings[FG_MEM_BCL_MH_THRESHOLD].value, data[mh_offset]);
	BCL_MA_TO_ADC(settings[FG_MEM_BCL_LM_THRESHOLD].value, data[lm_offset]);

	ret = fg_mem_write(chip, data, address, 4, 0, 0);
	if (ret)
		pr_err("spmi write failed. addr:%03x, ret:%d\n",
			address, ret);
	else
		pr_debug("New BCL LM threshold:%x MH threshold:%x\n",
			data[lm_offset], data[mh_offset]);
}

static int disable_bcl_lpm(struct fg_chip *chip)
{
	u8 data[4];
	u8 lm_offset = 0;
	u16 address = 0;
	int rc = 0;

	address = settings[FG_MEM_BCL_LM_THRESHOLD].address;
	lm_offset = settings[FG_MEM_BCL_LM_THRESHOLD].offset;
	rc = fg_mem_read(chip, data, address, 4, 0, 1);
	if (rc) {
		pr_err("Error reading BCL LM & MH threshold rc:%d\n", rc);
		return rc;
	}
	pr_debug("Old BCL LM threshold:%x\n", data[lm_offset]);

	/* Put BCL always above LPM */
	BCL_MA_TO_ADC(0, data[lm_offset]);

	rc = fg_mem_write(chip, data, address, 4, 0, 0);
	if (rc)
		pr_err("spmi write failed. addr:%03x, rc:%d\n",
			address, rc);
	else
		pr_debug("New BCL LM threshold:%x\n", data[lm_offset]);

	return rc;
}

static void bcl_hi_power_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
			struct fg_chip,
			bcl_hi_power_work);
	int rc;

	if (chip->bcl_lpm_disabled) {
		rc = disable_bcl_lpm(chip);
		if (rc)
			pr_err("failed to disable bcl low mode %d\n",
					rc);
	} else {
		update_bcl_thresholds(chip);
	}
}

#define VOLT_UV_TO_VOLTCMP8(volt_uv)	\
			((volt_uv - 2500000) / 9766)
static int update_irq_volt_empty(struct fg_chip *chip)
{
	u8 data;
	int volt_mv = settings[FG_MEM_IRQ_VOLT_EMPTY].value;

	data = (u8)VOLT_UV_TO_VOLTCMP8(volt_mv * 1000);

	if (fg_debug_mask & FG_STATUS)
		pr_info("voltage = %d, converted_raw = %04x\n", volt_mv, data);
	return fg_mem_write(chip, &data,
			settings[FG_MEM_IRQ_VOLT_EMPTY].address, 1,
			settings[FG_MEM_IRQ_VOLT_EMPTY].offset, 0);
}

static int update_cutoff_voltage(struct fg_chip *chip)
{
	u8 data[2];
	u16 converted_voltage_raw;
	s64 voltage_mv = settings[FG_MEM_CUTOFF_VOLTAGE].value;

	converted_voltage_raw = (s16)MICROUNITS_TO_ADC_RAW(voltage_mv * 1000);
	data[0] = cpu_to_le16(converted_voltage_raw) & 0xFF;
	data[1] = cpu_to_le16(converted_voltage_raw) >> 8;

	if (fg_debug_mask & FG_STATUS)
		pr_info("voltage = %lld, converted_raw = %04x, data = %02x %02x\n",
			voltage_mv, converted_voltage_raw, data[0], data[1]);
	return fg_mem_write(chip, data, settings[FG_MEM_CUTOFF_VOLTAGE].address,
				2, settings[FG_MEM_CUTOFF_VOLTAGE].offset, 0);
}

static int update_iterm(struct fg_chip *chip)
{
	u8 data[2];
	u16 converted_current_raw;
	s64 current_ma = -settings[FG_MEM_TERM_CURRENT].value;

	converted_current_raw = (s16)MICROUNITS_TO_ADC_RAW(current_ma * 1000);
	data[0] = cpu_to_le16(converted_current_raw) & 0xFF;
	data[1] = cpu_to_le16(converted_current_raw) >> 8;

	if (fg_debug_mask & FG_STATUS)
		pr_info("current = %lld, converted_raw = %04x, data = %02x %02x\n",
			current_ma, converted_current_raw, data[0], data[1]);
	return fg_mem_write(chip, data, settings[FG_MEM_TERM_CURRENT].address,
				2, settings[FG_MEM_TERM_CURRENT].offset, 0);
}

#define OF_READ_SETTING(type, qpnp_dt_property, retval, optional)	\
do {									\
	if (retval)							\
		break;							\
									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
					"qcom," qpnp_dt_property,	\
					&settings[type].value);		\
									\
	if ((retval == -EINVAL) && optional)				\
		retval = 0;						\
	else if (retval)						\
		pr_err("Error reading " #qpnp_dt_property		\
				" property rc = %d\n", rc);		\
} while (0)

#define OF_READ_PROPERTY(store, qpnp_dt_property, retval, default_val)	\
do {									\
	if (retval)							\
		break;							\
									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
					"qcom," qpnp_dt_property,	\
					&store);			\
									\
	if (retval == -EINVAL) {					\
		retval = 0;						\
		store = default_val;					\
	} else if (retval) {						\
		pr_err("Error reading " #qpnp_dt_property		\
				" property rc = %d\n", rc);		\
	}								\
} while (0)

static int fg_dischg_gain_dt_init(struct fg_chip *chip)
{
	struct device_node *node = chip->spmi->dev.of_node;
	struct property *prop;
	int i, rc = 0;
	size_t size;

	prop = of_find_property(node, "qcom,fg-dischg-voltage-gain-soc",
			NULL);
	if (!prop) {
		pr_err("qcom-fg-dischg-voltage-gain-soc not specified\n");
		goto out;
	}

	size = prop->length / sizeof(u32);
	if (size != VOLT_GAIN_MAX) {
		pr_err("Voltage gain SOC specified is of incorrect size\n");
		goto out;
	}

	rc = of_property_read_u32_array(node,
		"qcom,fg-dischg-voltage-gain-soc", chip->dischg_gain.soc, size);
	if (rc < 0) {
		pr_err("Reading qcom-fg-dischg-voltage-gain-soc failed, rc=%d\n",
			rc);
		goto out;
	}

	for (i = 0; i < VOLT_GAIN_MAX; i++) {
		if (chip->dischg_gain.soc[i] < 0 ||
				chip->dischg_gain.soc[i] > 100) {
			pr_err("Incorrect dischg-voltage-gain-soc\n");
			goto out;
		}
	}

	prop = of_find_property(node, "qcom,fg-dischg-med-voltage-gain",
			NULL);
	if (!prop) {
		pr_err("qcom-fg-dischg-med-voltage-gain not specified\n");
		goto out;
	}

	size = prop->length / sizeof(u32);
	if (size != VOLT_GAIN_MAX) {
		pr_err("med-voltage-gain specified is of incorrect size\n");
		goto out;
	}

	rc = of_property_read_u32_array(node,
		"qcom,fg-dischg-med-voltage-gain", chip->dischg_gain.medc_gain,
		size);
	if (rc < 0) {
		pr_err("Reading qcom-fg-dischg-med-voltage-gain failed, rc=%d\n",
			rc);
		goto out;
	}

	prop = of_find_property(node, "qcom,fg-dischg-high-voltage-gain",
			NULL);
	if (!prop) {
		pr_err("qcom-fg-dischg-high-voltage-gain not specified\n");
		goto out;
	}

	size = prop->length / sizeof(u32);
	if (size != VOLT_GAIN_MAX) {
		pr_err("high-voltage-gain specified is of incorrect size\n");
		goto out;
	}

	rc = of_property_read_u32_array(node,
		"qcom,fg-dischg-high-voltage-gain",
		chip->dischg_gain.highc_gain, size);
	if (rc < 0) {
		pr_err("Reading qcom-fg-dischg-high-voltage-gain failed, rc=%d\n",
			rc);
		goto out;
	}

	if (fg_debug_mask & FG_STATUS) {
		for (i = 0; i < VOLT_GAIN_MAX; i++)
			pr_info("SOC:%d MedC_Gain:%d HighC_Gain: %d\n",
				chip->dischg_gain.soc[i],
				chip->dischg_gain.medc_gain[i],
				chip->dischg_gain.highc_gain[i]);
	}
	return 0;
out:
	chip->dischg_gain.enable = false;
	return rc;
}

#define DEFAULT_EVALUATION_CURRENT_MA	1000
static int fg_of_init(struct fg_chip *chip)
{
	int rc = 0, sense_type, len = 0;
	const char *data;
	struct device_node *node = chip->spmi->dev.of_node;
	u32 temp[2] = {0};

	OF_READ_SETTING(FG_MEM_SOFT_HOT, "warm-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_SOFT_COLD, "cool-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_HARD_HOT, "hot-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_HARD_COLD, "cold-bat-decidegc", rc, 1);

	if (of_find_property(node, "qcom,cold-hot-jeita-hysteresis", NULL)) {
		int hard_hot = 0, soft_hot = 0, hard_cold = 0, soft_cold = 0;

		rc = of_property_read_u32_array(node,
			"qcom,cold-hot-jeita-hysteresis", temp, 2);
		if (rc) {
			pr_err("Error reading cold-hot-jeita-hysteresis rc=%d\n",
				rc);
			return rc;
		}

		chip->jeita_hysteresis_support = true;
		chip->cold_hysteresis = temp[0];
		chip->hot_hysteresis = temp[1];
		hard_hot = settings[FG_MEM_HARD_HOT].value;
		soft_hot = settings[FG_MEM_SOFT_HOT].value;
		hard_cold = settings[FG_MEM_HARD_COLD].value;
		soft_cold = settings[FG_MEM_SOFT_COLD].value;
		if (((hard_hot - chip->hot_hysteresis) < soft_hot) ||
			((hard_cold + chip->cold_hysteresis) > soft_cold)) {
			chip->jeita_hysteresis_support = false;
			pr_err("invalid hysteresis: hot_hysterresis = %d cold_hysteresis = %d\n",
				chip->hot_hysteresis, chip->cold_hysteresis);
		} else {
			pr_debug("cold_hysteresis = %d, hot_hysteresis = %d\n",
				chip->cold_hysteresis, chip->hot_hysteresis);
		}
	}

	OF_READ_SETTING(FG_MEM_BCL_LM_THRESHOLD, "bcl-lm-threshold-ma",
		rc, 1);
	OF_READ_SETTING(FG_MEM_BCL_MH_THRESHOLD, "bcl-mh-threshold-ma",
		rc, 1);
	OF_READ_SETTING(FG_MEM_TERM_CURRENT, "fg-iterm-ma", rc, 1);
	OF_READ_SETTING(FG_MEM_CHG_TERM_CURRENT, "fg-chg-iterm-ma", rc, 1);
	OF_READ_SETTING(FG_MEM_CUTOFF_VOLTAGE, "fg-cutoff-voltage-mv", rc, 1);
	data = of_get_property(chip->spmi->dev.of_node,
			"qcom,thermal-coefficients", &len);
	if (data && len == THERMAL_COEFF_N_BYTES) {
		memcpy(chip->thermal_coefficients, data, len);
		chip->use_thermal_coefficients = true;
	}
	OF_READ_SETTING(FG_MEM_RESUME_SOC, "resume-soc", rc, 1);
	settings[FG_MEM_RESUME_SOC].value =
		DIV_ROUND_CLOSEST(settings[FG_MEM_RESUME_SOC].value
				* FULL_SOC_RAW, FULL_CAPACITY);
	OF_READ_SETTING(FG_MEM_RESUME_SOC, "resume-soc-raw", rc, 1);
	OF_READ_SETTING(FG_MEM_IRQ_VOLT_EMPTY, "irq-volt-empty-mv", rc, 1);
	OF_READ_SETTING(FG_MEM_VBAT_EST_DIFF, "vbat-estimate-diff-mv", rc, 1);
	OF_READ_SETTING(FG_MEM_DELTA_SOC, "fg-delta-soc", rc, 1);
	OF_READ_SETTING(FG_MEM_BATT_LOW, "fg-vbatt-low-threshold", rc, 1);
	OF_READ_SETTING(FG_MEM_THERM_DELAY, "fg-therm-delay-us", rc, 1);
	OF_READ_PROPERTY(chip->learning_data.max_increment,
			"cl-max-increment-deciperc", rc, 5);
	OF_READ_PROPERTY(chip->learning_data.max_decrement,
			"cl-max-decrement-deciperc", rc, 100);
	OF_READ_PROPERTY(chip->learning_data.max_temp,
			"cl-max-temp-decidegc", rc, 450);
	OF_READ_PROPERTY(chip->learning_data.min_temp,
			"cl-min-temp-decidegc", rc, 150);
	OF_READ_PROPERTY(chip->learning_data.max_start_soc,
			"cl-max-start-capacity", rc, 15);
	OF_READ_PROPERTY(chip->learning_data.vbat_est_thr_uv,
			"cl-vbat-est-thr-uv", rc, 40000);
	OF_READ_PROPERTY(chip->learning_data.max_cap_limit,
			"cl-max-limit-deciperc", rc, 0);
	OF_READ_PROPERTY(chip->learning_data.min_cap_limit,
			"cl-min-limit-deciperc", rc, 0);
	OF_READ_PROPERTY(chip->evaluation_current,
			"aging-eval-current-ma", rc,
			DEFAULT_EVALUATION_CURRENT_MA);
	OF_READ_PROPERTY(chip->cc_cv_threshold_mv,
			"fg-cc-cv-threshold-mv", rc, 0);
	if (of_property_read_bool(chip->spmi->dev.of_node,
				"qcom,capacity-learning-on"))
		chip->batt_aging_mode = FG_AGING_CC;
	else if (of_property_read_bool(chip->spmi->dev.of_node,
				"qcom,capacity-estimation-on"))
		chip->batt_aging_mode = FG_AGING_ESR;
	else
		chip->batt_aging_mode = FG_AGING_NONE;
	if (chip->batt_aging_mode == FG_AGING_CC) {
		chip->learning_data.feedback_on = of_property_read_bool(
					chip->spmi->dev.of_node,
					"qcom,capacity-learning-feedback");
	}
	if (fg_debug_mask & FG_AGING)
		pr_info("battery aging mode: %d\n", chip->batt_aging_mode);

	/* Get the use-otp-profile property */
	chip->use_otp_profile = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,use-otp-profile");
	chip->hold_soc_while_full = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,hold-soc-while-full");

	sense_type = of_property_read_bool(chip->spmi->dev.of_node,
					"qcom,ext-sense-type");
	if (rc == 0) {
		if (fg_sense_type < 0)
			fg_sense_type = sense_type;

		if (fg_debug_mask & FG_STATUS) {
			if (fg_sense_type == INTERNAL_CURRENT_SENSE)
				pr_info("Using internal sense\n");
			else if (fg_sense_type == EXTERNAL_CURRENT_SENSE)
				pr_info("Using external sense\n");
			else
				pr_info("Using default sense\n");
		}
	} else {
		rc = 0;
	}

	chip->bad_batt_detection_en = of_property_read_bool(node,
				"qcom,bad-battery-detection-enable");

	chip->sw_rbias_ctrl = of_property_read_bool(node,
				"qcom,sw-rbias-control");

	chip->cyc_ctr.en = of_property_read_bool(node,
				"qcom,cycle-counter-en");
	if (chip->cyc_ctr.en)
		chip->cyc_ctr.id = 1;

	chip->esr_pulse_tune_en = of_property_read_bool(node,
					"qcom,esr-pulse-tuning-en");

	chip->soc_slope_limiter_en = of_property_read_bool(node,
					"qcom,fg-control-slope-limiter");
	if (chip->soc_slope_limiter_en) {
		OF_READ_PROPERTY(chip->slope_limit_temp,
			"fg-slope-limit-temp-threshold", rc,
			SLOPE_LIMIT_TEMP_THRESHOLD);

		OF_READ_PROPERTY(chip->slope_limit_coeffs[LOW_TEMP_CHARGE],
			"fg-slope-limit-low-temp-chg", rc,
			SLOPE_LIMIT_LOW_TEMP_CHG);

		OF_READ_PROPERTY(chip->slope_limit_coeffs[HIGH_TEMP_CHARGE],
			"fg-slope-limit-high-temp-chg", rc,
			SLOPE_LIMIT_HIGH_TEMP_CHG);

		OF_READ_PROPERTY(chip->slope_limit_coeffs[LOW_TEMP_DISCHARGE],
			"fg-slope-limit-low-temp-dischg", rc,
			SLOPE_LIMIT_LOW_TEMP_DISCHG);

		OF_READ_PROPERTY(chip->slope_limit_coeffs[HIGH_TEMP_DISCHARGE],
			"fg-slope-limit-high-temp-dischg", rc,
			SLOPE_LIMIT_HIGH_TEMP_DISCHG);

		if (fg_debug_mask & FG_STATUS)
			pr_info("slope-limiter, temp: %d coeffs: [%d %d %d %d]\n",
				chip->slope_limit_temp,
				chip->slope_limit_coeffs[LOW_TEMP_CHARGE],
				chip->slope_limit_coeffs[HIGH_TEMP_CHARGE],
				chip->slope_limit_coeffs[LOW_TEMP_DISCHARGE],
				chip->slope_limit_coeffs[HIGH_TEMP_DISCHARGE]);
	}

	OF_READ_PROPERTY(chip->rconn_mohm, "fg-rconn-mohm", rc, 0);

	chip->dischg_gain.enable = of_property_read_bool(node,
					"qcom,fg-dischg-voltage-gain-ctrl");
	if (chip->dischg_gain.enable) {
		rc = fg_dischg_gain_dt_init(chip);
		if (rc) {
			pr_err("Error in reading dischg_gain parameters, rc=%d\n",
				rc);
			rc = 0;
		}
	}

	chip->use_vbat_low_empty_soc = of_property_read_bool(node,
					"qcom,fg-use-vbat-low-empty-soc");

	OF_READ_PROPERTY(chip->batt_temp_low_limit,
			"fg-batt-temp-low-limit", rc, BATT_TEMP_LOW_LIMIT);

	OF_READ_PROPERTY(chip->batt_temp_high_limit,
			"fg-batt-temp-high-limit", rc, BATT_TEMP_HIGH_LIMIT);

	if (fg_debug_mask & FG_STATUS)
		pr_info("batt-temp-low_limit: %d batt-temp-high_limit: %d\n",
			chip->batt_temp_low_limit, chip->batt_temp_high_limit);

	OF_READ_PROPERTY(chip->cc_soc_limit_pct, "fg-cc-soc-limit-pct", rc, 0);

	if (fg_debug_mask & FG_STATUS)
		pr_info("cc-soc-limit-pct: %d\n", chip->cc_soc_limit_pct);

	chip->batt_info_restore = of_property_read_bool(node,
					"qcom,fg-restore-batt-info");

	if (fg_debug_mask & FG_STATUS)
		pr_info("restore: %d validate_by_ocv: %d range_pct: %d\n",
			chip->batt_info_restore, fg_batt_valid_ocv,
			fg_batt_range_pct);

	return rc;
}

static int fg_init_irqs(struct fg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("fg: spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		if ((resource->start == chip->vbat_adc_addr) ||
				(resource->start == chip->ibat_adc_addr) ||
				(resource->start == chip->tp_rev_addr))
			continue;

		rc = fg_read(chip, &subtype,
				resource->start + REG_OFFSET_PERP_SUBTYPE, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			return rc;
		}

		switch (subtype) {
		case FG_SOC:
			chip->soc_irq[FULL_SOC].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "full-soc");
			if (chip->soc_irq[FULL_SOC].irq < 0) {
				pr_err("Unable to get full-soc irq\n");
				return rc;
			}
			chip->soc_irq[EMPTY_SOC].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "empty-soc");
			if (chip->soc_irq[EMPTY_SOC].irq < 0) {
				pr_err("Unable to get empty-soc irq\n");
				return rc;
			}
			chip->soc_irq[DELTA_SOC].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "delta-soc");
			if (chip->soc_irq[DELTA_SOC].irq < 0) {
				pr_err("Unable to get delta-soc irq\n");
				return rc;
			}
			chip->soc_irq[FIRST_EST_DONE].irq = spmi_get_irq_byname(
				chip->spmi, spmi_resource, "first-est-done");
			if (chip->soc_irq[FIRST_EST_DONE].irq < 0) {
				pr_err("Unable to get first-est-done irq\n");
				return rc;
			}

			rc = devm_request_irq(chip->dev,
				chip->soc_irq[FULL_SOC].irq,
				fg_soc_irq_handler, IRQF_TRIGGER_RISING,
				"full-soc", chip);
			if (rc < 0) {
				pr_err("Can't request %d full-soc: %d\n",
					chip->soc_irq[FULL_SOC].irq, rc);
				return rc;
			}
			enable_irq_wake(chip->soc_irq[FULL_SOC].irq);
			chip->full_soc_irq_enabled = true;

			if (!chip->use_vbat_low_empty_soc) {
				rc = devm_request_irq(chip->dev,
					chip->soc_irq[EMPTY_SOC].irq,
					fg_empty_soc_irq_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					"empty-soc", chip);
				if (rc < 0) {
					pr_err("Can't request %d empty-soc: %d\n",
						chip->soc_irq[EMPTY_SOC].irq,
						rc);
					return rc;
				}
			}

			rc = devm_request_irq(chip->dev,
				chip->soc_irq[DELTA_SOC].irq,
				fg_soc_irq_handler, IRQF_TRIGGER_RISING,
				"delta-soc", chip);
			if (rc < 0) {
				pr_err("Can't request %d delta-soc: %d\n",
					chip->soc_irq[DELTA_SOC].irq, rc);
				return rc;
			}
			rc = devm_request_irq(chip->dev,
				chip->soc_irq[FIRST_EST_DONE].irq,
				fg_first_soc_irq_handler, IRQF_TRIGGER_RISING,
				"first-est-done", chip);
			if (rc < 0) {
				pr_err("Can't request %d delta-soc: %d\n",
					chip->soc_irq[FIRST_EST_DONE].irq, rc);
				return rc;
			}

			enable_irq_wake(chip->soc_irq[DELTA_SOC].irq);
			if (!chip->use_vbat_low_empty_soc)
				enable_irq_wake(chip->soc_irq[EMPTY_SOC].irq);
			break;
		case FG_MEMIF:
			chip->mem_irq[FG_MEM_AVAIL].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "mem-avail");
			if (chip->mem_irq[FG_MEM_AVAIL].irq < 0) {
				pr_err("Unable to get mem-avail irq\n");
				return rc;
			}
			rc = devm_request_irq(chip->dev,
					chip->mem_irq[FG_MEM_AVAIL].irq,
					fg_mem_avail_irq_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					"mem-avail", chip);
			if (rc < 0) {
				pr_err("Can't request %d mem-avail: %d\n",
					chip->mem_irq[FG_MEM_AVAIL].irq, rc);
				return rc;
			}
			break;
		case FG_BATT:
			chip->batt_irq[JEITA_SOFT_COLD].irq =
				spmi_get_irq_byname(chip->spmi, spmi_resource,
						"soft-cold");
			if (chip->batt_irq[JEITA_SOFT_COLD].irq < 0) {
				pr_err("Unable to get soft-cold irq\n");
				rc = -EINVAL;
				return rc;
			}
			rc = devm_request_threaded_irq(chip->dev,
					chip->batt_irq[JEITA_SOFT_COLD].irq,
					NULL,
					fg_jeita_soft_cold_irq_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT,
					"soft-cold", chip);
			if (rc < 0) {
				pr_err("Can't request %d soft-cold: %d\n",
					chip->batt_irq[JEITA_SOFT_COLD].irq,
								rc);
				return rc;
			}
			disable_irq(chip->batt_irq[JEITA_SOFT_COLD].irq);
			chip->batt_irq[JEITA_SOFT_COLD].disabled = true;
			chip->batt_irq[JEITA_SOFT_HOT].irq =
				spmi_get_irq_byname(chip->spmi, spmi_resource,
					"soft-hot");
			if (chip->batt_irq[JEITA_SOFT_HOT].irq < 0) {
				pr_err("Unable to get soft-hot irq\n");
				rc = -EINVAL;
				return rc;
			}
			rc = devm_request_threaded_irq(chip->dev,
					chip->batt_irq[JEITA_SOFT_HOT].irq,
					NULL,
					fg_jeita_soft_hot_irq_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT,
					"soft-hot", chip);
			if (rc < 0) {
				pr_err("Can't request %d soft-hot: %d\n",
					chip->batt_irq[JEITA_SOFT_HOT].irq, rc);
				return rc;
			}
			disable_irq(chip->batt_irq[JEITA_SOFT_HOT].irq);
			chip->batt_irq[JEITA_SOFT_HOT].disabled = true;
			chip->batt_irq[BATT_MISSING].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource,
					"batt-missing");
			if (chip->batt_irq[BATT_MISSING].irq < 0) {
				pr_err("Unable to get batt-missing irq\n");
				rc = -EINVAL;
				return rc;
			}
			rc = devm_request_threaded_irq(chip->dev,
					chip->batt_irq[BATT_MISSING].irq,
					NULL,
					fg_batt_missing_irq_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT,
					"batt-missing", chip);
			if (rc < 0) {
				pr_err("Can't request %d batt-missing: %d\n",
					chip->batt_irq[BATT_MISSING].irq, rc);
				return rc;
			}
			chip->batt_irq[VBATT_LOW].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource,
					"vbatt-low");
			if (chip->batt_irq[VBATT_LOW].irq < 0) {
				pr_err("Unable to get vbatt-low irq\n");
				rc = -EINVAL;
				return rc;
			}
			rc = devm_request_irq(chip->dev,
					chip->batt_irq[VBATT_LOW].irq,
					fg_vbatt_low_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					"vbatt-low", chip);
			if (rc < 0) {
				pr_err("Can't request %d vbatt-low: %d\n",
					chip->batt_irq[VBATT_LOW].irq, rc);
				return rc;
			}
			if (chip->use_vbat_low_empty_soc) {
				enable_irq_wake(chip->batt_irq[VBATT_LOW].irq);
				chip->vbat_low_irq_enabled = true;
			} else {
				disable_irq_nosync(
					chip->batt_irq[VBATT_LOW].irq);
				chip->vbat_low_irq_enabled = false;
			}
			break;
		case FG_ADC:
			break;
		default:
			pr_err("subtype %d\n", subtype);
			return -EINVAL;
		}
	}

	chip->irqs_enabled = true;
	return rc;
}

static void fg_cancel_all_works(struct fg_chip *chip)
{
	cancel_delayed_work_sync(&chip->check_sanity_work);
	cancel_delayed_work_sync(&chip->update_sram_data);
	cancel_delayed_work_sync(&chip->update_temp_work);
	cancel_delayed_work_sync(&chip->update_jeita_setting);
	cancel_delayed_work_sync(&chip->check_empty_work);
	cancel_delayed_work_sync(&chip->batt_profile_init);
	alarm_try_to_cancel(&chip->fg_cap_learning_alarm);
	alarm_try_to_cancel(&chip->hard_jeita_alarm);
	if (!chip->ima_error_handling)
		cancel_work_sync(&chip->ima_error_recovery_work);
	cancel_work_sync(&chip->rslow_comp_work);
	cancel_work_sync(&chip->set_resume_soc_work);
	cancel_work_sync(&chip->fg_cap_learning_work);
	cancel_work_sync(&chip->dump_sram);
	cancel_work_sync(&chip->status_change_work);
	cancel_work_sync(&chip->cycle_count_work);
	cancel_work_sync(&chip->update_esr_work);
	cancel_work_sync(&chip->sysfs_restart_work);
	cancel_work_sync(&chip->gain_comp_work);
	cancel_work_sync(&chip->init_work);
	cancel_work_sync(&chip->charge_full_work);
	cancel_work_sync(&chip->bcl_hi_power_work);
	cancel_work_sync(&chip->esr_extract_config_work);
	cancel_work_sync(&chip->slope_limiter_work);
	cancel_work_sync(&chip->dischg_gain_work);
	cancel_work_sync(&chip->cc_soc_store_work);
}

static void fg_cleanup(struct fg_chip *chip)
{
	fg_cancel_all_works(chip);
	power_supply_unregister(&chip->bms_psy);
	mutex_destroy(&chip->rslow_comp.lock);
	mutex_destroy(&chip->rw_lock);
	mutex_destroy(&chip->cyc_ctr.lock);
	mutex_destroy(&chip->learning_data.learning_lock);
	mutex_destroy(&chip->sysfs_restart_lock);
	mutex_destroy(&chip->ima_recovery_lock);
	wakeup_source_trash(&chip->resume_soc_wakeup_source.source);
	wakeup_source_trash(&chip->empty_check_wakeup_source.source);
	wakeup_source_trash(&chip->memif_wakeup_source.source);
	wakeup_source_trash(&chip->profile_wakeup_source.source);
	wakeup_source_trash(&chip->update_temp_wakeup_source.source);
	wakeup_source_trash(&chip->update_sram_wakeup_source.source);
	wakeup_source_trash(&chip->gain_comp_wakeup_source.source);
	wakeup_source_trash(&chip->capacity_learning_wakeup_source.source);
	wakeup_source_trash(&chip->esr_extract_wakeup_source.source);
	wakeup_source_trash(&chip->slope_limit_wakeup_source.source);
	wakeup_source_trash(&chip->dischg_gain_wakeup_source.source);
	wakeup_source_trash(&chip->fg_reset_wakeup_source.source);
	wakeup_source_trash(&chip->cc_soc_wakeup_source.source);
	wakeup_source_trash(&chip->sanity_wakeup_source.source);
}

static int fg_remove(struct spmi_device *spmi)
{
	struct fg_chip *chip = dev_get_drvdata(&spmi->dev);

	fg_cleanup(chip);
	dev_set_drvdata(&spmi->dev, NULL);
	return 0;
}

static int fg_memif_data_open(struct inode *inode, struct file *file)
{
	struct fg_log_buffer *log;
	struct fg_trans *trans;
	u8 *data_buf;

	size_t logbufsize = SZ_4K;
	size_t databufsize = SZ_4K;

	if (!dbgfs_data.chip) {
		pr_err("Not initialized data\n");
		return -EINVAL;
	}

	/* Per file "transaction" data */
	trans = kzalloc(sizeof(*trans), GFP_KERNEL);
	if (!trans) {
		pr_err("Unable to allocate memory for transaction data\n");
		return -ENOMEM;
	}

	/* Allocate log buffer */
	log = kzalloc(logbufsize, GFP_KERNEL);

	if (!log) {
		kfree(trans);
		pr_err("Unable to allocate memory for log buffer\n");
		return -ENOMEM;
	}

	log->rpos = 0;
	log->wpos = 0;
	log->len = logbufsize - sizeof(*log);

	/* Allocate data buffer */
	data_buf = kzalloc(databufsize, GFP_KERNEL);

	if (!data_buf) {
		kfree(trans);
		kfree(log);
		pr_err("Unable to allocate memory for data buffer\n");
		return -ENOMEM;
	}

	trans->log = log;
	trans->data = data_buf;
	trans->cnt = dbgfs_data.cnt;
	trans->addr = dbgfs_data.addr;
	trans->chip = dbgfs_data.chip;
	trans->offset = trans->addr;
	mutex_init(&trans->memif_dfs_lock);

	file->private_data = trans;
	return 0;
}

static int fg_memif_dfs_close(struct inode *inode, struct file *file)
{
	struct fg_trans *trans = file->private_data;

	if (trans && trans->log && trans->data) {
		file->private_data = NULL;
		mutex_destroy(&trans->memif_dfs_lock);
		kfree(trans->log);
		kfree(trans->data);
		kfree(trans);
	}

	return 0;
}

/**
 * print_to_log: format a string and place into the log buffer
 * @log: The log buffer to place the result into.
 * @fmt: The format string to use.
 * @...: The arguments for the format string.
 *
 * The return value is the number of characters written to @log buffer
 * not including the trailing '\0'.
 */
static int print_to_log(struct fg_log_buffer *log, const char *fmt, ...)
{
	va_list args;
	int cnt;
	char *buf = &log->data[log->wpos];
	size_t size = log->len - log->wpos;

	va_start(args, fmt);
	cnt = vscnprintf(buf, size, fmt, args);
	va_end(args);

	log->wpos += cnt;
	return cnt;
}

/**
 * write_next_line_to_log: Writes a single "line" of data into the log buffer
 * @trans: Pointer to SRAM transaction data.
 * @offset: SRAM address offset to start reading from.
 * @pcnt: Pointer to 'cnt' variable.  Indicates the number of bytes to read.
 *
 * The 'offset' is a 12-bit SRAM address.
 *
 * On a successful read, the pcnt is decremented by the number of data
 * bytes read from the SRAM.  When the cnt reaches 0, all requested bytes have
 * been read.
 */
static int
write_next_line_to_log(struct fg_trans *trans, int offset, size_t *pcnt)
{
	int i, j;
	u8 data[ITEMS_PER_LINE];
	struct fg_log_buffer *log = trans->log;

	int cnt = 0;
	int padding = offset % ITEMS_PER_LINE;
	int items_to_read = min(ARRAY_SIZE(data) - padding, *pcnt);
	int items_to_log = min(ITEMS_PER_LINE, padding + items_to_read);

	/* Buffer needs enough space for an entire line */
	if ((log->len - log->wpos) < MAX_LINE_LENGTH)
		goto done;

	memcpy(data, trans->data + (offset - trans->addr), items_to_read);

	*pcnt -= items_to_read;

	/* Each line starts with the aligned offset (12-bit address) */
	cnt = print_to_log(log, "%3.3X ", offset & 0xfff);
	if (cnt == 0)
		goto done;

	/* If the offset is unaligned, add padding to right justify items */
	for (i = 0; i < padding; ++i) {
		cnt = print_to_log(log, "-- ");
		if (cnt == 0)
			goto done;
	}

	/* Log the data items */
	for (j = 0; i < items_to_log; ++i, ++j) {
		cnt = print_to_log(log, "%2.2X ", data[j]);
		if (cnt == 0)
			goto done;
	}

	/* If the last character was a space, then replace it with a newline */
	if (log->wpos > 0 && log->data[log->wpos - 1] == ' ')
		log->data[log->wpos - 1] = '\n';

done:
	return cnt;
}

/**
 * get_log_data - reads data from SRAM and saves to the log buffer
 * @trans: Pointer to SRAM transaction data.
 *
 * Returns the number of "items" read or SPMI error code for read failures.
 */
static int get_log_data(struct fg_trans *trans)
{
	int cnt, rc;
	int last_cnt;
	int items_read;
	int total_items_read = 0;
	u32 offset = trans->offset;
	size_t item_cnt = trans->cnt;
	struct fg_log_buffer *log = trans->log;

	if (item_cnt == 0)
		return 0;

	if (item_cnt > SZ_4K) {
		pr_err("Reading too many bytes\n");
		return -EINVAL;
	}

	rc = fg_mem_read(trans->chip, trans->data,
			trans->addr, trans->cnt, 0, 0);
	if (rc) {
		pr_err("dump failed: rc = %d\n", rc);
		return rc;
	}
	/* Reset the log buffer 'pointers' */
	log->wpos = log->rpos = 0;

	/* Keep reading data until the log is full */
	do {
		last_cnt = item_cnt;
		cnt = write_next_line_to_log(trans, offset, &item_cnt);
		items_read = last_cnt - item_cnt;
		offset += items_read;
		total_items_read += items_read;
	} while (cnt && item_cnt > 0);

	/* Adjust the transaction offset and count */
	trans->cnt = item_cnt;
	trans->offset += total_items_read;

	return total_items_read;
}

/**
 * fg_memif_dfs_reg_read: reads value(s) from SRAM and fills user's buffer a
 *  byte array (coded as string)
 * @file: file pointer
 * @buf: where to put the result
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user bytes read, or negative error value
 */
static ssize_t fg_memif_dfs_reg_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	struct fg_trans *trans = file->private_data;
	struct fg_log_buffer *log = trans->log;
	size_t ret;
	size_t len;

	mutex_lock(&trans->memif_dfs_lock);
	/* Is the the log buffer empty */
	if (log->rpos >= log->wpos) {
		if (get_log_data(trans) <= 0) {
			len = 0;
			goto unlock_mutex;
		}
	}

	len = min(count, log->wpos - log->rpos);

	ret = copy_to_user(buf, &log->data[log->rpos], len);
	if (ret == len) {
		pr_err("error copy sram register values to user\n");
		len = -EFAULT;
		goto unlock_mutex;
	}

	/* 'ret' is the number of bytes not copied */
	len -= ret;

	*ppos += len;
	log->rpos += len;

unlock_mutex:
	mutex_unlock(&trans->memif_dfs_lock);
	return len;
}

/**
 * fg_memif_dfs_reg_write: write user's byte array (coded as string) to SRAM.
 * @file: file pointer
 * @buf: user data to be written.
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user byte written, or negative error value
 */
static ssize_t fg_memif_dfs_reg_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int bytes_read;
	int data;
	int pos = 0;
	int cnt = 0;
	u8  *values;
	size_t ret = 0;
	char *kbuf;
	u32 offset;

	struct fg_trans *trans = file->private_data;

	mutex_lock(&trans->memif_dfs_lock);
	offset = trans->offset;

	/* Make a copy of the user data */
	kbuf = kmalloc(count + 1, GFP_KERNEL);
	if (!kbuf) {
		ret = -ENOMEM;
		goto unlock_mutex;
	}

	ret = copy_from_user(kbuf, buf, count);
	if (ret == count) {
		pr_err("failed to copy data from user\n");
		ret = -EFAULT;
		goto free_buf;
	}

	count -= ret;
	*ppos += count;
	kbuf[count] = '\0';

	/* Override the text buffer with the raw data */
	values = kbuf;

	/* Parse the data in the buffer.  It should be a string of numbers */
	while (sscanf(kbuf + pos, "%i%n", &data, &bytes_read) == 1) {
		pos += bytes_read;
		values[cnt++] = data & 0xff;
	}

	if (!cnt)
		goto free_buf;

	pr_info("address %x, count %d\n", offset, cnt);
	/* Perform the write(s) */

	ret = fg_mem_write(trans->chip, values, offset,
				cnt, 0, 0);
	if (ret) {
		pr_err("SPMI write failed, err = %zu\n", ret);
	} else {
		ret = count;
		trans->offset += cnt > 4 ? 4 : cnt;
	}

free_buf:
	kfree(kbuf);
unlock_mutex:
	mutex_unlock(&trans->memif_dfs_lock);
	return ret;
}

static const struct file_operations fg_memif_dfs_reg_fops = {
	.open		= fg_memif_data_open,
	.release	= fg_memif_dfs_close,
	.read		= fg_memif_dfs_reg_read,
	.write		= fg_memif_dfs_reg_write,
};

/**
 * fg_dfs_create_fs: create debugfs file system.
 * @return pointer to root directory or NULL if failed to create fs
 */
static struct dentry *fg_dfs_create_fs(void)
{
	struct dentry *root, *file;

	pr_debug("Creating FG_MEM debugfs file-system\n");
	root = debugfs_create_dir(DFS_ROOT_NAME, NULL);
	if (IS_ERR_OR_NULL(root)) {
		pr_err("Error creating top level directory err:%ld",
			(long)root);
		if (PTR_ERR(root) == -ENODEV)
			pr_err("debugfs is not enabled in the kernel");
		return NULL;
	}

	dbgfs_data.help_msg.size = strlen(dbgfs_data.help_msg.data);

	file = debugfs_create_blob("help", S_IRUGO, root, &dbgfs_data.help_msg);
	if (!file) {
		pr_err("error creating help entry\n");
		goto err_remove_fs;
	}
	return root;

err_remove_fs:
	debugfs_remove_recursive(root);
	return NULL;
}

/**
 * fg_dfs_get_root: return a pointer to FG debugfs root directory.
 * @return a pointer to the existing directory, or if no root
 * directory exists then create one. Directory is created with file that
 * configures SRAM transaction, namely: address, and count.
 * @returns valid pointer on success or NULL
 */
struct dentry *fg_dfs_get_root(void)
{
	if (dbgfs_data.root)
		return dbgfs_data.root;

	if (mutex_lock_interruptible(&dbgfs_data.lock) < 0)
		return NULL;
	/* critical section */
	if (!dbgfs_data.root) { /* double checking idiom */
		dbgfs_data.root = fg_dfs_create_fs();
	}
	mutex_unlock(&dbgfs_data.lock);
	return dbgfs_data.root;
}

/*
 * fg_dfs_create: adds new fg_mem if debugfs entry
 * @return zero on success
 */
int fg_dfs_create(struct fg_chip *chip)
{
	struct dentry *root;
	struct dentry *file;

	root = fg_dfs_get_root();
	if (!root)
		return -ENOENT;

	dbgfs_data.chip = chip;

	file = debugfs_create_u32("count", DFS_MODE, root, &(dbgfs_data.cnt));
	if (!file) {
		pr_err("error creating 'count' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_x32("address", DFS_MODE,
			root, &(dbgfs_data.addr));
	if (!file) {
		pr_err("error creating 'address' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_file("data", DFS_MODE, root, &dbgfs_data,
							&fg_memif_dfs_reg_fops);
	if (!file) {
		pr_err("error creating 'data' entry\n");
		goto err_remove_fs;
	}

	return 0;

err_remove_fs:
	debugfs_remove_recursive(root);
	return -ENOMEM;
}

#define EXTERNAL_SENSE_OFFSET_REG	0x41C
#define EXT_OFFSET_TRIM_REG		0xF8
#define SEC_ACCESS_REG			0xD0
#define SEC_ACCESS_UNLOCK		0xA5
#define BCL_TRIM_REV_FIXED		12
static int bcl_trim_workaround(struct fg_chip *chip)
{
	u8 reg, rc;

	if (chip->tp_rev_addr == 0)
		return 0;

	rc = fg_read(chip, &reg, chip->tp_rev_addr, 1);
	if (rc) {
		pr_err("Failed to read tp reg, rc = %d\n", rc);
		return rc;
	}
	if (reg >= BCL_TRIM_REV_FIXED) {
		if (fg_debug_mask & FG_STATUS)
			pr_info("workaround not applied, tp_rev = %d\n", reg);
		return 0;
	}

	rc = fg_mem_read(chip, &reg, EXTERNAL_SENSE_OFFSET_REG, 1, 2, 0);
	if (rc) {
		pr_err("Failed to read ext sense offset trim, rc = %d\n", rc);
		return rc;
	}
	rc = fg_masked_write(chip, chip->soc_base + SEC_ACCESS_REG,
			SEC_ACCESS_UNLOCK, SEC_ACCESS_UNLOCK, 1);

	rc |= fg_masked_write(chip, chip->soc_base + EXT_OFFSET_TRIM_REG,
			0xFF, reg, 1);
	if (rc) {
		pr_err("Failed to write ext sense offset trim, rc = %d\n", rc);
		return rc;
	}

	return 0;
}

#define FG_ALG_SYSCTL_1			0x4B0
#define KI_COEFF_PRED_FULL_ADDR		0x408
#define TEMP_FRAC_SHIFT_REG		0x4A4
#define FG_ADC_CONFIG_REG		0x4B8
#define KI_COEFF_PRED_FULL_4_0_MSB	0x88
#define KI_COEFF_PRED_FULL_4_0_LSB	0x00
#define FG_BCL_CONFIG_OFFSET		0x3
#define ALERT_CFG_OFFSET		3
#define I_TERM_QUAL_BIT			BIT(1)
#define PATCH_NEG_CURRENT_BIT		BIT(3)
#define BCL_FORCED_HPM_IN_CHARGE	BIT(2)
#define IRQ_USE_VOLTAGE_HYST_BIT	BIT(0)
#define EMPTY_FROM_VOLTAGE_BIT		BIT(1)
#define EMPTY_FROM_SOC_BIT		BIT(2)
#define EMPTY_SOC_IRQ_MASK		(IRQ_USE_VOLTAGE_HYST_BIT | \
					EMPTY_FROM_SOC_BIT | \
					EMPTY_FROM_VOLTAGE_BIT)
static int fg_common_hw_init(struct fg_chip *chip)
{
	int rc;
	int resume_soc_raw;
	u8 val;

	update_iterm(chip);
	update_cutoff_voltage(chip);
	update_bcl_thresholds(chip);
	if (!chip->use_vbat_low_empty_soc)
		update_irq_volt_empty(chip);

	resume_soc_raw = settings[FG_MEM_RESUME_SOC].value;
	if (resume_soc_raw > 0) {
		rc = fg_set_resume_soc(chip, resume_soc_raw);
		if (rc) {
			pr_err("Couldn't set resume SOC for FG\n");
			return rc;
		}
	} else {
		pr_info("FG auto recharge threshold not specified in DT\n");
	}

	if (fg_sense_type >= 0) {
		rc = set_prop_sense_type(chip, fg_sense_type);
		if (rc) {
			pr_err("failed to config sense type %d rc=%d\n",
					fg_sense_type, rc);
			return rc;
		}
	}

	rc = fg_mem_masked_write(chip, settings[FG_MEM_DELTA_SOC].address, 0xFF,
			soc_to_setpoint(settings[FG_MEM_DELTA_SOC].value),
			settings[FG_MEM_DELTA_SOC].offset);
	if (rc) {
		pr_err("failed to write delta soc rc=%d\n", rc);
		return rc;
	}

	/* Override the voltage threshold for vbatt_low with empty_volt */
	if (chip->use_vbat_low_empty_soc)
		settings[FG_MEM_BATT_LOW].value =
			settings[FG_MEM_IRQ_VOLT_EMPTY].value;

	rc = fg_mem_masked_write(chip, settings[FG_MEM_BATT_LOW].address, 0xFF,
			batt_to_setpoint_8b(settings[FG_MEM_BATT_LOW].value),
			settings[FG_MEM_BATT_LOW].offset);
	if (rc) {
		pr_err("failed to write Vbatt_low rc=%d\n", rc);
		return rc;
	}

	rc = fg_mem_masked_write(chip, settings[FG_MEM_THERM_DELAY].address,
		THERM_DELAY_MASK,
		therm_delay_to_setpoint(settings[FG_MEM_THERM_DELAY].value),
		settings[FG_MEM_THERM_DELAY].offset);
	if (rc) {
		pr_err("failed to write therm_delay rc=%d\n", rc);
		return rc;
	}

	if (chip->use_thermal_coefficients) {
		fg_mem_write(chip, chip->thermal_coefficients,
			THERMAL_COEFF_ADDR, THERMAL_COEFF_N_BYTES,
			THERMAL_COEFF_OFFSET, 0);
	}

	if (!chip->sw_rbias_ctrl) {
		rc = fg_mem_masked_write(chip, EXTERNAL_SENSE_SELECT,
				BATT_TEMP_CNTRL_MASK,
				TEMP_SENSE_ALWAYS_BIT,
				BATT_TEMP_OFFSET);
		if (rc) {
			pr_err("failed to write BATT_TEMP_OFFSET rc=%d\n", rc);
			return rc;
		}
	}

	/* Read the cycle counter back from FG SRAM */
	if (chip->cyc_ctr.en)
		restore_cycle_counter(chip);

	if (chip->esr_pulse_tune_en) {
		rc = fg_mem_read(chip, &val, SYS_CFG_1_REG, 1, SYS_CFG_1_OFFSET,
				0);
		if (rc) {
			pr_err("unable to read sys_cfg_1: %d\n", rc);
			return rc;
		}

		if (!(val & ENABLE_ESR_PULSE_VAL))
			chip->esr_extract_disabled = true;

		if (fg_debug_mask & FG_STATUS)
			pr_info("ESR extract is %sabled\n",
				chip->esr_extract_disabled ? "dis" : "en");

		rc = fg_mem_read(chip, &val, CBITS_INPUT_FILTER_REG, 1,
				CBITS_RMEAS1_OFFSET, 0);
		if (rc) {
			pr_err("unable to read cbits_input_filter_reg: %d\n",
				rc);
			return rc;
		}

		if (val & (IMPTR_FAST_TIME_SHIFT | IMPTR_LONG_TIME_SHIFT))
			chip->imptr_pulse_slow_en = true;

		if (fg_debug_mask & FG_STATUS)
			pr_info("imptr_pulse_slow is %sabled\n",
				chip->imptr_pulse_slow_en ? "en" : "dis");
	}

	rc = fg_mem_read(chip, &val, RSLOW_CFG_REG, 1, RSLOW_CFG_OFFSET,
			0);
	if (rc) {
		pr_err("unable to read rslow cfg: %d\n", rc);
		return rc;
	}

	if (val & RSLOW_CFG_ON_VAL)
		chip->rslow_comp.active = true;

	if (fg_debug_mask & FG_STATUS)
		pr_info("rslow_comp active is %sabled\n",
			chip->rslow_comp.active ? "en" : "dis");

	/*
	 * Clear bits 0-2 in 0x4B3 and set them again to make empty_soc irq
	 * trigger again.
	 */
	rc = fg_mem_masked_write(chip, FG_ALG_SYSCTL_1, EMPTY_SOC_IRQ_MASK,
			0, ALERT_CFG_OFFSET);
	if (rc) {
		pr_err("failed to write to 0x4B3 rc=%d\n", rc);
		return rc;
	}

	/* Wait for a FG cycle before enabling empty soc irq configuration */
	msleep(FG_CYCLE_MS);

	rc = fg_mem_masked_write(chip, FG_ALG_SYSCTL_1, EMPTY_SOC_IRQ_MASK,
			EMPTY_SOC_IRQ_MASK, ALERT_CFG_OFFSET);
	if (rc) {
		pr_err("failed to write to 0x4B3 rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_8994_hw_init(struct fg_chip *chip)
{
	int rc = 0;
	u8 data[4];
	u64 esr_value;

	rc = fg_mem_masked_write(chip, EXTERNAL_SENSE_SELECT,
			PATCH_NEG_CURRENT_BIT,
			PATCH_NEG_CURRENT_BIT,
			EXTERNAL_SENSE_OFFSET);
	if (rc) {
		pr_err("failed to write patch current bit rc=%d\n", rc);
		return rc;
	}

	rc = bcl_trim_workaround(chip);
	if (rc) {
		pr_err("failed to redo bcl trim rc=%d\n", rc);
		return rc;
	}

	rc = fg_mem_masked_write(chip, FG_ADC_CONFIG_REG,
			BCL_FORCED_HPM_IN_CHARGE,
			BCL_FORCED_HPM_IN_CHARGE,
			FG_BCL_CONFIG_OFFSET);
	if (rc) {
		pr_err("failed to force hpm in charge rc=%d\n", rc);
		return rc;
	}

	fg_mem_masked_write(chip, FG_ALG_SYSCTL_1, I_TERM_QUAL_BIT, 0, 0);

	data[0] = 0xA2;
	data[1] = 0x12;

	rc = fg_mem_write(chip, data, TEMP_FRAC_SHIFT_REG, 2, 2, 0);
	if (rc) {
		pr_err("failed to write temp ocv constants rc=%d\n", rc);
		return rc;
	}

	data[0] = KI_COEFF_PRED_FULL_4_0_LSB;
	data[1] = KI_COEFF_PRED_FULL_4_0_MSB;
	fg_mem_write(chip, data, KI_COEFF_PRED_FULL_ADDR, 2, 2, 0);

	esr_value = ESR_DEFAULT_VALUE;
	rc = fg_mem_write(chip, (u8 *)&esr_value, MAXRSCHANGE_REG, 8,
			ESR_VALUE_OFFSET, 0);
	if (rc)
		pr_err("failed to write default ESR value rc=%d\n", rc);
	else
		pr_debug("set default value to esr filter\n");

	return 0;
}

#define FG_USBID_CONFIG_OFFSET		0x2
#define DISABLE_USBID_DETECT_BIT	BIT(0)
static int fg_8996_hw_init(struct fg_chip *chip)
{
	int rc;

	rc = fg_mem_masked_write(chip, FG_ADC_CONFIG_REG,
			BCL_FORCED_HPM_IN_CHARGE,
			BCL_FORCED_HPM_IN_CHARGE,
			FG_BCL_CONFIG_OFFSET);
	if (rc) {
		pr_err("failed to force hpm in charge rc=%d\n", rc);
		return rc;
	}

	/* enable usbid conversions for PMi8996 V1.0 */
	if (chip->pmic_revision[REVID_DIG_MAJOR] == 1
			&& chip->pmic_revision[REVID_ANA_MAJOR] == 0) {
		rc = fg_mem_masked_write(chip, FG_ADC_CONFIG_REG,
				DISABLE_USBID_DETECT_BIT,
				0, FG_USBID_CONFIG_OFFSET);
		if (rc) {
			pr_err("failed to enable usbid conversions: %d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int fg_8950_hw_init(struct fg_chip *chip)
{
	int rc;

	rc = fg_mem_masked_write(chip, FG_ADC_CONFIG_REG,
			BCL_FORCED_HPM_IN_CHARGE,
			BCL_FORCED_HPM_IN_CHARGE,
			FG_BCL_CONFIG_OFFSET);
	if (rc)
		pr_err("failed to force hpm in charge rc=%d\n", rc);

	return rc;
}

static int fg_hw_init(struct fg_chip *chip)
{
	int rc = 0;

	rc = fg_common_hw_init(chip);
	if (rc) {
		pr_err("Unable to initialize FG HW rc=%d\n", rc);
		return rc;
	}

	/* add PMIC specific hw init */
	switch (chip->pmic_subtype) {
	case PMI8994:
		rc = fg_8994_hw_init(chip);
		chip->wa_flag |= PULSE_REQUEST_WA;
		break;
	case PMI8996:
		rc = fg_8996_hw_init(chip);
		/* Setup workaround flag based on PMIC type */
		if (fg_sense_type == INTERNAL_CURRENT_SENSE)
			chip->wa_flag |= IADC_GAIN_COMP_WA;
		if (chip->pmic_revision[REVID_DIG_MAJOR] >= 1)
			chip->wa_flag |= USE_CC_SOC_REG;

		break;
	case PMI8950:
	case PMI8937:
	case PMI8940:
		rc = fg_8950_hw_init(chip);
		/* Setup workaround flag based on PMIC type */
		chip->wa_flag |= BCL_HI_POWER_FOR_CHGLED_WA;
		if (fg_sense_type == INTERNAL_CURRENT_SENSE)
			chip->wa_flag |= IADC_GAIN_COMP_WA;
		if (chip->pmic_revision[REVID_DIG_MAJOR] > 1)
			chip->wa_flag |= USE_CC_SOC_REG;

		break;
	}
	if (rc)
		pr_err("Unable to initialize PMIC specific FG HW rc=%d\n", rc);

	pr_debug("wa_flag=0x%x\n", chip->wa_flag);

	return rc;
}

static int fg_init_iadc_config(struct fg_chip *chip)
{
	u8 reg[2];
	int rc;

	/* read default gain config */
	rc = fg_mem_read(chip, reg, K_VCOR_REG, 2, DEF_GAIN_OFFSET, 0);
	if (rc) {
		pr_err("Failed to read default gain rc=%d\n", rc);
		return rc;
	}

	if (reg[1] || reg[0]) {
		/*
		 * Default gain register has valid value:
		 * - write to gain register.
		 */
		rc = fg_mem_write(chip, reg, GAIN_REG, 2,
						GAIN_OFFSET, 0);
		if (rc) {
			pr_err("Failed to write gain rc=%d\n", rc);
			return rc;
		}
	} else {
		/*
		 * Default gain register is invalid:
		 * - read gain register for default gain value
		 * - write to default gain register.
		 */
		rc = fg_mem_read(chip, reg, GAIN_REG, 2,
						GAIN_OFFSET, 0);
		if (rc) {
			pr_err("Failed to read gain rc=%d\n", rc);
			return rc;
		}
		rc = fg_mem_write(chip, reg, K_VCOR_REG, 2,
						DEF_GAIN_OFFSET, 0);
		if (rc) {
			pr_err("Failed to write default gain rc=%d\n",
								rc);
			return rc;
		}
	}

	chip->iadc_comp_data.dfl_gain_reg[0] = reg[0];
	chip->iadc_comp_data.dfl_gain_reg[1] = reg[1];
	chip->iadc_comp_data.dfl_gain = half_float(reg);

	pr_debug("IADC gain initial config reg_val 0x%x%x gain %lld\n",
		       reg[1], reg[0], chip->iadc_comp_data.dfl_gain);
	return 0;
}

#define EN_WR_FGXCT_PRD		BIT(6)
#define EN_RD_FGXCT_PRD		BIT(5)
#define FG_RESTART_TIMEOUT_MS	12000
static void ima_error_recovery_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				ima_error_recovery_work);
	bool tried_again = false;
	int rc;
	u8 buf[4] = {0, 0, 0, 0};

	fg_stay_awake(&chip->fg_reset_wakeup_source);
	mutex_lock(&chip->ima_recovery_lock);
	if (!chip->ima_error_handling) {
		pr_err("Scheduled by mistake?\n");
		mutex_unlock(&chip->ima_recovery_lock);
		fg_relax(&chip->fg_reset_wakeup_source);
		return;
	}

	/*
	 * SOC should be read and used until the error recovery completes.
	 * Without this, there could be a fluctuation in SOC values notified
	 * to the userspace.
	 */
	chip->use_last_soc = true;

	/* Block SRAM access till FG reset is complete */
	chip->block_sram_access = true;

	/* Release the mutex to avoid deadlock while cancelling the works */
	mutex_unlock(&chip->ima_recovery_lock);

	/* Cancel all the works */
	fg_cancel_all_works(chip);

	if (fg_debug_mask & FG_STATUS)
		pr_info("last_soc: %d\n", chip->last_soc);

	mutex_lock(&chip->ima_recovery_lock);
	/* Acquire IMA access forcibly from FG ALG */
	rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_IMA_CFG,
			EN_WR_FGXCT_PRD | EN_RD_FGXCT_PRD,
			EN_WR_FGXCT_PRD | EN_RD_FGXCT_PRD, 1);
	if (rc) {
		pr_err("Error in writing to IMA_CFG, rc=%d\n", rc);
		goto out;
	}

	/* Release the IMA access now so that FG reset can go through */
	rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_IMA_CFG,
			EN_WR_FGXCT_PRD | EN_RD_FGXCT_PRD, 0, 1);
	if (rc) {
		pr_err("Error in writing to IMA_CFG, rc=%d\n", rc);
		goto out;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("resetting FG\n");

	/* Assert FG reset */
	rc = fg_reset(chip, true);
	if (rc) {
		pr_err("Couldn't reset FG\n");
		goto out;
	}

	/* Wait for a small time before deasserting FG reset */
	msleep(100);

	if (fg_debug_mask & FG_STATUS)
		pr_info("clearing FG from reset\n");

	/* Deassert FG reset */
	rc = fg_reset(chip, false);
	if (rc) {
		pr_err("Couldn't clear FG reset\n");
		goto out;
	}

	/* Wait for at least a FG cycle before doing SRAM access */
	msleep(2000);

	chip->block_sram_access = false;

	if (!chip->init_done) {
		schedule_work(&chip->init_work);
		goto wait;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("Calling hw_init\n");

	/*
	 * Once FG is reset, everything in SRAM will be wiped out. Redo
	 * hw_init, update jeita settings etc., again to make sure all
	 * the settings got restored again.
	 */
	rc = fg_hw_init(chip);
	if (rc) {
		pr_err("Error in hw_init, rc=%d\n", rc);
		goto out;
	}

	update_jeita_setting(&chip->update_jeita_setting.work);

	if (chip->wa_flag & IADC_GAIN_COMP_WA) {
		rc = fg_init_iadc_config(chip);
		if (rc)
			goto out;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("loading battery profile\n");
	if (!chip->use_otp_profile) {
		chip->battery_missing = true;
		chip->profile_loaded = false;
		chip->soc_reporting_ready = false;
		chip->batt_type = default_batt_type;
		fg_handle_battery_insertion(chip);
	}

wait:
	rc = wait_for_completion_interruptible_timeout(&chip->fg_reset_done,
			msecs_to_jiffies(FG_RESTART_TIMEOUT_MS));

	/* If we were interrupted wait again one more time. */
	if (rc == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		pr_debug("interrupted, waiting again\n");
		goto wait;
	} else if (rc <= 0) {
		pr_err("fg_restart taking long time rc=%d\n", rc);
		goto out;
	}

	rc = fg_mem_write(chip, buf, fg_data[FG_DATA_VINT_ERR].address,
			fg_data[FG_DATA_VINT_ERR].len,
			fg_data[FG_DATA_VINT_ERR].offset, 0);
	if (rc < 0)
		pr_err("Error in clearing VACT_INT_ERR, rc=%d\n", rc);

	if (fg_debug_mask & FG_STATUS)
		pr_info("IMA error recovery done...\n");
out:
	fg_restore_soc(chip);
	fg_restore_cc_soc(chip);
	fg_enable_irqs(chip, true);
	update_sram_data_work(&chip->update_sram_data.work);
	update_temp_data(&chip->update_temp_work.work);
	schedule_delayed_work(&chip->check_sanity_work,
		msecs_to_jiffies(1000));
	chip->ima_error_handling = false;
	mutex_unlock(&chip->ima_recovery_lock);
	fg_relax(&chip->fg_reset_wakeup_source);
}

#define DIG_MINOR		0x0
#define DIG_MAJOR		0x1
#define ANA_MINOR		0x2
#define ANA_MAJOR		0x3
#define IACS_INTR_SRC_SLCT	BIT(3)
static int fg_memif_init(struct fg_chip *chip)
{
	int rc;
	u8 dig_major;

	rc = fg_read(chip, chip->revision, chip->mem_base + DIG_MINOR, 4);
	if (rc) {
		pr_err("Unable to read FG revision rc=%d\n", rc);
		return rc;
	}

	switch (chip->revision[DIG_MAJOR]) {
	case DIG_REV_1:
	case DIG_REV_2:
		chip->offset = offset[0].address;
		break;
	case DIG_REV_3:
		chip->offset = offset[1].address;
		chip->ima_supported = true;
		break;
	default:
		pr_err("Digital Major rev=%d not supported\n", dig_major);
		return -EINVAL;
	}

	if (chip->ima_supported) {
		/*
		 * Change the FG_MEM_INT interrupt to track IACS_READY
		 * condition instead of end-of-transaction. This makes sure
		 * that the next transaction starts only after the hw is ready.
		 */
		rc = fg_masked_write(chip,
			chip->mem_base + MEM_INTF_IMA_CFG, IACS_INTR_SRC_SLCT,
			IACS_INTR_SRC_SLCT, 1);
		if (rc) {
			pr_err("failed to configure interrupt source %d\n", rc);
			return rc;
		}

		/* check for error condition */
		rc = fg_check_ima_exception(chip, true);
		if (rc) {
			pr_err("Error in clearing IMA exception rc=%d", rc);
			return rc;
		}
	}

	return 0;
}

static int fg_detect_pmic_type(struct fg_chip *chip)
{
	struct pmic_revid_data *pmic_rev_id;
	struct device_node *revid_dev_node;

	revid_dev_node = of_parse_phandle(chip->spmi->dev.of_node,
					"qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR(pmic_rev_id)) {
		pr_err("Unable to get pmic_revid rc=%ld\n",
				PTR_ERR(pmic_rev_id));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	switch (pmic_rev_id->pmic_subtype) {
	case PMI8994:
	case PMI8950:
	case PMI8937:
	case PMI8996:
	case PMI8940:
		chip->pmic_subtype = pmic_rev_id->pmic_subtype;
		chip->pmic_revision[REVID_RESERVED]	= pmic_rev_id->rev1;
		chip->pmic_revision[REVID_VARIANT]	= pmic_rev_id->rev2;
		chip->pmic_revision[REVID_ANA_MAJOR]	= pmic_rev_id->rev3;
		chip->pmic_revision[REVID_DIG_MAJOR]	= pmic_rev_id->rev4;
		break;
	default:
		pr_err("PMIC subtype %d not supported\n",
				pmic_rev_id->pmic_subtype);
		return -EINVAL;
	}

	return 0;
}

#define INIT_JEITA_DELAY_MS 1000
static void delayed_init_work(struct work_struct *work)
{
	int rc;
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				init_work);

	/* hold memory access until initialization finishes */
	fg_mem_lock(chip);

	rc = fg_hw_init(chip);
	if (rc) {
		pr_err("failed to hw init rc = %d\n", rc);
		if (!chip->init_done && chip->ima_supported) {
			rc = fg_check_alg_status(chip);
			if (rc && rc != -EBUSY)
				pr_err("Couldn't check FG ALG status, rc=%d\n",
					rc);
			fg_mem_release(chip);
			return;
		}
		fg_mem_release(chip);
		fg_cleanup(chip);
		return;
	}
	/* release memory access before update_sram_data is called */
	fg_mem_release(chip);

	schedule_delayed_work(
		&chip->update_jeita_setting,
		msecs_to_jiffies(INIT_JEITA_DELAY_MS));

	if (chip->last_sram_update_time == 0)
		update_sram_data_work(&chip->update_sram_data.work);

	if (chip->last_temp_update_time == 0)
		update_temp_data(&chip->update_temp_work.work);

	if (!chip->use_otp_profile)
		schedule_delayed_work(&chip->batt_profile_init, 0);

	if (chip->ima_supported && fg_reset_on_lockup)
		schedule_delayed_work(&chip->check_sanity_work,
			msecs_to_jiffies(1000));

	if (chip->wa_flag & IADC_GAIN_COMP_WA) {
		rc = fg_init_iadc_config(chip);
		if (rc)
			goto done;
	}

	chip->input_present = is_input_present(chip);
	chip->otg_present = is_otg_present(chip);
	chip->init_done = true;
	pr_debug("FG: HW_init success\n");
	return;
done:
	fg_cleanup(chip);
}

static int fg_probe(struct spmi_device *spmi)
{
	struct device *dev = &(spmi->dev);
	struct fg_chip *chip;
	struct spmi_resource *spmi_resource;
	struct resource *resource;
	u8 subtype, reg;
	int rc = 0;

	if (!spmi) {
		pr_err("no valid spmi pointer\n");
		return -ENODEV;
	}

	if (!spmi->dev.of_node) {
		pr_err("device node missing\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(dev, sizeof(struct fg_chip), GFP_KERNEL);
	if (chip == NULL) {
		pr_err("Can't allocate fg_chip\n");
		return -ENOMEM;
	}

	chip->spmi = spmi;
	chip->dev = &(spmi->dev);

	wakeup_source_init(&chip->empty_check_wakeup_source.source,
			"qpnp_fg_empty_check");
	wakeup_source_init(&chip->memif_wakeup_source.source,
			"qpnp_fg_memaccess");
	wakeup_source_init(&chip->profile_wakeup_source.source,
			"qpnp_fg_profile");
	wakeup_source_init(&chip->update_temp_wakeup_source.source,
			"qpnp_fg_update_temp");
	wakeup_source_init(&chip->update_sram_wakeup_source.source,
			"qpnp_fg_update_sram");
	wakeup_source_init(&chip->resume_soc_wakeup_source.source,
			"qpnp_fg_set_resume_soc");
	wakeup_source_init(&chip->gain_comp_wakeup_source.source,
			"qpnp_fg_gain_comp");
	wakeup_source_init(&chip->capacity_learning_wakeup_source.source,
			"qpnp_fg_cap_learning");
	wakeup_source_init(&chip->esr_extract_wakeup_source.source,
			"qpnp_fg_esr_extract");
	wakeup_source_init(&chip->slope_limit_wakeup_source.source,
			"qpnp_fg_slope_limit");
	wakeup_source_init(&chip->dischg_gain_wakeup_source.source,
			"qpnp_fg_dischg_gain");
	wakeup_source_init(&chip->fg_reset_wakeup_source.source,
			"qpnp_fg_reset");
	wakeup_source_init(&chip->cc_soc_wakeup_source.source,
			"qpnp_fg_cc_soc");
	wakeup_source_init(&chip->sanity_wakeup_source.source,
			"qpnp_fg_sanity_check");
	spin_lock_init(&chip->sec_access_lock);
	mutex_init(&chip->rw_lock);
	mutex_init(&chip->cyc_ctr.lock);
	mutex_init(&chip->learning_data.learning_lock);
	mutex_init(&chip->rslow_comp.lock);
	mutex_init(&chip->sysfs_restart_lock);
	mutex_init(&chip->ima_recovery_lock);
	INIT_DELAYED_WORK(&chip->update_jeita_setting, update_jeita_setting);
	INIT_DELAYED_WORK(&chip->update_sram_data, update_sram_data_work);
	INIT_DELAYED_WORK(&chip->update_temp_work, update_temp_data);
	INIT_DELAYED_WORK(&chip->check_empty_work, check_empty_work);
	INIT_DELAYED_WORK(&chip->batt_profile_init, batt_profile_init);
	INIT_DELAYED_WORK(&chip->check_sanity_work, check_sanity_work);
	INIT_WORK(&chip->ima_error_recovery_work, ima_error_recovery_work);
	INIT_WORK(&chip->rslow_comp_work, rslow_comp_work);
	INIT_WORK(&chip->fg_cap_learning_work, fg_cap_learning_work);
	INIT_WORK(&chip->dump_sram, dump_sram);
	INIT_WORK(&chip->status_change_work, status_change_work);
	INIT_WORK(&chip->cycle_count_work, update_cycle_count);
	INIT_WORK(&chip->battery_age_work, battery_age_work);
	INIT_WORK(&chip->update_esr_work, update_esr_value);
	INIT_WORK(&chip->set_resume_soc_work, set_resume_soc_work);
	INIT_WORK(&chip->sysfs_restart_work, sysfs_restart_work);
	INIT_WORK(&chip->init_work, delayed_init_work);
	INIT_WORK(&chip->charge_full_work, charge_full_work);
	INIT_WORK(&chip->gain_comp_work, iadc_gain_comp_work);
	INIT_WORK(&chip->bcl_hi_power_work, bcl_hi_power_work);
	INIT_WORK(&chip->esr_extract_config_work, esr_extract_config_work);
	INIT_WORK(&chip->slope_limiter_work, slope_limiter_work);
	INIT_WORK(&chip->dischg_gain_work, discharge_gain_work);
	INIT_WORK(&chip->cc_soc_store_work, cc_soc_store_work);
	alarm_init(&chip->fg_cap_learning_alarm, ALARM_BOOTTIME,
			fg_cap_learning_alarm_cb);
	alarm_init(&chip->hard_jeita_alarm, ALARM_BOOTTIME,
			fg_hard_jeita_alarm_cb);
	init_completion(&chip->sram_access_granted);
	init_completion(&chip->sram_access_revoked);
	complete_all(&chip->sram_access_revoked);
	init_completion(&chip->batt_id_avail);
	init_completion(&chip->first_soc_done);
	init_completion(&chip->fg_reset_done);
	dev_set_drvdata(&spmi->dev, chip);

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("qpnp_chg: spmi resource absent\n");
			rc = -ENXIO;
			goto of_init_fail;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			rc = -ENXIO;
			goto of_init_fail;
		}

		if (strcmp("qcom,fg-adc-vbat",
					spmi_resource->of_node->name) == 0) {
			chip->vbat_adc_addr = resource->start;
			continue;
		} else if (strcmp("qcom,fg-adc-ibat",
					spmi_resource->of_node->name) == 0) {
			chip->ibat_adc_addr = resource->start;
			continue;
		} else if (strcmp("qcom,revid-tp-rev",
					spmi_resource->of_node->name) == 0) {
			chip->tp_rev_addr = resource->start;
			continue;
		}

		rc = fg_read(chip, &subtype,
				resource->start + REG_OFFSET_PERP_SUBTYPE, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			goto of_init_fail;
		}

		switch (subtype) {
		case FG_SOC:
			chip->soc_base = resource->start;
			break;
		case FG_MEMIF:
			chip->mem_base = resource->start;
			break;
		case FG_BATT:
			chip->batt_base = resource->start;
			break;
		default:
			pr_err("Invalid peripheral subtype=0x%x\n", subtype);
			rc = -EINVAL;
		}
	}

	rc = fg_detect_pmic_type(chip);
	if (rc) {
		pr_err("Unable to detect PMIC type rc=%d\n", rc);
		return rc;
	}

	rc = fg_memif_init(chip);
	if (rc) {
		pr_err("Unable to setup mem_if offsets rc=%d\n", rc);
		goto of_init_fail;
	}

	rc = fg_of_init(chip);
	if (rc) {
		pr_err("failed to parse devicetree rc%d\n", rc);
		goto of_init_fail;
	}

	if (chip->jeita_hysteresis_support) {
		rc = fg_init_batt_temp_state(chip);
		if (rc) {
			pr_err("failed to get battery status rc%d\n", rc);
			goto of_init_fail;
		}
	}

	/* check if the first estimate is already finished at this time */
	if (is_first_est_done(chip))
		complete_all(&chip->first_soc_done);

	reg = 0xFF;
	rc = fg_write(chip, &reg, INT_EN_CLR(chip->mem_base), 1);
	if (rc) {
		pr_err("failed to clear interrupts %d\n", rc);
		goto of_init_fail;
	}

	rc = fg_init_irqs(chip);
	if (rc) {
		pr_err("failed to request interrupts %d\n", rc);
		goto cancel_work;
	}

	chip->batt_type = default_batt_type;

	chip->bms_psy.name = "bms";
	chip->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->bms_psy.properties = fg_power_props;
	chip->bms_psy.num_properties = ARRAY_SIZE(fg_power_props);
	chip->bms_psy.get_property = fg_power_get_property;
	chip->bms_psy.set_property = fg_power_set_property;
	chip->bms_psy.external_power_changed = fg_external_power_changed;
	chip->bms_psy.supplied_to = fg_supplicants;
	chip->bms_psy.num_supplicants = ARRAY_SIZE(fg_supplicants);
	chip->bms_psy.property_is_writeable = fg_property_is_writeable;

	rc = power_supply_register(chip->dev, &chip->bms_psy);
	if (rc < 0) {
		pr_err("batt failed to register rc = %d\n", rc);
		goto of_init_fail;
	}
	chip->power_supply_registered = true;
	/*
	 * Just initialize the batt_psy_name here. Power supply
	 * will be obtained later.
	 */
	chip->batt_psy_name = "battery";

	if (chip->mem_base) {
		rc = fg_dfs_create(chip);
		if (rc < 0) {
			pr_err("failed to create debugfs rc = %d\n", rc);
			goto power_supply_unregister;
		}
	}

	/* Fake temperature till the actual temperature is read */
	chip->last_good_temp = 250;

	/* Initialize batt_info variables */
	chip->batt_range_ocv = &fg_batt_valid_ocv;
	chip->batt_range_pct = &fg_batt_range_pct;
	memset(chip->batt_info, INT_MAX, sizeof(chip->batt_info));

	schedule_work(&chip->init_work);

	pr_info("FG Probe success - FG Revision DIG:%d.%d ANA:%d.%d PMIC subtype=%d\n",
		chip->revision[DIG_MAJOR], chip->revision[DIG_MINOR],
		chip->revision[ANA_MAJOR], chip->revision[ANA_MINOR],
		chip->pmic_subtype);

	return rc;

power_supply_unregister:
	power_supply_unregister(&chip->bms_psy);
cancel_work:
	fg_cancel_all_works(chip);
of_init_fail:
	mutex_destroy(&chip->rslow_comp.lock);
	mutex_destroy(&chip->rw_lock);
	mutex_destroy(&chip->cyc_ctr.lock);
	mutex_destroy(&chip->learning_data.learning_lock);
	mutex_destroy(&chip->sysfs_restart_lock);
	mutex_destroy(&chip->ima_recovery_lock);
	wakeup_source_trash(&chip->resume_soc_wakeup_source.source);
	wakeup_source_trash(&chip->empty_check_wakeup_source.source);
	wakeup_source_trash(&chip->memif_wakeup_source.source);
	wakeup_source_trash(&chip->profile_wakeup_source.source);
	wakeup_source_trash(&chip->update_temp_wakeup_source.source);
	wakeup_source_trash(&chip->update_sram_wakeup_source.source);
	wakeup_source_trash(&chip->gain_comp_wakeup_source.source);
	wakeup_source_trash(&chip->capacity_learning_wakeup_source.source);
	wakeup_source_trash(&chip->esr_extract_wakeup_source.source);
	wakeup_source_trash(&chip->slope_limit_wakeup_source.source);
	wakeup_source_trash(&chip->dischg_gain_wakeup_source.source);
	wakeup_source_trash(&chip->fg_reset_wakeup_source.source);
	wakeup_source_trash(&chip->cc_soc_wakeup_source.source);
	wakeup_source_trash(&chip->sanity_wakeup_source.source);
	return rc;
}

static void check_and_update_sram_data(struct fg_chip *chip)
{
	unsigned long current_time = 0, next_update_time, time_left;

	get_current_time(&current_time);

	next_update_time = chip->last_temp_update_time
		+ (TEMP_PERIOD_UPDATE_MS / 1000);

	if (next_update_time > current_time)
		time_left = next_update_time - current_time;
	else
		time_left = 0;

	schedule_delayed_work(
		&chip->update_temp_work, msecs_to_jiffies(time_left * 1000));

	next_update_time = chip->last_sram_update_time
		+ (fg_sram_update_period_ms / 1000);

	if (next_update_time > current_time)
		time_left = next_update_time - current_time;
	else
		time_left = 0;

	schedule_delayed_work(
		&chip->update_sram_data, msecs_to_jiffies(time_left * 1000));
}

static int fg_suspend(struct device *dev)
{
	struct fg_chip *chip = dev_get_drvdata(dev);

	if (!chip->sw_rbias_ctrl)
		return 0;

	cancel_delayed_work(&chip->update_temp_work);
	cancel_delayed_work(&chip->update_sram_data);

	return 0;
}

static int fg_resume(struct device *dev)
{
	struct fg_chip *chip = dev_get_drvdata(dev);

	if (!chip->sw_rbias_ctrl)
		return 0;

	check_and_update_sram_data(chip);
	return 0;
}

static void fg_check_ima_idle(struct fg_chip *chip)
{
	bool rif_mem_sts = true;
	int rc, time_count = 0;

	mutex_lock(&chip->rw_lock);
	/* Make sure IMA is idle */
	while (1) {
		rc = fg_check_rif_mem_access(chip, &rif_mem_sts);
		if (rc)
			break;

		if (!rif_mem_sts)
			break;

		if  (time_count > 4) {
			pr_err("Waited for ~16ms polling RIF_MEM_ACCESS_REQ\n");
			fg_run_iacs_clear_sequence(chip);
			break;
		}

		/* Wait for 4ms before reading RIF_MEM_ACCESS_REQ again */
		usleep_range(4000, 4100);
		time_count++;
	}
	mutex_unlock(&chip->rw_lock);
}

static void fg_shutdown(struct spmi_device *spmi)
{
	struct fg_chip *chip = dev_get_drvdata(&spmi->dev);

	if (fg_debug_mask & FG_STATUS)
		pr_emerg("FG shutdown started\n");
	if (chip->rslow_comp.active)
		fg_rslow_charge_comp_clear(chip);
	fg_cancel_all_works(chip);
	fg_check_ima_idle(chip);
	chip->fg_shutdown = true;
	if (fg_debug_mask & FG_STATUS)
		pr_emerg("FG shutdown complete\n");
}

static const struct dev_pm_ops qpnp_fg_pm_ops = {
	.suspend	= fg_suspend,
	.resume		= fg_resume,
};

static int fg_reset_lockup_set(const char *val, const struct kernel_param *kp)
{
	int rc;
	struct power_supply *bms_psy;
	struct fg_chip *chip;
	int old_val = fg_reset_on_lockup;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Unable to set fg_reset_on_lockup: %d\n", rc);
		return rc;
	}

	if (fg_reset_on_lockup != 0 && fg_reset_on_lockup != 1) {
		pr_err("Bad value %d\n", fg_reset_on_lockup);
		fg_reset_on_lockup = old_val;
		return -EINVAL;
	}

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return 0;
	}

	chip = container_of(bms_psy, struct fg_chip, bms_psy);
	if (!chip->ima_supported) {
		pr_err("Cannot set this for non-IMA supported FG\n");
		fg_reset_on_lockup = old_val;
		return -EINVAL;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("fg_reset_on_lockup set to %d\n", fg_reset_on_lockup);

	if (fg_reset_on_lockup)
		schedule_delayed_work(&chip->check_sanity_work,
			msecs_to_jiffies(1000));
	else
		cancel_delayed_work_sync(&chip->check_sanity_work);

	return rc;
}

static struct kernel_param_ops fg_reset_ops = {
	.set = fg_reset_lockup_set,
	.get = param_get_int,
};

module_param_cb(reset_on_lockup, &fg_reset_ops, &fg_reset_on_lockup, 0644);

static int fg_sense_type_set(const char *val, const struct kernel_param *kp)
{
	int rc;
	struct power_supply *bms_psy;
	struct fg_chip *chip;
	int old_fg_sense_type = fg_sense_type;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Unable to set fg_sense_type: %d\n", rc);
		return rc;
	}

	if (fg_sense_type != 0 && fg_sense_type != 1) {
		pr_err("Bad value %d\n", fg_sense_type);
		fg_sense_type = old_fg_sense_type;
		return -EINVAL;
	}

	if (fg_debug_mask & FG_STATUS)
		pr_info("fg_sense_type set to %d\n", fg_sense_type);

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return 0;
	}

	chip = container_of(bms_psy, struct fg_chip, bms_psy);
	rc = set_prop_sense_type(chip, fg_sense_type);
	return rc;
}

static struct kernel_param_ops fg_sense_type_ops = {
	.set = fg_sense_type_set,
	.get = param_get_int,
};

module_param_cb(sense_type, &fg_sense_type_ops, &fg_sense_type, 0644);

static int fg_restart_set(const char *val, const struct kernel_param *kp)
{
	struct power_supply *bms_psy;
	struct fg_chip *chip;

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return 0;
	}
	chip = container_of(bms_psy, struct fg_chip, bms_psy);

	mutex_lock(&chip->sysfs_restart_lock);
	if (fg_restart != 0) {
		mutex_unlock(&chip->sysfs_restart_lock);
		return 0;
	}
	fg_restart = 1;
	mutex_unlock(&chip->sysfs_restart_lock);

	if (fg_debug_mask & FG_STATUS)
		pr_info("fuel gauge restart initiated from sysfs...\n");

	schedule_work(&chip->sysfs_restart_work);
	return 0;
}

static struct kernel_param_ops fg_restart_ops = {
	.set = fg_restart_set,
	.get = param_get_int,
};

module_param_cb(restart, &fg_restart_ops, &fg_restart, 0644);

static struct spmi_driver fg_driver = {
	.driver		= {
		.name	= QPNP_FG_DEV_NAME,
		.of_match_table	= fg_match_table,
		.pm	= &qpnp_fg_pm_ops,
	},
	.probe		= fg_probe,
	.remove		= fg_remove,
	.shutdown	= fg_shutdown,
};

static int __init fg_init(void)
{
	return spmi_driver_register(&fg_driver);
}

static void __exit fg_exit(void)
{
	return spmi_driver_unregister(&fg_driver);
}

module_init(fg_init);
module_exit(fg_exit);

MODULE_DESCRIPTION("QPNP Fuel Gauge Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_FG_DEV_NAME);
