/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

#include "pwrap_hal.h"
#include "mtk_spm_pmic_wrap.h"

#include "mtk_spm.h"
#include <mt-plat/upmu_common.h>

/*
 * Macro and Definition
 */
#undef TAG
#define TAG "[SPM_PWRAP]"
#define spm_pwrap_crit(fmt, args...)	\
	pr_debug(TAG"[CRTT]"fmt, ##args)
#define spm_pwrap_err(fmt, args...)	\
	pr_debug(TAG"[ERR]"fmt, ##args)
#define spm_pwrap_warn(fmt, args...)	\
	pr_debug(TAG"[WARN]"fmt, ##args)
#define spm_pwrap_info(fmt, args...)	\
	pr_debug(TAG""fmt, ##args)	/* pr_info(TAG""fmt, ##args) */
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
#define DEFAULT_VOLT_VSRAM      (100000)
#define DEFAULT_VOLT_VPROC      (100000)
#define DEFAULT_VOLT_VCORE      (100000)
#define NR_PMIC_WRAP_CMD (16)	/* 6755 has 16 pmic wrap cmd */
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

#if defined(CONFIG_MACH_MT6755)

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
#elif defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
static struct pmic_wrap_setting pw = {
	.phase = NR_PMIC_WRAP_PHASE,	/* invalid setting for init */
	.addr = {{0, 0} },

	.set[PMIC_WRAP_PHASE_NORMAL] = {
		._[IDX_NM_RESERVE1] = {0, 0,},
		._[IDX_NM_RESERVE2] = {0, 0,},
		._[IDX_NM_VCORE_HPM] = { PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(80000),},
		._[IDX_NM_VCORE_LPM] = { PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_NM_VCORE_ULPM] = { PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_NM_VCORE_RESERVE] = {0, 0,},
		.nr_idx = NR_IDX_NM,
	},

	.set[PMIC_WRAP_PHASE_SUSPEND] = {
		._[IDX_SP_VSRAM_PWR_ON] = {PMIC_RG_LDO_VSRAM_PROC_EN_ADDR, _BITS_(0:0, 1),},
		._[IDX_SP_VSRAM_SHUTDOWN] = {PMIC_RG_LDO_VSRAM_PROC_EN_ADDR, _BITS_(0:0, 0),},
		._[IDX_SP_VCORE_HPM] = {PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(80000),},
		._[IDX_SP_VCORE_LPM] = {PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_SP_VCORE_ULPM] = {PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_SP_VCORE_RESERVE] = {0, 0,},
		._[IDX_SP_VPROC_PWR_ON] = {PMIC_RG_BUCK_VPROC11_EN_ADDR, _BITS_(0:0, 1),},
		._[IDX_SP_VPROC_SHUTDOWN] = {PMIC_RG_BUCK_VPROC11_EN_ADDR, _BITS_(0:0, 0),},
		.nr_idx = NR_IDX_SP,
	},

	.set[PMIC_WRAP_PHASE_DEEPIDLE] = {
		._[IDX_DI_VSRAM_NORMAL] = {PMIC_RG_LDO_VSRAM_PROC_VOSEL_ADDR,
			VOLT_TO_PMIC_VAL_BASE(DEFAULT_VOLT_VSRAM, 51875),},
		._[IDX_DI_VSRAM_SLEEP] = {PMIC_RG_LDO_VSRAM_PROC_VOSEL_ADDR,
			VOLT_TO_PMIC_VAL_BASE(60000, 51875),},
		._[IDX_DI_VCORE_HPM] = {PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(80000),},
		._[IDX_DI_VCORE_LPM] = {PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_DI_VCORE_ULPM] = {PMIC_RG_BUCK_VCORE_VOSEL_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_DI_VCORE_RESERVE] = {0, 0,},
		._[IDX_DI_SRCCLKEN_IN2_NORMAL] = {PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(0:0, 1),},
		._[IDX_DI_SRCCLKEN_IN2_SLEEP] = {PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(0:0, 0),},
		._[IDX_DI_VPROC_NORMAL] = {PMIC_RG_BUCK_VPROC11_VOSEL_ADDR, VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VPROC),},
		._[IDX_DI_VPROC_SLEEP] = {PMIC_RG_BUCK_VPROC11_VOSEL_ADDR, VOLT_TO_PMIC_VAL(60000),},
		.nr_idx = NR_IDX_DI,
	},
};
#else
static struct pmic_wrap_setting pw = {
	.phase = NR_PMIC_WRAP_PHASE,	/* invalid setting for init */
	.addr = {{0, 0} },

	.set[PMIC_WRAP_PHASE_NORMAL] = {
		._[IDX_NM_RESERVE1] = {0, 0,},
		._[IDX_NM_RESERVE2] = {0, 0,},
		._[IDX_NM_VCORE_HPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(80000),},
		._[IDX_NM_VCORE_LPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_NM_VCORE_ULPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_NM_VCORE_RESERVE] = {0, 0,},
		.nr_idx = NR_IDX_NM,
	},

	.set[PMIC_WRAP_PHASE_SUSPEND] = {
		._[IDX_SP_VSRAM_PWR_ON] = { MT6351_PMIC_RG_VSRAM_PROC_EN_ADDR, _BITS_(1:1, 1),},
		._[IDX_SP_VSRAM_SHUTDOWN] = { MT6351_PMIC_RG_VSRAM_PROC_EN_ADDR, _BITS_(1:1, 0),},
		._[IDX_SP_VCORE_HPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(80000),},
		._[IDX_SP_VCORE_LPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_SP_VCORE_ULPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_SP_VCORE_RESERVE] = {0, 0,},
		.nr_idx = NR_IDX_SP,
	},

	.set[PMIC_WRAP_PHASE_DEEPIDLE] = {
		._[IDX_DI_VSRAM_NORMAL] = { MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR,
			VOLT_TO_PMIC_VAL(DEFAULT_VOLT_VSRAM),},
		._[IDX_DI_VSRAM_SLEEP] = { MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(60000),},
		._[IDX_DI_VCORE_HPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(80000),},
		._[IDX_DI_VCORE_LPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_DI_VCORE_ULPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_DI_VCORE_RESERVE] = {0, 0,},
		._[IDX_DI_SRCCLKEN_IN2_NORMAL] = { MT6351_PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(3:3, 1),},
		._[IDX_DI_SRCCLKEN_IN2_SLEEP] = { MT6351_PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(3:3, 0),},
		.nr_idx = NR_IDX_DI,
	},
};
#endif
#elif defined(CONFIG_MACH_MT6797)
static struct pmic_wrap_setting pw = {
	.phase = NR_PMIC_WRAP_PHASE,	/* invalid setting for init */
	.addr = {{0, 0} },

	.set[PMIC_WRAP_PHASE_NORMAL] = {
		._[IDX_NM_VCORE_HPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_NM_VCORE_TRANS1] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(98333),},
		._[IDX_NM_VCORE_TRANS2] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(96666),},
		._[IDX_NM_VCORE_TRANS3] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(94999),},
		._[IDX_NM_VCORE_TRANS4] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(93333),},
		._[IDX_NM_VCORE_TRANS5] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(91666),},
		._[IDX_NM_VCORE_LPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		._[IDX_NM_VCORE_TRANS6] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		._[IDX_NM_VCORE_TRANS7] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		._[IDX_NM_VCORE_ULPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		._[IDX_NM_PWM_MODE] = { MT6351_VGPU_ANA_CON0, _BITS_(2:0, 6),},
		._[IDX_NM_AUTO_MODE] = { MT6351_VGPU_ANA_CON0, _BITS_(2:0, 4),},
		.nr_idx = NR_IDX_NM,
	},

	.set[PMIC_WRAP_PHASE_SUSPEND] = {
		._[IDX_SP_VCORE_HPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_SP_VCORE_TRANS1] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(98333),},
		._[IDX_SP_VCORE_TRANS2] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(96666),},
		._[IDX_SP_VCORE_TRANS3] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(94999),},
		._[IDX_SP_VCORE_TRANS4] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(93333),},
		._[IDX_SP_VCORE_TRANS5] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(91666),},
		._[IDX_SP_VCORE_LPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		._[IDX_SP_VCORE_VOSEL_0P6V] = { MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_ADDR, VOLT_TO_PMIC_VAL(60000),},
		._[IDX_SP_VCORE_VSLEEP_SEL_0P6V] = { MT6351_PMIC_RG_VCORE_VSLEEP_SEL_ADDR, _BITS_(9:8, 3),},
		._[IDX_SP_VCORE_DPIDLE_SODI] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(77000),},
		._[IDX_SP_VSRAM_CORE_1P0V] = { MT6351_BUCK_VSRAM_PROC_CON4, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_SP_VSRAM_CORE_1P1V] = { MT6351_BUCK_VSRAM_PROC_CON4, VOLT_TO_PMIC_VAL(110000),},
		._[IDX_SP_VCORE_LQ_EN] = { MT6351_PMIC_RG_VCORE_VDIFF_ENLOWIQ_ADDR, _BITS_(1:1, 1),},
		._[IDX_SP_VCORE_LQ_DIS] = { MT6351_PMIC_RG_VCORE_VDIFF_ENLOWIQ_ADDR, _BITS_(1:1, 0),},
		._[IDX_SP_VCORE_VOSEL_0P7V] = { MT6351_PMIC_BUCK_VCORE_VOSEL_SLEEP_ADDR, VOLT_TO_PMIC_VAL(70000),},
		._[IDX_SP_VCORE_VSLEEP_SEL_0P7V] = { MT6351_PMIC_RG_VCORE_VSLEEP_SEL_ADDR, _BITS_(9:8, 0),},
		.nr_idx = NR_IDX_SP,
	},

	.set[PMIC_WRAP_PHASE_DEEPIDLE] = {
		._[IDX_DI_VCORE_HPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_DI_VCORE_TRANS1] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(98333),},
		._[IDX_DI_VCORE_TRANS2] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(96666),},
		._[IDX_DI_VCORE_TRANS3] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(94999),},
		._[IDX_DI_VCORE_TRANS4] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(93333),},
		._[IDX_DI_VCORE_TRANS5] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(91666),},
		._[IDX_DI_VCORE_LPM] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(90000),},
		._[IDX_DI_SRCCLKEN_IN2_NORMAL] = { MT6351_PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(3:3, 1),},
		._[IDX_DI_SRCCLKEN_IN2_SLEEP] = { MT6351_PMIC_RG_SRCLKEN_IN2_EN_ADDR, _BITS_(3:3, 0),},
		._[IDX_DI_VCORE_DPIDLE_SODI] = { MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR, VOLT_TO_PMIC_VAL(77000),},
		._[IDX_DI_VSRAM_CORE_1P0V] = { MT6351_BUCK_VSRAM_PROC_CON4, VOLT_TO_PMIC_VAL(100000),},
		._[IDX_DI_VSRAM_CORE_1P1V] = { MT6351_BUCK_VSRAM_PROC_CON4, VOLT_TO_PMIC_VAL(110000),},
		._[IDX_DI_VCORE_LQ_EN] = { MT6351_PMIC_RG_VCORE_VDIFF_ENLOWIQ_ADDR, _BITS_(1:1, 1),},
		._[IDX_DI_VCORE_LQ_DIS] = { MT6351_PMIC_RG_VCORE_VDIFF_ENLOWIQ_ADDR, _BITS_(1:1, 0),},
		.nr_idx = NR_IDX_DI,
	},
};
#else
#error "Does not support!"
#endif


