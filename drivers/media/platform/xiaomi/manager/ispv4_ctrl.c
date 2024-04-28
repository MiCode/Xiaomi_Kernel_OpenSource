/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/mfd/core.h>
#include <linux/mfd/ispv4_defs.h>
#include <linux/interrupt.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include "ispv4_ctrl.h"
#include "ispv4_regops.h"

#define ISPV4_PMIC_PWR_ON_TIMEOUT 100
#define ISPV4_PMIC_PWR_OFF_DELAY 2000
#define ISPV4_PWR_ON_DELAY_US 200
#define ISPV4_CHIP_RELEASE_DELAY_MS 500
#define ISPV4_CPUPLL_POLLING_TIMEOUT 100
#define ISPV4_CPU_PWR_POLLING_TIMEOUT 100

#define ISPV4_CLK_ACTIVE_STATE        "ispv4_clk_active"
#define ISPV4_CLK_INACTIVE_STATE      "ispv4_clk_inactive"

void disable_wdt_irq_inirq(void *data);
void enable_wdt_irq(void *data);
struct platform_device *ispv4_ctrl_pdev;
EXPORT_SYMBOL_GPL(ispv4_ctrl_pdev);

static int ispv4_pintctrl_get(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	int ret = 0;

	priv->pinctrl_clk = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(priv->pinctrl_clk)) {
		ret = PTR_ERR(priv->pinctrl_clk);
		dev_err(dev, "get 32k pinctrl fail, ret = %d", ret);
	}

	dev_info(dev, "get 32k pinctrl success");
	return ret;
}

static int ispv4_clk_get(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);

	priv->bb_clk = devm_clk_get(&pdev->dev, "ispv4_bb_clk4");
	if (IS_ERR_OR_NULL(priv->bb_clk)) {
		dev_err(&pdev->dev, "%s, get bb_clk failed", __func__);
		return PTR_ERR(priv->bb_clk);
	}

	priv->sleep_clk = devm_clk_get(&pdev->dev, "sleep_clk");
	if (IS_ERR_OR_NULL(priv->sleep_clk)) {
		dev_err(&pdev->dev, "%s, get sleep_clk failed", __func__);
		return PTR_ERR(priv->sleep_clk);
	}
	dev_info(&pdev->dev, "%s, get ispv4 clk success", __func__);

	return 0;
}

static int ispv4_clk_put(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);

	devm_clk_put(&pdev->dev, priv->bb_clk);
	devm_clk_put(&pdev->dev, priv->sleep_clk);

	dev_info(&pdev->dev, "%s, release ispv4 clk success", __func__);

	return 0;
}

static int ispv4_clk_enable(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	struct pinctrl_state *pinctrl_st;
	int ret = 0;

	pinctrl_st = pinctrl_lookup_state(priv->pinctrl_clk,
				   ISPV4_CLK_ACTIVE_STATE);
	if (IS_ERR_OR_NULL(pinctrl_st)) {
		dev_err(&pdev->dev, "%s, find active state failed", __func__);
		return PTR_ERR(pinctrl_st);
	}

	ret = pinctrl_select_state(priv->pinctrl_clk, pinctrl_st);
	if (ret) {
		dev_err(&pdev->dev, "%s, select active state failed", __func__);
		return ret;
	}

	ret = clk_prepare_enable(priv->bb_clk);
	if (ret)  {
		dev_err(&pdev->dev, "%s, enable bb_clk failed", __func__);
		return ret;
	}
	dev_info(&pdev->dev, "%s, enable bb_clk success", __func__);

	ret = clk_prepare_enable(priv->sleep_clk);
	if (ret)  {
		dev_err(&pdev->dev, "%s, enable sleep_clk failed", __func__);
		goto release_bb_clk;
	}

	dev_info(&pdev->dev, "%s, enable sleep_clk success", __func__);
	return 0;

release_bb_clk:
	clk_disable_unprepare(priv->bb_clk);
	return ret;
}

static int ispv4_clk_disable(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	struct pinctrl_state *pinctrl_st;
	int ret;

	pinctrl_st = pinctrl_lookup_state(priv->pinctrl_clk,
				   ISPV4_CLK_INACTIVE_STATE);
	if (IS_ERR_OR_NULL(pinctrl_st)) {
		dev_err(&pdev->dev, "%s, find inactive state failed", __func__);
		return PTR_ERR(pinctrl_st);
	}

	ret = pinctrl_select_state(priv->pinctrl_clk, pinctrl_st);
	if (ret) {
		dev_err(&pdev->dev, "%s, select inactive state failed", __func__);
		return ret;
	}

	if (priv->bb_clk)
		clk_disable_unprepare(priv->bb_clk);
	if (priv->sleep_clk)
		clk_disable_unprepare(priv->sleep_clk);

	//ispv4_clk_put(pdev);

	dev_info(&pdev->dev, "%s success", __func__);
	return 0;
}

static int ispv4_config_intc_io(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	ispv4_irq_info_t *irq_info = &priv->irq_info;
	struct device_node *np;
	char ioname[128];
	int ret = 0;
	int index;

	np = of_find_node_by_name(NULL, "xm_ispv4_int");
	if (np == NULL) {
		pr_err("ispv4 Can not find intr device tree node\n");
		return -ENODEV;
	}

	for (index = 0; index < ISPV4_MAX_IRQ; index++) {
		snprintf(ioname, 128, "ispv4_gpio_int%d", index);

		irq_info->gpio[index] = of_get_named_gpio(np, ioname, 0);
		if (irq_info->gpio[index] < 0) {
			dev_err(dev, "get gpio[%d] failed!\n", index);
			return -EINVAL;
		}

		irq_info->gpio_irq[index] = gpio_to_irq(irq_info->gpio[index]);

		ret = devm_gpio_request(dev, irq_info->gpio[index], ioname);
		if (ret) {
			dev_err(dev, "request gpio[%d] failed\n", index);
			return ret;
		}

		ret = gpio_direction_input(irq_info->gpio[index]);
		if (ret) {
			dev_err(dev,
				"cannot set direction for interrupt gpio[%d]\n",
				index);
			return ret;
		}
	}

	return ret;
}

void register_wdt_cb(void *data, int (*cb)(void *), void *cb_priv)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	priv->wdt_notify = cb;
	priv->wdt_notify_priv = cb_priv;
}

