/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/coresight.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <mach/scm.h>
#include <mach/jtag.h>

/* Coresight management registers */
#define CORESIGHT_ITCTRL	(0xF00)
#define CORESIGHT_CLAIMSET	(0xFA0)
#define CORESIGHT_CLAIMCLR	(0xFA4)
#define CORESIGHT_LAR		(0xFB0)
#define CORESIGHT_LSR		(0xFB4)
#define CORESIGHT_AUTHSTATUS	(0xFB8)
#define CORESIGHT_DEVID		(0xFC8)
#define CORESIGHT_DEVTYPE	(0xFCC)

#define CORESIGHT_UNLOCK	(0xC5ACCE55)

#define TIMEOUT_US		(100)

#define BM(lsb, msb)		((BIT(msb) - BIT(lsb)) + BIT(msb))
#define BMVAL(val, lsb, msb)	((val & BM(lsb, msb)) >> lsb)
#define BVAL(val, n)		((val & BIT(n)) >> n)

/* Trace registers */
#define ETMCR			(0x000)
#define ETMCCR			(0x004)
#define ETMTRIGGER		(0x008)
#define ETMASICCTLR		(0x00C)
#define ETMSR			(0x010)
#define ETMSCR			(0x014)
#define ETMTSSCR		(0x018)
#define ETMTECR2		(0x01C)
#define ETMTEEVR		(0x020)
#define ETMTECR1		(0x024)
#define ETMFFLR			(0x02C)
#define ETMVDEVR		(0x030)
#define ETMVDCR1		(0x034)
#define ETMVDCR3		(0x03C)
#define ETMACVRn(n)		(0x040 + (n * 4))
#define ETMACTRn(n)		(0x080 + (n * 4))
#define ETMDCVRn(n)		(0x0C0 + (n * 8))
#define ETMDCMRn(n)		(0x100 + (n * 8))
#define ETMCNTRLDVRn(n)		(0x140 + (n * 4))
#define ETMCNTENRn(n)		(0x150 + (n * 4))
#define ETMCNTRLDEVRn(n)	(0x160 + (n * 4))
#define ETMCNTVRn(n)		(0x170 + (n * 4))
#define ETMSQ12EVR		(0x180)
#define ETMSQ21EVR		(0x184)
#define ETMSQ23EVR		(0x188)
#define ETMSQ31EVR		(0x18C)
#define ETMSQ32EVR		(0x190)
#define ETMSQ13EVR		(0x194)
#define ETMSQR			(0x19C)
#define ETMEXTOUTEVRn(n)	(0x1A0 + (n * 4))
#define ETMCIDCVRn(n)		(0x1B0 + (n * 4))
#define ETMCIDCMR		(0x1BC)
#define ETMIMPSPEC0		(0x1C0)
#define ETMIMPSPEC1		(0x1C4)
#define ETMIMPSPEC2		(0x1C8)
#define ETMIMPSPEC3		(0x1CC)
#define ETMIMPSPEC4		(0x1D0)
#define ETMIMPSPEC5		(0x1D4)
#define ETMIMPSPEC6		(0x1D8)
#define ETMIMPSPEC7		(0x1DC)
#define ETMSYNCFR		(0x1E0)
#define ETMIDR			(0x1E4)
#define ETMCCER			(0x1E8)
#define ETMEXTINSELR		(0x1EC)
#define ETMTESSEICR		(0x1F0)
#define ETMEIBCR		(0x1F4)
#define ETMTSEVR		(0x1F8)
#define ETMAUXCR		(0x1FC)
#define ETMTRACEIDR		(0x200)
#define ETMIDR2			(0x208)
#define ETMVMIDCVR		(0x240)
#define ETMCLAIMSET		(0xFA0)
#define ETMCLAIMCLR		(0xFA4)
/* ETM Management registers */
#define ETMOSLAR		(0x300)
#define ETMOSLSR		(0x304)
#define ETMOSSRR		(0x308)
#define ETMPDCR			(0x310)
#define ETMPDSR			(0x314)

