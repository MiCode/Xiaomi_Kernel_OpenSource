// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define SMC_RET_NUM	4
#define SEC_RND_SIZE	(sizeof(u32) * SMC_RET_NUM)
#define MT67XX_RNG_MAGIC	0x74726e67

#define HWRNG_SMC_FAST_CALL_VAL(func_num) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
			   ARM_SMCCC_OWNER_SIP, (func_num))

#define to_sec_rng(p)	container_of(p, struct sec_rng_priv, rng)

struct sec_rng_priv {
	const char *method;
	uint16_t func_num;
	struct hwrng rng;
};

static bool __sec_get_rnd(struct sec_rng_priv *priv, uint32_t *val)
{
	struct arm_smccc_res res;

	if (!strncmp("smc", priv->method, strlen("smc")))
		arm_smccc_smc(HWRNG_SMC_FAST_CALL_VAL(priv->func_num),
				MT67XX_RNG_MAGIC, 0, 0, 0, 0, 0, 0, &res);
	else if (!strncmp("hvc", priv->method, strlen("hvc")))
		arm_smccc_hvc(HWRNG_SMC_FAST_CALL_VAL(priv->func_num),
				MT67XX_RNG_MAGIC, 0, 0, 0, 0, 0, 0, &res);
	else
		return false;

	if (!res.a0 && !res.a1 && !res.a2 && !res.a3)
		return false;

	val[0] = res.a0;
	val[1] = res.a1;
	val[2] = res.a2;
	val[3] = res.a3;

	return true;
}

static int sec_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	struct sec_rng_priv *priv = to_sec_rng(rng);
	u32 val[4] = {0};
	int retval = 0;
	int i;

	while (max >= SEC_RND_SIZE) {
		if (!__sec_get_rnd(priv, val))
			return retval;

		for (i = 0; i < SMC_RET_NUM; i++) {
			*(u32 *)buf = val[i];
			buf += sizeof(u32);
		}

		retval += SEC_RND_SIZE;
		max -= SEC_RND_SIZE;
	}

	return retval;
}

static int sec_rng_probe(struct platform_device *pdev)
{
	struct sec_rng_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (of_property_read_string(pdev->dev.of_node, "methods",
				&priv->method))
		return -ENXIO;

	if (of_property_read_u16(pdev->dev.of_node, "method-fid",
				&priv->func_num))
		return -ENXIO;

	if (of_property_read_u16(pdev->dev.of_node, "quality",
				&priv->rng.quality))
		return -ENXIO;

	priv->rng.name = pdev->name;
	priv->rng.read = sec_rng_read;
	priv->rng.priv = (unsigned long)&pdev->dev;

	ret = devm_hwrng_register(&pdev->dev, &priv->rng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register rng device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id sec_rng_match[] = {
	{ .compatible = "arm,sec-rng", },
	{}
};
MODULE_DEVICE_TABLE(of, sec_rng_match);

static struct platform_driver sec_rng_driver = {
	.probe = sec_rng_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.of_match_table = sec_rng_match,
	},
};

module_platform_driver(sec_rng_driver);

MODULE_DESCRIPTION("Security Random Number Generator Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
