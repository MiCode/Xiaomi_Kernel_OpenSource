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
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/hif/ehpi/ehpi.c#1
*/

/*! \file   "ehpi.c"
*    \brief  Brief description.
*
*    Detail description.
*/


/******************************************************************************
*                         C O M P I L E R   F L A G S
*******************************************************************************
*/
#if !defined(MCR_EHTCR)
#define MCR_EHTCR                           0x0054
#endif

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include "colibri.h"
#include "wlan_lib.h"

/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                             M A C R O S
********************************************************************************
*/

/*******************************************************************************
*              F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static u_int8_t kalDevRegRead_impl(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, OUT uint32_t *pu4Value);

static u_int8_t kalDevRegWrite_impl(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, IN uint32_t u4Value);

/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read a 32 bit register value from device.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register     The register offset.
* \param[out] pu4Value      Pointer to the 32-bit value of the register been read.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevRegRead(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, OUT uint32_t *pu4Value)
{
	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);
	ASSERT(pu4Value);

	/* 0. acquire spinlock */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	/* 1. I/O stuff */
	kalDevRegRead_impl(prGlueInfo, u4Register, pu4Value);

	/* 2. release spin lock */
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write a 32 bit register value to device.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register     The register offset.
* \param[out] u4Value       The 32-bit value of the register to be written.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevRegWrite(struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, IN uint32_t u4Value)
{
	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	/* 0. acquire spinlock */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	/* 1. I/O stuff */
	kalDevRegWrite_impl(prGlueInfo, u4Register, u4Value);

	/* 2. release spin lock */
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read port data from device in unit of byte.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             The register offset.
* \param[in] u2Len              The number of byte to be read.
* \param[out] pucBuf            Pointer to data buffer for read
* \param[in] u2ValidOutBufSize  Length of the buffer valid to be accessed
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t
kalDevPortRead(IN struct GLUE_INFO *prGlueInfo,
	       IN uint16_t u2Port, IN uint16_t u2Len, OUT uint8_t *pucBuf, IN uint16_t u2ValidOutBufSize)
{
	uint32_t i;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	/* 0. acquire spinlock */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	/* 1. indicate correct length to HIFSYS if larger than 4-bytes */
	if (u2Len > 4)
		kalDevRegWrite_impl(prGlueInfo, MCR_EHTCR, ALIGN_4(u2Len));

	/* 2. address cycle */
#if EHPI16
	writew(u2Port, prGlueInfo->rHifInfo.mcr_addr_base);
#elif EHPI8
	writew((u2Port & 0xFF), prGlueInfo->rHifInfo.mcr_addr_base);
	writew(((u2Port & 0xFF00) >> 8), prGlueInfo->rHifInfo.mcr_addr_base);
#endif

	/* 3. data cycle */
	for (i = 0; i < ALIGN_4(u2Len); i += 4) {
#if EHPI16
		*((uint16_t *)&(pucBuf[i])) = (uint16_t) (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFFFF);
		*((uint16_t *)&(pucBuf[i + 2])) = (uint16_t) (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFFFF);
#elif EHPI8
		*((uint8_t *)&(pucBuf[i])) = (uint8_t) (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF);
		*((uint8_t *)&(pucBuf[i + 1])) = (uint8_t) (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF);
		*((uint8_t *)&(pucBuf[i + 2])) = (uint8_t) (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF);
		*((uint8_t *)&(pucBuf[i + 3])) = (uint8_t) (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF);
#endif
	}

	/* 4. restore length to 4 if necessary */
	if (u2Len > 4)
		kalDevRegWrite_impl(prGlueInfo, MCR_EHTCR, 4);

	/* 5. release spin lock */
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write port data to device in unit of byte.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             The register offset.
* \param[in] u2Len              The number of byte to be write.
* \param[out] pucBuf            Pointer to data buffer for write
* \param[in] u2ValidOutBufSize  Length of the buffer valid to be accessed
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
u_int8_t
kalDevPortWrite(struct GLUE_INFO *prGlueInfo,
		IN uint16_t u2Port, IN uint16_t u2Len, IN uint8_t *pucBuf, IN uint16_t u2ValidInBufSize)
{
	uint32_t i;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	/* 0. acquire spinlock */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	/* 1. indicate correct length to HIFSYS if larger than 4-bytes */
	if (u2Len > 4)
		kalDevRegWrite_impl(prGlueInfo, MCR_EHTCR, ALIGN_4(u2Len));

	/* 2. address cycle */
#if EHPI16
	writew(u2Port, prGlueInfo->rHifInfo.mcr_addr_base);
#elif EHPI8
	writew((u2Port & 0xFF), prGlueInfo->rHifInfo.mcr_addr_base);
	writew(((u2Port & 0xFF00) >> 8), prGlueInfo->rHifInfo.mcr_addr_base);
#endif

	/* 3. data cycle */
	for (i = 0; i < ALIGN_4(u2Len); i += 4) {
#if EHPI16
		writew((uint32_t) (*((uint16_t *)&(pucBuf[i]))), prGlueInfo->rHifInfo.mcr_data_base);
		writew((uint32_t) (*((uint16_t *)&(pucBuf[i + 2]))), prGlueInfo->rHifInfo.mcr_data_base);
#elif EHPI8
		writew((uint32_t) (*((uint8_t *)&(pucBuf[i]))), prGlueInfo->rHifInfo.mcr_data_base);
		writew((uint32_t) (*((uint8_t *)&(pucBuf[i + 1]))), prGlueInfo->rHifInfo.mcr_data_base);
		writew((uint32_t) (*((uint8_t *)&(pucBuf[i + 2]))), prGlueInfo->rHifInfo.mcr_data_base);
		writew((uint32_t) (*((uint8_t *)&(pucBuf[i + 3]))), prGlueInfo->rHifInfo.mcr_data_base);
#endif
	}

	/* 4. restore length to 4 if necessary */
	if (u2Len > 4)
		kalDevRegWrite_impl(prGlueInfo, MCR_EHTCR, 4);

	/* 5. release spin lock */
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port with single byte (for SDIO compatibility)
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u4Addr             I/O port offset
* \param[in] ucData             single byte of data to be written
* \param[in] u4ValidInBufSize   Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevWriteWithSdioCmd52(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Addr, IN uint8_t ucData)
{
	uint32_t u4RegValue;
	u_int8_t bRet;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	/* 0. acquire spinlock */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	/* 1. there is no single byte access support for eHPI, use 4-bytes write-after-read approach instead */
	if (kalDevRegRead_impl(prGlueInfo, u4Addr, &u4RegValue) == TRUE) {
		u4RegValue &= 0x00;
		u4RegValue |= ucData;

		bRet = kalDevRegWrite_impl(prGlueInfo, u4Addr, u4RegValue);
	} else {
		bRet = FALSE;
	}

	/* 2. release spin lock */
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_EHPI_BUS);

	return bRet;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read a 32 bit register value from device
*        without spin lock protection and dedicated for internal use
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register     The register offset.
* \param[out] pu4Value      Pointer to the 32-bit value of the register been read.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
static u_int8_t kalDevRegRead_impl(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, OUT uint32_t *pu4Value)
{
	ASSERT(prGlueInfo);

	/* 1. address cycle */
#if EHPI16
	writew(u4Register, prGlueInfo->rHifInfo.mcr_addr_base);
#elif EHPI8
	writew((u4Register & 0xFF), prGlueInfo->rHifInfo.mcr_addr_base);
	writew(((u4Register & 0xFF00) >> 8), prGlueInfo->rHifInfo.mcr_addr_base);
#endif

	/* 2. data cycle */
#if EHPI16
	*pu4Value = (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFFFF);
	*pu4Value |= ((readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFFFF) << 16);
#elif EHPI8
	*pu4Value = (readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF);
	*pu4Value |= ((readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF) << 8);
	*pu4Value |= ((readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF) << 16);
	*pu4Value |= ((readw(prGlueInfo->rHifInfo.mcr_data_base) & 0xFF) << 24);
#endif

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write a 32 bit register value to device.
*        without spin lock protection and dedicated for internal use
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register     The register offset.
* \param[out] u4Value       The 32-bit value of the register to be written.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
static u_int8_t kalDevRegWrite_impl(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, IN uint32_t u4Value)
{
	ASSERT(prGlueInfo);

	/* 1. address cycle */
#if EHPI16
	writew(u4Register, prGlueInfo->rHifInfo.mcr_addr_base);
#elif EHPI8
	writew((u4Register & 0xFF), prGlueInfo->rHifInfo.mcr_addr_base);
	writew(((u4Register & 0xFF00) >> 8), prGlueInfo->rHifInfo.mcr_addr_base);
#endif

	/* 2. data cycle */
#if EHPI16
	writew(u4Value, prGlueInfo->rHifInfo.mcr_data_base);
	writew((u4Value & 0xFFFF0000) >> 16, prGlueInfo->rHifInfo.mcr_data_base);
#elif EHPI8
	writew((u4Value & 0x000000FF), prGlueInfo->rHifInfo.mcr_data_base);
	writew((u4Value & 0x0000FF00) >> 8, prGlueInfo->rHifInfo.mcr_data_base);
	writew((u4Value & 0x00FF0000) >> 16, prGlueInfo->rHifInfo.mcr_data_base);
	writew((u4Value & 0xFF000000) >> 24, prGlueInfo->rHifInfo.mcr_data_base);
#endif

	return TRUE;
}
