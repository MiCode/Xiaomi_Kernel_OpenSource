// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/regulator/consumer.h>
#include <lpm.h>
#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_power_gs.h>
#define PER_LINE_TO_PRINT 8
#define DEBUG_BUF_SIZE 2048
static char buf[DEBUG_BUF_SIZE] = { 0 };
static int slp_chk_golden_diff_mode = 1;
struct lpm_gs_pmic_info_inst {
	unsigned int count_pmic;
	struct lpm_gs_pmic_info *info;
};
static struct lpm_gs_pmic_info_inst lpm_pmics;
void generic_dump_pmic(const char *pmic_name, int pmic_num,
			      struct lpm_gs_pmic_user *user,
			      struct regmap *regmap)
{
	unsigned int i, k, val0 = 0, val1, val2, diff, dump_cnt = 0, ret;
	char *p;

	if (!pmic_name || !user)
		goto ERROR;
	if (IS_ERR(regmap)) {
		pr_info("%s regmap get failed\n", __func__);
		goto ERROR;
	}
	/* dump diff mode */
	if (slp_chk_golden_diff_mode) {
		p = buf;
		p += snprintf(p, sizeof(buf), "\n");
		p += snprintf(p, sizeof(buf) - (p - buf),
		"Scenario - %s - Addr      - Value    - Mask       - Golden     - Wrong Bit\n",
				pmic_name);
		for (i = 0; i < user->array_sz; i += 3) {
			ret = regmap_read(regmap, user->array[i], &val0);
			if (ret) {
				pr_info("read pmic-%d 0x%x error\n",
					pmic_num, user->array[i]);
				goto ERROR;
			}
			val1 = val0 & user->array[i + 1];
			val2 = user->array[i + 2] & user->array[i + 1];
			if (val1 != val2) {
				dump_cnt++;
				p += snprintf(p, sizeof(buf) - (p - buf),
			"%s - %s - 0x%08x - 0x%08x- 0x%08x - 0x%08x -",
			user->name, pmic_name, user->array[i], val0,
					user->array[i + 1], user->array[i + 2]);
				for (k = 0, diff = val1 ^ val2; diff != 0; k++,
						diff >>= 1) {
					if ((diff % 2) != 0)
						p += snprintf(p,
							sizeof(buf) - (p - buf),
							" %d", k);
				}
				p += snprintf(p, sizeof(buf) - (p - buf), "\n");
				if (dump_cnt &&
					((dump_cnt % PER_LINE_TO_PRINT) == 0)) {
					pr_info("%s", buf);
					p = buf;
					p += snprintf(p, sizeof(buf), "\n");
				}
			}
		}
		if (dump_cnt % PER_LINE_TO_PRINT)
			pr_info("%s", buf);
	/* dump raw data mode */
	} else {
		p = buf;
		p += snprintf(p, sizeof(buf), "\n");
		p += snprintf(p, sizeof(buf) - (p - buf),
		"Scenario - PMIC - Addr       - Value\n");
		for (i = 0; i < user->array_sz; i += 3) {
			ret = regmap_read(regmap, user->array[i], &val0);
			if (ret) {
				pr_info("read pmic-%d 0x%x error\n",
				pmic_num, user->array[i]);
				goto ERROR;
			}
			dump_cnt++;
			p += snprintf(p, sizeof(buf) - (p - buf),
				"%s - %s-%d - 0x%08x - 0x%08x\n",
				user->name, pmic_name, pmic_num,
					user->array[i], 0);
			if (dump_cnt && ((dump_cnt % PER_LINE_TO_PRINT) == 0)) {
				pr_info("%s", buf);
				p = buf;
				p += snprintf(p, sizeof(buf), "\n");
			}
		}
		if (dump_cnt % PER_LINE_TO_PRINT)
			pr_info("%s", buf);
	}
ERROR:
	return;
}
int lpm_gs_pmic_cmp_init(void *data)
{
	int i = 0;
	struct lpm_gs_pmic_info *_info;
	struct lpm_gs_pmic *const *_pmic;

	if (!data)
		return -EINVAL;
	_info = (struct lpm_gs_pmic_info *)data;
	for (i = 0, _pmic = _info->pmic; *_pmic ; i++, _pmic++) {
		if (_info->attach)
			_info->attach((*_pmic));
	}
	lpm_pmics.count_pmic = i;
	lpm_pmics.info = _info;
	return 0;
}
#define LPM_GS_NODE "power-gs"
#define LPM_GS_REGULATOR_NAME "value"
#define LPM_GS_REGULATOR_NODE "target"
struct regmap *lpm_gs_get_regmap(struct lpm_gs_pmic const *pmic)
{
	struct device_node *lpm, *regulator_node, *np;
	struct mt6397_chip *chip;
	struct platform_device *pmic_pdev;
	struct regmap *regmap = NULL;
	int idx = 0, retry = 0;
	const char *regulator_name = NULL;

