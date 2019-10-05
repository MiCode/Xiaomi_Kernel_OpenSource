/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef MCIFC_H_
#define MCIFC_H_

#include "platform.h"

/** @name MobiCore FastCall Defines
 * Defines for the two different FastCall's.
 */
/**/

/* --- global ---- */
#define MC_FC_INVALID	((u32)0)  /**< Invalid FastCall ID */

#if !defined(MC_ARMV7_FC)

#define FASTCALL_OWNER_TZOS_32  (0xBF000000) /**Trusted OS Fastcalls SMC32 */

/* SMC32 Trusted OS owned Fastcalls */
#define MC_FC_STD32_BASE	((u32)FASTCALL_OWNER_TZOS_32)
#define MC_FC_STD32(x)	((u32)(MC_FC_STD32_BASE + (x)))

#define MC_FC_INIT	MC_FC_STD32(1)  /**< Initializing FastCall. */
#define MC_FC_INFO	MC_FC_STD32(2)  /**< Info FastCall. */
#define MC_FC_MEM_TRACE	MC_FC_STD32(10)  /**< Enable SWd tracing via memory */
#define MC_FC_SWAP_CPU	MC_FC_STD32(54)  /**< Change new active Core */

#else

#define MC_FC_INIT	((u32)(-1))  /**< Initializing FastCall. */
#define MC_FC_INFO	((u32)(-2))  /**< Info FastCall. */
#define MC_FC_MEM_TRACE	((u32)(-31))  /**< Enable SWd tracing via memory */
#define MC_FC_SWAP_CPU	((u32)(0x84000005))  /**< Change new active Core */

#endif

/** @} */

/** @name MobiCore SMC Defines
 * Defines the different secure monitor calls (SMC) for world switching.
 */
/**< Yield to switch from NWd to SWd. */
#define MC_SMC_N_YIELD			3
/**< SIQ to switch from NWd to SWd. */
#define MC_SMC_N_SIQ			4
/** @} */

/** @name MobiCore status
 *  MobiCore status information.
 */
/**< MobiCore is not yet initialized. FastCall FcInit() to set up MobiCore.*/
#define MC_STATUS_NOT_INITIALIZED	0
/**< Bad parameters have been passed in FcInit(). */
#define MC_STATUS_BAD_INIT		1
/**< MobiCore did initialize properly. */
#define MC_STATUS_INITIALIZED		2
/**< MobiCore kernel halted due to an unrecoverable exception. Further
 * information is available extended info
 */
#define MC_STATUS_HALT			3
/** @} */

/** @name Extended Info Identifiers
 *  Extended info parameters for MC_FC_INFO to obtain further information
 *  depending on MobiCore state.
 */
/**< Version of the MobiCore Control Interface (MCI) */
#define MC_EXT_INFO_ID_MCI_VERSION	0
/**< MobiCore control flags */
#define MC_EXT_INFO_ID_FLAGS		1
/**< MobiCore halt condition code */
#define MC_EXT_INFO_ID_HALT_CODE	2
/**< MobiCore halt condition instruction pointer */
#define MC_EXT_INFO_ID_HALT_IP		3
/**< MobiCore fault counter */
#define MC_EXT_INFO_ID_FAULT_CNT	4
/**< MobiCore last fault cause */
#define MC_EXT_INFO_ID_FAULT_CAUSE	5
/**< MobiCore last fault meta */
#define MC_EXT_INFO_ID_FAULT_META	6
/**< MobiCore last fault threadid */
#define MC_EXT_INFO_ID_FAULT_THREAD	7
/**< MobiCore last fault instruction pointer */
#define MC_EXT_INFO_ID_FAULT_IP		8
/**< MobiCore last fault stack pointer */
#define MC_EXT_INFO_ID_FAULT_SP		9
/**< MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_DFSR	10
/**< MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_ADFSR	11
/**< MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_DFAR	12
/**< MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_IFSR	13
/**< MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_AIFSR	14
/**< MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_IFAR	15
/**< MobiCore configured by Daemon via fc_init flag */
#define MC_EXT_INFO_ID_MC_CONFIGURED	16
/**< MobiCore scheduling status: idle/non-idle */
#define MC_EXT_INFO_ID_MC_SCHED_STATUS	17
/**< MobiCore runtime status: initialized, halted */
#define MC_EXT_INFO_ID_MC_STATUS	18
/**< MobiCore exception handler last partner */
#define MC_EXT_INFO_ID_MC_EXC_PARTNER	19
/**< MobiCore exception handler last peer */
#define MC_EXT_INFO_ID_MC_EXC_IPCPEER	20
/**< MobiCore exception handler last IPC message */
#define MC_EXT_INFO_ID_MC_EXC_IPCMSG	21
/**< MobiCore exception handler last IPC data */
#define MC_EXT_INFO_ID_MC_EXC_IPCDATA	22
/**< MobiCore exception handler last UUID (uses 4 slots: 23 to 26) */
#define MC_EXT_INFO_ID_MC_EXC_UUID	23
#define MC_EXT_INFO_ID_MC_EXC_UUID1	24
#define MC_EXT_INFO_ID_MC_EXC_UUID2	25
#define MC_EXT_INFO_ID_MC_EXC_UUID3	26

/** @} */

/** @name FastCall return values
 * Return values of the MobiCore FastCalls.
 */
/**< No error. Everything worked fine. */
#define MC_FC_RET_OK				0
/**< FastCall was not successful. */
#define MC_FC_RET_ERR_INVALID			1
/**< MobiCore has already been initialized. */
#define MC_FC_RET_ERR_ALREADY_INITIALIZED	5
/** @} */

/** @name Init FastCall flags
 * Return flags of the Init FastCall.
 */
/**< SWd uses LPAE MMU table format. */
#define MC_FC_INIT_FLAG_LPAE			BIT(0)
/** @} */

#endif /** MCIFC_H_ */

/** @} */
