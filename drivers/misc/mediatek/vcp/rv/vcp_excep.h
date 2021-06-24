/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_EXCEP_H__
#define __VCP_EXCEP_H__

#include <linux/sizes.h>
#include "vcp_helper.h"
#include "vcp_ipi_pin.h"

#define AED_LOG_PRINT_SIZE	SZ_16K
#define VCP_LOCK_OFS	0xE0
#define VCP_TCM_LOCK_BIT	(1 << 20)

enum vcp_excep_id {
	EXCEP_RESET,
	EXCEP_BOOTUP,
	EXCEP_RUNTIME,
	VCP_NR_EXCEP,
};

struct vcp_status_reg {
	uint32_t status;
	uint32_t pc;
	uint32_t lr;
	uint32_t sp;
	uint32_t pc_latch;
	uint32_t lr_latch;
	uint32_t sp_latch;
};

extern void vcp_dump_last_regs(void);
extern void vcp_aed(enum VCP_RESET_TYPE type, enum vcp_core_id id);
extern void vcp_aed_reset(enum vcp_excep_id type, enum vcp_core_id id);
extern void vcp_aed_reset_inplace(enum vcp_excep_id type,
		enum vcp_core_id id);
extern void vcp_get_log(enum vcp_core_id id);
extern char *vcp_pickup_log_for_aee(void);
extern void aed_vcp_exception_api(const int *log, int log_size,
		const int *phy, int phy_size, const char *detail,
		const int db_opt);
extern void vcp_excep_cleanup(void);
enum { r0, r1, r2, r3, r12, lr, pc, psr};
extern int vcp_ee_enable;
extern int vcp_reset_counts;

extern struct vcp_status_reg *c0_m;
extern struct vcp_status_reg *c0_t1_m;
extern struct vcp_status_reg *c1_m;
extern struct vcp_status_reg *c1_t1_m;

#define MDUMP_L2TCM_SIZE     0x40000 /* L2_TCM , for all vcp maximum sram size */
#define MDUMP_L1C_SIZE       0x03c000
#define MDUMP_REGDUMP_SIZE   0x003c00 /* register backup (max size) */
#define MDUMP_TBUF_SIZE      0x000400
#define MDUMP_DRAM_SIZE      VCP_DRAM_MAPSIZE

struct MemoryDump {
	/*vcp sram*/
	char l2tcm[MDUMP_L2TCM_SIZE];
	char l1c[MDUMP_L1C_SIZE];
	/*vcp reg*/
	char regdump[MDUMP_REGDUMP_SIZE];
	char tbuf[MDUMP_TBUF_SIZE];
	char dram[MDUMP_DRAM_SIZE];
};

#endif
