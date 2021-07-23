/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <trace/events/sched.h>
#include <trace/events/mtk_events.h>

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif

#include <linux/mm.h>
#include <linux/swap.h>
#include <mt-plat/mtk_blocktag.h>
#include <helio-dvfsrc.h>
#include <linux/jiffies.h>

#include <linux/module.h>
#ifndef __CHECKER__
#define CREATE_TRACE_POINTS
#include "perf_tracker_trace.h"
#endif

#ifdef CONFIG_MTK_QOS_FRAMEWORK
#include <mtk_qos_sram.h>
#endif

#ifdef QOS_SHARE_SUPPORT
#include <mtk_qos_share.h>
#endif

#ifdef CONFIG_MTK_PERF_OBSERVER
#include <mt-plat/mtk_perfobserver.h>
#endif

#include <mt-plat/perf_tracker.h>
#include <linux/arch_topology.h>
#include <perf_tracker_internal.h>

#ifdef CONFIG_MTK_SKB_OWNER
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <net/tcp.h>
#endif

#ifdef CONFIG_MTK_GAUGE_VERSION
#include <mt-plat/mtk_battery.h>

static void fuel_gauge_handler(struct work_struct *work);

static int fuel_gauge_enable;
static int fuel_gauge_delay; /* ms */
static DECLARE_DELAYED_WORK(fuel_gauge, fuel_gauge_handler);
#endif

static int perf_tracker_on, perf_tracker_init;
static DEFINE_MUTEX(perf_ctl_mutex);
static int cluster_nr = -1;

#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
unsigned int gpu_pmu_enable;
unsigned int is_gpu_pmu_worked;
unsigned int gpu_pmu_period = 8000000; //8ms

void (*MTKGPUPower_model_start_symbol)(unsigned int interval_ns);
void (*MTKGPUPower_model_stop_symbol)(void);
void (*MTKGPUPower_model_suspend_symbol)(void);
void (*MTKGPUPower_model_resume_symbol)(void);
#endif

#ifdef CONFIG_MTK_SKB_OWNER
static int net_pkt_trace_enable;
#endif

#if !defined(CONFIG_MTK_BLOCK_TAG) || !defined(MTK_BTAG_FEATURE_MICTX_IOSTAT)
struct mtk_btag_mictx_iostat_struct {
	__u64 duration;  /* duration time for below performance data (ns) */
	__u32 tp_req_r;  /* throughput (per-request): read  (KB/s) */
	__u32 tp_req_w;  /* throughput (per-request): write (KB/s) */
	__u32 tp_all_r;  /* throughput (overlapped) : read  (KB/s) */
	__u32 tp_all_w;  /* throughput (overlapped) : write (KB/s) */
	__u32 reqsize_r; /* request size : read  (Bytes) */
	__u32 reqsize_w; /* request size : write (Bytes) */
	__u32 reqcnt_r;  /* request count: read */
	__u32 reqcnt_w;  /* request count: write */
	__u16 wl;        /* storage device workload (%) */
	__u16 q_depth;   /* storage cmdq queue depth */
};
#endif

#ifdef CONFIG_MTK_BLOCK_TAG
static struct mtk_btag_mictx_iostat_struct iostatptr;

void  __attribute__((weak)) mtk_btag_mictx_enable(int enable) {}

int __attribute__((weak)) mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_iostat_struct *io)
{
	return -1;
}
#endif

#ifdef CONFIG_MTK_GAUGE_VERSION
static void fuel_gauge_handler(struct work_struct *work)
{
	int curr, volt;

	if (!fuel_gauge_enable)
		return;

	/* read current(mA) and valtage(mV) from pmic */
	curr = battery_get_bat_current();
	volt = battery_get_bat_voltage();

	trace_fuel_gauge(curr, volt);

	queue_delayed_work(system_power_efficient_wq,
			&fuel_gauge, msecs_to_jiffies(fuel_gauge_delay));
}
#endif

