/*
 * Copyright (C) 2020 MediaTek Inc.
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

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <mtk_swpm_common.h>
#include <mtk_swpm_platform.h>
#include <mtk_swpm_sp_platform.h>
#include <mtk_swpm_sp_interface.h>

#define SWPM_INTERNAL_TEST (0)
#define DEFAULT_UPDATE_MS (10000)
#define CORE_SRAM (share_idx_ref_ext->core_idx_ext)
#define DDR_SRAM (share_idx_ref_ext->mem_idx_ext)
#define SUSPEND_SRAM (share_idx_ref_ext->suspend)

#define OPP_FREQ_TO_DDR(x) \
	((x != 1866) ? (x * 2) : ((x * 2) + 1))

static struct timer_list swpm_sp_timer;
static DEFINE_SPINLOCK(swpm_sp_spinlock);

/* share sram for extension index */
static struct share_index_ext *share_idx_ref_ext;
static struct share_ctrl_ext *share_idx_ctrl_ext;

static unsigned int update_interval_ms = DEFAULT_UPDATE_MS;

/* core voltage time distribution */
static struct vol_duration core_vol_duration[NR_CORE_VOLT];
/* core ip stat with time distribution */
static struct ip_stats core_ip_stats[NR_CORE_IP];

/* ddr freq in active time distribution */
static struct ddr_act_times ddr_act_duration[NR_DDR_FREQ];
/* ddr freq in active time distribution */
static struct ddr_sr_pd_times ddr_sr_pd_duration;
/* ddr ip stat with bw/freq distribution */
static struct ddr_ip_bc_stats ddr_ip_stats[NR_DDR_BC_IP];

struct suspend_time suspend_time;
static uint64_t total_suspend_us;

/* core ip (cam, img1, img2, ipe, disp venc, vdec, gpu, scp, adsp */
#define MAX_IP_NAME_LENGTH (16)
static char core_ip_str[NR_CORE_IP][MAX_IP_NAME_LENGTH] = {
	"CAM", "IMG1", "IMG2", "IPE", "DISP",
	"VENC", "VDEC", "SCP",
};
/* ddr bw ip (total r/total w/cpu/gpu/mm/md) */
static char ddr_bc_ip_str[NR_DDR_BC_IP][MAX_IP_NAME_LENGTH] = {
	"TOTAL_R", "TOTAL_W", "CPU", "GPU", "MM", "OTHERS",
};

/* critical section function */
static void swpm_sp_internal_update(void)
{
	int i, j;
	struct core_ip_pwr_sta *core_ip_sta_ptr;
	struct mem_ip_bc *ddr_ip_bc_ptr;
	unsigned int word_L, word_H;

	if (share_idx_ctrl_ext->clear_flag)
		return;

	if (share_idx_ref_ext && share_idx_ctrl_ext) {
		for (i = 0; i < NR_CORE_VOLT; i++) {
			core_vol_duration[i].duration +=
				CORE_SRAM.acc_time[i] / 1000;
		}
		for (i = 0; i < NR_DDR_FREQ; i++) {
			ddr_act_duration[i].active_time +=
			DDR_SRAM.acc_time[i] / 1000;
		}
		ddr_sr_pd_duration.sr_time +=
			DDR_SRAM.acc_sr_time / 1000;
		ddr_sr_pd_duration.pd_time +=
			DDR_SRAM.acc_pd_time / 1000;
		for (i = 0; i < NR_CORE_IP; i++) {
			if (!core_ip_stats[i].vol_times)
				continue;
			core_ip_sta_ptr = &(CORE_SRAM.pwr_state[i]);
			for (j = 0; j < NR_CORE_VOLT; j++) {
				core_ip_stats[i].vol_times[j].active_time +=
				core_ip_sta_ptr->state[j][PMSR_ACTIVE] / 1000;
				core_ip_stats[i].vol_times[j].idle_time +=
				core_ip_sta_ptr->state[j][PMSR_IDLE] / 1000;
				core_ip_stats[i].vol_times[j].off_time +=
				core_ip_sta_ptr->state[j][PMSR_OFF] / 1000;
			}
		}
		for (i = 0; i < NR_DDR_BC_IP; i++) {
			if (!ddr_ip_stats[i].bc_stats)
				continue;
			ddr_ip_bc_ptr = &(DDR_SRAM.data[i]);
			for (j = 0; j < NR_DDR_FREQ; j++) {
				word_H = ddr_ip_bc_ptr->word_cnt_H[j];
				word_L = ddr_ip_bc_ptr->word_cnt_L[j];
				ddr_ip_stats[i].bc_stats[j].value +=
				(((uint64_t) word_H << 32) | word_L) * 8;
			}
		}
		suspend_time.time_H = SUSPEND_SRAM.time_H;
		suspend_time.time_L = SUSPEND_SRAM.time_L;
		total_suspend_us +=
			((uint64_t)suspend_time.time_H << 32) |
			suspend_time.time_L;

		share_idx_ctrl_ext->clear_flag = 1;
	}
}

