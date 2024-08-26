/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_NU6601_H
#define __LINUX_NU6601_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

/*show debug message or not */
#define ENABLE_NU6601_DBG	0

/*
 * Device ID
 */

#define NU6601_DID_A		0x6601

/* NU6601 Private RegMap */
#define NU6601_REG_ANA_CTRL1				(0x90)
	#define VCONN_SS_TIME			BIT(7)
	#define VCONN_DISCHARGE_EN		BIT(6)
	#define VCONN_ALLOW				BIT(5)
	#define VCONN_OCP_LEVEL			BIT(2)
	#define PD_BUFFER_EN			BIT(1)
	#define DEAD_BAT_FORCEOFF		BIT(0)

#define NU6601_REG_CONNN_FLAG				(0x93)
	#define VCONN_OV_FLG		BIT(4)
	#define VCONN_UV_FLG		BIT(3)
	#define VCONN_OCP_FLG		BIT(2)
	#define VCONN_REV_FLG		BIT(0)

#define NU6601_REG_ANA_STATUS				(0x97)
	#define CC_PDPHY_OVP_STAT	BIT(2)
	#define VBUS_80				BIT(1)
	#define CABLE_TYPE			BIT(0)
	#define CABLE_A2C	(0)
	#define CABLE_C2C	(1)

#define NU6601_REG_ANA_INT					(0x98)
	#define INT_FSM_STA		BIT(4)
	#define INT_VBUS_80		BIT(1)

#define NU6601_REG_ANA_MASK					(0x99)
	#define M_FSM_STA		BIT(4)
	#define M_VBUS_80		BIT(1)

#define NU6601_REG_ANA_CTRL2				(0x9B)
	#define PD_AUTO_SHTD_DIS	BIT(6)
	#define TX_ALERT_IGNORE		BIT(5)
	#define CC_OVP_ENABLE		BIT(0)

#define NU6601_REG_ANA_CTRL3				(0x9F)
	#define CC_RX_VREF_SEL		BIT(5)
	#define CC_RX_VREF_SEL_EN	BIT(4)
	#define CC_STBY_PWR			BIT(3)
	#define CC_DISABLE			BIT(1)
	#define ERROR_RECOVERY		BIT(0)


#define NU6601_REG_SOFT_RST_CTRL			(0xA0)

#define NU6601_REG_DRP_CTRL					(0xA2)
	#define DRP_SRC_DC_SEL		BIT(2)
	#define TDRP_SEL			BIT(0)



#define NU6601_REG_CC_COMP_STATUS			(0xA3)
	#define CC_DET_COMP1		BIT(4)
	#define CC_DET_COMP2		BIT(0)

#define NU6601_REG_CC_FSM_STATUS			(0xA4)
	#define CC_POLARITY		BIT(5)
	#define CC_FSM			BIT(0)
#define CC_FSM_STATUS_MASK		0x1F
/*
 *Type-C CC Logic FSM states:
 *State changed would trigger
 * 0 (0x0): Unattached.SNK
 * 1 (0x1): Unattached.SRC
 * 2 (0x2): AttachWait.SNK
 * 3 (0x3): AttachWait.SRC
 * 4 (0x4): Try.SNK
 * 5 (0x5): Try.SRC (Reserved)
 * 6 (0x6): TryWait.SNK (Reserved)
 * 7 (0x7): TryWait.SRC
 * 8 (0x8): Attached.SNK
 * 9 (0x9): Attached.SRC
 * 10 (0xA): AudioAccessory
 * 13 (0xD): DebugAccessory.SNK
 * 14 (0xE): ErrorRecovery (No INT)
 * 15 (0xF): Disabled (No INT)
 * 16 (0x10): Manual Mode
 */
	#define UNATTACH_SNK		(0x0)
	#define UNATTACH_SRC		(0x1)
	#define ATTACHWAIT_SNK		(0x2)
	#define ATTACHWAIT_SRC		(0x3)
	#define TRY_SNK				(0x4)
	#define TRY_SRC				(0x5)
	#define TRYWAIT_SNK			(0x6)
	#define TRYWAIT_SRC			(0x7)
	#define ATTACHED_SNK		(0x8)
	#define ATTACHED_SRC		(0x9)
	#define AUDIOACC			(0xA)
	#define DEBUGACC_SNK		(0xD)
	#define DEBUGACC_NOINT		(0xE)
	#define DISABLED_NOINT		(0xF)
	#define MANUAL_MODE			(0x10)

/*
 *Type-C CC Logic FSM INT Mask:
 *0000 0001=1 Unattached.SNK & Unattached.SRC  INT Mask
 *0000 0010=1 AttachWait.SNK & AttachWait.SRC INT Mask
 *0000 0100=1 Try.SNK & Try.SRC  INT Mask
 *0000 1000=1 TryWait.SNK & TryWait.SRC INT Mask
 *0001 0000=1 Attached.SNK & Attached.SRC INT Mask
 *0010 0000=1DebugAccessory.SNK & AudioAccessory INT Mask
 */
#define NU6601_REG_CC_FSM_STATUS_MASK		(0xA5)
	#define CC_FSM_INT_MASK		BIT(0)

	#define UNATTACH_SNK_SRC_MASK	(0x01)
	#define ATTACHWAIT_SNK_SRC_MASK	(0x02)
	#define TRY_SNK_SRC_MASK		(0x04)
	#define TRYWAIT_SNK_SRC_MASK	(0x08)
	#define ATTACHED_SNK_SRC_MASK	(0x10)
	#define DEBUGACC_SNK_SRC_MASK	(0x20)


#define NU6601_REG_CC_FSM_CTRL				(0xA6)
	#define TRY_SNK_EN				BIT(7)
	#define DMA_SNK_EN				BIT(6)
	#define AUDIO_ACC_EN			BIT(5)
	#define POWER_SWAP				BIT(3)
	#define EXIT_SNK_BASED_ON_VBUS	BIT(2)
	#define EXIT_SNK_BASED_ON_CC	BIT(1)
	#define CC_MANUAL_MODE			BIT(0)



#define NU6601_REG_VBUS_DET_CTRL			(0xA7)
	#define VBUS_TCPC_PRESENT_THD	BIT(0)


#define NU6601_REG_CC_TIME_CTRL				(0xA8)
	#define CCDEB_120MS		(0x00)
	#define CCDEB_140MS		(0x01)
	#define CCDEB_160MS		(0x10)
	#define CCDEB_180MS		(0x11)

	#define PDDEB_12MS		(0x00)
	#define PDDEB_14MS		(0x01)
	#define PDDEB_16MS		(0x10)
	#define PDDEB_18MS		(0x11)

	#define DRPTRY_75MS		(0x00)
	#define DRPTRY_100MS	(0x01)
	#define DRPTRY_120MS	(0x10)
	#define DRPTRY_150MS	(0x11)

	#define SRCDIS_2MS		(0x00)
	#define SRCDIS_6MS		(0x01)
	#define SRCDIS_10MS		(0x10)
	#define SRCDIS_14MS		(0x11)

#define NU6601_REG_TEST_MODE_MUX			(0xA9)
	#define MUX_FOR_TCPC	BIT(4)
	#define MUX_FOR_PD		BIT(0)

#if ENABLE_NU6601_DBG
#define NU6601_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define NU6601_INFO(foramt, args...)
#endif

#endif /* #ifndef __LINUX_NU6601_H */
