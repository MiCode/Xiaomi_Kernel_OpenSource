/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/jtag.h>
#include <asm/smp_plat.h>
#include <asm/etmv4x.h>
#include <soc/qcom/socinfo.h>

#define CORESIGHT_LAR		(0xFB0)

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

#define ETM_CPMR_CLKEN			(0x4)
#define ETM_ARCH_V4			(0x40)
#define ETM_ARCH_V4_2			(0x42)

#define MAX_ETM_STATE_SIZE	(165)

#define TZ_DBG_ETM_FEAT_ID	(0x8)
#define TZ_DBG_ETM_VER		(0x400000)
#define HW_SOC_ID_M8953		(293)
#define GET_FEAT_VERSION_CMD	3

/* spread out etm register write */
#define etm_writel(etm, val, off)	\
do {							\
	writel_relaxed_no_log(val, etm->base + off);	\
	udelay(20);					\
} while (0)

#define etm_writel_log(etm, val, off)	\
		   __raw_writel(val, etm->base + off)

#define etm_readl(etm, off)		\
		   readl_relaxed_no_log(etm->base + off)

#define etm_writeq(etm, val, off)	\
		   writeq_relaxed_no_log(val, etm->base + off)

#define etm_readq(etm, off)		\
		   readq_relaxed_no_log(etm->base + off)

#define ETM_LOCK(base)							\
do {									\
	mb(); /* ensure configuration take effect before we lock it */	\
	etm_writel_log(base, 0x0, CORESIGHT_LAR);			\
} while (0)

#define ETM_UNLOCK(base)						\
do {									\
	etm_writel_log(base, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb(); /* ensure unlock take effect before we configure */	\
} while (0)

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
	bool			si_enable;
	bool			save_restore_disabled;
	bool			save_restore_enabled;
	bool			os_lock_present;
	bool			init;
	bool			enable;
	void __iomem		*base;
	struct device		*dev;
	uint64_t		*state;
	spinlock_t		spinlock;
	struct mutex		mutex;
};

static struct etm_ctx *etm[NR_CPUS];
static int cnt;

static struct clk *clock[NR_CPUS];

static ATOMIC_NOTIFIER_HEAD(etm_save_notifier_list);
static ATOMIC_NOTIFIER_HEAD(etm_restore_notifier_list);

int msm_jtag_save_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&etm_save_notifier_list, nb);
}
EXPORT_SYMBOL(msm_jtag_save_register);

int msm_jtag_save_unregister(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&etm_save_notifier_list, nb);
}
EXPORT_SYMBOL(msm_jtag_save_unregister);

int msm_jtag_restore_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&etm_restore_notifier_list, nb);
}
EXPORT_SYMBOL(msm_jtag_restore_register);

int msm_jtag_restore_unregister(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&etm_restore_notifier_list, nb);
}
EXPORT_SYMBOL(msm_jtag_restore_unregister);

static void etm_os_lock(struct etm_ctx *etmdata)
{
	if (etmdata->os_lock_present) {
		etm_writel(etmdata, 0x1, TRCOSLAR);
		/* Ensure OS lock is set before proceeding */
		mb();
	}
}

static void etm_os_unlock(struct etm_ctx *etmdata)
{
	if (etmdata->os_lock_present) {
		/* Ensure all writes are complete before clearing OS lock */
		mb();
		etm_writel(etmdata, 0x0, TRCOSLAR);
	}
}

static inline void etm_mm_save_state(struct etm_ctx *etmdata)
{
	int i, j, count;

	i = 0;
	mb(); /* ensure all register writes complete before saving them */
	isb();
	ETM_UNLOCK(etmdata);

	switch (etmdata->arch) {
	case ETM_ARCH_V4_2:
	case ETM_ARCH_V4:
		etm_os_lock(etmdata);

		/* poll until programmers' model becomes stable */
		for (count = TIMEOUT_US; (BVAL(etm_readl(etmdata, TRCSTATR), 1)
		     != 1) && count > 0; count--)
			udelay(1);
		if (count == 0)
			pr_err_ratelimited("programmers model is not stable\n"
					   );

		etmdata->state[i++] = etm_readl(etmdata, TRCPRGCTLR);
		if (!(etmdata->state[0] & BIT(0))) {
			atomic_notifier_call_chain(&etm_save_notifier_list,
							0, NULL);
			break;
		}
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
		for (j = 0; j < etmdata->nr_seq_state-1; j++)
			etmdata->state[i++] = etm_readl(etmdata, TRCSEQEVRn(j));
		etmdata->state[i++] = etm_readl(etmdata, TRCSEQRSTEVR);
		etmdata->state[i++] = etm_readl(etmdata, TRCSEQSTR);
		etmdata->state[i++] = etm_readl(etmdata, TRCEXTINSELR);
		for (j = 0; j < etmdata->nr_cntr; j++)  {
			etmdata->state[i++] = etm_readl(etmdata,
						       TRCCNTRLDVRn(j));
			etmdata->state[i++] = etm_readl(etmdata,
						       TRCCNTCTLRn(j));
			etmdata->state[i++] = etm_readl(etmdata,
						       TRCCNTVRn(j));
		}
		/* resource selection registers */
		for (j = 0; j < etmdata->nr_resource; j++)
			etmdata->state[i++] = etm_readl(etmdata, TRCRSCTLRn(j));
		/* comparator registers */
		for (j = 0; j < etmdata->nr_addr_cmp * 2; j++) {
			etmdata->state[i++] = etm_readq(etmdata, TRCACVRn(j));
			etmdata->state[i++] = etm_readq(etmdata, TRCACATRn(j));
		}
		for (j = 0; j < etmdata->nr_data_cmp; j++) {
			etmdata->state[i++] = etm_readq(etmdata, TRCDVCVRn(j));
			etmdata->state[i++] = etm_readq(etmdata, TRCDVCMRn(i));
		}
		for (j = 0; j < etmdata->nr_ctxid_cmp; j++)
			etmdata->state[i++] = etm_readq(etmdata, TRCCIDCVRn(j));
		etmdata->state[i++] = etm_readl(etmdata, TRCCIDCCTLR0);
		etmdata->state[i++] = etm_readl(etmdata, TRCCIDCCTLR1);
		for (j = 0; j < etmdata->nr_vmid_cmp; j++)
			etmdata->state[i++] = etm_readq(etmdata,
							TRCVMIDCVRn(j));
		etmdata->state[i++] = etm_readl(etmdata, TRCVMIDCCTLR0);
		etmdata->state[i++] = etm_readl(etmdata, TRCVMIDCCTLR1);
		/* single-shot comparator registers */
		for (j = 0; j < etmdata->nr_ss_cmp; j++) {
			etmdata->state[i++] = etm_readl(etmdata, TRCSSCCRn(j));
			etmdata->state[i++] = etm_readl(etmdata, TRCSSCSRn(j));
			etmdata->state[i++] = etm_readl(etmdata,
							TRCSSPCICRn(j));
		}
		/* claim tag registers */
		etmdata->state[i++] = etm_readl(etmdata, TRCCLAIMCLR);

		/* ensure trace unit is idle to be powered down */
		for (count = TIMEOUT_US; (BVAL(etm_readl(etmdata, TRCSTATR), 0)
		     != 1) && count > 0; count--)
			udelay(1);
		if (count == 0)
			pr_err_ratelimited("timeout waiting for idle state\n");

		atomic_notifier_call_chain(&etm_save_notifier_list, 0, NULL);

		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n",
				   etmdata->arch, __func__);
	}

	ETM_LOCK(etmdata);
}

