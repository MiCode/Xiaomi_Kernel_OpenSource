// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/power_supply.h>

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif

#define CREATE_TRACE_POINTS
#include <perf_tracker_trace.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_sram.h>
#include <mtk_qos_share.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include <dvfsrc-exp.h>
#endif

#include <perf_tracker.h>
#include <perf_tracker_internal.h>

static void fuel_gauge_handler(struct work_struct *work);
static int fuel_gauge_enable;
static int fuel_gauge_delay;
static DECLARE_DELAYED_WORK(fuel_gauge, fuel_gauge_handler);

#if IS_ENABLED(CONFIG_MTK_CHARGER)
static void charger_handler(struct work_struct *work);
static int charger_enable;
static int charger_delay;
static DECLARE_DELAYED_WORK(charger, charger_handler);
#endif


static int perf_tracker_on;
static DEFINE_MUTEX(perf_ctl_mutex);

static struct mtk_btag_mictx_iostat_struct iostat;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
static struct mtk_btag_mictx_id ufs_mictx_id = {.storage = BTAG_STORAGE_UFS};
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
static unsigned int gpu_pmu_enable;
static unsigned int is_gpu_pmu_worked;
static unsigned int gpu_pmu_period = 8000000; //8ms
#endif
static unsigned int mcupm_freq_enable;
/* 1: enable, 3: enable & enable debug */
static unsigned int support_cpu_pmu = 1;

static DEFINE_PER_CPU(u64, cpu_last_inst_spec);
static DEFINE_PER_CPU(u64, cpu_last_cycle);
static DEFINE_PER_CPU(u64, cpu_last_l3dc);
static int pmu_init;

static void set_pmu_enable(unsigned int enable)
{
	if (IS_ERR_OR_NULL((void *)csram_base))
		return;

	__raw_writel((u32)enable, csram_base + PERF_TRACKER_STATUS_OFFSET);
	/* make sure register access in order */
	wmb();
}

static void init_pmu_data(void)
{
	int i = 0;

	for (i = 0; i < num_possible_cpus(); i++) {
		per_cpu(cpu_last_inst_spec, i) = 0;
		per_cpu(cpu_last_cycle, i) = 0;
		per_cpu(cpu_last_l3dc, i) = 0;
	}
	set_pmu_enable(1);
	pmu_init = 1;
}

static void exit_pmu_data(void)
{
	pmu_init = 0;
	set_pmu_enable(0);
}

#define PMU_UPDATE_LOCK_OFFSET	0x12F0
#define PMU_IDX_CNT_OFFSET	0x12EC
u64 get_cpu_pmu(int cpu, u32 offset)
{
	u64 count = 0;

	if (IS_ERR_OR_NULL((void *)csram_base))
		return count;

	count = __raw_readl(csram_base + offset + (cpu * 0x4));
	return count;
}
EXPORT_SYMBOL_GPL(get_cpu_pmu);

