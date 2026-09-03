// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input/matrix_keypad.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>

#define SPRD_KPD_CTRL			0x0
#define SPRD_KPD_INT_EN			0x4
#define SPRD_KPD_INT_RAW_STATUS		0x8
#define SPRD_KPD_INT_MASK_STATUS	0xc
#define SPRD_KPD_INT_CLR		0x10
#define SPRD_KPD_POLARITY		0x18
#define SPRD_KPD_DEBOUNCE_CNT		0x1c
#define SPRD_KPD_LONG_KEY_CNT		0x20
#define SPRD_KPD_SLEEP_CNT		0x24
#define SPRD_KPD_CLK_DIV_CNT		0x28
#define SPRD_KPD_KEY_STATUS		0x2c
#define SPRD_KPD_SLEEP_STATUS		0x30
#define SPRD_KPD_DEBUG_STATUS1		0x34
#define SPRD_KPD_DEBUG_STATUS2		0x38

#define SPRD_KPD_EN			BIT(0)
#define SPRD_KPD_SLEEP_EN		BIT(1)
#define SPRD_KPD_LONG_KEY_EN		BIT(2)

#define SPRD_KPD_ROWS_MSK		GENMASK(23, 16)
#define SPRD_KPD_COLS_MSK		GENMASK(15, 8)

#define SPRD_KPD_INT_ALL		GENMASK(11, 0)
#define SPRD_KPD_INT_DOWNUP		GENMASK(7, 0)
#define SPRD_KPD_INT_LONG		GENMASK(11, 8)

#define SPRD_KPD_ROW_POLARITY		GENMASK(7, 0)
#define SPRD_KPD_COL_POLARITY		GENMASK(15, 8)

#define SPRD_KPD_PRESS_INTX(X, V) \
	(((V) >> (X)) & GENMASK(0, 0))
#define SPRD_KPD_RELEASE_INTX(X, V) \
	(((V) >> ((X) + 4)) & GENMASK(0, 0))
#define SPRD_KPD_INTX_COL(X, V) \
	(((V) >> ((X) << 3)) & GENMASK(2, 0))
#define SPRD_KPD_INTX_ROW(X, V) \
	(((V) >> (((X) << 3) + 4)) & GENMASK(2, 0))
#define SPRD_KPD_INTX_DOWN(X, V) \
	(((V) >> (((X) << 3) + 7)) & GENMASK(0, 0))

#define SPRD_KPD_RTC_HZ			32768
#define SPRD_DEF_LONG_KEY_MS		1000
#define SPRD_DEF_DIV_CNT		1
#define SPRD_KPD_INT_CNT		4
#define SPRD_KPD_ROWS_MAX		8
#define SPRD_KPD_COLS_MAX		8
#define SPRD_KPD_ROWS_SHIFT		16
#define SPRD_KPD_COLS_SHIFT		8

#define SPRD_CAP_WAKEUP			BIT(0)
#define SPRD_CAP_LONG_KEY		BIT(1)
#define SPRD_CAP_REPEAT			BIT(2)

struct sprd_keypad_data {
	u32 rows_en; /* enabled rows bits */
	u32 cols_en; /* enabled cols bits */
	u32 num_rows;
	u32 num_cols;
	u32 capabilities;
	u32 debounce_ms;
	void __iomem *base;
	struct input_dev *input_dev;
	struct clk *enable;
	struct clk *rtc;
};

static int sprd_keypad_enable(struct sprd_keypad_data *data)
{
	struct device *dev = data->input_dev->dev.parent;
	int ret;

	ret = clk_prepare_enable(data->rtc);
	if (ret) {
		dev_err(dev, "enable rtc failed.\n");
		return ret;
	}

	ret = clk_prepare_enable(data->enable);
	if (ret) {
		dev_err(dev, "enable keypad failed.\n");
		clk_disable_unprepare(data->rtc);
		return ret;
	}

	return 0;
}

static void sprd_keypad_disable(struct sprd_keypad_data *data)
{
	clk_disable_unprepare(data->enable);
	clk_disable_unprepare(data->rtc);
}

