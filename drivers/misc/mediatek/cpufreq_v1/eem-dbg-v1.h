/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eem-dbg-v1.h - eem data structure
 *
 * Copyright (c) 2020 MediaTek Inc.
 * chienwei Chang <chienwei.chang@mediatek.com>
 */

#define EEM_TAG	 "[CPU][EEM]"

static char *cpu_name[3] = {
	"L",
	"BIG",
	"CCI"
};

#define MAX_NR_FREQ					32
#define NR_PI_VF					6
#define MIN_SIZE_SN_DUMP_CPE		(7)
#define NUM_SN_CPU					(8)
#define SIZE_SN_MCUSYS_REG			(10)
#define SIZE_SN_DUMP_SENSOR			(64)
#define IDX_HW_RES_SN				(18) /* start index of Sensor Network efuse */
#define LOW_TEMP_OFF				(8)
#define HIGH_TEMP_OFF				(3)
#define AGING_VAL_CPU				(0x6) /* CPU aging margin : 37mv*/
#define AGING_VAL_CPU_B				(0x6) /* CPU aging margin : 37mv*/

#define EEMSN_V_BASE		(40000)
#define EEMSN_STEP			(625)

/* CPU */
#define CPU_PMIC_BASE		(0)
#define CPU_PMIC_BASE2		(0)
#define CPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/

#define VMIN_PREDICT_ENABLE	(0)
#define UPDATE_TO_UPOWER	(0)
#define ENABLE_COUNT_SNTEMP	(0)
#define SET_PMIC_VOLT_TO_DVFS	(0)

extern void __iomem *eem_base;
extern void __iomem *eem_csram_base;
extern void __iomem *sn_base;

#define EEM_BASEADDR		0x11278000
#define EEM_BASESIZE		0x1000
#define EEMSN_CSRAM_BASE	0x0011BC00  /* EB View:0x0011BC00 */
#define EEMSN_CSRAM_SIZE	0x1000
#define SN_BASEADDR			0x0C560000
#define SN_BASESIZE			0x1000

#define EEM_TEMPSPARE0			(eem_base + 0x8F0)

#define OFFS_SN_VOLT_S_4B		(0x0011BC00 + 0x0250) /* 148 */
#define OFFS_SN_VOLT_E_4B		(0x0011BC00 + 0x029C) /* 167 */

#define SN_CPEMONCTL            (SN_BASEADDR + 0xB00)
#define SN_CPEEN                (SN_BASEADDR + 0xB04)
#define SN_COMPAREDVOP          (SN_BASEADDR + 0xB08)
#define SN_CPEVMIN1T0           (SN_BASEADDR + 0xB0C)
#define SN_CPEVMIN3T2           (SN_BASEADDR + 0xB10)
#define SN_CPEVMIN5T4           (SN_BASEADDR + 0xB14)
#define SN_CPEVMIN7T6           (SN_BASEADDR + 0xB18)
#define SN_CPEIRQSTS            (SN_BASEADDR + 0xB1C)
#define SN_BCPUVMAXVMINOPP0     (SN_BASEADDR + 0xB20)
#define SN_LCPUVMAXVMINOPP0     (SN_BASEADDR + 0xB24)
#define SN_CPEINTSTS            (SN_BASEADDR + 0xB2C)
#define SN_CPEINTEN             (SN_BASEADDR + 0xB30)
#define SN_CPEINTSTSRAW         (SN_BASEADDR + 0xB34)
#define SN_CPECALSTS            (SN_BASEADDR + 0xB38)
#define SN_CPECALFSMSTSCORE01   (SN_BASEADDR + 0xB3C)
#define SN_CPECALFSMSTSCORE23   (SN_BASEADDR + 0xB40)
#define SN_CPECALFSMSTSCORE45   (SN_BASEADDR + 0xB44)
#define SN_CPECALFSMSTSCORE67   (SN_BASEADDR + 0xB48)

