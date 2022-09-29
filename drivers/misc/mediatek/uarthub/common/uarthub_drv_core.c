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
#include <linux/sched/clock.h>
#include <linux/timer.h>

static atomic_t g_uarthub_probe_called = ATOMIC_INIT(0);
struct platform_device *g_uarthub_pdev;
UARTHUB_CORE_IRQ_CB g_core_irq_callback;
unsigned int g_max_dev;
int g_uarthub_open;
struct clk *clk_apmixedsys_univpll;
int g_uarthub_disable;
static struct notifier_block uarthub_fb_notifier;
struct workqueue_struct *uarthub_workqueue;
static int g_last_err_type = -1;
static struct timespec64 tv_now_assert, tv_end_assert;
static int g_univpll_clk_ref_count;
struct mutex g_lock_univpll_clk;
struct mutex g_lock_dump_log;

#define DBG_LOG_LEN 1024

#define CLK_CTRL_UNIVPLL_REQ 1
#define INIT_UARTHUB_DEFAULT 0
#define UARTHUB_INFO_LOG 1
#define UARTHUB_DEBUG_LOG 0
#define UARTHUB_CONFIG_TRX_GPIO 0
#define SUPPORT_SSPM_DRIVER 1
#define UARTHUB_DEFAULT_DUMP_DEBUG_LOOP_MS 10
#define UARTHUB_DUMP_DEBUG_LOOP_ENABLE 0
#define UARTHUB_DUMP_DEBUG_LOOP_MODE 0
#define UARTHUB_ENABLE_UART_1_CHANNEL 1

#define DUMP_AP_UART_DBG_INFO 0

#if !(SUPPORT_SSPM_DRIVER)
#ifdef INIT_UARTHUB_DEFAULT
#undef INIT_UARTHUB_DEFAULT
#endif
#define INIT_UARTHUB_DEFAULT 1
#endif

struct uarthub_reg_base_addr reg_base_addr;
struct uarthub_ops_struct *g_uarthub_plat_ic_ops;
static struct hrtimer dump_hrtimer;

int g_enable_dump_loop;
int g_dump_loop_dur_ms = UARTHUB_DEFAULT_DUMP_DEBUG_LOOP_MS;

void __iomem *cmm_base_remap_addr;
void __iomem *dev0_base_remap_addr;
void __iomem *dev1_base_remap_addr;
void __iomem *dev2_base_remap_addr;
void __iomem *intfhub_base_remap_addr;

static int mtk_uarthub_probe(struct platform_device *pdev);
static int mtk_uarthub_remove(struct platform_device *pdev);
static int mtk_uarthub_suspend(struct device *dev);
static int mtk_uarthub_resume(struct device *dev);
static int uarthub_core_init(void);
static void uarthub_core_exit(void);
static irqreturn_t uarthub_irq_isr(int irq, void *arg);
static void trigger_uarthub_frame_error_worker_handler(struct work_struct *work);
static void trigger_uarthub_error_worker_handler(struct work_struct *work);
static void debug_info_worker_handler(struct work_struct *work);
static void debug_clk_info_work_worker_handler(struct work_struct *work);
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

static const struct dev_pm_ops uarthub_drv_pm_ops = {
	.suspend_noirq = mtk_uarthub_suspend,
	.resume_noirq = mtk_uarthub_resume,
};

struct platform_driver mtk_uarthub_dev_drv = {
	.probe = mtk_uarthub_probe,
	.remove = mtk_uarthub_remove,
	.driver = {
			.name = "mtk_uarthub",
			.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
			.of_match_table = apuarthub_of_ids,
#endif
			.pm = &uarthub_drv_pm_ops,
	}
};

static struct assert_ctrl uarthub_assert_ctrl;
static struct debug_info_ctrl uarthub_debug_info_ctrl;
static struct work_struct trigger_frame_error_work;
static struct debug_info_ctrl uarthub_debug_clk_info_ctrl;

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

static int mtk_uarthub_resume(struct device *dev)
{
	const char *prefix = "##########";
	const char *postfix = "##############";

	pr_info("[%s] %s uarthub enter resume %s\n",
		__func__, prefix, postfix);

#if UARTHUB_INFO_LOG
	uarthub_core_debug_clk_info("HUB_DBG_RESUME");
#endif

	return 0;
}

static int mtk_uarthub_suspend(struct device *dev)
{
	const char *prefix = "##########";
	const char *postfix = "##############";

	pr_info("[%s] %s uarthub enter suspend %s\n",
		__func__, prefix, postfix);

#if UARTHUB_DEBUG_LOG
	uarthub_core_debug_clk_info("HUB_DBG_RESUME");
#endif

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

#if UARTHUB_INFO_LOG
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

	INIT_WORK(&trigger_frame_error_work, trigger_uarthub_frame_error_worker_handler);
	INIT_WORK(&uarthub_assert_ctrl.trigger_assert_work, trigger_uarthub_error_worker_handler);
	INIT_WORK(&uarthub_debug_info_ctrl.debug_info_work, debug_info_worker_handler);
	INIT_WORK(&uarthub_debug_clk_info_ctrl.debug_info_work, debug_clk_info_work_worker_handler);

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

	mutex_init(&g_lock_univpll_clk);
	mutex_init(&g_lock_dump_log);

	hrtimer_init(&dump_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dump_hrtimer.function = dump_hrtimer_handler_cb;
#if UARTHUB_DUMP_DEBUG_LOOP_ENABLE
	uarthub_core_dump_trx_info_loop_ctrl(1, UARTHUB_DEFAULT_DUMP_DEBUG_LOOP_MS);
	uarthub_core_dump_trx_info_loop_trigger();
#endif

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
#if UARTHUB_DUMP_DEBUG_LOOP_MODE
	hrtimer_forward_now(hrt, ns_to_ktime(1000 * 10));
#else
	hrtimer_forward_now(hrt, ms_to_ktime(g_dump_loop_dur_ms));
#endif

#if UARTHUB_DUMP_DEBUG_LOOP_MODE
	uarthub_core_debug_uart_ip_info_loop_compare_diff();
#else
	uarthub_core_debug_uart_ip_info_loop();
#endif

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
#if UARTHUB_DEBUG_LOG
			uarthub_core_debug_info_with_tag_worker("UNBLANK_CB");
#endif
#if UARTHUB_INFO_LOG
		uarthub_core_debug_clk_info_worker("HUB_DBG_UNBLANK_CB");
#endif
#if UARTHUB_DUMP_DEBUG_LOOP_ENABLE
			uarthub_core_dump_trx_info_loop_trigger();
#endif
		} else if (data == MTK_DISP_BLANK_POWERDOWN) {
#if UARTHUB_DEBUG_LOG
			uarthub_core_debug_info_with_tag_worker("POWERDOWN_CB");
#endif
#if UARTHUB_INFO_LOG
		uarthub_core_debug_clk_info_worker("HUB_DBG_PWRDWN_CB");
#endif
#if UARTHUB_DUMP_DEBUG_LOOP_ENABLE
			uarthub_core_dump_trx_info_loop_trigger();
#endif
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

	if (!intfhub_base_remap_addr || uarthub_core_is_apb_bus_clk_enable() == 0)
		return IRQ_HANDLED;

	/* mask irq */
	UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
		0x3FFFF, 0x3FFFF);

	err_type = uarthub_core_check_irq_err_type();
	if (err_type > 0) {
		uarthub_core_set_trigger_uarthub_error_worker(err_type);
	} else {
		/* clear irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
		/* unmask irq */
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
			0x0, 0x3FFFF);
	}

	uarthub_core_set_trigger_uarthub_frame_error_worker();

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

	if (mask == 1) {
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
	} else {
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr),
			0x0, 0x3FFFF);
	}

	return 0;
}

int uarthub_core_irq_clear_ctrl(int err_type)
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

	if (err_type < 0) {
		UARTHUB_REG_WRITE_MASK(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
			0x3FFFF, 0x3FFFF);
	} else {
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_DEV0_IRQ_CLR(intfhub_base_remap_addr),
			err_type);
	}

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
	struct timespec64 now;

	if (g_max_dev <= 0)
		return -1;

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		irq_num = irq_of_parse_and_map(node, 0);
		irq_flag = irq_get_trigger_type(irq_num);
		pr_info("[%s] get irq id(%d) and irq trigger flag(%d) from DT\n",
			__func__, irq_num, irq_flag);

		ktime_get_real_ts64(&now);
		tv_end_assert.tv_sec = now.tv_sec;
		tv_end_assert.tv_nsec = now.tv_nsec;
		tv_end_assert.tv_sec += 1;

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

int uarthub_core_irq_free(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	int irq_num = 0;

	uarthub_core_irq_mask_ctrl(0);
	uarthub_core_irq_clear_ctrl(-1);

	if (pdev)
		node = pdev->dev.of_node;

	if (node) {
		irq_num = irq_of_parse_and_map(node, 0);
		pr_info("[%s] get irq id(%d) from DT\n",
			__func__, irq_num);

		free_irq(irq_num, NULL);
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

#if UARTHUB_INFO_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
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

#if UARTHUB_INFO_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	uarthub_core_irq_free(g_uarthub_pdev);

	uarthub_core_dev0_clear_txrx_request();
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

	if (state == 1) {
#if UARTHUB_INFO_LOG
		uarthub_core_debug_clk_info("HUB_DBG_SetTX_E");
#endif
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
				/* 0x08 = 0x7, fifo control register */
				UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(uarthub_dev_base), 0x7);
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

#if UARTHUB_DEBUG_LOG
				if (i != g_max_dev)
					pr_info("[%s] UARTHUB_DEV_%d OPEN done.\n", __func__, i);
				else
					pr_info("[%s] UARTHUB_DEV_CMM OPEN done.\n", __func__);
#endif
			}
		}
#endif
	}

	return (state == 1) ? 1 : 0;
}

int uarthub_core_get_host_wakeup_status(void)
{
	int state = 0;
	int state0 = 0, state1 = 0, state2 = 0;

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

	state0 = ((UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(
		intfhub_base_remap_addr), 0x3) == 0x2) ? 1 : 0);
	if (g_max_dev >= 2) {
		state1 = ((UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV1_STA(
			intfhub_base_remap_addr), 0x3) == 0x2) ? 1 : 0);
	}
	if (g_max_dev >= 3) {
		state2 = ((UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV2_STA(
			intfhub_base_remap_addr), 0x3) == 0x2) ? 1 : 0);
	}

	state = (state0 | (state1 << 1) | (state2 << 2));

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_get_host_set_fw_own_status(void)
{
	int state = 0;
	int state0 = 0, state1 = 0, state2 = 0;

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

	state0 = ((UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(
		intfhub_base_remap_addr), 0x3) == 0x1) ? 1 : 0);
	if (g_max_dev >= 2) {
		state1 = ((UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV1_STA(
			intfhub_base_remap_addr), 0x3) == 0x1) ? 1 : 0);
	}
	if (g_max_dev >= 3) {
		state2 = ((UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV2_STA(
			intfhub_base_remap_addr), 0x3) == 0x1) ? 1 : 0);
	}

	state = (state0 | (state1 << 1) | (state2 << 2));

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
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

#if UARTHUB_INFO_LOG
	pr_info("[%s] rx=[%d], state=[%d]\n", __func__, rx, state);
#endif

	return (state == 0) ? 1 : 0;
}


#define UARTHUB_IRQ_OP_LOG_SIZE     5
#define UARTHUB_LOG_IRQ_PKT_SIZE    12
#define UARTHUB_LOG_IRQ_IDX_ADDR(addr) (addr)

#define UARTHUB_TSK_OP_LOG_SIZE     20
#define UARTHUB_LOG_TSK_PKT_SIZE    20
#define UARTHUB_LOG_TSK_IDX_ADDR(addr) \
		(addr + (UARTHUB_LOG_IRQ_PKT_SIZE * UARTHUB_IRQ_OP_LOG_SIZE) + 4)

#define UARTHUB_CK_CNT_ADDR(addr) \
	(UARTHUB_LOG_TSK_IDX_ADDR(addr) + (UARTHUB_TSK_OP_LOG_SIZE * UARTHUB_LOG_TSK_PKT_SIZE) + 4)


#define UARTHUB_LAST_CK_ON(addr) (UARTHUB_CK_CNT_ADDR(addr) + 4)
#define UARTHUB_LAST_CK_ON_CNT(addr) (UARTHUB_LAST_CK_ON(addr) + 8)

#define UARTHUB_TMP_BUF_SZ  512
char g_buf[UARTHUB_TMP_BUF_SZ];

