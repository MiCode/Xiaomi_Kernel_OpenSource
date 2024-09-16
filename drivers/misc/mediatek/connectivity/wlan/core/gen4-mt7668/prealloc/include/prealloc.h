/* SPDX-License-Identifier: GPL-2.0 */
/*
 *Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _PREALLOC_H
#define _PREALLOC_H

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
enum ENUM_MEM_ID {
	MEM_ID_NIC_ADAPTER,
	MEM_ID_IO_BUFFER,
#if defined(_HIF_SDIO)
	MEM_ID_IO_CTRL,
	MEM_ID_RX_DATA,
#endif
#if defined(_HIF_USB)
	MEM_ID_TX_CMD,
	MEM_ID_TX_DATA_FFA,
	MEM_ID_TX_DATA,
	MEM_ID_RX_EVENT,
	MEM_ID_RX_DATA,
#endif

	MEM_ID_NUM, /* END, Do not modify */
};

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
#define PreLog(level, ...) pr_info("[wlan][MemPrealloc] " __VA_ARGS__)
#define MP_Dbg(...) PreLog(KERN_DEBUG, __VA_ARGS__)
#define MP_Info(...) PreLog(KERN_INFO, __VA_ARGS__)
#define MP_Err(...) PreLog(KERN_ERR, __VA_ARGS__)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
PVOID preallocGetMem(enum ENUM_MEM_ID memId);

#endif /* _PREALLOC_H */
