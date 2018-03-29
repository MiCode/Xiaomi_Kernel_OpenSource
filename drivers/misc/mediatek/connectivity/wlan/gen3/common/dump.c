/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to dump a segment of memory in bytes.
*
* \param[in] pucStartAddr   Pointer to the starting address of the memory to be dumped.
* \param[in] u4Length       Length of the memory to be dumped.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID dumpMemory8(IN PUINT_8 pucStartAddr, IN UINT_32 u4Length)
{
	ASSERT(pucStartAddr);

	LOG_FUNC("DUMP8 ADDRESS: %x, Length: %d\n", (ULONG) pucStartAddr, u4Length);

	while (u4Length > 0) {
		if (u4Length >= 16) {
			LOG_FUNC(
			"(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x %02x  %02x %02x %02x %02x\n",
				 (ULONG) pucStartAddr, pucStartAddr[0], pucStartAddr[1],
				 pucStartAddr[2], pucStartAddr[3], pucStartAddr[4], pucStartAddr[5],
				 pucStartAddr[6], pucStartAddr[7], pucStartAddr[8], pucStartAddr[9],
				 pucStartAddr[10], pucStartAddr[11], pucStartAddr[12],
				 pucStartAddr[13], pucStartAddr[14], pucStartAddr[15]);
			u4Length -= 16;
			pucStartAddr += 16;
		} else {
			switch (u4Length) {
			case 1:
				LOG_FUNC("(%p) %02x\n", pucStartAddr, pucStartAddr[0]);
				break;
			case 2:
				LOG_FUNC("(%p) %02x %02x\n", pucStartAddr, pucStartAddr[0], pucStartAddr[1]);
				break;
			case 3:
				LOG_FUNC("(%p) %02x %02x %02x\n",
					 pucStartAddr, pucStartAddr[0], pucStartAddr[1], pucStartAddr[2]);
				break;
			case 4:
				LOG_FUNC("(%p) %02x %02x %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3]);
				break;
			case 5:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4]);
				break;
			case 6:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5]);
				break;
			case 7:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6]);
				break;
			case 8:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7]);
				break;
			case 9:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
					 pucStartAddr[8]);
				break;
			case 10:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
					 pucStartAddr[8], pucStartAddr[9]);
				break;
			case 11:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
					 pucStartAddr[8], pucStartAddr[9], pucStartAddr[10]);
				break;
			case 12:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
					 pucStartAddr[8], pucStartAddr[9], pucStartAddr[10], pucStartAddr[11]);
				break;
			case 13:
				LOG_FUNC("(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x %02x  %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
					 pucStartAddr[8], pucStartAddr[9], pucStartAddr[10], pucStartAddr[11],
					 pucStartAddr[12]);
				break;
			case 14:
				LOG_FUNC(
				"(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x %02x  %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
					 pucStartAddr[8], pucStartAddr[9], pucStartAddr[10], pucStartAddr[11],
					 pucStartAddr[12], pucStartAddr[13]);
				break;
			case 15:
				LOG_FUNC(
				"(%p) %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x %02x  %02x %02x %02x\n",
					 pucStartAddr,
					 pucStartAddr[0], pucStartAddr[1], pucStartAddr[2], pucStartAddr[3],
					 pucStartAddr[4], pucStartAddr[5], pucStartAddr[6], pucStartAddr[7],
					 pucStartAddr[8], pucStartAddr[9], pucStartAddr[10], pucStartAddr[11],
					 pucStartAddr[12], pucStartAddr[13], pucStartAddr[14]);
				break;
			default:
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
* \param[in] pucStartAddr   Pointer to the starting address of the memory to be dumped.
* \param[in] u4Length       Length of the memory to be dumped.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID dumpMemory32(IN PUINT_32 pu4StartAddr, IN UINT_32 u4Length)
{
	PUINT_8 pucAddr;

	ASSERT(pu4StartAddr);

	LOG_FUNC("DUMP32 ADDRESS: %08x, Length: %d\n", (ULONG) pu4StartAddr, u4Length);

	if (IS_NOT_ALIGN_4((ULONG) pu4StartAddr)) {
		UINT_32 u4ProtrudeLen = sizeof(UINT_32) - ((ULONG) pu4StartAddr % 4);

		u4ProtrudeLen = ((u4Length < u4ProtrudeLen) ? u4Length : u4ProtrudeLen);
		LOG_FUNC("pu4StartAddr is not at DW boundary.\n");
		pucAddr = (PUINT_8) &pu4StartAddr[0];

		switch (u4ProtrudeLen) {
		case 1:
			LOG_FUNC("(%p) %02x------\n", pu4StartAddr, pucAddr[0]);
			break;
		case 2:
			LOG_FUNC("(%p) %02x%02x----\n", pu4StartAddr, pucAddr[1], pucAddr[0]);
			break;
		case 3:
			LOG_FUNC("(%p) %02x%02x%02x--\n", pu4StartAddr, pucAddr[2], pucAddr[1], pucAddr[0]);
			break;
		default:
			break;
		}

		u4Length -= u4ProtrudeLen;
		pu4StartAddr = (PUINT_32) ((ULONG) pu4StartAddr + u4ProtrudeLen);
	}

	while (u4Length > 0) {
		if (u4Length >= 16) {
			LOG_FUNC("(%p) %08x %08x %08x %08x\n",
				 pu4StartAddr, pu4StartAddr[0], pu4StartAddr[1], pu4StartAddr[2], pu4StartAddr[3]);
			pu4StartAddr += 4;
			u4Length -= 16;
		} else {
			switch (u4Length) {
			case 1:
				pucAddr = (PUINT_8) &pu4StartAddr[0];
				LOG_FUNC("(%p) ------%02x\n", pu4StartAddr, pucAddr[0]);
				break;
			case 2:
				pucAddr = (PUINT_8) &pu4StartAddr[0];
				LOG_FUNC("(%p) ----%02x%02x\n", pu4StartAddr, pucAddr[1], pucAddr[0]);
				break;
			case 3:
				pucAddr = (PUINT_8) &pu4StartAddr[0];
				LOG_FUNC("(%p) --%02x%02x%02x\n", pu4StartAddr, pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			case 4:
				LOG_FUNC("(%p) %08x\n", pu4StartAddr, pu4StartAddr[0]);
				break;
			case 5:
				pucAddr = (PUINT_8) &pu4StartAddr[1];
				LOG_FUNC("(%p) %08x ------%02x\n", pu4StartAddr, pu4StartAddr[0], pucAddr[0]);
				break;
			case 6:
				pucAddr = (PUINT_8) &pu4StartAddr[1];
				LOG_FUNC("(%p) %08x ----%02x%02x\n",
					 pu4StartAddr, pu4StartAddr[0], pucAddr[1], pucAddr[0]);
				break;
			case 7:
				pucAddr = (PUINT_8) &pu4StartAddr[1];
				LOG_FUNC("(%p) %08x --%02x%02x%02x\n",
					 pu4StartAddr, pu4StartAddr[0], pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			case 8:
				LOG_FUNC("(%p) %08x %08x\n", pu4StartAddr, pu4StartAddr[0], pu4StartAddr[1]);
				break;
			case 9:
				pucAddr = (PUINT_8) &pu4StartAddr[2];
				LOG_FUNC("(%p) %08x %08x ------%02x\n",
					 pu4StartAddr, pu4StartAddr[0], pu4StartAddr[1], pucAddr[0]);
				break;
			case 10:
				pucAddr = (PUINT_8) &pu4StartAddr[2];
				LOG_FUNC("(%p) %08x %08x ----%02x%02x\n",
					 pu4StartAddr, pu4StartAddr[0], pu4StartAddr[1], pucAddr[1], pucAddr[0]);
				break;
			case 11:
				pucAddr = (PUINT_8) &pu4StartAddr[2];
				LOG_FUNC("(%p) %08x %08x --%02x%02x%02x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1], pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			case 12:
				LOG_FUNC("(%p) %08x %08x %08x\n",
					 pu4StartAddr, pu4StartAddr[0], pu4StartAddr[1], pu4StartAddr[2]);
				break;
			case 13:
				pucAddr = (PUINT_8) &pu4StartAddr[3];
				LOG_FUNC("(%p) %08x %08x %08x ------%02x\n",
					 pu4StartAddr, pu4StartAddr[0], pu4StartAddr[1], pu4StartAddr[2], pucAddr[0]);
				break;
			case 14:
				pucAddr = (PUINT_8) &pu4StartAddr[3];
				LOG_FUNC("(%p) %08x %08x %08x ----%02x%02x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1], pu4StartAddr[2], pucAddr[1], pucAddr[0]);
				break;
			case 15:
				pucAddr = (PUINT_8) &pu4StartAddr[3];
				LOG_FUNC("(%p) %08x %08x %08x --%02x%02x%02x\n",
					 pu4StartAddr,
					 pu4StartAddr[0], pu4StartAddr[1], pu4StartAddr[2],
					 pucAddr[2], pucAddr[1], pucAddr[0]);
				break;
			default:
				break;
			}
			u4Length = 0;
		}
	}
}				/* end of dumpMemory32() */