int uarthub_core_dump_sspm_log(void)
{
	void __iomem *sys_sram_base_remap_addr = NULL;
	void __iomem *log_addr = NULL;
	int i, n, used;
	uint32_t val, irq_idx = 0, tsk_idx = 0;
	uint32_t v1, v2, v3;
	uint64_t t;
	char *tmp;

	if (g_uarthub_plat_ic_ops == NULL ||
		g_uarthub_plat_ic_ops->uarthub_plat_get_sys_sram_base_addr == NULL)
		return 0;

	sys_sram_base_remap_addr =
		g_uarthub_plat_ic_ops->uarthub_plat_get_sys_sram_base_addr();

	g_buf[0] = '\0';
	log_addr = UARTHUB_LOG_IRQ_IDX_ADDR(sys_sram_base_remap_addr);
	irq_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;

	tmp = g_buf;
	used = 0;
	for (i = 0; i < UARTHUB_IRQ_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%X] ",
							t,
							UARTHUB_REG_READ(log_addr + 8));
		if (n > 0)
			used += n;
		log_addr += UARTHUB_LOG_IRQ_PKT_SIZE;
	}
	pr_info("[%s][%x] %s", __func__, irq_idx, g_buf);

	log_addr = UARTHUB_LOG_TSK_IDX_ADDR(sys_sram_base_remap_addr);
	tsk_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;
	g_buf[0] = '\0';
	tmp = g_buf;
	used = 0;
	for (i = 0; i < UARTHUB_TSK_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%x-%x-%x]",
							t,
							UARTHUB_REG_READ(log_addr + 8),
							UARTHUB_REG_READ(log_addr + 12),
							UARTHUB_REG_READ(log_addr + 16));
		if (n > 0) {
			used += n;
			if ((i % (UARTHUB_TSK_OP_LOG_SIZE/2))
					== ((UARTHUB_TSK_OP_LOG_SIZE/2) - 1)) {
				pr_info("[%s][%x] %s", __func__, tsk_idx, g_buf);
				g_buf[0] = '\0';
				tmp = g_buf;
				used = 0;
			}
		}
		log_addr += UARTHUB_LOG_TSK_PKT_SIZE;
	}

	log_addr = UARTHUB_CK_CNT_ADDR(sys_sram_base_remap_addr);
	val = UARTHUB_REG_READ(log_addr);

	log_addr = UARTHUB_LAST_CK_ON(sys_sram_base_remap_addr);
	v1 = UARTHUB_REG_READ(log_addr);
	v2 = UARTHUB_REG_READ(log_addr + 4);

	log_addr = UARTHUB_LAST_CK_ON_CNT(sys_sram_base_remap_addr);
	v3 = UARTHUB_REG_READ(log_addr);

	pr_info("[%s] off/on cnt=[%d][%d] ckon=[%x][%x] cnt=[%x]", __func__,
			(val & 0xFFFF), (val >> 16),
			v1, v2, v3);

	return 0;
}

int uarthub_core_dev0_set_tx_request(void)
{
	int retry = 0;
	int val = 0;
	uint32_t timer_h = 0, timer_l = 0;

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

#if UARTHUB_INFO_LOG
	uarthub_core_debug_clk_info("HUB_DBG_SetTX_B");
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_SET(intfhub_base_remap_addr), 0x2);

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_spm_sys_timer)
		g_uarthub_plat_ic_ops->uarthub_plat_get_spm_sys_timer(&timer_h, &timer_l);

#if !(SUPPORT_SSPM_DRIVER)
	UARTHUB_SET_BIT(UARTHUB_INTFHUB_IRQ_CLR(intfhub_base_remap_addr), (0x1 << 0));
	pr_info("[%s] is_ready=[%d] sys_timer[%x][%x]\n", __func__,
					uarthub_core_dev0_is_uarthub_ready(),
					timer_h, timer_l);
#if UARTHUB_DEBUG_LOG
	uarthub_core_debug_info_with_tag(__func__);
#endif
#endif

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		retry = 20;
		while (retry-- > 0) {
			val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info();
			if (val == 1) {
#if UARTHUB_DEBUG_LOG
				pr_info("[%s] hw_ccf_pll on pass, retry=[%d]\n",
					__func__, retry);
#endif
				break;
			}
			usleep_range(1000, 1100);
		}

		if (val == 0) {
			pr_notice("[%s] hw_ccf_pll_on fail, retry=[%d]\n",
				__func__, retry);
		} else if (val < 0) {
			pr_notice("[%s] hw_ccf_pll_on info cannot be read, retry=[%d]\n",
				__func__, retry);
		}
	}

	return 0;
}

int uarthub_core_dev0_set_rx_request(void)
{
#if UARTHUB_INFO_LOG
	uint32_t timer_h = 0, timer_l = 0;
#endif

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

#if UARTHUB_INFO_LOG
	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_spm_sys_timer)
		g_uarthub_plat_ic_ops->uarthub_plat_get_spm_sys_timer(&timer_h, &timer_l);

	pr_info("[%s] sys_timer=[%x][%x]\n", __func__, timer_h, timer_l);
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_SET(intfhub_base_remap_addr), 0x1);

	return 0;
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

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_SET(intfhub_base_remap_addr), 0x3);

#if !(SUPPORT_SSPM_DRIVER)
	UARTHUB_SET_BIT(UARTHUB_INTFHUB_IRQ_CLR(intfhub_base_remap_addr), (0x1 << 0));
	pr_info("[%s] is_ready=[%d]\n", __func__, uarthub_core_dev0_is_uarthub_ready());
#if UARTHUB_DEBUG_LOG
	uarthub_core_debug_info_with_tag(__func__);
#endif
#endif

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		retry = 20;
		while (retry-- > 0) {
			val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info();
			if (val == 1) {
				pr_info("[%s] hw_ccf_pll on pass, retry=[%d]\n",
					__func__, retry);
				break;
			}
			usleep_range(1000, 1100);
		}

		if (val == 0) {
			pr_notice("[%s] hw_ccf_pll_on fail, retry=[%d]\n",
				__func__, retry);
		} else if (val < 0) {
			pr_notice("[%s] hw_ccf_pll_on info cannot be read, retry=[%d]\n",
				__func__, retry);
		}
	}

	return 0;
}

int uarthub_core_dev0_clear_tx_request(void)
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

#if UARTHUB_INFO_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_CLR(intfhub_base_remap_addr), 0x2);

	return 0;
}

int uarthub_core_dev0_clear_rx_request(void)
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

#if UARTHUB_INFO_LOG
	uarthub_core_debug_byte_cnt_info("HUB_DBG_ClrRX");
#endif

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_CLR(intfhub_base_remap_addr), 0x5);

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

#if UARTHUB_INFO_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif

	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_CLR(intfhub_base_remap_addr), 0x7);

	return 0;
}

int uarthub_core_get_uart_cmm_rx_count(void)
{
	int debug5_cmm = 0;
	int debug6_cmm = 0;
	int cmm_rx_cnt = 0;

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

	debug5_cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_5(cmm_base_remap_addr));
	debug6_cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_6(cmm_base_remap_addr));

	cmm_rx_cnt = (((debug5_cmm & 0xF0) >> 4) + ((debug6_cmm & 0x3) << 4));
#if UARTHUB_DEBUG_LOG
	pr_info("[%s] cmm_rx_cnt=[%d]\n", __func__, cmm_rx_cnt);
#endif

	return cmm_rx_cnt;
}

int uarthub_core_dump_trx_info_loop_trigger(void)
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
		(0x1 << 1)) >> 1);

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[%d], g_enable_dump_loop=[%d]\n", __func__, state, g_enable_dump_loop);
#endif

	if (state == 0 && g_enable_dump_loop == 1) {
		g_enable_dump_loop = 0;
		hrtimer_cancel(&dump_hrtimer);
	} else if (state == 1 && g_enable_dump_loop == 0) {
		g_enable_dump_loop = 1;
		hrtimer_start(&dump_hrtimer, 0, HRTIMER_MODE_REL);
	}

	return 0;
}

int uarthub_core_dump_trx_info_loop_ctrl(int enable, int loop_dur_ms)
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

	if (enable == 1) {
		g_dump_loop_dur_ms = loop_dur_ms;
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 1));
	} else {
		g_dump_loop_dur_ms = UARTHUB_DEFAULT_DUMP_DEBUG_LOOP_MS;
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr), (0x1 << 1));
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d], loop_dur_ms=[%d]\n", __func__, enable, loop_dur_ms);
#endif

	if (enable == 0 && g_enable_dump_loop == 1) {
		g_enable_dump_loop = 0;
		hrtimer_cancel(&dump_hrtimer);
	} else if (enable == 1 && g_enable_dump_loop == 0) {
		g_enable_dump_loop = 1;
		hrtimer_start(&dump_hrtimer, 0, HRTIMER_MODE_REL);
	}

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
#if UARTHUB_DEBUG_LOG
	unsigned int before_pll_sta = 0;
#endif

	if (g_max_dev <= 0)
		return -1;

	if (clk_apmixedsys_univpll == NULL || IS_ERR(clk_apmixedsys_univpll))
		return -2;

	if (mutex_lock_killable(&g_lock_univpll_clk))
		return -5;

	if (clk_on == 1 && g_univpll_clk_ref_count >= 1) {
		mutex_unlock(&g_lock_univpll_clk);
		return 0;
	} else if (clk_on == 0 && g_univpll_clk_ref_count <= 0) {
		mutex_unlock(&g_lock_univpll_clk);
		return 0;
	}

#if UARTHUB_DEBUG_LOG
	before_pll_sta = uarthub_core_is_univpll_on();
#endif

	if (clk_on == 1) {
		ret = clk_prepare_enable(clk_apmixedsys_univpll);
		if (ret) {
			pr_notice("[%s] UNIVPLL ON fail(%d)\n", __func__, ret);
			mutex_unlock(&g_lock_univpll_clk);
			return ret;
		}
		g_univpll_clk_ref_count++;

#if UARTHUB_DEBUG_LOG
		pr_info("[%s] UNIVPLL ON pass, ref_cnt=[%d], pll_sta=[%d --> %d] +\n",
			__func__, g_univpll_clk_ref_count,
			before_pll_sta, uarthub_core_is_univpll_on());
#endif
	} else {
		g_univpll_clk_ref_count--;
		clk_disable_unprepare(clk_apmixedsys_univpll);

#if UARTHUB_DEBUG_LOG
		pr_info("[%s] UNIVPLL OFF pass, ref_cnt=[%d], pll_sta=[%d --> %d] -\n",
			__func__, g_univpll_clk_ref_count,
			before_pll_sta, uarthub_core_is_univpll_on());
#endif
	}

	mutex_unlock(&g_lock_univpll_clk);
	return 0;
}

int uarthub_core_is_univpll_on(void)
{
	if (g_max_dev <= 0 || g_uarthub_plat_ic_ops == NULL)
		return 0;

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info)
		return g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info();

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

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1) {
		uarthub_core_irq_mask_ctrl(1);
		uarthub_core_irq_clear_ctrl(-1);
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 2));
	} else {
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr), (0x1 << 2));
		uarthub_core_irq_clear_ctrl(-1);
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

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif

	if (enable == 1) {
		uarthub_core_reset_to_ap_enable_only(1);
	} else {
#if UARTHUB_ENABLE_UART_1_CHANNEL
		if (g_max_dev >= 2)
			UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev1_base_remap_addr), 0x1);
#endif

		if (g_max_dev >= 3)
			UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev2_base_remap_addr), 0x1);

#if UARTHUB_INFO_LOG
		uarthub_core_debug_info_with_tag(__func__);
#endif
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

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		return -4;
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

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		return -4;
	}

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] rate_index=[%d]\n", __func__, rate_index);
#endif

	iRtn = uarthub_core_config_baud_rate(cmm_base_remap_addr, rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config external baud rate fail(%d), rate_index=[%d]\n",
			__func__, iRtn, rate_index);
		return -2;
	}

	return 0;
}

void uarthub_core_set_trigger_uarthub_frame_error_worker(void)
{
	queue_work(uarthub_workqueue, &trigger_frame_error_work);
}

static void trigger_uarthub_frame_error_worker_handler(struct work_struct *work)
{
	int uart_0_fe_value = 0;
	int uart_1_fe_value = 0;
	int uart_2_fe_value = 0;
	int uart_cmm_fe_value = 0;

	uarthub_core_clk_univpll_ctrl(1);
	if (uarthub_core_is_univpll_on() == 0) {
		pr_notice("[%s] uarthub_core_is_univpll_on=[0]\n", __func__);
		uarthub_core_clk_univpll_ctrl(0);
		return;
	}

	uart_0_fe_value = ((UARTHUB_REG_READ(UARTHUB_LSR(dev0_base_remap_addr)) & 0x8) >> 3);
	uart_1_fe_value = ((UARTHUB_REG_READ(UARTHUB_LSR(dev1_base_remap_addr)) & 0x8) >> 3);
	uart_2_fe_value = ((UARTHUB_REG_READ(UARTHUB_LSR(dev2_base_remap_addr)) & 0x8) >> 3);
	uart_cmm_fe_value = ((UARTHUB_REG_READ(UARTHUB_LSR(cmm_base_remap_addr)) & 0x8) >> 3);
	uarthub_core_clk_univpll_ctrl(0);

	if (uart_0_fe_value == 1)
		pr_notice("[%s] UART_0 detect frame error\n", __func__);

	if (uart_1_fe_value == 1)
		pr_notice("[%s] UART_1 detect frame error\n", __func__);

	if (uart_2_fe_value == 1)
		pr_notice("[%s] UART_2 detect frame error\n", __func__);

	if (uart_cmm_fe_value == 1)
		pr_notice("[%s] UART_CMM detect frame error\n", __func__);

	if ((uart_0_fe_value + uart_1_fe_value + uart_2_fe_value + uart_cmm_fe_value) != 0)
		uarthub_core_debug_info_with_tag(__func__);
}

void uarthub_core_set_trigger_uarthub_error_worker(int err_type)
{
	uarthub_assert_ctrl.err_type = err_type;
	queue_work(uarthub_workqueue, &uarthub_assert_ctrl.trigger_assert_work);
}

