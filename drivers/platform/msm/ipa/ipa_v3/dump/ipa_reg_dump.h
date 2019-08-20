/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#if !defined(_IPA_REG_DUMP_H_)
#define _IPA_REG_DUMP_H_

#include <linux/types.h>
#include <linux/string.h>

#include "ipa_i.h"

#include "ipa_pkt_cntxt.h"
#include "ipa_hw_common_ex.h"

#define IPA_0_IPA_WRAPPER_BASE 0 /* required by following includes */

#include "ipa_hwio.h"
#include "gsi_hwio.h"
#include "ipa_gcc_hwio.h"

#include "ipa_hwio_def.h"
#include "gsi_hwio_def.h"
#include "ipa_gcc_hwio_def.h"

#define IPA_DEBUG_CMDQ_DPS_SELECT_NUM_GROUPS     0x6
#define IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS     0x4
#define IPA_DEBUG_TESTBUS_RSRC_NUM_EP            7
#define IPA_DEBUG_TESTBUS_RSRC_NUM_GRP           3
#define IPA_TESTBUS_SEL_EP_MAX                   0x1F
#define IPA_TESTBUS_SEL_EXTERNAL_MAX             0x40
#define IPA_TESTBUS_SEL_INTERNAL_MAX             0xFF
#define IPA_TESTBUS_SEL_INTERNAL_PIPE_MAX        0x40
#define IPA_DEBUG_CMDQ_ACK_SELECT_NUM_GROUPS     0x9
#define IPA_RSCR_MNGR_DB_RSRC_ID_MAX             0x3F
#define IPA_RSCR_MNGR_DB_RSRC_TYPE_MAX           0xA

#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_ZEROS   (0x0)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_0   (0x1)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_1   (0x2)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_2   (0x3)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_3   (0x4)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_4   (0x5)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_DB_ENG  (0x9)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_0   (0xB)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_1   (0xC)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_2   (0xD)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_3   (0xE)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_4   (0xF)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_5   (0x10)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_6   (0x11)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_7   (0x12)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_0   (0x13)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_1   (0x14)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_2   (0x15)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_3   (0x16)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_4   (0x17)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_5   (0x18)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IE_0    (0x1B)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IE_1    (0x1C)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_0    (0x1F)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_1    (0x20)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_2    (0x21)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_3    (0x22)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_4    (0x23)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_0  (0x27)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_1  (0x28)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_2  (0x29)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_3  (0x2A)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_0   (0x2B)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_1   (0x2C)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_2   (0x2D)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_3   (0x2E)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_0 (0x33)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_1 (0x34)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_2 (0x35)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_3 (0x36)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_CSR     (0x3A)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_SDMA_0  (0x3C)
#define HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_SDMA_1  (0x3D)

#define IPA_DEBUG_TESTBUS_DEF_EXTERNAL           50
#define IPA_DEBUG_TESTBUS_DEF_INTERNAL           6

#define IPA_REG_SAVE_BYTES_PER_CHNL_SHRAM        8

#define IPA_REG_SAVE_GSI_NUM_EE                  3

#define IPA_REG_SAVE_NUM_EXTRA_ENDP_REGS         22

#define IPA_DEBUG_TESTBUS_RSRC_TYPE_CNT_BIT_MASK 0x7E000
#define IPA_DEBUG_TESTBUS_RSRC_TYPE_CNT_SHIFT    13

/*
 * A structure used to map a source address to destination address...
 */
struct map_src_dst_addr_s {
	u32  src_addr; /* register offset to copy value from */
	u32 *dst_addr; /* memory address to copy register value to */
};

/*
 * A macro to generate the names of scaler (ie. non-vector) registers
 * that reside in the *hwio.h files (said files contain the manifest
 * constants for the registers' offsets in the register memory map).
 */
#define GEN_SCALER_REG_OFST(reg_name) \
	(HWIO_ ## reg_name ## _ADDR)

/*
 * A macro to generate the names of vector registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate access to registers that are
 * addressed via one dimension.
 */
#define GEN_1xVECTOR_REG_OFST(reg_name, row) \
	(HWIO_ ## reg_name ## _ADDR(row))

/*
 * A macro to generate the names of vector registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate access to registers that are
 * addressed via two dimensions.
 */
#define GEN_2xVECTOR_REG_OFST(reg_name, row, col) \
	(HWIO_ ## reg_name ## _ADDR(row, col))

/*
 * A macro to generate the access to scaler registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate read access from a scaler
 * register..
 */
#define IPA_READ_SCALER_REG(reg_name) \
	HWIO_ ## reg_name ## _IN

/*
 * A macro to generate the access to vector registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate read access from a one
 * dimensional vector register...
 */
#define IPA_READ_1xVECTOR_REG(reg_name, row) \
	HWIO_ ## reg_name ## _INI(row)

/*
 * A macro to generate the access to vector registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate read access from a two
 * dimensional vector register...
 */
#define IPA_READ_2xVECTOR_REG(reg_name, row, col) \
	HWIO_ ## reg_name ## _INI2(row, col)

/*
 * A macro to generate the access to scaler registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate write access to a scaler
 * register..
 */
#define IPA_WRITE_SCALER_REG(reg_name, val) \
	HWIO_ ## reg_name ## _OUT(val)

/*
 * A macro to generate the access to vector registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate write access to a one
 * dimensional vector register...
 */
#define IPA_WRITE_1xVECTOR_REG(reg_name, row, val) \
	HWIO_ ## reg_name ## _OUTI(row, val)

/*
 * A macro to generate the access to vector registers that reside in
 * the *hwio.h files (said files contain the manifest constants for
 * the registers' offsets in the register memory map). More
 * specifically, this macro will generate write access to a two
 * dimensional vector register...
 */
#define IPA_WRITE_2xVECTOR_REG(reg_name, row, col, val) \
	HWIO_ ## reg_name ## _OUTI2(row, col, val)

/*
 * Macro that helps generate a mapping between a register's address
 * and where the register's value will get stored (ie. source and
 * destination address mapping) upon dump...
 */
#define GEN_SRC_DST_ADDR_MAP(reg_name, sub_struct, field_name) \
	{ GEN_SCALER_REG_OFST(reg_name), \
	  (u32 *)&ipa_reg_save.sub_struct.field_name }

/*
 * Macro to get value of bits 18:13, used tp get rsrc cnts from
 * IPA_DEBUG_DATA
 */
#define IPA_DEBUG_TESTBUS_DATA_GET_RSRC_CNT_BITS_FROM_DEBUG_DATA(x) \
	((x & IPA_DEBUG_TESTBUS_RSRC_TYPE_CNT_BIT_MASK) >> \
	 IPA_DEBUG_TESTBUS_RSRC_TYPE_CNT_SHIFT)

/*
 * Macro to pluck the gsi version from ram.
 */
#define IPA_REG_SAVE_GSI_VER(reg_name, var_name)	\
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.gsi.gen.var_name }
/*
 * Macro to define a particular register cfg entry for all 3 EE
 * indexed register
 */
#define IPA_REG_SAVE_CFG_ENTRY_GEN_EE(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, IPA_HW_Q6_EE), \
		(u32 *)&ipa_reg_save.ipa.gen_ee[IPA_HW_Q6_EE].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE), \
		(u32 *)&ipa_reg_save.ipa.gen_ee[IPA_HW_A7_EE].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, IPA_HW_HWP_EE), \
		(u32 *)&ipa_reg_save.ipa.gen_ee[IPA_HW_HWP_EE].var_name }

