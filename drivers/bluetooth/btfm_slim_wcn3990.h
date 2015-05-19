/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#ifndef BTFM_SLIM_WCN3990_H
#define BTFM_SLIM_WCN3990_H
#ifdef CONFIG_BTFM_SLIM_WCN3990
#include <btfm_slim.h>

/* Registers Address */
#define CHRK_SB_COMP_TEST			0x00000000
#define CHRK_SB_SLAVE_HW_REV_MSB		0x00000001
#define CHRK_SB_SLAVE_HW_REV_LSB		0x00000002
#define CHRK_SB_DEBUG_FEATURES			0x00000005
#define CHRK_SB_INTF_INT_EN			0x00000010
#define CHRK_SB_INTF_INT_STATUS			0x00000011
#define CHRK_SB_INTF_INT_CLR			0x00000012
#define CHRK_SB_FRM_CFG				0x00000013
#define CHRK_SB_FRM_STATUS			0x00000014
#define CHRK_SB_FRM_INT_EN			0x00000015
#define CHRK_SB_FRM_INT_STATUS			0x00000016
#define CHRK_SB_FRM_INT_CLR			0x00000017
#define CHRK_SB_FRM_WAKEUP			0x00000018
#define CHRK_SB_FRM_CLKCTL_DONE			0x00000019
#define CHRK_SB_FRM_IE_STATUS			0x0000001A
#define CHRK_SB_FRM_VE_STATUS			0x0000001B
#define CHRK_SB_PGD_TX_CFG_STATUS		0x00000020
#define CHRK_SB_PGD_RX_CFG_STATUS		0x00000021
#define CHRK_SB_PGD_DEV_INT_EN			0x00000022
#define CHRK_SB_PGD_DEV_INT_STATUS		0x00000023
#define CHRK_SB_PGD_DEV_INT_CLR			0x00000024
#define CHRK_SB_PGD_PORT_INT_EN_RX_0		0x00000030
#define CHRK_SB_PGD_PORT_INT_EN_RX_1		0x00000031
#define CHRK_SB_PGD_PORT_INT_EN_TX_0		0x00000032
#define CHRK_SB_PGD_PORT_INT_EN_TX_1		0x00000033
#define CHRK_SB_PGD_PORT_INT_STATUS_RX_0	0x00000034
#define CHRK_SB_PGD_PORT_INT_STATUS_RX_1	0x00000035
#define CHRK_SB_PGD_PORT_INT_STATUS_TX_0	0x00000036
#define CHRK_SB_PGD_PORT_INT_STATUS_TX_1	0x00000037
#define CHRK_SB_PGD_PORT_INT_CLR_RX_0		0x00000038
#define CHRK_SB_PGD_PORT_INT_CLR_RX_1		0x00000039
#define CHRK_SB_PGD_PORT_INT_CLR_TX_0		0x0000003A
#define CHRK_SB_PGD_PORT_INT_CLR_TX_1		0x0000003B
#define CHRK_SB_PGD_PORT_RX_CFGN(n)		(0x00000040 + n)
#define CHRK_SB_PGD_PORT_TX_CFGN(n)		(0x00000050 + n)
#define CHRK_SB_PGD_PORT_INT_RX_SOURCEN(n)	(0x00000060 + n)
#define CHRK_SB_PGD_PORT_INT_TX_SOURCEN(n)	(0x00000070 + n)
#define CHRK_SB_PGD_PORT_RX_STATUSN(n)		(0x00000080 + n)
#define CHRK_SB_PGD_PORT_TX_STATUSN(n)		(0x00000090 + n)
#define CHRK_SB_PGD_TX_PORTn_MULTI_CHNL_0(n)	(0x00000100 + 0x4*n)
#define CHRK_SB_PGD_TX_PORTn_MULTI_CHNL_1(n)	(0x00000101 + 0x4*n)
#define CHRK_SB_PGD_RX_PORTn_MULTI_CHNL_0(n)	(0x00000180 + 0x4*n)
#define CHRK_SB_PGD_RX_PORTn_MULTI_CHNL_1(n)	(0x00000181 + 0x4*n)
#define CHRK_SB_PGD_PORT_TX_OR_UR_CFGN(n)	(0x000001F0 + n)

/* Register Bit Setting */
#define CHRK_ENABLE_OVERRUN_AUTO_RECOVERY	(0x1 << 1)
#define CHRK_ENABLE_UNDERRUN_AUTO_RECOVERY	(0x1 << 0)
#define CHRK_SB_PGD_PORT_ENABLE			(0x1 << 0)
#define CHRK_SB_PGD_PORT_DISABLE		(0x0 << 0)
#define CHRK_SB_PGD_PORT_WM_L1			(0x1 << 1)
#define CHRK_SB_PGD_PORT_WM_L2			(0x2 << 1)
#define CHRK_SB_PGD_PORT_WM_L3			(0x3 << 1)
#define CHRK_SB_PGD_PORT_WM_LB			(0xB << 1)

#define CHRK_SB_PGD_PORT_RX_NUM			16
#define CHRK_SB_PGD_PORT_TX_NUM			16

/* PGD Port Map */
#define CHRK_SB_PGD_PORT_TX_SCO			0
#define CHRK_SB_PGD_PORT_TX1_FM			1
#define CHRK_SB_PGD_PORT_TX2_FM			2
#define CHRK_SB_PGD_PORT_RX_SCO			16
#define CHRK_SB_PGD_PORT_RX_A2P			17


/* Function Prototype */

/*
 * btfm_slim_chrk_hw_init: Initialize wcn3990 specific slimbus slave device
 * @btfmslim: slimbus slave device data pointer.
 * Returns:
 * 0: Success
 * else: Fail
 */
int btfm_slim_chrk_hw_init(struct btfmslim *btfmslim);

/*
 * btfm_slim_chrk_enable_rxport: Enable wcn3990 Rx port by given port number
 * @btfmslim: slimbus slave device data pointer.
 * @portNum: slimbus slave port number to enable
 * @rxport: rxport or txport
 * @enable: enable port or disable port
 * Returns:
 * 0: Success
 * else: Fail
 */
int btfm_slim_chrk_enable_port(struct btfmslim *btfmslim, uint8_t portNum,
	uint8_t rxport, uint8_t enable);

/* Specific defines for wcn3990 slimbus device */
#define WCN3990_SLIM_REG_OFFSET		0x0800

#ifdef SLIM_SLAVE_REG_OFFSET
#undef SLIM_SLAVE_REG_OFFSET
#define SLIM_SLAVE_REG_OFFSET		WCN3990_SLIM_REG_OFFSET
#endif

/* Assign vendor specific function */
extern struct btfmslim_ch wcn3990_txport[];
extern struct btfmslim_ch wcn3990_rxport[];

#ifdef SLIM_SLAVE_RXPORT
#undef SLIM_SLAVE_RXPORT
#define SLIM_SLAVE_RXPORT (&wcn3990_rxport[0])
#endif

#ifdef SLIM_SLAVE_TXPORT
#undef SLIM_SLAVE_TXPORT
#define SLIM_SLAVE_TXPORT (&wcn3990_txport[0])
#endif

#ifdef SLIM_SLAVE_INIT
#undef SLIM_SLAVE_INIT
#define SLIM_SLAVE_INIT btfm_slim_chrk_hw_init
#endif

#ifdef SLIM_SLAVE_PORT_EN
#undef SLIM_SLAVE_PORT_EN
#define SLIM_SLAVE_PORT_EN btfm_slim_chrk_enable_port
#endif
#endif /* CONFIG_BTFM_WCN3990 */
#endif /* BTFM_SLIM_WCN3990_H */