#define SN_C0ASENSORDATA        (SN_BASEADDR + 0xC00)
#define SN_C0TSENSORDATA        (SN_BASEADDR + 0xC04)
#define SN_C0VSENSORMINDATA0    (SN_BASEADDR + 0xC08)
#define SN_C0VSENSORMINDATA1    (SN_BASEADDR + 0xC0C)
#define SN_C0VSENSORMAXDATA0    (SN_BASEADDR + 0xC10)
#define SN_C0VSENSORMAXDATA1    (SN_BASEADDR + 0xC14)
#define SN_C0PSESNORDATA0       (SN_BASEADDR + 0xC18)
#define SN_C0PSESNORDATA1       (SN_BASEADDR + 0xC1C)
#define SN_C1ASENSORDATA        (SN_BASEADDR + 0xC20)
#define SN_C1TSENSORDATA        (SN_BASEADDR + 0xC24)
#define SN_C1VSENSORMINDATA0    (SN_BASEADDR + 0xC28)
#define SN_C1VSENSORMINDATA1    (SN_BASEADDR + 0xC2C)
#define SN_C1VSENSORMAXDATA0    (SN_BASEADDR + 0xC30)
#define SN_C1VSENSORMAXDATA1    (SN_BASEADDR + 0xC34)
#define SN_C1PSESNORDATA0       (SN_BASEADDR + 0xC38)
#define SN_C1PSESNORDATA1       (SN_BASEADDR + 0xC3C)
#define SN_C2ASENSORDATA        (SN_BASEADDR + 0xC40)
#define SN_C2TSENSORDATA        (SN_BASEADDR + 0xC44)
#define SN_C2VSENSORMINDATA0    (SN_BASEADDR + 0xC48)
#define SN_C2VSENSORMINDATA1    (SN_BASEADDR + 0xC4C)
#define SN_C2VSENSORMAXDATA0    (SN_BASEADDR + 0xC50)
#define SN_C2VSENSORMAXDATA1    (SN_BASEADDR + 0xC54)
#define SN_C2PSESNORDATA0       (SN_BASEADDR + 0xC58)
#define SN_C2PSESNORDATA1       (SN_BASEADDR + 0xC5C)
#define SN_C3ASENSORDATA        (SN_BASEADDR + 0xC60)
#define SN_C3TSENSORDATA        (SN_BASEADDR + 0xC64)
#define SN_C3VSENSORMINDATA0    (SN_BASEADDR + 0xC68)
#define SN_C3VSENSORMINDATA1    (SN_BASEADDR + 0xC6C)
#define SN_C3VSENSORMAXDATA0    (SN_BASEADDR + 0xC70)
#define SN_C3VSENSORMAXDATA1    (SN_BASEADDR + 0xC74)
#define SN_C3PSESNORDATA0       (SN_BASEADDR + 0xC78)
#define SN_C3PSESNORDATA1       (SN_BASEADDR + 0xC7C)
#define SN_C4ASENSORDATA        (SN_BASEADDR + 0xC80)
#define SN_C4TSENSORDATA        (SN_BASEADDR + 0xC84)
#define SN_C4VSENSORMINDATA0    (SN_BASEADDR + 0xC88)
#define SN_C4VSENSORMINDATA1    (SN_BASEADDR + 0xC8C)
#define SN_C4VSENSORMAXDATA0    (SN_BASEADDR + 0xC90)
#define SN_C4VSENSORMAXDATA1    (SN_BASEADDR + 0xC94)
#define SN_C4PSESNORDATA0       (SN_BASEADDR + 0xC98)
#define SN_C4PSESNORDATA1       (SN_BASEADDR + 0xC9C)
#define SN_C5ASENSORDATA        (SN_BASEADDR + 0xCA0)
#define SN_C5TSENSORDATA        (SN_BASEADDR + 0xCA4)
#define SN_C5VSENSORMINDATA0    (SN_BASEADDR + 0xCA8)
#define SN_C5VSENSORMINDATA1    (SN_BASEADDR + 0xCAC)
#define SN_C5VSENSORMAXDATA0    (SN_BASEADDR + 0xCB0)
#define SN_C5VSENSORMAXDATA1    (SN_BASEADDR + 0xCB4)
#define SN_C5PSESNORDATA0       (SN_BASEADDR + 0xCB8)
#define SN_C5PSESNORDATA1       (SN_BASEADDR + 0xCBC)
#define SN_C6ASENSORDATA        (SN_BASEADDR + 0xCC0)
#define SN_C6TSENSORDATA        (SN_BASEADDR + 0xCC4)
#define SN_C6VSENSORMINDATA0    (SN_BASEADDR + 0xCC8)
#define SN_C6VSENSORMINDATA1    (SN_BASEADDR + 0xCCC)
#define SN_C6VSENSORMAXDATA0    (SN_BASEADDR + 0xCD0)
#define SN_C6VSENSORMAXDATA1    (SN_BASEADDR + 0xCD4)
#define SN_C6PSESNORDATA0       (SN_BASEADDR + 0xCD8)
#define SN_C6PSESNORDATA1       (SN_BASEADDR + 0xCDC)
#define SN_C7ASENSORDATA        (SN_BASEADDR + 0xCE0)
#define SN_C7TSENSORDATA        (SN_BASEADDR + 0xCE4)
#define SN_C7VSENSORMINDATA0    (SN_BASEADDR + 0xCE8)
#define SN_C7VSENSORMINDATA1    (SN_BASEADDR + 0xCEC)
#define SN_C7VSENSORMAXDATA0    (SN_BASEADDR + 0xCF0)
#define SN_C7VSENSORMAXDATA1    (SN_BASEADDR + 0xCF4)
#define SN_C7PSESNORDATA0       (SN_BASEADDR + 0xCF8)
#define SN_C7PSESNORDATA1       (SN_BASEADDR + 0xCFC)