#define IPA_REG_SAVE_CFG_ENTRY_GSI_FIFO(reg_name, var_name, index) \
	{ GEN_SCALER_REG_OFST(reg_name), \
		(u32 *)&ipa_reg_save.ipa.gsi_fifo_status[index].var_name }

/*
 * Macro to define a particular register cfg entry for all pipe
 * indexed register
 */
#define IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.ipa.pipes[0].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 1), \
		(u32 *)&ipa_reg_save.ipa.pipes[1].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 2), \
		(u32 *)&ipa_reg_save.ipa.pipes[2].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 3), \
		(u32 *)&ipa_reg_save.ipa.pipes[3].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 4), \
		(u32 *)&ipa_reg_save.ipa.pipes[4].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 5), \
		(u32 *)&ipa_reg_save.ipa.pipes[5].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 6), \
		(u32 *)&ipa_reg_save.ipa.pipes[6].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 7), \
		(u32 *)&ipa_reg_save.ipa.pipes[7].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 8), \
		(u32 *)&ipa_reg_save.ipa.pipes[8].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 9), \
		(u32 *)&ipa_reg_save.ipa.pipes[9].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 10), \
		(u32 *)&ipa_reg_save.ipa.pipes[10].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 11), \
		(u32 *)&ipa_reg_save.ipa.pipes[11].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 12), \
		(u32 *)&ipa_reg_save.ipa.pipes[12].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 13), \
		(u32 *)&ipa_reg_save.ipa.pipes[13].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 14), \
		(u32 *)&ipa_reg_save.ipa.pipes[14].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 15), \
		(u32 *)&ipa_reg_save.ipa.pipes[15].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 16), \
		(u32 *)&ipa_reg_save.ipa.pipes[16].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 17), \
		(u32 *)&ipa_reg_save.ipa.pipes[17].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 18), \
		(u32 *)&ipa_reg_save.ipa.pipes[18].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 19), \
		(u32 *)&ipa_reg_save.ipa.pipes[19].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 20), \
		(u32 *)&ipa_reg_save.ipa.pipes[20].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 21), \
		(u32 *)&ipa_reg_save.ipa.pipes[21].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 22), \
		(u32 *)&ipa_reg_save.ipa.pipes[22].endp.var_name }

/*
 * Macro to define a particular register cfg entry for all pipe
 * indexed register
 */
#define IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(reg_name, var_name) \
	{ 0, 0 }

/*
 * Macro to define a particular register cfg entry for all resource
 * group register
 */
#define IPA_REG_SAVE_CFG_ENTRY_SRC_RSRC_GRP(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_grp[0].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 1), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_grp[1].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 2), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_grp[2].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 3), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_grp[3].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 4), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_grp[4].var_name }

/*
 * Macro to define a particular register cfg entry for all resource
 * group register
 */
#define IPA_REG_SAVE_CFG_ENTRY_DST_RSRC_GRP(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.ipa.dst_rsrc_grp[0].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 1), \
		(u32 *)&ipa_reg_save.ipa.dst_rsrc_grp[1].var_name }

/*
 * Macro to define a particular register cfg entry for all source
 * resource group count register
 */
#define IPA_REG_SAVE_CFG_ENTRY_SRC_RSRC_CNT_GRP(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_cnt[0].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 1), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_cnt[1].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 2), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_cnt[2].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 3), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_cnt[3].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 4), \
		(u32 *)&ipa_reg_save.ipa.src_rsrc_cnt[4].var_name }

/*
 * Macro to define a particular register cfg entry for all dest
 * resource group count register
 */
#define IPA_REG_SAVE_CFG_ENTRY_DST_RSRC_CNT_GRP(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.ipa.dst_rsrc_cnt[0].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 1), \
		(u32 *)&ipa_reg_save.ipa.dst_rsrc_cnt[1].var_name }

#define IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE), \
		(u32 *)&ipa_reg_save.gsi.gen_ee[IPA_HW_A7_EE].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, IPA_HW_Q6_EE), \
		(u32 *)&ipa_reg_save.gsi.gen_ee[IPA_HW_Q6_EE].var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, IPA_HW_UC_EE), \
		(u32 *)&ipa_reg_save.gsi.gen_ee[IPA_HW_UC_EE].var_name }

/*
 * Macro to define a particular register cfg entry for all GSI EE
 * register
 */
#define IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(reg_name, var_name) \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 0), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[0].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 1), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[1].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 2), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[2].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 3), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[3].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 4), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[4].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 5), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[5].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 6), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[6].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 7), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[7].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 8), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[8].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 9), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[9].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 10), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[10].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 11), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[11].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 12), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[12].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 13), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[13].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 14), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.a7[14].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_UC_EE, 0),	\
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.uc[0].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_UC_EE, 1), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.uc[1].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_UC_EE, 2),	\
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.uc[2].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_UC_EE, 3), \
		(u32 *)&ipa_reg_save.gsi.ch_cntxt.uc[3].var_name }

#define IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(reg_name, var_name) \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 0), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[0].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 1), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[1].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 2), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[2].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 3), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[3].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 4), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[4].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 5), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[5].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 6), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[6].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 7), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[7].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 8), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[8].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 9), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[9].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 10), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[10].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_A7_EE, 11), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.a7[11].var_name }, \
	{ GEN_2xVECTOR_REG_OFST(reg_name, IPA_HW_UC_EE, 0), \
		(u32 *)&ipa_reg_save.gsi.evt_cntxt.uc[0].var_name }

/*
 * Macro to define a particular register cfg entry for GSI QSB debug
 * registers
 */
#define IPA_REG_SAVE_CFG_ENTRY_GSI_QSB_DEBUG(reg_name, var_name) \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.gsi.debug.gsi_qsb_debug.var_name[0] }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 1), \
		(u32 *)&ipa_reg_save.gsi.debug.gsi_qsb_debug.var_name[1] }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 2), \
		(u32 *)&ipa_reg_save.gsi.debug.gsi_qsb_debug.var_name[2] }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 3), \
		(u32 *)&ipa_reg_save.gsi.debug.gsi_qsb_debug.var_name[3] }

