/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/debug.h#1
*/

/*! \file   debug.h
    \brief  Definition of SW debugging level.

    In this file, it describes the definition of various SW debugging levels and
    assert functions.
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

#define DBG_DISABLE_ALL_LOG             0

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_typedef.h"

extern UINT_8 aucDebugModule[];
extern UINT_32 u4DebugModule;

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

#define DBG_CLASS_ALL \
	(DBG_CLASS_MASK)

#if DBG
#define DBG_CLASS_DEFAULT \
	(DBG_CLASS_ALL)
#else
#define DBG_CLASS_DEFAULT \
	(DBG_CLASS_ERROR | DBG_CLASS_WARN | DBG_CLASS_INFO | DBG_CLASS_STATE | DBG_CLASS_TRACE)
#endif

/* Control of debug classes */
#define INIT_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define HAL_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define INTR_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define REQ_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define TX_DBG_CLASSES \
	(DBG_CLASS_ERROR | DBG_CLASS_WARN | DBG_CLASS_INFO | DBG_CLASS_STATE)

#define RX_DBG_CLASSES \
	(DBG_CLASS_ERROR | DBG_CLASS_WARN | DBG_CLASS_INFO | DBG_CLASS_STATE)

#define RFTEST_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define EMU_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define SW1_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define SW2_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define SW3_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define SW4_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define HEM_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define AIS_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define RLM_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define MEM_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define CNM_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define RSN_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define BSS_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define SCN_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define SAA_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define AAA_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define P2P_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define QM_DBG_CLASSES \
	(DBG_CLASS_DEFAULT | DBG_CLASS_TRACE)

#define SEC_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define BOW_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define WAPI_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)

#define ROAMING_DBG_CLASSES \
	(DBG_CLASS_DEFAULT)


/* Define INIT related debug classes */
#if (INIT_DBG_CLASSES & DBG_CLASS_ERROR)
#define INIT_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define INIT_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define INIT_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_ERROR_LOGDUMP8(_StartAddr, _Length)
#define INIT_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INIT_DBG_CLASSES & DBG_CLASS_WARN)
#define INIT_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define INIT_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define INIT_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_WARN_LOGDUMP8(_StartAddr, _Length)
#define INIT_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INIT_DBG_CLASSES & DBG_CLASS_STATE)
#define INIT_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define INIT_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define INIT_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_STATE_LOGDUMP8(_StartAddr, _Length)
#define INIT_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INIT_DBG_CLASSES & DBG_CLASS_EVENT)
#define INIT_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define INIT_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define INIT_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_EVENT_LOGDUMP8(_StartAddr, _Length)
#define INIT_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INIT_DBG_CLASSES & DBG_CLASS_TRACE)
#define INIT_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define INIT_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define INIT_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_TRACE_LOGDUMP8(_StartAddr, _Length)
#define INIT_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INIT_DBG_CLASSES & DBG_CLASS_INFO)
#define INIT_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define INIT_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define INIT_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_INFO_LOGDUMP8(_StartAddr, _Length)
#define INIT_INFO_LOGDUMP32(_StartAddr, _Length)
#error
#endif

