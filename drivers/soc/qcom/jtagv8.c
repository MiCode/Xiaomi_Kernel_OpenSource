/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/coresight.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/jtag.h>
#ifdef CONFIG_ARM64
#include <asm/debugv8.h>
#else
#include <asm/hardware/debugv8.h>
#endif

#define CORESIGHT_LAR		(0xFB0)

#define CORESIGHT_UNLOCK	(0xC5ACCE55)

#define TIMEOUT_US		(100)

#define BM(lsb, msb)		((BIT(msb) - BIT(lsb)) + BIT(msb))
#define BMVAL(val, lsb, msb)	((val & BM(lsb, msb)) >> lsb)
#define BVAL(val, n)		((val & BIT(n)) >> n)

#define ARM_DEBUG_ARCH_V8	(0x6)

#define MAX_DBG_REGS		(66)
#define MAX_DBG_STATE_SIZE	(MAX_DBG_REGS * num_possible_cpus())

#define OSLOCK_MAGIC		(0xC5ACCE55)
#define TZ_DBG_ETM_FEAT_ID	(0x8)
#define TZ_DBG_ETM_VER		(0x400000)

uint32_t msm_jtag_save_cntr[NR_CPUS];
uint32_t msm_jtag_restore_cntr[NR_CPUS];

/* access debug registers using system instructions */
struct dbg_cpu_ctx {
	uint32_t		*state;
};

struct dbg_ctx {
	uint8_t			arch;
	bool			save_restore_enabled;
	uint8_t			nr_wp;
	uint8_t			nr_bp;
	uint8_t			nr_ctx_cmp;
#ifdef CONFIG_ARM64
	uint64_t		*state;
#else
	uint32_t		*state;
#endif
};

static struct dbg_ctx dbg;
static struct notifier_block jtag_hotcpu_save_notifier;
static struct notifier_block jtag_hotcpu_restore_notifier;
static struct notifier_block jtag_cpu_pm_notifier;

