// SPDX-License-Identifier: GPL-2.0
//
// adsp_plat.c--  Mediatek ADSP platform control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Celine Liu <Celine.liu@mediatek.com>

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#ifdef wakelock
#include <linux/wakelock.h>
#endif
#include <linux/io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "adsp_feature_define.h"
#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_excep.h"
#include "adsp_dvfs.h"

#include <memory/mediatek/emi.h>
/* emi mpu define */
#define MPU_PROCT_D0_AP           0
#define MPU_PROCT_D10_ADSP        10
#define MPU_PROCT_REGION_ADSP     30

#ifdef CONFIG_ARM64
#define IOMEM(a)                     ((void __force __iomem *)((a)))
#endif

#define INFRA_AXI_PROT            (adspreg.infracfg_ao + 0x0220)
#define INFRA_AXI_PROT_STA1       (adspreg.infracfg_ao + 0x0228)
#define INFRA_AXI_PROT_SET        (adspreg.infracfg_ao + 0x02A0)
#define INFRA_AXI_PROT_CLR        (adspreg.infracfg_ao + 0x02A4)
#define ADSP_AXI_PROT_MASK        (0x1 << 15)
#define ADSP_AXI_PROT_READY_MASK  (0x1 << 15)
#define ADSP_WAY_EN_CTRL          (adspreg.pericfg + 0x0240)
#define ADSP_WAY_EN_MASK          (0x1 << 13)

#define adsp_reg_read(addr)             __raw_readl(IOMEM(addr))
#define adsp_reg_sync_write(addr, val)  mt_reg_sync_writel(val, addr)

/* adsp has only 1 emimpu region in mt6779 */
void adsp_set_emimpu_region(void)
{
#if ENABLE_ADSP_EMI_PROTECTION
	struct emimpu_region_t adsp_region;
	int ret = 0;

	ret = mtk_emimpu_init_region(&adsp_region,
				     MPU_PROCT_REGION_ADSP);
	if (ret < 0)
		pr_info("%s fail to init emimpu region\n", __func__);
	mtk_emimpu_set_addr(&adsp_region, adspreg.sharedram,
		    (adspreg.sharedram + adspreg.shared_size - 0x1));
	mtk_emimpu_set_apc(&adsp_region, MPU_PROCT_D0_AP,
		   MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&adsp_region, MPU_PROCT_D10_ADSP,
		   MTK_EMIMPU_NO_PROTECTION);
	ret = mtk_emimpu_set_protection(&adsp_region);
	if (ret < 0)
		pr_info("%s fail to set emimpu protection\n", __func__);
	mtk_emimpu_free_region(&adsp_region);
#endif
}

static bool is_adsp_bus_protect_ready(void)
{
	return ((adsp_reg_read(INFRA_AXI_PROT_STA1) & ADSP_AXI_PROT_READY_MASK)
		    == ADSP_AXI_PROT_READY_MASK);
}

void adsp_bus_sleep_protect(uint32_t enable)
{
	int timeout = 1000;

	if (enable) {
		/* enable adsp bus protect */
		adsp_reg_sync_write(INFRA_AXI_PROT_SET, ADSP_AXI_PROT_MASK);
		while (--timeout && !is_adsp_bus_protect_ready())
			udelay(1);
		if (!is_adsp_bus_protect_ready())
			pr_err("%s() ready timeout\n", __func__);
	} else {
		/* disable adsp bus protect */
		adsp_reg_sync_write(INFRA_AXI_PROT_CLR, ADSP_AXI_PROT_MASK);
	}
}

void adsp_way_en_ctrl(uint32_t enable)
{
	if (enable)
		adsp_reg_sync_write(ADSP_WAY_EN_CTRL,
			adsp_reg_read(ADSP_WAY_EN_CTRL) | ADSP_WAY_EN_MASK);
	else
		adsp_reg_sync_write(ADSP_WAY_EN_CTRL,
			adsp_reg_read(ADSP_WAY_EN_CTRL) & ~ADSP_WAY_EN_MASK);
}

#define SEMAPHORE_TIMEOUT 5000
/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return  ADSP_OK: get sema success
 *         ADSP_ERROR: adsp is disabled
 *         ADSP_SEMAPHORE_BUSY: release sema fail
 */
int get_adsp_semaphore(unsigned int flag)
{
	int read_back;
	int count = 0;
	int ret = ADSP_SEMAPHORE_BUSY;

	/* return 1 to prevent from access when driver not ready */
	if (is_adsp_ready(ADSP_A_ID) != 1)
		return ADSP_ERROR;

	flag = (flag * 2) + 1;
	read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), ADSP_SEMAPHORE);

		while (count != SEMAPHORE_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

			if (read_back == 1) {
				ret = ADSP_OK;
				break;
			}
			writel((1 << flag), ADSP_SEMAPHORE);
			count++;
		}

		if (ret)
			pr_debug("[ADSP] get adsp sema. %d TIMEOUT..!\n", flag);
	} else
		pr_debug("[ADSP] already hold adsp sema. %d\n", flag);

	return ret;
}

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  ADSP_OK: release sema success
 *         ADSP_ERROR: adsp is disabled
 *         ADSP_SEMAPHORE_BUSY: release sema fail

 */
int release_adsp_semaphore(unsigned int flag)
{
	int read_back;
	int ret = ADSP_SEMAPHORE_BUSY;

	/* return 1 to prevent from access when driver not ready */
	if (is_adsp_ready(ADSP_A_ID) != 1)
		return ADSP_ERROR;

	flag = (flag * 2) + 1;
	read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), ADSP_SEMAPHORE);
		read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = ADSP_OK;
		else
			pr_debug("[ADSP] %s %d failed\n", __func__, flag);
	} else
		pr_debug("[ADSP] %s %d not own by me\n", __func__, flag);

	return ret;
}
