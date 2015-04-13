/*
 * intel_pmic_ccsm.h - Intel MID PMIC CCSM Driver header file
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Jenny TC <jenny.tc@intel.com>
 */

#include <linux/power/intel_pmic_ccsm.h>
/*********************************************************************
 *		Generic defines
 *********************************************************************/

#ifndef BASINCOVE_VENDORID
#define BASINCOVE_VENDORID	(3 << 6)
#endif

#define SHADYCOVE_VENDORID	0x00

#ifndef PMIC_VENDOR_ID_MASK
#define PMIC_VENDOR_ID_MASK	(3 << 6)
#endif

#define PMIC_MINOR_REV_MASK	0x07

#ifndef PMIC_MAJOR_REV_MASK
#define PMIC_MAJOR_REV_MASK	(7 << 3)
#endif

#define	PMIC_BZONE_LOW		0
#define	PMIC_BZONE_HIGH		5

#define BC_PMIC_MAJOR_REV_A0	0x00
#define BC_PMIC_MAJOR_REV_B0	(1 << 3)

#define IRQLVL1_CHRGR_MASK		(1 << 5)

#define THRMZN0_SC_ADCVAL		0x25A1
#define THRMZN1_SC_ADCVAL		0x3512
#define THRMZN2_SC_ADCVAL		0x312D
#define THRMZN3_SC_ADCVAL		0x20FE
#define THRMZN4_SC_ADCVAL		0x10B8

#define CHGIRQ0_BZIRQ_MASK		(1 << 7)
#define CHGIRQ0_BAT_CRIT_MASK		(1 << 6)
#define CHGIRQ0_BAT1_ALRT_MASK		(1 << 5)
#define CHGIRQ0_BAT0_ALRT_MASK		(1 << 4)

#define MCHGIRQ0_RSVD_MASK		(1 << 7)
#define MCHGIRQ0_MBAT_CRIT_MASK		(1 << 6)
#define MCHGIRQ0_MBAT1_ALRT_MASK	(1 << 5)
#define MCHGIRQ0_MBAT0_ALRT_MASK	(1 << 4)

#define SCHGIRQ0_RSVD_MASK		(1 << 7)
#define SCHGIRQ0_SBAT_CRIT_MASK		(1 << 6)
#define SCHGIRQ0_SBAT1_ALRT_MASK	(1 << 5)
#define SCHGIRQ0_SBAT0_ALRT_MASK	(1 << 4)

#define CHRGRIRQ1_SUSBIDGNDDET_MASK	(1 << 4)
#define CHRGRIRQ1_SUSBIDFLTDET_MASK	(1 << 3)
#define CHRGRIRQ1_SUSBIDDET_MASK	(1 << 3)
#define CHRGRIRQ1_SBATTDET_MASK		(1 << 2)
#define CHRGRIRQ1_SDCDET_MASK		(1 << 1)
#define CHRGRIRQ1_SVBUSDET_MASK		(1 << 0)
#define MCHRGRIRQ1_SUSBIDGNDDET_MASK	(1 << 4)
#define MCHRGRIRQ1_SUSBIDFLTDET_MASK	(1 << 3)
#define MCHRGRIRQ1_SUSBIDDET_MASK	(1 << 3)
#define MCHRGRIRQ1_SBATTDET_MAS		(1 << 2)
#define MCHRGRIRQ1_SDCDET_MASK		(1 << 1)
#define MCHRGRIRQ1_SVBUSDET_MASK	(1 << 0)
#define SCHRGRIRQ1_SUSBIDGNDDET_MASK	(3 << 3)
#define SCHRGRIRQ1_SUSBIDDET_MASK	(1 << 3)
#define SCHRGRIRQ1_SBATTDET_MASK	(1 << 2)
#define SCHRGRIRQ1_SDCDET_MASK		(1 << 1)
#define SCHRGRIRQ1_SVBUSDET_MASK	(1 << 0)
#define SHRT_GND_DET			(1 << 3)
#define SHRT_FLT_DET			(1 << 4)

#define PMIC_CHRGR_INT0_MASK		0xB1
#define PMIC_CHRGR_CCSM_INT0_MASK	0xB0
#define PMIC_CHRGR_EXT_CHRGR_INT_MASK	0x01

#define CHGRCTRL0_WDT_NOKICK_MASK	(1 << 7)
#define CHGRCTRL0_DBPOFF_MASK		(1 << 6)
#define CHGRCTRL0_CCSM_OFF_MASK		(1 << 5)
#define CHGRCTRL0_TTLCK_MASK		(1 << 4)
#define CHGRCTRL0_SWCONTROL_MASK	(1 << 3)
#define CHGRCTRL0_EXTCHRDIS_MASK	(1 << 2)
#define	CHRCTRL0_EMRGCHREN_MASK		(1 << 1)
#define	CHRCTRL0_CHGRRESET_MASK		(1 << 0)

