// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"

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

#define UARTHUB_BAUD_RATE_3M  0
#define UARTHUB_BAUD_RATE_4M  1
#define UARTHUB_BAUD_RATE_12M 2
#define UARTHUB_BAUD_RATE_24M 3

static atomic_t g_uarthub_probe_called = ATOMIC_INIT(0);
struct platform_device *g_uarthub_pdev;
UARTHUB_CORE_IRQ_CB g_core_irq_callback;
unsigned int g_max_dev;
int g_uarthub_open;
struct clk *clk_apmixedsys_univpll;
int g_uarthub_disable;

#define CLK_CTRL_UNIVPLL_REQ 1
#define INIT_UARTHUB_DEFAULT 1
#define UARTHUB_DEBUG_LOG 1
#define UARTHUB_CONFIG_TRX_GPIO 0
#define UARTHUB_CONFIG_GLUE_CTR 0

struct uarthub_reg_base_addr reg_base_addr;
void __iomem *cmm_base_remap_addr;
void __iomem *dev0_base_remap_addr;
void __iomem *dev1_base_remap_addr;
void __iomem *dev2_base_remap_addr;
void __iomem *intfhub_base_remap_addr;
void __iomem *peri_cg_1_set_remap_addr;
unsigned int peri_cg_1_set_shift;

static int mtk_uarthub_probe(struct platform_device *pdev);
static int mtk_uarthub_remove(struct platform_device *pdev);
static int uarthub_core_init(void);
static void uarthub_core_exit(void);
static irqreturn_t uarthub_irq_isr(int irq, void *arg);
static void trigger_assert_worker_handler(struct work_struct *work);

#if IS_ENABLED(CONFIG_OF)
const struct of_device_id apuarthub_of_ids[] = {
	{ .compatible = "mediatek,uarthub", },
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

	atomic_set(&g_uarthub_probe_called, 1);
	return 0;
}

static int mtk_uarthub_remove(struct platform_device *pdev)
{
	pr_info("[%s] DO UARTHUB REMOVE\n", __func__);

	if (g_uarthub_pdev)
		g_uarthub_pdev = NULL;

	atomic_set(&g_uarthub_probe_called, 0);
	return 0;
}

static int uarthub_core_init(void)
{
	int iRet = -1, retry = 0;

	if (g_uarthub_disable == 1)
		return 0;

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
		pr_notice("[%s] g_uarthub_pdev=[NULL]\n", __func__);
		return -1;
	}

	iRet = uarthub_core_read_max_dev_from_dts(g_uarthub_pdev);
	if (iRet)
		return -1;

	if (g_max_dev <= 0)
		return -1;

	iRet = uarthub_core_read_reg_from_dts(g_uarthub_pdev);
	if (iRet)
		return -1;

#if UARTHUB_CONFIG_TRX_GPIO
	iRet = uarthub_core_config_gpio_from_dts(g_uarthub_pdev);
	if (iRet)
		return -1;
#endif

#if UARTHUB_CONFIG_GLUE_CTR
	uarthub_core_config_uart_glue_ctrl_from_dts(g_uarthub_pdev);
#endif

	uarthub_core_config_univpll_clk_remap_addr_from_dts(g_uarthub_pdev);

#if CLK_CTRL_UNIVPLL_REQ
	iRet = uarthub_core_clk_get_from_dts(g_uarthub_pdev);
	if (iRet)
		return -1;
#endif

	if (!reg_base_addr.vir_addr) {
		pr_notice("[%s] reg_base_addr.phy_addr(0x%lx) ioremap fail\n",
			__func__, reg_base_addr.phy_addr);
		return -1;
	}

	uarthub_assert_ctrl.uarthub_workqueue = create_singlethread_workqueue("uarthub_wq");
	if (!uarthub_assert_ctrl.uarthub_workqueue) {
		pr_info("[%s] workqueue create failed\n", __func__);
		return -1;
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
}

static void uarthub_core_exit(void)
{
	if (g_uarthub_disable == 1)
		return;

	platform_driver_unregister(&mtk_uarthub_dev_drv);

	if (peri_cg_1_set_remap_addr)
		iounmap(peri_cg_1_set_remap_addr);
}

