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

#include "pwrap_hal.h"
#include "mt_spm_pmic_wrap.h"

#include "mt_spm.h"
#include <mt-plat/upmu_common.h>

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
#define _BIT_(_bit_)                    (unsigned)(1 << (_bit_))
#define _BITS_(_bits_, _val_)         \
	  ((((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_)               (((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_)   (((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/*
 * PMIC_WRAP
 */
#define VOLT_TO_PMIC_VAL(volt)  (((volt) - 60000 + 625 - 1) / 625)	/* ((((volt) - 700 * 100 + 625 - 1) / 625) */
#define PMIC_VAL_TO_VOLT(pmic)  (((pmic) * 625) + 60000)	/* (((pmic) * 625) / 100 + 700) */

#define DEFAULT_VOLT_VSRAM      (100000)
#define DEFAULT_VOLT_VCORE      (100000)
#define NR_PMIC_WRAP_CMD (16)	/* 16 pmic wrap cmd */
#define MAX_RETRY_COUNT (100)

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

	.set[PMIC_WRAP_PHASE_NORMAL] = {
		._[IDX_NM_RESERVE1] = {0, 0,},
		._[IDX_NM_RESERVE2] = {0, 0,},
		._[IDX_NM_VCORE_HPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_NM_VCORE_TRANS2] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(96250),},
		._[IDX_NM_VCORE_TRANS1] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(93125),},
		._[IDX_NM_VCORE_LPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		.nr_idx = NR_IDX_NM,
	},

	.set[PMIC_WRAP_PHASE_SUSPEND] = {
		._[IDX_SP_VSRAM_PWR_ON] = {MT6351_PMIC_RG_VSRAM_PROC_EN_ADDR, _BITS_(1:1, 1),},
		._[IDX_SP_VSRAM_SHUTDOWN] = {MT6351_PMIC_RG_VSRAM_PROC_EN_ADDR, _BITS_(1:1, 0),},
		._[IDX_SP_VCORE_HPM] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_SP_VCORE_TRANS2] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(96250),},
		._[IDX_SP_VCORE_TRANS1] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(93125),},
		._[IDX_SP_VCORE_LPM] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		.nr_idx = NR_IDX_SP,
	},

	.set[PMIC_WRAP_PHASE_DEEPIDLE] = {
		._[IDX_DI_VSRAM_NORMAL] = {MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR,
			VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VSRAM),},
		._[IDX_DI_VSRAM_SLEEP] = {MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(70001),},
		._[IDX_DI_VCORE_HPM] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_DI_VCORE_TRANS2] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(96250),},
		._[IDX_DI_VCORE_TRANS1] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(93125),},
		._[IDX_DI_VCORE_LPM] = {MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		._[IDX_DI_SRCCLKEN_IN2_NORMAL] = {MT6351_PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(3:3, 1),},
		._[IDX_DI_SRCCLKEN_IN2_SLEEP] = {MT6351_PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(3:3, 0),},
		.nr_idx = NR_IDX_DI,
	},
};

static DEFINE_SPINLOCK(pmic_wrap_lock);
#define pmic_wrap_lock(flags) spin_lock_irqsave(&pmic_wrap_lock, flags)
#define pmic_wrap_unlock(flags) spin_unlock_irqrestore(&pmic_wrap_lock, flags)

void _mt_spm_pmic_table_init(void)
{
	struct pmic_wrap_cmd pwrap_cmd_default[NR_PMIC_WRAP_CMD] = {
		{(unsigned long)PMIC_WRAP_DVFS_ADR0, (unsigned long)PMIC_WRAP_DVFS_WDATA0,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR1, (unsigned long)PMIC_WRAP_DVFS_WDATA1,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR2, (unsigned long)PMIC_WRAP_DVFS_WDATA2,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR3, (unsigned long)PMIC_WRAP_DVFS_WDATA3,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR4, (unsigned long)PMIC_WRAP_DVFS_WDATA4,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR5, (unsigned long)PMIC_WRAP_DVFS_WDATA5,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR6, (unsigned long)PMIC_WRAP_DVFS_WDATA6,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR7, (unsigned long)PMIC_WRAP_DVFS_WDATA7,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR8, (unsigned long)PMIC_WRAP_DVFS_WDATA8,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR9, (unsigned long)PMIC_WRAP_DVFS_WDATA9,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR10, (unsigned long)PMIC_WRAP_DVFS_WDATA10,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR11, (unsigned long)PMIC_WRAP_DVFS_WDATA11,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR12, (unsigned long)PMIC_WRAP_DVFS_WDATA12,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR13, (unsigned long)PMIC_WRAP_DVFS_WDATA13,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR14, (unsigned long)PMIC_WRAP_DVFS_WDATA14,},
		{(unsigned long)PMIC_WRAP_DVFS_ADR15, (unsigned long)PMIC_WRAP_DVFS_WDATA15,},
	};


	memcpy(pw.addr, pwrap_cmd_default, sizeof(pwrap_cmd_default));

}

