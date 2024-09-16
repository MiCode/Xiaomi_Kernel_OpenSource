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
 ** Id: /os/linux/include/gl_kal.h
 */

/*! \file   gl_kal.h
 *  \brief  Declaration of KAL functions - kal*() which is provided
 *          by GLUE Layer.
 *
 *    Any definitions in this file will be shared among GLUE Layer
 *    and internal Driver Stack.
 */

#ifndef _GL_KAL_H
#define _GL_KAL_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "config.h"
#include "gl_os.h"
#include "gl_typedef.h"
#include "gl_wext_priv.h"
#include "link_drv.h"
#include "nic/mac.h"
#include "nic/wlan_def.h"
#include "wlan_lib.h"
#include "wlan_oid.h"

#if CFG_ENABLE_BT_OVER_WIFI
#include "nic/bow.h"
#endif

#if DBG
extern int allocatedMemSize;
#endif

extern int g_u4HaltFlag;
extern int g_u4WlanInitFlag;

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* Define how many concurrent operation networks. */
#define KAL_BSS_NUM             4

#define KAL_AIS_NUM           1

#if CFG_DUAL_P2PLIKE_INTERFACE
#define KAL_P2P_NUM             2
#else
#define KAL_P2P_NUM             1
#endif

/* Threading */
#if CFG_SUPPORT_MULTITHREAD
#define GLUE_FLAG_MAIN_PROCESS \
	(GLUE_FLAG_HALT | GLUE_FLAG_SUB_MOD_MULTICAST | \
	GLUE_FLAG_TX_CMD_DONE | GLUE_FLAG_TXREQ | GLUE_FLAG_TIMEOUT | \
	GLUE_FLAG_FRAME_FILTER | GLUE_FLAG_OID | GLUE_FLAG_RX)

#define GLUE_FLAG_HIF_PROCESS \
	(GLUE_FLAG_HALT | GLUE_FLAG_INT | GLUE_FLAG_HIF_TX | \
	GLUE_FLAG_HIF_TX_CMD | GLUE_FLAG_HIF_FW_OWN | \
	GLUE_FLAG_HIF_PRT_HIF_DBG_INFO | \
	GLUE_FLAG_UPDATE_WMM_QUOTA)

#define GLUE_FLAG_RX_PROCESS (GLUE_FLAG_HALT | GLUE_FLAG_RX_TO_OS)
#else
/* All flags for single thread driver */
#define GLUE_FLAG_TX_PROCESS  0xFFFFFFFF
#endif

#if CFG_SUPPORT_SNIFFER
#define RADIOTAP_FIELD_TSFT			BIT(0)
#define RADIOTAP_FIELD_FLAGS		BIT(1)
#define RADIOTAP_FIELD_RATE			BIT(2)
#define RADIOTAP_FIELD_CHANNEL		BIT(3)
#define RADIOTAP_FIELD_ANT_SIGNAL	BIT(5)
#define RADIOTAP_FIELD_ANT_NOISE	BIT(6)
#define RADIOTAP_FIELD_ANT			BIT(11)
#define RADIOTAP_FIELD_MCS			BIT(19)
#define RADIOTAP_FIELD_AMPDU		BIT(20)
#define RADIOTAP_FIELD_VHT			BIT(21)
#define RADIOTAP_FIELD_VENDOR       BIT(30)

#define RADIOTAP_LEN_VHT			48
#define RADIOTAP_FIELDS_VHT (RADIOTAP_FIELD_TSFT | \
				    RADIOTAP_FIELD_FLAGS | \
				    RADIOTAP_FIELD_RATE | \
				    RADIOTAP_FIELD_CHANNEL | \
				    RADIOTAP_FIELD_ANT_SIGNAL | \
				    RADIOTAP_FIELD_ANT_NOISE | \
				    RADIOTAP_FIELD_ANT | \
				    RADIOTAP_FIELD_AMPDU | \
				    RADIOTAP_FIELD_VHT | \
				    RADIOTAP_FIELD_VENDOR)

#define RADIOTAP_LEN_HT				36
#define RADIOTAP_FIELDS_HT (RADIOTAP_FIELD_TSFT | \
				    RADIOTAP_FIELD_FLAGS | \
				    RADIOTAP_FIELD_RATE | \
				    RADIOTAP_FIELD_CHANNEL | \
				    RADIOTAP_FIELD_ANT_SIGNAL | \
				    RADIOTAP_FIELD_ANT_NOISE | \
				    RADIOTAP_FIELD_ANT | \
				    RADIOTAP_FIELD_MCS | \
				    RADIOTAP_FIELD_AMPDU | \
				    RADIOTAP_FIELD_VENDOR)

#define RADIOTAP_LEN_LEGACY			26
#define RADIOTAP_FIELDS_LEGACY (RADIOTAP_FIELD_TSFT | \
				    RADIOTAP_FIELD_FLAGS | \
				    RADIOTAP_FIELD_RATE | \
				    RADIOTAP_FIELD_CHANNEL | \
				    RADIOTAP_FIELD_ANT_SIGNAL | \
				    RADIOTAP_FIELD_ANT_NOISE | \
				    RADIOTAP_FIELD_ANT | \
				    RADIOTAP_FIELD_VENDOR)
#endif

/* performance monitor feature */
#define PERF_MON_INIT_BIT       (0)
#define PERF_MON_DISABLE_BIT    (1)
#define PERF_MON_STOP_BIT       (2)
#define PERF_MON_RUNNING_BIT    (3)

#define PERF_MON_UPDATE_INTERVAL (1000)
#define PERF_MON_TP_MAX_THRESHOLD (10)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* Lock for process */
enum ENUM_SPIN_LOCK_CATEGORY_E {
	SPIN_LOCK_FSM = 0,

#if CFG_SUPPORT_MULTITHREAD
	SPIN_LOCK_TX_PORT_QUE,
	SPIN_LOCK_TX_CMD_QUE,
	SPIN_LOCK_TX_CMD_DONE_QUE,
	SPIN_LOCK_TC_RESOURCE,
	SPIN_LOCK_RX_TO_OS_QUE,
#endif

	/* FIX ME */
	SPIN_LOCK_RX_QUE,
	SPIN_LOCK_RX_FREE_QUE,
	SPIN_LOCK_TX_QUE,
	SPIN_LOCK_CMD_QUE,
	SPIN_LOCK_TX_RESOURCE,
	SPIN_LOCK_CMD_RESOURCE,
	SPIN_LOCK_QM_TX_QUEUE,
	SPIN_LOCK_CMD_PENDING,
	SPIN_LOCK_CMD_SEQ_NUM,
	SPIN_LOCK_TX_MSDU_INFO_LIST,
	SPIN_LOCK_TXING_MGMT_LIST,
	SPIN_LOCK_TX_SEQ_NUM,
	SPIN_LOCK_TX_COUNT,
	SPIN_LOCK_TXS_COUNT,
	/* end    */
	SPIN_LOCK_TX,
	/* TX/RX Direct : BEGIN */
	SPIN_LOCK_TX_DIRECT,
	SPIN_LOCK_RX_DIRECT,
	SPIN_LOCK_RX_DIRECT_REORDER,
	/* TX/RX Direct : END */
	SPIN_LOCK_IO_REQ,
	SPIN_LOCK_INT,
	SPIN_LOCK_UPDATE_WMM_QUOTA,

	SPIN_LOCK_MGT_BUF,
	SPIN_LOCK_MSG_BUF,
	SPIN_LOCK_STA_REC,

	SPIN_LOCK_MAILBOX,
	SPIN_LOCK_TIMER,

	/* SPIN_LOCK_BOW_TABLE,*/

	SPIN_LOCK_EHPI_BUS,	/* only for EHPI */
	SPIN_LOCK_NET_DEV,
	SPIN_LOCK_NUM
};

enum ENUM_MUTEX_CATEGORY_E {
	MUTEX_TX_CMD_CLEAR,
	MUTEX_TX_DATA_DONE_QUE,
	MUTEX_DEL_INF,
	MUTEX_CHIP_RST,
	MUTEX_SET_OWN,
	MUTEX_BOOST_CPU,
	MUTEX_NUM
};

/* event for assoc information update */
struct EVENT_ASSOC_INFO {
	uint8_t ucAssocReq;	/* 1 for assoc req, 0 for assoc rsp */
	uint8_t ucReassoc;	/* 0 for assoc, 1 for reassoc */
	uint16_t u2Length;
	uint8_t *pucIe;
};