static irqreturn_t uarthub_irq_isr(int irq, void *arg)
{
	int err_type = -1;

	uarthub_core_irq_mask_ctrl(1);
	err_type = uarthub_core_check_irq_err_type();
#if UARTHUB_DEBUG_LOG
	pr_info("[%s] err_type=[%d]\n", __func__, err_type);
#endif
	if (err_type != -1) {
		uarthub_core_set_trigger_assert_worker(err_type);
	} else {
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
		return -4;
	}

	irq_state = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr));

	for (id = 0; id < 18; id++) {
		if (((irq_state >> id) & 0x1) == 0x1) {
			err_type = id;
			break;
		}
	}

	if (err_type != -1)
		pr_info("[%s] Uarthub irq error id(%d)\n", __func__, err_type);
	else
		pr_notice("[%s] Uarthub irq error unknown)\n", __func__);

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
	struct device_node *node = NULL;
	struct resource res;
	int ret = -1;
	int flag;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		/* registers base address */
		ret = of_address_to_resource(node, 0, &res);
		if (ret) {
			pr_notice("[%s] Get uarthub phy reg base failed\n", __func__);
			return -1;
		}
		reg_base_addr.phy_addr = res.start;
		reg_base_addr.vir_addr = (unsigned long) of_iomap(node, 0);
		pr_info("[%s] Get uarthub vir reg base(0x%lx)\n",
			__func__, reg_base_addr.vir_addr);
		of_get_address(node, 0, &(reg_base_addr.size), &flag);

		pr_info("[%s] Get uarthub base, phy=(0x%lx) vir=(0x%lx) size=(0x%llx)",
			__func__, reg_base_addr.phy_addr,
			reg_base_addr.vir_addr, reg_base_addr.size);
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_read_max_dev_from_dts(struct platform_device *pdev)
{
	int max_dev = 0;
	struct device_node *node = NULL;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		if (of_property_read_u32(node, "max_dev", &max_dev)) {
			pr_notice("[%s] unable to get max_dev from dts\n", __func__);
			return -1;
		}
		pr_info("[%s] Get uarthub max dev(%d)\n", __func__, max_dev);
		g_max_dev = max_dev;
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

	return 0;
}

int uarthub_core_read_baud_rate_from_dts(int dev_index, struct platform_device *pdev)
{
	int baud_rate = 0;
	struct device_node *node = NULL;

	if (g_max_dev <= 0)
		return -1;

	if (dev_index < 0 || dev_index > g_max_dev) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return -1;
	}

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		if (of_property_read_u32_index(node, "baud_rate", dev_index, &baud_rate)) {
			pr_notice("[%s] unable to get baud_rate from dts\n", __func__);
			return -1;
		}
		pr_info("[%s] Get uarthub baud rate(%d)\n", __func__, baud_rate);
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

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

int uarthub_core_config_univpll_clk_remap_addr_from_dts(struct platform_device *pdev)
{
	unsigned int peri_cg_1_set_addr = 0;
	struct device_node *node = NULL;
	int iRtn = 0;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		iRtn = of_property_read_u32_index(node,
			"peri_cg_1_set", 0, &peri_cg_1_set_addr);
		if (iRtn) {
			pr_notice("[%s] get peri_cg_1_set_addr fail\n", __func__);
			return -1;
		}

		iRtn = of_property_read_u32_index(node,
			"peri_cg_1_set", 1, &peri_cg_1_set_shift);
		if (iRtn) {
			pr_notice("[%s] get peri_cg_1_set_shift fail\n", __func__);
			return -1;
		}

		pr_info("[%s] get uart_glue_ctrl info(addr:0x%x,mask:0x%x)\n",
			__func__, peri_cg_1_set_addr, peri_cg_1_set_shift);
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

	peri_cg_1_set_remap_addr = ioremap(peri_cg_1_set_addr, 0x10);
	if (!peri_cg_1_set_remap_addr) {
		pr_notice("[%s] peri_cg_1_set_remap_addr(%x) ioremap fail\n",
			__func__, peri_cg_1_set_addr);
		return -1;
	}

	return 0;
}