static void init_function_symbols(void)
{
#define _FUNC_SYMBOL_GET(_func_name_) \
	do { \
		_func_name_##_symbol = (void *)symbol_get(_func_name_); \
		if (_func_name_##_symbol == NULL) { \
			pr_debug("Symbol : %s is not found!\n", #_func_name_); \
		} \
	} while (0)
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
	_FUNC_SYMBOL_GET(MTKGPUPower_model_start);
	_FUNC_SYMBOL_GET(MTKGPUPower_model_stop);
	_FUNC_SYMBOL_GET(MTKGPUPower_model_suspend);
	_FUNC_SYMBOL_GET(MTKGPUPower_model_resume);
#endif
}

int perf_tracker_enable(int val)
{
	mutex_lock(&perf_ctl_mutex);

	val = (val > 0) ? 1 : 0;

	perf_tracker_on = val;
#ifdef CONFIG_MTK_BLOCK_TAG
	mtk_btag_mictx_enable(val);
#endif

#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
	// GPU PMU Recording
	if (val == 1 && gpu_pmu_enable && !is_gpu_pmu_worked) {
		if (MTKGPUPower_model_start_symbol)
			MTKGPUPower_model_start_symbol(gpu_pmu_period);
		is_gpu_pmu_worked = 1;
	} else if (val == 0 && is_gpu_pmu_worked) {
		if (MTKGPUPower_model_stop_symbol)
			MTKGPUPower_model_stop_symbol();
		is_gpu_pmu_worked = 0;
	}
#endif

	mutex_unlock(&perf_ctl_mutex);

	return (perf_tracker_on == val) ? 0 : -1;
}

unsigned int __attribute__((weak)) get_dram_data_rate(void)
{
	return 0;
}

u32 __attribute__((weak)) qos_sram_read(u32 offset)
{
	return 0;
}

int __attribute__((weak)) get_cur_vcore_uv()
{
	return 0;
}

int __attribute__((weak)) get_cur_ddr_khz()
{
	return 0;
}

unsigned int __attribute__((weak)) qos_rec_get_hist_bw(unsigned int idx, unsigned int type)
{
	return 0;
}

unsigned int __attribute__((weak)) qos_rec_get_hist_data_bw(unsigned int idx, unsigned int type)
{
	return 0;
}

unsigned int __attribute__((weak)) qos_rec_get_hist_idx(void)
{
	return 0xFFFF;
}


static inline u32 cpu_stall_ratio(int cpu)
{
#ifdef CM_STALL_RATIO_OFFSET
	return qos_sram_read(CM_STALL_RATIO_OFFSET + cpu * 4);
#else
	return 0;
#endif
}

#define K(x) ((x) << (PAGE_SHIFT - 10))
#define max_cpus 8
#define bw_hist_nums 8
#define bw_record_nums 32

void __perf_tracker(u64 wallclock,
		    long mm_available,
		    long mm_free)
{
	int dram_rate = 0;
#ifdef CONFIG_MTK_BLOCK_TAG
	struct mtk_btag_mictx_iostat_struct *iostat = &iostatptr;
#endif
	int bw_c = 0, bw_g = 0, bw_mm = 0, bw_total = 0, bw_idx = 0;
	u32 bw_record = 0, bw_data[bw_record_nums] = {0};
	int vcore_uv = 0;
	int i;
	int stall[max_cpus] = {0};
	unsigned int sched_freq[3] = {0};
	int cid;

#ifdef CONFIG_MTK_PERF_OBSERVER
	pob_qos_tracker(wallclock);
#endif

	if (!perf_tracker_on || !perf_tracker_init)
		return;

	/* dram freq */
	dram_rate = get_cur_ddr_khz();
	dram_rate = dram_rate / 1000;

	if (dram_rate <= 0)
		dram_rate = get_dram_data_rate();

	/* vcore  */
	vcore_uv = get_cur_vcore_uv();

#ifdef CONFIG_MTK_QOS_FRAMEWORK
	/* emi */
	bw_c  = qos_sram_read(QOS_DEBUG_1);
	bw_g  = qos_sram_read(QOS_DEBUG_2);
	bw_mm = qos_sram_read(QOS_DEBUG_3);
	bw_total = qos_sram_read(QOS_DEBUG_0);
#endif
	/* emi history */
	bw_idx = qos_rec_get_hist_idx();
	if (bw_idx != 0xFFFF) {
		for (bw_record = 0; bw_record < bw_record_nums; bw_record += 8) {
			/* occupied bw history */
			bw_data[bw_record]   = qos_rec_get_hist_bw(bw_idx, 0);
			bw_data[bw_record+1] = qos_rec_get_hist_bw(bw_idx, 1);
			bw_data[bw_record+2] = qos_rec_get_hist_bw(bw_idx, 2);
			bw_data[bw_record+3] = qos_rec_get_hist_bw(bw_idx, 3);
			/* data bw history */
			bw_data[bw_record+4] = qos_rec_get_hist_data_bw(bw_idx, 0);
			bw_data[bw_record+5] = qos_rec_get_hist_data_bw(bw_idx, 1);
			bw_data[bw_record+6] = qos_rec_get_hist_data_bw(bw_idx, 2);
			bw_data[bw_record+7] = qos_rec_get_hist_data_bw(bw_idx, 3);

			bw_idx -= 1;
			if (bw_idx < 0)
				bw_idx = bw_idx + bw_hist_nums;
		}
		/* trace for short bin */
		trace_perf_index_sbin(bw_data, bw_record);
	}

	/* sched: cpu freq */
	for (cid = 0; cid < cluster_nr; cid++)
		sched_freq[cid] = mt_cpufreq_get_cur_freq(cid);

	/* trace for short msg */
	trace_perf_index_s(
			sched_freq[0], sched_freq[1], sched_freq[2],
			dram_rate, bw_c, bw_g, bw_mm, bw_total,
			vcore_uv);

	if (!hit_long_check())
		return;

	/* free mem */
	if (mm_free == -1) {
		mm_free = global_zone_page_state(NR_FREE_PAGES);
		mm_available = si_mem_available();
	}

#ifdef CONFIG_MTK_BLOCK_TAG
	/* IO stat */
	if (mtk_btag_mictx_get_data(iostat))
		memset(iostat, 0, sizeof(struct mtk_btag_mictx_iostat_struct));
#endif

	/* cpu stall ratio */
	for (i = 0; i < nr_cpu_ids || i < max_cpus; i++)
		stall[i] = cpu_stall_ratio(i);

	/* trace for long msg */
	trace_perf_index_l(
			K(mm_free),
			K(mm_available),
#ifdef CONFIG_MTK_BLOCK_TAG
			iostat->wl,
			iostat->tp_req_r, iostat->tp_all_r,
			iostat->reqsize_r, iostat->reqcnt_r,
			iostat->tp_req_w, iostat->tp_all_w,
			iostat->reqsize_w, iostat->reqcnt_w,
			iostat->duration, iostat->q_depth,
#else
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
			stall);
}

#ifdef CONFIG_MTK_GAUGE_VERSION
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
#endif

#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
void perf_update_gpu_counter(unsigned int gpu_data[], unsigned int len)
{
	trace_perf_index_gpu(gpu_data, len);
}
EXPORT_SYMBOL(perf_update_gpu_counter);

static ssize_t show_gpu_pmu_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	uint is_inited = 0;

	if (MTKGPUPower_model_start_symbol)
		is_inited = 1;

	len += snprintf(buf, max_len, "gpu_pmu_enable = %u, is_inited = %u\n",
			gpu_pmu_enable, is_inited);
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

#ifdef CONFIG_MTK_SKB_OWNER
void perf_update_tcp_rtt(struct sock *sk, long seq_rtt_us)
{
	if (net_pkt_trace_enable && sk)
		trace_tcp_rtt(sk->sk_uid.val, ntohs(sk->sk_dport),
			sk->sk_num, sk->sk_family,
			ntohl(sk->sk_daddr), seq_rtt_us);
}

void perf_net_pkt_trace(struct sock *sk, struct sk_buff *skb, int copied)
{
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = NULL;
	struct udphdr *udph = NULL;
	int len, saddr, sport, dport;
	int seq = 0;

	if (net_pkt_trace_enable && iph->version == 4 && sk) {
		if (iph->protocol == 6) { /* TCP */
			tcph = (struct tcphdr *)((char *)iph+(iph->ihl*4));
			sport = ntohs(tcph->source);
			dport = ntohs(tcph->dest);
			seq = TCP_SKB_CB(skb)->seq;
		} else if (iph->protocol == 17) { /* UDP */
			udph = (struct udphdr *)((char *)iph+(iph->ihl*4));
			sport = ntohs(udph->source);
			dport = ntohs(udph->dest);
		} else {
			return;
		}

		len = ntohs(iph->tot_len);
		saddr = ntohl(iph->saddr);
		/* tracing for packet */
		trace_socket_packet(sk->sk_uid.val, iph->protocol,
				dport, sport, saddr, len, copied, seq);
	}
}
#endif

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
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (sscanf(buf, "%iu", &val) != 0)
		perf_tracker_enable(val);

	return count;
}