#if (INIT_DBG_CLASSES & DBG_CLASS_LOUD)
#define INIT_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define INIT_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define INIT_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_LOUD_LOGDUMP8(_StartAddr, _Length)
#define INIT_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INIT_DBG_CLASSES & DBG_CLASS_TEMP)
#define INIT_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INIT_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INIT_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define INIT_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INIT_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define INIT_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define INIT_TEMP_LOGDUMP8(_StartAddr, _Length)
#define INIT_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define HAL related debug classes */
#if (HAL_DBG_CLASSES & DBG_CLASS_ERROR)
#define HAL_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define HAL_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define HAL_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_ERROR_LOGDUMP8(_StartAddr, _Length)
#define HAL_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HAL_DBG_CLASSES & DBG_CLASS_WARN)
#define HAL_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define HAL_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define HAL_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_WARN_LOGDUMP8(_StartAddr, _Length)
#define HAL_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HAL_DBG_CLASSES & DBG_CLASS_STATE)
#define HAL_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define HAL_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define HAL_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_STATE_LOGDUMP8(_StartAddr, _Length)
#define HAL_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HAL_DBG_CLASSES & DBG_CLASS_EVENT)
#define HAL_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define HAL_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define HAL_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_EVENT_LOGDUMP8(_StartAddr, _Length)
#define HAL_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HAL_DBG_CLASSES & DBG_CLASS_TRACE)
#define HAL_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define HAL_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define HAL_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_TRACE_LOGDUMP8(_StartAddr, _Length)
#define HAL_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HAL_DBG_CLASSES & DBG_CLASS_INFO)
#define HAL_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define HAL_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define HAL_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_INFO_LOGDUMP8(_StartAddr, _Length)
#define HAL_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HAL_DBG_CLASSES & DBG_CLASS_LOUD)
#define HAL_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define HAL_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define HAL_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_LOUD_LOGDUMP8(_StartAddr, _Length)
#define HAL_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HAL_DBG_CLASSES & DBG_CLASS_TEMP)
#define HAL_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HAL_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HAL_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define HAL_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HAL_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define HAL_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define HAL_TEMP_LOGDUMP8(_StartAddr, _Length)
#define HAL_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define INTR related debug classes */
#if (INTR_DBG_CLASSES & DBG_CLASS_ERROR)
#define INTR_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define INTR_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define INTR_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_ERROR_LOGDUMP8(_StartAddr, _Length)
#define INTR_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INTR_DBG_CLASSES & DBG_CLASS_WARN)
#define INTR_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define INTR_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define INTR_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_WARN_LOGDUMP8(_StartAddr, _Length)
#define INTR_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INTR_DBG_CLASSES & DBG_CLASS_STATE)
#define INTR_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define INTR_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define INTR_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_STATE_LOGDUMP8(_StartAddr, _Length)
#define INTR_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INTR_DBG_CLASSES & DBG_CLASS_EVENT)
#define INTR_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define INTR_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define INTR_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_EVENT_LOGDUMP8(_StartAddr, _Length)
#define INTR_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INTR_DBG_CLASSES & DBG_CLASS_TRACE)
#define INTR_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define INTR_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define INTR_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_TRACE_LOGDUMP8(_StartAddr, _Length)
#define INTR_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INTR_DBG_CLASSES & DBG_CLASS_INFO)
#define INTR_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define INTR_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define INTR_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_INFO_LOGDUMP8(_StartAddr, _Length)
#define INTR_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INTR_DBG_CLASSES & DBG_CLASS_LOUD)
#define INTR_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define INTR_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define INTR_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_LOUD_LOGDUMP8(_StartAddr, _Length)
#define INTR_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (INTR_DBG_CLASSES & DBG_CLASS_TEMP)
#define INTR_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define INTR_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_INTR_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define INTR_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_INTR_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define INTR_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define INTR_TEMP_LOGDUMP8(_StartAddr, _Length)
#define INTR_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define REQ related debug classes */
#if (REQ_DBG_CLASSES & DBG_CLASS_ERROR)
#define REQ_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define REQ_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define REQ_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_ERROR_LOGDUMP8(_StartAddr, _Length)
#define REQ_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (REQ_DBG_CLASSES & DBG_CLASS_WARN)
#define REQ_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define REQ_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define REQ_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_WARN_LOGDUMP8(_StartAddr, _Length)
#define REQ_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (REQ_DBG_CLASSES & DBG_CLASS_STATE)
#define REQ_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define REQ_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define REQ_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_STATE_LOGDUMP8(_StartAddr, _Length)
#define REQ_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (REQ_DBG_CLASSES & DBG_CLASS_EVENT)
#define REQ_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define REQ_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define REQ_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_EVENT_LOGDUMP8(_StartAddr, _Length)
#define REQ_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (REQ_DBG_CLASSES & DBG_CLASS_TRACE)
#define REQ_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define REQ_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define REQ_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_TRACE_LOGDUMP8(_StartAddr, _Length)
#define REQ_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (REQ_DBG_CLASSES & DBG_CLASS_INFO)
#define REQ_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define REQ_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define REQ_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_INFO_LOGDUMP8(_StartAddr, _Length)
#define REQ_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (REQ_DBG_CLASSES & DBG_CLASS_LOUD)
#define REQ_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define REQ_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define REQ_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_LOUD_LOGDUMP8(_StartAddr, _Length)
#define REQ_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (REQ_DBG_CLASSES & DBG_CLASS_TEMP)
#define REQ_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define REQ_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_REQ_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define REQ_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_REQ_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define REQ_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define REQ_TEMP_LOGDUMP8(_StartAddr, _Length)
#define REQ_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define TX related debug classes */
#if (TX_DBG_CLASSES & DBG_CLASS_ERROR)
#define TX_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define TX_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define TX_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define TX_ERROR_LOGDUMP8(_StartAddr, _Length)
#define TX_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (TX_DBG_CLASSES & DBG_CLASS_WARN)
#define TX_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define TX_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define TX_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define TX_WARN_LOGDUMP8(_StartAddr, _Length)
#define TX_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (TX_DBG_CLASSES & DBG_CLASS_STATE)
#define TX_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define TX_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define TX_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define TX_STATE_LOGDUMP8(_StartAddr, _Length)
#define TX_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (TX_DBG_CLASSES & DBG_CLASS_EVENT)
#define TX_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define TX_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define TX_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define TX_EVENT_LOGDUMP8(_StartAddr, _Length)
#define TX_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (TX_DBG_CLASSES & DBG_CLASS_TRACE)
#define TX_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define TX_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define TX_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define TX_TRACE_LOGDUMP8(_StartAddr, _Length)
#define TX_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (TX_DBG_CLASSES & DBG_CLASS_INFO)
#define TX_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define TX_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define TX_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define TX_INFO_LOGDUMP8(_StartAddr, _Length)
#define TX_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (TX_DBG_CLASSES & DBG_CLASS_LOUD)
#define TX_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define TX_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define TX_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define TX_LOUD_LOGDUMP8(_StartAddr, _Length)
#define TX_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (TX_DBG_CLASSES & DBG_CLASS_TEMP)
#define TX_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define TX_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_TX_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define TX_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_TX_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define TX_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define TX_TEMP_LOGDUMP8(_StartAddr, _Length)
#define TX_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define RX related debug classes */
#if (RX_DBG_CLASSES & DBG_CLASS_ERROR)
#define RX_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define RX_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define RX_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define RX_ERROR_LOGDUMP8(_StartAddr, _Length)
#define RX_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RX_DBG_CLASSES & DBG_CLASS_WARN)
#define RX_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define RX_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define RX_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define RX_WARN_LOGDUMP8(_StartAddr, _Length)
#define RX_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RX_DBG_CLASSES & DBG_CLASS_STATE)
#define RX_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define RX_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define RX_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define RX_STATE_LOGDUMP8(_StartAddr, _Length)
#define RX_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RX_DBG_CLASSES & DBG_CLASS_EVENT)
#define RX_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define RX_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define RX_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define RX_EVENT_LOGDUMP8(_StartAddr, _Length)
#define RX_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RX_DBG_CLASSES & DBG_CLASS_TRACE)
#define RX_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define RX_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define RX_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define RX_TRACE_LOGDUMP8(_StartAddr, _Length)
#define RX_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RX_DBG_CLASSES & DBG_CLASS_INFO)
#define RX_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define RX_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define RX_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define RX_INFO_LOGDUMP8(_StartAddr, _Length)
#define RX_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RX_DBG_CLASSES & DBG_CLASS_LOUD)
#define RX_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define RX_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define RX_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define RX_LOUD_LOGDUMP8(_StartAddr, _Length)
#define RX_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RX_DBG_CLASSES & DBG_CLASS_TEMP)
#define RX_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RX_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RX_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define RX_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RX_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define RX_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define RX_TEMP_LOGDUMP8(_StartAddr, _Length)
#define RX_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define RFTEST related debug classes */
#if (RFTEST_DBG_CLASSES & DBG_CLASS_ERROR)
#define RFTEST_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define RFTEST_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define RFTEST_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_ERROR_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RFTEST_DBG_CLASSES & DBG_CLASS_WARN)
#define RFTEST_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define RFTEST_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define RFTEST_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_WARN_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RFTEST_DBG_CLASSES & DBG_CLASS_STATE)
#define RFTEST_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define RFTEST_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define RFTEST_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_STATE_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RFTEST_DBG_CLASSES & DBG_CLASS_EVENT)
#define RFTEST_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define RFTEST_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define RFTEST_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_EVENT_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RFTEST_DBG_CLASSES & DBG_CLASS_TRACE)
#define RFTEST_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define RFTEST_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define RFTEST_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_TRACE_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RFTEST_DBG_CLASSES & DBG_CLASS_INFO)
#define RFTEST_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define RFTEST_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define RFTEST_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_INFO_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RFTEST_DBG_CLASSES & DBG_CLASS_LOUD)
#define RFTEST_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define RFTEST_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define RFTEST_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_LOUD_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RFTEST_DBG_CLASSES & DBG_CLASS_TEMP)
#define RFTEST_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RFTEST_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RFTEST_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define RFTEST_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RFTEST_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define RFTEST_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define RFTEST_TEMP_LOGDUMP8(_StartAddr, _Length)
#define RFTEST_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define EMU related debug classes */
#if (EMU_DBG_CLASSES & DBG_CLASS_ERROR)
#define EMU_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define EMU_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define EMU_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_ERROR_LOGDUMP8(_StartAddr, _Length)
#define EMU_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (EMU_DBG_CLASSES & DBG_CLASS_WARN)
#define EMU_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define EMU_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define EMU_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_WARN_LOGDUMP8(_StartAddr, _Length)
#define EMU_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (EMU_DBG_CLASSES & DBG_CLASS_STATE)
#define EMU_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define EMU_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define EMU_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_STATE_LOGDUMP8(_StartAddr, _Length)
#define EMU_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (EMU_DBG_CLASSES & DBG_CLASS_EVENT)
#define EMU_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define EMU_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define EMU_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_EVENT_LOGDUMP8(_StartAddr, _Length)
#define EMU_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (EMU_DBG_CLASSES & DBG_CLASS_TRACE)
#define EMU_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define EMU_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define EMU_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_TRACE_LOGDUMP8(_StartAddr, _Length)
#define EMU_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (EMU_DBG_CLASSES & DBG_CLASS_INFO)
#define EMU_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define EMU_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define EMU_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_INFO_LOGDUMP8(_StartAddr, _Length)
#define EMU_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (EMU_DBG_CLASSES & DBG_CLASS_LOUD)
#define EMU_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define EMU_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define EMU_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_LOUD_LOGDUMP8(_StartAddr, _Length)
#define EMU_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (EMU_DBG_CLASSES & DBG_CLASS_TEMP)
#define EMU_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define EMU_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_EMU_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define EMU_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_EMU_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define EMU_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define EMU_TEMP_LOGDUMP8(_StartAddr, _Length)
#define EMU_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define SW1 related debug classes */
#if (SW1_DBG_CLASSES & DBG_CLASS_ERROR)
#define SW1_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define SW1_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define SW1_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_ERROR_LOGDUMP8(_StartAddr, _Length)
#define SW1_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW1_DBG_CLASSES & DBG_CLASS_WARN)
#define SW1_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define SW1_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define SW1_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_WARN_LOGDUMP8(_StartAddr, _Length)
#define SW1_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW1_DBG_CLASSES & DBG_CLASS_STATE)
#define SW1_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define SW1_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define SW1_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_STATE_LOGDUMP8(_StartAddr, _Length)
#define SW1_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW1_DBG_CLASSES & DBG_CLASS_EVENT)
#define SW1_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define SW1_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define SW1_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_EVENT_LOGDUMP8(_StartAddr, _Length)
#define SW1_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW1_DBG_CLASSES & DBG_CLASS_TRACE)
#define SW1_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define SW1_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define SW1_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_TRACE_LOGDUMP8(_StartAddr, _Length)
#define SW1_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW1_DBG_CLASSES & DBG_CLASS_INFO)
#define SW1_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define SW1_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define SW1_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_INFO_LOGDUMP8(_StartAddr, _Length)
#define SW1_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW1_DBG_CLASSES & DBG_CLASS_LOUD)
#define SW1_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define SW1_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define SW1_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_LOUD_LOGDUMP8(_StartAddr, _Length)
#define SW1_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW1_DBG_CLASSES & DBG_CLASS_TEMP)
#define SW1_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW1_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW1_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define SW1_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW1_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define SW1_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define SW1_TEMP_LOGDUMP8(_StartAddr, _Length)
#define SW1_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define SW2 related debug classes */
#if (SW2_DBG_CLASSES & DBG_CLASS_ERROR)
#define SW2_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define SW2_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define SW2_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_ERROR_LOGDUMP8(_StartAddr, _Length)
#define SW2_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW2_DBG_CLASSES & DBG_CLASS_WARN)
#define SW2_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define SW2_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define SW2_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_WARN_LOGDUMP8(_StartAddr, _Length)
#define SW2_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW2_DBG_CLASSES & DBG_CLASS_STATE)
#define SW2_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define SW2_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define SW2_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_STATE_LOGDUMP8(_StartAddr, _Length)
#define SW2_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW2_DBG_CLASSES & DBG_CLASS_EVENT)
#define SW2_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define SW2_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define SW2_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_EVENT_LOGDUMP8(_StartAddr, _Length)
#define SW2_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW2_DBG_CLASSES & DBG_CLASS_TRACE)
#define SW2_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define SW2_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define SW2_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_TRACE_LOGDUMP8(_StartAddr, _Length)
#define SW2_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW2_DBG_CLASSES & DBG_CLASS_INFO)
#define SW2_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define SW2_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define SW2_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_INFO_LOGDUMP8(_StartAddr, _Length)
#define SW2_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW2_DBG_CLASSES & DBG_CLASS_LOUD)
#define SW2_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define SW2_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define SW2_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_LOUD_LOGDUMP8(_StartAddr, _Length)
#define SW2_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW2_DBG_CLASSES & DBG_CLASS_TEMP)
#define SW2_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW2_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW2_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define SW2_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW2_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define SW2_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define SW2_TEMP_LOGDUMP8(_StartAddr, _Length)
#define SW2_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define SW3 related debug classes */
#if (SW3_DBG_CLASSES & DBG_CLASS_ERROR)
#define SW3_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define SW3_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define SW3_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_ERROR_LOGDUMP8(_StartAddr, _Length)
#define SW3_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW3_DBG_CLASSES & DBG_CLASS_WARN)
#define SW3_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define SW3_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define SW3_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_WARN_LOGDUMP8(_StartAddr, _Length)
#define SW3_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW3_DBG_CLASSES & DBG_CLASS_STATE)
#define SW3_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define SW3_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define SW3_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_STATE_LOGDUMP8(_StartAddr, _Length)
#define SW3_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW3_DBG_CLASSES & DBG_CLASS_EVENT)
#define SW3_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define SW3_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define SW3_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_EVENT_LOGDUMP8(_StartAddr, _Length)
#define SW3_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW3_DBG_CLASSES & DBG_CLASS_TRACE)
#define SW3_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define SW3_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define SW3_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_TRACE_LOGDUMP8(_StartAddr, _Length)
#define SW3_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW3_DBG_CLASSES & DBG_CLASS_INFO)
#define SW3_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define SW3_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define SW3_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_INFO_LOGDUMP8(_StartAddr, _Length)
#define SW3_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW3_DBG_CLASSES & DBG_CLASS_LOUD)
#define SW3_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define SW3_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define SW3_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_LOUD_LOGDUMP8(_StartAddr, _Length)
#define SW3_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW3_DBG_CLASSES & DBG_CLASS_TEMP)
#define SW3_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW3_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW3_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define SW3_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW3_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define SW3_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define SW3_TEMP_LOGDUMP8(_StartAddr, _Length)
#define SW3_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define SW4 related debug classes */
#if (SW4_DBG_CLASSES & DBG_CLASS_ERROR)
#define SW4_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define SW4_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define SW4_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_ERROR_LOGDUMP8(_StartAddr, _Length)
#define SW4_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW4_DBG_CLASSES & DBG_CLASS_WARN)
#define SW4_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define SW4_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define SW4_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_WARN_LOGDUMP8(_StartAddr, _Length)
#define SW4_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW4_DBG_CLASSES & DBG_CLASS_STATE)
#define SW4_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define SW4_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define SW4_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_STATE_LOGDUMP8(_StartAddr, _Length)
#define SW4_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW4_DBG_CLASSES & DBG_CLASS_EVENT)
#define SW4_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define SW4_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define SW4_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_EVENT_LOGDUMP8(_StartAddr, _Length)
#define SW4_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW4_DBG_CLASSES & DBG_CLASS_TRACE)
#define SW4_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define SW4_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define SW4_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_TRACE_LOGDUMP8(_StartAddr, _Length)
#define SW4_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW4_DBG_CLASSES & DBG_CLASS_INFO)
#define SW4_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define SW4_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define SW4_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_INFO_LOGDUMP8(_StartAddr, _Length)
#define SW4_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW4_DBG_CLASSES & DBG_CLASS_LOUD)
#define SW4_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define SW4_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define SW4_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_LOUD_LOGDUMP8(_StartAddr, _Length)
#define SW4_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SW4_DBG_CLASSES & DBG_CLASS_TEMP)
#define SW4_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SW4_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SW4_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define SW4_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SW4_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define SW4_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define SW4_TEMP_LOGDUMP8(_StartAddr, _Length)
#define SW4_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define HEM related debug classes */
#if (HEM_DBG_CLASSES & DBG_CLASS_ERROR)
#define HEM_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define HEM_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define HEM_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_ERROR_LOGDUMP8(_StartAddr, _Length)
#define HEM_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HEM_DBG_CLASSES & DBG_CLASS_WARN)
#define HEM_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define HEM_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define HEM_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_WARN_LOGDUMP8(_StartAddr, _Length)
#define HEM_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HEM_DBG_CLASSES & DBG_CLASS_STATE)
#define HEM_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define HEM_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define HEM_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_STATE_LOGDUMP8(_StartAddr, _Length)
#define HEM_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HEM_DBG_CLASSES & DBG_CLASS_EVENT)
#define HEM_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define HEM_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define HEM_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_EVENT_LOGDUMP8(_StartAddr, _Length)
#define HEM_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HEM_DBG_CLASSES & DBG_CLASS_TRACE)
#define HEM_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define HEM_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define HEM_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_TRACE_LOGDUMP8(_StartAddr, _Length)
#define HEM_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HEM_DBG_CLASSES & DBG_CLASS_INFO)
#define HEM_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define HEM_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define HEM_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_INFO_LOGDUMP8(_StartAddr, _Length)
#define HEM_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HEM_DBG_CLASSES & DBG_CLASS_LOUD)
#define HEM_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define HEM_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define HEM_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_LOUD_LOGDUMP8(_StartAddr, _Length)
#define HEM_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (HEM_DBG_CLASSES & DBG_CLASS_TEMP)
#define HEM_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define HEM_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_HEM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define HEM_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_HEM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define HEM_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define HEM_TEMP_LOGDUMP8(_StartAddr, _Length)
#define HEM_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define AIS related debug classes */
#if (AIS_DBG_CLASSES & DBG_CLASS_ERROR)
#define AIS_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define AIS_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define AIS_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_ERROR_LOGDUMP8(_StartAddr, _Length)
#define AIS_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AIS_DBG_CLASSES & DBG_CLASS_WARN)
#define AIS_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define AIS_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define AIS_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_WARN_LOGDUMP8(_StartAddr, _Length)
#define AIS_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AIS_DBG_CLASSES & DBG_CLASS_STATE)
#define AIS_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define AIS_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define AIS_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_STATE_LOGDUMP8(_StartAddr, _Length)
#define AIS_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AIS_DBG_CLASSES & DBG_CLASS_EVENT)
#define AIS_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define AIS_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define AIS_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_EVENT_LOGDUMP8(_StartAddr, _Length)
#define AIS_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AIS_DBG_CLASSES & DBG_CLASS_TRACE)
#define AIS_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define AIS_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define AIS_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_TRACE_LOGDUMP8(_StartAddr, _Length)
#define AIS_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AIS_DBG_CLASSES & DBG_CLASS_INFO)
#define AIS_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define AIS_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define AIS_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_INFO_LOGDUMP8(_StartAddr, _Length)
#define AIS_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AIS_DBG_CLASSES & DBG_CLASS_LOUD)
#define AIS_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define AIS_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define AIS_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_LOUD_LOGDUMP8(_StartAddr, _Length)
#define AIS_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AIS_DBG_CLASSES & DBG_CLASS_TEMP)
#define AIS_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AIS_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AIS_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define AIS_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AIS_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define AIS_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define AIS_TEMP_LOGDUMP8(_StartAddr, _Length)
#define AIS_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define RLM related debug classes */
#if (RLM_DBG_CLASSES & DBG_CLASS_ERROR)
#define RLM_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define RLM_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define RLM_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_ERROR_LOGDUMP8(_StartAddr, _Length)
#define RLM_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RLM_DBG_CLASSES & DBG_CLASS_WARN)
#define RLM_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define RLM_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define RLM_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_WARN_LOGDUMP8(_StartAddr, _Length)
#define RLM_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RLM_DBG_CLASSES & DBG_CLASS_STATE)
#define RLM_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define RLM_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define RLM_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_STATE_LOGDUMP8(_StartAddr, _Length)
#define RLM_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RLM_DBG_CLASSES & DBG_CLASS_EVENT)
#define RLM_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define RLM_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define RLM_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_EVENT_LOGDUMP8(_StartAddr, _Length)
#define RLM_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RLM_DBG_CLASSES & DBG_CLASS_TRACE)
#define RLM_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define RLM_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define RLM_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_TRACE_LOGDUMP8(_StartAddr, _Length)
#define RLM_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RLM_DBG_CLASSES & DBG_CLASS_INFO)
#define RLM_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define RLM_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define RLM_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_INFO_LOGDUMP8(_StartAddr, _Length)
#define RLM_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RLM_DBG_CLASSES & DBG_CLASS_LOUD)
#define RLM_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define RLM_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define RLM_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_LOUD_LOGDUMP8(_StartAddr, _Length)
#define RLM_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RLM_DBG_CLASSES & DBG_CLASS_TEMP)
#define RLM_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RLM_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RLM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define RLM_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RLM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define RLM_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define RLM_TEMP_LOGDUMP8(_StartAddr, _Length)
#define RLM_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define MEM related debug classes */
#if (MEM_DBG_CLASSES & DBG_CLASS_ERROR)
#define MEM_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define MEM_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define MEM_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_ERROR_LOGDUMP8(_StartAddr, _Length)
#define MEM_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (MEM_DBG_CLASSES & DBG_CLASS_WARN)
#define MEM_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define MEM_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define MEM_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_WARN_LOGDUMP8(_StartAddr, _Length)
#define MEM_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (MEM_DBG_CLASSES & DBG_CLASS_STATE)
#define MEM_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define MEM_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define MEM_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_STATE_LOGDUMP8(_StartAddr, _Length)
#define MEM_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (MEM_DBG_CLASSES & DBG_CLASS_EVENT)
#define MEM_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define MEM_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define MEM_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_EVENT_LOGDUMP8(_StartAddr, _Length)
#define MEM_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (MEM_DBG_CLASSES & DBG_CLASS_TRACE)
#define MEM_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define MEM_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define MEM_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_TRACE_LOGDUMP8(_StartAddr, _Length)
#define MEM_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (MEM_DBG_CLASSES & DBG_CLASS_INFO)
#define MEM_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define MEM_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define MEM_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_INFO_LOGDUMP8(_StartAddr, _Length)
#define MEM_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (MEM_DBG_CLASSES & DBG_CLASS_LOUD)
#define MEM_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define MEM_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define MEM_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_LOUD_LOGDUMP8(_StartAddr, _Length)
#define MEM_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (MEM_DBG_CLASSES & DBG_CLASS_TEMP)
#define MEM_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define MEM_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_MEM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define MEM_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_MEM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define MEM_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define MEM_TEMP_LOGDUMP8(_StartAddr, _Length)
#define MEM_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define CNM related debug classes */
#if (CNM_DBG_CLASSES & DBG_CLASS_ERROR)
#define CNM_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define CNM_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define CNM_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_ERROR_LOGDUMP8(_StartAddr, _Length)
#define CNM_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (CNM_DBG_CLASSES & DBG_CLASS_WARN)
#define CNM_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define CNM_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define CNM_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_WARN_LOGDUMP8(_StartAddr, _Length)
#define CNM_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (CNM_DBG_CLASSES & DBG_CLASS_STATE)
#define CNM_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define CNM_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define CNM_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_STATE_LOGDUMP8(_StartAddr, _Length)
#define CNM_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (CNM_DBG_CLASSES & DBG_CLASS_EVENT)
#define CNM_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define CNM_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define CNM_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_EVENT_LOGDUMP8(_StartAddr, _Length)
#define CNM_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (CNM_DBG_CLASSES & DBG_CLASS_TRACE)
#define CNM_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define CNM_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define CNM_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_TRACE_LOGDUMP8(_StartAddr, _Length)
#define CNM_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (CNM_DBG_CLASSES & DBG_CLASS_INFO)
#define CNM_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define CNM_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define CNM_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_INFO_LOGDUMP8(_StartAddr, _Length)
#define CNM_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (CNM_DBG_CLASSES & DBG_CLASS_LOUD)
#define CNM_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define CNM_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define CNM_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_LOUD_LOGDUMP8(_StartAddr, _Length)
#define CNM_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (CNM_DBG_CLASSES & DBG_CLASS_TEMP)
#define CNM_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define CNM_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_CNM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define CNM_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_CNM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define CNM_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define CNM_TEMP_LOGDUMP8(_StartAddr, _Length)
#define CNM_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define RSN related debug classes */
#if (RSN_DBG_CLASSES & DBG_CLASS_ERROR)
#define RSN_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define RSN_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define RSN_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_ERROR_LOGDUMP8(_StartAddr, _Length)
#define RSN_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RSN_DBG_CLASSES & DBG_CLASS_WARN)
#define RSN_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define RSN_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define RSN_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_WARN_LOGDUMP8(_StartAddr, _Length)
#define RSN_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RSN_DBG_CLASSES & DBG_CLASS_STATE)
#define RSN_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define RSN_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define RSN_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_STATE_LOGDUMP8(_StartAddr, _Length)
#define RSN_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RSN_DBG_CLASSES & DBG_CLASS_EVENT)
#define RSN_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define RSN_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define RSN_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_EVENT_LOGDUMP8(_StartAddr, _Length)
#define RSN_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RSN_DBG_CLASSES & DBG_CLASS_TRACE)
#define RSN_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define RSN_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define RSN_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_TRACE_LOGDUMP8(_StartAddr, _Length)
#define RSN_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RSN_DBG_CLASSES & DBG_CLASS_INFO)
#define RSN_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define RSN_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define RSN_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_INFO_LOGDUMP8(_StartAddr, _Length)
#define RSN_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RSN_DBG_CLASSES & DBG_CLASS_LOUD)
#define RSN_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define RSN_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define RSN_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_LOUD_LOGDUMP8(_StartAddr, _Length)
#define RSN_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (RSN_DBG_CLASSES & DBG_CLASS_TEMP)
#define RSN_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define RSN_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_RSN_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define RSN_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_RSN_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define RSN_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define RSN_TEMP_LOGDUMP8(_StartAddr, _Length)
#define RSN_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define BSS related debug classes */
#if (BSS_DBG_CLASSES & DBG_CLASS_ERROR)
#define BSS_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define BSS_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define BSS_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_ERROR_LOGDUMP8(_StartAddr, _Length)
#define BSS_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BSS_DBG_CLASSES & DBG_CLASS_WARN)
#define BSS_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define BSS_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define BSS_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_WARN_LOGDUMP8(_StartAddr, _Length)
#define BSS_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BSS_DBG_CLASSES & DBG_CLASS_STATE)
#define BSS_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define BSS_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define BSS_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_STATE_LOGDUMP8(_StartAddr, _Length)
#define BSS_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BSS_DBG_CLASSES & DBG_CLASS_EVENT)
#define BSS_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define BSS_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define BSS_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_EVENT_LOGDUMP8(_StartAddr, _Length)
#define BSS_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BSS_DBG_CLASSES & DBG_CLASS_TRACE)
#define BSS_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define BSS_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define BSS_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_TRACE_LOGDUMP8(_StartAddr, _Length)
#define BSS_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BSS_DBG_CLASSES & DBG_CLASS_INFO)
#define BSS_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define BSS_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define BSS_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_INFO_LOGDUMP8(_StartAddr, _Length)
#define BSS_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BSS_DBG_CLASSES & DBG_CLASS_LOUD)
#define BSS_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define BSS_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define BSS_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_LOUD_LOGDUMP8(_StartAddr, _Length)
#define BSS_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BSS_DBG_CLASSES & DBG_CLASS_TEMP)
#define BSS_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BSS_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BSS_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define BSS_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BSS_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define BSS_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define BSS_TEMP_LOGDUMP8(_StartAddr, _Length)
#define BSS_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define SCN related debug classes */
#if (SCN_DBG_CLASSES & DBG_CLASS_ERROR)
#define SCN_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define SCN_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define SCN_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_ERROR_LOGDUMP8(_StartAddr, _Length)
#define SCN_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SCN_DBG_CLASSES & DBG_CLASS_WARN)
#define SCN_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define SCN_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define SCN_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_WARN_LOGDUMP8(_StartAddr, _Length)
#define SCN_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SCN_DBG_CLASSES & DBG_CLASS_STATE)
#define SCN_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define SCN_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define SCN_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_STATE_LOGDUMP8(_StartAddr, _Length)
#define SCN_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SCN_DBG_CLASSES & DBG_CLASS_EVENT)
#define SCN_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define SCN_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define SCN_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_EVENT_LOGDUMP8(_StartAddr, _Length)
#define SCN_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SCN_DBG_CLASSES & DBG_CLASS_TRACE)
#define SCN_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define SCN_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define SCN_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_TRACE_LOGDUMP8(_StartAddr, _Length)
#define SCN_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SCN_DBG_CLASSES & DBG_CLASS_INFO)
#define SCN_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define SCN_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define SCN_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_INFO_LOGDUMP8(_StartAddr, _Length)
#define SCN_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SCN_DBG_CLASSES & DBG_CLASS_LOUD)
#define SCN_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define SCN_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define SCN_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_LOUD_LOGDUMP8(_StartAddr, _Length)
#define SCN_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SCN_DBG_CLASSES & DBG_CLASS_TEMP)
#define SCN_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SCN_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SCN_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define SCN_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SCN_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define SCN_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define SCN_TEMP_LOGDUMP8(_StartAddr, _Length)
#define SCN_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define SAA related debug classes */
#if (SAA_DBG_CLASSES & DBG_CLASS_ERROR)
#define SAA_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define SAA_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define SAA_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_ERROR_LOGDUMP8(_StartAddr, _Length)
#define SAA_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SAA_DBG_CLASSES & DBG_CLASS_WARN)
#define SAA_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define SAA_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define SAA_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_WARN_LOGDUMP8(_StartAddr, _Length)
#define SAA_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SAA_DBG_CLASSES & DBG_CLASS_STATE)
#define SAA_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define SAA_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define SAA_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_STATE_LOGDUMP8(_StartAddr, _Length)
#define SAA_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SAA_DBG_CLASSES & DBG_CLASS_EVENT)
#define SAA_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define SAA_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define SAA_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_EVENT_LOGDUMP8(_StartAddr, _Length)
#define SAA_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SAA_DBG_CLASSES & DBG_CLASS_TRACE)
#define SAA_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define SAA_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define SAA_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_TRACE_LOGDUMP8(_StartAddr, _Length)
#define SAA_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SAA_DBG_CLASSES & DBG_CLASS_INFO)
#define SAA_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define SAA_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define SAA_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_INFO_LOGDUMP8(_StartAddr, _Length)
#define SAA_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SAA_DBG_CLASSES & DBG_CLASS_LOUD)
#define SAA_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define SAA_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define SAA_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_LOUD_LOGDUMP8(_StartAddr, _Length)
#define SAA_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SAA_DBG_CLASSES & DBG_CLASS_TEMP)
#define SAA_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SAA_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SAA_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define SAA_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SAA_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define SAA_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define SAA_TEMP_LOGDUMP8(_StartAddr, _Length)
#define SAA_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define AAA related debug classes */
#if (AAA_DBG_CLASSES & DBG_CLASS_ERROR)
#define AAA_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define AAA_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define AAA_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_ERROR_LOGDUMP8(_StartAddr, _Length)
#define AAA_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AAA_DBG_CLASSES & DBG_CLASS_WARN)
#define AAA_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define AAA_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define AAA_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_WARN_LOGDUMP8(_StartAddr, _Length)
#define AAA_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AAA_DBG_CLASSES & DBG_CLASS_STATE)
#define AAA_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define AAA_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define AAA_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_STATE_LOGDUMP8(_StartAddr, _Length)
#define AAA_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AAA_DBG_CLASSES & DBG_CLASS_EVENT)
#define AAA_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define AAA_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define AAA_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_EVENT_LOGDUMP8(_StartAddr, _Length)
#define AAA_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AAA_DBG_CLASSES & DBG_CLASS_TRACE)
#define AAA_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define AAA_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define AAA_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_TRACE_LOGDUMP8(_StartAddr, _Length)
#define AAA_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AAA_DBG_CLASSES & DBG_CLASS_INFO)
#define AAA_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define AAA_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define AAA_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_INFO_LOGDUMP8(_StartAddr, _Length)
#define AAA_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AAA_DBG_CLASSES & DBG_CLASS_LOUD)
#define AAA_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define AAA_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define AAA_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_LOUD_LOGDUMP8(_StartAddr, _Length)
#define AAA_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (AAA_DBG_CLASSES & DBG_CLASS_TEMP)
#define AAA_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define AAA_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_AAA_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define AAA_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_AAA_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define AAA_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define AAA_TEMP_LOGDUMP8(_StartAddr, _Length)
#define AAA_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define P2P related debug classes */
#if (P2P_DBG_CLASSES & DBG_CLASS_ERROR)
#define P2P_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define P2P_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define P2P_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_ERROR_LOGDUMP8(_StartAddr, _Length)
#define P2P_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (P2P_DBG_CLASSES & DBG_CLASS_WARN)
#define P2P_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define P2P_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define P2P_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_WARN_LOGDUMP8(_StartAddr, _Length)
#define P2P_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (P2P_DBG_CLASSES & DBG_CLASS_STATE)
#define P2P_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define P2P_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define P2P_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_STATE_LOGDUMP8(_StartAddr, _Length)
#define P2P_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (P2P_DBG_CLASSES & DBG_CLASS_EVENT)
#define P2P_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define P2P_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define P2P_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_EVENT_LOGDUMP8(_StartAddr, _Length)
#define P2P_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (P2P_DBG_CLASSES & DBG_CLASS_TRACE)
#define P2P_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define P2P_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define P2P_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_TRACE_LOGDUMP8(_StartAddr, _Length)
#define P2P_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (P2P_DBG_CLASSES & DBG_CLASS_INFO)
#define P2P_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define P2P_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define P2P_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_INFO_LOGDUMP8(_StartAddr, _Length)
#define P2P_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (P2P_DBG_CLASSES & DBG_CLASS_LOUD)
#define P2P_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define P2P_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define P2P_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_LOUD_LOGDUMP8(_StartAddr, _Length)
#define P2P_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (P2P_DBG_CLASSES & DBG_CLASS_TEMP)
#define P2P_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define P2P_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_P2P_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define P2P_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_P2P_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define P2P_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define P2P_TEMP_LOGDUMP8(_StartAddr, _Length)
#define P2P_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define QM related debug classes */
#if (QM_DBG_CLASSES & DBG_CLASS_ERROR)
#define QM_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define QM_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define QM_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define QM_ERROR_LOGDUMP8(_StartAddr, _Length)
#define QM_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (QM_DBG_CLASSES & DBG_CLASS_WARN)
#define QM_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define QM_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define QM_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define QM_WARN_LOGDUMP8(_StartAddr, _Length)
#define QM_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (QM_DBG_CLASSES & DBG_CLASS_STATE)
#define QM_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define QM_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define QM_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define QM_STATE_LOGDUMP8(_StartAddr, _Length)
#define QM_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (QM_DBG_CLASSES & DBG_CLASS_EVENT)
#define QM_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define QM_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define QM_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define QM_EVENT_LOGDUMP8(_StartAddr, _Length)
#define QM_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (QM_DBG_CLASSES & DBG_CLASS_TRACE)
#define QM_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define QM_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define QM_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define QM_TRACE_LOGDUMP8(_StartAddr, _Length)
#define QM_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (QM_DBG_CLASSES & DBG_CLASS_INFO)
#define QM_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define QM_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define QM_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define QM_INFO_LOGDUMP8(_StartAddr, _Length)
#define QM_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (QM_DBG_CLASSES & DBG_CLASS_LOUD)
#define QM_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define QM_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define QM_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define QM_LOUD_LOGDUMP8(_StartAddr, _Length)
#define QM_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (QM_DBG_CLASSES & DBG_CLASS_TEMP)
#define QM_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define QM_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_QM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define QM_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_QM_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define QM_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define QM_TEMP_LOGDUMP8(_StartAddr, _Length)
#define QM_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define SEC related debug classes */
#if (SEC_DBG_CLASSES & DBG_CLASS_ERROR)
#define SEC_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define SEC_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define SEC_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_ERROR_LOGDUMP8(_StartAddr, _Length)
#define SEC_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SEC_DBG_CLASSES & DBG_CLASS_WARN)
#define SEC_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define SEC_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define SEC_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_WARN_LOGDUMP8(_StartAddr, _Length)
#define SEC_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SEC_DBG_CLASSES & DBG_CLASS_STATE)
#define SEC_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define SEC_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define SEC_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_STATE_LOGDUMP8(_StartAddr, _Length)
#define SEC_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SEC_DBG_CLASSES & DBG_CLASS_EVENT)
#define SEC_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define SEC_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define SEC_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_EVENT_LOGDUMP8(_StartAddr, _Length)
#define SEC_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SEC_DBG_CLASSES & DBG_CLASS_TRACE)
#define SEC_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define SEC_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define SEC_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_TRACE_LOGDUMP8(_StartAddr, _Length)
#define SEC_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SEC_DBG_CLASSES & DBG_CLASS_INFO)
#define SEC_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define SEC_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define SEC_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_INFO_LOGDUMP8(_StartAddr, _Length)
#define SEC_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SEC_DBG_CLASSES & DBG_CLASS_LOUD)
#define SEC_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define SEC_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define SEC_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_LOUD_LOGDUMP8(_StartAddr, _Length)
#define SEC_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (SEC_DBG_CLASSES & DBG_CLASS_TEMP)
#define SEC_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define SEC_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_SEC_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define SEC_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_SEC_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define SEC_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define SEC_TEMP_LOGDUMP8(_StartAddr, _Length)
#define SEC_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define BOW related debug classes */
#if (BOW_DBG_CLASSES & DBG_CLASS_ERROR)
#define BOW_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define BOW_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define BOW_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_ERROR_LOGDUMP8(_StartAddr, _Length)
#define BOW_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BOW_DBG_CLASSES & DBG_CLASS_WARN)
#define BOW_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define BOW_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define BOW_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_WARN_LOGDUMP8(_StartAddr, _Length)
#define BOW_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BOW_DBG_CLASSES & DBG_CLASS_STATE)
#define BOW_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define BOW_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define BOW_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_STATE_LOGDUMP8(_StartAddr, _Length)
#define BOW_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BOW_DBG_CLASSES & DBG_CLASS_EVENT)
#define BOW_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define BOW_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define BOW_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_EVENT_LOGDUMP8(_StartAddr, _Length)
#define BOW_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BOW_DBG_CLASSES & DBG_CLASS_TRACE)
#define BOW_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define BOW_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define BOW_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_TRACE_LOGDUMP8(_StartAddr, _Length)
#define BOW_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BOW_DBG_CLASSES & DBG_CLASS_INFO)
#define BOW_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define BOW_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define BOW_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_INFO_LOGDUMP8(_StartAddr, _Length)
#define BOW_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BOW_DBG_CLASSES & DBG_CLASS_LOUD)
#define BOW_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define BOW_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define BOW_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_LOUD_LOGDUMP8(_StartAddr, _Length)
#define BOW_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (BOW_DBG_CLASSES & DBG_CLASS_TEMP)
#define BOW_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define BOW_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_BOW_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define BOW_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_BOW_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define BOW_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define BOW_TEMP_LOGDUMP8(_StartAddr, _Length)
#define BOW_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define WAPI related debug classes */
#if (WAPI_DBG_CLASSES & DBG_CLASS_ERROR)
#define WAPI_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define WAPI_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define WAPI_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_ERROR_LOGDUMP8(_StartAddr, _Length)
#define WAPI_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (WAPI_DBG_CLASSES & DBG_CLASS_WARN)
#define WAPI_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define WAPI_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define WAPI_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_WARN_LOGDUMP8(_StartAddr, _Length)
#define WAPI_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (WAPI_DBG_CLASSES & DBG_CLASS_STATE)
#define WAPI_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define WAPI_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define WAPI_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_STATE_LOGDUMP8(_StartAddr, _Length)
#define WAPI_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (WAPI_DBG_CLASSES & DBG_CLASS_EVENT)
#define WAPI_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define WAPI_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define WAPI_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_EVENT_LOGDUMP8(_StartAddr, _Length)
#define WAPI_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (WAPI_DBG_CLASSES & DBG_CLASS_TRACE)
#define WAPI_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define WAPI_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define WAPI_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_TRACE_LOGDUMP8(_StartAddr, _Length)
#define WAPI_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (WAPI_DBG_CLASSES & DBG_CLASS_INFO)
#define WAPI_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define WAPI_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define WAPI_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_INFO_LOGDUMP8(_StartAddr, _Length)
#define WAPI_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (WAPI_DBG_CLASSES & DBG_CLASS_LOUD)
#define WAPI_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define WAPI_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define WAPI_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_LOUD_LOGDUMP8(_StartAddr, _Length)
#define WAPI_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (WAPI_DBG_CLASSES & DBG_CLASS_TEMP)
#define WAPI_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define WAPI_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_WAPI_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define WAPI_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_WAPI_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define WAPI_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define WAPI_TEMP_LOGDUMP8(_StartAddr, _Length)
#define WAPI_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

