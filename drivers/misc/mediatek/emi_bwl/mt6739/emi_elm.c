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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#include "emi_elm.h"
#include "emi_bwl.h"
#include "emi_mbw.h"
#include <mtk_dramc.h>

static void __iomem *CEN_EMI_BASE; /* not initialise statics to 0 or NULL */
static bool elm_enabled;
static DEFINE_SPINLOCK(elm_lock);
static unsigned int elm_valid;
static unsigned int dram_data_rate;
static unsigned int emi_cgma_st0;
static unsigned int emi_cgma_st1;
static unsigned int emi_cgma_st2;
static unsigned int emi_ebmint_st;
static unsigned int emi_ltct0;
static unsigned int emi_ltct1;
static unsigned int emi_ltct2;
static unsigned int emi_ltct3;
static unsigned int emi_ltst0;
static unsigned int emi_ltst1;
static unsigned int emi_ltst2;
static unsigned int emi_bmen;
static unsigned int emi_bmen2;
static unsigned int emi_msel;
static unsigned int emi_msel2;
static unsigned int emi_bmrw0;
static unsigned int emi_bcnt;
static unsigned int emi_wsct;
static unsigned int emi_wsct2;
static unsigned int emi_wsct3;
static unsigned int emi_wsct4;
static unsigned int emi_ttype[16];

bool is_elm_enabled(void)
{
	return elm_enabled;
}
EXPORT_SYMBOL(is_elm_enabled);

void enable_elm(void)
{
	unsigned long flags;
	int emi_dcm_status;

	spin_lock_irqsave(&elm_lock, flags);
	emi_dcm_status = BM_GetEmiDcm();
	BM_SetEmiDcm(0x40);

	/* Setup and enable BM */
	mt_reg_sync_writel(readl(IOMEM(EMI_BMEN)) & 0xfffffffc, EMI_BMEN);
	writel(0x00ff0000, IOMEM(EMI_BMEN));
	writel(0x00020001, IOMEM(EMI_MSEL));
	writel(0x00000018, IOMEM(EMI_MSEL2));
	writel(0x02000000, IOMEM(EMI_BMEN2));
	writel(0xffffffff, IOMEM(EMI_BMRW0));
	mt_reg_sync_writel(readl(IOMEM(EMI_BMEN)) | 0x1, EMI_BMEN);

	/* Setup and enable CGM */
	writel(0x0ffffff0, IOMEM(EMI_CGMA));

	/* Enable ELM */
	writel(readl(IOMEM(EMI_LTCT0_2ND)) | 0x8, IOMEM(EMI_LTCT0_2ND));
	mt_reg_sync_writel(readl(IOMEM(EMI_LTCT0_2ND)) | 0x80000000, EMI_LTCT0_2ND);
	mt_reg_sync_writel(readl(IOMEM(EMI_LTCT0_2ND)) | 0x1, EMI_LTCT0_2ND);

	elm_enabled = true;

	BM_SetEmiDcm(emi_dcm_status);
	spin_unlock_irqrestore(&elm_lock, flags);
}
EXPORT_SYMBOL(enable_elm);

void disable_elm(void)
{
	unsigned long flags;
	int emi_dcm_status;

	spin_lock_irqsave(&elm_lock, flags);
	emi_dcm_status = BM_GetEmiDcm();
	BM_SetEmiDcm(0x40);

	/* Clear BM */
	mt_reg_sync_writel(readl(IOMEM(EMI_BMEN)) & 0xfffffffc, EMI_BMEN);

	/* Disable CGM */
	writel(0x0, IOMEM(EMI_CGMA));

	/* Disable ELM */
	writel(readl(IOMEM(EMI_LTCT0_2ND)) & ~0x1, IOMEM(EMI_LTCT0_2ND));
	mt_reg_sync_writel(readl(IOMEM(EMI_LTCT0_2ND)) & ~0x8, EMI_LTCT0_2ND);

	elm_enabled = false;

	BM_SetEmiDcm(emi_dcm_status);
	spin_unlock_irqrestore(&elm_lock, flags);
}
EXPORT_SYMBOL(disable_elm);

static void lt_init(void)
{
	/* Ask EMI DE to provide the setting of EMI_LTCT0~3,
	 * which control the period and the threshold of ELM
	 */
	writel(0x1000cc08, IOMEM(EMI_LTCT0_2ND));
	writel(0x08230823, IOMEM(EMI_LTCT1_2ND));
	writel(0x08230682, IOMEM(EMI_LTCT2_2ND));
	mt_reg_sync_writel(0x362B3636, EMI_LTCT3_2ND);

	enable_elm();
}

