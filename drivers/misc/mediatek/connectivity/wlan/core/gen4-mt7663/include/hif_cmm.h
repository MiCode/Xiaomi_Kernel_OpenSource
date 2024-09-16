/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*! \file   "hif.h"
 *    \brief  Functions for the driver to register bus and setup the IRQ
 *
 *    Functions for the driver to register bus and setup the IRQ
 */


#ifndef _HIF_CMM_H
#define _HIF_CMM_H


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/* Interface Type */
enum MT_INF_TYPE {
	MT_DEV_INF_UNKNOWN = 0,
	MT_DEV_INF_PCI = 1,
	MT_DEV_INF_USB = 2,
	MT_DEV_INF_RBUS = 4,
	MT_DEV_INF_PCIE = 5,
	MT_DEV_INF_SDIO = 6,
	MT_DEV_INF_EHPI = 7,
	MT_DEV_INF_AXI = 8,
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define IS_SDIO_INF(__GlueInfo) \
	((__GlueInfo)->u4InfType == MT_DEV_INF_SDIO)
#define IS_USB_INF(__GlueInfo) \
	((__GlueInfo)->u4InfType == MT_DEV_INF_USB)
#define IS_PCIE_INF(__GlueInfo) \
	((__GlueInfo)->u4InfType == MT_DEV_INF_PCIE)
#define IS_EHPI_INF(__GlueInfo)	\
	((__GlueInfo)->u4InfType == MT_DEV_INF_PCIE)

#define HAL_WRITE_HIF_TXD(_prChipInfo, _pucOutputBuf, _u2InfoBufLen) \
{ \
	uint16_t _u2DataLen = (uint16_t)(_u2InfoBufLen); \
	uint8_t *_prBuf = (_pucOutputBuf); \
	if (_prChipInfo->fillHifTxDesc) \
		_prChipInfo->fillHifTxDesc(&_prBuf, &_u2DataLen); \
}

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */


/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#endif /* _HIF_CMM_H */