int ispv4_wdt_notify(struct ispv4_ctrl_data *priv)
{
	int ret = -1;
	if (priv->wdt_notify != NULL)
		ret = priv->wdt_notify(priv->wdt_notify_priv);
	return ret;
}

static irqreturn_t ispv4_wdt_irq(int irq, void *pdata)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)pdata;
	struct device *dev;
	uint32_t irq_status = 0;
	int ret;

	pr_info("ispv4 get interrupt form gpio[%d]\n", irq);

	if (priv == NULL)
		return IRQ_NONE;

	ret = ispv4_regops_read(AP_INTC_G0R0_INT_STATUS_REG_ADDR, &irq_status);
	if (irq_status & (AP_WDT_INTR_BIT | AP_WDT_INTRT_BIT)) {
		/*clear wdt irq*/
		ret = ispv4_regops_write(ISPV4_WDT_RESET_REG_ADDR1, ISPV4_WDT_RESET_VAL);
		ret |= ispv4_regops_write(ISPV4_WDT_RESET_REG_ADDR2, ISPV4_WDT_RESET_VAL);
		if (ret) {
			pr_err("ispv4 %s wdt irq %d reset failed!\n", __func__, irq);
			return IRQ_HANDLED;
		}
		dev = &priv->pdev->dev;
		disable_wdt_irq_inirq(priv);
		ret = ispv4_wdt_notify(priv);
		if (ret)
			dev_err(dev, "wdt notify fail");
	} else {
		return IRQ_NONE;
	}
	return IRQ_HANDLED;
}

void enable_wdt_irq(void *data)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	int rounter_g0r0_mask;
	int ret;

	ispv4_irq_info_t *irq_info = &priv->irq_info;

	if (irq_info->gpio_irq[ISPV4_WDT_IRQ]) {

		ret = ispv4_regops_read(AP_INTC_G0R0_INT_MASK_REG_ADDR,
					&rounter_g0r0_mask);
		if (ret)
			return;
		ret = ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR,
					 rounter_g0r0_mask & (~(AP_WDT_SYS_RSTT_BIT |
								AP_WDT_SYS_RST_BIT)));
		if (ret)
			return;

		if (priv->wdt_en_flag == false) {
			enable_irq(irq_info->gpio_irq[ISPV4_WDT_IRQ]);
			priv->wdt_en_flag = true;
		}
	}
}

void _disable_wdt_irq(void *data, bool sync)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	ispv4_irq_info_t *irq_info = &priv->irq_info;
	int rounter_g0r0_mask;
	if (irq_info->gpio_irq[ISPV4_WDT_IRQ] && priv->wdt_en_flag) {
		ispv4_regops_read(AP_INTC_G0R0_INT_MASK_REG_ADDR,
					&rounter_g0r0_mask);
		ispv4_regops_write(AP_INTC_G0R0_INT_MASK_REG_ADDR,
					 rounter_g0r0_mask | (AP_WDT_SYS_RSTT_BIT |
								AP_WDT_SYS_RST_BIT));
		if (sync)
			disable_irq(irq_info->gpio_irq[ISPV4_WDT_IRQ]);
		else
			disable_irq_nosync(irq_info->gpio_irq[ISPV4_WDT_IRQ]);
		priv->wdt_en_flag = false;
	}
}

void disable_wdt_irq(void *data)
{
	_disable_wdt_irq(data, true);
}

void disable_wdt_irq_inirq(void *data)
{
	_disable_wdt_irq(data, false);
}

static int ispv4_assign_wdt_irq(struct platform_device *pdev)
{
	unsigned long irqflags;
	struct device *dev = &pdev->dev;
	struct ispv4_ctrl_data *priv = platform_get_drvdata(pdev);
	ispv4_irq_info_t *irq_info = &priv->irq_info;
	int ret = 0;

	//Enable wdt irq to AP
	irqflags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	if (irq_info->gpio_irq[ISPV4_WDT_IRQ]) {
		ret = devm_request_threaded_irq(
			dev, irq_info->gpio_irq[ISPV4_WDT_IRQ], NULL,
			ispv4_wdt_irq, irqflags, "ispv4_wdt_irq", priv);
		if (ret) {
			dev_err(dev,
				"devm_request_threaded_irq for wdt fail\n");
			return ret;
		}
		// Will open at cpu running.
		disable_irq(irq_info->gpio_irq[ISPV4_WDT_IRQ]);
		priv->wdt_en_flag = false;
	}

	return ret;
}

void register_sof_cb(void *data, int (*cb)(void *), void *cb_priv)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	extern void enable_sof_irq(void *data);
	priv->sof_notify = cb;
	priv->sof_notify_priv = cb_priv;
	enable_sof_irq(priv);
}

int ispv4_sof_notify(struct ispv4_ctrl_data *priv)
{
	int ret = -1;
	if (priv->sof_notify != NULL)
		ret = priv->sof_notify(priv->sof_notify_priv);
	return ret;
}

static irqreturn_t ispv4_sof_irq(int irq, void *pdata)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)pdata;
	struct device *dev;
	int rounter_g2r1_status;
	int ret;

	pr_info("ispv4 %s get interrupt form gpio[%d] for sof!\n",
		__func__, irq);

	if (priv == NULL)
		return IRQ_NONE;

	ret = ispv4_regops_read(AP_INTC_G2R1_INT_STATUS_REG_ADDR,
				&rounter_g2r1_status);
	if (ret)
		return IRQ_NONE;

	if ((rounter_g2r1_status & AP_SOF_INT_BIT) == 0)
		return IRQ_NONE;

	dev = &priv->pdev->dev;
	ret = ispv4_sof_notify(priv);
	if (ret)
		dev_err(dev, "sof notify fail");

	return IRQ_HANDLED;
}

void enable_sof_irq(void *data)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	int rounter_g2r1_mask;
	int ret;

	ispv4_irq_info_t *irq_info = &priv->irq_info;

	if (irq_info->gpio_irq[ISPV4_SOF_IRQ]) {

		ret = ispv4_regops_read(AP_INTC_G2R1_INT_MASK_REG_ADDR,
					&rounter_g2r1_mask);
		if (ret)
			return;
		ret = ispv4_regops_write(AP_INTC_G2R1_INT_MASK_REG_ADDR,
					 rounter_g2r1_mask & (~AP_SOF_INT_BIT));
		if (ret)
			return;
	}
}

