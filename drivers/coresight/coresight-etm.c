/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/smp.h>
#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <asm/sections.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/memory_dump.h>

#include "coresight-priv.h"

#define etm_writel_mm(drvdata, val, off)  \
			__raw_writel((val), drvdata->base + off)
#define etm_readl_mm(drvdata, off)        \
			__raw_readl(drvdata->base + off)

#define etm_writel(drvdata, val, off)					\
({									\
	etm_writel_mm(drvdata, val, off);				\
})
#define etm_readl(drvdata, off)						\
({									\
	uint32_t val;							\
	val = etm_readl_mm(drvdata, off);				\
	val;								\
})

#define ETM_LOCK(drvdata)						\
do {									\
	/* recommended by spec to ensure ETM writes are committed prior
	 * to resuming execution
	 */								\
	mb();								\
	isb();								\
	etm_writel_mm(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define ETM_UNLOCK(drvdata)						\
do {									\
	etm_writel_mm(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);	\
	/* ensure unlock and any pending writes are committed prior to
	 * programming ETM registers
	 */								\
	mb();								\
	isb();								\
} while (0)

/*
 * Device registers:
 * 0x000 - 0x2FC: Trace		registers
 * 0x300 - 0x314: Management	registers
 * 0x318 - 0xEFC: Trace		registers
 *
 * Coresight registers
 * 0xF00 - 0xF9C: Management	registers
 * 0xFA0 - 0xFA4: Management	registers in PFTv1.0
 *		  Trace		registers in PFTv1.1
 * 0xFA8 - 0xFFC: Management	registers
 */

/* Trace registers (0x000-0x2FC) */
#define ETMCR				(0x000)
#define ETMCCR				(0x004)
#define ETMTRIGGER			(0x008)
#define ETMASSICCTLR			(0x00C)
#define ETMSR				(0x010)
#define ETMSCR				(0x014)
#define ETMTSSCR			(0x018)
#define ETMTECR2			(0x01C)
#define ETMTEEVR			(0x020)
#define ETMTECR1			(0x024)
#define ETMFFLR				(0x02C)
#define ETMVDEVR			(0x030)
#define ETMVDCR1			(0x034)
#define ETMVDCR3			(0x03C)
#define ETMACVRn(n)			(0x040 + (n * 4))
#define ETMACTRn(n)			(0x080 + (n * 4))
#define ETMDCVRn(n)			(0x0C0 + (n * 8))
#define ETMDCMRn(n)			(0x100 + (n * 8))
#define ETMCNTRLDVRn(n)			(0x140 + (n * 4))
#define ETMCNTENRn(n)			(0x150 + (n * 4))
#define ETMCNTRLDEVRn(n)		(0x160 + (n * 4))
#define ETMCNTVRn(n)			(0x170 + (n * 4))
#define ETMSQ12EVR			(0x180)
#define ETMSQ21EVR			(0x184)
#define ETMSQ23EVR			(0x188)
#define ETMSQ31EVR			(0x18C)
#define ETMSQ32EVR			(0x190)
#define ETMSQ13EVR			(0x194)
#define ETMSQR				(0x19C)
#define ETMEXTOUTEVRn(n)		(0x1A0 + (n * 4))
#define ETMCIDCVRn(n)			(0x1B0 + (n * 4))
#define ETMCIDCMR			(0x1BC)
#define ETMIMPSPEC0			(0x1C0)
#define ETMIMPSPEC1			(0x1C4)
#define ETMIMPSPEC2			(0x1C8)
#define ETMIMPSPEC3			(0x1CC)
#define ETMIMPSPEC4			(0x1D0)
#define ETMIMPSPEC5			(0x1D4)
#define ETMIMPSPEC6			(0x1D8)
#define ETMIMPSPEC7			(0x1DC)
#define ETMSYNCFR			(0x1E0)
#define ETMIDR				(0x1E4)
#define ETMCCER				(0x1E8)
#define ETMEXTINSELR			(0x1EC)
#define ETMTESSEICR			(0x1F0)
#define ETMEIBCR			(0x1F4)
#define ETMTSEVR			(0x1F8)
#define ETMAUXCR			(0x1FC)
#define ETMTRACEIDR			(0x200)
#define ETMIDR2				(0x208)
#define ETMVMIDCVR			(0x240)
/* Management registers (0x300-0x314) */
#define ETMOSLAR			(0x300)
#define ETMOSLSR			(0x304)
#define ETMOSSRR			(0x308)
#define ETMPDCR				(0x310)
#define ETMPDSR				(0x314)

#define ETM_MAX_ADDR_CMP		(16)
#define ETM_MAX_DATA_CMP		(8)
#define ETM_MAX_CNTR			(4)
#define ETM_MAX_CTXID_CMP		(3)
#define ETM_MAX_EXT_INP			(4)
#define ETM_MAX_EXT_OUTP		(4)

#define ETM_MODE_EXCLUDE		BIT(0)
#define ETM_MODE_CYCACC			BIT(1)
#define ETM_MODE_STALL			BIT(2)
#define ETM_MODE_TIMESTAMP		BIT(3)
#define ETM_MODE_CTXID			BIT(4)
#define ETM_MODE_DATA_TRACE_VAL		BIT(5)
#define ETM_MODE_DATA_TRACE_ADDR	BIT(6)
#define ETM_MODE_ALL			(0x7F)

#define ETM_DATACMP_ENABLE		(0x2)

#define ETM_EVENT_MASK			(0x1FFFF)
#define ETM_SYNC_MASK			(0xFFF)
#define ETM_ALL_MASK			(0xFFFFFFFF)

#define ETM_SEQ_STATE_MAX_VAL		(0x2)

#define ETM_REG_DUMP_VER_OFF		(4)
#define ETM_REG_DUMP_VER		(1)

#define CPMR_ETMCLKEN			(8)

enum etm_addr_type {
	ETM_ADDR_TYPE_NONE,
	ETM_ADDR_TYPE_SINGLE,
	ETM_ADDR_TYPE_RANGE,
	ETM_ADDR_TYPE_START,
	ETM_ADDR_TYPE_STOP,
};

#ifdef CONFIG_CORESIGHT_ETM_DEFAULT_RESET
static int boot_reset = 1;
#else
static int boot_reset;
#endif
module_param_named(
	boot_reset, boot_reset, int, S_IRUGO
);

#ifdef CONFIG_CORESIGHT_ETM_DEFAULT_ENABLE
static int boot_enable = 1;
#else
static int boot_enable;
#endif
module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

#ifdef CONFIG_CORESIGHT_ETM_PCSAVE_DEFAULT_ENABLE
static int boot_pcsave_enable = 1;
#else
static int boot_pcsave_enable;
#endif
module_param_named(
	boot_pcsave_enable, boot_pcsave_enable, int, S_IRUGO
);

struct etm_drvdata {
	void __iomem			*base;
	uint32_t			reg_size;
	struct device			*dev;
	struct coresight_device		*csdev;
	struct clk			*clk;
	spinlock_t			spinlock;
	struct mutex			mutex;
	struct wakeup_source		ws;
	int				cpu;
	uint8_t				arch;
	bool				enable;
	bool				sticky_enable;
	bool				boot_enable;
	bool				os_unlock;
	bool				init;
	uint8_t				nr_addr_cmp;
	uint8_t				nr_cntr;
	uint8_t				nr_ext_inp;
	uint8_t				nr_ext_out;
	uint8_t				nr_ctxid_cmp;
	uint8_t				nr_data_cmp;
	uint8_t				reset;
	uint32_t			mode;
	uint32_t			ctrl;
	uint32_t			trigger_event;
	uint32_t			startstop_ctrl;
	uint32_t			enable_event;
	uint32_t			enable_ctrl1;
	uint32_t			enable_ctrl2;
	uint32_t			fifofull_level;
	uint8_t				addr_idx;
	uint32_t			addr_val[ETM_MAX_ADDR_CMP];
	uint32_t			addr_acctype[ETM_MAX_ADDR_CMP];
	uint32_t			addr_type[ETM_MAX_ADDR_CMP];
	bool				data_trace_support;
	uint32_t			data_val[ETM_MAX_ADDR_CMP];
	uint32_t			data_mask[ETM_MAX_ADDR_CMP];
	uint32_t			viewdata_event;
	uint32_t			viewdata_ctrl1;
	uint32_t			viewdata_ctrl3;
	uint8_t				cntr_idx;
	uint32_t			cntr_rld_val[ETM_MAX_CNTR];
	uint32_t			cntr_event[ETM_MAX_CNTR];
	uint32_t			cntr_rld_event[ETM_MAX_CNTR];
	uint32_t			cntr_val[ETM_MAX_CNTR];
	uint32_t			seq_12_event;
	uint32_t			seq_21_event;
	uint32_t			seq_23_event;
	uint32_t			seq_31_event;
	uint32_t			seq_32_event;
	uint32_t			seq_13_event;
	uint32_t			seq_curr_state;
	uint8_t				ctxid_idx;
	uint32_t			ctxid_val[ETM_MAX_CTXID_CMP];
	uint32_t			ctxid_mask;
	uint32_t			sync_freq;
	uint32_t			timestamp_event;
	bool				pcsave_impl;
	bool				pcsave_enable;
	bool				pcsave_sticky_enable;
	bool				pcsave_boot_enable;
	bool				round_robin;
	struct msm_dump_data		reg_data;
};

static int count;
static struct etm_drvdata *etmdrvdata[NR_CPUS];
static struct notifier_block etm_cpu_notifier;

static bool etm_os_lock_present(struct etm_drvdata *drvdata)
{
	uint32_t etmoslsr;

	etmoslsr = etm_readl(drvdata, ETMOSLSR);
	if (!BVAL(etmoslsr, 0) && !BVAL(etmoslsr, 3))
		return false;
	return true;
}

/*
 * Unlock OS lock to allow memory mapped access on Krait and in general
 * so that ETMSR[1] can be polled while clearing the ETMCR[10] prog bit
 * since ETMSR[1] is set when prog bit is set or OS lock is set.
 */
static void etm_os_unlock(void *info)
{
	struct etm_drvdata *drvdata = (struct etm_drvdata *) info;

	ETM_UNLOCK(drvdata);
	if (etm_os_lock_present(drvdata)) {
		etm_writel(drvdata, 0x0, ETMOSLAR);
		/* ensure os lock is unlocked before we return */
		mb();
	}
	ETM_LOCK(drvdata);
}

/*
 * ETM clock is derived from the processor clock and gets enabled on a
 * logical OR of below items on Krait (v2 onwards):
 * 1.CPMR[ETMCLKEN] is 1
 * 2.ETMCR[PD] is 0
 * 3.ETMPDCR[PU] is 1
 * 4.Reset is asserted (core or debug)
 * 5.APB memory mapped requests (eg. EDAP access)
 *
 * 1., 2. and 3. above are permanent enables whereas 4. and 5. are temporary
 * enables
 *
 * We rely on 5. to be able to access ETMCR/ETMPDCR and then use 2./3. above
 * for ETM clock vote in the driver and the save-restore code uses 1. above
 * for its vote
 */
static void etm_set_pwrdwn(struct etm_drvdata *drvdata)
{
	uint32_t etmcr;

	/* ensure pending cp14 accesses complete before setting pwrdwn */
	mb();
	isb();
	etmcr = etm_readl(drvdata, ETMCR);
	etmcr |= BIT(0);
	etm_writel(drvdata, etmcr, ETMCR);
}

static void etm_clr_pwrdwn(struct etm_drvdata *drvdata)
{
	uint32_t etmcr;

	etmcr = etm_readl(drvdata, ETMCR);
	etmcr &= ~BIT(0);
	etm_writel(drvdata, etmcr, ETMCR);
	/* ensure pwrup completes before subsequent cp14 accesses */
	mb();
	isb();
}

static void etm_set_pwrup(struct etm_drvdata *drvdata)
{
	uint32_t etmpdcr;

	etmpdcr = etm_readl_mm(drvdata, ETMPDCR);
	etmpdcr |= BIT(3);
	etm_writel_mm(drvdata, etmpdcr, ETMPDCR);
	/* ensure pwrup completes before subsequent cp14 accesses */
	mb();
	isb();
}

static void etm_clr_pwrup(struct etm_drvdata *drvdata)
{
	uint32_t etmpdcr;

	/* ensure pending cp14 accesses complete before clearing pwrup */
	mb();
	isb();
	etmpdcr = etm_readl_mm(drvdata, ETMPDCR);
	etmpdcr &= ~BIT(3);
	etm_writel_mm(drvdata, etmpdcr, ETMPDCR);
}

static void etm_set_prog(struct etm_drvdata *drvdata)
{
	uint32_t etmcr;
	int count;

	etmcr = etm_readl(drvdata, ETMCR);
	etmcr |= BIT(10);
	etm_writel(drvdata, etmcr, ETMCR);
	/* recommended by spec for cp14 accesses to ensure etmcr write is
	 * complete before polling etmsr
	 */
	isb();
	for (count = TIMEOUT_US; BVAL(etm_readl(drvdata, ETMSR), 1) != 1
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while setting prog bit, ETMSR: %#x\n",
	     etm_readl(drvdata, ETMSR));
}

static void etm_clr_prog(struct etm_drvdata *drvdata)
{
	uint32_t etmcr;
	int count;

	etmcr = etm_readl(drvdata, ETMCR);
	etmcr &= ~BIT(10);
	etm_writel(drvdata, etmcr, ETMCR);
	/* recommended by spec for cp14 accesses to ensure etmcr write is
	 * complete before polling etmsr
	 */
	isb();
	for (count = TIMEOUT_US; BVAL(etm_readl(drvdata, ETMSR), 1) != 0
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while clearing prog bit, ETMSR: %#x\n",
	     etm_readl(drvdata, ETMSR));
}

static void etm_enable_pcsave(void *info)
{
	struct etm_drvdata *drvdata = info;

	ETM_UNLOCK(drvdata);

	/*
	 * ETMPDCR is only accessible via memory mapped interface and so use
	 * it first to enable power/clock to allow subsequent cp14 accesses.
	 */
	etm_set_pwrup(drvdata);
	etm_clr_pwrdwn(drvdata);
	etm_clr_pwrup(drvdata);

	ETM_LOCK(drvdata);
}

static void etm_disable_pcsave(void *info)
{
	struct etm_drvdata *drvdata = info;

	ETM_UNLOCK(drvdata);

	if (!drvdata->enable)
		etm_set_pwrdwn(drvdata);

	ETM_LOCK(drvdata);
}

static bool etm_version_gte(uint8_t arch, uint8_t base_arch)
{
	if (arch >= base_arch && ((arch & PFT_ARCH_MAJOR) != PFT_ARCH_MAJOR))
		return true;
	else
		return false;
}

static void etm_reset_data(struct etm_drvdata *drvdata)
{
	int i;

	spin_lock(&drvdata->spinlock);

	drvdata->mode = ETM_MODE_EXCLUDE;
	drvdata->ctrl = 0x0;
	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_0))
		drvdata->ctrl |= BIT(11);
	drvdata->trigger_event = 0x406F;
	drvdata->startstop_ctrl = 0x0;
	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_2))
		drvdata->enable_ctrl2 = 0x0;
	drvdata->enable_event = 0x6F;
	drvdata->enable_ctrl1 = 0x1000000;
	drvdata->fifofull_level = 0x28;
	if (drvdata->data_trace_support == true) {
		drvdata->mode |= (ETM_MODE_DATA_TRACE_VAL |
					ETM_MODE_DATA_TRACE_ADDR);
		drvdata->ctrl |= BIT(2) | BIT(3);
		drvdata->viewdata_event = 0x6F;
		drvdata->viewdata_ctrl1 = 0x0;
		drvdata->viewdata_ctrl3 = 0x10000;
	}
	drvdata->addr_idx = 0x0;
	for (i = 0; i < drvdata->nr_addr_cmp; i++) {
		drvdata->addr_val[i] = 0x0;
		drvdata->addr_acctype[i] = 0x0;
		drvdata->addr_type[i] = ETM_ADDR_TYPE_NONE;
	}
	for (i = 0; i < drvdata->nr_data_cmp; i++) {
		drvdata->data_val[i] = 0;
		drvdata->data_mask[i] = ~(0);
	}
	drvdata->cntr_idx = 0x0;
	for (i = 0; i < drvdata->nr_cntr; i++) {
		drvdata->cntr_rld_val[i] = 0x0;
		drvdata->cntr_event[i] = 0x406F;
		drvdata->cntr_rld_event[i] = 0x406F;
		drvdata->cntr_val[i] = 0x0;
	}
	drvdata->seq_12_event = 0x406F;
	drvdata->seq_21_event = 0x406F;
	drvdata->seq_23_event = 0x406F;
	drvdata->seq_31_event = 0x406F;
	drvdata->seq_32_event = 0x406F;
	drvdata->seq_13_event = 0x406F;
	drvdata->seq_curr_state = 0x0;
	drvdata->ctxid_idx = 0x0;
	for (i = 0; i < drvdata->nr_ctxid_cmp; i++)
		drvdata->ctxid_val[i] = 0x0;
	drvdata->ctxid_mask = 0x0;
	drvdata->sync_freq = 0x80;
	drvdata->timestamp_event = 0x406F;

	spin_unlock(&drvdata->spinlock);
}

