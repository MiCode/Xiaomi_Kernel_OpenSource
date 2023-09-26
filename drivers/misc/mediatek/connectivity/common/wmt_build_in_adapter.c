/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__*/
#include <linux/kernel.h>

#include "wmt_build_in_adapter.h"
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/cdev.h>

/*device tree mode*/
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#endif

#include <linux/interrupt.h>
#include <linux/ratelimit.h>

#define CONNADP_LOG_LOUD    4
#define CONNADP_LOG_DBG     3
#define CONNADP_LOG_INFO    2
#define CONNADP_LOG_WARN    1
#define CONNADP_LOG_ERR     0

#if defined(CONFIG_MACH_MT6873)
#include <clk-mt6873-pg.h>
#define DUMP_CLOCK_FAIL_CALLBACK 1
#endif

/*******************************************************************************
 * Connsys adaptation layer logging utility
 ******************************************************************************/
static unsigned int gConnAdpDbgLvl = CONNADP_LOG_INFO;

#define CONNADP_LOUD_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_LOUD) \
		pr_info("[L]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_DBG_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_DBG) \
		pr_info("[D]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_INFO_FUNC(fmt, arg...)  \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_INFO) \
		pr_info("[I]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_WARN_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_WARN) \
		pr_info("[W]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_ERR_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_ERR) \
		pr_info("[E]%s(%d):"  fmt, __func__, __LINE__, ##arg); \
} while (0)

/* device node related macro */
#define CONN_DBG_DEV_NUM 1
#define CONN_DBG_DRVIER_NAME "conn_dbg_drv"
#define CONN_DBG_DEVICE_NAME "conn_dbg_dev"
#define CONN_DBG_DEV_MAJOR 156

/* device node related */
static int gConnDbgMajor = CONN_DBG_DEV_MAJOR;
static struct class *pConnDbgClass;
static struct device *pConnDbgDev;
static struct cdev gConnDbgdev;

/*******************************************************************************
 * Bridging from platform -> wmt_drv.ko
 ******************************************************************************/
static struct wmt_platform_bridge bridge;

ssize_t conn_dbg_dev_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *f_pos)
{
	pr_info("%s\n", __func__);
	if (bridge.debug_cb)
		return bridge.debug_cb(filp, buffer, count, f_pos);

	return 0;
}

const struct file_operations gConnDbgDevFops = {
	.write = conn_dbg_dev_write,
};

static int conn_dbg_dev_init(void)
{
	dev_t dev_id = MKDEV(gConnDbgMajor, 0);
	int ret = 0;

	ret = register_chrdev_region(dev_id, CONN_DBG_DEV_NUM, CONN_DBG_DRVIER_NAME);
	if (ret) {
		pr_info("%s fail to register chrdev.(%d)\n", __func__, ret);
		return -1;
	}

	cdev_init(&gConnDbgdev, &gConnDbgDevFops);
	gConnDbgdev.owner = THIS_MODULE;

	ret = cdev_add(&gConnDbgdev, dev_id, CONN_DBG_DEV_NUM);
	if (ret) {
		pr_info("cdev_add() fails (%d)\n", ret);
		goto err1;
	}

	pConnDbgClass = class_create(THIS_MODULE, CONN_DBG_DEVICE_NAME);
	if (IS_ERR(pConnDbgClass)) {
		pr_info("class create fail, error code(%ld)\n", PTR_ERR(pConnDbgClass));
		goto err2;
	}

	pConnDbgDev = device_create(pConnDbgClass, NULL, dev_id, NULL, CONN_DBG_DEVICE_NAME);
	if (IS_ERR(pConnDbgDev)) {
		pr_info("device create fail, error code(%ld)\n", PTR_ERR(pConnDbgDev));
		goto err3;
	}

	return 0;
err3:

	pr_info("[%s] err3", __func__);
	if (pConnDbgClass) {
		class_destroy(pConnDbgClass);
		pConnDbgClass = NULL;
	}
err2:
	pr_info("[%s] err2", __func__);
	cdev_del(&gConnDbgdev);

err1:
	pr_info("[%s] err1", __func__);
	unregister_chrdev_region(dev_id, CONN_DBG_DEV_NUM);

	return -1;
}

static int conn_dbg_dev_deinit(void)
{
	dev_t dev_id = MKDEV(gConnDbgMajor, 0);

	if (pConnDbgDev) {
		device_destroy(pConnDbgClass, dev_id);
		pConnDbgDev = NULL;
	}

	if (pConnDbgClass) {
		class_destroy(pConnDbgClass);
		pConnDbgClass = NULL;
	}

	cdev_del(&gConnDbgdev);
	unregister_chrdev_region(dev_id, CONN_DBG_DEV_NUM);

	return 0;
}

#ifdef DUMP_CLOCK_FAIL_CALLBACK
static void wmt_clock_debug_dump(enum subsys_id sys)
{
	if (sys == SYS_CONN)
		mtk_wcn_cmb_stub_clock_fail_dump();
}

static struct pg_callbacks wmt_clk_subsys_handle = {
	.debug_dump = wmt_clock_debug_dump
};
#endif