static void trigger_uarthub_error_worker_handler(struct work_struct *work)
{
	struct assert_ctrl *queue = container_of(work, struct assert_ctrl, trigger_assert_work);
	int err_type = (int) queue->err_type;
	int id = 0;
	int err_total = 0;
	int err_index = 0;
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	tv_now_assert.tv_sec = now.tv_sec;
	tv_now_assert.tv_nsec = now.tv_nsec;

	if (g_last_err_type == err_type) {
		if ((((tv_now_assert.tv_sec == tv_end_assert.tv_sec) &&
				(tv_now_assert.tv_nsec > tv_end_assert.tv_nsec)) ||
				(tv_now_assert.tv_sec > tv_end_assert.tv_sec)) == false) {
			uarthub_core_irq_clear_ctrl(err_type);
			uarthub_core_irq_mask_ctrl(0);
			tv_end_assert = tv_now_assert;
			tv_end_assert.tv_sec += 1;
			return;
		}
	} else
		g_last_err_type = err_type;

	tv_end_assert = tv_now_assert;
	tv_end_assert.tv_sec += 1;

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
					pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], hub_irq_cb=[NULL]\n",
						__func__, err_total, err_index,
						id, UARTHUB_irq_err_type_str[id]);
				} else {
					pr_info("[%s] %d-%d, err_id=[%d], reason=[%s], hub_irq_cb=[%p]\n",
						__func__, err_total, err_index, id,
						UARTHUB_irq_err_type_str[id], g_core_irq_callback);
				}
			}
		}
	}

	if (!intfhub_base_remap_addr || uarthub_core_is_apb_bus_clk_enable() == 0) {
		uarthub_core_irq_clear_ctrl(err_type);
		uarthub_core_irq_mask_ctrl(0);
		return;
	}

	if (uarthub_core_is_bypass_mode() == 1) {
		pr_info("[%s] ignore irq error in bypass mode\n", __func__);
		uarthub_core_irq_mask_ctrl(1);
		uarthub_core_irq_clear_ctrl(-1);
		return;
	}

	if (uarthub_core_is_assert_state() == 1) {
		pr_info("[%s] ignore irq error if assert flow\n", __func__);
		uarthub_core_irq_clear_ctrl(err_type);
		uarthub_core_irq_mask_ctrl(0);
		return;
	}

	uarthub_core_debug_info_with_tag(__func__);

	uarthub_core_irq_clear_ctrl(err_type);
	uarthub_core_irq_mask_ctrl(0);

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

	if (assert_ctrl == uarthub_core_is_assert_state()) {
		if (assert_ctrl == 1)
			pr_info("[%s] assert state has been set\n", __func__);
		else
			pr_info("[%s] assert state has been cleared\n", __func__);
		return 0;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] assert_ctrl=[%d]\n", __func__, assert_ctrl);
#endif

	if (assert_ctrl == 1) {
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr),
			(0x1 << 0));
	} else {
		UARTHUB_CLR_BIT(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr),
			(0x1 << 0));
	}

	return 0;
}

int uarthub_core_reset_flow_control(void)
{
	void __iomem *uarthub_dev_base = NULL;
	struct uarthub_uart_ip_debug_info debug1 = {0};
	struct uarthub_uart_ip_debug_info debug8 = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;
	int retry = 0;
	int i = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s] uarthub_core_is_uarthub_clk_enable=[0]\n", __func__);
		return -2;
	}

	debug8.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr));
	debug8.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev1_base_remap_addr));
	debug8.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev2_base_remap_addr));
	debug8.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr));

	if (((debug8.dev0 & 0x8) >> 3) == 0 && ((debug8.dev1 & 0x8) >> 3) == 0 &&
			((debug8.dev2 & 0x8) >> 3) == 0 && ((debug8.cmm & 0x8) >> 3) == 0)
		return 0;

	debug1.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr));
	debug1.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev1_base_remap_addr));
	debug1.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev2_base_remap_addr));
	debug1.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr));

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][BEGIN] xcstate(wsend_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		__func__,
		((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		", swtxdis(det_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		((debug8.dev0 & 0x8) >> 3),
		((debug8.dev1 & 0x8) >> 3),
		((debug8.dev2 & 0x8) >> 3),
		((debug8.cmm & 0x8) >> 3));

	pr_info("%s\n", dmp_info_buf);

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

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(UARTHUB_DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		UARTHUB_REG_WRITE(UARTHUB_MCR(uarthub_dev_base), 0x10);
		UARTHUB_REG_WRITE(UARTHUB_DMA_EN(uarthub_dev_base), 0x0);
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(uarthub_dev_base), 0x0);
		UARTHUB_REG_WRITE(UARTHUB_SLEEP_REQ(uarthub_dev_base), 0x1);
		UARTHUB_REG_WRITE(UARTHUB_SLEEP_EN(uarthub_dev_base), 0x1);

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(UARTHUB_DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		debug8.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr));
		debug8.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev1_base_remap_addr));
		debug8.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev2_base_remap_addr));
		debug8.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr));
		debug1.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr));
		debug1.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev1_base_remap_addr));
		debug1.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev2_base_remap_addr));
		debug1.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr));

		len = 0;
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][SLEEP_REQ][%d] xcstate(wsend_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			__func__, i,
			((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0));

		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			", swtxdis(det_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			((debug8.dev0 & 0x8) >> 3),
			((debug8.dev1 & 0x8) >> 3),
			((debug8.dev2 & 0x8) >> 3),
			((debug8.cmm & 0x8) >> 3));

		pr_info("%s\n", dmp_info_buf);

		UARTHUB_REG_WRITE(UARTHUB_SLEEP_REQ(uarthub_dev_base), 0x0);
		UARTHUB_REG_WRITE(UARTHUB_SLEEP_EN(uarthub_dev_base), 0x0);

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(UARTHUB_DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(uarthub_dev_base), 0x1);
		UARTHUB_REG_WRITE(UARTHUB_DMA_EN(uarthub_dev_base), 0x3);
		UARTHUB_REG_WRITE(UARTHUB_MCR(uarthub_dev_base), 0x0);

		debug8.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr));
		debug8.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev1_base_remap_addr));
		debug8.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev2_base_remap_addr));
		debug8.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr));
		debug1.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr));
		debug1.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev1_base_remap_addr));
		debug1.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev2_base_remap_addr));
		debug1.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr));

		len = 0;
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][SLEEP_REQ_DIS][%d] xcstate(wsend_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			__func__, i,
			((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0));

		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			", swtxdis(det_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			((debug8.dev0 & 0x8) >> 3),
			((debug8.dev1 & 0x8) >> 3),
			((debug8.dev2 & 0x8) >> 3),
			((debug8.cmm & 0x8) >> 3));

		pr_info("%s\n", dmp_info_buf);
	}

	return 0;
}

int uarthub_core_reset(void)
{
#if UARTHUB_INFO_LOG
	pr_info("[%s] g_max_dev=[%d]\n", __func__, g_max_dev);
#endif
	return uarthub_core_reset_to_ap_enable_only(0);
}

int uarthub_core_reset_to_ap_enable_only(int ap_only)
{
	int trx_state = -1;
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

	trx_state = UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr), 0x3);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] g_max_dev=[%d], dev1_fifoe=[%d], dev2_fifoe=[%d], trx_state=[0x%x]\n",
		__func__, g_max_dev, dev1_fifoe, dev2_fifoe, trx_state);
#endif

	/* set trx request */
	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA_SET(intfhub_base_remap_addr), 0x3);

	/* disable and clear uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(cmm_base_remap_addr), 0x0);
	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev0_base_remap_addr), 0x0);

#if UARTHUB_ENABLE_UART_1_CHANNEL
	if (g_max_dev >= 2)
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev1_base_remap_addr), 0x0);
#endif

	if (g_max_dev >= 3)
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev2_base_remap_addr), 0x0);

	/* sw_rst3 4 times */
	for (i = 0; i < 4; i++) {
		UARTHUB_SET_BIT(UARTHUB_INTFHUB_CON4(intfhub_base_remap_addr), (0x1 << 3));
		usleep_range(5, 6);
	}

	/* enable uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(cmm_base_remap_addr), 0x1);
	UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev0_base_remap_addr), 0x1);

#if UARTHUB_ENABLE_UART_1_CHANNEL
	if (g_max_dev >= 2 && dev1_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev1_base_remap_addr), 0x1);
#endif

	if (g_max_dev >= 3 && dev2_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(UARTHUB_IIR_FCR(dev2_base_remap_addr), 0x1);

	/* restore trx request state */
	UARTHUB_REG_WRITE(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr),
		((UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr))
		& (~(0x3))) | trx_state));

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

#if UARTHUB_INFO_LOG
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

	if (!g_uarthub_plat_ic_ops)
		return 0;

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_cg_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_clk_cg_info();
		if (state != 0x0) {
			pr_notice("[%s] UARTHUB HCLK/PCLK GC OFF(0x%x)\n", __func__, state);
			return 0;
		}
	}

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

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_cg_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_cg_info();
		if (state != 0x0) {
			pr_notice("[%s] UARTHUB CG OFF(0x%x)\n", __func__, state);
			return 0;
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info();
		if (state != 1) {
			pr_notice("[%s] UNIVPLL CLK is OFF(0x%x)\n", __func__, state);
			return 0;
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_info) {
		state = g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_info();
		if (state == 0) {
			pr_notice("[%s] UARTHUB SPM RES is not all on\n", __func__);
			return 0;
		} else if (state < 0) {
			pr_notice("[%s] UARTHUB SPM RES cannot be accessed\n", __func__);
			return 0;
		}
	}

	state = UARTHUB_REG_READ_BIT(
		UARTHUB_INTFHUB_CON1(intfhub_base_remap_addr), 0xFFFF);
	if (state != 0x8581) {
		pr_notice("[%s] APB BUS CLK is OFF\n", __func__);
		return 0;
	}

	state = (UARTHUB_REG_READ_BIT(UARTHUB_INTFHUB_DEV0_STA(
		intfhub_base_remap_addr), 0x3));

	if (state == 0) {
		pr_notice("[%s] AP host clear the tx/rx req\n",
			__func__);
		return 0;
	}

	return 1;
}