static void __etm_enable(void *info)
{
	int i;
	uint32_t etmcr;
	struct etm_drvdata *drvdata = info;

	ETM_UNLOCK(drvdata);
	/*
	 * Vote for ETM power/clock enable. ETMPDCR is only accessible via
	 * memory mapped interface and so use it first to enable power/clock
	 * to allow subsequent cp14 accesses.
	 */
	etm_set_pwrup(drvdata);
	/*
	 * Clear power down bit since when this bit is set writes to
	 * certain registers might be ignored. This is also a pre-requisite
	 * for trace enable.
	 */
	etm_clr_pwrdwn(drvdata);
	etm_clr_pwrup(drvdata);
	etm_set_prog(drvdata);

	etmcr = etm_readl(drvdata, ETMCR);
	etmcr &= (BIT(10) | BIT(0));
	etm_writel(drvdata, drvdata->ctrl | etmcr, ETMCR);
	etm_writel(drvdata, drvdata->trigger_event, ETMTRIGGER);
	etm_writel(drvdata, drvdata->startstop_ctrl, ETMTSSCR);
	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_2))
		etm_writel(drvdata, drvdata->enable_ctrl2, ETMTECR2);
	etm_writel(drvdata, drvdata->enable_event, ETMTEEVR);
	etm_writel(drvdata, drvdata->enable_ctrl1, ETMTECR1);
	etm_writel(drvdata, drvdata->fifofull_level, ETMFFLR);
	if (drvdata->data_trace_support == true) {
		etm_writel(drvdata, drvdata->viewdata_event, ETMVDEVR);
		etm_writel(drvdata, drvdata->viewdata_ctrl1, ETMVDCR1);
		etm_writel(drvdata, drvdata->viewdata_ctrl3, ETMVDCR3);
	}
	for (i = 0; i < drvdata->nr_addr_cmp; i++) {
		etm_writel(drvdata, drvdata->addr_val[i], ETMACVRn(i));
		etm_writel(drvdata, drvdata->addr_acctype[i], ETMACTRn(i));
	}
	for (i = 0; i < drvdata->nr_data_cmp; i++) {
		etm_writel(drvdata, drvdata->data_val[i], ETMDCVRn(i));
		etm_writel(drvdata, drvdata->data_mask[i], ETMDCMRn(i));
	}
	for (i = 0; i < drvdata->nr_cntr; i++) {
		etm_writel(drvdata, drvdata->cntr_rld_val[i], ETMCNTRLDVRn(i));
		etm_writel(drvdata, drvdata->cntr_event[i], ETMCNTENRn(i));
		etm_writel(drvdata, drvdata->cntr_rld_event[i],
			   ETMCNTRLDEVRn(i));
		etm_writel(drvdata, drvdata->cntr_val[i], ETMCNTVRn(i));
	}
	etm_writel(drvdata, drvdata->seq_12_event, ETMSQ12EVR);
	etm_writel(drvdata, drvdata->seq_21_event, ETMSQ21EVR);
	etm_writel(drvdata, drvdata->seq_23_event, ETMSQ23EVR);
	etm_writel(drvdata, drvdata->seq_31_event, ETMSQ31EVR);
	etm_writel(drvdata, drvdata->seq_32_event, ETMSQ32EVR);
	etm_writel(drvdata, drvdata->seq_13_event, ETMSQ13EVR);
	etm_writel(drvdata, drvdata->seq_curr_state, ETMSQR);
	for (i = 0; i < drvdata->nr_ext_out; i++)
		etm_writel(drvdata, 0x0000406F, ETMEXTOUTEVRn(i));
	for (i = 0; i < drvdata->nr_ctxid_cmp; i++)
		etm_writel(drvdata, drvdata->ctxid_val[i], ETMCIDCVRn(i));
	etm_writel(drvdata, drvdata->ctxid_mask, ETMCIDCMR);
	etm_writel(drvdata, drvdata->sync_freq, ETMSYNCFR);
	etm_writel(drvdata, 0x00000000, ETMEXTINSELR);
	etm_writel(drvdata, drvdata->timestamp_event, ETMTSEVR);
	etm_writel(drvdata, 0x00000000, ETMAUXCR);
	etm_writel(drvdata, drvdata->cpu + 1, ETMTRACEIDR);
	etm_writel(drvdata, 0x00000000, ETMVMIDCVR);

	etm_clr_prog(drvdata);
	ETM_LOCK(drvdata);

	dev_dbg(drvdata->dev, "cpu: %d enable smp call done\n", drvdata->cpu);
}

