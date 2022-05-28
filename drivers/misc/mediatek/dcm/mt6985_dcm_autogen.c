// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/io.h>

#include <mt6985_dcm_internal.h>
#include <mt6985_dcm_autogen.h>
#include <mtk_dcm.h>
/*====================auto gen code 20220412_201000=====================*/
#define TOPCKGEN_INFRA_IOMMU_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22))
#define TOPCKGEN_INFRA_IOMMU_DCM_REG0_ON ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12) | \
			(0x0 << 13) | \
			(0x0 << 14) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12) | \
			(0x0 << 13) | \
			(0x0 << 14) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 11) | \
			(0x0 << 12) | \
			(0x0 << 13) | \
			(0x0 << 14) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20) | \
			(0x0 << 21) | \
			(0x0 << 22))
#define TOPCKGEN_INFRA_IOMMU_DCM_REG0_OFF ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 11) | \
			(0x1 << 12) | \
			(0x1 << 13) | \
			(0x1 << 14) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20) | \
			(0x1 << 21) | \
			(0x1 << 22))

bool dcm_topckgen_infra_iommu_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(TOPCKGEN_MMU_DCM_DIS) &
		TOPCKGEN_INFRA_IOMMU_DCM_REG0_MASK) ==
		(unsigned int) TOPCKGEN_INFRA_IOMMU_DCM_REG0_ON);

	return ret;
}

void dcm_topckgen_infra_iommu_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'topckgen_infra_iommu_dcm'" */
		reg_write(TOPCKGEN_MMU_DCM_DIS,
			(reg_read(TOPCKGEN_MMU_DCM_DIS) &
			~TOPCKGEN_INFRA_IOMMU_DCM_REG0_MASK) |
			TOPCKGEN_INFRA_IOMMU_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'topckgen_infra_iommu_dcm'" */
		reg_write(TOPCKGEN_MMU_DCM_DIS,
			(reg_read(TOPCKGEN_MMU_DCM_DIS) &
			~TOPCKGEN_INFRA_IOMMU_DCM_REG0_MASK) |
			TOPCKGEN_INFRA_IOMMU_DCM_REG0_OFF);
	}
}

#define TOPCKGEN_INFRA_RSI_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define TOPCKGEN_INFRA_RSI_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define TOPCKGEN_INFRA_RSI_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3))

bool dcm_topckgen_infra_rsi_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(TOPCKGEN_RSI_DCM_CON) &
		TOPCKGEN_INFRA_RSI_DCM_REG0_MASK) ==
		(unsigned int) TOPCKGEN_INFRA_RSI_DCM_REG0_ON);

	return ret;
}

void dcm_topckgen_infra_rsi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'topckgen_infra_rsi_dcm'" */
		reg_write(TOPCKGEN_RSI_DCM_CON,
			(reg_read(TOPCKGEN_RSI_DCM_CON) &
			~TOPCKGEN_INFRA_RSI_DCM_REG0_MASK) |
			TOPCKGEN_INFRA_RSI_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'topckgen_infra_rsi_dcm'" */
		reg_write(TOPCKGEN_RSI_DCM_CON,
			(reg_read(TOPCKGEN_RSI_DCM_CON) &
			~TOPCKGEN_INFRA_RSI_DCM_REG0_MASK) |
			TOPCKGEN_INFRA_RSI_DCM_REG0_OFF);
	}
}

#define IFRBUS_AO_INFRA_BUS_DCM_REG0_MASK ((0x7fffffff << 0) | \
			(0x1 << 31))
#define IFRBUS_AO_INFRA_BUS_DCM_REG0_ON ((0x7555098 << 0) | \
			(0x1 << 31))
#define IFRBUS_AO_INFRA_BUS_DCM_REG0_OFF ((0x7555098 << 0) | \
			(0x1 << 31))

bool dcm_ifrbus_ao_infra_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(DCM_SET_RW_0) &
		IFRBUS_AO_INFRA_BUS_DCM_REG0_MASK) ==
		(unsigned int) IFRBUS_AO_INFRA_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_ifrbus_ao_infra_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'ifrbus_ao_infra_bus_dcm'" */
		reg_write(DCM_SET_RW_0,
			(reg_read(DCM_SET_RW_0) &
			~IFRBUS_AO_INFRA_BUS_DCM_REG0_MASK) |
			IFRBUS_AO_INFRA_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'ifrbus_ao_infra_bus_dcm'" */
		reg_write(DCM_SET_RW_0,
			(reg_read(DCM_SET_RW_0) &
			~IFRBUS_AO_INFRA_BUS_DCM_REG0_MASK) |
			IFRBUS_AO_INFRA_BUS_DCM_REG0_OFF);
	}
}