int uarthub_core_debug_bt_tx_timeout(const char *tag)
{
	const char *def_tag = "HUB_DBG_APMDA";
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
	const char *def_tag = "HUB_DBG_APMDA";
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

static int g_bk_crc;
static int g_bk_bypass;
static int g_bk_uart_pad_mode;
static int g_bk_gpio_tx_mode;
static int g_bk_gpio_rx_mode;
static int g_bk_gpio_tx_dir;
static int g_bk_gpio_rx_dir;
static int g_bk_gpio_tx_ies;
static int g_bk_gpio_rx_ies;
static int g_bk_gpio_tx_pu;
static int g_bk_gpio_rx_pu;
static int g_bk_gpio_tx_pd;
static int g_bk_gpio_rx_pd;
static int g_bk_gpio_tx_drv;
static int g_bk_gpio_rx_drv;
static int g_bk_gpio_tx_smt;
static int g_bk_gpio_rx_smt;
static int g_bk_gpio_tx_tdsel;
static int g_bk_gpio_rx_tdsel;
static int g_bk_gpio_tx_rdsel;
static int g_bk_gpio_rx_rdsel;
static int g_bk_gpio_tx_sec_en;
static int g_bk_gpio_rx_sec_en;
static int g_bk_gpio_rx_din;

int uarthub_core_debug_uart_ip_info_loop_compare_diff(void)
{
	const char *def_tag = "HUB_DBG_LOOP";
	struct uarthub_uart_ip_debug_info crc_bypass = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	struct uarthub_gpio_trx_info gpio_base_addr;
	int len = 0;
	int val = 0;
	int cur_crc = 0;
	int cur_bypass = 0;
	int cur_uart_pad_mode = 0;
	int cur_gpio_tx_mode = 0;
	int cur_gpio_rx_mode = 0;
	int cur_gpio_tx_dir = 0;
	int cur_gpio_rx_dir = 0;
	int cur_gpio_tx_ies = 0;
	int cur_gpio_rx_ies = 0;
	int cur_gpio_tx_pu = 0;
	int cur_gpio_rx_pu = 0;
	int cur_gpio_tx_pd = 0;
	int cur_gpio_rx_pd = 0;
	int cur_gpio_tx_drv = 0;
	int cur_gpio_rx_drv = 0;
	int cur_gpio_tx_smt = 0;
	int cur_gpio_rx_smt = 0;
	int cur_gpio_tx_tdsel = 0;
	int cur_gpio_rx_tdsel = 0;
	int cur_gpio_tx_rdsel = 0;
	int cur_gpio_rx_rdsel = 0;
	int cur_gpio_tx_sec_en = 0;
	int cur_gpio_rx_sec_en = 0;
	int cur_gpio_rx_din = 0;
	unsigned long long kt_sec;
	unsigned long kt_nsec;
	struct timespec64 now;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	kt_sec = local_clock();
	kt_nsec = do_div(kt_sec, 1000000000)/1000;

	ktime_get_real_ts64(&now);

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_info("[%s] ++++++++++++++++++++++++++++++++++++++++ [%llu.%06lu][%d.%ds]\n",
			def_tag, kt_sec, kt_nsec, now.tv_sec, (now.tv_nsec / NSEC_PER_USEC));
		pr_notice("[%s] uarthub_core_is_apb_bus_clk_enable=[0]\n", def_tag);
		pr_info("[%s] ----------------------------------------\n", def_tag);
		return -1;
	}

	crc_bypass.dev0 =
		UARTHUB_REG_READ(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr));

	cur_crc = ((crc_bypass.dev0 & 0x2) >> 1);
	cur_bypass = ((crc_bypass.dev0 & 0x4) >> 2);

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info(&gpio_base_addr);
		if (val == 0) {
			cur_gpio_tx_mode = gpio_base_addr.tx_mode.gpio_value;
			cur_gpio_rx_mode = gpio_base_addr.rx_mode.gpio_value;
			cur_gpio_tx_dir = gpio_base_addr.tx_dir.gpio_value;
			cur_gpio_rx_dir = gpio_base_addr.rx_dir.gpio_value;
			cur_gpio_tx_ies = gpio_base_addr.tx_ies.gpio_value;
			cur_gpio_rx_ies = gpio_base_addr.rx_ies.gpio_value;
			cur_gpio_tx_pu = gpio_base_addr.tx_pu.gpio_value;
			cur_gpio_rx_pu = gpio_base_addr.rx_pu.gpio_value;
			cur_gpio_tx_pd = gpio_base_addr.tx_pd.gpio_value;
			cur_gpio_rx_pd = gpio_base_addr.rx_pd.gpio_value;
			cur_gpio_tx_drv = gpio_base_addr.tx_drv.gpio_value;
			cur_gpio_rx_drv = gpio_base_addr.rx_drv.gpio_value;
			cur_gpio_tx_smt = gpio_base_addr.tx_smt.gpio_value;
			cur_gpio_rx_smt = gpio_base_addr.rx_smt.gpio_value;
			cur_gpio_tx_tdsel = gpio_base_addr.tx_tdsel.gpio_value;
			cur_gpio_rx_tdsel = gpio_base_addr.rx_tdsel.gpio_value;
			cur_gpio_tx_rdsel = gpio_base_addr.tx_rdsel.gpio_value;
			cur_gpio_rx_rdsel = gpio_base_addr.rx_rdsel.gpio_value;
			cur_gpio_tx_sec_en = gpio_base_addr.tx_sec_en.gpio_value;
			cur_gpio_rx_sec_en = gpio_base_addr.rx_sec_en.gpio_value;
			cur_gpio_rx_din = gpio_base_addr.rx_din.gpio_value;
		}
	}

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_peri_uart_pad_mode) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_peri_uart_pad_mode();
		if (val >= 0)
			cur_uart_pad_mode = val;
	}

	if (cur_crc == g_bk_crc &&
			cur_bypass == g_bk_bypass &&
			cur_uart_pad_mode == g_bk_uart_pad_mode &&
			cur_gpio_tx_mode == g_bk_gpio_tx_mode &&
			cur_gpio_rx_mode == g_bk_gpio_rx_mode &&
			cur_gpio_tx_dir == g_bk_gpio_tx_dir &&
			cur_gpio_rx_dir == g_bk_gpio_rx_dir &&
			cur_gpio_tx_ies == g_bk_gpio_tx_ies &&
			cur_gpio_rx_ies == g_bk_gpio_rx_ies &&
			cur_gpio_tx_pu == g_bk_gpio_tx_pu &&
			cur_gpio_rx_pu == g_bk_gpio_rx_pu &&
			cur_gpio_tx_pd == g_bk_gpio_tx_pd &&
			cur_gpio_rx_pd == g_bk_gpio_rx_pd &&
			cur_gpio_tx_drv == g_bk_gpio_tx_drv &&
			cur_gpio_rx_drv == g_bk_gpio_rx_drv &&
			cur_gpio_tx_smt == g_bk_gpio_tx_smt &&
			cur_gpio_rx_smt == g_bk_gpio_rx_smt &&
			cur_gpio_tx_tdsel == g_bk_gpio_tx_tdsel &&
			cur_gpio_rx_tdsel == g_bk_gpio_rx_tdsel &&
			cur_gpio_tx_rdsel == g_bk_gpio_tx_rdsel &&
			cur_gpio_rx_rdsel == g_bk_gpio_rx_rdsel &&
			cur_gpio_tx_sec_en == g_bk_gpio_tx_sec_en &&
			cur_gpio_rx_sec_en == g_bk_gpio_rx_sec_en &&
			cur_gpio_rx_din == g_bk_gpio_rx_din) {
		return 0;
	}

	g_bk_crc = cur_crc;
	g_bk_bypass = cur_bypass;
	g_bk_uart_pad_mode = cur_uart_pad_mode;
	g_bk_gpio_tx_mode = cur_gpio_tx_mode;
	g_bk_gpio_rx_mode = cur_gpio_rx_mode;
	g_bk_gpio_tx_dir = cur_gpio_tx_dir;
	g_bk_gpio_rx_dir = cur_gpio_rx_dir;
	g_bk_gpio_tx_ies = cur_gpio_tx_ies;
	g_bk_gpio_rx_ies = cur_gpio_rx_ies;
	g_bk_gpio_tx_pu = cur_gpio_tx_pu;
	g_bk_gpio_rx_pu = cur_gpio_rx_pu;
	g_bk_gpio_tx_pd = cur_gpio_tx_pd;
	g_bk_gpio_rx_pd = cur_gpio_rx_pd;
	g_bk_gpio_tx_drv = cur_gpio_tx_drv;
	g_bk_gpio_rx_drv = cur_gpio_rx_drv;
	g_bk_gpio_tx_smt = cur_gpio_tx_smt;
	g_bk_gpio_rx_smt = cur_gpio_rx_smt;
	g_bk_gpio_tx_tdsel = cur_gpio_tx_tdsel;
	g_bk_gpio_rx_tdsel = cur_gpio_rx_tdsel;
	g_bk_gpio_tx_rdsel = cur_gpio_tx_rdsel;
	g_bk_gpio_rx_rdsel = cur_gpio_rx_rdsel;
	g_bk_gpio_tx_sec_en = cur_gpio_tx_sec_en;
	g_bk_gpio_rx_sec_en = cur_gpio_rx_sec_en;
	g_bk_gpio_rx_din = cur_gpio_rx_din;

	pr_info("[%s] ++++++++++++++++++++++++++++++++++++++++ [%llu.%06lu][%d.%ds]\n",
		def_tag, kt_sec, kt_nsec, now.tv_sec, (now.tv_nsec / NSEC_PER_USEC));

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] crc_bypass=[%d-%d]",
		def_tag, cur_crc, cur_bypass);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___UART_PAD_MODE=[0x%x(%s)]",
		cur_uart_pad_mode, ((cur_uart_pad_mode == 0) ? "UARTHUB" : "UART_PAD"));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] GPIO TX_MODE=[addr:0x%lx,mask:0x%lx,exp:0x%lx,read:0x%lx]",
		def_tag,
		gpio_base_addr.tx_mode.addr, gpio_base_addr.tx_mode.mask,
		gpio_base_addr.tx_mode.value, cur_gpio_tx_mode);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___RX_MODE=[addr:0x%lx,mask:0x%lx,exp:0x%lx,read:0x%lx]",
		gpio_base_addr.rx_mode.addr, gpio_base_addr.rx_mode.mask,
		gpio_base_addr.rx_mode.value, cur_gpio_rx_mode);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___RX_PAD_INFO=[0x%lx]", cur_gpio_rx_din);

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] GPIO DIR=[T:0x%lx,R:0x%lx]___DRV=[T:0x%lx,R:0x%lx]",
		def_tag,
		cur_gpio_tx_dir, cur_gpio_rx_dir, cur_gpio_tx_drv, cur_gpio_rx_drv);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___PU=[T:0x%lx,R:0x%lx]___PD=[T:0x%lx,R:0x%lx]",
		cur_gpio_tx_pu, cur_gpio_rx_pu, cur_gpio_tx_pd, cur_gpio_rx_pd);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___IES=[T:0x%lx,R:0x%lx]___SMT=[T:0x%lx,R:0x%lx]",
		cur_gpio_tx_ies, cur_gpio_rx_ies, cur_gpio_tx_smt, cur_gpio_rx_smt);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___TDSEL=[T:0x%lx,R:0x%lx]___RDSEL=[T:0x%lx,R:0x%lx]",
		cur_gpio_tx_tdsel, cur_gpio_rx_tdsel, cur_gpio_tx_rdsel, cur_gpio_rx_rdsel);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___SEC_EN=[T:0x%lx,R:0x%lx]", cur_gpio_tx_sec_en, cur_gpio_rx_sec_en);

	pr_info("%s\n", dmp_info_buf);

	pr_info("[%s] ----------------------------------------\n", def_tag);

	return 0;
}

int uarthub_core_debug_uart_ip_info_loop(void)
{
	const char *def_tag = "HUB_DBG_LOOP";
	struct uarthub_uart_ip_debug_info debug1 = {0};
	struct uarthub_uart_ip_debug_info debug2 = {0};
	struct uarthub_uart_ip_debug_info debug3 = {0};
	struct uarthub_uart_ip_debug_info debug4 = {0};
	struct uarthub_uart_ip_debug_info debug5 = {0};
	struct uarthub_uart_ip_debug_info debug6 = {0};
	struct uarthub_uart_ip_debug_info debug7 = {0};
	struct uarthub_uart_ip_debug_info debug8 = {0};
	struct uarthub_uart_ip_debug_info pkt_cnt = {0};
	struct uarthub_uart_ip_debug_info dev_sta = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	pr_info("[%s_%d] ++++++++++++++++++++++++++++++++++++++++\n",
		def_tag, g_dump_loop_dur_ms);

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s_%d] uarthub_core_is_apb_bus_clk_enable=[0]\n",
			def_tag, g_dump_loop_dur_ms);
		pr_info("[%s_%d] ----------------------------------------\n",
			def_tag, g_dump_loop_dur_ms);
		return -3;
	}

	pkt_cnt.dev0 =
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr));
	dev_sta.dev0 =
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr));

	if (g_max_dev >= 2) {
		pkt_cnt.dev1 =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(intfhub_base_remap_addr));
		dev_sta.dev1 =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_STA(intfhub_base_remap_addr));
	}

	if (g_max_dev >= 3) {
		pkt_cnt.dev2 =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(intfhub_base_remap_addr));
		dev_sta.dev2 =
			UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_STA(intfhub_base_remap_addr));
	}

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s_%d] tx_pkt_cnt=[%d-%d-%d]___rx_pkt_cnt=[%d-%d-%d]",
		def_tag, g_dump_loop_dur_ms,
		((pkt_cnt.dev0 & 0xFF000000) >> 24), ((pkt_cnt.dev1 & 0xFF000000) >> 24),
		((pkt_cnt.dev2 & 0xFF000000) >> 24), ((pkt_cnt.dev0 & 0xFF00) >> 8),
		((pkt_cnt.dev1 & 0xFF00) >> 8), ((pkt_cnt.dev2 & 0xFF00) >> 8),
		dev_sta.dev0, dev_sta.dev1, dev_sta.dev2);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"___INTFHUB_DEVx_STA=[0x%x-0x%x-0x%x]",
		dev_sta.dev0, dev_sta.dev1, dev_sta.dev2);

	pr_info("%s\n", dmp_info_buf);

	if (uarthub_core_is_uarthub_clk_enable() == 0) {
		pr_notice("[%s_%d] uarthub_core_is_uarthub_clk_enable=[0]\n",
			def_tag, g_dump_loop_dur_ms);
		pr_info("[%s_%d] ----------------------------------------\n",
			def_tag, g_dump_loop_dur_ms);
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

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s_%d] op_rx_req=[%d-%d-%d-%d],ip_tx_dma=[%d-%d-%d-%d]",
		def_tag, g_dump_loop_dur_ms,
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
		def_tag, g_dump_loop_dur_ms,
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

	pr_info("[%s_%d] ----------------------------------------\n",
		def_tag, g_dump_loop_dur_ms);

	return 0;
}