#define WDT_NOKICK_ENABLE		(1 << 7)
#define WDT_NOKICK_DISABLE		(~WDT_NOKICK_ENABLE & 0xFF)

#define EXTCHRDIS_ENABLE		(1 << 2)
#define EXTCHRDIS_DISABLE		(~EXTCHRDIS_ENABLE & 0xFF)
#define SWCONTROL_ENABLE		(1 << 3)
#define EMRGCHREN_ENABLE		(1 << 1)

#define CHGRCTRL1_DBPEN_MASK		(1 << 7)
#define CHGRCTRL1_OTGMODE_MASK		(1 << 6)
#define CHGRCTRL1_FTEMP_EVENT_MASK	(1 << 5)
#define CHGRCTRL1_FUSB_INLMT_1500	(1 << 4)
#define CHGRCTRL1_FUSB_INLMT_900	(1 << 3)
#define CHGRCTRL1_FUSB_INLMT_500	(1 << 2)
#define CHGRCTRL1_FUSB_INLMT_150	(1 << 1)
#define CHGRCTRL1_FUSB_INLMT_100	(1 << 0)

#define CHGRSTATUS_SDPB_MASK		(1 << 4)
#define CHGRSTATUS_CHGDISLVL_MASK	(1 << 2)
#define CHGRSTATUS_CHGDETB_LATCH_MASK	(1 << 1)
#define CHGDETB_MASK			(1 << 0)

#define THRMBATZONE_MASK		0x07

#define USBIDEN_MASK		0x01
#define ACADETEN_MASK		(1 << 1)

#define ID_SHORT		(1 << 4)
#define ID_SHORT_VBUS		(1 << 4)
#define ID_NOT_SHORT_VBUS	0
#define ID_FLOAT_STS		(1 << 3)
#define R_ID_FLOAT_DETECT	(1 << 3)
#define R_ID_FLOAT_NOT_DETECT	0
#define ID_RAR_BRC_STS		(3 << 1)
#define ID_ACA_NOT_DETECTED	0
#define R_ID_A			(1 << 1)
#define R_ID_B			(2 << 1)
#define R_ID_C			(3 << 1)
#define ID_GND			(1 << 0)
#define ID_TYPE_A		0
#define ID_TYPE_B		1
#define is_aca(x) ((x & R_ID_A) || (x & R_ID_B) || (x & R_ID_C))


#define USBSRCDET_RETRY_CNT		5
#define USBSRCDET_SLEEP_TIME		200
#define USBSRCDET_SUSBHWDET_MASK	(3 << 0)
#define USBSRCDET_USBSRCRSLT_MASK	(0x0F << 2)
#define USBSRCDET_SDCD_MASK		(1 << 6)
#define USBSRCDET_SUSBHWDET_DETON	(1 << 0)
#define USBSRCDET_SUSBHWDET_DETSUCC	(1 << 1)
#define USBSRCDET_SUSBHWDET_DETFAIL	(3 << 0)

#define USBPHYCTRL_CHGDET_N_POL_MASK	(1 << 1)
#define USBPHYCTRL_USBPHYRSTB_MASK	(1 << 0)

/* Registers on I2C-dev2-0x6E */
#define USBPATH_USBSEL_MASK	(1 << 3)

#define HVDCPDET_SLEEP_TIME		2000

#define DBG_USBBC1_SWCTRL_EN_MASK	(1 << 7)
#define DBG_USBBC1_EN_CMP_DM_MASK	(1 << 2)
#define DBG_USBBC1_EN_CMP_DP_MASK	(1 << 1)
#define DBG_USBBC1_EN_CHG_DET_MASK	(1 << 0)

#define DBG_USBBC2_EN_VDMSRC_MASK	(1 << 1)
#define DBG_USBBC2_EN_VDPSRC_MASK	(1 << 0)

#define DBG_USBBCSTAT_VDATDET_MASK	(1 << 2)
#define DBG_USBBCSTAT_CMP_DM_MASK	(1 << 1)
#define DBG_USBBCSTAT_CMP_DP_MASK	(1 << 0)