__maybe_unused
static int ispv4_assign_sof_irq(struct platform_device *pdev)
{
	unsigned long irqflags;
	struct device *dev = &pdev->dev;
	struct ispv4_ctrl_data *priv = platform_get_drvdata(pdev);
	ispv4_irq_info_t *irq_info = &priv->irq_info;
	int ret = 0;

	irqflags = IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_SHARED;
	if (irq_info->gpio_irq[ISPV4_SOF_IRQ]) {
		ret = devm_request_threaded_irq(
			dev, irq_info->gpio_irq[ISPV4_SOF_IRQ], NULL,
			ispv4_sof_irq, irqflags, "ispv4_sof_irq", priv);
		if (ret) {
			dev_err(dev,
				"devm_request_threaded_irq for sof fail\n");
			return ret;
		}

		//TODO: whether to turn on or off sof irq in camera process
		/*Enable sof irq*/
		enable_sof_irq(priv);
	}

	return ret;
}

static int ispv4_turnoff_pmu_irq(void)
{
	int ret;

	ret = ispv4_regops_clear_and_set(ISPV4_PMU_SRC_REG_ADDR,
				ISPV4_PMU_SRC_BIT, ISPV4_PMU_SRC_BIT);

	ret |= ispv4_regops_write(AP_INTC_G0R1_INT_MASK_REG_ADDR, 0xffffffff);
	ret |= ispv4_regops_write(AP_INTC_G1R1_INT_MASK_REG_ADDR, 0xffffffff);
	ret |= ispv4_regops_write(AP_INTC_G2R1_INT_MASK_REG_ADDR, 0xffffffff);
	return ret;
}

static irqreturn_t ispv4_pmu_irq(int irq, void *pdata)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)pdata;
	struct device *dev;
	int irq_status = 0;
	int ret;

	if (priv == NULL || priv->pmu_reply == true)
		return IRQ_NONE;

	dev = &priv->pdev->dev;

	ret = ispv4_regops_read(AP_INTC_G1R1_INT_STATUS_REG_ADDR,
				&irq_status);
	if (ret)
		return IRQ_NONE;

	if ((irq_status & AP_PMU_INT_BIT) == 0)
		return IRQ_NONE;
	dev_info(dev, "get interrupt form gpio[%d] for pmu!\n", irq);

	ret = ispv4_turnoff_pmu_irq();
	if (ret)
		dev_err(dev, "Turn off pmu interrupt done!\n");
	else
		priv->pmu_reply = true;

	complete(&priv->pmu_com);
	dev_info(dev, "get pmu interrupt[%d] done!\n", irq);

	return IRQ_HANDLED;
}

static int ispv4_assign_pmu_irq(struct platform_device *pdev)
{
	unsigned long irqflags;
	struct device *dev = &pdev->dev;
	struct ispv4_ctrl_data *priv = platform_get_drvdata(pdev);
	ispv4_irq_info_t *irq_info = &priv->irq_info;
	int ret = 0;

	priv->pmu_reply=false;
	init_completion(&priv->pmu_com);

	irqflags = IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED;
	if (irq_info->gpio_irq[ISPV4_PMU_IRQ]) {
		ret = devm_request_threaded_irq(
			dev, irq_info->gpio_irq[ISPV4_PMU_IRQ], NULL,
			ispv4_pmu_irq, irqflags, "ispv4_pmu_irq", priv);
		if (ret) {
			dev_err(dev,
				"devm_request_threaded_irq for pmu fail\n");
			return ret;
		}
	}

	return ret;
}

void ispv4_mipi_iso_enable(void *data)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	gpio_set_value(priv->ispv4_mipi_iso_en, 1);
}

void ispv4_mipi_iso_disable(void *data)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	gpio_set_value(priv->ispv4_mipi_iso_en, 0);
}

static int ispv4_config_ctlio(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	//MIPI CTRL
	priv->ispv4_mipi_iso_en = of_get_named_gpio(np, "ispv4_mipi_iso_en", 0);
	if (priv->ispv4_mipi_iso_en < 0) {
		dev_err(&pdev->dev, "%s, get ispv4_mipi_iso_en fail", __func__);
		return priv->ispv4_mipi_iso_en;
	}

	ret = devm_gpio_request(&pdev->dev, priv->ispv4_mipi_iso_en, "mipi_iso_en");
	if (ret) {
		dev_err(&pdev->dev, "%s, request ispv4_mipi_iso_en fail", __func__);
		return ret;
	}

	ret = gpio_direction_output(priv->ispv4_mipi_iso_en, 0);
	if (ret) {
		dev_err(&pdev->dev, "%s, set ispv4_mipi_iso_en output low fail",
				__func__);
		return ret;
	}
	//PWR ON
	priv->ispv4_pmic_pwr_on = of_get_named_gpio(np, "ispv4_pmic_pwr_on", 0);
	if (priv->ispv4_pmic_pwr_on < 0) {
		dev_err(&pdev->dev, "%s, get ispv4_pmic_pwr_on fail", __func__);
		return priv->ispv4_pmic_pwr_on;
	}

	ret = devm_gpio_request(&pdev->dev, priv->ispv4_pmic_pwr_on, "pmic_pwr_on");
	if (ret) {
		dev_err(&pdev->dev, "%s, request ispv4_pmic_pwr_on fail", __func__);
		return ret;
	}

	ret = gpio_direction_output(priv->ispv4_pmic_pwr_on, 0);
	if (ret) {
		dev_err(&pdev->dev, "%s, set ispv4_pmic_pwr_on output low fail", __func__);
		return ret;
	}
	//RESET_N
	priv->ispv4_reset_n = of_get_named_gpio(np, "ispv4_reset_n", 0);
	if (priv->ispv4_reset_n < 0) {
		dev_err(&pdev->dev, "%s, get ispv4_reset_n fail", __func__);
		return priv->ispv4_reset_n;
	}

	ret = devm_gpio_request(&pdev->dev, priv->ispv4_reset_n, "reset_n");
	if (ret) {
		dev_err(&pdev->dev, "%s, request ispv4_reset_n fail", __func__);
		return ret;
	}

	ret = gpio_direction_output(priv->ispv4_reset_n, 0);
	if (ret) {
		dev_err(&pdev->dev, "%s, set ispv4_reset_n output high fail", __func__);
		return ret;
	}
	//PMIC err irq
	priv->ispv4_pmic_irq = of_get_named_gpio(np, "ispv4_pmic_irq", 0);
	if (priv->ispv4_pmic_irq < 0) {
		dev_err(&pdev->dev, "%s, get ispv4_pmic_irq fail", __func__);
		return priv->ispv4_pmic_irq;
	}
	priv->ispv4_pmic_irqnum = gpio_to_irq(priv->ispv4_pmic_irq);

	ret = devm_gpio_request(&pdev->dev, priv->ispv4_pmic_irq, "pmic_irq");
	if (ret) {
		dev_err(&pdev->dev, "%s, request ispv4_pmic_irq fail", __func__);
		return ret;
	}

	ret = gpio_direction_input(priv->ispv4_pmic_irq);
	if (ret) {
		dev_err(&pdev->dev, "%s, set ispv4_pmic_irq input fail", __func__);
		return ret;
	}
	//PMIC GD irq
	priv->ispv4_pmic_pwr_gd = of_get_named_gpio(np, "ispv4_pmic_pwr_gd", 0);
	if (priv->ispv4_pmic_pwr_gd < 0) {
		dev_err(&pdev->dev, "%s, get ispv4_pmic_pwr_gd fail", __func__);
		return priv->ispv4_pmic_pwr_gd;
	}
	priv->ispv4_pmic_pwr_gd_irqnum = gpio_to_irq(priv->ispv4_pmic_pwr_gd);

	ret = devm_gpio_request(&pdev->dev, priv->ispv4_pmic_pwr_gd, "pmic_pwr_gd");
	if (ret) {
		dev_err(&pdev->dev, "%s, request ispv4_pmic_pwr_gd fail", __func__);
		return ret;
	}

	ret = gpio_direction_input(priv->ispv4_pmic_pwr_gd);
	if (ret) {
		dev_err(&pdev->dev, "%s, set ispv4_pmic_pwr_gd input fail", __func__);
		return ret;
	}

	return ret;
}

