/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ADV7481_H__
#define __ADV7481_H__

/**
 * adv7481_platform_data
 * structure to pass board specific information to the ADV7481 driver
 * @rstb_gpio: put active low to hold chip in reset state
 * @pwdnb_gpio: put active low to allow chip to power-down and disable I2C
 * @irq_gpio : active low to trigger IRQ1
 * @irq2_gpio : active low to trigger IRQ2
 * @irq3_gpio : active low to trigger IRQ3
 * @i2c_csi_txa: CSI TXA I2C Map Address
 * @i2c_csi_txb; CSI TXB I2C Map Address
 * @i2c_hdmi:   hdmi I2C Map Address
 * @i2c_cp:     cp I2C Map Address
 * @i2c_sdp:    sdp I2C Map Address
 */
struct adv7481_platform_data {
		int rstb_gpio;
		int pwdnb_gpio;
		int irq1_gpio;
		int irq2_gpio;
		int irq3_gpio;
		int i2c_csi_txa;
		int i2c_csi_txb;
		int i2c_hdmi;
		int i2c_cp;
		int i2c_sdp;
		int i2c_rep;

};


/*
 * Mode of operation.
 * This is used as the input argument of the s_routing video op.
 */
enum adv7481_input {
	ADV7481_IP_CVBS_1 = 0,
	ADV7481_IP_CVBS_2,
	ADV7481_IP_CVBS_3,
	ADV7481_IP_CVBS_4,
	ADV7481_IP_CVBS_5,
	ADV7481_IP_CVBS_6,
	ADV7481_IP_CVBS_7,
	ADV7481_IP_CVBS_8,
	ADV7481_IP_HDMI,
	ADV7481_IP_TTL,
	ADV7481_IP_CVBS_1_HDMI_SIM,
	ADV7481_IP_CVBS_2_HDMI_SIM,
	ADV7481_IP_CVBS_3_HDMI_SIM,
	ADV7481_IP_CVBS_4_HDMI_SIM,
	ADV7481_IP_CVBS_5_HDMI_SIM,
	ADV7481_IP_CVBS_6_HDMI_SIM,
	ADV7481_IP_CVBS_7_HDMI_SIM,
	ADV7481_IP_CVBS_8_HDMI_SIM,
	ADV7481_IP_NONE,
	ADV7481_IP_INVALID,
};
/*
 * CSI Tx
 * This is used as the output argument of the s_routing video op.
 */
enum adv7481_output {
	ADV7481_OP_CSIA,
	ADV7481_OP_CSIB,
	ADV7481_OP_TTL
};
enum adv7481_csi_lane_en {
	ADV7481_CSI_1LANE_EN = 0x1,
	ADV7481_CSI_4LANE_EN = 0x2,
};
enum adv7481_mipi_lane {
	ADV7481_MIPI_1LANE = 0x1,
	ADV7481_MIPI_2LANE = 0x2,
	ADV7481_MIPI_4LANE = 0x4,
};
enum adv7481_csi_params {
	ADV7481_AUTO_PARAMS = 0x1,
};
#endif