#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK ((0x1 << 4) | \
			(0x1 << 7) | \
			(0x1 << 10) | \
			(0x1 << 12))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK ((0x1 << 15))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK ((0x1 << 13))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON ((0x1 << 4) | \
			(0x1 << 7) | \
			(0x1 << 10) | \
			(0x1 << 12))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON ((0x1 << 15))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON ((0x1 << 13))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG0_OFF ((0x0 << 4) | \
			(0x0 << 7) | \
			(0x0 << 10) | \
			(0x0 << 12))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG1_OFF ((0x0 << 15))
#define PERI_AO_BCRM_PERI_BUS_DCM_REG2_OFF ((0x0 << 13))

bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) ==
		(unsigned int) PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) ==
		(unsigned int) PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
		PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) ==
		(unsigned int) PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON);

	return ret;
}

void dcm_peri_ao_bcrm_peri_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'peri_ao_bcrm_peri_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG1_ON);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'peri_ao_bcrm_peri_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG1_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG1_OFF);
		reg_write(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2,
			(reg_read(
			VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2) &
			~PERI_AO_BCRM_PERI_BUS_DCM_REG2_MASK) |
			PERI_AO_BCRM_PERI_BUS_DCM_REG2_OFF);
	}
}

#define UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK ((0x1 << 3) | \
			(0x1 << 6) | \
			(0x1 << 8))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK ((0x1 << 15))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG0_ON ((0x1 << 3) | \
			(0x1 << 6) | \
			(0x1 << 8))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG1_ON ((0x1 << 15))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG0_OFF ((0x0 << 3) | \
			(0x0 << 6) | \
			(0x0 << 8))
#define UFS0_AO_BCRM_UFS_BUS_DCM_REG1_OFF ((0x0 << 15))

bool dcm_ufs0_ao_bcrm_ufs_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0) &
		UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK) ==
		(unsigned int) UFS0_AO_BCRM_UFS_BUS_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1) &
		UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK) ==
		(unsigned int) UFS0_AO_BCRM_UFS_BUS_DCM_REG1_ON);

	return ret;
}

void dcm_ufs0_ao_bcrm_ufs_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'ufs0_ao_bcrm_ufs_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'ufs0_ao_bcrm_ufs_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG0_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1) &
			~UFS0_AO_BCRM_UFS_BUS_DCM_REG1_MASK) |
			UFS0_AO_BCRM_UFS_BUS_DCM_REG1_OFF);
	}
}

#define PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_MASK ((0x1 << 3) | \
			(0x1 << 6) | \
			(0x1 << 8))
#define PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_MASK ((0x1 << 13))
#define PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_ON ((0x1 << 3) | \
			(0x1 << 6) | \
			(0x1 << 8))
#define PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_ON ((0x1 << 13))
#define PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_OFF ((0x0 << 3) | \
			(0x0 << 6) | \
			(0x0 << 8))
#define PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_OFF ((0x0 << 13))

bool dcm_pcie0_ao_bcrm_pextp_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_0) &
		PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_MASK) ==
		(unsigned int) PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_ON);
	ret &= ((reg_read(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_1) &
		PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_MASK) ==
		(unsigned int) PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_ON);

	return ret;
}

void dcm_pcie0_ao_bcrm_pextp_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'pcie0_ao_bcrm_pextp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_0) &
			~PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_MASK) |
			PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_ON);
		reg_write(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_1) &
			~PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_MASK) |
			PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'pcie0_ao_bcrm_pextp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_0,
			(reg_read(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_0) &
			~PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_MASK) |
			PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG0_OFF);
		reg_write(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_1,
			(reg_read(VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_1) &
			~PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_MASK) |
			PCIE0_AO_BCRM_PEXTP_BUS_DCM_REG1_OFF);
	}
}

#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK ((0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 5) | \
			(0x1f << 14))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON ((0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 5) | \
			(0x0 << 14))
#define VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF ((0x1 << 1) | \
			(0x1 << 2))

bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0) &
		VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) ==
		(unsigned int) VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'vlp_ao_bcrm_vlp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'vlp_ao_bcrm_vlp_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0) &
			~VLP_AO_BCRM_VLP_BUS_DCM_REG0_MASK) |
			VLP_AO_BCRM_VLP_BUS_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_ON ((0x1 << 0))
