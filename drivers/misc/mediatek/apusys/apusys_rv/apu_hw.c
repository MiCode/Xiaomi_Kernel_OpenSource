// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include "apu.h"
#include "apu_hw.h"
#include "apu_config.h"

void apu_setup_reviser(struct mtk_apu *apu, int boundary, int ns, int domain)
{
	/*Setup IOMMU */
	iowrite32(boundary, apu->apu_sctrl_reviser + 0x300);
	iowrite32(boundary, apu->apu_sctrl_reviser + 0x304);
	iowrite32((ns << 4) | domain, apu->apu_ao_ctl + 0x10);
}

void apu_reset_mp(struct mtk_apu *apu)
{
	/* reset uP */
	iowrite32(0, apu->md32_sysctrl);
	mdelay(100);
	/* Enable IOMMU only(iommu_tr_en = 1/acp_en = 0) */
	iowrite32(0xEA9, apu->md32_sysctrl);
}

void apu_setup_boot(struct mtk_apu *apu)
{
	int boot_from_tcm;

	if (TCM_OFFSET == 0)
		boot_from_tcm = 1;
	else
		boot_from_tcm = 0;

	/* Set uP boot addr to DRAM.
	 * If boot from tcm == 1, boot addr will always map to
	 * 0x1d700000 no matter what value boot_addr is
	 */
	iowrite32((u32)DRAM_IOVA_ADDR|boot_from_tcm, apu->apu_ao_ctl + 0x4);

	/* set predefined MPU region for cache access */
	iowrite32(0xAB, apu->apu_ao_ctl);
}

void apu_start_mp(struct mtk_apu *apu)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* Release runstall */
	iowrite32(0x0, apu->apu_ao_ctl + MD32_RUNSTALL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	msleep(10); /* XXX remove it */
	for (i = 0; i < 20; i++) {
		pr_info("apu boot: pc=%08x, sp=%08x\n",
		ioread32(apu->md32_sysctrl + 0x838),
				ioread32(apu->md32_sysctrl+0x840));
		msleep(1);
	}
}

void apu_stop_mp(struct mtk_apu *apu)
{
	unsigned long flags;

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* disable apu wdt */
	iowrite32(ioread32(apu->apu_wdt + 4) &
		(~(0x1U << 31)), apu->apu_wdt + 4);
	/* clear wdt interrupt */
	iowrite32(0x1, apu->apu_wdt);
	/* Hold runstall */
	iowrite32(0x1, apu->apu_ao_ctl + MD32_RUNSTALL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
}

void apu_setup_dump(struct mtk_apu *apu, dma_addr_t da)
{
	/* Set dump addr in mbox */
	apu->conf_buf->ramdump_offset = da;

	/* Set coredump type(AP dump by default) */
	apu->conf_buf->ramdump_type = 0;
}