#define TT_I2CDADDR_ADDR		0x00
#define TT_CHGRINIT0OS_ADDR		0x01
#define TT_CHGRINIT1OS_ADDR		0x02
#define TT_CHGRINIT2OS_ADDR		0x03
#define TT_CHGRINIT3OS_ADDR		0x04
#define TT_CHGRINIT4OS_ADDR		0x05
#define TT_CHGRINIT5OS_ADDR		0x06
#define TT_CHGRINIT6OS_ADDR		0x07
#define TT_CHGRINIT7OS_ADDR		0x08
#define TT_USBINPUTICCOS_ADDR		0x09
#define TT_USBINPUTICCMASK_ADDR		0x0A
#define TT_CHRCVOS_ADDR			0X0B
#define TT_CHRCVMASK_ADDR		0X0C
#define TT_CHRCCOS_ADDR			0X0D
#define TT_CHRCCMASK_ADDR		0X0E
#define TT_LOWCHROS_ADDR		0X0F
#define TT_LOWCHRMASK_ADDR		0X10
#define TT_WDOGRSTOS_ADDR		0X11
#define TT_WDOGRSTMASK_ADDR		0X12
#define TT_CHGRENOS_ADDR		0X13
#define TT_CHGRENMASK_ADDR		0X14

#define TT_CUSTOMFIELDEN_ADDR		0X15
#define TT_HOT_LC_EN			(1 << 1)
#define TT_COLD_LC_EN			(1 << 0)
#define TT_HOT_COLD_LC_MASK		(TT_HOT_LC_EN | TT_COLD_LC_EN)
#define TT_HOT_COLD_LC_EN		(TT_HOT_LC_EN | TT_COLD_LC_EN)
#define TT_HOT_COLD_LC_DIS		0

#define TT_CHGRINIT0VAL_ADDR		0X20
#define TT_CHGRINIT1VAL_ADDR		0X21
#define TT_CHGRINIT2VAL_ADDR		0X22
#define TT_CHGRINIT3VAL_ADDR		0X23
#define TT_CHGRINIT4VAL_ADDR		0X24
#define TT_CHGRINIT5VAL_ADDR		0X25
#define TT_CHGRINIT6VAL_ADDR		0X26
#define TT_CHGRINIT7VAL_ADDR		0X27
#define TT_USBINPUTICC100VAL_ADDR	0X28
#define TT_USBINPUTICC150VAL_ADDR	0X29
#define TT_USBINPUTICC500VAL_ADDR	0X2A
#define TT_USBINPUTICC900VAL_ADDR	0X2B
#define TT_USBINPUTICC1500VAL_ADDR	0X2C
#define TT_CHRCVEMRGLOWVAL_ADDR		0X2D
#define TT_CHRCVCOLDVAL_ADDR		0X2E
#define TT_CHRCVCOOLVAL_ADDR		0X2F
#define TT_CHRCVWARMVAL_ADDR		0X30
#define TT_CHRCVHOTVAL_ADDR		0X31
#define TT_CHRCVEMRGHIVAL_ADDR		0X32
#define TT_CHRCCEMRGLOWVAL_ADDR		0X33
#define TT_CHRCCCOLDVAL_ADDR		0X34
#define TT_CHRCCCOOLVAL_ADDR		0X35
#define TT_CHRCCWARMVAL_ADDR		0X36
#define TT_CHRCCHOTVAL_ADDR		0X37
#define TT_CHRCCEMRGHIVAL_ADDR		0X38
#define TT_LOWCHRENVAL_ADDR		0X39
#define TT_LOWCHRDISVAL_ADDR		0X3A
#define TT_WDOGRSTVAL_ADDR		0X3B
#define TT_CHGRENVAL_ADDR		0X3C
#define TT_CHGRDISVAL_ADDR		0X3D

#define MTHRMIRQ1_CCSM_MASK		0x80
#define MTHRMIRQ2_CCSM_MASK		0x3

#define MPWRSRCIRQ_CCSM_MASK		0x9F
#define MPWRSRCIRQ_CCSM_VAL		0x84

#define CHGDISFN_EN_CCSM_VAL		0x50
#define CHGDISFN_DIS_CCSM_VAL		0x11
#define CHGDISFN_CCSM_MASK		0x51

/*Interrupt registers*/
#define BATT_CHR_BATTDET_MASK	(1 << 2)
/*Status registers*/
#define BATT_PRESENT		1
#define BATT_NOT_PRESENT	0

#define BATT_STRING_MAX		8
#define BATTID_STR_LEN		8

#define CHARGER_PRESENT		1
#define CHARGER_NOT_PRESENT	0

/*FIXME: Modify default values */
#define BATT_DEAD_CUTOFF_VOLT		3400	/* 3400 mV */
#define BATT_CRIT_CUTOFF_VOLT		3700	/* 3700 mV */

#define MSIC_BATT_TEMP_MAX		60	/* 60 degrees */
#define MSIC_BATT_TEMP_MIN		0

#define BATT_TEMP_WARM			45	/* 45 degrees */
#define MIN_BATT_PROF			4

