/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _GL_KAL_H
#define _GL_KAL_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"
#include "gl_typedef.h"
#include "gl_os.h"
#include "link.h"
#include "nic/mac.h"
#include "nic/wlan_def.h"
#include "wlan_lib.h"
#include "wlan_oid.h"
#include "gl_wext_priv.h"
#include <asm/div64.h>

#if CFG_ENABLE_BT_OVER_WIFI
#include "nic/bow.h"
#endif

#if KERNEL_VERSION(4, 11, 0) <= CFG80211_VERSION_CODE
#include "linux/sched/types.h"
#endif

#if DBG
extern int allocatedMemSize;
#endif

#if CFG_SUPPORT_MET_PROFILING
#include "linux/kallsyms.h"
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#include <linux/trace_events.h>
#else
#include <linux/ftrace_event.h>
#endif
#endif

extern BOOLEAN fgIsUnderSuspend;
extern UINT_32 TaskIsrCnt;
extern BOOLEAN fgIsResetting;
extern int wlanHardStartXmit(struct sk_buff *prSkb, struct net_device *prDev);
extern UINT_32 u4MemAllocCnt, u4MemFreeCnt;


extern struct delayed_work sched_workq;

#if defined(MT6620) && CFG_MULTI_ECOVER_SUPPORT
extern ENUM_WMTHWVER_TYPE_T mtk_wcn_wmt_hwver_get(VOID);
#endif
extern PUINT8 wmt_lib_get_fwinfor_from_emi(UINT8 section, UINT32 offset, PUINT8 buff, UINT32 len);
extern PUINT8 mtk_wcn_consys_emi_virt_addr_get(UINT32 ctrl_state_offset);

extern BOOLEAN fgIsUnderSuspend;

/* for built-in WMT */
extern void connectivity_export_show_stack(struct task_struct *tsk, unsigned long *sp);
extern void connectivity_export_tracing_record_cmdline(struct task_struct *tsk);
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* #define USEC_PER_MSEC   (1000) */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_SPIN_LOCK_CATEGORY_E {
	SPIN_LOCK_FSM = 0,

	/* FIX ME */
	SPIN_LOCK_RX_QUE,
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
	SPIN_LOCK_TX,
	SPIN_LOCK_IO_REQ,
	SPIN_LOCK_INT,

	SPIN_LOCK_MGT_BUF,
	SPIN_LOCK_MSG_BUF,
	SPIN_LOCK_STA_REC,

	SPIN_LOCK_MAILBOX,
	SPIN_LOCK_TIMER,

	SPIN_LOCK_BOW_TABLE,

	SPIN_LOCK_EHPI_BUS,	/* only for EHPI */
	SPIN_LOCK_NET_DEV,

#if CFG_SUPPORT_MULTITHREAD
	SPIN_LOCK_RX_DATA_QUE,
#endif
	SPIN_LOCK_NUM
} ENUM_SPIN_LOCK_CATEGORY_E;

/* event for assoc information update */
typedef struct _EVENT_ASSOC_INFO {
	UINT_8 ucAssocReq;	/* 1 for assoc req, 0 for assoc rsp */
	UINT_8 ucReassoc;	/* 0 for assoc, 1 for reassoc */
	UINT_16 u2Length;
	PUINT_8 pucIe;
} EVENT_ASSOC_INFO, *P_EVENT_ASSOC_INFO;

typedef enum _ENUM_KAL_NETWORK_TYPE_INDEX_T {
	KAL_NETWORK_TYPE_AIS_INDEX = 0,
#if CFG_ENABLE_WIFI_DIRECT
	KAL_NETWORK_TYPE_P2P_INDEX,
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	KAL_NETWORK_TYPE_BOW_INDEX,
#endif
	KAL_NETWORK_TYPE_INDEX_NUM
} ENUM_KAL_NETWORK_TYPE_INDEX_T;

typedef enum _ENUM_PKT_TYPE_T {
	ENUM_PKT_802_11,	/* 802.11 or non-802.11 */
	ENUM_PKT_802_3,		/* 802.3 or ethernetII */
	ENUM_PKT_1X,		/* 1x frame or not */
	ENUM_PKT_PROTECTED_1X,	/* prtected 1x frame */
	ENUM_PKT_WPI_1X,	/* WAPI */
	ENUM_PKT_VLAN_EXIST,	/* VLAN tag exist */
	ENUM_PKT_DHCP,		/* DHCP frame */
	ENUM_PKT_ARP,		/* ARP */
	ENUM_PKT_ICMP,		/* ICMP */
	ENUM_PKT_TDLS,		/* TDLS */
	ENUM_PKT_DNS,		/* DNS */

	ENUM_PKT_FLAG_NUM
} ENUM_PKT_TYPE_T;


