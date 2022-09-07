// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slimbus.h>
#include "btfm_slim.h"
#include "btfm_slim_slave.h"

/* SLAVE (WCN3990/QCA6390) Port assignment */
struct btfmslim_ch slave_rxport[] = {
	{.id = BTFM_BT_SCO_A2DP_SLIM_RX, .name = "SCO_A2P_Rx",
	.port = SLAVE_SB_PGD_PORT_RX_SCO},
	{.id = BTFM_BT_SPLIT_A2DP_SLIM_RX, .name = "A2P_Rx",
	.port = SLAVE_SB_PGD_PORT_RX_A2P},
	{.id = BTFM_SLIM_NUM_CODEC_DAIS, .name = "",
	.port = BTFM_SLIM_PGD_PORT_LAST},
};

struct btfmslim_ch slave_txport[] = {
	{.id = BTFM_BT_SCO_SLIM_TX, .name = "SCO_Tx",
	.port = SLAVE_SB_PGD_PORT_TX_SCO},
	{.id = BTFM_FM_SLIM_TX, .name = "FM_Tx1",
	.port = SLAVE_SB_PGD_PORT_TX1_FM},
	{.id = BTFM_FM_SLIM_TX, .name = "FM_Tx2",
	.port = SLAVE_SB_PGD_PORT_TX2_FM},
	{.id = BTFM_BT_SPLIT_A2DP_SLIM_TX, .name = "A2DP_Tx",
	.port = SLAVE_SB_PGD_PORT_TX_A2DP},
	{.id = BTFM_SLIM_NUM_CODEC_DAIS, .name = "",
	.port = BTFM_SLIM_PGD_PORT_LAST},
};

/* Function description */
int btfm_slim_slave_hw_init(struct btfmslim *btfmslim)
{
	int ret = 0;
	uint32_t reg;

	BTFMSLIM_DBG("");

	if (!btfmslim)
		return -EINVAL;

	/* Get SB_SLAVE_HW_REV_MSB value*/
	reg = SLAVE_SB_SLAVE_HW_REV_MSB;
	ret = btfm_slim_read(btfmslim, reg, IFD);
	if (ret < 0)
		BTFMSLIM_ERR("failed to read (%d) reg 0x%x", ret, reg);

	BTFMSLIM_DBG("Major Rev: 0x%x, Minor Rev: 0x%x",
		(ret & 0xF0) >> 4, (ret & 0x0F));

	/* Get SB_SLAVE_HW_REV_LSB value*/
	reg = SLAVE_SB_SLAVE_HW_REV_LSB;
	ret = btfm_slim_read(btfmslim, reg, IFD);
	if (ret < 0)
		BTFMSLIM_ERR("failed to read (%d) reg 0x%x", ret, reg);
	else {
		BTFMSLIM_INFO("read (%d) reg 0x%x", ret, reg);
		ret = 0;
	}
	return ret;
}

static inline int is_fm_port(struct btfmslim *btfmslim)
{
	BTFMSLIM_INFO("dai id is %d", btfmslim->dai_id);
	if (btfmslim->dai_id == BTFM_FM_SLIM_TX)
		return 1;
	else
		return 0;
}

