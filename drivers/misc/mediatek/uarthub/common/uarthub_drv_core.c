// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/regmap.h>

/*device tree mode*/
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#endif

#include <linux/interrupt.h>
#include <linux/ratelimit.h>

static atomic_t g_uarthub_probe_called = ATOMIC_INIT(0);
struct platform_device *g_uarthub_pdev;
UARTHUB_CORE_IRQ_CB g_core_irq_callback;
unsigned int g_max_dev;
int g_uarthub_open;
struct clk *clk_apmixedsys_univpll;
int g_uarthub_disable;

#define CLK_CTRL_UNIVPLL_REQ 0
#define INIT_UARTHUB_DEFAULT 0
#define UARTHUB_DEBUG_LOG 1
#define UARTHUB_CONFIG_TRX_GPIO 0
#define SUPPORT_SSPM_DRIVER 1
#define UARTHUB_ERR_IRQ_ASSERT_ENABLE 0

#if !(SUPPORT_SSPM_DRIVER)
#ifdef INIT_UARTHUB_DEFAULT
#undef INIT_UARTHUB_DEFAULT
#endif
#define INIT_UARTHUB_DEFAULT 1
#endif

struct uarthub_reg_base_addr reg_base_addr;
struct uarthub_gpio_trx_info gpio_base_addr;
struct uarthub_ops_struct *g_uarthub_plat_ic_ops;

void __iomem *cmm_base_remap_addr;
void __iomem *dev0_base_remap_addr;
void __iomem *dev1_base_remap_addr;
void __iomem *dev2_base_remap_addr;
void __iomem *intfhub_base_remap_addr;

static int mtk_uarthub_probe(struct platform_device *pdev);
static int mtk_uarthub_remove(struct platform_device *pdev);
static int uarthub_core_init(void);
static void uarthub_core_exit(void);
static irqreturn_t uarthub_irq_isr(int irq, void *arg);
static void trigger_assert_worker_handler(struct work_struct *work);

#if IS_ENABLED(CONFIG_OF)
struct uarthub_ops_struct __weak mt6886_plat_data = {};
struct uarthub_ops_struct __weak mt6983_plat_data = {};
struct uarthub_ops_struct __weak mt6985_plat_data = {};

const struct of_device_id apuarthub_of_ids[] = {
	{ .compatible = "mediatek,mt6886-uarthub", .data = &mt6886_plat_data },
	{ .compatible = "mediatek,mt6983-uarthub", .data = &mt6983_plat_data },
	{ .compatible = "mediatek,mt6985-uarthub", .data = &mt6985_plat_data },
	{}
};
#endif

struct platform_driver mtk_uarthub_dev_drv = {
	.probe = mtk_uarthub_probe,
	.remove = mtk_uarthub_remove,
	.driver = {
			.name = "mtk_uarthub",
			.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
			.of_match_table = apuarthub_of_ids,
#endif
	}
};

static struct assert_ctrl uarthub_assert_ctrl;

static int mtk_uarthub_probe(struct platform_device *pdev)
{
	pr_info("[%s] DO UARTHUB PROBE\n", __func__);

	if (pdev)
		g_uarthub_pdev = pdev;

	g_uarthub_plat_ic_ops = uarthub_core_get_platform_ic_ops(pdev);

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_init_remap_reg)
		g_uarthub_plat_ic_ops->uarthub_plat_init_remap_reg();

	atomic_set(&g_uarthub_probe_called, 1);
	return 0;
}

static int mtk_uarthub_remove(struct platform_device *pdev)
{
	pr_info("[%s] DO UARTHUB REMOVE\n", __func__);

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_deinit_unmap_reg)
		g_uarthub_plat_ic_ops->uarthub_plat_deinit_unmap_reg();

	if (g_uarthub_pdev)
		g_uarthub_pdev = NULL;

	atomic_set(&g_uarthub_probe_called, 0);
	return 0;
}

struct uarthub_ops_struct *uarthub_core_get_platform_ic_ops(struct platform_device *pdev)
{
	const struct of_device_id *of_id = NULL;

	if (!pdev) {
		pr_notice("[%s] g_uarthub_pdev is NULL\n", __func__);
		return NULL;
	}

	of_id = of_match_node(apuarthub_of_ids, pdev->dev.of_node);
	if (!of_id || !of_id->data) {
		pr_notice("[%s] failed to look up compatible string\n", __func__);
		return NULL;
	}

	return (struct uarthub_ops_struct *)of_id->data;
}