/* 3GHz * 4ms * 10 IPC * 3 chances */
#define MAX_PMU_VALUE	360000000
#define DEBUG_BIT	2
#define DEFINE_GET_CUR_CPU_PMU_FUNC(_name, _pmu_offset)						\
	u64 get_cur_cpu_##_name(int cpu)							\
	{											\
		u64 cur = 0, res = 0;								\
		int retry = 3;									\
												\
		if (cpu >= nr_cpu_ids || !pmu_init)						\
			return res;								\
												\
		do {										\
			cur = get_cpu_pmu(cpu, _pmu_offset);					\
			/* invalid counter */							\
			if (cur == 0 || cur == 0xDEADDEAD)					\
				return 0;							\
												\
			/* handle overflow case */						\
			if (cur < per_cpu(cpu_last_##_name, cpu))				\
				res = ((u64)0xffffffff -					\
					per_cpu(cpu_last_##_name, cpu) + (0x7fffffff & cur));	\
			else									\
				res = per_cpu(cpu_last_##_name, cpu) == 0 ?			\
					0 : cur - per_cpu(cpu_last_##_name, cpu);		\
			--retry;								\
		} while (res > MAX_PMU_VALUE && retry > 0);					\
												\
		if (res > MAX_PMU_VALUE && retry == 0) {					\
			if (support_cpu_pmu & DEBUG_BIT)					\
				trace_cpu_pmu_debug(cpu, "_" #_name ":", 0, cur, res);		\
			return 0;								\
		}										\
												\
		per_cpu(cpu_last_##_name, cpu) = cur;						\
		if (support_cpu_pmu & DEBUG_BIT)						\
			trace_cpu_pmu_debug(cpu, "_" #_name ":", 1, cur, res);			\
		return res;									\
	}

DEFINE_GET_CUR_CPU_PMU_FUNC(inst_spec, CPU_INST_SPEC_OFFSET);
DEFINE_GET_CUR_CPU_PMU_FUNC(cycle, CPU_IDX_CYCLES_OFFSET);
DEFINE_GET_CUR_CPU_PMU_FUNC(l3dc, CPU_L3DC_OFFSET);

#define OFFS_DVFS_CUR_OPP_S	0x98
#define OFFS_MCUPM_CUR_OPP_S	0x544
#define OFFS_MCUPM_CUR_FREQ_S	0x11e0		//gearless freq

static unsigned int cpudvfs_get_cur_freq(int cluster_id, bool is_mcupm)
{
	u32 val = 0;
	u32 offset = 0;
	struct ppm_data *p = &cluster_ppm_info[cluster_id];

	if (IS_ERR_OR_NULL((void *)csram_base))
		return 0;

	if (is_gearless_support())
		offset = OFFS_MCUPM_CUR_FREQ_S;
	else
		offset = OFFS_MCUPM_CUR_OPP_S;

	if (is_mcupm)
		val = __raw_readl(csram_base +
				(offset + (cluster_id * 0x4)));
	else
		val = __raw_readl(csram_base +
				(OFFS_DVFS_CUR_OPP_S + (cluster_id * 0x120)));

	if (is_gearless_support())
		return val;

	if (p->init && val < p->opp_nr)
		return p->dvfs_tbl[val].frequency;

	return 0;
}

#define DSU_VOLT_2_CLUSTER	0x514
#define DSU_FREQ_2_CLUSTER	0x11e8
#define DSU_VOLT_3_CLUSTER	0x51c
#define DSU_FREQ_3_CLUSTER	0x11ec
#define MCUPM_OFFSET_BASE	0x133c

unsigned int csram_read(unsigned int offs)
{
	if (IS_ERR_OR_NULL((void *)csram_base))
		return 0;
	return __raw_readl(csram_base + (offs));
}

int perf_tracker_enable(int on)
{
	mutex_lock(&perf_ctl_mutex);
	perf_tracker_on = on;
	mutex_unlock(&perf_ctl_mutex);

	return (perf_tracker_on == on) ? 0 : -1;
}
EXPORT_SYMBOL_GPL(perf_tracker_enable);

static inline u32 cpu_stall_ratio(int cpu)
{
#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	return qos_sram_read(CM_STALL_RATIO_ID_0 + cpu);
#else
	return 0;
#endif
}

static inline void format_sbin_data(char *buf, u32 size, u32 *sbin_data, u32 lens)
{
	char *ptr = buf;
	char *buffer_end = buf + size;
	int i;

	ptr += snprintf(ptr, buffer_end - ptr, "ARRAY[");
	for (i = 0; i < lens; i++) {
		ptr += snprintf(ptr, buffer_end - ptr, "%02x, %02x, %02x, %02x, ",
				(*(sbin_data+i)) & 0xff, (*(sbin_data+i) >> 8) & 0xff,
				(*(sbin_data+i) >> 16) & 0xff, (*(sbin_data+i) >> 24) & 0xff);
	}
	ptr -= 2;
	ptr += snprintf(ptr, buffer_end - ptr, "]");
}

enum {
	SBIN_BW_RECORD		= 1U << 0,
	SBIN_DSU_RECORD		= 1U << 1,
	SBIN_MCUPM_RECORD	= 1U << 2,
	SBIN_PMU_RECORD		= 1U << 3,
};

#define K(x) ((x) << (PAGE_SHIFT - 10))
#define max_cpus 8
#define bw_hist_nums 8
#define bw_record_nums 32
#define dsu_record_nums 2
#define mcupm_record_nums 9
/* inst-spec, l3dc, cpu-cycles */
#define CPU_PMU_NUMS  (3 * max_cpus)
#define PRINT_BUFFER_SIZE ((bw_record_nums+dsu_record_nums+mcupm_record_nums+CPU_PMU_NUMS) * 16 + 8)

#define for_each_cpu_get_pmu(cpu, _pmu)						\
	do {									\
		for ((cpu) = 0; (cpu) < max_cpus; (cpu)++)			\
			sbin_data[sbin_lens + cpu] =				\
					(u32)get_cur_cpu_##_pmu(cpu);		\
		sbin_lens += max_cpus;						\
	} while (0)

void perf_tracker(u64 wallclock,
		  bool hit_long_check)
{
	long mm_available = 0, mm_free = 0;
	int dram_rate = 0;
	struct mtk_btag_mictx_iostat_struct *iostat_ptr = &iostat;
	int bw_c = 0, bw_g = 0, bw_mm = 0, bw_total = 0, bw_idx = 0xFFFF;
	u32 bw_record = 0;
	u32 sbin_data[bw_record_nums+dsu_record_nums+mcupm_record_nums+CPU_PMU_NUMS] = {0};
	int sbin_lens = 0;
	char sbin_data_print[PRINT_BUFFER_SIZE] = {0};
	u32 sbin_data_ctl = 0;
	u32 dsu_v = 0, dsu_f = 0;
	int vcore_uv = 0;
	int i;
	int stall[max_cpus] = {0};
	unsigned int sched_freq[3] = {0};
	unsigned int cpu_mcupm_freq[3] = {0};
	int cid;

	if (!perf_tracker_on)
		return;

	/* dram freq */
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
	dram_rate = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_KHZ);
	dram_rate = dram_rate / 1000;
	/* vcore  */
	vcore_uv = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_UV);
#endif

#if IS_ENABLED(CONFIG_MTK_DRAMC)
	if (dram_rate <= 0)
		dram_rate = mtk_dramc_get_data_rate();
#endif

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	/* emi */
	bw_c  = qos_sram_read(QOS_DEBUG_1);
	bw_g  = qos_sram_read(QOS_DEBUG_2);
	bw_mm = qos_sram_read(QOS_DEBUG_3);
	bw_total = qos_sram_read(QOS_DEBUG_0);

	/* emi history */
	bw_idx = qos_rec_get_hist_idx();
	if (bw_idx != 0xFFFF) {
		for (bw_record = 0; bw_record < bw_record_nums; bw_record += 8) {
			/* occupied bw history */
			sbin_data[bw_record]   = qos_rec_get_hist_bw(bw_idx, 0);
			sbin_data[bw_record+1] = qos_rec_get_hist_bw(bw_idx, 1);
			sbin_data[bw_record+2] = qos_rec_get_hist_bw(bw_idx, 2);
			sbin_data[bw_record+3] = qos_rec_get_hist_bw(bw_idx, 3);
			/* data bw history */
			sbin_data[bw_record+4] = qos_rec_get_hist_data_bw(bw_idx, 0);
			sbin_data[bw_record+5] = qos_rec_get_hist_data_bw(bw_idx, 1);
			sbin_data[bw_record+6] = qos_rec_get_hist_data_bw(bw_idx, 2);
			sbin_data[bw_record+7] = qos_rec_get_hist_data_bw(bw_idx, 3);

			bw_idx -= 1;
			if (bw_idx < 0)
				bw_idx = bw_idx + bw_hist_nums;
		}
	}
#endif
	sbin_lens += bw_record_nums;
	sbin_data_ctl |= SBIN_BW_RECORD;
	/* dsu */
	if (cluster_nr == 2) {
		dsu_v = csram_read(DSU_VOLT_2_CLUSTER);
		dsu_f = csram_read(DSU_FREQ_2_CLUSTER);
	} else if (cluster_nr == 3) {
		dsu_v = csram_read(DSU_VOLT_3_CLUSTER);
		dsu_f = csram_read(DSU_FREQ_3_CLUSTER);
	}
	sbin_data[sbin_lens] = dsu_v;
	sbin_data[sbin_lens+1] = dsu_f;
	sbin_lens += dsu_record_nums;
	sbin_data_ctl |= SBIN_DSU_RECORD;

	/* mcupm freq */
	if (mcupm_freq_enable) {
		for (i = 0; i < mcupm_record_nums; i++)
			sbin_data[sbin_lens+i] = csram_read(
				MCUPM_OFFSET_BASE+i*4);

		sbin_lens += mcupm_record_nums;
		sbin_data_ctl |= SBIN_MCUPM_RECORD;
	}

	if (support_cpu_pmu) {
		/* get pmu */
		for_each_cpu_get_pmu(i, inst_spec);
		for_each_cpu_get_pmu(i, cycle);
		for_each_cpu_get_pmu(i, l3dc);
		sbin_data_ctl |= SBIN_PMU_RECORD;
	}

	format_sbin_data(sbin_data_print, sizeof(sbin_data_print), sbin_data, sbin_lens);
	trace_perf_index_sbin(sbin_data_print, sbin_lens, sbin_data_ctl);

	/* trace for short bin */
	/* sched: cpu freq */
	for (cid = 0; cid < cluster_nr; cid++) {
		sched_freq[cid] = cpudvfs_get_cur_freq(cid, false);
		cpu_mcupm_freq[cid] = cpudvfs_get_cur_freq(cid, true);
	}

	/* trace for short msg */
	trace_perf_index_s(
			sched_freq[0], sched_freq[1], sched_freq[2],
			dram_rate, bw_c, bw_g, bw_mm, bw_total,
			vcore_uv, cpu_mcupm_freq[0], cpu_mcupm_freq[1], cpu_mcupm_freq[2]);

	if (!hit_long_check)
		return;

	/* free mem */
	mm_free = global_zone_page_state(NR_FREE_PAGES);
	mm_available = si_mem_available();

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	/* If getting I/O stat fail, fallback to zero value. */
	if (mtk_btag_mictx_get_data(ufs_mictx_id, iostat_ptr))
		memset(iostat_ptr, 0,
			sizeof(struct mtk_btag_mictx_iostat_struct));
#endif

	/* cpu stall ratio */
	for (i = 0; i < nr_cpu_ids || i < max_cpus; i++)
		stall[i] = cpu_stall_ratio(i);

	/* trace for long msg */
	trace_perf_index_l(
			K(mm_free),
			K(mm_available),
			iostat_ptr,
			stall
			);

}

/*
 * make perf tracker on
 * /sys/devices/system/cpu/perf/enable
 * 1: on
 * 0: off
 */
static ssize_t show_perf_enable(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "enable = %d\n",
			perf_tracker_on);
	return len;
}

static ssize_t store_perf_enable(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t count)
{
	int val = 0;

	mutex_lock(&perf_ctl_mutex);

	if (sscanf(buf, "%iu", &val) != 0) {
		val = (val > 0) ? 1 : 0;

		perf_tracker_on = val;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
		mtk_btag_mictx_enable(&ufs_mictx_id, val);
#endif
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
		// GPU PMU Recording
		if (val == 1 && gpu_pmu_enable && !is_gpu_pmu_worked) {
			mtk_ltr_gpu_pmu_start(gpu_pmu_period);
			is_gpu_pmu_worked = 1;
		} else if (val == 0 && is_gpu_pmu_worked) {
			mtk_ltr_gpu_pmu_stop();
			is_gpu_pmu_worked = 0;
		}
#endif
		/* do something after on/off perf_tracker */
		if (perf_tracker_on) {
			insert_freq_qos_hook();
			init_pmu_data();
		} else {
			remove_freq_qos_hook();
			exit_pmu_data();
		}
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static void fuel_gauge_handler(struct work_struct *work)
{
	int curr, volt, cap;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!fuel_gauge_enable)
		return;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	volt = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	cap = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	curr = val.intval;

	curr = curr/1000;
	volt = volt/1000;
	trace_fuel_gauge(curr, volt, cap);
	queue_delayed_work(system_power_efficient_wq,
			&fuel_gauge, msecs_to_jiffies(fuel_gauge_delay));
}

static ssize_t show_fuel_gauge_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "fuel_gauge_enable = %u\n",
			fuel_gauge_enable);
	return len;
}

static ssize_t store_fuel_gauge_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		fuel_gauge_enable = (tmp > 0) ? 1 : 0;

	if (fuel_gauge_enable) {
		/* default delay 8ms */
		fuel_gauge_delay = (fuel_gauge_delay > 0) ?
				fuel_gauge_delay : 8;

		/* start fuel gauge tracking */
		queue_delayed_work(system_power_efficient_wq,
				&fuel_gauge,
				msecs_to_jiffies(fuel_gauge_delay));
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static ssize_t show_fuel_gauge_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "fuel_gauge_period = %u(ms)\n",
				fuel_gauge_delay);
	return len;
}

static ssize_t store_fuel_gauge_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		if (tmp > 0) /* ms */
			fuel_gauge_delay = tmp;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}


#if IS_ENABLED(CONFIG_MTK_CHARGER)
static void charger_handler(struct work_struct *work)
{
	int volt, temp;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!charger_enable)
		return;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
	volt = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	temp = val.intval;
	trace_charger(temp, volt);

	queue_delayed_work(system_power_efficient_wq,
			&charger, msecs_to_jiffies(charger_delay));
}

static ssize_t show_charger_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "charger_enable = %u\n",
			charger_enable);
	return len;
}

