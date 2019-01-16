/* BEGIN PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
//add Touch driver for G610-T11
/* BEGIN PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* BEGIN PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/*
 * cyttsp4_device_access.h
 * Cypress TrueTouch(TM) Standard Product V4 Device Access module.
 * Configuration and Test command/status user interface.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_DEVICE_ACCESS_H
#define _LINUX_CYTTSP4_DEVICE_ACCESS_H

#define CYTTSP4_DEVICE_ACCESS_NAME "cyttsp4_device_access"

#define CYTTSP4_INPUT_ELEM_SZ (sizeof("0xHH") + 1)
#define CYTTSP4_TCH_PARAM_SIZE_BLK_SZ 128

/* Timeout values in ms. */
#define CY_DA_REQUEST_EXCLUSIVE_TIMEOUT	5000
#define CY_DA_COMMAND_COMPLETE_TIMEOUT	5000

struct cyttsp4_device_access_platform_data {
	char const *device_access_dev_name;
};

#define CY_CMD_IN_DATA_OFFSET_VALUE 0

#define CY_CMD_OUT_STATUS_OFFSET 0
#define CY_CMD_RET_PNL_OUT_ELMNT_SZ_OFFS_H 2
#define CY_CMD_RET_PNL_OUT_ELMNT_SZ_OFFS_L 3
#define CY_CMD_RET_PNL_OUT_DATA_FORMAT_OFFS 4

#define CY_CMD_RET_PANEL_ELMNT_SZ_MASK 0x07

/* BEGIN PN:SPBB-1276  ,Modified by l00184147, 2013/3/7*/
#define I2C_BUF_MAX_SIZE 250
/* END PN:SPBB-1276  ,Modified by l00184147, 2013/3/7*/

enum scanDataTypeList {
	CY_MUT_RAW,
	CY_MUT_BASE,
	CY_MUT_DIFF,
	CY_SELF_RAW,
	CY_SELF_BASE,
	CY_SELF_DIFF,
	CY_BAL_RAW,
	CY_BAL_BASE,
	CY_BAL_DIFF,
	/* BEGIN PN:DTS2013061703557 ,Added by l00184147, 2013/6/17*/
	CY_BUTON_DATA,
	/* END PN:DTS2013061703557 ,Added by l00184147, 2013/6/17*/
};

/* BEGIN PN:DTS2013061703557 ,Added by l00184147, 2013/6/17*/
enum check_data_type{
	CY_CHK_MUT_RAW,
	CY_CHK_SELF_RAW,
	CY_CHK_BUTTON,
};
/* END PN:DTS2013061703557 ,Added by l00184147, 2013/6/17*/

#endif /* _LINUX_CYTTSP4_DEVICE_ACCESS_H */
/* END PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/* END PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* END PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
