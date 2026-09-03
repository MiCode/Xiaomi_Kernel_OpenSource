// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Spreadtrum Communications Inc.

#include <asm/page.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/sprd_soc_id.h>

struct proc_dir_entry *socid_base;

static const char * const syscon_name[] = {
	"chip-id",
	"plat-id",
	"implement-id",
	"manufacture-id",
	"version-id"
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};
static struct register_gpr syscon_regs[ARRAY_SIZE(syscon_name)];

int sprd_get_soc_id(sprd_soc_id_type_t soc_id_type, u32 *id, int id_len)
{
	int ret;
	u32 chip_id[2];

	switch (soc_id_type) {
	case AON_CHIP_ID:
	case AON_PLAT_ID:
		if (syscon_regs[soc_id_type].gpr == NULL) {
			pr_err("fail to get soc_id_type(%d)\n", soc_id_type);
			return -EINVAL;
		}
		if (id_len < 2) {
			pr_err("id_len < 2\n");
			return -EINVAL;
		}

		ret = regmap_read(syscon_regs[soc_id_type].gpr,
				  syscon_regs[soc_id_type].reg, &chip_id[0]);
		if (ret) {
			pr_err("Failed to read chip_id[0]\n");
			return -EINVAL;
		}
		ret = regmap_read(syscon_regs[soc_id_type].gpr,
				  syscon_regs[soc_id_type].reg + 0x4, &chip_id[1]);
		if (ret) {
			pr_err("Failed to read chip_id[1]\n");
			return -EINVAL;
		}
		*id = chip_id[0];
		*(id + 1) = chip_id[1];
		break;
	case AON_IMPL_ID:
	case AON_MFT_ID:
	case AON_VER_ID:
		if (syscon_regs[soc_id_type].gpr == NULL) {
			pr_err("fail to get soc_id_type(%d)\n", soc_id_type);
			return -EINVAL;
		}
		ret = regmap_read(syscon_regs[soc_id_type].gpr,
				  syscon_regs[soc_id_type].reg, id);
		if (ret) {
			pr_err("Failed to read soc id\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(sprd_get_soc_id);

static ssize_t read_socid(struct file *file, char  *buf,
			size_t count, loff_t *data)
{
	u32 value[2] = {0};
	int i, n = 0;
	char c[140] = {0};

	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		n += snprintf(c + n, sizeof(c)-n, "%s ", syscon_name[i]);
		sprd_get_soc_id(i, &value[0], 2);
		n += snprintf(c + n, sizeof(c)-n, "0x%x", value[0]);
		if (i <= AON_PLAT_ID)
			n += snprintf(c + n, sizeof(c)-n, "  0x%x", value[1]);
		n += snprintf(c + n, sizeof(c)-n, "%s", "\n");
	}

	return simple_read_from_buffer(buf, count, data, c, n);
}

static inline int open_socid_fs(struct inode *inode, struct file *file)
{
	return single_open(file, 0, NULL);
}

static const struct proc_ops socid_fops = {
	.proc_open = open_socid_fs,
	.proc_read = read_socid,
};

static int sprd_create_socid_node(void)
{
	socid_base = proc_mkdir("socid", NULL);
	if (!socid_base)
		return -ENOMEM;

	if (!proc_create("socid_inf", 0444, socid_base, &socid_fops)) {
		pr_err("%s: create soc_id_inf fail\n", __func__);
		return -ENOENT;
	}

	return 0;
}

static int sprd_soc_id_probe(struct platform_device *pdev)
{
	int i;
	struct device_node *np = pdev->dev.of_node;
	const char *pname;
	struct regmap *tregmap = NULL;
	uint32_t args[2];

	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap =  syscon_regmap_lookup_by_phandle_args(np, pname, 2, args);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}

		syscon_regs[i].gpr = tregmap;
		syscon_regs[i].reg = args[0];
		syscon_regs[i].mask = args[1];
		pr_debug("dts[%s] 0x%x 0x%x\n", pname,
			syscon_regs[i].reg,
			syscon_regs[i].mask);
	}

	return sprd_create_socid_node();
}

static int sprd_soc_id_remove(struct platform_device *pdev)
{
	remove_proc_entry("socid_inf", socid_base);
	remove_proc_entry("socid", NULL);

	return 0;
}

static const struct of_device_id sprd_soc_id_of_match[] = {
	{.compatible = "sprd,soc-id"},
	{},
};

static struct platform_driver sprd_soc_id_driver = {
	.probe = sprd_soc_id_probe,
	.remove = sprd_soc_id_remove,
	.driver = {
		.name = "sprd-soc-id",
		.of_match_table = sprd_soc_id_of_match,
	},
};

module_platform_driver(sprd_soc_id_driver);

MODULE_AUTHOR("Luting Guo <luting.guo@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum soc id driver");
MODULE_LICENSE("GPL v2");