#ifdef CONFIG_ARM64
static int dbg_read_arch64_bxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = dbg_readq(DBGBVR0_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR0_EL1);
		break;
	case 1:
		state[i++] = dbg_readq(DBGBVR1_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR1_EL1);
		break;
	case 2:
		state[i++] = dbg_readq(DBGBVR2_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR2_EL1);
		break;
	case 3:
		state[i++] = dbg_readq(DBGBVR3_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR3_EL1);
		break;
	case 4:
		state[i++] = dbg_readq(DBGBVR4_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR4_EL1);
		break;
	case 5:
		state[i++] = dbg_readq(DBGBVR5_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR5_EL1);
		break;
	case 6:
		state[i++] = dbg_readq(DBGBVR6_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR6_EL1);
		break;
	case 7:
		state[i++] = dbg_readq(DBGBVR7_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR7_EL1);
		break;
	case 8:
		state[i++] = dbg_readq(DBGBVR8_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR8_EL1);
		break;
	case 9:
		state[i++] = dbg_readq(DBGBVR9_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR9_EL1);
		break;
	case 10:
		state[i++] = dbg_readq(DBGBVR10_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR10_EL1);
		break;
	case 11:
		state[i++] = dbg_readq(DBGBVR11_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR11_EL1);
		break;
	case 12:
		state[i++] = dbg_readq(DBGBVR12_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR12_EL1);
		break;
	case 13:
		state[i++] = dbg_readq(DBGBVR13_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR13_EL1);
		break;
	case 14:
		state[i++] = dbg_readq(DBGBVR14_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR14_EL1);
		break;
	case 15:
		state[i++] = dbg_readq(DBGBVR15_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGBCR15_EL1);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int dbg_write_arch64_bxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		dbg_write(state[i++], DBGBVR0_EL1);
		dbg_write(state[i++], DBGBCR0_EL1);
		break;
	case 1:
		dbg_write(state[i++], DBGBVR1_EL1);
		dbg_write(state[i++], DBGBCR1_EL1);
		break;
	case 2:
		dbg_write(state[i++], DBGBVR2_EL1);
		dbg_write(state[i++], DBGBCR2_EL1);
		break;
	case 3:
		dbg_write(state[i++], DBGBVR3_EL1);
		dbg_write(state[i++], DBGBCR3_EL1);
		break;
	case 4:
		dbg_write(state[i++], DBGBVR4_EL1);
		dbg_write(state[i++], DBGBCR4_EL1);
		break;
	case 5:
		dbg_write(state[i++], DBGBVR5_EL1);
		dbg_write(state[i++], DBGBCR5_EL1);
		break;
	case 6:
		dbg_write(state[i++], DBGBVR6_EL1);
		dbg_write(state[i++], DBGBCR6_EL1);
		break;
	case 7:
		dbg_write(state[i++], DBGBVR7_EL1);
		dbg_write(state[i++], DBGBCR7_EL1);
		break;
	case 8:
		dbg_write(state[i++], DBGBVR8_EL1);
		dbg_write(state[i++], DBGBCR8_EL1);
		break;
	case 9:
		dbg_write(state[i++], DBGBVR9_EL1);
		dbg_write(state[i++], DBGBCR9_EL1);
		break;
	case 10:
		dbg_write(state[i++], DBGBVR10_EL1);
		dbg_write(state[i++], DBGBCR10_EL1);
		break;
	case 11:
		dbg_write(state[i++], DBGBVR11_EL1);
		dbg_write(state[i++], DBGBCR11_EL1);
		break;
	case 12:
		dbg_write(state[i++], DBGBVR12_EL1);
		dbg_write(state[i++], DBGBCR12_EL1);
		break;
	case 13:
		dbg_write(state[i++], DBGBVR13_EL1);
		dbg_write(state[i++], DBGBCR13_EL1);
		break;
	case 14:
		dbg_write(state[i++], DBGBVR14_EL1);
		dbg_write(state[i++], DBGBCR14_EL1);
		break;
	case 15:
		dbg_write(state[i++], DBGBVR15_EL1);
		dbg_write(state[i++], DBGBCR15_EL1);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int dbg_read_arch64_wxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = dbg_readq(DBGWVR0_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR0_EL1);
		break;
	case 1:
		state[i++] = dbg_readq(DBGWVR1_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR1_EL1);
		break;
	case 2:
		state[i++] = dbg_readq(DBGWVR2_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR2_EL1);
		break;
	case 3:
		state[i++] = dbg_readq(DBGWVR3_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR3_EL1);
		break;
	case 4:
		state[i++] = dbg_readq(DBGWVR4_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR4_EL1);
		break;
	case 5:
		state[i++] = dbg_readq(DBGWVR5_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR5_EL1);
		break;
	case 6:
		state[i++] = dbg_readq(DBGWVR6_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR6_EL1);
		break;
	case 7:
		state[i++] = dbg_readq(DBGWVR7_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR7_EL1);
		break;
	case 8:
		state[i++] = dbg_readq(DBGWVR8_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR8_EL1);
		break;
	case 9:
		state[i++] = dbg_readq(DBGWVR9_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR9_EL1);
		break;
	case 10:
		state[i++] = dbg_readq(DBGWVR10_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR10_EL1);
		break;
	case 11:
		state[i++] = dbg_readq(DBGWVR11_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR11_EL1);
		break;
	case 12:
		state[i++] = dbg_readq(DBGWVR12_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR12_EL1);
		break;
	case 13:
		state[i++] = dbg_readq(DBGWVR13_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR13_EL1);
		break;
	case 14:
		state[i++] = dbg_readq(DBGWVR14_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR14_EL1);
		break;
	case 15:
		state[i++] = dbg_readq(DBGWVR15_EL1);
		state[i++] = (uint64_t)dbg_readl(DBGWCR15_EL1);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int dbg_write_arch64_wxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		dbg_write(state[i++], DBGWVR0_EL1);
		dbg_write(state[i++], DBGWCR0_EL1);
		break;
	case 1:
		dbg_write(state[i++], DBGWVR1_EL1);
		dbg_write(state[i++], DBGWCR1_EL1);
		break;
	case 2:
		dbg_write(state[i++], DBGWVR2_EL1);
		dbg_write(state[i++], DBGWCR2_EL1);
		break;
	case 3:
		dbg_write(state[i++], DBGWVR3_EL1);
		dbg_write(state[i++], DBGWCR3_EL1);
		break;
	case 4:
		dbg_write(state[i++], DBGWVR4_EL1);
		dbg_write(state[i++], DBGWCR4_EL1);
		break;
	case 5:
		dbg_write(state[i++], DBGWVR5_EL1);
		dbg_write(state[i++], DBGWCR5_EL1);
		break;
	case 6:
		dbg_write(state[i++], DBGWVR0_EL1);
		dbg_write(state[i++], DBGWCR6_EL1);
		break;
	case 7:
		dbg_write(state[i++], DBGWVR7_EL1);
		dbg_write(state[i++], DBGWCR7_EL1);
		break;
	case 8:
		dbg_write(state[i++], DBGWVR8_EL1);
		dbg_write(state[i++], DBGWCR8_EL1);
		break;
	case 9:
		dbg_write(state[i++], DBGWVR9_EL1);
		dbg_write(state[i++], DBGWCR9_EL1);
		break;
	case 10:
		dbg_write(state[i++], DBGWVR10_EL1);
		dbg_write(state[i++], DBGWCR10_EL1);
		break;
	case 11:
		dbg_write(state[i++], DBGWVR11_EL1);
		dbg_write(state[i++], DBGWCR11_EL1);
		break;
	case 12:
		dbg_write(state[i++], DBGWVR12_EL1);
		dbg_write(state[i++], DBGWCR12_EL1);
		break;
	case 13:
		dbg_write(state[i++], DBGWVR13_EL1);
		dbg_write(state[i++], DBGWCR13_EL1);
		break;
	case 14:
		dbg_write(state[i++], DBGWVR14_EL1);
		dbg_write(state[i++], DBGWCR14_EL1);
		break;
	case 15:
		dbg_write(state[i++], DBGWVR15_EL1);
		dbg_write(state[i++], DBGWCR15_EL1);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static inline void dbg_save_state(int cpu)
{
	int i, j;

	i = cpu * MAX_DBG_REGS;

	switch (dbg.arch) {
	case ARM_DEBUG_ARCH_V8:
		/* Set OS Lock to inform the debugger that the OS is in the
		 * process of saving debug registers. It prevents accidental
		 * modification of the debug regs by the external debugger.
		 */
		dbg_write(0x1, OSLAR_EL1);
		/* Ensure OS lock is set before proceeding */
		isb();

		dbg.state[i++] =  (uint32_t)dbg_readl(MDSCR_EL1);
		for (j = 0; j < dbg.nr_bp; j++)
			i = dbg_read_arch64_bxr((uint64_t *)dbg.state, i, j);
		for (j = 0; j < dbg.nr_wp; j++)
			i = dbg_read_arch64_wxr((uint64_t *)dbg.state, i, j);
		dbg.state[i++] =  (uint32_t)dbg_readl(MDCCINT_EL1);
		dbg.state[i++] =  (uint32_t)dbg_readl(DBGCLAIMCLR_EL1);
		dbg.state[i++] =  (uint32_t)dbg_readl(OSECCR_EL1);
		dbg.state[i++] =  (uint32_t)dbg_readl(OSDTRRX_EL1);
		dbg.state[i++] =  (uint32_t)dbg_readl(OSDTRTX_EL1);

		/* Set the OS double lock */
		isb();
		dbg_write(0x1, OSDLR_EL1);
		isb();
		break;
	default:
		pr_err_ratelimited("unsupported dbg arch %d in %s\n", dbg.arch,
				   __func__);
	}
}

