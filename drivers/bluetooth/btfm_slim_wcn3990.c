/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017 XiaoMi, Inc.
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
#include <linux/slimbus/slimbus.h>
#include <btfm_slim.h>
#include <btfm_slim_wcn3990.h>

/* WCN3990 Port assignment */
struct btfmslim_ch wcn3990_rxport[] = {
	{.id = BTFM_BT_SCO_A2DP_SLIM_RX, .name = "SCO_A2P_Rx",
	.port = CHRK_SB_PGD_PORT_RX_SCO},
	{.id = BTFM_BT_SPLIT_A2DP_SLIM_RX, .name = "A2P_Rx",
	.port = CHRK_SB_PGD_PORT_RX_A2P},
	{.id = BTFM_SLIM_NUM_CODEC_DAIS, .name = "",
	.port = BTFM_SLIM_PGD_PORT_LAST},
};

struct btfmslim_ch wcn3990_txport[] = {
	{.id = BTFM_FM_SLIM_TX, .name = "FM_Tx1",
	.port = CHRK_SB_PGD_PORT_TX1_FM},
	{.id = BTFM_FM_SLIM_TX, .name = "FM_Tx2",
	.port = CHRK_SB_PGD_PORT_TX2_FM},
	{.id = BTFM_BT_SCO_SLIM_TX, .name = "SCO_Tx",
	.port = CHRK_SB_PGD_PORT_TX_SCO},
	{.id = BTFM_SLIM_NUM_CODEC_DAIS, .name = "",
	.port = BTFM_SLIM_PGD_PORT_LAST},
};

/* Function description */
int btfm_slim_chrk_hw_init(struct btfmslim *btfmslim)
{
	int ret = 0;
	uint8_t reg_val;
	uint16_t reg;

	BTFMSLIM_DBG("");

	if (!btfmslim)
		return -EINVAL;

	/* Get SB_SLAVE_HW_REV_MSB value*/
	reg = CHRK_SB_SLAVE_HW_REV_MSB;
	ret = btfm_slim_read(btfmslim, reg,  1, &reg_val, IFD);
	if (ret) {
		BTFMSLIM_ERR("failed to read (%d) reg 0x%x", ret, reg);
		goto error;
	}
	BTFMSLIM_DBG("Major Rev: 0x%x, Minor Rev: 0x%x",
		(reg_val & 0xF0) >> 4, (reg_val & 0x0F));

	/* Get SB_SLAVE_HW_REV_LSB value*/
	reg = CHRK_SB_SLAVE_HW_REV_LSB;
	ret = btfm_slim_read(btfmslim, reg,  1, &reg_val, IFD);
	if (ret) {
		BTFMSLIM_ERR("failed to read (%d) reg 0x%x", ret, reg);
		goto error;
	}
	BTFMSLIM_DBG("Step Rev: 0x%x", reg_val);

error:
	return ret;
}

static inline int is_fm_port(uint8_t port_num)
{
	if (port_num == CHRK_SB_PGD_PORT_TX1_FM ||
		port_num == CHRK_SB_PGD_PORT_TX2_FM)
		return 1;
	else
		return 0;
}

int btfm_slim_chrk_enable_port(struct btfmslim *btfmslim, uint8_t port_num,
	uint8_t rxport, uint8_t enable)
{
	int ret = 0;
	uint8_t reg_val = 0;
	uint8_t port_bit = 0;
	uint16_t reg;

	BTFMSLIM_DBG("port(%d) enable(%d)", port_num, enable);
	if (rxport) {
		if (enable) {
			/* For SCO Rx, A2DP Rx */
			reg_val = 0x1;
			port_bit = port_num - 0x10;
			reg = CHRK_SB_PGD_RX_PORTn_MULTI_CHNL_0(port_bit);
			BTFMSLIM_DBG("writing reg_val (%d) to reg(%x)",
					reg_val, reg);
			ret = btfm_slim_write(btfmslim, reg, 1, &reg_val, IFD);
			if (ret) {
				BTFMSLIM_ERR("failed to write (%d) reg 0x%x",
						ret, reg);
				goto error;
			}
		}
		/* Port enable */
		reg = CHRK_SB_PGD_PORT_RX_CFGN(port_num - 0x10);
		goto enable_disable_rxport;
	}
	if (!enable)
		goto enable_disable_txport;

	/* txport */
	/* Multiple Channel Setting */
	if (is_fm_port(port_num)) {
		reg_val = (0x1 << CHRK_SB_PGD_PORT_TX1_FM) |
				(0x1 << CHRK_SB_PGD_PORT_TX2_FM);
		reg = CHRK_SB_PGD_TX_PORTn_MULTI_CHNL_0(port_num);
		ret = btfm_slim_write(btfmslim, reg, 1, &reg_val, IFD);
		if (ret) {
			BTFMSLIM_ERR("failed to write (%d) reg 0x%x", ret, reg);
			goto error;
		}
	}

	/* Enable Tx port hw auto recovery for underrun or overrun error */
	reg_val = (CHRK_ENABLE_OVERRUN_AUTO_RECOVERY |
				CHRK_ENABLE_UNDERRUN_AUTO_RECOVERY);
	reg = CHRK_SB_PGD_PORT_TX_OR_UR_CFGN(port_num);
	ret = btfm_slim_write(btfmslim, reg, 1, &reg_val, IFD);
	if (ret) {
		BTFMSLIM_ERR("failed to write (%d) reg 0x%x", ret, reg);
		goto error;
	}

enable_disable_txport:
	/* Port enable */
	reg = CHRK_SB_PGD_PORT_TX_CFGN(port_num);

enable_disable_rxport:
	if (enable) {
		if (is_fm_port(port_num))
			reg_val = CHRK_SB_PGD_PORT_ENABLE |
					CHRK_SB_PGD_PORT_WM_L3;
		else
			reg_val = CHRK_SB_PGD_PORT_ENABLE |
					CHRK_SB_PGD_PORT_WM_LB;
	} else
		reg_val = CHRK_SB_PGD_PORT_DISABLE;

	ret = btfm_slim_write(btfmslim, reg, 1, &reg_val, IFD);
	if (ret)
		BTFMSLIM_ERR("failed to write (%d) reg 0x%x", ret, reg);

error:
	return ret;
}