#define ETM_MAX_ADDR_CMP	(16)
#define ETM_MAX_CNTR		(4)
#define ETM_MAX_CTXID_CMP	(3)

/* DBG Registers */
#define DBGDIDR			(0x0)
#define DBGWFAR			(0x18)
#define DBGVCR			(0x1C)
#define DBGDTRRXext		(0x80)
#define DBGDSCRext		(0x88)
#define DBGDTRTXext		(0x8C)
#define DBGDRCR			(0x90)
#define DBGBVRn(n)		(0x100 + (n * 4))
#define DBGBCRn(n)		(0x140 + (n * 4))
#define DBGWVRn(n)		(0x180 + (n * 4))
#define DBGWCRn(n)		(0x1C0 + (n * 4))
#define DBGPRCR			(0x310)
#define DBGITMISCOUT		(0xEF8)
#define DBGITMISCIN		(0xEFC)
#define DBGCLAIMSET		(0xFA0)
#define DBGCLAIMCLR		(0xFA4)

#define DBGDSCR_MASK		(0x6C30FC3C)

#define MAX_DBG_STATE_SIZE	(90)
#define MAX_ETM_STATE_SIZE	(78)

#define TZ_DBG_ETM_FEAT_ID	(0x8)
#define TZ_DBG_ETM_VER		(0x400000)

#define ARCH_V3_5		(0x25)
#define ARM_DEBUG_ARCH_V7B	(0x3)

#define etm_write(etm, val, off)	\
			__raw_writel(val, etm->base + off)
#define etm_read(etm, off)	\
			__raw_readl(etm->base + off)

#define dbg_write(dbg, val, off)	\
			__raw_writel(val, dbg->base + off)
#define dbg_read(dbg, off)	\
			__raw_readl(dbg->base + off)

#define ETM_LOCK(base)						\
do {									\
	/* recommended by spec to ensure ETM writes are committed prior
	 * to resuming execution
	 */								\
	mb();								\
	etm_write(base, 0x0, CORESIGHT_LAR);			\
} while (0)

#define ETM_UNLOCK(base)						\
do {									\
	etm_write(base, CORESIGHT_UNLOCK, CORESIGHT_LAR);	\
	/* ensure unlock and any pending writes are committed prior to
	 * programming ETM registers
	 */								\
	mb();								\
} while (0)

#define DBG_LOCK(base)						\
do {									\
	/* recommended by spec to ensure ETM writes are committed prior
	 * to resuming execution
	 */								\
	mb();								\
	dbg_write(base, 0x0, CORESIGHT_LAR);			\
} while (0)

#define DBG_UNLOCK(base)						\
do {									\
	dbg_write(base, CORESIGHT_UNLOCK, CORESIGHT_LAR);	\
	/* ensure unlock and any pending writes are committed prior to
	 * programming ETM registers
	 */								\
	mb();								\
} while (0)

uint32_t msm_jtag_save_cntr[NR_CPUS];
uint32_t msm_jtag_restore_cntr[NR_CPUS];

struct dbg_cpu_ctx {
	void __iomem		*base;
	uint32_t		*state;
};

struct dbg_ctx {
	uint8_t			arch;
	uint8_t			nr_wp;
	uint8_t			nr_bp;
	uint8_t			nr_ctx_cmp;
	struct dbg_cpu_ctx	*cpu_ctx[NR_CPUS];
	bool			save_restore_enabled[NR_CPUS];
};
static struct dbg_ctx dbg;

struct etm_cpu_ctx {
	void __iomem		*base;
	struct device		*dev;
	uint32_t		*state;
};

struct etm_ctx {
	uint8_t			arch;
	uint8_t			nr_addr_cmp;
	uint8_t			nr_data_cmp;
	uint8_t			nr_cntr;
	uint8_t			nr_ext_inp;
	uint8_t			nr_ext_out;
	uint8_t			nr_ctxid_cmp;
	struct etm_cpu_ctx	*cpu_ctx[NR_CPUS];
	bool			save_restore_enabled[NR_CPUS];
};