#define SN_LCPUAGINGCOEF0       (SN_BASEADDR + 0xD00)
#define SN_LCPUAGINGCOEF1       (SN_BASEADDR + 0xD04)
#define SN_LCPUAGINGCOEF2       (SN_BASEADDR + 0xD08)
#define SN_LCPUTEMPCOEF0        (SN_BASEADDR + 0xD0C)
#define SN_LCPUTEMPCOEF1        (SN_BASEADDR + 0xD10)
#define SN_LCPUTEMPCOEF2        (SN_BASEADDR + 0xD14)
#define SN_LCPUVOLTCOEF0        (SN_BASEADDR + 0xD18)
#define SN_LCPUVOLTCOEF1        (SN_BASEADDR + 0xD1C)
#define SN_LCPUVOLTCOEF2        (SN_BASEADDR + 0xD20)
#define SN_LCPUVOLTCOEF3        (SN_BASEADDR + 0xD24)
#define SN_LCPUVOLTCOEF4        (SN_BASEADDR + 0xD28)
#define SN_LCPUVOLTCOEF5        (SN_BASEADDR + 0xD2C)
#define SN_LCPUVOLTCOEF6        (SN_BASEADDR + 0xD30)
#define SN_LCPUVOLTCOEF7        (SN_BASEADDR + 0xD34)
#define SN_LCPUPROCESSCOEF0     (SN_BASEADDR + 0xD38)
#define SN_LCPUPROCESSCOEF1     (SN_BASEADDR + 0xD3C)
#define SN_LCPUPROCESSCOEF2     (SN_BASEADDR + 0xD40)
#define SN_LCPUPROCESSCOEF3     (SN_BASEADDR + 0xD44)
#define SN_LCPUPROCESSCOEF4     (SN_BASEADDR + 0xD48)
#define SN_LCPUPROCESSCOEF5     (SN_BASEADDR + 0xD4C)
#define SN_LCPUPROCESSCOEF6     (SN_BASEADDR + 0xD50)
#define SN_LCPUPROCESSCOEF7     (SN_BASEADDR + 0xD54)
#define SN_LCPUPROCESSCOEF8     (SN_BASEADDR + 0xD58)
#define SN_LCPUPROCESSCOEF9     (SN_BASEADDR + 0xD5C)
#define SN_LCPUPROCESSCOEF10    (SN_BASEADDR + 0xD60)
#define SN_LCPUPROCESSCOEF11    (SN_BASEADDR + 0xD64)
#define SN_LCPUPROCESSCOEF12    (SN_BASEADDR + 0xD68)
#define SN_LCPUPROCESSCOEF13    (SN_BASEADDR + 0xD6C)
#define SN_LCPUPROCESSCOEF14    (SN_BASEADDR + 0xD70)
#define SN_LCPUPROCESSCOEF15    (SN_BASEADDR + 0xD74)
#define SN_LCPUPROCESSCOEF16    (SN_BASEADDR + 0xD78)
#define SN_LCPUPROCESSCOEF17    (SN_BASEADDR + 0xD7C)
#define SN_LCPUPROCESSCOEF18    (SN_BASEADDR + 0xD80)
#define SN_LCPUPROCESSCOEF19    (SN_BASEADDR + 0xD84)
#define SN_LCPUPROCESSCOEF20    (SN_BASEADDR + 0xD88)
#define SN_LCPUPROCESSCOEF21    (SN_BASEADDR + 0xD8C)
#define SN_LCPUPROCESSCOEF22    (SN_BASEADDR + 0xD90)
#define SN_LCPUPROCESSCOEF23    (SN_BASEADDR + 0xD94)
#define SN_LCPUPROCESSCOEF24    (SN_BASEADDR + 0xD98)
#define SN_LCPUPROCESSCOEF25    (SN_BASEADDR + 0xD9C)
#define SN_LCPUPROCESSCOEF26    (SN_BASEADDR + 0xDA0)
#define SN_LCPUPROCESSCOEF27    (SN_BASEADDR + 0xDA4)
#define SN_LCPUPROCESSCOEF28    (SN_BASEADDR + 0xDA8)
#define SN_LCPUPROCESSCOEF29    (SN_BASEADDR + 0xDAC)
#define SN_LCPUPROCESSCOEF30    (SN_BASEADDR + 0xDB0)
#define SN_LCPUPROCESSCOEF31    (SN_BASEADDR + 0xDB4)
#define SN_LCPUPROCESSCOEF32    (SN_BASEADDR + 0xDB8)
#define SN_LCPUPROCESSCOEF33    (SN_BASEADDR + 0xDBC)
#define SN_LCPUPROCESSCOEF34    (SN_BASEADDR + 0xDC0)
#define SN_LCPUPROCESSCOEF35    (SN_BASEADDR + 0xDC4)
#define SN_LCPUPROCESSCOEF36    (SN_BASEADDR + 0xDC8)
#define SN_LCPUINTERCEPTION0    (SN_BASEADDR + 0xDCC)
#define SN_LCPUINTERCEPTION1    (SN_BASEADDR + 0xDD0)
/* SN_LCPUINTERCEPTION1 = LCPUINTERCEPTION1WLCPUVOLTCOEF */

