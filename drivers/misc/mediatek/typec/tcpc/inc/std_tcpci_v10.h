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

#ifndef STD_TCPCI_V10_H_
#define STD_TCPCI_V10_H_

/* Standard TCPC V10 RegMap */

#define TCPC_V10_REG_VID				(0x00)
#define TCPC_V10_REG_PID				(0x02)
#define TCPC_V10_REG_DID				(0x04)
#define TCPC_V10_REG_TYPEC_REV				(0x06)
#define TCPC_V10_REG_PD_REV				(0x08)
#define TCPC_V10_REG_PDIF_REV				(0x0A)

#define TCPC_V10_REG_ALERT				(0x10)
#define TCPC_V10_REG_ALERT_MASK				(0x12)
#define TCPC_V10_REG_POWER_STATUS_MASK			(0x14)
#define TCPC_V10_REG_FAULT_STATUS_MASK			(0x15)

#define TCPC_V10_REG_TCPC_CTRL				(0x19)
#define TCPC_V10_REG_ROLE_CTRL				(0x1A)
#define TCPC_V10_REG_FAULT_CTRL				(0x1B)
#define TCPC_V10_REG_POWER_CTRL				(0x1C)

#define TCPC_V10_REG_CC_STATUS				(0x1D)
#define TCPC_V10_REG_POWER_STATUS			(0x1E)
#define TCPC_V10_REG_FAULT_STATUS			(0x1F)

#define TCPC_V10_REG_COMMAND				(0x23)

#define TCPC_V10_REG_MSG_HDR_INFO			(0x2e)

#define TCPC_V10_REG_RX_DETECT				(0x2f)

#define TCPC_V10_REG_RX_BYTE_CNT			(0x30)
#define TCPC_V10_REG_RX_BUF_FRAME_TYPE			(0x31)
#define TCPC_V10_REG_RX_HDR				(0x32)
#define TCPC_V10_REG_RX_DATA				(0x34)

#define TCPC_V10_REG_TRANSMIT				(0x50)
#define TCPC_V10_REG_TX_BYTE_CNT			(0x51)
#define TCPC_V10_REG_TX_HDR				(0x52)
#define TCPC_V10_REG_TX_DATA				(0x54)/* through 0x6f */

/*
 * TCPC_V10_REG_ALERT				(0x10)
 * TCPC_V10_REG_ALERT_MASK		(0x12)
 */
#define TCPC_V10_REG_VBUS_SINK_DISCONNECT		(1<<11)
#define TCPC_V10_REG_RX_OVERFLOW			(1<<10)
#define TCPC_V10_REG_ALERT_FAULT			(1<<9)
#define TCPC_V10_REG_ALERT_LO_VOLT			(1<<8)
#define TCPC_V10_REG_ALERT_HI_VOLT			(1<<7)
#define TCPC_V10_REG_ALERT_TX_SUCCESS			(1<<6)
#define TCPC_V10_REG_ALERT_TX_DISCARDED			(1<<5)
#define TCPC_V10_REG_ALERT_TX_FAILED			(1<<4)
#define TCPC_V10_REG_ALERT_RX_HARD_RST			(1<<3)
#define TCPC_V10_REG_ALERT_RX_STATUS			(1<<2)
#define TCPC_V10_REG_ALERT_POWER_STATUS			(1<<1)
#define TCPC_V10_REG_ALERT_CC_STATUS			(1<<0)

/*
 * TCPC_V10_REG_POWER_STATUS_MASK	(0x14)
 * TCPC_V10_REG_POWER_STATUS			(0x19)
 */

#define TCPC_V10_REG_POWER_STATUS_TCPC_INITIAL		(1<<6)
#define TCPC_V10_REG_POWER_STATUS_SRC_HV		(1<<5)
#define TCPC_V10_REG_POWER_STATUS_SRC_VBUS		(1<<4)
#define TCPC_V10_REG_POWER_STATUS_VBUS_PRES_DET		(1<<3)
#define TCPC_V10_REG_POWER_STATUS_VBUS_PRES		(1<<2)
#define TCPC_V10_REG_POWER_STATUS_VCONN_PRES		(1<<1)
#define TCPC_V10_REG_POWER_STATUS_SINK_VBUS		(1<<0)

/*
 * TCPC_V10_REG_FAULT_STATUS_MASK	(0x15)
 * TCPC_V10_REG_FAULT_STATUS			(0x1F)
 */

