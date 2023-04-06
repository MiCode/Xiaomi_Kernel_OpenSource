/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef BTFM_SLIM_SLAVE_H
#define BTFM_SLIM_SLAVE_H
#include "btfm_slim.h"

/* Registers Address */
#define SLAVE_SB_COMP_TEST			0x00000000
#define SLAVE_SB_SLAVE_HW_REV_MSB		0x00000001
#define SLAVE_SB_SLAVE_HW_REV_LSB		0x00000002
#define SLAVE_SB_DEBUG_FEATURES			0x00000005
#define SLAVE_SB_INTF_INT_EN			0x00000010
#define SLAVE_SB_INTF_INT_STATUS			0x00000011
#define SLAVE_SB_INTF_INT_CLR			0x00000012
#define SLAVE_SB_FRM_CFG				0x00000013
#define SLAVE_SB_FRM_STATUS			0x00000014
#define SLAVE_SB_FRM_INT_EN			0x00000015
#define SLAVE_SB_FRM_INT_STATUS			0x00000016
#define SLAVE_SB_FRM_INT_CLR			0x00000017
#define SLAVE_SB_FRM_WAKEUP			0x00000018
#define SLAVE_SB_FRM_CLKCTL_DONE			0x00000019
#define SLAVE_SB_FRM_IE_STATUS			0x0000001A
#define SLAVE_SB_FRM_VE_STATUS			0x0000001B
#define SLAVE_SB_PGD_TX_CFG_STATUS		0x00000020
#define SLAVE_SB_PGD_RX_CFG_STATUS		0x00000021
#define SLAVE_SB_PGD_DEV_INT_EN			0x00000022
#define SLAVE_SB_PGD_DEV_INT_STATUS		0x00000023
#define SLAVE_SB_PGD_DEV_INT_CLR			0x00000024
#define SLAVE_SB_PGD_PORT_INT_EN_RX_0		0x00000030
#define SLAVE_SB_PGD_PORT_INT_EN_RX_1		0x00000031
#define SLAVE_SB_PGD_PORT_INT_EN_TX_0		0x00000032
#define SLAVE_SB_PGD_PORT_INT_EN_TX_1		0x00000033
#define SLAVE_SB_PGD_PORT_INT_STATUS_RX_0	0x00000034
#define SLAVE_SB_PGD_PORT_INT_STATUS_RX_1	0x00000035
#define SLAVE_SB_PGD_PORT_INT_STATUS_TX_0	0x00000036
#define SLAVE_SB_PGD_PORT_INT_STATUS_TX_1	0x00000037
#define SLAVE_SB_PGD_PORT_INT_CLR_RX_0		0x00000038
#define SLAVE_SB_PGD_PORT_INT_CLR_RX_1		0x00000039
#define SLAVE_SB_PGD_PORT_INT_CLR_TX_0		0x0000003A
#define SLAVE_SB_PGD_PORT_INT_CLR_TX_1		0x0000003B
#define SLAVE_SB_PGD_PORT_RX_CFGN(n)		(0x00000040 + n)
#define SLAVE_SB_PGD_PORT_TX_CFGN(n)		(0x00000050 + n)
#define SLAVE_SB_PGD_PORT_INT_RX_SOURCEN(n)	(0x00000060 + n)
#define SLAVE_SB_PGD_PORT_INT_TX_SOURCEN(n)	(0x00000070 + n)
#define SLAVE_SB_PGD_PORT_RX_STATUSN(n)		(0x00000080 + n)
#define SLAVE_SB_PGD_PORT_TX_STATUSN(n)		(0x00000090 + n)
#define SLAVE_SB_PGD_TX_PORTn_MULTI_CHNL_0(n)	(0x00000100 + 0x4*n)
#define SLAVE_SB_PGD_TX_PORTn_MULTI_CHNL_1(n)	(0x00000101 + 0x4*n)
#define SLAVE_SB_PGD_RX_PORTn_MULTI_CHNL_0(n)	(0x00000180 + 0x4*n)
#define SLAVE_SB_PGD_RX_PORTn_MULTI_CHNL_1(n)	(0x00000181 + 0x4*n)
#define SLAVE_SB_PGD_PORT_TX_OR_UR_CFGN(n)	(0x000001F0 + n)

/* Register Bit Setting */
#define SLAVE_ENABLE_OVERRUN_AUTO_RECOVERY	(0x1 << 1)
#define SLAVE_ENABLE_UNDERRUN_AUTO_RECOVERY	(0x1 << 0)
#define SLAVE_SB_PGD_PORT_ENABLE			(0x1 << 0)
#define SLAVE_SB_PGD_PORT_DISABLE		(0x0 << 0)
#define SLAVE_SB_PGD_PORT_WM_L1			(0x1 << 1)
#define SLAVE_SB_PGD_PORT_WM_L2			(0x2 << 1)
#define SLAVE_SB_PGD_PORT_WM_L3			(0x3 << 1)
#define SLAVE_SB_PGD_PORT_WM_L8			(0x8 << 1)
#define SLAVE_SB_PGD_PORT_WM_LB			(0xB << 1)

#define SLAVE_SB_PGD_PORT_RX_NUM			16
#define SLAVE_SB_PGD_PORT_TX_NUM			16

