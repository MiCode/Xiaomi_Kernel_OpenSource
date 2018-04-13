/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/of_coresight.h>
#include <linux/of_address.h>
#include <linux/coresight.h>
#include <linux/pm_wakeup.h>
#include <linux/cpumask.h>
#include <asm/sections.h>
#include <asm/etmv4x.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/memory_dump.h>

#include "coresight-priv.h"

#define etm_writel(drvdata, val, off)					\
		   __raw_writel((val), drvdata->base + off)
#define etm_readl(drvdata, off)						\
		  __raw_readl(drvdata->base + off)

#define etm_writeq(drvdata, val, off)					\
		   __raw_writeq((val), drvdata->base + off)
#define etm_readq(drvdata, off)						\
		  __raw_readq(drvdata->base + off)

#define ETM_LOCK(drvdata)						\
do {									\
	mb();								\
	isb();								\
	etm_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define ETM_UNLOCK(drvdata)						\
do {									\
	etm_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
	isb();								\
} while (0)

/*
 * Device registers:
 * 0x000 - 0x2FC: Trace         registers
 * 0x300 - 0x314: Management    registers
 * 0x318 - 0xEFC: Trace         registers
 * 0xF00: Management		registers
 * 0xFA0 - 0xFA4: Trace		registers
 * 0xFA8 - 0xFFC: Management	registers
 */
/* Trace registers (0x000-0x2FC) */
/* Main control and configuration registers */
#define TRCPRGCTLR			(0x004)
#define TRCPROCSELR			(0x008)
#define TRCSTATR			(0x00C)
#define TRCCONFIGR			(0x010)
#define TRCAUXCTLR			(0x018)
#define TRCEVENTCTL0R			(0x020)
#define TRCEVENTCTL1R			(0x024)
#define TRCSTALLCTLR			(0x02C)
#define TRCTSCTLR			(0x030)
#define TRCSYNCPR			(0x034)
#define TRCCCCTLR			(0x038)
#define TRCBBCTLR			(0x03C)
#define TRCTRACEIDR			(0x040)
#define TRCQCTLR			(0x044)
/* Filtering control registers */
#define TRCVICTLR			(0x080)
#define TRCVIIECTLR			(0x084)
#define TRCVISSCTLR			(0x088)
#define TRCVIPCSSCTLR			(0x08C)
#define TRCVDCTLR			(0x0A0)
#define TRCVDSACCTLR			(0x0A4)
#define TRCVDARCCTLR			(0x0A8)
/* Derived resources registers */
#define TRCSEQEVRn(n)			(0x100 + (n * 4))
#define TRCSEQRSTEVR			(0x118)
#define TRCSEQSTR			(0x11C)
#define TRCEXTINSELR			(0x120)
#define TRCCNTRLDVRn(n)			(0x140 + (n * 4))
#define TRCCNTCTLRn(n)			(0x150 + (n * 4))
#define TRCCNTVRn(n)			(0x160 + (n * 4))
/* ID registers */
#define TRCIDR8				(0x180)
#define TRCIDR9				(0x184)
#define TRCIDR10			(0x188)
#define TRCIDR11			(0x18C)
#define TRCIDR12			(0x190)
#define TRCIDR13			(0x194)
#define TRCIMSPEC0			(0x1C0)
#define TRCIMSPECn(n)			(0x1C0 + (n * 4))
#define TRCIDR0				(0x1E0)
#define TRCIDR1				(0x1E4)
#define TRCIDR2				(0x1E8)
#define TRCIDR3				(0x1EC)
#define TRCIDR4				(0x1F0)
#define TRCIDR5				(0x1F4)
#define TRCIDR6				(0x1F8)
#define TRCIDR7				(0x1FC)
/* Resource selection registers */
#define TRCRSCTLRn(n)			(0x200 + (n * 4))
/* Single-shot comparator registers */
#define TRCSSCCRn(n)			(0x280 + (n * 4))
#define TRCSSCSRn(n)			(0x2A0 + (n * 4))
#define TRCSSPCICRn(n)			(0x2C0 + (n * 4))
/* Management registers (0x300-0x314) */
#define TRCOSLAR			(0x300)
#define TRCOSLSR			(0x304)
#define TRCPDCR				(0x310)
#define TRCPDSR				(0x314)
/* Trace registers (0x318-0xEFC) */
/* Comparator registers */
#define TRCACVRn(n)			(0x400 + (n * 8))
#define TRCACATRn(n)			(0x480 + (n * 8))
#define TRCDVCVRn(n)			(0x500 + (n * 16))
#define TRCDVCMRn(n)			(0x580 + (n * 16))
#define TRCCIDCVRn(n)			(0x600 + (n * 8))
#define TRCVMIDCVRn(n)			(0x640 + (n * 8))
#define TRCCIDCCTLR0			(0x680)
#define TRCCIDCCTLR1			(0x684)
#define TRCVMIDCCTLR0			(0x688)
#define TRCVMIDCCTLR1			(0x68C)
/* Management register (0xF00) */
/* Integration control registers */
#define TRCITCTRL			(0xF00)
/* Trace registers (0xFA0-0xFA4) */
/* Claim tag registers */
#define TRCCLAIMSET			(0xFA0)
#define TRCCLAIMCLR			(0xFA4)
/* Management registers (0xFA8-0xFFC) */
#define TRCDEVAFF0			(0xFA8)
#define TRCDEVAFF1			(0xFAC)
#define TRCLAR				(0xFB0)
#define TRCLSR				(0xFB4)
#define TRCAUTHSTATUS			(0xFB8)
#define TRCDEVARCH			(0xFBC)
#define TRCDEVID			(0xFC8)
#define TRCDEVTYPE			(0xFCC)
#define TRCPIDR4			(0xFD0)
#define TRCPIDR5			(0xFD4)
#define TRCPIDR6			(0xFD8)
#define TRCPIDR7			(0xFDC)
#define TRCPIDR0			(0xFE0)
#define TRCPIDR1			(0xFE4)
#define TRCPIDR2			(0xFE8)
#define TRCPIDR3			(0xFEC)
#define TRCCIDR0			(0xFF0)
#define TRCCIDR1			(0xFF4)
#define TRCCIDR2			(0xFF8)
#define TRCCIDR3			(0xFFC)

/* ETMv4 resources */
#define ETM_MAX_NR_PE			(8)
#define ETM_MAX_CNTR			(4)
#define ETM_MAX_SEQ_STATES		(4)
#define ETM_MAX_EXT_INP_SEL		(4)
#define ETM_MAX_EXT_INP			(256)
#define ETM_MAX_EXT_OUT			(4)
#define ETM_MAX_SINGLE_ADDR_CMP		(16)
#define ETM_MAX_ADDR_RANGE_CMP		(ETM_MAX_SINGLE_ADDR_CMP / 2)
#define ETM_MAX_DATA_VAL_CMP		(8)
#define ETM_MAX_CTXID_CMP		(8)
#define ETM_MAX_VMID_CMP		(8)
#define ETM_MAX_PE_CMP			(8)
#define ETM_MAX_RES_SEL			(16)
#define ETM_MAX_SS_CMP			(8)

#define ETM_CPMR_CLKEN			(0x4)
#define ETM_ARCH_V4			(0x40)
#define ETM_SYNC_MASK                   (0x1F)
#define ETM_CYC_THRESHOLD_MASK		(0xFFF)
#define ETM_EVENT_MASK			(0xFF)
#define ETM_CNTR_MAX_VAL		(0xFFFF)

/* ETMv4 programming modes */
#define ETM_MODE_EXCLUDE		BIT(0)
#define ETM_MODE_LOAD			BIT(1)
#define ETM_MODE_STORE			BIT(2)
#define ETM_MODE_LOAD_STORE		BIT(3)
#define ETM_MODE_BB			BIT(4)
#define ETM_MODE_CYCACC			BIT(5)
#define ETM_MODE_CTXID			BIT(6)
#define ETM_MODE_VMID			BIT(7)
#define ETM_MODE_COND(val)		BMVAL(val, 8, 10)
#define ETM_MODE_TIMESTAMP		BIT(11)
#define ETM_MODE_RETURNSTACK		BIT(12)
#define ETM_MODE_QELEM(val)		BMVAL(val, 13, 14)
#define ETM_MODE_DATA_TRACE_ADDR	BIT(15)
#define ETM_MODE_DATA_TRACE_VAL		BIT(16)
#define ETM_MODE_ISTALL			BIT(17)
#define ETM_MODE_DSTALL			BIT(18)
#define ETM_MODE_ATB_TRIGGER		BIT(19)
#define ETM_MODE_LPOVERRIDE		BIT(20)
#define ETM_MODE_ISTALL_EN		BIT(21)
#define ETM_MODE_DSTALL_EN		BIT(22)
#define ETM_MODE_INSTPRIO		BIT(23)
#define ETM_MODE_NOOVERFLOW		BIT(24)
#define ETM_MODE_TRACE_RESET		BIT(25)
#define ETM_MODE_TRACE_ERR		BIT(26)
#define ETM_MODE_VIEWINST_STARTSTOP	BIT(27)
#define ETM_MODE_ALL			(0xFFFFFFF)

#define ETM_REG_DUMP_VER_OFF		(4)
#define ETM_REG_DUMP_VER		(1)

#ifdef CONFIG_CORESIGHT_ETMV4_DEFAULT_RESET
static int boot_reset = 1;
#else
static int boot_reset;
#endif
module_param_named(
	boot_reset, boot_reset, int, S_IRUGO
);

#ifdef CONFIG_CORESIGHT_ETMV4_DEFAULT_ENABLE
static int boot_enable = 1;
#else
static int boot_enable;
#endif
module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

enum etm_addr_type {
	ETM_ADDR_TYPE_NONE,
	ETM_ADDR_TYPE_SINGLE,
	ETM_ADDR_TYPE_RANGE,
	ETM_ADDR_TYPE_START,
	ETM_ADDR_TYPE_STOP,
};

/* Address comparator access types */
enum etm_addr_acctype {
	ETM_INSTR_ADDR,
	ETM_DATA_LOAD_ADDR,
	ETM_DATA_STORE_ADDR,
	ETM_DATA_LOAD_STORE_ADDR,
};

/* Address comparator context types */
enum etm_addr_ctxtype {
	ETM_CTX_NONE,
	ETM_CTX_CTXID,
	ETM_CTX_VMID,
	ETM_CTX_CTXID_VMID,
};

/* Data value match */
enum etm_data_match {
	ETM_DATA_CMP_NONE,
	ETM_DATA_CMP_MATCH,
	ETM_DATA_CMP_RESERVED,
	ETM_DATA_CMP_MISMATCH
};

/* Data value comparator size */
enum etm_data_size {
	ETM_DATA_TYPE_BYTE,
	ETM_DATA_TYPE_HWORD,
	ETM_DATA_TYPE_WORD,
	ETM_DATA_TYPE_DWORD,
};

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
	uint8_t				nr_pe;
	uint8_t				nr_pe_cmp;
	uint8_t				nr_addr_cmp; /* comparator pairs */
	uint8_t				nr_data_cmp;
	uint8_t				nr_cntr;
	uint32_t			nr_ext_inp;
	uint8_t				nr_ext_inp_sel;
	uint8_t				nr_ext_out;
	uint8_t				nr_ctxid_cmp;
	uint8_t				nr_vmid_cmp;
	uint8_t				nr_seq_state;
	uint8_t				nr_event;
	uint8_t				nr_resource;
	uint8_t				nr_ss_cmp;
	uint8_t				reset;
	uint32_t			mode;
	uint8_t				instr_addr_size;
	uint8_t				trcid;
	uint8_t				trcid_size;
	bool				enable_noovrflw;
	bool				instrp0_support;
	bool				trc_cond_support;
	bool				cond_type_support;
	bool				retstack_support;
	bool				trc_exdata_support;
	bool				trc_error_support;
	bool				atbtrig_support;
	bool				lp_override_support;
	bool				reduced_cntr_support;
	uint32_t			pe_sel;
	uint32_t			cfg; /* controls trace options */
	uint32_t			event_ctrl0;
	uint32_t			event_ctrl1;
	bool				stallctrl_support;
	bool				stall_pe_support;
	bool				no_overflow_support;
	uint32_t			stall_ctrl;
	uint8_t				ts_size;
	uint32_t			ts_ctrl;
	bool				syncpr_fixed;
	uint32_t			syncfreq;
	bool				trc_cyccnt_support;
	uint8_t				cyccnt_size;
	uint8_t				cyccnt_min;
	uint32_t			cyccnt_ctrl;
	bool				trc_bb_support;
	uint32_t			bb_ctrl;
	bool				qfilt_support;
	bool				q_support;
	uint32_t			qelem;
	uint32_t			vinst_ctrl;
	uint32_t			vinst_incex_ctrl;
	uint32_t			vinst_startstop_ctrl;
	uint32_t			vinst_startstop_pe_ctrl;
	uint32_t			vdata_ctrl;
	uint32_t			vdata_incex_single;
	uint32_t			vdata_incex_range;
	uint8_t				seq_idx;
	uint32_t			seq_ctrl[ETM_MAX_SEQ_STATES];
	uint32_t			seq_rst;
	uint32_t			seq_state;
	uint8_t				cntr_idx;
	uint32_t			cntr_rld_val[ETM_MAX_CNTR];
	uint32_t			cntr_ctrl[ETM_MAX_CNTR];
	uint32_t			cntr_val[ETM_MAX_CNTR];
	uint8_t				resource_idx;
	uint32_t			resource_ctrl[ETM_MAX_RES_SEL];
	uint8_t				ss_idx;
	uint32_t			ss_ctrl[ETM_MAX_SS_CMP];
	uint32_t			ss_status[ETM_MAX_SS_CMP];
	uint32_t			ss_pe_cmp[ETM_MAX_SS_CMP];
	uint8_t				addr_idx;
	uint64_t			addr_val[ETM_MAX_SINGLE_ADDR_CMP];
	uint64_t			addr_acctype[ETM_MAX_SINGLE_ADDR_CMP];
	uint8_t				addr_type[ETM_MAX_SINGLE_ADDR_CMP];
	bool				trc_data_support;
	bool				data_addr_cmp_support;
	uint8_t				data_addr_size;
	uint8_t				data_val_size;
	uint8_t				data_idx;
	uint64_t			data_val[ETM_MAX_DATA_VAL_CMP];
	uint64_t			data_mask[ETM_MAX_DATA_VAL_CMP];
	uint8_t				ctxid_idx;
	uint8_t				ctxid_size;
	uint64_t			ctxid_val[ETM_MAX_CTXID_CMP];
	uint32_t			ctxid_mask0;
	uint32_t			ctxid_mask1;
	uint8_t				vmid_idx;
	uint8_t				vmid_size;
	uint64_t			vmid_val[ETM_MAX_VMID_CMP];
	uint32_t			vmid_mask0;
	uint32_t			vmid_mask1;
	uint8_t				commit_mode;
	uint8_t				s_ex_level;
	uint8_t				ns_ex_level;
	uint32_t			ext_inp;
	struct msm_dump_data		reg_data;
	struct etm_cgc_data		*cgc_data;
};

