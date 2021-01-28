/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __SCP_EXCEP_H__
#define __SCP_EXCEP_H__

#include <linux/sizes.h>
#include "scp_helper.h"
#include "scp_ipi_pin.h"

#define AED_LOG_PRINT_SIZE	SZ_16K
#define SCP_LOCK_OFS	0xE0
#define SCP_TCM_LOCK_BIT	(1 << 20)

enum scp_excep_id {
	EXCEP_RESET,
	EXCEP_BOOTUP,
	EXCEP_RUNTIME,
	SCP_NR_EXCEP,
};

struct scp_status_reg {
	uint32_t status;
	uint32_t pc;
	uint32_t lr;
	uint32_t sp;
	uint32_t pc_latch;
	uint32_t lr_latch;
	uint32_t sp_latch;
};

extern void scp_dump_last_regs(void);
extern void scp_aed(enum SCP_RESET_TYPE type, enum scp_core_id id);
extern void scp_aed_reset(enum scp_excep_id type, enum scp_core_id id);
extern void scp_aed_reset_inplace(enum scp_excep_id type,
		enum scp_core_id id);
extern void scp_get_log(enum scp_core_id id);
extern char *scp_pickup_log_for_aee(void);
extern void aed_scp_exception_api(const int *log, int log_size,
		const int *phy, int phy_size, const char *detail,
		const int db_opt);
extern void scp_excep_cleanup(void);
enum { r0, r1, r2, r3, r12, lr, pc, psr};
extern int scp_ee_enable;
extern int scp_reset_counts;

extern struct scp_status_reg c0_m;
extern struct scp_status_reg c1_m;

#define MDUMP_L2TCM_SIZE	0x180000 /* L2_TCM */
#define MDUMP_L1C_SIZE		0x03c000
#define MDUMP_REGDUMP_SIZE	0x003f00 /* register backup (max size) */
#define MDUMP_TBUF_SIZE		0x000100
#define MDUMP_DRAM_SIZE		SCP_DRAM_MAPSIZE

struct MemoryDump {
	/*scp sram*/
	char l2tcm[MDUMP_L2TCM_SIZE];
	char l1c[MDUMP_L1C_SIZE];
	/*scp reg*/
	char regdump[MDUMP_REGDUMP_SIZE];
	char tbuf[MDUMP_TBUF_SIZE];
	char dram[MDUMP_DRAM_SIZE];
};

#endif
