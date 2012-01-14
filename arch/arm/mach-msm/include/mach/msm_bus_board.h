/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_BUS_BOARD_H
#define __ASM_ARCH_MSM_BUS_BOARD_H

#include <linux/types.h>
#include <linux/input.h>

enum context {
	DUAL_CTX,
	ACTIVE_CTX,
	NUM_CTX
};

struct msm_bus_fabric_registration {
	unsigned int id;
	char *name;
	struct msm_bus_node_info *info;
	unsigned int len;
	int ahb;
	const char *fabclk[NUM_CTX];
	unsigned int offset;
	unsigned int haltid;
	unsigned int rpm_enabled;
	const unsigned int nmasters;
	const unsigned int nslaves;
	const unsigned int ntieredslaves;
	bool il_flag;
	const struct msm_bus_board_algorithm *board_algo;
};

enum msm_bus_bw_tier_type {
	MSM_BUS_BW_TIER1 = 1,
	MSM_BUS_BW_TIER2,
	MSM_BUS_BW_COUNT,
	MSM_BUS_BW_SIZE = 0x7FFFFFFF,
};

struct msm_bus_halt_vector {
	uint32_t haltval;
	uint32_t haltmask;
};

extern struct msm_bus_fabric_registration msm_bus_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_cpss_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_def_fab_pdata;

extern struct msm_bus_fabric_registration msm_bus_8960_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_8064_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_9615_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_9615_def_fab_pdata;
void msm_bus_rpm_set_mt_mask(void);
int msm_bus_board_rpm_get_il_ids(uint16_t *id);
int msm_bus_board_get_iid(int id);

/*
 * These macros specify the convention followed for allocating
 * ids to fabrics, masters and slaves for 8x60.
 *
 * A node can be identified as a master/slave/fabric by using
 * these ids.
 */
#define FABRIC_ID_KEY 1024
#define SLAVE_ID_KEY ((FABRIC_ID_KEY) >> 1)
#define NUM_FAB 5
#define MAX_FAB_KEY 7168  /* OR(All fabric ids) */

#define GET_FABID(id) ((id) & MAX_FAB_KEY)

#define NODE_ID(id) ((id) & (FABRIC_ID_KEY - 1))
#define IS_SLAVE(id) ((NODE_ID(id)) >= SLAVE_ID_KEY ? 1 : 0)
#define CHECK_ID(iid, id) (((iid & id) != id) ? -ENXIO : iid)

/*
 * The following macros are used to format the data for port halt
 * and unhalt requests.
 */
#define MSM_BUS_CLK_HALT 0x1
#define MSM_BUS_CLK_HALT_MASK 0x1
#define MSM_BUS_CLK_HALT_FIELDSIZE 0x1
#define MSM_BUS_CLK_UNHALT 0x0

#define MSM_BUS_MASTER_SHIFT(master, fieldsize) \
	((master) * (fieldsize))

#define MSM_BUS_SET_BITFIELD(word, fieldmask, fieldvalue) \
	{	\
		(word) &= ~(fieldmask);	\
		(word) |= (fieldvalue);	\
	}