static irqreturn_t ispv4_pmic_err_irq(int irq, void *dev_id)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)dev_id;

	struct platform_device *pdev = priv->pdev;

	dev_info(&pdev->dev, "get pmic err interrupt!\n");

	pr_err("ispv4 pmic err, gpio value %d", gpio_get_value(priv->ispv4_pmic_irq));

	//complete(&priv->pwr_err_com);

	//TODO: Add solution for pmic err irq.

	return IRQ_HANDLED;
}

static irqreturn_t ispv4_pmic_pwr_gd_irq(int irq, void *dev_id)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)dev_id;

	struct platform_device *pdev = priv->pdev;

	dev_info(&pdev->dev, "get pmic pwr good interrupt!\n");

	complete(&priv->pwr_gd_com);

	return IRQ_HANDLED;
}

static int ispv4_assign_ctlirq(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	int ret = 0;

	init_completion(&priv->pwr_gd_com);
	init_completion(&priv->pwr_err_com);

	ret = devm_request_threaded_irq(&pdev->dev,
					priv->ispv4_pmic_irqnum,
					NULL, ispv4_pmic_err_irq,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"ispv4_pmic_err_irq", priv);
	if (ret) {
		dev_err(&pdev->dev,
			"devm_request_threaded_irq for pmic err fail\n");
		return ret;
	}
	//disable_irq(priv->ispv4_pmic_irqnum);

	ret = devm_request_threaded_irq(&pdev->dev,
					priv->ispv4_pmic_pwr_gd_irqnum,
					NULL, ispv4_pmic_pwr_gd_irq,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"ispv4_pmic_pwr_gd", priv);
	if (ret) {
		dev_err(&pdev->dev,
			"devm_request_threaded_irq for pmic pwr gd fail\n");
		return ret;
	}
	//disable_irq(priv->ispv4_pmic_pwr_gd_irqnum);

	return ret;
}
#if 0
static inline void ispv4_enable_pmic_irq(struct ispv4_ctrl_data *priv)
{
	enable_irq(priv->ispv4_pmic_irqnum);
	enable_irq(priv->ispv4_pmic_pwr_gd_irqnum);
}

static inline void ispv4_disable_pmic_irq(struct ispv4_ctrl_data *priv)
{
	disable_irq(priv->ispv4_pmic_irqnum);
	disable_irq(priv->ispv4_pmic_pwr_gd_irqnum);
}
#endif
static inline void ispv4_pmic_pon(struct ispv4_ctrl_data *priv)
{
	gpio_set_value(priv->ispv4_pmic_pwr_on, 1);
}

static inline void ispv4_pmic_poff(struct ispv4_ctrl_data *priv)
{
	gpio_set_value(priv->ispv4_pmic_pwr_on, 0);
}

static inline int ispv4_get_pmic_status(struct ispv4_ctrl_data *priv)
{
	return gpio_get_value(priv->ispv4_pmic_pwr_on);
}

static inline void ispv4_release_reset(struct ispv4_ctrl_data *priv)
{
	gpio_set_value(priv->ispv4_reset_n, 1);
}

static inline void ispv4_de_release_reset(struct ispv4_ctrl_data *priv)
{
	gpio_set_value(priv->ispv4_reset_n, 0);
}

void ispv4_power_off_pmic(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);

	//ispv4_disable_pmic_irq(priv);
	ispv4_pmic_poff(priv);
	udelay(ISPV4_PMIC_PWR_OFF_DELAY);
	ispv4_clk_disable(pdev);
	dev_info(&pdev->dev, "%s", __func__);
}


int ispv4_power_on_pmic(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	int ret = 0;

	if (ispv4_get_pmic_status(priv) == ISPV4_PMIC_ON) {
		dev_err(&pdev->dev, "pmic already power on!");
		return 0;
	}
	priv->pmu_reply = false;

	(void)ispv4_set_lowsp();

	//ispv4_enable_pmic_irq(priv);
	ispv4_pmic_pon(priv);
	ispv4_clk_enable(pdev);

	ret = wait_for_completion_timeout(&priv->pwr_gd_com,
					  msecs_to_jiffies(ISPV4_PMIC_PWR_ON_TIMEOUT));
	if (!ret) {
		dev_err(&pdev->dev, "pmic pwr_on timeout!");
		ret = -ETIMEDOUT;
		goto pmic_pwr_off;
	}

	return 0;

pmic_pwr_off:
	ispv4_power_off_pmic(pdev);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_power_on_pmic);