static int etm_enable(struct coresight_device *csdev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	pm_stay_awake(drvdata->dev);

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		goto err_clk;

	spin_lock(&drvdata->spinlock);

	/*
	 * Executing __etm_enable on the cpu whose ETM is being enabled
	 * ensures that register writes occur when cpu is powered.
	 */
	ret = smp_call_function_single(drvdata->cpu, __etm_enable, drvdata, 1);
	if (ret)
		goto err;
	drvdata->enable = true;
	drvdata->sticky_enable = true;

	spin_unlock(&drvdata->spinlock);

	pm_relax(drvdata->dev);

	dev_info(drvdata->dev, "ETM tracing enabled\n");
	return 0;
err:
	spin_unlock(&drvdata->spinlock);
	clk_disable_unprepare(drvdata->clk);
err_clk:
	pm_relax(drvdata->dev);
	return ret;
}

static void __etm_disable(void *info)
{
	struct etm_drvdata *drvdata = info;

	ETM_UNLOCK(drvdata);
	etm_set_prog(drvdata);

	/* program trace enable to low by using always false event */
	etm_writel(drvdata, 0x6F | BIT(14), ETMTEEVR);

	if (!drvdata->pcsave_enable)
		etm_set_pwrdwn(drvdata);
	ETM_LOCK(drvdata);

	dev_dbg(drvdata->dev, "cpu: %d disable smp call done\n", drvdata->cpu);
}