static void swpm_sp_routine(unsigned long data)
{
	unsigned long flags;

	spin_lock_irqsave(&swpm_sp_spinlock, flags);
	swpm_sp_internal_update();
	spin_unlock_irqrestore(&swpm_sp_spinlock, flags);

	mod_timer(&swpm_sp_timer,
		  jiffies + msecs_to_jiffies(update_interval_ms));
}

static void swpm_sp_dispatcher(unsigned int type,
			       unsigned int val)
{
	switch (type) {
	case SYNC_DATA:
		/* do update */
		swpm_sp_routine(0);
		break;
	case SET_INTERVAL:
		/* set update interval */
		break;
	}
}

static int32_t swpm_ddr_act_times(int32_t freq_num,
			      struct ddr_act_times *ddr_times)
{
	unsigned long flags;

	if (ddr_times && freq_num == NR_DDR_FREQ) {
		spin_lock_irqsave(&swpm_sp_spinlock, flags);
		memcpy(ddr_times, ddr_act_duration,
		       sizeof(struct ddr_act_times) * NR_DDR_FREQ);
		spin_unlock_irqrestore(&swpm_sp_spinlock, flags);
	}
	return 0;
}
static int32_t swpm_ddr_sr_pd_times(struct ddr_sr_pd_times *ddr_times)
{
	unsigned long flags;

	if (ddr_times) {
		spin_lock_irqsave(&swpm_sp_spinlock, flags);
		memcpy(ddr_times, &ddr_sr_pd_duration,
		       sizeof(struct ddr_sr_pd_times));
		spin_unlock_irqrestore(&swpm_sp_spinlock, flags);
	}
	return 0;
}
static int32_t swpm_ddr_freq_data_ip_stats(int32_t data_ip_num,
					   int32_t freq_num,
					   void *stats)
{
	unsigned long flags;

	if (stats && data_ip_num == NR_DDR_BC_IP &&
	    freq_num == NR_DDR_FREQ) {
		spin_lock_irqsave(&swpm_sp_spinlock, flags);
		memcpy(stats, ddr_ip_stats,
		       sizeof(struct ddr_ip_bc_stats) * NR_DDR_BC_IP);
		spin_unlock_irqrestore(&swpm_sp_spinlock, flags);
	}
	return 0;
}
static int32_t swpm_vcore_ip_vol_stats(int32_t ip_num,
				       int32_t vol_num,
				       void *stats)
{
	unsigned long flags;

	if (stats && ip_num == NR_CORE_IP &&
	    vol_num == NR_CORE_VOLT) {
		spin_lock_irqsave(&swpm_sp_spinlock, flags);
		memcpy(stats, core_ip_stats,
		       sizeof(struct ip_stats) * NR_CORE_IP);
		spin_unlock_irqrestore(&swpm_sp_spinlock, flags);
	}
	return 0;
}
static int32_t swpm_vcore_vol_duration(int32_t vol_num,
				       struct vol_duration *duration)
{
	unsigned long flags;

	if (duration && vol_num == NR_CORE_VOLT) {
		spin_lock_irqsave(&swpm_sp_spinlock, flags);
		memcpy(duration, core_vol_duration,
		       sizeof(struct vol_duration) * NR_CORE_VOLT);
		spin_unlock_irqrestore(&swpm_sp_spinlock, flags);
	}
	return 0;
}
static int32_t swpm_plat_nums(enum swpm_num_type type)
{
	switch (type) {
	case DDR_DATA_IP:
		return NR_DDR_BC_IP;
	case DDR_FREQ:
		return NR_DDR_FREQ;
	case CORE_IP:
		return NR_CORE_IP;
	case CORE_VOL:
		return NR_CORE_VOLT;
	}
	return 0;
}