#define SN_BCPUAGINGCOEF0       (SN_BASEADDR + 0xE00)
#define SN_BCPUAGINGCOEF1       (SN_BASEADDR + 0xE04)
#define SN_BCPUAGINGCOEF2       (SN_BASEADDR + 0xE08)
#define SN_BCPUTEMPCOEF0        (SN_BASEADDR + 0xE0C)
#define SN_BCPUTEMPCOEF1        (SN_BASEADDR + 0xE10)
#define SN_BCPUTEMPCOEF2        (SN_BASEADDR + 0xE14)
#define SN_BCPUVOLTCOEF0        (SN_BASEADDR + 0xE18)
#define SN_BCPUVOLTCOEF1        (SN_BASEADDR + 0xE1C)
#define SN_BCPUVOLTCOEF2        (SN_BASEADDR + 0xE20)
#define SN_BCPUVOLTCOEF3        (SN_BASEADDR + 0xE24)
#define SN_BCPUVOLTCOEF4        (SN_BASEADDR + 0xE28)
#define SN_BCPUVOLTCOEF5        (SN_BASEADDR + 0xE2C)
#define SN_BCPUVOLTCOEF6        (SN_BASEADDR + 0xE30)
#define SN_BCPUVOLTCOEF7        (SN_BASEADDR + 0xE34)
#define SN_BCPUPROCESSCOEF0     (SN_BASEADDR + 0xE38)
#define SN_BCPUPROCESSCOEF1     (SN_BASEADDR + 0xE3C)
#define SN_BCPUPROCESSCOEF2     (SN_BASEADDR + 0xE40)
#define SN_BCPUPROCESSCOEF3     (SN_BASEADDR + 0xE44)
#define SN_BCPUPROCESSCOEF4     (SN_BASEADDR + 0xE48)
#define SN_BCPUPROCESSCOEF5     (SN_BASEADDR + 0xE4C)
#define SN_BCPUPROCESSCOEF6     (SN_BASEADDR + 0xE50)
#define SN_BCPUPROCESSCOEF7     (SN_BASEADDR + 0xE54)
#define SN_BCPUPROCESSCOEF8     (SN_BASEADDR + 0xE58)
#define SN_BCPUPROCESSCOEF9     (SN_BASEADDR + 0xE5C)
#define SN_BCPUPROCESSCOEF10    (SN_BASEADDR + 0xE60)
#define SN_BCPUPROCESSCOEF11    (SN_BASEADDR + 0xE64)
#define SN_BCPUPROCESSCOEF12    (SN_BASEADDR + 0xE68)
#define SN_BCPUPROCESSCOEF13    (SN_BASEADDR + 0xE6C)
#define SN_BCPUPROCESSCOEF14    (SN_BASEADDR + 0xE70)
#define SN_BCPUPROCESSCOEF15    (SN_BASEADDR + 0xE74)
#define SN_BCPUPROCESSCOEF16    (SN_BASEADDR + 0xE78)
#define SN_BCPUPROCESSCOEF17    (SN_BASEADDR + 0xE7C)
#define SN_BCPUPROCESSCOEF18    (SN_BASEADDR + 0xE80)
#define SN_BCPUPROCESSCOEF19    (SN_BASEADDR + 0xE84)
#define SN_BCPUPROCESSCOEF20    (SN_BASEADDR + 0xE88)
#define SN_BCPUPROCESSCOEF21    (SN_BASEADDR + 0xE8C)
#define SN_BCPUPROCESSCOEF22    (SN_BASEADDR + 0xE90)
#define SN_BCPUPROCESSCOEF23    (SN_BASEADDR + 0xE94)
#define SN_BCPUPROCESSCOEF24    (SN_BASEADDR + 0xE98)
#define SN_BCPUPROCESSCOEF25    (SN_BASEADDR + 0xE9C)
#define SN_BCPUPROCESSCOEF26    (SN_BASEADDR + 0xEA0)
#define SN_BCPUPROCESSCOEF27    (SN_BASEADDR + 0xEA4)
#define SN_BCPUPROCESSCOEF28    (SN_BASEADDR + 0xEA8)
#define SN_BCPUPROCESSCOEF29    (SN_BASEADDR + 0xEAC)
#define SN_BCPUPROCESSCOEF30    (SN_BASEADDR + 0xEB0)
#define SN_BCPUPROCESSCOEF31    (SN_BASEADDR + 0xEB4)
#define SN_BCPUPROCESSCOEF32    (SN_BASEADDR + 0xEB8)
#define SN_BCPUPROCESSCOEF33    (SN_BASEADDR + 0xEBC)
#define SN_BCPUPROCESSCOEF34    (SN_BASEADDR + 0xEC0)
#define SN_BCPUPROCESSCOEF35    (SN_BASEADDR + 0xEC4)
#define SN_BCPUPROCESSCOEF36    (SN_BASEADDR + 0xEC8)
#define SN_BCPUINTERCEPTION0    (SN_BASEADDR + 0xECC)
#define SN_BCPUINTERCEPTION1    (SN_BASEADDR + 0xED0)

