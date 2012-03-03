/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/init.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <mach/scm.h>

#include "qdss.h"
#include "cp14.h"

 /* no of dbg regs + 1 (for storing the reg count) */
#define MAX_DBG_REGS		(90)
#define MAX_DBG_STATE_SIZE	(MAX_DBG_REGS * num_possible_cpus())
 /* no of etm regs + 1 (for storing the reg count) */
#define MAX_ETM_REGS		(78)
#define MAX_ETM_STATE_SIZE	(MAX_ETM_REGS * num_possible_cpus())

#define DBGDSCR_MASK		(0x6C30FC3C)
#define CPMR_ETMCLKEN		(0x8)
#define TZ_DBG_ETM_FEAT_ID	(0x8)
#define TZ_DBG_ETM_VER		(0x400000)


uint32_t msm_jtag_save_cntr[NR_CPUS];
uint32_t msm_jtag_restore_cntr[NR_CPUS];

struct dbg_ctx {
	uint8_t		arch;
	bool		save_restore_enabled;
	uint8_t		nr_wp;
	uint8_t		nr_bp;
	uint8_t		nr_ctx_cmp;
	uint32_t	*state;
};
static struct dbg_ctx dbg;

struct etm_ctx {
	uint8_t		arch;
	bool		save_restore_enabled;
	uint8_t		nr_addr_cmp;
	uint8_t		nr_cntr;
	uint8_t		nr_ext_inp;
	uint8_t		nr_ext_out;
	uint8_t		nr_ctxid_cmp;
	uint32_t	*state;
};
static struct etm_ctx etm;

static int dbg_read_bxr(uint32_t *state, int i, int j)
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

static int dbg_write_bxr(uint32_t *state, int i, int j)
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

static int dbg_read_wxr(uint32_t *state, int i, int j)
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

static int dbg_write_wxr(uint32_t *state, int i, int j)
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

static inline bool dbg_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ARM_DEBUG_ARCH_V7_1:
	case ARM_DEBUG_ARCH_V7:
	case ARM_DEBUG_ARCH_V7B:
		break;
	default:
		return false;
	}
	return true;
}


static inline void dbg_save_state(int cpu)
{
	int i, j, cnt;

	i = cpu * MAX_DBG_REGS;

	switch (dbg.arch) {
	case ARM_DEBUG_ARCH_V7_1:
		/* Set OS lock to inform the debugger that the OS is in the
		 * process of saving debug registers. It prevents accidental
		 * modification of the debug regs by the external debugger.
		 */
		dbg_write(OSLOCK_MAGIC, DBGOSLAR);
		isb();

		/* We skip saving DBGBXVRn since not supported on Krait */

		dbg.state[i++] = dbg_read(DBGWFAR);
		for (j = 0; j < dbg.nr_bp; j++)
			i = dbg_read_bxr(dbg.state, i, j);
		for (j = 0; j < dbg.nr_wp; j++)
			i = dbg_read_wxr(dbg.state, i, j);
		dbg.state[i++] = dbg_read(DBGVCR);
		dbg.state[i++] = dbg_read(DBGCLAIMCLR);
		dbg.state[i++] = dbg_read(DBGDTRTXext);
		dbg.state[i++] = dbg_read(DBGDTRRXext);
		dbg.state[i++] = dbg_read(DBGDSCRext);

		/* Set the OS double lock */
		isb();
		dbg_write(0x1, DBGOSDLR);
		isb();
		break;
	case ARM_DEBUG_ARCH_V7B:
	case ARM_DEBUG_ARCH_V7:
		/* Set OS lock to inform the debugger that the OS is in the
		 * process of saving dbg registers. It prevents accidental
		 * modification of the dbg regs by the external debugger
		 * and resets the internal counter.
		 */
		dbg_write(OSLOCK_MAGIC, DBGOSLAR);
		isb();

		cnt = dbg_read(DBGOSSRR); /* save count for restore */
		/* MAX_DBG_REGS = no of dbg regs + 1 (for storing the reg count)
		 * check for state overflow, if not enough space, don't save
		 */
		if (cnt >= MAX_DBG_REGS)
			cnt = 0;
		dbg.state[i++] = cnt;
		for (j = 0; j < cnt; j++)
			dbg.state[i++] = dbg_read(DBGOSSRR);
		break;
	default:
		pr_err_ratelimited("unsupported dbg arch %d in %s\n", dbg.arch,
								__func__);
	}
}