/* support max 2 clusters now */
#define MAX_CLUSTER_NUM		2
#define NTS_CGC_OVERRIDE	BIT(9)
#define CNT_CGC_OVERRIDE	BIT(8)
#define APB_CGC_OVERRIDE	BIT(7)
#define ATB_CGC_OVERRIDE	BIT(6)

struct etm_cgc_data {
	void __iomem		*base;
	uint32_t		phy_addr;
	uint32_t		len;
	cpumask_t		control_mask;
};

static struct etm_cgc_data *etm_cgc_data[MAX_CLUSTER_NUM];

static int count;
static struct etm_drvdata *etmdrvdata[NR_CPUS];
static struct notifier_block etm_cpu_notifier;
static struct notifier_block etm_cpu_dying_notifier;

static bool etm_os_lock_present(struct etm_drvdata *drvdata)
{
	uint32_t etmoslsr;

	etmoslsr = etm_readl(drvdata, TRCOSLSR);
	/*
	 * For ARMv7-A, ARMv7-R, and ARMv8-A PEs bits[0,3] are always
	 * 0b10 to indicate that the OS Lock is implemented.
	 */
	if ((BVAL(etmoslsr, 0) == 0) && (BVAL(etmoslsr, 3) == 1))
		return true;

	return false;
}

static void etm_os_unlock(void *info)
{
	struct etm_drvdata *drvdata = (struct etm_drvdata *) info;

	ETM_UNLOCK(drvdata);
	if (etm_os_lock_present(drvdata)) {
		etm_writel(drvdata, 0x0, TRCOSLAR);
		/* ensure os lock is unlocked before we return */
		mb();
	}
	ETM_LOCK(drvdata);
}

static bool etm_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ETM_ARCH_V4:
		break;
	default:
		return false;
	}
	return true;
}

static int etm_set_mode_exclude(struct etm_drvdata *drvdata, bool exclude)
{
	uint8_t idx = drvdata->addr_idx;

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP)
		return -EINVAL;

	if (BMVAL(drvdata->addr_acctype[idx], 0, 1) == ETM_INSTR_ADDR) {
		if (idx % 2 != 0)
			return -EINVAL;

		/*
		 * We are performing instruction address comparison. Set the
		 * relevant bit of ViewInst Include/Exclude Control register
		 * for corresponding address comparator pair.
		 */
		if (drvdata->addr_type[idx] != ETM_ADDR_TYPE_RANGE ||
		    drvdata->addr_type[idx + 1] != ETM_ADDR_TYPE_RANGE)
			return -EINVAL;

		if (exclude == true) {
			/*
			 * Set exclude bit and unset the include bit
			 * corresponding to comparator pair
			 */
			drvdata->vinst_incex_ctrl |= BIT(idx / 2 + 16);
			drvdata->vinst_incex_ctrl &= ~BIT(idx / 2);
		} else {
			/*
			 * Set include bit and unset exclude bit
			 * corresponding to comparator pair
			 */
			drvdata->vinst_incex_ctrl |= BIT(idx / 2);
			drvdata->vinst_incex_ctrl &= ~BIT(idx / 2 + 16);
		}
	} else {
		/*
		 * We are performing data address comparison. Set the relevant
		 * bit of ViewData Include/Exclude address range comparator or
		 * single address comparator for corresponding address
		 * comparator.
		 */
		if (drvdata->addr_type[idx] == ETM_ADDR_TYPE_RANGE &&
		    drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_RANGE) {
			if (idx % 2 != 0)
				return -EINVAL;

			if (exclude == true) {
				/*
				 * Set exclude bit and unset include bit
				 * corresponding to comparator pair.
				 */
				drvdata->vdata_incex_range |= BIT(idx / 2 + 16);
				drvdata->vdata_incex_range &= ~BIT(idx / 2);
			} else {
				/*
				 * Set include bit cnd unset include bit
				 * corresponding to comparator pair.
				 */
				drvdata->vdata_incex_range |= BIT(idx / 2);
				drvdata->vdata_incex_range &= ~BIT(idx / 2
								   + 16);
			}
		} else if (drvdata->addr_type[idx] == ETM_ADDR_TYPE_SINGLE) {
			/* Set single address comparator for Viewdata */
			if (exclude == true) {
				drvdata->vdata_incex_single |= BIT(idx + 16);
				drvdata->vdata_incex_single &= ~BIT(idx);
			} else {
				drvdata->vdata_incex_single |= BIT(idx);
				drvdata->vdata_incex_single &= ~BIT(idx + 16);
			}
		}
	}
	return 0;
}

