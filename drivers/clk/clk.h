/*
 * linux/drivers/clk/clk.h
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct clk_hw;

#if defined(CONFIG_OF)
struct clk *__of_clk_get_from_provider(struct of_phandle_args *clkspec,
				       const char *dev_id, const char *con_id);
#endif

#ifdef CONFIG_COMMON_CLK
struct clk *__clk_create_clk(struct clk_hw *hw, const char *dev_id,
			     const char *con_id);
void __clk_free_clk(struct clk *clk);

/* Debugfs API to print the enabled clocks */
void clock_debug_print_enabled(void);
int clk_register_debug(struct clk_hw *hw, struct dentry *dentry);
void clk_debug_measure_add(struct clk_hw *hw, struct dentry *dentry);

#else
/* All these casts to avoid ifdefs in clkdev... */
static inline struct clk *
__clk_create_clk(struct clk_hw *hw, const char *dev_id, const char *con_id)
{
	return (struct clk *)hw;
}
static inline void __clk_free_clk(struct clk *clk) { }
static struct clk_hw *__clk_get_hw(struct clk *clk)
{
	return (struct clk_hw *)clk;
}

#endif
