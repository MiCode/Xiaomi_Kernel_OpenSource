/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_L1C_H
#define __VCP_L1C_H

#define VCP_IL1C 0
#define VCP_DL1C 1

#define L1C_BASE			(vcpreg.l1cctrl)
#define L1C_SEL(x) ((struct L1C_REGISTER_T *)(L1C_BASE+x*0x3000))


/* L1C_OP register definitions
 */
	#define L1C_OP_EN_OFFSET		(0)
	#define L1C_OP_EN_MASK			(1<<L1C_OP_EN_OFFSET)
	#define L1C_OP_OP_OFFSET		(1)
	#define L1C_OP_OP_MASK			(15<<L1C_OP_OP_OFFSET)

enum vcp_l1c_status_t {
	VCP_L1C_STATUS_OK = 0
};

/* structure type to access the L1C register
 */
struct L1C_REGISTER_T {
	uint32_t L1C_CON;
	uint32_t L1C_OP;
	uint32_t L1C_HCNT0L;
	uint32_t L1C_HCNT0U;
	uint32_t L1C_CCNT0L;
	uint32_t L1C_CCNT0U;
	uint32_t L1C_HCNT1L;
	uint32_t L1C_HCNT1U;
	uint32_t L1C_CCNT1L;
	uint32_t L1C_CCNT1U;
	uint32_t RESERVED0[1];
	uint32_t L1C_REGION_EN;
	uint32_t RESERVED1[2036];/**< (0x2000-12*4)/4 */
	uint32_t L1C_ENTRY_N[16];
	uint32_t L1C_END_ENTRY_N[16];
};

enum L1C_line_command_t {
	L1C_INVA = 1,
	L1C_INVL = 2,
	L1C_INVW = 4,
	L1C_FLUA = 9,
	L1C_FLUD = 10,
	L1C_FLUS = 12
};

enum vcp_l1c_status_t vcp_l1c_flua(uint32_t L1C_type);
#endif