int uarthub_core_config_uart_glue_ctrl_from_dts(struct platform_device *pdev)
{
	void __iomem *uart_glue_ctrl_remap_addr = NULL;
	unsigned int uart_glue_ctrl_addr = 0;
	unsigned int uart_glue_ctrl_mask = 0;
	unsigned int uart_glue_ctrl_value = 0;
	struct device_node *node = NULL;
	int iRtn = 0;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		iRtn = of_property_read_u32_index(node,
			"uart_glue_ctrl", 0, &uart_glue_ctrl_addr);
		if (iRtn) {
			pr_notice("[%s] get uart_glue_ctrl_addr fail\n", __func__);
			return -1;
		}

		iRtn = of_property_read_u32_index(node,
			"uart_glue_ctrl", 1, &uart_glue_ctrl_mask);
		if (iRtn) {
			pr_notice("[%s] get uart_glue_ctrl_mask fail\n", __func__);
			return -1;
		}

		iRtn = of_property_read_u32_index(node,
			"uart_glue_ctrl", 2, &uart_glue_ctrl_value);
		if (iRtn) {
			pr_notice("[%s] get uart_glue_ctrl_value fail\n", __func__);
			return -1;
		}

		pr_info("[%s] get uart_glue_ctrl info(addr:0x%x,mask:0x%x,value:0x%x)\n",
			__func__, uart_glue_ctrl_addr, uart_glue_ctrl_mask, uart_glue_ctrl_value);
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

	uart_glue_ctrl_remap_addr = ioremap(uart_glue_ctrl_addr, 0x10);
	if (!uart_glue_ctrl_remap_addr) {
		pr_notice("[%s] uart_glue_ctrl_remap_addr(%x) ioremap fail\n",
			__func__, uart_glue_ctrl_addr);
		return -1;
	}

	UARTHUB_REG_WRITE_MASK(uart_glue_ctrl_remap_addr,
		uart_glue_ctrl_value, uart_glue_ctrl_mask);

	if (uart_glue_ctrl_remap_addr)
		iounmap(uart_glue_ctrl_remap_addr);

	return 0;
}

int uarthub_core_config_gpio_from_dts(struct platform_device *pdev)
{
	unsigned int tx_addr = 0, tx_mask = 0, tx_value = 0;
	unsigned int rx_addr = 0, rx_mask = 0, rx_value = 0;
	unsigned int tmp_addr = 0, tmp_mask = 0, tmp_value = 0;
	int i = 0, gpio_index = 0;
	void __iomem *tx_remap_addr = NULL;
	void __iomem *rx_remap_addr = NULL;
	struct device_node *node = NULL;
	int iRtn = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		for (i = 0; i < 2; i++) {
			tmp_addr = 0;
			tmp_mask = 0;
			tmp_value = 0;
			gpio_index = i * 3;

			iRtn = of_property_read_u32_index(node,
				"gpio", (gpio_index + 0), &tmp_addr);
			if (iRtn) {
				pr_notice("[%s] get %s_addr fail\n",
					__func__, ((i == 0) ? "tx" : "rx"));
				return -1;
			}

			iRtn = of_property_read_u32_index(node,
				"gpio", (gpio_index + 1), &tmp_mask);
			if (iRtn) {
				pr_notice("[%s] get %s_mask fail\n",
					__func__, ((i == 0) ? "tx" : "rx"));
				return -1;
			}

			iRtn = of_property_read_u32_index(node,
				"gpio", (gpio_index + 2), &tmp_value);
			if (iRtn) {
				pr_notice("[%s] get %s_value fail\n",
					__func__, ((i == 0) ? "tx" : "rx"));
				return -1;
			}

			if (i == 0) {
				tx_addr = tmp_addr;
				tx_mask = tmp_mask;
				tx_value = tmp_value;
			} else {
				rx_addr = tmp_addr;
				rx_mask = tmp_mask;
				rx_value = tmp_value;
			}

			pr_info("[%s] get gpio uarthub uart %s info(addr:0x%x,mask:0x%x,value:0x%x)\n",
				__func__, ((i == 0) ? "tx" : "rx"), ((i == 0) ? tx_addr : rx_addr),
				((i == 0) ? tx_mask : rx_mask), ((i == 0) ? tx_value : rx_value));
		}
	} else {
		pr_notice("[%s] can't find UARTHUB compatible node\n", __func__);
		return -1;
	}

	tx_remap_addr = ioremap(tx_addr, 0x10);
	if (!tx_remap_addr) {
		pr_notice("[%s] tx_remap_addr(%x) ioremap fail\n", __func__, tx_addr);
		return -1;
	}

	rx_remap_addr = ioremap(rx_addr, 0x10);
	if (!rx_remap_addr) {
		pr_notice("[%s] rx_remap_addr(%x) ioremap fail\n", __func__, tx_addr);
		if (tx_remap_addr)
			iounmap(tx_remap_addr);
		return -1;
	}

	UARTHUB_REG_WRITE_MASK(tx_remap_addr, tx_value, tx_mask);
	UARTHUB_REG_WRITE_MASK(rx_remap_addr, rx_value, rx_mask);

	if (tx_remap_addr)
		iounmap(tx_remap_addr);

	if (rx_remap_addr)
		iounmap(rx_remap_addr);

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