typedef enum _ENUM_AMPDU_TYPE_E {
	MTK_AMPDU_TX_DESC,
	MTK_AMPDU_RX_DESC,
	MTK_AMPDU_NUM
} ENUM_AMPDU_TYPE;

typedef enum _ENUM_KAL_MEM_ALLOCATION_TYPE_E {
	PHY_MEM_TYPE,		/* physically continuous */
	VIR_MEM_TYPE,		/* virtually continuous */
	MEM_TYPE_NUM
} ENUM_KAL_MEM_ALLOCATION_TYPE;

enum ENUM_BUILD_VARIANT_E {
	MTK_BUILD_VAR_ENG = 1,		/*eng load*/
	MTK_BUILD_VAR_USERDEUB = 2,	/*userdebug load*/
	MTK_BUILD_VAR_USER = 3	/*user load*/
};

#if CONFIG_ANDROID		/* Defined in Android kernel source */
#if (KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE)
typedef struct wakeup_source KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;
#else
typedef struct wake_lock KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;
#endif
#else
typedef UINT_32 KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;
#endif

#if CFG_SUPPORT_AGPS_ASSIST
typedef enum _ENUM_MTK_AGPS_ATTR {
	MTK_ATTR_AGPS_INVALID,
	MTK_ATTR_AGPS_CMD,
	MTK_ATTR_AGPS_DATA,
	MTK_ATTR_AGPS_IFINDEX,
	MTK_ATTR_AGPS_IFNAME,
	MTK_ATTR_AGPS_MAX
} ENUM_MTK_CCX_ATTR;

typedef enum _ENUM_AGPS_EVENT {
	AGPS_EVENT_WLAN_ON,
	AGPS_EVENT_WLAN_OFF,
	AGPS_EVENT_WLAN_AP_LIST,
	WIFI_EVENT_CHIP_RESET,
} ENUM_CCX_EVENT;
BOOLEAN kalIndicateAgpsNotify(P_ADAPTER_T prAdapter, UINT_8 cmd, PUINT_8 data, UINT_16 dataLen);
#endif

struct KAL_HALT_CTRL_T {
	struct semaphore lock;
	struct task_struct *owner;
	BOOLEAN fgHalt;
	BOOLEAN fgHeldByKalIoctl;
	OS_SYSTIME u4HoldStart;
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

/*----------------------------------------------------------------------------*/
/* Macros of bit operation                                                    */
/*----------------------------------------------------------------------------*/
#define KAL_SET_BIT(bitOffset, value)             set_bit(bitOffset, &value)
#define KAL_CLR_BIT(bitOffset, value)             clear_bit(bitOffset, &value)
#define KAL_TEST_AND_CLEAR_BIT(bitOffset, value)  test_and_clear_bit(bitOffset, &value)
#define KAL_TEST_BIT(bitOffset, value)            test_bit(bitOffset, &value)

/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_SPIN_LOCK_DECLARATION()             unsigned long __u4Flags

#define KAL_ACQUIRE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	    kalAcquireSpinLock(((P_ADAPTER_T)_prAdapter)->prGlueInfo, _rLockCategory, &__u4Flags)

#define KAL_RELEASE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	    kalReleaseSpinLock(((P_ADAPTER_T)_prAdapter)->prGlueInfo, _rLockCategory, __u4Flags)

/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
#define KAL_GET_PKT_QUEUE_ENTRY(_p)             GLUE_GET_PKT_QUEUE_ENTRY(_p)
#define KAL_GET_PKT_DESCRIPTOR(_prQueueEntry)   GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)
#define KAL_GET_PKT_TID(_p)                     GLUE_GET_PKT_TID(_p)
#define KAL_GET_PKT_IS1X(_p)                    GLUE_GET_PKT_IS1X(_p)
#define KAL_GET_PKT_HEADER_LEN(_p)              GLUE_GET_PKT_HEADER_LEN(_p)
#define KAL_GET_PKT_PAYLOAD_LEN(_p)             GLUE_GET_PKT_PAYLOAD_LEN(_p)
#define KAL_GET_PKT_ARRIVAL_TIME(_p)            GLUE_GET_PKT_ARRIVAL_TIME(_p)

/*----------------------------------------------------------------------------*/
/* Macros of wake_lock operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#if CONFIG_ANDROID		/* Defined in Android kernel source */
#if (KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE)
#if (KERNEL_VERSION(4, 14, 149) <= LINUX_VERSION_CODE)
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName) \
	_prWakeLock = wakeup_source_register(NULL, _pcName);
