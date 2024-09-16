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
 ** Id: common/dump.c
 */

/*! \file   "dump.c"
 *    \brief  Provide memory dump function for debugging.
 *
 *    Provide memory dump function for debugging.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

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

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to dump a segment of memory in bytes.
 *
 * \param[in] pucStartAddr   Pointer to the starting address of the memory
 *                           to be dumped.
 * \param[in] u4Length       Length of the memory to be dumped.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void dumpMemory8(IN uint8_t *pucStartAddr,
		 IN uint32_t u4Length)
{
	ASSERT(pucStartAddr);

	LOG_FUNC("DUMP8 ADDRESS: %p, Length: %d\n", pucStartAddr,
		 u4Length);

#define case16 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x %02x %02x %02x  %02x %02x %02x %02x\n"

#define case07 "(%p) %02x %02x %02x %02x  %02x %02x %02x\n"

#define case08 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x\n"

#define case09 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x\n"

#define case10 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x %02x\n"

#define case11 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x %02x %02x\n"

#define case12 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x %02x %02x %02x\n"

#define case13 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x %02x %02x %02x  %02x\n"

#define case14 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x %02x %02x %02x  %02x %02x\n"

#define case15 "(%p) %02x %02x %02x %02x  %02x %02x %02x %02x" \
			" - %02x %02x %02x %02x  %02x %02x %02x\n"

	while (u4Length > 0) {
		if (u4Length >= 16) {
			LOG_FUNC
			(case16,
			 pucStartAddr, pucStartAddr[0], pucStartAddr[1],
			 pucStartAddr[2], pucStartAddr[3], pucStartAddr[4],
			 pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
			 pucStartAddr[8], pucStartAddr[9], pucStartAddr[10],
			 pucStartAddr[11], pucStartAddr[12],
			 pucStartAddr[13], pucStartAddr[14],
			 pucStartAddr[15]);
			u4Length -= 16;
			pucStartAddr += 16;
		} else {
			switch (u4Length) {
			case 1:
				LOG_FUNC("(%p) %02x\n", pucStartAddr,
					 pucStartAddr[0]);
				break;
			case 2:
				LOG_FUNC("(%p) %02x %02x\n", pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1]);
				break;
			case 3:
				LOG_FUNC("(%p) %02x %02x %02x\n",
					 pucStartAddr, pucStartAddr[0],
					 pucStartAddr[1], pucStartAddr[2]);
				break;
			case 4:
				LOG_FUNC("(%p) %02x %02x %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1],
					 pucStartAddr[2], pucStartAddr[3]);
				break;
			case 5:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1],
					 pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4]);
				break;
			case 6:
				LOG_FUNC
				("(%p) %02x %02x %02x %02x  %02x %02x\n",
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5]);
				break;
			case 7:
				LOG_FUNC
				(case07,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6]);
				break;
			case 8:
				LOG_FUNC
				(case08,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7]);
				break;
			case 9:
				LOG_FUNC
				(case09,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7], pucStartAddr[8]);
				break;
			case 10:
				LOG_FUNC
				(case10,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7], pucStartAddr[8],
				 pucStartAddr[9]);
				break;
			case 11:
				LOG_FUNC
				(case11,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7], pucStartAddr[8],
				 pucStartAddr[9], pucStartAddr[10]);
				break;
			case 12:
				LOG_FUNC
				(case12,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7], pucStartAddr[8],
				 pucStartAddr[9], pucStartAddr[10],
				 pucStartAddr[11]);
				break;
			case 13:
				LOG_FUNC
				(case13,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7], pucStartAddr[8],
				 pucStartAddr[9], pucStartAddr[10],
				 pucStartAddr[11], pucStartAddr[12]);
				break;
			case 14:
				LOG_FUNC
				(case14,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7], pucStartAddr[8],
				 pucStartAddr[9], pucStartAddr[10],
				 pucStartAddr[11], pucStartAddr[12],
				 pucStartAddr[13]);
				break;
			case 15:
			default:
				LOG_FUNC
				(case15,
				 pucStartAddr, pucStartAddr[0],
				 pucStartAddr[1], pucStartAddr[2],
				 pucStartAddr[3], pucStartAddr[4],
				 pucStartAddr[5], pucStartAddr[6],
				 pucStartAddr[7], pucStartAddr[8],
				 pucStartAddr[9], pucStartAddr[10],
				 pucStartAddr[11], pucStartAddr[12],
				 pucStartAddr[13], pucStartAddr[14]);
				break;
			}
			u4Length = 0;
		}
	}

	LOG_FUNC("\n");
}				/* end of dumpMemory8() */