#define MCUSYS_CPU0_BASEADDR	0x0C530000
#define MCUSYS_CPU1_BASEADDR	0x0C530800
#define MCUSYS_CPU2_BASEADDR	0x0C531000
#define MCUSYS_CPU3_BASEADDR	0x0C531800
#define MCUSYS_CPU4_BASEADDR	0x0C532000
#define MCUSYS_CPU5_BASEADDR	0x0C532800
#define MCUSYS_CPU6_BASEADDR	0x0C533000
#define MCUSYS_CPU7_BASEADDR	0x0C533800

#define L_FREQ_BASE				2000000
#define B_FREQ_BASE				2600000
#define	CCI_FREQ_BASE			1470000
#define B_M_FREQ_BASE			1800000

struct eemsn_devinfo {
	/* M_HW_RES0 0x11c1_0580 */
	unsigned int FT_PGM:8;
	unsigned int FT_BIN:4;
	unsigned int RSV0_1:20;
	/* M_HW_RES1 */
	unsigned int CPU_B_MTDES:8;
	unsigned int CPU_B_INITEN:1;
	unsigned int CPU_B_MONEN:1;
	unsigned int CPU_B_DVFS_LOW:3;
	unsigned int CPU_B_SPEC:3;
	unsigned int CPU_B_BDES:8;
	unsigned int CPU_B_MDES:8;

	/* M_HW_RES2 */
	unsigned int CPU_B_HI_MTDES:8;
	unsigned int CPU_B_HI_INITEN:1;
	unsigned int CPU_B_HI_MONEN:1;
	unsigned int CPU_B_HI_DVFS_LOW:3;
	unsigned int CPU_B_HI_SPEC:3;
	unsigned int CPU_B_HI_BDES:8;
	unsigned int CPU_B_HI_MDES:8;

	/* M_HW_RES3 */
	unsigned int CPU_B_LO_MTDES:8;
	unsigned int CPU_B_LO_INITEN:1;
	unsigned int CPU_B_LO_MONEN:1;
	unsigned int CPU_B_LO_DVFS_LOW:3;
	unsigned int CPU_B_LO_SPEC:3;
	unsigned int CPU_B_LO_BDES:8;
	unsigned int CPU_B_LO_MDES:8;

	/* M_HW_RES4 */
	unsigned int CPU_L_MTDES:8;
	unsigned int CPU_L_INITEN:1;
	unsigned int CPU_L_MONEN:1;
	unsigned int CPU_L_DVFS_LOW:3;
	unsigned int CPU_L_SPEC:3;
	unsigned int CPU_L_BDES:8;
	unsigned int CPU_L_MDES:8;

	/* M_HW_RES5 */
	unsigned int CPU_L_HI_MTDES:8;
	unsigned int CPU_L_HI_INITEN:1;
	unsigned int CPU_L_HI__MONEN:1;
	unsigned int CPU_L_HI_DVFS_LOW:3;
	unsigned int CPU_L_HI_SPEC:3;
	unsigned int CPU_L_HI_BDES:8;
	unsigned int CPU_L_HI_MDES:8;

	/* M_HW_RES6 */
	unsigned int CPU_L_LO_MTDES:8;
	unsigned int CPU_L_LO_INITEN:1;
	unsigned int CPU_L_LO_MONEN:1;
	unsigned int CPU_L_LO_DVFS_LOW:3;
	unsigned int CPU_L_LO_SPEC:3;
	unsigned int CPU_L_LO_BDES:8;
	unsigned int CPU_L_LO_MDES:8;

	/* M_HW_RES7 */
	unsigned int CCI_MTDES:8;
	unsigned int CCI_INITEN:1;
	unsigned int CCI_MONEN:1;
	unsigned int CCI_DVFS_LOW:3;
	unsigned int CCI_SPEC:3;
	unsigned int CCI_BDES:8;
	unsigned int CCI_MDES:8;