#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock) \
{ \
	wakeup_source_unregister(_prWakeLock); \
	_prWakeLock = NULL; \
}
#else
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName) \
{ \
	_prWakeLock = kalMemAlloc(sizeof(KAL_WAKE_LOCK_T), \
	VIR_MEM_TYPE); \
	if (!_prWakeLock) { \
		DBGLOG(HAL, ERROR, \
		"KAL_WAKE_LOCK_INIT init fail!\n"); \
		} \
	else { \
		wakeup_source_init(_prWakeLock, _pcName); \
	} \
}
#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock) \
{ \
	if (!_prWakeLock) { \
		wakeup_source_trash(_prWakeLock); \
		_prWakeLock = NULL; \
	} \
}
#endif
#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock) \
	__pm_stay_awake(_prWakeLock)
#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout) \
	__pm_wakeup_event(_prWakeLock, JIFFIES_TO_MSEC(_u4Timeout))
#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock) \
	__pm_relax(_prWakeLock)
#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock) \
	((_prWakeLock)->active)
#else
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName) \
	wake_lock_init(_prWakeLock, WAKE_LOCK_SUSPEND, _pcName)

#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock) \
	wake_lock_destroy(_prWakeLock)

#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock) \
	wake_lock(_prWakeLock)

#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout) \
	wake_lock_timeout(_prWakeLock, _u4Timeout)

#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock) \
	wake_unlock(_prWakeLock)

#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock) \
	wake_lock_active(_prWakeLock)
	typedef struct wake_lock KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;
#define KAL_WAKELOCK_DECLARE(_lock) \
	struct wake_lock _lock
#endif

#else
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName)
#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout)
#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock)
typedef UINT_32 KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;
#define KAL_WAKELOCK_DECLARE(_lock)
#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock)
#endif

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
#if DBG
#define kalMemAlloc(u4Size, eMemType) ({ \
	void *pvAddr; \
	if (eMemType == PHY_MEM_TYPE) { \
		pvAddr = kmalloc(u4Size, GFP_KERNEL); \
	} \
	else { \
		if (u4Size > PAGE_SIZE) \
			pvAddr = vmalloc(u4Size); \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL); \
	} \
	if (pvAddr) {   \
		allocatedMemSize += u4Size;   \
		DBGLOG(INIT, INFO, "%p(%u) allocated (%s:%s)\n", \
			pvAddr, (UINT_32)u4Size, __FILE__, __func__); \
	} \
	pvAddr; \
})
#else
#define kalMemAlloc(u4Size, eMemType) ({ \
	void *pvAddr; \
	if (eMemType == PHY_MEM_TYPE) { \
		pvAddr = kmalloc(u4Size, GFP_KERNEL); \
	} \
	else { \
		if (u4Size > PAGE_SIZE) \
			pvAddr = vmalloc(u4Size); \
		else \
			pvAddr = kmalloc(u4Size, GFP_KERNEL); \
	} \
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
#define kalMemFree(pvAddr, eMemType, u4Size) \
{   \
	if (pvAddr) { \
		allocatedMemSize -= u4Size; \
		DBGLOG(INIT, INFO, "%p(%u) freed (%s:%s)\n", \
			pvAddr, (UINT_32)u4Size, __FILE__, __func__);  \
	} \
	kvfree(pvAddr); \
}
#else
#define kalMemFree(pvAddr, eMemType, u4Size) \
{   \
	kvfree(pvAddr); \
}
#endif

#define kalUdelay(u4USec)                           udelay(u4USec)

#define kalMdelay(u4MSec)                           mdelay(u4MSec)
#define kalMsleep(u4MSec)                           msleep(u4MSec)
#define kalUsleepRange(u4StarUSec, u4EndUSec)        usleep_range(u4StarUSec, u4EndUSec)

/* Copy memory from user space to kernel space */
#define kalMemCopyFromUser(_pvTo, _pvFrom, _u4N)    copy_from_user(_pvTo, _pvFrom, _u4N)

/* Copy memory from kernel space to user space */
#define kalMemCopyToUser(_pvTo, _pvFrom, _u4N)      copy_to_user(_pvTo, _pvFrom, _u4N)

/* Copy memory block with specific size */
#define kalMemCopy(pvDst, pvSrc, u4Size)            memcpy(pvDst, pvSrc, u4Size)

/* Set memory block with specific pattern */
#define kalMemSet(pvAddr, ucPattern, u4Size)        memset(pvAddr, ucPattern, u4Size)

/* Compare two memory block with specific length.
 * Return zero if they are the same.
 */
#define kalMemCmp(pvAddr1, pvAddr2, u4Size)         memcmp(pvAddr1, pvAddr2, u4Size)

