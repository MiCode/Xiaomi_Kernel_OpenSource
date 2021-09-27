// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/of.h>

#define REVID_REVISION1	0x0
#define REVID_REVISION2	0x1
#define REVID_REVISION3	0x2
#define REVID_REVISION4	0x3
#define REVID_TYPE	0x4
#define REVID_SUBTYPE	0x5
#define REVID_STATUS1	0x8
#define REVID_SPARE_0	0x60
#define REVID_TP_REV	0xf1
#define REVID_FAB_ID	0xf2

#define QPNP_REVID_DEV_NAME "qcom,qpnp-revid"

static const char *const pmic_names[] = {
	[0] =	"Unknown PMIC",
	[PM8941_SUBTYPE] = "PM8941",
	[PM8841_SUBTYPE] = "PM8841",
	[PM8019_SUBTYPE] = "PM8019",
	[PM8226_SUBTYPE] = "PM8226",
	[PM8110_SUBTYPE] = "PM8110",
	[PMA8084_SUBTYPE] = "PMA8084",
	[PMI8962_SUBTYPE] = "PMI8962",
	[PMD9635_SUBTYPE] = "PMD9635",
	[PM8994_SUBTYPE] = "PM8994",
	[PMI8994_SUBTYPE] = "PMI8994",
	[PM8916_SUBTYPE] = "PM8916",
	[PM8004_SUBTYPE] = "PM8004",
	[PM8909_SUBTYPE] = "PM8909",
	[PM2433_SUBTYPE] = "PM2433",
	[PMD9655_SUBTYPE] = "PMD9655",
	[PM8950_SUBTYPE] = "PM8950",
	[PMI8950_SUBTYPE] = "PMI8950",
	[PMK8001_SUBTYPE] = "PMK8001",
	[PMI8996_SUBTYPE] = "PMI8996",
	[PM8998_SUBTYPE] = "PM8998",
	[PMI8998_SUBTYPE] = "PMI8998",
	[PM8005_SUBTYPE] = "PM8005",
	[PM8937_SUBTYPE] = "PM8937",
	[PM660L_SUBTYPE] = "PM660L",
	[PM660_SUBTYPE] = "PM660",
	[PMI632_SUBTYPE] = "PMI632",
	[PM2250_SUBTYPE] = "PM2250",
	[PM8150_SUBTYPE] = "PM8150",
	[PM8150B_SUBTYPE] = "PM8150B",
	[PM8150L_SUBTYPE] = "PM8150L",
	[PM6150_SUBTYPE] = "PM6150",
	[PM7250B_SUBTYPE] = "PM7250B",
	[PM6350_SUBTYPE] = "PM6350",
	[PMK8350_SUBTYPE] = "PMK8350",
	[PMR735B_SUBTYPE] = "PMR735B",
	[PM6125_SUBTYPE] = "PM6125",
	[PM8008_SUBTYPE] = "PM8008",
	[PM8010_SUBTYPE] = "PM8010",
	[SMB1355_SUBTYPE] = "SMB1355",
	[SMB1390_SUBTYPE] = "SMB1390",
};

struct revid_chip {
	struct list_head	link;
	struct device_node	*dev_node;
	struct pmic_revid_data	data;
};

static LIST_HEAD(revid_chips);
static DEFINE_MUTEX(revid_chips_lock);

static const struct of_device_id qpnp_revid_match_table[] = {
	{ .compatible = QPNP_REVID_DEV_NAME },
	{}
};

static u8 qpnp_read_byte(struct regmap *regmap, u16 addr)
{
	int rc;
	int val;

	rc = regmap_read(regmap, addr, &val);
	if (rc) {
		pr_err("read failed rc=%d\n", rc);
		return 0;
	}
	return (u8)val;
}

/**
 * get_revid_data - Return the revision information of PMIC
 * @dev_node: Pointer to the revid peripheral of the PMIC for which
 *		revision information is seeked
 *
 * CONTEXT: Should be called in non atomic context
 *
 * RETURNS: pointer to struct pmic_revid_data filled with the information
 *		about the PMIC revision
 */
struct pmic_revid_data *get_revid_data(struct device_node *dev_node)
{
	struct revid_chip *revid_chip;

	if (!dev_node)
		return ERR_PTR(-EINVAL);

