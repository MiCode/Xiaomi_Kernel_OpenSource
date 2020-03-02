// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hw_random.h>
#include <linux/of.h>
#include <linux/arm-smccc.h>
#include <mt-plat/mtk_secure_api.h>

#define PFX			KBUILD_MODNAME ": "
#define MT67XX_RNG_MAGIC	0x74726e67
#define SMC_RET_NUM		4

struct mt67xx_rng_priv {
	struct hwrng rng;
};


static void __rng_sec_read(uint32_t *val)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_GET_RND,
		      MT67XX_RNG_MAGIC, 0, 0, 0, 0, 0, 0, &res);

	val[0] = res.a0;
	val[1] = res.a1;
	val[2] = res.a2;
	val[3] = res.a3;
}

static int mt67xx_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	int i, retval = 0;
	uint32_t val[4] = {0};
	size_t get_rnd_size = sizeof(u32) * SMC_RET_NUM;

	if (!buf) {
		pr_err("%s, buf is NULL\n", __func__);
		return -EFAULT;
	}

	while (max >= get_rnd_size) {
		__rng_sec_read(val);

		for (i = 0; i < SMC_RET_NUM; i++) {
			*(u32 *)buf = val[i];
			buf += sizeof(u32);
		}

		retval += get_rnd_size;
		max -= get_rnd_size;
	}

	return retval;
}

static int mt67xx_rng_probe(struct platform_device *pdev)
{
	int ret;
	struct mt67xx_rng_priv *priv;

	pr_info(PFX "driver registered\n");
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->rng.name = KBUILD_MODNAME;
	priv->rng.read = mt67xx_rng_read;
	priv->rng.priv = (unsigned long)&pdev->dev;
	priv->rng.quality = 900;

	ret = devm_hwrng_register(&pdev->dev, &priv->rng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register rng device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id mt67xx_rng_match[] = {
	{ .compatible = "mediatek,mt67xx-rng", },
	{}
};
MODULE_DEVICE_TABLE(of, mt67xx_rng_match);

static struct platform_driver mt67xx_rng_driver = {
	.probe = mt67xx_rng_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.of_match_table = mt67xx_rng_match,
	},
};

module_platform_driver(mt67xx_rng_driver);

MODULE_DESCRIPTION("Mediatek MT67XX Random Number Generator Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