/* Zero specific memory block */
#define kalMemZero(pvAddr, u4Size)                  memset(pvAddr, 0, u4Size)

/* string operation */
#define kalStrCpy(dest, src)                         strcpy(dest, src)
#define kalStrnCpy(dest, src, n)                      strncpy(dest, src, n)
#define kalStrCmp(ct, cs)                            strcmp(ct, cs)
#define kalStrnCmp(ct, cs, n)                         strncmp(ct, cs, n)
#define kalStrChr(s, c)                              strchr(s, c)
#define kalStrrChr(s, c)                             strrchr(s, c)
#define kalStrnChr(s, n, c)                           strnchr(s, n, c)
#define kalStrLen(s)                                strlen(s)
#define kalStrnLen(s, b)                             strnlen(s, b)
#define kalStrniCmp(s1, s2, n)                          strncasecmp(s1, s2, n)
/*
 * #define kalStrtoul(cp, endp, base)                    simple_strtoul(cp, endp, base)
 * #define kalStrtol(cp, endp, base)                     simple_strtol(cp, endp, base)
 */
#define kalkStrtou32(cp, base, resp)                   kstrtou32(cp, base, resp)
#define kalkStrtos32(cp, base, resp)                    kstrtos32(cp, base, resp)
#define kalSnprintf(buf, size, fmt, ...)              snprintf(buf, size, fmt, __VA_ARGS__)
#define kalSprintf(buf, fmt, ...)                     sprintf(buf, fmt, __VA_ARGS__)
/* remove for AOSP */
/* #define kalSScanf(buf, fmt, ...)                      sscanf(buf, fmt, __VA_ARGS__) */
#define kalStrStr(ct, cs)                            strstr(ct, cs)
#define kalStrSep(s, ct)                            strsep(s, ct)
#define kalStrCat(dest, src)                         strcat(dest, src)

/* defined for wince sdio driver only */
#if defined(_HIF_SDIO)
#define kalDevSetPowerState(prGlueInfo, ePowerMode) glSetPowerState(prGlueInfo, ePowerMode)
#else
#define kalDevSetPowerState(prGlueInfo, ePowerMode)
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Notify OS with SendComplete event of the specific packet. Linux should
*        free packets here.
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
* \brief This function is used to locate the starting address of incoming ethernet
*        frame for skb.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
*
* \return starting address of ethernet frame buffer.
*/
/*----------------------------------------------------------------------------*/
#define kalQueryBufferPointer(prGlueInfo, pvPacket)     \
	    ((PUINT_8)((struct sk_buff *)pvPacket)->data)

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to query the length of valid buffer which is accessible during
*         port read/write.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
*
* \return starting address of ethernet frame buffer.
*/
/*----------------------------------------------------------------------------*/
#define kalQueryValidBufferLength(prGlueInfo, pvPacket)     \
	    ((UINT_32)((struct sk_buff *)pvPacket)->end -  \
	     (UINT_32)((struct sk_buff *)pvPacket)->data)

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to copy the entire frame from skb to the destination
*        address in the input parameter.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
* \param[in] pucDestBuffer  Destination Address
*
* \return -
*/
/*----------------------------------------------------------------------------*/
#define kalCopyFrame(prGlueInfo, pvPacket, pucDestBuffer)   \
	do {struct sk_buff *skb = (struct sk_buff *)pvPacket; \
	memcpy(pucDestBuffer, skb->data, skb->len); } while (0)

#define kalGetTimeTick()                            jiffies_to_msecs(jiffies)

#define WLAN_TAG                                    "[wlan]"
#define kalPrint(_Fmt...)                           pr_info(WLAN_TAG _Fmt)
#define kalPrintLimited(_Fmt...)                    pr_info_ratelimited(WLAN_TAG _Fmt)

#define kalBreakPoint() \
do { \
	WARN_ON(1); \
	panic("Oops"); \
} while (0)

#if CFG_ENABLE_AEE_MSG
#define kalSendAeeException                         aee_kernel_exception
#define kalSendAeeWarning                           aee_kernel_warning
#define kalSendAeeReminding                         aee_kernel_reminding
#else
#define kalSendAeeException(_module, _desc, ...)
#define kalSendAeeWarning(_module, _desc, ...)
#define kalSendAeeReminding(_module, _desc, ...)
#endif

#define PRINTF_ARG(...)                             __VA_ARGS__
#define SPRINTF(buf, arg)                           {buf += sprintf((char *)(buf), PRINTF_ARG arg); }

#define USEC_TO_SYSTIME(_usec)      ((_usec) / USEC_PER_MSEC)
#define MSEC_TO_SYSTIME(_msec)      (_msec)