static void etm_disable(struct coresight_device *csdev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	pm_stay_awake(drvdata->dev);

	/*
	 * Taking hotplug lock here protects from clocks getting disabled
	 * with tracing being left on (crash scenario) if user disable occurs
	 * after cpu online mask indicates the cpu is offline but before the
	 * DYING hotplug callback is serviced by the ETM driver.
	 */
	get_online_cpus();
	spin_lock(&drvdata->spinlock);

	/*
	 * Executing __etm_disable on the cpu whose ETM is being disabled
	 * ensures that register writes occur when cpu is powered.
	 */
	smp_call_function_single(drvdata->cpu, __etm_disable, drvdata, 1);
	drvdata->enable = false;

	spin_unlock(&drvdata->spinlock);
	put_online_cpus();

	clk_disable_unprepare(drvdata->clk);

	pm_relax(drvdata->dev);

	dev_info(drvdata->dev, "ETM tracing disabled\n");
}

static const struct coresight_ops_source etm_source_ops = {
	.enable		= etm_enable,
	.disable	= etm_disable,
};

static const struct coresight_ops etm_cs_ops = {
	.source_ops	= &etm_source_ops,
};

static ssize_t etm_show_nr_addr_cmp(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_addr_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_addr_cmp, S_IRUGO, etm_show_nr_addr_cmp, NULL);

static ssize_t etm_show_nr_cntr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_cntr;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_cntr, S_IRUGO, etm_show_nr_cntr, NULL);

static ssize_t etm_show_nr_ctxid_cmp(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_ctxid_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_ctxid_cmp, S_IRUGO, etm_show_nr_ctxid_cmp, NULL);

static ssize_t etm_show_reset(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->reset;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

/* Reset to trace everything i.e. exclude nothing. */
static ssize_t etm_store_reset(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		etm_reset_data(drvdata);

	return size;
}
static DEVICE_ATTR(reset, S_IRUGO | S_IWUSR, etm_show_reset, etm_store_reset);

static ssize_t etm_show_mode(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->mode;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_mode(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->mode = val & ETM_MODE_ALL;

	if (drvdata->mode & ETM_MODE_EXCLUDE)
		drvdata->enable_ctrl1 |= BIT(24);
	else
		drvdata->enable_ctrl1 &= ~BIT(24);

	if (drvdata->mode & ETM_MODE_CYCACC)
		drvdata->ctrl |= BIT(12);
	else
		drvdata->ctrl &= ~BIT(12);

	if (drvdata->mode & ETM_MODE_STALL)
		drvdata->ctrl |= BIT(7);
	else
		drvdata->ctrl &= ~BIT(7);

	if (drvdata->mode & ETM_MODE_TIMESTAMP)
		drvdata->ctrl |= BIT(28);
	else
		drvdata->ctrl &= ~BIT(28);

	if (drvdata->mode & ETM_MODE_CTXID)
		drvdata->ctrl |= (BIT(14) | BIT(15));
	else
		drvdata->ctrl &= ~(BIT(14) | BIT(15));
	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_0)) {
		if (drvdata->mode & ETM_MODE_DATA_TRACE_VAL)
			drvdata->ctrl |= BIT(2);
		else
			drvdata->ctrl &= ~(BIT(2));

		if (drvdata->mode & ETM_MODE_DATA_TRACE_ADDR)
			drvdata->ctrl |= (BIT(3));
		else
			drvdata->ctrl &= ~(BIT(3));
	}
	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, etm_show_mode, etm_store_mode);

static ssize_t etm_show_trigger_event(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->trigger_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_trigger_event(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->trigger_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(trigger_event, S_IRUGO | S_IWUSR, etm_show_trigger_event,
		   etm_store_trigger_event);

static ssize_t etm_show_enable_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->enable_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_enable_event(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->enable_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(enable_event, S_IRUGO | S_IWUSR, etm_show_enable_event,
		   etm_store_enable_event);

static ssize_t etm_show_fifofull_level(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->fifofull_level;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_fifofull_level(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->fifofull_level = val;
	return size;
}
static DEVICE_ATTR(fifofull_level, S_IRUGO | S_IWUSR, etm_show_fifofull_level,
		   etm_store_fifofull_level);

static ssize_t etm_show_addr_idx(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->addr_idx;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_addr_idx(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val >= drvdata->nr_addr_cmp)
		return -EINVAL;

	/*
	 * Use spinlock to ensure index doesn't change while it gets
	 * dereferenced multiple times within a spinlock block elsewhere.
	 */
	spin_lock(&drvdata->spinlock);
	drvdata->addr_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_idx, S_IRUGO | S_IWUSR, etm_show_addr_idx,
		   etm_store_addr_idx);

static ssize_t etm_show_addr_single(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_SINGLE)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val = drvdata->addr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_addr_single(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_SINGLE)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	drvdata->addr_val[idx] = val;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_SINGLE;
	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_2))
		drvdata->enable_ctrl2 |= (1 << idx);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_single, S_IRUGO | S_IWUSR, etm_show_addr_single,
		   etm_store_addr_single);

static ssize_t etm_show_addr_range(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (!((drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_NONE) ||
	      (drvdata->addr_type[idx] == ETM_ADDR_TYPE_RANGE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_RANGE))) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val1 = drvdata->addr_val[idx];
	val2 = drvdata->addr_val[idx + 1];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx %#lx\n", val1, val2);
}

static ssize_t etm_store_addr_range(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;
	uint8_t idx;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;
	/* lower address comparator cannot have a higher address value */
	if (val1 > val2)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (!((drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_NONE) ||
	      (drvdata->addr_type[idx] == ETM_ADDR_TYPE_RANGE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_RANGE))) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	drvdata->addr_val[idx] = val1;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_RANGE;
	drvdata->addr_val[idx + 1] = val2;
	drvdata->addr_type[idx + 1] = ETM_ADDR_TYPE_RANGE;
	drvdata->enable_ctrl1 |= (1 << (idx/2));
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_range, S_IRUGO | S_IWUSR, etm_show_addr_range,
		   etm_store_addr_range);

static ssize_t etm_show_addr_start(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_START)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val = drvdata->addr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_addr_start(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_START)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	drvdata->addr_val[idx] = val;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_START;
	drvdata->startstop_ctrl |= (1 << idx);
	drvdata->enable_ctrl1 |= BIT(25);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_start, S_IRUGO | S_IWUSR, etm_show_addr_start,
		   etm_store_addr_start);

