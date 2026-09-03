// SPDX-License-Identifier: GPL-2.0

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <trace/hooks/regmap.h>

#include "internal.h"

/* backup for regmap_mmio_context */
struct regmap_mmio_context {
	void __iomem *regs;
	unsigned int val_bytes;
	bool relaxed_mmio;

	bool attached_clk;
	struct clk *clk;

	void (*reg_write)(struct regmap_mmio_context *ctx,
			  unsigned int reg, unsigned int val);
	unsigned int (*reg_read)(struct regmap_mmio_context *ctx,
				unsigned int reg);
};

static int sprd_regmap_mmio_update_bits(void *context, unsigned int reg,
					unsigned int mask, unsigned int val)
{
	struct regmap_mmio_context *ctx = context;
	unsigned int set, clr;
#ifdef CONFIG_SPRD_REGMAP_DEBUG
	int __maybe_unused tmp;
#endif
	int ret;

	if (!IS_ERR(ctx->clk)) {
		ret = clk_enable(ctx->clk);
		if (ret < 0)
			return ret;
	}

	set = val & mask;
	clr = ~set & mask;

	if (set)
		writel(set, ctx->regs + reg + 0x1000);

	if (clr)
		writel(clr, ctx->regs + reg + 0x2000);

#ifdef CONFIG_SPRD_REGMAP_DEBUG
	tmp = readl(ctx->regs + reg);
	if ((tmp & mask) != (val & mask))
		WARN_ONCE(1, "reg:0x%x mask:0x%x val:0x%x not support set/clr\n",
			 reg, mask, val);
#endif

	if (!IS_ERR(ctx->clk))
		clk_disable(ctx->clk);

	return 0;
}

static void sprd_regmap_update(void *data, const struct regmap_config *config,
				struct regmap *map)
{
	if (!config->use_hwlock)
		map->reg_update_bits = sprd_regmap_mmio_update_bits;
}

static int regmap_hook_init(void)
{
	int ret;

	ret = register_trace_android_vh_regmap_update(sprd_regmap_update, NULL);
	if (ret)
		pr_err("register_trace_android_vh_regmap_update fail, ret[%d]\n", ret);

	return ret;
}

static void regmap_hook_exit(void)
{
	pr_info("%s--\n", __func__);
}

postcore_initcall(regmap_hook_init);
module_exit(regmap_hook_exit);

MODULE_DESCRIPTION("unisoc regmap policy");
MODULE_LICENSE("GPL");