#define MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mcusys_par_wrap_cpc_pbi_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_CPC_DCM_Enable) &
		MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpc_pbi_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_cpc_pbi_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CPC_DCM_Enable,
			(reg_read(MCUSYS_PAR_WRAP_CPC_DCM_Enable) &
			~MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_cpc_pbi_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CPC_DCM_Enable,
			(reg_read(MCUSYS_PAR_WRAP_CPC_DCM_Enable) &
			~MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPC_PBI_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_MASK ((0x1 << 1))
#define MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_ON ((0x1 << 1))
#define MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_OFF ((0x0 << 1))

bool dcm_mcusys_par_wrap_cpc_turbo_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_CPC_DCM_Enable) &
		MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpc_turbo_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_cpc_turbo_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CPC_DCM_Enable,
			(reg_read(MCUSYS_PAR_WRAP_CPC_DCM_Enable) &
			~MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_cpc_turbo_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CPC_DCM_Enable,
			(reg_read(MCUSYS_PAR_WRAP_CPC_DCM_Enable) &
			~MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPC_TURBO_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 16))
#define MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 16))
#define MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 16))

bool dcm_mcusys_par_wrap_mcu_acp_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_acp_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_acp_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_acp_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ACP_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20))
#define MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6) | \
			(0x1 << 7) | \
			(0x1 << 8) | \
			(0x1 << 9) | \
			(0x1 << 10) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 19) | \
			(0x1 << 20))
#define MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6) | \
			(0x0 << 7) | \
			(0x0 << 8) | \
			(0x0 << 9) | \
			(0x0 << 10) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 19) | \
			(0x0 << 20))

bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN) &
		MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_adb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_adb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN,
			(reg_read(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN) &
			~MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_adb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN,
			(reg_read(MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN) &
			~MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_ADB_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK ((0xffff << 8) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_ON ((0xffff << 8) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_OFF ((0x0 << 8) | \
			(0x0 << 24))

bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_apb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_apb_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_APB_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2))

bool dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_CI700_DCM_CTRL) &
		MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_bkr_ldcm'" */
		reg_write(MCUSYS_PAR_WRAP_CI700_DCM_CTRL,
			(reg_read(MCUSYS_PAR_WRAP_CI700_DCM_CTRL) &
			~MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_bkr_ldcm'" */
		reg_write(MCUSYS_PAR_WRAP_CI700_DCM_CTRL,
			(reg_read(MCUSYS_PAR_WRAP_CI700_DCM_CTRL) &
			~MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BKR_LDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK ((0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_ON ((0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_OFF ((0x0 << 16) | \
			(0x0 << 20) | \
			(0x0 << 24))
#define MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8) | \
			(0x0 << 12))

bool dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
		MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG1) &
		MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_bus_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_bus_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG1,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG1) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_bus_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG1,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG1) &
			~MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_BUS_QDCM_REG1_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_ON ((0x0 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_OFF ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2))

bool dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0) &
		MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_cbip_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_cbip_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_ON);
		reg_write(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_cbip_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG1_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG2_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG3_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG4_OFF);
		reg_write(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_MASK) |
			MCUSYS_PAR_WRAP_MCU_CBIP_DCM_REG5_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12) | \
			(0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24) | \
			(0x1 << 28))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8) | \
			(0x1 << 12) | \
			(0x1 << 16) | \
			(0x1 << 20) | \
			(0x1 << 24) | \
			(0x1 << 28))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 4) | \
			(0x1 << 8))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8) | \
			(0x0 << 12) | \
			(0x0 << 16) | \
			(0x0 << 20) | \
			(0x0 << 24) | \
			(0x0 << 28))
#define MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 4) | \
			(0x0 << 8))

bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG2) &
		MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG3) &
		MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_core_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_core_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG2,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG2) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG3,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG3) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_core_qdcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG2,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG2) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG3,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG3) &
			~MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_CORE_QDCM_REG1_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK ((0xffffffff << 0))
#define MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_ON ((0x161 << 0))
#define MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_OFF ((0x160 << 0))

bool dcm_mcusys_par_wrap_mcu_dsu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG1) &
		MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_dsu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_dsu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG1,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG1) &
			~MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_dsu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG1,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG1) &
			~MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_DSU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 12))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_ON ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 12))