/* network type */
enum ENUM_KAL_NETWORK_TYPE_INDEX {
	KAL_NETWORK_TYPE_AIS_INDEX = 0,
#if CFG_ENABLE_WIFI_DIRECT
	KAL_NETWORK_TYPE_P2P_INDEX,
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	KAL_NETWORK_TYPE_BOW_INDEX,
#endif
	KAL_NETWORK_TYPE_INDEX_NUM
};

/* malloc type */
enum ENUM_KAL_MEM_ALLOCATION_TYPE_E {
	PHY_MEM_TYPE,		/* physically continuous */
	VIR_MEM_TYPE,		/* virtually continuous */
	MEM_TYPE_NUM
};

/* suspend/resume related */
#define KAL_WAKE_LOCK_T uint32_t

/* TODO: unknown feature */
#if CFG_SUPPORT_AGPS_ASSIST
enum ENUM_MTK_AGPS_ATTR {
	MTK_ATTR_AGPS_INVALID,
	MTK_ATTR_AGPS_CMD,
	MTK_ATTR_AGPS_DATA,
	MTK_ATTR_AGPS_IFINDEX,
	MTK_ATTR_AGPS_IFNAME,
	MTK_ATTR_AGPS_MAX
};

enum ENUM_AGPS_EVENT {
	AGPS_EVENT_WLAN_ON,
	AGPS_EVENT_WLAN_OFF,
	AGPS_EVENT_WLAN_AP_LIST,
};
u_int8_t kalIndicateAgpsNotify(struct ADAPTER *prAdapter,
			       uint8_t cmd,
			       uint8_t *data, uint16_t dataLen);
#endif /* CFG_SUPPORT_AGPS_ASSIST */

#if CFG_SUPPORT_SNIFFER
/* Vendor Namespace
 * Bit Number 30
 * Required Alignment 2 bytes
 */
struct RADIOTAP_FIELD_VENDOR_ {
	uint8_t aucOUI[3];
	uint8_t ucSubNamespace;
	uint16_t u2DataLen;
	uint8_t ucData;
} __KAL_ATTRIB_PACKED__;

struct MONITOR_RADIOTAP {
	/* radiotap header */
	uint8_t ucItVersion;	/* set to 0 */
	uint8_t ucItPad;
	uint16_t u2ItLen;	/* entire length */
	uint32_t u4ItPresent;	/* fields present */

	/* TSFT
	 * Bit Number 0
	 * Required Alignment 8 bytes
	 * Unit microseconds
	 */
	uint64_t u8MacTime;

	/* Flags
	 * Bit Number 1
	 */
	uint8_t ucFlags;

	/* Rate
	 * Bit Number 2
	 * Unit 500 Kbps
	 */
	uint8_t ucRate;

	/* Channel
	 * Bit Number 3
	 * Required Alignment 2 bytes
	 */
	uint16_t u2ChFrequency;
	uint16_t u2ChFlags;

	/* Antenna signal
	 * Bit Number 5
	 * Unit dBm
	 */
	uint8_t ucAntennaSignal;

	/* Antenna noise
	 * Bit Number 6
	 * Unit dBm
	 */
	uint8_t ucAntennaNoise;

	/* Antenna
	 * Bit Number 11
	 * Unit antenna index
	 */
	uint8_t ucAntenna;

	/* MCS
	 * Bit Number 19
	 * Required Alignment 1 byte
	 */
	uint8_t ucMcsKnown;
	uint8_t ucMcsFlags;
	uint8_t ucMcsMcs;

	/* A-MPDU status
	 * Bit Number 20
	 * Required Alignment 4 bytes
	 */
	uint32_t u4AmpduRefNum;
	uint16_t u2AmpduFlags;
	uint8_t ucAmpduDelimiterCRC;
	uint8_t ucAmpduReserved;

	/* VHT
	 * Bit Number 21
	 * Required Alignment 2 bytes
	 */
	uint16_t u2VhtKnown;
	uint8_t ucVhtFlags;
	uint8_t ucVhtBandwidth;
	uint8_t aucVhtMcsNss[4];
	uint8_t ucVhtCoding;
	uint8_t ucVhtGroupId;
	uint16_t u2VhtPartialAid;

	/* extension space */
	uint8_t aucReserve[12];
} __KAL_ATTRIB_PACKED__;
#endif

/* driver halt */
struct KAL_HALT_CTRL_T {
/* TODO: os-related */
#if 0
	struct semaphore lock;
	struct task_struct *owner;
#endif
	u_int8_t fgHalt;
	u_int8_t fgHeldByKalIoctl;
	OS_SYSTIME u4HoldStart;
};

struct KAL_THREAD_SCHEDSTATS {
	/* when marked: the profiling start time(ms),
	 * when unmarked: total duration(ms)
	 */
	unsigned long long time;
	/* time spent in exec (sum_exec_runtime) */
	unsigned long long exec;
	/* time spent in run-queue while not being scheduled (wait_sum) */
	unsigned long long runnable;
	/* time spent waiting for I/O (iowait_sum) */
	unsigned long long iowait;
};