/*
 * IPA HW Platform Type
 */
enum ipa_hw_ee_e {
	IPA_HW_A7_EE  = 0, /* A7's execution environment */
	IPA_HW_Q6_EE  = 1, /* Q6's execution environment */
	IPA_HW_UC_EE  = 2, /* UC's execution environment */
	IPA_HW_HWP_EE = 3, /* HWP's execution environment */
	IPA_HW_EE_MAX,     /* Max EE to support */
};

/*
 * General IPA register save data struct (ie. this is where register
 * values, once read, get placed...
 */
struct ipa_gen_regs_s {
	struct ipa_hwio_def_ipa_state_s
	  ipa_state;
	struct ipa_hwio_def_ipa_gsi_conf_s
	  ipa_gsi_conf;
	struct ipa_hwio_def_ipa_state_rx_active_s
	  ipa_state_rx_active;
	struct ipa_hwio_def_ipa_state_tx_wrapper_s
	  ipa_state_tx_wrapper;
	struct ipa_hwio_def_ipa_state_tx0_s
	  ipa_state_tx0;
	struct ipa_hwio_def_ipa_state_tx1_s
	  ipa_state_tx1;
	struct ipa_hwio_def_ipa_state_aggr_active_s
	  ipa_state_aggr_active;
	struct ipa_hwio_def_ipa_state_dfetcher_s
	  ipa_state_dfetcher;
	struct ipa_hwio_def_ipa_state_fetcher_mask_s
	  ipa_state_fetcher_mask;
	struct ipa_hwio_def_ipa_state_gsi_aos_s
	  ipa_state_gsi_aos;
	struct ipa_hwio_def_ipa_state_gsi_if_s
	  ipa_state_gsi_if;
	struct ipa_hwio_def_ipa_state_gsi_skip_s
	  ipa_state_gsi_skip;
	struct ipa_hwio_def_ipa_state_gsi_tlv_s
	  ipa_state_gsi_tlv;
	struct ipa_hwio_def_ipa_tag_timer_s
	  ipa_tag_timer;
	struct ipa_hwio_def_ipa_dpl_timer_lsb_s
	  ipa_dpl_timer_lsb;
	struct ipa_hwio_def_ipa_dpl_timer_msb_s
	  ipa_dpl_timer_msb;
	struct ipa_hwio_def_ipa_proc_iph_cfg_s
	  ipa_proc_iph_cfg;
	struct ipa_hwio_def_ipa_route_s
	  ipa_route;
	struct ipa_hwio_def_ipa_spare_reg_1_s
	  ipa_spare_reg_1;
	struct ipa_hwio_def_ipa_spare_reg_2_s
	  ipa_spare_reg_2;
	struct ipa_hwio_def_ipa_log_s
	  ipa_log;
	struct ipa_hwio_def_ipa_log_buf_status_cfg_s
	  ipa_log_buf_status_cfg;
	struct ipa_hwio_def_ipa_log_buf_status_addr_s
	  ipa_log_buf_status_addr;
	struct ipa_hwio_def_ipa_log_buf_status_write_ptr_s
	  ipa_log_buf_status_write_ptr;
	struct ipa_hwio_def_ipa_log_buf_status_ram_ptr_s
	  ipa_log_buf_status_ram_ptr;
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_cfg_s
	  ipa_log_buf_hw_cmd_cfg;
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_addr_s
	  ipa_log_buf_hw_cmd_addr;
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_write_ptr_s
	  ipa_log_buf_hw_cmd_write_ptr;
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_ram_ptr_s
	  ipa_log_buf_hw_cmd_ram_ptr;
	struct ipa_hwio_def_ipa_comp_hw_version_s
	  ipa_comp_hw_version;
	struct ipa_hwio_def_ipa_filt_rout_hash_en_s
	  ipa_filt_rout_hash_en;
	struct ipa_hwio_def_ipa_filt_rout_hash_flush_s
	  ipa_filt_rout_hash_flush;
	struct ipa_hwio_def_ipa_state_fetcher_s
	  ipa_state_fetcher;
	struct ipa_hwio_def_ipa_ipv4_filter_init_values_s
	  ipa_ipv4_filter_init_values;
	struct ipa_hwio_def_ipa_ipv6_filter_init_values_s
	  ipa_ipv6_filter_init_values;
	struct ipa_hwio_def_ipa_ipv4_route_init_values_s
	  ipa_ipv4_route_init_values;
	struct ipa_hwio_def_ipa_ipv6_route_init_values_s
	  ipa_ipv6_route_init_values;
	struct ipa_hwio_def_ipa_bcr_s
	  ipa_bcr;
	struct ipa_hwio_def_ipa_bam_activated_ports_s
	  ipa_bam_activated_ports;
	struct ipa_hwio_def_ipa_tx_commander_cmdq_status_s
	  ipa_tx_commander_cmdq_status;
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_en_s
	  ipa_log_buf_hw_snif_el_en;
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_wr_n_rd_sel_s
	  ipa_log_buf_hw_snif_el_wr_n_rd_sel;
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_cli_mux_s
	  ipa_log_buf_hw_snif_el_cli_mux;
	struct ipa_hwio_def_ipa_state_acl_s
	  ipa_state_acl;
	struct ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_s
	  ipa_sys_pkt_proc_cntxt_base;
	struct ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_msb_s
	  ipa_sys_pkt_proc_cntxt_base_msb;
	struct ipa_hwio_def_ipa_local_pkt_proc_cntxt_base_s
	  ipa_local_pkt_proc_cntxt_base;
	struct ipa_hwio_def_ipa_rsrc_grp_cfg_s
	  ipa_rsrc_grp_cfg;
	struct ipa_hwio_def_ipa_comp_cfg_s
	  ipa_comp_cfg;
};

/*
 * General IPA register save data struct
 */
struct ipa_reg_save_gen_ee_s {
	struct ipa_hwio_def_ipa_irq_stts_ee_n_s
	  ipa_irq_stts_ee_n;
	struct ipa_hwio_def_ipa_irq_en_ee_n_s
	  ipa_irq_en_ee_n;
	struct ipa_hwio_def_ipa_fec_addr_ee_n_s
	  ipa_fec_addr_ee_n;
	struct ipa_hwio_def_ipa_fec_attr_ee_n_s
	  ipa_fec_attr_ee_n;
	struct ipa_hwio_def_ipa_snoc_fec_ee_n_s
	  ipa_snoc_fec_ee_n;
	struct ipa_hwio_def_ipa_holb_drop_irq_info_ee_n_s
	  ipa_holb_drop_irq_info_ee_n;
	struct ipa_hwio_def_ipa_suspend_irq_info_ee_n_s
	  ipa_suspend_irq_info_ee_n;
	struct ipa_hwio_def_ipa_suspend_irq_en_ee_n_s
	  ipa_suspend_irq_en_ee_n;
};

