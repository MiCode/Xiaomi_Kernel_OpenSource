/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string_helpers.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include "pmic-voter.h"

#define fg_dbg(chip, reason, fmt, ...)			\
	do {							\
		if (*chip->debug_mask & (reason))		\
			pr_info(fmt, ##__VA_ARGS__);	\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

#define SRAM_READ	"fg_sram_read"
#define SRAM_WRITE	"fg_sram_write"
#define PROFILE_LOAD	"fg_profile_load"
#define DELTA_SOC	"fg_delta_soc"

#define DEBUG_PRINT_BUFFER_SIZE		64
/* 3 byte address + 1 space character */
#define ADDR_LEN			4
/* Format is 'XX ' */
#define CHARS_PER_ITEM			3
/* 4 data items per line */
#define ITEMS_PER_LINE			4
#define MAX_LINE_LENGTH			(ADDR_LEN + (ITEMS_PER_LINE *	\
					CHARS_PER_ITEM) + 1)		\

#define FG_SRAM_ADDRESS_MAX		255

/* Debug flag definitions */
enum fg_debug_flag {
	FG_IRQ			= BIT(0), /* Show interrupts */
	FG_STATUS		= BIT(1), /* Show FG status changes */
	FG_POWER_SUPPLY		= BIT(2), /* Show POWER_SUPPLY */
	FG_SRAM_WRITE		= BIT(3), /* Show SRAM writes */
	FG_SRAM_READ		= BIT(4), /* Show SRAM reads */
	FG_BUS_WRITE		= BIT(5), /* Show REGMAP writes */
	FG_BUS_READ		= BIT(6), /* Show REGMAP reads */
};

/* SRAM access */
enum sram_access_flags {
	FG_IMA_DEFAULT	= 0,
	FG_IMA_ATOMIC	= BIT(0),
	FG_IMA_NO_WLOCK	= BIT(1),
};

/* JEITA */
enum {
	JEITA_COLD = 0,
	JEITA_COOL,
	JEITA_WARM,
	JEITA_HOT,
	NUM_JEITA_LEVELS,
};

/* FG irqs */
enum fg_irq_index {
	MSOC_FULL_IRQ = 0,
	MSOC_HIGH_IRQ,
	MSOC_EMPTY_IRQ,
	MSOC_LOW_IRQ,
	MSOC_DELTA_IRQ,
	BSOC_DELTA_IRQ,
	SOC_READY_IRQ,
	SOC_UPDATE_IRQ,
	BATT_TEMP_DELTA_IRQ,
	BATT_MISSING_IRQ,
	ESR_DELTA_IRQ,
	VBATT_LOW_IRQ,
	VBATT_PRED_DELTA_IRQ,
	DMA_GRANT_IRQ,
	MEM_XCP_IRQ,
	IMA_RDY_IRQ,
	FG_IRQ_MAX,
};

/* WA flags */
enum {
	DELTA_SOC_IRQ_WA = BIT(0),
};

/* SRAM parameters */
enum fg_sram_param_id {
	FG_SRAM_BATT_SOC = 0,
	FG_SRAM_VOLTAGE_PRED,
	FG_SRAM_OCV,
	FG_SRAM_RSLOW,
	/* Entries below here are configurable during initialization */
	FG_SRAM_CUTOFF_VOLT,
	FG_SRAM_EMPTY_VOLT,
	FG_SRAM_VBATT_LOW,
	FG_SRAM_ESR_TIMER_DISCHG_MAX,
	FG_SRAM_ESR_TIMER_DISCHG_INIT,
	FG_SRAM_ESR_TIMER_CHG_MAX,
	FG_SRAM_ESR_TIMER_CHG_INIT,
	FG_SRAM_SYS_TERM_CURR,
	FG_SRAM_CHG_TERM_CURR,
	FG_SRAM_DELTA_SOC_THR,
	FG_SRAM_RECHARGE_SOC_THR,
	FG_SRAM_MAX,
};