#define MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_OFF ((0x0 << 0))

bool dcm_mcusys_par_wrap_mcu_io_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
		MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_ON);
	ret &= ((reg_read(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG) &
		MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_io_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_io_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_io_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_QDCM_CONFIG0,
			(reg_read(MCUSYS_PAR_WRAP_QDCM_CONFIG0) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG0_OFF);
		reg_write(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG,
			(reg_read(MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG) &
			~MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_MCU_IO_DCM_REG1_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_ON ((0x1 << 0))
#define MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_OFF ((0x0 << 0))

bool dcm_mcusys_par_wrap_mcu_misc_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG) &
		MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_misc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_misc_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG,
			(reg_read(
			MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG) &
			~MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_misc_dcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG,
			(reg_read(
			MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG) &
			~MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_MISC_DCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK ((0xff << 0))
#define MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_ON ((0xff << 0))
#define MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_OFF ((0x0 << 0))

bool dcm_mcusys_par_wrap_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_MASK ((0x1 << 0))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG1_MASK ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG2_MASK ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG3_MASK ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_ON ((0x1 << 0))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG1_ON ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG2_ON ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG3_ON ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_OFF ((0x0 << 0))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG1_OFF ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG2_OFF ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG3_OFF ((0x1 << 8))

bool dcm_mcusys_par_wrap_cpu0_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu0_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG1_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG2_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG3_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU0_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_MASK ((0x1 << 1))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG1_MASK ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG2_MASK ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG3_MASK ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_ON ((0x1 << 1))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG1_ON ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG2_ON ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG3_ON ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_OFF ((0x0 << 1))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG1_OFF ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG2_OFF ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG3_OFF ((0x1 << 8))

bool dcm_mcusys_par_wrap_cpu1_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu1_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG1_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG2_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG3_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU1_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_MASK ((0x1 << 2))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG1_MASK ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG2_MASK ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG3_MASK ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_ON ((0x1 << 2))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG1_ON ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG2_ON ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG3_ON ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_OFF ((0x0 << 2))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG1_OFF ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG2_OFF ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG3_OFF ((0x1 << 8))

bool dcm_mcusys_par_wrap_cpu2_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu2_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG1_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG2_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG3_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU2_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_MASK ((0x1 << 3))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG1_MASK ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG2_MASK ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG3_MASK ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_ON ((0x1 << 3))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG1_ON ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG2_ON ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG3_ON ((0x1 << 8))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_OFF ((0x0 << 3))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG1_OFF ((0x7 << 9))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG2_OFF ((0x1000 << 4))
#define MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG3_OFF ((0x1 << 8))

bool dcm_mcusys_par_wrap_cpu3_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu3_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG1_MASK) |
			MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG1_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG2_MASK) |
			MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG2_ON);
		reg_write(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF,
			(reg_read(MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF) &
			~MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG3_MASK) |
			MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG3_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU3_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_MASK ((0x1 << 4))
#define MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_ON ((0x1 << 4))
#define MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_OFF ((0x0 << 4))

bool dcm_mcusys_par_wrap_cpu4_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu4_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU4_MCU_STALLDCM_REG0_OFF);
	}
}


#define MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_MASK ((0x1 << 5))
#define MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_ON ((0x1 << 5))
#define MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_OFF ((0x0 << 5))

bool dcm_mcusys_par_wrap_cpu5_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu5_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU5_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_MASK ((0x1 << 6))
#define MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_ON ((0x1 << 6))
#define MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_OFF ((0x0 << 6))

bool dcm_mcusys_par_wrap_cpu6_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu6_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU6_MCU_STALLDCM_REG0_OFF);
	}
}

#define MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_MASK ((0x1 << 7))
#define MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_ON ((0x1 << 7))
#define MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_OFF ((0x0 << 7))

bool dcm_mcusys_par_wrap_cpu7_mcu_stalldcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
		MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_MASK) ==
		(unsigned int) MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_ON);

	return ret;
}

void dcm_mcusys_par_wrap_cpu7_mcu_stalldcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mcusys_par_wrap_mcu_stalldcm'" */
		reg_write(MCUSYS_PAR_WRAP_MP0_DCM_CFG0,
			(reg_read(MCUSYS_PAR_WRAP_MP0_DCM_CFG0) &
			~MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_MASK) |
			MCUSYS_PAR_WRAP_CPU7_MCU_STALLDCM_REG0_OFF);
	}
}
