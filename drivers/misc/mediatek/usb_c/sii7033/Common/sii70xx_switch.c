/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#include <Wrap.h>
#include "si_time.h"
#include "si_usbpd_regs.h"
#include "si_usbpd_main.h"


void si_enable_switch_control(struct sii70xx_drv_context *drv_context,
			      enum xceiv_config rx_tx, bool off_on)
{
	if (off_on) {
#if defined(RX_7024_9679)
		if (rx_tx == PD_RX)
			sii_platform_wr_reg8(REG_ADDR__PDCTR12, 0x9);
#endif
		sii_platform_wr_reg8(REG_ADDR__ANA_SWCH_CTRL0,
				     BIT_MSK__ANA_SWCH_CTRL0__RI_ANA_SWCH_EN);
		/* enable analog switch   */

		sii_platform_wr_reg8(REG_ADDR__ANA_SWCH_CTRL1, 0x81);
		/* enable analog switch oscillator             */

		sii_platform_wr_reg8(REG_ADDR__ANA_SWCH_CTRL2, 0x1E);
		/* Analog switch test bus             */

		sii_platform_wr_reg8(REG_ADDR__ANA_CCPD_CTRL0, 0x12);
		/* pin select for LDO/BGR on --forceable turn on   */

		pr_info("SII70XX SWITCH POSITION = ENABLE\n");
	} else {
		sii_platform_wr_reg8(0x88, 0x8);
		sii_platform_wr_reg8(REG_ADDR__ANA_SWCH_CTRL0, 0x00);
		/* enable analog switch   */
		sii_platform_wr_reg8(REG_ADDR__ANA_SWCH_CTRL1, 0x00);
		/*enable analog switch oscillator             */
		sii_platform_wr_reg8(REG_ADDR__ANA_SWCH_CTRL2, 0x00);
		/*Analog switch test bus             */
		sii_platform_wr_reg8(REG_ADDR__ANA_CCPD_CTRL0, 0x00);
		/*pin select for LDO/BGR on --forceable turn on   */
		pr_info("SII70XX SWITCH POSITION = DISABLE\n");
	}
}
