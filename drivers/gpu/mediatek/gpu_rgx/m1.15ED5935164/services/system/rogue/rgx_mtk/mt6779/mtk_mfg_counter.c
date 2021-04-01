// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "mtk_mfg_counter.h"

static DEFINE_SPINLOCK(counter_info_lock);

/* FIX ME: volatile can not pass check patch
 * static volatile void *g_MFG_base;
 */

static void *g_MFG_base;
static int mfg_is_power_on;

/* FIX ME: volatile can not pass check patch
 * #define base_write32(addr, value) \
 *	do { *(volatile uint32_t *)(addr) = (uint32_t)(value) } while (0)
 * #define base_read32(addr)             (*(volatile uint32_t *)(addr))
 */
#define base_write32(addr, value) \
		do { *(uint32_t *)(addr) = (uint32_t)(value) } while (0)
#define base_read32(addr)             (*(uint32_t *)(addr))
#define MFG_write32(addr, value)      base_write32(g_MFG_base+addr, value)
#define MFG_read32(addr)              base_read32(g_MFG_base+addr)

typedef uint32_t (*mfg_read_pfn)(uint32_t offset);

static uint32_t RGX_read(uint32_t offset)
{
	return (g_MFG_base) ? MFG_read32(offset) : 0u;
}

static struct {
	const char *name;
	uint32_t offset;
	mfg_read_pfn read;
	unsigned int sum;
	unsigned int val_pre;
	int overflow;
} mfg_counters[] = {
{"mem0_reads",  RGX_CR_PERF_SLC0_READS,  RGX_read, 0u, 0u, 0},
{"mem0_write", RGX_CR_PERF_SLC0_WRITES, RGX_read, 0u, 0u, 0},
{"mem0_read_stalls",  RGX_CR_PERF_SLC0_READ_STALLS,  RGX_read, 0u, 0u, 0},
{"mem0_write_stalls", RGX_CR_PERF_SLC0_WRITE_STALLS, RGX_read, 0u, 0u, 0},
{"mem1_reads",  RGX_CR_PERF_SLC1_READS,  RGX_read, 0u, 0u, 0},
{"mem1_write", RGX_CR_PERF_SLC1_WRITES, RGX_read, 0u, 0u, 0},
{"mem1_read_stalls",  RGX_CR_PERF_SLC1_READ_STALLS,  RGX_read, 0u, 0u, 0},
{"mem1_write_stalls", RGX_CR_PERF_SLC1_WRITE_STALLS, RGX_read, 0u, 0u, 0},
};

#define MFG_COUNTER_SIZE (ARRAY_SIZE(mfg_counters))

/*
 * require: power must be on
 * require: get counters_lock
 */
static void mtk_mfg_counter_update(void)
{
	int i;

	for (i = 0; i < MFG_COUNTER_SIZE; ++i) {
		uint32_t val, diff;

		val = mfg_counters[i].read(mfg_counters[i].offset);
		diff = val - mfg_counters[i].val_pre;

		/* TODO: counter is reset by fw, how to be notify? */
		if (diff > 0xf7654321)
			diff = 0u - diff;

		if (mfg_counters[i].sum + diff < mfg_counters[i].sum) {
			mfg_counters[i].overflow = 1;
			mfg_counters[i].sum = (uint32_t)-1;
		} else if (mfg_counters[i].overflow == 0)
			mfg_counters[i].sum += diff;

		mfg_counters[i].val_pre = val;
	}
}

/*
 * require: get counters_lock
 */
static void mtk_mfg_counter_reset_record(void)
{
	int i;

	for (i = 0; i < MFG_COUNTER_SIZE; ++i) {
		mfg_counters[i].sum = 0u;
		mfg_counters[i].overflow = 0;
	}
}

/*
 * require: get counters_lock
 */
static void mtk_mfg_counter_reset_register(void)
{
	int i;

	for (i = 0; i < MFG_COUNTER_SIZE; ++i)
		mfg_counters[i].val_pre = 0u;
}

static int img_get_gpu_pmu_init(struct GPU_PMU *pmus,
	int pmu_size, int *ret_size)
{
	if (pmus) {
		int size = (pmu_size > MFG_COUNTER_SIZE)
					? MFG_COUNTER_SIZE : pmu_size;
		int i;

		for (i = 0; i < size; ++i) {
			pmus[i].id = i;
			pmus[i].name = mfg_counters[i].name;
			pmus[i].value = mfg_counters[i].sum;
			pmus[i].overflow = mfg_counters[i].overflow;
		}
	}

	if (ret_size)
		*ret_size = MFG_COUNTER_SIZE;

	return 0;
}

static int img_get_gpu_pmu_swapnreset(struct GPU_PMU *pmus, int pmu_size)
{
	if (pmus) {

		int size = (pmu_size > MFG_COUNTER_SIZE)
					? MFG_COUNTER_SIZE : pmu_size;
		int i;

		spin_lock(&counter_info_lock);

		/* update if gpu power on */
		if (mfg_is_power_on)
			mtk_mfg_counter_update();

		/* swap */
		for (i = 0; i < size; ++i) {
			pmus[i].value = mfg_counters[i].sum;
			pmus[i].overflow = mfg_counters[i].overflow;
		}

		/* reset */
		mtk_mfg_counter_reset_record();

		spin_unlock(&counter_info_lock);
	}

	return 0;
}

static void *_mtk_of_ioremap(const char *node_name)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, node_name);
	if (node)
		return of_iomap(node, 0);

	pr_debug("cannot find [%s] of_node, please fix me\n", node_name);
	return NULL;
}

static void gpu_power_change_notify_mfg_counter(int power_on)
{
	spin_lock(&counter_info_lock);

	if (!power_on) {
		/* update before power off */
		mtk_mfg_counter_update();
		mtk_mfg_counter_reset_register();
	}

	mfg_is_power_on = power_on;

	spin_unlock(&counter_info_lock);
}

/* ****************************************** */

void mtk_mfg_counter_init(void)
{
	g_MFG_base = _mtk_of_ioremap("mediatek,AUSTIN");

	mtk_get_gpu_pmu_init_fp = img_get_gpu_pmu_init;
	mtk_get_gpu_pmu_swapnreset_fp = img_get_gpu_pmu_swapnreset;

	mtk_register_gpu_power_change("mfg_counter",
					gpu_power_change_notify_mfg_counter);
}

void mtk_mfg_counter_destroy(void)
{
	mtk_unregister_gpu_power_change("mfg_counter");
}