int ispv4_power_on_chip(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	int ret = 0;

	/* Wait for power stable  */
	usleep_range(ISPV4_PWR_ON_DELAY_US, ISPV4_PWR_ON_DELAY_US + 5);

	ispv4_release_reset(priv);

	/* Wait for pmu irq */
	ret = wait_for_completion_timeout(&priv->pmu_com,
					  msecs_to_jiffies(ISPV4_CHIP_RELEASE_DELAY_MS));
	if (!ret) {
		dev_err(&pdev->dev, "wait for pmu irq timeout!");
		ret = -ETIMEDOUT;
		goto pmu_irq_fail;
	}
	dev_info(&pdev->dev, "%s, ispv4 power up success!", __func__);
	return 0;

pmu_irq_fail:
	ispv4_de_release_reset(priv);

	/* close pmic if failed. */
	ispv4_power_off_pmic(pdev);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_power_on_chip);

int ispv4_power_on_cpu(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	int ret = 0;

	if (ispv4_get_pmic_status(priv) == ISPV4_PMIC_ON) {
		dev_err(&pdev->dev, "pmic already power on!");
		return 0;
	}
	priv->pmu_reply = false;

	(void)ispv4_set_lowsp();

	//ispv4_enable_pmic_irq(priv);
	ispv4_pmic_pon(priv);
	ispv4_clk_enable(pdev);

	ret = wait_for_completion_timeout(&priv->pwr_gd_com,
					  msecs_to_jiffies(ISPV4_PMIC_PWR_ON_TIMEOUT));
	if (!ret) {
		dev_err(&pdev->dev, "pmic pwr_on timeout!");
		ret = -ETIMEDOUT;
		goto pmic_pwr_off;
	}

	/* Wait for power stable  */
	usleep_range(ISPV4_PWR_ON_DELAY_US, ISPV4_PWR_ON_DELAY_US + 5);

	ispv4_release_reset(priv);

	/* Wait for pmu irq */
	ret = wait_for_completion_timeout(&priv->pmu_com,
					  msecs_to_jiffies(ISPV4_CHIP_RELEASE_DELAY_MS));
	if (!ret) {
		dev_err(&pdev->dev, "wait for pmu irq timeout!");
		ret = -ETIMEDOUT;
		goto pmu_irq_fail;
	}
	dev_info(&pdev->dev, "%s, ispv4 power up success!", __func__);
	return 0;

pmu_irq_fail:
	ispv4_de_release_reset(priv);

pmic_pwr_off:
	/* close pmic if failed. */
	//ispv4_disable_pmic_irq(priv);
	ispv4_pmic_poff(priv);
	udelay(ISPV4_PMIC_PWR_OFF_DELAY);
	ispv4_clk_disable(pdev);

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_power_on_cpu);

void ispv4_power_off_cpu(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);

	if (ispv4_get_pmic_status(priv) == ISPV4_PMIC_OFF) {
		dev_err(&pdev->dev, "pmic already power off!");
		return;
	}
	//ispv4_disable_pmic_irq(priv);
	ispv4_de_release_reset(priv);
	ispv4_pmic_poff(priv);
	udelay(ISPV4_PMIC_PWR_OFF_DELAY);
	ispv4_clk_disable(pdev);
}
EXPORT_SYMBOL_GPL(ispv4_power_off_cpu);

int ispv4_get_power_status(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);

	if (ispv4_get_pmic_status(priv) == ISPV4_PMIC_OFF) {
		dev_info(&pdev->dev, "pmic is off!");
		return -1;
	}

	dev_info(&pdev->dev, "pmic is on!");
	return 0;
}

#if !(IS_ENABLED(CONFIG_MIISP_CHIP))
void ispv4_fpga_reset(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	ispv4_release_reset(priv);
	ispv4_de_release_reset(priv);
	mdelay(50);
	ispv4_release_reset(priv);
	mdelay(50);
}
EXPORT_SYMBOL_GPL(ispv4_fpga_reset);
#endif

int ispv4_ddrpll_enable(void)
{
	return ispv4_regops_write(DDRPLL_CON0, DPLL_ENABLE);
}
EXPORT_SYMBOL_GPL(ispv4_ddrpll_enable);

int ispv4_ddrpll_disable(void)
{
	return ispv4_regops_write(DDRPLL_CON0, DPLL_DISABLE);
}
EXPORT_SYMBOL_GPL(ispv4_ddrpll_disable);

int ispv4_ddrpll_2133m(void)
{
	int ret;
	ret = ispv4_regops_write(DDRPLL_CON1, DPLL_2133_CON1_SET);
	pr_info("ispv4 pll_ddr set 0x%lx = 0x%lx\n", DDRPLL_CON1, DPLL_2133_CON1_SET);
	ret |= ispv4_regops_write(DDRPLL_CON2, DPLL_2133_CON2_SET);
	pr_info("ispv4 pll_ddr set 0x%lx = 0x%lx\n", DDRPLL_CON2, DPLL_2133_CON2_SET);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_ddrpll_2133m);

int ispv4_ddrpll_1600m(void)
{
	int ret;
	ret = ispv4_regops_write(DDRPLL_CON1, DPLL_1600_CON1_SET);
	pr_info("ispv4 pll_ddr set 0x%lx = 0x%lx\n", DDRPLL_CON1, DPLL_1600_CON1_SET);
	ret |= ispv4_regops_write(DDRPLL_CON2, DPLL_1600_CON2_SET);
	pr_info("ispv4 pll_ddr set 0x%lx = 0x%lx\n", DDRPLL_CON2, DPLL_1600_CON2_SET);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_ddrpll_1600m);

inline int ispv4_plltest_status(u32 addr)
{
	uint32_t data;
	int ret, i = 0;
	while(i++ < ISPV4_PLLTEST_TIMEOUT) {
		ret = ispv4_regops_read(addr, &data);
		if (ret)
			return ret;
		if ((data & PLL_CON4_MASK) == PLL_CON4_VALUE) {
			return 0;
		}
	}
	return (ktime_get_ns() / 1000);
}
EXPORT_SYMBOL_GPL(ispv4_plltest_status);

