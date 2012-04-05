/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_IRQS_9615_H
#define __ASM_ARCH_MSM_IRQS_9615_H

/* MSM ACPU Interrupt Numbers */

/*
 * 0-15:  STI/SGI (software triggered/generated interrupts)
 * 16-31: PPI (private peripheral interrupts)
 * 32+:   SPI (shared peripheral interrupts)
 */

#define FIQ_START     16
#define GIC_PPI_START 16
#define GIC_SPI_START 32

#define INT_DEBUG_TIMER_EXP			(GIC_PPI_START + 1)
#define INT_GP_TIMER_EXP			(GIC_PPI_START + 2)
#define INT_GP_TIMER2_EXP			(GIC_PPI_START + 3)
#define WDT0_ACCSCSSNBARK_INT			(GIC_PPI_START + 4)
#define WDT1_ACCSCSSNBARK_INT			(GIC_PPI_START + 5)
#define AVS_SVICINT				(GIC_PPI_START + 6)
#define AVS_SVICINTSWDONE			(GIC_PPI_START + 7)
#define CPU_DBGCPUXCOMMRXFULL			(GIC_PPI_START + 8)
#define CPU_DBGCPUXCOMMTXEMPTY			(GIC_PPI_START + 9)
#define INT_ARMQC_PERFMON			(GIC_PPI_START + 10)
#define SC_AVSCPUXDOWN				(GIC_PPI_START + 11)
#define SC_AVSCPUXUP				(GIC_PPI_START + 12)
#define SC_SICCPUXACGIRPTREQ			(GIC_PPI_START + 13)
#define SC_SICCPUXEXTFAULTIRPTREQ		(GIC_PPI_START + 14)
/* PPI 15 is unused */

