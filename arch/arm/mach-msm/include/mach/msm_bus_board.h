/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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
	const char *name;
	struct msm_bus_node_info *info;
	unsigned int len;
	int ahb;
	const char *fabclk[NUM_CTX];
	const char *iface_clk;
	unsigned int offset;
	unsigned int haltid;
	unsigned int rpm_enabled;
	unsigned int nmasters;
	unsigned int nslaves;
	unsigned int ntieredslaves;
	bool il_flag;
	const struct msm_bus_board_algorithm *board_algo;
	int hw_sel;
	void *hw_data;
	uint32_t qos_freq;
	uint32_t qos_baseoffset;
	bool virt;
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
extern struct msm_bus_fabric_registration msm_bus_8960_sg_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_8064_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_9615_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_9615_def_fab_pdata;

extern struct msm_bus_fabric_registration msm_bus_8930_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_8974_sys_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_mmss_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_bimc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_ocmem_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_periph_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_config_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_ocmem_vnoc_pdata;

extern struct msm_bus_fabric_registration msm_bus_9625_sys_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_9625_bimc_pdata;
extern struct msm_bus_fabric_registration msm_bus_9625_periph_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_9625_config_noc_pdata;

void msm_bus_rpm_set_mt_mask(void);
int msm_bus_board_rpm_get_il_ids(uint16_t *id);
int msm_bus_board_get_iid(int id);

#define NFAB_MSM8226 6
#define NFAB_MSM8610 5

/*
 * These macros specify the convention followed for allocating
 * ids to fabrics, masters and slaves for 8x60.
 *
 * A node can be identified as a master/slave/fabric by using
 * these ids.
 */
#define FABRIC_ID_KEY 1024
#define SLAVE_ID_KEY ((FABRIC_ID_KEY) >> 1)
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

#define RPM_BUS_SLAVE_REQ	0x766c7362
#define RPM_BUS_MASTER_REQ	0x73616d62

enum msm_bus_rpm_slave_field_type {
	RPM_SLAVE_FIELD_BW = 0x00007762,
};

enum msm_bus_rpm_mas_field_type {
	RPM_MASTER_FIELD_BW =		0x00007762,
	RPM_MASTER_FIELD_BW_T0 =	0x30747762,
	RPM_MASTER_FIELD_BW_T1 =	0x31747762,
	RPM_MASTER_FIELD_BW_T2 =	0x32747762,
};

/* Topology related enums */
enum msm_bus_fabric_type {
	MSM_BUS_FAB_DEFAULT = 0,
	MSM_BUS_FAB_APPSS = 0,
	MSM_BUS_FAB_SYSTEM = 1024,
	MSM_BUS_FAB_MMSS = 2048,
	MSM_BUS_FAB_SYSTEM_FPB = 3072,
	MSM_BUS_FAB_CPSS_FPB = 4096,
};

enum msm_bus_fab_noc_bimc_type {
	MSM_BUS_FAB_BIMC = 0,
	MSM_BUS_FAB_SYS_NOC = 1024,
	MSM_BUS_FAB_MMSS_NOC = 2048,
	MSM_BUS_FAB_OCMEM_NOC = 3072,
	MSM_BUS_FAB_PERIPH_NOC = 4096,
	MSM_BUS_FAB_CONFIG_NOC = 5120,
	MSM_BUS_FAB_OCMEM_VNOC = 6144,
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

	MSM_BUS_MASTER_LPASS_AHB,
	MSM_BUS_MASTER_QDSS_BAM,
	MSM_BUS_MASTER_SNOC_CFG,
	MSM_BUS_MASTER_CRYPTO_CORE0,
	MSM_BUS_MASTER_CRYPTO_CORE1,
	MSM_BUS_MASTER_MSS_NAV,
	MSM_BUS_MASTER_OCMEM_DMA,
	MSM_BUS_MASTER_WCSS,
	MSM_BUS_MASTER_QDSS_ETR,
	MSM_BUS_MASTER_USB3,

	MSM_BUS_MASTER_JPEG,
	MSM_BUS_MASTER_VIDEO_P0,
	MSM_BUS_MASTER_VIDEO_P1,

	MSM_BUS_MASTER_MSS_PROC,
	MSM_BUS_MASTER_JPEG_OCMEM,
	MSM_BUS_MASTER_MDP_OCMEM,
	MSM_BUS_MASTER_VIDEO_P0_OCMEM,
	MSM_BUS_MASTER_VIDEO_P1_OCMEM,
	MSM_BUS_MASTER_VFE_OCMEM,
	MSM_BUS_MASTER_CNOC_ONOC_CFG,
	MSM_BUS_MASTER_RPM_INST,
	MSM_BUS_MASTER_RPM_DATA,
	MSM_BUS_MASTER_RPM_SYS,
	MSM_BUS_MASTER_DEHR,
	MSM_BUS_MASTER_QDSS_DAP,
	MSM_BUS_MASTER_TIC,

