// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014, 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/qti-regmap-debugfs.h>
#include <linux/spmi.h>
#include <linux/types.h>
#include <linux/regmap.h>
#include <soc/qcom/qcom-spmi-pmic.h>
#include <linux/device.h>

#define PMIC_REV2		0x101
#define PMIC_REV3		0x102
#define PMIC_REV4		0x103
#define PMIC_TYPE		0x104
#define PMIC_SUBTYPE		0x105
#define PMIC_FAB_ID		0x1f2

#define PMIC_TYPE_VALUE		0x51

#define PMIC_REV4_V2		0x02

struct qcom_spmi_dev {
	int num_usids;
	struct qcom_spmi_pmic pmic;
	struct regmap *regmap;
};

static DEFINE_MUTEX(pmic_spmi_revid_lock);

#define N_USIDS(n)		((void *)n)

static const struct of_device_id pmic_spmi_id_table[] = {
	{ .compatible = "qcom,pm660", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm660l", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8004", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8005", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8019", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8028", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8110", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8150", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8150b", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8150c", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8150l", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8226", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8841", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8901", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8909", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8916", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8937", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8941", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8950", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8994", .data = N_USIDS(2) },
	{ .compatible = "qcom,pm8998", .data = N_USIDS(2) },
	{ .compatible = "qcom,pma8084", .data = N_USIDS(2) },
	{ .compatible = "qcom,pmd9635", .data = N_USIDS(2) },
	{ .compatible = "qcom,pmi8950", .data = N_USIDS(2) },
	{ .compatible = "qcom,pmi8962", .data = N_USIDS(2) },
	{ .compatible = "qcom,pmi8994", .data = N_USIDS(2) },
	{ .compatible = "qcom,pmi8998", .data = N_USIDS(2) },
	{ .compatible = "qcom,pmk8002", .data = N_USIDS(2) },
	{ .compatible = "qcom,pmp8074", .data = N_USIDS(2) },
	{ .compatible = "qcom,smb2351", .data = N_USIDS(2) },
	{ .compatible = "qcom,spmi-pmic", .data = N_USIDS(1) },
	{ }
};

/*
 * A PMIC can be represented by multiple SPMI devices, but
 * only the base PMIC device will contain a reference to
 * the revision information.
 *
 * This function takes a pointer to a pmic device and
 * returns a pointer to the base PMIC device.
 *
 * This only supports PMICs with 1 or 2 USIDs.
 */
static struct spmi_device *qcom_pmic_get_base_usid(struct spmi_device *sdev, struct qcom_spmi_dev *ctx)
{
	struct device_node *spmi_bus;
	struct device_node *child;
	int function_parent_usid, ret;
	u32 pmic_addr;

	/*
	 * Quick return if the function device is already in the base
	 * USID. This will always be hit for PMICs with only 1 USID.
	 */
	if (sdev->usid % ctx->num_usids == 0) {
		get_device(&sdev->dev);
		return sdev;
	}

	function_parent_usid = sdev->usid;

	/*
	 * Walk through the list of PMICs until we find the sibling USID.
	 * The goal is to find the first USID which is less than the
	 * number of USIDs in the PMIC array, e.g. for a PMIC with 2 USIDs
	 * where the function device is under USID 3, we want to find the
	 * device for USID 2.
	 */
	spmi_bus = of_get_parent(sdev->dev.of_node);
	sdev = ERR_PTR(-ENODATA);
	for_each_child_of_node(spmi_bus, child) {
		ret = of_property_read_u32_index(child, "reg", 0, &pmic_addr);
		if (ret) {
			of_node_put(child);
			sdev = ERR_PTR(ret);
			break;
		}

		if (pmic_addr == function_parent_usid - (ctx->num_usids - 1)) {
			sdev = spmi_find_device_by_of_node(child);
			if (!sdev) {
				/*
				 * If the base USID for this PMIC hasn't been
				 * registered yet then we need to defer.
				 */
				sdev = ERR_PTR(-EPROBE_DEFER);
			}
			of_node_put(child);
			break;
		}
	}

	of_node_put(spmi_bus);

	return sdev;
}

static int pmic_spmi_get_base_revid(struct spmi_device *sdev, struct qcom_spmi_dev *ctx)
{
	struct qcom_spmi_dev *base_ctx;
	struct spmi_device *base;
	int ret = 0;

	base = qcom_pmic_get_base_usid(sdev, ctx);
	if (IS_ERR(base))
		return PTR_ERR(base);

	/*
	 * Copy revid info from base device if it has probed and is still
	 * bound to its driver.
	 */
	mutex_lock(&pmic_spmi_revid_lock);
	base_ctx = spmi_device_get_drvdata(base);
	if (!base_ctx) {
		ret = -EPROBE_DEFER;
		goto out_unlock;
	}
	memcpy(&ctx->pmic, &base_ctx->pmic, sizeof(ctx->pmic));
out_unlock:
	mutex_unlock(&pmic_spmi_revid_lock);

	put_device(&base->dev);

	return ret;
}

static int pmic_spmi_load_revid(struct regmap *map, struct device *dev,
				 struct qcom_spmi_pmic *pmic)
{
	int ret;

	ret = regmap_read(map, PMIC_TYPE, &pmic->type);
	if (ret < 0)
		return ret;

	if (pmic->type != PMIC_TYPE_VALUE)
		return ret;

	ret = regmap_read(map, PMIC_SUBTYPE, &pmic->subtype);
	if (ret < 0)
		return ret;

	pmic->name = of_match_device(pmic_spmi_id_table, dev)->compatible;

	ret = regmap_read(map, PMIC_REV2, &pmic->rev2);
	if (ret < 0)
		return ret;

	ret = regmap_read(map, PMIC_REV3, &pmic->minor);
	if (ret < 0)
		return ret;

	ret = regmap_read(map, PMIC_REV4, &pmic->major);
	if (ret < 0)
		return ret;

	if (pmic->subtype == PMI8998_SUBTYPE || pmic->subtype == PM660_SUBTYPE) {
		ret = regmap_read(map, PMIC_FAB_ID, &pmic->fab_id);
		if (ret < 0)
			return ret;
	}

	/*
	 * In early versions of PM8941 and PM8226, the major revision number
	 * started incrementing from 0 (eg 0 = v1.0, 1 = v2.0).
	 * Increment the major revision number here if the chip is an early
	 * version of PM8941 or PM8226.
	 */
	if ((pmic->subtype == PM8941_SUBTYPE || pmic->subtype == PM8226_SUBTYPE) &&
	    pmic->major < PMIC_REV4_V2)
		pmic->major++;

	if (pmic->subtype == PM8110_SUBTYPE)
		pmic->minor = pmic->rev2;

	dev_dbg(dev, "%x: %s v%d.%d\n",
		pmic->subtype, pmic->name, pmic->major, pmic->minor);

	return 0;
}

/**
 * qcom_pmic_get() - Get a pointer to the base PMIC device
 *
 * This function takes a struct device for a driver which is a child of a PMIC.
 * And locates the PMIC revision information for it.
 *
 * @dev: the pmic function device
 * @return: the struct qcom_spmi_pmic* pointer associated with the function device
 */
const struct qcom_spmi_pmic *qcom_pmic_get(struct device *dev)
{
	struct spmi_device *sdev;
	struct qcom_spmi_dev *spmi;

	/*
	 * Make sure the device is actually a child of a PMIC
	 */
	if (!of_match_device(pmic_spmi_id_table, dev->parent))
		return ERR_PTR(-EINVAL);

	sdev = to_spmi_device(dev->parent);
	spmi = dev_get_drvdata(&sdev->dev);

	return &spmi->pmic;
}
EXPORT_SYMBOL_GPL(qcom_pmic_get);

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0xffff,
	.fast_io	= true,
};