static DEFINE_SPINLOCK(pmic_wrap_lock);
#define pmic_wrap_lock(flags) spin_lock_irqsave(&pmic_wrap_lock, flags)
#define pmic_wrap_unlock(flags) spin_unlock_irqrestore(&pmic_wrap_lock, flags)

#if 0
static DEFINE_MUTEX(pmic_wrap_mutex);

#define pmic_wrap_lock(flags) \
do { \
	/* to fix compile warning */  \
	flags = (unsigned long)&flags; \
	mutex_lock(&pmic_wrap_mutex); \
} while (0)

#define pmic_wrap_unlock(flags) \
do { \
	/* to fix compile warning */  \
	flags = (unsigned long)&flags; \
	mutex_unlock(&pmic_wrap_mutex); \
} while (0)
#endif

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

	/* 6755 has 16 PWRAP commands */
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

	WARN_ON(phase >= NR_PMIC_WRAP_PHASE);

#if 0				/* TODO: FIXME, check IPO-H case */

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

	WARN_ON(phase >= NR_PMIC_WRAP_PHASE);
	WARN_ON(idx >= pw.set[phase].nr_idx);

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

	WARN_ON(phase >= NR_PMIC_WRAP_PHASE);
	WARN_ON(idx >= pw.set[phase].nr_idx);

	/*
	 * spm_pwrap_info("@%s: phase = 0x%x, idx = %d, cmd_addr = 0x%x, cmd_wdata = 0x%x\n",
	 * __func__, phase, idx, cmd_addr, cmd_wdata);
	 */

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

	WARN_ON(phase >= NR_PMIC_WRAP_PHASE);
	WARN_ON(idx >= pw.set[phase].nr_idx);

	pmic_wrap_lock(flags);

	*p_cmd_addr = pw.set[phase]._[idx].cmd_addr;
	*p_cmd_wdata = pw.set[phase]._[idx].cmd_wdata;

	pmic_wrap_unlock(flags);
	/*
	 * spm_pwrap_info("@%s: phase = 0x%x, idx = %d, original cmd_addr = 0x%x, cmd_wdata = 0x%x\n",
	 *  __func__, phase, idx, *p_cmd_addr, *p_cmd_wdata);
	 */

}
EXPORT_SYMBOL(mt_spm_pmic_wrap_get_cmd_full);

