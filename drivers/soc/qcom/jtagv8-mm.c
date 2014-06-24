/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/scm.h>
#include <soc/qcom/jtag.h>

#define CORESIGHT_LAR		(0xFB0)

#define CORESIGHT_UNLOCK	(0xC5ACCE55)

#define TIMEOUT_US		(100)

#define BM(lsb, msb)		((BIT(msb) - BIT(lsb)) + BIT(msb))
#define BMVAL(val, lsb, msb)	((val & BM(lsb, msb)) >> lsb)
#define BVAL(val, n)		((val & BIT(n)) >> n)

/*
 * ETMv4 registers:
 * 0x000 - 0x2FC: Trace         registers
 * 0x300 - 0x314: Management    registers
 * 0x318 - 0xEFC: Trace         registers
 * 0xF00: Management		registers
 * 0xFA0 - 0xFA4: Trace		registers
 * 0xFA8 - 0xFFC: Management	registers
 */

/* Trace registers (0x000-0x2FC) */
/* Main control and configuration registers  */
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
#define ETM_MAX_RES_SEL			(32)
#define ETM_MAX_SS_CMP			(8)

#define ETM_ARCH_V4			(0x40)

#define MAX_ETM_STATE_SIZE	(165)

#define TZ_DBG_ETM_FEAT_ID	(0x8)
#define TZ_DBG_ETM_VER		(0x400000)

#define etm_writel(etm, val, off)	\
		   __raw_writel(val, etm->base + off)
#define etm_readl(etm, off)		\
		  __raw_readl(etm->base + off)

#define etm_writeq(etm, val, off)	\
		   __raw_writeq(val, etm->base + off)
#define etm_readq(etm, off)		\
		  __raw_readq(etm->base + off)

#define ETM_LOCK(base)							\
do {									\
	mb();								\
	etm_writel(base, 0x0, CORESIGHT_LAR);				\
} while (0)