static const struct regmap_config spmi_regmap_can_sleep_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0xffff,
	.fast_io	= false,
};
static int pmic_spmi_probe(struct spmi_device *sdev)
{
	struct device_node *root = sdev->dev.of_node;
	struct regmap *regmap;
	struct qcom_spmi_dev *ctx;
	int ret;
	const char *pmic_name = NULL;

	if (of_property_read_bool(root, "qcom,can-sleep"))
		regmap = devm_regmap_init_spmi_ext(sdev,
						&spmi_regmap_can_sleep_config);
	else
		regmap = devm_regmap_init_spmi_ext(sdev, &spmi_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ctx = devm_kzalloc(&sdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->num_usids = (uintptr_t)device_get_match_data(&sdev->dev);
	ctx->regmap = regmap;

	/* Only the first slave id for a PMIC contains this information */
	if (sdev->usid % ctx->num_usids == 0) {
		ret = pmic_spmi_load_revid(regmap, &sdev->dev, &ctx->pmic);
		if (ret < 0)
			return ret;
	} else {
		ret = pmic_spmi_get_base_revid(sdev, ctx);
		if (ret)
			return ret;
	}

	mutex_lock(&pmic_spmi_revid_lock);
	spmi_device_set_drvdata(sdev, ctx);
	mutex_unlock(&pmic_spmi_revid_lock);

	devm_regmap_qti_debugfs_register(&sdev->dev, regmap);

	//get pmic-name
	for_each_child_of_node_scoped(root, child) {
		if (of_device_is_compatible(child, "qcom,pmic-ecid")) {
			ret = of_property_read_string(child, "qcom,pmic-name", &pmic_name);
			if (ret >=0 )
				dev_set_name(&sdev->dev, "%s:%s", dev_name(&sdev->dev), pmic_name);
			break;
		}
	}

	pr_info("spmi probe init: %s", dev_name(&sdev->dev));

	return devm_of_platform_populate(&sdev->dev);
}

static void pmic_spmi_remove(struct spmi_device *sdev)
{
	mutex_lock(&pmic_spmi_revid_lock);
	spmi_device_set_drvdata(sdev, NULL);
	mutex_unlock(&pmic_spmi_revid_lock);
}

static void print_regmap(struct regmap *regmap, char *name, unsigned int reg)
{
	unsigned int val;
	regmap_read(regmap, reg, &val);
	pr_info("%s val:0x%x, %s", name, val, (val & 0x80)?"on":"off");
}
static void print_special_ldo(struct device *dev)
{
	struct qcom_spmi_dev *ctx = dev_get_drvdata(dev);

        if (ctx == NULL)
                return ;
	if (strstr(dev_name(dev), "pmh0110_f")) { //PMH0110_F
                //L1F
		print_regmap(ctx->regmap, "L1F", 0xc108);
		//L2F
		print_regmap(ctx->regmap, "L2F", 0xc208);
	} else if (strstr(dev_name(dev), "pmh0101")) { //PMH0101
                //L2B
		print_regmap(ctx->regmap, "L2B", 0xc208);
                //L8B
		print_regmap(ctx->regmap, "L8B", 0xc808);
                //L9B
		print_regmap(ctx->regmap, "L9B", 0xc908);
                //L10B
		print_regmap(ctx->regmap, "L10B", 0xca08);
                //L12B
		print_regmap(ctx->regmap, "L12B", 0xcc08);
		//L13B
		print_regmap(ctx->regmap, "L13B", 0xcd08);
		//L14B
		print_regmap(ctx->regmap, "L14B", 0xce08);
		//L16B
		print_regmap(ctx->regmap, "L16B", 0xd008);
		//L18B
		print_regmap(ctx->regmap, "L18B", 0xd108);
        } else if (strstr(dev_name(dev), "pm8010_m")) { //PM8010_M
                //L1M
		print_regmap(ctx->regmap, "L1M", 0x4008);
		//L2M
		print_regmap(ctx->regmap, "L2M", 0x4108);
		//L3M
		print_regmap(ctx->regmap, "L3M", 0x4208);
		//L4M
		print_regmap(ctx->regmap, "L4M", 0x4308);
		//L5M
		print_regmap(ctx->regmap, "L5M", 0x4408);
		//L6M
		print_regmap(ctx->regmap, "L6M", 0x4508);
		//L7M
		print_regmap(ctx->regmap, "L7M", 0x4608);
        }
}
static int pmic_spmi_suspend(struct device *dev)
{
	print_special_ldo(dev);
	return 0;
}
static int pmic_spmi_resume(struct device *dev)
{
	print_special_ldo(dev);
	return 0;
}
static DEFINE_SIMPLE_DEV_PM_OPS(pmic_spmi_pm_ops, pmic_spmi_suspend, pmic_spmi_resume);

MODULE_DEVICE_TABLE(of, pmic_spmi_id_table);

static struct spmi_driver pmic_spmi_driver = {
	.probe = pmic_spmi_probe,
	.remove = pmic_spmi_remove,
	.driver = {
		.name = "pmic-spmi",
		.of_match_table = pmic_spmi_id_table,
		.pm = pm_sleep_ptr(&pmic_spmi_pm_ops),
	},
};
module_spmi_driver(pmic_spmi_driver);

MODULE_DESCRIPTION("Qualcomm SPMI PMIC driver");
MODULE_ALIAS("spmi:spmi-pmic");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Josh Cartwright <joshc@codeaurora.org>");
MODULE_AUTHOR("Stanimir Varbanov <svarbanov@mm-sol.com>");