static struct etm_ctx etm;

static struct clk *clock[NR_CPUS];

static void etm_set_pwrdwn(struct etm_cpu_ctx *etmdata)
{
	uint32_t etmcr;

	/* ensure all writes are complete before setting pwrdwn */
	mb();
	etmcr = etm_read(etmdata, ETMCR);
	etmcr |= BIT(0);
	etm_write(etmdata, etmcr, ETMCR);
}

static void etm_clr_pwrdwn(struct etm_cpu_ctx *etmdata)
{
	uint32_t etmcr;

	etmcr = etm_read(etmdata, ETMCR);
	etmcr &= ~BIT(0);
	etm_write(etmdata, etmcr, ETMCR);
	/* ensure pwrup completes before subsequent register accesses */
	mb();
}

static void etm_set_prog(struct etm_cpu_ctx *etmdata)
{
	uint32_t etmcr;
	int count;

	etmcr = etm_read(etmdata, ETMCR);
	etmcr |= BIT(10);
	etm_write(etmdata, etmcr, ETMCR);
	for (count = TIMEOUT_US; BVAL(etm_read(etmdata, ETMSR), 1) != 1
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while setting prog bit, ETMSR: %#x\n",
	     etm_read(etmdata, ETMSR));
}

static inline void etm_save_state(struct etm_cpu_ctx *etmdata)
{
	int i, j;

	i = 0;
	ETM_UNLOCK(etmdata);

	switch (etm.arch) {
	case ETM_ARCH_V3_5:
		etmdata->state[i++] = etm_read(etmdata, ETMTRIGGER);
		etmdata->state[i++] = etm_read(etmdata, ETMASICCTLR);
		etmdata->state[i++] = etm_read(etmdata, ETMSR);
		etmdata->state[i++] = etm_read(etmdata, ETMTSSCR);
		etmdata->state[i++] = etm_read(etmdata, ETMTECR2);
		etmdata->state[i++] = etm_read(etmdata, ETMTEEVR);
		etmdata->state[i++] = etm_read(etmdata, ETMTECR1);
		etmdata->state[i++] = etm_read(etmdata, ETMFFLR);
		etmdata->state[i++] = etm_read(etmdata, ETMVDEVR);
		etmdata->state[i++] = etm_read(etmdata, ETMVDCR1);
		etmdata->state[i++] = etm_read(etmdata, ETMVDCR3);
		for (j = 0; j < etm.nr_addr_cmp; j++) {
			etmdata->state[i++] = etm_read(etmdata,
								ETMACVRn(j));
			etmdata->state[i++] = etm_read(etmdata,
								ETMACTRn(j));
		}
		for (j = 0; j < etm.nr_data_cmp; j++) {
			etmdata->state[i++] = etm_read(etmdata,
								ETMDCVRn(j));
			etmdata->state[i++] = etm_read(etmdata,
								ETMDCMRn(j));
		}
		for (j = 0; j < etm.nr_cntr; j++) {
			etmdata->state[i++] = etm_read(etmdata,
							ETMCNTRLDVRn(j));
			etmdata->state[i++] = etm_read(etmdata,
							ETMCNTENRn(j));
			etmdata->state[i++] = etm_read(etmdata,
							ETMCNTRLDEVRn(j));
			etmdata->state[i++] = etm_read(etmdata,
							ETMCNTVRn(j));
		}
		etmdata->state[i++] = etm_read(etmdata, ETMSQ12EVR);
		etmdata->state[i++] = etm_read(etmdata, ETMSQ21EVR);
		etmdata->state[i++] = etm_read(etmdata, ETMSQ23EVR);
		etmdata->state[i++] = etm_read(etmdata, ETMSQ31EVR);
		etmdata->state[i++] = etm_read(etmdata, ETMSQ32EVR);
		etmdata->state[i++] = etm_read(etmdata, ETMSQ13EVR);
		etmdata->state[i++] = etm_read(etmdata, ETMSQR);
		for (j = 0; j < etm.nr_ext_out; j++)
			etmdata->state[i++] = etm_read(etmdata,
							ETMEXTOUTEVRn(j));
		for (j = 0; j < etm.nr_ctxid_cmp; j++)
			etmdata->state[i++] = etm_read(etmdata,
							ETMCIDCVRn(j));
		etmdata->state[i++] = etm_read(etmdata, ETMCIDCMR);
		etmdata->state[i++] = etm_read(etmdata, ETMSYNCFR);
		etmdata->state[i++] = etm_read(etmdata, ETMEXTINSELR);
		etmdata->state[i++] = etm_read(etmdata, ETMTSEVR);
		etmdata->state[i++] = etm_read(etmdata, ETMAUXCR);
		etmdata->state[i++] = etm_read(etmdata, ETMTRACEIDR);
		etmdata->state[i++] = etm_read(etmdata, ETMVMIDCVR);
		etmdata->state[i++] = etm_read(etmdata, ETMCLAIMCLR);
		etmdata->state[i++] = etm_read(etmdata, ETMCR);
		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n", etm.arch,
								__func__);
	}

	ETM_LOCK(etmdata);
}