static inline void dbg_restore_state(int cpu)
{
	int i, j;

	i = cpu * MAX_DBG_REGS;

	switch (dbg.arch) {
	case ARM_DEBUG_ARCH_V8:
		/* Clear the OS double lock */
		isb();
		dbg_write(0x0, OSDLR_EL1);
		isb();

		/* Set OS lock. Lock will already be set after power collapse
		 * but this write is included to ensure it is set.
		 */
		dbg_write(0x1, OSLAR_EL1);
		isb();

		dbg_write(dbg.state[i++], MDSCR_EL1);
		for (j = 0; j < dbg.nr_bp; j++)
			i = dbg_write_arch64_bxr((uint64_t *)dbg.state, i, j);
		for (j = 0; j < dbg.nr_wp; j++)
			i = dbg_write_arch64_wxr((uint64_t *)dbg.state, i, j);
		dbg_write(dbg.state[i++], MDCCINT_EL1);
		dbg_write(dbg.state[i++], DBGCLAIMSET_EL1);
		dbg_write(dbg.state[i++], OSECCR_EL1);
		dbg_write(dbg.state[i++], OSDTRRX_EL1);
		dbg_write(dbg.state[i++], OSDTRTX_EL1);

		isb();
		dbg_write(0x0, OSLAR_EL1);
		isb();
		break;
	default:
		pr_err_ratelimited("unsupported dbg arch %d in %s\n", dbg.arch,
				   __func__);
	}
}

