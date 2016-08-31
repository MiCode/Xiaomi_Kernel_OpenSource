
/*
 *tlv320aic325x-registers: Register bits for AIC3XXX codecs
 *
 *
 * Author:      Mukund Navada <navada@ti.com>
 *              Mehar Bajwa <mehar.bajwa@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __MFD_AIC3XXX_REGISTERS_H__
#define __MFD_AIC3XXX_REGISTERS_H__
#define AIC3XXX_MAKE_REG(book, page, offset)	(unsigned int)(book << 8 | \
							(page << 8) | \
							offset)

#define AIC3XXX_RESET		AIC3XXX_MAKE_REG(0, 0, 1)
#define AIC3XXX_REV_PG_ID		AIC3XXX_MAKE_REG(0, 0, 2)
#define AIC3XXX_REV_M		(0b01110000)
#define AIC3XXX_REV_S		4
#define AIC3XXX_PG_M			(0b00000111)
#define AIC3XXX_PG_S		0

#define AIC3XXX_INT_STICKY_FLAG1		AIC3XXX_MAKE_REG(0, 0, 42)
#define AIC3XXX_LEFT_DAC_OVERFLOW_INT	0x80
#define AIC3XXX_RIGHT_DAC_OVERFLOW_INT	0x40
#define AIC3XXX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT	0x20
#define AIC3XXX_LEFT_ADC_OVERFLOW_INT	0x08
#define AIC3XXX_RIGHT_ADC_OVERFLOW_INT	0x04
#define AIC3XXX_MINIDSP_A_BARREL_SHIFT_OVERFLOW_INT	0x02
#define AIC3XXX_INT_STICKY_FLAG2		AIC3XXX_MAKE_REG(0, 0, 44)
#define AIC3XXX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT	0x80
#define AIC3XXX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT	0x40
#define AIC3XXX_BUTTON_PRESS_INT			0x20
#define AIC3XXX_HEADSET_PLUG_UNPLUG_INT			0x10
#define AIC3XXX_LEFT_DRC_THRES_INT			0x08
#define AIC3XXX_RIGHT_DRC_THRES_INT			0x04
#define AIC3XXX_MINIDSP_D_STD_INT			0x02
#define AIC3XXX_MINIDSP_D_AUX_INT			0x01
#define AIC3XXX_INT_STICKY_FLAG3		AIC3XXX_MAKE_REG(0, 0, 45)
#define AIC3XXX_SPK_OVER_CURRENT_INT			0x80
#define AIC3XXX_LEFT_AGC_NOISE_INT			0x40
#define AIC3XXX_RIGHT_AGC_NOISE_INT			0x20
#define AIC3XXX_MINIDSP_A_STD_INT			0x10
#define AIC3XXX_MINIDSP_A_AUX_INT			0x08
#define AIC3XXX_LEFT_ADC_DC_DATA_AVAILABLE_INT		0x04
#define AIC3XXX_RIGHT_ADC_DC_DATA_AVAILABLE_INT		0x02
#define AIC3XXX_CP_SHORT_CIRCUIT_INT			0x01
#define AIC3XXX_INT1_CNTL		AIC3XXX_MAKE_REG(0, 0, 48)
#define AIC3XXX_HEADSET_IN_M		0x80
#define AIC3XXX_BUTTON_PRESS_M	0x40
#define AIC3XXX_DAC_DRC_THRES_M	0x20
#define AIC3XXX_AGC_NOISE_M		0x10
#define AIC3XXX_OVER_CURRENT_M	0x08
#define AIC3XXX_OVERFLOW_M		0x04
#define AIC3XXX_SPK_OVERCURRENT_M	0x02
#define AIC3XXX_CP_SHORT_CIRCUIT_M	0x02
#define AIC3XXX_INT2_CNTL		AIC3XXX_MAKE_REG(0, 0, 49)
#define AIC3XXX_INT_FMT			AIC3XXX_MAKE_REG(0, 0, 51)
#define AIC3XXX_DEVICE_ID		AIC3XXX_MAKE_REG(0, 0, 125)
#endif