static inline void dbg_restore_state(int cpu)
{
	int i, j, cnt;

	i = cpu * MAX_DBG_REGS;

	switch (dbg.arch) {
	case ARM_DEBUG_ARCH_V7_1:
		/* Clear the OS double lock */
		isb();
		dbg_write(0x0, DBGOSDLR);
		isb();

		/* Set OS lock. Lock will already be set after power collapse
		 * but this write is included to ensure it is set.
		 */
		dbg_write(OSLOCK_MAGIC, DBGOSLAR);
		isb();

		/* We skip restoring DBGBXVRn since not supported on Krait */

		dbg_write(dbg.state[i++], DBGWFAR);
		for (j = 0; j < dbg.nr_bp; j++)
			i = dbg_write_bxr(dbg.state, i, j);
		for (j = 0; j < dbg.nr_wp; j++)
			i = dbg_write_wxr(dbg.state, i, j);
		dbg_write(dbg.state[i++], DBGVCR);
		dbg_write(dbg.state[i++], DBGCLAIMSET);
		dbg_write(dbg.state[i++], DBGDTRTXext);
		dbg_write(dbg.state[i++], DBGDTRRXext);
		dbg_write(dbg.state[i++] & DBGDSCR_MASK, DBGDSCRext);

		isb();
		dbg_write(0x0, DBGOSLAR);
		isb();
		break;
	case ARM_DEBUG_ARCH_V7B:
	case ARM_DEBUG_ARCH_V7:
		/* Clear sticky bit */
		dbg_read(DBGPRSR);
		isb();

		/* Set OS lock. Lock will already be set after power collapse
		 * but this write is required to reset the internal counter used
		 * for DBG state restore.
		 */
		dbg_write(OSLOCK_MAGIC, DBGOSLAR);
		isb();

		dbg_read(DBGOSSRR); /* dummy read of OSSRR */
		cnt = dbg.state[i++];
		for (j = 0; j < cnt; j++) {
			/* DBGDSCR special case
			 * DBGDSCR = DBGDSCR & DBGDSCR_MASK
			 */
			if (j == 20)
				dbg_write(dbg.state[i++] & DBGDSCR_MASK,
								DBGOSSRR);
			else
				dbg_write(dbg.state[i++], DBGOSSRR);
		}

		/* Clear the OS lock */
		isb();
		dbg_write(0x0, DBGOSLAR);
		isb();
		break;
	default:
		pr_err_ratelimited("unsupported dbg arch %d in %s\n", dbg.arch,
								__func__);
	}

}