#define ETM_UNLOCK(base)						\
do {									\
	etm_writel(base, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

/* memory mapped ETMv4 register access */
struct etm_cpu_ctx {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		*state;
};

struct etm_ctx {
	uint8_t			arch;
	uint8_t			nr_pe;
	uint8_t			nr_pe_cmp;
	uint8_t			nr_addr_cmp;
	uint8_t			nr_data_cmp;
	uint8_t			nr_cntr;
	uint8_t			nr_ext_inp;
	uint8_t			nr_ext_inp_sel;
	uint8_t			nr_ext_out;
	uint8_t			nr_ctxid_cmp;
	uint8_t			nr_vmid_cmp;
	uint8_t			nr_seq_state;
	uint8_t			nr_event;
	uint8_t			nr_resource;
	uint8_t			nr_ss_cmp;
	struct etm_cpu_ctx	*cpu_ctx[NR_CPUS];
	bool			save_restore_enabled[NR_CPUS];
	bool			os_lock_present;
};

static struct etm_ctx etm;

static struct clk *clock[NR_CPUS];

static void etm_os_lock(struct etm_cpu_ctx *etmdata)
{
	if (etm.os_lock_present) {
		etm_writel(etmdata, 0x1, TRCOSLAR);
		/* Ensure OS lock is set before proceeding */
		mb();
	}
}

static void etm_os_unlock(struct etm_cpu_ctx *etmdata)
{
	if (etm.os_lock_present) {
		/* Ensure all writes are complete before clearing OS lock */
		mb();
		etm_writel(etmdata, 0x0, TRCOSLAR);
	}
}

static inline void etm_save_state(struct etm_cpu_ctx *etmdata)
{
	int i, j, count;

	i = 0;
	mb();
	isb();
	ETM_UNLOCK(etmdata);

	switch (etm.arch) {
	case ETM_ARCH_V4:
		etm_os_lock(etmdata);

		/* poll until programmers' model becomes stable */
		for (count = TIMEOUT_US; (BVAL(etm_readl(etmdata, TRCSTATR), 1)
		     != 1) && count > 0; count--)
			udelay(1);
		if (count == 0)
			pr_err_ratelimited("programmers model is not stable\n"
					   );

		/* main control and configuration registers */
		etmdata->state[i++] = etm_readl(etmdata, TRCPROCSELR);
		etmdata->state[i++] = etm_readl(etmdata, TRCCONFIGR);
		etmdata->state[i++] = etm_readl(etmdata, TRCAUXCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCEVENTCTL0R);
		etmdata->state[i++] = etm_readl(etmdata, TRCEVENTCTL1R);
		etmdata->state[i++] = etm_readl(etmdata, TRCSTALLCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCTSCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCSYNCPR);
		etmdata->state[i++] = etm_readl(etmdata, TRCCCCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCBBCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCTRACEIDR);
		etmdata->state[i++] = etm_readl(etmdata, TRCQCTLR);
		/* filtering control registers */
		etmdata->state[i++] = etm_readl(etmdata, TRCVICTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCVIIECTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCVISSCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCVIPCSSCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCVDCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCVDSACCTLR);
		etmdata->state[i++] = etm_readl(etmdata, TRCVDARCCTLR);
		/* derived resource registers */
		for (j = 0; j < etm.nr_seq_state-1; j++)
			etmdata->state[i++] = etm_readl(etmdata, TRCSEQEVRn(j));
		etmdata->state[i++] = etm_readl(etmdata, TRCSEQRSTEVR);
		etmdata->state[i++] = etm_readl(etmdata, TRCSEQSTR);
		etmdata->state[i++] = etm_readl(etmdata, TRCEXTINSELR);
		for (j = 0; j < etm.nr_cntr; j++)  {
			etmdata->state[i++] = etm_readl(etmdata,
						       TRCCNTRLDVRn(j));
			etmdata->state[i++] = etm_readl(etmdata,
						       TRCCNTCTLRn(j));
			etmdata->state[i++] = etm_readl(etmdata,
						       TRCCNTVRn(j));
		}
		/* resource selection registers */
		for (j = 0; j < etm.nr_resource; j++)
			etmdata->state[i++] = etm_readl(etmdata, TRCRSCTLRn(j));
		/* comparator registers */
		for (j = 0; j < etm.nr_addr_cmp * 2; j++) {
			etmdata->state[i++] = etm_readq(etmdata, TRCACVRn(j));
			etmdata->state[i++] = etm_readq(etmdata, TRCACATRn(j));
		}
		for (j = 0; j < etm.nr_data_cmp; j++) {
			etmdata->state[i++] = etm_readq(etmdata, TRCDVCVRn(j));
			etmdata->state[i++] = etm_readq(etmdata, TRCDVCMRn(i));
		}
		for (j = 0; j < etm.nr_ctxid_cmp; j++)
			etmdata->state[i++] = etm_readq(etmdata, TRCCIDCVRn(j));
		etmdata->state[i++] = etm_readl(etmdata, TRCCIDCCTLR0);
		etmdata->state[i++] = etm_readl(etmdata, TRCCIDCCTLR1);
		for (j = 0; j < etm.nr_vmid_cmp; j++)
			etmdata->state[i++] = etm_readq(etmdata,
							TRCVMIDCVRn(j));
		etmdata->state[i++] = etm_readl(etmdata, TRCVMIDCCTLR0);
		etmdata->state[i++] = etm_readl(etmdata, TRCVMIDCCTLR1);
		/* single-shot comparator registers */
		for (j = 0; j < etm.nr_ss_cmp; j++) {
			etmdata->state[i++] = etm_readl(etmdata, TRCSSCCRn(j));
			etmdata->state[i++] = etm_readl(etmdata, TRCSSCSRn(j));
			etmdata->state[i++] = etm_readl(etmdata,
							TRCSSPCICRn(j));
		}
		/* claim tag registers */
		etmdata->state[i++] = etm_readl(etmdata, TRCCLAIMCLR);
		/* program ctrl register */
		etmdata->state[i++] = etm_readl(etmdata, TRCPRGCTLR);

		/* ensure trace unit is idle to be powered down */
		for (count = TIMEOUT_US; (BVAL(etm_readl(etmdata, TRCSTATR), 0)
		     != 1) && count > 0; count--)
			udelay(1);
		if (count == 0)
			pr_err_ratelimited("timeout waiting for idle state\n");
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
	case ETM_ARCH_V4:
		/* check OS lock is locked */
		if (BVAL(etm_readl(etmdata, TRCOSLSR), 1) != 1) {
			pr_err_ratelimited("OS lock is unlocked\n");
			etm_os_lock(etmdata);
		}

		/* main control and configuration registers */
		etm_writel(etmdata, etmdata->state[i++], TRCPROCSELR);
		etm_writel(etmdata, etmdata->state[i++], TRCCONFIGR);
		etm_writel(etmdata, etmdata->state[i++], TRCAUXCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCEVENTCTL0R);
		etm_writel(etmdata, etmdata->state[i++], TRCEVENTCTL1R);
		etm_writel(etmdata, etmdata->state[i++], TRCSTALLCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCTSCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCSYNCPR);
		etm_writel(etmdata, etmdata->state[i++], TRCCCCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCBBCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCTRACEIDR);
		etm_writel(etmdata, etmdata->state[i++], TRCQCTLR);
		/* filtering control registers */
		etm_writel(etmdata, etmdata->state[i++], TRCVICTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCVIIECTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCVISSCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCVIPCSSCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCVDCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCVDSACCTLR);
		etm_writel(etmdata, etmdata->state[i++], TRCVDARCCTLR);
		/* derived resources registers */
		for (j = 0; j < etm.nr_seq_state-1; j++)
			etm_writel(etmdata, etmdata->state[i++], TRCSEQEVRn(j));
		etm_writel(etmdata, etmdata->state[i++], TRCSEQRSTEVR);
		etm_writel(etmdata, etmdata->state[i++], TRCSEQSTR);
		etm_writel(etmdata, etmdata->state[i++], TRCEXTINSELR);
		for (j = 0; j < etm.nr_cntr; j++)  {
			etm_writel(etmdata, etmdata->state[i++],
				  TRCCNTRLDVRn(j));
			etm_writel(etmdata, etmdata->state[i++],
				  TRCCNTCTLRn(j));
			etm_writel(etmdata, etmdata->state[i++], TRCCNTVRn(j));
		}
		/* resource selection registers */
		for (j = 0; j < etm.nr_resource; j++)
			etm_writel(etmdata, etmdata->state[i++], TRCRSCTLRn(j));
		/* comparator registers */
		for (j = 0; j < etm.nr_addr_cmp * 2; j++) {
			etm_writeq(etmdata, etmdata->state[i++], TRCACVRn(j));
			etm_writeq(etmdata, etmdata->state[i++], TRCACATRn(j));
		}
		for (j = 0; j < etm.nr_data_cmp; j++) {
			etm_writeq(etmdata, etmdata->state[i++], TRCDVCVRn(j));
			etm_writeq(etmdata, etmdata->state[i++], TRCDVCMRn(j));
		}
		for (j = 0; j < etm.nr_ctxid_cmp; j++)
			etm_writeq(etmdata, etmdata->state[i++], TRCCIDCVRn(j));
		etm_writel(etmdata, etmdata->state[i++], TRCCIDCCTLR0);
		etm_writel(etmdata, etmdata->state[i++], TRCCIDCCTLR1);
		for (j = 0; j < etm.nr_vmid_cmp; j++)
			etm_writeq(etmdata, etmdata->state[i++],
				   TRCVMIDCVRn(j));
		etm_writel(etmdata, etmdata->state[i++], TRCVMIDCCTLR0);
		etm_writel(etmdata, etmdata->state[i++], TRCVMIDCCTLR1);
		/* e-shot comparator registers */
		for (j = 0; j < etm.nr_ss_cmp; j++) {
			etm_writel(etmdata, etmdata->state[i++], TRCSSCCRn(j));
			etm_writel(etmdata, etmdata->state[i++], TRCSSCSRn(j));
			etm_writel(etmdata, etmdata->state[i++],
				   TRCSSPCICRn(j));
		}
		/* claim tag registers */
		etm_writel(etmdata, etmdata->state[i++], TRCCLAIMSET);
		/* program ctrl register */
		etm_writel(etmdata, etmdata->state[i++], TRCPRGCTLR);

		etm_os_unlock(etmdata);
		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n", etm.arch,
				   __func__);
	}

	ETM_LOCK(etmdata);
}

void msm_jtag_mm_save_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	if (etm.save_restore_enabled[cpu])
		etm_save_state(etm.cpu_ctx[cpu]);
}
EXPORT_SYMBOL(msm_jtag_mm_save_state);

