/*
 * Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
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
extern struct clk_ops clk_dummy_ops;

/* Debugfs Measure Clocks */

/**
 * struct measure_clk_data - Structure of clk measure
 *
 * @cxo:		XO clock.
 * @xo_div4_cbcr:	offset of debug XO/4 div register.
 * @ctl_reg:		offset of debug control register.
 * @status_reg:		offset of debug status register.
 *
 */
struct measure_clk_data {
	struct clk *cxo;
	u32 xo_div4_cbcr;
	u32 ctl_reg;
	u32 status_reg;
};

/**
 * List of Debug clock controllers.
 */
enum debug_cc {
	GCC,
	MMCC,
	GPU,
	CPU,
};

/**
 * struct clk_src - Struture of clock source for debug mux
 *
 * @parents:	clock name to be used as parent for debug mux.
 * @sel:	debug mux index at global clock controller.
 * @dbg_cc:     indicates the clock controller for recursive debug clock
 *		controllers.
 * @next_sel:	indicates the debug mux index at recursive debug mux.
 * @mask:	indicates the mask required at recursive debug mux.
 * @shift:	indicates the shift required at recursive debug mux.
 * @en_mask:	indicates the enable bit mask at recursive debug mux.
 *		Incase the recursive debug mux does not have a enable bit,
 *		0xFF should be used to indicate the same, otherwise global
 *		enable bit would be used.
 */
struct clk_src {
	const char  *parents;
	int sel;
	enum debug_cc dbg_cc;
	int next_sel;
	u32 mask;
	u32 shift;
	u32 en_mask;
};

#define MUX_SRC_LIST(...) \
	.parent = (struct clk_src[]){__VA_ARGS__}, \
	.num_parents = ARRAY_SIZE(((struct clk_src[]){__VA_ARGS__}))

/**
 * struct clk_debug_mux - Struture of clock debug mux
 *
 * @parent:		structure of clk_src
 * @num_parents:	number of parents
 * @regmap:		regmaps of debug mux
 * @num_parent_regmap:	number of regmap of debug mux
 * @priv:		private measure_clk_data to be used by debug mux
 * @en_mask:		indicates the enable bit mask at global clock
 *			controller debug mux.
 * @mask:		indicates the mask to be used at global clock
 *			controller debug mux.
 * @debug_offset:	Start of debug mux offset.
 * @hw:			handle between common and hardware-specific interfaces.
 */
struct clk_debug_mux {
	struct clk_src *parent;
	int num_parents;
	struct regmap **regmap;
	int num_parent_regmap;
	void *priv;
	u32 en_mask;
	u32 mask;
	u32 debug_offset;
	struct clk_hw hw;
};

#define BM(msb, lsb) (((((uint32_t)-1) << (31-msb)) >> (31-msb+lsb)) << lsb)
#define BVAL(msb, lsb, val)     (((val) << lsb) & BM(msb, lsb))

#define to_clk_measure(_hw) container_of((_hw), struct clk_debug_mux, hw)

extern const struct clk_ops clk_debug_mux_ops;

#endif