static int uarthub_core_init(void)
{
	int iRet = -1, retry = 0;

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_uarthub_disable=[%d]\n", __func__, g_uarthub_disable);
#endif

	iRet = platform_driver_register(&mtk_uarthub_dev_drv);
	if (iRet)
		pr_notice("[%s] Uarthub driver registered failed(%d)\n", __func__, iRet);
	else {
		while (atomic_read(&g_uarthub_probe_called) == 0 && retry < 100) {
			msleep(50);
			retry++;
			pr_info("[%s] g_uarthub_probe_called = 0, retry = %d\n", __func__, retry);
		}
	}

	if (!g_uarthub_pdev) {
		pr_notice("[%s] g_uarthub_pdev is NULL\n", __func__);
		goto ERROR;
	}

	uarthub_core_check_disable_from_dts(g_uarthub_pdev);

	if (g_uarthub_disable == 1)
		return 0;

	if (!g_uarthub_plat_ic_ops) {
		pr_notice("[%s] g_uarthub_plat_ic_ops is NULL\n", __func__);
		goto ERROR;
	}

	iRet = uarthub_core_get_max_dev();
	if (iRet)
		goto ERROR;

	if (g_max_dev <= 0)
		goto ERROR;

	iRet = uarthub_core_read_reg_from_dts(g_uarthub_pdev);
	if (iRet)
		goto ERROR;

#if UARTHUB_CONFIG_TRX_GPIO
	iRet = uarthub_core_config_hub_mode_gpio();
	if (iRet)
		goto ERROR;
#endif

#if CLK_CTRL_UNIVPLL_REQ
	iRet = uarthub_core_clk_get_from_dts(g_uarthub_pdev);
	if (iRet)
		goto ERROR;
#endif

	if (!reg_base_addr.vir_addr) {
		pr_notice("[%s] reg_base_addr.phy_addr(0x%lx) ioremap fail\n",
			__func__, reg_base_addr.phy_addr);
		goto ERROR;
	}

	uarthub_assert_ctrl.uarthub_workqueue = create_singlethread_workqueue("uarthub_wq");
	if (!uarthub_assert_ctrl.uarthub_workqueue) {
		pr_notice("[%s] workqueue create failed\n", __func__);
		goto ERROR;
	}

	INIT_WORK(&uarthub_assert_ctrl.trigger_assert_work, trigger_assert_worker_handler);

	cmm_base_remap_addr =
		(void __iomem *) UARTHUB_CMM_BASE_ADDR(reg_base_addr.vir_addr);
	dev0_base_remap_addr =
		(void __iomem *) UARTHUB_DEV_0_BASE_ADDR(reg_base_addr.vir_addr);
	if (g_max_dev >= 2)
		dev1_base_remap_addr =
			(void __iomem *) UARTHUB_DEV_1_BASE_ADDR(reg_base_addr.vir_addr);
	if (g_max_dev >= 3)
		dev2_base_remap_addr =
			(void __iomem *) UARTHUB_DEV_2_BASE_ADDR(reg_base_addr.vir_addr);
	intfhub_base_remap_addr =
		(void __iomem *) UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.vir_addr);

	return 0;

ERROR:
	g_uarthub_disable = 1;
	return -1;
}

static void uarthub_core_exit(void)
{
	if (g_uarthub_disable == 1)
		return;

	platform_driver_unregister(&mtk_uarthub_dev_drv);
}

static irqreturn_t uarthub_irq_isr(int irq, void *arg)
{
	int err_type = -1;

	if (uarthub_core_is_bypass_mode() == 1) {
		pr_info("[%s] ignore irq error in bypass mode\n", __func__);
		uarthub_core_irq_mask_ctrl(1);
		uarthub_core_irq_clear_ctrl();
		return IRQ_HANDLED;
	}

	uarthub_core_irq_mask_ctrl(1);
	err_type = uarthub_core_check_irq_err_type();
	if (err_type >= 0) {
		pr_info("[%s] err_type=[%d], reason=[%s]\n",
			__func__, err_type, UARTHUB_irq_err_type_str[err_type]);
		uarthub_core_set_trigger_assert_worker(err_type);
	} else {
		pr_info("[%s] err_type=[%d]\n", __func__, err_type);
		uarthub_core_irq_clear_ctrl();
		uarthub_core_irq_mask_ctrl(0);
	}

	return IRQ_HANDLED;
}

int uarthub_core_irq_mask_ctrl(int mask)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] mask=[%d]\n", __func__, mask);
#endif

	if (mask == 1) {
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
	} else {
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
			0x0, 0x3FFFF);
	}

	return 0;
}

int uarthub_core_irq_clear_ctrl(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
		0x3FFFF, 0x3FFFF);

	return 0;
}

int uarthub_core_check_irq_err_type(void)
{
	int irq_state = 0;
	int err_type = -1;
	int id = 0;

	if (g_uarthub_disable == 1)
		return -1;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	irq_state = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr));

	for (id = 0; id < 18; id++) {
		if (((irq_state >> id) & 0x1) == 0x1) {
			err_type = id;
			break;
		}
	}

	if (err_type == -1)
		pr_info("[%s] cannot find the irq error type\n", __func__);

	return err_type;
}