static void dbg_init_arch_data(void)
{
	uint64_t dbgfr;

	/* This will run on core0 so use it to populate parameters */
	dbgfr = dbg_readq(ID_AA64DFR0_EL1);
	dbg.arch = BMVAL(dbgfr, 0, 3);
	dbg.nr_bp = BMVAL(dbgfr, 12, 15) + 1;
	dbg.nr_wp = BMVAL(dbgfr, 20, 23) + 1;
	dbg.nr_ctx_cmp = BMVAL(dbgfr, 28, 31) + 1;
}
#else

static int dbg_read_arch32_bxr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = dbg_read(DBGBVR0);
		state[i++] = dbg_read(DBGBCR0);
		break;
	case 1:
		state[i++] = dbg_read(DBGBVR1);
		state[i++] = dbg_read(DBGBCR1);
		break;
	case 2:
		state[i++] = dbg_read(DBGBVR2);
		state[i++] = dbg_read(DBGBCR2);
		break;
	case 3:
		state[i++] = dbg_read(DBGBVR3);
		state[i++] = dbg_read(DBGBCR3);
		break;
	case 4:
		state[i++] = dbg_read(DBGBVR4);
		state[i++] = dbg_read(DBGBCR4);
		break;
	case 5:
		state[i++] = dbg_read(DBGBVR5);
		state[i++] = dbg_read(DBGBCR5);
		break;
	case 6:
		state[i++] = dbg_read(DBGBVR6);
		state[i++] = dbg_read(DBGBCR6);
		break;
	case 7:
		state[i++] = dbg_read(DBGBVR7);
		state[i++] = dbg_read(DBGBCR7);
		break;
	case 8:
		state[i++] = dbg_read(DBGBVR8);
		state[i++] = dbg_read(DBGBCR8);
		break;
	case 9:
		state[i++] = dbg_read(DBGBVR9);
		state[i++] = dbg_read(DBGBCR9);
		break;
	case 10:
		state[i++] = dbg_read(DBGBVR10);
		state[i++] = dbg_read(DBGBCR10);
		break;
	case 11:
		state[i++] = dbg_read(DBGBVR11);
		state[i++] = dbg_read(DBGBCR11);
		break;
	case 12:
		state[i++] = dbg_read(DBGBVR12);
		state[i++] = dbg_read(DBGBCR12);
		break;
	case 13:
		state[i++] = dbg_read(DBGBVR13);
		state[i++] = dbg_read(DBGBCR13);
		break;
	case 14:
		state[i++] = dbg_read(DBGBVR14);
		state[i++] = dbg_read(DBGBCR14);
		break;
	case 15:
		state[i++] = dbg_read(DBGBVR15);
		state[i++] = dbg_read(DBGBCR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int dbg_write_arch32_bxr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		dbg_write(state[i++], DBGBVR0);
		dbg_write(state[i++], DBGBCR0);
		break;
	case 1:
		dbg_write(state[i++], DBGBVR1);
		dbg_write(state[i++], DBGBCR1);
		break;
	case 2:
		dbg_write(state[i++], DBGBVR2);
		dbg_write(state[i++], DBGBCR2);
		break;
	case 3:
		dbg_write(state[i++], DBGBVR3);
		dbg_write(state[i++], DBGBCR3);
		break;
	case 4:
		dbg_write(state[i++], DBGBVR4);
		dbg_write(state[i++], DBGBCR4);
		break;
	case 5:
		dbg_write(state[i++], DBGBVR5);
		dbg_write(state[i++], DBGBCR5);
		break;
	case 6:
		dbg_write(state[i++], DBGBVR6);
		dbg_write(state[i++], DBGBCR6);
		break;
	case 7:
		dbg_write(state[i++], DBGBVR7);
		dbg_write(state[i++], DBGBCR7);
		break;
	case 8:
		dbg_write(state[i++], DBGBVR8);
		dbg_write(state[i++], DBGBCR8);
		break;
	case 9:
		dbg_write(state[i++], DBGBVR9);
		dbg_write(state[i++], DBGBCR9);
		break;
	case 10:
		dbg_write(state[i++], DBGBVR10);
		dbg_write(state[i++], DBGBCR10);
		break;
	case 11:
		dbg_write(state[i++], DBGBVR11);
		dbg_write(state[i++], DBGBCR11);
		break;
	case 12:
		dbg_write(state[i++], DBGBVR12);
		dbg_write(state[i++], DBGBCR12);
		break;
	case 13:
		dbg_write(state[i++], DBGBVR13);
		dbg_write(state[i++], DBGBCR13);
		break;
	case 14:
		dbg_write(state[i++], DBGBVR14);
		dbg_write(state[i++], DBGBCR14);
		break;
	case 15:
		dbg_write(state[i++], DBGBVR15);
		dbg_write(state[i++], DBGBCR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int dbg_read_arch32_wxr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = dbg_read(DBGWVR0);
		state[i++] = dbg_read(DBGWCR0);
		break;
	case 1:
		state[i++] = dbg_read(DBGWVR1);
		state[i++] = dbg_read(DBGWCR1);
		break;
	case 2:
		state[i++] = dbg_read(DBGWVR2);
		state[i++] = dbg_read(DBGWCR2);
		break;
	case 3:
		state[i++] = dbg_read(DBGWVR3);
		state[i++] = dbg_read(DBGWCR3);
		break;
	case 4:
		state[i++] = dbg_read(DBGWVR4);
		state[i++] = dbg_read(DBGWCR4);
		break;
	case 5:
		state[i++] = dbg_read(DBGWVR5);
		state[i++] = dbg_read(DBGWCR5);
		break;
	case 6:
		state[i++] = dbg_read(DBGWVR6);
		state[i++] = dbg_read(DBGWCR6);
		break;
	case 7:
		state[i++] = dbg_read(DBGWVR7);
		state[i++] = dbg_read(DBGWCR7);
		break;
	case 8:
		state[i++] = dbg_read(DBGWVR8);
		state[i++] = dbg_read(DBGWCR8);
		break;
	case 9:
		state[i++] = dbg_read(DBGWVR9);
		state[i++] = dbg_read(DBGWCR9);
		break;
	case 10:
		state[i++] = dbg_read(DBGWVR10);
		state[i++] = dbg_read(DBGWCR10);
		break;
	case 11:
		state[i++] = dbg_read(DBGWVR11);
		state[i++] = dbg_read(DBGWCR11);
		break;
	case 12:
		state[i++] = dbg_read(DBGWVR12);
		state[i++] = dbg_read(DBGWCR12);
		break;
	case 13:
		state[i++] = dbg_read(DBGWVR13);
		state[i++] = dbg_read(DBGWCR13);
		break;
	case 14:
		state[i++] = dbg_read(DBGWVR14);
		state[i++] = dbg_read(DBGWCR14);
		break;
	case 15:
		state[i++] = dbg_read(DBGWVR15);
		state[i++] = dbg_read(DBGWCR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int dbg_write_arch32_wxr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		dbg_write(state[i++], DBGWVR0);
		dbg_write(state[i++], DBGWCR0);
		break;
	case 1:
		dbg_write(state[i++], DBGWVR1);
		dbg_write(state[i++], DBGWCR1);
		break;
	case 2:
		dbg_write(state[i++], DBGWVR2);
		dbg_write(state[i++], DBGWCR2);
		break;
	case 3:
		dbg_write(state[i++], DBGWVR3);
		dbg_write(state[i++], DBGWCR3);
		break;
	case 4:
		dbg_write(state[i++], DBGWVR4);
		dbg_write(state[i++], DBGWCR4);
		break;
	case 5:
		dbg_write(state[i++], DBGWVR5);
		dbg_write(state[i++], DBGWCR5);
		break;
	case 6:
		dbg_write(state[i++], DBGWVR6);
		dbg_write(state[i++], DBGWCR6);
		break;
	case 7:
		dbg_write(state[i++], DBGWVR7);
		dbg_write(state[i++], DBGWCR7);
		break;
	case 8:
		dbg_write(state[i++], DBGWVR8);
		dbg_write(state[i++], DBGWCR8);
		break;
	case 9:
		dbg_write(state[i++], DBGWVR9);
		dbg_write(state[i++], DBGWCR9);
		break;
	case 10:
		dbg_write(state[i++], DBGWVR10);
		dbg_write(state[i++], DBGWCR10);
		break;
	case 11:
		dbg_write(state[i++], DBGWVR11);
		dbg_write(state[i++], DBGWCR11);
		break;
	case 12:
		dbg_write(state[i++], DBGWVR12);
		dbg_write(state[i++], DBGWCR12);
		break;
	case 13:
		dbg_write(state[i++], DBGWVR13);
		dbg_write(state[i++], DBGWCR13);
		break;
	case 14:
		dbg_write(state[i++], DBGWVR14);
		dbg_write(state[i++], DBGWCR14);
		break;
	case 15:
		dbg_write(state[i++], DBGWVR15);
		dbg_write(state[i++], DBGWCR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static inline void dbg_save_state(int cpu)
{
	int i, j;

	i = cpu * MAX_DBG_REGS;

	switch (dbg.arch) {
	case ARM_DEBUG_ARCH_V8:
		/* Set OS Lock to inform the debugger that the OS is in the
		 * process of saving debug registers. It prevents accidental
		 * modification of the debug regs by the external debugger.
		 */
		dbg_write(OSLOCK_MAGIC, DBGOSLAR);
		/* Ensure OS lock is set before proceeding */
		isb();

		dbg.state[i++] =  dbg_read(DBGDSCRext);
		for (j = 0; j < dbg.nr_bp; j++)
			i = dbg_read_arch32_bxr(dbg.state, i, j);
		for (j = 0; j < dbg.nr_wp; j++)
			i = dbg_read_arch32_wxr(dbg.state, i, j);
		dbg.state[i++] =  dbg_read(DBGDCCINT);
		dbg.state[i++] =  dbg_read(DBGCLAIMCLR);
		dbg.state[i++] =  dbg_read(DBGOSECCR);
		dbg.state[i++] =  dbg_read(DBGDTRRXext);
		dbg.state[i++] =  dbg_read(DBGDTRTXext);

		/* Set the OS double lock */
		isb();
		dbg_write(0x1, DBGOSDLR);
		isb();
		break;
	default:
		pr_err_ratelimited("unsupported dbg arch %d in %s\n", dbg.arch,
				   __func__);
	}
}

static inline void dbg_restore_state(int cpu)
{
	int i, j;

	i = cpu * MAX_DBG_REGS;

	switch (dbg.arch) {
	case ARM_DEBUG_ARCH_V8:
		/* Clear the OS double lock */
		isb();
		dbg_write(0x0, DBGOSDLR);
		isb();

		/* Set OS lock. Lock will already be set after power collapse
		 * but this write is included to ensure it is set.
		 */
		dbg_write(OSLOCK_MAGIC, DBGOSLAR);
		isb();

		dbg_write(dbg.state[i++], DBGDSCRext);
		for (j = 0; j < dbg.nr_bp; j++)
			i = dbg_write_arch32_bxr((uint32_t *)dbg.state, i, j);
		for (j = 0; j < dbg.nr_wp; j++)
			i = dbg_write_arch32_wxr((uint32_t *)dbg.state, i, j);
		dbg_write(dbg.state[i++], DBGDCCINT);
		dbg_write(dbg.state[i++], DBGCLAIMSET);
		dbg_write(dbg.state[i++], DBGOSECCR);
		dbg_write(dbg.state[i++], DBGDTRRXext);
		dbg_write(dbg.state[i++], DBGDTRTXext);

		isb();
		dbg_write(0x0, DBGOSLAR);
		isb();
		break;
	default:
		pr_err_ratelimited("unsupported dbg arch %d in %s\n", dbg.arch,
				   __func__);
	}
}

static void dbg_init_arch_data(void)
{
	uint32_t dbgdidr;

	/* This will run on core0 so use it to populate parameters */
	dbgdidr = dbg_read(DBGDIDR);
	dbg.arch = BMVAL(dbgdidr, 16, 19);
	dbg.nr_ctx_cmp = BMVAL(dbgdidr, 20, 23) + 1;
	dbg.nr_bp = BMVAL(dbgdidr, 24, 27) + 1;
	dbg.nr_wp = BMVAL(dbgdidr, 28, 31) + 1;
}
#endif

/*
 * msm_jtag_save_state - save debug registers
 *
 * Debug registers are saved before power collapse if debug
 * architecture is supported respectively and TZ isn't supporting
 * the save and restore of debug registers.
 *
 * CONTEXT:
 * Called with preemption off and interrupts locked from:
 * 1. per_cpu idle thread context for idle power collapses
 * or
 * 2. per_cpu idle thread context for hotplug/suspend power collapse
 *    for nonboot cpus
 * or
 * 3. suspend thread context for suspend power collapse for core0
 *
 * In all cases we will run on the same cpu for the entire duration.
 */
void msm_jtag_save_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	msm_jtag_save_cntr[cpu]++;
	/* ensure counter is updated before moving forward */
	mb();

	msm_jtag_mm_save_state();
	if (dbg.save_restore_enabled)
		dbg_save_state(cpu);
}
EXPORT_SYMBOL(msm_jtag_save_state);

void msm_jtag_restore_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	/* Attempt restore only if save has been done. If power collapse
	 * is disabled, hotplug off of non-boot core will result in WFI
	 * and hence msm_jtag_save_state will not occur. Subsequently,
	 * during hotplug on of non-boot core when msm_jtag_restore_state
	 * is called via msm_platform_secondary_init, this check will help
	 * bail us out without restoring.
	 */
	if (msm_jtag_save_cntr[cpu] == msm_jtag_restore_cntr[cpu])
		return;
	else if (msm_jtag_save_cntr[cpu] != msm_jtag_restore_cntr[cpu] + 1)
		pr_err_ratelimited("jtag imbalance, save:%lu, restore:%lu\n",
				   (unsigned long)msm_jtag_save_cntr[cpu],
				   (unsigned long)msm_jtag_restore_cntr[cpu]);

	msm_jtag_restore_cntr[cpu]++;
	/* ensure counter is updated before moving forward */
	mb();

	if (dbg.save_restore_enabled)
		dbg_restore_state(cpu);
	msm_jtag_mm_restore_state();
}
EXPORT_SYMBOL(msm_jtag_restore_state);

static inline bool dbg_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ARM_DEBUG_ARCH_V8:
		break;
	default:
		return false;
	}
	return true;
}