/* Define ROAMING related debug classes */
#if (ROAMING_DBG_CLASSES & DBG_CLASS_ERROR)
#define ROAMING_ERROR_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_ERROR_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#define ROAMING_ERROR_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_ERROR, _StartAddr, _Length)
#else
#define ROAMING_ERROR_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_ERROR_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_ERROR_LOGDUMP32(_StartAddr, _Length)
#endif

#if (ROAMING_DBG_CLASSES & DBG_CLASS_WARN)
#define ROAMING_WARN_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_WARN_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#define ROAMING_WARN_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_WARN, _StartAddr, _Length)
#else
#define ROAMING_WARN_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_WARN_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_WARN_LOGDUMP32(_StartAddr, _Length)
#endif

#if (ROAMING_DBG_CLASSES & DBG_CLASS_STATE)
#define ROAMING_STATE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_STATE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#define ROAMING_STATE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_STATE, _StartAddr, _Length)
#else
#define ROAMING_STATE_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_STATE_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_STATE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (ROAMING_DBG_CLASSES & DBG_CLASS_EVENT)
#define ROAMING_EVENT_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_EVENT_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#define ROAMING_EVENT_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_EVENT, _StartAddr, _Length)
#else
#define ROAMING_EVENT_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_EVENT_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_EVENT_LOGDUMP32(_StartAddr, _Length)
#endif