int uarthub_core_irq_register(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	int iret = 0;
	int irq_num = 0;
	int irq_flag = 0;

	if (g_max_dev <= 0)
		return -1;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		irq_num = irq_of_parse_and_map(node, 0);
		irq_flag = irq_get_trigger_type(irq_num);
		pr_info("[%s] get irq id(%d) and irq trigger flag(%d) from DT\n",
			__func__, irq_num, irq_flag);

		iret = request_irq(irq_num, uarthub_irq_isr, irq_flag,
			"UARTHUB_IRQ", NULL);

		if (iret) {
			pr_notice("[%s] UARTHUB IRQ(%d) not available!!\n", __func__, irq_num);
			return -1;
		}

		uarthub_core_irq_mask_ctrl(0);
	} else {
		pr_notice("[%s] can't find CONSYS compatible node\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_read_reg_from_dts(struct platform_device *pdev)
{
	if (!g_uarthub_plat_ic_ops) {
		pr_notice("[%s] g_uarthub_plat_ic_ops is NULL\n", __func__);
		return -1;
	}

	if (!g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_addr_info) {
		pr_notice("[%s] uarthub_plat_get_uarthub_addr_info is NULL\n", __func__);
		return -1;
	}

	return g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_addr_info(&reg_base_addr);
}

int uarthub_core_check_disable_from_dts(struct platform_device *pdev)
{
	int uarthub_disable = 0;
	struct device_node *node = NULL;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		if (of_property_read_u32(node, "uarthub_disable", &uarthub_disable)) {
			pr_notice("[%s] unable to get uarthub_disable from dts\n", __func__);
			return -1;
		}
		pr_info("[%s] Get uarthub_disable(%d)\n", __func__, uarthub_disable);
		g_uarthub_disable = ((uarthub_disable == 0) ? 0 : 1);
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_get_max_dev(void)
{
	if (!g_uarthub_plat_ic_ops) {
		pr_notice("[%s] g_uarthub_plat_ic_ops is NULL\n", __func__);
		return -1;
	}

	if (!g_uarthub_plat_ic_ops->uarthub_plat_get_max_num_dev_host) {
		pr_notice("[%s] uarthub_plat_get_max_num_dev_host is NULL\n", __func__);
		return -1;
	}

	g_max_dev = g_uarthub_plat_ic_ops->uarthub_plat_get_max_num_dev_host();
	pr_info("[%s] Get uarthub max dev(%d)\n", __func__, g_max_dev);

	return 0;
}

int uarthub_core_get_default_baud_rate(int dev_index)
{
	int baud_rate = 0;

	if (g_max_dev <= 0)
		return -1;

	if (dev_index < 0 || dev_index > g_max_dev) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return -1;
	}

	if (!g_uarthub_plat_ic_ops) {
		pr_notice("[%s] g_uarthub_plat_ic_ops is NULL\n", __func__);
		return -1;
	}

	if (!g_uarthub_plat_ic_ops->uarthub_plat_get_default_baud_rate) {
		pr_notice("[%s] uarthub_plat_get_default_baud_rate is NULL\n", __func__);
		return -1;
	}

	baud_rate = g_uarthub_plat_ic_ops->uarthub_plat_get_default_baud_rate(dev_index);
	pr_info("[%s] Get uarthub baud rate, index=[%d], baud_rate=[%d]\n",
		__func__, dev_index, baud_rate);

	return baud_rate;
}

int uarthub_core_clk_get_from_dts(struct platform_device *pdev)
{
	clk_apmixedsys_univpll = devm_clk_get(&pdev->dev, "univpll");
	if (IS_ERR(clk_apmixedsys_univpll)) {
		pr_notice("[%s] cannot get clk_apmixedsys_univpll clock.\n", __func__);
		return PTR_ERR(clk_apmixedsys_univpll);
	}
	pr_info("[%s] clk_apmixedsys_univpll=[%p]\n", __func__, clk_apmixedsys_univpll);

	return 0;
}

int uarthub_core_config_hub_mode_gpio(void)
{
	int iRtn = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (!g_uarthub_plat_ic_ops) {
		pr_notice("[%s] g_uarthub_plat_ic_ops is NULL\n", __func__);
		return -1;
	}

	if (!g_uarthub_plat_ic_ops->uarthub_plat_config_gpio_trx) {
		pr_notice("[%s] uarthub_plat_config_gpio_trx is NULL\n", __func__);
		return -1;
	}

	iRtn = g_uarthub_plat_ic_ops->uarthub_plat_config_gpio_trx();

	if (iRtn < 0) {
		pr_notice("[%s] config GPIO hub mode trx fail\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_open(void)
{
	int iRet = 0;
#if INIT_UARTHUB_DEFAULT
	void __iomem *uarthub_dev_base = dev0_base_remap_addr;
	int baud_rate = -1;
	int i = 0;
#endif

	if (g_uarthub_disable == 1) {
		pr_info("[%s] g_uarthub_disable=[1]\n", __func__);
		return 0;
	}

	if (g_max_dev <= 0)
		return -1;

#if UARTHUB_DEBUG_LOG

#if CLK_CTRL_UNIVPLL_REQ
	pr_info("[%s] CLK_CTRL_UNIVPLL_REQ=[1]\n", __func__);
#else
	pr_info("[%s] CLK_CTRL_UNIVPLL_REQ=[0]\n", __func__);
#endif

#if INIT_UARTHUB_DEFAULT
	pr_info("[%s] INIT_UARTHUB_DEFAULT=[1]\n", __func__);
#else
	pr_info("[%s] INIT_UARTHUB_DEFAULT=[0]\n", __func__);
#endif

#if SUPPORT_SSPM_DRIVER
	pr_info("[%s] SUPPORT_SSPM_DRIVER=[1]\n", __func__);
#else
	pr_info("[%s] SUPPORT_SSPM_DRIVER=[0]\n", __func__);
#endif

#if UARTHUB_ERR_IRQ_ASSERT_ENABLE
	pr_info("[%s] UARTHUB_ERR_IRQ_ASSERT_ENABLE=[1]\n", __func__);
#else
	pr_info("[%s] UARTHUB_ERR_IRQ_ASSERT_ENABLE=[0]\n", __func__);
#endif

#endif

#if CLK_CTRL_UNIVPLL_REQ
	uarthub_core_clk_univpll_ctrl(1);
#endif

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if INIT_UARTHUB_DEFAULT
	for (i = 0; i <= g_max_dev; i++) {
		if (i != g_max_dev) {
			if (i == 0)
				uarthub_dev_base = dev0_base_remap_addr;
			else if (i == 1)
				uarthub_dev_base = dev1_base_remap_addr;
			else if (i == 2)
				uarthub_dev_base = dev2_base_remap_addr;
		} else
			uarthub_dev_base = cmm_base_remap_addr;

		baud_rate = uarthub_core_get_default_baud_rate(i);
		if (baud_rate >= 0)
			uarthub_core_config_baud_rate(uarthub_dev_base, baud_rate);

		/* 0x4c = 0x3,  rx/tx channel dma enable */
		UARTHUB_REG_WRITE(UARTHUB_DMA_EN(uarthub_dev_base), 0x3);
		/* 0x08 = 0x87, fifo control register */
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(uarthub_dev_base), 0x87);
		/* 0x0c = 0x3,  byte length: 8 bit*/
		UARTHUB_REG_WRITE(UARTHUB_LCR(uarthub_dev_base), 0x3);
		/* 0x98 = 0xa,  xon1/xoff1 flow control enable */
		UARTHUB_REG_WRITE(UARTHUB_EFR(uarthub_dev_base), 0xa);
		/* 0xa8 = 0x13, xoff1 keyword */
		UARTHUB_REG_WRITE(UARTHUB_XOFF1(uarthub_dev_base), 0x13);
		/* 0xa0 = 0x11, xon1 keyword */
		UARTHUB_REG_WRITE(UARTHUB_XON1(uarthub_dev_base), 0x11);
		/* 0xac = 0x13, xoff2 keyword */
		UARTHUB_REG_WRITE(UARTHUB_XOFF2(uarthub_dev_base), 0x13);
		/* 0xa4 = 0x11, xon2 keyword */
		UARTHUB_REG_WRITE(UARTHUB_XON2(uarthub_dev_base), 0x11);
		/* 0x44 = 0x1,  esc char enable */
		UARTHUB_REG_WRITE(UARTHUB_ESCAPE_EN(uarthub_dev_base), 0x1);
		/* 0x40 = 0xdb, esc char */
		UARTHUB_REG_WRITE(UARTHUB_ESCAPE_DAT(uarthub_dev_base), 0xdb);

		if (i != g_max_dev)
			pr_info("[%s] UARTHUB_DEV_%d OPEN done.\n", __func__, i);
		else
			pr_info("[%s] UARTHUB_DEV_CMM OPEN done.\n", __func__);
	}

	g_uarthub_open = 1;

	uarthub_core_bypass_mode_ctrl(1)
	uarthub_core_crc_ctrl(1);
	uarthub_core_debug_info_with_tag(__func__);
#else
	g_uarthub_open = 1;
#endif

	iRet = uarthub_core_irq_register(g_uarthub_pdev);
	if (iRet)
		return -1;

	return 0;
}

int uarthub_core_close(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	uarthub_core_irq_mask_ctrl(1);
	uarthub_core_irq_clear_ctrl();

#if CLK_CTRL_UNIVPLL_REQ
	uarthub_core_clk_univpll_ctrl(0);
#endif
	return 0;
}

int uarthub_core_dev0_is_uarthub_ready(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr),
		(0x1 << 9)) >> 9);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	if (state == 1)
		uarthub_core_debug_info_with_tag(__func__);

	return (state == 1) ? 1 : 0;
}

int uarthub_core_dev0_is_txrx_idle(int rx)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	if (rx == 1) {
		state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr),
			(0x1 << 0)) >> 0);
	} else {
		state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr),
			(0x1 << 1)) >> 1);
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] rx=[%d], state=[%d]\n", __func__, rx, state);
#endif

	return (state == 0) ? 1 : 0;
}

