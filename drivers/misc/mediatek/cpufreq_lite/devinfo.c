// SPDX-License-Identifier: GPL-2.0
/*
 * cpufreq-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Tungchen Shih <tungchen.shih@mediatek.com>
 */

#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
static const uint16_t devinfo_table[] = {
	3539,   492,    1038,   106,    231,    17,     46,     2179,
	4,      481,    1014,   103,    225,    17,     45,     2129,
	3,      516,    1087,   111,    242,    19,     49,     2282,
	4,      504,    1063,   108,    236,    18,     47,     2230,
	4,      448,    946,    96,     210,    15,     41,     1986,
	2,      438,    924,    93,     205,    14,     40,     1941,
	2,      470,    991,    101,    220,    16,     43,     2080,
	3,      459,    968,    98,     215,    16,     42,     2033,
	3,      594,    1250,   129,    279,    23,     57,     2621,
	6,      580,    1221,   126,    273,    22,     56,     2561,
	6,      622,    1309,   136,    293,    24,     60,     2745,
	7,      608,    1279,   132,    286,    23,     59,     2683,
	6,      541,    1139,   117,    254,    20,     51,     2390,
	5,      528,    1113,   114,    248,    19,     50,     2335,
	4,      566,    1193,   123,    266,    21,     54,     2503,
	5,      553,    1166,   120,    260,    21,     53,     2446,
	5,      338,    715,    70,     157,    9,      29,     1505,
	3153,   330,    699,    69,     153,    9,      28,     1470,
	3081,   354,    750,    74,     165,    10,     31,     1576,
	3302,   346,    732,    72,     161,    10,     30,     1540,
	3227,   307,    652,    63,     142,    8,      26,     1371,
	2875,   300,    637,    62,     139,    7,      25,     1340,
	2809,   322,    683,    67,     149,    8,      27,     1436,
	3011,   315,    667,    65,     146,    8,      26,     1404,
	2942,   408,    862,    86,     191,    13,     37,     1811,
	1,      398,    842,    84,     186,    12,     36,     1769,
	1,      428,    903,    91,     200,    14,     39,     1896,
	2,      418,    882,    89,     195,    13,     38,     1853,
	2,      371,    785,    78,     173,    11,     33,     1651,
	3458,   363,    767,    76,     169,    10,     32,     1613,
	3379,   389,    823,    82,     182,    12,     35,     1729,
	1,      380,    804,    80,     177,    11,     34,     1689,
};
void __iomem *dev_base;
int get_devinfo(int i)
{
	return readl(dev_base + (i + 1) * 4);
}
EXPORT_SYMBOL_GPL(get_devinfo);
int mtk_devinfo_init(struct platform_device *pdev)
{
	int i, j;
	int val;
	struct nvmem_cell *cell;
	unsigned char *buf;
	size_t len;
	struct resource *dev_res;

	dev_res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (dev_res)
		dev_base = ioremap(dev_res->start, resource_size(dev_res));
	else
		return -1;
	cell = nvmem_cell_get(&pdev->dev, "lkginfo");
	if (IS_ERR(cell)) {
		pr_info("Fail to get Cell\n");
		return PTR_ERR(cell);
	}
	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	for (i = 0, j = 4; i < len; i++, j += 4) {
		if (buf[i]) {
			val = devinfo_table[buf[i]];
			writel_relaxed(val, dev_base + j);
		}
	}
	writel_relaxed(0x5a5a55aa, dev_base);
	kfree(buf);
	return 0;
}
MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver v0.1.1");
MODULE_AUTHOR("Chienwei Chang <chiewei.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");