#define MSM_BUS_MASTER_HALT(u32haltmask, u32haltval, master) \
	MSM_BUS_SET_BITFIELD(u32haltmask, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\
	MSM_BUS_SET_BITFIELD(u32haltval, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_HALT<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\

#define MSM_BUS_MASTER_UNHALT(u32haltmask, u32haltval, master) \
	MSM_BUS_SET_BITFIELD(u32haltmask, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\
	MSM_BUS_SET_BITFIELD(u32haltval, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_UNHALT<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\

/* Topology related enums */
enum msm_bus_fabric_type {
	MSM_BUS_FAB_DEFAULT = 0,
	MSM_BUS_FAB_APPSS = 0,
	MSM_BUS_FAB_SYSTEM = 1024,
	MSM_BUS_FAB_MMSS = 2048,
	MSM_BUS_FAB_SYSTEM_FPB = 3072,
	MSM_BUS_FAB_CPSS_FPB = 4096,
};

enum msm_bus_fabric_master_type {
	MSM_BUS_MASTER_FIRST = 1,
	MSM_BUS_MASTER_AMPSS_M0 = 1,
	MSM_BUS_MASTER_AMPSS_M1,
	MSM_BUS_APPSS_MASTER_FAB_MMSS,
	MSM_BUS_APPSS_MASTER_FAB_SYSTEM,

	MSM_BUS_SYSTEM_MASTER_FAB_APPSS,
	MSM_BUS_MASTER_SPS,
	MSM_BUS_MASTER_ADM_PORT0,
	MSM_BUS_MASTER_ADM_PORT1,
	MSM_BUS_SYSTEM_MASTER_ADM1_PORT0,
	MSM_BUS_MASTER_ADM1_PORT1,
	MSM_BUS_MASTER_LPASS_PROC,
	MSM_BUS_MASTER_MSS_PROCI,
	MSM_BUS_MASTER_MSS_PROCD,
	MSM_BUS_MASTER_MSS_MDM_PORT0,
	MSM_BUS_MASTER_LPASS,
	MSM_BUS_SYSTEM_MASTER_CPSS_FPB,
	MSM_BUS_SYSTEM_MASTER_SYSTEM_FPB,
	MSM_BUS_SYSTEM_MASTER_MMSS_FPB,
	MSM_BUS_MASTER_ADM1_CI,
	MSM_BUS_MASTER_ADM0_CI,
	MSM_BUS_MASTER_MSS_MDM_PORT1,

	MSM_BUS_MASTER_MDP_PORT0,
	MSM_BUS_MASTER_MDP_PORT1,
	MSM_BUS_MMSS_MASTER_ADM1_PORT0,
	MSM_BUS_MASTER_ROTATOR,
	MSM_BUS_MASTER_GRAPHICS_3D,
	MSM_BUS_MASTER_JPEG_DEC,
	MSM_BUS_MASTER_GRAPHICS_2D_CORE0,
	MSM_BUS_MASTER_VFE,
	MSM_BUS_MASTER_VPE,
	MSM_BUS_MASTER_JPEG_ENC,
	MSM_BUS_MASTER_GRAPHICS_2D_CORE1,
	MSM_BUS_MMSS_MASTER_APPS_FAB,
	MSM_BUS_MASTER_HD_CODEC_PORT0,
	MSM_BUS_MASTER_HD_CODEC_PORT1,

	MSM_BUS_MASTER_SPDM,
	MSM_BUS_MASTER_RPM,

	MSM_BUS_MASTER_MSS,
	MSM_BUS_MASTER_RIVA,
	MSM_BUS_SYSTEM_MASTER_UNUSED_6,
	MSM_BUS_MASTER_MSS_SW_PROC,
	MSM_BUS_MASTER_MSS_FW_PROC,
	MSM_BUS_MMSS_MASTER_UNUSED_2,
	MSM_BUS_MASTER_GSS_NAV,
	MSM_BUS_MASTER_PCIE,
	MSM_BUS_MASTER_SATA,
	MSM_BUS_MASTER_CRYPTO,

	MSM_BUS_MASTER_VIDEO_CAP,
	MSM_BUS_MASTER_GRAPHICS_3D_PORT1,
	MSM_BUS_MASTER_VIDEO_ENC,
	MSM_BUS_MASTER_VIDEO_DEC,
	MSM_BUS_MASTER_LAST = MSM_BUS_MMSS_MASTER_UNUSED_2,

	MSM_BUS_SYSTEM_FPB_MASTER_SYSTEM =
		MSM_BUS_SYSTEM_MASTER_SYSTEM_FPB,
	MSM_BUS_CPSS_FPB_MASTER_SYSTEM =
		MSM_BUS_SYSTEM_MASTER_CPSS_FPB,
};

enum msm_bus_fabric_slave_type {
	MSM_BUS_SLAVE_FIRST = SLAVE_ID_KEY,
	MSM_BUS_SLAVE_EBI_CH0 = SLAVE_ID_KEY,
	MSM_BUS_SLAVE_EBI_CH1,
	MSM_BUS_SLAVE_AMPSS_L2,
	MSM_BUS_APPSS_SLAVE_FAB_MMSS,
	MSM_BUS_APPSS_SLAVE_FAB_SYSTEM,

	MSM_BUS_SYSTEM_SLAVE_FAB_APPS,
	MSM_BUS_SLAVE_SPS,
	MSM_BUS_SLAVE_SYSTEM_IMEM,
	MSM_BUS_SLAVE_AMPSS,
	MSM_BUS_SLAVE_MSS,
	MSM_BUS_SLAVE_LPASS,
	MSM_BUS_SYSTEM_SLAVE_CPSS_FPB,
	MSM_BUS_SYSTEM_SLAVE_SYSTEM_FPB,
	MSM_BUS_SYSTEM_SLAVE_MMSS_FPB,
	MSM_BUS_SLAVE_CORESIGHT,
	MSM_BUS_SLAVE_RIVA,

	MSM_BUS_SLAVE_SMI,
	MSM_BUS_MMSS_SLAVE_FAB_APPS,
	MSM_BUS_MMSS_SLAVE_FAB_APPS_1,
	MSM_BUS_SLAVE_MM_IMEM,
	MSM_BUS_SLAVE_CRYPTO,

	MSM_BUS_SLAVE_SPDM,
	MSM_BUS_SLAVE_RPM,
	MSM_BUS_SLAVE_RPM_MSG_RAM,
	MSM_BUS_SLAVE_MPM,
	MSM_BUS_SLAVE_PMIC1_SSBI1_A,
	MSM_BUS_SLAVE_PMIC1_SSBI1_B,
	MSM_BUS_SLAVE_PMIC1_SSBI1_C,
	MSM_BUS_SLAVE_PMIC2_SSBI2_A,
	MSM_BUS_SLAVE_PMIC2_SSBI2_B,

	MSM_BUS_SLAVE_GSBI1_UART,
	MSM_BUS_SLAVE_GSBI2_UART,
	MSM_BUS_SLAVE_GSBI3_UART,
	MSM_BUS_SLAVE_GSBI4_UART,
	MSM_BUS_SLAVE_GSBI5_UART,
	MSM_BUS_SLAVE_GSBI6_UART,
	MSM_BUS_SLAVE_GSBI7_UART,
	MSM_BUS_SLAVE_GSBI8_UART,
	MSM_BUS_SLAVE_GSBI9_UART,
	MSM_BUS_SLAVE_GSBI10_UART,
	MSM_BUS_SLAVE_GSBI11_UART,
	MSM_BUS_SLAVE_GSBI12_UART,
	MSM_BUS_SLAVE_GSBI1_QUP,
	MSM_BUS_SLAVE_GSBI2_QUP,
	MSM_BUS_SLAVE_GSBI3_QUP,
	MSM_BUS_SLAVE_GSBI4_QUP,
	MSM_BUS_SLAVE_GSBI5_QUP,
	MSM_BUS_SLAVE_GSBI6_QUP,
	MSM_BUS_SLAVE_GSBI7_QUP,
	MSM_BUS_SLAVE_GSBI8_QUP,
	MSM_BUS_SLAVE_GSBI9_QUP,
	MSM_BUS_SLAVE_GSBI10_QUP,
	MSM_BUS_SLAVE_GSBI11_QUP,
	MSM_BUS_SLAVE_GSBI12_QUP,
	MSM_BUS_SLAVE_EBI2_NAND,
	MSM_BUS_SLAVE_EBI2_CS0,
	MSM_BUS_SLAVE_EBI2_CS1,
	MSM_BUS_SLAVE_EBI2_CS2,
	MSM_BUS_SLAVE_EBI2_CS3,
	MSM_BUS_SLAVE_EBI2_CS4,
	MSM_BUS_SLAVE_EBI2_CS5,
	MSM_BUS_SLAVE_USB_FS1,
	MSM_BUS_SLAVE_USB_FS2,
	MSM_BUS_SLAVE_TSIF,
	MSM_BUS_SLAVE_MSM_TSSC,
	MSM_BUS_SLAVE_MSM_PDM,
	MSM_BUS_SLAVE_MSM_DIMEM,
	MSM_BUS_SLAVE_MSM_TCSR,
	MSM_BUS_SLAVE_MSM_PRNG,
	MSM_BUS_SLAVE_GSS,
	MSM_BUS_SLAVE_SATA,
	MSM_BUS_SLAVE_LAST = MSM_BUS_SLAVE_MSM_PRNG,

	MSM_BUS_SYSTEM_FPB_SLAVE_SYSTEM =
		MSM_BUS_SYSTEM_SLAVE_SYSTEM_FPB,
	MSM_BUS_CPSS_FPB_SLAVE_SYSTEM =
		MSM_BUS_SYSTEM_SLAVE_CPSS_FPB,
};

#endif /*__ASM_ARCH_MSM_BUS_BOARD_H */