static inline void etm_restore_state(struct etm_cpu_ctx *etmdata)
{
	int i, j;

	i = 0;
	ETM_UNLOCK(etmdata);

	switch (etm.arch) {
	case ETM_ARCH_V3_5:
		etm_clr_pwrdwn(etmdata);
		etm_write(etmdata, etmdata->state[i++], ETMTRIGGER);
		etm_write(etmdata, etmdata->state[i++], ETMASICCTLR);
		etm_write(etmdata, etmdata->state[i++], ETMSR);
		etm_write(etmdata, etmdata->state[i++], ETMTSSCR);
		etm_write(etmdata, etmdata->state[i++], ETMTECR2);
		etm_write(etmdata, etmdata->state[i++], ETMTEEVR);
		etm_write(etmdata, etmdata->state[i++], ETMTECR1);
		etm_write(etmdata, etmdata->state[i++], ETMFFLR);
		etm_write(etmdata, etmdata->state[i++], ETMVDEVR);
		etm_write(etmdata, etmdata->state[i++], ETMVDCR1);
		etm_write(etmdata, etmdata->state[i++], ETMVDCR3);
		for (j = 0; j < etm.nr_addr_cmp; j++) {
			etm_write(etmdata, etmdata->state[i++],
								ETMACVRn(j));
			etm_write(etmdata, etmdata->state[i++],
								ETMACTRn(j));
		}
		for (j = 0; j < etm.nr_data_cmp; j++) {
			etm_write(etmdata, etmdata->state[i++],
								ETMDCVRn(j));
			etm_write(etmdata, etmdata->state[i++],
								ETMDCMRn(j));
		}
		for (j = 0; j < etm.nr_cntr; j++) {
			etm_write(etmdata, etmdata->state[i++],
							ETMCNTRLDVRn(j));
			etm_write(etmdata, etmdata->state[i++],
							ETMCNTENRn(j));
			etm_write(etmdata, etmdata->state[i++],
							ETMCNTRLDEVRn(j));
			etm_write(etmdata, etmdata->state[i++],
							ETMCNTVRn(j));
		}
		etm_write(etmdata, etmdata->state[i++], ETMSQ12EVR);
		etm_write(etmdata, etmdata->state[i++], ETMSQ21EVR);
		etm_write(etmdata, etmdata->state[i++], ETMSQ23EVR);
		etm_write(etmdata, etmdata->state[i++], ETMSQ31EVR);
		etm_write(etmdata, etmdata->state[i++], ETMSQ32EVR);
		etm_write(etmdata, etmdata->state[i++], ETMSQ13EVR);
		etm_write(etmdata, etmdata->state[i++], ETMSQR);
		for (j = 0; j < etm.nr_ext_out; j++)
			etm_write(etmdata, etmdata->state[i++],
							ETMEXTOUTEVRn(j));
		for (j = 0; j < etm.nr_ctxid_cmp; j++)
			etm_write(etmdata, etmdata->state[i++],
							ETMCIDCVRn(j));
		etm_write(etmdata, etmdata->state[i++], ETMCIDCMR);
		etm_write(etmdata, etmdata->state[i++], ETMSYNCFR);
		etm_write(etmdata, etmdata->state[i++], ETMEXTINSELR);
		etm_write(etmdata, etmdata->state[i++], ETMTSEVR);
		etm_write(etmdata, etmdata->state[i++], ETMAUXCR);
		etm_write(etmdata, etmdata->state[i++], ETMTRACEIDR);
		etm_write(etmdata, etmdata->state[i++], ETMVMIDCVR);
		etm_write(etmdata, etmdata->state[i++], ETMCLAIMSET);
		/*
		 * Set ETMCR at last as we dont know the saved status of pwrdwn
		 * bit
		 */
		etm_write(etmdata, etmdata->state[i++], ETMCR);
		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n", etm.arch,
								__func__);
	}

	ETM_LOCK(etmdata);
}