	MSM_BUS_MASTER_SDCC_1,
	MSM_BUS_MASTER_SDCC_3,
	MSM_BUS_MASTER_SDCC_4,
	MSM_BUS_MASTER_SDCC_2,
	MSM_BUS_MASTER_TSIF,
	MSM_BUS_MASTER_BAM_DMA,
	MSM_BUS_MASTER_BLSP_2,
	MSM_BUS_MASTER_USB_HSIC,
	MSM_BUS_MASTER_BLSP_1,
	MSM_BUS_MASTER_USB_HS,
	MSM_BUS_MASTER_PNOC_CFG,
	MSM_BUS_MASTER_V_OCMEM_GFX3D,
	MSM_BUS_MASTER_IPA,
	MSM_BUS_MASTER_QPIC,
	MSM_BUS_MASTER_MDPE,

	MSM_BUS_MASTER_LAST,

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

	MSM_BUS_SLAVE_USB3,
	MSM_BUS_SLAVE_WCSS,
	MSM_BUS_SLAVE_OCIMEM,
	MSM_BUS_SLAVE_SNOC_OCMEM,
	MSM_BUS_SLAVE_SERVICE_SNOC,
	MSM_BUS_SLAVE_QDSS_STM,

	MSM_BUS_SLAVE_CAMERA_CFG,
	MSM_BUS_SLAVE_DISPLAY_CFG,
	MSM_BUS_SLAVE_OCMEM_CFG,
	MSM_BUS_SLAVE_CPR_CFG,
	MSM_BUS_SLAVE_CPR_XPU_CFG,
	MSM_BUS_SLAVE_MISC_CFG,
	MSM_BUS_SLAVE_MISC_XPU_CFG,
	MSM_BUS_SLAVE_VENUS_CFG,
	MSM_BUS_SLAVE_MISC_VENUS_CFG,
	MSM_BUS_SLAVE_GRAPHICS_3D_CFG,
	MSM_BUS_SLAVE_MMSS_CLK_CFG,
	MSM_BUS_SLAVE_MMSS_CLK_XPU_CFG,
	MSM_BUS_SLAVE_MNOC_MPU_CFG,
	MSM_BUS_SLAVE_ONOC_MPU_CFG,
	MSM_BUS_SLAVE_SERVICE_MNOC,

	MSM_BUS_SLAVE_OCMEM,
	MSM_BUS_SLAVE_SERVICE_ONOC,

	MSM_BUS_SLAVE_SDCC_1,
	MSM_BUS_SLAVE_SDCC_3,
	MSM_BUS_SLAVE_SDCC_2,
	MSM_BUS_SLAVE_SDCC_4,
	MSM_BUS_SLAVE_BAM_DMA,
	MSM_BUS_SLAVE_BLSP_2,
	MSM_BUS_SLAVE_USB_HSIC,
	MSM_BUS_SLAVE_BLSP_1,
	MSM_BUS_SLAVE_USB_HS,
	MSM_BUS_SLAVE_PDM,
	MSM_BUS_SLAVE_PERIPH_APU_CFG,
	MSM_BUS_SLAVE_PNOC_MPU_CFG,
	MSM_BUS_SLAVE_PRNG,
	MSM_BUS_SLAVE_SERVICE_PNOC,

	MSM_BUS_SLAVE_CLK_CTL,
	MSM_BUS_SLAVE_CNOC_MSS,
	MSM_BUS_SLAVE_SECURITY,
	MSM_BUS_SLAVE_TCSR,
	MSM_BUS_SLAVE_TLMM,
	MSM_BUS_SLAVE_CRYPTO_0_CFG,
	MSM_BUS_SLAVE_CRYPTO_1_CFG,
	MSM_BUS_SLAVE_IMEM_CFG,
	MSM_BUS_SLAVE_MESSAGE_RAM,
	MSM_BUS_SLAVE_BIMC_CFG,
	MSM_BUS_SLAVE_BOOT_ROM,
	MSM_BUS_SLAVE_CNOC_MNOC_MMSS_CFG,
	MSM_BUS_SLAVE_PMIC_ARB,
	MSM_BUS_SLAVE_SPDM_WRAPPER,
	MSM_BUS_SLAVE_DEHR_CFG,
	MSM_BUS_SLAVE_QDSS_CFG,
	MSM_BUS_SLAVE_RBCPR_CFG,
	MSM_BUS_SLAVE_RBCPR_QDSS_APU_CFG,
	MSM_BUS_SLAVE_SNOC_MPU_CFG,
	MSM_BUS_SLAVE_CNOC_ONOC_CFG,
	MSM_BUS_SLAVE_CNOC_MNOC_CFG,
	MSM_BUS_SLAVE_PNOC_CFG,
	MSM_BUS_SLAVE_SNOC_CFG,
	MSM_BUS_SLAVE_EBI1_DLL_CFG,
	MSM_BUS_SLAVE_PHY_APU_CFG,
	MSM_BUS_SLAVE_EBI1_PHY_CFG,
	MSM_BUS_SLAVE_SERVICE_CNOC,
	MSM_BUS_SLAVE_IPS_CFG,
	MSM_BUS_SLAVE_QPIC,
	MSM_BUS_SLAVE_DSI_CFG,

	MSM_BUS_SLAVE_LAST,

	MSM_BUS_SYSTEM_FPB_SLAVE_SYSTEM =
		MSM_BUS_SYSTEM_SLAVE_SYSTEM_FPB,
	MSM_BUS_CPSS_FPB_SLAVE_SYSTEM =
		MSM_BUS_SYSTEM_SLAVE_CPSS_FPB,
};

#endif /*__ASM_ARCH_MSM_BUS_BOARD_H */