int uarthub_core_dev0_set_txrx_request(void)
{
	int retry = 0;
	int val = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

#if UARTHUB_DEBUG_LOG
	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info) {
		pr_info("[%s] hw_ccf_pll_done before set trx req, state=[%d]\n", __func__,
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info());
	}
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_SET(intfhub_base_remap_addr), 0x3);

#if !(SUPPORT_SSPM_DRIVER)
	UARTHUB_SET_BIT(UARTHUB_INTFHUB_IRQ_CLR(intfhub_base_remap_addr), (0x1 << 0));
	pr_info("[%s] is_ready=[%d]\n", __func__, uarthub_core_dev0_is_uarthub_ready());
#if UARTHUB_DEBUG_LOG
	uarthub_core_debug_info_with_tag(__func__);
#endif
#endif

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info) {
		retry = 20;
		while (retry-- > 0) {
			val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info();
			if (val == 1) {
				pr_info("[%s] hw_ccf_pll_done pass, retry=[%d]\n",
					__func__, retry);
				break;
			}
			usleep_range(1000, 1100);
		}

		if (val == 0) {
			pr_notice("[%s] hw_ccf_pll_done fail, retry=[%d]\n",
				__func__, retry);
		} else if (val < 0) {
			pr_notice("[%s] hw_ccf_pll_done info cannot be read, retry=[%d]\n",
				__func__, retry);
		}
	}

	return 0;
}

int uarthub_core_dev0_clear_txrx_request(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_CLR(intfhub_base_remap_addr), 0x7);

	return 0;
}

int uarthub_core_is_assert_state(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr),
		(0x1 << 0)) >> 0);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return (state == 1) ? 1 : 0;
}

int uarthub_core_irq_register_cb(UARTHUB_CORE_IRQ_CB irq_callback)
{
	if (g_uarthub_disable == 1)
		return 0;

	g_core_irq_callback = irq_callback;
	return 0;
}

int uarthub_core_clk_univpll_ctrl(int clk_on)
{
	int iRet = 0;

	if (g_max_dev <= 0)
		return -1;

	if (clk_on == 1) {
		iRet = clk_prepare_enable(clk_apmixedsys_univpll);
		usleep_range(5000, 6000);
		if (iRet) {
			pr_notice("[%s] clk_prepare_enable(clk_apmixedsys_univpll) fail(%d)\n",
				__func__, iRet);
			return iRet;
		}
		pr_info("[%s] clk_prepare_enable(clk_apmixedsys_univpll) ok\n", __func__);
	} else {
		clk_disable_unprepare(clk_apmixedsys_univpll);
		pr_info("[%s] clk_disable_unprepare(clk_apmixedsys_univpll) calling\n", __func__);
	}

	return 0;
}

int uarthub_core_crc_ctrl(int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1)
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 1));
	else
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 1));

	return 0;
}

int uarthub_core_bypass_mode_ctrl(int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1) {
		uarthub_core_irq_mask_ctrl(1);
		uarthub_core_irq_clear_ctrl();
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 2));
	} else {
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 2));
		uarthub_core_irq_clear_ctrl();
		uarthub_core_irq_mask_ctrl(0);
	}

	return 0;
}

int uarthub_core_md_adsp_fifo_ctrl(int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1) {
		uarthub_core_reset_to_ap_enable_only(1);
	} else {
		if (g_max_dev >= 2) {
			UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev1_base_remap_addr),
				((UARTHUB_REG_READ(UARTHUB_FCR_RD(
				dev1_base_remap_addr)) & (~(0x1))) | (0x1)));
		}

		if (g_max_dev >= 3) {
			UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev2_base_remap_addr),
				((UARTHUB_REG_READ(UARTHUB_FCR_RD(
				dev2_base_remap_addr)) & (~(0x1))) | (0x1)));
		}
	}

	return 0;
}

