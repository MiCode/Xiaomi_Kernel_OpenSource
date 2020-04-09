// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <mtk_ram_console.h>
#include <asm/system_misc.h>
#include <asm/traps.h>
#include <linux/signal.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#include "../systracker_v2.h"

#ifdef CONFIG_ARM64
static int read_timeout_handler(unsigned long addr,
	unsigned int fsr, struct pt_regs *regs)
{
	int i = 0;

#ifdef SYSTRACKER_TEST_SUIT
	systracker_test_cleanup();
#endif
	/* avoid nested hit */
	if (readl(IOMEM(BUS_DBG_CON)) &
		(BUS_DBG_CON_IRQ_WP_STA | BUS_DBG_CON_IRQ_WP_RD_STA)) {
		pr_notice("%s:%d: WP_TRACKER hit\n", __func__, __LINE__);
		systracker_watchpoint_disable();
	} else
		pr_notice("%s:%d: read timeout\n", __func__, __LINE__);

	aee_dump_backtrace(regs, NULL);

	if (readl(IOMEM(BUS_DBG_CON)) &
		(BUS_DBG_CON_IRQ_AR_STA0|BUS_DBG_CON_IRQ_AR_STA1)) {
		for (i = 0; i < 32; i++) {
			pr_notice("AR_TRACKER Timeout Entry[%d]: ReadAddr:0x%x,",
			       i,
			       readl(IOMEM(BUS_DBG_AR_TRACK_L(i))));

			pr_notice("Length:0x%x, TransactionID:0x%x!\n",
			       readl(IOMEM(BUS_DBG_AR_TRACK_H(i))),
			       readl(IOMEM(BUS_DBG_AR_TRANS_TID(i))));
		}
	}

	/* return -1 to indicate kernel go on its flow */
	return -1;
}

static void write_timeout_handler(struct pt_regs *regs, void *priv)
{
	int i = 0;

#ifdef SYSTRACKER_TEST_SUIT
	systracker_test_cleanup();
#endif
	/* avoid nested hit */
	if (readl(IOMEM(BUS_DBG_CON)) &
		(BUS_DBG_CON_IRQ_WP_STA | BUS_DBG_CON_IRQ_WP_RD_STA)) {
		pr_notice("%s:%d: WP_TRACKER hit\n", __func__, __LINE__);
		systracker_watchpoint_disable();
	} else
		pr_debug("%s:%d: write timeout\n", __func__, __LINE__);

	aee_dump_backtrace(regs, NULL);

	if (readl(IOMEM(BUS_DBG_CON)) &
		((BUS_DBG_CON_IRQ_AW_STA0|BUS_DBG_CON_IRQ_AW_STA1))) {
		for (i = 0; i < 32; i++) {
			pr_notice("AW_TRACKER Timeout Entry[%d]: WriteAddr:0x%x, ",
			       i,
			       readl(IOMEM(BUS_DBG_AW_TRACK_L(i))));
			pr_notice("Length:0x%x, TransactionID:0x%x!\n",
			       readl(IOMEM(BUS_DBG_AW_TRACK_H(i))),
			       readl(IOMEM(BUS_DBG_AW_TRANS_TID(i))));
		}

		pr_notice("W_TRACK_DATA6:0x%x, W_TRACK_DATA7:0x%x, W_TRACK_DATA_VALID:0x%x!\n",
			       readl(IOMEM(BUS_DBG_W_TRACK_DATA6)),
			       readl(IOMEM(BUS_DBG_W_TRACK_DATA7)),
			       readl(IOMEM(BUS_DBG_W_TRACK_DATA_VALID)));
	}
}

static int systracker_platform_hook_fault(void)
{
	int ret = 0;

	/* We use ARM64's synchroneous external abort for read timeout */
	hook_fault_code(0x10,
			read_timeout_handler,
			SIGTRAP,
			0,
			"Systracker debug exception");

	/* for 64bit, we should register async abort handler */
	ret = register_async_abort_handler(write_timeout_handler, NULL);
	if (ret) {
		pr_notice("%s:%d: register_async_abort_handler failed\n",
			__func__,
			__LINE__);
		return -1;
	}

	return 0;
}
#else
int systracker_handler(unsigned long addr,
		       unsigned int fsr,
		       struct pt_regs *regs)
{
	int i;

#ifdef SYSTRACKER_TEST_SUIT
	writel(0x1 << 6, p1_clr);
#endif

	/* avoid nested hit */
	if (readl(IOMEM(BUS_DBG_CON)) & BUS_DBG_CON_IRQ_WP_STA) {
		pr_notice("%s:%d: WP_TRACKER hit\n", __func__, __LINE__);
		systracker_watchpoint_disable();
	}