/*
 * Pipe Endp IPA register save data struct
 */
struct ipa_reg_save_pipe_endp_s {
	struct ipa_hwio_def_ipa_endp_init_ctrl_n_s
	  ipa_endp_init_ctrl_n;
	struct ipa_hwio_def_ipa_endp_init_ctrl_scnd_n_s
	  ipa_endp_init_ctrl_scnd_n;
	struct ipa_hwio_def_ipa_endp_init_cfg_n_s
	  ipa_endp_init_cfg_n;
	struct ipa_hwio_def_ipa_endp_init_nat_n_s
	  ipa_endp_init_nat_n;
	struct ipa_hwio_def_ipa_endp_init_hdr_n_s
	  ipa_endp_init_hdr_n;
	struct ipa_hwio_def_ipa_endp_init_hdr_ext_n_s
	  ipa_endp_init_hdr_ext_n;
	struct ipa_hwio_def_ipa_endp_init_hdr_metadata_mask_n_s
	  ipa_endp_init_hdr_metadata_mask_n;
	struct ipa_hwio_def_ipa_endp_init_hdr_metadata_n_s
	  ipa_endp_init_hdr_metadata_n;
	struct ipa_hwio_def_ipa_endp_init_mode_n_s
	  ipa_endp_init_mode_n;
	struct ipa_hwio_def_ipa_endp_init_aggr_n_s
	  ipa_endp_init_aggr_n;
	struct ipa_hwio_def_ipa_endp_init_hol_block_en_n_s
	  ipa_endp_init_hol_block_en_n;
	struct ipa_hwio_def_ipa_endp_init_hol_block_timer_n_s
	  ipa_endp_init_hol_block_timer_n;
	struct ipa_hwio_def_ipa_endp_init_deaggr_n_s
	  ipa_endp_init_deaggr_n;
	struct ipa_hwio_def_ipa_endp_status_n_s
	  ipa_endp_status_n;
	struct ipa_hwio_def_ipa_endp_init_rsrc_grp_n_s
	  ipa_endp_init_rsrc_grp_n;
	struct ipa_hwio_def_ipa_endp_init_seq_n_s
	  ipa_endp_init_seq_n;
	struct ipa_hwio_def_ipa_endp_gsi_cfg_tlv_n_s
	  ipa_endp_gsi_cfg_tlv_n;
	struct ipa_hwio_def_ipa_endp_gsi_cfg_aos_n_s
	  ipa_endp_gsi_cfg_aos_n;
	struct ipa_hwio_def_ipa_endp_gsi_cfg1_n_s
	  ipa_endp_gsi_cfg1_n;
	struct ipa_hwio_def_ipa_endp_gsi_cfg2_n_s
	  ipa_endp_gsi_cfg2_n;
	struct ipa_hwio_def_ipa_endp_filter_router_hsh_cfg_n_s
	  ipa_endp_filter_router_hsh_cfg_n;
};

/*
 * Pipe IPA register save data struct
 */
struct ipa_reg_save_pipe_s {
	u8				active;
	struct ipa_reg_save_pipe_endp_s endp;
};

/*
 * HWP IPA register save data struct
 */
struct ipa_reg_save_hwp_s {
	struct ipa_hwio_def_ipa_uc_qmb_sys_addr_s
	  ipa_uc_qmb_sys_addr;
	struct ipa_hwio_def_ipa_uc_qmb_local_addr_s
	  ipa_uc_qmb_local_addr;
	struct ipa_hwio_def_ipa_uc_qmb_length_s
	  ipa_uc_qmb_length;
	struct ipa_hwio_def_ipa_uc_qmb_trigger_s
	  ipa_uc_qmb_trigger;
	struct ipa_hwio_def_ipa_uc_qmb_pending_tid_s
	  ipa_uc_qmb_pending_tid;
	struct ipa_hwio_def_ipa_uc_qmb_completed_rd_fifo_peek_s
	  ipa_uc_qmb_completed_rd_fifo_peek;
	struct ipa_hwio_def_ipa_uc_qmb_completed_wr_fifo_peek_s
	  ipa_uc_qmb_completed_wr_fifo_peek;
	struct ipa_hwio_def_ipa_uc_qmb_misc_s
	  ipa_uc_qmb_misc;
	struct ipa_hwio_def_ipa_uc_qmb_status_s
	  ipa_uc_qmb_status;
	struct ipa_hwio_def_ipa_uc_qmb_bus_attrib_s
	  ipa_uc_qmb_bus_attrib;
};

/*
 * IPA TESTBUS entry struct
 */
struct ipa_reg_save_ipa_testbus_entry_s {
	union ipa_hwio_def_ipa_debug_data_sel_u testbus_sel;
	union ipa_hwio_def_ipa_debug_data_u testbus_data;
};
/* IPA TESTBUS per EP struct */
struct ipa_reg_save_ipa_testbus_ep_s {
	struct ipa_reg_save_ipa_testbus_entry_s
	entry_ep[IPA_TESTBUS_SEL_INTERNAL_PIPE_MAX + 1]
	[IPA_TESTBUS_SEL_EXTERNAL_MAX + 1];
};

/* IPA TESTBUS save data struct */
struct ipa_reg_save_ipa_testbus_s {
	struct ipa_reg_save_ipa_testbus_ep_s
	  ep[IPA_TESTBUS_SEL_EP_MAX + 1];
};

/*
 * Debug IPA register save data struct
 */