static inline void etm_mm_restore_state(struct etm_ctx *etmdata)
{
	int i, j;

	i = 0;
	ETM_UNLOCK(etmdata);

	switch (etmdata->arch) {
	case ETM_ARCH_V4_2:
	case ETM_ARCH_V4:
		atomic_notifier_call_chain(&etm_restore_notifier_list, 0, NULL);

		/* check OS lock is locked */
		if (BVAL(etm_readl(etmdata, TRCOSLSR), 1) != 1) {
			pr_err_ratelimited("OS lock is unlocked\n");
			etm_os_lock(etmdata);
		}

		if (!(etmdata->state[0] & BIT(0))) {
			etm_os_unlock(etmdata);
			break;
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
		for (j = 0; j < etmdata->nr_seq_state-1; j++)
			etm_writel(etmdata, etmdata->state[i++], TRCSEQEVRn(j));
		etm_writel(etmdata, etmdata->state[i++], TRCSEQRSTEVR);
		etm_writel(etmdata, etmdata->state[i++], TRCSEQSTR);
		etm_writel(etmdata, etmdata->state[i++], TRCEXTINSELR);
		for (j = 0; j < etmdata->nr_cntr; j++)  {
			etm_writel(etmdata, etmdata->state[i++],
				  TRCCNTRLDVRn(j));
			etm_writel(etmdata, etmdata->state[i++],
				  TRCCNTCTLRn(j));
			etm_writel(etmdata, etmdata->state[i++], TRCCNTVRn(j));
		}
		/* resource selection registers */
		for (j = 0; j < etmdata->nr_resource; j++)
			etm_writel(etmdata, etmdata->state[i++], TRCRSCTLRn(j));
		/* comparator registers */
		for (j = 0; j < etmdata->nr_addr_cmp * 2; j++) {
			etm_writeq(etmdata, etmdata->state[i++], TRCACVRn(j));
			etm_writeq(etmdata, etmdata->state[i++], TRCACATRn(j));
		}
		for (j = 0; j < etmdata->nr_data_cmp; j++) {
			etm_writeq(etmdata, etmdata->state[i++], TRCDVCVRn(j));
			etm_writeq(etmdata, etmdata->state[i++], TRCDVCMRn(j));
		}
		for (j = 0; j < etmdata->nr_ctxid_cmp; j++)
			etm_writeq(etmdata, etmdata->state[i++], TRCCIDCVRn(j));
		etm_writel(etmdata, etmdata->state[i++], TRCCIDCCTLR0);
		etm_writel(etmdata, etmdata->state[i++], TRCCIDCCTLR1);
		for (j = 0; j < etmdata->nr_vmid_cmp; j++)
			etm_writeq(etmdata, etmdata->state[i++],
				   TRCVMIDCVRn(j));
		etm_writel(etmdata, etmdata->state[i++], TRCVMIDCCTLR0);
		etm_writel(etmdata, etmdata->state[i++], TRCVMIDCCTLR1);
		/* e-shot comparator registers */
		for (j = 0; j < etmdata->nr_ss_cmp; j++) {
			etm_writel(etmdata, etmdata->state[i++], TRCSSCCRn(j));
			etm_writel(etmdata, etmdata->state[i++], TRCSSCSRn(j));
			etm_writel(etmdata, etmdata->state[i++],
				   TRCSSPCICRn(j));
		}
		/* claim tag registers */
		etm_writel(etmdata, etmdata->state[i++], TRCCLAIMSET);
		/* program ctrl register */
		etm_writel(etmdata, etmdata->state[0], TRCPRGCTLR);

		etm_os_unlock(etmdata);
		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n",
				   etmdata->arch,  __func__);
	}

	ETM_LOCK(etmdata);
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

static int etm_read_ssxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = trc_readl(ETMSEQEVR0);
		break;
	case 1:
		state[i++] = trc_readl(ETMSEQEVR1);
		break;
	case 2:
		state[i++] = trc_readl(ETMSEQEVR2);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_crxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = trc_readl(ETMCNTRLDVR0);
		state[i++] = trc_readl(ETMCNTCTLR0);
		state[i++] = trc_readl(ETMCNTVR0);
		break;
	case 1:
		state[i++] = trc_readl(ETMCNTRLDVR1);
		state[i++] = trc_readl(ETMCNTCTLR1);
		state[i++] = trc_readl(ETMCNTVR1);
		break;
	case 2:
		state[i++] = trc_readl(ETMCNTRLDVR2);
		state[i++] = trc_readl(ETMCNTCTLR2);
		state[i++] = trc_readl(ETMCNTVR2);
		break;
	case 3:
		state[i++] = trc_readl(ETMCNTRLDVR3);
		state[i++] = trc_readl(ETMCNTCTLR3);
		state[i++] = trc_readl(ETMCNTVR3);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_rsxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 2:
		state[i++] = trc_readl(ETMRSCTLR2);
		break;
	case 3:
		state[i++] = trc_readl(ETMRSCTLR3);
		break;
	case 4:
		state[i++] = trc_readl(ETMRSCTLR4);
		break;
	case 5:
		state[i++] = trc_readl(ETMRSCTLR5);
		break;
	case 6:
		state[i++] = trc_readl(ETMRSCTLR6);
		break;
	case 7:
		state[i++] = trc_readl(ETMRSCTLR7);
		break;
	case 8:
		state[i++] = trc_readl(ETMRSCTLR8);
		break;
	case 9:
		state[i++] = trc_readl(ETMRSCTLR9);
		break;
	case 10:
		state[i++] = trc_readl(ETMRSCTLR10);
		break;
	case 11:
		state[i++] = trc_readl(ETMRSCTLR11);
		break;
	case 12:
		state[i++] = trc_readl(ETMRSCTLR12);
		break;
	case 13:
		state[i++] = trc_readl(ETMRSCTLR13);
		break;
	case 14:
		state[i++] = trc_readl(ETMRSCTLR14);
		break;
	case 15:
		state[i++] = trc_readl(ETMRSCTLR15);
		break;
	case 16:
		state[i++] = trc_readl(ETMRSCTLR16);
		break;
	case 17:
		state[i++] = trc_readl(ETMRSCTLR17);
		break;
	case 18:
		state[i++] = trc_readl(ETMRSCTLR18);
		break;
	case 19:
		state[i++] = trc_readl(ETMRSCTLR19);
		break;
	case 20:
		state[i++] = trc_readl(ETMRSCTLR20);
		break;
	case 21:
		state[i++] = trc_readl(ETMRSCTLR21);
		break;
	case 22:
		state[i++] = trc_readl(ETMRSCTLR22);
		break;
	case 23:
		state[i++] = trc_readl(ETMRSCTLR23);
		break;
	case 24:
		state[i++] = trc_readl(ETMRSCTLR24);
		break;
	case 25:
		state[i++] = trc_readl(ETMRSCTLR25);
		break;
	case 26:
		state[i++] = trc_readl(ETMRSCTLR26);
		break;
	case 27:
		state[i++] = trc_readl(ETMRSCTLR27);
		break;
	case 28:
		state[i++] = trc_readl(ETMRSCTLR28);
		break;
	case 29:
		state[i++] = trc_readl(ETMRSCTLR29);
		break;
	case 30:
		state[i++] = trc_readl(ETMRSCTLR30);
		break;
	case 31:
		state[i++] = trc_readl(ETMRSCTLR31);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_acr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = trc_readq(ETMACVR0);
		state[i++] = trc_readq(ETMACATR0);
		break;
	case 1:
		state[i++] = trc_readq(ETMACVR1);
		state[i++] = trc_readq(ETMACATR1);
		break;
	case 2:
		state[i++] = trc_readq(ETMACVR2);
		state[i++] = trc_readq(ETMACATR2);
		break;
	case 3:
		state[i++] = trc_readq(ETMACVR3);
		state[i++] = trc_readq(ETMACATR3);
		break;
	case 4:
		state[i++] = trc_readq(ETMACVR4);
		state[i++] = trc_readq(ETMACATR4);
		break;
	case 5:
		state[i++] = trc_readq(ETMACVR5);
		state[i++] = trc_readq(ETMACATR5);
		break;
	case 6:
		state[i++] = trc_readq(ETMACVR6);
		state[i++] = trc_readq(ETMACATR6);
		break;
	case 7:
		state[i++] = trc_readq(ETMACVR7);
		state[i++] = trc_readq(ETMACATR7);
		break;
	case 8:
		state[i++] = trc_readq(ETMACVR8);
		state[i++] = trc_readq(ETMACATR8);
		break;
	case 9:
		state[i++] = trc_readq(ETMACVR9);
		state[i++] = trc_readq(ETMACATR9);
		break;
	case 10:
		state[i++] = trc_readq(ETMACVR10);
		state[i++] = trc_readq(ETMACATR10);
		break;
	case 11:
		state[i++] = trc_readq(ETMACVR11);
		state[i++] = trc_readq(ETMACATR11);
		break;
	case 12:
		state[i++] = trc_readq(ETMACVR12);
		state[i++] = trc_readq(ETMACATR12);
		break;
	case 13:
		state[i++] = trc_readq(ETMACVR13);
		state[i++] = trc_readq(ETMACATR13);
		break;
	case 14:
		state[i++] = trc_readq(ETMACVR14);
		state[i++] = trc_readq(ETMACATR14);
		break;
	case 15:
		state[i++] = trc_readq(ETMACVR15);
		state[i++] = trc_readq(ETMACATR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_dvcr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = trc_readq(ETMDVCVR0);
		state[i++] = trc_readq(ETMDVCMR0);
		break;
	case 1:
		state[i++] = trc_readq(ETMDVCVR1);
		state[i++] = trc_readq(ETMDVCMR1);
		break;
	case 2:
		state[i++] = trc_readq(ETMDVCVR2);
		state[i++] = trc_readq(ETMDVCMR2);
		break;
	case 3:
		state[i++] = trc_readq(ETMDVCVR3);
		state[i++] = trc_readq(ETMDVCMR3);
		break;
	case 4:
		state[i++] = trc_readq(ETMDVCVR4);
		state[i++] = trc_readq(ETMDVCMR4);
		break;
	case 5:
		state[i++] = trc_readq(ETMDVCVR5);
		state[i++] = trc_readq(ETMDVCMR5);
		break;
	case 6:
		state[i++] = trc_readq(ETMDVCVR6);
		state[i++] = trc_readq(ETMDVCMR6);
		break;
	case 7:
		state[i++] = trc_readq(ETMDVCVR7);
		state[i++] = trc_readq(ETMDVCMR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_ccvr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = trc_readq(ETMCIDCVR0);
		break;
	case 1:
		state[i++] = trc_readq(ETMCIDCVR1);
		break;
	case 2:
		state[i++] = trc_readq(ETMCIDCVR2);
		break;
	case 3:
		state[i++] = trc_readq(ETMCIDCVR3);
		break;
	case 4:
		state[i++] = trc_readq(ETMCIDCVR4);
		break;
	case 5:
		state[i++] = trc_readq(ETMCIDCVR5);
		break;
	case 6:
		state[i++] = trc_readq(ETMCIDCVR6);
		break;
	case 7:
		state[i++] = trc_readq(ETMCIDCVR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_vcvr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = trc_readq(ETMVMIDCVR0);
		break;
	case 1:
		state[i++] = trc_readq(ETMVMIDCVR1);
		break;
	case 2:
		state[i++] = trc_readq(ETMVMIDCVR2);
		break;
	case 3:
		state[i++] = trc_readq(ETMVMIDCVR3);
		break;
	case 4:
		state[i++] = trc_readq(ETMVMIDCVR4);
		break;
	case 5:
		state[i++] = trc_readq(ETMVMIDCVR5);
		break;
	case 6:
		state[i++] = trc_readq(ETMVMIDCVR6);
		break;
	case 7:
		state[i++] = trc_readq(ETMVMIDCVR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_read_sscr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		state[i++] = trc_readl(ETMSSCCR0);
		state[i++] = trc_readl(ETMSSCSR0);
		state[i++] = trc_readl(ETMSSPCICR0);
		break;
	case 1:
		state[i++] = trc_readl(ETMSSCCR1);
		state[i++] = trc_readl(ETMSSCSR1);
		state[i++] = trc_readl(ETMSSPCICR1);
		break;
	case 2:
		state[i++] = trc_readl(ETMSSCCR2);
		state[i++] = trc_readl(ETMSSCSR2);
		state[i++] = trc_readl(ETMSSPCICR2);
		break;
	case 3:
		state[i++] = trc_readl(ETMSSCCR3);
		state[i++] = trc_readl(ETMSSCSR3);
		state[i++] = trc_readl(ETMSSPCICR3);
		break;
	case 4:
		state[i++] = trc_readl(ETMSSCCR4);
		state[i++] = trc_readl(ETMSSCSR4);
		state[i++] = trc_readl(ETMSSPCICR4);
		break;
	case 5:
		state[i++] = trc_readl(ETMSSCCR5);
		state[i++] = trc_readl(ETMSSCSR5);
		state[i++] = trc_readl(ETMSSPCICR5);
		break;
	case 6:
		state[i++] = trc_readl(ETMSSCCR6);
		state[i++] = trc_readl(ETMSSCSR6);
		state[i++] = trc_readl(ETMSSPCICR6);
		break;
	case 7:
		state[i++] = trc_readl(ETMSSCCR7);
		state[i++] = trc_readl(ETMSSCSR7);
		state[i++] = trc_readl(ETMSSPCICR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static inline void etm_si_save_state(struct etm_ctx *etmdata)
{
	int i, j, count;

	i = 0;
	/* Ensure all writes are complete before saving ETM registers */
	mb();
	isb();

	/* Vote for ETM power/clock enable */
	etm_clk_enable();

	switch (etmdata->arch) {
	case ETM_ARCH_V4_2:
	case ETM_ARCH_V4:
		trc_write(0x1, ETMOSLAR);
		isb();

		/* poll until programmers' model becomes stable */
		for (count = TIMEOUT_US; (BVAL(trc_readl(ETMSTATR), 1)
		     != 1) && count > 0; count--)
			udelay(1);
		if (count == 0)
			pr_err_ratelimited("programmers model is not stable\n");

		/* main control and configuration registers */
		etmdata->state[i++] = trc_readl(ETMCONFIGR);
		etmdata->state[i++] = trc_readl(ETMEVENTCTL0R);
		etmdata->state[i++] = trc_readl(ETMEVENTCTL1R);
		etmdata->state[i++] = trc_readl(ETMSTALLCTLR);
		etmdata->state[i++] = trc_readl(ETMTSCTLR);
		etmdata->state[i++] = trc_readl(ETMSYNCPR);
		etmdata->state[i++] = trc_readl(ETMCCCTLR);
		etmdata->state[i++] = trc_readl(ETMTRACEIDR);
		/* filtering control registers */
		etmdata->state[i++] = trc_readl(ETMVICTLR);
		etmdata->state[i++] = trc_readl(ETMVIIECTLR);
		etmdata->state[i++] = trc_readl(ETMVISSCTLR);
		/* derived resource registers */
		for (j = 0; j < etmdata->nr_seq_state-1; j++)
			i = etm_read_ssxr(etmdata->state, i, j);
		etmdata->state[i++] = trc_readl(ETMSEQRSTEVR);
		etmdata->state[i++] = trc_readl(ETMSEQSTR);
		etmdata->state[i++] = trc_readl(ETMEXTINSELR);
		for (j = 0; j < etmdata->nr_cntr; j++)
			i = etm_read_crxr(etmdata->state, i, j);
		/* resource selection registers */
		for (j = 0; j < etmdata->nr_resource; j++)
			i = etm_read_rsxr(etmdata->state, i, j + 2);
		/* comparator registers */
		for (j = 0; j < etmdata->nr_addr_cmp * 2; j++)
			i = etm_read_acr(etmdata->state, i, j);
		for (j = 0; j < etmdata->nr_data_cmp; j++)
			i = etm_read_dvcr(etmdata->state, i, j);
		for (j = 0; j < etmdata->nr_ctxid_cmp; j++)
			i = etm_read_ccvr(etmdata->state, i, j);
		etmdata->state[i++] = trc_readl(ETMCIDCCTLR0);
		for (j = 0; j < etmdata->nr_vmid_cmp; j++)
			i = etm_read_vcvr(etmdata->state, i, j);
		/* single-shot comparator registers */
		for (j = 0; j < etmdata->nr_ss_cmp; j++)
			i = etm_read_sscr(etmdata->state, i, j);
		/* program ctrl register */
		etmdata->state[i++] = trc_readl(ETMPRGCTLR);

		/* ensure trace unit is idle to be powered down */
		for (count = TIMEOUT_US; (BVAL(trc_readl(ETMSTATR), 0)
		     != 1) && count > 0; count--)
			udelay(1);
		if (count == 0)
			pr_err_ratelimited("timeout waiting for idle state\n");

		atomic_notifier_call_chain(&etm_save_notifier_list, 0, NULL);

		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n",
				   etmdata->arch, __func__);
	}

	/* Vote for ETM power/clock disable */
	etm_clk_disable();
}

static int etm_write_ssxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		trc_write(state[i++], ETMSEQEVR0);
		break;
	case 1:
		trc_write(state[i++], ETMSEQEVR1);
		break;
	case 2:
		trc_write(state[i++], ETMSEQEVR2);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_crxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		trc_write(state[i++], ETMCNTRLDVR0);
		trc_write(state[i++], ETMCNTCTLR0);
		trc_write(state[i++], ETMCNTVR0);
		break;
	case 1:
		trc_write(state[i++], ETMCNTRLDVR1);
		trc_write(state[i++], ETMCNTCTLR1);
		trc_write(state[i++], ETMCNTVR1);
		break;
	case 2:
		trc_write(state[i++], ETMCNTRLDVR2);
		trc_write(state[i++], ETMCNTCTLR2);
		trc_write(state[i++], ETMCNTVR2);
		break;
	case 3:
		trc_write(state[i++], ETMCNTRLDVR3);
		trc_write(state[i++], ETMCNTCTLR3);
		trc_write(state[i++], ETMCNTVR3);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_rsxr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 2:
		trc_write(state[i++], ETMRSCTLR2);
		break;
	case 3:
		trc_write(state[i++], ETMRSCTLR3);
		break;
	case 4:
		trc_write(state[i++], ETMRSCTLR4);
		break;
	case 5:
		trc_write(state[i++], ETMRSCTLR5);
		break;
	case 6:
		trc_write(state[i++], ETMRSCTLR6);
		break;
	case 7:
		trc_write(state[i++], ETMRSCTLR7);
		break;
	case 8:
		trc_write(state[i++], ETMRSCTLR8);
		break;
	case 9:
		trc_write(state[i++], ETMRSCTLR9);
		break;
	case 10:
		trc_write(state[i++], ETMRSCTLR10);
		break;
	case 11:
		trc_write(state[i++], ETMRSCTLR11);
		break;
	case 12:
		trc_write(state[i++], ETMRSCTLR12);
		break;
	case 13:
		trc_write(state[i++], ETMRSCTLR13);
		break;
	case 14:
		trc_write(state[i++], ETMRSCTLR14);
		break;
	case 15:
		trc_write(state[i++], ETMRSCTLR15);
		break;
	case 16:
		trc_write(state[i++], ETMRSCTLR16);
		break;
	case 17:
		trc_write(state[i++], ETMRSCTLR17);
		break;
	case 18:
		trc_write(state[i++], ETMRSCTLR18);
		break;
	case 19:
		trc_write(state[i++], ETMRSCTLR19);
		break;
	case 20:
		trc_write(state[i++], ETMRSCTLR20);
		break;
	case 21:
		trc_write(state[i++], ETMRSCTLR21);
		break;
	case 22:
		trc_write(state[i++], ETMRSCTLR22);
		break;
	case 23:
		trc_write(state[i++], ETMRSCTLR23);
		break;
	case 24:
		trc_write(state[i++], ETMRSCTLR24);
		break;
	case 25:
		trc_write(state[i++], ETMRSCTLR25);
		break;
	case 26:
		trc_write(state[i++], ETMRSCTLR26);
		break;
	case 27:
		trc_write(state[i++], ETMRSCTLR27);
		break;
	case 28:
		trc_write(state[i++], ETMRSCTLR28);
		break;
	case 29:
		trc_write(state[i++], ETMRSCTLR29);
		break;
	case 30:
		trc_write(state[i++], ETMRSCTLR30);
		break;
	case 31:
		trc_write(state[i++], ETMRSCTLR31);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_acr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		trc_write(state[i++], ETMACVR0);
		trc_write(state[i++], ETMACATR0);
		break;
	case 1:
		trc_write(state[i++], ETMACVR1);
		trc_write(state[i++], ETMACATR1);
		break;
	case 2:
		trc_write(state[i++], ETMACVR2);
		trc_write(state[i++], ETMACATR2);
		break;
	case 3:
		trc_write(state[i++], ETMACVR3);
		trc_write(state[i++], ETMACATR3);
		break;
	case 4:
		trc_write(state[i++], ETMACVR4);
		trc_write(state[i++], ETMACATR4);
		break;
	case 5:
		trc_write(state[i++], ETMACVR5);
		trc_write(state[i++], ETMACATR5);
		break;
	case 6:
		trc_write(state[i++], ETMACVR6);
		trc_write(state[i++], ETMACATR6);
		break;
	case 7:
		trc_write(state[i++], ETMACVR7);
		trc_write(state[i++], ETMACATR7);
		break;
	case 8:
		trc_write(state[i++], ETMACVR8);
		trc_write(state[i++], ETMACATR8);
		break;
	case 9:
		trc_write(state[i++], ETMACVR9);
		trc_write(state[i++], ETMACATR9);
		break;
	case 10:
		trc_write(state[i++], ETMACVR10);
		trc_write(state[i++], ETMACATR10);
		break;
	case 11:
		trc_write(state[i++], ETMACVR11);
		trc_write(state[i++], ETMACATR11);
		break;
	case 12:
		trc_write(state[i++], ETMACVR12);
		trc_write(state[i++], ETMACATR12);
		break;
	case 13:
		trc_write(state[i++], ETMACVR13);
		trc_write(state[i++], ETMACATR13);
		break;
	case 14:
		trc_write(state[i++], ETMACVR14);
		trc_write(state[i++], ETMACATR14);
		break;
	case 15:
		trc_write(state[i++], ETMACVR15);
		trc_write(state[i++], ETMACATR15);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_dvcr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		trc_write(state[i++], ETMDVCVR0);
		trc_write(state[i++], ETMDVCMR0);
		break;
	case 1:
		trc_write(state[i++], ETMDVCVR1);
		trc_write(state[i++], ETMDVCMR1);
		break;
	case 2:
		trc_write(state[i++], ETMDVCVR2);
		trc_write(state[i++], ETMDVCMR2);
		break;
	case 3:
		trc_write(state[i++], ETMDVCVR3);
		trc_write(state[i++], ETMDVCMR3);
		break;
	case 4:
		trc_write(state[i++], ETMDVCVR4);
		trc_write(state[i++], ETMDVCMR4);
		break;
	case 5:
		trc_write(state[i++], ETMDVCVR5);
		trc_write(state[i++], ETMDVCMR5);
		break;
	case 6:
		trc_write(state[i++], ETMDVCVR6);
		trc_write(state[i++], ETMDVCMR6);
		break;
	case 7:
		trc_write(state[i++], ETMDVCVR7);
		trc_write(state[i++], ETMDVCMR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_ccvr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		trc_write(state[i++], ETMCIDCVR0);
		break;
	case 1:
		trc_write(state[i++], ETMCIDCVR1);
		break;
	case 2:
		trc_write(state[i++], ETMCIDCVR2);
		break;
	case 3:
		trc_write(state[i++], ETMCIDCVR3);
		break;
	case 4:
		trc_write(state[i++], ETMCIDCVR4);
		break;
	case 5:
		trc_write(state[i++], ETMCIDCVR5);
		break;
	case 6:
		trc_write(state[i++], ETMCIDCVR6);
		break;
	case 7:
		trc_write(state[i++], ETMCIDCVR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_vcvr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		trc_write(state[i++], ETMVMIDCVR0);
		break;
	case 1:
		trc_write(state[i++], ETMVMIDCVR1);
		break;
	case 2:
		trc_write(state[i++], ETMVMIDCVR2);
		break;
	case 3:
		trc_write(state[i++], ETMVMIDCVR3);
		break;
	case 4:
		trc_write(state[i++], ETMVMIDCVR4);
		break;
	case 5:
		trc_write(state[i++], ETMVMIDCVR5);
		break;
	case 6:
		trc_write(state[i++], ETMVMIDCVR6);
		break;
	case 7:
		trc_write(state[i++], ETMVMIDCVR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static int etm_write_sscr(uint64_t *state, int i, int j)
{
	switch (j) {
	case 0:
		trc_write(state[i++], ETMSSCCR0);
		trc_write(state[i++], ETMSSCSR0);
		trc_write(state[i++], ETMSSPCICR0);
		break;
	case 1:
		trc_write(state[i++], ETMSSCCR1);
		trc_write(state[i++], ETMSSCSR1);
		trc_write(state[i++], ETMSSPCICR1);
		break;
	case 2:
		trc_write(state[i++], ETMSSCCR2);
		trc_write(state[i++], ETMSSCSR2);
		trc_write(state[i++], ETMSSPCICR2);
		break;
	case 3:
		trc_write(state[i++], ETMSSCCR3);
		trc_write(state[i++], ETMSSCSR3);
		trc_write(state[i++], ETMSSPCICR3);
		break;
	case 4:
		trc_write(state[i++], ETMSSCCR4);
		trc_write(state[i++], ETMSSCSR4);
		trc_write(state[i++], ETMSSPCICR4);
		break;
	case 5:
		trc_write(state[i++], ETMSSCCR5);
		trc_write(state[i++], ETMSSCSR5);
		trc_write(state[i++], ETMSSPCICR5);
		break;
	case 6:
		trc_write(state[i++], ETMSSCCR6);
		trc_write(state[i++], ETMSSCSR6);
		trc_write(state[i++], ETMSSPCICR6);
		break;
	case 7:
		trc_write(state[i++], ETMSSCCR7);
		trc_write(state[i++], ETMSSCSR7);
		trc_write(state[i++], ETMSSPCICR7);
		break;
	default:
		pr_err_ratelimited("idx %d out of bounds in %s\n", j, __func__);
	}
	return i;
}

static inline void etm_si_restore_state(struct etm_ctx *etmdata)
{
	int i, j;

	i = 0;

	/* Vote for ETM power/clock enable */
	etm_clk_enable();

	switch (etmdata->arch) {
	case ETM_ARCH_V4_2:
	case ETM_ARCH_V4:
		atomic_notifier_call_chain(&etm_restore_notifier_list, 0, NULL);

		/* check OS lock is locked */
		if (BVAL(trc_readl(ETMOSLSR), 1) != 1) {
			pr_err_ratelimited("OS lock is unlocked\n");
			trc_write(0x1, ETMOSLAR);
			isb();
		}

		/* main control and configuration registers */
		trc_write(etmdata->state[i++], ETMCONFIGR);
		trc_write(etmdata->state[i++], ETMEVENTCTL0R);
		trc_write(etmdata->state[i++], ETMEVENTCTL1R);
		trc_write(etmdata->state[i++], ETMSTALLCTLR);
		trc_write(etmdata->state[i++], ETMTSCTLR);
		trc_write(etmdata->state[i++], ETMSYNCPR);
		trc_write(etmdata->state[i++], ETMCCCTLR);
		trc_write(etmdata->state[i++], ETMTRACEIDR);
		/* filtering control registers */
		trc_write(etmdata->state[i++], ETMVICTLR);
		trc_write(etmdata->state[i++], ETMVIIECTLR);
		trc_write(etmdata->state[i++], ETMVISSCTLR);
		/* derived resources registers */
		for (j = 0; j < etmdata->nr_seq_state-1; j++)
			i = etm_write_ssxr(etmdata->state, i, j);
		trc_write(etmdata->state[i++], ETMSEQRSTEVR);
		trc_write(etmdata->state[i++], ETMSEQSTR);
		trc_write(etmdata->state[i++], ETMEXTINSELR);
		for (j = 0; j < etmdata->nr_cntr; j++)
			i = etm_write_crxr(etmdata->state, i, j);
		/* resource selection registers */
		for (j = 0; j < etmdata->nr_resource; j++)
			i = etm_write_rsxr(etmdata->state, i, j + 2);
		/* comparator registers */
		for (j = 0; j < etmdata->nr_addr_cmp * 2; j++)
			i = etm_write_acr(etmdata->state, i, j);
		for (j = 0; j < etmdata->nr_data_cmp; j++)
			i = etm_write_dvcr(etmdata->state, i, j);
		for (j = 0; j < etmdata->nr_ctxid_cmp; j++)
			i = etm_write_ccvr(etmdata->state, i, j);
		trc_write(etmdata->state[i++], ETMCIDCCTLR0);
		for (j = 0; j < etmdata->nr_vmid_cmp; j++)
			i = etm_write_vcvr(etmdata->state, i, j);
		/* single-shot comparator registers */
		for (j = 0; j < etmdata->nr_ss_cmp; j++)
			i = etm_write_sscr(etmdata->state, i, j);
		/* program ctrl register */
		trc_write(etmdata->state[i++], ETMPRGCTLR);

		isb();
		trc_write(0x0, ETMOSLAR);
		break;
	default:
		pr_err_ratelimited("unsupported etm arch %d in %s\n",
				   etmdata->arch,  __func__);
	}

	/* Vote for ETM power/clock disable */
	etm_clk_disable();
}

void msm_jtag_etm_save_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	if (!etm[cpu] || etm[cpu]->save_restore_disabled)
		return;

	if (etm[cpu]->save_restore_enabled) {
		if (etm[cpu]->si_enable)
			etm_si_save_state(etm[cpu]);
		else
			etm_mm_save_state(etm[cpu]);
	}
}
EXPORT_SYMBOL(msm_jtag_etm_save_state);

void msm_jtag_etm_restore_state(void)
{
	int cpu;

	cpu = raw_smp_processor_id();

	if (!etm[cpu] || etm[cpu]->save_restore_disabled)
		return;

	/*
	 * Check to ensure we attempt to restore only when save
	 * has been done is accomplished by callee function.
	 */
	if (etm[cpu]->save_restore_enabled) {
		if (etm[cpu]->si_enable)
			etm_si_restore_state(etm[cpu]);
		else
			etm_mm_restore_state(etm[cpu]);
	}
}
EXPORT_SYMBOL(msm_jtag_etm_restore_state);

static inline bool etm_arch_supported(uint8_t arch)
{
	switch (arch) {
	case ETM_ARCH_V4_2:
	case ETM_ARCH_V4:
		break;
	default:
		return false;
	}
	return true;
}

static void etm_os_lock_init(struct etm_ctx *etmdata)
{
	uint32_t etmoslsr;

	etmoslsr = etm_readl(etmdata, TRCOSLSR);
	if ((BVAL(etmoslsr, 0) == 0)  && BVAL(etmoslsr, 3))
		etmdata->os_lock_present = true;
	else
		etmdata->os_lock_present = false;
}

static void etm_init_arch_data(void *info)
{
	uint32_t val;
	struct etm_ctx  *etmdata = info;

	ETM_UNLOCK(etmdata);

	etm_os_lock_init(etmdata);

	val = etm_readl(etmdata, TRCIDR1);
	etmdata->arch = BMVAL(val, 4, 11);

	/* number of resources trace unit supports */
	val = etm_readl(etmdata, TRCIDR4);
	etmdata->nr_addr_cmp = BMVAL(val, 0, 3);
	etmdata->nr_data_cmp = BMVAL(val, 4, 7);
	etmdata->nr_resource = BMVAL(val, 16, 19);
	etmdata->nr_ss_cmp = BMVAL(val, 20, 23);
	etmdata->nr_ctxid_cmp = BMVAL(val, 24, 27);
	etmdata->nr_vmid_cmp = BMVAL(val, 28, 31);

	val = etm_readl(etmdata, TRCIDR5);
	etmdata->nr_seq_state = BMVAL(val, 25, 27);
	etmdata->nr_cntr = BMVAL(val, 28, 30);

	ETM_LOCK(etmdata);
}

static int jtag_mm_etm_starting(unsigned int cpu)
{
	if (!etm[cpu])
		return 0;

	spin_lock(&etm[cpu]->spinlock);
	if (!etm[cpu]->init) {
		etm_init_arch_data(etm[cpu]);
		etm[cpu]->init = true;
	}
	spin_unlock(&etm[cpu]->spinlock);

	return 0;
}

static int jtag_mm_etm_online(unsigned int cpu)
{
	struct scm_desc desc = {0};
	int ret;

	if (!etm[cpu])
		return 0;

	mutex_lock(&etm[cpu]->mutex);
	if (etm[cpu]->enable) {
		mutex_unlock(&etm[cpu]->mutex);
		return 0;
	}
	if (etm_arch_supported(etm[cpu]->arch)) {
		desc.args[0] = TZ_DBG_ETM_FEAT_ID;
		desc.arginfo = SCM_ARGS(1);
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO,
				GET_FEAT_VERSION_CMD), &desc);
		if (!ret) {
			if (desc.ret[0] < TZ_DBG_ETM_VER)
				etm[cpu]->save_restore_enabled = true;
			else
				pr_info("etm save-restore supported by TZ\n");
		}
	} else
		pr_info("etm arch %u not supported\n", etm[cpu]->arch);
	etm[cpu]->enable = true;
	mutex_unlock(&etm[cpu]->mutex);

	return 0;
}

static bool skip_etm_save_restore(void)
{
	uint32_t id;
	uint32_t version;

	id = socinfo_get_id();
	version = socinfo_get_version();

	if (id == HW_SOC_ID_M8953 && SOCINFO_VERSION_MAJOR(version) == 1 &&
	    SOCINFO_VERSION_MINOR(version) == 0)
		return true;

	return false;
}

static int jtag_mm_etm_probe(struct platform_device *pdev, uint32_t cpu)
{
	struct etm_ctx *etmdata;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct scm_desc desc = {0};
	int ret;

	/* Allocate memory per cpu */
	etmdata = devm_kzalloc(dev, sizeof(struct etm_ctx), GFP_KERNEL);
	if (!etmdata)
		return -ENOMEM;


	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "etm-base");
	if (!res)
		return -ENODEV;

	etmdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!etmdata->base)
		return -EINVAL;

	etmdata->si_enable = of_property_read_bool(pdev->dev.of_node,
						   "qcom,si-enable");
	etmdata->save_restore_disabled = of_property_read_bool(
					 pdev->dev.of_node,
					 "qcom,save-restore-disable");

	if (skip_etm_save_restore())
		etmdata->save_restore_disabled = 1;

	/* Allocate etm state save space per core */
	etmdata->state = devm_kzalloc(dev,
				      MAX_ETM_STATE_SIZE * sizeof(uint64_t),
				      GFP_KERNEL);
	if (!etmdata->state)
		return -ENOMEM;

	spin_lock_init(&etmdata->spinlock);
	mutex_init(&etmdata->mutex);

	if (cnt++ == 0) {
		cpuhp_setup_state_nocalls(CPUHP_AP_ARM_MM_CORESIGHT4_STARTING,
					  "AP_ARM_CORESIGHT4_STARTING",
					  jtag_mm_etm_starting, NULL);
		ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
						"AP_ARM_CORESIGHT4_ONLINE",
						jtag_mm_etm_online, NULL);
	}

	get_online_cpus();

	if (!smp_call_function_single(cpu, etm_init_arch_data, etmdata,
				      1))
		etmdata->init = true;

	etm[cpu] = etmdata;

	put_online_cpus();

	mutex_lock(&etmdata->mutex);
	if (etmdata->init && !etmdata->enable) {
		if (etm_arch_supported(etmdata->arch)) {
			desc.args[0] = TZ_DBG_ETM_FEAT_ID;
			desc.arginfo = SCM_ARGS(1);
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO,
					GET_FEAT_VERSION_CMD), &desc);
			if (!ret) {
				if (desc.ret[0] < TZ_DBG_ETM_VER)
					etmdata->save_restore_enabled = true;
				else
					pr_info("etm save-restore supported by TZ\n");
			}
		} else
			pr_info("etm arch %u not supported\n", etmdata->arch);
		etmdata->enable = true;
	}
	mutex_unlock(&etmdata->mutex);
	return 0;
}

