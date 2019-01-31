/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
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

#define MUX_UPD(_id, _name, _parents, _mux_ofs, _shift, _width, _gate,	\
			_upd_ofs, _upd) {				\
		.id = _id,						\
		.name = _name,						\
		.mux_ofs = _mux_ofs,					\
		.upd_ofs = _upd_ofs,					\
		.mux_shift = _shift,					\
		.mux_width = _width,					\
		.gate_shift = _gate,					\
		.upd_shift = _upd,					\
		.parent_names = _parents,				\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = 0,					\
	}

struct mtk_mux_clr_set_upd {
	int id;
	const char *name;
	const char * const *parent_names;
	u32 flags;

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

#define MUX_CLR_SET_UPD(_id, _name, _parents, _mux_ofs, _mux_set_ofs,	\
			_mux_clr_ofs, _shift, _width, _gate,		\
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
		.flags = 0,					\
	}

#define MUX_CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs, _mux_set_ofs,\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd, _flags) {			\
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
		.flags = _flags,					\
	}


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