#define PMIC_REG_NAME_LEN		28
#define PMIC_REG_DEF(x) { .reg_name = #x, .addr = x }

#define BIT_POS(x) BIT(x - ((x/16)*16))

#define PMIC_CCSM_IRQ_MAX 6

enum pmic_models {
	INTEL_PMIC_UNKNOWN = 0,
	INTEL_PMIC_BCOVE,
	INTEL_PMIC_SCOVE,
	INTEL_PMIC_WCOVE,
};

enum pmic_charger_aca_type {
	RID_UNKNOWN = 0,
	RID_A,
	RID_B,
	RID_C,
	RID_FLOAT,
	RID_GND,
};

enum pmic_charger_cable_type {
	PMIC_CHARGER_TYPE_NONE = 0,
	PMIC_CHARGER_TYPE_SDP,
	PMIC_CHARGER_TYPE_DCP,
	PMIC_CHARGER_TYPE_CDP,
	PMIC_CHARGER_TYPE_ACA,
	PMIC_CHARGER_TYPE_SE1,
	PMIC_CHARGER_TYPE_MHL,
	PMIC_CHARGER_TYPE_FLOAT_DP_DN,
	PMIC_CHARGER_TYPE_OTHER,
	PMIC_CHARGER_TYPE_DCP_EXTPHY,
};


char *pmic_regs_name[] = {
	"pmic_id",
	"pmic_irqlvl1",
	"pmic_mirqlvl1",
	"pmic_chgrirq0",
	"pmic_schgrirq0",
	"pmic_mchgrirq0",
	"pmic_chgrirq1",
	"pmic_schgrirq1",
	"pmic_mchgrirq1",
	"pmic_chgrctrl0",
	"pmic_chgrctrl1",
	"pmic_lowbattdet0",
	"pmic_lowbattdet1",
	"pmic_battdetctrl",
	"pmic_vbusdetctrl",
	"pmic_vdcindetctrl",
	"pmic_chgrstatus",
	"pmic_usbidctrl",
	"pmic_usbidstat",
	"pmic_wakesrc",
	"pmic_usbphyctrl",
	"pmic_dbg_usbbc1",
	"pmic_dbg_usbbc2",
	"pmic_dbg_usbbcstat",
	"pmic_usbpath",
	"pmic_usbsrcdetstat",
	"pmic_chrttaddr",
	"pmic_chrttdata",
	"pmic_thrmbatzone",
	"pmic_thrmzn0h",
	"pmic_thrmzn0l",
	"pmic_thrmzn1h",
	"pmic_thrmzn1l",
	"pmic_thrmzn2h",
	"pmic_thrmzn2l",
	"pmic_thrmzn3h",
	"pmic_thrmzn3l",
	"pmic_thrmzn4h",
	"pmic_thrmzn4l",
	"pmic_thrmirq0",
	"pmic_mthrmirq0",
	"pmic_sthrmirq0",
	"pmic_thrmirq1",
	"pmic_mthrmirq1",
	"pmic_sthrmirq1",
	"pmic_thrmirq2",
	"pmic_mthrmirq2",
	"pmic_sthrmirq2",
};

struct pmic_event {
	struct list_head node;
	u16 pwrsrc_int;
	u16 battemp_int;
	u16 misc_int;
	u16 pwrsrc_int_stat;
	u16 battemp_int_stat;
	u16 misc_int_stat;
};

struct pmic_regs_def {
	char reg_name[PMIC_REG_NAME_LEN];
	u16 addr;
};

struct pmic_chrgr_drv_context {
	bool invalid_batt;
	bool is_batt_present;
	bool current_sense_enabled;
	bool is_internal_usb_phy;
	enum pmic_charger_cable_type charger_type;
	bool otg_mode_enabled;
	bool tt_lock;
	unsigned int irq[PMIC_CCSM_IRQ_MAX];		/* GPE_ID or IRQ# */
	int vbus_state;
	int irq_cnt;
	int batt_health;
	int pmic_model;
	int intmap_size;
	int reg_cnt;
	void __iomem *pmic_intr_iomap;
	struct pmic_regs *reg_map;
	struct device *dev;
	struct pmic_ccsm_int_cfg *intmap;
	struct ps_batt_chg_prof *bcprof;
	struct ps_pse_mod_prof *actual_bcprof;
	struct ps_pse_mod_prof *runtime_bcprof;
	struct intel_pmic_ccsm_platform_data *pdata;
	struct usb_phy *otg;
	struct thermal_cooling_device *vbus_cdev;
	struct list_head evt_queue;
	struct work_struct evt_work;
	struct mutex evt_queue_lock;
};
