/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __FG_CORE_H__
#define __FG_CORE_H__

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string_helpers.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/pmic-voter.h>

#define fg_dbg(fg, reason, fmt, ...)			\
	do {							\
		if (*fg->debug_mask & (reason))		\
			pr_info(fmt, ##__VA_ARGS__);	\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

#define PARAM(_id, _addr_word, _addr_byte, _len, _num, _den, _offset,	\
	      _enc, _dec)						\
	[FG_SRAM_##_id] = {						\
		.addr_word	= _addr_word,				\
		.addr_byte	= _addr_byte,				\
		.len		= _len,					\
		.numrtr		= _num,					\
		.denmtr		= _den,					\
		.offset		= _offset,				\
		.encode		= _enc,					\
		.decode		= _dec,					\
	}								\

/* Awake votable reasons */
#define SRAM_READ		"fg_sram_read"
#define SRAM_WRITE		"fg_sram_write"
#define PROFILE_LOAD		"fg_profile_load"
#define TTF_PRIMING		"fg_ttf_priming"
#define ESR_CALIB		"fg_esr_calib"

/* Delta BSOC irq votable reasons */
#define DELTA_BSOC_IRQ_VOTER	"fg_delta_bsoc_irq"

/* Delta ESR irq votable reasons */
#define DELTA_ESR_IRQ_VOTER	"fg_delta_esr_irq"

/* Battery missing irq votable reasons */
#define BATT_MISS_IRQ_VOTER	"fg_batt_miss_irq"

#define ESR_FCC_VOTER		"fg_esr_fcc"

#define FG_PARALLEL_EN_VOTER	"fg_parallel_en"
#define MEM_ATTN_IRQ_VOTER	"fg_mem_attn_irq"

#define DEBUG_BOARD_VOTER	"fg_debug_board"

#define BUCKET_COUNT			8
#define BUCKET_SOC_PCT			(256 / BUCKET_COUNT)

#define MAX_CC_STEPS			20

#define FULL_CAPACITY			100
#define FULL_SOC_RAW			255

#define DEBUG_BATT_SOC			67
#define BATT_MISS_SOC			50
#define ESR_SOH_SOC			50
#define EMPTY_SOC			0

enum prof_load_status {
	PROFILE_MISSING,
	PROFILE_LOADED,
	PROFILE_SKIPPED,
	PROFILE_NOT_LOADED,
};

/* Debug flag definitions */
enum fg_debug_flag {
	FG_IRQ			= BIT(0), /* Show interrupts */
	FG_STATUS		= BIT(1), /* Show FG status changes */
	FG_POWER_SUPPLY		= BIT(2), /* Show POWER_SUPPLY */
	FG_SRAM_WRITE		= BIT(3), /* Show SRAM writes */
	FG_SRAM_READ		= BIT(4), /* Show SRAM reads */
	FG_BUS_WRITE		= BIT(5), /* Show REGMAP writes */
	FG_BUS_READ		= BIT(6), /* Show REGMAP reads */
	FG_CAP_LEARN		= BIT(7), /* Show capacity learning */
	FG_TTF			= BIT(8), /* Show time to full */
	FG_FVSS			= BIT(9), /* Show FVSS */
};

/* SRAM access */
enum sram_access_flags {
	FG_IMA_DEFAULT	= 0,
	FG_IMA_ATOMIC	= BIT(0),
	FG_IMA_NO_WLOCK	= BIT(1),
};

/* JEITA */
enum jeita_levels {
	JEITA_COLD = 0,
	JEITA_COOL,
	JEITA_WARM,
	JEITA_HOT,
	NUM_JEITA_LEVELS,
};

/* FG irqs */
enum fg_irq_index {
	/* FG_BATT_SOC */
	MSOC_FULL_IRQ = 0,
	MSOC_HIGH_IRQ,
	MSOC_EMPTY_IRQ,
	MSOC_LOW_IRQ,
	MSOC_DELTA_IRQ,
	BSOC_DELTA_IRQ,
	SOC_READY_IRQ,
	SOC_UPDATE_IRQ,
	/* FG_BATT_INFO */
	BATT_TEMP_DELTA_IRQ,
	BATT_MISSING_IRQ,
	ESR_DELTA_IRQ,
	VBATT_LOW_IRQ,
	VBATT_PRED_DELTA_IRQ,
	/* FG_MEM_IF */
	DMA_GRANT_IRQ,
	MEM_XCP_IRQ,
	IMA_RDY_IRQ,
	FG_GEN3_IRQ_MAX,
	/* GEN4 FG_MEM_IF */
	MEM_ATTN_IRQ,
	DMA_XCP_IRQ,
	/* GEN4 FG_ADC_RR */
	BATT_TEMP_COLD_IRQ,
	BATT_TEMP_HOT_IRQ,
	BATT_ID_IRQ,
	FG_GEN4_IRQ_MAX,
};

/*
 * List of FG_SRAM parameters. Please add a parameter only if it is an entry
 * that will be used either to configure an entity (e.g. termination current)
 * which might need some encoding (or) it is an entry that will be read from
 * SRAM and decoded (e.g. CC_SOC_SW) for SW to use at various places. For
 * generic read/writes to SRAM registers, please use fg_sram_read/write APIs
 * directly without adding an entry here.
 */
enum fg_sram_param_id {
	FG_SRAM_BATT_SOC = 0,
	FG_SRAM_FULL_SOC,
	FG_SRAM_MONOTONIC_SOC,
	FG_SRAM_VOLTAGE_PRED,
	FG_SRAM_OCV,
	FG_SRAM_VBAT_FLT,
	FG_SRAM_VBAT_TAU,
	FG_SRAM_VBAT_FINAL,
	FG_SRAM_IBAT_FINAL,
	FG_SRAM_IBAT_FLT,
	FG_SRAM_RCONN,
	FG_SRAM_ESR,
	FG_SRAM_ESR_MDL,
	FG_SRAM_ESR_ACT,
	FG_SRAM_RSLOW,
	FG_SRAM_ALG_FLAGS,
	FG_SRAM_CC_SOC,
	FG_SRAM_CC_SOC_SW,
	FG_SRAM_ACT_BATT_CAP,
	FG_SRAM_TIMEBASE,
	/* Entries below here are configurable during initialization */
	FG_SRAM_CUTOFF_VOLT,
	FG_SRAM_EMPTY_VOLT,
	FG_SRAM_VBATT_LOW,
	FG_SRAM_FLOAT_VOLT,
	FG_SRAM_VBATT_FULL,
	FG_SRAM_ESR_TIMER_DISCHG_MAX,
	FG_SRAM_ESR_TIMER_DISCHG_INIT,
	FG_SRAM_ESR_TIMER_CHG_MAX,
	FG_SRAM_ESR_TIMER_CHG_INIT,
	FG_SRAM_ESR_PULSE_THRESH,
	FG_SRAM_SYS_TERM_CURR,
	FG_SRAM_CHG_TERM_CURR,
	FG_SRAM_CHG_TERM_BASE_CURR,
	FG_SRAM_CUTOFF_CURR,
	FG_SRAM_DELTA_MSOC_THR,
	FG_SRAM_DELTA_BSOC_THR,
	FG_SRAM_RECHARGE_SOC_THR,
	FG_SRAM_RECHARGE_VBATT_THR,
	FG_SRAM_KI_COEFF_LOW_DISCHG,
	FG_SRAM_KI_COEFF_MED_DISCHG,
	FG_SRAM_KI_COEFF_HI_DISCHG,
	FG_SRAM_KI_COEFF_LO_MED_DCHG_THR,
	FG_SRAM_KI_COEFF_MED_HI_DCHG_THR,
	FG_SRAM_KI_COEFF_LOW_CHG,
	FG_SRAM_KI_COEFF_MED_CHG,
	FG_SRAM_KI_COEFF_HI_CHG,
	FG_SRAM_KI_COEFF_LO_MED_CHG_THR,
	FG_SRAM_KI_COEFF_MED_HI_CHG_THR,
	FG_SRAM_KI_COEFF_FULL_SOC,
	FG_SRAM_KI_COEFF_CUTOFF,
	FG_SRAM_ESR_TIGHT_FILTER,
	FG_SRAM_ESR_BROAD_FILTER,
	FG_SRAM_SLOPE_LIMIT,
	FG_SRAM_BATT_TEMP_COLD,
	FG_SRAM_BATT_TEMP_HOT,
	FG_SRAM_ESR_CAL_SOC_MIN,
	FG_SRAM_ESR_CAL_SOC_MAX,
	FG_SRAM_ESR_CAL_TEMP_MIN,
	FG_SRAM_ESR_CAL_TEMP_MAX,
	FG_SRAM_DELTA_ESR_THR,
	FG_SRAM_MAX,
};

struct fg_sram_param {
	u16 addr_word;
	int addr_byte;
	u8  len;
	int value;
	int numrtr;
	int denmtr;
	int offset;
	void (*encode)(struct fg_sram_param *sp, enum fg_sram_param_id id,
		int val, u8 *buf);
	int (*decode)(struct fg_sram_param *sp, enum fg_sram_param_id id,
		int val);
};

struct fg_dma_address {
	/* Starting word address of the partition */
	u16 partition_start;
	/* Last word address of the partition */
	u16 partition_end;
	/*
	 * Byte offset in the FG_DMA peripheral that maps to the partition_start
	 * in SRAM
	 */
	u16 spmi_addr_base;
};

enum fg_alg_flag_id {
	ALG_FLAG_SOC_LT_OTG_MIN = 0,
	ALG_FLAG_SOC_LT_RECHARGE,
	ALG_FLAG_IBATT_LT_ITERM,
	ALG_FLAG_IBATT_GT_HPM,
	ALG_FLAG_IBATT_GT_UPM,
	ALG_FLAG_VBATT_LT_RECHARGE,
	ALG_FLAG_VBATT_GT_VFLOAT,
	ALG_FLAG_MAX,
};

enum fg_version {
	GEN3_FG = 1,
	GEN4_FG,
};

struct fg_alg_flag {
	char	*name;
	u8	bit;
	bool	invalid;
};

enum wa_flags {
	PMI8998_V1_REV_WA = BIT(0),
	PM660_TSMC_OSC_WA = BIT(1),
	PM8150B_V1_DMA_WA = BIT(2),
	PM8150B_V1_RSLOW_COMP_WA = BIT(3),
	PM8150B_V2_RSLOW_SCALE_FN_WA = BIT(4),
};

enum slope_limit_status {
	LOW_TEMP_DISCHARGE = 0,
	LOW_TEMP_CHARGE,
	HIGH_TEMP_DISCHARGE,
	HIGH_TEMP_CHARGE,
	SLOPE_LIMIT_NUM_COEFFS,
};

enum esr_timer_config {
	TIMER_RETRY = 0,
	TIMER_MAX,
	NUM_ESR_TIMERS,
};

enum fg_ttf_mode {
	FG_TTF_MODE_NORMAL = 0,
	FG_TTF_MODE_QNOVO,
};

/* parameters from battery profile */
struct fg_batt_props {
	const char	*batt_type_str;
	char		*batt_profile;
	int		float_volt_uv;
	int		vbatt_full_mv;
	int		fastchg_curr_ma;
	int		*therm_coeffs;
	int		therm_ctr_offset;
	int		therm_pull_up_kohms;
	int		*rslow_normal_coeffs;
	int		*rslow_low_coeffs;
};

struct fg_cyc_ctr_data {
	bool		en;
	bool		started[BUCKET_COUNT];
	u16		count[BUCKET_COUNT];
	u8		last_soc[BUCKET_COUNT];
	int		id;
	struct mutex	lock;
};

struct fg_cap_learning {
	bool		active;
	int		init_cc_soc_sw;
	int64_t		nom_cap_uah;
	int64_t		init_cc_uah;
	int64_t		final_cc_uah;
	int64_t		learned_cc_uah;
	struct mutex	lock;
};

struct fg_irq_info {
	const char		*name;
	const irq_handler_t	handler;
	bool			wakeable;
	int			irq;
};

struct fg_circ_buf {
	int	arr[10];
	int	size;
	int	head;
};

struct fg_cc_step_data {
	int arr[MAX_CC_STEPS];
	int sel;
};

struct fg_pt {
	s32 x;
	s32 y;
};

struct fg_ttf {
	struct fg_circ_buf	ibatt;
	struct fg_circ_buf	vbatt;
	struct fg_cc_step_data	cc_step;
	struct mutex		lock;
	int			mode;
	int			last_ttf;
	s64			last_ms;
};

static const struct fg_pt fg_ln_table[] = {
	{ 1000,		0 },
	{ 2000,		693 },
	{ 4000,		1386 },
	{ 6000,		1792 },
	{ 8000,		2079 },
	{ 16000,	2773 },
	{ 32000,	3466 },
	{ 64000,	4159 },
	{ 128000,	4852 },
};

/* each tuple is - <temperature in degC, Timebase> */
static const struct fg_pt fg_tsmc_osc_table[] = {
	{ -20,		395064 },
	{ -10,		398114 },
	{   0,		401669 },
	{  10,		404641 },
	{  20,		408856 },
	{  25,		412449 },
	{  30,		416532 },
	{  40,		420289 },
	{  50,		425020 },
	{  60,		430160 },
	{  70,		434175 },
	{  80,		439475 },
	{  90,		444992 },
};

struct fg_memif {
	struct fg_dma_address	*addr_map;
	int			num_partitions;
	u16			address_max;
	u8			num_bytes_per_word;
};

struct fg_dev {
	struct thermal_zone_device	*tz_dev;
	struct device		*dev;
	struct pmic_revid_data	*pmic_rev_id;
	struct regmap		*regmap;
	struct dentry		*dfs_root;
	struct power_supply	*fg_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct power_supply	*parallel_psy;
	struct power_supply	*pc_port_psy;
	struct fg_irq_info	*irqs;
	struct votable		*awake_votable;
	struct votable		*delta_bsoc_irq_en_votable;
	struct votable		*batt_miss_irq_en_votable;
	struct fg_sram_param	*sp;
	struct fg_memif		sram;
	struct fg_alg_flag	*alg_flags;
	int			*debug_mask;
	struct fg_batt_props	bp;
	struct notifier_block	nb;
	struct mutex		bus_lock;
	struct mutex		sram_rw_lock;
	struct mutex		charge_full_lock;
	struct mutex		qnovo_esr_ctrl_lock;
	u32			batt_soc_base;
	u32			batt_info_base;
	u32			mem_if_base;
	u32			rradc_base;
	u32			wa_flags;
	int			batt_id_ohms;
	int			charge_status;
	int			prev_charge_status;
	int			charge_done;
	int			charge_type;
	int			online_status;
	int			last_soc;
	int			last_batt_temp;
	int			health;
	int			maint_soc;
	int			delta_soc;
	int			last_msoc;
	int			last_recharge_volt_mv;
	bool			profile_available;
	enum prof_load_status	profile_load_status;
	bool			battery_missing;
	bool			fg_restarting;
	bool			charge_full;
	bool			recharge_soc_adjusted;
	bool			soc_reporting_ready;
	bool			use_ima_single_mode;
	bool			use_dma;
	bool			qnovo_enable;
	enum fg_version		version;
	struct completion	soc_update;
	struct completion	soc_ready;
	struct delayed_work	profile_load_work;
	struct work_struct	status_change_work;
	struct delayed_work	sram_dump_work;
};

/* Debugfs data structures are below */

/* Log buffer */
struct fg_log_buffer {
	size_t		rpos;
	size_t		wpos;
	size_t		len;
	char		data[0];
};

/* transaction parameters */
struct fg_trans {
	struct fg_dev		*fg;
	struct mutex		fg_dfs_lock; /* Prevent thread concurrency */
	struct fg_log_buffer	*log;
	u32			cnt;
	u16			addr;
	u32			offset;
	u8			*data;
};

struct fg_dbgfs {
	struct debugfs_blob_wrapper	help_msg;
	struct fg_dev			*fg;
	struct dentry			*root;
	u32				cnt;
	u32				addr;
};

extern int fg_decode_voltage_24b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
extern int fg_decode_voltage_15b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
extern int fg_decode_current_16b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
extern int fg_decode_current_24b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
extern int fg_decode_cc_soc(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int value);
extern int fg_decode_value_16b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
extern int fg_decode_default(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
extern int fg_decode(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
extern void fg_encode_voltage(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val_mv, u8 *buf);
extern void fg_encode_current(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val_ma, u8 *buf);
extern void fg_encode_default(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val, u8 *buf);
extern void fg_encode(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val, u8 *buf);
extern int fg_get_sram_prop(struct fg_dev *fg, enum fg_sram_param_id id,
	int *val);
extern int fg_get_msoc_raw(struct fg_dev *fg, int *val);
extern int fg_get_msoc(struct fg_dev *fg, int *val);
extern const char *fg_get_battery_type(struct fg_dev *fg);
extern int fg_get_battery_resistance(struct fg_dev *fg, int *val);
extern int fg_get_battery_voltage(struct fg_dev *fg, int *val);
extern int fg_get_battery_current(struct fg_dev *fg, int *val);
extern int fg_set_esr_timer(struct fg_dev *fg, int cycles_init, int cycles_max,
				bool charging, int flags);
extern int fg_set_constant_chg_voltage(struct fg_dev *fg, int volt_uv);
extern int fg_register_interrupts(struct fg_dev *fg, int size);
extern void fg_unregister_interrupts(struct fg_dev *fg, void *data, int size);
extern int fg_sram_write(struct fg_dev *fg, u16 address, u8 offset,
			u8 *val, int len, int flags);
extern int fg_sram_read(struct fg_dev *fg, u16 address, u8 offset,
			u8 *val, int len, int flags);
extern int fg_sram_masked_write(struct fg_dev *fg, u16 address, u8 offset,
			u8 mask, u8 val, int flags);
extern int fg_interleaved_mem_read(struct fg_dev *fg, u16 address,
			u8 offset, u8 *val, int len);
extern int fg_interleaved_mem_write(struct fg_dev *fg, u16 address,
			u8 offset, u8 *val, int len, bool atomic_access);
extern int fg_direct_mem_read(struct fg_dev *fg, u16 address,
			u8 offset, u8 *val, int len);
extern int fg_direct_mem_write(struct fg_dev *fg, u16 address,
			u8 offset, u8 *val, int len, bool atomic_access);
extern int fg_read(struct fg_dev *fg, int addr, u8 *val, int len);
extern int fg_write(struct fg_dev *fg, int addr, u8 *val, int len);
extern int fg_masked_write(struct fg_dev *fg, int addr, u8 mask, u8 val);
extern int fg_dump_regs(struct fg_dev *fg);
extern int fg_restart(struct fg_dev *fg, int wait_time_ms);
extern int fg_memif_init(struct fg_dev *fg);
extern int fg_clear_ima_errors_if_any(struct fg_dev *fg, bool check_hw_sts);
extern int fg_clear_dma_errors_if_any(struct fg_dev *fg);
extern int fg_debugfs_create(struct fg_dev *fg);
extern void fill_string(char *str, size_t str_len, u8 *buf, int buf_len);
extern void dump_sram(struct fg_dev *fg, u8 *buf, int addr, int len);
extern s64 fg_float_decode(u16 val);
extern bool usb_psy_initialized(struct fg_dev *fg);
extern bool dc_psy_initialized(struct fg_dev *fg);
extern bool batt_psy_initialized(struct fg_dev *fg);
extern bool pc_port_psy_initialized(struct fg_dev *fg);
extern void fg_notify_charger(struct fg_dev *fg);
extern bool is_input_present(struct fg_dev *fg);
extern bool is_qnovo_en(struct fg_dev *fg);
extern bool is_parallel_charger_available(struct fg_dev *fg);
extern void fg_circ_buf_add(struct fg_circ_buf *buf, int val);
extern void fg_circ_buf_clr(struct fg_circ_buf *buf);
extern int fg_circ_buf_avg(struct fg_circ_buf *buf, int *avg);
extern int fg_circ_buf_median(struct fg_circ_buf *buf, int *median);
extern int fg_lerp(const struct fg_pt *pts, size_t tablesize, s32 input,
			s32 *output);
#endif