#define TCPC_V10_REG_FAULT_STATUS_VCONN_OV		(1<<7)
#define TCPC_V10_REG_FAULT_STATUS_FORCE_OFF_VBUS	(1<<6)
#define TCPC_V10_REG_FAULT_STATUS_AUTO_DISC_FAIL	(1<<5)
#define TCPC_V10_REG_FAULT_STATUS_FORCE_DISC_FAIL	(1<<4)
#define TCPC_V10_REG_FAULT_STATUS_VBUS_OC		(1<<3)
#define TCPC_V10_REG_FAULT_STATUS_VBUS_OV		(1<<2)
#define TCPC_V10_REG_FAULT_STATUS_VCONN_OC		(1<<1)
#define TCPC_V10_REG_FAULT_STATUS_I2C_ERROR		(1<<0)

/*
 * TCPC_V10_REG_ROLE_CTRL			(0x1A)
 */

#define TCPC_V10_REG_ROLE_CTRL_DRP			(1<<6)

#define TCPC_V10_REG_ROLE_CTRL_RES_SET(drp, rp, cc1, cc2) \
		((drp) << 6 | (rp) << 4 | (cc2) << 2 | (cc1))

#define CC_RD	0x02
#define CC_RP	0x01
#define CC_OPEN	0x03
#define CC_RA	0x00

/*
 * TCPC_V10_REG_TCPC_CTRL			(0x19)
 */

#define TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE	(1<<1)
#define TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT	(1<<0)

/*
 * TCPC_V10_REG_FAULT_CTRL		(0x1B)
 */

#define TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV	(1<<7)
#define TCPC_V10_REG_FAULT_CTRL_DIS_SNK_VBUS_OC	(1<<2)
#define TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OC	(1<<0)

/*
 * TCPC_V10_REG_POWER_CTRL		(0x1C)
 */

#define TCPC_V10_REG_POWER_CTRL_VCONN		(1<<0)

/*
 * TCPC_V10_REG_CC_STATUS			(0x1D)
 */

#define TCPC_V10_REG_CC_STATUS_DRP_TOGGLING		(1<<5)
#define TCPC_V10_REG_CC_STATUS_DRP_RESULT(reg)	(((reg) & 0x10) >> 4)
#define TCPC_V10_REG_CC_STATUS_CC2(reg)  (((reg) & 0xc) >> 2)
#define TCPC_V10_REG_CC_STATUS_CC1(reg)  ((reg) & 0x3)

/*
 * TCPC_V10_REG_COMMAND			(0x23)
 */

enum tcpm_v10_command {
	TCPM_CMD_WAKE_I2C = 0x11,
	TCPM_CMD_DISABLE_VBUS_DETECT = 0x22,
	TCPM_CMD_ENABLE_VBUS_DETECT = 0x33,
	TCPM_CMD_DISABLE_SINK_VBUS = 0x44,
	TCPM_CMD_ENABLE_SINK_VBUS = 0x55,
	TCPM_CMD_DISABLE_SOURCE_VBUS = 0x66,
	TCPM_CMD_ENABLE_SOURCE_VBUS = 0x77,
	TCPM_CMD_SOURCE_VBUS_HV = 0x88,
	TCPM_CMD_LOOK_CONNECTION = 0x99,
	TCPM_CMD_RX_ONE_MODE = 0xAA,
	TCPM_CMD_I2C_IDLE = 0xFF,
};

/*
 * TCPC_V10_REG_MSG_HDR_INFO		(0x2e)
 * According to PD30_Rev11 ECR,
 * The sender of a GoodCRC Message should set :
 * the Specification Revsiion field to 10b.
 */

#define TCPC_V10_REG_MSG_HDR_INFO_SET(drole, prole) \
		((drole) << 3 | ((PD_REV20) << 1) | (prole))
#define TCPC_V10_REG_MSG_HDR_INFO_DROLE(reg) (((reg) & 0x8) >> 3)
#define TCPC_V10_REG_MSG_HDR_INFO_PROLE(reg) ((reg) & 0x1)


/*
 * TCPC_V10_REG_TRANSMIT				(0x50)
 */

#ifdef CONFIG_USB_PD_REV30
#define TCPC_V10_REG_TRANSMIT_SET(retry, type) \
		((retry) << 4 | (type))
#else
#define TCPC_V10_REG_TRANSMIT_SET(retry, type) \
		(PD_RETRY_COUNT << 4 | (type))
#endif /* CONFIG_USB_PD_REV30 */

#endif /* STD_TCPCI_V10_H_ */