#define MSEC_TO_JIFFIES(_msec)      msecs_to_jiffies(_msec)
#define JIFFIES_TO_MSEC(_jiffie)    jiffies_to_msecs(_jiffie)

#define KAL_HALT_LOCK_TIMEOUT_NORMAL_CASE		3000 /* 3s */

/******************************************************************
 *
 ******************************************************************/
#if KERNEL_VERSION(4, 19, 0) > CFG80211_VERSION_CODE
#define KERNEL_event_trace_printk(ip, fmt, args...)               \
do {                                                       \
    __trace_printk_check_format(fmt, ##args);              \
    connectivity_export_tracing_record_cmdline(current);                \
    if (__builtin_constant_p(fmt)) {                       \
        static const char *trace_printk_fmt                \
          __attribute__((section("__trace_printk_fmt"))) = \
            __builtin_constant_p(fmt) ? fmt : NULL;        \
                                                           \
        __trace_bprintk(ip, trace_printk_fmt, ##args);     \
     } else                                                \
        __trace_printk(ip, fmt, ##args);                   \
} while (0)
#endif
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Routines in gl_kal.c                                                       */
/*----------------------------------------------------------------------------*/
VOID
kalAcquireSpinLock(IN P_GLUE_INFO_T prGlueInfo,
		   IN ENUM_SPIN_LOCK_CATEGORY_E rLockCategory, OUT unsigned long *pu4Flags);

VOID kalReleaseSpinLock(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_SPIN_LOCK_CATEGORY_E rLockCategory, IN UINT_32 u4Flags);

VOID kalUpdateMACAddress(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucMacAddr);

VOID kalPacketFree(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket);

PVOID kalPacketAlloc(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Size, OUT PPUINT_8 ppucData);

VOID kalOsTimerInitialize(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prTimerHandler);

BOOLEAN kalSetTimer(IN P_GLUE_INFO_T prGlueInfo, IN OS_SYSTIME rInterval);

WLAN_STATUS
kalProcessRxPacket(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN PUINT_8 pucPacketStart, IN UINT_32 u4PacketLen,
		   /* IN PBOOLEAN           pfgIsRetain, */
		   IN BOOLEAN fgIsRetain, IN ENUM_CSUM_RESULT_T aeCSUM[]
);

WLAN_STATUS kalRxIndicatePkts(IN P_GLUE_INFO_T prGlueInfo, IN PVOID apvPkts[], IN UINT_8 ucPktNum);

VOID
kalIndicateStatusAndComplete(IN P_GLUE_INFO_T prGlueInfo, IN WLAN_STATUS eStatus, IN PVOID pvBuf, IN UINT_32 u4BufLen);

VOID
kalUpdateReAssocReqInfo(IN P_GLUE_INFO_T prGlueInfo,
			IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest);

VOID kalUpdateReAssocRspInfo(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen);

#if CFG_TX_FRAGMENT
BOOLEAN
kalQueryTxPacketHeader(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID pvPacket, OUT PUINT_16 pu2EtherTypeLen, OUT PUINT_8 pucEthDestAddr);
#endif /* CFG_TX_FRAGMENT */

VOID kalSendCompleteAndAwakeQueue(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
VOID kalQueryTxChksumOffloadParam(IN PVOID pvPacket, OUT PUINT_8 pucFlag);

VOID kalUpdateRxCSUMOffloadParam(IN PVOID pvPacket, IN ENUM_CSUM_RESULT_T eCSUM[]);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

BOOLEAN kalRetrieveNetworkAddress(IN P_GLUE_INFO_T prGlueInfo, PARAM_MAC_ADDRESS *prMacAddr);

VOID
kalReadyOnChannel(IN P_GLUE_INFO_T prGlueInfo,
		  IN UINT_64 u8Cookie,
		  IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum, IN UINT_32 u4DurationMs);

VOID
kalRemainOnChannelExpired(IN P_GLUE_INFO_T prGlueInfo,
			  IN UINT_64 u8Cookie, IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum);

VOID
kalIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo,
			IN UINT_64 u8Cookie, IN BOOLEAN fgIsAck, IN PUINT_8 pucFrameBuf, IN UINT_32 u4FrameLen);

VOID kalIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo, IN P_SW_RFB_T prSwRfb);

/*----------------------------------------------------------------------------*/
/* Routines in interface - ehpi/sdio.c                                                       */
/*----------------------------------------------------------------------------*/
BOOLEAN kalDevRegRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value);

BOOLEAN kalDevRegWrite(P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value);

BOOLEAN
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo,
	       IN UINT_16 u2Port, IN UINT_32 u2Len, OUT PUINT_8 pucBuf, IN UINT_32 u2ValidOutBufSize);