	/* M_HW_RES8 */
	unsigned int GPU_MTDES:8;
	unsigned int GPU_INITEN:1;
	unsigned int GPU_MONEN:1;
	unsigned int GPU_DVFS_LOW:3;
	unsigned int GPU_SPEC:3;
	unsigned int GPU_BDES:8;
	unsigned int GPU_MDES:8;

	/* M_HW_RES9 */
	unsigned int GPU_HI_MTDES:8;
	unsigned int GPU_HI_INITEN:1;
	unsigned int GPU_HI_MONEN:1;
	unsigned int GPU_HI_DVFS_LOW:3;
	unsigned int GPU_HI_SPEC:3;
	unsigned int GPU_HI_BDES:8;
	unsigned int GPU_HI_MDES:8;

	/* M_HW_RES10 */
	unsigned int GPU_LO_MTDES:8;
	unsigned int GPU_LO_INITEN:1;
	unsigned int GPU_LO_MONEN:1;
	unsigned int GPU_LO_DVFS_LOW:3;
	unsigned int GPU_LO_SPEC:3;
	unsigned int GPU_LO_BDES:8;
	unsigned int GPU_LO_MDES:8;

	/* M_HW_RES11 */
	unsigned int MD_VMODEM:32;

	/* M_HW_RES12 */
	unsigned int MD_VNR:32;

	/* M_HW_RES13 */
	unsigned int CPU_B_HI_DCBDET:8;
	unsigned int CPU_B_HI_DCMDET:8;
	unsigned int CPU_B_DCBDET:8;
	unsigned int CPU_B_DCMDET:8;

	/* M_HW_RES14 */
	unsigned int CPU_L_DCBDET:8;
	unsigned int CPU_L_DCMDET:8;
	unsigned int CPU_B_LO_DCBDET:8;
	unsigned int CPU_B_LO_DCMDET:8;

	/* M_HW_RES15 */
	unsigned int CPU_L_LO_DCBDET:8;
	unsigned int CPU_L_LO_DCMDET:8;
	unsigned int CPU_L_HI_DCBDET:8;
	unsigned int CPU_L_HI_DCMDET:8;

	/* M_HW_RES16 */
	unsigned int GPU_DCBDET:8;
	unsigned int GPU_DCMDET:8;
	unsigned int CCI_DCBDET:8;
	unsigned int CCI_DCMDET:8;


	/* M_HW_RES17 */
	unsigned int GPU_LO_DCBDET:8;
	unsigned int GPU_LO_DCMDET:8;
	unsigned int GPU_HI_DCBDET:8;
	unsigned int GPU_HI_DCMDET:8;

	/* M_HW_RES21 */
	unsigned int BCPU_A_T0_SVT:8;
	unsigned int BCPU_A_T0_LVT:8;
	unsigned int BCPU_A_T0_ULVT:8;
	unsigned int LCPU_A_T0_SVT:8;

	/* M_HW_RES22 */
	unsigned int LCPU_A_T0_LVT:8;
	unsigned int LCPU_A_T0_ULVT:8;
	unsigned int DELTA_VC_BCPU:4;
	unsigned int DELTA_VC_LCPU:4;
	unsigned int DELTA_VC_RT_BCPU:4;
	unsigned int DELTA_VC_RT_LCPU:4;

	/* M_HW_RES23 */
	unsigned int DELTA_VDPPM_BCPU:5;
	unsigned int DELTA_VDPPM_LCPU:5;
	unsigned int ATE_TEMP:3;
	unsigned int SN_PATTERN:3;
	unsigned int A_T0_SVT_BCPU_0P95V:8;
	unsigned int A_T0_SVT_LCPU_0P95V:8;

	/* M_HW_RES24 */
	unsigned int T_SVT_HV_BCPU:8;
	unsigned int T_SVT_LV_BCPU:8;
	unsigned int T_SVT_HV_LCPU:8;
	unsigned int T_SVT_LV_LCPU:8;

	/* M_HW_RES25 */
	unsigned int T_SVT_HV_BCPU_RT:8;
	unsigned int T_SVT_LV_BCPU_RT:8;
	unsigned int T_SVT_HV_LCPU_RT:8;
	unsigned int T_SVT_LV_LCPU_RT:8;
};


enum sn_det_id {
	SN_DET_L = 0,
#ifdef TRI_CLUSTER_SUPPORT
	SN_DET_BL,
#endif
	SN_DET_B,
	NR_SN_DET,
};

enum eemsn_det_id {
	EEMSN_DET_L,
#ifdef TRI_CLUSTER_SUPPORT
	EEMSN_DET_BL,
#endif
	EEMSN_DET_B,
	EEMSN_DET_CCI,

	NR_EEMSN_DET,
};

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
#ifdef CFG_3_GEAR
	MT_CPU_DVFS_B,
