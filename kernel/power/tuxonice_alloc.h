/*
 * kernel/power/tuxonice_alloc.h
 *
 * Copyright (C) 2008-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 */

#include <linux/slab.h>
#define TOI_WAIT_GFP (GFP_NOFS | __GFP_NOWARN)
#define TOI_ATOMIC_GFP (GFP_ATOMIC | __GFP_NOWARN)

#ifdef CONFIG_PM_DEBUG
extern void *toi_kzalloc(int fail_num, size_t size, gfp_t flags);
extern void toi_kfree(int fail_num, const void *arg, int size);

extern unsigned long toi_get_free_pages(int fail_num, gfp_t mask, unsigned int order);
#define toi_get_free_page(FAIL_NUM, MASK) toi_get_free_pages(FAIL_NUM, MASK, 0)
extern unsigned long toi_get_zeroed_page(int fail_num, gfp_t mask);
extern void toi_free_page(int fail_num, unsigned long buf);
extern void toi__free_page(int fail_num, struct page *page);
extern void toi_free_pages(int fail_num, struct page *page, int order);
extern struct page *toi_alloc_page(int fail_num, gfp_t mask);
extern int toi_alloc_init(void);
extern void toi_alloc_exit(void);

extern void toi_alloc_print_debug_stats(void);

#else				/* CONFIG_PM_DEBUG */

#define toi_kzalloc(FAIL, SIZE, FLAGS) (kzalloc(SIZE, FLAGS))
#define toi_kfree(FAIL, ALLOCN, SIZE) (kfree(ALLOCN))

#define toi_get_free_pages(FAIL, FLAGS, ORDER) __get_free_pages(FLAGS, ORDER)
#define toi_get_free_page(FAIL, FLAGS) __get_free_page(FLAGS)
#define toi_get_zeroed_page(FAIL, FLAGS) get_zeroed_page(FLAGS)
#define toi_free_page(FAIL, ALLOCN) do { free_page(ALLOCN); } while (0)
#define toi__free_page(FAIL, PAGE) __free_page(PAGE)
#define toi_free_pages(FAIL, PAGE, ORDER) __free_pages(PAGE, ORDER)
#define toi_alloc_page(FAIL, MASK) alloc_page(MASK)
static inline int toi_alloc_init(void)
{
	return 0;
}

static inline void toi_alloc_exit(void)
{
}

static inline void toi_alloc_print_debug_stats(void)
{
}

#endif

extern int toi_trace_allocs;
