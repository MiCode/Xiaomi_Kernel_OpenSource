/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __QCOM_CLK_COMMON_H__
#define __QCOM_CLK_COMMON_H__

#include <linux/reset-controller.h>
#include "clk-rcg.h"

struct platform_device;
struct regmap_config;
struct clk_regmap;
struct qcom_reset_map;
struct regmap;
struct freq_tbl;
struct clk_hw;
struct parent_map;

struct qcom_cc_desc {
	const struct regmap_config *config;
	struct clk_regmap **clks;
	struct clk_hw **hwclks;
	size_t num_clks;
	size_t num_hwclks;
	const struct qcom_reset_map *resets;
	size_t num_resets;
	struct gdsc **gdscs;
	size_t num_gdscs;
};

struct clk_dummy {
	struct clk_hw hw;
	struct reset_controller_dev reset;
	unsigned long rrate;
};

struct clk_dfs {
	struct clk_rcg2 *rcg;
	u8 rcg_flags;
};

struct qcom_cc_dfs_desc {
	struct clk_dfs *clks;
	size_t num_clks;
};

extern const struct freq_tbl *qcom_find_freq(const struct freq_tbl *f,
					     unsigned long rate);
extern int qcom_find_src_index(struct clk_hw *hw, const struct parent_map *map,
			       u8 src);

extern int qcom_cc_register_board_clk(struct device *dev, const char *path,
				      const char *name, unsigned long rate);
extern int qcom_cc_register_sleep_clk(struct device *dev);

extern struct regmap *qcom_cc_map(struct platform_device *pdev,
				  const struct qcom_cc_desc *desc);
extern int qcom_cc_really_probe(struct platform_device *pdev,
				const struct qcom_cc_desc *desc,
				struct regmap *regmap);
extern int qcom_cc_probe(struct platform_device *pdev,
			 const struct qcom_cc_desc *desc);

extern int qcom_cc_register_rcg_dfs(struct platform_device *pdev,
			 const struct qcom_cc_dfs_desc *desc);

extern struct clk_ops clk_dummy_ops;

#define BM(msb, lsb)	(((((uint32_t)-1) << (31-msb)) >> (31-msb+lsb)) << lsb)
#define BVAL(msb, lsb, val)	(((val) << lsb) & BM(msb, lsb))

#endif
