/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spmi.h>

#define REVID_REVISION1	0x0
#define REVID_REVISION2	0x1
#define REVID_REVISION3	0x2
#define REVID_REVISION4	0x3
#define REVID_TYPE	0x4
#define REVID_SUBTYPE	0x5
#define REVID_STATUS1	0x8

#define QPNP_REVID_DEV_NAME "qcom,qpnp-revid"

static const char *const pmic_names[] = {
	"Unknown PMIC",
	"PM8941",
	"PM8841",
	"PM8019",
	"PM8226",
	"PM8110"
};

static struct of_device_id qpnp_revid_match_table[] = {
	{ .compatible = QPNP_REVID_DEV_NAME },
	{}
};

static u8 qpnp_read_byte(struct spmi_device *spmi, u16 addr)
{
	int rc;
	u8 val;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, &val, 1);
	if (rc) {
		pr_err("SPMI read failed rc=%d\n", rc);
		return 0;
	}
	return val;
}

#define PM8941_PERIPHERAL_SUBTYPE	0x01
static size_t build_pmic_string(char *buf, size_t n, int sid,
		u8 subtype, u8 rev1, u8 rev2, u8 rev3, u8 rev4)
{
	size_t pos = 0;
	/*
	 * In early versions of PM8941, the major revision number started
	 * incrementing from 0 (eg 0 = v1.0, 1 = v2.0).
	 * Increment the major revision number here if the chip is an early
	 * version of PM8941.
	 */
	if ((int)subtype == PM8941_PERIPHERAL_SUBTYPE && rev4 < 0x02)
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
static int __devinit qpnp_revid_probe(struct spmi_device *spmi)
{
	u8 rev1, rev2, rev3, rev4, pmic_type, pmic_subtype, pmic_status;
	u8 option1, option2, option3, option4;
	struct resource *resource;
	char pmic_string[PMIC_STRING_MAXLENGTH] = {'\0'};

	resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!resource) {
		pr_err("Unable to get spmi resource for REVID\n");
		return -EINVAL;
	}
	pmic_type = qpnp_read_byte(spmi, resource->start + REVID_TYPE);
	if (pmic_type != PMIC_PERIPHERAL_TYPE) {
		pr_err("Invalid REVID peripheral type: %02X\n", pmic_type);
		return -EINVAL;
	}

	rev1 = qpnp_read_byte(spmi, resource->start + REVID_REVISION1);
	rev2 = qpnp_read_byte(spmi, resource->start + REVID_REVISION2);
	rev3 = qpnp_read_byte(spmi, resource->start + REVID_REVISION3);
	rev4 = qpnp_read_byte(spmi, resource->start + REVID_REVISION4);

	pmic_subtype = qpnp_read_byte(spmi, resource->start + REVID_SUBTYPE);
	pmic_status = qpnp_read_byte(spmi, resource->start + REVID_STATUS1);

	option1 = pmic_status & 0x3;
	option2 = (pmic_status >> 2) & 0x3;
	option3 = (pmic_status >> 4) & 0x3;
	option4 = (pmic_status >> 6) & 0x3;

	build_pmic_string(pmic_string, PMIC_STRING_MAXLENGTH, spmi->sid,
			pmic_subtype, rev1, rev2, rev3, rev4);
	pr_info("%s options: %d, %d, %d, %d\n",
			pmic_string, option1, option2, option3, option4);
	return 0;
}

static struct spmi_driver qpnp_revid_driver = {
	.probe	= qpnp_revid_probe,
	.driver	= {
		.name		= QPNP_REVID_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_revid_match_table,
	},
};

static int __init qpnp_revid_init(void)
{
	return spmi_driver_register(&qpnp_revid_driver);
}

static void __exit qpnp_revid_exit(void)
{
	return spmi_driver_unregister(&qpnp_revid_driver);
}

module_init(qpnp_revid_init);
module_exit(qpnp_revid_exit);

MODULE_DESCRIPTION("QPNP REVID DRIVER");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_REVID_DEV_NAME);