#endif

#if CLK_CTRL_UNIVPLL_REQ
	uarthub_core_clk_univpll_ctrl(1);
#endif

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		uarthub_core_debug_info();
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

		baud_rate = uarthub_core_read_baud_rate_from_dts(i, g_uarthub_pdev);
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

	uarthub_core_bypass_mode_ctrl(1);
	uarthub_core_crc_ctrl(1);
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
		uarthub_core_debug_info();
		return -4;
	}

	state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr),
		(0x1 << 9)) >> 9);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_SET(intfhub_base_remap_addr), 0x3);

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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1)
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 2));
	else
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 2));

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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] rate_index=[%d]\n", __func__, rate_index);
#endif

	if (rate_index == UARTHUB_BAUD_RATE_3M) {
		UARTHUB_REG_WRITE(UARTHUB_FEATURE_SEL(uarthub_dev_base), 0x1);  /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(UARTHUB_HIGHSPEED(uarthub_dev_base), 0x3);    /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_COUNT(uarthub_dev_base), 0x21);/* 0x28 = 0x21 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_POINT(uarthub_dev_base), 0x11);/* 0x2c = 0x11 */
		UARTHUB_REG_WRITE(UARTHUB_DLL(uarthub_dev_base), 0x1);          /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_L(uarthub_dev_base), 0xdb);   /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_M(uarthub_dev_base), 0x1);    /* 0x58 = 0x1  */
	} else if (rate_index == UARTHUB_BAUD_RATE_4M) {
		UARTHUB_REG_WRITE(UARTHUB_FEATURE_SEL(uarthub_dev_base), 0x1);  /* 0x9c = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_HIGHSPEED(uarthub_dev_base), 0x3);    /* 0x24 = 0x3 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_COUNT(uarthub_dev_base), 0x19);/* 0x28 = 0x19 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_POINT(uarthub_dev_base), 0xd); /* 0x2c = 0xd  */
		UARTHUB_REG_WRITE(UARTHUB_DLL(uarthub_dev_base), 0x1);          /* 0x90 = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_L(uarthub_dev_base), 0x0);    /* 0x54 = 0x0  */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_M(uarthub_dev_base), 0x0);    /* 0x58 = 0x0  */
	} else if (rate_index == UARTHUB_BAUD_RATE_12M) {
		UARTHUB_REG_WRITE(UARTHUB_FEATURE_SEL(uarthub_dev_base), 0x1);  /* 0x9c = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_HIGHSPEED(uarthub_dev_base), 0x3);    /* 0x24 = 0x3 */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_COUNT(uarthub_dev_base), 0x7); /* 0x28 = 0x7  */
		UARTHUB_REG_WRITE(UARTHUB_SAMPLE_POINT(uarthub_dev_base), 0x4); /* 0x2c = 0x4  */
		UARTHUB_REG_WRITE(UARTHUB_DLL(uarthub_dev_base), 0x1);          /* 0x90 = 0x1 */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_L(uarthub_dev_base), 0xdb);   /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(UARTHUB_FRACDIV_M(uarthub_dev_base), 0x1);    /* 0x58 = 0x1  */
	} else if (rate_index == UARTHUB_BAUD_RATE_24M) {
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
#if UARTHUB_DEBUG_LOG
	pr_info("[%s] err_type=[%d]\n", __func__, err_type);
#endif
	queue_work(uarthub_assert_ctrl.uarthub_workqueue, &uarthub_assert_ctrl.trigger_assert_work);
}

static void trigger_assert_worker_handler(struct work_struct *work)
{
	struct assert_ctrl *queue = container_of(work, struct assert_ctrl, trigger_assert_work);
	int err_type = (int) queue->err_type;

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] err_type=[%d]\n", __func__, err_type);
#endif

	uarthub_core_assert_state_ctrl(1);

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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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
		uarthub_core_debug_info();
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

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[0x%x]\n", __func__, state);
#endif

	return (state == 0x8581) ? 1 : 0;
}