static int _spm_dvfs_ctrl_volt(u32 value)
{
	u32 ap_dvfs_con;
	int retry = 0;

	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (1U << 0));

	ap_dvfs_con = spm_read(SPM_DVFS_CON);

	/* 16 PWRAP commands */
	spm_write(SPM_DVFS_CON, (ap_dvfs_con & ~(0xF)) | value);

	udelay(5);

	while ((spm_read(SPM_DVFS_CON) & (0x1 << 31)) == 0) {
		if (retry >= MAX_RETRY_COUNT) {
			spm_pwrap_err("@%s:  SPM write fail!\n", __func__);
			return -1;
		}

		retry++;
		/* spm_pwrap_info("wait for ACK signal from PMIC wrapper, retry = %d\n", retry); */

		udelay(5);
	}
	return 0;
}

void mt_spm_pmic_wrap_set_phase(enum pmic_wrap_phase_id phase)
{
	int i;
	unsigned long flags;

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);

#if 0
	if (pw.phase == phase)
		return;
#endif

	if (pw.addr[0].cmd_addr == 0) {
		spm_pwrap_warn("pmic table not initialized\n");
		_mt_spm_pmic_table_init();
	}

	pmic_wrap_lock(flags);

	pw.phase = phase;

	for (i = 0; i < pw.set[phase].nr_idx; i++) {
		spm_write(pw.addr[i].cmd_addr, pw.set[phase]._[i].cmd_addr);
		spm_write(pw.addr[i].cmd_wdata, pw.set[phase]._[i].cmd_wdata);
	}

	pmic_wrap_unlock(flags);
}
EXPORT_SYMBOL(mt_spm_pmic_wrap_set_phase);

void mt_spm_pmic_wrap_set_cmd(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata)
{				/* just set wdata value */
	unsigned long flags;

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);
	BUG_ON(idx >= pw.set[phase].nr_idx);

	/* spm_pwrap_info("@%s: phase = 0x%x, idx = %d, cmd_wdata = 0x%x\n", __func__, phase, idx, cmd_wdata); */

	pmic_wrap_lock(flags);

	pw.set[phase]._[idx].cmd_wdata = cmd_wdata;

	if (pw.phase == phase)
		spm_write(pw.addr[idx].cmd_wdata, cmd_wdata);

	pmic_wrap_unlock(flags);

}
EXPORT_SYMBOL(mt_spm_pmic_wrap_set_cmd);

void mt_spm_pmic_wrap_set_cmd_full(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_addr,
				   unsigned int cmd_wdata)
{
	unsigned long flags;

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);
	BUG_ON(idx >= pw.set[phase].nr_idx);

	/* spm_pwrap_info("@%s: phase = 0x%x, idx = %d, cmd_addr = 0x%x, cmd_wdata = 0x%x\n",
	   __func__, phase, idx, cmd_addr, cmd_wdata); */

	pmic_wrap_lock(flags);

	pw.set[phase]._[idx].cmd_addr = cmd_addr;
	pw.set[phase]._[idx].cmd_wdata = cmd_wdata;

	if (pw.phase == phase) {
		spm_write(pw.addr[idx].cmd_addr, cmd_addr);
		spm_write(pw.addr[idx].cmd_wdata, cmd_wdata);
	}

	pmic_wrap_unlock(flags);
}
EXPORT_SYMBOL(mt_spm_pmic_wrap_set_cmd_full);

void mt_spm_pmic_wrap_get_cmd_full(enum pmic_wrap_phase_id phase, int idx, unsigned int *p_cmd_addr,
				   unsigned int *p_cmd_wdata)
{
	unsigned long flags;

	BUG_ON(phase >= NR_PMIC_WRAP_PHASE);
	BUG_ON(idx >= pw.set[phase].nr_idx);

	pmic_wrap_lock(flags);

	*p_cmd_addr = pw.set[phase]._[idx].cmd_addr;
	*p_cmd_wdata = pw.set[phase]._[idx].cmd_wdata;

	pmic_wrap_unlock(flags);
	/* spm_pwrap_info("@%s: phase = 0x%x, idx = %d, original cmd_addr = 0x%x, cmd_wdata = 0x%x\n",
	   __func__, phase, idx, *p_cmd_addr, *p_cmd_wdata); */

}
EXPORT_SYMBOL(mt_cpufreq_get_pmic_cmd_full);

void mt_spm_pmic_wrap_apply_cmd(int idx)
{				/* kick spm */
	unsigned long flags;

	BUG_ON(idx >= pw.set[pw.phase].nr_idx);

	/* spm_pwrap_info("@%s: idx = %d\n", __func__, idx); */

	pmic_wrap_lock(flags);

	_spm_dvfs_ctrl_volt(idx);

	pmic_wrap_unlock(flags);
}
EXPORT_SYMBOL(mt_spm_pmic_wrap_apply_cmd);

int mt_spm_pmic_wrap_init(void)
{
	if (pw.addr[0].cmd_addr == 0)
		_mt_spm_pmic_table_init();

	/* init PMIC_WRAP & volt */
	mt_spm_pmic_wrap_set_phase(PMIC_WRAP_PHASE_NORMAL);

	return 0;
}

MODULE_DESCRIPTION("SPM-PMIC_WRAP Driver v0.1");
