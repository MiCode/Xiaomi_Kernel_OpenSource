/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MT_PTP_
#define _MT_PTP_

#include <linux/kernel.h>
#include <mt-plat/sync_write.h>

/* not used
#ifdef __MT_PTP_C__
#define PTP_EXTERN
#else
#define PTP_EXTERN extern
#endif
*/

#define PTP_EXTERN
#define HAS_PTPOD_VCORE		0	/* Disable PTPOD Vcore. Vcore DVFS dominates Vcore voltage */
#define EN_PTP_OD		(1)	/* enable/disable PTP-OD (SW) */
#define PTPOD_ID_SHIFT		12
#define PTPOD_ID_MASK		0x3F
extern void __iomem *ptpod_base;

/* PTP Register Definition */
#define PTP_BASEADDR        ptpod_base
/* overlap with PTP_TEMPSPARE3. #define PTP_REVISIONID      (PTP_BASEADDR + 0x0FC) */
#define PTP_DESCHAR         (PTP_BASEADDR + 0x200)
#define PTP_TEMPCHAR        (PTP_BASEADDR + 0x204)
#define PTP_DETCHAR         (PTP_BASEADDR + 0x208)
#define PTP_AGECHAR         (PTP_BASEADDR + 0x20C)
#define PTP_DCCONFIG        (PTP_BASEADDR + 0x210)
#define PTP_AGECONFIG       (PTP_BASEADDR + 0x214)
#define PTP_FREQPCT30       (PTP_BASEADDR + 0x218)
#define PTP_FREQPCT74       (PTP_BASEADDR + 0x21C)
#define PTP_LIMITVALS       (PTP_BASEADDR + 0x220)
#define PTP_VBOOT           (PTP_BASEADDR + 0x224)
#define PTP_DETWINDOW       (PTP_BASEADDR + 0x228)
#define PTP_PTPCONFIG       (PTP_BASEADDR + 0x22C)
#define PTP_TSCALCS         (PTP_BASEADDR + 0x230)
#define PTP_RUNCONFIG       (PTP_BASEADDR + 0x234)
#define PTP_PTPEN           (PTP_BASEADDR + 0x238)
#define PTP_INIT2VALS       (PTP_BASEADDR + 0x23C)
#define PTP_DCVALUES        (PTP_BASEADDR + 0x240)
#define PTP_AGEVALUES       (PTP_BASEADDR + 0x244)
#define PTP_VOP30           (PTP_BASEADDR + 0x248)
#define PTP_VOP74           (PTP_BASEADDR + 0x24C)
#define PTP_TEMP            (PTP_BASEADDR + 0x250)
#define PTP_PTPINTSTS       (PTP_BASEADDR + 0x254)
#define PTP_PTPINTSTSRAW    (PTP_BASEADDR + 0x258)
#define PTP_PTPINTEN        (PTP_BASEADDR + 0x25C)
#define PTP_CHKSHIFT        (PTP_BASEADDR + 0x264)
#define PTP_VDESIGN30       (PTP_BASEADDR + 0x26C)
#define PTP_VDESIGN74       (PTP_BASEADDR + 0x270)
#define PTP_DVT30           (PTP_BASEADDR + 0x274)
#define PTP_DVT74           (PTP_BASEADDR + 0x278)
#define PTP_AGECOUNT	    (PTP_BASEADDR + 0x27C)
#define PTP_SMSTATE0        (PTP_BASEADDR + 0x280)
#define PTP_SMSTATE1        (PTP_BASEADDR + 0x284)

#define PTP_PTPCORESEL      (PTP_BASEADDR + 0x400)
/* not used
#define PTPODCORE3EN        (19):(19)
#define PTPODCORE2EN        (18):(18)
#define PTPODCORE1EN        (17):(17)
#define PTPODCORE0EN        (16):(16)
#define APBSEL              (3):(0)
*/

#define PTP_THERMINTST      (PTP_BASEADDR + 0x404)
#define PTP_PTPODINTST      (PTP_BASEADDR + 0x408)
/* not used
#define PTPODINT0		(0):(0)
#define PTPODINT1		(1):(1)
#define PTPODINT2		(2):(2)
#define PTPODINT3		(3):(3)
*/