enum ENUM_CMD_TX_RESULT {
	CMD_TX_RESULT_SUCCESS,
	CMD_TX_RESULT_FAILED,
	CMD_TX_RESULT_QUEUED,
	CMD_TX_RESULT_NUM
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
/* os-related: need implementation */
#define KAL_SET_BIT(bitOffset, value)      set_bit(bitOffset, &value)
#define KAL_CLR_BIT(bitOffset, value)      clear_bit(bitOffset, &value)
#define KAL_TEST_AND_CLEAR_BIT(bitOffset, value)  \
	test_and_clear_bit(bitOffset, &value)
#define KAL_TEST_BIT(bitOffset, value)     test_bit(bitOffset, &value)
#define SUSPEND_FLAG_FOR_WAKEUP_REASON	(0)
#define SUSPEND_FLAG_CLEAR_WHEN_RESUME	(1)


/*----------------------------------------------------------------------------*/
/* Macros of getting current thread id                                        */
/*----------------------------------------------------------------------------*/
/* TODO: os-related: need implementation */
#define KAL_GET_CURRENT_THREAD_ID() (0)
#define KAL_GET_CURRENT_THREAD_NAME() ("n/a")

/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_SPIN_LOCK_DECLARATION()             unsigned long __ulFlags

#define KAL_ACQUIRE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	kalAcquireSpinLock(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory, &__ulFlags)

#define KAL_RELEASE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	kalReleaseSpinLock(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory, __ulFlags)

/*----------------------------------------------------------------------------*/
/* Macros of MUTEX operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_ACQUIRE_MUTEX(_prAdapter, _rLockCategory)   \
	kalAcquireMutex(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory)

#define KAL_RELEASE_MUTEX(_prAdapter, _rLockCategory)   \
	kalReleaseMutex(((struct ADAPTER *)_prAdapter)->prGlueInfo,  \
	_rLockCategory)

/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
#define KAL_GET_PKT_QUEUE_ENTRY(_p)             GLUE_GET_PKT_QUEUE_ENTRY(_p)
#define KAL_GET_PKT_DESCRIPTOR(_prQueueEntry)  \
	GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)
#define KAL_GET_PKT_TID(_p)                     GLUE_GET_PKT_TID(_p)
#define KAL_GET_PKT_IS1X(_p)                    GLUE_GET_PKT_IS1X(_p)
#define KAL_GET_PKT_HEADER_LEN(_p)              GLUE_GET_PKT_HEADER_LEN(_p)
#define KAL_GET_PKT_PAYLOAD_LEN(_p)             GLUE_GET_PKT_PAYLOAD_LEN(_p)
#define KAL_GET_PKT_ARRIVAL_TIME(_p)            GLUE_GET_PKT_ARRIVAL_TIME(_p)

/*----------------------------------------------------------------------------*/
/* Macros for kernel related defines                      */
/*----------------------------------------------------------------------------*/
/* TODO: os-related: need implementation */
#define IEEE80211_CHAN_PASSIVE_FLAG	(0)
#define IEEE80211_CHAN_PASSIVE_STR		"PASSIVE"

/**
 * enum nl80211_band - Frequency band
 * @NL80211_BAND_2GHZ: 2.4 GHz ISM band
 * @NL80211_BAND_5GHZ: around 5 GHz band (4.9 - 5.7 GHz)
 * @NL80211_BAND_60GHZ: around 60 GHz band (58.32 - 64.80 GHz)
 * @NUM_NL80211_BANDS: number of bands, avoid using this in userspace
 *	 since newer kernel versions may support more bands
 */
#define KAL_BAND_2GHZ (0)
#define KAL_BAND_5GHZ (1)
#define KAL_NUM_BANDS (2)

/**
 * enum nl80211_reg_rule_flags - regulatory rule flags
 * @NL80211_RRF_NO_OFDM: OFDM modulation not allowed
 * @NL80211_RRF_AUTO_BW: maximum available bandwidth should be calculated
 *  base on contiguous rules and wider channels will be allowed to cross
 *  multiple contiguous/overlapping frequency ranges.
 * @NL80211_RRF_DFS: DFS support is required to be used
 */
#define KAL_RRF_NO_OFDM (0)
#define KAL_RRF_DFS     (1)
#define KAL_RRF_AUTO_BW 0

/**
 * kalCfg80211ScanDone - abstraction of cfg80211_scan_done
 *
 * @request: the corresponding scan request (sanity checked by callers!)
 * @aborted: set to true if the scan was aborted for any reason,
 *	userspace will be notified of that
 *
 * Since linux-4.8.y the 2nd parameter is changed from bool to
 * struct cfg80211_scan_info, but we don't use all fields yet.
 */
#define kalCfg80211ScanDone(_request, aborted)

/* Consider on some Android platform, using request_firmware_direct()
 * may cause system failed to load firmware. So we still use
 * request_firmware().
 */
#define REQUEST_FIRMWARE(_fw, _name, _dev)

/*----------------------------------------------------------------------------*/
/* Macros of wake_lock operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName)
#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout)
#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock)

/*----------------------------------------------------------------------------*/
/*!
 * \brief Cache memory allocation
 *
 * \param[in] u4Size Required memory size.
 * \param[in] eMemType  Memory allocation type
 *
 * \return Pointer to allocated memory
 *         or NULL
 */
/*----------------------------------------------------------------------------*/
#define kmalloc(_size, _type) kal_kmalloc(_size, _type)
#define vmalloc(_size) kal_vmalloc(_size)
#define in_interrupt() (FALSE)
#define kfree(_addr) kal_kfree(_addr)
#define vfree(_addr) kal_vfree(_addr)

#if DBG
#define kalMemAlloc(u4Size, eMemType) ({    \
	void *pvAddr; \
	if (eMemType == PHY_MEM_TYPE) { \
		if (in_interrupt()) \
			pvAddr = kmalloc(u4Size, GFP_ATOMIC);   \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
	} \
	else { \
		pvAddr = vmalloc(u4Size);   \
	} \
	if (pvAddr) {   \
		allocatedMemSize += u4Size;   \
		DBGLOG(INIT, INFO, "0x%p(%ld) allocated (%s:%s)\n", \
		    pvAddr, (uint32_t)u4Size, __FILE__, __func__);  \
	}   \
	pvAddr; \
})
#else
#define kalMemAlloc(u4Size, eMemType) ({    \
	void *pvAddr; \
	if (eMemType == PHY_MEM_TYPE) { \
		if (in_interrupt()) \
			pvAddr = kmalloc(u4Size, GFP_ATOMIC);   \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
	} \
	else { \
		pvAddr = vmalloc(u4Size);   \
	} \
	if (!pvAddr) \
		ASSERT_NOMEM(); \
	pvAddr; \
})
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief Free allocated cache memory
 *
 * \param[in] pvAddr Required memory size.
 * \param[in] eMemType  Memory allocation type
 * \param[in] u4Size Allocated memory size.
 *
 * \return -
 */
/*----------------------------------------------------------------------------*/
#if DBG
#define kalMemFree(pvAddr, eMemType, u4Size)  \
{   \
	if (pvAddr) {   \
		allocatedMemSize -= u4Size; \
		DBGLOG(INIT, INFO, "0x%p(%ld) freed (%s:%s)\n", \
			pvAddr, (uint32_t)u4Size, __FILE__, __func__);  \
	}   \
	if (eMemType == PHY_MEM_TYPE) { \
		kfree(pvAddr); \
	} \
	else { \
		vfree(pvAddr); \
	} \
}
#else
#define kalMemFree(pvAddr, eMemType, u4Size)  \
{   \
	if (eMemType == PHY_MEM_TYPE) { \
		kfree(pvAddr); \
	} \
	else { \
		vfree(pvAddr); \
	} \
}
#endif

#define kalUdelay(u4USec) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalMdelay(u4MSec) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalMsleep(u4MSec) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalUsleep_range(u4MinUSec, u4MaxUSec) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* Copy memory from user space to kernel space */
#define kalMemCopyFromUser(_pvTo, _pvFrom, _u4N) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* Copy memory from kernel space to user space */
#define kalMemCopyToUser(_pvTo, _pvFrom, _u4N) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* Copy memory block with specific size */
#define kalMemCopy(pvDst, pvSrc, u4Size)  \
	memcpy(pvDst, pvSrc, u4Size)

/* Set memory block with specific pattern */
#define kalMemSet(pvAddr, ucPattern, u4Size)  \
	memset(pvAddr, ucPattern, u4Size)

/* Compare two memory block with specific length.
 * Return zero if they are the same.
 */
#define kalMemCmp(pvAddr1, pvAddr2, u4Size)  \
	memcmp(pvAddr1, pvAddr2, u4Size)

/* Zero specific memory block */
#define kalMemZero(pvAddr, u4Size)  \
	memset(pvAddr, 0, u4Size)

/* Move memory block with specific size */
#define kalMemMove(pvDst, pvSrc, u4Size)  \
	memmove(pvDst, pvSrc, u4Size)

