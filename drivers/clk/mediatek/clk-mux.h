/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRV_CLK_MUX_H
#define __DRV_CLK_MUX_H

#include <linux/clk-provider.h>

struct mtk_clk_mux {
	struct clk_hw hw;
	struct regmap *regmap;

	const char *name;

	int mux_set_ofs;
	int mux_clr_ofs;
	int mux_ofs;
	int upd_ofs;

	s8 mux_shift;
	s8 mux_width;
	s8 gate_shift;
	s8 upd_shift;

	spinlock_t *lock;
};

extern const struct clk_ops mtk_mux_upd_ops;
extern const struct clk_ops mtk_mux_clr_set_upd_ops;

struct clk *mtk_clk_register_mux(const struct mtk_mux *mux,
				 struct regmap *regmap,
				 spinlock_t *lock);

struct mtk_mux_upd {
	int id;
	const char *name;
	const char * const *parent_names;

	u32 mux_ofs;
	u32 upd_ofs;

	s8 mux_shift;
	s8 mux_width;
	s8 gate_shift;
	s8 upd_shift;

	s8 num_parents;
};
/*
 *#define MUX_UPD(_id, _name, _parents, _mux_ofs, _shift, _width, _gate,\
 *			_upd_ofs, _upd) {				\
 *		.id = _id,						\
 *		.name = _name,						\
 *		.mux_ofs = _mux_ofs,					\
 *		.upd_ofs = _upd_ofs,					\
 *		.mux_shift = _shift,					\
 *		.mux_width = _width,					\
 *		.gate_shift = _gate,					\
 *		.upd_shift = _upd,					\
 *		.parent_names = _parents,				\
 *		.num_parents = ARRAY_SIZE(_parents),			\
 *	}
 */
struct mtk_mux_clr_set_upd {
	int id;
	const char *name;
	const char * const *parent_names;

	u32 mux_ofs;
	u32 mux_set_ofs;
	u32 mux_clr_ofs;
	u32 upd_ofs;

	s8 mux_shift;
	s8 mux_width;
	s8 gate_shift;
	s8 upd_shift;

	s8 num_parents;
};
#if defined(CONFIG_MACH_MT6739)
#define MUX_CLR_SET_UPD_MT6739(_id, _name, _parents, _mux_ofs, _mux_set_ofs, \
		_mux_clr_ofs, _shift, _width, _gate,	\
		_upd_ofs, _upd) {				\
		.id = _id,						\
		.name = _name,						\
		.mux_ofs = _mux_ofs,					\
		.mux_set_ofs = _mux_set_ofs,				\
		.mux_clr_ofs = _mux_clr_ofs,				\
		.upd_ofs = _upd_ofs,					\
		.mux_shift = _shift,					\
		.mux_width = _width,					\
		.gate_shift = _gate,					\
		.upd_shift = _upd,					\
		.parent_names = _parents,				\
		.num_parents = ARRAY_SIZE(_parents),			\
	}
#endif

struct clk *mtk_clk_register_mux_upd(const struct mtk_mux_upd *tm,
		void __iomem *base, spinlock_t *lock);

struct clk *mtk_clk_register_mux_clr_set_upd(
	const struct mtk_mux_clr_set_upd *tm,
		void __iomem *base, spinlock_t *lock);

void mtk_clk_register_mux_upds(const struct mtk_mux_upd *tms,
		int num, void __iomem *base, spinlock_t *lock,
		struct clk_onecell_data *clk_data);

void mtk_clk_register_mux_clr_set_upds(const struct mtk_mux_clr_set_upd *tms,
		int num, void __iomem *base, spinlock_t *lock,
		struct clk_onecell_data *clk_data);

#endif /* __DRV_CLK_MUX_H */