#if (ROAMING_DBG_CLASSES & DBG_CLASS_TRACE)
#define ROAMING_TRACE_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_TRACE_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#define ROAMING_TRACE_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_TRACE, _StartAddr, _Length)
#else
#define ROAMING_TRACE_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_TRACE_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_TRACE_LOGDUMP32(_StartAddr, _Length)
#endif

#if (ROAMING_DBG_CLASSES & DBG_CLASS_INFO)
#define ROAMING_INFO_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_INFO_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#define ROAMING_INFO_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_INFO, _StartAddr, _Length)
#else
#define ROAMING_INFO_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_INFO_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_INFO_LOGDUMP32(_StartAddr, _Length)
#endif

#if (ROAMING_DBG_CLASSES & DBG_CLASS_LOUD)
#define ROAMING_LOUD_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_LOUD_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#define ROAMING_LOUD_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_LOUD, _StartAddr, _Length)
#else
#define ROAMING_LOUD_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_LOUD_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_LOUD_LOGDUMP32(_StartAddr, _Length)
#endif

#if (ROAMING_DBG_CLASSES & DBG_CLASS_TEMP)
#define ROAMING_TEMP_LOGFUNC(_Module, _Class, _Fmt) \
	DBGLOG_FUNC(_Module, _Class, _Fmt)