static int etm_read_acxr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = etm_read(ETMACVR0);
		state[i++] = etm_read(ETMACTR0);
		break;
	case 1:
		state[i++] = etm_read(ETMACVR1);
		state[i++] = etm_read(ETMACTR1);
		break;
	case 2:
		state[i++] = etm_read(ETMACVR2);
		state[i++] = etm_read(ETMACTR2);
		break;
	case 3:
		state[i++] = etm_read(ETMACVR3);
		state[i++] = etm_read(ETMACTR3);
		break;
	case 4:
		state[i++] = etm_read(ETMACVR4);
		state[i++] = etm_read(ETMACTR4);
		break;
	case 5:
		state[i++] = etm_read(ETMACVR5);
		state[i++] = etm_read(ETMACTR5);
		break;
	case 6:
		state[i++] = etm_read(ETMACVR6);
		state[i++] = etm_read(ETMACTR6);
		break;
	case 7:
		state[i++] = etm_read(ETMACVR7);
		state[i++] = etm_read(ETMACTR7);
		break;
	case 8:
		state[i++] = etm_read(ETMACVR8);
		state[i++] = etm_read(ETMACTR8);
		break;
	case 9:
		state[i++] = etm_read(ETMACVR9);
		state[i++] = etm_read(ETMACTR9);
		break;
	case 10:
		state[i++] = etm_read(ETMACVR10);
		state[i++] = etm_read(ETMACTR10);
		break;
	case 11:
		state[i++] = etm_read(ETMACVR11);
		state[i++] = etm_read(ETMACTR11);
		break;
	case 12:
		state[i++] = etm_read(ETMACVR12);
		state[i++] = etm_read(ETMACTR12);
		break;
	case 13:
		state[i++] = etm_read(ETMACVR13);
		state[i++] = etm_read(ETMACTR13);
		break;
	case 14:
		state[i++] = etm_read(ETMACVR14);
		state[i++] = etm_read(ETMACTR14);
		break;
	case 15:
		state[i++] = etm_read(ETMACVR15);
		state[i++] = etm_read(ETMACTR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_acxr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		etm_write(state[i++], ETMACVR0);
		etm_write(state[i++], ETMACTR0);
		break;
	case 1:
		etm_write(state[i++], ETMACVR1);
		etm_write(state[i++], ETMACTR1);
		break;
	case 2:
		etm_write(state[i++], ETMACVR2);
		etm_write(state[i++], ETMACTR2);
		break;
	case 3:
		etm_write(state[i++], ETMACVR3);
		etm_write(state[i++], ETMACTR3);
		break;
	case 4:
		etm_write(state[i++], ETMACVR4);
		etm_write(state[i++], ETMACTR4);
		break;
	case 5:
		etm_write(state[i++], ETMACVR5);
		etm_write(state[i++], ETMACTR5);
		break;
	case 6:
		etm_write(state[i++], ETMACVR6);
		etm_write(state[i++], ETMACTR6);
		break;
	case 7:
		etm_write(state[i++], ETMACVR7);
		etm_write(state[i++], ETMACTR7);
		break;
	case 8:
		etm_write(state[i++], ETMACVR8);
		etm_write(state[i++], ETMACTR8);
		break;
	case 9:
		etm_write(state[i++], ETMACVR9);
		etm_write(state[i++], ETMACTR9);
		break;
	case 10:
		etm_write(state[i++], ETMACVR10);
		etm_write(state[i++], ETMACTR10);
		break;
	case 11:
		etm_write(state[i++], ETMACVR11);
		etm_write(state[i++], ETMACTR11);
		break;
	case 12:
		etm_write(state[i++], ETMACVR12);
		etm_write(state[i++], ETMACTR12);
		break;
	case 13:
		etm_write(state[i++], ETMACVR13);
		etm_write(state[i++], ETMACTR13);
		break;
	case 14:
		etm_write(state[i++], ETMACVR14);
		etm_write(state[i++], ETMACTR14);
		break;
	case 15:
		etm_write(state[i++], ETMACVR15);
		etm_write(state[i++], ETMACTR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_cntx(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = etm_read(ETMCNTRLDVR0);
		state[i++] = etm_read(ETMCNTENR0);
		state[i++] = etm_read(ETMCNTRLDEVR0);
		state[i++] = etm_read(ETMCNTVR0);
		break;
	case 1:
		state[i++] = etm_read(ETMCNTRLDVR1);
		state[i++] = etm_read(ETMCNTENR1);
		state[i++] = etm_read(ETMCNTRLDEVR1);
		state[i++] = etm_read(ETMCNTVR1);
		break;
	case 2:
		state[i++] = etm_read(ETMCNTRLDVR2);
		state[i++] = etm_read(ETMCNTENR2);
		state[i++] = etm_read(ETMCNTRLDEVR2);
		state[i++] = etm_read(ETMCNTVR2);
		break;
	case 3:
		state[i++] = etm_read(ETMCNTRLDVR3);
		state[i++] = etm_read(ETMCNTENR3);
		state[i++] = etm_read(ETMCNTRLDEVR3);
		state[i++] = etm_read(ETMCNTVR3);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_cntx(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		etm_write(state[i++], ETMCNTRLDVR0);
		etm_write(state[i++], ETMCNTENR0);
		etm_write(state[i++], ETMCNTRLDEVR0);
		etm_write(state[i++], ETMCNTVR0);
		break;
	case 1:
		etm_write(state[i++], ETMCNTRLDVR1);
		etm_write(state[i++], ETMCNTENR1);
		etm_write(state[i++], ETMCNTRLDEVR1);
		etm_write(state[i++], ETMCNTVR1);
		break;
	case 2:
		etm_write(state[i++], ETMCNTRLDVR2);
		etm_write(state[i++], ETMCNTENR2);
		etm_write(state[i++], ETMCNTRLDEVR2);
		etm_write(state[i++], ETMCNTVR2);
		break;
	case 3:
		etm_write(state[i++], ETMCNTRLDVR3);
		etm_write(state[i++], ETMCNTENR3);
		etm_write(state[i++], ETMCNTRLDEVR3);
		etm_write(state[i++], ETMCNTVR3);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_extoutevr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = etm_read(ETMEXTOUTEVR0);
		break;
	case 1:
		state[i++] = etm_read(ETMEXTOUTEVR1);
		break;
	case 2:
		state[i++] = etm_read(ETMEXTOUTEVR2);
		break;
	case 3:
		state[i++] = etm_read(ETMEXTOUTEVR3);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_extoutevr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		etm_write(state[i++], ETMEXTOUTEVR0);
		break;
	case 1:
		etm_write(state[i++], ETMEXTOUTEVR1);
		break;
	case 2:
		etm_write(state[i++], ETMEXTOUTEVR2);
		break;
	case 3:
		etm_write(state[i++], ETMEXTOUTEVR3);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_cidcvr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = etm_read(ETMCIDCVR0);
		break;
	case 1:
		state[i++] = etm_read(ETMCIDCVR1);
		break;
	case 2:
		state[i++] = etm_read(ETMCIDCVR2);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_cidcvr(uint32_t *state, int i, int j)
{
	switch (j) {
	case 0:
		etm_write(state[i++], ETMCIDCVR0);
		break;
	case 1:
		etm_write(state[i++], ETMCIDCVR1);
		break;
	case 2:
		etm_write(state[i++], ETMCIDCVR2);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static inline void etm_clk_disable(void)
{
	uint32_t cpmr;

	isb();
	asm volatile("mrc p15, 7, %0, c15, c0, 5" : "=r" (cpmr));
	cpmr  &= ~CPMR_ETMCLKEN;
	asm volatile("mcr p15, 7, %0, c15, c0, 5" : : "r" (cpmr));
}

static inline void etm_clk_enable(void)
{
	uint32_t cpmr;

	asm volatile("mrc p15, 7, %0, c15, c0, 5" : "=r" (cpmr));
	cpmr  |= CPMR_ETMCLKEN;
	asm volatile("mcr p15, 7, %0, c15, c0, 5" : : "r" (cpmr));
	isb();
}

static inline bool etm_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ETM_ARCH_V3_3:
	case PFT_ARCH_V1_1:
		break;
	default:
		return false;
	}
	return true;
}

static inline void etm_save_state(int cpu)
{
	int i, j, cnt;

	i = cpu * MAX_ETM_REGS;

	/* Vote for ETM power/clock enable */
	etm_clk_enable();

	switch (etm.arch) {
	case PFT_ARCH_V1_1:
		/* Set OS lock to inform the debugger that the OS is in the
		 * process of saving etm registers. It prevents accidental
		 * modification of the etm regs by the external debugger.
		 *
		 * We don't poll for ETMSR[1] since it doesn't get set
		 */
		etm_write(OSLOCK_MAGIC, ETMOSLAR);
		isb();

		etm.state[i++] = etm_read(ETMCR);
		etm.state[i++] = etm_read(ETMTRIGGER);
		etm.state[i++] = etm_read(ETMSR);
		etm.state[i++] = etm_read(ETMTSSCR);
		etm.state[i++] = etm_read(ETMTEEVR);
		etm.state[i++] = etm_read(ETMTECR1);
		etm.state[i++] = etm_read(ETMFFLR);
		for (j = 0; j < etm.nr_addr_cmp; j++)
			i = etm_read_acxr(etm.state, i, j);
		for (j = 0; j < etm.nr_cntr; j++)
			i = etm_read_cntx(etm.state, i, j);
		etm.state[i++] = etm_read(ETMSQ12EVR);
		etm.state[i++] = etm_read(ETMSQ21EVR);
		etm.state[i++] = etm_read(ETMSQ23EVR);
		etm.state[i++] = etm_read(ETMSQ31EVR);
		etm.state[i++] = etm_read(ETMSQ32EVR);
		etm.state[i++] = etm_read(ETMSQ13EVR);
		etm.state[i++] = etm_read(ETMSQR);
		for (j = 0; j < etm.nr_ext_out; j++)
			i = etm_read_extoutevr(etm.state, i, j);
		for (j = 0; j < etm.nr_ctxid_cmp; j++)
			i = etm_read_cidcvr(etm.state, i, j);
		etm.state[i++] = etm_read(ETMCIDCMR);
		etm.state[i++] = etm_read(ETMSYNCFR);
		etm.state[i++] = etm_read(ETMEXTINSELR);
		etm.state[i++] = etm_read(ETMTSEVR);
		etm.state[i++] = etm_read(ETMAUXCR);
		etm.state[i++] = etm_read(ETMTRACEIDR);
		etm.state[i++] = etm_read(ETMVMIDCVR);
		etm.state[i++] = etm_read(ETMCLAIMCLR);
		break;
	case ETM_ARCH_V3_3:
		/* In ETMv3.3, it is possible for the coresight lock to be
		 * implemented for CP14 interface but we currently assume that
		 * it is not, so no need to unlock and lock coresight lock
		 * (ETMLAR).
		 *
		 * Also since save and restore is not conditional i.e. always
		 * occurs when enabled, there is no need to clear the sticky
		 * PDSR bit while saving. It will be cleared during boot up/init
		 * and then by the restore procedure.
		 */

		/* Set OS lock to inform the debugger that the OS is in the
		 * process of saving etm registers. It prevents accidental
		 * modification of the etm regs by the external debugger
		 * and resets the internal counter.
		 */
		etm_write(OSLOCK_MAGIC, ETMOSLAR);
		isb();

		cnt = etm_read(ETMOSSRR); /* save count for restore */
		/* MAX_ETM_REGS = no of etm regs + 1 (for storing the reg count)
		 * check for state overflow, if not enough space, don't save
		 */
		if (cnt >= MAX_ETM_REGS)
			cnt = 0;
		etm.state[i++] = cnt;
		for (j = 0; j < cnt; j++)
			etm.state[i++] = etm_read(ETMOSSRR);
		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n", etm.arch,
								__func__);
	}

	/* Vote for ETM power/clock disable */
	etm_clk_disable();
}

static inline void etm_restore_state(int cpu)
{
	int i, j, cnt;

	i = cpu * MAX_ETM_REGS;

	/* Vote for ETM power/clock enable */
	etm_clk_enable();

	switch (etm.arch) {
	case PFT_ARCH_V1_1:
		/* Set OS lock. Lock will already be set after power collapse
		 * but this write is included to ensure it is set.
		 *
		 * We don't poll for ETMSR[1] since it doesn't get set
		 */
		etm_write(OSLOCK_MAGIC, ETMOSLAR);
		isb();

		etm_write(etm.state[i++], ETMCR);
		etm_write(etm.state[i++], ETMTRIGGER);
		etm_write(etm.state[i++], ETMSR);
		etm_write(etm.state[i++], ETMTSSCR);
		etm_write(etm.state[i++], ETMTEEVR);
		etm_write(etm.state[i++], ETMTECR1);
		etm_write(etm.state[i++], ETMFFLR);
		for (j = 0; j < etm.nr_addr_cmp; j++)
			i = etm_write_acxr(etm.state, i, j);
		for (j = 0; j < etm.nr_cntr; j++)
			i = etm_write_cntx(etm.state, i, j);
		etm_write(etm.state[i++], ETMSQ12EVR);
		etm_write(etm.state[i++], ETMSQ21EVR);
		etm_write(etm.state[i++], ETMSQ23EVR);
		etm_write(etm.state[i++], ETMSQ31EVR);
		etm_write(etm.state[i++], ETMSQ32EVR);
		etm_write(etm.state[i++], ETMSQ13EVR);
		etm_write(etm.state[i++], ETMSQR);
		for (j = 0; j < etm.nr_ext_out; j++)
			i = etm_write_extoutevr(etm.state, i, j);
		for (j = 0; j < etm.nr_ctxid_cmp; j++)
			i = etm_write_cidcvr(etm.state, i, j);
		etm_write(etm.state[i++], ETMCIDCMR);
		etm_write(etm.state[i++], ETMSYNCFR);
		etm_write(etm.state[i++], ETMEXTINSELR);
		etm_write(etm.state[i++], ETMTSEVR);
		etm_write(etm.state[i++], ETMAUXCR);
		etm_write(etm.state[i++], ETMTRACEIDR);
		etm_write(etm.state[i++], ETMVMIDCVR);
		etm_write(etm.state[i++], ETMCLAIMSET);

		/* Clear the OS lock */
		isb();
		etm_write(0x0, ETMOSLAR);
		isb();
		break;
	case ETM_ARCH_V3_3:
		/* In ETMv3.3, it is possible for the coresight lock to be
		 * implemented for CP14 interface but we currently assume that
		 * it is not, so no need to unlock and lock coresight lock
		 * (ETMLAR).
		 */

		/* Clear sticky bit */
		etm_read(ETMPDSR);
		isb();

		/* Set OS lock. Lock will already be set after power collapse
		 * but this write is required to reset the internal counter used
		 * for ETM state restore.
		 */
		etm_write(OSLOCK_MAGIC, ETMOSLAR);
		isb();

		etm_read(ETMOSSRR); /* dummy read of OSSRR */
		cnt = etm.state[i++];
		for (j = 0; j < cnt; j++)
			etm_write(etm.state[i++], ETMOSSRR);

		/* Clear the OS lock */
		isb();
		etm_write(0x0, ETMOSLAR);
		isb();
		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n", etm.arch,
								__func__);
	}

	/* Vote for ETM power/clock disable */
	etm_clk_disable();
}

/* msm_jtag_save_state and msm_jtag_restore_state should be fast
 *
 * These functions will be called either from:
 * 1. per_cpu idle thread context for idle power collapses.
 * 2. per_cpu idle thread context for hotplug/suspend power collapse for
 *    nonboot cpus.
 * 3. suspend thread context for core0.
 *
 * In all cases we are guaranteed to be running on the same cpu for the
 * entire duration.
 */
void msm_jtag_save_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	msm_jtag_save_cntr[cpu]++;
	/* ensure counter is updated before moving forward */
	mb();

	if (dbg.save_restore_enabled)
		dbg_save_state(cpu);
	if (etm.save_restore_enabled)
		etm_save_state(cpu);
}

void msm_jtag_restore_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	msm_jtag_restore_cntr[cpu]++;
	/* ensure counter is updated before moving forward */
	mb();

	if (dbg.save_restore_enabled)
		dbg_restore_state(cpu);
	if (etm.save_restore_enabled)
		etm_restore_state(cpu);
}

