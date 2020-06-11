/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __PORT_SMEM_H__
#define __PORT_SMEM_H__

#include <linux/timer.h>
#include <linux/hrtimer.h>
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "port_t.h"

enum {
	TYPE_RAW = 0,
	TYPE_CCB,
};

enum {
	CCB_USER_INVALID = 0,
	CCB_USER_OK,
	CCB_USER_ERR,
};

struct ccci_ccb_ctrl {
	unsigned char *ctrl_addr_phy;
	unsigned char *ctrl_addr_vir;
};

#ifdef DEBUG_FOR_CCB
#define CCB_POLL_PTR_MAX    (2*3)
struct buffer_header {
	unsigned int dl_guard_band;
	unsigned int dl_alloc_index;
	unsigned int dl_free_index;
	unsigned int dl_read_index;
	unsigned int dl_write_index;
	unsigned int dl_page_size;
	unsigned int dl_data_buffer_size;
	unsigned int dl_guard_band_e;
	unsigned int ul_guard_band;
	unsigned int ul_alloc_index;
	unsigned int ul_free_index;
	unsigned int ul_read_index;
	unsigned int ul_write_index;
	unsigned int ul_page_size;
	unsigned int ul_data_buffer_size;
	unsigned int ul_guard_band_e;
};

struct dump_ptr {
	unsigned int al_id;
	unsigned int fr_id;
	unsigned int r_id;
	unsigned int w_id;
};
#endif

struct ccci_smem_port {
	enum SMEM_USER_ID user_id;
	unsigned char type;
	unsigned short core_id;

	unsigned char state;
	unsigned int wakeup;
	unsigned char wk_cnt;
	phys_addr_t addr_phy;
	unsigned char *addr_vir;
	unsigned int length;
	struct port_t *port;
	wait_queue_head_t rx_wq;
	int ccb_ctrl_offset;
	struct hrtimer notify_timer;
	spinlock_t write_lock;
#ifdef DEBUG_FOR_CCB
	struct buffer_header *ccb_vir_addr;
	unsigned long long last_poll_time[CCB_POLL_PTR_MAX];
	unsigned long long last_poll_t_exit[CCB_POLL_PTR_MAX];
	unsigned long long last_rx_wk_time;
	unsigned int last_mask[2];
	atomic_t poll_processing[2];
	unsigned char poll_save_idx;
	struct dump_ptr last_in[CCB_POLL_PTR_MAX];
	struct dump_ptr last_out[CCB_POLL_PTR_MAX];
#endif
};
#endif