static int jtag_mm_probe(struct platform_device *pdev)
{
	int ret, i, cpu = -1;
	struct device *dev = &pdev->dev;
	struct device_node *cpu_node;

	cpu_node = of_parse_phandle(pdev->dev.of_node,
				    "qcom,coresight-jtagmm-cpu", 0);
	if (!cpu_node) {
		dev_err(dev, "Jtag-mm cpu handle not specified\n");
		return -ENODEV;
	}
	for_each_possible_cpu(i) {
		if (cpu_node == of_get_cpu_node(i, NULL)) {
			cpu = i;
			break;
		}
	}

	if (cpu == -1) {
		dev_err(dev, "invalid Jtag-mm cpu handle\n");
		return -EINVAL;
	}

	clock[cpu] = devm_clk_get(dev, "core_clk");
	if (IS_ERR(clock[cpu])) {
		ret = PTR_ERR(clock[cpu]);
		return ret;
	}

	ret = clk_prepare_enable(clock[cpu]);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, clock[cpu]);

	ret  = jtag_mm_etm_probe(pdev, cpu);
	if (ret)
		clk_disable_unprepare(clock[cpu]);

	return ret;
}

static void jtag_mm_etm_remove(void)
{
	cpuhp_remove_state_nocalls(CPUHP_AP_ARM_MM_CORESIGHT4_STARTING);
}

static int jtag_mm_remove(struct platform_device *pdev)
{
	struct clk *clock = platform_get_drvdata(pdev);

	if (--cnt == 0)
		jtag_mm_etm_remove();
	clk_disable_unprepare(clock);
	return 0;
}

static const struct of_device_id msm_qdss_mm_match[] = {
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