#define PTP_THSTAGE0ST      (PTP_BASEADDR + 0x40C)
#define PTP_THSTAGE1ST      (PTP_BASEADDR + 0x410)
#define PTP_THSTAGE2ST      (PTP_BASEADDR + 0x414)
#define PTP_THAHBST0        (PTP_BASEADDR + 0x418)
#define PTP_THAHBST1        (PTP_BASEADDR + 0x41C)
#define PTP_PTPSPARE0       (PTP_BASEADDR + 0x420)
#define PTP_PTPSPARE1       (PTP_BASEADDR + 0x424)
#define PTP_PTPSPARE2       (PTP_BASEADDR + 0x428)
#define PTP_PTPSPARE3       (PTP_BASEADDR + 0x42C)
#define PTP_THSLPEVEB       (PTP_BASEADDR + 0x430)

/* Thermal Register Definition */
#define THERMAL_BASE            ptpod_base

#define PTP_TEMPMONCTL0         (THERMAL_BASE + 0x000)
#define PTP_TEMPMONCTL1         (THERMAL_BASE + 0x004)
#define PTP_TEMPMONCTL2         (THERMAL_BASE + 0x008)
#define PTP_TEMPMONINT          (THERMAL_BASE + 0x00C)
#define PTP_TEMPMONINTSTS       (THERMAL_BASE + 0x010)
#define PTP_TEMPMONIDET0        (THERMAL_BASE + 0x014)
#define PTP_TEMPMONIDET1        (THERMAL_BASE + 0x018)
#define PTP_TEMPMONIDET2        (THERMAL_BASE + 0x01C)
#define PTP_TEMPH2NTHRE         (THERMAL_BASE + 0x024)
#define PTP_TEMPHTHRE           (THERMAL_BASE + 0x028)
#define PTP_TEMPCTHRE           (THERMAL_BASE + 0x02C)
#define PTP_TEMPOFFSETH         (THERMAL_BASE + 0x030)
#define PTP_TEMPOFFSETL         (THERMAL_BASE + 0x034)
#define PTP_TEMPMSRCTL0         (THERMAL_BASE + 0x038)
#define PTP_TEMPMSRCTL1         (THERMAL_BASE + 0x03C)
#define PTP_TEMPAHBPOLL         (THERMAL_BASE + 0x040)
#define PTP_TEMPAHBTO           (THERMAL_BASE + 0x044)
#define PTP_TEMPADCPNP0         (THERMAL_BASE + 0x048)
#define PTP_TEMPADCPNP1         (THERMAL_BASE + 0x04C)
#define PTP_TEMPADCPNP2         (THERMAL_BASE + 0x050)
#define PTP_TEMPADCMUX          (THERMAL_BASE + 0x054)
#define PTP_TEMPADCEXT          (THERMAL_BASE + 0x058)
#define PTP_TEMPADCEXT1         (THERMAL_BASE + 0x05C)
#define PTP_TEMPADCEN           (THERMAL_BASE + 0x060)
#define PTP_TEMPPNPMUXADDR      (THERMAL_BASE + 0x064)
#define PTP_TEMPADCMUXADDR      (THERMAL_BASE + 0x068)
#define PTP_TEMPADCEXTADDR      (THERMAL_BASE + 0x06C)
#define PTP_TEMPADCEXT1ADDR     (THERMAL_BASE + 0x070)
#define PTP_TEMPADCENADDR       (THERMAL_BASE + 0x074)
#define PTP_TEMPADCVALIDADDR    (THERMAL_BASE + 0x078)
#define PTP_TEMPADCVOLTADDR     (THERMAL_BASE + 0x07C)
#define PTP_TEMPRDCTRL          (THERMAL_BASE + 0x080)
#define PTP_TEMPADCVALIDMASK    (THERMAL_BASE + 0x084)
#define PTP_TEMPADCVOLTAGESHIFT (THERMAL_BASE + 0x088)
#define PTP_TEMPADCWRITECTRL    (THERMAL_BASE + 0x08C)
#define PTP_TEMPMSR0            (THERMAL_BASE + 0x090)
#define PTP_TEMPMSR1            (THERMAL_BASE + 0x094)
#define PTP_TEMPMSR2            (THERMAL_BASE + 0x098)
#define PTP_TEMPIMMD0           (THERMAL_BASE + 0x0A0)
#define PTP_TEMPIMMD1           (THERMAL_BASE + 0x0A4)
#define PTP_TEMPIMMD2           (THERMAL_BASE + 0x0A8)
#define PTP_TEMPMONIDET3        (THERMAL_BASE + 0x0B0)
#define PTP_TEMPADCPNP3         (THERMAL_BASE + 0x0B4)
#define PTP_TEMPMSR3            (THERMAL_BASE + 0x0B8)
#define PTP_TEMPIMMD3           (THERMAL_BASE + 0x0BC)
#define PTP_TEMPPROTCTL         (THERMAL_BASE + 0x0C0)
#define PTP_TEMPPROTTA          (THERMAL_BASE + 0x0C4)
#define PTP_TEMPPROTTB          (THERMAL_BASE + 0x0C8)
#define PTP_TEMPPROTTC          (THERMAL_BASE + 0x0CC)
#define PTP_TEMPSPARE0          (THERMAL_BASE + 0x0F0)
#define PTP_TEMPSPARE1          (THERMAL_BASE + 0x0F4)
#define PTP_TEMPSPARE2          (THERMAL_BASE + 0x0F8)
#define PTP_TEMPSPARE3          (THERMAL_BASE + 0x0FC)

