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

#ifndef _DEBUG_H
#define _DEBUG_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#ifndef BUILD_QA_DBG
#define BUILD_QA_DBG 0
#endif

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_typedef.h"

extern UINT_8 aucDebugModule[];

extern UINT_32 u4DriverLogLevel;
extern UINT_32 u4FwLogLevel;

extern void set_logtoomuch_enable(int value);
extern int get_logtoomuch_enable(void);

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* Define debug category (class):
 * (1) ERROR (2) WARN (3) STATE (4) EVENT (5) TRACE (6) INFO (7) LOUD (8) TEMP
 */
#define DBG_CLASS_ERROR         BIT(0)
#define DBG_CLASS_WARN          BIT(1)
#define DBG_CLASS_STATE         BIT(2)
#define DBG_CLASS_EVENT         BIT(3)
#define DBG_CLASS_TRACE         BIT(4)
#define DBG_CLASS_INFO          BIT(5)
#define DBG_CLASS_LOUD          BIT(6)
#define DBG_CLASS_TEMP          BIT(7)
#define DBG_CLASS_MASK          BITS(0, 7)

#define DBG_LOG_LEVEL_OFF       (DBG_CLASS_ERROR | DBG_CLASS_WARN | DBG_CLASS_INFO | DBG_CLASS_STATE)
#define DBG_LOG_LEVEL_DEFAULT   (DBG_LOG_LEVEL_OFF | DBG_CLASS_EVENT | DBG_CLASS_TRACE)
#define DBG_LOG_LEVEL_EXTREME   (DBG_LOG_LEVEL_DEFAULT | DBG_CLASS_LOUD | DBG_CLASS_TEMP)

enum PKT_PHASE {
	PHASE_XMIT_RCV,
	PHASE_ENQ_QM,
	PHASE_HIF_TX,
};
#if defined(LINUX)
#define DBG_PRINTF_64BIT_DEC    "lld"

#else /* Windows */
#define DBG_PRINTF_64BIT_DEC    "I64d"

#endif
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* Define debug module index */
typedef enum _ENUM_DBG_MODULE_T {
	DBG_INIT_IDX = 0,	/* For driver initial */
	DBG_HAL_IDX,		/* For HAL(HW) Layer */
	DBG_INTR_IDX,		/* For Interrupt */
	DBG_REQ_IDX,
	DBG_TX_IDX,
	DBG_RX_IDX,
	DBG_RFTEST_IDX,		/* For RF test mode */
	DBG_EMU_IDX,		/* Developer specific */

	DBG_SW1_IDX,		/* Developer specific */
	DBG_SW2_IDX,		/* Developer specific */
	DBG_SW3_IDX,		/* Developer specific */
	DBG_SW4_IDX,		/* Developer specific */

	DBG_HEM_IDX,		/* HEM */
	DBG_AIS_IDX,		/* AIS */
	DBG_RLM_IDX,		/* RLM */
	DBG_MEM_IDX,		/* RLM */
	DBG_CNM_IDX,		/* CNM */
	DBG_RSN_IDX,		/* RSN */
	DBG_BSS_IDX,		/* BSS */
	DBG_SCN_IDX,		/* SCN */
	DBG_SAA_IDX,		/* SAA */
	DBG_AAA_IDX,		/* AAA */
	DBG_P2P_IDX,		/* P2P */
	DBG_QM_IDX,		/* QUE_MGT */
	DBG_SEC_IDX,		/* SEC */
	DBG_BOW_IDX,		/* BOW */
	DBG_WAPI_IDX,		/* WAPI */
	DBG_ROAMING_IDX,	/* ROAMING */
	DBG_TDLS_IDX,		/* TDLS *//* CFG_SUPPORT_TDLS */
	DBG_OID_IDX,
	DBG_NIC_IDX,
	DBG_WNM_IDX,
	DBG_WMM_IDX,

	DBG_MODULE_NUM		/* Notice the XLOG check */
} ENUM_DBG_MODULE_T;
enum PKT_TYPE {
	PKT_RX,
	PKT_TX,
	PKT_TX_DONE
};