static void etm_reset_data(struct etm_drvdata *drvdata)
{
	int i;

	spin_lock(&drvdata->spinlock);

	drvdata->mode = 0x0;

	/* Disable data tracing: do not trace load and store data transfers */
	drvdata->mode &= ~(ETM_MODE_LOAD | ETM_MODE_STORE);
	drvdata->cfg &= ~(BIT(1) | BIT(2));
	/* Disable data value and data address tracing */
	drvdata->mode &= ~(ETM_MODE_DATA_TRACE_ADDR |
				  ETM_MODE_DATA_TRACE_VAL);
	drvdata->cfg &= ~(BIT(16) | BIT(17));

	/* Disable all events tracing */
	drvdata->event_ctrl0 = 0x0;
	drvdata->event_ctrl1 = 0x0;

	/* Disable timestamp event */
	drvdata->ts_ctrl = 0x0;

	/* Disable stalling */
	drvdata->stall_ctrl = 0x0;

	/* Reset trace synchronization period  to 2^8 = 256 bytes*/
	if (drvdata->syncpr_fixed == false)
		drvdata->syncfreq = 0x8;

	/*
	 * Enable ViewInst to trace everything with start-stop logic in
	 * started state. ARM recommends start-stop logic is set before
	 * each trace run.
	 */
	drvdata->vinst_ctrl |= BIT(0);
	if (drvdata->nr_addr_cmp) {
		drvdata->mode |= ETM_MODE_VIEWINST_STARTSTOP;
		drvdata->vinst_ctrl |= BIT(9);
	}

	/* No address range filtering for ViewInst */
	drvdata->vinst_incex_ctrl = 0x0;

	/* No start-stop filtering for ViewInst */
	drvdata->vinst_startstop_ctrl = 0x0;

	/* Disable ViewData */
	drvdata->vdata_ctrl = 0x0;
	/* No address filtering for ViewData */
	drvdata->vdata_incex_single = 0x0;
	drvdata->vdata_incex_range = 0x0;

	/* Disable seq events */
	for (i = 0; i < drvdata->nr_seq_state-1; i++)
		drvdata->seq_ctrl[i] = 0x0;
	drvdata->seq_rst = 0x0;
	drvdata->seq_state = 0x0;

	/* Disable external input events */
	drvdata->ext_inp = 0x0;

	drvdata->cntr_idx = 0x0;
	for (i = 0; i < drvdata->nr_cntr; i++) {
		drvdata->cntr_rld_val[i] = 0x0;
		drvdata->cntr_ctrl[i] = 0x0;
		drvdata->cntr_val[i] = 0x0;
	}

	drvdata->resource_idx = 0x0;
	for (i = 0; i < drvdata->nr_resource; i++)
		drvdata->resource_ctrl[i] = 0x0;

	drvdata->ss_idx = 0x0;
	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		drvdata->ss_ctrl[i] = 0x0;
		drvdata->ss_pe_cmp[i] = 0x0;
	}

	drvdata->addr_idx = 0x0;
	for (i = 0; i < drvdata->nr_addr_cmp * 2; i++) {
		drvdata->addr_val[i] = 0x0;
		drvdata->addr_acctype[i] = 0x0;
		drvdata->addr_type[i] = ETM_ADDR_TYPE_NONE;
	}

	drvdata->data_idx = 0x0;
	for (i = 0; i < drvdata->nr_data_cmp; i++) {
		drvdata->data_val[i] = 0;
		drvdata->data_mask[i] = ~(0);
	}

	drvdata->ctxid_idx = 0x0;
	for (i = 0; i < drvdata->nr_ctxid_cmp; i++)
		drvdata->ctxid_val[i] = 0x0;
	drvdata->ctxid_mask0 = 0x0;
	drvdata->ctxid_mask1 = 0x0;

	drvdata->vmid_idx = 0x0;
	for (i = 0; i < drvdata->nr_vmid_cmp; i++)
		drvdata->vmid_val[i] = 0x0;
	drvdata->vmid_mask0 = 0x0;
	drvdata->vmid_mask1 = 0x0;

	drvdata->trcid = drvdata->cpu + 1;

	spin_unlock(&drvdata->spinlock);
}

static inline void etm_clk_disable(void)
{
	uint32_t cpmr;

	isb();
	cpmr = trc_readl(CPMR_EL1);
	cpmr  &= ~ETM_CPMR_CLKEN;
	trc_write(cpmr, CPMR_EL1);
}

static inline void etm_clk_enable(void)
{
	uint32_t cpmr;

	cpmr = trc_readl(CPMR_EL1);
	cpmr  |= ETM_CPMR_CLKEN;
	trc_write(cpmr, CPMR_EL1);
	isb();
}

static inline void etm_trace_disable(void)
{
	etm_clk_enable();
	trc_write(0x0, ETMPRGCTLR);
	etm_clk_disable();
}

static inline void etm_trace_enable(void)
{
	etm_clk_enable();
	trc_write(0x1, ETMPRGCTLR);
	etm_clk_disable();
}

static void __etm_enable(void *info)
{
	int i;
	struct etm_drvdata *drvdata = info;

	ETM_UNLOCK(drvdata);

	/* Disable the trace unit before programming trace registers */
	if (drvdata->enable_noovrflw)
		etm_trace_disable();
	else
		etm_writel(drvdata, 0, TRCPRGCTLR);

	etm_writel(drvdata, drvdata->pe_sel, TRCPROCSELR);
	etm_writel(drvdata, drvdata->cfg, TRCCONFIGR);
	etm_writel(drvdata, 0x0, TRCAUXCTLR);
	etm_writel(drvdata, drvdata->event_ctrl0, TRCEVENTCTL0R);
	etm_writel(drvdata, drvdata->event_ctrl1, TRCEVENTCTL1R);
	etm_writel(drvdata, drvdata->stall_ctrl, TRCSTALLCTLR);
	etm_writel(drvdata, drvdata->ts_ctrl, TRCTSCTLR);
	etm_writel(drvdata, drvdata->syncfreq, TRCSYNCPR);
	etm_writel(drvdata, drvdata->cyccnt_ctrl, TRCCCCTLR);
	etm_writel(drvdata, drvdata->bb_ctrl, TRCBBCTLR);
	etm_writel(drvdata, drvdata->trcid, TRCTRACEIDR);
	etm_writel(drvdata, drvdata->qelem, TRCQCTLR);
	etm_writel(drvdata, drvdata->vinst_ctrl, TRCVICTLR);
	etm_writel(drvdata, drvdata->vinst_incex_ctrl, TRCVIIECTLR);
	etm_writel(drvdata, drvdata->vinst_startstop_ctrl, TRCVISSCTLR);
	etm_writel(drvdata, drvdata->vinst_startstop_pe_ctrl, TRCVIPCSSCTLR);
	etm_writel(drvdata, drvdata->vdata_ctrl, TRCVDCTLR);
	etm_writel(drvdata, drvdata->vdata_incex_single, TRCVDSACCTLR);
	etm_writel(drvdata, drvdata->vdata_incex_range, TRCVDARCCTLR);
	for (i = 0; i < drvdata->nr_seq_state - 1; i++)
		etm_writel(drvdata, drvdata->seq_ctrl[i], TRCSEQEVRn(i));
	etm_writel(drvdata, drvdata->seq_rst, TRCSEQRSTEVR);
	etm_writel(drvdata, drvdata->seq_state, TRCSEQSTR);
	etm_writel(drvdata, drvdata->ext_inp, TRCEXTINSELR);
	for (i = 0; i < drvdata->nr_cntr; i++) {
		etm_writel(drvdata, drvdata->cntr_rld_val[i], TRCCNTRLDVRn(i));
		etm_writel(drvdata,  drvdata->cntr_ctrl[i], TRCCNTCTLRn(i));
		etm_writel(drvdata, drvdata->cntr_val[i], TRCCNTVRn(i));
	}
	for (i = 0; i < drvdata->nr_resource; i++)
		etm_writel(drvdata, drvdata->resource_ctrl[i], TRCRSCTLRn(i));

	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		etm_writel(drvdata, drvdata->ss_ctrl[i], TRCSSCCRn(i));
		etm_writel(drvdata, drvdata->ss_status[i], TRCSSCSRn(i));
		etm_writel(drvdata, drvdata->ss_pe_cmp[i], TRCSSPCICRn(i));
	}
	for (i = 0; i < drvdata->nr_addr_cmp; i++) {
		etm_writeq(drvdata, drvdata->addr_val[i], TRCACVRn(i));
		etm_writeq(drvdata, drvdata->addr_acctype[i], TRCACATRn(i));
	}
	for (i = 0; i < drvdata->nr_data_cmp; i++) {
		etm_writeq(drvdata, drvdata->data_val[i], TRCDVCVRn(i));
		etm_writeq(drvdata, drvdata->data_mask[i], TRCDVCMRn(i));
	}

	for (i = 0; i < drvdata->nr_ctxid_cmp; i++)
		etm_writeq(drvdata, drvdata->ctxid_val[i], TRCCIDCVRn(i));
	etm_writel(drvdata, drvdata->ctxid_mask0, TRCCIDCCTLR0);
	etm_writel(drvdata, drvdata->ctxid_mask1, TRCCIDCCTLR1);

	for (i = 0; i < drvdata->nr_vmid_cmp; i++)
		etm_writeq(drvdata, drvdata->vmid_val[i], TRCVMIDCVRn(i));
	etm_writel(drvdata, drvdata->vmid_mask0, TRCVMIDCCTLR0);
	etm_writel(drvdata, drvdata->vmid_mask1, TRCVMIDCCTLR1);

	/* Enable the trace unit */
	if (drvdata->enable_noovrflw)
		etm_trace_enable();
	else
		etm_writel(drvdata, 1, TRCPRGCTLR);

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

	dev_info(drvdata->dev, "ETMv4 tracing enabled\n");
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

	mb();
	isb();
	if (drvdata->enable_noovrflw)
		etm_trace_disable();
	else
		etm_writel(drvdata, 0, TRCPRGCTLR);

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
	.enable         = etm_enable,
	.disable        = etm_disable,
};

static const struct coresight_ops etm_cs_ops = {
	.source_ops     = &etm_source_ops,
};

static ssize_t etm_show_nr_pe_cmp(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_pe_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_pe_cmp, S_IRUGO, etm_show_nr_pe_cmp, NULL);

static ssize_t etm_show_nr_addr_cmp(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_addr_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_addr_cmp, S_IRUGO, etm_show_nr_addr_cmp, NULL);

static ssize_t etm_show_nr_data_cmp(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_data_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_data_cmp, S_IRUGO, etm_show_nr_data_cmp, NULL);

static ssize_t etm_show_nr_cntr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_cntr;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_cntr, S_IRUGO, etm_show_nr_cntr, NULL);

static ssize_t etm_show_nr_ext_inp(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_ext_inp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_ext_inp, S_IRUGO, etm_show_nr_ext_inp, NULL);

static ssize_t etm_show_nr_ext_out(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_ext_out;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_ext_out, S_IRUGO, etm_show_nr_ext_out, NULL);

static ssize_t etm_show_nr_ctxid_cmp(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_ctxid_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_ctxid_cmp, S_IRUGO, etm_show_nr_ctxid_cmp, NULL);

static ssize_t etm_show_nr_vmid_cmp(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_vmid_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_vmid_cmp, S_IRUGO, etm_show_nr_vmid_cmp, NULL);

static ssize_t etm_show_nr_seq_state(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_seq_state;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_seq_state, S_IRUGO, etm_show_nr_seq_state, NULL);

static ssize_t etm_show_nr_resource(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_resource;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_resource, S_IRUGO, etm_show_nr_resource, NULL);