BOOLEAN
kalDevPortWrite(P_GLUE_INFO_T prGlueInfo,
		IN UINT_16 u2Port, IN UINT_32 u2Len, IN PUINT_8 pucBuf, IN UINT_32 u2ValidInBufSize);

BOOLEAN kalDevWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Addr, IN UINT_8 ucData);

void kalDevLoopbkAuto(IN P_GLUE_INFO_T GlueInfo);

#if CFG_SUPPORT_EXT_CONFIG
UINT_32 kalReadExtCfg(IN P_GLUE_INFO_T prGlueInfo);
#endif

UINT_8 kalGetPktEtherType(IN PUINT_8 pucPkt);

BOOLEAN
kalQoSFrameClassifierAndPacketInfo(IN P_GLUE_INFO_T prGlueInfo,
				   IN P_NATIVE_PACKET prPacket,
				   OUT PUINT_8 pucPriorityParam,
				   OUT PUINT_32 pu4PacketLen,
				   OUT PUINT_8 pucEthDestAddr,
				   OUT PBOOLEAN pfgIs1X,
				   OUT PBOOLEAN pfgIsPAL, OUT PUINT_8 pucNetworkType,
				   OUT PVOID prGenUse);

VOID
kalOidComplete(IN P_GLUE_INFO_T prGlueInfo,
	       IN BOOLEAN fgSetQuery, IN UINT_32 u4SetQueryInfoLen, IN WLAN_STATUS rOidStatus);

WLAN_STATUS
kalIoctl(IN P_GLUE_INFO_T prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN PVOID pvInfoBuf,
	 IN UINT_32 u4InfoBufLen,
	 IN BOOLEAN fgRead, IN BOOLEAN fgWaitResp, IN BOOLEAN fgCmd, IN BOOLEAN fgIsP2pOid, OUT PUINT_32 pu4QryInfoLen);

VOID kalHandleAssocInfo(IN P_GLUE_INFO_T prGlueInfo, IN P_EVENT_ASSOC_INFO prAssocInfo);

#if CFG_ENABLE_FW_DOWNLOAD

PVOID kalFirmwareImageMapping(IN P_GLUE_INFO_T prGlueInfo, OUT PPVOID ppvMapFileBuf, OUT PUINT_32 pu4FileLength);

VOID kalFirmwareImageUnmapping(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prFwHandle, IN PVOID pvMapFileBuf);
#endif

/*----------------------------------------------------------------------------*/
/* Card Removal Check                                                         */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsCardRemoved(IN P_GLUE_INFO_T prGlueInfo);

/*----------------------------------------------------------------------------*/
/* TX                                                                         */
/*----------------------------------------------------------------------------*/
VOID kalFlushPendingTxPackets(IN P_GLUE_INFO_T prGlueInfo);

/*----------------------------------------------------------------------------*/
/* Media State Indication                                                     */
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalGetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo);

VOID kalSetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicate);

/*----------------------------------------------------------------------------*/
/* OID handling                                                               */
/*----------------------------------------------------------------------------*/
VOID kalOidCmdClearance(IN P_GLUE_INFO_T prGlueInfo);

VOID kalOidClearance(IN P_GLUE_INFO_T prGlueInfo);

VOID kalEnqueueCommand(IN P_GLUE_INFO_T prGlueInfo, IN P_QUE_ENTRY_T prQueueEntry);

#if CFG_ENABLE_BT_OVER_WIFI
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi handling                                              */
/*----------------------------------------------------------------------------*/
VOID kalIndicateBOWEvent(IN P_GLUE_INFO_T prGlueInfo, IN P_AMPC_EVENT prEvent);

ENUM_BOW_DEVICE_STATE kalGetBowState(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr);

BOOLEAN kalSetBowState(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_BOW_DEVICE_STATE eBowState, PARAM_MAC_ADDRESS rPeerAddr);

ENUM_BOW_DEVICE_STATE kalGetBowGlobalState(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetBowFreqInKHz(IN P_GLUE_INFO_T prGlueInfo);

UINT_8 kalGetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr);

VOID kalSetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRole, IN PARAM_MAC_ADDRESS rPeerAddr);

UINT_8 kalGetBowAvailablePhysicalLinkCount(IN P_GLUE_INFO_T prGlueInfo);

#if CFG_BOW_SEPARATE_DATA_PATH
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi Net Device Init/Uninit                                */
/*----------------------------------------------------------------------------*/
BOOLEAN kalInitBowDevice(IN P_GLUE_INFO_T prGlueInfo, IN const char *prDevName);

BOOLEAN kalUninitBowDevice(IN P_GLUE_INFO_T prGlueInfo);
#endif /* CFG_BOW_SEPARATE_DATA_PATH */
#endif /* CFG_ENABLE_BT_OVER_WIFI */