int ispv4_config_dpll_gpio(struct platform_device *pdev)
{
	int ret = 0;

	ret = ispv4_regops_clear_and_set(DPLL_TEST_OUT, PLL_TEST_MASK,
					 PLL_TEST_VALUE);
	if (ret) {
		dev_err(&pdev->dev, "DPLL_TEST_OUT set error");
		return ret;
	}
	ret = ispv4_regops_clear_and_set(DPLL_TEST_EN, PLL_TEST_MASK,
					 PLL_TEST_VALUE);
	if (ret) {
		dev_err(&pdev->dev, "DPLL_TEST_EN set error");
		return ret;
	}
	ret = ispv4_regops_clear_and_set(DPLL_TEST_LOCK, PLL_TEST_MASK,
					 PLL_TEST_VALUE);
	if (ret) {
		dev_err(&pdev->dev, "DPLL_TEST_LOCK set error");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_config_dpll_gpio);

int ispv4_config_cpll_gpio(struct platform_device *pdev)
{
	int ret = 0;

	ret = ispv4_regops_clear_and_set(CPLL_TEST_OUT, PLL_TEST_MASK,
					 PLL_TEST_VALUE);
	if (ret) {
		dev_err(&pdev->dev, "CPLL_TEST_OUT set error");
		return ret;
	}
	ret = ispv4_regops_clear_and_set(CPLL_TEST_EN, PLL_TEST_MASK,
					 PLL_TEST_VALUE);
	if (ret) {
		dev_err(&pdev->dev, "CPLL_TEST_EN set error");
		return ret;
	}
	ret = ispv4_regops_clear_and_set(CPLL_TEST_LOCK, PLL_TEST_MASK,
					 PLL_TEST_VALUE);
	if (ret) {
		dev_err(&pdev->dev, "CPLL_TEST_LOCK set error");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_config_cpll_gpio);

static inline int ispv4_clk_pcie_spi_213M(void)
{
	return ispv4_regops_write(AHB_ISP_CON, AHB_ISP_CON_VALUE_D);
}

static inline int ispv4_axi_matrix_533M(void)
{
	return ispv4_regops_write(AXI_MATRIX_CON, AXI_MATRIX_CON_VALUE_D);
}

static inline int ispv4_phy_pcie_88M(void)
{
	return ispv4_regops_write(PHY_PCIE_CON, PHY_PCIE_CON_VALUE_D);
}

static inline int ispv4_pcie_spi_freq(void)
{
	int ret = 0;
	ret = ispv4_clk_pcie_spi_213M();
	if (ret)
		return ret;
	ret = ispv4_axi_matrix_533M();
	if (ret)
		return ret;
	return ispv4_phy_pcie_88M();
}

inline int ispv4_pll_en(void)
{
	return ispv4_regops_write(CPUPLL_CON0, CPUPLL_CON0_ENABLE);
}
EXPORT_SYMBOL_GPL(ispv4_pll_en);

inline int ispv4_pll_disenable(void)
{
	return ispv4_regops_write(CPUPLL_CON0, CPUPLL_CON0_DISABLE);
}
EXPORT_SYMBOL_GPL(ispv4_pll_disenable);

static inline int ispv4_pll_status(void)
{
	uint32_t data;
	int ret, i = 0;
	while(i++ < ISPV4_CPUPLL_POLLING_TIMEOUT) {
		ret = ispv4_regops_read(CPUPLL_CON4, &data);
		if (ret)
			return ret;
		if ((data & CPUPLL_CON4_MASK) == CPUPLL_CON4_VALUE)
			return 0;
	}
	return -1;
}

static inline int ispv4_pll_config(void)
{
	int ret;
	ret = ispv4_pll_en();
	if (ret)
		return ret;
	return ispv4_pll_status();
}

static inline int ispv4_release_cpu(void)
{
	return ispv4_regops_write(CPU_RST_CORE_SW_RST,
				  CPU_RST_CORE_SW_RST_VALUE);
}

static inline int ispv4_cpu_pwr_on(void)
{
	return ispv4_regops_clear_and_set(CPUPLL_CON0,
			CPUPLL_CON0_MASK, CPUPLL_CON0_VALUE);
}

static inline int ispv4_cpu_pwr_status(void)
{
	uint32_t data;
	int ret, i = 0;
	while(i++ < ISPV4_CPU_PWR_POLLING_TIMEOUT) {
		ret = ispv4_regops_read(TOP_CPU_PWR_STATUS, &data);
		if (ret)
			return ret;
		if ((data & TOP_CPU_PWR_STATUS_MASK) == TOP_CPU_PWR_STATUS_VALUE)
			return 0;
	}
	return -1;
}

static inline int ispv4_cpu_pwr_on_config(void)
{
	int ret;
	ret = ispv4_cpu_pwr_on();
	if (ret)
		return ret;
	return ispv4_cpu_pwr_status();
}

static inline int ispv4_busmon_set_timeout(void)
{
	return ispv4_regops_clear_and_set(BUSMON_DEBUG_TIMEOUT,
			BUSMON_DEBUG_TIMEOUT_MASK, BUSMON_DEBUG_TIMEOUT_VALUE);
}

int ispv4_power_on_sequence_preconfig(struct platform_device *pdev)
{
	int ret = 0;

	ret = ispv4_pll_config();
	if (ret) {
		dev_err(&pdev->dev, "ispv4_pll_config fail");
		return ret;
	}

	ret = ispv4_pcie_spi_freq();
	if (ret) {
		dev_err(&pdev->dev, "ispv4_pcie_spi_freq fail");
		return ret;
	}

	(void)ispv4_set_highsp();

	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_power_on_sequence_preconfig);

int ispv4_normal_power_on_sequence(struct platform_device *pdev)
{
	int ret = 0;

	ret = ispv4_power_on_cpu(pdev);
	if (ret) {
		dev_err(&pdev->dev, "ispv4_power_on_cpu fail");
		return ret;
	}

	ret = ispv4_power_on_sequence_preconfig(pdev);
	if (ret) {
		dev_err(&pdev->dev, "ispv4_power_on_sequence preconfig fail");
		return ret;
	}

	return ret;
}

/*No use: Already processed in Boot func*/
int ispv4_normal_power_on_sequence_late(struct platform_device *pdev)
{
	int ret = 0;

	ret = ispv4_busmon_set_timeout();
	if (ret) {
		dev_err(&pdev->dev, "busmon set fail");
		return ret;
	}

	ret = ispv4_cpu_pwr_on_config();
	if (ret) {
		dev_err(&pdev->dev, "cpu pwr on config fail");
		return ret;
	}

	ret = ispv4_release_cpu();
	if (ret) {
		dev_err(&pdev->dev, "ispv4_release_cpu fail");
		return ret;
	}

	return ret;
}

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	struct ispv4_ctrl_data *pdata = NULL;

	pdata = container_of(comp, struct ispv4_ctrl_data, comp_dev);
	priv->v4l2_ctrl.data = pdata;
	priv->v4l2_ctrl.pdev = pdata->pdev;
	priv->v4l2_ctrl.clk_enable = ispv4_clk_enable;
	priv->v4l2_ctrl.clk_disable = ispv4_clk_disable;
	priv->v4l2_ctrl.ispv4_power_on_seq = ispv4_normal_power_on_sequence;
	priv->v4l2_ctrl.ispv4_power_off_seq = ispv4_power_off_cpu;
	priv->v4l2_ctrl.ispv4_get_powerstat = ispv4_get_power_status;
#if !(IS_ENABLED(CONFIG_MIISP_CHIP))
	priv->v4l2_ctrl.ispv4_fpga_reset = ispv4_fpga_reset;
#endif
	priv->v4l2_ctrl.mipi_iso_enable = ispv4_mipi_iso_enable;
	priv->v4l2_ctrl.mipi_iso_disable = ispv4_mipi_iso_disable;
	priv->v4l2_ctrl.enable_wdt_irq = enable_wdt_irq;
	priv->v4l2_ctrl.disable_wdt_irq = disable_wdt_irq;
	priv->v4l2_ctrl.register_wdt_cb = register_wdt_cb;
	priv->v4l2_ctrl.register_sof_cb = register_sof_cb;
	priv->v4l2_ctrl.avalid = true;
	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_ctrl.data = NULL;
	priv->v4l2_ctrl.pdev = NULL;
	priv->v4l2_ctrl.clk_enable = NULL;
	priv->v4l2_ctrl.clk_disable = NULL;
	priv->v4l2_ctrl.avalid = false;
}

static const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind,
};

