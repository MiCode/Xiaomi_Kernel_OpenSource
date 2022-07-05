// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "mtk_disp_notify.h"

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
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
static struct notifier_block uarthub_fb_notifier;
spinlock_t g_clear_trx_req_lock;
struct workqueue_struct *uarthub_workqueue;
static struct hrtimer dump_hrtimer;

#define DBG_LOG_LEN 1024

#define CLK_CTRL_UNIVPLL_REQ 0
#define INIT_UARTHUB_DEFAULT 0
#define UARTHUB_DEBUG_LOG 1
#define UARTHUB_CONFIG_TRX_GPIO 0
#define SUPPORT_SSPM_DRIVER 1
#define UARTHUB_ASSERT_BIT_DISABLE_MD_ADSP_FIFO 0
#define UARTHUB_ERR_IRQ_DUMP_WORKER 1
#define UARTHUB_DUMP_DEBUG_LOOP_MS 30
#define UARTHUB_DUMP_DEBUG_LOOP_ENABLE 0

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
static void debug_info_worker_handler(struct work_struct *work);
static int uarthub_fb_notifier_callback(struct notifier_block *nb, unsigned long value, void *v);
static enum hrtimer_restart dump_hrtimer_handler_cb(struct hrtimer *hrt);

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
static struct debug_info_ctrl uarthub_debug_info_ctrl;

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
	int ret = -1, retry = 0;

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_uarthub_disable=[%d]\n", __func__, g_uarthub_disable);
#endif

	ret = platform_driver_register(&mtk_uarthub_dev_drv);
	if (ret)
		pr_notice("[%s] Uarthub driver registered failed(%d)\n", __func__, ret);
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

	ret = uarthub_core_get_max_dev();
	if (ret)
		goto ERROR;

	if (g_max_dev <= 0)
		goto ERROR;

	ret = uarthub_core_get_uarthub_reg();
	if (ret)
		goto ERROR;

#if UARTHUB_CONFIG_TRX_GPIO
	ret = uarthub_core_config_hub_mode_gpio();
	if (ret)
		goto ERROR;
#endif

#if CLK_CTRL_UNIVPLL_REQ
	ret = uarthub_core_clk_get_from_dts(g_uarthub_pdev);
	if (ret)
		goto ERROR;
#endif

	if (!reg_base_addr.vir_addr) {
		pr_notice("[%s] reg_base_addr.phy_addr(0x%lx) ioremap fail\n",
			__func__, reg_base_addr.phy_addr);
		goto ERROR;
	}

	uarthub_workqueue = create_singlethread_workqueue("uarthub_wq");
	if (!uarthub_workqueue) {
		pr_notice("[%s] workqueue create failed\n", __func__);
		goto ERROR;
	}

	INIT_WORK(&uarthub_assert_ctrl.trigger_assert_work, trigger_assert_worker_handler);
	INIT_WORK(&uarthub_debug_info_ctrl.debug_info_work, debug_info_worker_handler);

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

	uarthub_fb_notifier.notifier_call = uarthub_fb_notifier_callback;
	ret = mtk_disp_notifier_register("uarthub_driver", &uarthub_fb_notifier);
	if (ret)
		pr_notice("uarthub register fb_notifier failed! ret(%d)\n", ret);
	else
		pr_info("uarthub register fb_notifier OK!\n");

	spin_lock_init(&g_clear_trx_req_lock);

	hrtimer_init(&dump_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dump_hrtimer.function = dump_hrtimer_handler_cb;

	return 0;

ERROR:
	g_uarthub_disable = 1;
	return -1;
}

static void uarthub_core_exit(void)
{
	if (g_uarthub_disable == 1)
		return;

	mtk_disp_notifier_unregister(&uarthub_fb_notifier);
	platform_driver_unregister(&mtk_uarthub_dev_drv);
}

static enum hrtimer_restart dump_hrtimer_handler_cb(struct hrtimer *hrt)
{
	unsigned long flags;

	hrtimer_forward_now(hrt, ms_to_ktime(UARTHUB_DUMP_DEBUG_LOOP_MS));

	spin_lock_irqsave(&g_clear_trx_req_lock, flags);
	uarthub_core_debug_uart_ip_info_loop();
	spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);

	return HRTIMER_RESTART;
}

static int uarthub_fb_notifier_callback(struct notifier_block *nb, unsigned long value, void *v)
{
	int data = 0;
	const char *prefix = "@@@@@@@@@@";
	const char *postfix = "@@@@@@@@@@@@@@";

	if (!v)
		return 0;

	data = *(int *)v;

	if (value == MTK_DISP_EVENT_BLANK) {
		pr_info("%s+\n", __func__);
		if (data == MTK_DISP_BLANK_UNBLANK) {
			pr_info("[%s] %s uarthub enter UNBLANK %s\n",
				__func__, prefix, postfix);
			uarthub_core_debug_info_with_tag_worker("UNBLANK_CB");
		} else if (data == MTK_DISP_BLANK_POWERDOWN) {
			uarthub_core_debug_info_with_tag_worker("POWERDOWN_CB");
			pr_info("[%s] %s uarthub enter early POWERDOWN %s\n",
				__func__, prefix, postfix);
		} else {
			pr_info("[%s] %s data(%d) is not UNBLANK or POWERDOWN %s\n",
				__func__, prefix, data, postfix);
		}
		pr_info("%s-\n", __func__);
	}

	return 0;
}