static irqreturn_t sprd_keypad_handler(int irq, void *id)
{
	struct platform_device *pdev = id;
	struct device *dev = &pdev->dev;
	struct sprd_keypad_data *data = platform_get_drvdata(pdev);
	u32 int_status = readl_relaxed(data->base + SPRD_KPD_INT_MASK_STATUS);
	u32 key_status = readl_relaxed(data->base + SPRD_KPD_KEY_STATUS);
	unsigned short *keycodes = data->input_dev->keycode;
	u32 row_shift = get_count_order(data->num_cols);
	unsigned short key;
	int col, row;
	u32 i;

	writel_relaxed(SPRD_KPD_INT_ALL, data->base + SPRD_KPD_INT_CLR);

	for (i = 0; i < SPRD_KPD_INT_CNT; i++) {
		if (SPRD_KPD_PRESS_INTX(i, int_status)) {
			col = SPRD_KPD_INTX_COL(i, key_status);
			row = SPRD_KPD_INTX_ROW(i, key_status);
			key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
			input_report_key(data->input_dev, key, 1);
			input_sync(data->input_dev);
			dev_dbg(dev, "%dD\n", key);
		}
		if (SPRD_KPD_RELEASE_INTX(i, int_status)) {
			col = SPRD_KPD_INTX_COL(i, key_status);
			row = SPRD_KPD_INTX_ROW(i, key_status);
			key = keycodes[MATRIX_SCAN_CODE(row, col, row_shift)];
			input_report_key(data->input_dev, key, 0);
			input_sync(data->input_dev);
			dev_dbg(dev, "%dU\n", key);
		}
	}

	return IRQ_HANDLED;
}

static u32 sprd_keypad_time_to_counter(u32 array_size, u32 time_ms)
{
	u32 value;

	/*
	 * y(ms) = (x + 1) * array_size
	 *		/ (32.768 / (clk_div_num + 1))
	 * y means time in ms
	 * x means counter
	 * array_size equal to rows * columns
	 * clk_div_num is devider to keypad source clock
	 **/
	value = SPRD_KPD_RTC_HZ * time_ms;
	value = value / (1000 * array_size *
			(SPRD_DEF_DIV_CNT + 1));
	if (value >= 1)
		value -= 1;

	return value;
}

static int sprd_keypad_hw_init(struct sprd_keypad_data *data)
{
	u32 value;

	writel_relaxed(SPRD_KPD_INT_ALL, data->base + SPRD_KPD_INT_CLR);
	writel_relaxed(SPRD_KPD_ROW_POLARITY | SPRD_KPD_COL_POLARITY,
			data->base + SPRD_KPD_POLARITY);
	writel_relaxed(SPRD_DEF_DIV_CNT, data->base + SPRD_KPD_CLK_DIV_CNT);

	value = sprd_keypad_time_to_counter(data->num_rows * data->num_cols,
						SPRD_DEF_LONG_KEY_MS);
	writel_relaxed(value, data->base + SPRD_KPD_LONG_KEY_CNT);

	value = sprd_keypad_time_to_counter(data->num_rows * data->num_cols,
						data->debounce_ms);
	writel_relaxed(value, data->base + SPRD_KPD_DEBOUNCE_CNT);

	value = SPRD_KPD_INT_DOWNUP;
	if (data->capabilities & SPRD_CAP_LONG_KEY)
		value |= SPRD_KPD_INT_LONG;
	writel_relaxed(value, data->base + SPRD_KPD_INT_EN);

	value = SPRD_KPD_RTC_HZ - 1;
	writel_relaxed(value, data->base + SPRD_KPD_SLEEP_CNT);

	/* set enabled rows and columns */
	value = (((data->rows_en << SPRD_KPD_ROWS_SHIFT)
		| (data->cols_en << SPRD_KPD_COLS_SHIFT))
		& (SPRD_KPD_ROWS_MSK | SPRD_KPD_COLS_MSK))
		| SPRD_KPD_EN | SPRD_KPD_SLEEP_EN;
	if (data->capabilities & SPRD_CAP_LONG_KEY)
		value |= SPRD_KPD_LONG_KEY_EN;
	writel_relaxed(value, data->base + SPRD_KPD_CTRL);

	return 0;
}

static int __maybe_unused sprd_keypad_suspend(struct device *dev)
{
	struct sprd_keypad_data *data = dev_get_drvdata(dev);

	if (!device_may_wakeup(dev))
		sprd_keypad_disable(data);

	return 0;
}

static int __maybe_unused sprd_keypad_resume(struct device *dev)
{
	struct sprd_keypad_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (!device_may_wakeup(dev)) {
		ret = sprd_keypad_enable(data);
		if (ret)
			return ret;
		ret = sprd_keypad_hw_init(data);
	}

	return ret;
}

static SIMPLE_DEV_PM_OPS(sprd_keypad_pm_ops,
			sprd_keypad_suspend, sprd_keypad_resume);