/*----------------------------------------------------------------------------*/
/* Firmware Download Handling                                                 */
/*----------------------------------------------------------------------------*/
UINT_32 kalGetFwStartAddress(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetFwLoadAddress(IN P_GLUE_INFO_T prGlueInfo);

/*----------------------------------------------------------------------------*/
/* Security Frame Clearance                                                   */
/*----------------------------------------------------------------------------*/
VOID kalClearSecurityFrames(IN P_GLUE_INFO_T prGlueInfo);

VOID kalClearSecurityFramesByNetType(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_NETWORK_TYPE_INDEX_T eNetworkTypeIdx);

VOID kalSecurityFrameSendComplete(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN WLAN_STATUS rStatus);

/*----------------------------------------------------------------------------*/
/* Management Frame Clearance                                                 */
/*----------------------------------------------------------------------------*/
VOID kalClearMgmtFrames(IN P_GLUE_INFO_T prGlueInfo);

VOID kalClearMgmtFramesByNetType(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_NETWORK_TYPE_INDEX_T eNetworkTypeIdx);

UINT_32 kalGetTxPendingFrameCount(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetTxPendingCmdCount(IN P_GLUE_INFO_T prGlueInfo);

BOOLEAN kalSetTimer(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Interval);

BOOLEAN kalCancelTimer(IN P_GLUE_INFO_T prGlueInfo);

VOID kalScanDone(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN WLAN_STATUS status);

UINT_32 kalRandomNumber(VOID);

#if KERNEL_VERSION(4, 15, 0) <= CFG80211_VERSION_CODE
VOID kalTimeoutHandler(struct timer_list *timer);
#else
VOID kalTimeoutHandler(ULONG arg);
#endif

VOID kalSetEvent(P_GLUE_INFO_T pr);

/*----------------------------------------------------------------------------*/
/* NVRAM/Registry Service                                                     */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsConfigurationExist(IN P_GLUE_INFO_T prGlueInfo);

P_REG_INFO_T kalGetConfiguration(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalGetConfigurationVersion(IN P_GLUE_INFO_T prGlueInfo,
			   OUT PUINT_16 pu2Part1CfgOwnVersion,
			   OUT PUINT_16 pu2Part1CfgPeerVersion,
			   OUT PUINT_16 pu2Part2CfgOwnVersion, OUT PUINT_16 pu2Part2CfgPeerVersion);

BOOLEAN kalCfgDataRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset,
							IN UINT_32 u4Len, OUT PUINT_16 pu2Data);

BOOLEAN kalCfgDataRead16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, OUT PUINT_16 pu2Data);

BOOLEAN kalCfgDataWrite16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, IN UINT_16 u2Data);

/*----------------------------------------------------------------------------*/
/* WSC Connection                                                     */
/*----------------------------------------------------------------------------*/
BOOLEAN kalWSCGetActiveState(IN P_GLUE_INFO_T prGlueInfo);

/*----------------------------------------------------------------------------*/
/* RSSI Updating                                                              */
/*----------------------------------------------------------------------------*/
VOID
kalUpdateRSSI(IN P_GLUE_INFO_T prGlueInfo,
	      IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN INT_8 cRssi, IN INT_8 cLinkQuality);

/*----------------------------------------------------------------------------*/
/* I/O Buffer Pre-allocation                                                  */
/*----------------------------------------------------------------------------*/
BOOLEAN kalInitIOBuffer(VOID);

VOID kalUninitIOBuffer(VOID);

PVOID kalAllocateIOBuffer(IN UINT_32 u4AllocSize);

VOID kalReleaseIOBuffer(IN PVOID pvAddr, IN UINT_32 u4Size);

VOID
kalGetChannelList(IN P_GLUE_INFO_T prGlueInfo,
		  IN ENUM_BAND_T eSpecificBand,
		  IN UINT_8 ucMaxChannelNum, IN PUINT_8 pucNumOfChannel, IN P_RF_CHANNEL_INFO_T paucChannelList);

BOOLEAN kalIsAPmode(IN P_GLUE_INFO_T prGlueInfo);

ULONG kalIOPhyAddrGet(IN ULONG VirtAddr);

VOID kalDmaBufGet(IN VOID *dev, OUT VOID **VirtAddr, OUT VOID **PhyAddr);

#if CFG_SUPPORT_802_11W
/*----------------------------------------------------------------------------*/
/* 802.11W                                                                    */
/*----------------------------------------------------------------------------*/
UINT_32 kalGetMfpSetting(IN P_GLUE_INFO_T prGlueInfo);
UINT_8 kalGetRsnIeMfpCap(IN P_GLUE_INFO_T prGlueInfo);
#endif