void mt_spm_pmic_wrap_apply_cmd(int idx)
{				/* kick spm */
	unsigned long flags;

	WARN_ON(idx >= pw.set[pw.phase].nr_idx);

	/* spm_pwrap_info("@%s: idx = %d\n", __func__, idx); */

	pmic_wrap_lock(flags);

	_spm_dvfs_ctrl_volt(idx);

	pmic_wrap_unlock(flags);
}
EXPORT_SYMBOL(mt_spm_pmic_wrap_apply_cmd);

#if defined(CONFIG_MACH_MT6797)
void mt_spm_update_pmic_wrap(void)
{
	pw.set[PMIC_WRAP_PHASE_SUSPEND]._[IDX_DI_VSRAM_CORE_1P0V].cmd_addr = MT6351_BUCK_VSRAM_PROC_CON5;
	pw.set[PMIC_WRAP_PHASE_SUSPEND]._[IDX_DI_VSRAM_CORE_1P1V].cmd_addr = MT6351_BUCK_VSRAM_PROC_CON5;
	pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[IDX_DI_VSRAM_CORE_1P0V].cmd_addr = MT6351_BUCK_VSRAM_PROC_CON5;
	pw.set[PMIC_WRAP_PHASE_DEEPIDLE]._[IDX_DI_VSRAM_CORE_1P1V].cmd_addr = MT6351_BUCK_VSRAM_PROC_CON5;
}
EXPORT_SYMBOL(mt_spm_pmic_wrap_update);
#endif

int mt_spm_pmic_wrap_init(void)
{
	if (pw.addr[0].cmd_addr == 0)
		_mt_spm_pmic_table_init();

	/* init PMIC_WRAP & volt */
	mt_spm_pmic_wrap_set_phase(PMIC_WRAP_PHASE_NORMAL);

	return 0;
}

MODULE_DESCRIPTION("SPM-PMIC_WRAP Driver v0.1");