static ssize_t store_charger_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		charger_enable = (tmp > 0) ? 1 : 0;

	if (charger_enable) {
		/* default delay 1000ms */
		charger_delay = (charger_delay > 0) ?
				charger_delay : 1000;

		queue_delayed_work(system_power_efficient_wq,
				&charger,
				msecs_to_jiffies(charger_delay));
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static ssize_t show_charger_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "charger_period = %u(ms)\n",
				charger_delay);
	return len;
}

static ssize_t store_charger_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		if (tmp > 0) /* ms */
			charger_delay = tmp;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
static ssize_t show_gpu_pmu_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "gpu_pmu_enable = %u is_working = %u\n",
		gpu_pmu_enable, is_gpu_pmu_worked);
	return len;
}

static ssize_t store_gpu_pmu_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &gpu_pmu_enable) == 0)
		gpu_pmu_enable = (gpu_pmu_enable > 0) ? 1 : 0;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static ssize_t show_gpu_pmu_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "gpu_pmu_period = %u\n",
			gpu_pmu_period);
	return len;
}

static ssize_t store_gpu_pmu_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &gpu_pmu_period) == 0) {
		if (gpu_pmu_period < 1000000) // 1ms
			gpu_pmu_period = 1000000;
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}
#endif

static ssize_t show_mcupm_freq_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "mcupm_freq_enable = %u\n", mcupm_freq_enable);
	return len;
}

