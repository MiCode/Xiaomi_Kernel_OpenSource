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
struct clk_core;

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
struct clk *__of_clk_get_from_provider(struct of_phandle_args *clkspec,
				       const char *dev_id, const char *con_id);
#endif

#ifdef CONFIG_COMMON_CLK
struct clk *__clk_create_clk(struct clk_hw *hw, const char *dev_id,
			     const char *con_id);
void __clk_free_clk(struct clk *clk);

/* Debugfs API to print the enabled clocks */
void clock_debug_print_enabled(bool print_parent);
void clk_debug_print_hw(struct clk_core *clk, struct seq_file *f);

#define WARN_CLK(core, name, cond, fmt, ...) do {		\
		clk_debug_print_hw(core, NULL);			\
		WARN(cond, "%s: " fmt, name, ##__VA_ARGS__);	\
} while (0)

#define clock_debug_output(m, c, fmt, ...)		\
do {							\
	if (m)						\
		seq_printf(m, fmt, ##__VA_ARGS__);	\
	else if (c)					\
		pr_cont(fmt, ##__VA_ARGS__);		\
	else						\
		pr_info(fmt, ##__VA_ARGS__);		\
} while (0)

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

void clock_debug_print_enabled(void) {}
#endif
