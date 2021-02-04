/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <pwrap_hal.h>
#include <mtk_spm_pmic_wrap.h>

#include <mtk_spm.h>
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif

/*
 * Macro and Definition
 */
#undef TAG
#define TAG "[SPM_PWRAP]"
#define spm_pwrap_crit(fmt, args...)	\
	pr_err(TAG"[CRTT]"fmt, ##args)
#define spm_pwrap_err(fmt, args...)	\
	pr_err(TAG"[ERR]"fmt, ##args)
#define spm_pwrap_warn(fmt, args...)	\
	pr_warn(TAG"[WARN]"fmt, ##args)
#define spm_pwrap_info(fmt, args...)	\
	pr_warn(TAG""fmt, ##args)	/* pr_info(TAG""fmt, ##args) */
#define spm_pwrap_debug(fmt, args...)	\
	pr_debug(TAG""fmt, ##args)


/*
 * BIT Operation
 */
#define _BIT_(_bit_) (unsigned int)(1 << (_bit_))
#define _BITS_(_bits_, _val_) \
	((((unsigned int) -1 >> (31 - ((1) ? \
			_bits_))) & ~((1U << ((0) ? _bits_)) - 1)) \
			& ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_) \
	(((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
			& ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_) \
	(((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/*
 * PMIC_WRAP
 */
#define VCORE_BASE_UV 50000 /* PMIC MT6356 */
/* ((((volt) - 700 * 100 + 625 - 1) / 625) */
#define VOLT_TO_PMIC_VAL(volt)  (((volt) - VCORE_BASE_UV + 625 - 1) / 625)
/* (((pmic) * 625) / 100 + 700) */
#define PMIC_VAL_TO_VOLT(pmic)  (((pmic) * 625) + VCORE_BASE_UV)

#define DEFAULT_VOLT_VSRAM      (100000)
#define DEFAULT_VOLT_VCORE      (100000)
#define NR_PMIC_WRAP_CMD (16)	/* 16 pmic wrap cmd */
#define MAX_RETRY_COUNT (100)
#define SPM_DATA_SHIFT (16)

struct pmic_wrap_cmd {
	unsigned long cmd_addr;
	unsigned long cmd_wdata;
};

struct pmic_wrap_setting {
	enum pmic_wrap_phase_id phase;
	struct pmic_wrap_cmd addr[NR_PMIC_WRAP_CMD];
	struct {
		struct {
			unsigned long cmd_addr;
			unsigned long cmd_wdata;
		} _[NR_PMIC_WRAP_CMD];
		const int nr_idx;
	} set[NR_PMIC_WRAP_PHASE];
};

static struct pmic_wrap_setting pw = {
	.phase = NR_PMIC_WRAP_PHASE,	/* invalid setting for init */
	.addr = {{0, 0} },

	.set[PMIC_WRAP_PHASE_ALLINONE] = {
		._[CMD_0]
		   = {MT6356_BUCK_VCORE_ELR0, VOLT_TO_PMIC_VAL(70000),},
		._[CMD_1]
		   = {MT6356_BUCK_VCORE_ELR0, VOLT_TO_PMIC_VAL(80000),},
		._[CMD_2]    = {0, 0,},
		._[CMD_3]    = {0, 0,},
		._[CMD_4]
		   = {MT6356_LDO_VSRAM_CON3,
				   _BITS_(14:8, VOLT_TO_PMIC_VAL(80000)),},
		._[CMD_5]
		   = {MT6356_LDO_VSRAM_CON3,
				   _BITS_(14:8, VOLT_TO_PMIC_VAL(90000)),},
		._[CMD_6]    = {MT6356_TOP_SPI_CON0, _BITS_(0:0, 0),},
		._[CMD_7]    = {MT6356_TOP_SPI_CON0, _BITS_(0:0, 1),},
		._[CMD_8]    = {MT6356_BUCK_VPROC_CON0, _BITS_(1:0, 3),},
		._[CMD_9]    = {MT6356_BUCK_VPROC_CON0, _BITS_(1:0, 1),},
		._[CMD_10]   = {MT6356_LDO_VSRAM_PROC_CON0, _BITS_(1:0, 3),},
		._[CMD_11]   = {MT6356_LDO_VSRAM_PROC_CON0, _BITS_(1:0, 1),},
		._[CMD_12]   = {MT6356_BUCK_VPROC_CON0, _BITS_(1:0, 0),},
		._[CMD_13]   = {MT6356_BUCK_VPROC_CON0, _BITS_(1:0, 1),},
		._[CMD_14]   = {MT6356_LDO_VSRAM_PROC_CON0, _BITS_(1:0, 0),},
		._[CMD_15]   = {MT6356_LDO_VSRAM_PROC_CON0, _BITS_(1:0, 1),},
		.nr_idx = NR_IDX_ALL,
	},
};

static DEFINE_SPINLOCK(pmic_wrap_lock);
#define pmic_wrap_lock(flags) spin_lock_irqsave(&pmic_wrap_lock, flags)
#define pmic_wrap_unlock(flags) spin_unlock_irqrestore(&pmic_wrap_lock, flags)

void _mt_spm_pmic_table_init(void)
{
	struct pmic_wrap_cmd pwrap_cmd_default[NR_PMIC_WRAP_CMD] = {
		{(unsigned long)SPM_DVFS_CMD0, (unsigned long)SPM_DVFS_CMD0,},
		{(unsigned long)SPM_DVFS_CMD1, (unsigned long)SPM_DVFS_CMD1,},
		{(unsigned long)SPM_DVFS_CMD2, (unsigned long)SPM_DVFS_CMD2,},
		{(unsigned long)SPM_DVFS_CMD3, (unsigned long)SPM_DVFS_CMD3,},
		{(unsigned long)SPM_DVFS_CMD4, (unsigned long)SPM_DVFS_CMD4,},
		{(unsigned long)SPM_DVFS_CMD5, (unsigned long)SPM_DVFS_CMD5,},
		{(unsigned long)SPM_DVFS_CMD6, (unsigned long)SPM_DVFS_CMD6,},
		{(unsigned long)SPM_DVFS_CMD7, (unsigned long)SPM_DVFS_CMD7,},
		{(unsigned long)SPM_DVFS_CMD8, (unsigned long)SPM_DVFS_CMD8,},
		{(unsigned long)SPM_DVFS_CMD9, (unsigned long)SPM_DVFS_CMD9,},
		{(unsigned long)SPM_DVFS_CMD10, (unsigned long)SPM_DVFS_CMD10,},
		{(unsigned long)SPM_DVFS_CMD11, (unsigned long)SPM_DVFS_CMD11,},
		{(unsigned long)SPM_DVFS_CMD12, (unsigned long)SPM_DVFS_CMD12,},
		{(unsigned long)SPM_DVFS_CMD13, (unsigned long)SPM_DVFS_CMD13,},
		{(unsigned long)SPM_DVFS_CMD14, (unsigned long)SPM_DVFS_CMD14,},
		{(unsigned long)SPM_DVFS_CMD15, (unsigned long)SPM_DVFS_CMD15,},
	};

	memcpy(pw.addr, pwrap_cmd_default, sizeof(pwrap_cmd_default));
}

void mt_spm_pmic_wrap_set_phase(enum pmic_wrap_phase_id phase)
{
	int idx;
	unsigned long flags;

	WARN_ON(phase >= NR_PMIC_WRAP_PHASE);

	if (pw.phase == phase)
		return;

	if (pw.addr[0].cmd_addr == 0)
		_mt_spm_pmic_table_init();

	pmic_wrap_lock(flags);

	pw.phase = phase;

	for (idx = 0; idx < pw.set[phase].nr_idx; idx++)
		spm_write(pw.addr[idx].cmd_addr, (pw.set[phase]._[idx].cmd_addr
				<< SPM_DATA_SHIFT)
				| (pw.set[phase]._[idx].cmd_wdata));

	spm_pwrap_warn("pmic table init: done\n");

	pmic_wrap_unlock(flags);
}
EXPORT_SYMBOL(mt_spm_pmic_wrap_set_phase);

void mt_spm_pmic_wrap_set_cmd(
		enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata)
{				/* just set wdata value */
	unsigned long flags;

	WARN_ON(phase >= NR_PMIC_WRAP_PHASE);
	WARN_ON(idx >= pw.set[phase].nr_idx);

	pmic_wrap_lock(flags);

	pw.set[phase]._[idx].cmd_wdata = cmd_wdata;

	if (pw.phase == phase)
		spm_write(pw.addr[idx].cmd_addr,
				(pw.set[phase]._[idx].cmd_addr
						<< SPM_DATA_SHIFT) | cmd_wdata);

	pmic_wrap_unlock(flags);
}
EXPORT_SYMBOL(mt_spm_pmic_wrap_set_cmd);

MODULE_DESCRIPTION("SPM-PMIC_WRAP Driver v0.1");