#endif
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

enum eem_phase {
	EEM_PHASE_INIT01,
	EEM_PHASE_INIT02,
	EEM_PHASE_MON,
	EEM_PHASE_SEN,

	NR_EEM_PHASE,
};

enum eem_features {
	FEA_INIT01	= BIT(EEM_PHASE_INIT01),
	FEA_INIT02	= BIT(EEM_PHASE_INIT02),
	FEA_MON		= BIT(EEM_PHASE_MON),
	FEA_SEN	= BIT(EEM_PHASE_SEN),
};

enum {
	IPI_EEMSN_SHARERAM_INIT,
	IPI_EEMSN_INIT,
	IPI_EEMSN_PROBE,
	IPI_EEMSN_INIT01,
	IPI_EEMSN_GET_EEM_VOLT,
	IPI_EEMSN_INIT02,
	IPI_EEMSN_DEBUG_PROC_WRITE,
	IPI_EEMSN_SEND_UPOWER_TBL_REF,

	IPI_EEMSN_CUR_VOLT_PROC_SHOW,

	IPI_EEMSN_DUMP_PROC_SHOW,
	IPI_EEMSN_AGING_DUMP_PROC_SHOW,

	IPI_EEMSN_OFFSET_PROC_WRITE,
	IPI_EEMSN_SNAGING_PROC_WRITE,

	IPI_EEMSN_LOGEN_PROC_SHOW,
	IPI_EEMSN_LOGEN_PROC_WRITE,

	IPI_EEMSN_EN_PROC_SHOW,
	IPI_EEMSN_EN_PROC_WRITE,
	IPI_EEMSN_SNEN_PROC_SHOW,
	IPI_EEMSN_SNEN_PROC_WRITE,
	// IPI_EEMSN_VCORE_GET_VOLT,
	// IPI_EEMSN_GPU_DVFS_GET_STATUS,
	IPI_EEMSN_FAKE_SN_INIT_ISR,
	IPI_EEMSN_FORCE_SN_SENSING,
	IPI_EEMSN_PULL_DATA,
	IPI_EEMSN_FAKE_SN_SENSING_ISR,
	NR_EEMSN_IPI,
};

struct A_Tused_VT {
	unsigned int A_Tused_SVT:8;
	unsigned int A_Tused_LVT:8;
	unsigned int A_Tused_ULVT:8;
	unsigned int A_Tused_RSV0:8;
};

struct dvfs_vf_tbl {
	unsigned short pi_freq_tbl[NR_PI_VF];
	unsigned char pi_volt_tbl[NR_PI_VF];
	unsigned char pi_vf_num;
};

struct sensing_stru {
#if VMIN_PREDICT_ENABLE
	uint64_t CPE_Vmin_HW;
#endif
	unsigned int SN_Vmin;
	int CPE_Vmin;
	unsigned int cur_volt;
#if !VMIN_PREDICT_ENABLE
	/* unsigned int count_cur_volt_HT; */
	int Sensor_Volt_HT;
	int Sensor_Volt_RT;
	int8_t CPE_temp;
	int8_t SN_temp;
	unsigned char T_SVT_current;
#endif
	unsigned short cur_temp;
	unsigned char cur_oppidx;
	unsigned char dst_volt_pmic;
};

struct sn_log_data {
	unsigned long long timestamp;
	unsigned int reg_dump_cpe[MIN_SIZE_SN_DUMP_CPE];
	unsigned int reg_dump_sndata[SIZE_SN_DUMP_SENSOR];
	unsigned int reg_dump_sn_cpu[NUM_SN_CPU][SIZE_SN_MCUSYS_REG];
	struct sensing_stru sd[NR_SN_DET];
	unsigned int footprint[NR_SN_DET];
	unsigned int allfp;
#if VMIN_PREDICT_ENABLE
	unsigned int sn_cpe_vop;
#endif
};

struct sn_log_cal_data {
	/* struct sn_param sn_cpu_param; */
	int64_t cpe_init_aging;
	struct A_Tused_VT atvt;
	int TEMP_CAL;
	int volt_cross;
	short CPE_Aging;
	int8_t sn_aging;
	int8_t delta_vc;
	uint8_t T_SVT_HV_RT;
	uint8_t T_SVT_LV_RT;
};

struct sn_param {
	/* for SN aging*/
	int Param_A_Tused_SVT;
	int Param_A_Tused_LVT;
	int Param_A_Tused_ULVT;
	int Param_A_T0_SVT;
	int Param_A_T0_LVT;
	int Param_A_T0_ULVT;
	int Param_ATE_temp;
	int Param_temp;
	int Param_INTERCEPTION;
	int8_t A_GB;
	int8_t sn_temp_threshold;
	int8_t Default_Aging;
	int8_t threshold_H;
	int8_t threshold_L;