static int __init msm_jtag_dbg_init(void)
{
	int ret;
	uint32_t dbgdidr;

	/* This will run on core0 so use it to populate parameters */

	/* Populate dbg_ctx data */

	dbgdidr = dbg_read(DBGDIDR);
	dbg.arch = BMVAL(dbgdidr, 16, 19);
	dbg.nr_ctx_cmp = BMVAL(dbgdidr, 20, 23) + 1;
	dbg.nr_bp = BMVAL(dbgdidr, 24, 27) + 1;
	dbg.nr_wp = BMVAL(dbgdidr, 28, 31) + 1;

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
	dbg.state = kzalloc(MAX_DBG_STATE_SIZE * sizeof(uint32_t), GFP_KERNEL);
	if (!dbg.state) {
		ret = -ENOMEM;
		goto dbg_err;
	}
dbg_out:
	return 0;
dbg_err:
	return ret;
}
arch_initcall(msm_jtag_dbg_init);

static int __init msm_jtag_etm_init(void)
{
	int ret;
	uint32_t etmidr;
	uint32_t etmccr;

	/* Vote for ETM power/clock enable */
	etm_clk_enable();

	/* Clear sticky bit in PDSR - required for ETMv3.3 (8660) */
	etm_read(ETMPDSR);
	isb();

	/* Populate etm_ctx data */

	etmidr = etm_read(ETMIDR);
	etm.arch = BMVAL(etmidr, 4, 11);

	etmccr = etm_read(ETMCCR);
	etm.nr_addr_cmp = BMVAL(etmccr, 0, 3) * 2;
	etm.nr_cntr = BMVAL(etmccr, 13, 15);
	etm.nr_ext_inp = BMVAL(etmccr, 17, 19);
	etm.nr_ext_out = BMVAL(etmccr, 20, 22);
	etm.nr_ctxid_cmp = BMVAL(etmccr, 24, 25);

	if (etm_arch_supported(etm.arch)) {
		if (scm_get_feat_version(TZ_DBG_ETM_FEAT_ID) < TZ_DBG_ETM_VER) {
			etm.save_restore_enabled = true;
		} else {
			pr_info("etm save-restore supported by TZ\n");
			goto etm_out;
		}
	} else {
		pr_info("etm arch %u not supported\n", etm.arch);
		goto etm_out;
	}

	/* Vote for ETM power/clock disable */
	etm_clk_disable();

	/* Allocate etm state save space */
	etm.state = kzalloc(MAX_ETM_STATE_SIZE * sizeof(uint32_t), GFP_KERNEL);
	if (!etm.state) {
		ret = -ENOMEM;
		goto etm_err;
	}
etm_out:
	etm_clk_disable();
	return 0;
etm_err:
	return ret;
}
arch_initcall(msm_jtag_etm_init);