static ssize_t etm_show_nr_ss_cmp(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->nr_ss_cmp;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR(nr_ss_cmp, S_IRUGO, etm_show_nr_ss_cmp, NULL);

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
	unsigned long val, mode;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->mode = val & ETM_MODE_ALL;

	if (drvdata->mode & ETM_MODE_EXCLUDE)
		etm_set_mode_exclude(drvdata, true);
	else
		etm_set_mode_exclude(drvdata, false);

	if (drvdata->instrp0_support == true) {
		drvdata->cfg  &= ~(BIT(1) | BIT(2));
		if (drvdata->mode & ETM_MODE_LOAD)
			drvdata->cfg  |= BIT(1);
		if (drvdata->mode & ETM_MODE_STORE)
			drvdata->cfg  |= BIT(2);
		if (drvdata->mode & ETM_MODE_LOAD_STORE)
			drvdata->cfg  |= BIT(1) | BIT(2);
	}

	if ((drvdata->mode & ETM_MODE_BB) && (drvdata->trc_bb_support == true))
		drvdata->cfg |= BIT(3);
	else
		drvdata->cfg &= ~BIT(3);

	if ((drvdata->mode & ETM_MODE_CYCACC) &&
	    (drvdata->trc_cyccnt_support == true))
		drvdata->cfg |= BIT(4);
	else
		drvdata->cfg &= ~BIT(4);

	if ((drvdata->mode & ETM_MODE_CTXID) && (drvdata->ctxid_size))
		drvdata->cfg |= BIT(6);
	else
		drvdata->cfg &= ~BIT(6);

	if ((drvdata->mode & ETM_MODE_VMID) && (drvdata->vmid_size))
		drvdata->cfg |= BIT(7);
	else
		drvdata->cfg &= ~BIT(7);

	mode = ETM_MODE_COND(drvdata->mode);
	if (drvdata->trc_cond_support == true) {
		drvdata->cfg &= ~(BIT(8) | BIT(9) | BIT(10));
		drvdata->cfg |= mode << 8;
	}

	if ((drvdata->mode & ETM_MODE_TIMESTAMP) && (drvdata->ts_size))
		drvdata->cfg |= BIT(11);
	else
		drvdata->cfg &= ~BIT(11);

	if ((drvdata->mode & ETM_MODE_RETURNSTACK) &&
	    (drvdata->retstack_support == true))
		drvdata->cfg |= BIT(12);
	else
		drvdata->cfg &= ~BIT(12);

	mode = ETM_MODE_QELEM(drvdata->mode);
	drvdata->cfg &= ~(BIT(13) | BIT(14));
	if ((mode & BIT(0)) && (drvdata->q_support & BIT(0)))
		drvdata->cfg |= BIT(13);
	if ((mode & BIT(1)) && (drvdata->q_support & BIT(1)))
		drvdata->cfg |= BIT(14);

	if (drvdata->trc_data_support == true) {
		if (drvdata->mode & ETM_MODE_DATA_TRACE_ADDR)
			drvdata->cfg |= BIT(16);
		else
			drvdata->cfg &= ~BIT(16);
		if (drvdata->mode & ETM_MODE_DATA_TRACE_VAL)
			drvdata->cfg |= BIT(17);
		else
			drvdata->cfg &= ~BIT(17);
		if ((drvdata->mode & ETM_MODE_DATA_TRACE_ADDR) ||
		    (drvdata->mode & ETM_MODE_DATA_TRACE_VAL)) {
			/* Enable ViewData */
			drvdata->vdata_ctrl = 0x1;
			/*
			 * ETMv4 spec mandates bit[0] of traceid field must be
			 * zero if data trace is enabled. Keep the traceid
			 * consistent irrespective of data tracing is enabled
			 * or not.
			 */
			drvdata->trcid = (drvdata->cpu + 2) * 2;
		}
	}

	if ((drvdata->mode & ETM_MODE_ATB_TRIGGER) &&
	    (drvdata->atbtrig_support == true))
		drvdata->event_ctrl1 |= BIT(11);
	else
		 drvdata->event_ctrl1 &= ~BIT(11);

	if ((drvdata->mode & ETM_MODE_LPOVERRIDE) &&
	    (drvdata->lp_override_support == true))
		drvdata->event_ctrl1 |= BIT(12);
	else
		drvdata->event_ctrl1 &= ~BIT(12);

	if (drvdata->mode & ETM_MODE_ISTALL_EN)
		drvdata->stall_ctrl |= BIT(8);
	else
		drvdata->stall_ctrl &= ~BIT(8);

	if ((drvdata->mode & ETM_MODE_DSTALL_EN) &&
	    (drvdata->trc_data_support == true))
		drvdata->stall_ctrl |= BIT(9);
	else
		drvdata->stall_ctrl &= ~BIT(9);

	if (drvdata->mode & ETM_MODE_INSTPRIO)
		drvdata->stall_ctrl |= BIT(10);
	else
		drvdata->stall_ctrl &= ~BIT(10);

	if ((drvdata->mode & ETM_MODE_NOOVERFLOW) &&
	    (drvdata->no_overflow_support == true))
		drvdata->stall_ctrl |= BIT(13);
	else
		drvdata->stall_ctrl &= ~BIT(13);

	if (drvdata->mode & ETM_MODE_VIEWINST_STARTSTOP)
		drvdata->vinst_ctrl |= BIT(9);
	else
		drvdata->vinst_ctrl &= ~BIT(9);

	if (drvdata->mode & ETM_MODE_TRACE_RESET)
		drvdata->vinst_ctrl |= BIT(10);
	else
		drvdata->vinst_ctrl &= ~BIT(10);

	if ((drvdata->mode & ETM_MODE_TRACE_ERR) &&
	    (drvdata->trc_error_support == true))
		drvdata->vinst_ctrl |= BIT(11);
	else
		drvdata->vinst_ctrl &= ~BIT(11);

	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, etm_show_mode, etm_store_mode);