static struct swpm_internal_ops plat_ops = {
	.cmd = swpm_sp_dispatcher,
	.ddr_act_times_get = swpm_ddr_act_times,
	.ddr_sr_pd_times_get = swpm_ddr_sr_pd_times,
	.ddr_freq_data_ip_stats_get =
		swpm_ddr_freq_data_ip_stats,
	.vcore_ip_vol_stats_get =
		swpm_vcore_ip_vol_stats,
	.vcore_vol_duration_get =
		swpm_vcore_vol_duration,
	.num_get = swpm_plat_nums,
};

/* critical section function */
static void swpm_sp_timer_init(void)
{
	swpm_sp_timer.function = swpm_sp_routine;
	swpm_sp_timer.expires =
		jiffies + msecs_to_jiffies(update_interval_ms);
	swpm_sp_timer.data = 0;
	init_timer_deferrable(&swpm_sp_timer);
	add_timer(&swpm_sp_timer);
}

#if SWPM_INTERNAL_TEST
static int swpm_sp_test_proc_show(struct seq_file *m, void *v)
{
	int i, j;
	int32_t core_vol_num, core_ip_num;

	struct ip_stats *core_ip_stats_ptr;
	struct vol_duration *core_duration_ptr;

	core_vol_num = get_vcore_vol_num();
	core_ip_num = get_vcore_ip_num();

	core_duration_ptr =
	kmalloc_array(core_vol_num, sizeof(struct vol_duration), GFP_KERNEL);
	core_ip_stats_ptr =
	kmalloc_array(core_ip_num, sizeof(struct ip_stats), GFP_KERNEL);

	swpm_sp_dispatcher(SYNC_DATA, 0);

	get_vcore_vol_duration(core_vol_num, core_duration_ptr);
	get_vcore_ip_vol_stats(core_ip_num, core_vol_num,
			       core_ip_stats_ptr);

	seq_printf(m, "VCORE_VOL_NUM = %d\n", core_vol_num);
	seq_printf(m, "VCORE_IP_NUM = %d\n", core_ip_num);


	for (i = 0; i < core_vol_num; i++) {
		seq_printf(m, "VCORE %d mV : %lld ms\n",
			   core_duration_ptr[i].vol,
			   core_duration_ptr[i].duration);
	}
	for (i = 0; i < core_ip_num; i++) {
		seq_printf(m, "VCORE IP %s\n",
			   core_ip_stats_ptr[i].ip_name);
		for (j = 0; j < core_vol_num; j++) {
			seq_printf(m, "%d mV",
			core_ip_stats_ptr[i].vol_times[j].vol);
			seq_printf(m, "\t active_time : %lld ms",
			core_ip_stats_ptr[i].vol_times[j].active_time);
			seq_printf(m, "\t idle_time : %lld ms",
			core_ip_stats_ptr[i].vol_times[j].idle_time);
			seq_printf(m, "\t off_time : %lld ms\n",
			core_ip_stats_ptr[i].vol_times[j].off_time);
		}
	}
	kfree(core_ip_stats_ptr);
	kfree(core_duration_ptr);

	return 0;
}
PROC_FOPS_RO(swpm_sp_test);
#endif

static int swpm_sp_ddr_idx_proc_show(struct seq_file *m, void *v)
{
	int i, j;
	int32_t ddr_freq_num, ddr_bc_ip_num;

	struct ddr_act_times *ddr_act_times_ptr;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr;
	struct ddr_ip_bc_stats *ddr_ip_stats_ptr;

	ddr_freq_num = get_ddr_freq_num();
	ddr_bc_ip_num = get_ddr_data_ip_num();

	ddr_act_times_ptr =
	kmalloc_array(ddr_freq_num, sizeof(struct ddr_act_times), GFP_KERNEL);
	ddr_sr_pd_times_ptr =
	kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	ddr_ip_stats_ptr =
	kmalloc_array(ddr_bc_ip_num,
		sizeof(struct ddr_ip_bc_stats), GFP_KERNEL);

	swpm_sp_dispatcher(SYNC_DATA, 0);

	get_ddr_act_times(ddr_freq_num, ddr_act_times_ptr);
	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);
	get_ddr_freq_data_ip_stats(ddr_bc_ip_num,
				   ddr_freq_num,
				   ddr_ip_stats_ptr);

	seq_printf(m, "SR time(msec): %lld\n",
		   ddr_sr_pd_times_ptr->sr_time);
	seq_printf(m, "PD time(msec): %lld\n",
		   ddr_sr_pd_times_ptr->pd_time);

	for (i = 0; i < ddr_freq_num; i++) {
		seq_printf(m, "Freq %dMhz: ",
			   ddr_act_times_ptr[i].freq);
		seq_printf(m, "Time(msec):%lld ",
			   ddr_act_times_ptr[i].active_time);
		for (j = 0; j < ddr_bc_ip_num; j++) {
			seq_printf(m, "%llu/",
				ddr_ip_stats_ptr[j].bc_stats[i].value);
		}
		seq_putc(m, '\n');
	}
	kfree(ddr_act_times_ptr);
	kfree(ddr_sr_pd_times_ptr);
	kfree(ddr_ip_stats_ptr);

	return 0;
}
PROC_FOPS_RO(swpm_sp_ddr_idx);

