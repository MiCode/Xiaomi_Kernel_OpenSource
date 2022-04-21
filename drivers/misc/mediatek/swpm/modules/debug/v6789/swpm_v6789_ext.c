// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_module.h>
#include <swpm_module_ext.h>
#include <swpm_v6789.h>
#include <swpm_v6789_ext.h>

#define SWPM_INTERNAL_TEST (0)
#define DEFAULT_UPDATE_MS (10000)
#define CORE_SRAM (share_idx_ref_ext->core_idx_ext)
#define DDR_SRAM (share_idx_ref_ext->mem_idx_ext)
#define SUSPEND_SRAM (share_idx_ref_ext->suspend)
#define DATA_SRAM (share_data_ref)

#define OPP_FREQ_TO_DDR(x) \
	((x == 1066 || x == 1333) ? (x * 2 + 1) : (x * 2))

static struct timer_list swpm_sp_timer;
static DEFINE_SPINLOCK(swpm_sp_spinlock);

/* share sram for extension index */
static struct share_index_ext *share_idx_ref_ext;
static struct share_ctrl_ext *share_idx_ctrl_ext;
static struct share_data *share_data_ref;

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
static char core_ip_str[NR_CORE_IP][MAX_IP_NAME_LENGTH] = {
	"DISP", "VENC", "VDEC", "SCP",
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

	if (share_idx_ref_ext && share_idx_ctrl_ext) {

		if (share_idx_ctrl_ext->clear_flag)
			return;

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

static void swpm_sp_routine(struct timer_list *t)
{
	unsigned long flags;

	spin_lock_irqsave(&swpm_sp_spinlock, flags);
	swpm_sp_internal_update();
	spin_unlock_irqrestore(&swpm_sp_spinlock, flags);

	mod_timer(t, jiffies + msecs_to_jiffies(update_interval_ms));
}

static void swpm_sp_dispatcher(unsigned int type,
			       unsigned int val)
{
	switch (type) {
	case SYNC_DATA:
		/* do update */
		swpm_sp_routine(&swpm_sp_timer);
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
	int i;
	struct ddr_ip_bc_stats *p = stats;

	if (p && data_ip_num == NR_DDR_BC_IP && freq_num == NR_DDR_FREQ) {
		spin_lock_irqsave(&swpm_sp_spinlock, flags);
		for (i = 0; i < NR_DDR_BC_IP && p[i].bc_stats; i++) {
			strncpy(p[i].ip_name,
				ddr_ip_stats[i].ip_name,
				MAX_IP_NAME_LENGTH - 1);
			memcpy(p[i].bc_stats,
			       ddr_ip_stats[i].bc_stats,
			       sizeof(struct ddr_bc_stats) * NR_DDR_FREQ);
		}
		spin_unlock_irqrestore(&swpm_sp_spinlock, flags);
	}
	return 0;
}
static int32_t swpm_vcore_ip_vol_stats(int32_t ip_num,
				       int32_t vol_num,
				       void *stats)
{
	unsigned long flags;
	int i;
	struct ip_stats *p = stats;

	if (p && ip_num == NR_CORE_IP && vol_num == NR_CORE_VOLT) {
		spin_lock_irqsave(&swpm_sp_spinlock, flags);
		for (i = 0; i < NR_CORE_IP && p[i].vol_times; i++) {
			strncpy(p[i].ip_name,
				core_ip_stats[i].ip_name,
				MAX_IP_NAME_LENGTH - 1);
			memcpy(p[i].vol_times,
			       core_ip_stats[i].vol_times,
			       sizeof(struct ip_vol_times) * NR_CORE_VOLT);
		}
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
	timer_setup(&swpm_sp_timer, swpm_sp_routine, TIMER_DEFERRABLE);
	mod_timer(&swpm_sp_timer, jiffies + msecs_to_jiffies(update_interval_ms));
}

/* void swpm_v6789_ext_init(phys_addr_t ref_addr, */
/*		  phys_addr_t ctrl_addr) */
void swpm_v6789_ext_init(void)
{
	int i, j;

	/* init extension index address */
	/* swpm_sp_init(sspm_sbuf_get(wrap_d->share_index_ext_addr), */
	/*     sspm_sbuf_get(wrap_d->share_ctrl_ext_addr)); */

	if (wrap_d) {
		share_idx_ref_ext =
		(struct share_index_ext *)
		sspm_sbuf_get(wrap_d->share_index_ext_addr);
		share_idx_ctrl_ext =
		(struct share_ctrl_ext *)
		sspm_sbuf_get(wrap_d->share_ctrl_ext_addr);
		share_data_ref =
		(struct share_data *)
		sspm_sbuf_get(wrap_d->share_data_addr);
	} else {
		share_idx_ref_ext = NULL;
		share_idx_ctrl_ext = NULL;
		share_data_ref = NULL;
	}

	/* core_ip_stats initialize */
	for (i = 0; i < NR_CORE_IP; i++) {
		strncpy(core_ip_stats[i].ip_name,
			core_ip_str[i], MAX_IP_NAME_LENGTH - 1);
		core_ip_stats[i].vol_times =
		kmalloc(sizeof(struct ip_vol_times) * NR_CORE_VOLT, GFP_KERNEL);
		if (core_ip_stats[i].vol_times) {
			for (j = 0; DATA_SRAM && j < NR_CORE_VOLT; j++) {
				core_ip_stats[i].vol_times[j].active_time = 0;
				core_ip_stats[i].vol_times[j].idle_time = 0;
				core_ip_stats[i].vol_times[j].off_time = 0;
				core_ip_stats[i].vol_times[j].vol =
					DATA_SRAM->core_volt_tbl[j];
			}
		}
	}
	/* core duration initialize */
	for (i = 0; DATA_SRAM && i < NR_CORE_VOLT; i++) {
		core_vol_duration[i].duration = 0;
		core_vol_duration[i].vol =
			DATA_SRAM->core_volt_tbl[i];
	}

	/* ddr act duration initialize */
	for (i = 0; DATA_SRAM && i < NR_DDR_FREQ; i++) {
		ddr_act_duration[i].active_time = 0;
		ddr_act_duration[i].freq =
			OPP_FREQ_TO_DDR(DATA_SRAM->ddr_opp_freq[i]);
	}
	/* ddr sr pd duration initialize */
	ddr_sr_pd_duration.sr_time = 0;
	ddr_sr_pd_duration.pd_time = 0;

	/* ddr bc ip initialize */
	for (i = 0; i < NR_DDR_BC_IP; i++) {
		strncpy(ddr_ip_stats[i].ip_name,
			ddr_bc_ip_str[i], MAX_IP_NAME_LENGTH - 1);
		ddr_ip_stats[i].bc_stats =
		kmalloc(sizeof(struct ddr_bc_stats) * NR_DDR_FREQ, GFP_KERNEL);
		if (ddr_ip_stats[i].bc_stats) {
			for (j = 0; DATA_SRAM && j < NR_DDR_FREQ; j++) {
				ddr_ip_stats[i].bc_stats[j].value = 0;
				ddr_ip_stats[i].bc_stats[j].freq =
				OPP_FREQ_TO_DDR(DATA_SRAM->ddr_opp_freq[j]);
			}
		}
	}
	total_suspend_us = 0;

#if SWPM_TEST
	pr_notice("share_index_ext size = %zu bytes\n",
		sizeof(struct share_index_ext));
#endif

	swpm_sp_timer_init();

	mtk_register_swpm_ops(&plat_ops);
}

void swpm_v6789_ext_exit(void)
{
	del_timer_sync(&swpm_sp_timer);
}

