/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __MEMORY_LOWPOWER_INTERNAL_H
#define __MEMORY_LOWPOWER_INTERNAL_H

/* Memory Lowpower State & Action */
enum power_state {
	MLP_INIT,		/* Memory-lowpower is initialized */
	MLP_SCREENON,
	MLP_SCREENOFF,
	MLP_SCREENIDLE,
	MLP_ENABLE,
	MLP_ENABLE_DCS,
	MLP_ENABLE_PASR,
};

#define TEST_MEMORY_LOWPOWER_STATE(uname, lname) \
static inline int Mlps##uname(unsigned long *state) \
		{ return test_bit(MLP_##lname, state); }

#define SET_MEMORY_LOWPOWER_STATE(uname, lname)	\
static inline void SetMlps##uname(unsigned long *state) \
			{ set_bit(MLP_##lname, state); }

#define CLEAR_MEMORY_LOWPOWER_STATE(uname, lname) \
static inline void ClearMlps##uname(unsigned long *state) \
			{ clear_bit(MLP_##lname, state); }

TEST_MEMORY_LOWPOWER_STATE(Init, INIT)
TEST_MEMORY_LOWPOWER_STATE(ScreenOn, SCREENON)
TEST_MEMORY_LOWPOWER_STATE(ScreenIdle, SCREENIDLE)
TEST_MEMORY_LOWPOWER_STATE(Enable, ENABLE)
TEST_MEMORY_LOWPOWER_STATE(EnableDCS, ENABLE_DCS)
TEST_MEMORY_LOWPOWER_STATE(EnablePASR, ENABLE_PASR)

SET_MEMORY_LOWPOWER_STATE(Init, INIT)
SET_MEMORY_LOWPOWER_STATE(ScreenOn, SCREENON)
SET_MEMORY_LOWPOWER_STATE(ScreenIdle, SCREENIDLE)
SET_MEMORY_LOWPOWER_STATE(Enable, ENABLE)
SET_MEMORY_LOWPOWER_STATE(EnableDCS, ENABLE_DCS)
SET_MEMORY_LOWPOWER_STATE(EnablePASR, ENABLE_PASR)

CLEAR_MEMORY_LOWPOWER_STATE(Init, INIT)
CLEAR_MEMORY_LOWPOWER_STATE(ScreenOn, SCREENON)
CLEAR_MEMORY_LOWPOWER_STATE(ScreenIdle, SCREENIDLE)
CLEAR_MEMORY_LOWPOWER_STATE(Enable, ENABLE)
CLEAR_MEMORY_LOWPOWER_STATE(EnableDCS, ENABLE_DCS)
CLEAR_MEMORY_LOWPOWER_STATE(EnablePASR, ENABLE_PASR)

#define IS_ACTION_SCREENON(action)	(action == MLP_SCREENON)
#define IS_ACTION_SCREENOFF(action)	(action == MLP_SCREENOFF)
#define IS_ACTION_SCREENIDLE(action)	(action == MLP_SCREENIDLE)

/* Memory Lowpower Features & their operations */
enum power_level {
	MLP_LEVEL_DCS,
	MLP_LEVEL_PASR,
	NR_MLP_LEVEL,
};

typedef void (*get_range_t) (int, unsigned long *, unsigned long *);

/* Feature specific operations */
struct memory_lowpower_operation {
	struct list_head link;
	enum power_level level;
	/*
	 * Taking actions before entering this feature -
	 * callee needs to issue func to get the range "times" times
	 */
	int (*config)(int times, get_range_t func);
	/* Entering this feature */
	int (*enable)(void);
	/* Leaving this feature */
	int (*disable)(void);
	/* Taking actions after leaving this feature */
	int (*restore)(void);
};

struct memory_lowpower_statistics {
	u64 nr_acquire_memory;
	u64 nr_release_memory;
	u64 nr_full_acquire;
	u64 nr_partial_acquire;
	u64 nr_empty_acquire;
};

/*
 * Examples for feature specific operations,
 *
 * DCS:
 *  config - data collection, trigger LPDMA (4->2)
 *  enable - Notify PowerMCU to turn off high channels
 * disable - Notify PowerMCU to turn on high channels
 * restore - trigger LPDMA (2->4)
 *
 * PASR:
 *  config - Identify banks/ranks, trigger APMCU flow
 *  enable - No operations (in the enable flow of DCS in PowerMCU/SPM)
 * disable - No operations (in the disable flow of DCS in PowerMCU/SPM)
 * restore - Trigger APMCU flow for reset
 *
 * (Not absolutely)
 * Operations are called in low to high level order for configure/enable.
 * Operations are called in reverse order for disable/restore.
 */

/* memory-lowpower-task APIs */
extern bool memory_lowpower_task_inited(void);
extern void register_memory_lowpower_operation(struct memory_lowpower_operation *handler);
extern void unregister_memory_lowpower_operation(struct memory_lowpower_operation *handler);

/* memory-lowpower APIs */
extern bool memory_lowpower_inited(void);
extern int get_memory_lowpower_cma(void);
extern int put_memory_lowpower_cma(void);
extern int get_memory_lowpower_cma_aligned(int count, unsigned int align, struct page **pages);
extern int put_memory_lowpower_cma_aligned(int count, struct page *pages);
extern int memory_lowpower_task_init(void);
extern phys_addr_t memory_lowpower_cma_base(void);
extern phys_addr_t memory_lowpower_cma_size(void);
extern void set_memory_lowpower_aligned(int aligned);

#endif /* __MEMORY_LOWPOWER_INTERNAL_H */
