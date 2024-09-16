/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/pinctrl/consumer.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/poll.h>

#include <linux/io.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include "gps_lna_drv.h"

#define PFX                         "[LNA_CTL] "
#define GPS_LOG_INFO                 2
static unsigned int gDbgLevel = GPS_LOG_INFO;

#define GPS_INFO_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_INFO)	\
		pr_info(PFX "[I]%s: "  fmt, __func__, ##arg);	\
} while (0)


/* #ifdef CONFIG_OF */
const struct of_device_id gps_lna_of_ids[] = {
	{ .compatible = "mediatek,gps", },
	{}
};
/* #endif */

enum gps_lna_pinctrl_state_enum {
	GPS_DL_L1_LNA_DISABLE,
	GPS_DL_L1_LNA_DSP_CTRL,
	GPS_DL_L1_LNA_ENABLE,
	GPS_DL_L5_LNA_DISABLE,
	GPS_DL_L5_LNA_DSP_CTRL,
	GPS_DL_L5_LNA_ENABLE,
	GPS_DL_PINCTRL_STATE_CNT
};

const char *const gps_lna_pinctrl_state_name_list[GPS_DL_PINCTRL_STATE_CNT] = {
	"gps_l1_lna_disable",
	"gps_l1_lna_dsp_ctrl",
	"gps_l1_lna_enable",
	"gps_l5_lna_disable",
	"gps_l5_lna_dsp_ctrl",
	"gps_l5_lna_enable",
};

struct pinctrl_state *g_gps_lna_pinctrl_state_struct_list[GPS_DL_PINCTRL_STATE_CNT];
struct pinctrl *g_gps_lna_pinctrl_ptr;

void gps_lna_pinctrl_show_info(void)
{
	enum gps_lna_pinctrl_state_enum state_id;
	const char *p_name;
	struct pinctrl_state *p_state;

	GPS_INFO_FUNC("pinctrl_ptr = 0x%p", g_gps_lna_pinctrl_ptr);

	for (state_id = 0; state_id < GPS_DL_PINCTRL_STATE_CNT; state_id++) {
		p_name = gps_lna_pinctrl_state_name_list[state_id];
		p_state = g_gps_lna_pinctrl_state_struct_list[state_id];
		GPS_INFO_FUNC("state id = %d, ptr = 0x%p, name = %s", state_id, p_state, p_name);
	}
}

void gps_lna_pinctrl_context_init(void)
{
	enum gps_lna_pinctrl_state_enum state_id;
	const char *p_name;
	struct pinctrl_state *p_state;

	if (IS_ERR(g_gps_lna_pinctrl_ptr)) {
		GPS_INFO_FUNC("pinctrl is error");
		return;
	}

	for (state_id = 0; state_id < GPS_DL_PINCTRL_STATE_CNT; state_id++) {
		p_name = gps_lna_pinctrl_state_name_list[state_id];
		p_state = pinctrl_lookup_state(g_gps_lna_pinctrl_ptr, p_name);

		if (IS_ERR(p_state)) {
			GPS_INFO_FUNC("lookup fail: state id = %d, name = %s", state_id, p_name);
			g_gps_lna_pinctrl_state_struct_list[state_id] = NULL;
			continue;
		}

		g_gps_lna_pinctrl_state_struct_list[state_id] = p_state;
		GPS_INFO_FUNC("lookup okay: state id = %d, name = %s", state_id, p_name);
	}
}

void gps_lna_pin_ctrl(enum gps_lna_link_id_enum link_id, bool dsp_is_on, bool force_en)
{
	struct pinctrl_state *p_state = NULL;
	int ret;

	/*ASSERT_LINK_ID(link_id, GDL_VOIDF());*/

	if (GPS_DATA_LINK_ID0 == link_id) {
		if (dsp_is_on && force_en)
			p_state = g_gps_lna_pinctrl_state_struct_list[GPS_DL_L1_LNA_ENABLE];
		else if (dsp_is_on)
			p_state = g_gps_lna_pinctrl_state_struct_list[GPS_DL_L1_LNA_DSP_CTRL];
		else
			p_state = g_gps_lna_pinctrl_state_struct_list[GPS_DL_L1_LNA_DISABLE];
	}

	if (GPS_DATA_LINK_ID1 == link_id) {
		if (dsp_is_on && force_en)
			p_state = g_gps_lna_pinctrl_state_struct_list[GPS_DL_L5_LNA_ENABLE];
		else if (dsp_is_on)
			p_state = g_gps_lna_pinctrl_state_struct_list[GPS_DL_L5_LNA_DSP_CTRL];
		else
			p_state = g_gps_lna_pinctrl_state_struct_list[GPS_DL_L5_LNA_DISABLE];
	}

	if (p_state == NULL) {
		GPS_INFO_FUNC("link_id = %d, on = %d, force = %d, state is null", link_id, dsp_is_on, force_en);
		return;
	}

	if (g_gps_lna_pinctrl_ptr == NULL) {
		GPS_INFO_FUNC("link_id = %d, on = %d, force = %d, g_gps_lna_pinctrl_ptr is null",
			link_id, dsp_is_on, force_en);
		return;
	}
	ret = pinctrl_select_state(g_gps_lna_pinctrl_ptr, p_state);
	if (ret != 0)
		GPS_INFO_FUNC("link_id = %d, on = %d, force = %d, select ret = %d", link_id,  dsp_is_on, force_en, ret);
	else
		GPS_INFO_FUNC("link_id = %d, on = %d, force = %d, select ret = %d", link_id,  dsp_is_on, force_en, ret);
}

