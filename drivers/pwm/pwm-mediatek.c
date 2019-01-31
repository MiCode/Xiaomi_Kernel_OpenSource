/*******************************************************************************
 *  PWM Drvier
 *
 * Mediatek Pulse Width Modulator driver
 *
 * Copyright (C) 2015 John Crispin <blogic at openwrt.org>
 * Copyright (C) 2017 Zhi Mao <zhi.mao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public Licence,
 * version 2, as publish by the Free Software Foundation.
 *
 * This program is distributed and in hope it will be useful, but WITHOUT
 * ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/types.h>

#define PWM_EN_REG          0x0000
#define PWMCON              0x00
#define PWMGDUR             0x0c
#define PWMWAVENUM          0x28
#define PWMDWIDTH           0x2c
#define PWMTHRES            0x30

#define PWM_CLK_DIV_MAX     7
#define PWM_NUM_MAX         8

#define PWM_CLK_NAME_MAIN   "main"
#define PWM_CLK_NAME_TOP    "top"

static const char * const mtk_pwm_clk_name[PWM_NUM_MAX] = {
	"pwm1", "pwm2", "pwm3", "pwm4", "pwm5", "pwm6", "pwm7", "pwm8"
};

struct mtk_com_pwm_data {
	const unsigned long *pwm_register;
	unsigned int pwm_nums;
};

/**
 * struct mtk_pwm_chip - struct representing pwm chip
 *
 * @mmio_base: base address of pwm chip
 * @chip: linux pwm chip representation
 */
struct mtk_pwm_chip {
	struct device *dev;
	void __iomem *mmio_base;
	struct pwm_chip chip;
	struct clk *clk_top;
	struct clk *clk_main;
	struct clk *clk_pwm[PWM_NUM_MAX];
	const struct mtk_com_pwm_data *data;
};

/*==========================================*/
static const unsigned long mtk_pwm_com_register[] = {
	0x0010, 0x0050, 0x0090, 0x00d0, 0x0110, 0x0150, 0x0190, 0x0220
};
/*==========================================*/

/*==========================================*/
static const struct mtk_com_pwm_data mt2712_pwm_data = {
	.pwm_register = mtk_pwm_com_register,
	.pwm_nums = 8,
};

static const struct mtk_com_pwm_data mt7622_pwm_data = {
	.pwm_register = mtk_pwm_com_register,
	.pwm_nums = 6,
};

static const struct mtk_com_pwm_data mt7623_pwm_data = {
	.pwm_register = mtk_pwm_com_register,
	.pwm_nums = 5,
};

static const struct mtk_com_pwm_data mt8167_pwm_data = {
	.pwm_register = mtk_pwm_com_register,
	.pwm_nums = 3,
};
/*==========================================*/

static const struct of_device_id mtk_pwm_of_match[] = {
	{.compatible = "mediatek,mt2712-pwm", .data = &mt2712_pwm_data},
	{.compatible = "mediatek,mt7622-pwm", .data = &mt7622_pwm_data},
	{.compatible = "mediatek,mt7623-pwm", .data = &mt7623_pwm_data},
	{.compatible = "mediatek,mt8167-pwm", .data = &mt8167_pwm_data},
	{},
};

static inline struct mtk_pwm_chip *to_mtk_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct mtk_pwm_chip, chip);
}

static int mtk_pwm_clk_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	int ret = 0;

	ret = clk_prepare_enable(pc->clk_top);
	if (ret < 0)
		return ret;

	ret = clk_prepare_enable(pc->clk_main);
	if (ret < 0) {
		clk_disable_unprepare(pc->clk_top);
		return ret;
	}

	ret = clk_prepare_enable(pc->clk_pwm[pwm->hwpwm]);
	if (ret < 0) {
		clk_disable_unprepare(pc->clk_main);
		clk_disable_unprepare(pc->clk_top);
		return ret;
	}

	return ret;
}

static void mtk_pwm_clk_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);

	clk_disable_unprepare(pc->clk_pwm[pwm->hwpwm]);
	clk_disable_unprepare(pc->clk_main);
	clk_disable_unprepare(pc->clk_top);
}

static inline u32 mtk_pwm_readl(struct mtk_pwm_chip *chip,
				u32 pwm_no, unsigned long offset)
{
	void __iomem *reg = chip->mmio_base + chip->data->pwm_register[pwm_no] + offset;

	return readl(reg);
}