static inline void dbg_save_state(struct dbg_cpu_ctx *dbgdata)
{
	int i, j;

	i = 0;
	DBG_UNLOCK(dbgdata);

	dbgdata->state[i++] =  dbg_read(dbgdata, DBGWFAR);
	dbgdata->state[i++] =  dbg_read(dbgdata, DBGVCR);
	for (j = 0; j < dbg.nr_bp; j++) {
		dbgdata->state[i++] =  dbg_read(dbgdata, DBGBVRn(j));
		dbgdata->state[i++] =  dbg_read(dbgdata, DBGBCRn(j));
	}
	for (j = 0; j < dbg.nr_wp; j++) {
		dbgdata->state[i++] =  dbg_read(dbgdata, DBGWVRn(j));
		dbgdata->state[i++] =  dbg_read(dbgdata, DBGWCRn(j));
	}
	dbgdata->state[i++] =  dbg_read(dbgdata, DBGPRCR);
	dbgdata->state[i++] =  dbg_read(dbgdata, DBGCLAIMSET);
	dbgdata->state[i++] =  dbg_read(dbgdata, DBGCLAIMCLR);
	dbgdata->state[i++] =  dbg_read(dbgdata, DBGDTRTXext);
	dbgdata->state[i++] =  dbg_read(dbgdata, DBGDTRRXext);
	dbgdata->state[i++] =  dbg_read(dbgdata, DBGDSCRext);

	DBG_LOCK(dbgdata);
}

static inline void dbg_restore_state(struct dbg_cpu_ctx *dbgdata)
{
	int i, j;

	i = 0;
	DBG_UNLOCK(dbgdata);

	dbg_write(dbgdata, dbgdata->state[i++], DBGWFAR);
	dbg_write(dbgdata, dbgdata->state[i++], DBGVCR);
	for (j = 0; j < dbg.nr_bp; j++) {
		dbg_write(dbgdata, dbgdata->state[i++], DBGBVRn(j));
		dbg_write(dbgdata, dbgdata->state[i++], DBGBCRn(j));
	}
	for (j = 0; j < dbg.nr_wp; j++) {
		dbg_write(dbgdata, dbgdata->state[i++], DBGWVRn(j));
		dbg_write(dbgdata, dbgdata->state[i++], DBGWCRn(j));
	}
	dbg_write(dbgdata, dbgdata->state[i++], DBGPRCR);
	dbg_write(dbgdata, dbgdata->state[i++], DBGCLAIMSET);
	dbg_write(dbgdata, dbgdata->state[i++], DBGCLAIMCLR);
	dbg_write(dbgdata, dbgdata->state[i++], DBGDTRTXext);
	dbg_write(dbgdata, dbgdata->state[i++], DBGDTRRXext);
	dbg_write(dbgdata, dbgdata->state[i++] & DBGDSCR_MASK,
								DBGDSCRext);

	DBG_LOCK(dbgdata);
}

