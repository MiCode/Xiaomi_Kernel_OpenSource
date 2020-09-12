/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __MTK_SWPM_SP_PLATFORM_H__
#define __MTK_SWPM_SP_PLATFORM_H__

#include <mtk_swpm_platform.h>

/* numbers of power state (active, idle, off) */
enum pmsr_power_state {
	PMSR_ACTIVE,
	PMSR_IDLE,
	PMSR_OFF,

	NR_POWER_STATE,
};
/* #define NR_POWER_STATE (3) */

/* core ip (cam, img1, img2, ipe, disp, venc, vdec, scp */
enum core_ip_state {
	CORE_IP_CAM,
	CORE_IP_IMG1,
	CORE_IP_IMG2,
	CORE_IP_IPE,
	CORE_IP_DISP,
	CORE_IP_VENC,
	CORE_IP_VDEC,
	CORE_IP_SCP,

	NR_CORE_IP,
};
/* #define NR_CORE_IP (8) */

/* ddr byte count ip (total read, total write, cpu, mm, gpu, others) */
enum ddr_bc_ip {
	DDR_BC_TOTAL_R,
	DDR_BC_TOTAL_W,
	DDR_BC_TOTAL_CPU,
	DDR_BC_TOTAL_GPU,
	DDR_BC_TOTAL_MM,
	DDR_BC_TOTAL_OTHERS,

	NR_DDR_BC_IP,
};
/* #define NR_DDR_BC_IP (6) */

/* core extension ip state */
struct core_ip_pwr_sta {
	unsigned int state[NR_CORE_VOLT][NR_POWER_STATE];
};

/* core extension index structure */
struct core_index_ext {
	/* core voltage distribution (us) */
	unsigned int acc_time[NR_CORE_VOLT];

	/* core ip power state distribution */
	struct core_ip_pwr_sta pwr_state[NR_CORE_IP];
};

/* mem extension ip word count (1 word -> 8 bytes @ 64bits) */
struct mem_ip_bc {
	unsigned int word_cnt_L[NR_DDR_FREQ];
	unsigned int word_cnt_H[NR_DDR_FREQ];
};

/* dram extension structure */
struct mem_index_ext {
	/* dram freq in state distribution (us) */
	unsigned int acc_time[NR_DDR_FREQ][NR_POWER_STATE];

	/* dram ip byte count in freq distribution */
	struct mem_ip_bc data[NR_DDR_BC_IP];
};

struct suspend_time {
	/* total suspended time H/L*/
	unsigned int time_L;
	unsigned int time_H;
};

struct share_index_ext {
	struct core_index_ext core_idx_ext;
	struct mem_index_ext mem_idx_ext;
	struct suspend_time suspend;

	/* last core volt index */
	unsigned int last_volt_idx;

	/* last ddr freq index */
	unsigned int last_freq_idx;
};

struct share_ctrl_ext {
	unsigned int lock;
	unsigned int clear_flag;
};

extern struct core_swpm_rec_data *core_ptr;
extern struct mem_swpm_rec_data *mem_ptr;

extern void swpm_sp_init(phys_addr_t ref_addr,
			 phys_addr_t ctrl_addr);
extern void swpm_sp_exit(void);

#endif /* __MTK_SWPM_SP_PLATFORM_H__ */