void msm_jtag_mm_restore_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	/*
	 * Check to ensure we attempt to restore only when save
	 * has been done is accomplished by callee function.
	 */
	if (etm.save_restore_enabled[cpu])
		etm_restore_state(etm.cpu_ctx[cpu]);
}
EXPORT_SYMBOL(msm_jtag_mm_restore_state);

static inline bool etm_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ETM_ARCH_V4:
		break;
	default:
		return false;
	}
	return true;
}

static void etm_os_lock_init(struct etm_cpu_ctx *etmdata)
{
	uint32_t etmoslsr;

	etmoslsr = etm_readl(etmdata, TRCOSLSR);
	if ((BVAL(etmoslsr, 0) == 0)  && BVAL(etmoslsr, 3))
		etm.os_lock_present = true;
	else
		etm.os_lock_present = false;
}

static void etm_init_arch_data(void *info)
{
	uint32_t val;
	struct etm_cpu_ctx  *etmdata = info;

	ETM_UNLOCK(etmdata);

	etm_os_lock_init(etmdata);

	val = etm_readl(etmdata, TRCIDR1);
	etm.arch = BMVAL(val, 4, 11);

	/* number of resources trace unit supports */
	val = etm_readl(etmdata, TRCIDR4);
	etm.nr_addr_cmp = BMVAL(val, 0, 3);
	etm.nr_data_cmp = BMVAL(val, 4, 7);
	etm.nr_resource = BMVAL(val, 16, 19);
	etm.nr_ss_cmp = BMVAL(val, 20, 23);
	etm.nr_ctxid_cmp = BMVAL(val, 24, 27);
	etm.nr_vmid_cmp = BMVAL(val, 28, 31);

	val = etm_readl(etmdata, TRCIDR5);
	etm.nr_seq_state = BMVAL(val, 25, 27);
	etm.nr_cntr = BMVAL(val, 28, 30);

	ETM_LOCK(etmdata);
}