static int jtag_hotcpu_save_callback(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	switch (action & (~CPU_TASKS_FROZEN)) {
	case CPU_DYING:
		msm_jtag_save_state();
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block jtag_hotcpu_save_notifier = {
	.notifier_call = jtag_hotcpu_save_callback,
};

static int jtag_hotcpu_restore_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	switch (action & (~CPU_TASKS_FROZEN)) {
	case CPU_STARTING:
		msm_jtag_restore_state();
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block jtag_hotcpu_restore_notifier = {
	.notifier_call = jtag_hotcpu_restore_callback,
	.priority = 1,
};

static int jtag_cpu_pm_callback(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_PM_ENTER:
		msm_jtag_save_state();
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		msm_jtag_restore_state();
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block jtag_cpu_pm_notifier = {
	.notifier_call = jtag_cpu_pm_callback,
};

static int __init msm_jtag_dbg_init(void)
{
	int ret;

	if (msm_jtag_fuse_apps_access_disabled())
		return -EPERM;

	/* This will run on core0 so use it to populate parameters */
	dbg_init_arch_data();

	if (dbg_arch_supported(dbg.arch)) {
		if (scm_get_feat_version(TZ_DBG_ETM_FEAT_ID) < TZ_DBG_ETM_VER) {
			dbg.save_restore_enabled = true;
		} else {
			pr_info("dbg save-restore supported by TZ\n");
			goto dbg_out;
		}
	} else {
		pr_info("dbg arch %u not supported\n", dbg.arch);
		goto dbg_out;
	}

	/* Allocate dbg state save space */
#ifdef CONFIG_ARM64
	dbg.state = kzalloc(MAX_DBG_STATE_SIZE * sizeof(uint64_t), GFP_KERNEL);
#else
	dbg.state = kzalloc(MAX_DBG_STATE_SIZE * sizeof(uint32_t), GFP_KERNEL);
#endif
	if (!dbg.state) {
		ret = -ENOMEM;
		goto dbg_err;
	}

	register_hotcpu_notifier(&jtag_hotcpu_save_notifier);
	register_hotcpu_notifier(&jtag_hotcpu_restore_notifier);
	cpu_pm_register_notifier(&jtag_cpu_pm_notifier);
dbg_out:
	return 0;
dbg_err:
	return ret;
}
arch_initcall(msm_jtag_dbg_init);