static ssize_t etm_show_pe(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->pe_sel;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_pe(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	if (val > drvdata->nr_pe) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	drvdata->pe_sel = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(pe, S_IRUGO | S_IWUSR, etm_show_pe, etm_store_pe);

static ssize_t etm_show_event(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->event_ctrl0;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_event(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	switch (drvdata->nr_event) {
	case 0x0:
		drvdata->event_ctrl0 = val & 0xFF;
		break;
	case 0x1:
		drvdata->event_ctrl0 = val & 0xFFFF;
		break;
	case 0x2:
		drvdata->event_ctrl0 = val & 0xFFFFFF;
		break;
	case 0x3:
		drvdata->event_ctrl0 = val;
		break;
	default:
		break;
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(event, S_IRUGO | S_IWUSR, etm_show_event, etm_store_event);

static ssize_t etm_show_event_instren(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = BMVAL(drvdata->event_ctrl1, 0, 3);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_event_instren(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->event_ctrl1 &= ~(BIT(0) | BIT(1) | BIT(2) | BIT(3));
	switch (drvdata->nr_event) {
	case 0x0:
		drvdata->event_ctrl1 |= val & BIT(1);
		break;
	case 0x1:
		drvdata->event_ctrl1 |= val & (BIT(0) | BIT(1));
		break;
	case 0x2:
		drvdata->event_ctrl1 |= val & (BIT(0) | BIT(1) | BIT(2));
		break;
	case 0x3:
		drvdata->event_ctrl1 |= val & 0xF;
		break;
	default:
		break;
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(event_instren, S_IRUGO | S_IWUSR, etm_show_event_instren,
		   etm_store_event_instren);

static ssize_t etm_show_event_dataen(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = BVAL(drvdata->event_ctrl1, 4);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_event_dataen(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (drvdata->trc_data_support != true)
		return -EINVAL;

	if (val)
		drvdata->event_ctrl1 |= BIT(4);
	else
		drvdata->event_ctrl1 &= ~BIT(4);
	return size;
}
static DEVICE_ATTR(event_dataen, S_IRUGO | S_IWUSR, etm_show_event_dataen,
		   etm_store_event_dataen);

static ssize_t etm_show_event_ts(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->ts_ctrl;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_event_ts(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!drvdata->ts_size)
		return -EINVAL;

	drvdata->ts_ctrl = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(event_ts, S_IRUGO | S_IWUSR, etm_show_event_ts,
		   etm_store_event_ts);

static ssize_t etm_show_syncfreq(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->syncfreq;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_syncfreq(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (drvdata->syncpr_fixed == true)
		return -EINVAL;

	drvdata->syncfreq = val & ETM_SYNC_MASK;
	return size;
}
static DEVICE_ATTR(syncfreq, S_IRUGO | S_IWUSR, etm_show_syncfreq,
		   etm_store_syncfreq);

static ssize_t etm_show_cyc_threshold(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->cyccnt_ctrl;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cyc_threshold(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val < drvdata->cyccnt_min)
		return -EINVAL;

	drvdata->cyccnt_ctrl = val & ETM_CYC_THRESHOLD_MASK;
	return size;
}
static DEVICE_ATTR(cyc_threshold, S_IRUGO | S_IWUSR, etm_show_cyc_threshold,
		   etm_store_cyc_threshold);

static ssize_t etm_show_bb_ctrl(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->bb_ctrl;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_bb_ctrl(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (drvdata->trc_bb_support == false)
		return -EINVAL;
	if (!drvdata->nr_addr_cmp)
		return -EINVAL;
	/*
	 * Bit[7: 0] selects which address range comparator is used for
	 * branch broadcast control.
	 */
	if (BMVAL(val, 0, 7) > drvdata->nr_addr_cmp)
		return -EINVAL;

	drvdata->bb_ctrl = val;
	return size;
}
static DEVICE_ATTR(bb_ctrl, S_IRUGO | S_IWUSR, etm_show_bb_ctrl,
		   etm_store_bb_ctrl);

static ssize_t etm_show_event_vinst(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->vinst_ctrl & ETM_EVENT_MASK;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_event_vinst(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	val &= ETM_EVENT_MASK;
	drvdata->vinst_ctrl &= ~ETM_EVENT_MASK;
	drvdata->vinst_ctrl |= val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(event_vinst, S_IRUGO | S_IWUSR, etm_show_event_vinst,
		   etm_store_event_vinst);

static ssize_t etm_show_s_exlevel_vinst(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = BMVAL(drvdata->vinst_ctrl, 16, 19);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_s_exlevel_vinst(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->vinst_ctrl &= ~(BIT(16) | BIT(17) | BIT(19));
	/* enable instruction tracing for corresponding exception level */
	val &= drvdata->s_ex_level;
	drvdata->vinst_ctrl |= (val << 16);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(s_exlevel_vinst, S_IRUGO | S_IWUSR,
		   etm_show_s_exlevel_vinst,
		   etm_store_s_exlevel_vinst);

static ssize_t etm_show_ns_exlevel_vinst(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = BMVAL(drvdata->vinst_ctrl, 20, 23);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_ns_exlevel_vinst(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->vinst_ctrl &= ~(BIT(20) | BIT(21) | BIT(22));
	/* enable instruction tracing for corresponding exception level */
	val &= drvdata->ns_ex_level;
	drvdata->vinst_ctrl |= (val << 20);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(ns_exlevel_vinst, S_IRUGO | S_IWUSR,
		   etm_show_ns_exlevel_vinst,
		   etm_store_ns_exlevel_vinst);

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
	if (val >= drvdata->nr_addr_cmp * 2)
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
	uint8_t idx = drvdata->addr_idx;

	spin_lock(&drvdata->spinlock);
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_SINGLE)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	val = (unsigned long)drvdata->addr_val[idx];
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

	drvdata->addr_val[idx] = (uint64_t)val;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_SINGLE;
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

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (!((drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_NONE) ||
	      (drvdata->addr_type[idx] == ETM_ADDR_TYPE_RANGE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_RANGE))) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val1 = (unsigned long)drvdata->addr_val[idx];
	val2 = (unsigned long)drvdata->addr_val[idx + 1];
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

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (!((drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_NONE) ||
	      (drvdata->addr_type[idx] == ETM_ADDR_TYPE_RANGE &&
	       drvdata->addr_type[idx + 1] == ETM_ADDR_TYPE_RANGE))) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	drvdata->addr_val[idx] = (uint64_t)val1;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_RANGE;
	drvdata->addr_val[idx + 1] = (uint64_t)val2;
	drvdata->addr_type[idx + 1] = ETM_ADDR_TYPE_RANGE;
	/*
	 * Program include or exclude control bits for vinst or vdata
	 * whenever we change addr comparators to ETM_ADDR_TYPE_RANGE
	 */
	if (drvdata->mode & ETM_MODE_EXCLUDE)
		etm_set_mode_exclude(drvdata, true);
	else
		etm_set_mode_exclude(drvdata, false);

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

	val = (unsigned long)drvdata->addr_val[idx];
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
	if (!drvdata->nr_addr_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      drvdata->addr_type[idx] == ETM_ADDR_TYPE_START)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	drvdata->addr_val[idx] = (uint64_t)val;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_START;
	drvdata->vinst_startstop_ctrl |= BIT(idx);
	drvdata->vinst_ctrl |= BIT(9);
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

	val = (unsigned long)drvdata->addr_val[idx];
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
	if (!drvdata->nr_addr_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	if (!(drvdata->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	       drvdata->addr_type[idx] == ETM_ADDR_TYPE_STOP)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	drvdata->addr_val[idx] = (uint64_t)val;
	drvdata->addr_type[idx] = ETM_ADDR_TYPE_STOP;
	drvdata->vinst_startstop_ctrl |= BIT(idx + 16);
	drvdata->vinst_ctrl |= BIT(9);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_stop, S_IRUGO | S_IWUSR, etm_show_addr_stop,
		   etm_store_addr_stop);

static ssize_t etm_show_addr_instdatatype(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t len;
	uint8_t val, idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	val = BMVAL(drvdata->addr_acctype[idx], 0, 1);
	len = scnprintf(buf, PAGE_SIZE, "%s\n",
			val == ETM_INSTR_ADDR ? "instr" :
			(val == ETM_DATA_LOAD_ADDR ? "data_load" :
			(val == ETM_DATA_STORE_ADDR ? "data_store" :
			"data_load_store")));
	spin_unlock(&drvdata->spinlock);
	return len;
}

static ssize_t etm_store_addr_instdatatype(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	uint8_t idx;
	char str[20] = "";

	if (strlen(buf) >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!strcmp(str, "instr")) {
		drvdata->addr_acctype[idx] &= ~(BIT(0) | BIT(1));
	} else if (drvdata->data_addr_cmp_support == true) {
		/*
		 * Non-zero value implies data address comparison, check if
		 * supported
		 */
		if (!strcmp(str, "data_load")) {
			drvdata->addr_acctype[idx] |= BIT(0);
			drvdata->addr_acctype[idx] &= ~BIT(1);
		} else if (!strcmp(str, "data_store")) {
			drvdata->addr_acctype[idx] &= ~BIT(0);
			drvdata->addr_acctype[idx] |= BIT(1);
		} else if (!strcmp(str, "data_load_store")) {
			drvdata->addr_acctype[idx] |= (BIT(0) | BIT(1));
		}
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_instdatatype, S_IRUGO | S_IWUSR,
		   etm_show_addr_instdatatype,
		   etm_store_addr_instdatatype);

static ssize_t etm_show_addr_ctxtype(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t len;
	uint8_t idx, val;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	val = BMVAL(drvdata->addr_acctype[idx], 2, 3);
	len = scnprintf(buf, PAGE_SIZE, "%s\n", val == ETM_CTX_NONE ? "none" :
			(val == ETM_CTX_CTXID ? "ctxid" :
			(val == ETM_CTX_VMID ? "vmid" : "all")));
	spin_unlock(&drvdata->spinlock);
	return len;
}

static ssize_t etm_store_addr_ctxtype(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	uint8_t idx;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (!strcmp(str, "none"))
		drvdata->addr_acctype[idx] &= ~(BIT(2) | BIT(3));
	else if (!strcmp(str, "ctxid")) {
		if (drvdata->nr_ctxid_cmp) {
			drvdata->addr_acctype[idx] |= BIT(2);
			drvdata->addr_acctype[idx] &= ~BIT(3);
		}
	} else if (!strcmp(str, "vmid")) {
		if (drvdata->nr_vmid_cmp) {
			drvdata->addr_acctype[idx] &= ~BIT(2);
			drvdata->addr_acctype[idx] |= BIT(3);
		}
	} else if (!strcmp(str, "all")) {
		if (drvdata->nr_ctxid_cmp)
			drvdata->addr_acctype[idx] |= BIT(2);
		if (drvdata->nr_vmid_cmp)
			drvdata->addr_acctype[idx] |= BIT(3);
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_ctxtype, S_IRUGO | S_IWUSR, etm_show_addr_ctxtype,
		   etm_store_addr_ctxtype);

static ssize_t etm_show_addr_context(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	val = BMVAL(drvdata->addr_acctype[idx], 4, 6);
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_addr_context(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if ((drvdata->nr_ctxid_cmp <= 1) && (drvdata->nr_vmid_cmp <= 1))
		return -EINVAL;
	if (val >=  (drvdata->nr_ctxid_cmp >= drvdata->nr_vmid_cmp ?
		     drvdata->nr_ctxid_cmp : drvdata->nr_vmid_cmp))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	drvdata->addr_acctype[idx] &= ~(BIT(4) | BIT(5) | BIT(6));
	drvdata->addr_acctype[idx] |= (val << 4);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_context, S_IRUGO | S_IWUSR, etm_show_addr_context,
		   etm_store_addr_context);

static ssize_t etm_show_addr_dvalmatch(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t len;
	uint8_t idx, val;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	val = BMVAL(drvdata->addr_acctype[idx], 16, 17);
	len = scnprintf(buf, PAGE_SIZE, "%s\n", val == ETM_DATA_CMP_NONE ?
			"none" : (val == ETM_DATA_CMP_MATCH ? "match" :
			"nomatch"));
	spin_unlock(&drvdata->spinlock);
	return len;
}

static ssize_t etm_store_addr_dvalmatch(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	uint8_t idx;

	if (strlen(buf) >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	/* Supported if corresponding data value comparator is supported */
	if ((idx >> 1) >= drvdata->nr_data_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (!strcmp(str, "none")) {
		drvdata->addr_acctype[idx] &= ~(BIT(16) | BIT(17));
	} else if (!strcmp(str, "match")) {
		drvdata->addr_acctype[idx] |= BIT(16);
		drvdata->addr_acctype[idx] &= ~BIT(17);
	} else if (!strcmp(str, "nomatch")) {
		drvdata->addr_acctype[idx] |= BIT(16) | BIT(17);
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_dvalmatch, S_IRUGO | S_IWUSR, etm_show_addr_dvalmatch,
		   etm_store_addr_dvalmatch);

static ssize_t etm_show_addr_dvalsize(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t len;
	uint8_t idx, val;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	val = BMVAL(drvdata->addr_acctype[idx], 18, 19);
	len = scnprintf(buf, PAGE_SIZE, "%s\n", val == ETM_DATA_TYPE_BYTE ?
			"byte" : (val == ETM_DATA_TYPE_HWORD ? "halfword" :
			(val == ETM_DATA_TYPE_WORD ? "word" : "double")));
	spin_unlock(&drvdata->spinlock);
	return len;
}

static ssize_t etm_store_addr_dvalsize(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	uint8_t idx;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;
	/* allowed if data value tracing is supported */
	if (!drvdata->data_val_size)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	/* allowed if corresponding data value comparator is present */
	if ((idx >> 1) >= drvdata->nr_data_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (!strcmp(str, "byte")) {
		drvdata->addr_acctype[idx] &= ~(BIT(18) | BIT(19));
	} else if (!strcmp(str, "halfword")) {
		drvdata->addr_acctype[idx] |= BIT(18);
		drvdata->addr_acctype[idx] &= ~BIT(19);
	} else if (!strcmp(str, "word")) {
		drvdata->addr_acctype[idx] &= ~BIT(18);
		drvdata->addr_acctype[idx] |= BIT(19);
	} else if (!strcmp(str, "double")) {
		/* check if 64 bit values are supported */
		if (drvdata->data_val_size != 0x8)
			drvdata->addr_acctype[idx] |= BIT(18) | BIT(19);
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_dvalsize, S_IRUGO | S_IWUSR, etm_show_addr_dvalsize,
		   etm_store_addr_dvalsize);

static ssize_t etm_show_addr_dvalrange(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	val = BVAL(drvdata->addr_acctype[idx], 20);
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_addr_dvalrange(struct device *dev,
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
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	/* Supported if corresponding data value comparator is supported */
	if ((idx >> 1) >= drvdata->nr_data_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (idx >= ETM_MAX_SINGLE_ADDR_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	if (val)
		drvdata->addr_acctype[idx] |= (BIT(20));
	else
		drvdata->addr_acctype[idx] &= ~(BIT(20));
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(addr_dvalrange, S_IRUGO | S_IWUSR, etm_show_addr_dvalrange,
		   etm_store_addr_dvalrange);

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

	if (idx >= ETM_MAX_DATA_VAL_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	val = (unsigned long)drvdata->data_val[idx];
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

	drvdata->data_val[data_idx] = (uint64_t)val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(data_val, S_IRUGO | S_IWUSR, etm_show_data_val,
		   etm_store_data_val);

static ssize_t etm_show_data_mask(struct device *dev,
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

	if (idx >= ETM_MAX_DATA_VAL_CMP) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	val = (unsigned long)drvdata->data_mask[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
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

	drvdata->data_mask[data_idx] = (uint64_t)mask;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(data_mask, S_IRUGO | S_IWUSR, etm_show_data_mask,
		   etm_store_data_mask);

static ssize_t etm_show_seq_idx(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->seq_idx;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_idx(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val >= drvdata->nr_seq_state - 1)
		return -EINVAL;

	/*
	 * Use spinlock to ensure index doesn't change while it gets
	 * dereferenced multiple times within a spinlock block elsewhere.
	 */
	spin_lock(&drvdata->spinlock);
	drvdata->seq_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(seq_idx, S_IRUGO | S_IWUSR, etm_show_seq_idx,
		   etm_store_seq_idx);

static ssize_t etm_show_seq_event(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t	idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->seq_idx;
	val = drvdata->seq_ctrl[idx];
	spin_unlock(&drvdata->spinlock);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_event(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t	idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->seq_idx;
	drvdata->seq_ctrl[idx] = val & 0xFF;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(seq_event, S_IRUGO | S_IWUSR, etm_show_seq_event,
		   etm_store_seq_event);

static ssize_t etm_show_seq_reset_event(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	val = drvdata->seq_rst;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_reset_event(struct device *dev,
					 struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!(drvdata->nr_seq_state))
		return -EINVAL;

	drvdata->seq_rst = val & ETM_EVENT_MASK;
	return size;
}
static DEVICE_ATTR(seq_reset_event, S_IRUGO | S_IWUSR,
		   etm_show_seq_reset_event,
		   etm_store_seq_reset_event);

static ssize_t etm_show_seq_state(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	val = drvdata->seq_state;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_seq_state(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val >= drvdata->nr_seq_state)
		return -EINVAL;

	drvdata->seq_state = val;
	return size;
}
static DEVICE_ATTR(seq_state, S_IRUGO | S_IWUSR,
		   etm_show_seq_state,
		   etm_store_seq_state);

static ssize_t etm_show_cntr_idx(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->cntr_idx;

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
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->cntr_idx;
	val = drvdata->cntr_rld_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_rld_val(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val > ETM_CNTR_MAX_VAL)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->cntr_idx;
	drvdata->cntr_rld_val[idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_rld_val, S_IRUGO | S_IWUSR, etm_show_cntr_rld_val,
		   etm_store_cntr_rld_val);


static ssize_t etm_show_cntr_val(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t	idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->cntr_idx;
	val = drvdata->cntr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_val(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val > ETM_CNTR_MAX_VAL)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->cntr_idx;
	drvdata->cntr_val[idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_val, S_IRUGO | S_IWUSR, etm_show_cntr_val,
		   etm_store_cntr_val);

static ssize_t etm_show_cntr_ctrl(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->cntr_idx;
	val = drvdata->cntr_ctrl[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_cntr_ctrl(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->cntr_idx;
	drvdata->cntr_ctrl[idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(cntr_ctrl, S_IRUGO | S_IWUSR, etm_show_cntr_ctrl,
		   etm_store_cntr_ctrl);

static ssize_t etm_show_resource_idx(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->resource_idx;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_resource_idx(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	/* Resource selector pair 0 is always implemented and reserved */
	if ((val == 0) || (val >= drvdata->nr_resource))
		return -EINVAL;

	/*
	 * Use spinlock to ensure index doesn't change while it gets
	 * dereferenced multiple times within a spinlock block elsewhere.
	 */
	spin_lock(&drvdata->spinlock);
	drvdata->resource_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(resource_idx, S_IRUGO | S_IWUSR, etm_show_resource_idx,
		   etm_store_resource_idx);

static ssize_t etm_show_resource_ctrl(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->resource_idx;
	val = drvdata->resource_ctrl[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_resource_ctrl(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->resource_idx;
	/* For odd idx pair inversal bit is RES0 */
	if (idx % 2 != 0)
		val &= ~BIT(21);
	drvdata->resource_ctrl[idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(resource_ctrl, S_IRUGO | S_IWUSR, etm_show_resource_ctrl,
		   etm_store_resource_ctrl);

static ssize_t etm_show_ctxid_idx(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
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
				  struct device_attribute *attr,
				  char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->ctxid_idx;
	val = (unsigned long)drvdata->ctxid_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_ctxid_val(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	uint8_t idx;

	/*
	 * only implemented when ctxid tracing is enabled, i.e. at least one
	 * ctxid comparator is implemented and ctxid is greater than 0 bits
	 * in length
	 */
	if (!drvdata->ctxid_size || !drvdata->nr_ctxid_cmp)
		return -EINVAL;
	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = drvdata->ctxid_idx;
	drvdata->ctxid_val[idx] = (uint64_t)val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(ctxid_val, S_IRUGO | S_IWUSR, etm_show_ctxid_val,
		   etm_store_ctxid_val);

static ssize_t etm_show_ctxid_masks(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	spin_lock(&drvdata->spinlock);
	val1 = drvdata->ctxid_mask0;
	val2 = drvdata->ctxid_mask1;
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx %#lx\n", val1, val2);
}

static ssize_t etm_store_ctxid_masks(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2, mask;
	uint8_t i, j, maskbyte;

	/*
	 * only implemented when ctxid tracing is enabled, i.e. at least one
	 * ctxid comparator is implemented and ctxid is greater than 0 bits
	 * in length
	 */
	if (!drvdata->ctxid_size || !drvdata->nr_ctxid_cmp)
		return -EINVAL;
	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	/*
	 * each byte[0..3] controls mask value applied to ctxid
	 * comparator[0..3]
	 */
	switch (drvdata->nr_ctxid_cmp) {
	case 0x1:
		drvdata->ctxid_mask0 = val1 & 0xFF;
		break;
	case 0x2:
		drvdata->ctxid_mask0 = val1 & 0xFFFF;
		break;
	case 0x3:
		drvdata->ctxid_mask0 = val1 & 0xFFFFFF;
		break;
	case 0x4:
		drvdata->ctxid_mask0 = val1;
		break;
	case 0x5:
		drvdata->ctxid_mask0 = val1;
		drvdata->ctxid_mask1 = val2 & 0xFF;
		break;
	case 0x6:
		drvdata->ctxid_mask0 = val1;
		drvdata->ctxid_mask1 = val2 & 0xFFFF;
		break;
	case 0x7:
		drvdata->ctxid_mask0 = val1;
		drvdata->ctxid_mask1 = val2 & 0xFFFFFF;
		break;
	case 0x8:
		drvdata->ctxid_mask0 = val1;
		drvdata->ctxid_mask1 = val2;
		break;
	default:
		break;
	}
	/*
	 * If software sets a mask bit to 1, it must program relevant byte
	 * of ctxid comparator value 0x0, otherwise behavior is unpredictable.
	 * For example, if bit[3] of ctxid_mask0 is 1, we must clear bits[31:24]
	 * of ctxid comparator0 value (corresponding to byte 0) register.
	 */
	mask = drvdata->ctxid_mask0;
	for (i = 0; i < drvdata->nr_ctxid_cmp; i++) {
		/* mask value of corresponding ctxid comparator */
		maskbyte = mask & ETM_EVENT_MASK;
		/*
		 * each bit corresponds to a byte of respective ctxid comparator
		 * value register
		 */
		for (j = 0; j < 8; j++) {
			if (maskbyte & 1)
				drvdata->ctxid_val[i] &= ~(0xFF << (j * 8));
			maskbyte >>= 1;
		}
		/* Select the next ctxid comparator mask value */
		if (i == 3)
			/* ctxid comparators[4-7] */
			mask = drvdata->ctxid_mask1;
		else
			mask >>= 0x8;
	}

	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(ctxid_masks, S_IRUGO | S_IWUSR, etm_show_ctxid_masks,
		   etm_store_ctxid_masks);

static ssize_t etm_show_vmid_idx(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->vmid_idx;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_vmid_idx(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (val >= drvdata->nr_vmid_cmp)
		return -EINVAL;

	/*
	 * Use spinlock to ensure index doesn't change while it gets
	 * dereferenced multiple times within a spinlock block elsewhere.
	 */
	spin_lock(&drvdata->spinlock);
	drvdata->vmid_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(vmid_idx, S_IRUGO | S_IWUSR, etm_show_vmid_idx,
		   etm_store_vmid_idx);

static ssize_t etm_show_vmid_val(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	val = (unsigned long)drvdata->vmid_val[drvdata->vmid_idx];
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t etm_store_vmid_val(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	/*
	 * only implemented when vmid tracing is enabled, i.e. at least one
	 * vmid comparator is implemented and at least 8 bit vmid size
	 */
	if (!drvdata->vmid_size || !drvdata->nr_vmid_cmp)
		return -EINVAL;
	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	drvdata->vmid_val[drvdata->vmid_idx] = (uint64_t)val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(vmid_val, S_IRUGO | S_IWUSR, etm_show_vmid_val,
		   etm_store_vmid_val);

static ssize_t etm_show_vmid_masks(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	spin_lock(&drvdata->spinlock);
	val1 = drvdata->vmid_mask0;
	val2 = drvdata->vmid_mask1;
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx %#lx\n", val1, val2);
}

static ssize_t etm_store_vmid_masks(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2, mask;
	uint8_t i, j, maskbyte;

	/*
	 * only implemented when vmid tracing is enabled, i.e. at least one
	 * vmid comparator is implemented and at least 8 bit vmid size
	 */
	if (!drvdata->vmid_size || !drvdata->nr_vmid_cmp)
		return -EINVAL;
	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);

	/*
	 * each byte[0..3] controls mask value applied to vmid
	 * comparator[0..3]
	 */
	switch (drvdata->nr_vmid_cmp) {
	case 0x1:
		drvdata->vmid_mask0 = val1 & 0xFF;
		break;
	case 0x2:
		drvdata->vmid_mask0 = val1 & 0xFFFF;
		break;
	case 0x3:
		drvdata->vmid_mask0 = val1 & 0xFFFFFF;
		break;
	case 0x4:
		drvdata->vmid_mask0 = val1;
		break;
	case 0x5:
		drvdata->vmid_mask0 = val1;
		drvdata->vmid_mask1 = val2 & 0xFF;
		break;
	case 0x6:
		drvdata->vmid_mask0 = val1;
		drvdata->vmid_mask1 = val2 & 0xFFFF;
		break;
	case 0x7:
		drvdata->vmid_mask0 = val1;
		drvdata->vmid_mask1 = val2 & 0xFFFFFF;
		break;
	case 0x8:
		drvdata->vmid_mask0 = val1;
		drvdata->vmid_mask1 = val2;
		break;
	default:
		break;
	}
	/*
	 * If software sets a mask bit to 1, it must program relevant byte
	 * of vmid comparator value 0x0, otherwise behavior is unpredictable.
	 * For example, if bit[3] of vmid_mask0 is 1, we must clear bits[31:24]
	 * of vmid comparator0 value (corresponding to byte 0) register.
	 */
	mask = drvdata->vmid_mask0;
	for (i = 0; i < drvdata->nr_vmid_cmp; i++) {
		/* mask value of corresponding vmid comparator */
		maskbyte = mask & ETM_EVENT_MASK;
		/*
		 * each bit corresponds to a byte of respective vmid comparator
		 * value register
		 */
		for (j = 0; j < 8; j++) {
			if (maskbyte & 1)
				drvdata->vmid_val[i] &= ~(0xFF << (j * 8));
			maskbyte >>= 1;
		}
		/* Select the next vmid comparator mask value */
		if (i == 3)
			/* vmid comparators[4-7] */
			mask = drvdata->vmid_mask1;
		else
			mask >>= 0x8;
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR(vmid_masks, S_IRUGO | S_IWUSR, etm_show_vmid_masks,
		   etm_store_vmid_masks);

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
	&dev_attr_nr_pe_cmp.attr,
	&dev_attr_nr_addr_cmp.attr,
	&dev_attr_nr_data_cmp.attr,
	&dev_attr_nr_cntr.attr,
	&dev_attr_nr_ext_inp.attr,
	&dev_attr_nr_ext_out.attr,
	&dev_attr_nr_ctxid_cmp.attr,
	&dev_attr_nr_vmid_cmp.attr,
	&dev_attr_nr_seq_state.attr,
	&dev_attr_nr_resource.attr,
	&dev_attr_nr_ss_cmp.attr,
	&dev_attr_reset.attr,
	&dev_attr_mode.attr,
	&dev_attr_pe.attr,
	&dev_attr_event.attr,
	&dev_attr_event_instren.attr,
	&dev_attr_event_dataen.attr,
	&dev_attr_event_ts.attr,
	&dev_attr_syncfreq.attr,
	&dev_attr_cyc_threshold.attr,
	&dev_attr_bb_ctrl.attr,
	&dev_attr_event_vinst.attr,
	&dev_attr_s_exlevel_vinst.attr,
	&dev_attr_ns_exlevel_vinst.attr,
	&dev_attr_addr_idx.attr,
	&dev_attr_addr_instdatatype.attr,
	&dev_attr_addr_single.attr,
	&dev_attr_addr_range.attr,
	&dev_attr_addr_start.attr,
	&dev_attr_addr_stop.attr,
	&dev_attr_addr_ctxtype.attr,
	&dev_attr_addr_context.attr,
	&dev_attr_addr_dvalmatch.attr,
	&dev_attr_addr_dvalsize.attr,
	&dev_attr_addr_dvalrange.attr,
	&dev_attr_data_val.attr,
	&dev_attr_data_mask.attr,
	&dev_attr_seq_idx.attr,
	&dev_attr_seq_state.attr,
	&dev_attr_seq_event.attr,
	&dev_attr_seq_reset_event.attr,
	&dev_attr_cntr_idx.attr,
	&dev_attr_cntr_rld_val.attr,
	&dev_attr_cntr_val.attr,
	&dev_attr_cntr_ctrl.attr,
	&dev_attr_resource_idx.attr,
	&dev_attr_resource_ctrl.attr,
	&dev_attr_ctxid_idx.attr,
	&dev_attr_ctxid_val.attr,
	&dev_attr_ctxid_masks.attr,
	&dev_attr_vmid_idx.attr,
	&dev_attr_vmid_val.attr,
	&dev_attr_vmid_masks.attr,
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

static void etm_init_arch_data(void *info)
{
	uint32_t etmidr0;
	uint32_t etmidr1;
	uint32_t etmidr2;
	uint32_t etmidr3;
	uint32_t etmidr4;
	uint32_t etmidr5;
	struct etm_drvdata *drvdata = info;

	ETM_UNLOCK(drvdata);

	/* check the state of the fuse */
	if (!coresight_authstatus_enabled(drvdata->base))
		goto out;

	/* find all capabilities */
	/* tracing capabilities of trace unit */
	etmidr0 = etm_readl(drvdata, TRCIDR0);
	if (BVAL(etmidr0, 1) && BVAL(etmidr0, 2))
		drvdata->instrp0_support = true;
	else
		drvdata->instrp0_support = false;
	if (BVAL(etmidr0, 3) && BVAL(etmidr0, 4))
		drvdata->trc_data_support = true;
	else
		drvdata->trc_data_support = false;
	if (BVAL(etmidr0, 5))
		drvdata->trc_bb_support = true;
	else
		drvdata->trc_bb_support = false;
	if (BVAL(etmidr0, 6))
		drvdata->trc_cond_support = true;
	else
		drvdata->trc_cond_support = false;
	if (BVAL(etmidr0, 7))
		drvdata->trc_cyccnt_support = true;
	else
		drvdata->trc_cyccnt_support = false;
	if (BVAL(etmidr0, 9))
		drvdata->retstack_support = true;
	else
		drvdata->retstack_support = false;
	drvdata->nr_event = BMVAL(etmidr0, 10, 11);
	if (BMVAL(etmidr0, 12, 13))
		drvdata->cond_type_support = true;
	else
		drvdata->cond_type_support = false;
	if (BVAL(etmidr0, 14))
		drvdata->qfilt_support = true;
	else
		drvdata->qfilt_support = false;
	drvdata->q_support = BMVAL(etmidr0, 15, 16);
	if (BVAL(etmidr0, 17))
		drvdata->trc_exdata_support = true;
	else
		drvdata->trc_exdata_support = false;
	drvdata->ts_size = BMVAL(etmidr0, 24, 28);
	drvdata->commit_mode = BVAL(etmidr0, 29);

	/* base architecture of trace unit */
	etmidr1 = etm_readl(drvdata, TRCIDR1);
	drvdata->arch = BMVAL(etmidr1, 4, 11);

	/* maximum size of resources */
	etmidr2 = etm_readl(drvdata, TRCIDR2);
	drvdata->instr_addr_size = BMVAL(etmidr2, 0, 4);
	drvdata->ctxid_size = BMVAL(etmidr2, 5, 9);
	drvdata->vmid_size = BMVAL(etmidr2, 10, 14);
	drvdata->data_addr_size = BMVAL(etmidr2, 15, 19);
	drvdata->data_val_size = BMVAL(etmidr2, 20, 24);
	drvdata->cyccnt_size = BMVAL(etmidr2, 25, 28);

	etmidr3 = etm_readl(drvdata, TRCIDR3);
	drvdata->cyccnt_min = BMVAL(etmidr3, 0, 11);
	drvdata->s_ex_level = BMVAL(etmidr3, 16, 19);
	drvdata->ns_ex_level = BMVAL(etmidr3, 20, 23);
	if (BVAL(etmidr3, 24))
		drvdata->trc_error_support = true;
	else
		drvdata->trc_error_support = false;
	if (BVAL(etmidr3, 25))
		drvdata->syncpr_fixed = true;
	else
		drvdata->syncpr_fixed = false;
	if (BVAL(etmidr3, 26))
		drvdata->stallctrl_support = true;
	else
		drvdata->stallctrl_support = false;
	if (BVAL(etmidr3, 27))
		drvdata->stall_pe_support = true;
	else
		drvdata->stall_pe_support = false;
	drvdata->nr_pe = BMVAL(etmidr3, 28, 30);
	if (BVAL(etmidr3, 31))
		drvdata->no_overflow_support = true;
	else
		drvdata->no_overflow_support = false;

	/* number of resources trace unit supports */
	etmidr4 = etm_readl(drvdata, TRCIDR4);
	drvdata->nr_addr_cmp = BMVAL(etmidr4, 0, 3);
	if (drvdata->nr_addr_cmp > ETM_MAX_ADDR_RANGE_CMP) {
		dev_err(drvdata->dev,
			"nr_addr_cmp out of bounds %u\n", drvdata->nr_addr_cmp);
		drvdata->nr_addr_cmp = ETM_MAX_ADDR_RANGE_CMP;
	}
	drvdata->nr_data_cmp = BMVAL(etmidr4, 4, 7);
	if (drvdata->nr_data_cmp > ETM_MAX_DATA_VAL_CMP) {
		dev_err(drvdata->dev,
			"nr_data_cmp out of bounds %u\n", drvdata->nr_data_cmp);
		drvdata->nr_data_cmp = ETM_MAX_DATA_VAL_CMP;
	}

	if (BVAL(etmidr4, 8))
		drvdata->data_addr_cmp_support = true;
	else
		drvdata->data_addr_cmp_support = false;

	drvdata->nr_pe_cmp = BMVAL(etmidr4, 12, 15);
	if (drvdata->nr_pe_cmp > ETM_MAX_PE_CMP) {
		dev_err(drvdata->dev,
			"nr_pe_cmp out of bounds %u\n", drvdata->nr_pe_cmp);
		drvdata->nr_pe_cmp = ETM_MAX_PE_CMP;
	}
	drvdata->nr_resource = BMVAL(etmidr4, 16, 19);
	if (drvdata->nr_resource > ETM_MAX_RES_SEL) {
		dev_err(drvdata->dev,
			"nr_resource out of bounds %u\n", drvdata->nr_resource);
		drvdata->nr_resource = ETM_MAX_RES_SEL;
	}
	drvdata->nr_ss_cmp = BMVAL(etmidr4, 20, 23);
	if (drvdata->nr_ss_cmp > ETM_MAX_SS_CMP) {
		dev_err(drvdata->dev,
			"nr_ss_cmp out of bounds %u\n", drvdata->nr_ss_cmp);
		drvdata->nr_ss_cmp = ETM_MAX_SS_CMP;
	}
	drvdata->nr_ctxid_cmp = BMVAL(etmidr4, 24, 27);
	if (drvdata->nr_ctxid_cmp > ETM_MAX_CTXID_CMP) {
		dev_err(drvdata->dev,
			"nr_ctxid_cmp out of bounds %u\n",
			drvdata->nr_ctxid_cmp);
		drvdata->nr_ctxid_cmp = ETM_MAX_CTXID_CMP;
	}
	drvdata->nr_vmid_cmp = BMVAL(etmidr4, 28, 31);
	if (drvdata->nr_vmid_cmp > ETM_MAX_VMID_CMP) {
		dev_err(drvdata->dev,
			"nr_vmid_cmp out of bounds %u\n", drvdata->nr_vmid_cmp);
		drvdata->nr_vmid_cmp = ETM_MAX_VMID_CMP;
	}

	etmidr5 = etm_readl(drvdata, TRCIDR5);
	drvdata->nr_ext_inp = BMVAL(etmidr5, 0, 8);
	if (drvdata->nr_ext_inp > ETM_MAX_EXT_INP) {
		dev_err(drvdata->dev,
			"nr_ext_inp out of bounds %lu\n",
			(unsigned long)drvdata->nr_ext_inp);
		drvdata->nr_ext_inp = ETM_MAX_EXT_INP;
	}
	drvdata->nr_ext_inp_sel = BMVAL(etmidr5, 9, 11);
	if (drvdata->nr_ext_inp_sel > ETM_MAX_EXT_INP_SEL) {
		dev_err(drvdata->dev,
			"nr_ext_inp_sel out of bounds %u\n",
			drvdata->nr_ext_inp_sel);
		drvdata->nr_ext_inp_sel = ETM_MAX_EXT_INP_SEL;
	}

	drvdata->trcid_size = BMVAL(etmidr5, 16, 21);
	if (BVAL(etmidr5, 22))
		drvdata->atbtrig_support = true;
	else
		drvdata->atbtrig_support = false;
	if (BVAL(etmidr5, 23))
		drvdata->lp_override_support = true;
	else
		drvdata->lp_override_support = false;

	drvdata->nr_seq_state = BMVAL(etmidr5, 25, 27);
	if (drvdata->nr_seq_state > ETM_MAX_SEQ_STATES) {
		dev_err(drvdata->dev,
			"nr_seq_state out of bounds %u\n",
			drvdata->nr_seq_state);
		drvdata->nr_seq_state = ETM_MAX_SEQ_STATES;
	}
	drvdata->nr_cntr = BMVAL(etmidr5, 28, 30);
	if (drvdata->nr_cntr > ETM_MAX_CNTR) {
		dev_err(drvdata->dev,
			"nr_cntr out of bounds %u\n", drvdata->nr_cntr);
		drvdata->nr_cntr = ETM_MAX_CNTR;
	}

	if (BVAL(etmidr5, 31))
		drvdata->reduced_cntr_support = true;
	else
		drvdata->reduced_cntr_support = false;
out:
	ETM_LOCK(drvdata);
}

static void etm_init_default_data(struct etm_drvdata *drvdata)
{
	int i;

	drvdata->pe_sel = 0x0;
	drvdata->cfg = 0x0;

	/* disable all events tracing */
	drvdata->event_ctrl0 = 0x0;
	drvdata->event_ctrl1 = 0x0;

	/* disable stalling */
	drvdata->stall_ctrl = 0x0;
	/* Set NOOVERFLOW bit */
	if (drvdata->no_overflow_support && drvdata->enable_noovrflw)
		drvdata->stall_ctrl |= BIT(13);

	/* disable timestamp event */
	drvdata->ts_ctrl = 0x0;

	/* enable trace synchronization */
	if (drvdata->syncpr_fixed == false)
		drvdata->syncfreq = 0x8;

	/*
	 *  enable viewInst to trace everything with start-stop logic in
	 *  started state
	 */
	drvdata->vinst_ctrl |= BIT(0);
	/* set initial state of start-stop logic */
	if (drvdata->nr_addr_cmp)
		drvdata->vinst_ctrl |= BIT(9);

	/* no start-stop filtering for ViewInst */
	drvdata->vinst_startstop_ctrl = 0x0;

	/* Disable ViewData */
	drvdata->vdata_ctrl = 0x0;
	/* No address filtering for ViewData */
	drvdata->vdata_incex_single = 0x0;
	drvdata->vdata_incex_range = 0x0;

	/* disable seq events */
	for (i = 0; i < drvdata->nr_seq_state-1; i++)
		drvdata->seq_ctrl[i] = 0x0;
	drvdata->seq_rst = 0x0;
	drvdata->seq_state = 0x0;

	/* disable external input events */
	drvdata->ext_inp = 0x0;

	for (i = 0; i < drvdata->nr_cntr; i++) {
		drvdata->cntr_rld_val[i] = 0x0;
		drvdata->cntr_ctrl[i] = 0x0;
		drvdata->cntr_val[i] = 0x0;
	}

	for (i = 2; i < drvdata->nr_resource * 2; i++)
		drvdata->resource_ctrl[i] = 0x0;

	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		drvdata->ss_ctrl[i] = 0x0;
		drvdata->ss_pe_cmp[i] = 0x0;
	}

	if (drvdata->nr_addr_cmp >= 1) {
		drvdata->addr_val[0] = (unsigned long)_stext;
		drvdata->addr_val[1] = (unsigned long)_etext;
		drvdata->addr_type[0] = ETM_ADDR_TYPE_RANGE;
		drvdata->addr_type[1] = ETM_ADDR_TYPE_RANGE;

		/* address range filtering for ViewInst */
		drvdata->vinst_incex_ctrl = 0x1;
	}

	for (i = 0; i < drvdata->nr_data_cmp; i++) {
		drvdata->data_val[i] = 0;
		drvdata->data_mask[i] = 0;
	}

	for (i = 0; i < drvdata->nr_ctxid_cmp; i++)
		drvdata->ctxid_val[i] = 0x0;
	drvdata->ctxid_mask0 = 0x0;
	drvdata->ctxid_mask1 = 0x0;

	for (i = 0; i < drvdata->nr_vmid_cmp; i++)
		drvdata->vmid_val[i] = 0x0;
	drvdata->vmid_mask0 = 0x0;
	drvdata->vmid_mask1 = 0x0;

	drvdata->trcid = drvdata->cpu + 1;
}

static int etm_late_init(struct etm_drvdata *drvdata)
{
	void *baddr;
	struct msm_dump_entry dump_entry;
	struct coresight_desc *desc;
	struct device *dev = drvdata->dev;
	int ret;

	if (etm_arch_supported(drvdata->arch) == false)
		return -EINVAL;

	etm_init_default_data(drvdata);

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

	dev_info(dev, "ETMv4 initialized\n");

	if (boot_reset)
		etm_reset_data(drvdata);

	if (boot_enable) {
		coresight_enable(drvdata->csdev);
		drvdata->boot_enable = true;
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

		if (etmdrvdata[cpu]->enable)
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
		break;

	case CPU_UP_CANCELED:
		if (clk_disable[cpu]) {
			clk_disable_unprepare(etmdrvdata[cpu]->clk);
			clk_disable[cpu] = false;
		}
		break;
	}
out:
	return NOTIFY_OK;
err1:
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

static int etm_cpu_dying_callback(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	if (!etmdrvdata[cpu])
		goto out;

	switch (action & (~CPU_TASKS_FROZEN)) {
	case CPU_DYING:
		spin_lock(&etmdrvdata[cpu]->spinlock);
		if (etmdrvdata[cpu]->enable)
			__etm_disable(etmdrvdata[cpu]);
		spin_unlock(&etmdrvdata[cpu]->spinlock);
		break;
	}
out:
	return NOTIFY_OK;
}

static struct notifier_block etm_cpu_dying_notifier = {
	.notifier_call = etm_cpu_dying_callback,
	.priority = 1,
};

static int etm_parse_cgc_data(struct platform_device *pdev,
			      struct etm_drvdata *drvdata)
{
	struct device_node *cgc_node = NULL;
	struct etm_cgc_data *cgc_data = NULL;
	int cluster, val, regs[2], ret, start, len;

	cgc_node = of_parse_phandle(pdev->dev.of_node,
				    "qcom,cpuss-debug-cgc", 0);
	if (!cgc_node)
		return 0;

	ret = of_property_read_u32(cgc_node, "cluster", &cluster);
	if (ret || cluster >= MAX_CLUSTER_NUM)
		return -EINVAL;

	ret = of_property_read_u32_array(cgc_node, "reg",
					 (u32 *)&regs, 2);
	if (ret)
		return ret;

	start = regs[0];
	len = regs[1];

	if (!etm_cgc_data[cluster]) {
		cgc_data = devm_kzalloc(&pdev->dev, sizeof(*cgc_data),
					GFP_KERNEL);
		if (!cgc_data)
			return -ENOMEM;

		cgc_data->phy_addr = (uint32_t)start;
		cgc_data->len = (uint32_t)len;
		cgc_data->base = devm_ioremap(&pdev->dev, start, len);
		if (!cgc_data->base)
			return -ENOMEM;

		val = __raw_readl(cgc_data->base);
		/* disable ATB and NTS clock gating */
		val |= (ATB_CGC_OVERRIDE | NTS_CGC_OVERRIDE);
		__raw_writel(val, cgc_data->base);
		etm_cgc_data[cluster] = cgc_data;
	} else {
		if (etm_cgc_data[cluster]->phy_addr != start
		    || etm_cgc_data[cluster]->len != len) {
			dev_err(&pdev->dev, "duplicated cluster %d setting\n",
				cluster);
			etm_cgc_data[cluster]->base = 0x0;
			return -EINVAL;
		}
	}
	cpumask_set_cpu(drvdata->cpu, &etm_cgc_data[cluster]->control_mask);
	drvdata->cgc_data = etm_cgc_data[cluster];

	return 0;
}

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

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk)) {
		ret = PTR_ERR(drvdata->clk);
		goto err0;
	}

	drvdata->enable_noovrflw = of_property_read_bool(pdev->dev.of_node,
						"qcom,noovrflw-enable");

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

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		goto err0;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		goto err0;

	if (count++ == 0) {
		register_hotcpu_notifier(&etm_cpu_notifier);
		register_hotcpu_notifier(&etm_cpu_dying_notifier);
	}

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

	/* parse clock gating control DT and disable clock gating */
	etm_parse_cgc_data(pdev, drvdata);

	return 0;
err1:
	if (--count == 0) {
		unregister_hotcpu_notifier(&etm_cpu_notifier);
		unregister_hotcpu_notifier(&etm_cpu_dying_notifier);
	}
	etmdrvdata[cpu] = NULL;
err0:
	wakeup_source_trash(&drvdata->ws);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int etm_remove(struct platform_device *pdev)
{
	struct etm_drvdata *drvdata = platform_get_drvdata(pdev);

	if (drvdata) {
		coresight_unregister(drvdata->csdev);
		if (--count == 0) {
			unregister_hotcpu_notifier(&etm_cpu_notifier);
			unregister_hotcpu_notifier(&etm_cpu_dying_notifier);
		}
		wakeup_source_trash(&drvdata->ws);
	}
	return 0;
}

static struct of_device_id etm_match[] = {
	{.compatible = "arm,coresight-etmv4"},
	{}
};

static struct platform_driver etm_driver = {
	.probe          = etm_probe,
	.remove         = etm_remove,
	.driver         = {
		.name   = "coresight-etmv4",
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
MODULE_DESCRIPTION("CoreSight Embedded Trace Macrocell v4 driver");
