/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_SWPM_COMMON_H__
#define __MTK_SWPM_COMMON_H__

#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <mt-plat/sync_write.h>
#include <mtk_swpm_platform.h>
#include <mtk_swpm_sp_interface.h>


/* LOG */
#undef TAG
#define TAG     "[SWPM] "

/* PROCFS */
#define PROC_FOPS_RW(name)                                                 \
	static int name ## _proc_open(struct inode *inode,                 \
		struct file *file)                                         \
	{                                                                  \
		return single_open(file, name ## _proc_show,               \
			PDE_DATA(inode));                                  \
	}                                                                  \
	static const struct file_operations name ## _proc_fops = {         \
		.owner      = THIS_MODULE,                                 \
		.open       = name ## _proc_open,                          \
		.read	    = seq_read,                                    \
		.llseek	    = seq_lseek,                                   \
		.release    = single_release,                              \
		.write      = name ## _proc_write,                         \
	}
#define PROC_FOPS_RO(name)                                                 \
	static int name ## _proc_open(struct inode *inode,                 \
		struct file *file)                                         \
	{                                                                  \
		return single_open(file, name ## _proc_show,               \
			PDE_DATA(inode));                                  \
	}                                                                  \
	static const struct file_operations name ## _proc_fops = {         \
		.owner = THIS_MODULE,                                      \
		.open  = name ## _proc_open,                               \
		.read  = seq_read,                                         \
		.llseek = seq_lseek,                                       \
		.release = single_release,                                 \
	}
#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

#define swpm_err                swpm_info
#define swpm_warn               swpm_info
#define swpm_info(fmt, args...) pr_notice(TAG""fmt, ##args)
#define swpm_dbg(fmt, args...)                           \
	do {                                             \
		if (swpm_debug)                          \
			swpm_info(fmt, ##args);          \
		else                                     \
			pr_debug(TAG""fmt, ##args);      \
	} while (0)

#define swpm_readl(addr)	__raw_readl(addr)
#define swpm_writel(addr, val)	mt_reg_sync_writel(val, addr)

#define swpm_lock(lock)		mutex_lock(lock)
#define swpm_unlock(lock)	mutex_unlock(lock)

#define swpm_get_status(type)  ((swpm_status & (1 << type)) >> type)
#define swpm_set_status(type)  (swpm_status |= (1 << type))
#define swpm_clr_status(type)  (swpm_status &= ~(1 << type))
#define for_each_pwr_mtr(i)    for (i = 0; i < NR_POWER_METER; i++)

struct swpm_entry {
	const char *name;
	const struct file_operations *fops;
};

struct swpm_mem_ref_tbl {
	bool valid;
	phys_addr_t *virt;
};

/* swpm extension interface types */
enum swpm_num_type {
	DDR_DATA_IP,
	DDR_FREQ,
	CORE_IP,
	CORE_VOL,
};
enum swpm_cmd_type {
	SYNC_DATA,
	SET_INTERVAL,
};

/* swpm extension internal ops structure */
struct swpm_internal_ops {
	void (*const cmd)(unsigned int type,
			  unsigned int val);
	int32_t (*const ddr_times_get)
		(int32_t freq_num,
		 struct ddr_times *ddr_times);
	int32_t (*const ddr_freq_data_ip_stats_get)
		(int32_t data_ip_num,
		 int32_t freq_num,
		 void *stats);
	int32_t (*const vcore_ip_vol_stats_get)
		(int32_t ip_num,
		 int32_t vol_num,
		 void *stats);
	int32_t (*const vcore_vol_duration_get)
		(int32_t vol_num,
		 int64_t *duration);
	int32_t (*const num_get)
		(enum swpm_num_type type);
};

extern bool swpm_debug;
extern unsigned int swpm_status;
extern struct mutex swpm_mutex;
extern unsigned int swpm_log_mask;
extern struct timer_list swpm_timer;

extern int swpm_append_procfs(struct swpm_entry *p);
extern int swpm_create_procfs(void);
extern void swpm_update_periodic_timer(void);
extern int swpm_set_periodic_timer(void (*func)(unsigned long));
extern void swpm_get_rec_addr(phys_addr_t *phys,
			      phys_addr_t *virt,
			      unsigned long long *size);
extern int swpm_reserve_mem_init(phys_addr_t *virt,
				 unsigned long long *size);
extern int swpm_interface_manager_init(struct swpm_mem_ref_tbl *ref_tbl,
				       unsigned int tbl_size);
extern void swpm_set_enable(unsigned int type, unsigned int enable);
extern void swpm_set_update_cnt(unsigned int type, unsigned int cnt);
extern char *swpm_power_rail_to_string(enum power_rail p);
extern unsigned int swpm_get_avg_power(unsigned int type,
				       unsigned int avg_window);
/* swpm extension ops registry */
extern int mtk_register_swpm_ops(struct swpm_internal_ops *ops);

#endif