static irqreturn_t uarthub_irq_isr(int irq, void *arg)
{
	int err_type = -1;
	int is_bypass = 0;
	int is_assert = 0;
	unsigned long flags;
#if !(UARTHUB_ERR_IRQ_DUMP_WORKER)
	int id = 0;
	int err_total = 0;
	int err_index = 0;
#endif

	if (!intfhub_base_remap_addr || uarthub_core_is_apb_bus_clk_enable() == 0)
		return IRQ_HANDLED;

	if (spin_trylock_irqsave(&g_clear_trx_req_lock, flags) == 0) {
		pr_notice("[%s] fail to get g_clear_trx_req_lock lock\n", __func__);
		/* clear irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
		return IRQ_HANDLED;
	}

	/* mask irq */
	UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
		0x3FFFF, 0x3FFFF);

	/* check is bypass */
	is_bypass = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr),
		(0x1 << 2)) >> 2);

	/* check is assert state */
	is_assert = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr),
		(0x1 << 0)) >> 0);

	if (g_max_dev <= 0 || is_bypass == 1 || is_assert == 1) {
		/* clear irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
		spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);
		return IRQ_HANDLED;
	}

	err_type = uarthub_core_check_irq_err_type();
#if !(UARTHUB_ERR_IRQ_DUMP_WORKER)
	pr_info("[%s] err_type=[0x%x]\n", __func__, err_type);
#endif
	if (err_type > 0) {
#if !(UARTHUB_ERR_IRQ_DUMP_WORKER)
		err_total = 0;
		for (id = 0; id < irq_err_type_max; id++) {
			if (((err_type >> id) & 0x1) == 0x1)
				err_total++;
		}

		if (err_total > 0) {
			err_index = 0;
			for (id = 0; id < irq_err_type_max; id++) {
				if (((err_type >> id) & 0x1) == 0x1) {
					err_index++;
					if (g_core_irq_callback == NULL) {
						pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], g_core_irq_callback=[NULL]\n",
							__func__, err_total, err_index,
							id, UARTHUB_irq_err_type_str[id]);
					} else {
						pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], g_core_irq_callback=[%p]\n",
							__func__, err_total, err_index,
							id, UARTHUB_irq_err_type_str[id],
							g_core_irq_callback);
					}
				}
			}
		}

		uarthub_core_debug_info_with_tag_no_spinlock(__func__);

		if (g_core_irq_callback)
			(*g_core_irq_callback)(err_type);

		/* clear irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
		/* unmask irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
			0x0, 0x3FFFF);
		spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);
#else
		spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);
		uarthub_core_set_trigger_assert_worker(err_type);
#endif
	} else {
		/* clear irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
		/* unmask irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
			0x0, 0x3FFFF);

		spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);
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
	return UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr));
}

int uarthub_core_irq_register(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	int ret = 0;
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

		ret = request_irq(irq_num, uarthub_irq_isr, irq_flag,
			"UARTHUB_IRQ", NULL);

		if (ret) {
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

int uarthub_core_get_uarthub_reg(void)
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
	int ret = 0;
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

#if UARTHUB_ASSERT_BIT_DISABLE_MD_ADSP_FIFO
	pr_info("[%s] UARTHUB_ASSERT_BIT_DISABLE_MD_ADSP_FIFO=[1]\n", __func__);
#else
	pr_info("[%s] UARTHUB_ASSERT_BIT_DISABLE_MD_ADSP_FIFO=[0]\n", __func__);
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
	g_uarthub_open = 1;

	uarthub_core_bypass_mode_ctrl(1)
	uarthub_core_crc_ctrl(1);
	uarthub_core_debug_info_with_tag(__func__);
#else
	g_uarthub_open = 1;
#endif

	ret = uarthub_core_irq_register(g_uarthub_pdev);
	if (ret)
		return -1;

	return 0;
}

int uarthub_core_close(void)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -1;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	uarthub_core_irq_mask_ctrl(1);
	uarthub_core_irq_clear_ctrl();

#if CLK_CTRL_UNIVPLL_REQ
	uarthub_core_clk_univpll_ctrl(0);
#endif
	g_uarthub_open = 0;

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

	if (state == 1) {
		uarthub_core_irq_clear_ctrl();
#if INIT_UARTHUB_DEFAULT
		if (uarthub_core_is_uarthub_clk_enable() == 1) {
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
		}
#endif
		uarthub_core_debug_info_with_tag(__func__);
	}

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

#if UARTHUB_DUMP_DEBUG_LOOP_ENABLE
	hrtimer_start(&dump_hrtimer, 0, HRTIMER_MODE_REL);
#endif

	return 0;
}

int uarthub_core_dev0_clear_txrx_request(void)
{
	unsigned long flags;

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

#if UARTHUB_DUMP_DEBUG_LOOP_ENABLE
	hrtimer_cancel(&dump_hrtimer);
#endif

	spin_lock_irqsave(&g_clear_trx_req_lock, flags);
	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_CLR(intfhub_base_remap_addr), 0x7);
	spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);

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
	int ret = 0;

	if (g_max_dev <= 0)
		return -1;

	if (clk_on == 1) {
		ret = clk_prepare_enable(clk_apmixedsys_univpll);
		usleep_range(5000, 6000);
		if (ret) {
			pr_notice("[%s] clk_prepare_enable(clk_apmixedsys_univpll) fail(%d)\n",
				__func__, ret);
			return ret;
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

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
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

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
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
	queue_work(uarthub_workqueue, &uarthub_assert_ctrl.trigger_assert_work);
}

static void trigger_assert_worker_handler(struct work_struct *work)
{
	struct assert_ctrl *queue = container_of(work, struct assert_ctrl, trigger_assert_work);
	int err_type = (int) queue->err_type;
	int id = 0;
	int err_total = 0;
	int err_index = 0;
	unsigned long flags;

	if (spin_trylock_irqsave(&g_clear_trx_req_lock, flags) == 0) {
		pr_notice("[%s] fail to get g_clear_trx_req_lock lock\n", __func__);
		uarthub_core_irq_clear_ctrl();
		uarthub_core_irq_mask_ctrl(0);
		return;
	}

	if (uarthub_core_is_bypass_mode() == 1) {
		pr_info("[%s] ignore irq error in bypass mode\n", __func__);
		uarthub_core_irq_mask_ctrl(1);
		uarthub_core_irq_clear_ctrl();
		spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);
		return;
	}

	if (uarthub_core_is_assert_state() == 1) {
		pr_info("[%s] ignore irq error if assert flow\n", __func__);
		uarthub_core_irq_clear_ctrl();
		spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);
		return;
	}

	pr_info("[%s] err_type=[0x%x]\n", __func__, err_type);
	err_total = 0;
	for (id = 0; id < irq_err_type_max; id++) {
		if (((err_type >> id) & 0x1) == 0x1)
			err_total++;
	}

	if (err_total > 0) {
		err_index = 0;
		for (id = 0; id < irq_err_type_max; id++) {
			if (((err_type >> id) & 0x1) == 0x1) {
				err_index++;
				if (g_core_irq_callback == NULL) {
					pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], g_core_irq_callback=[NULL]\n",
						__func__, err_total, err_index,
						id, UARTHUB_irq_err_type_str[id]);
				} else {
					pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], g_core_irq_callback=[%p]\n",
						__func__, err_total, err_index, id,
						UARTHUB_irq_err_type_str[id], g_core_irq_callback);
				}
			}
		}
	}

	uarthub_core_debug_info_with_tag_no_spinlock(__func__);

	uarthub_core_irq_clear_ctrl();
	uarthub_core_irq_mask_ctrl(0);
	spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);

	if (g_core_irq_callback)
		(*g_core_irq_callback)(err_type);
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

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);

		if (assert_ctrl == 1)
			UARTHUB_SET_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 0));
		else
			UARTHUB_CLR_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 0));

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
		uarthub_core_irq_mask_ctrl(1);
		uarthub_core_irq_clear_ctrl();
#if UARTHUB_ASSERT_BIT_DISABLE_MD_ADSP_FIFO
		uarthub_core_reset_to_ap_enable_only(1);
#endif
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 0));
	} else {
#if UARTHUB_ASSERT_BIT_DISABLE_MD_ADSP_FIFO
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
#endif
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 0));
		uarthub_core_irq_clear_ctrl();
		uarthub_core_irq_mask_ctrl(0);
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

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
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

int uarthub_core_is_uarthub_clk_enable(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (!g_uarthub_plat_ic_ops)
		return 0;

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_gating_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_gating_info();
		if (state != 0x0) {
			pr_notice("[%s] UARTHUB CLK is GATING(0x%x)\n", __func__, state);
			return 0;
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info();
		if (state != 1) {
			pr_notice("[%s] UNIVPLL CLK is OFF(0x%x)\n", __func__, state);
			return 0;
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_1_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_1_info();
		if (state != 0x1D) {
			/* the expect value is 0x1D */
			pr_notice("[%s] UARTHUB SPM RES 1 is not all on(0x%x)\n", __func__, state);
			return 0;
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_2_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_2_info();
		if (state != 0x17) {
			/* the expect value is 0x17 */
			pr_notice("[%s] UARTHUB SPM RES 2 is not all on(0x%x)\n", __func__, state);
			return 0;
		}
	}

	state = UARTHUB_REG_READ_BIT(
		UARTHUB_INTFHUB_CON1(intfhub_base_remap_addr), 0xFFFF);
	if (state != 0x8581) {
		pr_notice("[%s] APB BUS CLK is OFF\n", __func__);
		return 0;
	}

	state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr),
		(0x3 << 8)) >> 8);

	if (state != 0x3) {
		pr_notice("[%s] UARTHUB is not ready, cannot read UART_IP CR\n", __func__);
		return 0;
	}

	return 1;
}