int uarthub_core_is_bypass_mode(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr),
		(0x1 << 2)) >> 2);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return (state == 1) ? 1 : 0;
}

int uarthub_core_rx_error_crc_info(int dev_index, int *p_crc_error_data, int *p_crc_result)
{
	unsigned int crc_err_reg_value = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (p_crc_error_data == NULL && p_crc_result == NULL)
		return -2;

	if (dev_index < 0 || dev_index >= g_max_dev) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return -3;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	if (dev_index == 0) {
		crc_err_reg_value = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_RX_ERR_CRC_STA(
			intfhub_base_remap_addr));
	} else if (dev_index == 1) {
		crc_err_reg_value = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_RX_ERR_CRC_STA(
			intfhub_base_remap_addr));
	} else if (dev_index == 2) {
		crc_err_reg_value = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_RX_ERR_CRC_STA(
			intfhub_base_remap_addr));
	}

	if (p_crc_error_data)
		*p_crc_error_data = ((crc_err_reg_value >> 0) & 0xffff);
	if (p_crc_result)
		*p_crc_result = ((crc_err_reg_value >> 16) & 0xffff);

#if UARTHUB_DEBUG_LOG
	if (p_crc_error_data && p_crc_result) {
		pr_info("[%s] dev_index=[%d], crc_error_data=[%d], crc_result=[%d]\n",
			__func__, dev_index, *p_crc_error_data, *p_crc_result);
	} else if (p_crc_error_data) {
		pr_info("[%s] dev_index=[%d], crc_error_data=[%d]\n",
			__func__, dev_index, *p_crc_error_data);
	} else if (p_crc_result) {
		pr_info("[%s] dev_index=[%d], crc_result=[%d]\n",
			__func__, dev_index, *p_crc_result);
	} else
		pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif

	return 0;
}

int uarthub_core_timeout_info(int dev_index, int rx,
	int *p_timeout_counter, int *p_pkt_counter)
{
	unsigned int timeout_reg_value = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (p_timeout_counter == NULL && p_pkt_counter == NULL)
		return -2;

	if (dev_index < 0 || dev_index >= g_max_dev) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return -3;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	if (dev_index == 0) {
		timeout_reg_value =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr));
	} else if (dev_index == 1) {
		timeout_reg_value =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(intfhub_base_remap_addr));
	} else if (dev_index == 2) {
		timeout_reg_value =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(intfhub_base_remap_addr));
	}

	if (rx == 1) {
		if (p_timeout_counter)
			*p_timeout_counter = ((timeout_reg_value >> 0) & 0xff);
		if (p_pkt_counter)
			*p_pkt_counter = ((timeout_reg_value >> 8) & 0xff);
	} else {
		if (p_timeout_counter)
			*p_timeout_counter = ((timeout_reg_value >> 16) & 0xff);
		if (p_pkt_counter)
			*p_pkt_counter = ((timeout_reg_value >> 24) & 0xff);
	}

#if UARTHUB_DEBUG_LOG
	if (p_timeout_counter && p_pkt_counter) {
		pr_info("[%s] dev_index=[%d], rx=[%d], timeout_counter=[%d], pkt_counter=[%d]\n",
			__func__, dev_index, rx, *p_timeout_counter, *p_pkt_counter);
	} else if (p_timeout_counter) {
		pr_info("[%s] dev_index=[%d], rx=[%d], timeout_counter=[%d]\n",
			__func__, dev_index, rx, *p_timeout_counter);
	} else if (p_pkt_counter) {
		pr_info("[%s] dev_index=[%d], rx=[%d], pkt_counter=[%d]\n",
			__func__, dev_index, rx, *p_pkt_counter);
	} else
		pr_info("[%s] dev_index=[%d], rx=[%d]\n", __func__, dev_index, rx);
#endif

	return 0;
}

int uarthub_core_config_baud_rate(void __iomem *uarthub_dev_base, int rate_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!uarthub_dev_base) {
		pr_notice("[%s] uarthub_dev_base(0x%lx) is not been init\n",
			__func__, (unsigned long) uarthub_dev_base);
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] rate_index=[%d]\n", __func__, rate_index);
#endif

	if (rate_index == (int)baud_rate_115200) {
		UARTHUB_REG_WRITE(UARTHUB_FEATURE_SEL(uarthub_dev_base), 0x1);  /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(UARTHUB_HIGHSPEED(uarthub_dev_base), 0x3);    /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_COUNT(uarthub_dev_base), 0xe0);/* 0x28 = 0xe0 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_POINT(uarthub_dev_base), 0x70);/* 0x2c = 0x70 */
		UARTHUB_REG_WRITE(UARTHUB_DLL(uarthub_dev_base), 0x1);          /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_L(uarthub_dev_base), 0xf6);   /* 0x54 = 0xf6 */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_M(uarthub_dev_base), 0x1);    /* 0x58 = 0x1  */
	} else if (rate_index == (int)baud_rate_3m) {
		UARTHUB_REG_WRITE(UARTHUB_FEATURE_SEL(uarthub_dev_base), 0x1);  /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(UARTHUB_HIGHSPEED(uarthub_dev_base), 0x3);    /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_COUNT(uarthub_dev_base), 0x21);/* 0x28 = 0x21 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_POINT(uarthub_dev_base), 0x11);/* 0x2c = 0x11 */
		UARTHUB_REG_WRITE(UARTHUB_DLL(uarthub_dev_base), 0x1);          /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_L(uarthub_dev_base), 0xdb);   /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_M(uarthub_dev_base), 0x1);    /* 0x58 = 0x1  */
	} else if (rate_index == (int)baud_rate_4m) {
		UARTHUB_REG_WRITE(UARTHUB_FEATURE_SEL(uarthub_dev_base), 0x1);  /* 0x9c = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_HIGHSPEED(uarthub_dev_base), 0x3);    /* 0x24 = 0x3 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_COUNT(uarthub_dev_base), 0x19);/* 0x28 = 0x19 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_POINT(uarthub_dev_base), 0xd); /* 0x2c = 0xd  */
		UARTHUB_REG_WRITE(UARTHUB_DLL(uarthub_dev_base), 0x1);          /* 0x90 = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_L(uarthub_dev_base), 0x0);    /* 0x54 = 0x0  */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_M(uarthub_dev_base), 0x0);    /* 0x58 = 0x0  */
	} else if (rate_index == (int)baud_rate_12m) {
		UARTHUB_REG_WRITE(UARTHUB_FEATURE_SEL(uarthub_dev_base), 0x1);  /* 0x9c = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_HIGHSPEED(uarthub_dev_base), 0x3);    /* 0x24 = 0x3 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_COUNT(uarthub_dev_base), 0x7); /* 0x28 = 0x7  */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_POINT(uarthub_dev_base), 0x4); /* 0x2c = 0x4  */
		UARTHUB_REG_WRITE(UARTHUB_DLL(uarthub_dev_base), 0x1);          /* 0x90 = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_L(uarthub_dev_base), 0xdb);   /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_M(uarthub_dev_base), 0x1);    /* 0x58 = 0x1  */
	} else if (rate_index == (int)baud_rate_24m) {
		/* TODO: support 24M baud rate */
	} else {
		pr_notice("[%s] not support rate_index(%d)\n", __func__, rate_index);
		return -2;
	}

	return 0;
}