void msm_jtag_save_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	msm_jtag_save_cntr[cpu]++;
	/* ensure counter is updated before moving forward */
	mb();

	if (dbg.save_restore_enabled[cpu])
		dbg_save_state(dbg.cpu_ctx[cpu]);
	if (etm.save_restore_enabled[cpu])
		etm_save_state(etm.cpu_ctx[cpu]);
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

	if (dbg.save_restore_enabled[cpu])
		dbg_restore_state(dbg.cpu_ctx[cpu]);
	if (etm.save_restore_enabled[cpu])
		etm_restore_state(etm.cpu_ctx[cpu]);
}
EXPORT_SYMBOL(msm_jtag_restore_state);

static inline bool etm_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ETM_ARCH_V3_5:
		break;
	default:
		return false;
	}
	return true;
}

static void __devinit etm_init_arch_data(void *info)
{
	uint32_t etmidr;
	uint32_t etmccr;
	struct etm_cpu_ctx  *etmdata = info;

	/*
	 * Clear power down bit since when this bit is set writes to
	 * certain registers might be ignored.
	 */
	ETM_UNLOCK(etmdata);

	etm_clr_pwrdwn(etmdata);
	/* Set prog bit. It will be set from reset but this is included to
	 * ensure it is set
	 */
	etm_set_prog(etmdata);

	/* find all capabilities */
	etmidr = etm_read(etmdata, ETMIDR);
	etm.arch = BMVAL(etmidr, 4, 11);

	etmccr = etm_read(etmdata, ETMCCR);
	etm.nr_addr_cmp = BMVAL(etmccr, 0, 3) * 2;
	etm.nr_data_cmp = BMVAL(etmccr, 4, 7);
	etm.nr_cntr = BMVAL(etmccr, 13, 15);
	etm.nr_ext_inp = BMVAL(etmccr, 17, 19);
	etm.nr_ext_out = BMVAL(etmccr, 20, 22);
	etm.nr_ctxid_cmp = BMVAL(etmccr, 24, 25);

	etm_set_pwrdwn(etmdata);

	ETM_LOCK(etmdata);
}

static int __devinit jtag_mm_etm_probe(struct platform_device *pdev,
								uint32_t cpu)
{
	struct etm_cpu_ctx *etmdata;
	struct resource *res;
	struct device *dev = &pdev->dev;

	/* Allocate memory per cpu */
	etmdata = devm_kzalloc(dev, sizeof(struct etm_cpu_ctx), GFP_KERNEL);
	if (!etmdata)
		return -ENOMEM;

	etm.cpu_ctx[cpu] = etmdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	etmdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!etmdata->base)
		return -EINVAL;

	/* Allocate etm state save space per core */
	etmdata->state = devm_kzalloc(dev,
			(MAX_ETM_STATE_SIZE * sizeof(uint32_t)), GFP_KERNEL);
	if (!etmdata->state)
		return -ENOMEM;

	smp_call_function_single(0, etm_init_arch_data, etmdata, 1);

	if (etm_arch_supported(etm.arch)) {
		if (scm_get_feat_version(TZ_DBG_ETM_FEAT_ID) < TZ_DBG_ETM_VER)
			etm.save_restore_enabled[cpu] = true;
		else
			pr_info("etm save-restore supported by TZ\n");
	} else
		pr_info("etm arch %u not supported\n", etm.arch);
	return 0;
}

static inline bool dbg_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ARM_DEBUG_ARCH_V7B:
		break;
	default:
		return false;
	}
	return true;
}