#ifdef CONFIG_MTK_SKB_OWNER
static ssize_t show_perf_net_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len;
	unsigned int max_len = 4096;

	len = snprintf(buf, max_len, "enable = %d\n",
			net_pkt_trace_enable);
	return len;
}

static ssize_t store_perf_net_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val;

	mutex_lock(&perf_ctl_mutex);

	if (sscanf(buf, "%iu", &val) != 0) {
		val = (val > 0) ? 1 : 0;

		net_pkt_trace_enable = val;
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}
#endif

static struct kobj_attribute perf_enable_attr =
__ATTR(enable, 0600, show_perf_enable, store_perf_enable);
#ifdef CONFIG_MTK_GAUGE_VERSION
static struct kobj_attribute perf_fuel_gauge_enable_attr =
__ATTR(fuel_gauge_enable, 0600,
	show_fuel_gauge_enable, store_fuel_gauge_enable);
static struct kobj_attribute perf_fuel_gauge_period_attr =
__ATTR(fuel_gauge_period, 0600,
	show_fuel_gauge_period, store_fuel_gauge_period);
#endif
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
static struct kobj_attribute perf_gpu_pmu_enable_attr =
__ATTR(gpu_pmu_enable, 0600, show_gpu_pmu_enable, store_gpu_pmu_enable);
static struct kobj_attribute perf_gpu_pmu_period_attr =
__ATTR(gpu_pmu_period, 0600, show_gpu_pmu_period, store_gpu_pmu_period);
#endif
#ifdef CONFIG_MTK_SKB_OWNER
static struct kobj_attribute perf_net_enable_attr =
__ATTR(net_pkt_enable, 0600, show_perf_net_enable, store_perf_net_enable);
#endif

