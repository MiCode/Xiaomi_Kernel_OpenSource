// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/syscore_ops.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/interconnect.h>
#include <mt-plat/dvfsrc-exp.h>
#include <linux/of.h>

#include "net_speed.h"
#include "md_spd_dvfs_fn.h"
#include "md_spd_dvfs_method.h"


#define MAX_C_NUM 4

struct dvfs_ref {
	u64 speed;
	int cx_freq[MAX_C_NUM]; /* Cluster 0 ~ 3 */
	int dram_lvl;
	int irq_affinity;
	int task_affinity;
	int rps;
	int bat_affinity;
};

/* downlink */
static const struct dvfs_ref s_dl_dvfs_tbl_v0[] = { /* default */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{100000000000LL, {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x3D, 0xFF}, /* 100G */
	/* normal */
	{0LL,            {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x3D, 0xFF},
};

static const struct dvfs_ref s_dl_dvfs_tbl_v1[] = { /* 6:2 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{4000000000LL, {-1, -1, -1, -1}, -1, 0x02, 0xC0, 0xC0, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x3F, 0xFF},
};

static const struct dvfs_ref s_dl_dvfs_tbl_v2[] = { /* 4:4 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{4000000000LL, {-1, -1, -1, -1}, -1, 0x02, 0xF0, 0xF0, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x0F, 0xFF},
};

static const struct dvfs_ref s_dl_dvfs_tbl_v3[] = { /* 4:3:1 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{1000000000LL, {-1, -1, -1, -1}, -1, 0x02, 0x30, 0x40, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x0F, 0xFF},
};

static const struct dvfs_ref s_dl_dvfs_tbl_v4[] = { /* 4:3:1 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{2000000000LL, {-1, -1, -1, -1}, -1, 0x02, 0x10, 0x20, 0x40},
	{1000000000LL, {-1, -1, -1, -1}, -1, 0x02, 0x04, 0x08, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x0F, 0xFF},
};

/* uplink */
static const struct dvfs_ref s_ul_dvfs_tbl_v0[] = { /* default */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{100000000000LL, {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x3D, 0xFF}, /* 100G */
	/* normal */
	{0LL,            {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x3D, 0xFF},
};

static const struct dvfs_ref s_ul_dvfs_tbl_v1[] = { /* 6:2 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{450000000LL, {2000000, 2000000, -1, -1}, -1, 0x02, 0xC0, 0xC0, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x3F, 0xFF},
};

static const struct dvfs_ref s_ul_dvfs_tbl_v2[] = { /* 4:4 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{450000000LL, {2000000, 2000000, -1, -1}, -1, 0x02, 0xF0, 0xF0, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x0F, 0xFF},
};

static const struct dvfs_ref s_ul_dvfs_tbl_v3[] = { /* 4:3:1 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{450000000LL, {2000000, 2000000, -1, -1}, 1, 0x02, 0x70, 0x70, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x0F, 0xFF},
};

static const struct dvfs_ref s_ul_dvfs_tbl_v4[] = { /* 4:3:1 */
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps, bat*/
	{450000000LL, {2000000, 2000000, -1, -1}, 1, 0x02, 0x10, 0x20, 0xFF},
	/* normal */
	{0LL,          {-1, -1, -1, -1}, -1, 0xFF, 0xFF, 0x0F, 0xFF},
};

#define QOS_PREFER_CPU_BITMAP_V0_2_6	(0xC0)
#define QOS_PREFER_CPU_BITMAP_V1_4_4	(0xF0)
#define QOS_PREFER_CPU_BITMAP_V2_1_3_4	(0x70)

struct dvfs_ref_tbl {
	const struct dvfs_ref *dl_tbl;
	const struct dvfs_ref *ul_tbl;
	unsigned int dl_tbl_item_num;
	unsigned int ul_tbl_item_num;
	unsigned int prefer_core_bitmap;
};

static const struct dvfs_ref_tbl table_entry[] = {
	{s_dl_dvfs_tbl_v0, s_ul_dvfs_tbl_v0, (unsigned int)ARRAY_SIZE(s_dl_dvfs_tbl_v0),
		(unsigned int)ARRAY_SIZE(s_ul_dvfs_tbl_v0), QOS_PREFER_CPU_BITMAP_V0_2_6},

	{s_dl_dvfs_tbl_v1, s_ul_dvfs_tbl_v1, (unsigned int)ARRAY_SIZE(s_dl_dvfs_tbl_v1),
		(unsigned int)ARRAY_SIZE(s_ul_dvfs_tbl_v1), QOS_PREFER_CPU_BITMAP_V0_2_6},

	{s_dl_dvfs_tbl_v2, s_ul_dvfs_tbl_v2, (unsigned int)ARRAY_SIZE(s_dl_dvfs_tbl_v2),
		(unsigned int)ARRAY_SIZE(s_ul_dvfs_tbl_v2), QOS_PREFER_CPU_BITMAP_V1_4_4},

	{s_dl_dvfs_tbl_v3, s_ul_dvfs_tbl_v3, (unsigned int)ARRAY_SIZE(s_dl_dvfs_tbl_v3),
		(unsigned int)ARRAY_SIZE(s_ul_dvfs_tbl_v3), QOS_PREFER_CPU_BITMAP_V2_1_3_4},

	{s_dl_dvfs_tbl_v4, s_ul_dvfs_tbl_v4, (unsigned int)ARRAY_SIZE(s_dl_dvfs_tbl_v4),
		(unsigned int)ARRAY_SIZE(s_ul_dvfs_tbl_v4), QOS_PREFER_CPU_BITMAP_V2_1_3_4},
};

static const struct dvfs_ref *s_dl_dvfs_tbl;
static int s_dl_dvfs_items_num;
static const struct dvfs_ref *s_ul_dvfs_tbl;
static int s_ul_dvfs_items_num;
static unsigned int s_prefer_cpu_bitmap;

static struct task_struct *s_rx_push_task;
static struct task_struct *s_alloc_bat_task;

static int s_curr_dl_idx, s_curr_ul_idx;

static void spd_qos_tbl_init(void)
{
	int ret;
	unsigned int ver = 0;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,dpmaif");
	if (node) {
		ret = of_property_read_u32(node, "net_spd_ver", &ver);
		if (ret < 0)
			pr_info("ccci: spd: [%s] not found: net_spd_ver, ret=%d\n",
				__func__, ret);
	}

	pr_info("ccci: spd: qos tbl ver:%u(%u)\n", ver, (unsigned int)ARRAY_SIZE(table_entry));
	if (ver >= (unsigned int)ARRAY_SIZE(table_entry)) {
		pr_info("ccci: spd: change qos version %u to default: 0\n", ver);
		ver = 0;
	}

	s_dl_dvfs_tbl = table_entry[ver].dl_tbl;
	s_dl_dvfs_items_num = table_entry[ver].dl_tbl_item_num;

	s_ul_dvfs_tbl = table_entry[ver].ul_tbl;
	s_ul_dvfs_items_num = table_entry[ver].ul_tbl_item_num;

	s_prefer_cpu_bitmap = table_entry[ver].prefer_core_bitmap;

	if (s_ul_dvfs_items_num)
		s_curr_ul_idx = s_ul_dvfs_items_num - 1;
	if (s_dl_dvfs_items_num)
		s_curr_dl_idx = s_dl_dvfs_items_num - 1;
}

static inline int get_speed_hint(u64 speed, int curr_idx,
				const struct dvfs_ref *ref, int num)
{
	int i, new_idx = curr_idx;
	u64 middle_speed;

	if (num < 2) /* 0 and 1 */
		return -1; /* No change and no action */

	for (i = 0; i < num; i++) {
		if (speed >= ref[i].speed) {
			new_idx = i;
			break;
		}
	}

	if (new_idx == curr_idx)
		return -1; /* No change and no action */

	if (new_idx < curr_idx) {
		pr_info("ccci : spd: curr_idx: %x; new_idx :%x]\n", curr_idx, new_idx);
		return new_idx;
	}

	middle_speed = (ref[new_idx].speed + ref[new_idx - 1].speed);
	middle_speed = middle_speed >> 1;

	if (speed >= middle_speed)
		return -1;

	pr_info("ccci : spd: speed: %lld; middle_speed :%lld\n",
			speed, middle_speed);

	return new_idx;
}


static int s_final_cpu_freq[MAX_C_NUM];
static int s_dram_lvl;
unsigned int s_isr_affinity, s_task_affinity, s_rps;

static inline void apply_qos_cpu_freq(void)
{
	unsigned int i, need_update = 0;
	const struct dvfs_ref *dl_ref, *ul_ref;

	dl_ref = &s_dl_dvfs_tbl[s_curr_dl_idx];
	ul_ref = &s_ul_dvfs_tbl[s_curr_ul_idx];

	for (i = 0; i < MAX_C_NUM; i++) {
		if (ul_ref->cx_freq[i] <= dl_ref->cx_freq[i])
			s_final_cpu_freq[i] = dl_ref->cx_freq[i];
		else
			s_final_cpu_freq[i] = ul_ref->cx_freq[i];

		if (s_final_cpu_freq[i] > -1)
			need_update = 1;
	}

	if (need_update)
		mtk_ccci_qos_cpu_cluster_freq_update(
				s_final_cpu_freq, MAX_C_NUM);
}

static inline void apply_qos_dram_freq(void)
{
	const struct dvfs_ref *dl_ref, *ul_ref;
	int dram_lvl = -1;

	dl_ref = &s_dl_dvfs_tbl[s_curr_dl_idx];
	ul_ref = &s_ul_dvfs_tbl[s_curr_ul_idx];

	if (dl_ref->dram_lvl >= ul_ref->dram_lvl)
		dram_lvl = dl_ref->dram_lvl;
	else
		dram_lvl = ul_ref->dram_lvl;

	if (dram_lvl != s_dram_lvl) {
		s_dram_lvl = dram_lvl;
		mtk_ccci_qos_dram_update(s_dram_lvl);
	}
}

static inline void apply_qos_rps(void)
{
	const struct dvfs_ref *dl_ref, *ul_ref;
	int case_type, dl_rps = 0, ul_rps = 0;

	dl_ref = &s_dl_dvfs_tbl[s_curr_dl_idx];
	ul_ref = &s_ul_dvfs_tbl[s_curr_ul_idx];
	dl_rps = dl_ref->rps;
	ul_rps = ul_ref->rps;

	if (dl_ref->rps & s_prefer_cpu_bitmap) {
		s_rps = dl_ref->rps;
		case_type = 0;
	} else if (ul_ref->rps & s_prefer_cpu_bitmap) {
		s_rps = ul_ref->rps;
		case_type = 1;
	} else {
		s_rps = dl_ref->rps;
		case_type = 2;
	}

	pr_info("ccci: spd: rps val:0x%x[dl:0x%x -- ul:0x%x]<%d>\n",
			s_rps, dl_rps, ul_rps, case_type);
	set_ccmni_rps(s_rps);
}

static inline void apply_task_affinity(u32 push_cpus, int cpu_nr,
		struct task_struct *task)
{
	struct cpumask tmask;
	int i, ret;

	if (!task)
		return;

	cpumask_clear(&tmask);

	for (i = 0; i < cpu_nr; i++) {
		if (push_cpus & (1 << i))
			cpumask_set_cpu(i, &tmask);
	}


	ret = set_cpus_allowed_ptr(task, &tmask);
	pr_info("ccci: spd: task cpus: (0x%X); ret: %d\n",
		push_cpus, ret);
}

static inline void apply_qos_task_affinity(void)
{
	const struct dvfs_ref *dl_ref;

	dl_ref = &s_dl_dvfs_tbl[s_curr_dl_idx];

	apply_task_affinity(dl_ref->task_affinity, 8,
			s_rx_push_task);

	apply_task_affinity(dl_ref->bat_affinity, 8,
			s_alloc_bat_task);
}

static inline void spd_qos_method(u64 dl_speed[], u32 dl_num, u64 ul_speed[], u32 ul_num)
{
	unsigned int i;
	u64 dl_spd = 0ULL, ul_spd = 0ULL;
	int new_idx, ul_change = 0, dl_change = 0;

	for (i = 0; i < dl_num; i++)
		dl_spd += dl_speed[i];
	for (i = 0; i < ul_num; i++)
		ul_spd += ul_speed[i];

	new_idx = get_speed_hint(dl_spd, s_curr_dl_idx, s_dl_dvfs_tbl,
		s_dl_dvfs_items_num);
	if (new_idx >= 0) {
		s_curr_dl_idx = new_idx;
		dl_change = 1;
	}

	new_idx = get_speed_hint(ul_spd, s_curr_ul_idx, s_ul_dvfs_tbl,
		s_ul_dvfs_items_num);
	if (new_idx >= 0) {
		s_curr_ul_idx = new_idx;
		ul_change = 1;
	}
	if (ul_change || dl_change) {
		pr_info("ccci : spd: new idx[dl:%x--ul:%x]\n", s_curr_dl_idx, s_curr_ul_idx);
		apply_qos_cpu_freq();
		apply_qos_dram_freq();
		apply_qos_rps();

		if (dl_change)
			apply_qos_task_affinity();
	}
}

void mtk_ccci_spd_qos_set_task(
	struct task_struct *rx_push_task,
	struct task_struct *alloc_bat_task)
{
	s_rx_push_task = rx_push_task;
	s_alloc_bat_task = alloc_bat_task;
}

void mtk_ccci_spd_qos_method_init(void)
{
	spd_qos_tbl_init();
	mtk_ccci_register_speed_callback(spd_qos_method, NULL);
}