static int lt_reg_save(void)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&elm_lock, flags);
	dram_data_rate = get_dram_data_rate();

	emi_cgma_st0 = readl(IOMEM(EMI_CGMA_ST0));
	emi_cgma_st1 = readl(IOMEM(EMI_CGMA_ST1));
	emi_cgma_st2 = readl(IOMEM(EMI_CGMA_ST2));
	emi_ebmint_st = readl(IOMEM(EMI_EBMINT_ST));

	emi_ltct0 = readl(IOMEM(EMI_LTCT0_2ND));
	emi_ltct1 = readl(IOMEM(EMI_LTCT1_2ND));
	emi_ltct2 = readl(IOMEM(EMI_LTCT2_2ND));
	emi_ltct3 = readl(IOMEM(EMI_LTCT3_2ND));
	emi_ltst0 = readl(IOMEM(EMI_LTST0_2ND));
	emi_ltst1 = readl(IOMEM(EMI_LTST1_2ND));
	emi_ltst2 = readl(IOMEM(EMI_LTST2_2ND));

	emi_bmen = readl(IOMEM(EMI_BMEN));
	emi_bmen2 = readl(IOMEM(EMI_BMEN2));
	emi_msel = readl(IOMEM(EMI_MSEL));
	emi_msel2 = readl(IOMEM(EMI_MSEL2));
	emi_bmrw0 = readl(IOMEM(EMI_BMRW0));
	emi_bcnt = readl(IOMEM(EMI_BCNT));
	emi_wsct = readl(IOMEM(EMI_WSCT));
	emi_wsct2 = readl(IOMEM(EMI_WSCT2));
	emi_wsct3 = readl(IOMEM(EMI_WSCT3));
	emi_wsct4 = readl(IOMEM(EMI_WSCT4));

	for (i = 0; i < 16; i++)
		emi_ttype[i] = readl(IOMEM(EMI_TTYPE(i)));

	elm_valid = 1;
	spin_unlock_irqrestore(&elm_lock, flags);

#if 0
	pr_info(
		"[EMI ELM] ddr data rate: %d, valid: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n",
		dram_data_rate, elm_valid,
		"EMI_CGMA_ST0", emi_cgma_st0,
		"EMI_CGMA_ST1", emi_cgma_st1,
		"EMI_CGMA_ST2", emi_cgma_st2,
		"EMI_EBMINT_ST", emi_ebmint_st,
		"EMI_LTCT0", emi_ltct0,
		"EMI_LTCT1", emi_ltct1,
		"EMI_LTCT2", emi_ltct2,
		"EMI_LTCT3", emi_ltct3,
		"EMI_LTST0", emi_ltst0,
		"EMI_LTST1", emi_ltst1,
		"EMI_LTST2", emi_ltst2,
		"EMI_BMEN", emi_bmen,
		"EMI_BMEN2", emi_bmen2,
		"EMI_MSEL", emi_msel,
		"EMI_MSEL2", emi_msel2,
		"EMI_BMRW0", emi_bmrw0,
		"EMI_BCNT", emi_bcnt,
		"EMI_WSCT", emi_wsct,
		"EMI_WSCT2", emi_wsct2,
		"EMI_WSCT3", emi_wsct3,
		"EMI_WSCT4", emi_wsct4,
		"EMI_TTYPE1", emi_ttype[0],
		"EMI_TTYPE2", emi_ttype[1],
		"EMI_TTYPE3", emi_ttype[2],
		"EMI_TTYPE4", emi_ttype[3],
		"EMI_TTYPE5", emi_ttype[4],
		"EMI_TTYPE6", emi_ttype[5],
		"EMI_TTYPE7", emi_ttype[6],
		"EMI_TTYPE8", emi_ttype[7],
		"EMI_TTYPE9", emi_ttype[8],
		"EMI_TTYPE10", emi_ttype[9],
		"EMI_TTYPE11", emi_ttype[10],
		"EMI_TTYPE12", emi_ttype[11],
		"EMI_TTYPE13", emi_ttype[12],
		"EMI_TTYPE14", emi_ttype[13],
		"EMI_TTYPE15", emi_ttype[14],
		"EMI_TTYPE16", emi_ttype[15]
	);
#endif
	return 0;
}

