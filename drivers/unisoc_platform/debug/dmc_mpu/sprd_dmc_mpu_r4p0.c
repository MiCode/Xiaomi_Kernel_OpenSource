/*
 *copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "dmc_mpu.h"

#define SPRD_DMC_MPU_DUMP_ADDR          0x3010
#define SPRD_DMC_MPU_VIO_ADDR		0x3014
#define SPRD_DMC_MPU_VIO_CMD		0x3018
#define SPRD_MPU_BASE_CFG		0x301c
#define SPRD_MPU_CFG0			0x3020
#define SPRD_MPU_CFG0_ID_MASK_VAL	0x3024
#define SPRD_MPU_CFG0_LOW_RANGE		0x3028
#define SPRD_MPU_CFG0_HIGH_RANGE	0x302c
#define SPRD_MPU_CFG1			0x3030
#define SPRD_MPU_CFG1_ID_MASK_VAL	0x3034
#define SPRD_MPU_CFG1_LOW_RANGE		0x3038
#define SPRD_MPU_CFG1_HIGH_RANGE	0x303c
#define SPRD_MPU_CFG2			0x3040
#define SPRD_MPU_CFG2_ID_MASK_VAL	0x3044
#define SPRD_MPU_CFG2_LOW_RANGE		0x3048
#define SPRD_MPU_CFG2_HIGH_RANGE	0x304c
#define SPRD_MPU_CFG3			0x3050
#define SPRD_MPU_CFG3_ID_MASK_VAL	0x3054
#define SPRD_MPU_CFG3_LOW_RANGE		0x3058
#define SPRD_MPU_CFG3_HIGH_RANGE	0x305c
#define SPRD_MPU_CFG4			0x3060
#define SPRD_MPU_CFG4_ID_MASK_VAL	0x3064
#define SPRD_MPU_CFG4_LOW_RANGE		0x3068
#define SPRD_MPU_CFG4_HIGH_RANGE	0x306c
#define SPRD_MPU_CFG5			0x3070
#define SPRD_MPU_CFG5_ID_MASK_VAL	0x3074
#define SPRD_MPU_CFG5_LOW_RANGE		0x3078
#define SPRD_MPU_CFG5_HIGH_RANGE	0x307c
#define SPRD_MPU_CFG6			0x3080
#define SPRD_MPU_CFG6_ID_MASK_VAL	0x3084
#define SPRD_MPU_CFG6_LOW_RANGE		0x3088
#define SPRD_MPU_CFG6_HIGH_RANGE	0x308c
#define SPRD_MPU_CFG7			0x3090
#define SPRD_MPU_CFG7_ID_MASK_VAL	0x3094
#define SPRD_MPU_CFG7_LOW_RANGE		0x3098
#define SPRD_MPU_CFG7_HIGH_RANGE	0x309c
#define SPRD_MPU_CFG8			0x30a0
#define SPRD_MPU_CFG8_ID_MASK_VAL	0x30a4
#define SPRD_MPU_CFG8_LOW_RANGE		0x30a8
#define SPRD_MPU_CFG8_HIGH_RANGE	0x30ac
#define SPRD_MPU_CFG9			0x30b0
#define SPRD_MPU_CFG9_ID_MASK_VAL	0x30b4
#define SPRD_MPU_CFG9_LOW_RANGE		0x30b8
#define SPRD_MPU_CFG9_HIGH_RANGE	0x30bc
#define SPRD_MPU_CFG10			0x30c0
#define SPRD_MPU_CFG10_ID_MASK_VAL	0x30c4
#define SPRD_MPU_CFG10_LOW_RANGE	0x30c8
#define SPRD_MPU_CFG10_HIGH_RANGE	0x30cc
#define SPRD_MPU_CFG11			0x30d0
#define SPRD_MPU_CFG11_ID_MASK_VAL	0x30d4
#define SPRD_MPU_CFG11_LOW_RANGE	0x30d8
#define SPRD_MPU_CFG11_HIGH_RANGE	0x30dc
#define SPRD_MPU_CFG12			0x30e0
#define SPRD_MPU_CFG12_ID_MASK_VAL	0x30e4
#define SPRD_MPU_CFG12_LOW_RANGE	0x30e8
#define SPRD_MPU_CFG12_HIGH_RANGE	0x30ec
#define SPRD_MPU_CFG13			0x30f0
#define SPRD_MPU_CFG13_ID_MASK_VAL	0x30f4
#define SPRD_MPU_CFG13_LOW_RANGE	0x30f8
#define SPRD_MPU_CFG13_HIGH_RANGE	0x30fc
#define SPRD_MPU_CFG14			0x3100
#define SPRD_MPU_CFG14_ID_MASK_VAL	0x3104
#define SPRD_MPU_CFG14_LOW_RANGE	0x3108
#define SPRD_MPU_CFG14_HIGH_RANGE	0x310c
#define SPRD_MPU_CFG15			0x3110
#define SPRD_MPU_CFG15_ID_MASK_VAL	0x3114
#define SPRD_MPU_CFG15_LOW_RANGE	0x3118
#define SPRD_MPU_CFG15_HIGH_RANGE	0x311c

#define SPRD_APB_MPU_INT_CTRL		0x32F0

#define SPRD_MPU_VIO_INT_CLR_BIT	BIT(9)
#define SPRD_MPU_VIO_INT_EN		BIT(8)
#define SPRD_MPU_EN_BIT			BIT(0)


#define SPRD_CFG(n) \
		(SPRD_MPU_CFG0 + 0x10 * (n))
#define SPRD_CFG_ID_MASK(n)	 \
		(SPRD_MPU_CFG0_ID_MASK_VAL + 0x10 * (n))
#define SPRD_CFG_LOW(n) \
		(SPRD_MPU_CFG0_LOW_RANGE + 0x10 * (n))
#define SPRD_CFG_HIGH(n) \
		(SPRD_MPU_CFG0_HIGH_RANGE + 0x10 * (n))

#define SPRD_MPU_SET_OFFSET		0x1000
#define SPRD_MPU_CLR_OFFSET		0x2000
#define SPRD_MPU_SET(reg)		((reg) + SPRD_MPU_SET_OFFSET)
#define SPRD_MPU_CLR(reg)		((reg) + SPRD_MPU_CLR_OFFSET)

#define SPRD_MPU_VIO_USERID(cmd) \
		(((cmd) & GENMASK(31, 24)) >>  24)
#define SPRD_MPU_VIO_ADDR(cmd)		(((cmd) & GENMASK(22, 21)) >> 21)
#define SPRD_MPU_VIO_WR(cmd)		((cmd) & BIT(20))
#define SPRD_MPU_VIO_PORT(cmd)		(((cmd) & GENMASK(19, 16)) >> 16)
#define SPRD_MPU_VIO_ID(cmd)		((cmd) & GENMASK(15, 0))
#define SPRD_MPU_VIO_REAL_ADDR(h, l, offset) \
		(((h) << 32) + (l) + (offset))

#define SPRD_MPU_CFG_EN(en)		(((en) << 8) & BIT(8))
#define SPRD_MPU_CFG_ID(id)		(((id) << 7) & BIT(7))
#define SPRD_MPU_CFG_INCLUDE(mode)	(((mode) << 6) & BIT(6))
#define SPRD_MPU_CFG_PORT(n)		((n) & GENMASK(3, 0))
#define SPRD_MPU_ID_VAL(m, v) \
		((((m) << 16) & GENMASK(31, 16)) | ((v) & GENMASK(15, 0)))

#define SPRD_MPU_MON_ADDR(v)		((v) >> 6)
#define SPRD_MPU_BASE_OFFSET		0X20000
#define SPRD_MPU_DUMP_FIXED_ADDR	0x00

struct sprd_dmpu_base {
	void __iomem *base;
};

struct sprd_dmpu_device {
	struct sprd_dmpu_core core;
	struct sprd_dmpu_base *addr;
	struct regmap *irq_enable;
	struct regmap *irq_clear;
	u32 enable_offset;
	u32 enable_bit;
	u32 clear_offset;
	u32 clear_bit;
};

#define to_sprd_dmpu_device(x)	container_of(x, struct sprd_dmpu_device, core)

static void sprd_dmc_mpu_enable(struct sprd_dmpu_core *core,
			       u32 pub, bool enable)
{
	struct sprd_dmpu_device *sprd_mpu = to_sprd_dmpu_device(core);

	if (enable)
		writel_relaxed(SPRD_MPU_EN_BIT,
			       sprd_mpu->addr[pub].base +
			       SPRD_MPU_BASE_CFG);
	else
		writel_relaxed(0, sprd_mpu->addr[pub].base +
			       SPRD_MPU_BASE_CFG);
}

static void sprd_dmc_mpu_clr_irq(struct sprd_dmpu_core *core, u32 pub)
{
	struct sprd_dmpu_device *sprd_mpu = to_sprd_dmpu_device(core);

	regmap_update_bits(sprd_mpu->irq_clear, sprd_mpu->clear_offset,
			   sprd_mpu->clear_bit, sprd_mpu->clear_bit);
	regmap_update_bits(sprd_mpu->irq_clear, sprd_mpu->clear_offset,
			   sprd_mpu->clear_bit, 0);
}

static void sprd_dmc_mpu_irq_enable(struct sprd_dmpu_core *core, u32 pub)
{
	struct sprd_dmpu_device *sprd_mpu = to_sprd_dmpu_device(core);

	regmap_update_bits(sprd_mpu->irq_enable, sprd_mpu->enable_offset,
			   sprd_mpu->enable_bit, sprd_mpu->enable_bit);
}

static void sprd_dmc_mpu_irq_disable(struct sprd_dmpu_core *core, u32 pub)
{
	struct sprd_dmpu_device *sprd_mpu = to_sprd_dmpu_device(core);

	regmap_update_bits(sprd_mpu->irq_enable, sprd_mpu->enable_offset,
			   sprd_mpu->enable_bit, 0);
}

static void sprd_dmc_mpu_vio_cmd(struct sprd_dmpu_core *core, u32 pub)
{
	struct sprd_dmpu_device *sprd_mpu = to_sprd_dmpu_device(core);
	struct sprd_dmpu_info *mpu_info = core->mpu_info;
	struct sprd_dmpu_violate *vio = &mpu_info->vio;
	u32 vio_cmd = readl_relaxed(sprd_mpu->addr[pub].base +
				    SPRD_DMC_MPU_VIO_CMD);
	u32 addr_h, addr;

	vio->userid = SPRD_MPU_VIO_USERID(vio_cmd);
	vio->wr = SPRD_MPU_VIO_WR(vio_cmd);
	vio->port = SPRD_MPU_VIO_PORT(vio_cmd);
	vio->id = SPRD_MPU_VIO_ID(vio_cmd);
	addr_h = SPRD_MPU_VIO_ADDR(vio_cmd);
	addr = readl_relaxed(sprd_mpu->addr[pub].base +
				  SPRD_DMC_MPU_VIO_ADDR);
	vio->addr = SPRD_MPU_VIO_REAL_ADDR((u64)addr_h, (u64)addr,
					   (u64)core->ddr_addr_offset);
}

static void
sprd_dmc_mpu_config_channel(struct sprd_dmpu_core *core,
			    u32 pub, u32 n)
{
	struct sprd_dmpu_device *sprd_mpu = to_sprd_dmpu_device(core);
	struct sprd_dmpu_chn_cfg *cfg = core->cfg;
	u64 addr_min = cfg[n].addr_start - core->ddr_addr_offset;
	u64 addr_max = cfg[n].addr_end - core->ddr_addr_offset;
	u32 mpu_cfg = 0;

	if (!cfg[n].en) {
		dev_err(core->dev, "channel%d: disable\n", n);
		return;
	}

	if (addr_min > addr_max) {
		dev_err(core->dev, "channel%d: address config error\n", n);
		return;
	}

	mpu_cfg |= SPRD_MPU_CFG_EN(cfg[n].en);
	mpu_cfg |= SPRD_MPU_CFG_ID(cfg[n].id_type);
	mpu_cfg |= SPRD_MPU_CFG_INCLUDE(cfg[n].include);

	if (!(cfg[n].mode & SPRD_MPU_R_MODE))
		mpu_cfg |= BIT(4);
	else if (cfg[n].mode & SPRD_MPU_W_MODE)
		mpu_cfg |= BIT(5);

	mpu_cfg |= SPRD_MPU_CFG_PORT(cfg[n].port);
	writel_relaxed(mpu_cfg, sprd_mpu->addr[pub].base + SPRD_CFG(n));
	writel_relaxed(SPRD_MPU_ID_VAL(cfg[n].id_mask, cfg[n].userid),
		       sprd_mpu->addr[pub].base + SPRD_CFG_ID_MASK(n));
	writel_relaxed(SPRD_MPU_MON_ADDR(addr_min),
		       sprd_mpu->addr[pub].base + SPRD_CFG_LOW(n));
	writel_relaxed(SPRD_MPU_MON_ADDR(addr_max),
		       sprd_mpu->addr[pub].base + SPRD_CFG_HIGH(n));
}

static void sprd_dmc_mpu_channel_dump_cfg(struct sprd_dmpu_core *core,
					u32 pub)
{
	struct sprd_dmpu_device *sprd_mpu = to_sprd_dmpu_device(core);

	writel_relaxed(SPRD_MPU_DUMP_FIXED_ADDR,
		       sprd_mpu->addr[pub].base + SPRD_DMC_MPU_DUMP_ADDR);
}

static struct sprd_dmpu_ops ops = {
	.enable = sprd_dmc_mpu_enable,
	.clr_irq = sprd_dmc_mpu_clr_irq,
	.vio_cmd = sprd_dmc_mpu_vio_cmd,
	.irq_enable = sprd_dmc_mpu_irq_enable,
	.irq_disable = sprd_dmc_mpu_irq_disable,
	.config = sprd_dmc_mpu_config_channel,
	.dump_cfg = sprd_dmc_mpu_channel_dump_cfg,
};

static int sprd_dmc_mpu_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_dmpu_device *sprd_mpu;
	struct sprd_dmpu_base *addr;
	struct resource *res;
	struct regmap *tregmap;
	bool interleaved;
	u32 args[2];
	int i, ret;

	sprd_mpu = devm_kzalloc(&pdev->dev, sizeof(*sprd_mpu), GFP_KERNEL);
	if (!sprd_mpu)
		return -ENOMEM;

	interleaved = of_property_read_bool(pdev->dev.of_node,
					    "sprd,ddr-interleaved");
	addr = devm_kzalloc(&pdev->dev, sizeof(*addr) << interleaved,
			    GFP_KERNEL);
	if (!addr)
		return -ENOMEM;
	sprd_mpu->addr = addr;

	for (i = 0; i <= interleaved; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(&pdev->dev,
				"dmc mpu get io resource %d failed\n", i);
			return -ENODEV;
		}

		addr[i].base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(addr[i].base))
			return PTR_ERR(addr[i].base);
	}

	tregmap = syscon_regmap_lookup_by_phandle_args(np, "mpu-irq-clr-syscon",
						       2, args);
	if (IS_ERR(tregmap)) {
		dev_err(&pdev->dev, "get the irq_clear node fail\n");
		return PTR_ERR(tregmap);
	}

	sprd_mpu->irq_clear = tregmap;
	sprd_mpu->clear_offset = args[0];
	sprd_mpu->clear_bit = args[1];

	tregmap = syscon_regmap_lookup_by_phandle_args(np, "mpu-irq-en-syscon",
						       2, args);
	if (IS_ERR(tregmap)) {
		dev_err(&pdev->dev, "get the irq_clear node fail\n");
		return PTR_ERR(tregmap);
	}

	sprd_mpu->irq_enable = tregmap;
	sprd_mpu->enable_offset = args[0];
	sprd_mpu->enable_bit = args[1];

	platform_set_drvdata(pdev, &sprd_mpu->core);

	ret = sprd_dmc_mpu_register(pdev, &sprd_mpu->core, &ops);
	if (ret)
		return ret;

	return 0;
}

static int sprd_dmc_mpu_remove(struct platform_device *pdev)
{
	struct sprd_dmpu_device *sprd_mpu = platform_get_drvdata(pdev);

	sprd_dmc_mpu_unregister(&sprd_mpu->core);

	return 0;
}

static const struct of_device_id sprd_dmc_mpu_of_match[] = {
	{ .compatible = "sprd,dmc-mpu", },
	{},
};
MODULE_DEVICE_TABLE(of, sprd_dmc_mpu_of_match);

static struct platform_driver sprd_dmc_mpu_driver = {
	.probe    = sprd_dmc_mpu_probe,
	.remove   = sprd_dmc_mpu_remove,
	.driver = {
		.name = "sprd-dmc-mpu",
		.of_match_table = sprd_dmc_mpu_of_match,
	},
};

module_platform_driver(sprd_dmc_mpu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobs Wang <Bobs.wang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc platform dmc mpu driver");