typedef enum _ENUM_DBG_SCAN_T {
	DBG_SCAN_WRITE_BEFORE,		/*Start send ScanRequest*/
	DBG_SCAN_WRITE_DONE,		/*hal write success and ScanRequest done*/
} ENUM_DBG_SCAN_T;

struct WLAN_DEBUG_INFO {
	BOOLEAN fgVoE5_7Test:1;
	BOOLEAN fgReserved:7;
};

/* Define debug TRAFFIC_CLASS index */

typedef enum _ENUM_DEBUG_TRAFFIC_CLASS_INDEX_T {
	DEBUG_TC0_INDEX = 0,		/* HIF TX0: AC0 packets */
	DEBUG_TC1_INDEX,		/* HIF TX0: AC1 packets & non-QoS packets */
	DEBUG_TC2_INDEX,		/* HIF TX0: AC2 packets */
	DEBUG_TC3_INDEX,		/* HIF TX0: AC3 packets */
	DEBUG_TC4_INDEX,		/* HIF TX1: Command packets or 802.1x packets */
	DEBUG_TC5_INDEX,		/* HIF TX0: BMCAST packets */
	DEBUG_TC_NUM			/* Maximum number of Traffic Classes. */
} ENUM_DEBUG_TRAFFIC_CLASS_INDEX_T;

/* XLOG */
/* #define XLOG_DBG_MODULE_IDX    28 */ /* DBG_MODULE_NUM */
/* #if (XLOG_DBG_MODULE_IDX != XLOG_DBG_MODULE_IDX) */
/* #error "Please modify the DBG_MODULE_NUM and make sure this include at XLOG" */
/* #endif */

/* Define who owns developer specific index */
#define DBG_YARCO_IDX           DBG_SW1_IDX
#define DBG_KEVIN_IDX           DBG_SW2_IDX
#define DBG_CMC_IDX             DBG_SW3_IDX
#define DBG_GEORGE_IDX          DBG_SW4_IDX

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
/* Debug print format string for the OS system time */
#define OS_SYSTIME_DBG_FORMAT               "0x%08x"

/* Debug print argument for the OS system time */
#define OS_SYSTIME_DBG_ARGUMENT(systime)    (systime)

/* Debug print format string for the MAC Address */
#define MACSTR          "%pM"
/* "%02x:%02x:%02x:%02x:%02x:%02x" */

/* Debug print argument for the MAC Address */
#define MAC2STR(a)	a
/* ((PUINT_8)a)[0], ((PUINT_8)a)[1], ((PUINT_8)a)[2], ((PUINT_8)a)[3], ((PUINT_8)a)[4], ((PUINT_8)a)[5] */

#if (CFG_REFACTORY_PMKSA == 1)
#define PMKSTR "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%03x%02x%02x"
#endif

/* The pre-defined format to dump the value of a varaible with its name shown. */
#define DUMPVAR(variable, format)           (#variable " = " format "\n", variable)

/* The pre-defined format to dump the MAC type value with its name shown. */
#define DUMPMACADDR(addr)                   (#addr " = %pM\n", (addr))

/* for HIDE some information for user load */
#ifdef BUILD_QA_DBG
#define HIDE(_str) _str
#else
#define HIDE(_str) "***"
#endif

/* Basiclly, we just do renaming of KAL functions although they should
 * be defined as "Nothing to do" if DBG=0. But in some compiler, the macro
 * syntax does not support  #define LOG_FUNC(x,...)
 *
 * A caller shall not invoke these three macros when DBG=0.
 */

#define LOG_FUNC                kalPrint
#define LOG_FUNC_LIMITED        kalPrintLimited