void elm_dump(char *buf, unsigned int leng)
{
	unsigned long flags;
	int err;

	spin_lock_irqsave(&elm_lock, flags);
	err = snprintf(buf, leng,
		"[EMI ELM] ddr data rate: %d, valid: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n%s: 0x%08x\n"
		"%s: 0x%08x\n",
		dram_data_rate, elm_valid,
		"EMI_CGMA_ST0", emi_cgma_st0,
		"EMI_CGMA_ST1", emi_cgma_st1,
		"EMI_CGMA_ST2", emi_cgma_st2,
		"EMI_EBMINT_ST", emi_ebmint_st,
		"EMI_LTCT0", emi_ltct0,
		"EMI_LTCT1", emi_ltct1,
		"EMI_LTCT2", emi_ltct2,
		"EMI_LTCT3", emi_ltct3,
		"EMI_LTST0", emi_ltst0,
		"EMI_LTST1", emi_ltst1,
		"EMI_LTST2", emi_ltst2,
		"EMI_BMEN", emi_bmen,
		"EMI_BMEN2", emi_bmen2,
		"EMI_MSEL", emi_msel,
		"EMI_MSEL2", emi_msel2,
		"EMI_BMRW0", emi_bmrw0,
		"EMI_BCNT", emi_bcnt,
		"EMI_WSCT", emi_wsct,
		"EMI_WSCT2", emi_wsct2,
		"EMI_WSCT3", emi_wsct3,
		"EMI_WSCT4", emi_wsct4,
		"EMI_TTYPE1", emi_ttype[0],
		"EMI_TTYPE2", emi_ttype[1],
		"EMI_TTYPE3", emi_ttype[2],
		"EMI_TTYPE4", emi_ttype[3],
		"EMI_TTYPE5", emi_ttype[4],
		"EMI_TTYPE6", emi_ttype[5],
		"EMI_TTYPE7", emi_ttype[6],
		"EMI_TTYPE8", emi_ttype[7],
		"EMI_TTYPE9", emi_ttype[8],
		"EMI_TTYPE10", emi_ttype[9],
		"EMI_TTYPE11", emi_ttype[10],
		"EMI_TTYPE12", emi_ttype[11],
		"EMI_TTYPE13", emi_ttype[12],
		"EMI_TTYPE14", emi_ttype[13],
		"EMI_TTYPE15", emi_ttype[14],
		"EMI_TTYPE16", emi_ttype[15]
	);
	spin_unlock_irqrestore(&elm_lock, flags);
	if (err < 0)
		buf = NULL;
}

/* elm_irq handler */
static irqreturn_t elm_isr(int irq, void *dev_id)
{
	unsigned long flags;
	int emi_dcm_status;

	pr_info("[EMI ELM] violation\n");
	lt_reg_save();

	/* Reset counter */
	spin_lock_irqsave(&elm_lock, flags);
	emi_dcm_status = BM_GetEmiDcm();
	BM_SetEmiDcm(0x40);
	mt_reg_sync_writel(readl(IOMEM(EMI_BMEN)) & 0xfffffffc, EMI_BMEN);
	mt_reg_sync_writel(readl(IOMEM(EMI_LTCT0_2ND)) | 0x80000000, EMI_LTCT0_2ND);
	mt_reg_sync_writel(readl(IOMEM(EMI_BMEN)) | 0x1, EMI_BMEN);
	BM_SetEmiDcm(emi_dcm_status);
	spin_unlock_irqrestore(&elm_lock, flags);

	return IRQ_HANDLED;
}

static struct platform_driver elm_ctrl = {
	.driver = {
		.name = "elm_ctrl",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
	.id_table = NULL,
};

int elm_init(unsigned int elm_irq)
{
	int ret = 0;

	CEN_EMI_BASE = mt_cen_emi_base_get();

	if (elm_irq != 0) {
		ret = request_irq(elm_irq, (irq_handler_t)elm_isr, IRQF_TRIGGER_NONE, "elm", &elm_ctrl);
		if (ret != 0) {
			pr_info("Fail to request emi_bw_irqnr interrupt. Error = %d.\n", ret);
			return ret;
		}
	} else
		pr_info("[EMI] fail to register elm_irq\n");

	elm_valid = 0;
	lt_init();

	return ret;
}

void suspend_elm(void)
{
	unsigned long flags;
	int emi_dcm_status;

	spin_lock_irqsave(&elm_lock, flags);
	emi_dcm_status = BM_GetEmiDcm();
	BM_SetEmiDcm(0x40);

	/* Clear BM */
	mt_reg_sync_writel(readl(IOMEM(EMI_BMEN)) & 0xfffffffc, EMI_BMEN);

	/* Disable CGM */
	writel(0x0, IOMEM(EMI_CGMA));

	/* Disable ELM */
	writel(readl(IOMEM(EMI_LTCT0_2ND)) & ~0x1, IOMEM(EMI_LTCT0_2ND));
	mt_reg_sync_writel(readl(IOMEM(EMI_LTCT0_2ND)) & ~0x8, EMI_LTCT0_2ND);

	BM_SetEmiDcm(emi_dcm_status);
	spin_unlock_irqrestore(&elm_lock, flags);
}

void resume_elm(void)
{
	if (is_elm_enabled())
		lt_init();
}

