// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <gs/mtk_lpm_pwr_gs.h>
#include <gs/v1/mtk_power_gs.h>


#define PER_LINE_TO_PRINT 8
#define DEBUG_BUF_SIZE 2048

static char buf[DEBUG_BUF_SIZE] = { 0 };
static int slp_chk_golden_diff_mode = 1;

struct mtk_lpm_gs_pmic_info_inst {
	unsigned int count_pmic;
	struct mtk_lpm_gs_pmic_info *info;
};

static struct mtk_lpm_gs_pmic_info_inst mtk_lpm_pmics;

void generic_dump_pmic(const char *pmic_name, int pmic_num,
			      struct mtk_lpm_gs_pmic_user *user,
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
					user->array[i], val0);

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

int mtk_lpm_gs_pmic_cmp_init(void *data)
{
	int i = 0;
	struct mtk_lpm_gs_pmic_info *_info;
	struct mtk_lpm_gs_pmic *const *_pmic;

	if (!data)
		return -EINVAL;

	_info = (struct mtk_lpm_gs_pmic_info *)data;

	for (i = 0, _pmic = _info->pmic; *_pmic ; i++, _pmic++) {
		if (_info->attach)
			_info->attach((*_pmic));
	}

	mtk_lpm_pmics.count_pmic = i;
	mtk_lpm_pmics.info = _info;

	return 0;
}

int mtk_lpm_gs_pmic_cmp(int user)
{
	int i = 0;
	struct mtk_lpm_gs_pmic_info *_info = mtk_lpm_pmics.info;
	struct mtk_lpm_gs_pmic *const *_pmic;
	struct regulator *regulator;
	struct regmap *regmap;

	for (i = 0, _pmic = _info->pmic; *_pmic ; i++, _pmic++) {
		regulator = regulator_get_optional(NULL, (*_pmic)->regulator);

		if (IS_ERR(regulator)) {
			pr_info("%s regulator get failed\n", __func__);
			continue;
		}
		regmap = regulator_get_regmap(regulator);
		if (IS_ERR(regmap)) {
			pr_info("%s regmap get failed\n", __func__);
			continue;
		}
		generic_dump_pmic((*_pmic)->pwr_domain, i,
				  &((*_pmic)->user[user]), regmap);
	}

	return 0;
}

struct mtk_lpm_gs_cmp mtk_lpm_gs_cmp_pmic = {
	.cmp_init = mtk_lpm_gs_pmic_cmp_init,
	.cmp = mtk_lpm_gs_pmic_cmp,
};

int mtk_lpm_gs_dump_pmic_init(void)
{
	mtk_lpm_pwr_gs_compare_register(MTK_LPM_GS_CMP_PMIC,
					&mtk_lpm_gs_cmp_pmic);
	return 0;
}

void mtk_lpm_gs_dump_pmic_deinit(void)
{
	mtk_lpm_pwr_gs_compare_unregister(MTK_LPM_GS_CMP_PMIC);
}