#if defined(LINUX)
#define DBGLOG(_Module, _Class, _Fmt, ...) \
	do { \
		if ((aucDebugModule[DBG_##_Module##_IDX] & DBG_CLASS_##_Class) == 0) \
			break; \
		LOG_FUNC("%s:(" #_Module " " #_Class ") " _Fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define DBGLOGLIMITED(_Module, _Class, _Fmt, ...) \
	do { \
		if ((aucDebugModule[DBG_##_Module##_IDX] & DBG_CLASS_##_Class) == 0) \
			break; \
		LOG_FUNC_LIMITED("%s:(" #_Module " " #_Class ") " _Fmt, __func__, ##__VA_ARGS__); \
	} while (0)
#else
#define DBGLOG(_Module, _Class, _Fmt)
#define DBGLOGLIMITED(_Module, _Class, _Fmt)
#endif

#if DBG

#define TMP_BUF_LEN   256
#define TMP_WBUF_LEN  (TMP_BUF_LEN * 2)

extern PINT_16 g_wbuf_p;
extern PINT_8 g_buf_p;

/* If __FUNCTION__ is already defined by compiler, we just use it. */
#if defined(__func__)
#define DEBUGFUNC(_Func)
#else
#define DEBUGFUNC(_Func) \
	static const char __func__[] = _Func
#endif

#define DBGLOG_MEM8(_Module, _Class, _StartAddr, _Length) \
	{ \
		if (aucDebugModule[DBG_##_Module##_IDX] & DBG_CLASS_##_Class) { \
			LOG_FUNC("%s:(" #_Module " " #_Class ")\n", __func__); \
			dumpMemory8((PUINT_8) (_StartAddr), (UINT_32) (_Length)); \
		} \
	}

#define DBGLOG_MEM32(_Module, _Class, _StartAddr, _Length) \
	{ \
		if (aucDebugModule[DBG_##_Module##_IDX] & DBG_CLASS_##_Class) { \
			LOG_FUNC("%s:(" #_Module " " #_Class ")\n", __func__); \
			dumpMemory32((PUINT_32) (_StartAddr), (UINT_32) (_Length)); \
		} \
	}

#undef ASSERT

#ifdef _lint
#define ASSERT(_exp) \
	{ \
		if (!(_exp)) { \
			do {} while (1); \
		} \
	}
#else
#define ASSERT(_exp) \
	{ \
		if (!(_exp)) { \
			LOG_FUNC("Assertion failed: %s:%d %s\n", __FILE__, __LINE__, #_exp); \
			kalBreakPoint(); \
		} \
	}
#endif /* _lint */

#define ASSERT_REPORT(_exp, _fmt) \
	{ \
		if (!(_exp)) { \
			LOG_FUNC("Assertion failed: %s:%d %s\n", __FILE__, __LINE__, #_exp); \
			LOG_FUNC _fmt; \
			kalBreakPoint(); \
		} \
	}

#define DISP_STRING(_str)       _str

#else /* !DBG */

#define DEBUGFUNC(_Func)
#define INITLOG(_Fmt)
#define ERRORLOG(_Fmt)
#define WARNLOG(_Fmt)

#define DBGLOG_MEM8(_Module, _Class, _StartAddr, _Length) \
	{ \
		if (aucDebugModule[DBG_##_Module##_IDX] & DBG_CLASS_##_Class) { \
			LOG_FUNC("%s: (" #_Module " " #_Class ")\n", __func__); \
			dumpMemory8((PUINT_8) (_StartAddr), (UINT_32) (_Length)); \
		} \
	}

#define DBGLOG_MEM32(_Module, _Class, _StartAddr, _Length)

#undef ASSERT

#if BUILD_QA_DBG
#if defined(LINUX)		/* For debugging in Linux w/o GDB */
#define ASSERT(_exp) \
	{ \
		if (!(_exp)) { \
			LOG_FUNC("Assertion failed: %s:%d (%s)\n", __FILE__, __LINE__, #_exp); \
			kalBreakPoint(); \
		} \
	}

#define ASSERT_REPORT(_exp, _fmt) \
	{ \
		if (!(_exp)) { \
			LOG_FUNC("Assertion failed: %s:%d (%s)\n", __FILE__, __LINE__, #_exp); \
			LOG_FUNC _fmt; \
			kalBreakPoint(); \
		} \
	}
#else
#ifdef WINDOWS_CE
#define UNICODE_TEXT(_msg)  TEXT(_msg)
#define ASSERT(_exp) \
	{ \
		if (!(_exp)) { \
			TCHAR rUbuf[256]; \
			kalBreakPoint(); \
			_stprintf(rUbuf, TEXT("Assertion failed: %s:%d %s\n"), \
				  UNICODE_TEXT(__FILE__), __LINE__, UNICODE_TEXT(#_exp)); \
			MessageBox(NULL, rUbuf, TEXT("ASSERT!"), MB_OK); \
		} \
	}

#define ASSERT_REPORT(_exp, _fmt) \
	{ \
		if (!(_exp)) { \
			TCHAR rUbuf[256]; \
			kalBreakPoint(); \
			_stprintf(rUbuf, TEXT("Assertion failed: %s:%d %s\n"), \
				  UNICODE_TEXT(__FILE__), __LINE__, UNICODE_TEXT(#_exp)); \
			MessageBox(NULL, rUbuf, TEXT("ASSERT!"), MB_OK); \
		} \
	}
#else
#define ASSERT(_exp) \
	{ \
		if (!(_exp)) { \
			kalBreakPoint(); \
		} \
	}

#define ASSERT_REPORT(_exp, _fmt) \
	{ \
		if (!(_exp)) { \
			kalBreakPoint(); \
		} \
	}
#endif /* WINDOWS_CE */
#endif /* LINUX */
#else
#define ASSERT(_exp) \
	{ \
		if (!(_exp)) { \
			LOG_FUNC("Warning at %s:%d (%s)\n", __func__, __LINE__, #_exp); \
		} \
	}

#define ASSERT_REPORT(_exp, _fmt) \
	{ \
		if (!(_exp)) { \
			LOG_FUNC("Warning at %s:%d (%s)\n", __func__, __LINE__, #_exp); \
			LOG_FUNC _fmt; \
		} \
	}
#endif /* BUILD_QA_DBG */

#define DISP_STRING(_str)       ""

#endif /* DBG */

#if CFG_STARTUP_DEBUG
#if defined(LINUX)
#define DBGPRINTF kalPrint
#else
#define DBGPRINTF DbgPrint
#endif
#else
#define DBGPRINTF(...)
#endif

/* The following macro is used for debugging packed structures. */
#ifndef DATA_STRUCT_INSPECTING_ASSERT
#define DATA_STRUCT_INSPECTING_ASSERT(expr) \
{ \
	switch (0) {case 0: case (expr): default:; } \
}
#endif
#define DBGLOG_MEM8_IE_ONE_LINE(_Module, _Class, _String, _StartAddr, _Length) \
	{ \
		if (aucDebugModule[DBG_##_Module##_IDX] & DBG_CLASS_##_Class) { \
			dumpMemory8IEOneLine((PUINT_8) (_String), (PUINT_8) (_StartAddr), (UINT_32) (_Length)); \
		} \
	}

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID dumpMemory8IEOneLine(IN PUINT_8 aucBSSID, IN PUINT_8 pucStartAddr, IN UINT_32 u4Length);

VOID dumpMemory8(IN PUINT_8 pucStartAddr, IN UINT_32 u4Length);

VOID dumpMemory32(IN PUINT_32 pu4StartAddr, IN UINT_32 u4Length);

VOID wlanDebugInit(VOID);

VOID wlanDebugUninit(VOID);

VOID wlanTraceReleaseTcRes(P_ADAPTER_T prAdapter, PUINT_8 aucTxRlsCnt, UINT_8 ucAvailable);

VOID wlanTraceTxCmd(P_ADAPTER_T prAdapter, P_CMD_INFO_T prCmd);

VOID wlanReadFwStatus(P_ADAPTER_T prAdapter);

VOID wlanDumpTxReleaseCount(P_ADAPTER_T prAdapter);

VOID wlanDumpTcResAndTxedCmd(PUINT_8 pucBuf, UINT_32 maxLen);

VOID wlanDumpCommandFwStatus(VOID);

VOID wlanDebugScanTargetBSSRecord(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);

VOID wlanDebugScanTargetBSSDump(P_ADAPTER_T prAdapter);

VOID wlanPktDebugDumpInfo(P_ADAPTER_T prAdapter);
VOID wlanPktDebugTraceInfoIP(UINT_8 status, UINT_8 eventType, UINT_8 ucIpProto, UINT_16 u2IpId);
VOID wlanPktDebugTraceInfoARP(UINT_8 status, UINT_8 eventType, UINT_16 u2ArpOpCode);
VOID wlanPktDebugTraceInfo(UINT_8 status, UINT_8 eventType
	, UINT_16 u2EtherType, UINT_8 ucIpProto, UINT_16 u2IpId, UINT_16 u2ArpOpCode);
VOID wlanDebugHifDescriptorDump(P_ADAPTER_T prAdapter, ENUM_AMPDU_TYPE type
	, ENUM_DEBUG_TRAFFIC_CLASS_INDEX_T tcIndex);
VOID wlanDebugScanRecord(P_ADAPTER_T prAdapter, ENUM_DBG_SCAN_T recordType);
VOID wlanDebugScanDump(P_ADAPTER_T prAdapter);


VOID wlanFWDLDebugInit(VOID);

VOID wlanFWDLDebugStartSectionPacketInfo(UINT_32 u4Section, UINT_32 u4DownloadSize,
	UINT_32 u4ResponseTime);

VOID wlanFWDLDebugAddTxStartTime(UINT_32 u4TxStartTime);

VOID wlanFWDLDebugAddTxDoneTime(UINT_32 u4TxDoneTime);

VOID wlanFWDLDebugAddRxStartTime(UINT_32 u4RxStartTime);

VOID wlanFWDLDebugAddRxDoneTime(UINT_32 u4RxDoneTime);

VOID wlanFWDLDebugDumpInfo(VOID);

VOID wlanFWDLDebugUninit(VOID);

UINT_32 wlanFWDLDebugGetPktCnt(VOID);

VOID wlanDumpMcuChipId(P_ADAPTER_T prAdapter);

VOID wlanPktStausDebugUpdateProcessTime(UINT_32 u4DbgTxPktStatusIndex);
VOID wlanPktStatusDebugDumpInfo(P_ADAPTER_T prAdapter);



VOID wlanPktStatusDebugTraceInfoSeq(P_ADAPTER_T prAdapter, UINT_16 u2NoSeq);
VOID wlanPktStatusDebugTraceInfoARP(UINT_8 status, UINT_8 eventType, UINT_16 u2ArpOpCode, PUINT_8 pucPkt
	, P_MSDU_INFO_T prMsduInfo);
VOID wlanPktStatusDebugTraceInfoIP(UINT_8 status, UINT_8 eventType, UINT_8 ucIpProto, UINT_16 u2IpId
	, PUINT_8 pucPkt, P_MSDU_INFO_T prMsduInfo);
VOID wlanPktStatusDebugTraceInfo(UINT_8 status, UINT_8 eventType, UINT_16 u2EtherType
	, UINT_8 ucIpProto, UINT_16 u2IpId, UINT_16 u2ArpOpCode, PUINT_8 pucPkt, P_MSDU_INFO_T prMsduInfo);
VOID wlanDebugCommandRecodTime(P_CMD_INFO_T prCmdInfo);
VOID wlanDebugCommandRecodDump(VOID);
#if CFG_SUPPORT_EMI_DEBUG
VOID wlanReadFwInfoFromEmi(IN PUINT_32 pAddr);

VOID wlanFillTimestamp(P_ADAPTER_T prAdapter, PVOID pvPacket, UINT_8 ucPhase);
#endif

VOID wlanDbgLogLevelInit(VOID);
VOID wlanDbgLogLevelUninit(VOID);
UINT_32 wlanDbgLevelUiSupport(IN P_ADAPTER_T prAdapter, UINT_32 u4Version, UINT_32 ucModule);
UINT_32 wlanDbgGetLogLevelImpl(IN P_ADAPTER_T prAdapter, UINT_32 u4Version, UINT_32 ucModule);
VOID wlanDbgSetLogLevelImpl(IN P_ADAPTER_T prAdapter, UINT_32 u4Version,
		UINT_32 u4Module, UINT_32 u4level);
VOID wlanDbgLevelSync(VOID);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _DEBUG_H */