	aee_dump_backtrace(regs, NULL);
	if (readl(IOMEM(BUS_DBG_CON)) &
		(BUS_DBG_CON_IRQ_AR_STA0|BUS_DBG_CON_IRQ_AR_STA1)) {
		for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
			pr_notice("AR_TRACKER Timeout Entry[%d]: ReadAddr:0x%x, ",
			       i,
			       readl(IOMEM(BUS_DBG_AR_TRACK_L(i))));
			pr_notice("Length:0x%x, TransactionID:0x%x!\n",
			       readl(IOMEM(BUS_DBG_AR_TRACK_H(i))),
			       readl(IOMEM(BUS_DBG_AR_TRANS_TID(i))));
		}
	}

	if (readl(IOMEM(BUS_DBG_CON)) &
		(BUS_DBG_CON_IRQ_AW_STA0|BUS_DBG_CON_IRQ_AW_STA1)) {
		for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
			pr_notice("AW_TRACKER Timeout Entry[%d]: WriteAddr:0x%x, ",
			       i,
			       readl(IOMEM(BUS_DBG_AW_TRACK_L(i))));
			pr_notice("Length:0x%x, TransactionID:0x%x!\n",
			       readl(IOMEM(BUS_DBG_AW_TRACK_H(i))),
			       readl(IOMEM(BUS_DBG_AW_TRANS_TID(i))));
		}

		pr_notice("W_TRACK_DATA6:0x%x, W_TRACK_DATA7:0x%x, W_TRACK_DATA_VALID:0x%x!\n",
			       readl(IOMEM(BUS_DBG_W_TRACK_DATA6)),
			       readl(IOMEM(BUS_DBG_W_TRACK_DATA7)),
			       readl(IOMEM(BUS_DBG_W_TRACK_DATA_VALID)));
	}

	return -1;
}

/* ARM32 version */
static int __init systracker_platform_hook_fault(void)
{

#ifdef CONFIG_ARM_LPAE
	hook_fault_code(0x10,
			systracker_handler,
			SIGTRAP,
			0,
			"Systracker debug exception");
	hook_fault_code(0x11,
			systracker_handler,
			SIGTRAP,
			0,
			"Systracker debug exception");
#else
	hook_fault_code(0x8,
			systracker_handler,
			SIGTRAP,
			0,
			"Systracker debug exception");
	hook_fault_code(0x16,
			systracker_handler,
			SIGTRAP,
			0,
			"Systracker debug exception");
#endif
	return 0;
}
#endif

#ifdef SYSTRACKER_TEST_SUIT
void __iomem *p1_set;
void __iomem *p1_clr;
void __iomem *mm_area1;
void __iomem *wp_ptr;

static int systracker_platform_test_init(void)
{
	p1_set = ioremap(0x10001250, 0x4);
	p1_clr = ioremap(0x10001e98, 0x4);

	/* use mmsys reg base for our test */
	mm_area1 = ioremap(0x13000000, 0x4);

	/* use EMI CONA for our test */
	wp_ptr = ioremap(0x10219000, 0x4);

	return 0;
}

static void systracker_platform_test_cleanup(void)
{
	writel(0x1 << 6, p1_clr);
}

static void systracker_platform_wp_test(void)
{
	unsigned int val;

	pr_info("%s:%d: we use ptr = 0x%p\n", __func__, __LINE__, wp_ptr);

	systracker_set_watchpoint_addr(0x10219000);
	systracker_set_watchpoint_mask(0x7fff);
	systracker_watchpoint_enable();

	/* touch it */
	val = readl(wp_ptr);

	pr_info("after we touched watchpoint %x\n", val);
}

static void systracker_platform_read_timeout_test(void)
{
	/* FIXME: testing
	 * track_config.enable_slave_err = 0;
	 * systracker_enable();
	 */
	void __iomem *tmp = ioremap(0x13000000, 0x4);

	pr_info("0x13000000=0x%x\n", readl(tmp));
}

static void systracker_platform_write_timeout_test(void)
{
	void __iomem *tmp = ioremap(0x13000000, 0x4);

	writel(0xdeaddead, tmp);
}

static void systracker_platform_withrecord_test(void)
{
	writel(readl(IOMEM(BUS_DBG_CON_INFRA)) &
	       ~BUS_DBG_CON_HALT_ON_EN, IOMEM(BUS_DBG_CON_INFRA));
	writel(0x1 << 6, p1_set);

	readl(mm_area1);
}

static void systracker_platform_notimeout_test(void)
{
	writel(0x1 << 6, p1_set);
	/* disable timeout */
	writel(readl(IOMEM(BUS_DBG_CON_INFRA)) &
		~(BUS_DBG_CON_TIMEOUT_EN), IOMEM(BUS_DBG_CON_INFRA));
	/* read it, should cause bus hang */
	readl(mm_area1);
	/* never come back */
	pr_notice("failed??\n");
}
#endif
/* end of SYSTRACKER_TEST_SUIT */

/*
 * mt_systracker_init: initialize driver.
 * Always return 0.
 */
static int __init mt_systracker_init(void)
{
	struct mt_systracker_driver *systracker_drv;

	systracker_drv = get_mt_systracker_drv();

	systracker_drv->systracker_hook_fault =
		systracker_platform_hook_fault;
#ifdef SYSTRACKER_TEST_SUIT
	systracker_drv->systracker_test_init =
		systracker_platform_test_init;
	systracker_drv->systracker_test_cleanup =
		systracker_platform_test_cleanup;
	systracker_drv->systracker_wp_test =
		systracker_platform_wp_test;
	systracker_drv->systracker_read_timeout_test =
		systracker_platform_read_timeout_test;
	systracker_drv->systracker_write_timeout_test =
		systracker_platform_write_timeout_test;
	systracker_drv->systracker_withrecord_test =
		systracker_platform_withrecord_test;
	systracker_drv->systracker_notimeout_test =
		systracker_platform_notimeout_test;
#endif
	return 0;
}

arch_initcall(mt_systracker_init);
MODULE_DESCRIPTION("system tracker driver v2.0");