static ssize_t etm_show_addr_stop(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_STOP)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val = drvdata->addr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_addr_stop(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_STOP)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	drvdata->addr_val[idx] = val;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_STOP;
	drvdata->startstop_ctrl |= (1 << (idx + 16));
	drvdata->enable_ctrl1 |= BIT(25);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_stop, S_IRUGO | S_IWUSR, etm_show_addr_stop,
		   etm_store_addr_stop);

static ssize_t etm_show_addr_acctype(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	spin_lock(&drvdata->spinlock);
	val = drvdata->addr_acctype[drvdata->addr_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_addr_acctype(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->addr_acctype[drvdata->addr_idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_acctype, S_IRUGO | S_IWUSR, etm_show_addr_acctype,
		   etm_store_addr_acctype);

static ssize_t etm_show_data_val(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	idx = idx >> 1;
	if (idx >= drvdata->nr_data_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val = drvdata->data_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_data_val(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx, data_idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	/* Adjust index to use the correct data comparator */
	data_idx = idx >> 1;
	/* Only idx = 0, 2, 4, 6... are valid */
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (data_idx >= drvdata->nr_data_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (!BVAL(drvdata->addr_acctype[idx], ETM_DATACMP_ENABLE)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (drvdata->addr_type[idx] == ETM_ADDR_TYPE_RANGE) {
		if (!BVAL(drvdata->addr_acctype[idx + 1], ETM_DATACMP_ENABLE)) {
			spin_unlock(&drvdata->spinlock);
			return -EPERM;
		}
	}

	drvdata->data_val[data_idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(data_val, S_IRUGO | S_IWUSR, etm_show_data_val,
			etm_store_data_val);

static ssize_t etm_show_data_mask(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long mask;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	idx = idx >> 1;
	if (idx >= drvdata->nr_data_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	mask = drvdata->data_mask[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", mask);
}

static ssize_t etm_store_data_mask(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long mask;
	uint8_t idx, data_idx;

	if (sscanf(buf, "%lx", &mask) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	/* Adjust index to use the correct data comparator */
	data_idx = idx >> 1;
	/* Only idx = 0, 2, 4, 6... are valid */
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (data_idx >= drvdata->nr_data_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (!BVAL(drvdata->addr_acctype[idx], ETM_DATACMP_ENABLE)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (drvdata->addr_type[idx] == ETM_ADDR_TYPE_RANGE) {
		if (!BVAL(drvdata->addr_acctype[idx + 1], ETM_DATACMP_ENABLE)) {
			spin_unlock(&drvdata->spinlock);
			return -EPERM;
		}
	}

	drvdata->data_mask[data_idx] = mask;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(data_mask, S_IRUGO | S_IWUSR, etm_show_data_mask,
			etm_store_data_mask);

static ssize_t etm_show_cntr_idx(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->addr_idx;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_idx(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val >= drvdata->nr_cntr)
		return -EINVAL;

	/*
	 * Use spinlock to ensure index doesn't change while it gets
	 * dereferenced multiple times within a spinlock block elsewhere.
	 */
	spin_lock(&drvdata->spinlock);
	drvdata->cntr_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_idx, S_IRUGO | S_IWUSR, etm_show_cntr_idx,
		   etm_store_cntr_idx);

static ssize_t etm_show_cntr_rld_val(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	spin_lock(&drvdata->spinlock);
	val = drvdata->cntr_rld_val[drvdata->cntr_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_rld_val(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->cntr_rld_val[drvdata->cntr_idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_rld_val, S_IRUGO | S_IWUSR, etm_show_cntr_rld_val,
		   etm_store_cntr_rld_val);

static ssize_t etm_show_cntr_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	spin_lock(&drvdata->spinlock);
	val = drvdata->cntr_event[drvdata->cntr_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_event(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->cntr_event[drvdata->cntr_idx] = val & ETM_EVENT_MASK;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_event, S_IRUGO | S_IWUSR, etm_show_cntr_event,
		   etm_store_cntr_event);

static ssize_t etm_show_cntr_rld_event(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	spin_lock(&drvdata->spinlock);
	val = drvdata->cntr_rld_event[drvdata->cntr_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_rld_event(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->cntr_rld_event[drvdata->cntr_idx] = val & ETM_EVENT_MASK;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_rld_event, S_IRUGO | S_IWUSR, etm_show_cntr_rld_event,
		   etm_store_cntr_rld_event);

static ssize_t etm_show_cntr_val(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	spin_lock(&drvdata->spinlock);
	val = drvdata->cntr_val[drvdata->cntr_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_val(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->cntr_val[drvdata->cntr_idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_val, S_IRUGO | S_IWUSR, etm_show_cntr_val,
		   etm_store_cntr_val);

static ssize_t etm_show_seq_12_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_12_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_12_event(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->seq_12_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(seq_12_event, S_IRUGO | S_IWUSR, etm_show_seq_12_event,
		   etm_store_seq_12_event);

static ssize_t etm_show_seq_21_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_21_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_21_event(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->seq_21_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(seq_21_event, S_IRUGO | S_IWUSR, etm_show_seq_21_event,
		   etm_store_seq_21_event);

static ssize_t etm_show_seq_23_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_23_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_23_event(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->seq_23_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(seq_23_event, S_IRUGO | S_IWUSR, etm_show_seq_23_event,
		   etm_store_seq_23_event);

static ssize_t etm_show_seq_31_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_31_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_31_event(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->seq_31_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(seq_31_event, S_IRUGO | S_IWUSR, etm_show_seq_31_event,
		   etm_store_seq_31_event);

static ssize_t etm_show_seq_32_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_32_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_32_event(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->seq_32_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(seq_32_event, S_IRUGO | S_IWUSR, etm_show_seq_32_event,
		   etm_store_seq_32_event);

static ssize_t etm_show_seq_13_event(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_13_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_13_event(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->seq_13_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(seq_13_event, S_IRUGO | S_IWUSR, etm_show_seq_13_event,
		   etm_store_seq_13_event);

static ssize_t etm_show_seq_curr_state(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_curr_state;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_curr_state(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val > ETM_SEQ_STATE_MAX_VAL)
		return -EINVAL;

	drvdata->seq_curr_state = val;
	return size;
}
static DEVICE_ATTR(seq_curr_state, S_IRUGO | S_IWUSR, etm_show_seq_curr_state,
		   etm_store_seq_curr_state);

static ssize_t etm_show_ctxid_idx(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->ctxid_idx;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_ctxid_idx(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val >= drvdata->nr_ctxid_cmp)
		return -EINVAL;

	/*
	 * Use spinlock to ensure index doesn't change while it gets
	 * dereferenced multiple times within a spinlock block elsewhere.
	 */
	spin_lock(&drvdata->spinlock);
	drvdata->ctxid_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(ctxid_idx, S_IRUGO | S_IWUSR, etm_show_ctxid_idx,
		   etm_store_ctxid_idx);

static ssize_t etm_show_ctxid_val(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	spin_lock(&drvdata->spinlock);
	val = drvdata->ctxid_val[drvdata->ctxid_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_ctxid_val(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->ctxid_val[drvdata->ctxid_idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(ctxid_val, S_IRUGO | S_IWUSR, etm_show_ctxid_val,
		   etm_store_ctxid_val);

static ssize_t etm_show_ctxid_mask(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->ctxid_mask;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_ctxid_mask(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->ctxid_mask = val;
	return size;
}
static DEVICE_ATTR(ctxid_mask, S_IRUGO | S_IWUSR, etm_show_ctxid_mask,
		   etm_store_ctxid_mask);

static ssize_t etm_show_sync_freq(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->sync_freq;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_sync_freq(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->sync_freq = val & ETM_SYNC_MASK;
	return size;
}
static DEVICE_ATTR(sync_freq, S_IRUGO | S_IWUSR, etm_show_sync_freq,
		   etm_store_sync_freq);

static ssize_t etm_show_timestamp_event(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->timestamp_event;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_timestamp_event(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->timestamp_event = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(timestamp_event, S_IRUGO | S_IWUSR, etm_show_timestamp_event,
		   etm_store_timestamp_event);

static ssize_t etm_show_pcsave(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	val = drvdata->pcsave_enable;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static int __etm_store_pcsave(struct etm_drvdata *drvdata, unsigned long val)
{
	int ret = 0;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	spin_lock(&drvdata->spinlock);
	if (val) {
		if (drvdata->pcsave_enable)
			goto out;

		ret = smp_call_function_single(drvdata->cpu, etm_enable_pcsave,
					       drvdata, 1);
		if (ret)
			goto out;
		drvdata->pcsave_enable = true;
		drvdata->pcsave_sticky_enable = true;

		dev_info(drvdata->dev, "PC save enabled\n");
	} else {
		if (!drvdata->pcsave_enable)
			goto out;

		ret = smp_call_function_single(drvdata->cpu, etm_disable_pcsave,
					       drvdata, 1);
		if (ret)
			goto out;
		drvdata->pcsave_enable = false;

		dev_info(drvdata->dev, "PC save disabled\n");
	}
out:
	spin_unlock(&drvdata->spinlock);

	clk_disable_unprepare(drvdata->clk);
	return ret;
}

static ssize_t etm_store_pcsave(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	ret = __etm_store_pcsave(drvdata, val);
	if (ret)
		return ret;

	return size;
}
static DEVICE_ATTR(pcsave, S_IRUGO | S_IWUSR, etm_show_pcsave,
		   etm_store_pcsave);

static ssize_t etm_show_cpu(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	int cpu;

	cpu = drvdata->cpu;
	if (cpu < 0)
		return -EINVAL;
	return scnprintf(buf, PAGE_SIZE, "%#x\n", cpu);
}
static DEVICE_ATTR(cpu, S_IRUGO, etm_show_cpu, NULL);

static struct attribute *etm_attrs[] = {
	&dev_attr_nr_addr_cmp.attr,
	&dev_attr_nr_cntr.attr,
	&dev_attr_nr_ctxid_cmp.attr,
	&dev_attr_reset.attr,
	&dev_attr_mode.attr,
	&dev_attr_trigger_event.attr,
	&dev_attr_enable_event.attr,
	&dev_attr_fifofull_level.attr,
	&dev_attr_addr_idx.attr,
	&dev_attr_addr_single.attr,
	&dev_attr_addr_range.attr,
	&dev_attr_addr_start.attr,
	&dev_attr_addr_stop.attr,
	&dev_attr_addr_acctype.attr,
	&dev_attr_data_val.attr,
	&dev_attr_data_mask.attr,
	&dev_attr_cntr_idx.attr,
	&dev_attr_cntr_rld_val.attr,
	&dev_attr_cntr_event.attr,
	&dev_attr_cntr_rld_event.attr,
	&dev_attr_cntr_val.attr,
	&dev_attr_seq_12_event.attr,
	&dev_attr_seq_21_event.attr,
	&dev_attr_seq_23_event.attr,
	&dev_attr_seq_31_event.attr,
	&dev_attr_seq_32_event.attr,
	&dev_attr_seq_13_event.attr,
	&dev_attr_seq_curr_state.attr,
	&dev_attr_ctxid_idx.attr,
	&dev_attr_ctxid_val.attr,
	&dev_attr_ctxid_mask.attr,
	&dev_attr_sync_freq.attr,
	&dev_attr_timestamp_event.attr,
	&dev_attr_cpu.attr,
	NULL,
};

static struct attribute_group etm_attr_grp = {
	.attrs = etm_attrs,
};

static const struct attribute_group *etm_attr_grps[] = {
	&etm_attr_grp,
	NULL,
};

int coresight_etm_get_funnel_port(int cpu)
{
	struct coresight_platform_data *pdata;

	if (!etmdrvdata[cpu])
		return -ENODEV;

	pdata = etmdrvdata[cpu]->dev->platform_data;
	return pdata->child_ports[0];
}
EXPORT_SYMBOL(coresight_etm_get_funnel_port);

static bool etm_arch_supported(uint8_t arch)
{
	switch (arch) {
	case PFT_ARCH_V1_1:
		break;
	case ETM_ARCH_V3_5:
		break;
	default:
		return false;
	}
	return true;
}

static void etm_init_arch_data(void *info)
{
	uint32_t etmidr;
	uint32_t etmccr;
	uint32_t etmcr;
	struct etm_drvdata *drvdata = info;

	ETM_UNLOCK(drvdata);
	/*
	 * Vote for ETM power/clock enable. ETMPDCR is only accessible via
	 * memory mapped interface and so use it first to enable power/clock
	 * to allow subsequent cp14 accesses.
	 */
	etm_set_pwrup(drvdata);
	/*
	 * Clear power down bit since when this bit is set writes to
	 * certain registers might be ignored.
	 */
	etm_clr_pwrdwn(drvdata);
	etm_clr_pwrup(drvdata);
	/* Set prog bit. It will be set from reset but this is included to
	 * ensure it is set
	 */
	etm_set_prog(drvdata);

	/* find all capabilities */
	etmidr = etm_readl(drvdata, ETMIDR);
	drvdata->arch = BMVAL(etmidr, 4, 11);

	etmccr = etm_readl(drvdata, ETMCCR);
	drvdata->nr_addr_cmp = BMVAL(etmccr, 0, 3) * 2;
	if (drvdata->nr_addr_cmp > ETM_MAX_ADDR_CMP) {
		dev_err(drvdata->dev,
			"nr_addr_cmp out of bounds %u\n", drvdata->nr_addr_cmp);
		drvdata->nr_addr_cmp = ETM_MAX_ADDR_CMP;
	}
	drvdata->nr_cntr = BMVAL(etmccr, 13, 15);
	if (drvdata->nr_cntr > ETM_MAX_CNTR) {
		dev_err(drvdata->dev,
			"nr_cntr out of bounds %u\n", drvdata->nr_cntr);
		drvdata->nr_cntr = ETM_MAX_CNTR;
	}
	drvdata->nr_ext_inp = BMVAL(etmccr, 17, 19);
	if (drvdata->nr_ext_inp > ETM_MAX_EXT_INP) {
		dev_err(drvdata->dev,
			"nr_ext_inp out of bounds %u\n", drvdata->nr_ext_inp);
		drvdata->nr_ext_inp = ETM_MAX_EXT_INP;
	}
	drvdata->nr_ext_out = BMVAL(etmccr, 20, 22);
	if (drvdata->nr_ext_out > ETM_MAX_EXT_OUTP) {
		dev_err(drvdata->dev,
			"nr_ext_out out of bounds %u\n", drvdata->nr_ext_out);
		drvdata->nr_ext_out = ETM_MAX_EXT_OUTP;
	}
	drvdata->nr_ctxid_cmp = BMVAL(etmccr, 24, 25);
	if (drvdata->nr_ctxid_cmp > ETM_MAX_CTXID_CMP) {
		dev_err(drvdata->dev,
			"nr_ctxid_cmp out of bounds %u\n",
			drvdata->nr_ctxid_cmp);
		drvdata->nr_ctxid_cmp = ETM_MAX_CTXID_CMP;
	}
	drvdata->nr_data_cmp = BMVAL(etmccr, 4, 7);
	if (drvdata->nr_data_cmp > ETM_MAX_DATA_CMP) {
		dev_err(drvdata->dev,
			"nr_data_cmp out of bounds %u\n", drvdata->nr_data_cmp);
		drvdata->nr_data_cmp = ETM_MAX_DATA_CMP;
	}

	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_0)) {
		etmcr = etm_readl(drvdata, ETMCR);
		etmcr |= (BIT(2) | BIT(3));
		etm_writel(drvdata, etmcr, ETMCR);
		etmcr = etm_readl(drvdata, ETMCR);
		if (BVAL(etmcr, 2) || BVAL(etmcr, 3))
			drvdata->data_trace_support = true;
		else
			drvdata->data_trace_support = false;
	} else
		drvdata->data_trace_support = false;

	etm_set_pwrdwn(drvdata);
	ETM_LOCK(drvdata);
}

static void etm_init_default_data(struct etm_drvdata *drvdata)
{
	int i;

	drvdata->trigger_event = 0x406F;
	drvdata->enable_event = 0x6F;
	drvdata->enable_ctrl1 = 0x1;
	drvdata->fifofull_level	= 0x28;
	if (drvdata->nr_addr_cmp >= 2) {
		drvdata->addr_val[0] = (uint32_t) _stext;
		drvdata->addr_val[1] = (uint32_t) _etext;
		drvdata->addr_type[0] = ETM_ADDR_TYPE_RANGE;
		drvdata->addr_type[1] = ETM_ADDR_TYPE_RANGE;
		if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_0)) {
			drvdata->addr_acctype[0] = 0x19;
			drvdata->addr_acctype[1] = 0x19;
		}
	}
	for (i = 0; i < drvdata->nr_cntr; i++) {
		drvdata->cntr_event[i] = 0x406F;
		drvdata->cntr_rld_event[i] = 0x406F;
	}
	drvdata->seq_12_event = 0x406F;
	drvdata->seq_21_event = 0x406F;
	drvdata->seq_23_event = 0x406F;
	drvdata->seq_31_event = 0x406F;
	drvdata->seq_32_event = 0x406F;
	drvdata->seq_13_event = 0x406F;
	drvdata->sync_freq = 0x80;
	drvdata->timestamp_event = 0x406F;

	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_0))
		drvdata->ctrl |= BIT(11);
	if (etm_version_gte(drvdata->arch, ETM_ARCH_V1_2))
		drvdata->enable_ctrl2 = 0x0;
	if (drvdata->data_trace_support == true) {
		drvdata->mode |= (ETM_MODE_DATA_TRACE_VAL |
						ETM_MODE_DATA_TRACE_ADDR);
		drvdata->ctrl |= BIT(2) | BIT(3);
		drvdata->viewdata_ctrl1 = 0x0;
		drvdata->viewdata_ctrl3 = 0x10000;
		drvdata->viewdata_event = 0x6F;
	}
	for (i = 0; i < drvdata->nr_data_cmp; i++) {
		drvdata->data_val[i] = 0;
		drvdata->data_mask[i] = ~(0);
	}
}

static int etm_late_init(struct etm_drvdata *drvdata)
{
	void *baddr;
	struct msm_client_dump dump;
	struct msm_dump_entry dump_entry;
	struct coresight_desc *desc;
	struct device *dev = drvdata->dev;
	int ret;

	if (etm_arch_supported(drvdata->arch) == false)
		return -EINVAL;

	etm_init_default_data(drvdata);

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) == 1) {
		baddr = devm_kzalloc(dev, PAGE_SIZE + drvdata->reg_size,
				     GFP_KERNEL);
		if (baddr) {
			dump.id = MSM_ETM0_REG + drvdata->cpu;
			dump.start_addr = virt_to_phys(baddr);
			dump.end_addr = dump.start_addr + PAGE_SIZE +
					drvdata->reg_size;
			ret = msm_dump_tbl_register(&dump);
			if (ret) {
				devm_kfree(dev, baddr);
				dev_err(dev, "ETM REG dump setup failed\n");
			}
		} else {
			dev_err(dev, "ETM REG dump space allocation failed\n");
		}
	} else {
		baddr = devm_kzalloc(dev, drvdata->reg_size, GFP_KERNEL);
		if (baddr) {
			drvdata->reg_data.addr = virt_to_phys(baddr);
			drvdata->reg_data.len = drvdata->reg_size;
			dump_entry.id = MSM_DUMP_DATA_ETM_REG + drvdata->cpu;
			dump_entry.addr = virt_to_phys(&drvdata->reg_data);
			ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
						     &dump_entry);
			if (ret) {
				devm_kfree(dev, baddr);
				dev_err(dev, "ETM REG dump setup failed\n");
			}
		} else {
			dev_err(dev, "ETM REG dump space allocation failed\n");
		}
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto err0;
	}

	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_PROC;
	desc->ops = &etm_cs_ops;
	desc->pdata = drvdata->dev->platform_data;
	desc->dev = drvdata->dev;
	desc->groups = etm_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err1;
	}

	if (drvdata->pcsave_impl) {
		ret = device_create_file(&drvdata->csdev->dev,
					 &dev_attr_pcsave);
		if (ret)
			dev_err(dev, "ETM pcsave dev node creation failed\n");
	}

	dev_info(dev, "ETM initialized\n");

	if (boot_reset)
		etm_reset_data(drvdata);

	if (boot_enable) {
		coresight_enable(drvdata->csdev);
		drvdata->boot_enable = true;
	}

	if (drvdata->pcsave_impl && boot_pcsave_enable) {
		__etm_store_pcsave(drvdata, 1);
		drvdata->pcsave_boot_enable = true;
	}

	return 0;
err1:
	devm_kfree(dev, desc);
err0:
	devm_kfree(dev, baddr);
	return ret;
}

static int etm_cpu_callback(struct notifier_block *nfb, unsigned long action,
			    void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	static bool clk_disable[NR_CPUS];
	int ret;
	struct platform_device *pdev;

	if (!etmdrvdata[cpu])
		goto out;

	switch (action & (~CPU_TASKS_FROZEN)) {
	case CPU_UP_PREPARE:
		if (!etmdrvdata[cpu]->os_unlock) {
			ret = clk_prepare_enable(etmdrvdata[cpu]->clk);
			if (ret) {
				dev_err(etmdrvdata[cpu]->dev,
					"ETM clk enable during hotplug failed"
					"for cpu: %d, ret: %d\n", cpu, ret);
				goto err0;
			}
			clk_disable[cpu] = true;
		}
		break;

	case CPU_STARTING:
		spin_lock(&etmdrvdata[cpu]->spinlock);
		if (!etmdrvdata[cpu]->os_unlock) {
			etm_os_unlock(etmdrvdata[cpu]);
			etmdrvdata[cpu]->os_unlock = true;
			etm_init_arch_data(etmdrvdata[cpu]);
		}

		if (etmdrvdata[cpu]->enable && etmdrvdata[cpu]->round_robin)
			__etm_enable(etmdrvdata[cpu]);
		spin_unlock(&etmdrvdata[cpu]->spinlock);
		break;

	case CPU_ONLINE:
		mutex_lock(&etmdrvdata[cpu]->mutex);
		if (!etmdrvdata[cpu]->init) {
			ret = etm_late_init(etmdrvdata[cpu]);
			if (ret) {
				dev_err(etmdrvdata[cpu]->dev,
					"ETM init failed. Cpu: %d, ret: %d\n",
					cpu, ret);
				mutex_unlock(&etmdrvdata[cpu]->mutex);
				goto err1;
			}
			etmdrvdata[cpu]->init = true;
		}
		mutex_unlock(&etmdrvdata[cpu]->mutex);

		if (clk_disable[cpu]) {
			clk_disable_unprepare(etmdrvdata[cpu]->clk);
			clk_disable[cpu] = false;
		}

		if (etmdrvdata[cpu]->boot_enable &&
		    !etmdrvdata[cpu]->sticky_enable)
			coresight_enable(etmdrvdata[cpu]->csdev);

		if (etmdrvdata[cpu]->pcsave_boot_enable &&
		    !etmdrvdata[cpu]->pcsave_sticky_enable)
			__etm_store_pcsave(etmdrvdata[cpu], 1);
		break;

	case CPU_UP_CANCELED:
		if (clk_disable[cpu]) {
			clk_disable_unprepare(etmdrvdata[cpu]->clk);
			clk_disable[cpu] = false;
		}
		break;

	case CPU_DYING:
		spin_lock(&etmdrvdata[cpu]->spinlock);
		if (etmdrvdata[cpu]->enable && etmdrvdata[cpu]->round_robin)
			__etm_disable(etmdrvdata[cpu]);
		spin_unlock(&etmdrvdata[cpu]->spinlock);
		break;
	}
out:
	return NOTIFY_OK;
err1:
	if (--count == 0)
		unregister_hotcpu_notifier(&etm_cpu_notifier);
	if (clk_disable[cpu]) {
		clk_disable_unprepare(etmdrvdata[cpu]->clk);
		clk_disable[cpu] = false;
	}
	devm_clk_put(etmdrvdata[cpu]->dev, etmdrvdata[cpu]->clk);
	wakeup_source_trash(&etmdrvdata[cpu]->ws);
	devm_iounmap(etmdrvdata[cpu]->dev, etmdrvdata[cpu]->base);
	pdev = to_platform_device(etmdrvdata[cpu]->dev);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(etmdrvdata[cpu]->dev, etmdrvdata[cpu]);
	etmdrvdata[cpu] = NULL;
err0:
	return notifier_from_errno(ret);
}

static struct notifier_block etm_cpu_notifier = {
	.notifier_call = etm_cpu_callback,
};

static int etm_probe(struct platform_device *pdev)
{
	int ret, cpu;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct etm_drvdata *drvdata;
	struct resource *res;
	struct device_node *cpu_node;

	pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "etm-base");
	if (!res)
		return -ENODEV;
	drvdata->reg_size = resource_size(res);

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	spin_lock_init(&drvdata->spinlock);
	mutex_init(&drvdata->mutex);
	wakeup_source_init(&drvdata->ws, "coresight-etm");

	drvdata->pcsave_impl = of_property_read_bool(pdev->dev.of_node,
						     "qcom,pc-save");

	drvdata->cpu = -1;
	cpu_node = of_parse_phandle(pdev->dev.of_node, "coresight-etm-cpu", 0);
	if (!cpu_node) {
		dev_err(drvdata->dev, "ETM cpu handle not specified\n");
		ret = -ENODEV;
		goto err0;
	}
	for_each_possible_cpu(cpu) {
		if (cpu_node == of_get_cpu_node(cpu, NULL)) {
			drvdata->cpu = cpu;
			break;
		}
	}
	if (drvdata->cpu == -1) {
		dev_err(drvdata->dev, "invalid ETM cpu handle\n");
		ret = -EINVAL;
		goto err0;
	}

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk)) {
		ret = PTR_ERR(drvdata->clk);
		goto err0;
	}

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		goto err0;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		goto err0;

	if (!coresight_authstatus_enabled(drvdata->base)) {
		clk_disable_unprepare(drvdata->clk);
		wakeup_source_trash(&drvdata->ws);
		return -EPERM;
	}

	if (count++ == 0)
		register_hotcpu_notifier(&etm_cpu_notifier);

	get_online_cpus();

	/*
	 * This is safe wrt CPU_UP_PREPARE and CPU_STARTING hotplug callbacks
	 * on the non-boot CPUs that may enable the clock and perform
	 * etm_os_unlock since they occur before the cpu online mask is updated
	 * for the cpu which is checked by this smp call.
	 */
	if (!smp_call_function_single(drvdata->cpu, etm_os_unlock, drvdata,
				      1)) {
		drvdata->os_unlock = true;
		ret = smp_call_function_single(drvdata->cpu, etm_init_arch_data,
					 drvdata, 1);
		if (ret) {
			put_online_cpus();
			clk_disable_unprepare(drvdata->clk);
			goto err1;
		}
	}

	etmdrvdata[drvdata->cpu] = drvdata;

	put_online_cpus();

	clk_disable_unprepare(drvdata->clk);

	mutex_lock(&drvdata->mutex);
	if (drvdata->os_unlock && !drvdata->init) {
		ret = etm_late_init(drvdata);
		if (ret) {
			mutex_unlock(&drvdata->mutex);
			goto err1;
		}
		drvdata->init = true;
	}
	mutex_unlock(&drvdata->mutex);

	return 0;
err1:
	if (--count == 0)
		unregister_hotcpu_notifier(&etm_cpu_notifier);
err0:
	wakeup_source_trash(&drvdata->ws);
	return ret;
}

static int etm_remove(struct platform_device *pdev)
{
	struct etm_drvdata *drvdata = platform_get_drvdata(pdev);

	if (drvdata) {
		device_remove_file(&drvdata->csdev->dev, &dev_attr_pcsave);
		coresight_unregister(drvdata->csdev);
		if (--count == 0)
			unregister_hotcpu_notifier(&etm_cpu_notifier);
		wakeup_source_trash(&drvdata->ws);
	}
	return 0;
}

static struct of_device_id etm_match[] = {
	{.compatible = "arm,coresight-etm"},
	{}
};

static struct platform_driver etm_driver = {
	.probe          = etm_probe,
	.remove         = etm_remove,
	.driver         = {
		.name   = "coresight-etm",
		.owner	= THIS_MODULE,
		.of_match_table = etm_match,
	},
};

int __init etm_init(void)
{
	return platform_driver_register(&etm_driver);
}
module_init(etm_init);

void __exit etm_exit(void)
{
	platform_driver_unregister(&etm_driver);
}
module_exit(etm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Program Flow Trace driver");