void wmt_export_platform_bridge_register(struct wmt_platform_bridge *cb)
{
	if (unlikely(!cb))
		return;
	bridge.thermal_query_cb = cb->thermal_query_cb;
	bridge.trigger_assert_cb = cb->trigger_assert_cb;
	bridge.clock_fail_dump_cb = cb->clock_fail_dump_cb;
	bridge.conninfra_reg_readable_cb = cb->conninfra_reg_readable_cb;
	bridge.conninfra_reg_is_bus_hang_cb = cb->conninfra_reg_is_bus_hang_cb;
	bridge.conninfra_reg_is_bus_hang_no_lock_cb = cb->conninfra_reg_is_bus_hang_no_lock_cb;
#ifdef DUMP_CLOCK_FAIL_CALLBACK
	register_pg_callback(&wmt_clk_subsys_handle);
#endif

	if (cb->debug_cb != NULL) {
		bridge.debug_cb = cb->debug_cb;
		conn_dbg_dev_init();
	}

	CONNADP_INFO_FUNC("\n");
}
EXPORT_SYMBOL(wmt_export_platform_bridge_register);

void wmt_export_platform_bridge_unregister(void)
{
	if (bridge.debug_cb)
		conn_dbg_dev_deinit();
	memset(&bridge, 0, sizeof(struct wmt_platform_bridge));
	CONNADP_INFO_FUNC("\n");
}
EXPORT_SYMBOL(wmt_export_platform_bridge_unregister);

int mtk_wcn_cmb_stub_query_ctrl(void)
{
	CONNADP_DBG_FUNC("\n");
	if (unlikely(!bridge.thermal_query_cb)) {
		CONNADP_WARN_FUNC("Thermal query not registered\n");
		return -1;
	} else
		return bridge.thermal_query_cb();
}

int mtk_wcn_cmb_stub_trigger_assert(void)
{
	CONNADP_DBG_FUNC("\n");
	/* dump backtrace for checking assert reason */
	dump_stack();
	if (unlikely(!bridge.trigger_assert_cb)) {
		CONNADP_WARN_FUNC("Trigger assert not registered\n");
		return -1;
	} else
		return bridge.trigger_assert_cb();
}

void mtk_wcn_cmb_stub_clock_fail_dump(void)
{
	CONNADP_DBG_FUNC("\n");
	if (unlikely(!bridge.clock_fail_dump_cb))
		CONNADP_WARN_FUNC("Clock fail dump not registered\n");
	else
		bridge.clock_fail_dump_cb();
}

int mtk_wcn_conninfra_reg_readable(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, 5*HZ, 1);

	if (unlikely(!bridge.conninfra_reg_readable_cb)) {
		if (__ratelimit(&_rs))
			CONNADP_WARN_FUNC("reg_readable not registered\n");
		return -1;
	} else
		return bridge.conninfra_reg_readable_cb();
}

int mtk_wcn_conninfra_is_bus_hang(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, 5*HZ, 1);

	if (unlikely(!bridge.conninfra_reg_is_bus_hang_cb)) {
		if (__ratelimit(&_rs))
			CONNADP_WARN_FUNC("is_bus_hang not registered\n");
		return -1;
	} else
		return bridge.conninfra_reg_is_bus_hang_cb();
}

int mtk_wcn_conninfra_conn_bus_dump(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, 5*HZ, 1);
	int ret = 0;

	if (unlikely(!bridge.conninfra_reg_is_bus_hang_no_lock_cb)) {
		if (__ratelimit(&_rs))
			CONNADP_WARN_FUNC("is_bus_hang_no_lock not registered\n");
		ret = -1;
	} else {
		ret = bridge.conninfra_reg_is_bus_hang_no_lock_cb();
	}

	return ret;
}

/*******************************************************************************
 * SDIO integration with platform MMC driver
 ******************************************************************************/
static void mtk_wcn_cmb_sdio_request_eirq(msdc_sdio_irq_handler_t irq_handler,
					  void *data);
static void mtk_wcn_cmb_sdio_enable_eirq(void);
static void mtk_wcn_cmb_sdio_disable_eirq(void);
static void mtk_wcn_cmb_sdio_register_pm(pm_callback_t pm_cb, void *data);

struct sdio_ops mt_sdio_ops[4] = {
	{NULL, NULL, NULL, NULL},
	{NULL, NULL, NULL, NULL},
	{mtk_wcn_cmb_sdio_request_eirq, mtk_wcn_cmb_sdio_enable_eirq,
		mtk_wcn_cmb_sdio_disable_eirq, mtk_wcn_cmb_sdio_register_pm},
	{mtk_wcn_cmb_sdio_request_eirq, mtk_wcn_cmb_sdio_enable_eirq,
		mtk_wcn_cmb_sdio_disable_eirq, mtk_wcn_cmb_sdio_register_pm}
};

static atomic_t sdio_claim_irq_enable_flag;
static atomic_t irq_enable_flag;

static msdc_sdio_irq_handler_t mtk_wcn_cmb_sdio_eirq_handler;
static void *mtk_wcn_cmb_sdio_eirq_data;