int uarthub_core_config_internal_baud_rate(int dev_index, int rate_index)
{
	void __iomem *uarthub_dev_base = dev0_base_remap_addr;
	int iRtn = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!reg_base_addr.vir_addr) {
		pr_notice("[%s] reg_base_addr.phy_addr(0x%lx) is not been init\n",
			__func__, reg_base_addr.phy_addr);
		return -1;
	}

	if (dev_index < 0 || dev_index >= g_max_dev) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return -3;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] dev_index=[%d], rate_index=[%d]\n",
		__func__, dev_index, rate_index);
#endif

	if (dev_index == 0)
		uarthub_dev_base = dev0_base_remap_addr;
	else if (dev_index == 1)
		uarthub_dev_base = dev1_base_remap_addr;
	else if (dev_index == 2)
		uarthub_dev_base = dev2_base_remap_addr;

	iRtn = uarthub_core_config_baud_rate(uarthub_dev_base, rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config internal baud rate fail(%d), dev_index=[%d], rate_index=[%d]\n",
			__func__, iRtn, dev_index, rate_index);
		return -3;
	}

	return 0;
}

int uarthub_core_config_external_baud_rate(int rate_index)
{
	int iRtn = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!cmm_base_remap_addr) {
		pr_notice("[%s] cmm_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_CMM_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] rate_index=[%d]\n", __func__, rate_index);
#endif

	iRtn = uarthub_core_config_baud_rate(cmm_base_remap_addr, rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config external baud rate fail(%d), rate_indexe=[%d]\n",
			__func__, iRtn, rate_index);
		return -2;
	}

	return 0;
}

void uarthub_core_set_trigger_assert_worker(int err_type)
{
	uarthub_assert_ctrl.err_type = err_type;
	queue_work(uarthub_assert_ctrl.uarthub_workqueue, &uarthub_assert_ctrl.trigger_assert_work);
}

static void trigger_assert_worker_handler(struct work_struct *work)
{
	struct assert_ctrl *queue = container_of(work, struct assert_ctrl, trigger_assert_work);
	int err_type = (int) queue->err_type;

	pr_info("[%s] err_type=[%d], reason=[%s], g_core_irq_callback=[%p]\n",
		__func__, err_type, UARTHUB_irq_err_type_str[err_type], g_core_irq_callback);

#if UARTHUB_ERR_IRQ_ASSERT_ENABLE
	uarthub_core_assert_state_ctrl(1);
#else
	uarthub_core_debug_info_with_tag(__func__);
#endif

	if (g_core_irq_callback)
		(*g_core_irq_callback)(err_type);

	uarthub_core_irq_clear_ctrl();
	uarthub_core_irq_mask_ctrl(0);
}

int uarthub_core_assert_state_ctrl(int assert_ctrl)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	if (assert_ctrl == uarthub_core_is_assert_state()) {
		if (assert_ctrl == 1)
			pr_info("[%s] assert state has been set\n", __func__);
		else
			pr_info("[%s] assert state has been cleared\n", __func__);
		return 0;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] assert_ctrl=[%d]\n", __func__, assert_ctrl);
#endif

	if (assert_ctrl == 1) {
		uarthub_core_reset_to_ap_enable_only(1);
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 0));
		uarthub_core_debug_info_with_tag(__func__);
	} else {
		if (g_max_dev >= 2) {
			UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev1_base_remap_addr),
				((UARTHUB_REG_READ(UARTHUB_FCR_RD(
				dev1_base_remap_addr)) & (~(0x1))) | (0x1)));
		}

		if (g_max_dev >= 3) {
			UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev2_base_remap_addr),
				((UARTHUB_REG_READ(UARTHUB_FCR_RD(
				dev2_base_remap_addr)) & (~(0x1))) | (0x1)));
		}

		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 0));
	}

	return 0;
}

int uarthub_core_reset(void)
{
#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif
	return uarthub_core_reset_to_ap_enable_only(0);
}