/* PTP Structure */
typedef struct {
	unsigned int ADC_CALI_EN;
	unsigned int PTPINITEN;
	unsigned int PTPMONEN;

	unsigned int MDES;
	unsigned int BDES;
	unsigned int DCCONFIG;
	unsigned int DCMDET;
	unsigned int DCBDET;
	unsigned int AGECONFIG;
	unsigned int AGEM;
	unsigned int AGEDELTA;
	unsigned int DVTFIXED;
	unsigned int VCO;
	unsigned int MTDES;
	unsigned int MTS;
	unsigned int BTS;

	unsigned char FREQPCT0;
	unsigned char FREQPCT1;
	unsigned char FREQPCT2;
	unsigned char FREQPCT3;
	unsigned char FREQPCT4;
	unsigned char FREQPCT5;
	unsigned char FREQPCT6;
	unsigned char FREQPCT7;

	unsigned int DETWINDOW;
	unsigned int VMAX;
	unsigned int VMIN;
	unsigned int DTHI;
	unsigned int DTLO;
	unsigned int VBOOT;
	unsigned int DETMAX;

	unsigned int DCVOFFSETIN;
	unsigned int AGEVOFFSETIN;
} PTP_INIT_T;

typedef enum {
	PTP_CTRL_LITTLE = 0,
	PTP_CTRL_BIG = 1,
	PTP_CTRL_GPU = 2,
#if HAS_PTPOD_VCORE
	PTP_CTRL_VCORE = 3,
#endif
	NR_PTP_CTRL,
} ptp_ctrl_id;

typedef enum {
	PTP_DET_LITTLE = PTP_CTRL_LITTLE,
	PTP_DET_BIG = PTP_CTRL_BIG,
	PTP_DET_GPU = PTP_CTRL_GPU,
#if HAS_PTPOD_VCORE
	PTP_DET_VCORE_PDN = PTP_CTRL_VCORE,
#endif
	NR_PTP_DET,		/* 5 */
} ptp_det_id;

/* PTP Extern Function */
PTP_EXTERN unsigned int mt_ptp_get_level(void);
PTP_EXTERN void mt_ptp_lock(unsigned long *flags);
PTP_EXTERN void mt_ptp_unlock(unsigned long *flags);
PTP_EXTERN int mt_ptp_idle_can_enter(void);
PTP_EXTERN int mt_ptp_status(ptp_det_id id);
extern u32 get_devinfo_with_index(u32 index);
extern bool mt_gpufreq_IsPowerOn(void);

#undef PTP_EXTERN

#endif