struct ipa_reg_save_dbg_s {
	struct ipa_hwio_def_ipa_debug_data_s
	  ipa_debug_data;
	struct ipa_hwio_def_ipa_step_mode_status_s
	  ipa_step_mode_status;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_0_cmd_s
	  ipa_rx_splt_cmdq_0_cmd;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_0_data_rd_0_s
	  ipa_rx_splt_cmdq_0_data_rd_0;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_0_data_rd_1_s
	  ipa_rx_splt_cmdq_0_data_rd_1;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_0_data_rd_2_s
	  ipa_rx_splt_cmdq_0_data_rd_2;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_0_data_rd_3_s
	  ipa_rx_splt_cmdq_0_data_rd_3;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_0_status_s
	  ipa_rx_splt_cmdq_0_status;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_1_cmd_s
	  ipa_rx_splt_cmdq_1_cmd;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_1_data_rd_0_s
	  ipa_rx_splt_cmdq_1_data_rd_0;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_1_data_rd_1_s
	  ipa_rx_splt_cmdq_1_data_rd_1;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_1_data_rd_2_s
	  ipa_rx_splt_cmdq_1_data_rd_2;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_1_data_rd_3_s
	  ipa_rx_splt_cmdq_1_data_rd_3;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_1_status_s
	  ipa_rx_splt_cmdq_1_status;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_2_cmd_s
	  ipa_rx_splt_cmdq_2_cmd;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_2_data_rd_0_s
	  ipa_rx_splt_cmdq_2_data_rd_0;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_2_data_rd_1_s
	  ipa_rx_splt_cmdq_2_data_rd_1;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_2_data_rd_2_s
	  ipa_rx_splt_cmdq_2_data_rd_2;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_2_data_rd_3_s
	  ipa_rx_splt_cmdq_2_data_rd_3;
	struct ipa_hwio_def_ipa_rx_splt_cmdq_2_status_s
	  ipa_rx_splt_cmdq_2_status;
	struct ipa_hwio_def_ipa_rx_hps_cmdq_cmd_s
	  ipa_rx_hps_cmdq_cmd;
	union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_0_u
		ipa_rx_hps_cmdq_data_rd_0_arr[
		IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_1_u
		ipa_rx_hps_cmdq_data_rd_1_arr[
		IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_2_u
		ipa_rx_hps_cmdq_data_rd_2_arr[
		IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_3_u
		ipa_rx_hps_cmdq_data_rd_3_arr[
		IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_rx_hps_cmdq_count_u
	  ipa_rx_hps_cmdq_count_arr[IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_rx_hps_cmdq_status_u
	  ipa_rx_hps_cmdq_status_arr[IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS];
	struct ipa_hwio_def_ipa_rx_hps_cmdq_status_empty_s
	  ipa_rx_hps_cmdq_status_empty;
	struct ipa_hwio_def_ipa_rx_hps_clients_min_depth_0_s
	  ipa_rx_hps_clients_min_depth_0;
	struct ipa_hwio_def_ipa_rx_hps_clients_max_depth_0_s
	  ipa_rx_hps_clients_max_depth_0;
	struct ipa_hwio_def_ipa_hps_dps_cmdq_cmd_s
	  ipa_hps_dps_cmdq_cmd;
	union ipa_hwio_def_ipa_hps_dps_cmdq_data_rd_0_u
		ipa_hps_dps_cmdq_data_rd_0_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	union ipa_hwio_def_ipa_hps_dps_cmdq_count_u
		ipa_hps_dps_cmdq_count_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	union ipa_hwio_def_ipa_hps_dps_cmdq_status_u
		ipa_hps_dps_cmdq_status_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	struct ipa_hwio_def_ipa_hps_dps_cmdq_status_empty_s
		ipa_hps_dps_cmdq_status_empty;
	union ipa_hwio_def_ipa_rx_hps_cmdq_cfg_wr_u
		ipa_rx_hps_cmdq_cfg_wr;
	union ipa_hwio_def_ipa_rx_hps_cmdq_cfg_rd_u
		ipa_rx_hps_cmdq_cfg_rd;

	struct ipa_hwio_def_ipa_dps_tx_cmdq_cmd_s
	  ipa_dps_tx_cmdq_cmd;
	union ipa_hwio_def_ipa_dps_tx_cmdq_data_rd_0_u
		ipa_dps_tx_cmdq_data_rd_0_arr[
		IPA_DEBUG_CMDQ_DPS_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_dps_tx_cmdq_count_u
		ipa_dps_tx_cmdq_count_arr[IPA_DEBUG_CMDQ_DPS_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_dps_tx_cmdq_status_u
	ipa_dps_tx_cmdq_status_arr[IPA_DEBUG_CMDQ_DPS_SELECT_NUM_GROUPS];
	struct ipa_hwio_def_ipa_dps_tx_cmdq_status_empty_s
	  ipa_dps_tx_cmdq_status_empty;

	struct ipa_hwio_def_ipa_ackmngr_cmdq_cmd_s
	  ipa_ackmngr_cmdq_cmd;
	union ipa_hwio_def_ipa_ackmngr_cmdq_data_rd_u
		ipa_ackmngr_cmdq_data_rd_arr[
		IPA_DEBUG_CMDQ_ACK_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_ackmngr_cmdq_count_u
	  ipa_ackmngr_cmdq_count_arr[IPA_DEBUG_CMDQ_ACK_SELECT_NUM_GROUPS];
	union ipa_hwio_def_ipa_ackmngr_cmdq_status_u
		ipa_ackmngr_cmdq_status_arr[
		IPA_DEBUG_CMDQ_ACK_SELECT_NUM_GROUPS];
	struct ipa_hwio_def_ipa_ackmngr_cmdq_status_empty_s
	  ipa_ackmngr_cmdq_status_empty;

	struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_cmd_s
	  ipa_prod_ackmngr_cmdq_cmd;
	union ipa_hwio_def_ipa_prod_ackmngr_cmdq_data_rd_u
		ipa_prod_ackmngr_cmdq_data_rd_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	union ipa_hwio_def_ipa_prod_ackmngr_cmdq_count_u
		ipa_prod_ackmngr_cmdq_count_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	union ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_u
		ipa_prod_ackmngr_cmdq_status_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_empty_s
	  ipa_prod_ackmngr_cmdq_status_empty;

	struct ipa_hwio_def_ipa_ntf_tx_cmdq_cmd_s
	  ipa_ntf_tx_cmdq_cmd;
	union ipa_hwio_def_ipa_ntf_tx_cmdq_data_rd_0_u
		ipa_ntf_tx_cmdq_data_rd_0_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	union ipa_hwio_def_ipa_ntf_tx_cmdq_count_u
		ipa_ntf_tx_cmdq_count_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	union ipa_hwio_def_ipa_ntf_tx_cmdq_status_u
		ipa_ntf_tx_cmdq_status_arr[IPA_TESTBUS_SEL_EP_MAX + 1];
	struct ipa_hwio_def_ipa_ntf_tx_cmdq_status_empty_s
	  ipa_ntf_tx_cmdq_status_empty;

	union ipa_hwio_def_ipa_rsrc_mngr_db_rsrc_read_u
		ipa_rsrc_mngr_db_rsrc_read_arr[IPA_RSCR_MNGR_DB_RSRC_TYPE_MAX +
					       1][IPA_RSCR_MNGR_DB_RSRC_ID_MAX
						  + 1];
	union ipa_hwio_def_ipa_rsrc_mngr_db_list_read_u
		ipa_rsrc_mngr_db_list_read_arr[IPA_RSCR_MNGR_DB_RSRC_TYPE_MAX +
					       1][IPA_RSCR_MNGR_DB_RSRC_ID_MAX
						  + 1];
};

/* Source Resource Group IPA register save data struct */
struct ipa_reg_save_src_rsrc_grp_s {
	struct ipa_hwio_def_ipa_src_rsrc_grp_01_rsrc_type_n_s
	  ipa_src_rsrc_grp_01_rsrc_type_n;
	struct ipa_hwio_def_ipa_src_rsrc_grp_23_rsrc_type_n_s
	  ipa_src_rsrc_grp_23_rsrc_type_n;
};

/* Source Resource Group IPA register save data struct */
struct ipa_reg_save_dst_rsrc_grp_s {
	struct ipa_hwio_def_ipa_dst_rsrc_grp_01_rsrc_type_n_s
	  ipa_dst_rsrc_grp_01_rsrc_type_n;
	struct ipa_hwio_def_ipa_dst_rsrc_grp_23_rsrc_type_n_s
	  ipa_dst_rsrc_grp_23_rsrc_type_n;
};

/* Source Resource Group Count IPA register save data struct */
struct ipa_reg_save_src_rsrc_cnt_s {
	struct ipa_hwio_def_ipa_src_rsrc_grp_0123_rsrc_type_cnt_n_s
	  ipa_src_rsrc_grp_0123_rsrc_type_cnt_n;
};

/* Destination Resource Group Count IPA register save data struct */
struct ipa_reg_save_dst_rsrc_cnt_s {
	struct ipa_hwio_def_ipa_dst_rsrc_grp_0123_rsrc_type_cnt_n_s
	  ipa_dst_rsrc_grp_0123_rsrc_type_cnt_n;
};

/* GSI General register save data struct */
struct ipa_reg_save_gsi_gen_s {
	struct gsi_hwio_def_gsi_cfg_s
	  gsi_cfg;
	struct gsi_hwio_def_gsi_ree_cfg_s
	  gsi_ree_cfg;
	struct ipa_hwio_def_ipa_gsi_top_gsi_inst_ram_n_s
	  ipa_gsi_top_gsi_inst_ram_n;
};

/* GSI General EE register save data struct */
struct ipa_reg_save_gsi_gen_ee_s {
	struct gsi_hwio_def_gsi_manager_ee_qos_n_s
	  gsi_manager_ee_qos_n;
	struct gsi_hwio_def_ee_n_gsi_status_s
	  ee_n_gsi_status;
	struct gsi_hwio_def_ee_n_cntxt_type_irq_s
	  ee_n_cntxt_type_irq;
	struct gsi_hwio_def_ee_n_cntxt_type_irq_msk_s
	  ee_n_cntxt_type_irq_msk;
	struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_s
	  ee_n_cntxt_src_gsi_ch_irq;
	struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_s
	  ee_n_cntxt_src_ev_ch_irq;
	struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_msk_s
	  ee_n_cntxt_src_gsi_ch_irq_msk;
	struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_msk_s
	  ee_n_cntxt_src_ev_ch_irq_msk;
	struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_s
	  ee_n_cntxt_src_ieob_irq;
	struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_msk_s
	  ee_n_cntxt_src_ieob_irq_msk;
	struct gsi_hwio_def_ee_n_cntxt_gsi_irq_stts_s
	  ee_n_cntxt_gsi_irq_stts;
	struct gsi_hwio_def_ee_n_cntxt_glob_irq_stts_s
	  ee_n_cntxt_glob_irq_stts;
	struct gsi_hwio_def_ee_n_error_log_s
	  ee_n_error_log;
	struct gsi_hwio_def_ee_n_cntxt_scratch_0_s
	  ee_n_cntxt_scratch_0;
	struct gsi_hwio_def_ee_n_cntxt_scratch_1_s
	  ee_n_cntxt_scratch_1;
};

/* GSI QSB debug register save data struct */
struct ipa_reg_save_gsi_qsb_debug_s {
	struct
	  gsi_hwio_def_gsi_debug_qsb_log_last_misc_idn_s
		qsb_log_last_misc[GSI_HW_QSB_LOG_MISC_MAX];
};

static u32 ipa_reg_save_gsi_ch_test_bus_selector_array[] = {
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_ZEROS,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_2,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_3,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MCS_4,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_DB_ENG,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_2,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_3,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_4,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_5,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_6,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_REE_7,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_2,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_3,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_4,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_EVE_5,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IE_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IE_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_2,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_3,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_IC_4,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_2,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_MOQA_3,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_2,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_TMR_3,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_1,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_2,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_RD_WR_3,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_CSR,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_SDMA_0,
	HWIO_GSI_DEBUG_TEST_BUS_SELECTOR_SDMA_1,
};

/*
 * GSI QSB debug bus register save data struct
 */
struct ipa_reg_save_gsi_test_bus_s {
	u32 test_bus_selector[
		ARRAY_SIZE(ipa_reg_save_gsi_ch_test_bus_selector_array)];
	struct
	  gsi_hwio_def_gsi_test_bus_reg_s
	  test_bus_reg[ARRAY_SIZE(ipa_reg_save_gsi_ch_test_bus_selector_array)];
};

/* GSI debug MCS registers save data struct */
struct ipa_reg_save_gsi_mcs_regs_s {
	struct
	  gsi_hwio_def_gsi_debug_sw_rf_n_read_s
		mcs_reg[HWIO_GSI_DEBUG_SW_RF_n_READ_MAXn + 1];
};

/* GSI debug counters save data struct */
struct ipa_reg_save_gsi_debug_cnt_s {
	struct
	  gsi_hwio_def_gsi_debug_countern_s
		cnt[HWIO_GSI_DEBUG_COUNTERn_MAXn + 1];
};

/* GSI IRAM pointers (IEP) save data struct */
struct ipa_reg_save_gsi_iram_ptr_regs_s {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_cmd_s
	  ipa_gsi_top_gsi_iram_ptr_ch_cmd;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ee_generic_cmd_s
	  ipa_gsi_top_gsi_iram_ptr_ee_generic_cmd;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_db_s
	  ipa_gsi_top_gsi_iram_ptr_ch_db;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ev_db_s
	  ipa_gsi_top_gsi_iram_ptr_ev_db;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_new_re_s
	  ipa_gsi_top_gsi_iram_ptr_new_re;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_dis_comp_s
	  ipa_gsi_top_gsi_iram_ptr_ch_dis_comp;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_empty_s
	  ipa_gsi_top_gsi_iram_ptr_ch_empty;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_event_gen_comp_s
	  ipa_gsi_top_gsi_iram_ptr_event_gen_comp;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_timer_expired_s
	  ipa_gsi_top_gsi_iram_ptr_timer_expired;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_write_eng_comp_s
	  ipa_gsi_top_gsi_iram_ptr_write_eng_comp;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_read_eng_comp_s
	  ipa_gsi_top_gsi_iram_ptr_read_eng_comp;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_uc_gp_int_s
	  ipa_gsi_top_gsi_iram_ptr_uc_gp_int;
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_int_mod_stopped_s
	  ipa_gsi_top_gsi_iram_ptr_int_mod_stopped;
};

/* GSI SHRAM pointers save data struct */
struct ipa_reg_save_gsi_shram_ptr_regs_s {
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ch_cntxt_base_addr_s
	  ipa_gsi_top_gsi_shram_ptr_ch_cntxt_base_addr;
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ev_cntxt_base_addr_s
	  ipa_gsi_top_gsi_shram_ptr_ev_cntxt_base_addr;
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_storage_base_addr_s
	  ipa_gsi_top_gsi_shram_ptr_re_storage_base_addr;
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_esc_buf_base_addr_s
	  ipa_gsi_top_gsi_shram_ptr_re_esc_buf_base_addr;
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ee_scrach_base_addr_s
	  ipa_gsi_top_gsi_shram_ptr_ee_scrach_base_addr;
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_func_stack_base_addr_s
	  ipa_gsi_top_gsi_shram_ptr_func_stack_base_addr;
};

/* GSI debug register save data struct */
struct ipa_reg_save_gsi_debug_s {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_busy_reg_s
	  ipa_gsi_top_gsi_debug_busy_reg;
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_event_pending_s
	  ipa_gsi_top_gsi_debug_event_pending;
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_timer_pending_s
	  ipa_gsi_top_gsi_debug_timer_pending;
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_rd_wr_pending_s
	  ipa_gsi_top_gsi_debug_rd_wr_pending;
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_from_sw_s
	  ipa_gsi_top_gsi_debug_pc_from_sw;
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_sw_stall_s
	  ipa_gsi_top_gsi_debug_sw_stall;
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_for_debug_s
	  ipa_gsi_top_gsi_debug_pc_for_debug;
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_qsb_log_err_trns_id_s
	  ipa_gsi_top_gsi_debug_qsb_log_err_trns_id;
	struct ipa_reg_save_gsi_qsb_debug_s		gsi_qsb_debug;
	struct ipa_reg_save_gsi_test_bus_s		gsi_test_bus;
	struct ipa_reg_save_gsi_mcs_regs_s		gsi_mcs_regs;
	struct ipa_reg_save_gsi_debug_cnt_s		gsi_cnt_regs;
	struct ipa_reg_save_gsi_iram_ptr_regs_s		gsi_iram_ptrs;
	struct ipa_reg_save_gsi_shram_ptr_regs_s	gsi_shram_ptrs;
};

/* GSI MCS channel scratch registers save data struct */
struct ipa_reg_save_gsi_mcs_channel_scratch_regs_s {
	struct gsi_hwio_def_gsi_shram_n_s
	  scratch4;
	struct gsi_hwio_def_gsi_shram_n_s
	  scratch5;
};

/* GSI Channel Context register save data struct */
struct ipa_reg_save_gsi_ch_cntxt_per_ep_s {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_0_s
	  ee_n_gsi_ch_k_cntxt_0;
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_1_s
	  ee_n_gsi_ch_k_cntxt_1;
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_2_s
	  ee_n_gsi_ch_k_cntxt_2;
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_3_s
	  ee_n_gsi_ch_k_cntxt_3;
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_4_s
	  ee_n_gsi_ch_k_cntxt_4;
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_5_s
	  ee_n_gsi_ch_k_cntxt_5;
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_6_s
	  ee_n_gsi_ch_k_cntxt_6;
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_7_s
	  ee_n_gsi_ch_k_cntxt_7;
	struct gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_read_ptr_s
	  ee_n_gsi_ch_k_re_fetch_read_ptr;
	struct gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_write_ptr_s
	  ee_n_gsi_ch_k_re_fetch_write_ptr;
	struct gsi_hwio_def_ee_n_gsi_ch_k_qos_s
	  ee_n_gsi_ch_k_qos;
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_0_s
	  ee_n_gsi_ch_k_scratch_0;
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_1_s
	  ee_n_gsi_ch_k_scratch_1;
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_2_s
	  ee_n_gsi_ch_k_scratch_2;
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_3_s
	  ee_n_gsi_ch_k_scratch_3;
	struct gsi_hwio_def_gsi_debug_ee_n_ch_k_vp_table_s
	  gsi_debug_ee_n_ch_k_vp_table;
	struct ipa_reg_save_gsi_mcs_channel_scratch_regs_s mcs_channel_scratch;
};

/* GSI Event Context register save data struct */
struct ipa_reg_save_gsi_evt_cntxt_per_ep_s {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_0_s
	  ee_n_ev_ch_k_cntxt_0;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_1_s
	  ee_n_ev_ch_k_cntxt_1;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_2_s
	  ee_n_ev_ch_k_cntxt_2;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_3_s
	  ee_n_ev_ch_k_cntxt_3;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_4_s
	  ee_n_ev_ch_k_cntxt_4;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_5_s
	  ee_n_ev_ch_k_cntxt_5;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_6_s
	  ee_n_ev_ch_k_cntxt_6;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_7_s
	  ee_n_ev_ch_k_cntxt_7;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_8_s
	  ee_n_ev_ch_k_cntxt_8;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_9_s
	  ee_n_ev_ch_k_cntxt_9;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_10_s
	  ee_n_ev_ch_k_cntxt_10;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_11_s
	  ee_n_ev_ch_k_cntxt_11;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_12_s
	  ee_n_ev_ch_k_cntxt_12;
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_13_s
	  ee_n_ev_ch_k_cntxt_13;
	struct gsi_hwio_def_ee_n_ev_ch_k_scratch_0_s
	  ee_n_ev_ch_k_scratch_0;
	struct gsi_hwio_def_ee_n_ev_ch_k_scratch_1_s
	  ee_n_ev_ch_k_scratch_1;
	struct gsi_hwio_def_gsi_debug_ee_n_ev_k_vp_table_s
	  gsi_debug_ee_n_ev_k_vp_table;
};

/* GSI FIFO status register save data struct */
struct ipa_reg_save_gsi_fifo_status_s {
	union ipa_hwio_def_ipa_gsi_fifo_status_ctrl_u	gsi_fifo_status_ctrl;
	union ipa_hwio_def_ipa_gsi_tlv_fifo_status_u	gsi_tlv_fifo_status;
	union ipa_hwio_def_ipa_gsi_tlv_pub_fifo_status_u
							gsi_tlv_pub_fifo_status;
	union ipa_hwio_def_ipa_gsi_aos_fifo_status_u	gsi_aos_fifo_status;
};

/* GSI Channel Context register save top level data struct */
struct ipa_reg_save_gsi_ch_cntxt_s {
	struct ipa_reg_save_gsi_ch_cntxt_per_ep_s
		a7[IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_A7];
	struct ipa_reg_save_gsi_ch_cntxt_per_ep_s
		uc[IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_UC];
};

/* GSI Event Context register save top level data struct */
struct ipa_reg_save_gsi_evt_cntxt_s {
	struct ipa_reg_save_gsi_evt_cntxt_per_ep_s
		a7[IPA_HW_REG_SAVE_GSI_NUM_EVT_CNTXT_A7];
	struct ipa_reg_save_gsi_evt_cntxt_per_ep_s
		uc[IPA_HW_REG_SAVE_GSI_NUM_EVT_CNTXT_UC];
};

/* Top level IPA register save data struct */
struct ipa_regs_save_hierarchy_s {
	struct ipa_gen_regs_s			gen;
	struct ipa_reg_save_gen_ee_s	gen_ee[IPA_HW_EE_MAX];
	struct ipa_reg_save_hwp_s		hwp;
	struct ipa_reg_save_dbg_s		dbg;
	struct ipa_reg_save_ipa_testbus_s	*testbus;
	struct ipa_reg_save_pipe_s		pipes[IPA_HW_PIPE_ID_MAX];
	struct ipa_reg_save_src_rsrc_grp_s
	  src_rsrc_grp[IPA_HW_SRC_RSRP_TYPE_MAX];
	struct ipa_reg_save_dst_rsrc_grp_s
	  dst_rsrc_grp[IPA_HW_DST_RSRP_TYPE_MAX];
	struct ipa_reg_save_src_rsrc_cnt_s
	  src_rsrc_cnt[IPA_HW_SRC_RSRP_TYPE_MAX];
	struct ipa_reg_save_dst_rsrc_cnt_s
	  dst_rsrc_cnt[IPA_HW_DST_RSRP_TYPE_MAX];
};

/* Top level GSI register save data struct */
struct gsi_regs_save_hierarchy_s {
	u32 fw_ver;
	struct ipa_reg_save_gsi_gen_s		gen;
	struct ipa_reg_save_gsi_gen_ee_s	gen_ee[IPA_REG_SAVE_GSI_NUM_EE];
	struct ipa_reg_save_gsi_ch_cntxt_s	ch_cntxt;
	struct ipa_reg_save_gsi_evt_cntxt_s	evt_cntxt;
	struct ipa_reg_save_gsi_debug_s		debug;
};

/* Source resources for a resource group */
struct ipa_reg_save_src_rsrc_cnts_s {
	u8 pkt_cntxt;
	u8 descriptor_list;
	u8 data_descriptor_buffer;
	u8 hps_dmars;
	u8 reserved_acks;
};

/* Destination resources for a resource group */
struct ipa_reg_save_dst_rsrc_cnts_s {
	u8 reserved_sectors;
	u8 dps_dmars;
};

/* Resource count structure for a resource group */
struct ipa_reg_save_rsrc_cnts_per_grp_s {
	/* Resource group number */
	u8 resource_group;
	/* Source resources for a resource group */
	struct ipa_reg_save_src_rsrc_cnts_s src;
	/* Destination resources for a resource group */
	struct ipa_reg_save_dst_rsrc_cnts_s dst;
};

/* Top level resource count structure */
struct ipa_reg_save_rsrc_cnts_s {
	/* Resource count structure for PCIE group */
	struct ipa_reg_save_rsrc_cnts_per_grp_s pcie;
	/* Resource count structure for DDR group */
	struct ipa_reg_save_rsrc_cnts_per_grp_s ddr;
};

/* Top level IPA and GSI registers save data struct */
struct regs_save_hierarchy_s {
	struct ipa_regs_save_hierarchy_s
		ipa;
	struct gsi_regs_save_hierarchy_s
		gsi;
	bool
		pkt_ctntx_active[IPA_HW_PKT_CTNTX_MAX];
	union ipa_hwio_def_ipa_ctxh_ctrl_u
		pkt_ctntxt_lock;
	enum ipa_hw_pkt_cntxt_state_e
		pkt_cntxt_state[IPA_HW_PKT_CTNTX_MAX];
	struct ipa_pkt_ctntx_s
		pkt_ctntx[IPA_HW_PKT_CTNTX_MAX];
	struct ipa_reg_save_rsrc_cnts_s
		rsrc_cnts;
	struct ipa_reg_save_gsi_fifo_status_s
		gsi_fifo_status[IPA_HW_PIPE_ID_MAX];
};

/*
 * The following section deals with handling IPA registers' memory
 * access relative to pre-defined memory protection schemes
 * (ie. "access control").
 *
 * In a nut shell, the intent of the data stuctures below is to allow
 * higher level register accessors to be unaware of what really is
 * going on at the lowest level (ie. real vs non-real access).  This
 * methodology is also designed to allow for platform specific "access
 * maps."
 */

/*
 * Function for doing an actual read
 */
static inline u32
act_read(void __iomem *addr)
{
	u32 val = ioread32(addr);

	return val;
}

/*
 * Function for doing an actual write
 */
static inline void
act_write(void __iomem *addr, u32 val)
{
	iowrite32(val, addr);
}

/*
 * Function that pretends to do a read
 */
static inline u32
nop_read(void __iomem *addr)
{
	return IPA_MEM_INIT_VAL;
}

/*
 * Function that pretends to do a write
 */
static inline void
nop_write(void __iomem *addr, u32 val)
{
}

/*
 * The following are used to define struct reg_access_funcs_s below...
 */
typedef u32 (*reg_read_func_t)(
	void __iomem *addr);
typedef void (*reg_write_func_t)(
	void __iomem *addr,
	u32 val);

/*
 * The following in used to define io_matrix[] below...
 */
struct reg_access_funcs_s {
	reg_read_func_t  read;
	reg_write_func_t write;
};

/*
 * The following will be used to appropriately index into the
 * read/write combos defined in io_matrix[] below...
 */
#define AA_COMBO 0 /* actual read, actual write */
#define AN_COMBO 1 /* actual read, no-op write  */
#define NA_COMBO 2 /* no-op read,  actual write */
#define NN_COMBO 3 /* no-op read,  no-op write  */

/*
 * The following will be used to dictate registers' access methods
 * relative to the state of secure debug...whether it's enabled or
 * disabled.
 *
 * NOTE: The table below defines all access combinations.
 */
static struct reg_access_funcs_s io_matrix[] = {
	{ act_read, act_write }, /* the AA_COMBO */
	{ act_read, nop_write }, /* the AN_COMBO */
	{ nop_read, act_write }, /* the NA_COMBO */
	{ nop_read, nop_write }, /* the NN_COMBO */
};

/*
 * The following will be used to define and drive IPA's register
 * access rules.
 */
struct reg_mem_access_map_t {
	u32 addr_range_begin;
	u32 addr_range_end;
	struct reg_access_funcs_s *access[2];
};

#endif /* #if !defined(_IPA_REG_DUMP_H_) */