int uarthub_core_reset_to_ap_enable_only(int ap_only)
{
	int tx_state = -1;
	int dev1_fifoe = -1, dev2_fifoe = -1;
	int i = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	if (g_max_dev >= 2) {
		dev1_fifoe = (UARTHUB_REG_READ_BIT(UARTHUB_FCR_RD(dev1_base_remap_addr),
			(0x1 << 0)) >> 0);
	}

	if (g_max_dev >= 3) {
		dev2_fifoe = (UARTHUB_REG_READ_BIT(UARTHUB_FCR_RD(dev2_base_remap_addr),
			(0x1 << 0)) >> 0);
	}

	tx_state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr),
		(0x1 << 1)) >> 1);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d], dev1_fifoe=[%d], dev2_fifoe=[%d], tx_state=[%d]\n",
		__func__, g_max_dev, dev1_fifoe, dev2_fifoe, tx_state);
#endif

	/* set tx request */
	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_SET(intfhub_base_remap_addr),
		((UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(
		intfhub_base_remap_addr)) & (~(0x10))) | (0x10)));

	/* disable and clear uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(cmm_base_remap_addr),
		(UARTHUB_REG_READ(UARTHUB_FCR_RD(cmm_base_remap_addr)) & (~(0x1))));

	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev0_base_remap_addr),
		(UARTHUB_REG_READ(UARTHUB_FCR_RD(dev0_base_remap_addr)) & (~(0x1))));

	if (g_max_dev >= 2) {
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev1_base_remap_addr),
			(UARTHUB_REG_READ(UARTHUB_FCR_RD(dev1_base_remap_addr)) & (~(0x1))));
	}

	if (g_max_dev >= 3) {
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev2_base_remap_addr),
			(UARTHUB_REG_READ(UARTHUB_FCR_RD(dev2_base_remap_addr)) & (~(0x1))));
	}

	/* sw_rst3 4 times */
	for (i = 0; i < 4; i++) {
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_CON4(intfhub_base_remap_addr), (0x1 << 3));
		usleep_range(5, 6);
	}

	/* enable uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(cmm_base_remap_addr),
		((UARTHUB_REG_READ(UARTHUB_FCR_RD(
		cmm_base_remap_addr)) & (~(0x1))) | (0x1)));

	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev0_base_remap_addr),
		((UARTHUB_REG_READ(UARTHUB_FCR_RD(
		dev0_base_remap_addr)) & (~(0x1))) | (0x1)));

	if (g_max_dev >= 2 && dev1_fifoe == 1 && ap_only == 0) {
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev1_base_remap_addr),
			((UARTHUB_REG_READ(UARTHUB_FCR_RD(
			dev1_base_remap_addr)) & (~(0x1))) | (0x1)));
	}

	if (g_max_dev >= 3 && dev2_fifoe == 1 && ap_only == 0) {
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev2_base_remap_addr),
			((UARTHUB_REG_READ(UARTHUB_FCR_RD(
			dev2_base_remap_addr)) & (~(0x1))) | (0x1)));
	}

	if (tx_state == 0) {
		/* clear tx request */
		UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_CLR(intfhub_base_remap_addr),
			((UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(
			intfhub_base_remap_addr)) & (~(0x10))) | (0x10)));
	}

	return 0;
}

int uarthub_core_loopback_test(int dev_index, int tx_to_rx, int enable)
{
	int offset = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!intfhub_base_remap_addr) {
		pr_notice("[%s] intfhub_base_remap_addr(0x%lx) is not been init\n",
			__func__, UARTHUB_INTFHUB_BASE_ADDR(reg_base_addr.phy_addr));
		return -1;
	}

	if (dev_index < 0 || dev_index >= g_max_dev) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return -2;
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -4;
	}

	offset = ((dev_index + 1) * 2) - 1;
	offset = (tx_to_rx == 0) ? offset : (offset - 1);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] dev_index=[%d], tx_to_rx=[%d], enable=[%d], offset=[%d]\n",
		__func__, dev_index, tx_to_rx, enable, offset);
#endif

	if (enable == 0)
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_LOOPBACK(intfhub_base_remap_addr), (0x1 << offset));
	else
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_LOOPBACK(intfhub_base_remap_addr), (0x1 << offset));

	return 0;
}

int uarthub_core_is_apb_bus_clk_enable(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	state = UARTHUB_REG_READ_BIT(
		UARTHUB_INTFHUB_CON1(intfhub_base_remap_addr), 0xFFFF);

	return (state == 0x8581) ? 1 : 0;
}

int uarthub_core_debug_info(void)
{
	return uarthub_core_debug_info_with_tag(NULL);
}