#define strnicmp(s1, s2, n) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* string operation */
#define kalStrCpy(dest, src)               strcpy(dest, src)
#define kalStrnCpy(dest, src, n)           strncpy(dest, src, n)
#define kalStrCmp(ct, cs)                  strcmp(ct, cs)
#define kalStrnCmp(ct, cs, n)              strncmp(ct, cs, n)
#define kalStrChr(s, c)                    strchr(s, c)
#define kalStrrChr(s, c)                   strrchr(s, c)
#define kalStrnChr(s, n, c)                strnchr(s, n, c)
#define kalStrLen(s)                       strlen(s)
#define kalStrnLen(s, b)                   strnlen(s, b)
#define kalStrniCmp(ct, cs, n)             strncasecmp(ct, cs, n)
/* #define kalStrtoul(cp, endp, base)      simple_strtoul(cp, endp, base) */
/* #define kalStrtol(cp, endp, base)       simple_strtol(cp, endp, base) */
#define kalkStrtou8(cp, base, resp)        kal_strtou8(cp, base, resp)
#define kalkStrtou16(cp, base, resp)       kal_strtou16(cp, base, resp)
#define kalkStrtou32(cp, base, resp)       kal_strtou32(cp, base, resp)
#define kalkStrtos32(cp, base, resp)       kal_strtos32(cp, base, resp)
#define kalSnprintf(buf, size, fmt, ...)   \
	snprintf(buf, size, fmt, ##__VA_ARGS__)
#define kalScnprintf(buf, size, fmt, ...)  \
	kal_scnprintf(buf, size, fmt, ##__VA_ARGS__)
#define kalSprintf(buf, fmt, ...)          sprintf(buf, fmt, __VA_ARGS__)
/* remove for AOSP */
/* #define kalSScanf(buf, fmt, ...)        sscanf(buf, fmt, __VA_ARGS__) */
#define kalStrStr(ct, cs)                  strstr(ct, cs)
#define kalStrSep(s, ct)                   strsep(s, ct)
#define kalStrCat(dest, src)               strcat(dest, src)
#define kalIsXdigit(c)                     isxdigit(c)
#define kalStrtoint(_data, _base, _res) kal_strtoint(_data, _base, _res)
#define kalStrtoul(_data, _base, _res) kal_strtoul(_data, _base, _res)
#define kalStrtokR(_buf, _tok, _saved) \
	strtok_r((char *)_buf, _tok, (char **)_saved)
/* implementation for no op API */
int8_t kal_atoi(uint8_t ch);
#define kalAtoi(_ch) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* defined for wince sdio driver only */
#if defined(_HIF_SDIO)
#define kalDevSetPowerState(prGlueInfo, ePowerMode) \
	glSetPowerState(prGlueInfo, ePowerMode)
#else
#define kalDevSetPowerState(prGlueInfo, ePowerMode)
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief Notify OS with SendComplete event of the specific packet.
 *        Linux should free packets here.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 * \param[in] status         Status Code for OS upper layer
 *
 * \return -
 */
/*----------------------------------------------------------------------------*/
#define kalSendComplete(prGlueInfo, pvPacket, status)   \
	    kalSendCompleteAndAwakeQueue(prGlueInfo, pvPacket)

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is used to locate the starting address of incoming
 *        ethernet frame for skb.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 *
 * \return starting address of ethernet frame buffer.
 */
/*----------------------------------------------------------------------------*/
#define kalQueryBufferPointer(prGlueInfo, pvPacket) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is used to query the length of valid buffer which is
 *        accessible during port read/write.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 *
 * \return starting address of ethernet frame buffer.
 */
/*----------------------------------------------------------------------------*/
#define kalQueryValidBufferLength(prGlueInfo, pvPacket) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is used to copy the entire frame from skb to the
 *        destination address in the input parameter.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pvPacket       Pointer of Packet Handle
 * \param[in] pucDestBuffer  Destination Address
 *
 * \return -
 */
/*----------------------------------------------------------------------------*/
#define kalCopyFrame(prGlueInfo, pvPacket, pucDestBuffer) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalGetTimeTick() KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define WLAN_TAG                        "[wlan]"
#define kalPrint(_Fmt...) printf(WLAN_TAG _Fmt)
#define kalPrintLimited(_Fmt...) printf(WLAN_TAG _Fmt)

#define kalBreakPoint() \
do { \
	DBGLOG(INIT, ERROR, "WARN_ON()"); \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__); \
} while (0)

#if CFG_ENABLE_AEE_MSG
#else
#define kalSendAeeException(_module, _desc, ...) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalSendAeeWarning(_module, _desc, ...) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalSendAeeReminding(_module, _desc, ...) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#endif

#define PRINTF_ARG(...)      __VA_ARGS__
#define SPRINTF(buf, arg)    {buf += sprintf((char *)(buf), PRINTF_ARG arg); }

#define USEC_TO_SYSTIME(_usec)      ((_usec) / USEC_PER_MSEC)
#define MSEC_TO_SYSTIME(_msec)      (_msec)

#define MSEC_TO_JIFFIES(_msec) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_TIME_INTERVAL_DECLARATION()     uint32_t timeval __rTs, __rTe
#define KAL_REC_TIME_START()                do_gettimeofday(&__rTs)
#define KAL_REC_TIME_END()                  do_gettimeofday(&__rTe)
#define KAL_GET_TIME_INTERVAL() \
	((SEC_TO_USEC(__rTe.tv_sec) + __rTe.tv_usec) - \
	(SEC_TO_USEC(__rTs.tv_sec) + __rTs.tv_usec))
#define KAL_ADD_TIME_INTERVAL(_Interval) \
	{ \
		(_Interval) += KAL_GET_TIME_INTERVAL(); \
	}

/* TODO: os-related HIF should we move to os/xxx/hif/include? */
#if defined(_HIF_PCIE)
#define KAL_DMA_TO_DEVICE	PCI_DMA_TODEVICE
#define KAL_DMA_FROM_DEVICE	PCI_DMA_FROMDEVICE

#define KAL_DMA_ALLOC_COHERENT(_dev, _size, _handle) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_FREE_COHERENT(_dev, _size, _addr, _handle) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_MAP_SINGLE(_dev, _ptr, _size, _dir) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_UNMAP_SINGLE(_dev, _addr, _size, _dir) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_MAPPING_ERROR(_dev, _addr) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
#define KAL_DMA_TO_DEVICE	DMA_TO_DEVICE
#define KAL_DMA_FROM_DEVICE	DMA_FROM_DEVICE

#define KAL_DMA_ALLOC_COHERENT(_dev, _size, _handle) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_FREE_COHERENT(_dev, _size, _addr, _handle) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_MAP_SINGLE(_dev, _ptr, _size, _dir) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_UNMAP_SINGLE(_dev, _addr, _size, _dir) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define KAL_DMA_MAPPING_ERROR(_dev, _addr) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#endif

#if defined(_HIF_AXI)
#define KAL_ARCH_SETUP_DMA_OPS(_dev, _base, _size, _iommu, _coherent) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
#define KAL_ARCH_SETUP_DMA_OPS(_dev, _base, _size, _iommu, _coherent)
#endif

/*----------------------------------------------------------------------------*/
/* Macros of show stack operations for using in Driver Layer                  */
/*----------------------------------------------------------------------------*/
#define kal_show_stack(_adapter, _task, _sp)

/*----------------------------------------------------------------------------*/
/* Macros of systrace operations for using in Driver Layer                    */
/*----------------------------------------------------------------------------*/
#define kalTraceBegin(_fmt, ...)
#define kalTraceEnd()
#define kalTraceInt(_value, _fmt, ...)
#define kalTraceCall()
#define kalTraceEvent(_fmt, ...)
#define TRACE(_expr, _fmt, ...) _expr

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Routines in gl_kal.c                                                       */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalAcquireSpinLock(_pr, _rLockCate, _plFlags) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr, _rLockCate, _plFlags)

#define kalReleaseSpinLock(_prGlueInfo, _rLockCategory, _ulFlags) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalUpdateMACAddress(_prGlueInfo, _pucMacAddr) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo, _pucMacAddr)

#define kalAcquireMutex(_prGlueInfo, _rMutexCategory) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo, _rMutexCategory)

#define kalReleaseMutex(_prGlueInfo, _rMutexCategory) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo, _rMutexCategory)

#define kalPacketFree(_prGlueInfo, _pvPacket) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo, _pvPacket)

#define kalPacketAlloc(_prGlueInfo, _u4Size, _ppucData) \
((void *) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo))

#define kalPacketAllocWithHeadroom(_prGlueInfo, _u4Size, _ppucData) \
((void *) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo))
#else
void kalAcquireSpinLock(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_SPIN_LOCK_CATEGORY_E rLockCategory,
			OUT unsigned long *plFlags);

void kalReleaseSpinLock(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_SPIN_LOCK_CATEGORY_E rLockCategory,
			IN unsigned long ulFlags);

void kalUpdateMACAddress(IN struct GLUE_INFO *prGlueInfo,
			 IN uint8_t *pucMacAddr);

void kalAcquireMutex(IN struct GLUE_INFO *prGlueInfo,
		     IN enum ENUM_MUTEX_CATEGORY_E rMutexCategory);

void kalReleaseMutex(IN struct GLUE_INFO *prGlueInfo,
		     IN enum ENUM_MUTEX_CATEGORY_E rMutexCategory);

void kalPacketFree(IN struct GLUE_INFO *prGlueInfo,
		   IN void *pvPacket);

void *kalPacketAlloc(IN struct GLUE_INFO *prGlueInfo,
		     IN uint32_t u4Size,
		     OUT uint8_t **ppucData);

void *kalPacketAllocWithHeadroom(IN struct GLUE_INFO
				 *prGlueInfo,
				 IN uint32_t u4Size, OUT uint8_t **ppucData);
#endif

void kalOsTimerInitialize(IN struct GLUE_INFO *prGlueInfo,
			  IN void *prTimerHandler);

u_int8_t kalSetTimer(IN struct GLUE_INFO *prGlueInfo,
		     IN OS_SYSTIME rInterval);

#ifdef CFG_REMIND_IMPLEMENT
#define kalProcessRxPacket(_prGlueInfo, _pvPacket, _pucPacketStart, \
	_u4PacketLen, _fgIsRetain, _aeCSUM) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
uint32_t
kalProcessRxPacket(IN struct GLUE_INFO *prGlueInfo,
		   IN void *pvPacket,
		   IN uint8_t *pucPacketStart, IN uint32_t u4PacketLen,
		   /* IN PBOOLEAN           pfgIsRetain, */
		   IN u_int8_t fgIsRetain, IN enum ENUM_CSUM_RESULT aeCSUM[]);
#endif

uint32_t kalRxIndicatePkts(IN struct GLUE_INFO *prGlueInfo,
			   IN void *apvPkts[],
			   IN uint8_t ucPktNum);