static void swpm_sp_procfs_init(void)
{
#if SWPM_INTERNAL_TEST
	struct swpm_entry swpm_sp_test = PROC_ENTRY(swpm_sp_test);
#endif
	struct swpm_entry swpm_sp_ddr_idx = PROC_ENTRY(swpm_sp_ddr_idx);

#if SWPM_INTERNAL_TEST
	swpm_append_procfs(&swpm_sp_test);
#endif
	swpm_append_procfs(&swpm_sp_ddr_idx);
}

void swpm_sp_init(phys_addr_t ref_addr,
		  phys_addr_t ctrl_addr)
{
	int i, j;

	/* core_ip_stats initialize */
	for (i = 0; i < NR_CORE_IP; i++) {
		strncpy(core_ip_stats[i].ip_name,
			core_ip_str[i], MAX_IP_NAME_LENGTH);
		core_ip_stats[i].vol_times =
		kmalloc(sizeof(struct ip_vol_times) * NR_CORE_VOLT, GFP_KERNEL);
		if (core_ip_stats[i].vol_times) {
			for (j = 0; core_ptr && j < NR_CORE_VOLT; j++) {
				core_ip_stats[i].vol_times[j].active_time = 0;
				core_ip_stats[i].vol_times[j].idle_time = 0;
				core_ip_stats[i].vol_times[j].off_time = 0;
				core_ip_stats[i].vol_times[j].vol =
					core_ptr->core_volt_tbl[j];
			}
		}
	}
	/* core duration initialize */
	for (i = 0; core_ptr && i < NR_CORE_VOLT; i++) {
		core_vol_duration[i].duration = 0;
		core_vol_duration[i].vol =
			core_ptr->core_volt_tbl[i];
	}

	/* ddr act duration initialize */
	for (i = 0; mem_ptr && i < NR_DDR_FREQ; i++) {
		ddr_act_duration[i].active_time = 0;
		ddr_act_duration[i].freq =
		OPP_FREQ_TO_DDR(mem_ptr->ddr_opp_freq[i]);
	}
	/* ddr sr pd duration initialize */
	ddr_sr_pd_duration.sr_time = 0;
	ddr_sr_pd_duration.pd_time = 0;

	/* ddr bc ip initialize */
	for (i = 0; i < NR_DDR_BC_IP; i++) {
		strncpy(ddr_ip_stats[i].ip_name,
			ddr_bc_ip_str[i], MAX_IP_NAME_LENGTH);
		ddr_ip_stats[i].bc_stats =
		kmalloc(sizeof(struct ddr_bc_stats) * NR_DDR_FREQ, GFP_KERNEL);
		if (ddr_ip_stats[i].bc_stats) {
			for (j = 0; mem_ptr && j < NR_DDR_FREQ; j++) {
				ddr_ip_stats[i].bc_stats[j].value = 0;
				ddr_ip_stats[i].bc_stats[j].freq =
				OPP_FREQ_TO_DDR(mem_ptr->ddr_opp_freq[j]);
			}
		}
	}
	total_suspend_us = 0;

	share_idx_ref_ext =
		(struct share_index_ext *)ref_addr;
	share_idx_ctrl_ext =
		(struct share_ctrl_ext *)ctrl_addr;
	swpm_err("share_index_ext size = %zu bytes\n",
		sizeof(struct share_index_ext));

	swpm_sp_timer_init();

	swpm_sp_procfs_init();

	mtk_register_swpm_ops(&plat_ops);
}

void swpm_sp_exit(void)
{
	del_timer(&swpm_sp_timer);
}