int uarthub_core_debug_info_with_tag(const char *tag)
{
	int val = 0;
	int apb_bus_clk_enable = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
		__func__, ((tag == NULL) ? "null" : tag));

	apb_bus_clk_enable = uarthub_core_is_apb_bus_clk_enable();
	pr_info("[%s][%s] APB BUS CLK=[0x%x]\n", __func__,
		((tag == NULL) ? "null" : tag), apb_bus_clk_enable);

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_gating_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_gating_info();
		if (val >= 0) {
			pr_info("[%s][%s] UARTHUB CLK GATING=[0x%x]\n",
				__func__, ((tag == NULL) ? "null" : tag), val);
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info();
		if (val >= 0) {
			pr_info("[%s][%s] UNIVPLL CLK ON=[0x%x]\n",
				__func__, ((tag == NULL) ? "null" : tag), val);
		}
	}

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info();
		if (val >= 0) {
			pr_info("[%s][%s] UART MUX=[0x%x]\n",
				__func__, ((tag == NULL) ? "null" : tag), val);
		}
	}

	if (apb_bus_clk_enable == 0) {
		pr_info("[%s][%s] ----------------------------------------\n",
			__func__, ((tag == NULL) ? "null" : tag));
		return -2;
	}

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info(&gpio_base_addr);
		if (val == 0) {
			pr_info("[%s][%s] GPIO TX=[phy_addr:0x%lx, mask:0x%lx, exp_value:0x%lx, read_value:0x%lx]\n",
				__func__, ((tag == NULL) ? "null" : tag),
				gpio_base_addr.gpio_tx.addr, gpio_base_addr.gpio_tx.mask,
				gpio_base_addr.gpio_tx.value, gpio_base_addr.gpio_tx.gpio_value);

			pr_info("[%s][%s] GPIO RX=[phy_addr:0x%lx, mask:0x%lx, exp_value:0x%lx, read_value:0x%lx]\n",
				__func__, ((tag == NULL) ? "null" : tag),
				gpio_base_addr.gpio_rx.addr, gpio_base_addr.gpio_rx.mask,
				gpio_base_addr.gpio_rx.value, gpio_base_addr.gpio_rx.gpio_value);
		}
	}

	pr_info("[%s][%s] 0x9c,FEATURE_SEL=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x24,HIGHSPEED=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_HIGHSPEED(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_HIGHSPEED(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_HIGHSPEED(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_HIGHSPEED(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x28,SAMPLE_COUNT=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x2c,SAMPLE_POINT=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x90,DLL=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_DLL(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DLL(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DLL(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DLL(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x54,FRACDIV_L=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_FRACDIV_L(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_FRACDIV_L(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_FRACDIV_L(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FRACDIV_L(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x58,FRACDIV_M=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_FRACDIV_M(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_FRACDIV_M(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_FRACDIV_M(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FRACDIV_M(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x4c,DMA_EN=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_DMA_EN(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DMA_EN(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DMA_EN(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DMA_EN(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0xc,LCR=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_LCR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_LCR(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_LCR(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_LCR(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x98,EFR=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_EFR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_EFR(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_EFR(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_EFR(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0xa8,XOFF1=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_XOFF1(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_XOFF1(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_XOFF1(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XOFF1(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0xa0,XON1=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_XON1(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_XON1(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_XON1(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XON1(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0xac,XOFF2=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_XOFF2(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_XOFF2(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_XOFF2(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XOFF2(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0xa4,XON2=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_XON2(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_XON2(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_XON2(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XON2(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x44,ESCAPE_EN=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x40,ESCAPE_DAT=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x70,TX_FIFO_OFFSET=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_TX_FIFO_OFFSET(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_TX_FIFO_OFFSET(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_TX_FIFO_OFFSET(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_TX_FIFO_OFFSET(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x7c,RX_FIFO_OFFSET=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_RX_FIFO_OFFSET(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_RX_FIFO_OFFSET(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_RX_FIFO_OFFSET(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_RX_FIFO_OFFSET(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x5c,FCR_RD=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_FCR_RD(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_FCR_RD(
			dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_FCR_RD(
			dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FCR_RD(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x10,MCR=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_MCR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_MCR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_MCR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_MCR(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x14,LSR(rx:0,tx:6)=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_LSR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_LSR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_LSR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_LSR(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x64,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"xcstete,txstate",
		UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x68,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"ip_tx_dma[3:0],rxstate",
		UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_2(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x6c,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"rx_offset_dma,ip_tx_dma[5:4]",
		UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_3(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x70,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"tx_roffset[1:0],tx_woffset",
		UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_4(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x74,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"op_rx_req[3:0],tx_roffset[5:2]",
		UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_5(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x78,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"roffset_dma,op_rx_req[5:4]",
		UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_6(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x7c,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"rx_woffset",
		UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_7(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x80,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		"hwfifo_limit,vfifo_limit,sleeping,hwtxdis,swtxdis,suppload,xoffdet,xondet",
		UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0xe4,INTFHUB_LOOPBACK=[0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_LOOPBACK(intfhub_base_remap_addr)));

	pr_info("[%s][%s] 0xf4,INTFHUB_DBG=[0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr)));

	pr_info("[%s][%s] INTFHUB_DEVx_STA=[d0(0x0):0x%x, d1(0x40):0x%x, d2(0x80):0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_STA(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_STA(
			intfhub_base_remap_addr)) : 0));

	pr_info("[%s][%s] INTFHUB_DEVx_PKT_CNT=[d0(0x1c):0x%x, d1(0x50):0x%x, d2(0x90):0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(
			intfhub_base_remap_addr)) : 0));

	pr_info("[%s][%s] INTFHUB_DEVx_CRC_STA=[d0(0x20):0x%x, d1(0x54):0x%x, d2(0x94):0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_CRC_STA(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_CRC_STA(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_CRC_STA(
			intfhub_base_remap_addr)) : 0));

	pr_info("[%s][%s] INTFHUB_DEVx_RX_ERR_CRC_STA=[d0(0x10):0x%x, d1(0x14):0x%x, d2(0x18):0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_RX_ERR_CRC_STA(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_RX_ERR_CRC_STA(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_RX_ERR_CRC_STA(
			intfhub_base_remap_addr)) : 0));

	pr_info("[%s][%s] 0x30,INTFHUB_DEV0_IRQ_STA=[0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr)));

	pr_info("[%s][%s] 0xd0,INTFHUB_IRQ_STA=[0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_IRQ_STA(intfhub_base_remap_addr)));

	pr_info("[%s][%s] 0xe0,INTFHUB_STA0=[0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr)));

	pr_info("[%s][%s] 0xc8,config_crc_bypass=[0x%x]\n",
		__func__, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr)));

	pr_info("[%s][%s] ----------------------------------------\n",
		__func__, ((tag == NULL) ? "null" : tag));

	return 0;
}

/*---------------------------------------------------------------------------*/

module_init(uarthub_core_init);
module_exit(uarthub_core_exit);

/*---------------------------------------------------------------------------*/

MODULE_AUTHOR("CTD/SE5/CS5/Johnny.Yao");
MODULE_DESCRIPTION("MTK UARTHUB Driver$1.0$");
MODULE_LICENSE("GPL");