#define APCC_QGICACGIRPTREQ			(GIC_SPI_START + 0)
#define APCC_QGICL2PERFMONIRPTREQ		(GIC_SPI_START + 1)
#define SC_SICL2PERFMONIRPTREQ			APCC_QGICL2PERFMONIRPTREQ
#define APCC_QGICL2IRPTREQ			(GIC_SPI_START + 2)
#define APCC_QGICMPUIRPTREQ			(GIC_SPI_START + 3)
#define TLMM_MSM_DIR_CONN_IRQ_0			(GIC_SPI_START + 4)
#define TLMM_MSM_DIR_CONN_IRQ_1			(GIC_SPI_START + 5)
#define TLMM_MSM_DIR_CONN_IRQ_2			(GIC_SPI_START + 6)
#define TLMM_MSM_DIR_CONN_IRQ_3			(GIC_SPI_START + 7)
#define TLMM_MSM_DIR_CONN_IRQ_4			(GIC_SPI_START + 8)
#define TLMM_MSM_DIR_CONN_IRQ_5			(GIC_SPI_START + 9)
#define TLMM_MSM_DIR_CONN_IRQ_6			(GIC_SPI_START + 10)
#define TLMM_MSM_DIR_CONN_IRQ_7			(GIC_SPI_START + 11)
#define TLMM_MSM_DIR_CONN_IRQ_8			(GIC_SPI_START + 12)
#define TLMM_MSM_DIR_CONN_IRQ_9			(GIC_SPI_START + 13)
/* 14 Reserved */
#define PM8018_SEC_IRQ_N			(GIC_SPI_START + 15)
#define TLMM_MSM_SUMMARY_IRQ			(GIC_SPI_START + 16)
#define SPDM_RT_1_IRQ				(GIC_SPI_START + 17)
#define SPDM_DIAG_IRQ				(GIC_SPI_START + 18)
#define RPM_APCC_CPU0_GP_HIGH_IRQ		(GIC_SPI_START + 19)
#define RPM_APCC_CPU0_GP_MEDIUM_IRQ		(GIC_SPI_START + 20)
#define RPM_APCC_CPU0_GP_LOW_IRQ		(GIC_SPI_START + 21)
#define RPM_APCC_CPU0_WAKE_UP_IRQ		(GIC_SPI_START + 22)
/* 23-28 Reserved */
#define SSBI2_1_SC_CPU0_SECURE_IRQ		(GIC_SPI_START + 29)
#define SSBI2_1_SC_CPU0_NON_SECURE_IRQ		(GIC_SPI_START + 30)
/* 31 Reserved */
#define MSMC_SC_PRI_CE_IRQ			(GIC_SPI_START + 32)
#define SLIMBUS0_CORE_EE1_IRQ			(GIC_SPI_START + 33)
#define SLIMBUS0_BAM_EE1_IRQ			(GIC_SPI_START + 34)
#define Q6FW_WDOG_EXPIRED_IRQ			(GIC_SPI_START + 35)
#define Q6SW_WDOG_EXPIRED_IRQ			(GIC_SPI_START + 36)
#define MSS_TO_APPS_IRQ_0			(GIC_SPI_START + 37)
#define MSS_TO_APPS_IRQ_1			(GIC_SPI_START + 38)
#define MSS_TO_APPS_IRQ_2			(GIC_SPI_START + 39)
#define MSS_TO_APPS_IRQ_3			(GIC_SPI_START + 40)
#define MSS_TO_APPS_IRQ_4			(GIC_SPI_START + 41)
#define MSS_TO_APPS_IRQ_5			(GIC_SPI_START + 42)
#define MSS_TO_APPS_IRQ_6			(GIC_SPI_START + 43)
#define MSS_TO_APPS_IRQ_7			(GIC_SPI_START + 44)
#define MSS_TO_APPS_IRQ_8			(GIC_SPI_START + 45)
#define MSS_TO_APPS_IRQ_9			(GIC_SPI_START + 46)
/* 47-84  Reserved */
#define LPASS_SCSS_AUDIO_IF_OUT0_IRQ		(GIC_SPI_START + 85)
#define LPASS_SCSS_MIDI_IRQ			(GIC_SPI_START + 86)
#define LPASS_Q6SS_WDOG_EXPIRED			(GIC_SPI_START + 87)
#define LPASS_SCSS_GP_LOW_IRQ			(GIC_SPI_START + 88)
#define LPASS_SCSS_GP_MEDIUM_IRQ		(GIC_SPI_START + 89)
#define LPASS_SCSS_GP_HIGH_IRQ			(GIC_SPI_START + 90)
#define TOP_IMEM_IRQ				(GIC_SPI_START + 91)
#define FABRIC_SYS_IRQ				(GIC_SPI_START + 92)
/* 93 Reserved */
#define USB1_HS_BAM_IRQ				(GIC_SPI_START + 94)
/* 95,96 unnamed */
#define SDC2_BAM_IRQ				(GIC_SPI_START + 97)
#define SDC1_BAM_IRQ				(GIC_SPI_START + 98)
#define FABRIC_SPS_IRQ				(GIC_SPI_START + 99)
#define USB1_HS_IRQ				(GIC_SPI_START + 100)
/* 101,102 unnamed */
#define SDC2_IRQ_0				(GIC_SPI_START + 103)
#define SDC1_IRQ_0				(GIC_SPI_START + 104)
#define SPS_BAM_DMA_IRQ				(GIC_SPI_START + 105)
#define SPS_SEC_VIOL_IRQ			(GIC_SPI_START + 106)
#define SPS_MTI_0				(GIC_SPI_START + 107)
#define SPS_MTI_1				(GIC_SPI_START + 108)
#define SPS_MTI_2				(GIC_SPI_START + 109)
#define SPS_MTI_3				(GIC_SPI_START + 110)
#define SPS_MTI_4				(GIC_SPI_START + 111)
#define SPS_MTI_5				(GIC_SPI_START + 112)
#define SPS_MTI_6				(GIC_SPI_START + 113)
#define SPS_MTI_7				(GIC_SPI_START + 114)
#define SPS_MTI_8				(GIC_SPI_START + 115)
#define SPS_MTI_9				(GIC_SPI_START + 116)
#define SPS_MTI_10				(GIC_SPI_START + 117)
#define SPS_MTI_11				(GIC_SPI_START + 118)
#define SPS_MTI_12				(GIC_SPI_START + 119)
#define SPS_MTI_13				(GIC_SPI_START + 120)
#define SPS_MTI_14				(GIC_SPI_START + 121)
#define SPS_MTI_15				(GIC_SPI_START + 122)
#define SPS_MTI_16				(GIC_SPI_START + 123)
#define SPS_MTI_17				(GIC_SPI_START + 124)
#define SPS_MTI_18				(GIC_SPI_START + 125)
#define SPS_MTI_19				(GIC_SPI_START + 126)
#define SPS_MTI_20				(GIC_SPI_START + 127)
#define SPS_MTI_21				(GIC_SPI_START + 128)
#define SPS_MTI_22				(GIC_SPI_START + 129)
#define SPS_MTI_23				(GIC_SPI_START + 130)
#define SPS_MTI_24				(GIC_SPI_START + 131)
#define SPS_MTI_25				(GIC_SPI_START + 132)
#define SPS_MTI_26				(GIC_SPI_START + 133)
#define SPS_MTI_27				(GIC_SPI_START + 134)
#define SPS_MTI_28				(GIC_SPI_START + 135)
#define SPS_MTI_29				(GIC_SPI_START + 136)
#define SPS_MTI_30				(GIC_SPI_START + 137)
#define SPS_MTI_31				(GIC_SPI_START + 138)
#define CSIPHY_0_4LN_IRQ			(GIC_SPI_START + 139)
#define CSIPHY_1_2LN_IRQ			(GIC_SPI_START + 140)
/* 141-145 Reserved */
#define GSBI1_UARTDM_IRQ			(GIC_SPI_START + 146)
#define GSBI1_QUP_IRQ				(GIC_SPI_START + 147)
#define GSBI2_UARTDM_IRQ			(GIC_SPI_START + 148)
#define GSBI2_QUP_IRQ			        (GIC_SPI_START + 149)
#define GSBI3_UARTDM_IRQ			(GIC_SPI_START + 150)
#define GSBI3_QUP_IRQ				(GIC_SPI_START + 151)
#define GSBI4_UARTDM_IRQ			(GIC_SPI_START + 152)
#define GSBI4_QUP_IRQ				(GIC_SPI_START + 153)
#define GSBI5_UARTDM_IRQ			(GIC_SPI_START + 154)
#define GSBI5_QUP_IRQ				(GIC_SPI_START + 155)
/* 156-167 Reserved */
#define MSMC_SC_SEC_TMR_IRQ			(GIC_SPI_START + 168)
#define MSMC_SC_SEC_WDOG_BARK_IRQ		(GIC_SPI_START + 169)
#define ADM_0_SCSS_0_IRQ			(GIC_SPI_START + 170)
#define ADM_0_SCSS_1_IRQ			(GIC_SPI_START + 171)
#define ADM_0_SCSS_2_IRQ			(GIC_SPI_START + 172)
#define ADM_0_SCSS_3_IRQ			(GIC_SPI_START + 173)
/* 174 Reserved */
#define CC_SCSS_WDT1CPU0BITEEXPIRED		(GIC_SPI_START + 175)
/* 176 Reserved */
#define CC_SCSS_WDT0CPU0BITEEXPIRED		(GIC_SPI_START + 177)
#define TSENS_UPPER_LOWER_INT			(GIC_SPI_START + 178)
/* 179-182 Reserved */
#define XPU_SUMMARY_IRQ				(GIC_SPI_START + 183)
#define BUS_EXCEPTION_SUMMARY_IRQ		(GIC_SPI_START + 184)
#define HSDDRX_EBI1CH0_IRQ			(GIC_SPI_START + 185)
/* 186-208 Reserved */
#define A2_BAM_IRQ				(GIC_SPI_START + 209)
/* 210-215 Reserved */
#define QDSS_ETB_IRQ				(GIC_SPI_START + 216)
/* 216 Reserved */
#define QDSS_CTI2KPSS_CPU0_IRQ			(GIC_SPI_START + 218)
#define TLMM_MSM_DIR_CONN_IRQ_16		(GIC_SPI_START + 219)
#define TLMM_MSM_DIR_CONN_IRQ_17		(GIC_SPI_START + 220)
#define TLMM_MSM_DIR_CONN_IRQ_18		(GIC_SPI_START + 221)
#define TLMM_MSM_DIR_CONN_IRQ_19		(GIC_SPI_START + 222)
#define TLMM_MSM_DIR_CONN_IRQ_20		(GIC_SPI_START + 223)
#define TLMM_MSM_DIR_CONN_IRQ_21		(GIC_SPI_START + 224)
#define MSM_SPARE0_IRQ				(GIC_SPI_START + 225)
#define PMIC_SEC_IRQ_N				(GIC_SPI_START + 226)
#define USB_HSIC_BAM_IRQ			(GIC_SPI_START + 231)
#define USB_HSIC_IRQ				(GIC_SPI_START + 232)

#define NR_MSM_IRQS 288
#define NR_GPIO_IRQS 88
#define NR_PM8018_IRQS 256
#define NR_WCD9XXX_IRQS 49
#define NR_TABLA_IRQS NR_WCD9XXX_IRQS
#define NR_BOARD_IRQS (NR_PM8018_IRQS + NR_WCD9XXX_IRQS)
#define NR_TLMM_MSM_DIR_CONN_IRQ 8 /*Need to Verify this Count*/
#define NR_MSM_GPIOS NR_GPIO_IRQS

/* Backwards compatible IRQ macros. */
#define INT_ADM_AARM				ADM_0_SCSS_0_IRQ

/* smd/smsm interrupts */
#define INT_A9_M2A_0                    MSS_TO_APPS_IRQ_0
#define INT_A9_M2A_5                    MSS_TO_APPS_IRQ_1
#define INT_ADSP_A11                    LPASS_SCSS_GP_HIGH_IRQ
#define INT_ADSP_A11_SMSM               LPASS_SCSS_GP_MEDIUM_IRQ

#endif