	lpm = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);
	if (!lpm) {
		pr_info("[name:mtk_lpm][P] - get lpm dts node fail!\n");
		return NULL;
	}
	while ((np = of_parse_phandle(lpm, LPM_GS_NODE, idx))) {
		of_property_read_string(np, LPM_GS_REGULATOR_NAME, &regulator_name);
		if (regulator_name && !strcmp(regulator_name, pmic->regulator)) {
			regulator_node = of_parse_phandle(np, LPM_GS_REGULATOR_NODE, 0);
			if (!regulator_node) {
				pr_info("[name:mtk_lpm][P] - get pwrap node fail!\n");
				break;
			}
			regulator_node = of_find_compatible_node(regulator_node,
								 NULL, regulator_name);
RETRY:
			if (!regulator_node) {
				pr_info("[name:mtk_lpm][P] - get pwrap node fail!\n");
				break;
			}
			pmic_pdev = of_find_device_by_node(regulator_node);
			if (!pmic_pdev) {
				pr_info("[name:mtk_lpm][P] - get pmic pdev fail!\n");
				break;
			}
			chip = dev_get_drvdata(&(pmic_pdev->dev));
			if (!chip) {
				regulator_node = of_get_next_parent(regulator_node);
				retry++;
				/* FIXME */
				if (retry == 1)
					goto RETRY;
				pr_info("[name:mtk_lpm][P] - get pmic chip fail!\n");
				break;
			}
			regmap = chip->regmap;
			if (IS_ERR(regmap)) {
				pr_info("[name:mtk_lpm][P] - regmap get failed\n");
				break;
			}
			of_node_put(regulator_node);
			return regmap;
		}
		of_node_put(np);
		idx++;
	}
	of_node_put(lpm);
	return NULL;
}
static struct regmap *pmic_get_regmap(const char *name)
{
	struct device_node *np;
	struct platform_device *pdev;

	np = of_find_node_by_name(NULL, name);
	if (!np)
		return NULL;

	pdev = of_find_device_by_node(np->child);
	if (!pdev)
		return NULL;

	return dev_get_regmap(pdev->dev.parent, NULL);
}

int lpm_gs_pmic_cmp(int user)
{
	int i = 0;
	struct lpm_gs_pmic_info *_info = lpm_pmics.info;
	struct lpm_gs_pmic *const *_pmic;
	struct regmap *regmap;

	for (i = 0, _pmic = _info->pmic; *_pmic ; i++, _pmic++) {
		regmap = NULL;
		if (!strcmp((*_pmic)->pwr_domain, "6363"))
			regmap = pmic_get_regmap("pmic");
		else if (!strcmp((*_pmic)->pwr_domain, "6366"))
			regmap = pmic_get_regmap("pwrap");
		else if (!strcmp((*_pmic)->pwr_domain, "6368") ||
			 !strcmp((*_pmic)->pwr_domain, "6373"))
			regmap = pmic_get_regmap("second_pmic");

		if (IS_ERR(regmap) || !regmap)
			continue;

		generic_dump_pmic((*_pmic)->pwr_domain, i,
				  &((*_pmic)->user[user]), regmap);
	}
	return 0;
}
struct lpm_gs_cmp lpm_gs_cmp_pmic = {
	.cmp_init = lpm_gs_pmic_cmp_init,
	.cmp = lpm_gs_pmic_cmp,
};
int lpm_gs_dump_pmic_init(void)
{
	lpm_pwr_gs_compare_register(LPM_GS_CMP_PMIC,
					&lpm_gs_cmp_pmic);
	return 0;
}
void lpm_gs_dump_pmic_deinit(void)
{
	lpm_pwr_gs_compare_unregister(LPM_GS_CMP_PMIC);
}