int uarthub_core_debug_bt_tx_timeout(const char *tag)
{
	const char *def_tag = "UARTHUB_DBG_APMDA";
	void __iomem *ap_uart_base_remap_addr = NULL;
	void __iomem *ap_dma_uart_3_tx_int_remap_addr = NULL;
	struct uarthub_uart_ip_debug_info debug1 = {0};
	struct uarthub_uart_ip_debug_info debug2 = {0};
	struct uarthub_uart_ip_debug_info debug3 = {0};
	struct uarthub_uart_ip_debug_info debug4 = {0};
	struct uarthub_uart_ip_debug_info debug5 = {0};
	struct uarthub_uart_ip_debug_info debug6 = {0};
	struct uarthub_uart_ip_debug_info debug7 = {0};
	struct uarthub_uart_ip_debug_info debug8 = {0};

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (g_uarthub_plat_ic_ops) {
		if (g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr) {
			ap_uart_base_remap_addr =
				g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr();
		}

		if (g_uarthub_plat_ic_ops->uarthub_plat_get_ap_dma_tx_int_addr) {
			ap_dma_uart_3_tx_int_remap_addr =
				g_uarthub_plat_ic_ops->uarthub_plat_get_ap_dma_tx_int_addr();
		}
	}

	pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		pr_info("[%s][%s] ----------------------------------------\n",
			def_tag, ((tag == NULL) ? "null" : tag));
		return -1;
	}

	if (ap_dma_uart_3_tx_int_remap_addr) {
		pr_info("[%s][%s] 0=[0x%x],4=[0x%x],1C=[0x%x],2C=[0x%x],30=[0x%x],40=[0x%x],50=[0x%x]\n",
			def_tag, ((tag == NULL) ? "null" : tag),
			UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x00),
			UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x04),
			UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x1C),
			UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x2C),
			UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x30),
			UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x40),
			UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x50));
	}

	pr_info("[%s][%s] 0x30,INTFHUB_DEV0_IRQ_STA=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr)));

	if (ap_uart_base_remap_addr != NULL) {
		debug1.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_1(ap_uart_base_remap_addr));
		debug2.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_2(ap_uart_base_remap_addr));
		debug3.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_3(ap_uart_base_remap_addr));
		debug4.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_4(ap_uart_base_remap_addr));
		debug5.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_5(ap_uart_base_remap_addr));
		debug6.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_6(ap_uart_base_remap_addr));
		debug7.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_7(ap_uart_base_remap_addr));
		debug8.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_8(ap_uart_base_remap_addr));
	}

	debug1.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr));
	debug2.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev0_base_remap_addr));
	debug3.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev0_base_remap_addr));
	debug4.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev0_base_remap_addr));
	debug5.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev0_base_remap_addr));
	debug6.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev0_base_remap_addr));
	debug7.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev0_base_remap_addr));
	debug8.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr));

	debug1.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr));
	debug2.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_2(cmm_base_remap_addr));
	debug3.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_3(cmm_base_remap_addr));
	debug4.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_4(cmm_base_remap_addr));
	debug5.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_5(cmm_base_remap_addr));
	debug6.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_6(cmm_base_remap_addr));
	debug7.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_7(cmm_base_remap_addr));
	debug8.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr));

	pr_info("[%s][%s] 0x64=[ap:0x%x, d0:0x%x, cmm:0x%x],%s%s%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug1.ap, debug1.dev0, debug1.cmm,
		" xcstate[2:0],txstate[4:0]",
		" (txstate,[0]:idle,[1]:start,[2]:data1,[3]:data2)",
		" (xcstate,[0]:idle,[1]:wait_for_xoff,[2]:send_xoff1,[4]:wait_for_xon,[5]:send_xon)");

	pr_info("[%s][%s] 0x68=[ap:0x%x, d0:0x%x, cmm:0x%x],%s%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug2.ap, debug2.dev0, debug2.cmm,
		" ip_tx_dma[3:0],rxstate[3:0]",
		" (rxstate,[0]:idle,[1]:start,[2]:data1,[3]:data2)");

	pr_info("[%s][%s] 0x6c=[ap:0x%x, d0:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug3.ap, debug3.dev0, debug3.cmm,
		" rx_offset_dma[5:0],ip_tx_dma[5:4]");

	pr_info("[%s][%s] 0x70=[ap:0x%x, d0:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug4.ap, debug4.dev0, debug4.cmm,
		" tx_roffset[1:0],tx_woffset[5:0]");

	pr_info("[%s][%s] 0x74=[ap:0x%x, d0:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug5.ap, debug5.dev0, debug5.cmm,
		" op_rx_req[3:0],tx_roffset[5:2]");

	pr_info("[%s][%s] 0x78=[ap:0x%x, d0:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug6.ap, debug6.dev0, debug6.cmm,
		" roffset_dma[5:0],op_rx_req[5:4]");

	pr_info("[%s][%s] 0x7c=[ap:0x%x, d0:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug7.ap, debug7.dev0, debug7.cmm,
		" rx_woffset[5:0]");

	pr_info("[%s][%s] 0x80=[ap:0x%x, d0:0x%x, cmm:0x%x],%s%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug8.ap, debug8.dev0, debug8.cmm,
		" hwfifo_limit,vfifo_limit,sleeping,hwtxdis,swtxdis(detect_xoff),",
		"suppload,xoffdet,xondet");

	pr_info("[%s][%s] op_rx_req=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		(((debug5.ap & 0xF0) >> 4) + ((debug6.ap & 0x3) << 4)),
		(((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4)),
		(((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4)));

	pr_info("[%s][%s] ip_tx_dma=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		(((debug2.ap & 0xF0) >> 4) + ((debug3.ap & 0x3) << 4)),
		(((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4)),
		(((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4)));

	pr_info("[%s][%s] tx_woffset=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		(debug4.ap & 0x3F), (debug4.dev0 & 0x3F), (debug4.cmm & 0x3F));

	pr_info("[%s][%s] rx_woffset=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		(debug7.ap & 0x3F), (debug7.dev0 & 0x3F), (debug7.cmm & 0x3F));

	pr_info("[%s][%s] xcstate(wait_for_xoff)=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		((((debug1.ap & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0));

	pr_info("[%s][%s] xcstate(send_xoff1)=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		((((debug1.ap & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.dev0 & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 2) ? 1 : 0));

	pr_info("[%s][%s] xcstate(wait_for_xon)=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		((((debug1.ap & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.dev0 & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 4) ? 1 : 0));

	pr_info("[%s][%s] xcstate(send_xon)=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		((((debug1.ap & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.dev0 & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 5) ? 1 : 0));

	pr_info("[%s][%s] swtxdis(detect_xoff)=[ap:%d, d0:%d, cmm:%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		((debug8.ap & 0x8) >> 3),
		((debug8.dev0 & 0x8) >> 3),
		((debug8.cmm & 0x8) >> 3));

	pr_info("[%s][%s] ----------------------------------------\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	return 0;
}

int uarthub_core_debug_apdma_uart_info_with_tag_ex(const char *tag, int boundary)
{
	const char *def_tag = "UARTHUB_DBG_APMDA";
	void __iomem *ap_dma_uart_3_tx_int_remap_addr = NULL;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_ap_dma_tx_int_addr) {
		ap_dma_uart_3_tx_int_remap_addr =
			g_uarthub_plat_ic_ops->uarthub_plat_get_ap_dma_tx_int_addr();
	}

	if (boundary == 1) {
		pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
			def_tag, ((tag == NULL) ? "null" : tag));
	}

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		if (boundary == 1) {
			pr_info("[%s][%s] ----------------------------------------\n",
				def_tag, ((tag == NULL) ? "null" : tag));
		}
		return -1;
	}

	if (!ap_dma_uart_3_tx_int_remap_addr) {
		pr_notice("[%s] ap_dma_uart_3_tx_int_remap_addr=[NULL]\n", __func__);
		if (boundary == 1) {
			pr_info("[%s][%s] ----------------------------------------\n",
				def_tag, ((tag == NULL) ? "null" : tag));
		}
		return -1;
	}

	pr_info("[%s][%s] 0=[0x%x],4=[0x%x],8=[0x%x],c=[0x%x],10=[0x%x],14=[0x%x],18=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x00),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x04),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x08),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x0c),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x10),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x14),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x18));

	pr_info("[%s][%s] 1c=[0x%x],20=[0x%x],24=[0x%x],28=[0x%x],2c=[0x%x],30=[0x%x],34=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x1c),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x20),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x24),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x28),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x2c),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x30),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x34));

	pr_info("[%s][%s] 38=[0x%x],3c=[0x%x],40=[0x%x],44=[0x%x],48=[0x%x],4c=[0x%x],50=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x38),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x3c),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x40),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x44),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x48),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x4c),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x50));

	pr_info("[%s][%s] 54=[0x%x],58=[0x%x],5c=[0x%x],60=[0x%x],64=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x54),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x58),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x5c),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x60),
		UARTHUB_REG_READ(ap_dma_uart_3_tx_int_remap_addr + 0x64));

	if (boundary == 1) {
		pr_info("[%s][%s] ----------------------------------------\n",
			def_tag, ((tag == NULL) ? "null" : tag));
	}

	return 0;
}

int uarthub_core_debug_uart_ip_info_loop(void)
{
	const char *def_tag = "UARTHUB_DBG_LOOP";
	struct uarthub_uart_ip_debug_info debug1 = {0};
	struct uarthub_uart_ip_debug_info debug2 = {0};
	struct uarthub_uart_ip_debug_info debug3 = {0};
	struct uarthub_uart_ip_debug_info debug4 = {0};
	struct uarthub_uart_ip_debug_info debug5 = {0};
	struct uarthub_uart_ip_debug_info debug6 = {0};
	struct uarthub_uart_ip_debug_info debug7 = {0};
	struct uarthub_uart_ip_debug_info debug8 = {0};
	struct uarthub_uart_ip_debug_info pkt_cnt = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	pr_info("[%s_%d] ++++++++++++++++++++++++++++++++++++++++\n",
		def_tag, UARTHUB_DUMP_DEBUG_LOOP_MS);

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s_%d] uarthub_core_is_uarthub_clk_enable=[0]\n",
			def_tag, UARTHUB_DUMP_DEBUG_LOOP_MS);
		pr_info("[%s_%d] ----------------------------------------\n",
			def_tag, UARTHUB_DUMP_DEBUG_LOOP_MS);
		return -2;
	}

	debug1.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr));
	debug2.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev0_base_remap_addr));
	debug3.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev0_base_remap_addr));
	debug4.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev0_base_remap_addr));
	debug5.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev0_base_remap_addr));
	debug6.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev0_base_remap_addr));
	debug7.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev0_base_remap_addr));
	debug8.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr));
	pkt_cnt.dev0 =
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr));

	if (g_max_dev >= 2) {
		debug1.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev1_base_remap_addr));
		debug2.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev1_base_remap_addr));
		debug3.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev1_base_remap_addr));
		debug4.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev1_base_remap_addr));
		debug5.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev1_base_remap_addr));
		debug6.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev1_base_remap_addr));
		debug7.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev1_base_remap_addr));
		debug8.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev1_base_remap_addr));
		pkt_cnt.dev1 =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(intfhub_base_remap_addr));
	}

	if (g_max_dev >= 3) {
		debug1.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev2_base_remap_addr));
		debug2.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev2_base_remap_addr));
		debug3.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev2_base_remap_addr));
		debug4.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev2_base_remap_addr));
		debug5.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev2_base_remap_addr));
		debug6.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev2_base_remap_addr));
		debug7.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev2_base_remap_addr));
		debug8.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev2_base_remap_addr));
		pkt_cnt.dev2 =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(intfhub_base_remap_addr));
	}

	debug1.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr));
	debug2.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_2(cmm_base_remap_addr));
	debug3.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_3(cmm_base_remap_addr));
	debug4.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_4(cmm_base_remap_addr));
	debug5.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_5(cmm_base_remap_addr));
	debug6.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_6(cmm_base_remap_addr));
	debug7.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_7(cmm_base_remap_addr));
	debug8.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr));

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s_%d] op_rx_req=[%d-%d-%d-%d],ip_tx_dma=[%d-%d-%d-%d]",
		def_tag, UARTHUB_DUMP_DEBUG_LOOP_MS,
		(((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4)),
		(((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4)),
		(((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4)),
		(((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4)),
		(((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4)),
		(((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4)),
		(((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4)),
		(((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___tx_woffset=[%d-%d-%d-%d],rx_woffset=[%d-%d-%d-%d]",
		(debug4.dev0 & 0x3F), (debug4.dev1 & 0x3F),
		(debug4.dev2 & 0x3F), (debug4.cmm & 0x3F),
		(debug7.dev0 & 0x3F), (debug7.dev1 & 0x3F),
		(debug7.dev2 & 0x3F), (debug7.cmm & 0x3F));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s_%d] wait_for_xoff=[%d-%d-%d-%d],send_xoff1=[%d-%d-%d-%d]",
		def_tag, UARTHUB_DUMP_DEBUG_LOOP_MS,
		((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev0 & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 2) ? 1 : 0));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___wait_for_xon=[%d-%d-%d-%d],detect_xoff=[%d-%d-%d-%d],send_xon=[%d-%d-%d-%d]",
		((((debug1.dev0 & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 4) ? 1 : 0),
		((debug8.dev0 & 0x8) >> 3), ((debug8.dev1 & 0x8) >> 3),
		((debug8.dev2 & 0x8) >> 3), ((debug8.cmm & 0x8) >> 3),
		((((debug1.dev0 & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 5) ? 1 : 0));

	pr_info("%s\n", dmp_info_buf);

	pr_info("[%s_%d] tx_pkt_cnt=[%d-%d-%d],rx_pkt_cnt=[%d-%d-%d]\n",
		def_tag, UARTHUB_DUMP_DEBUG_LOOP_MS,
		((pkt_cnt.dev0 & 0xFF000000) >> 24), ((pkt_cnt.dev1 & 0xFF000000) >> 24),
		((pkt_cnt.dev2 & 0xFF000000) >> 24), ((pkt_cnt.dev0 & 0xFF00) >> 8),
		((pkt_cnt.dev1 & 0xFF00) >> 8), ((pkt_cnt.dev2 & 0xFF00) >> 8));

	pr_info("[%s_%d] ----------------------------------------\n",
		def_tag, UARTHUB_DUMP_DEBUG_LOOP_MS);

	return 0;
}

int uarthub_core_debug_uart_ip_info_with_tag_ex(const char *tag, int boundary)
{
	const char *def_tag = "UARTHUB_DBG_UIP";
	struct uarthub_uart_ip_debug_info debug1 = {0};
	struct uarthub_uart_ip_debug_info debug2 = {0};
	struct uarthub_uart_ip_debug_info debug3 = {0};
	struct uarthub_uart_ip_debug_info debug4 = {0};
	struct uarthub_uart_ip_debug_info debug5 = {0};
	struct uarthub_uart_ip_debug_info debug6 = {0};
	struct uarthub_uart_ip_debug_info debug7 = {0};
	struct uarthub_uart_ip_debug_info debug8 = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (boundary == 1) {
		pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
			def_tag, ((tag == NULL) ? "null" : tag));
	}

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		if (boundary == 1) {
			pr_info("[%s][%s] ----------------------------------------\n",
				def_tag, ((tag == NULL) ? "null" : tag));
		}
		return -2;
	}

	debug1.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr));
	debug2.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev0_base_remap_addr));
	debug3.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev0_base_remap_addr));
	debug4.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev0_base_remap_addr));
	debug5.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev0_base_remap_addr));
	debug6.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev0_base_remap_addr));
	debug7.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev0_base_remap_addr));
	debug8.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr));

	if (g_max_dev >= 2) {
		debug1.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev1_base_remap_addr));
		debug2.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev1_base_remap_addr));
		debug3.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev1_base_remap_addr));
		debug4.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev1_base_remap_addr));
		debug5.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev1_base_remap_addr));
		debug6.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev1_base_remap_addr));
		debug7.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev1_base_remap_addr));
		debug8.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev1_base_remap_addr));
	}

	if (g_max_dev >= 3) {
		debug1.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev2_base_remap_addr));
		debug2.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev2_base_remap_addr));
		debug3.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev2_base_remap_addr));
		debug4.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev2_base_remap_addr));
		debug5.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev2_base_remap_addr));
		debug6.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev2_base_remap_addr));
		debug7.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev2_base_remap_addr));
		debug8.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev2_base_remap_addr));
	}

	debug1.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr));
	debug2.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_2(cmm_base_remap_addr));
	debug3.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_3(cmm_base_remap_addr));
	debug4.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_4(cmm_base_remap_addr));
	debug5.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_5(cmm_base_remap_addr));
	debug6.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_6(cmm_base_remap_addr));
	debug7.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_7(cmm_base_remap_addr));
	debug8.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr));

	pr_info("[%s][%s] 0x64=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s%s%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug1.dev0, debug1.dev1, debug1.dev2, debug1.cmm,
		" xcstate[2:0],txstate[4:0]",
		" (txstate,[0]:idle,[1]:start,[2]:data1,[3]:data2)",
		" (xcstate,[0]:idle,[1]:wait_for_xoff,[2]:send_xoff1,[4]:wait_for_xon,[5]:send_xon)");

	pr_info("[%s][%s] 0x68=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug2.dev0, debug2.dev1, debug2.dev2, debug2.cmm,
		" ip_tx_dma[3:0],rxstate[3:0]",
		" (rxstate,[0]:idle,[1]:start,[2]:data1,[3]:data2)");

	pr_info("[%s][%s] 0x6c=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug3.dev0, debug3.dev1, debug3.dev2, debug3.cmm,
		" rx_offset_dma[5:0],ip_tx_dma[5:4]");

	pr_info("[%s][%s] 0x70=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug4.dev0, debug4.dev1, debug4.dev2, debug4.cmm,
		" tx_roffset[1:0],tx_woffset[5:0]");

	pr_info("[%s][%s] 0x74=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug5.dev0, debug5.dev1, debug5.dev2, debug5.cmm,
		" op_rx_req[3:0],tx_roffset[5:2]");

	pr_info("[%s][%s] 0x78=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug6.dev0, debug6.dev1, debug6.dev2, debug6.cmm,
		" roffset_dma[5:0],op_rx_req[5:4]");

	pr_info("[%s][%s] 0x7c=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug7.dev0, debug7.dev1, debug7.dev2, debug7.cmm,
		" rx_woffset[5:0]");

	pr_info("[%s][%s] 0x80=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x],%s%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		debug8.dev0, debug8.dev1, debug8.dev2, debug8.cmm,
		" hwfifo_limit,vfifo_limit,sleeping,hwtxdis,swtxdis(detect_xoff),",
		"suppload,xoffdet,xondet");

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] op_rx_req=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		def_tag, ((tag == NULL) ? "null" : tag),
		(((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4)),
		(((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4)),
		(((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4)),
		(((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___ip_tx_dma=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		(((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4)),
		(((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4)),
		(((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4)),
		(((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4)));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] tx_woffset=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		def_tag, ((tag == NULL) ? "null" : tag),
		(debug4.dev0 & 0x3F), (debug4.dev1 & 0x3F),
		(debug4.dev2 & 0x3F), (debug4.cmm & 0x3F));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___rx_woffset=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		(debug7.dev0 & 0x3F), (debug7.dev1 & 0x3F),
		(debug7.dev2 & 0x3F), (debug7.cmm & 0x3F));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] xcstate(wait_for_send_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		def_tag, ((tag == NULL) ? "null" : tag),
		((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___xcstate(sending_xoff1)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		((((debug1.dev0 & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 2) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 2) ? 1 : 0));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___xcstate(wait_for_send_xon)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		((((debug1.dev0 & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 4) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 4) ? 1 : 0));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] xcstate(sending_xon)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		def_tag, ((tag == NULL) ? "null" : tag),
		((((debug1.dev0 & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 5) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 5) ? 1 : 0));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___swtxdis(detect_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		((debug8.dev0 & 0x8) >> 3), ((debug8.dev1 & 0x8) >> 3),
		((debug8.dev2 & 0x8) >> 3), ((debug8.cmm & 0x8) >> 3));

	pr_info("%s\n", dmp_info_buf);

	if (boundary == 1) {
		pr_info("[%s][%s] ----------------------------------------\n",
			def_tag, ((tag == NULL) ? "null" : tag));
	}

	return 0;
}

int uarthub_core_debug_info(void)
{
	return uarthub_core_debug_info_with_tag(NULL);
}

int uarthub_core_debug_info_with_tag_worker(const char *tag)
{
	int len = 0;

	uarthub_debug_info_ctrl.tag[0] = '\0';

	if (tag != NULL) {
		len = snprintf(uarthub_debug_info_ctrl.tag,
			sizeof(uarthub_debug_info_ctrl.tag), "%s", tag);
		if (len < 0) {
			uarthub_debug_info_ctrl.tag[0] = '\0';
			pr_info("%s tag is NULL\n", __func__);
		}
	}

	queue_work(uarthub_workqueue, &uarthub_debug_info_ctrl.debug_info_work);

	return 0;
}

static void debug_info_worker_handler(struct work_struct *work)
{
	struct debug_info_ctrl *queue = container_of(work, struct debug_info_ctrl, debug_info_work);

	uarthub_core_debug_info_with_tag(queue->tag);
}

int uarthub_core_debug_info_with_tag(const char *tag)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&g_clear_trx_req_lock, flags);
	ret = uarthub_core_debug_info_with_tag_no_spinlock(tag);
	spin_unlock_irqrestore(&g_clear_trx_req_lock, flags);

	return ret;
}

int uarthub_core_debug_info_with_tag_no_spinlock(const char *tag)
{
	int val = 0;
	int apb_bus_clk_enable = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	const char *def_tag = "UARTHUB_DBG";

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	len = 0;
	apb_bus_clk_enable = uarthub_core_is_apb_bus_clk_enable();
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] APB_BUS_CLK=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), apb_bus_clk_enable);

	if (apb_bus_clk_enable == 0) {
		pr_info("%s\n", dmp_info_buf);
		pr_info("[%s][%s] ----------------------------------------\n",
			def_tag, ((tag == NULL) ? "null" : tag));
		return -2;
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_gating_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_gating_info();
		if (val >= 0) {
			/* the expect value is 0x0 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"___CLK_GATING=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_1_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_1_info();
		if (val >= 0) {
			/* the expect value is 0x1D */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"___SPM_RES1=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_2_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_2_info();
		if (val >= 0) {
			/* the expect value is 0x17 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"___SPM_RES2=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_done_info();
		if (val >= 0) {
			/* the expect value is 0x1 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"___UNIVPLL_CLK=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info();
		if (val >= 0) {
			/* the expect value is 0x2 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"___UART_MUX=[0x%x]", val);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info(&gpio_base_addr);
		if (val == 0) {
			len = 0;
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] GPIO TX=[phy_addr:0x%lx, mask:0x%lx, exp_value:0x%lx, read_value:0x%lx]",
				def_tag, ((tag == NULL) ? "null" : tag),
				gpio_base_addr.gpio_tx.addr, gpio_base_addr.gpio_tx.mask,
				gpio_base_addr.gpio_tx.value, gpio_base_addr.gpio_tx.gpio_value);

			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"___RX=[phy_addr:0x%lx, mask:0x%lx, exp_value:0x%lx, read_value:0x%lx]",
				gpio_base_addr.gpio_rx.addr, gpio_base_addr.gpio_rx.mask,
				gpio_base_addr.gpio_rx.value, gpio_base_addr.gpio_rx.gpio_value);

			pr_info("%s\n", dmp_info_buf);
		}
	}

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] 0xe4,INTFHUB_LOOPBACK=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_LOOPBACK(intfhub_base_remap_addr)));

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0xf4,INTFHUB_DBG=[0x%x],assert_bit[0]=[%d]",
		def_tag, ((tag == NULL) ? "null" : tag), val, (val & 0x1));

	pr_info("%s\n", dmp_info_buf);

	pr_info("[%s][%s] INTFHUB_DEVx_STA=[d0(0x0):0x%x, d1(0x40):0x%x, d2(0x80):0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_STA(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_STA(
			intfhub_base_remap_addr)) : 0),
		" intfhub_ready[9],intfhub_busy[8],hw_tx_busy[3],hw_rx_busy[2],sw_tx_sta[1],sw_rx_sta[0]");

	pr_info("[%s][%s] INTFHUB_DEVx_PKT_CNT=[d0(0x1c):0x%x, d1(0x50):0x%x, d2(0x90):0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(
			intfhub_base_remap_addr)) : 0),
		" tx_pkt_cnt[31:24],tx_timeout_cnt[23:16],rx_pkt_cnt[15:8],rx_timeout_cnt[7:0]");

	pr_info("[%s][%s] INTFHUB_DEVx_CRC_STA=[d0(0x20):0x%x, d1(0x54):0x%x, d2(0x94):0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_CRC_STA(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_CRC_STA(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_CRC_STA(
			intfhub_base_remap_addr)) : 0),
		" rx_crc_data[31:16],tx_crc_result[15:0]");

	pr_info("[%s][%s] INTFHUB_DEVx_RX_ERR_CRC_STA=[d0(0x10):0x%x, d1(0x14):0x%x, d2(0x18):0x%x],%s\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_RX_ERR_CRC_STA(intfhub_base_remap_addr)),
		((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_RX_ERR_CRC_STA(
			intfhub_base_remap_addr)) : 0),
		((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_RX_ERR_CRC_STA(
			intfhub_base_remap_addr)) : 0),
		" rx_err_crc_result[31:16],rx_err_crc_data[15:0]");

	pr_info("[%s][%s] 0x30,INTFHUB_DEV0_IRQ_STA=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr)));

	pr_info("[%s][%s] 0xd0,INTFHUB_IRQ_STA=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_IRQ_STA(intfhub_base_remap_addr)));

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr));
	pr_info("[%s][%s] 0xe0,INTFHUB_STA0=[0x%x], intfhub_busy[1]=[%d], intfhub_active[0]=[%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag), val,
		((val & 0x2) >> 1), (val & 0x1));

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr));
	pr_info("[%s][%s] 0xc8,INTFHUB_CON2=[0x%x], crc_en[1]=[%d],bypass[2]=[%d]\n",
		def_tag, ((tag == NULL) ? "null" : tag), val,
		((val & 0x2) >> 1), ((val & 0x4) >> 2));

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_CON3(intfhub_base_remap_addr));
	pr_info("[%s][%s] 0xcc,INTFHUB_CON3=[0x%x], dev_timeout_time[27:24]=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag), val, ((val & 0xF000000) >> 24));

	if (uarthub_core_debug_uart_ip_info_with_tag_ex(tag, 0) == -2) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		pr_info("[%s][%s] ----------------------------------------\n",
			def_tag, ((tag == NULL) ? "null" : tag));
		return -1;
	}

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] 0x9c,FEATURE_SEL=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0x24,HIGHSPEED=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_HIGHSPEED(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_HIGHSPEED(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_HIGHSPEED(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_HIGHSPEED(cmm_base_remap_addr)));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] 0x28,SAMPLE_COUNT=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0x2c,SAMPLE_POINT=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0x90,DLL=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_DLL(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_DLL(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_DLL(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DLL(cmm_base_remap_addr)));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] 0x54,FRACDIV_L=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_FRACDIV_L(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_FRACDIV_L(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_FRACDIV_L(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FRACDIV_L(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0x58,FRACDIV_M=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_FRACDIV_M(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_FRACDIV_M(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_FRACDIV_M(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FRACDIV_M(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0x4c,DMA_EN=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_DMA_EN(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_DMA_EN(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_DMA_EN(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_DMA_EN(cmm_base_remap_addr)));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] 0x08,IIR_FCR=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_IIR_FCR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_IIR_FCR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_IIR_FCR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_IIR_FCR(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0xc,LCR=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_LCR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_LCR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_LCR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_LCR(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0x98,EFR=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_EFR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_EFR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_EFR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_EFR(cmm_base_remap_addr)));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] 0xa0,XON1=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_XON1(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_XON1(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_XON1(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XON1(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0xa8,XOFF1=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_XOFF1(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_XOFF1(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_XOFF1(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XOFF1(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0xa4,XON2=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_XON2(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_XON2(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_XON2(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XON2(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0xac,XOFF2=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_XOFF2(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_XOFF2(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_XOFF2(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_XOFF2(cmm_base_remap_addr)));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] 0x44,ESCAPE_EN=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(cmm_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___0x40,ESCAPE_DAT=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]",
		UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(cmm_base_remap_addr)));

	pr_info("%s\n", dmp_info_buf);

	pr_info("[%s][%s] 0x5c,FCR_RD,RFTL1_RFTL0[7:6],TFTL1_TFTL0[5:4],FIFOE[0]=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_FCR_RD(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_FCR_RD(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_FCR_RD(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_FCR_RD(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x10,MCR,XOFF_STATUS[7],Loop[4]=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_MCR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_MCR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_MCR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_MCR(cmm_base_remap_addr)));

	pr_info("[%s][%s] 0x14,%s=[d0:0x%x, d1:0x%x, d2:0x%x, cmm:0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		"LSR,FIFOERR[7],TEMT[6],THRE[5],DR[0],(idle=0x60)",
		UARTHUB_REG_READ(UARTHUB_LSR(dev0_base_remap_addr)),
		((g_max_dev >= 2) ?
			UARTHUB_REG_READ(UARTHUB_LSR(dev1_base_remap_addr)) : 0),
		((g_max_dev >= 3) ?
			UARTHUB_REG_READ(UARTHUB_LSR(dev2_base_remap_addr)) : 0),
		UARTHUB_REG_READ(UARTHUB_LSR(cmm_base_remap_addr)));

	pr_info("[%s][%s] ----------------------------------------\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	return 0;
}

/*---------------------------------------------------------------------------*/

module_init(uarthub_core_init);
module_exit(uarthub_core_exit);

/*---------------------------------------------------------------------------*/

MODULE_AUTHOR("CTD/SE5/CS5/Johnny.Yao");
MODULE_DESCRIPTION("MTK UARTHUB Driver$1.0$");
MODULE_LICENSE("GPL");