static int ispv4_clk_config(void *data, u64 val)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	struct platform_device *pdev = priv->pdev;
	int ret = 0;

	if(val)
		ret = ispv4_clk_enable(pdev);
	else
		ret = ispv4_clk_disable(pdev);

	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(dbg_ispv4_clk, NULL, ispv4_clk_config, "%llu/n");

static int ispv4_pmic_config(void *data, u64 val)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	struct platform_device *pdev = priv->pdev;
	int ret = 0;

	if(val) {
		ispv4_pmic_pon(priv);
		dev_info(&pdev->dev, "%s, pmic pon on", __func__);
	}
	else {
		ispv4_pmic_poff(priv);
		dev_info(&pdev->dev, "%s, pmic pon off", __func__);
	}
	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(dbg_ispv4_pmic, NULL, ispv4_pmic_config, "%llu/n");

static int ispv4_release_rst_config(void *data, u64 val)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	struct platform_device *pdev = priv->pdev;
	int ret = 0;

	if(val) {
		ispv4_release_reset(priv);
		dev_info(&pdev->dev, "%s, release reset", __func__);
	}
	else {
		ispv4_de_release_reset(priv);
		dev_info(&pdev->dev, "%s, de release reset", __func__);
	}
	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(dbg_ispv4_release_rst, NULL, ispv4_release_rst_config, "%llu/n");

static int ispv4_pwr_cpu_config(void *data, u64 val)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	struct platform_device *pdev = priv->pdev;
	int ret = 0;

	if(val) {
		ret = ispv4_power_on_cpu(pdev);
		dev_info(&pdev->dev, "%s, power on cpu %s (ret=%d)",
			 __func__, ret ? "fail":"success", ret);
	}
	else {
		ispv4_power_off_cpu(pdev);
		dev_info(&pdev->dev, "%s, power off cpu", __func__);
	}
	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(dbg_ispv4_pwr_cpu, NULL, ispv4_pwr_cpu_config, "%llu/n");