	unsigned char T_GB;

	/* Formula for CPE_Vmin (Vmin prediction) */
	unsigned char CPE_GB;
	unsigned char MSSV_GB;

};

struct eemsn_det;

struct eemsn_det_ops {
	int (*get_volt)(struct eemsn_det *det);
	int (*get_temp)(struct eemsn_det *det);
	void (*get_freq_table)(struct eemsn_det *det);
	void (*get_orig_volt_table)(struct eemsn_det *det);

	/* interface to PMIC */
	int (*volt_2_pmic)(struct eemsn_det *det, int volt);
	int (*volt_2_eem)(struct eemsn_det *det, int volt);
	int (*pmic_2_volt)(struct eemsn_det *det, int pmic_val);
	int (*eem_2_pmic)(struct eemsn_det *det, int eev_val);
};


struct eemsn_det {
	int64_t		cpe_init_aging;
	int temp; /* det temperature */

	/* dvfs */
	unsigned int max_freq_khz;
	unsigned int mid_freq_khz;
	unsigned int turn_freq;
	unsigned int cur_volt;
	unsigned int *p_sn_cpu_coef;
	struct sn_param *p_sn_cpu_param;


	struct eemsn_det_ops *ops;
	enum eemsn_det_id det_id;

	unsigned int volt_tbl_pmic[MAX_NR_FREQ]; /* pmic value */

	/* for PMIC */
	unsigned short eemsn_v_base;
	unsigned short eemsn_step;
	unsigned short pmic_base;
	unsigned short pmic_step;
	short cpe_volt_total_mar;

	/* dvfs */
	unsigned short freq_tbl[MAX_NR_FREQ];
	//unsigned char volt_tbl[NR_FREQ]; /* eem value */
	unsigned char volt_tbl_init2[MAX_NR_FREQ]; /* eem value */
	unsigned char volt_tbl_orig[MAX_NR_FREQ]; /* pmic value */
	unsigned char dst_volt_pmic;
	unsigned char volt_tbl0_min; /* pmic value */

	unsigned char features; /* enum eemsn_features */
	unsigned char cur_phase;
	unsigned char cur_oppidx;

	const char *name;

	unsigned char disabled; /* Disabled by error or sysfs */
	unsigned char num_freq_tbl;

	unsigned char turn_pt;
	unsigned char vmin_high;
	unsigned char vmin_mid;
	int8_t delta_vc;
	int8_t sn_aging;
	int8_t volt_offset;
	int8_t volt_clamp;
	/* int volt_offset:8; */
	unsigned int delta_ir:4;
	unsigned int delta_vdppm:5;

#if UPDATE_TO_UPOWER
	/* only when init2, eemsn need to set volt to upower */
	unsigned int set_volt_to_upower:1;
#endif
};


struct eemsn_log_det {
	unsigned char mc50flag;
	unsigned char features;
	int8_t volt_clamp;
	int8_t volt_offset;
};

struct eemsn_vlog_det {
	unsigned int temp;
	unsigned short freq_tbl[MAX_NR_FREQ];
	unsigned short volt_tbl_pmic[MAX_NR_FREQ];
	unsigned char volt_tbl_orig[MAX_NR_FREQ];
	unsigned char volt_tbl_init2[MAX_NR_FREQ];
	/* unsigned char volt_tbl[NR_FREQ]; */
	unsigned char num_freq_tbl;
	unsigned char lock;
	unsigned char turn_pt;
	enum eemsn_det_id det_id;
};


struct eemsn_dbg_log {
	struct dvfs_vf_tbl vf_tbl_det[NR_EEMSN_DET];
	unsigned char eemsn_enable;
	unsigned char sn_enable;
	unsigned char ctrl_aging_Enable;
	struct eemsn_log_det det_log[NR_EEMSN_DET];
	struct eemsn_vlog_det det_vlog[NR_EEMSN_DET];
	struct sn_log_data sn_log;
	struct sn_log_cal_data sn_cal_data[NR_SN_DET];
	struct sn_param sn_cpu_param[NR_SN_DET];
	struct eemsn_devinfo efuse_devinfo;
	unsigned int efuse_sv;
	unsigned int efuse_sv2;
	unsigned int picachu_sn_mem_base_phys;
	unsigned char init2_v_ready;
	unsigned char init_vboot_done;
	unsigned char segCode;
	unsigned char lock;
#if ENABLE_COUNT_SNTEMP
	unsigned int sn_temp_cnt[NR_SN_DET][5];
#endif
};

struct eem_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} data;
	} u;
};

#define det_to_id(det)	((det) - &eemsn_detectors[0])

extern struct eemsn_det eemsn_detectors[NR_EEMSN_DET];
unsigned int detid_to_dvfsid(struct eemsn_det *det);