static inline void mtk_pwm_writel(struct mtk_pwm_chip *chip,
				u32 pwm_no, unsigned long offset,
				unsigned long val)
{
	void __iomem *reg = chip->mmio_base + chip->data->pwm_register[pwm_no] + offset;

	writel(val, reg);
}

static int mtk_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			int duty_ns, int period_ns)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	u32 value;
	u32 resolution = 100 / 4;
	u32 clkdiv = 0;
	u32 clksrc_rate;

	u32 data_width, thresh;


	mtk_pwm_clk_enable(chip, pwm);

	clksrc_rate = clk_get_rate(pc->clk_pwm[pwm->hwpwm]);
	resolution = 1000000000 / clksrc_rate;

	while (period_ns / resolution  > 8191) {
		clkdiv++;
		resolution *= 2;
	}

	if (clkdiv > PWM_CLK_DIV_MAX) {
		dev_err(pc->dev, "period %d not supported\n", period_ns);
		return -EINVAL;
	}

	data_width = period_ns / resolution;
	thresh = duty_ns / resolution;

	value = mtk_pwm_readl(pc, pwm->hwpwm, PWMCON);
	value = value | BIT(15) | clkdiv;
	mtk_pwm_writel(pc, pwm->hwpwm, PWMCON, value);

	mtk_pwm_writel(pc, pwm->hwpwm, PWMDWIDTH, data_width);
	mtk_pwm_writel(pc, pwm->hwpwm, PWMTHRES, thresh);

	mtk_pwm_clk_disable(chip, pwm);

	return 0;
}

static int mtk_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	u32 val;

	mtk_pwm_clk_enable(chip, pwm);

	val = readl(pc->mmio_base + PWM_EN_REG);
	val |= BIT(pwm->hwpwm);
	writel(val, pc->mmio_base + PWM_EN_REG);

	return 0;
}

static void mtk_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm_chip *pc = to_mtk_pwm_chip(chip);
	u32 val;

	val = readl(pc->mmio_base + PWM_EN_REG);
	val &= ~BIT(pwm->hwpwm);
	writel(val, pc->mmio_base + PWM_EN_REG);

	mtk_pwm_clk_disable(chip, pwm);
}

static const struct pwm_ops mtk_pwm_ops = {
	.config = mtk_pwm_config,
	.enable = mtk_pwm_enable,
	.disable = mtk_pwm_disable,
	.owner = THIS_MODULE,
};

static int mtk_pwm_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct mtk_pwm_chip *pc;
	struct resource *r;
	int ret;
	int i;

	id = of_match_device(mtk_pwm_of_match, &pdev->dev);
	if (!id)
		return -EINVAL;

	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	pc->data = id->data;
	pc->dev = &pdev->dev;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pc->mmio_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(pc->mmio_base))
		return PTR_ERR(pc->mmio_base);

	for (i = 0; i < pc->data->pwm_nums; i++) {
		pc->clk_pwm[i] = devm_clk_get(&pdev->dev, mtk_pwm_clk_name[i]);
		if (IS_ERR(pc->clk_pwm[i])) {
			dev_err(&pdev->dev, "[PWM] clock: %s fail\n", mtk_pwm_clk_name[i]);
			return PTR_ERR(pc->clk_pwm[i]);
		}
	}

	pc->clk_main = devm_clk_get(&pdev->dev, PWM_CLK_NAME_MAIN);
	if (IS_ERR(pc->clk_main))
		return PTR_ERR(pc->clk_main);

	pc->clk_top = devm_clk_get(&pdev->dev, PWM_CLK_NAME_TOP);
	if (IS_ERR(pc->clk_top))
		return PTR_ERR(pc->clk_top);

	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &mtk_pwm_ops;
	pc->chip.npwm = pc->data->pwm_nums;

	platform_set_drvdata(pdev, pc);

	ret = pwmchip_add(&pc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_pwm_remove(struct platform_device *pdev)
{
	struct mtk_pwm_chip *pc = platform_get_drvdata(pdev);

	return pwmchip_remove(&pc->chip);
}

static struct platform_driver mtk_pwm_driver = {
	.driver = {
		.name = "mtk-pwm",
		.owner = THIS_MODULE,
		.of_match_table = mtk_pwm_of_match,
	},
	.probe = mtk_pwm_probe,
	.remove = mtk_pwm_remove,
};
MODULE_DEVICE_TABLE(of, mtk_pwm_of_match);

module_platform_driver(mtk_pwm_driver);

MODULE_AUTHOR("Zhi Mao <zhi.mao@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC PWM driver");
MODULE_LICENSE("GPL v2");