static void __devinit dbg_init_arch_data(void *info)
{
	uint32_t dbgdidr;
	struct dbg_cpu_ctx *dbgdata = info;

	/* This will run on core0 so use it to populate parameters */
	dbgdidr = dbg_read(dbgdata, DBGDIDR);
	dbg.arch = BMVAL(dbgdidr, 16, 19);
	dbg.nr_ctx_cmp = BMVAL(dbgdidr, 20, 23) + 1;
	dbg.nr_bp = BMVAL(dbgdidr, 24, 27) + 1;
	dbg.nr_wp = BMVAL(dbgdidr, 28, 31) + 1;
}



static int __devinit jtag_mm_dbg_probe(struct platform_device *pdev,
								uint32_t cpu)
{
	struct dbg_cpu_ctx *dbgdata;
	struct resource *res;
	struct device *dev = &pdev->dev;

	/* Allocate memory per cpu */
	dbgdata = devm_kzalloc(dev, sizeof(struct dbg_cpu_ctx), GFP_KERNEL);
	if (!dbgdata)
		return -ENOMEM;

	dbg.cpu_ctx[cpu] = dbgdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	dbgdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!dbgdata->base)
		return -EINVAL;

	/* Allocate etm state save space per core */
	dbgdata->state = devm_kzalloc(dev,
			(MAX_DBG_STATE_SIZE * sizeof(uint32_t)), GFP_KERNEL);
	if (!dbgdata->state)
		return -ENOMEM;

	smp_call_function_single(0, dbg_init_arch_data, dbgdata, 1);

	if (dbg_arch_supported(dbg.arch)) {
		if (scm_get_feat_version(TZ_DBG_ETM_FEAT_ID) < TZ_DBG_ETM_VER)
			dbg.save_restore_enabled[cpu] = true;
		else
			pr_info("dbg save-restore supported by TZ\n");
	} else
		pr_info("dbg arch %u not supported\n", dbg.arch);
	return 0;
}

static int __devinit jtag_mm_probe(struct platform_device *pdev)
{
	int etm_ret, dbg_ret, ret;
	static uint32_t cpu;
	static uint32_t count;
	struct device *dev = &pdev->dev;

	cpu = count;
	count++;

	clock[cpu] = devm_clk_get(dev, "core_clk");
	if (IS_ERR(clock[cpu])) {
		ret = PTR_ERR(clock[cpu]);
		return ret;
	}

	ret = clk_set_rate(clock[cpu], CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	ret = clk_prepare_enable(clock[cpu]);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, clock[cpu]);

	etm_ret  = jtag_mm_etm_probe(pdev, cpu);

	dbg_ret = jtag_mm_dbg_probe(pdev, cpu);

	/* The probe succeeds even when only one of the etm and dbg probes
	 * succeeds. This allows us to save-restore etm and dbg registers
	 * independently.
	 */
	if (etm_ret && dbg_ret) {
		clk_disable_unprepare(clock[cpu]);
		ret = etm_ret;
	} else
		ret = 0;
	return ret;
}

static int __devexit jtag_mm_remove(struct platform_device *pdev)
{
	struct clk *clock = platform_get_drvdata(pdev);

	clk_disable_unprepare(clock);
	return 0;
}

static struct of_device_id msm_qdss_mm_match[] = {
	{ .compatible = "qcom,jtag-mm"},
	{}
};

static struct platform_driver jtag_mm_driver = {
	.probe          = jtag_mm_probe,
	.remove         = __devexit_p(jtag_mm_remove),
	.driver         = {
		.name   = "msm-jtag-mm",
		.owner	= THIS_MODULE,
		.of_match_table	= msm_qdss_mm_match,
		},
};

static int __init jtag_mm_init(void)
{
	return platform_driver_register(&jtag_mm_driver);
}
module_init(jtag_mm_init);

static void __exit jtag_mm_exit(void)
{
	platform_driver_unregister(&jtag_mm_driver);
}
module_exit(jtag_mm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Coresight debug and ETM save-restore driver");