	mutex_lock(&revid_chips_lock);
	list_for_each_entry(revid_chip, &revid_chips, link) {
		if (dev_node == revid_chip->dev_node) {
			mutex_unlock(&revid_chips_lock);
			return &revid_chip->data;
		}
	}
	mutex_unlock(&revid_chips_lock);
	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(get_revid_data);

#define PM8941_PERIPHERAL_SUBTYPE	0x01
#define PM8226_PERIPHERAL_SUBTYPE	0x04
#define PMD9655_PERIPHERAL_SUBTYPE	0x0F
#define PMI8950_PERIPHERAL_SUBTYPE	0x11
#define PMI8937_PERIPHERAL_SUBTYPE	0x37
static size_t build_pmic_string(char *buf, size_t n, int sid,
		u8 subtype, u8 rev1, u8 rev2, u8 rev3, u8 rev4)
{
	size_t pos = 0;
	/*
	 * In early versions of PM8941 and PM8226, the major revision number
	 * started incrementing from 0 (eg 0 = v1.0, 1 = v2.0).
	 * Increment the major revision number here if the chip is an early
	 * version of PM8941 or PM8226.
	 */
	if (((int)subtype == PM8941_PERIPHERAL_SUBTYPE
			|| (int)subtype == PM8226_PERIPHERAL_SUBTYPE)
			&& rev4 < 0x02)
		rev4++;

	pos += snprintf(buf + pos, n - pos, "PMIC@SID%d", sid);
	if (subtype >= ARRAY_SIZE(pmic_names) || subtype == 0)
		pos += snprintf(buf + pos, n - pos, ": %s (subtype: 0x%02X)",
				pmic_names[0], subtype);
	else
		pos += snprintf(buf + pos, n - pos, ": %s",
				pmic_names[subtype]);
	pos += snprintf(buf + pos, n - pos, " v%d.%d", rev4, rev3);
	if (rev2 || rev1)
		pos += snprintf(buf + pos, n - pos, ".%d", rev2);
	if (rev1)
		pos += snprintf(buf + pos, n - pos, ".%d", rev1);
	return pos;
}

#define PMIC_PERIPHERAL_TYPE		0x51
#define PMIC_STRING_MAXLENGTH		80
static int qpnp_revid_probe(struct platform_device *pdev)
{
	u8 rev1, rev2, rev3, rev4, pmic_type, pmic_subtype, pmic_status;
	u8 option1, option2, option3, option4, spare0;
	unsigned int base;
	int rc, fab_id, tp_rev;
	char pmic_string[PMIC_STRING_MAXLENGTH] = {'\0'};
	struct revid_chip *revid_chip;
	struct regmap *regmap;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "reg", &base);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Couldn't find reg in node = %s rc = %d\n",
			pdev->dev.of_node->full_name, rc);
		return rc;
	}
	pmic_type = qpnp_read_byte(regmap, base + REVID_TYPE);
	if (pmic_type != PMIC_PERIPHERAL_TYPE) {
		pr_err("Invalid REVID peripheral type: %02X\n", pmic_type);
		return -EINVAL;
	}

	rev1 = qpnp_read_byte(regmap, base + REVID_REVISION1);
	rev2 = qpnp_read_byte(regmap, base + REVID_REVISION2);
	rev3 = qpnp_read_byte(regmap, base + REVID_REVISION3);
	rev4 = qpnp_read_byte(regmap, base + REVID_REVISION4);

	pmic_subtype = qpnp_read_byte(regmap, base + REVID_SUBTYPE);
	if (pmic_subtype != PMD9655_PERIPHERAL_SUBTYPE)
		pmic_status = qpnp_read_byte(regmap, base + REVID_STATUS1);
	else
		pmic_status = 0;

	/* special case for PMI8937 */
	if (pmic_subtype == PMI8950_PERIPHERAL_SUBTYPE) {
		/* read spare register */
		spare0 = qpnp_read_byte(regmap, base + REVID_SPARE_0);
		if (spare0)
			pmic_subtype = PMI8937_PERIPHERAL_SUBTYPE;
	}

	if (of_property_read_bool(pdev->dev.of_node, "qcom,fab-id-valid"))
		fab_id = qpnp_read_byte(regmap, base + REVID_FAB_ID);
	else
		fab_id = -EINVAL;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,tp-rev-valid"))
		tp_rev = qpnp_read_byte(regmap, base + REVID_TP_REV);
	else
		tp_rev = -EINVAL;

	revid_chip = devm_kzalloc(&pdev->dev, sizeof(struct revid_chip),
						GFP_KERNEL);
	if (!revid_chip)
		return -ENOMEM;

	revid_chip->dev_node = pdev->dev.of_node;
	revid_chip->data.rev1 = rev1;
	revid_chip->data.rev2 = rev2;
	revid_chip->data.rev3 = rev3;
	revid_chip->data.rev4 = rev4;
	revid_chip->data.pmic_subtype = pmic_subtype;
	revid_chip->data.pmic_type = pmic_type;
	revid_chip->data.fab_id = fab_id;
	revid_chip->data.tp_rev = tp_rev;

	if (pmic_subtype < ARRAY_SIZE(pmic_names))
		revid_chip->data.pmic_name = pmic_names[pmic_subtype];
	else
		revid_chip->data.pmic_name = pmic_names[0];

	mutex_lock(&revid_chips_lock);
	list_add(&revid_chip->link, &revid_chips);
	mutex_unlock(&revid_chips_lock);

	option1 = pmic_status & 0x3;
	option2 = (pmic_status >> 2) & 0x3;
	option3 = (pmic_status >> 4) & 0x3;
	option4 = (pmic_status >> 6) & 0x3;

	build_pmic_string(pmic_string, PMIC_STRING_MAXLENGTH,
			  to_spmi_device(pdev->dev.parent)->usid,
			pmic_subtype, rev1, rev2, rev3, rev4);
	pr_info("%s options: %d, %d, %d, %d\n",
			pmic_string, option1, option2, option3, option4);
	return 0;
}

static struct platform_driver qpnp_revid_driver = {
	.probe	= qpnp_revid_probe,
	.driver	= {
		.name		= QPNP_REVID_DEV_NAME,
		.of_match_table	= qpnp_revid_match_table,
	},
};

static int __init qpnp_revid_init(void)
{
	return platform_driver_register(&qpnp_revid_driver);
}

static void __exit qpnp_revid_exit(void)
{
	return platform_driver_unregister(&qpnp_revid_driver);
}

subsys_initcall(qpnp_revid_init);
module_exit(qpnp_revid_exit);

MODULE_DESCRIPTION("QPNP REVID DRIVER");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_REVID_DEV_NAME);