struct fg_sram_param {
	u16 address;
	int offset;
	u8  len;
	int value;
	int numrtr;
	int denmtr;
	void (*encode)(struct fg_sram_param *sp, enum fg_sram_param_id id,
		int val, u8 *buf);
	int (*decode)(struct fg_sram_param *sp, enum fg_sram_param_id id,
		int val);
};

/* DT parameters for FG device */
struct fg_dt_props {
	int	cutoff_volt_mv;
	int	empty_volt_mv;
	int	vbatt_low_thr_mv;
	int	chg_term_curr_ma;
	int	sys_term_curr_ma;
	int	delta_soc_thr;
	int	recharge_soc_thr;
	int	rsense_sel;
	int	jeita_thresholds[NUM_JEITA_LEVELS];
	int	esr_timer_charging;
	int	esr_timer_awake;
	int	esr_timer_asleep;
};

/* parameters from battery profile */
struct fg_batt_props {
	const char	*batt_type_str;
	char		*batt_profile;
	int		float_volt_uv;
	int		fastchg_curr_ma;
	int		batt_id_kohm;
};

struct fg_irq_info {
	const char		*name;
	const irq_handler_t	handler;
	int			irq;
	bool			wakeable;
};

struct fg_chip {
	struct device		*dev;
	struct pmic_revid_data	*pmic_rev_id;
	struct regmap		*regmap;
	struct dentry		*dentry;
	struct power_supply	*fg_psy;
	struct power_supply	*batt_psy;
	struct iio_channel	*batt_id_chan;
	struct fg_memif		*sram;
	struct fg_irq_info	*irqs;
	struct votable		*awake_votable;
	struct fg_sram_param	*sp;
	int			*debug_mask;
	char			*batt_profile;
	struct fg_dt_props	dt;
	struct fg_batt_props	bp;
	struct notifier_block	nb;
	struct mutex		bus_lock;
	struct mutex		sram_rw_lock;
	u32			batt_soc_base;
	u32			batt_info_base;
	u32			mem_if_base;
	int			nom_cap_uah;
	bool			batt_id_avail;
	bool			profile_loaded;
	bool			battery_missing;
	struct completion	soc_update;
	struct completion	soc_ready;
	struct delayed_work	profile_load_work;
	struct work_struct	status_change_work;
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
	struct fg_chip		*chip;
	struct fg_log_buffer	*log;
	u32			cnt;
	u16			addr;
	u32			offset;
	u8			*data;
};

struct fg_dbgfs {
	struct debugfs_blob_wrapper	help_msg;
	struct fg_chip			*chip;
	struct dentry			*root;
	u32				cnt;
	u32				addr;
};

extern int fg_sram_write(struct fg_chip *chip, u16 address, u8 offset,
			u8 *val, int len, int flags);
extern int fg_sram_read(struct fg_chip *chip, u16 address, u8 offset,
			u8 *val, int len, int flags);
extern int fg_sram_masked_write(struct fg_chip *chip, u16 address, u8 offset,
			u8 mask, u8 val, int flags);
extern int fg_interleaved_mem_read(struct fg_chip *chip, u16 address,
			u8 offset, u8 *val, int len);
extern int fg_interleaved_mem_write(struct fg_chip *chip, u16 address,
			u8 offset, u8 *val, int len, bool atomic_access);
extern int fg_read(struct fg_chip *chip, int addr, u8 *val, int len);
extern int fg_write(struct fg_chip *chip, int addr, u8 *val, int len);
extern int fg_masked_write(struct fg_chip *chip, int addr, u8 mask, u8 val);
extern int fg_ima_init(struct fg_chip *chip);
extern int fg_sram_debugfs_create(struct fg_chip *chip);
extern void fill_string(char *str, size_t str_len, u8 *buf, int buf_len);
extern int64_t twos_compliment_extend(int64_t val, int s_bit_pos);
#endif
