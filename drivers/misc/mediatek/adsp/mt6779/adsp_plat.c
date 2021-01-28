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

#ifdef CONFIG_MTK_DEVINFO
#include <linux/nvmem-consumer.h>
#include <linux/of_platform.h>
#endif

#if ENABLE_ADSP_EMI_PROTECTION
#include <mt_emi_api.h>
#endif
#include "mtk_devinfo.h"

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

#if ENABLE_ADSP_EMI_PROTECTION
void set_adsp_mpu(phys_addr_t phys_addr, size_t size)
{
	struct emi_region_info_t region_info;

	region_info.start = phys_addr;
	region_info.end = phys_addr + size - 0x1;
	region_info.region = MPU_REGION_ID_ADSP_SMEM;
	SET_ACCESS_PERMISSION(region_info.apc, UNLOCK,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, NO_PROTECTION, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
	emi_mpu_set_protection(&region_info);
}
#endif

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

int adsp_get_devinfo(void)
{
	struct platform_device *pdev;
	struct device_node *node;
	struct nvmem_cell *efuse_cell;
	unsigned int *efuse_buf;
	size_t efuse_len;
	unsigned int val;

	node = of_find_compatible_node(NULL, NULL, "mediatek,audio_dsp");
	if (!node) {
		pr_err("%s cannot find node\n", __func__);
		goto ERROR;
	}
	pdev = of_device_alloc(node, NULL, NULL);
	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_segment_cell");
	if (IS_ERR(efuse_cell)) {
		pr_err("%s, cannot get efuse_cell\n", __func__);
		goto ERROR;
	}
	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		pr_err("%s, cannot get efuse_buf\n", __func__);
		goto ERROR;
	}

	val = *efuse_buf;
	kfree(efuse_buf);
	platform_device_put(pdev);

	if ((val == 0x09) || (val == 0x90) || (val == 0x08) || (val == 0x10)
	|| (val == 0x06) || (val == 0x60) || (val == 0x04) || (val == 0x20))
		adspreg.segment = ADSP_SEGMENT_P95;
	else
		adspreg.segment = ADSP_SEGMENT_P90;
	pr_info("%s val=%d segment=%d\n", __func__, val, adspreg.segment);
	return 0;

ERROR:
	return -ENODEV;
}

#define SEMAPHORE_TIMEOUT 5000
/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return  1 :get sema success
 *        -1 :get sema timeout
 */
int get_adsp_semaphore(int flag)
{
	int read_back;
	int count = 0;
	int ret = -1;

	/* return 1 to prevent from access when driver not ready */
	if (is_adsp_ready(ADSP_A_ID) != 1)
		return -1;

	flag = (flag * 2) + 1;
	read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), ADSP_SEMAPHORE);

		while (count != SEMAPHORE_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), ADSP_SEMAPHORE);
			count++;
		}

		if (ret < 0)
			pr_debug("[ADSP] get adsp sema. %d TIMEOUT..!\n", flag);
	} else
		pr_debug("[ADSP] already hold adsp sema. %d\n", flag);

	return ret;
}

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  1 :release sema success
 *        -1 :release sema fail
 */
int release_adsp_semaphore(int flag)
{
	int read_back;
	int ret = -1;

	/* return 1 to prevent from access when driver not ready */
	if (is_adsp_ready(ADSP_A_ID) != 1)
		return -1;

	flag = (flag * 2) + 1;
	read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), ADSP_SEMAPHORE);
		read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_debug("[ADSP] %s %d failed\n", __func__, flag);
	} else
		pr_debug("[ADSP] %s %d not own by me\n", __func__, flag);

	return ret;
}