#ifdef CFG_REMIND_IMPLEMENT
#define kalRxIndicateOnePkt(_prGlueInfo, _pvPkt) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalIndicateStatusAndComplete(_prGlInfo, _eStatus, _pvBuf, _u4BufLen, \
	_ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalUpdateReAssocReqInfo(_prGlueInfo, _pucFrameBody, _u4FrameBodyLen, \
	_fgReassocRequest, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalUpdateReAssocRspInfo(_prGlueInfo, _pucFrameBody, _u4FrameBodyLen, \
	_ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
uint32_t kalRxIndicateOnePkt(IN struct GLUE_INFO
			     *prGlueInfo, IN void *pvPkt);

void
kalIndicateStatusAndComplete(IN struct GLUE_INFO
			     *prGlueInfo,
			     IN uint32_t eStatus, IN void *pvBuf,
			     IN uint32_t u4BufLen,
			     IN uint8_t ucBssIndex);

void
kalUpdateReAssocReqInfo(IN struct GLUE_INFO *prGlueInfo,
			IN uint8_t *pucFrameBody, IN uint32_t u4FrameBodyLen,
			IN u_int8_t fgReassocRequest,
			IN uint8_t ucBssIndex);

void kalUpdateReAssocRspInfo(IN struct GLUE_INFO
			     *prGlueInfo,
			     IN uint8_t *pucFrameBody,
			     IN uint32_t u4FrameBodyLen,
			     IN uint8_t ucBssIndex);
#endif

#if CFG_TX_FRAGMENT
u_int8_t
kalQueryTxPacketHeader(IN struct GLUE_INFO *prGlueInfo,
		       IN void *pvPacket, OUT uint16_t *pu2EtherTypeLen,
		       OUT uint8_t *pucEthDestAddr);
#endif /* CFG_TX_FRAGMENT */

#ifdef CFG_REMIND_IMPLEMENT
#define kalSendCompleteAndAwakeQueue(_prGlueInfo, _pvPacket) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo, _pvPacket)
#else
void kalSendCompleteAndAwakeQueue(IN struct GLUE_INFO
				  *prGlueInfo,
				  IN void *pvPacket);
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
#ifdef CFG_REMIND_IMPLEMENT
#define kalQueryTxChksumOffloadParam(_pvPacket, _pucFlag) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void kalQueryTxChksumOffloadParam(IN void *pvPacket,
				  OUT uint8_t *pucFlag);
#endif

void kalUpdateRxCSUMOffloadParam(IN void *pvPacket,
				 IN enum ENUM_CSUM_RESULT eCSUM[]);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

#ifdef CFG_REMIND_IMPLEMENT
#define kalRetrieveNetworkAddress(_prGlueInfo, _prMacAddr) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo, _prMacAddr)

#define kalReadyOnChannel(_prGlueInfo, _u8Cookie, _eBand, _eSco, \
	_ucChannelNum, _u4DurationMs, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalRemainOnChannelExpired(_prGlueInfo, _u8Cookie, _eBand, \
	_eSco, _ucChannelNum, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#if CFG_SUPPORT_DFS
#define	kalIndicateChannelSwitch(_prGlueInfo, _eSco, _ucChannelNum) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#endif


#define kalIndicateMgmtTxStatus(_prGlueInfo, _u8Cookie, _fgIsAck, \
	_pucFrameBuf, _u4FrameLen, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalIndicateRxMgmtFrame(_prGlueInfo, _prSwRfb, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalRetrieveNetworkAddress(IN struct GLUE_INFO *prGlueInfo,
				IN OUT uint8_t *prMacAddr);

void
kalReadyOnChannel(IN struct GLUE_INFO *prGlueInfo,
		  IN uint64_t u8Cookie,
		  IN enum ENUM_BAND eBand, IN enum ENUM_CHNL_EXT eSco,
		  IN uint8_t ucChannelNum, IN uint32_t u4DurationMs,
		  IN uint8_t ucBssIndex);

void
kalRemainOnChannelExpired(IN struct GLUE_INFO *prGlueInfo,
			  IN uint64_t u8Cookie, IN enum ENUM_BAND eBand,
			  IN enum ENUM_CHNL_EXT eSco, IN uint8_t ucChannelNum,
			  IN uint8_t ucBssIndex);

#if CFG_SUPPORT_DFS
void
kalIndicateChannelSwitch(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_CHNL_EXT eSco,
			IN uint8_t ucChannelNum);
#endif

void
kalIndicateMgmtTxStatus(IN struct GLUE_INFO *prGlueInfo,
			IN uint64_t u8Cookie, IN u_int8_t fgIsAck,
			IN uint8_t *pucFrameBuf, IN uint32_t u4FrameLen,
			IN uint8_t ucBssIndex);

void kalIndicateRxMgmtFrame(IN struct GLUE_INFO *prGlueInfo,
			    IN struct SW_RFB *prSwRfb,
			    IN uint8_t ucBssIndex);
#endif
/*----------------------------------------------------------------------------*/
/* Routines in interface - ehpi/sdio.c                                        */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalDevRegRead(_prGlueInfo, _u4Register, _pu4Value) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalDevRegRead(IN struct GLUE_INFO *prGlueInfo,
		       IN uint32_t u4Register,
		       OUT uint32_t *pu4Value);
#endif
u_int8_t kalDevRegRead_mac(IN struct GLUE_INFO *prGlueInfo,
			   IN uint32_t u4Register, OUT uint32_t *pu4Value);

#ifdef CFG_REMIND_IMPLEMENT
#define kalDevRegWrite(_prGlueInfo, _u4Register, _u4Value) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalDevRegWrite(struct GLUE_INFO *prGlueInfo,
			IN uint32_t u4Register,
			IN uint32_t u4Value);
#endif
u_int8_t kalDevRegWrite_mac(struct GLUE_INFO *prGlueInfo,
			    IN uint32_t u4Register, IN uint32_t u4Value);

u_int8_t
kalDevPortRead(IN struct GLUE_INFO *prGlueInfo,
	       IN uint16_t u2Port, IN uint32_t u2Len, OUT uint8_t *pucBuf,
	       IN uint32_t u2ValidOutBufSize);

u_int8_t
kalDevPortWrite(struct GLUE_INFO *prGlueInfo,
		IN uint16_t u2Port, IN uint32_t u2Len, IN uint8_t *pucBuf,
		IN uint32_t u2ValidInBufSize);

#ifdef CFG_REMIND_IMPLEMENT
#define kalDevWriteData(_prGlueInfo, _prMsduInfo) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalDevWriteCmd(_prGlueInfo, _prCmdInfo, _ucTC) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalDevKickData(_prGlueInfo) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalDevWriteData(IN struct GLUE_INFO *prGlueInfo,
			 IN struct MSDU_INFO *prMsduInfo);
enum ENUM_CMD_TX_RESULT kalDevWriteCmd(IN struct GLUE_INFO *prGlueInfo,
			IN struct CMD_INFO *prCmdInfo, IN uint8_t ucTC);
u_int8_t kalDevKickData(IN struct GLUE_INFO *prGlueInfo);
#endif
void kalDevReadIntStatus(IN struct ADAPTER *prAdapter,
			 OUT uint32_t *pu4IntStatus);

u_int8_t kalDevWriteWithSdioCmd52(IN struct GLUE_INFO
				  *prGlueInfo,
				  IN uint32_t u4Addr, IN uint8_t ucData);

#if CFG_SUPPORT_EXT_CONFIG
uint32_t kalReadExtCfg(IN struct GLUE_INFO *prGlueInfo);
#endif

#ifdef CFG_REMIND_IMPLEMENT
#define kalQoSFrameClassifierAndPacketInfo(_pr, _prPacket, _prTxPktInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#define kalGetEthDestAddr(_prGlueInfo, _prPacket, _pucEthDestAddr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalOidComplete(_prGlueInfo, _fgSetQuery, _u4SetQueryInfoLen, \
	       _rOidStatus) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalIoctl(_pr, _pfnOidHandler, _pvInfoBuf, _u4InfoBufLen, \
	_fgRead, _fgWaitResp, _fgCmd, _pu4QryInfoLen) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr, \
	_pfnOidHandler, _pvInfoBuf, _u4InfoBufLen, \
	_fgRead, _fgWaitResp, _fgCmd, _pu4QryInfoLen)

#define kalIoctlByBssIdx(_pr, _pfnOidHandler, _pvInfoBuf, _u4InfoBufLen, \
	_fgRead, _fgWaitResp, _fgCmd, _pu4QryInfoLen, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr, \
	_pfnOidHandler, _pvInfoBuf, _u4InfoBufLen, \
	_fgRead, _fgWaitResp, _fgCmd, _pu4QryInfoLen, _ucBssIndex)