int uarthub_core_debug_uart_ip_info_with_tag_ex(const char *tag, int boundary)
{
	const char *def_tag = "HUB_DBG_UIP";
	void __iomem *ap_uart_base_remap_addr = NULL;
	struct uarthub_uart_ip_debug_info debug1 = {0};
	struct uarthub_uart_ip_debug_info debug2 = {0};
	struct uarthub_uart_ip_debug_info debug3 = {0};
	struct uarthub_uart_ip_debug_info debug4 = {0};
	struct uarthub_uart_ip_debug_info debug5 = {0};
	struct uarthub_uart_ip_debug_info debug6 = {0};
	struct uarthub_uart_ip_debug_info debug7 = {0};
	struct uarthub_uart_ip_debug_info debug8 = {0};
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0, cmm_sta = 0, ap_sta = 0;
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

	if (g_uarthub_plat_ic_ops && g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr)
		ap_uart_base_remap_addr =
			g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr();

	uarthub_core_clk_univpll_ctrl(1);
	if (uarthub_core_is_univpll_on() == 0) {
		pr_notice("[%s] uarthub_core_is_univpll_on=[0]\n", __func__);
		if (boundary == 1) {
			pr_info("[%s][%s] ----------------------------------------\n",
				def_tag, ((tag == NULL) ? "null" : tag));
		}
		uarthub_core_clk_univpll_ctrl(0);
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

	if (ap_uart_base_remap_addr) {
		debug1.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_1(ap_uart_base_remap_addr));
		debug2.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_2(ap_uart_base_remap_addr));
		debug3.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_3(ap_uart_base_remap_addr));
		debug4.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_4(ap_uart_base_remap_addr));
		debug5.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_5(ap_uart_base_remap_addr));
		debug6.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_6(ap_uart_base_remap_addr));
		debug7.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_7(ap_uart_base_remap_addr));
		debug8.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_8(ap_uart_base_remap_addr));
	}

	uarthub_core_clk_univpll_ctrl(0);

	len = 0;
	dev0_sta = (((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4));
	dev1_sta = (((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4));
	dev2_sta = (((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4));
	cmm_sta = (((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4));
	ap_sta = (((debug5.ap & 0xF0) >> 4) + ((debug6.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] op_rx_req=[%d]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] op_rx_req=[%d-%d-%d-%d-%d]",
			def_tag, ((tag == NULL) ? "null" : tag),
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
	}

	dev0_sta = (((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4));
	dev1_sta = (((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4));
	dev2_sta = (((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4));
	cmm_sta = (((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4));
	ap_sta = (((debug2.ap & 0xF0) >> 4) + ((debug3.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ip_tx_dma=[%d]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ip_tx_dma=[%d-%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
	}

	dev0_sta = (debug7.dev0 & 0x3F);
	dev1_sta = (debug7.dev1 & 0x3F);
	dev2_sta = (debug7.dev2 & 0x3F);
	cmm_sta =  (debug7.cmm & 0x3F);
	ap_sta =  (debug7.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",rx_woffset=[%d]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",rx_woffset=[%d-%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
	}

	dev0_sta = (debug4.dev0 & 0x3F);
	dev1_sta = (debug4.dev1 & 0x3F);
	dev2_sta = (debug4.dev2 & 0x3F);
	cmm_sta =  (debug4.cmm & 0x3F);
	ap_sta =  (debug4.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",tx_woffset=[%d]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",tx_woffset=[%d-%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = ((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0);
	dev1_sta = ((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0);
	dev2_sta = ((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0);
	cmm_sta = ((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0);
	ap_sta = ((((debug1.ap & 0xE0) >> 5) == 1) ? 1 : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] xcstate(wsend_xoff)=[%d]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] xcstate(wsend_xoff)=[%d-%d-%d-%d-%d]",
			def_tag, ((tag == NULL) ? "null" : tag),
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
	}

	dev0_sta = ((debug8.dev0 & 0x8) >> 3);
	dev1_sta = ((debug8.dev1 & 0x8) >> 3);
	dev2_sta = ((debug8.dev2 & 0x8) >> 3);
	cmm_sta = ((debug8.cmm & 0x8) >> 3);
	ap_sta = ((debug8.ap & 0x8) >> 3);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",swtxdis(det_xoff)=[%d]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",swtxdis(det_xoff)=[%d-%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
	}

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
	int val = 0;
	int apb_bus_clk_enable = 0;
	struct uarthub_gpio_trx_info gpio_base_addr;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int gpio_info = 0;
	const char *def_tag = "HUB_DBG";
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	void __iomem *ap_uart_base_remap_addr = NULL;
	struct uarthub_uart_ip_debug_info feature_sel = {0};
	struct uarthub_uart_ip_debug_info highspeed = {0};
	struct uarthub_uart_ip_debug_info dll = {0};
	struct uarthub_uart_ip_debug_info sample_cnt = {0};
	struct uarthub_uart_ip_debug_info sample_pt = {0};
	struct uarthub_uart_ip_debug_info fracdiv_l = {0};
	struct uarthub_uart_ip_debug_info fracdiv_m = {0};
	struct uarthub_uart_ip_debug_info dma_en = {0};
	struct uarthub_uart_ip_debug_info iir_fcr = {0};
	struct uarthub_uart_ip_debug_info lcr = {0};
	struct uarthub_uart_ip_debug_info efr = {0};
	struct uarthub_uart_ip_debug_info xon1 = {0};
	struct uarthub_uart_ip_debug_info xoff1 = {0};
	struct uarthub_uart_ip_debug_info xon2 = {0};
	struct uarthub_uart_ip_debug_info xoff2 = {0};
	struct uarthub_uart_ip_debug_info esc_en = {0};
	struct uarthub_uart_ip_debug_info esc_dat = {0};
	struct uarthub_uart_ip_debug_info fcr_rd = {0};
	struct uarthub_uart_ip_debug_info mcr = {0};
	struct uarthub_uart_ip_debug_info lsr = {0};

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	if (!g_uarthub_plat_ic_ops)
		return -1;

	if (mutex_lock_killable(&g_lock_dump_log))
		return -5;

	pr_info("[%s][%s] ++++++++++++++++++++++++++++++++++++++++\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	uarthub_core_dump_sspm_log();

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] IDBG=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), val);

	apb_bus_clk_enable = uarthub_core_is_apb_bus_clk_enable();
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB=[%d]", apb_bus_clk_enable);

	if (apb_bus_clk_enable == 0) {
		pr_info("%s\n", dmp_info_buf);
		pr_info("[%s][%s] ----------------------------------------\n",
			def_tag, ((tag == NULL) ? "null" : tag));
		mutex_unlock(&g_lock_dump_log);
		return -2;
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_cg_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_cg_info();
		if (val >= 0) {
			/* the expect value is 0x0 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",HCG=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_peri_clk_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_peri_clk_info();
		if (val >= 0) {
			/* the expect value is 0x800 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UPCLK=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_peri_uart_pad_mode) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_peri_uart_pad_mode();
		if (val >= 0) {
			/* the expect value is 0x800 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UPAD=[0x%x(%s)]",
				val, ((val == 0) ? "HUB" : "UART_PAD"));
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_info();
		if (val == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SPM=[1]");
		} else if (val == 0) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SPM=[0]");
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SPM=[F]");
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info();
		if (val >= 0) {
			/* the expect value is 0x1 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UVPLL=[%d]", val);
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info();
		if (val >= 0) {
			/* the expect value is 0x2 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UMUX=[0x%x]", val);
		}
	}

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	gpio_info = 0;
	if (g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_gpio_trx_info(&gpio_base_addr);
		if (val == 0) {
			len = 0;
			if (gpio_base_addr.tx_mode.gpio_value ==
					gpio_base_addr.rx_mode.gpio_value) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					"[%s][%s] GPIO MODE=[T/R:0x%lx]",
					def_tag, ((tag == NULL) ? "null" : tag),
					gpio_base_addr.tx_mode.gpio_value);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					"[%s][%s] GPIO MODE=[T:0x%lx,R:0x%lx]",
					def_tag, ((tag == NULL) ? "null" : tag),
					gpio_base_addr.tx_mode.gpio_value,
					gpio_base_addr.rx_mode.gpio_value);
			}

			if (gpio_base_addr.tx_drv.gpio_value ==
					gpio_base_addr.rx_drv.gpio_value) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",DRV=[T/R:0x%lx]", gpio_base_addr.tx_drv.gpio_value);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",DRV=[T:0x%lx,R:0x%lx]",
					gpio_base_addr.tx_drv.gpio_value,
					gpio_base_addr.rx_drv.gpio_value);
			}

			gpio_info = 1;
		}
	}

	if (gpio_info == 0) {
		len = 0;
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] ILPBACK(0xe4)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			UARTHUB_REG_READ(UARTHUB_INTFHUB_LOOPBACK(intfhub_base_remap_addr)));
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ILPBACK(0xe4)=[0x%x]",
			UARTHUB_REG_READ(UARTHUB_INTFHUB_LOOPBACK(intfhub_base_remap_addr)));
	}

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG(0xf4)=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr));
	dev1_sta = ((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_STA(
		intfhub_base_remap_addr)) : 0);
	dev2_sta = ((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_STA(
		intfhub_base_remap_addr)) : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			dev0_sta, dev1_sta, dev2_sta);
	}

	dev0_sta = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(intfhub_base_remap_addr));
	dev1_sta = ((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(
		intfhub_base_remap_addr)) : 0);
	dev2_sta = ((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(
		intfhub_base_remap_addr)) : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_PKT_CNT(0x1c/0x50/0x90)=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_PKT_CNT(0x1c/0x50/0x90)=[0x%x-0x%x-0x%x]",
			dev0_sta, dev1_sta, dev2_sta);
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_CRC_STA(intfhub_base_remap_addr));
	dev1_sta = ((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_CRC_STA(
		intfhub_base_remap_addr)) : 0);
	dev2_sta = ((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_CRC_STA(
		intfhub_base_remap_addr)) : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_CRC_STA(0x20/0x54/0x94)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_CRC_STA(0x20/0x54/0x94)=[0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			dev0_sta, dev1_sta, dev2_sta);
	}

	dev0_sta = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_RX_ERR_CRC_STA(intfhub_base_remap_addr));
	dev1_sta = ((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_RX_ERR_CRC_STA(
		intfhub_base_remap_addr)) : 0);
	dev2_sta = ((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_RX_ERR_CRC_STA(
		intfhub_base_remap_addr)) : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_RX_ERR_CRC_STA(0x10/0x14/0x18)=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_RX_ERR_CRC_STA(0x10/0x14/0x18)=[0x%x-0x%x-0x%x]",
			dev0_sta, dev1_sta, dev2_sta);
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] IDEV0_IRQ_STA/MASK(0x30/0x38)=[0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_STA(intfhub_base_remap_addr)),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_IRQ_MASK(intfhub_base_remap_addr)));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IIRQ_STA/MASK(0xd0/0xd8)=[0x%x-0x%x]",
		UARTHUB_REG_READ(UARTHUB_INTFHUB_IRQ_STA(intfhub_base_remap_addr)),
		UARTHUB_REG_READ(UARTHUB_INTFHUB_IRQ_MASK(intfhub_base_remap_addr)));

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0(0xe0)=[0x%x]", val);

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_CON2(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ICON2(0xc8)=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	if (uarthub_core_debug_uart_ip_info_with_tag_ex(tag, 0) == -2) {
		pr_notice("[%s] uarthub_core_is_univpll_on=[0]\n", __func__);
		pr_info("[%s][%s] ----------------------------------------\n",
			def_tag, ((tag == NULL) ? "null" : tag));
		mutex_unlock(&g_lock_dump_log);
		return -1;
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr)
		ap_uart_base_remap_addr =
			g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr();

	uarthub_core_clk_univpll_ctrl(1);
	if (uarthub_core_is_univpll_on() == 0) {
		pr_notice("[%s] uarthub_core_is_univpll_on=[0]\n", __func__);
		uarthub_core_clk_univpll_ctrl(0);
		mutex_unlock(&g_lock_dump_log);
		return -1;
	}

	feature_sel.dev0 = UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(dev0_base_remap_addr));
	highspeed.dev0 = UARTHUB_REG_READ(UARTHUB_HIGHSPEED(dev0_base_remap_addr));
	dll.dev0 = UARTHUB_REG_READ(UARTHUB_DLL(dev0_base_remap_addr));
	sample_cnt.dev0 = UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(dev0_base_remap_addr));
	sample_pt.dev0 = UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(dev0_base_remap_addr));
	fracdiv_l.dev0 = UARTHUB_REG_READ(UARTHUB_FRACDIV_L(dev0_base_remap_addr));
	fracdiv_m.dev0 = UARTHUB_REG_READ(UARTHUB_FRACDIV_M(dev0_base_remap_addr));
	dma_en.dev0 = UARTHUB_REG_READ(UARTHUB_DMA_EN(dev0_base_remap_addr));
	iir_fcr.dev0 = UARTHUB_REG_READ(UARTHUB_IIR_FCR(dev0_base_remap_addr));
	lcr.dev0 = UARTHUB_REG_READ(UARTHUB_LCR(dev0_base_remap_addr));
	efr.dev0 = UARTHUB_REG_READ(UARTHUB_EFR(dev0_base_remap_addr));
	xon1.dev0 = UARTHUB_REG_READ(UARTHUB_XON1(dev0_base_remap_addr));
	xoff1.dev0 = UARTHUB_REG_READ(UARTHUB_XOFF1(dev0_base_remap_addr));
	xon2.dev0 = UARTHUB_REG_READ(UARTHUB_XON2(dev0_base_remap_addr));
	xoff2.dev0 = UARTHUB_REG_READ(UARTHUB_XOFF2(dev0_base_remap_addr));
	esc_en.dev0 = UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(dev0_base_remap_addr));
	esc_dat.dev0 = UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(dev0_base_remap_addr));
	fcr_rd.dev0 = UARTHUB_REG_READ(UARTHUB_FCR_RD(dev0_base_remap_addr));
	mcr.dev0 = UARTHUB_REG_READ(UARTHUB_MCR(dev0_base_remap_addr));
	lsr.dev0 = UARTHUB_REG_READ(UARTHUB_LSR(dev0_base_remap_addr));

	if (g_max_dev >= 2) {
		feature_sel.dev1 = UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(dev1_base_remap_addr));
		highspeed.dev1 = UARTHUB_REG_READ(UARTHUB_HIGHSPEED(dev1_base_remap_addr));
		dll.dev1 = UARTHUB_REG_READ(UARTHUB_DLL(dev1_base_remap_addr));
		sample_cnt.dev1 = UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(dev1_base_remap_addr));
		sample_pt.dev1 = UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(dev1_base_remap_addr));
		fracdiv_l.dev1 = UARTHUB_REG_READ(UARTHUB_FRACDIV_L(dev1_base_remap_addr));
		fracdiv_m.dev1 = UARTHUB_REG_READ(UARTHUB_FRACDIV_M(dev1_base_remap_addr));
		dma_en.dev1 = UARTHUB_REG_READ(UARTHUB_DMA_EN(dev1_base_remap_addr));
		iir_fcr.dev1 = UARTHUB_REG_READ(UARTHUB_IIR_FCR(dev1_base_remap_addr));
		lcr.dev1 = UARTHUB_REG_READ(UARTHUB_LCR(dev1_base_remap_addr));
		efr.dev1 = UARTHUB_REG_READ(UARTHUB_EFR(dev1_base_remap_addr));
		xon1.dev1 = UARTHUB_REG_READ(UARTHUB_XON1(dev1_base_remap_addr));
		xoff1.dev1 = UARTHUB_REG_READ(UARTHUB_XOFF1(dev1_base_remap_addr));
		xon2.dev1 = UARTHUB_REG_READ(UARTHUB_XON2(dev1_base_remap_addr));
		xoff2.dev1 = UARTHUB_REG_READ(UARTHUB_XOFF2(dev1_base_remap_addr));
		esc_en.dev1 = UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(dev1_base_remap_addr));
		esc_dat.dev1 = UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(dev1_base_remap_addr));
		fcr_rd.dev1 = UARTHUB_REG_READ(UARTHUB_FCR_RD(dev1_base_remap_addr));
		mcr.dev1 = UARTHUB_REG_READ(UARTHUB_MCR(dev1_base_remap_addr));
		lsr.dev1 = UARTHUB_REG_READ(UARTHUB_LSR(dev1_base_remap_addr));
	}

	if (g_max_dev >= 3) {
		feature_sel.dev2 = UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(dev2_base_remap_addr));
		highspeed.dev2 = UARTHUB_REG_READ(UARTHUB_HIGHSPEED(dev2_base_remap_addr));
		dll.dev2 = UARTHUB_REG_READ(UARTHUB_DLL(dev2_base_remap_addr));
		sample_cnt.dev2 = UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(dev2_base_remap_addr));
		sample_pt.dev2 = UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(dev2_base_remap_addr));
		fracdiv_l.dev2 = UARTHUB_REG_READ(UARTHUB_FRACDIV_L(dev2_base_remap_addr));
		fracdiv_m.dev2 = UARTHUB_REG_READ(UARTHUB_FRACDIV_M(dev2_base_remap_addr));
		dma_en.dev2 = UARTHUB_REG_READ(UARTHUB_DMA_EN(dev2_base_remap_addr));
		iir_fcr.dev2 = UARTHUB_REG_READ(UARTHUB_IIR_FCR(dev2_base_remap_addr));
		lcr.dev2 = UARTHUB_REG_READ(UARTHUB_LCR(dev2_base_remap_addr));
		efr.dev2 = UARTHUB_REG_READ(UARTHUB_EFR(dev2_base_remap_addr));
		xon1.dev2 = UARTHUB_REG_READ(UARTHUB_XON1(dev2_base_remap_addr));
		xoff1.dev2 = UARTHUB_REG_READ(UARTHUB_XOFF1(dev2_base_remap_addr));
		xon2.dev2 = UARTHUB_REG_READ(UARTHUB_XON2(dev2_base_remap_addr));
		xoff2.dev2 = UARTHUB_REG_READ(UARTHUB_XOFF2(dev2_base_remap_addr));
		esc_en.dev2 = UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(dev2_base_remap_addr));
		esc_dat.dev2 = UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(dev2_base_remap_addr));
		fcr_rd.dev2 = UARTHUB_REG_READ(UARTHUB_FCR_RD(dev2_base_remap_addr));
		mcr.dev2 = UARTHUB_REG_READ(UARTHUB_MCR(dev2_base_remap_addr));
		lsr.dev2 = UARTHUB_REG_READ(UARTHUB_LSR(dev2_base_remap_addr));
	}

	feature_sel.cmm = UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(cmm_base_remap_addr));
	highspeed.cmm = UARTHUB_REG_READ(UARTHUB_HIGHSPEED(cmm_base_remap_addr));
	dll.cmm = UARTHUB_REG_READ(UARTHUB_DLL(cmm_base_remap_addr));
	sample_cnt.cmm = UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(cmm_base_remap_addr));
	sample_pt.cmm = UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(cmm_base_remap_addr));
	fracdiv_l.cmm = UARTHUB_REG_READ(UARTHUB_FRACDIV_L(cmm_base_remap_addr));
	fracdiv_m.cmm = UARTHUB_REG_READ(UARTHUB_FRACDIV_M(cmm_base_remap_addr));
	dma_en.cmm = UARTHUB_REG_READ(UARTHUB_DMA_EN(cmm_base_remap_addr));
	iir_fcr.cmm = UARTHUB_REG_READ(UARTHUB_IIR_FCR(cmm_base_remap_addr));
	lcr.cmm = UARTHUB_REG_READ(UARTHUB_LCR(cmm_base_remap_addr));
	efr.cmm = UARTHUB_REG_READ(UARTHUB_EFR(cmm_base_remap_addr));
	xon1.cmm = UARTHUB_REG_READ(UARTHUB_XON1(cmm_base_remap_addr));
	xoff1.cmm = UARTHUB_REG_READ(UARTHUB_XOFF1(cmm_base_remap_addr));
	xon2.cmm = UARTHUB_REG_READ(UARTHUB_XON2(cmm_base_remap_addr));
	xoff2.cmm = UARTHUB_REG_READ(UARTHUB_XOFF2(cmm_base_remap_addr));
	esc_en.cmm = UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(cmm_base_remap_addr));
	esc_dat.cmm = UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(cmm_base_remap_addr));
	fcr_rd.cmm = UARTHUB_REG_READ(UARTHUB_FCR_RD(cmm_base_remap_addr));
	mcr.cmm = UARTHUB_REG_READ(UARTHUB_MCR(cmm_base_remap_addr));
	lsr.cmm = UARTHUB_REG_READ(UARTHUB_LSR(cmm_base_remap_addr));

	if (ap_uart_base_remap_addr) {
#if DUMP_AP_UART_DBG_INFO
		feature_sel.ap = UARTHUB_REG_READ(UARTHUB_FEATURE_SEL(ap_uart_base_remap_addr));
		highspeed.ap = UARTHUB_REG_READ(UARTHUB_HIGHSPEED(ap_uart_base_remap_addr));
		dll.ap = UARTHUB_REG_READ(UARTHUB_DLL(ap_uart_base_remap_addr));
		sample_cnt.ap = UARTHUB_REG_READ(UARTHUB_SAMPLE_COUNT(ap_uart_base_remap_addr));
		sample_pt.ap = UARTHUB_REG_READ(UARTHUB_SAMPLE_POINT(ap_uart_base_remap_addr));
		fracdiv_l.ap = UARTHUB_REG_READ(UARTHUB_FRACDIV_L(ap_uart_base_remap_addr));
		fracdiv_m.ap = UARTHUB_REG_READ(UARTHUB_FRACDIV_M(ap_uart_base_remap_addr));
		dma_en.ap = UARTHUB_REG_READ(UARTHUB_DMA_EN(ap_uart_base_remap_addr));
		iir_fcr.ap = UARTHUB_REG_READ(UARTHUB_IIR_FCR(ap_uart_base_remap_addr));
		lcr.ap = UARTHUB_REG_READ(UARTHUB_LCR(ap_uart_base_remap_addr));
		efr.ap = UARTHUB_REG_READ(UARTHUB_EFR(ap_uart_base_remap_addr));
		xon1.ap = UARTHUB_REG_READ(UARTHUB_XON1(ap_uart_base_remap_addr));
		xoff1.ap = UARTHUB_REG_READ(UARTHUB_XOFF1(ap_uart_base_remap_addr));
		xon2.ap = UARTHUB_REG_READ(UARTHUB_XON2(ap_uart_base_remap_addr));
		xoff2.ap = UARTHUB_REG_READ(UARTHUB_XOFF2(ap_uart_base_remap_addr));
		esc_en.ap = UARTHUB_REG_READ(UARTHUB_ESCAPE_EN(ap_uart_base_remap_addr));
		esc_dat.ap = UARTHUB_REG_READ(UARTHUB_ESCAPE_DAT(ap_uart_base_remap_addr));
		fcr_rd.ap = UARTHUB_REG_READ(UARTHUB_FCR_RD(ap_uart_base_remap_addr));
		mcr.ap = UARTHUB_REG_READ(UARTHUB_MCR(ap_uart_base_remap_addr));
		lsr.ap = UARTHUB_REG_READ(UARTHUB_LSR(ap_uart_base_remap_addr));
#else
		feature_sel.ap = feature_sel.dev0;
		highspeed.ap = highspeed.dev0;
		dll.ap = dll.dev0;
		sample_cnt.ap = sample_cnt.dev0;
		sample_pt.ap = sample_pt.dev0;
		fracdiv_l.ap = fracdiv_l.dev0;
		fracdiv_m.ap = fracdiv_m.dev0;
		dma_en.ap = dma_en.dev0;
		iir_fcr.ap = iir_fcr.dev0;
		lcr.ap = lcr.dev0;
		efr.ap = efr.dev0;
		xon1.ap = xon1.dev0;
		xoff1.ap = xoff1.dev0;
		xon2.ap = xon2.dev0;
		xoff2.ap = xoff2.dev0;
		esc_en.ap = esc_en.dev0;
		esc_dat.ap = esc_dat.dev0;
		fcr_rd.ap = fcr_rd.dev0;
		mcr.ap = mcr.dev0;
		lsr.ap = lsr.dev0;
#endif
	}

	uarthub_core_clk_univpll_ctrl(0);

	len = 0;
	if (feature_sel.dev0 == feature_sel.dev1 && feature_sel.dev1 == feature_sel.dev2 &&
			feature_sel.dev2 == feature_sel.cmm && feature_sel.cmm == feature_sel.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FEATURE_SEL(0x9c)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), feature_sel.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FEATURE_SEL(0x9c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			feature_sel.dev0, feature_sel.dev1, feature_sel.dev2,
			feature_sel.cmm, feature_sel.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FEATURE_SEL(0x9c)=[0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			feature_sel.dev0, feature_sel.dev1, feature_sel.dev2, feature_sel.cmm);
#endif
	}

	if (highspeed.dev0 == highspeed.dev1 && highspeed.dev1 == highspeed.dev2 &&
			highspeed.dev2 == highspeed.cmm && highspeed.cmm == highspeed.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HIGHSPEED(0x24)=[0x%x]", highspeed.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HIGHSPEED(0x24)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			highspeed.dev0, highspeed.dev1, highspeed.dev2,
			highspeed.cmm, highspeed.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HIGHSPEED(0x24)=[0x%x-0x%x-0x%x-0x%x]",
			highspeed.dev0, highspeed.dev1, highspeed.dev2, highspeed.cmm);
#endif
	}

	if (dll.dev0 == dll.dev1 && dll.dev1 == dll.dev2 &&
			dll.dev2 == dll.cmm && dll.cmm == dll.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DLL(0x90)=[0x%x]", dll.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DLL(0x90)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			dll.dev0, dll.dev1, dll.dev2, dll.cmm, dll.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DLL(0x90)=[0x%x-0x%x-0x%x-0x%x]",
			dll.dev0, dll.dev1, dll.dev2, dll.cmm);
#endif
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (sample_cnt.dev0 == sample_cnt.dev1 && sample_cnt.dev1 == sample_cnt.dev2 &&
			sample_cnt.dev2 == sample_cnt.cmm && sample_cnt.cmm == sample_cnt.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] SAMPLE_CNT(0x28)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), sample_cnt.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] SAMPLE_CNT(0x28)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			sample_cnt.dev0, sample_cnt.dev1, sample_cnt.dev2,
			sample_cnt.cmm, sample_cnt.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] SAMPLE_CNT(0x28)=[0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			sample_cnt.dev0, sample_cnt.dev1, sample_cnt.dev2, sample_cnt.cmm);
#endif
	}

	if (sample_pt.dev0 == sample_pt.dev1 && sample_pt.dev1 == sample_pt.dev2 &&
			sample_pt.dev2 == sample_pt.cmm && sample_pt.cmm == sample_pt.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SAMPLE_PT(0x2c)=[0x%x]", sample_pt.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SAMPLE_PT(0x2c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			sample_pt.dev0, sample_pt.dev1, sample_pt.dev2,
			sample_pt.cmm, sample_pt.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SAMPLE_PT(0x2c)=[0x%x-0x%x-0x%x-0x%x]",
			sample_pt.dev0, sample_pt.dev1, sample_pt.dev2, sample_pt.cmm);
#endif
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (fracdiv_l.dev0 == fracdiv_l.dev1 && fracdiv_l.dev1 == fracdiv_l.dev2 &&
			fracdiv_l.dev2 == fracdiv_l.cmm && fracdiv_l.cmm == fracdiv_l.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FRACDIV_L(0x54)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), fracdiv_l.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FRACDIV_L(0x54)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			fracdiv_l.dev0, fracdiv_l.dev1, fracdiv_l.dev2,
			fracdiv_l.cmm, fracdiv_l.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FRACDIV_L(0x54)=[0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			fracdiv_l.dev0, fracdiv_l.dev1, fracdiv_l.dev2, fracdiv_l.cmm);
#endif
	}

	if (fracdiv_m.dev0 == fracdiv_m.dev1 && fracdiv_m.dev1 == fracdiv_m.dev2 &&
			fracdiv_m.dev2 == fracdiv_m.cmm && fracdiv_m.cmm == fracdiv_m.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",FRACDIV_M(0x58)=[0x%x]", fracdiv_m.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",FRACDIV_M(0x58)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			fracdiv_m.dev0, fracdiv_m.dev1, fracdiv_m.dev2,
			fracdiv_m.cmm, fracdiv_m.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",FRACDIV_M(0x58)=[0x%x-0x%x-0x%x-0x%x]",
			fracdiv_m.dev0, fracdiv_m.dev1, fracdiv_m.dev2, fracdiv_m.cmm);
#endif
	}

	if (dma_en.dev0 == dma_en.dev1 && dma_en.dev1 == dma_en.dev2 &&
			dma_en.dev2 == dma_en.cmm && dma_en.cmm == dma_en.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DMA_EN(0x4c)=[0x%x]", dma_en.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DMA_EN(0x4c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			dma_en.dev0, dma_en.dev1, dma_en.dev2,
			dma_en.cmm, dma_en.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DMA_EN(0x4c)=[0x%x-0x%x-0x%x-0x%x]",
			dma_en.dev0, dma_en.dev1, dma_en.dev2, dma_en.cmm);
#endif
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (iir_fcr.dev0 == iir_fcr.dev1 && iir_fcr.dev1 == iir_fcr.dev2 &&
			iir_fcr.dev2 == iir_fcr.cmm && iir_fcr.cmm == iir_fcr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FCR(0x8)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), iir_fcr.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FCR(0x8)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			iir_fcr.dev0, iir_fcr.dev1, iir_fcr.dev2,
			iir_fcr.cmm, iir_fcr.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FCR(0x8)=[0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			iir_fcr.dev0, iir_fcr.dev1, iir_fcr.dev2, iir_fcr.cmm);
#endif
	}

	if (lcr.dev0 == lcr.dev1 && lcr.dev1 == lcr.dev2 &&
			lcr.dev2 == lcr.cmm && lcr.cmm == lcr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LCR(0xc)=[0x%x]", lcr.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LCR(0xc)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			lcr.dev0, lcr.dev1, lcr.dev2,
			lcr.cmm, lcr.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LCR(0xc)=[0x%x-0x%x-0x%x-0x%x]",
			lcr.dev0, lcr.dev1, lcr.dev2, lcr.cmm);
#endif
	}

	if (efr.dev0 == efr.dev1 && efr.dev1 == efr.dev2 &&
			efr.dev2 == efr.cmm && efr.cmm == efr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",EFR(0x98)=[0x%x]", efr.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",EFR(0x98)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			efr.dev0, efr.dev1, efr.dev2, efr.cmm, efr.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",EFR(0x98)=[0x%x-0x%x-0x%x-0x%x]",
			efr.dev0, efr.dev1, efr.dev2, efr.cmm);
#endif
	}

	if (xon1.dev0 == xon1.dev1 && xon1.dev1 == xon1.dev2 &&
			xon1.dev2 == xon1.cmm && xon1.cmm == xon1.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XON1(0xa0)=[0x%x]", xon1.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XON1(0xa0)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			xon1.dev0, xon1.dev1, xon1.dev2, xon1.cmm, xon1.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XON1(0xa0)=[0x%x-0x%x-0x%x-0x%x]",
			xon1.dev0, xon1.dev1, xon1.dev2, xon1.cmm);
#endif
	}

	if (xoff1.dev0 == xoff1.dev1 && xoff1.dev1 == xoff1.dev2 &&
			xoff1.dev2 == xoff1.cmm && xoff1.cmm == xoff1.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF1(0xa8)=[0x%x]", xoff1.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF1(0xa8)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			xoff1.dev0, xoff1.dev1, xoff1.dev2, xoff1.cmm, xoff1.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF1(0xa8)=[0x%x-0x%x-0x%x-0x%x]",
			xoff1.dev0, xoff1.dev1, xoff1.dev2, xoff1.cmm);
#endif
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (xon2.dev0 == xon2.dev1 && xon2.dev1 == xon2.dev2 &&
			xon2.dev2 == xon2.cmm && xon2.cmm == xon2.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] XON2(0xa4)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), xon2.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] XON2(0xa4)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			xon2.dev0, xon2.dev1, xon2.dev2, xon2.cmm, xon2.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] XON2(0xa4)=[0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			xon2.dev0, xon2.dev1, xon2.dev2, xon2.cmm);
#endif
	}

	if (xoff2.dev0 == xoff2.dev1 && xoff2.dev1 == xoff2.dev2 &&
			xoff2.dev2 == xoff2.cmm && xoff2.cmm == xoff2.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF2(0xac)=[0x%x]", xoff2.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF2(0xac)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			xoff2.dev0, xoff2.dev1, xoff2.dev2, xoff2.cmm, xoff2.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF2(0xac)=[0x%x-0x%x-0x%x-0x%x]",
			xoff2.dev0, xoff2.dev1, xoff2.dev2, xoff2.cmm);
#endif
	}

	if (esc_en.dev0 == esc_en.dev1 && esc_en.dev1 == esc_en.dev2 &&
			esc_en.dev2 == esc_en.cmm && esc_en.cmm == esc_en.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_EN(0x44)=[0x%x]", esc_en.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_EN(0x44)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			esc_en.dev0, esc_en.dev1, esc_en.dev2,
			esc_en.cmm, esc_en.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_EN(0x44)=[0x%x-0x%x-0x%x-0x%x]",
			esc_en.dev0, esc_en.dev1, esc_en.dev2, esc_en.cmm);
#endif
	}

	if (esc_dat.dev0 == esc_dat.dev1 && esc_dat.dev1 == esc_dat.dev2 &&
			esc_dat.dev2 == esc_dat.cmm && esc_dat.cmm == esc_dat.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_DAT(0x40)=[0x%x]", esc_dat.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_DAT(0x40)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			esc_dat.dev0, esc_dat.dev1, esc_dat.dev2,
			esc_dat.cmm, esc_dat.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_DAT(0x40)=[0x%x-0x%x-0x%x-0x%x]",
			esc_dat.dev0, esc_dat.dev1, esc_dat.dev2, esc_dat.cmm);
#endif
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (fcr_rd.dev0 == fcr_rd.dev1 && fcr_rd.dev1 == fcr_rd.dev2 &&
			fcr_rd.dev2 == fcr_rd.cmm && fcr_rd.cmm == fcr_rd.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FCR_RD(0x5c)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), fcr_rd.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FCR_RD(0x5c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			fcr_rd.dev0, fcr_rd.dev1, fcr_rd.dev2,
			fcr_rd.cmm, fcr_rd.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FCR_RD(0x5c)=[0x%x-0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			fcr_rd.dev0, fcr_rd.dev1, fcr_rd.dev2, fcr_rd.cmm);
#endif
	}

	if (mcr.dev0 == mcr.dev1 && mcr.dev1 == mcr.dev2 &&
			mcr.dev2 == mcr.cmm && mcr.cmm == mcr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",MCR(0x10)=[0x%x]", mcr.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",MCR(0x10)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			mcr.dev0, mcr.dev1, mcr.dev2,
			mcr.cmm, mcr.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",MCR(0x10)=[0x%x-0x%x-0x%x-0x%x]",
			mcr.dev0, mcr.dev1, mcr.dev2, mcr.cmm);
#endif
	}

	if (lsr.dev0 == lsr.dev1 && lsr.dev1 == lsr.dev2 &&
			lsr.dev2 == lsr.cmm && lsr.cmm == lsr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LSR(0x14)=[0x%x]", lsr.dev0);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LSR(0x14)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
			lsr.dev0, lsr.dev1, lsr.dev2,
			lsr.cmm, lsr.ap);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LSR(0x14)=[0x%x-0x%x-0x%x-0x%x]",
			lsr.dev0, lsr.dev1, lsr.dev2, lsr.cmm);
#endif
	}

	pr_info("%s\n", dmp_info_buf);

	pr_info("[%s][%s] ----------------------------------------\n",
		def_tag, ((tag == NULL) ? "null" : tag));

	mutex_unlock(&g_lock_dump_log);
	return 0;
}

int uarthub_core_debug_dump_tx_rx_count(const char *tag, int trigger_point)
{
	static int cur_tx_pkt_cnt_d0;
	static int cur_tx_pkt_cnt_d1;
	static int cur_tx_pkt_cnt_d2;
	static int cur_rx_pkt_cnt_d0;
	static int cur_rx_pkt_cnt_d1;
	static int cur_rx_pkt_cnt_d2;
	static int d0_wait_for_send_xoff;
	static int d1_wait_for_send_xoff;
	static int d2_wait_for_send_xoff;
	static int cmm_wait_for_send_xoff;
	static int ap_wait_for_send_xoff;
	static int d0_detect_xoff;
	static int d1_detect_xoff;
	static int d2_detect_xoff;
	static int cmm_detect_xoff;
	static int ap_detect_xoff;
	static int d0_rx_bcnt;
	static int d1_rx_bcnt;
	static int d2_rx_bcnt;
	static int cmm_rx_bcnt;
	static int ap_rx_bcnt;
	static int d0_tx_bcnt;
	static int d1_tx_bcnt;
	static int d2_tx_bcnt;
	static int cmm_tx_bcnt;
	static int ap_tx_bcnt;
	static int pre_trigger_point = -1;
	struct uarthub_uart_ip_debug_info pkt_cnt = {0};
	struct uarthub_uart_ip_debug_info debug1 = {0};
	struct uarthub_uart_ip_debug_info debug2 = {0};
	struct uarthub_uart_ip_debug_info debug3 = {0};
	struct uarthub_uart_ip_debug_info debug5 = {0};
	struct uarthub_uart_ip_debug_info debug6 = {0};
	struct uarthub_uart_ip_debug_info debug8 = {0};
	void __iomem *ap_uart_base_remap_addr = NULL;
	const char *def_tag = "HUB_DBG";
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;

	if (trigger_point != DUMP0 && trigger_point != DUMP1) {
		pr_notice("[%s] trigger_point = %d is invalid\n", __func__, trigger_point);
		return -1;
	}

	if (trigger_point == DUMP1 && pre_trigger_point == 0) {
		len = 0;
		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump0, pcnt=[R:%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump0, pcnt=[R:%d-%d-%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		}

		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", cur_tx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d]",
				cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		}

		if (d0_rx_bcnt == d1_rx_bcnt && d1_rx_bcnt == d2_rx_bcnt &&
				d2_rx_bcnt == cmm_rx_bcnt && cmm_rx_bcnt == ap_rx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",bcnt=[R:%d", d0_rx_bcnt);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",bcnt=[R:%d-%d-%d-%d-%d",
				d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
				cmm_rx_bcnt, ap_rx_bcnt);
		}

		if (d0_tx_bcnt == d1_tx_bcnt && d1_tx_bcnt == d2_tx_bcnt &&
				d2_tx_bcnt == cmm_tx_bcnt && cmm_tx_bcnt == ap_tx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", d0_tx_bcnt);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d-%d]",
				d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt,
				cmm_tx_bcnt, ap_tx_bcnt);
		}

		if (d0_wait_for_send_xoff == d1_wait_for_send_xoff &&
				d1_wait_for_send_xoff == d2_wait_for_send_xoff &&
				d2_wait_for_send_xoff == cmm_wait_for_send_xoff &&
				cmm_wait_for_send_xoff == ap_wait_for_send_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d]", d0_wait_for_send_xoff);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d-%d-%d-%d-%d]",
				d0_wait_for_send_xoff, d1_wait_for_send_xoff,
				d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
				ap_wait_for_send_xoff);
		}

		if (d0_detect_xoff == d1_detect_xoff &&
				d1_detect_xoff == d2_detect_xoff &&
				d2_detect_xoff == cmm_detect_xoff &&
				cmm_detect_xoff == ap_detect_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d]", d0_detect_xoff);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d-%d]",
				d0_detect_xoff, d1_detect_xoff, d2_detect_xoff,
				cmm_detect_xoff, ap_detect_xoff);
		}

		pr_info("%s\n", dmp_info_buf);
	}

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -1;
	}

	pkt_cnt.dev0 = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_PKT_CNT(
		intfhub_base_remap_addr));
	if (g_max_dev >= 2)
		pkt_cnt.dev1 = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_PKT_CNT(
			intfhub_base_remap_addr));
	if (g_max_dev >= 3)
		pkt_cnt.dev2 = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_PKT_CNT(
			intfhub_base_remap_addr));

	cur_tx_pkt_cnt_d0 = ((pkt_cnt.dev0 & 0xFF000000) >> 24);
	cur_tx_pkt_cnt_d1 = ((pkt_cnt.dev1 & 0xFF000000) >> 24);
	cur_tx_pkt_cnt_d2 = ((pkt_cnt.dev2 & 0xFF000000) >> 24);
	cur_rx_pkt_cnt_d0 = ((pkt_cnt.dev0 & 0xFF00) >> 8);
	cur_rx_pkt_cnt_d1 = ((pkt_cnt.dev1 & 0xFF00) >> 8);
	cur_rx_pkt_cnt_d2 = ((pkt_cnt.dev2 & 0xFF00) >> 8);

	if (g_uarthub_plat_ic_ops &&
			g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr)
		ap_uart_base_remap_addr =
			g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr();

	uarthub_core_clk_univpll_ctrl(1);
	if (uarthub_core_is_univpll_on() == 1) {
		debug1.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev0_base_remap_addr));
		debug2.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev0_base_remap_addr));
		debug3.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev0_base_remap_addr));
		debug5.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev0_base_remap_addr));
		debug6.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev0_base_remap_addr));
		debug8.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev0_base_remap_addr));
		if (g_max_dev >= 2) {
			debug1.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev1_base_remap_addr));
			debug2.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev1_base_remap_addr));
			debug3.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev1_base_remap_addr));
			debug5.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev1_base_remap_addr));
			debug6.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev1_base_remap_addr));
			debug8.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev1_base_remap_addr));
		}
		if (g_max_dev >= 3) {
			debug1.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_1(dev2_base_remap_addr));
			debug2.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev2_base_remap_addr));
			debug3.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev2_base_remap_addr));
			debug5.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev2_base_remap_addr));
			debug6.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev2_base_remap_addr));
			debug8.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_8(dev2_base_remap_addr));
		}
		debug1.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_1(cmm_base_remap_addr));
		debug2.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_2(cmm_base_remap_addr));
		debug3.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_3(cmm_base_remap_addr));
		debug5.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_5(cmm_base_remap_addr));
		debug6.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_6(cmm_base_remap_addr));
		debug8.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_8(cmm_base_remap_addr));
		if (ap_uart_base_remap_addr) {
#if DUMP_AP_UART_DBG_INFO
			debug1.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_1(ap_uart_base_remap_addr));
			debug2.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_2(ap_uart_base_remap_addr));
			debug3.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_3(ap_uart_base_remap_addr));
			debug5.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_5(ap_uart_base_remap_addr));
			debug6.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_6(ap_uart_base_remap_addr));
			debug8.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_8(ap_uart_base_remap_addr));
#else
			debug1.ap = debug1.dev0;
			debug2.ap = debug2.dev0;
			debug3.ap = debug3.dev0;
			debug5.ap = debug5.dev0;
			debug6.ap = debug6.dev0;
			debug8.ap = debug8.dev0;
#endif
		}
	} else
		pr_notice("[%s] uarthub_core_is_univpll_on=[0]\n", __func__);
	uarthub_core_clk_univpll_ctrl(0);

	d0_wait_for_send_xoff = ((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0);
	d1_wait_for_send_xoff = ((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0);
	d2_wait_for_send_xoff = ((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0);
	cmm_wait_for_send_xoff = ((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0);
	ap_wait_for_send_xoff = ((((debug1.ap & 0xE0) >> 5) == 1) ? 1 : 0);

	d0_detect_xoff = ((debug8.dev0 & 0x8) >> 3);
	d1_detect_xoff = ((debug8.dev1 & 0x8) >> 3);
	d2_detect_xoff = ((debug8.dev2 & 0x8) >> 3);
	cmm_detect_xoff = ((debug8.cmm & 0x8) >> 3);
	ap_detect_xoff = ((debug8.ap & 0x8) >> 3);

	d0_rx_bcnt = (((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4));
	d1_rx_bcnt = (((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4));
	d2_rx_bcnt = (((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4));
	cmm_rx_bcnt = (((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4));
	ap_rx_bcnt = (((debug5.ap & 0xF0) >> 4) + ((debug6.ap & 0x3) << 4));
	d0_tx_bcnt = (((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4));
	d1_tx_bcnt = (((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4));
	d2_tx_bcnt = (((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4));
	cmm_tx_bcnt = (((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4));
	ap_tx_bcnt = (((debug2.ap & 0xF0) >> 4) + ((debug3.ap & 0x3) << 4));

	if (trigger_point != DUMP0) {
		len = 0;
		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump1, pcnt=[R:%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump1, pcnt=[R:%d-%d-%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		}

		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", cur_tx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d]",
				cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		}

		if (d0_rx_bcnt == d1_rx_bcnt && d1_rx_bcnt == d2_rx_bcnt &&
				d2_rx_bcnt == cmm_rx_bcnt && cmm_rx_bcnt == ap_rx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",bcnt=[R:%d", d0_rx_bcnt);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
#if DUMP_AP_UART_DBG_INFO
				",bcnt=[R:%d-%d-%d-%d-%d",
				d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
				cmm_rx_bcnt, ap_rx_bcnt);
#else
				",bcnt=[R:%d-%d-%d-%d",
				d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt, cmm_rx_bcnt);
#endif
		}

		if (d0_tx_bcnt == d1_tx_bcnt && d1_tx_bcnt == d2_tx_bcnt &&
				d2_tx_bcnt == cmm_tx_bcnt && cmm_tx_bcnt == ap_tx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", d0_tx_bcnt);
		} else {
#if DUMP_AP_UART_DBG_INFO
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d-%d]",
				d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt,
				cmm_tx_bcnt, ap_tx_bcnt);
#else
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d]",
				d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt, cmm_tx_bcnt);
#endif
		}

		if (d0_wait_for_send_xoff == d1_wait_for_send_xoff &&
				d1_wait_for_send_xoff == d2_wait_for_send_xoff &&
				d2_wait_for_send_xoff == cmm_wait_for_send_xoff &&
				cmm_wait_for_send_xoff == ap_wait_for_send_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d]", d0_wait_for_send_xoff);
		} else {
#if DUMP_AP_UART_DBG_INFO
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d-%d-%d-%d-%d]",
				d0_wait_for_send_xoff, d1_wait_for_send_xoff,
				d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
				ap_wait_for_send_xoff);
#else
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d-%d-%d-%d]",
				d0_wait_for_send_xoff, d1_wait_for_send_xoff,
				d2_wait_for_send_xoff, cmm_wait_for_send_xoff);
#endif
		}

		if (d0_detect_xoff == d1_detect_xoff &&
				d1_detect_xoff == d2_detect_xoff &&
				d2_detect_xoff == cmm_detect_xoff &&
				cmm_detect_xoff == ap_detect_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d]", d0_detect_xoff);
		} else {
#if DUMP_AP_UART_DBG_INFO
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d-%d]",
				d0_detect_xoff, d1_detect_xoff,
				d2_detect_xoff, cmm_detect_xoff,
				ap_detect_xoff);
#else
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d]",
				d0_detect_xoff, d1_detect_xoff,
				d2_detect_xoff, cmm_detect_xoff);
#endif
		}

		pr_info("%s\n", dmp_info_buf);
	}

	pre_trigger_point = trigger_point;
	return 0;
}

int uarthub_core_debug_clk_info_worker(const char *tag)
{
	int len = 0;

	uarthub_debug_clk_info_ctrl.tag[0] = '\0';

	if (tag != NULL) {
		len = snprintf(uarthub_debug_clk_info_ctrl.tag,
			sizeof(uarthub_debug_clk_info_ctrl.tag), "%s", tag);
		if (len < 0) {
			uarthub_debug_clk_info_ctrl.tag[0] = '\0';
			pr_info("%s tag is NULL\n", __func__);
		}
	}

	queue_work(uarthub_workqueue, &uarthub_debug_clk_info_ctrl.debug_info_work);

	return 0;
}

static void debug_clk_info_work_worker_handler(struct work_struct *work)
{
	struct debug_info_ctrl *queue = container_of(work, struct debug_info_ctrl, debug_info_work);

	uarthub_core_debug_clk_info(queue->tag);
}

int uarthub_core_debug_clk_info(const char *tag)
{
	int val = 0;
	int apb_bus_clk_enable = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int len = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (!g_uarthub_plat_ic_ops)
		return -1;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return -2;
	}

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	apb_bus_clk_enable = uarthub_core_is_apb_bus_clk_enable();
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB=[0x%x]", apb_bus_clk_enable);

	if (apb_bus_clk_enable == 0) {
		pr_info("%s\n", dmp_info_buf);
		return -2;
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_cg_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uarthub_cg_info();
		if (val >= 0) {
			/* the expect value is 0x0 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",HCG=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_peri_clk_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_peri_clk_info();
		if (val >= 0) {
			/* the expect value is 0x800 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UPCLK=[0x%x]", val);
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_spm_res_info();
		if (val == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SPM=[1]");
		} else if (val == 0) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SPM=[0]");
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SPM=[F]");
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info();
		if (val >= 0) {
			/* the expect value is 0x1 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UVPLL=[%d]", val);
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info();
		if (val >= 0) {
			/* the expect value is 0x2 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UMUX=[0x%x]", val);
		}
	}

	dev0_sta = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr));
	dev1_sta = ((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_STA(
		intfhub_base_remap_addr)) : 0);
	dev2_sta = ((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_STA(
		intfhub_base_remap_addr)) : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
	}

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_core_debug_byte_cnt_info(const char *tag)
{
	struct uarthub_uart_ip_debug_info debug2 = {0};
	struct uarthub_uart_ip_debug_info debug3 = {0};
	struct uarthub_uart_ip_debug_info debug4 = {0};
	struct uarthub_uart_ip_debug_info debug5 = {0};
	struct uarthub_uart_ip_debug_info debug6 = {0};
	struct uarthub_uart_ip_debug_info debug7 = {0};
	void __iomem *ap_uart_base_remap_addr = NULL;
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0, cmm_sta = 0, ap_sta = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_max_dev <= 0)
		return -1;

	uarthub_core_clk_univpll_ctrl(1);
	if (uarthub_core_is_univpll_on() == 0) {
		pr_notice("[%s] uarthub_core_is_univpll_on=[0]\n", __func__);
		uarthub_core_clk_univpll_ctrl(0);
		return -1;
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr)
		ap_uart_base_remap_addr =
			g_uarthub_plat_ic_ops->uarthub_plat_get_ap_uart_base_addr();

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	debug2.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev0_base_remap_addr));
	debug3.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev0_base_remap_addr));
	debug4.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev0_base_remap_addr));
	debug5.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev0_base_remap_addr));
	debug6.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev0_base_remap_addr));
	debug7.dev0 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev0_base_remap_addr));

	if (g_max_dev >= 2) {
		debug2.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev1_base_remap_addr));
		debug3.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev1_base_remap_addr));
		debug4.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev1_base_remap_addr));
		debug5.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev1_base_remap_addr));
		debug6.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev1_base_remap_addr));
		debug7.dev1 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev1_base_remap_addr));
	}

	if (g_max_dev >= 3) {
		debug2.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_2(dev2_base_remap_addr));
		debug3.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_3(dev2_base_remap_addr));
		debug4.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_4(dev2_base_remap_addr));
		debug5.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_5(dev2_base_remap_addr));
		debug6.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_6(dev2_base_remap_addr));
		debug7.dev2 = UARTHUB_REG_READ(UARTHUB_DEBUG_7(dev2_base_remap_addr));
	}

	debug2.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_2(cmm_base_remap_addr));
	debug3.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_3(cmm_base_remap_addr));
	debug4.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_4(cmm_base_remap_addr));
	debug5.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_5(cmm_base_remap_addr));
	debug6.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_6(cmm_base_remap_addr));
	debug7.cmm = UARTHUB_REG_READ(UARTHUB_DEBUG_7(cmm_base_remap_addr));

	if (ap_uart_base_remap_addr) {
#if DUMP_AP_UART_DBG_INFO
		debug2.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_2(ap_uart_base_remap_addr));
		debug3.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_3(ap_uart_base_remap_addr));
		debug4.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_4(ap_uart_base_remap_addr));
		debug5.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_5(ap_uart_base_remap_addr));
		debug6.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_6(ap_uart_base_remap_addr));
		debug7.ap = UARTHUB_REG_READ(UARTHUB_DEBUG_7(ap_uart_base_remap_addr));
#else
		debug2.ap = debug2.dev0;
		debug3.ap = debug3.dev0;
		debug4.ap = debug4.dev0;
		debug5.ap = debug5.dev0;
		debug6.ap = debug6.dev0;
		debug7.ap = debug7.dev0;
#endif
	}
	uarthub_core_clk_univpll_ctrl(0);

	dev0_sta = (((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4));
	dev1_sta = (((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4));
	dev2_sta = (((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4));
	cmm_sta = (((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4));
	ap_sta = (((debug5.ap & 0xF0) >> 4) + ((debug6.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d", dev0_sta);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d-%d",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta);
#endif
	}

	dev0_sta = (((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4));
	dev1_sta = (((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4));
	dev2_sta = (((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4));
	cmm_sta = (((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4));
	ap_sta = (((debug2.ap & 0xF0) >> 4) + ((debug3.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d]", dev0_sta);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta);
#endif
	}

	dev0_sta = (debug7.dev0 & 0x3F);
	dev1_sta = (debug7.dev1 & 0x3F);
	dev2_sta = (debug7.dev2 & 0x3F);
	cmm_sta = (debug7.cmm & 0x3F);
	ap_sta = (debug7.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo=[R:%d", dev0_sta);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo=[R:%d-%d-%d-%d-%d",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo=[R:%d-%d-%d-%d",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta);
#endif
	}

	dev0_sta = (debug4.dev0 & 0x3F);
	dev1_sta = (debug4.dev1 & 0x3F);
	dev2_sta = (debug4.dev2 & 0x3F);
	cmm_sta = (debug4.cmm & 0x3F);
	ap_sta = (debug4.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d]", dev0_sta);
	} else {
#if DUMP_AP_UART_DBG_INFO
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
#else
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d-%d]",
			dev0_sta, dev1_sta, dev2_sta, cmm_sta);
#endif
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_hwccf_univpll_on_info();
		if (val >= 0) {
			/* the expect value is 0x1 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UVPLL=[%d]", val);
		}
	}

	if (g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info) {
		val = g_uarthub_plat_ic_ops->uarthub_plat_get_uart_mux_info();
		if (val >= 0) {
			/* the expect value is 0x2 */
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",UMUX=[0x%x]", val);
		}
	}

	dev0_sta = UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV0_STA(intfhub_base_remap_addr));
	dev1_sta = ((g_max_dev >= 2) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV1_STA(
		intfhub_base_remap_addr)) : 0);
	dev2_sta = ((g_max_dev >= 3) ? UARTHUB_REG_READ(UARTHUB_INTFHUB_DEV2_STA(
		intfhub_base_remap_addr)) : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
	}

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_STA0(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	val = UARTHUB_REG_READ(UARTHUB_INTFHUB_DBG(intfhub_base_remap_addr));
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

/*---------------------------------------------------------------------------*/

module_init(uarthub_core_init);
module_exit(uarthub_core_exit);

/*---------------------------------------------------------------------------*/

MODULE_AUTHOR("CTD/SE5/CS5/Johnny.Yao");
MODULE_DESCRIPTION("MTK UARTHUB Driver$1.0$");
MODULE_LICENSE("GPL");