/* PGD Port Map */
#define SLAVE_SB_PGD_PORT_TX_SCO			0
#define SLAVE_SB_PGD_PORT_TX1_FM			1
#define SLAVE_SB_PGD_PORT_TX2_FM			2
#define CHRKVER3_SB_PGD_PORT_TX1_FM			5
#define CHRKVER3_SB_PGD_PORT_TX2_FM			4
#define SLAVE_SB_PGD_PORT_RX_SCO			16
#define SLAVE_SB_PGD_PORT_RX_A2P			17
#define SLAVE_SB_PGD_PORT_TX_A2DP			2

enum {
	QCA_CHEROKEE_SOC_ID_0200  = 0x40010200,
	QCA_CHEROKEE_SOC_ID_0201  = 0x40010201,
	QCA_CHEROKEE_SOC_ID_0210  = 0x40010214,
	QCA_CHEROKEE_SOC_ID_0211  = 0x40010224,
	QCA_CHEROKEE_SOC_ID_0310  = 0x40010310,
	QCA_CHEROKEE_SOC_ID_0320  = 0x40010320,
	QCA_CHEROKEE_SOC_ID_0320_UMC  = 0x40014320,
};

enum {
	QCA_APACHE_SOC_ID_0100  = 0x40020120,
	QCA_APACHE_SOC_ID_0110  = 0x40020130,
	QCA_APACHE_SOC_ID_0120  = 0x40020140,
	QCA_APACHE_SOC_ID_0121  = 0x40020150,
};

enum {
	QCA_COMANCHE_SOC_ID_0101  = 0x40070101,
	QCA_COMANCHE_SOC_ID_0110  = 0x40070110,
	QCA_COMANCHE_SOC_ID_0120  = 0x40070120,
	QCA_COMANCHE_SOC_ID_0130  = 0x40070130,
	QCA_COMANCHE_SOC_ID_4130  = 0x40074130,
	QCA_COMANCHE_SOC_ID_5120  = 0x40075120,
	QCA_COMANCHE_SOC_ID_5130  = 0x40075130,
};

enum {
	QCA_HASTINGS_SOC_ID_0200 = 0x400A0200,
};

enum {
	QCA_HSP_SOC_ID_0100 = 0x400C0100,
	QCA_HSP_SOC_ID_0110 = 0x400C0110,
	QCA_HSP_SOC_ID_0200 = 0x400C0200,
	QCA_HSP_SOC_ID_0210 = 0x400C0210,
	QCA_HSP_SOC_ID_1201 = 0x400C1201,
	QCA_HSP_SOC_ID_1211 = 0x400C1211,
};

enum {
	QCA_MOSELLE_SOC_ID_0100 = 0x40140100,
	QCA_MOSELLE_SOC_ID_0110 = 0x40140110,
	QCA_MOSELLE_SOC_ID_0120 = 0x40140120,
};

enum {
	QCA_HAMILTON_SOC_ID_0100 = 0x40170100,
	QCA_HAMILTON_SOC_ID_0101 = 0x40170101,
	QCA_HAMILTON_SOC_ID_0200 = 0x40170200,
};

/* Function Prototype */

/*
 * btfm_slim_slave_hw_init: Initialize slave specific slimbus slave device
 * @btfmslim: slimbus slave device data pointer.
 * Returns:
 * 0: Success
 * else: Fail
 */
int btfm_slim_slave_hw_init(struct btfmslim *btfmslim);

/*
 * btfm_slim_slave_enable_rxport: Enable slave Rx port by given port number
 * @btfmslim: slimbus slave device data pointer.
 * @portNum: slimbus slave port number to enable
 * @rxport: rxport or txport
 * @enable: enable port or disable port
 * Returns:
 * 0: Success
 * else: Fail
 */
int btfm_slim_slave_enable_port(struct btfmslim *btfmslim, uint8_t portNum,
	uint8_t rxport, uint8_t enable);

/* Specific defines for slave slimbus device */
#define SLAVE_SLIM_REG_OFFSET		0x0800

#ifdef SLIM_SLAVE_REG_OFFSET
#undef SLIM_SLAVE_REG_OFFSET
#define SLIM_SLAVE_REG_OFFSET		SLAVE_SLIM_REG_OFFSET
#endif

/* Assign vendor specific function */
extern struct btfmslim_ch slave_txport[];
extern struct btfmslim_ch slave_rxport[];

#ifdef SLIM_SLAVE_RXPORT
#undef SLIM_SLAVE_RXPORT
#define SLIM_SLAVE_RXPORT (&slave_rxport[0])
#endif

#ifdef SLIM_SLAVE_TXPORT
#undef SLIM_SLAVE_TXPORT
#define SLIM_SLAVE_TXPORT (&slave_txport[0])
#endif

#ifdef SLIM_SLAVE_INIT
#undef SLIM_SLAVE_INIT
#define SLIM_SLAVE_INIT btfm_slim_slave_hw_init
#endif

#ifdef SLIM_SLAVE_PORT_EN
#undef SLIM_SLAVE_PORT_EN
#define SLIM_SLAVE_PORT_EN btfm_slim_slave_enable_port
#endif
#endif