static int ispv4_pwr_seq_config(void *data, u64 val)
{
	struct ispv4_ctrl_data *priv = (struct ispv4_ctrl_data *)data;
	struct platform_device *pdev = priv->pdev;
	int ret = 0;

	if(val) {
		ret = ispv4_normal_power_on_sequence(pdev);
		dev_info(&pdev->dev, "%s, power on v400 whole sequence %s (ret=%d)",
			 __func__, ret ? "fail":"success", ret);
	}
	else {
		ispv4_power_off_cpu(pdev);
		dev_info(&pdev->dev, "%s, power off v400", __func__);
	}
	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(dbg_ispv4_pwr_seq, NULL, ispv4_pwr_seq_config, "%llu/n");

static int ispv4_ctrl_debugfs(struct platform_device *pdev)
{
	struct ispv4_ctrl_data *priv =
		(struct ispv4_ctrl_data *)platform_get_drvdata(pdev);
	int ret = 0;

	priv->dbg_dentry = debugfs_create_dir("ispv4_ctrl", NULL);
	if (IS_ERR_OR_NULL(priv->dbg_dentry)) {
		ret = PTR_ERR(priv->dbg_dentry);
		dev_err(&pdev->dev, "%s, debugfs create dir failed, ret=%d",
				     __func__, ret);
		return ret;
	}

	priv->dbg_clk_dentry = debugfs_create_file("ispv4_clk_enable", 0222,
						   priv->dbg_dentry,
						   priv, &dbg_ispv4_clk);
	if (IS_ERR_OR_NULL(priv->dbg_clk_dentry)) {
		ret = PTR_ERR(priv->dbg_clk_dentry);
		dev_err(&pdev->dev, "%s, debugfs create for clk_enable failed, ret=%d",
				     __func__, ret);
		goto remove_debugdir;
	}

	priv->dbg_pmic_pon_dentry = debugfs_create_file("ispv4_pmic_pon", 0222,
							priv->dbg_dentry,
							priv, &dbg_ispv4_pmic);
	if (IS_ERR_OR_NULL(priv->dbg_pmic_pon_dentry)) {
		ret = PTR_ERR(priv->dbg_pmic_pon_dentry);
		dev_err(&pdev->dev, "%s, debugfs create for pmic_pon failed, ret=%d",
				     __func__, ret);
		goto remove_debugdir;
	}

	priv->dbg_release_rst_dentry = debugfs_create_file("ispv4_release_rst", 0222,
							   priv->dbg_dentry,
							   priv, &dbg_ispv4_release_rst);
	if (IS_ERR_OR_NULL(priv->dbg_release_rst_dentry)) {
		ret = PTR_ERR(priv->dbg_release_rst_dentry);
		dev_err(&pdev->dev, "%s, debugfs create for release rst failed, ret=%d",
				     __func__, ret);
		goto remove_debugdir;
	}

	priv->dbg_pwr_cpu_dentry = debugfs_create_file("ispv4_pwr_cpu", 0222,
							priv->dbg_dentry,
							priv, &dbg_ispv4_pwr_cpu);
	if (IS_ERR_OR_NULL(priv->dbg_pwr_cpu_dentry)) {
		ret = PTR_ERR(priv->dbg_pwr_cpu_dentry);
		dev_err(&pdev->dev, "%s, debugfs create for pwr cpu failed, ret=%d",
				     __func__, ret);
		goto remove_debugdir;
	}

	priv->dbg_pwr_seq_dentry = debugfs_create_file("ispv4_pwr_sequence", 0222,
							priv->dbg_dentry,
							priv, &dbg_ispv4_pwr_seq);
	if (IS_ERR_OR_NULL(priv->dbg_pwr_seq_dentry)) {
		ret = PTR_ERR(priv->dbg_pwr_seq_dentry);
		dev_err(&pdev->dev, "%s, debugfs create for pwr seq failed, ret=%d",
				     __func__, ret);
		goto remove_debugdir;
	}

	return 0;

remove_debugdir:
	debugfs_remove_recursive(priv->dbg_dentry);
	priv->dbg_dentry = NULL;
	priv->dbg_clk_dentry = NULL;
	priv->dbg_pmic_pon_dentry = NULL;
	priv->dbg_release_rst_dentry = NULL;
	priv->dbg_pwr_cpu_dentry = NULL;
	priv->dbg_pwr_seq_dentry = NULL;
	return ret;
}

static int ispv4_ctrl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct ispv4_ctrl_data *priv;

	dev_info(&pdev->dev, "%s", __func__);

	priv = devm_kzalloc(&pdev->dev, sizeof(struct ispv4_ctrl_data), GFP_KERNEL);
	if (priv == NULL) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;

	ret = ispv4_pintctrl_get(pdev);
	if (ret) {
		dev_err(&pdev->dev, "get pinctrl fail %d\n", ret);
		return ret;
	}

	ret = ispv4_clk_get(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s, Get clk failed", __func__);
		return ret;
	}

	ret = ispv4_config_ctlio(pdev);
	if (ret) {
		dev_err(&pdev->dev, "config control io fail %d\n", ret);
		return ret;
	}

	ret = ispv4_assign_ctlirq(pdev);
	if (ret) {
		dev_err(&pdev->dev, "assgin control interrupts fail %d\n", ret);
		return ret;
	}

	ret = ispv4_config_intc_io(pdev);
	if (ret) {
		dev_err(&pdev->dev, "config intc io fail %d\n", ret);
		return ret;
	}

	ret = ispv4_assign_wdt_irq(pdev);
	if (ret) {
		dev_err(&pdev->dev, "assign wdt interrupt fail %d\n", ret);
		return ret;
	}

	ret = ispv4_assign_pmu_irq(pdev);
	if (ret) {
		dev_err(&pdev->dev, "assign pmu interrupt fail %d\n", ret);
		return ret;
	}

	// ret = ispv4_assign_sof_irq(pdev);
	// if (ret) {
	// 	dev_err(&pdev->dev, "assign pmu interrupt fail %d\n", ret);
	// 	return ret;
	// }

	ret = ispv4_ctrl_debugfs(pdev);
	if (ret) {
		dev_err(&pdev->dev, "create debugfs fail %d\n", ret);
		return ret;
	}

	device_initialize(&priv->comp_dev);
	dev_set_name(&priv->comp_dev, "ispv4-ctrl");
	pr_err("comp add %s! priv = %x, comp_name = %s\n", __FUNCTION__, priv,
	       dev_name(&priv->comp_dev));
	ret = component_add(&priv->comp_dev, &comp_ops);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"register ispv4 controller component failed. ret:%d\n",
			ret);
		return ret;
	}

	ispv4_ctrl_pdev = pdev;
	return ret;
}

static int ispv4_ctrl_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct ispv4_ctrl_data *priv = platform_get_drvdata(pdev);

	ispv4_clk_put(pdev);
	debugfs_remove_recursive(priv->dbg_dentry);
	priv->dbg_dentry = NULL;
	priv->dbg_clk_dentry = NULL;
	priv->dbg_pmic_pon_dentry = NULL;
	priv->dbg_release_rst_dentry = NULL;
	priv->dbg_pwr_cpu_dentry = NULL;
	priv->dbg_pwr_seq_dentry = NULL;
	component_del(&priv->comp_dev, &comp_ops);
	ispv4_ctrl_pdev = NULL;
	return ret;
}

static const struct of_device_id ispv4_ctrl_of_match[] = {
	{.compatible = "xiaomi,ispv4-controller",},
	{},
};

static struct platform_driver ispv4_ctrl_drv = {
	.driver = {
		.name = "ispv4-controller",
		.owner = THIS_MODULE,
		.of_match_table = ispv4_ctrl_of_match,
	},
	.probe = ispv4_ctrl_probe,
	.remove = ispv4_ctrl_remove,
};

static int __init ispv4_controller_init(void)
{
	return platform_driver_register(&ispv4_ctrl_drv);
}

static void __exit ispv4_controller_exit(void)
{
	platform_driver_unregister(&ispv4_ctrl_drv);
}

module_init(ispv4_controller_init);
module_exit(ispv4_controller_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ISPV4 controller driver for Xiaomi, Inc.");