static int gps_lna_probe(struct platform_device *pdev)
{
	struct resource *irq;
	int i;
	bool okay;

	g_gps_lna_pinctrl_ptr = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(g_gps_lna_pinctrl_ptr))
		GPS_INFO_FUNC("devm_pinctrl_get fail");
	if (!IS_ERR(g_gps_lna_pinctrl_ptr)) {
		gps_lna_pinctrl_context_init();
		gps_lna_pinctrl_show_info();
	}
	GPS_INFO_FUNC("do gps_lna_probe");

	return 0;
}

static int gps_lna_remove(struct platform_device *pdev)
{
	GPS_INFO_FUNC("do gps_lna_remove");
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int gps_lna_drv_suspend(struct device *dev)
{
#if 0
	struct platform_device *pdev = to_platform_device(dev);
	pm_message_t state = PMSG_SUSPEND;

	return mtk_btif_suspend(pdev, state);
#endif
	return 0;
}

static int gps_lna_drv_resume(struct device *dev)
{
#if 0
	struct platform_device *pdev = to_platform_device(dev);

	return mtk_btif_resume(pdev);
#endif
	return 0;
}

static int gps_lna_plat_suspend(struct platform_device *pdev, pm_message_t state)
{
#if 0
	int i_ret = 0;
	struct _mtk_btif_ *p_btif = NULL;

	BTIF_DBG_FUNC("++\n");
	p_btif = platform_get_drvdata(pdev);
	i_ret = _btif_suspend(p_btif);
	BTIF_DBG_FUNC("--, i_ret:%d\n", i_ret);
	return i_ret;
#endif
	return 0;
}

static int gps_lna_plat_resume(struct platform_device *pdev)
{
#if 0
	int i_ret = 0;
	struct _mtk_btif_ *p_btif = NULL;

	BTIF_DBG_FUNC("++\n");
	p_btif = platform_get_drvdata(pdev);
	i_ret = _btif_resume(p_btif);
	BTIF_DBG_FUNC("--, i_ret:%d\n", i_ret);
	return i_ret;
#endif
	return 0;
}


const struct dev_pm_ops gps_lna_drv_pm_ops = {
	.suspend = gps_lna_drv_suspend,
	.resume = gps_lna_drv_resume,
};

struct platform_driver gps_lna_dev_drv = {
	.probe = gps_lna_probe,
	.remove = gps_lna_remove,
/* #ifdef CONFIG_PM */
	.suspend = gps_lna_plat_suspend,
	.resume = gps_lna_plat_resume,
/* #endif */
	.driver = {
		.name = "gps", /* mediatek,gps */
		.owner = THIS_MODULE,
/* #ifdef CONFIG_PM */
		.pm = &gps_lna_drv_pm_ops,
/* #endif */
/* #ifdef CONFIG_OF */
		.of_match_table = gps_lna_of_ids,
/* #endif */
	}
};

static ssize_t driver_flag_read(struct device_driver *drv, char *buf)
{
	return sprintf(buf, "gps lna driver debug level:%d\n", 1);
}

static ssize_t driver_flag_set(struct device_driver *drv,
				   const char *buffer, size_t count)
{
	GPS_INFO_FUNC("buffer = %s, count = %zd", buffer, count);
	return count;
}

#define DRIVER_ATTR(_name, _mode, _show, _store) \
	struct driver_attribute driver_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)
static DRIVER_ATTR(flag, 0644, driver_flag_read, driver_flag_set);


int gps_lna_linux_plat_drv_register(void)
{
	int result;

	result = platform_driver_register(&gps_lna_dev_drv);
	/* if (result) */
	GPS_INFO_FUNC("platform_driver_register, ret(%d)\n", result);

	result = driver_create_file(&gps_lna_dev_drv.driver, &driver_attr_flag);
	/* if (result) */
	GPS_INFO_FUNC("driver_create_file, ret(%d)\n", result);

	return 0;
}

int gps_lna_linux_plat_drv_unregister(void)
{
	driver_remove_file(&gps_lna_dev_drv.driver, &driver_attr_flag);
	platform_driver_unregister(&gps_lna_dev_drv);

	return 0;
}