int btfm_slim_slave_enable_port(struct btfmslim *btfmslim, uint8_t port_num,
	uint8_t rxport, uint8_t enable)
{
	int ret = 0;
	uint8_t reg_val = 0, en;
	uint8_t rxport_num = 0;
	uint16_t reg;

	BTFMSLIM_DBG("port(%d) enable(%d)", port_num, enable);
	if (rxport) {
		BTFMSLIM_DBG("sample rate is %d", btfmslim->sample_rate);
		if (enable &&
			btfmslim->sample_rate != 44100 &&
			btfmslim->sample_rate != 88200) {
			BTFMSLIM_DBG("setting multichannel bit");
			/* For SCO Rx, A2DP Rx other than 44.1 and 88.2Khz */
			if (port_num < 24) {
				rxport_num = port_num - 16;
				reg_val = 0x01 << rxport_num;
				reg = SLAVE_SB_PGD_RX_PORTn_MULTI_CHNL_0(
					rxport_num);
			} else {
				rxport_num = port_num - 24;
				reg_val = 0x01 << rxport_num;
				reg = SLAVE_SB_PGD_RX_PORTn_MULTI_CHNL_1(
					rxport_num);
			}

			BTFMSLIM_DBG("writing reg_val (%d) to reg(%x)",
				reg_val, reg);
			ret = btfm_slim_write(btfmslim, reg, reg_val, IFD);
			if (ret < 0) {
				BTFMSLIM_ERR("failed to write (%d) reg 0x%x",
					ret, reg);
				goto error;
			}
		}
		/* Port enable */
		reg = SLAVE_SB_PGD_PORT_RX_CFGN(port_num - 0x10);
		goto enable_disable_rxport;
	}
	if (!enable)
		goto enable_disable_txport;

	/* txport */
	/* Multiple Channel Setting */
	if (is_fm_port(btfmslim)) {
		if (port_num == CHRKVER3_SB_PGD_PORT_TX1_FM)
			reg_val = (0x1 << CHRKVER3_SB_PGD_PORT_TX1_FM);
		else if (port_num == CHRKVER3_SB_PGD_PORT_TX2_FM)
			reg_val = (0x1 << CHRKVER3_SB_PGD_PORT_TX2_FM);
		else
			reg_val = (0x1 << SLAVE_SB_PGD_PORT_TX1_FM) |
					(0x1 << SLAVE_SB_PGD_PORT_TX2_FM);
		reg = SLAVE_SB_PGD_TX_PORTn_MULTI_CHNL_0(port_num);
		BTFMSLIM_INFO("writing reg_val (%d) to reg(%x)", reg_val, reg);
		ret = btfm_slim_write(btfmslim, reg, reg_val, IFD);
		if (ret < 0) {
			BTFMSLIM_ERR("failed to write (%d) reg 0x%x", ret, reg);
			goto error;
		}
	} else if (port_num == SLAVE_SB_PGD_PORT_TX_SCO) {
		/* SCO Tx */
		reg_val = 0x1 << SLAVE_SB_PGD_PORT_TX_SCO;
		reg = SLAVE_SB_PGD_TX_PORTn_MULTI_CHNL_0(port_num);
		BTFMSLIM_DBG("writing reg_val (%d) to reg(%x)",
				reg_val, reg);
		ret = btfm_slim_write(btfmslim, reg, reg_val, IFD);
		if (ret < 0) {
			BTFMSLIM_ERR("failed to write (%d) reg 0x%x",
					ret, reg);
			goto error;
		}
	} else if (port_num == SLAVE_SB_PGD_PORT_TX_A2DP) {
		/* A2DP Tx */
		reg_val = 0x1 << SLAVE_SB_PGD_PORT_TX_A2DP;
		reg = SLAVE_SB_PGD_TX_PORTn_MULTI_CHNL_0(port_num);
		BTFMSLIM_DBG("writing reg_val (%d) to reg(%x)",
				reg_val, reg);
		ret = btfm_slim_write(btfmslim, reg, reg_val, IFD);
		if (ret < 0) {
			BTFMSLIM_ERR("failed to write (%d) reg 0x%x",
					ret, reg);
			goto error;
		}
	}

	/* Enable Tx port hw auto recovery for underrun or overrun error */
	reg_val = (SLAVE_ENABLE_OVERRUN_AUTO_RECOVERY |
				SLAVE_ENABLE_UNDERRUN_AUTO_RECOVERY);
	reg = SLAVE_SB_PGD_PORT_TX_OR_UR_CFGN(port_num);
	ret = btfm_slim_write(btfmslim, reg, reg_val, IFD);
	if (ret < 0) {
		BTFMSLIM_ERR("failed to write (%d) reg 0x%x", ret, reg);
		goto error;
	}

enable_disable_txport:
	/* Port enable */
	reg = SLAVE_SB_PGD_PORT_TX_CFGN(port_num);

enable_disable_rxport:
	if (enable)
		en = SLAVE_SB_PGD_PORT_ENABLE;
	else
		en = SLAVE_SB_PGD_PORT_DISABLE;

	if (is_fm_port(btfmslim))
		reg_val = en | SLAVE_SB_PGD_PORT_WM_L8;
	else if (port_num == SLAVE_SB_PGD_PORT_TX_SCO)
		reg_val = enable ? en | SLAVE_SB_PGD_PORT_WM_L1 : en;
	else
		reg_val = enable ? en | SLAVE_SB_PGD_PORT_WM_LB : en;

	if (enable && port_num == SLAVE_SB_PGD_PORT_TX_SCO)
		BTFMSLIM_INFO("programming SCO Tx with reg_val %d to reg 0x%x",
				reg_val, reg);
	else if (enable && port_num == SLAVE_SB_PGD_PORT_TX_A2DP)
		BTFMSLIM_INFO("programming A2DP Tx with reg_val %d to reg 0x%x",
				reg_val, reg);

	ret = btfm_slim_write(btfmslim, reg, reg_val, IFD);
	if (ret < 0)
		BTFMSLIM_ERR("failed to write (%d) reg 0x%x", ret, reg);

error:
	return ret;
}
