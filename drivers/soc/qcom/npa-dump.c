/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include <soc/qcom/rpm-smd.h>

#define RESOURCE_NAME_MAX	64
#define MISC_RESOURCE		0x6373696D /* = misc */

struct npa_ver_header {
	u32 ver;
	u32 timestamp[2];
	u32 size;
};

struct npa_res {
	u8 units[4];
	u32 active_max;
	u32 active_state;
	u32 request_state;
};

struct npa_res_client {
	char name[4];
	char type[4];
	u32 request_state;
};

static u8 rpm_data[8];

static struct msm_rpm_kvp kvp = {
	.key = 0x706D7564, /*  = dump */
	.data = &rpm_data[0],
	.length = sizeof(rpm_data),
};

static int npa_file_read(struct seq_file *m, void *unused)
{
	int ret;
	struct npa_ver_header ver;
	struct npa_res res;
	struct npa_res_client client;
	u8 *pos = m->private;
	char name[RESOURCE_NAME_MAX];
	char *p;
	int i;
	u64 ts;

	if (!pos || !*pos)
		return -EINVAL;

	ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
					MISC_RESOURCE, 0, &kvp, 1);
	if (ret)
		return ret;

	/**
	 * | Version (uint32) | Timestamp (uint64) | NPA Dump Size (uint32) |
	 * | Resource Name (char[variable]) | Unit (char[4]) |
	 *      Active Max (uint32) | Active State (uint32) |
	 *      Request State (uint32) |
	 *  | Client Name (char[4]) | Type (char[4]) | Request State (uint32) |
	 *  | Client Name (char[4]) | Type (char[4]) | Request State (uint32) |
	 *   ...
	 *  | Client Name (char[4]) | Type (char[4]) | Request State (uint32) |
	 *  | NULL (uint32) |
	 */

	/* Header */
	memcpy_fromio(&ver, pos, sizeof(ver));
	ts = ver.timestamp[0] | ver.timestamp[1] << sizeof(u32);
	seq_printf(m, "Version = %u Timestamp = 0x%llx Size = %u\n",
			ver.ver, ts, ver.size);
	pos += sizeof(ver);

	while (*pos) {
		i = 0;
		p = pos;
		memset(name, 0, RESOURCE_NAME_MAX);
		/**
		 * Read the resource name (null terminated),
		 * aligned on a 4 byte boundary.
		 */
		do {
			name[i] = readb_relaxed(p++);
			if (i % 4 == 0)
				pos += 4;
			if (!name[i])
				break;
			i++;
		} while (1);

		memcpy_fromio(&res, pos, sizeof(res));
		seq_printf(m,
			"Resource Name: %s Unit: %.4s Active Max: %u Active State: %u Request State: %u\n",
			name, res.units, res.active_max,
			res.active_state, res.request_state);
		pos += sizeof(res);

		/* Read the clients */
		while (*pos) {
			memcpy_fromio(&client, pos, sizeof(client));
			seq_printf(m,
				"\tClient Name: %.4s Type: %.4s Request State: %u\n",
				client.name, client.type, client.request_state);
			pos += sizeof(client);
		}

		/* Skip the NULL terminator for the resource */
		pos += sizeof(u32);
	}

	return ret;
}

static int npa_file_open(struct inode *inode, struct file *file)
{
	if (!inode->i_private)
		return -ENODEV;

	return single_open(file, npa_file_read, inode->i_private);
}

static const struct file_operations npa_fops = {
	.open		= npa_file_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int npa_dump_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *npa_base, *rpm_base;
	struct dentry *dent;
	int ret;

	/* Get the location of the NPA log's start address offset */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rpm_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rpm_base))
		return PTR_ERR(rpm_base);

	/* Offset the log's start address from the RPM phys address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	npa_base = devm_ioremap(&pdev->dev,
				res->start + readl_relaxed(rpm_base),
				resource_size(res));
	if (IS_ERR(npa_base))
		return PTR_ERR(npa_base);

	dent = debugfs_create_file("npa-dump", S_IRUGO, NULL,
					npa_base, &npa_fops);
	if (!dent) {
		pr_err("%s: error debugfs_create_file failed\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, dent);

	return ret;
}

static int npa_dump_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);

	return 0;
}

static const struct of_device_id npa_dump_table[] = {
	{.compatible = "qcom,npa-dump"},
	{},
};

static struct platform_driver npa_dump_driver = {
	.probe  = npa_dump_probe,
	.remove = npa_dump_remove,
	.driver = {
		.name = "npa-dump",
		.of_match_table = npa_dump_table,
	},
};
module_platform_driver(npa_dump_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RPM NPA Dump driver");
MODULE_ALIAS("platform:npa-dump");