unsigned int wifi_irq = 0xffffffff;
EXPORT_SYMBOL(wifi_irq);

pm_callback_t mtk_wcn_cmb_sdio_pm_cb;
EXPORT_SYMBOL(mtk_wcn_cmb_sdio_pm_cb);

void *mtk_wcn_cmb_sdio_pm_data;
EXPORT_SYMBOL(mtk_wcn_cmb_sdio_pm_data);

static int _mtk_wcn_sdio_irq_flag_set(int flag)
{
	if (flag != 0)
		atomic_set(&sdio_claim_irq_enable_flag, 1);
	else
		atomic_set(&sdio_claim_irq_enable_flag, 0);

	CONNADP_DBG_FUNC("sdio_claim_irq_enable_flag:%d\n",
			atomic_read(&sdio_claim_irq_enable_flag));

	return atomic_read(&sdio_claim_irq_enable_flag);
}

int wmt_export_mtk_wcn_sdio_irq_flag_set(int flag)
{
	return _mtk_wcn_sdio_irq_flag_set(flag);
}
EXPORT_SYMBOL(wmt_export_mtk_wcn_sdio_irq_flag_set);

static irqreturn_t mtk_wcn_cmb_sdio_eirq_handler_stub(int irq, void *data)
{
	if ((mtk_wcn_cmb_sdio_eirq_handler != NULL) &&
	    (atomic_read(&sdio_claim_irq_enable_flag) != 0))
		mtk_wcn_cmb_sdio_eirq_handler(mtk_wcn_cmb_sdio_eirq_data);
	return IRQ_HANDLED;
}

void mtk_wcn_cmb_sdio_request_eirq_by_wmt(void)
{
#ifdef CONFIG_OF
	int ret = -EINVAL;
	struct device_node *node;

	CONNADP_INFO_FUNC("enter\n");
	_mtk_wcn_sdio_irq_flag_set(0);
	atomic_set(&irq_enable_flag, 1);

	node = (struct device_node *)of_find_compatible_node(NULL, NULL,
					"mediatek,connectivity-combo");
	if (node) {
		wifi_irq = irq_of_parse_and_map(node, 0);/* get wifi eint num */
		ret = request_irq(wifi_irq, mtk_wcn_cmb_sdio_eirq_handler_stub,
				IRQF_TRIGGER_LOW, "WIFI-eint", NULL);
		CONNADP_DBG_FUNC("WIFI EINT irq %d !!\n", wifi_irq);

		if (ret)
			CONNADP_WARN_FUNC("WIFI EINT LINE NOT AVAILABLE!!\n");
		else
			mtk_wcn_cmb_sdio_disable_eirq();/*state:power off*/
	} else
		CONNADP_WARN_FUNC("can't find connectivity compatible node\n");

	CONNADP_INFO_FUNC("exit\n");
	return;
#endif
}
EXPORT_SYMBOL(mtk_wcn_cmb_sdio_request_eirq_by_wmt);

static void mtk_wcn_cmb_sdio_request_eirq(msdc_sdio_irq_handler_t irq_handler,
					  void *data)
{
#ifdef CONFIG_OF
	CONNADP_INFO_FUNC("enter\n");
	mtk_wcn_cmb_sdio_eirq_data = data;
	mtk_wcn_cmb_sdio_eirq_handler = irq_handler;

	CONNADP_INFO_FUNC("exit\n");
#else
	CONNADP_ERR_FUNC("not implemented\n");
#endif
}

static void mtk_wcn_cmb_sdio_register_pm(pm_callback_t pm_cb, void *data)
{
	CONNADP_DBG_FUNC("cmb_sdio_register_pm (0x%p, 0x%p)\n", pm_cb, data);
	/* register pm change callback */
	mtk_wcn_cmb_sdio_pm_cb = pm_cb;
	mtk_wcn_cmb_sdio_pm_data = data;
}

static void mtk_wcn_cmb_sdio_enable_eirq(void)
{
	if (atomic_read(&irq_enable_flag))
		CONNADP_DBG_FUNC("wifi eint has been enabled\n");
	else {
		atomic_set(&irq_enable_flag, 1);
		if (wifi_irq != 0xffffffff) {
			enable_irq(wifi_irq);
			CONNADP_DBG_FUNC(" enable WIFI EINT %d!\n", wifi_irq);
		}
	}
}

static void mtk_wcn_cmb_sdio_disable_eirq(void)
{
	if (!atomic_read(&irq_enable_flag))
		CONNADP_DBG_FUNC("wifi eint has been disabled!\n");
	else {
		if (wifi_irq != 0xffffffff) {
			disable_irq_nosync(wifi_irq);
			CONNADP_DBG_FUNC("disable WIFI EINT %d!\n", wifi_irq);
		}
		atomic_set(&irq_enable_flag, 0);
	}
}

void wmt_export_mtk_wcn_cmb_sdio_disable_eirq(void)
{
	mtk_wcn_cmb_sdio_disable_eirq();
}
EXPORT_SYMBOL(wmt_export_mtk_wcn_cmb_sdio_disable_eirq);