UINT_32 kalWriteToFile(const PUINT_8 pucPath, BOOLEAN fgDoAppend, PUINT_8 pucData, UINT_32 u4Size);

/*----------------------------------------------------------------------------*/
/* NL80211                                                                    */
/*----------------------------------------------------------------------------*/
VOID
kalIndicateBssInfo(IN P_GLUE_INFO_T prGlueInfo,
		   IN PUINT_8 pucFrameBuf, IN UINT_32 u4BufLen, IN UINT_8 ucChannelNum, IN INT_32 i4SignalStrength);

void kalCfg80211ScanDone(struct cfg80211_scan_request *request, bool aborted);

/*----------------------------------------------------------------------------*/
/* PNO Support                                                                */
/*----------------------------------------------------------------------------*/
VOID kalSchedScanResults(IN P_GLUE_INFO_T prGlueInfo);

VOID kalSchedScanStopped(IN P_GLUE_INFO_T prGlueInfo);

#if CFG_SUPPORT_EMI_DEBUG
/*----------------------------------------------------------------------------*/
/* WMT Support                                                                 */
/*----------------------------------------------------------------------------*/
PINT8 kalGetFwInfoFormEmi(UINT8 section, UINT32 offset, PUINT8 buff, UINT32 len);
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

int tx_thread(void *data);
int rx_thread(void *data);
VOID kalWakeupRxThread(P_GLUE_INFO_T prGlueInfo);

VOID kalHifAhbKalWakeLockTimeout(IN P_GLUE_INFO_T prGlueInfo);
VOID kalMetProfilingStart(IN P_GLUE_INFO_T prGlueInfo, IN struct sk_buff *prSkb);
VOID kalMetProfilingFinish(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);
int kalMetInitProcfs(IN P_GLUE_INFO_T prGlueInfo);
int kalMetRemoveProcfs(void);

UINT_64 kalGetBootTime(void);

INT_32 kalReadToFile(const PUINT_8 pucPath, PUINT_8 pucData, UINT_32 u4Size, PUINT_32 pu4ReadSize);
#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
BOOLEAN kalIsWakeupByWlan(P_ADAPTER_T  prAdapter);
#endif
INT_32 kalHaltLock(UINT_32 waitMs);
INT_32 kalHaltTryLock(VOID);
VOID kalHaltUnlock(VOID);
VOID kalSetHalted(BOOLEAN fgHalt);
BOOLEAN kalIsHalted(VOID);
VOID kalGetAPMCUMen(IN P_GLUE_INFO_T prGlueInfo, IN UINT32 u4StartAddr
		, IN UINT32 u4Offset, IN UINT32 index, OUT PUINT_8 pucBuffer, IN UINT_32 u4BufferLen);

INT_32 kalPerMonInit(IN P_GLUE_INFO_T prGlueInfo);
INT_32 kalPerMonDisable(IN P_GLUE_INFO_T prGlueInfo);
INT_32 kalPerMonEnable(IN P_GLUE_INFO_T prGlueInfo);
INT_32 kalPerMonStart(IN P_GLUE_INFO_T prGlueInfo);
INT_32 kalPerMonStop(IN P_GLUE_INFO_T prGlueInfo);
INT_32 kalPerMonDestroy(IN P_GLUE_INFO_T prGlueInfo);
VOID kalPerMonHandler(IN P_ADAPTER_T prAdapter, ULONG ulParam);
INT_32 kalBoostCpu(IN P_GLUE_INFO_T prGlueInfo, UINT_32 core_num);
INT32 kalSetCpuNumFreq(UINT_32 core_num, UINT_32 freq);
INT_32 kalFbNotifierReg(IN P_GLUE_INFO_T prGlueInfo);
VOID kalFbNotifierUnReg(VOID);
#if CFG_SUPPORT_SET_CAM_BY_PROC
VOID nicConfigProcSetCamCfgWrite(BOOLEAN enabled);
#endif
VOID kalChangeSchedParams(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgNormalThread);
#if CFG_SUPPORT_WPA3
int kalExternalAuthRequest(IN P_ADAPTER_T prAdapter);
#endif

u_int8_t kalIsValidMacAddr(IN const uint8_t *addr);

void kalScanParseRandomMac(
	P_BSS_INFO_T prBssInfo,
	const struct cfg80211_scan_request *request,
	uint8_t *pucRandomMac);

BOOLEAN kalSchedScanParseRandomMac(
	P_BSS_INFO_T prBssInfo,
	const struct cfg80211_sched_scan_request *request,
	uint8_t *pucRandomMac, uint8_t *pucRandomMacMask);
#endif /* _GL_KAL_H */