#define SET_IOCTL_BSSIDX(_pr, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr, \
	_ucBssIndex)

#define GET_IOCTL_BSSIDX(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#else
u_int8_t
kalQoSFrameClassifierAndPacketInfo(IN struct GLUE_INFO
				   *prGlueInfo,
				   IN void *prPacket,
				   OUT struct TX_PACKET_INFO *prTxPktInfo);

u_int8_t kalGetEthDestAddr(IN struct GLUE_INFO *prGlueInfo,
			   IN void *prPacket,
			   OUT uint8_t *pucEthDestAddr);

void
kalOidComplete(IN struct GLUE_INFO *prGlueInfo,
	       IN u_int8_t fgSetQuery, IN uint32_t u4SetQueryInfoLen,
	       IN uint32_t rOidStatus);

uint32_t
kalIoctl(IN struct GLUE_INFO *prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN void *pvInfoBuf,
	 IN uint32_t u4InfoBufLen, IN u_int8_t fgRead,
	 IN u_int8_t fgWaitResp,
	 IN u_int8_t fgCmd, OUT uint32_t *pu4QryInfoLen);

uint32_t
kalIoctlByBssIdx(IN struct GLUE_INFO *prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN void *pvInfoBuf,
	 IN uint32_t u4InfoBufLen, IN u_int8_t fgRead,
	 IN u_int8_t fgWaitResp, IN u_int8_t fgCmd,
	 OUT uint32_t *pu4QryInfoLen,
	 IN uint8_t ucBssIndex);

void SET_IOCTL_BSSIDX(
	 IN struct ADAPTER *prAdapter,
	 IN uint8_t ucBssIndex);

uint8_t GET_IOCTL_BSSIDX(
	 IN struct ADAPTER *prAdapter);

#endif

void kalHandleAssocInfo(IN struct GLUE_INFO *prGlueInfo,
			IN struct EVENT_ASSOC_INFO *prAssocInfo);

#if CFG_ENABLE_FW_DOWNLOAD
#ifdef CFG_REMIND_IMPLEMENT
#define kalFirmwareImageMapping(_prGlueInfo, _ppvMapFileBuf, \
	_pu4FileLength, _eDlIdx) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalFirmwareImageUnmapping(_prGlueInfo, _prFwHandle, _pvMapFileBuf) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void *kalFirmwareImageMapping(IN struct GLUE_INFO
			      *prGlueInfo,
			      OUT void **ppvMapFileBuf,
			      OUT uint32_t *pu4FileLength,
			      IN enum ENUM_IMG_DL_IDX_T eDlIdx);
void kalFirmwareImageUnmapping(IN struct GLUE_INFO
			       *prGlueInfo,
			       IN void *prFwHandle, IN void *pvMapFileBuf);
#endif
#endif

