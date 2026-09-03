/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sdio_dev.h
 * Abstract : This file is a implementation for itm sipc command/event function
 *
 * Authors	: QI.SUN
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MEM_PD_MGR__
#define __MEM_PD_MGR__

#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>

#define MEM_PD_MGR_HEADER "[mem_pd]"

#define MEM_PD_MGR_ERR(fmt, args...)	\
	pr_err(MEM_PD_MGR_HEADER fmt "\n", ## args)
#define MEM_PD_MGR_INFO(fmt, args...)	\
	pr_info(MEM_PD_MGR_HEADER fmt "\n", ## args)
#define MEM_PD_MGR_DBG(fmt, args...)	\
	pr_debug(MEM_PD_MGR_HEADER fmt "\n", ## args)

/* cp2 create thread status */
#define THREAD_CREATE 1
#define THREAD_DELETE 0

struct mem_pd_debug_t {
	unsigned int mem_pd_open_bt;
	unsigned int mem_pd_open_wifi;
	unsigned int mem_pd_close_bt;
	unsigned int mem_pd_close_wifi;
};

struct mem_pd_t {
	struct mutex mem_pd_lock;
	struct completion wifi_open_completion;
	struct completion wifi_cls_cpl;
	struct completion bt_open_completion;
	struct completion bt_close_completion;
	struct completion save_bin_completion;
	unsigned int wifi_state;
	unsigned int bt_state;
	unsigned int cp_version;
	unsigned int bin_save_done;
	char *wifi_mem;
	char *bt_mem;
	char *wifi_clear;
	char *bt_clear;
	struct mem_pd_debug_t mem_pd_debug;
	unsigned int cp_mem_all_off;
};

struct mem_pd_meminfo_t {
	unsigned int wifi_begin_addr;
	unsigned int wifi_end_addr;
	unsigned int wifi_size;
	unsigned int bt_begin_addr;
	unsigned int bt_end_addr;
	unsigned int bt_size;
	unsigned int wifi_iram_mask;
	unsigned int wifi_dram_mask;
	unsigned int bt_iram_mask;
	unsigned int bt_dram_mask;
	unsigned int chip_id;
};

unsigned int mem_pd_wifi_state(void);
int mem_pd_poweroff_deinit(void);
int inform_cp_wifi_download(void);
int mem_pd_mgr(enum wcn_sub_sys subsys, int val);
int mem_pd_save_bin(void);
int mem_pd_init(void);
int mem_pd_exit(void);
unsigned int marlin_get_wcn_chipid(void);
#endif