/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to dump a segment of memory in double words.
 *
 * \param[in] pucStartAddr   Pointer to the starting address of the memory
 *                           to be dumped.
 * \param[in] u4Length       Length of the memory to be dumped.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void dumpMemory32(IN uint32_t *pu4StartAddr,
		  IN uint32_t u4Length)
{
	uint8_t *pucAddr;

	ASSERT(pu4StartAddr);

	LOG_FUNC("DUMP32 ADDRESS: %p, Length: %d\n", pu4StartAddr,
		 u4Length);

	if (IS_NOT_ALIGN_4((unsigned long)pu4StartAddr)) {
		uint32_t u4ProtrudeLen =
			sizeof(uint32_t) - ((unsigned long)pu4StartAddr % 4);

		u4ProtrudeLen =
			((u4Length < u4ProtrudeLen) ? u4Length : u4ProtrudeLen);
		LOG_FUNC("pu4StartAddr is not at DW boundary.\n");
		pucAddr = (uint8_t *) &pu4StartAddr[0];

		switch (u4ProtrudeLen) {
		case 1:
			LOG_FUNC("(%p) %02x------\n", pu4StartAddr, pucAddr[0]);
			break;
		case 2:
			LOG_FUNC("(%p) %02x%02x----\n", pu4StartAddr,
				 pucAddr[1], pucAddr[0]);
			break;
		case 3:
			LOG_FUNC("(%p) %02x%02x%02x--\n", pu4StartAddr,
				 pucAddr[2], pucAddr[1], pucAddr[0]);
			break;
		default:
			break;
		}

		u4Length -= u4ProtrudeLen;
		pu4StartAddr = (uint32_t *)
			       ((unsigned long)pu4StartAddr + u4ProtrudeLen);
	}

	while (u4Length > 0) {
		if (u4Length >= 16) {
			LOG_FUNC("(%p) %08x %08x %08x %08x\n",
				 pu4StartAddr,
				 pu4StartAddr[0], pu4StartAddr[1],
				 pu4StartAddr[2], pu4StartAddr[3]);
			pu4StartAddr += 4;
			u4Length -= 16;
		} else {
			switch (u4Length) {
			case 1:
				pucAddr = (uint8_t *) &pu4StartAddr[0];
				LOG_FUNC("(%p) ------%02x\n",
					 pu4StartAddr, pucAddr[0]);
				break;
			case 2:
				pucAddr = (uint8_t *) &pu4StartAddr[0];
				LOG_FUNC("(%p) ----%02x%02x\n", pu4StartAddr,
					 pucAddr[1], pucAddr[0]);
				break;
			case 3:
				pucAddr = (uint8_t *) &pu4StartAddr[0];
				LOG_FUNC("(%p) --%02x%02x%02x\n", pu4StartAddr,
					 pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			case 4:
				LOG_FUNC("(%p) %08x\n", pu4StartAddr,
					 pu4StartAddr[0]);
				break;
			case 5:
				pucAddr = (uint8_t *) &pu4StartAddr[1];
				LOG_FUNC("(%p) %08x ------%02x\n", pu4StartAddr,
					 pu4StartAddr[0], pucAddr[0]);
				break;
			case 6:
				pucAddr = (uint8_t *) &pu4StartAddr[1];
				LOG_FUNC("(%p) %08x ----%02x%02x\n",
					 pu4StartAddr, pu4StartAddr[0],
					 pucAddr[1], pucAddr[0]);
				break;
			case 7:
				pucAddr = (uint8_t *) &pu4StartAddr[1];
				LOG_FUNC("(%p) %08x --%02x%02x%02x\n",
					 pu4StartAddr, pu4StartAddr[0],
					 pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			case 8:
				LOG_FUNC("(%p) %08x %08x\n", pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1]);
				break;
			case 9:
				pucAddr = (uint8_t *) &pu4StartAddr[2];
				LOG_FUNC("(%p) %08x %08x ------%02x\n",
					 pu4StartAddr, pu4StartAddr[0],
					 pu4StartAddr[1], pucAddr[0]);
				break;
			case 10:
				pucAddr = (uint8_t *) &pu4StartAddr[2];
				LOG_FUNC("(%p) %08x %08x ----%02x%02x\n",
					 pu4StartAddr, pu4StartAddr[0],
					 pu4StartAddr[1], pucAddr[1],
					 pucAddr[0]);
				break;
			case 11:
				pucAddr = (uint8_t *) &pu4StartAddr[2];
				LOG_FUNC("(%p) %08x %08x --%02x%02x%02x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1],
					 pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			case 12:
				LOG_FUNC("(%p) %08x %08x %08x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1],
					 pu4StartAddr[2]);
				break;
			case 13:
				pucAddr = (uint8_t *) &pu4StartAddr[3];
				LOG_FUNC("(%p) %08x %08x %08x ------%02x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1],
					 pu4StartAddr[2], pucAddr[0]);
				break;
			case 14:
				pucAddr = (uint8_t *) &pu4StartAddr[3];
				LOG_FUNC("(%p) %08x %08x %08x ----%02x%02x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1],
					 pu4StartAddr[2],
					 pucAddr[1], pucAddr[0]);
				break;
			case 15:
			default:
				pucAddr = (uint8_t *) &pu4StartAddr[3];
				LOG_FUNC("(%p) %08x %08x %08x --%02x%02x%02x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1],
					 pu4StartAddr[2],
					 pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			}
			u4Length = 0;
		}
	}
}				/* end of dumpMemory32() */