int uarthub_core_debug_info(void)
{
	void __iomem *uarthub_dev_base = dev0_base_remap_addr;
	int apb_bus_clk_enable = 0;
	int i = 0;
	char buf[256] = {0};

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	apb_bus_clk_enable = uarthub_core_is_apb_bus_clk_enable();
	pr_info("[%s] APB BUS CLK=[0x%x]\n", __func__, apb_bus_clk_enable);

	if (peri_cg_1_set_remap_addr) {
		pr_info("[%s] UNIVPLL CLK=[0x%x]\n",
			__func__,
			(UARTHUB_REG_READ_BIT(peri_cg_1_set_remap_addr,
				(0x1 << peri_cg_1_set_shift)) >> peri_cg_1_set_shift));
	}

	if (apb_bus_clk_enable == 0)
		return -2;

	for (i = 0; i <= g_max_dev; i++) {
		if (i != g_max_dev) {
			if (i == 0)
				uarthub_dev_base = dev0_base_remap_addr;
			else if (i == 1)
				uarthub_dev_base = dev1_base_remap_addr;
			else if (i == 2)
				uarthub_dev_base = dev2_base_remap_addr;
			if (snprintf(buf, sizeof(buf), "%d", i) < 0)
				buf[0] = '\0';
		} else {
			uarthub_dev_base = cmm_base_remap_addr;
			if (snprintf(buf, sizeof(buf), "%s", "cmm") < 0)
				buf[0] = '\0';
		}

		pr_info("[%s] DEV_%s, FEATURE_SEL=[0x%08x]\n",
			__func__, buf, UARTHUB_FEATURE_SEL(uarthub_dev_base));
		pr_info("[%s] DEV_%s, HIGHSPEED=[0x%08x]\n",
			__func__, buf, UARTHUB_HIGHSPEED(uarthub_dev_base));
		pr_info("[%s] DEV_%s, SAMPLE_COUNT=[0x%08x]\n",
			__func__, buf, UARTHUB_SAMPLE_COUNT(uarthub_dev_base));
		pr_info("[%s] DEV_%s, SAMPLE_POINT=[0x%08x]\n",
			__func__, buf, UARTHUB_SAMPLE_POINT(uarthub_dev_base));
		pr_info("[%s] DEV_%s, DLL=[0x%08x]\n",
			__func__, buf, UARTHUB_DLL(uarthub_dev_base));
		pr_info("[%s] DEV_%s, FRACDIV_L=[0x%08x]\n",
			__func__, buf, UARTHUB_FRACDIV_L(uarthub_dev_base));
		pr_info("[%s] DEV_%s, FRACDIV_M=[0x%08x]\n",
			__func__, buf, UARTHUB_FRACDIV_M(uarthub_dev_base));
		pr_info("[%s] DEV_%s, DMA_EN=[0x%08x]\n",
			__func__, buf, UARTHUB_DMA_EN(uarthub_dev_base));
		pr_info("[%s] DEV_%s, LCR=[0x%08x]\n",
			__func__, buf, UARTHUB_LCR(uarthub_dev_base));
		pr_info("[%s] DEV_%s, EFR=[0x%08x]\n",
			__func__, buf, UARTHUB_EFR(uarthub_dev_base));
		pr_info("[%s] DEV_%s, XOFF1=[0x%08x]\n",
			__func__, buf, UARTHUB_XOFF1(uarthub_dev_base));
		pr_info("[%s] DEV_%s, XON1=[0x%08x]\n",
			__func__, buf, UARTHUB_XON1(uarthub_dev_base));
		pr_info("[%s] DEV_%s, XOFF2=[0x%08x]\n",
			__func__, buf, UARTHUB_XOFF2(uarthub_dev_base));
		pr_info("[%s] DEV_%s, XON2=[0x%08x]\n",
			__func__, buf, UARTHUB_XON2(uarthub_dev_base));
		pr_info("[%s] DEV_%s, ESCAPE_EN=[0x%08x]\n",
			__func__, buf, UARTHUB_ESCAPE_EN(uarthub_dev_base));
		pr_info("[%s] DEV_%s, ESCAPE_DAT=[0x%08x]\n",
			__func__, buf, UARTHUB_ESCAPE_DAT(uarthub_dev_base));
		pr_info("[%s] DEV_%s, TX_FIFO_OFFSET=[0x%08x]\n",
			__func__, buf, UARTHUB_TX_FIFO_OFFSET(uarthub_dev_base));
		pr_info("[%s] DEV_%s, RX_FIFO_OFFSET=[0x%08x]\n",
			__func__, buf, UARTHUB_RX_FIFO_OFFSET(uarthub_dev_base));
	}

	pr_info("[%s] INTFHUB_LOOPBACK=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_LOOPBACK(intfhub_base_remap_addr));
	pr_info("[%s] INTFHUB_DBG=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	pr_info("[%s] INTFHUB_DEV0_STA=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr));
	pr_info("[%s] INTFHUB_DEV0_RX_ERR_CRC_STA=[0x%08x]\n", __func__,
		UARTHUB_INTFHUB_DEV0_RX_ERR_CRC_STA(intfhub_base_remap_addr));
	if (g_max_dev >= 2) {
		pr_info("[%s] INTFHUB_DEV1_RX_ERR_CRC_STA=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV1_RX_ERR_CRC_STA(intfhub_base_remap_addr));
	}
	if (g_max_dev >= 3) {
		pr_info("[%s] INTFHUB_DEV2_RX_ERR_CRC_STA=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV2_RX_ERR_CRC_STA(intfhub_base_remap_addr));
	}
	pr_info("[%s] INTFHUB_DEV0_PKT_CNT=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr));
	pr_info("[%s] INTFHUB_DEV0_CRC_STA=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_DEV0_CRC_STA(intfhub_base_remap_addr));
	pr_info("[%s] INTFHUB_DEV0_IRQ_STA=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr));
	if (g_max_dev >= 2) {
		pr_info("[%s] INTFHUB_DEV1_STA=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV1_STA(intfhub_base_remap_addr));
		pr_info("[%s] INTFHUB_DEV1_PKT_CNT=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV1_PKT_CNT(intfhub_base_remap_addr));
		pr_info("[%s] INTFHUB_DEV1_CRC_STA=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV1_CRC_STA(intfhub_base_remap_addr));
	}
	if (g_max_dev >= 3) {
		pr_info("[%s] INTFHUB_DEV2_STA=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV2_STA(intfhub_base_remap_addr));
		pr_info("[%s] INTFHUB_DEV2_PKT_CNT=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV2_PKT_CNT(intfhub_base_remap_addr));
		pr_info("[%s] INTFHUB_DEV2_CRC_STA=[0x%08x]\n", __func__,
			UARTHUB_INTFHUB_DEV2_CRC_STA(intfhub_base_remap_addr));
	}
	pr_info("[%s] INTFHUB_IRQ_STA=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_IRQ_STA(intfhub_base_remap_addr));
	pr_info("[%s] INTFHUB_STA0=[0x%08x]\n",
		__func__, UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr));

	pr_info("[%s] trx_state=[d0:0x%x, d1:0x%x, d2:0x%x]\n",
		__func__,
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_STA(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_STA(
			intfhub_base_remap_addr)) : 0));

	pr_info("[%s] timeout_info=[d0:0x%x, d1:0x%x, d2:0x%x]\n",
		__func__,
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(
			intfhub_base_remap_addr)) : 0));

	pr_info("[%s] config_crc_bypass=[0x%x]\n",
		__func__,
		UARTHUB_REG_READ(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr)));

	pr_info("[%s] flow_ctrl_state=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__,
		UARTHUB_REG_READ(UARTHUB_MCR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_MCR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_MCR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_MCR(cmm_base_remap_addr)));

	pr_info("[%s] fifo_info(rx:0,tx:6)=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		__func__,
		UARTHUB_REG_READ(UARTHUB_LSR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_LSR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_LSR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_LSR(cmm_base_remap_addr)));

	return 0;
}

/*---------------------------------------------------------------------------*/

module_init(uarthub_core_init);
module_exit(uarthub_core_exit);

/*---------------------------------------------------------------------------*/

MODULE_AUTHOR("CTD/SE5/CS5/Johnny.Yao");
MODULE_DESCRIPTION("MTK UARTHUB Driver$1.0$");
MODULE_LICENSE("GPL");