/*----------------------------------------------------------------------------*/
/* Card Removal Check                                                         */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalIsCardRemoved(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalIsCardRemoved(IN struct GLUE_INFO *prGlueInfo);
#endif
/*----------------------------------------------------------------------------*/
/* TX                                                                         */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalFlushPendingTxPackets(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void kalFlushPendingTxPackets(IN struct GLUE_INFO
			      *prGlueInfo);
#endif

/*----------------------------------------------------------------------------*/
/* Media State Indication                                                     */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalGetMediaStateIndicated(_prGlueInfo, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalSetMediaStateIndicated(_prGlueInfo, _eParamMediaStateIndicate, \
	_ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
enum ENUM_PARAM_MEDIA_STATE kalGetMediaStateIndicated(
	IN struct GLUE_INFO
	*prGlueInfo,
	IN uint8_t ucBssIndex);

void kalSetMediaStateIndicated(IN struct GLUE_INFO *prGlueInfo,
		IN enum ENUM_PARAM_MEDIA_STATE eParamMediaStateIndicate,
		IN uint8_t ucBssIndex);
#endif

/*----------------------------------------------------------------------------*/
/* OID handling                                                               */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalOidCmdClearance(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalOidClearance(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalEnqueueCommand(_prGlueInfo, _prQueueEntry) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void kalOidCmdClearance(IN struct GLUE_INFO *prGlueInfo);

void kalOidClearance(IN struct GLUE_INFO *prGlueInfo);

void kalEnqueueCommand(IN struct GLUE_INFO *prGlueInfo,
		       IN struct QUE_ENTRY *prQueueEntry);
#endif

#if CFG_ENABLE_BT_OVER_WIFI
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi handling                                              */
/*----------------------------------------------------------------------------*/
void kalIndicateBOWEvent(IN struct GLUE_INFO *prGlueInfo,
			 IN struct BT_OVER_WIFI_EVENT *prEvent);

enum ENUM_BOW_DEVICE_STATE kalGetBowState(
	IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

u_int8_t kalSetBowState(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_BOW_DEVICE_STATE eBowState,
			uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

enum ENUM_BOW_DEVICE_STATE kalGetBowGlobalState(
	IN struct GLUE_INFO
	*prGlueInfo);

uint32_t kalGetBowFreqInKHz(IN struct GLUE_INFO
			    *prGlueInfo);

uint8_t kalGetBowRole(IN struct GLUE_INFO *prGlueInfo,
		      IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

void kalSetBowRole(IN struct GLUE_INFO *prGlueInfo,
		   IN uint8_t ucRole,
		   IN uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN]);

uint8_t kalGetBowAvailablePhysicalLinkCount(
	IN struct GLUE_INFO *prGlueInfo);

#if CFG_BOW_SEPARATE_DATA_PATH
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi Net Device Init/Uninit                                */
/*----------------------------------------------------------------------------*/
u_int8_t kalInitBowDevice(IN struct GLUE_INFO *prGlueInfo,
			  IN const char *prDevName);

u_int8_t kalUninitBowDevice(IN struct GLUE_INFO
			    *prGlueInfo);
#endif /* CFG_BOW_SEPARATE_DATA_PATH */
#endif /* CFG_ENABLE_BT_OVER_WIFI */

/*----------------------------------------------------------------------------*/
/* Security Frame Clearance                                                   */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalClearSecurityFrames(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalClearSecurityFramesByBssIdx(_prGlueInfo, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalSecurityFrameSendComplete(_prGlueInfo, _pvPacket, _rStatus) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void kalClearSecurityFrames(IN struct GLUE_INFO
			    *prGlueInfo);

void kalClearSecurityFramesByBssIdx(IN struct GLUE_INFO
				    *prGlueInfo,
				    IN uint8_t ucBssIndex);

void kalSecurityFrameSendComplete(IN struct GLUE_INFO
				  *prGlueInfo,
				  IN void *pvPacket, IN uint32_t rStatus);
#endif
/*----------------------------------------------------------------------------*/
/* Management Frame Clearance                                                 */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalClearMgmtFrames(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalClearMgmtFramesByBssIdx(_prGlueInfo, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalGetTxPendingFrameCount(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalGetTxPendingCmdCount(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalClearCommandQueue(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalSetTimer(_prGlueInfo, _u4Interval) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void kalClearMgmtFrames(IN struct GLUE_INFO *prGlueInfo);

void kalClearMgmtFramesByBssIdx(IN struct GLUE_INFO
				*prGlueInfo,
				IN uint8_t ucBssIndex);

uint32_t kalGetTxPendingFrameCount(IN struct GLUE_INFO
				   *prGlueInfo);

uint32_t kalGetTxPendingCmdCount(IN struct GLUE_INFO
				 *prGlueInfo);

void kalClearCommandQueue(IN struct GLUE_INFO *prGlueInfo);

u_int8_t kalSetTimer(IN struct GLUE_INFO *prGlueInfo,
		     IN uint32_t u4Interval);
#endif

u_int8_t kalCancelTimer(IN struct GLUE_INFO *prGlueInfo);

#ifdef CFG_REMIND_IMPLEMENT
#define kalScanDone(_prGlueInfo, _eNetTypeIdx, _status) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void kalScanDone(IN struct GLUE_INFO *prGlueInfo,
		 IN enum ENUM_KAL_NETWORK_TYPE_INDEX eNetTypeIdx,
		 IN uint32_t status);
#endif

#if CFG_SUPPORT_SCAN_CACHE_RESULT
uint8_t kalUpdateBssTimestamp(IN struct GLUE_INFO *prGlueInfo);
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */

void kalTimeoutHandler(unsigned long arg);

#ifdef CFG_REMIND_IMPLEMENT
#define kalRandomNumber() KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalSetEvent(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#define kalSetIntEvent(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#define kalSetWmmUpdateEvent(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#define kalSetHifDbgEvent(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)
#else
uint32_t kalRandomNumber(void);

void kalSetEvent(struct GLUE_INFO *pr);

void kalSetIntEvent(struct GLUE_INFO *pr);

void kalSetWmmUpdateEvent(struct GLUE_INFO *pr);

void kalSetHifDbgEvent(struct GLUE_INFO *pr);
#endif

#if CFG_SUPPORT_MULTITHREAD
#ifdef CFG_REMIND_IMPLEMENT
#define kalSetTxEvent2Hif(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#define kalSetTxEvent2Rx(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#define kalSetTxCmdEvent2Hif(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)
#define kalSetTxCmdDoneEvent(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)

#define kalSetRxProcessEvent(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)
#else
void kalSetTxEvent2Hif(struct GLUE_INFO *pr);

void kalSetTxEvent2Rx(struct GLUE_INFO *pr);

void kalSetTxCmdEvent2Hif(struct GLUE_INFO *pr);

void kalSetTxCmdDoneEvent(struct GLUE_INFO *pr);

void kalSetRxProcessEvent(struct GLUE_INFO *pr);
#endif
#endif
/*----------------------------------------------------------------------------*/
/* NVRAM/Registry Service                                                     */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalIsConfigurationExist(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalGetConfiguration(_prGlueInfo) \
((struct REG_INFO *)KAL_NEED_IMPLEMENT(__FILE__, __func__, \
	__LINE__, _prGlueInfo))

#define kalCfgDataRead16(_prGlueInfo, _u4Offset, _pu2Data) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalCfgDataWrite16(_prGlueInfo, _u4Offset, _u2Data) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalIsConfigurationExist(IN struct GLUE_INFO
				 *prGlueInfo);

struct REG_INFO *kalGetConfiguration(IN struct GLUE_INFO
				     *prGlueInfo);

u_int8_t kalCfgDataRead(IN struct GLUE_INFO *prGlueInfo,
			IN uint32_t u4Offset,
			IN ssize_t len, OUT uint16_t *pu2Data);

u_int8_t kalCfgDataRead16(IN struct GLUE_INFO *prGlueInfo,
			  IN uint32_t u4Offset,
			  OUT uint16_t *pu2Data);

u_int8_t kalCfgDataWrite16(IN struct GLUE_INFO *prGlueInfo,
			   IN uint32_t u4Offset, IN uint16_t u2Data);
#endif
/*----------------------------------------------------------------------------*/
/* WSC Connection                                                     */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalWSCGetActiveState(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalWSCGetActiveState(IN struct GLUE_INFO
			      *prGlueInfo);
#endif

/*----------------------------------------------------------------------------*/
/* RSSI Updating                                                              */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalUpdateRSSI(_prGlueInfo, _ucBssIndex, _cRssi, _cLinkQuality) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void
kalUpdateRSSI(IN struct GLUE_INFO *prGlueInfo,
	      IN uint8_t ucBssIndex,
	      IN int8_t cRssi,
	      IN int8_t cLinkQuality);
#endif

/*----------------------------------------------------------------------------*/
/* I/O Buffer Pre-allocation                                                  */
/*----------------------------------------------------------------------------*/
u_int8_t kalInitIOBuffer(u_int8_t is_pre_alloc);

void kalUninitIOBuffer(void);

#ifdef CFG_REMIND_IMPLEMENT
#define kalAllocateIOBuffer(_u4AllocSize) \
((uint8_t *) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__))

#define kalReleaseIOBuffer(_pvAddr, _u4Size) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void *kalAllocateIOBuffer(IN uint32_t u4AllocSize);

void kalReleaseIOBuffer(IN void *pvAddr,
			IN uint32_t u4Size);
#endif

void
kalGetChannelList(IN struct GLUE_INFO *prGlueInfo,
		  IN enum ENUM_BAND eSpecificBand,
		  IN uint8_t ucMaxChannelNum, IN uint8_t *pucNumOfChannel,
		  IN struct RF_CHANNEL_INFO *paucChannelList);

u_int8_t kalIsAPmode(IN struct GLUE_INFO *prGlueInfo);

#if CFG_SUPPORT_802_11W
/*----------------------------------------------------------------------------*/
/* 802.11W                                                                    */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalGetMfpSetting(_prGlueInfo, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalGetRsnIeMfpCap(_prGlueInfo, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
uint32_t kalGetMfpSetting(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucBssIndex);
uint8_t kalGetRsnIeMfpCap(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucBssIndex);
#endif
#endif

/*----------------------------------------------------------------------------*/
/* file opetation                                                             */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalWriteToFile(_pucPath, _fgDoAppend, _pucData, _u4Size) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalCheckPath(_pucPath) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalTrunkPath(_pucPath) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalReadToFile(_pucPath, _pucData, _u4Size, _pu4ReadSize) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* used only under os folder */
#define kalRequestFirmware(_pucPath, _pucData, _u4Size, _pu4ReadSize, _dev) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
uint32_t kalWriteToFile(const uint8_t *pucPath,
			u_int8_t fgDoAppend,
			uint8_t *pucData, uint32_t u4Size);

uint32_t kalCheckPath(const uint8_t *pucPath);

uint32_t kalTrunkPath(const uint8_t *pucPath);

int32_t kalReadToFile(const uint8_t *pucPath,
		      uint8_t *pucData,
		      uint32_t u4Size, uint32_t *pu4ReadSize);

/* used only under os folder */
int32_t kalRequestFirmware(const uint8_t *pucPath,
			   uint8_t *pucData,
			   uint32_t u4Size, uint32_t *pu4ReadSize,
			   void *dev);
#endif
/*----------------------------------------------------------------------------*/
/* NL80211                                                                    */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalIndicateBssInfo(_prGlueInfo, _pucFrameBuf, _u4BufLen, \
	_ucChannelNum, _i4SignalStrength) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
void
kalIndicateBssInfo(IN struct GLUE_INFO *prGlueInfo,
		   IN uint8_t *pucFrameBuf, IN uint32_t u4BufLen,
		   IN uint8_t ucChannelNum, IN int32_t i4SignalStrength);
#endif
/*----------------------------------------------------------------------------*/
/* Net device                                                                 */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalHardStartXmit(_prSkb, _prDev, _prGlueInfo, _ucBssIndex) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalIsPairwiseEapolPacket(_prPacket) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalGetIPv4Address(_prDev, _u4MaxNumOfAddr, _pucIpv4Addrs, \
		_pu4NumOfIpv4Addr) \
		KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#if IS_ENABLED(CONFIG_IPV6)
#define kalGetIPv6Address(_prDev, _u4MaxNumOfAddr, _pucIpv6Addrs, \
		_pu4NumOfIpv6Addr) \
		KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#endif /* IS_ENABLED(CONFIG_IPV6) */

#define kalSetNetAddressFromInterface(_prGlueInfo, _prDev, _fgSet) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalResetStats(_prDev) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalGetStats(_prDev) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
uint32_t
kalHardStartXmit(struct sk_buff *prSkb,
		 IN struct net_device *prDev,
		 struct GLUE_INFO *prGlueInfo, uint8_t ucBssIndex);

u_int8_t kalIsPairwiseEapolPacket(IN void *prPacket);

u_int8_t
kalGetIPv4Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv4Addrs,
		  OUT uint32_t *pu4NumOfIpv4Addr);

#if IS_ENABLED(CONFIG_IPV6)
u_int8_t
kalGetIPv6Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv6Addrs,
		  OUT uint32_t *pu4NumOfIpv6Addr);
#else
static inline u_int8_t
kalGetIPv6Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv6Addrs,
		  OUT uint32_t *pu4NumOfIpv6Addr) {
	/* Not support IPv6 */
	*pu4NumOfIpv6Addr = 0;
	return 0;
}
#endif /* IS_ENABLED(CONFIG_IPV6) */

void kalSetNetAddressFromInterface(IN struct GLUE_INFO
				   *prGlueInfo,
				   IN struct net_device *prDev,
				   IN u_int8_t fgSet);

uint32_t kalResetStats(IN struct net_device *prDev);

void *kalGetStats(IN struct net_device *prDev);
#endif

void kalResetPacket(IN struct GLUE_INFO *prGlueInfo,
		    IN void *prPacket);

#if CFG_SUPPORT_QA_TOOL
#ifdef CFG_REMIND_IMPLEMENT
#define kalFileOpen(_path, _flags, _rights) \
((void *) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__))

#define kalFileClose(_file) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalFileRead(_file, _offset, _data, _size) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
struct file *kalFileOpen(const char *path, int flags,
			 int rights);

void kalFileClose(struct file *file);

uint32_t kalFileRead(struct file *file,
		     unsigned long long offset,
		     unsigned char *data, unsigned int size);
#endif
#endif

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
/*----------------------------------------------------------------------------*/
/* SDIO Read/Write Pattern Support                                            */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalSetSdioTestPattern(_prGlueInfo, _fgEn, _fgRead) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
u_int8_t kalSetSdioTestPattern(IN struct GLUE_INFO
			       *prGlueInfo,
			       IN u_int8_t fgEn, IN u_int8_t fgRead);
#endif
#endif

/*----------------------------------------------------------------------------*/
/* PNO Support                                                                */
/*----------------------------------------------------------------------------*/
#ifdef CFG_REMIND_IMPLEMENT
#define kalSchedScanResults(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalSchedScanStopped(_prGlueInfo, _fgDriverTriggerd) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)

#define kalSetFwOwnEvent2Hif(_pr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _pr)
#else
void kalSchedScanResults(IN struct GLUE_INFO *prGlueInfo);

void kalSchedScanStopped(IN struct GLUE_INFO *prGlueInfo,
			 u_int8_t fgDriverTriggerd);

void kalSetFwOwnEvent2Hif(struct GLUE_INFO *pr);
#endif

#if CFG_ASSERT_DUMP
#ifdef CFG_REMIND_IMPLEMENT
#define kalOpenCorDumpFile(_fgIsN9) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalWriteCorDumpFile(_pucBuffer, _u2Size, _fgIsN9) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalCloseCorDumpFile(_fgIsN9) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
/* Core Dump out put file */
uint32_t kalOpenCorDumpFile(u_int8_t fgIsN9);
uint32_t kalWriteCorDumpFile(uint8_t *pucBuffer,
			     uint16_t u2Size,
			     u_int8_t fgIsN9);
uint32_t kalCloseCorDumpFile(u_int8_t fgIsN9);
#endif
#endif
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#if CFG_WOW_SUPPORT
#ifdef CFG_REMIND_IMPLEMENT
#define kalWowInit(_glueinfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalWowProcess(_glueinfo, _enable) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void kalWowInit(IN struct GLUE_INFO *prGlueInfo);
void kalWowProcess(IN struct GLUE_INFO *prGlueInfo,
		   uint8_t enable);
#endif
#endif

int main_thread(void *data);

#if CFG_SUPPORT_MULTITHREAD
int hif_thread(void *data);
int rx_thread(void *data);
#endif

#ifdef CFG_REMIND_IMPLEMENT
#define kalGetBootTime() KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
uint64_t kalGetBootTime(void);
#endif

int kalMetInitProcfs(IN struct GLUE_INFO *prGlueInfo);
int kalMetRemoveProcfs(void);

uint8_t kalGetEapolKeyType(void *prPacket);

#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
#ifdef CFG_REMIND_IMPLEMENT
#define kalIsWakeupByWlan(_prAdapter) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
u_int8_t kalIsWakeupByWlan(struct ADAPTER *prAdapter);
#endif
#endif

int32_t kalHaltLock(uint32_t waitMs);
int32_t kalHaltTryLock(void);
void kalHaltUnlock(void);
void kalSetHalted(u_int8_t fgHalt);
u_int8_t kalIsHalted(void);

#ifdef CFG_REMIND_IMPLEMENT
#define kalFreeTxMsduWorker(_work) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void kalFreeTxMsduWorker(struct work_struct *work);
#endif
void kalFreeTxMsdu(struct ADAPTER *prAdapter,
		   struct MSDU_INFO *prMsduInfo);

int32_t kalPerMonInit(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonDisable(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonEnable(IN struct GLUE_INFO *prGlueInfo);
#ifdef CFG_REMIND_IMPLEMENT
#define kalPerMonStart(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _prGlueInfo)
#else
int32_t kalPerMonStart(IN struct GLUE_INFO *prGlueInfo);
#endif
int32_t kalPerMonStop(IN struct GLUE_INFO *prGlueInfo);
int32_t kalPerMonDestroy(IN struct GLUE_INFO *prGlueInfo);
void kalPerMonHandler(IN struct ADAPTER *prAdapter,
		      unsigned long ulParam);
uint32_t kalPerMonGetInfo(IN struct ADAPTER *prAdapter,
			  IN uint8_t *pucBuf,
			  IN uint32_t u4Max);
int32_t kalBoostCpu(IN struct ADAPTER *prAdapter,
		    IN uint32_t u4TarPerfLevel,
		    IN uint32_t u4BoostCpuTh);
int32_t kalCheckTputLoad(IN struct ADAPTER *prAdapter,
			 IN uint32_t u4CurrPerfLevel,
			 IN uint32_t u4TarPerfLevel,
			 IN int32_t i4Pending,
			 IN uint32_t u4Used);
void kalSetRpsMap(IN struct GLUE_INFO *glue, IN unsigned long value);
#if CFG_MTK_ANDROID_EMI
void kalSetEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
			    uint32_t size, bool enable);
void kalSetDrvEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
			       uint32_t size);
#endif
int32_t kalSetCpuNumFreq(uint32_t u4CoreNum,
			 uint32_t u4Freq);
int32_t kalPerMonSetForceEnableFlag(uint8_t uFlag);
int32_t kalFbNotifierReg(IN struct GLUE_INFO *prGlueInfo);
void kalFbNotifierUnReg(void);

#ifdef CFG_REMIND_IMPLEMENT
#define kalInitDevWakeup(_prAdapter, _prDev) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void kalInitDevWakeup(struct ADAPTER *prAdapter, struct device *prDev);
#endif

#ifdef CFG_REMIND_IMPLEMENT
#define kalIsValidMacAddr(_addr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalScanParseRandomMac(_prdev, prRrequest, _pucRandomMac) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalSchedScanParseRandomMac(_prdev, prRrequest, _pucRandomMac \
	, _pucRandomMacMask) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalScanReqLog(prRrequest) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define kalScanResultLog(_prAdapter, _prMgmt) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
u_int8_t kalIsValidMacAddr(const uint8_t *addr);

u_int8_t kalScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_scan_request *request, uint8_t *pucRandomMac);

u_int8_t kalSchedScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_sched_scan_request *request,
	uint8_t *pucRandomMac, uint8_t *pucRandomMacMask);

void kalScanReqLog(struct cfg80211_scan_request *request);
void kalScanResultLog(struct ADAPTER *prAdapter, struct ieee80211_mgmt *mgmt);
#endif
void kalScanLogCacheFlushBSS(struct ADAPTER *prAdapter,
	const uint16_t logBufLen);

#ifdef CFG_REMIND_IMPLEMENT
#define kalMaskMemCmp(_cs, _ct, _mask, _count) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalChannelScoSwitch(_channel_type, _prChnlSco) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalChannelFormatSwitch(_prChannel_def, _prChannel \
		, _prRfChnlInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalRemoveBss(_prGlueInfo, \
	_aucBSSID, _ucChannelNum, \
	_eBand) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define kalIsResetting(_void) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
int kalMaskMemCmp(const void *cs, const void *ct,
	const void *mask, size_t count);

u_int8_t
kalChannelScoSwitch(IN enum nl80211_channel_type channel_type,
		IN enum ENUM_CHNL_EXT *prChnlSco);

u_int8_t
kalChannelFormatSwitch(IN struct cfg80211_chan_def *channel_def,
		IN struct ieee80211_channel *channel,
		IN struct RF_CHANNEL_INFO *prRfChnlInfo);

void kalRemoveBss(struct GLUE_INFO *prGlueInfo,
	uint8_t aucBSSID[],
	uint8_t ucChannelNum,
	enum ENUM_BAND eBand);

u_int8_t kalIsResetting(void);
#endif

#if CFG_CHIP_RESET_SUPPORT
void kalRemoveProbe(IN struct GLUE_INFO *prGlueInfo);
#endif

#endif /* _GL_KAL_H */