static ssize_t store_mcupm_freq_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &mcupm_freq_enable) == 0)
		mcupm_freq_enable = (mcupm_freq_enable > 0) ? 1 : 0;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static ssize_t show_cpu_pmu_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "cpu_pmu_enable = %u\n", support_cpu_pmu);
	return len;
}

#define SUPPORT_BIT	3
static ssize_t store_cpu_pmu_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &support_cpu_pmu) == 0)
		support_cpu_pmu &= SUPPORT_BIT;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

struct kobj_attribute perf_tracker_enable_attr =
__ATTR(enable, 0600, show_perf_enable, store_perf_enable);
struct kobj_attribute perf_fuel_gauge_enable_attr =
__ATTR(fuel_gauge_enable, 0600,	show_fuel_gauge_enable, store_fuel_gauge_enable);
struct kobj_attribute perf_fuel_gauge_period_attr =
__ATTR(fuel_gauge_period, 0600,	show_fuel_gauge_period, store_fuel_gauge_period);

#if IS_ENABLED(CONFIG_MTK_CHARGER)
struct kobj_attribute perf_charger_enable_attr =
__ATTR(charger_enable, 0600, show_charger_enable, store_charger_enable);
struct kobj_attribute perf_charger_period_attr =
__ATTR(charger_period, 0600, show_charger_period, store_charger_period);
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
struct kobj_attribute perf_gpu_pmu_enable_attr =
__ATTR(gpu_pmu_enable, 0600, show_gpu_pmu_enable, store_gpu_pmu_enable);
struct kobj_attribute perf_gpu_pmu_period_attr =
__ATTR(gpu_pmu_period, 0600, show_gpu_pmu_period, store_gpu_pmu_period);
#endif

struct kobj_attribute perf_mcupm_freq_enable_attr =
__ATTR(mcupm_freq_enable, 0600, show_mcupm_freq_enable, store_mcupm_freq_enable);

struct kobj_attribute perf_cpu_pmu_enable_attr =
__ATTR(cpu_pmu_enable, 0600, show_cpu_pmu_enable, store_cpu_pmu_enable);