static struct attribute *perf_attrs[] = {
	&perf_enable_attr.attr,
#ifdef CONFIG_MTK_GAUGE_VERSION
	&perf_fuel_gauge_enable_attr.attr,
	&perf_fuel_gauge_period_attr.attr,
#endif
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
	&perf_gpu_pmu_enable_attr.attr,
	&perf_gpu_pmu_period_attr.attr,
#endif
#ifdef CONFIG_MTK_SKB_OWNER
	&perf_net_enable_attr.attr,
#endif
	NULL,
};

static struct attribute_group perf_attr_group = {
	.attrs = perf_attrs,
};

static int init_perf_tracker(void)
{
	int ret = 0;
	struct kobject *kobj = NULL;

	perf_tracker_init = 1;
	cluster_nr = arch_get_nr_clusters();

	if (unlikely(cluster_nr <= 0 || cluster_nr > 3))
		cluster_nr = 3;

	kobj = kobject_create_and_add("perf", &cpu_subsys.dev_root->kobj);

	if (kobj) {
		ret = sysfs_create_group(kobj, &perf_attr_group);
		if (ret)
			kobject_put(kobj);
		else
			kobject_uevent(kobj, KOBJ_ADD);
	}

	init_function_symbols();

	return 0;
}
late_initcall_sync(init_perf_tracker);