static int jtag_mm_etm_probe(struct platform_device *pdev, uint32_t cpu)
{
	struct etm_cpu_ctx *etmdata;
	struct resource *res;
	struct device *dev = &pdev->dev;

	/* Allocate memory per cpu */
	etmdata = devm_kzalloc(dev, sizeof(struct etm_cpu_ctx), GFP_KERNEL);
	if (!etmdata)
		return -ENOMEM;

	etm.cpu_ctx[cpu] = etmdata;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "etm-base");

	etmdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!etmdata->base)
		return -EINVAL;

	/* Allocate etm state save space per core */
	etmdata->state = devm_kzalloc(dev,
				      MAX_ETM_STATE_SIZE * sizeof(uint64_t),
				      GFP_KERNEL);
	if (!etmdata->state)
		return -ENOMEM;

	if (cpu == 0) {
		if (smp_call_function_single(cpu, etm_init_arch_data, etmdata,
					     1))
			dev_err(dev, "Jtagmm: ETM arch init failed\n");
	}
	if (etm_arch_supported(etm.arch)) {
		if (scm_get_feat_version(TZ_DBG_ETM_FEAT_ID) < TZ_DBG_ETM_VER)
			etm.save_restore_enabled[cpu] = true;
		else
			pr_info("etm save-restore supported by TZ\n");
	} else
		pr_info("etm arch %u not supported\n", etm.arch);
	return 0;
}

static int jtag_mm_probe(struct platform_device *pdev)
{
	int ret;
	static uint32_t cpu;
	static uint32_t count;
	struct device *dev = &pdev->dev;

	if (msm_jtag_fuse_apps_access_disabled())
		return -EPERM;

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

	ret  = jtag_mm_etm_probe(pdev, cpu);
	if (ret)
		clk_disable_unprepare(clock[cpu]);

	return ret;
}

static int jtag_mm_remove(struct platform_device *pdev)
{
	struct clk *clock = platform_get_drvdata(pdev);

	clk_disable_unprepare(clock);
	return 0;
}

static struct of_device_id msm_qdss_mm_match[] = {
	{ .compatible = "qcom,jtagv8-mm"},
	{}
};

static struct platform_driver jtag_mm_driver = {
	.probe          = jtag_mm_probe,
	.remove         = jtag_mm_remove,
	.driver         = {
		.name   = "msm-jtagv8-mm",
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
MODULE_DESCRIPTION("CoreSight DEBUGv8 and ETMv4 save-restore driver");
