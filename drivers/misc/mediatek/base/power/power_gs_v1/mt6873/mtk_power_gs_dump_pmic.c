// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include "mtk_power_gs_array.h"

#define PER_LINE_TO_PRINT 8
#define DEBUG_BUF_SIZE 2000

static char buf[DEBUG_BUF_SIZE] = { 0 };
static const char * const pmic_reg_name[]
  /* 6359p    6315-1      6315-2      6315-3 */
= { "vcore", "6_vbuck1", "7_vbuck1", "3_vbuck1"};
static const char * const pmic_name[]
= { "6359p", "6315-1", "6315-2", "6315-3"};

int write_pmic(int pmic_num, unsigned int addr,
		unsigned int mask, unsigned int reg_val)
{
	struct regulator *regulator;
	struct regmap *regmap;
	int ret, tmp_val = 0;

	if (pmic_num < 0 || pmic_num >= 4)
		return -1;
	regulator = regulator_get_optional(NULL, pmic_reg_name[pmic_num]);
	if (IS_ERR(regulator)) {
		pr_notice("%s regulator get failed\n", __func__);
		goto ERROR;
	}
	regmap = regulator_get_regmap(regulator);
	if (IS_ERR(regmap)) {
		pr_notice("%s regmap get failed\n", __func__);
		goto ERROR;
	}
	ret = regmap_read(regmap, addr, &tmp_val);
	if (ret) {
		pr_notice("%s regmap read fail\n", __func__);
		goto ERROR;
	}
	tmp_val &= (~mask);
	tmp_val |= reg_val;
	ret = regmap_write(regmap, addr, tmp_val);
	if (ret) {
		pr_notice("%s regmap write fail\n", __func__);
		goto ERROR;
	}
	return 0;
ERROR:
	return -1;
}


void dump_pmic(int pmic_num, const char *scenario,
	const unsigned int *pmic_gs, unsigned int pmic_gs_len)
{
	unsigned int i, k, val0 = 0, val1, val2, diff, dump_cnt = 0, ret;
	char *p;
	struct regulator *regulator;
	struct regmap *regmap;

	if (pmic_num < 0 || pmic_num >= 4)
		return;
	regulator = regulator_get_optional(NULL, pmic_reg_name[pmic_num]);
	if (IS_ERR(regulator)) {
		pr_notice("%s regulator get failed\n", __func__);
		goto ERROR;
	}
	regmap = regulator_get_regmap(regulator);
	if (IS_ERR(regmap)) {
		pr_notice("%s regmap get failed\n", __func__);
		goto ERROR;
	}
	/* dump diff mode */
	if (slp_chk_golden_diff_mode) {
		p = buf;
		p += snprintf(p, sizeof(buf), "\n");
		p += snprintf(p, sizeof(buf) - (p - buf),
		"Scenario - %s - Addr      - Value    - Mask       - Golden     - Wrong Bit\n",
				pmic_name[pmic_num]);

		for (i = 0; i < pmic_gs_len; i += 3) {
			ret = regmap_read(regmap, pmic_gs[i], &val0);
			if (ret) {
				pr_notice("read pmic 6315-%d 0x%x error\n",
					pmic_num, pmic_gs[i]);
				goto ERROR;
			}
			val1 = val0 & pmic_gs[i + 1];
			val2 = pmic_gs[i + 2] & pmic_gs[i + 1];

			if (val1 != val2) {
				dump_cnt++;
				p += snprintf(p, sizeof(buf) - (p - buf),
			"%s - %s - 0x%08x - 0x%08x- 0x%08x - 0x%08x -",
			scenario, pmic_name[pmic_num], pmic_gs[i], val0,
					pmic_gs[i + 1], pmic_gs[i + 2]);

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
					pr_notice("%s", buf);
					p = buf;
					p += snprintf(p, sizeof(buf), "\n");
				}
			}

		}
		if (dump_cnt % PER_LINE_TO_PRINT)
			pr_notice("%s", buf);

	/* dump raw data mode */
	} else {
		p = buf;
		p += snprintf(p, sizeof(buf), "\n");
		p += snprintf(p, sizeof(buf) - (p - buf),
		"Scenario - PMIC - Addr       - Value\n");

		for (i = 0; i < pmic_gs_len; i += 3) {
			ret = regmap_read(regmap, pmic_gs[i], &val0);
			if (ret) {
				pr_notice("read pmic 6315-%d 0x%x error\n",
				pmic_num, pmic_gs[i]);
				goto ERROR;
			}
			dump_cnt++;
			p += snprintf(p, sizeof(buf) - (p - buf),
				"%s - 6315-%d - 0x%08x - 0x%08x\n",
				scenario, pmic_num, pmic_gs[i], val0);

			if (dump_cnt && ((dump_cnt % PER_LINE_TO_PRINT) == 0)) {
				pr_notice("%s", buf);
				p = buf;
				p += snprintf(p, sizeof(buf), "\n");
			}
		}
		if (dump_cnt % PER_LINE_TO_PRINT)
			pr_notice("%s", buf);
	}
ERROR:
	return;
}