static int sprd_keypad_parse_dt(struct device *dev)
{
	struct sprd_keypad_data *data = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	int ret;

	ret = matrix_keypad_parse_properties(dev, &data->num_rows, &data->num_cols);
	if (ret)
		return ret;

	if (data->num_rows > SPRD_KPD_ROWS_MAX
		|| data->num_cols > SPRD_KPD_COLS_MAX) {
		dev_err(dev, "invalid num_rows or num_cols\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "debounce-interval", &data->debounce_ms);
	if (ret) {
		data->debounce_ms = 5;
		dev_warn(dev, "parse debounce-interval failed.\n");
	}

	if (of_get_property(np, "linux,repeat", NULL))
		data->capabilities |= SPRD_CAP_REPEAT;
	if (of_get_property(np, "sprd,support_long_key", NULL))
		data->capabilities |= SPRD_CAP_LONG_KEY;
	if (of_get_property(np, "wakeup-source", NULL))
		data->capabilities |= SPRD_CAP_WAKEUP;

	data->enable = devm_clk_get(dev, "enable");
	if (IS_ERR(data->enable)) {
		if (PTR_ERR(data->enable) != -EPROBE_DEFER)
			dev_err(dev, "get enable clk failed.\n");
		return PTR_ERR(data->enable);
	}

	data->rtc = devm_clk_get(dev, "rtc");
	if (IS_ERR(data->rtc)) {
		if (PTR_ERR(data->enable) != -EPROBE_DEFER)
			dev_err(dev, "get rtc clk failed.\n");
		return PTR_ERR(data->rtc);
	}

	return 0;
}

static int sprd_keypad_probe(struct platform_device *pdev)
{
	struct sprd_keypad_data *data;
	struct resource *res;
	int ret, irq, i, j, row_shift;
	unsigned long rows, cols;
	unsigned short *keycodes;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->base)) {
		dev_err(&pdev->dev, "ioremap resource failed.\n");
		ret =  PTR_ERR(data->base);
		goto err_free;
	}

	platform_set_drvdata(pdev, data);
	ret = sprd_keypad_parse_dt(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "keypad parse dts failed.\n");
		goto err_free;
	}

	data->input_dev = devm_input_allocate_device(&pdev->dev);
	if (IS_ERR(data->input_dev)) {
		dev_err(&pdev->dev, "alloc input dev failed.\n");
		ret =  PTR_ERR(data->input_dev);
		goto err_free;
	}

	data->input_dev->name = "sprd-keypad";
	data->input_dev->phys = "sprd-key/input0";

	ret = matrix_keypad_build_keymap(NULL, NULL, data->num_rows,
					 data->num_cols, NULL, data->input_dev);
	if (ret) {
		dev_err(&pdev->dev, "keypad build keymap failed.\n");
		goto err_free;
	}

	rows = cols = 0;
	row_shift = get_count_order(data->num_cols);
	keycodes = data->input_dev->keycode;
	for (i = 0; i < data->num_rows; i++) {
		for (j = 0; j < data->num_cols; j++) {
			if (!!keycodes[MATRIX_SCAN_CODE(i, j, row_shift)]) {
				set_bit(i, &rows);
				set_bit(j, &cols);
			}
		}
	}
	data->rows_en = rows;
	data->cols_en = cols;

	if (data->capabilities & SPRD_CAP_REPEAT)
		set_bit(EV_REP, data->input_dev->evbit);

	input_set_drvdata(data->input_dev, data);

	ret = sprd_keypad_enable(data);
	if (ret) {
		dev_err(&pdev->dev, "keypad enable failed.\n");
		goto err_free;
	}

	ret = sprd_keypad_hw_init(data);
	if (ret) {
		dev_err(&pdev->dev, "keypad hw init failed.\n");
		goto clk_free;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "platform get irq failed.\n");
		goto clk_free;
	}

	ret = devm_request_irq(&pdev->dev, irq, sprd_keypad_handler,
				IRQF_NO_SUSPEND, dev_name(&pdev->dev), pdev);
	if (ret) {
		dev_err(&pdev->dev, "request irq failed.\n");
		goto clk_free;
	}

	ret = input_register_device(data->input_dev);
	if (ret) {
		dev_err(&pdev->dev, "register input dev failed\n");
		goto clk_free;
	}

	if (data->capabilities & SPRD_CAP_WAKEUP)
		device_init_wakeup(&pdev->dev, true);

	return 0;

clk_free:
	sprd_keypad_disable(data);
err_free:
	devm_kfree(&pdev->dev, data);
	return ret;
}

static int sprd_keypad_remove(struct platform_device *pdev)
{
	struct sprd_keypad_data *data = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	if (data->capabilities & SPRD_CAP_WAKEUP)
		device_init_wakeup(&pdev->dev, false);

	input_unregister_device(data->input_dev);
	devm_free_irq(&pdev->dev, irq, pdev);
	sprd_keypad_disable(data);

	return 0;
}

static const struct of_device_id sprd_keypad_match[] = {
	{ .compatible = "sprd,sc9860-keypad", },
	{},
};

static struct platform_driver sprd_keypad_driver = {
	.driver = {
		.name = "sprd-keypad",
		.owner = THIS_MODULE,
		.of_match_table = sprd_keypad_match,
		.pm = &sprd_keypad_pm_ops,
	},
	.probe = sprd_keypad_probe,
	.remove = sprd_keypad_remove,
};

module_platform_driver(sprd_keypad_driver);

MODULE_DESCRIPTION("Spreadtrum KPD Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Neo Hou <neo.hou@unisoc.com>");