#define ROAMING_TEMP_LOGDUMP8(_StartAddr, _Length) \
	dumpMemory8(DBG_ROAMING_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#define ROAMING_TEMP_LOGDUMP32(_StartAddr, _Length) \
	dumpMemory32(DBG_ROAMING_IDX, DBG_CLASS_TEMP, _StartAddr, _Length)
#else
#define ROAMING_TEMP_LOGFUNC(_Module, _Class, _Fmt)
#define ROAMING_TEMP_LOGDUMP8(_StartAddr, _Length)
#define ROAMING_TEMP_LOGDUMP32(_StartAddr, _Length)
#endif

#if defined(LINUX)
#define DBG_PRINTF_64BIT_DEC    "lld"
#else				/* Windows */
#define DBG_PRINTF_64BIT_DEC    "I64d"
#endif

#define DBG_ALL_MODULE_IDX      0xFFFFFFFF

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* Define debug module index */
typedef enum _ENUM_DBG_MODULE_T {
	DBG_INIT_IDX = 0,	/* 0x00 *//* For driver initial */
	DBG_HAL_IDX,		/* 0x01 *//* For HAL(HW) Layer */
	DBG_INTR_IDX,		/* 0x02 *//* For Interrupt */
	DBG_REQ_IDX,		/* 0x03 */
	DBG_TX_IDX,		/* 0x04 */
	DBG_RX_IDX,		/* 0x05 */
	DBG_RFTEST_IDX,		/* 0x06 *//* For RF test mode */
	DBG_EMU_IDX,		/* 0x07 *//* Developer specific */

	DBG_SW1_IDX,		/* 0x08 *//* Developer specific */
	DBG_SW2_IDX,		/* 0x09 *//* Developer specific */
	DBG_SW3_IDX,		/* 0x0A *//* Developer specific */
	DBG_SW4_IDX,		/* 0x0B *//* Developer specific */

	DBG_HEM_IDX,		/* 0x0C *//* HEM */
	DBG_AIS_IDX,		/* 0x0D *//* AIS */
	DBG_RLM_IDX,		/* 0x0E *//* RLM */
	DBG_MEM_IDX,		/* 0x0F *//* RLM */
	DBG_CNM_IDX,		/* 0x10 *//* CNM */
	DBG_RSN_IDX,		/* 0x11 *//* RSN */
	DBG_BSS_IDX,		/* 0x12 *//* BSS */
	DBG_SCN_IDX,		/* 0x13 *//* SCN */
	DBG_SAA_IDX,		/* 0x14 *//* SAA */
	DBG_AAA_IDX,		/* 0x15 *//* AAA */
	DBG_P2P_IDX,		/* 0x16 *//* P2P */
	DBG_QM_IDX,		/* 0x17 *//* QUE_MGT */
	DBG_SEC_IDX,		/* 0x18 *//* SEC */
	DBG_BOW_IDX,		/* 0x19 *//* BOW */
	DBG_WAPI_IDX,		/* 0x1A *//* WAPI */
	DBG_ROAMING_IDX,	/* 0x1B *//* ROAMING */
	DBG_TDLS_IDX,		/* 0x1C *//* TDLS *//* CFG_SUPPORT_TDLS */
	DBG_MODULE_NUM		/* Notice the XLOG check */
} ENUM_DBG_MODULE_T;

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
#define MACSTR          "%02x:%02x:%02x:%02x:%02x:%02x"

/* Debug print argument for the MAC Address */
#define MAC2STR(a)      ((PUINT_8)a)[0], ((PUINT_8)a)[1], ((PUINT_8)a)[2], \
			((PUINT_8)a)[3], ((PUINT_8)a)[4], ((PUINT_8)a)[5]

/* Debug print format string for the IPv4 Address */
#define IPV4STR         "%u.%u.%u.%u"

/* Debug print argument for the IPv4 Address */
#define IPV4TOSTR(a)    ((PUINT_8)a)[0], ((PUINT_8)a)[1], ((PUINT_8)a)[2], ((PUINT_8)a)[3]

/* Debug print format string for the MAC Address */
#define IPV6STR         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"

/* Debug print argument for the MAC Address */
#define IPV6TOSTR(a)    ((PUINT_8)a)[0], ((PUINT_8)a)[1], ((PUINT_8)a)[2], ((PUINT_8)a)[3], \
			((PUINT_8)a)[4], ((PUINT_8)a)[5], ((PUINT_8)a)[6], ((PUINT_8)a)[7], \
			((PUINT_8)a)[8], ((PUINT_8)a)[9], ((PUINT_8)a)[10], ((PUINT_8)a)[11], \
			((PUINT_8)a)[12], ((PUINT_8)a)[13], ((PUINT_8)a)[14], ((PUINT_8)a)[15]


/* The pre-defined format to dump the value of a varaible with its name shown. */
#define DUMPVAR(variable, format)           (#variable " = " format "\n", variable)

/* The pre-defined format to dump the MAC type value with its name shown. */
#define DUMPMACADDR(addr)                   (#addr " = " MACSTR "\n", MAC2STR(addr))


/* Basiclly, we just do renaming of KAL functions although they should
 * be defined as "Nothing to do" if DBG=0. But in some compiler, the macro
 * syntax does not support  #define LOG_FUNC(x,...)
 *
 * A caller shall not invoke these three macros when DBG=0.
 */
#define LOG_FUNC(_Fmt...)                kalDbgLog(DBG_CLASS_INFO, (_Fmt));

/* If __FUNCTION__ is already defined by compiler, we just use it. */
#define DEBUGFUNC(_Func)
/* Disabled due to AOSP
#if defined(__FUNCTION__)
#define DEBUGFUNC(_Func)
#else
    #define DEBUGFUNC(_Func) static const char __FUNCTION__[] = _Func;
#endif
*/

#define DBGLOG_FUNC(_Module, _Class, _Fmt) \
{ \
	if (aucDebugModule[DBG_##_Module##_IDX] & DBG_CLASS_##_Class) { \
		if ((DBG_CLASS_##_Class == DBG_CLASS_ERROR) || (DBG_CLASS_##_Class == DBG_CLASS_WARN)) { \
			kalDbgLog(DBG_CLASS_##_Class, ("%s:%u (" #_Module " " #_Class ") ", __func__, __LINE__)); \
		} \
		kalDbgLog(DBG_CLASS_##_Class, _Fmt); \
	} \
}

#define DBGFUNC(_DbgIdx, _DbgClass, _Fmt) \
{ \
	if (aucDebugModule[(_DbgIdx)] & (_DbgClass)) { \
		kalDbgLog((_DbgClass), _Fmt); \
	} \
}

#if DBG_DISABLE_ALL_LOG
#define DBGLOG(_Module, _Class, _Fmt)
#define DBGLOG_MEM8(_Module, _Class, _StartAddr, _Length)
#define DBGLOG_MEM32(_Module, _Class, _StartAddr, _Length)
#else
#define DBGLOG(_Module, _Class, _Fmt) \
{ \
	_Module##_##_Class##_LOGFUNC(_Module, _Class, _Fmt); \
}

#define DBGLOG_MEM8(_Module, _Class, _StartAddr, _Length) \
{ \
	_Module##_##_Class##_LOGFUNC( \
		_Module, _Class, ("(" #_Module " " #_Class ") Dump \"" #_StartAddr "\" in %s\n", __func__));\
	_Module##_##_Class##_LOGDUMP8((PUINT_8)(_StartAddr), (UINT_32)(_Length)); \
}

#define DBGLOG_MEM32(_Module, _Class, _StartAddr, _Length) \
{ \
	_Module##_##_Class##_LOGFUNC( \
		_Module, _Class, ("(" #_Module " " #_Class ") Dump \"" #_StartAddr "\" in %s", __func__));\
	_Module##_##_Class##_LOGDUMP32((PUINT_32)(_StartAddr), (UINT_32)(_Length)); \
}
#endif

#define DISP_STRING(_str)       _str

#undef ASSERT
#undef ASSERT_REPORT

#if (BUILD_QA_DBG || DBG)
#ifdef _lint
#define ASSERT(_exp) \
		{ \
			if (!(_exp)) { \
				do {} while (1); \
			} \
		}

#define ASSERT_REPORT(_exp, _fmt) \
	    { \
		LOG_FUNC("Assertion failed: %s:%d (%s)\n", __FILE__, __LINE__, #_exp); \
		LOG_FUNC _fmt; \
			if (!(_exp)) { \
				do {} while (1); \
			} \
	    }
#elif defined(WINDOWS_CE)
#define UNICODE_TEXT(_msg)  TEXT(_msg)
#define ASSERT(_exp) \
		{ \
			if (!(_exp) && !fgIsBusAccessFailed) { \
				TCHAR rUbuf[256]; \
				kalBreakPoint(); \
				_stprintf(rUbuf, TEXT("Assertion failed: %s:%d %s\n"), \
				UNICODE_TEXT(__FILE__), \
				__LINE__, \
				UNICODE_TEXT(#_exp)); \
				MessageBox(NULL, rUbuf, TEXT("ASSERT!"), MB_OK); \
			} \
		}

#define ASSERT_REPORT(_exp, _fmt) \
		{ \
			if (!(_exp) && !fgIsBusAccessFailed) { \
				TCHAR rUbuf[256]; \
				kalBreakPoint(); \
				_stprintf(rUbuf, TEXT("Assertion failed: %s:%d %s\n"), \
				UNICODE_TEXT(__FILE__), \
				__LINE__, \
				UNICODE_TEXT(#_exp)); \
				MessageBox(NULL, rUbuf, TEXT("ASSERT!"), MB_OK); \
			} \
		}
#else
#define ASSERT(_exp) \
		{ \
			if (!(_exp) && !fgIsBusAccessFailed) { \
				LOG_FUNC("Assertion failed: %s:%d (%s)\n", __FILE__, __LINE__, #_exp); \
			} \
		}

#define ASSERT_REPORT(_exp, _fmt) \
		{ \
			if (!(_exp) && !fgIsBusAccessFailed) { \
				LOG_FUNC("Assertion failed: %s:%d (%s)\n", __FILE__, __LINE__, #_exp); \
				LOG_FUNC _fmt; \
			} \
		}
#endif				/* WINDOWS_CE */
#else
#define ASSERT(_exp)
#define ASSERT_REPORT(_exp, _fmt)
#endif				/* BUILD_QA_DBG */

/* The following macro is used for debugging packed structures. */
#ifndef DATA_STRUC_INSPECTING_ASSERT
#define DATA_STRUC_INSPECTING_ASSERT(expr) \
		{switch (0) {case 0: case (expr): default:; } }
#endif

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

VOID
dumpMemory8(IN UINT_32 u4DbgIndex,
	    IN UINT_8 ucDbgClass, IN PUINT_8 pucStartAddr, IN UINT_32 u4Length);

VOID
dumpMemory32(IN UINT_32 u4DbgIndex,
	     IN UINT_8 ucDbgClass, IN PUINT_32 pu4StartAddr, IN UINT_32 u4Length);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif				/* _DEBUG_H */
