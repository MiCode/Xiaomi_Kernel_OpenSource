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

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/cnm_timer.h#1
*/

/*
 * ! \file   cnm_timer.h
 *   \brief  Declaration of timer obj and related timer macro for setup time out
 *      event.
 *
 * In this file we declare the timer object and provide several macro for
 * Protocol functional blocks to setup their own time out event.
 */

#ifndef _CNM_TIMER_H
#define _CNM_TIMER_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#undef MSEC_PER_SEC
#define MSEC_PER_SEC                    1000
#undef USEC_PER_MSEC
#define USEC_PER_MSEC                   1000
#undef USEC_PER_SEC
#define USEC_PER_SEC                    1000000

#define USEC_PER_TU                     1024	/* microsecond */

#define MSEC_PER_MIN                    (60 * MSEC_PER_SEC)

#define MGMT_MAX_TIMEOUT_INTERVAL       ((UINT_32)0x7fffffff)

#define WAKE_LOCK_MAX_TIME              5	/* Unit: sec */

#define OBSS_SCAN_MIN_INTERVAL  10	/* In unit of sec */
/*
 * If WAKE_LOCK_MAX_TIME is too large, the whole system may always keep awake
 * because of periodic timer of OBSS scanning
 */
#if (WAKE_LOCK_MAX_TIME >= OBSS_SCAN_MIN_INTERVAL)
#error WAKE_LOCK_MAX_TIME is too large
#endif

enum ENUM_TIMER_WAKELOCK_TYPE_T {
	TIMER_WAKELOCK_AUTO,
	TIMER_WAKELOCK_NONE,
	TIMER_WAKELOCK_REQUEST,
	TIMER_WAKELOCK_NUM
};

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef VOID(*PFN_MGMT_TIMEOUT_FUNC) (P_ADAPTER_T, ULONG);

typedef struct _TIMER_T {
	LINK_ENTRY_T rLinkEntry;
	OS_SYSTIME rExpiredSysTime;
	UINT_16 u2Minutes;
	UINT_16 u2Reserved;
	ULONG ulDataPtr;
	PFN_MGMT_TIMEOUT_FUNC pfMgmtTimeOutFunc;
	enum ENUM_TIMER_WAKELOCK_TYPE_T eType;
	PUINT_8 pucFileAndLineForNextTimer;
	PUINT_8 pucFileAndLineOwn;
} TIMER_T, *P_TIMER_T;

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
/* Check if time "a" is before time "b" */
/* In 32-bit variable, 0x00000001~0x7fffffff -> positive number,
 *                     0x80000000~0xffffffff -> negative number
 */
#define TIME_BEFORE_64bit(a, b)       (a < b)

#define TIME_BEFORE(a, b)        ((UINT_32)((UINT_32)(a) - (UINT_32)(b)) > 0x7fffffff)

/* #define TIME_BEFORE(a,b)        ((INT_32)((INT_32)(b) - (INT_32)(a)) > 0)
 * may cause UNexpect result between Free build and Check build for WinCE
 */

#define TIME_AFTER(a, b)         TIME_BEFORE(b, a)

#define SYSTIME_TO_SEC(_systime)            ((_systime) / KAL_HZ)
#define SEC_TO_SYSTIME(_sec)                ((_sec) * KAL_HZ)

/* The macros to convert second & millisecond */
#define MSEC_TO_SEC(_msec)                  ((_msec) / MSEC_PER_SEC)
#define SEC_TO_MSEC(_sec)                   ((UINT_32)(_sec) * MSEC_PER_SEC)

/* The macros to convert millisecond & microsecond */
#define USEC_TO_MSEC(_usec)                 ((_usec) / USEC_PER_MSEC)
#define MSEC_TO_USEC(_msec)                 ((UINT_32)(_msec) * USEC_PER_MSEC)

/* The macros to convert TU & microsecond, TU & millisecond */
#define TU_TO_USEC(_tu)                     ((_tu) * USEC_PER_TU)
#define TU_TO_MSEC(_tu)                     USEC_TO_MSEC(TU_TO_USEC(_tu))

/* The macros to convert TU & & OS system time, round up by 0.5 */
#define TU_TO_SYSTIME(_tu)                  MSEC_TO_SYSTIME(TU_TO_MSEC(_tu))
#define SYSTIME_TO_TU(_systime)             \
	((SYSTIME_TO_USEC(_systime) + ((USEC_PER_TU / 2) - 1)) / USEC_PER_TU)

/* The macros to convert OS system time & microsecond */
#define SYSTIME_TO_USEC(_systime)           (((_systime) * USEC_PER_SEC) / KAL_HZ)

/* The macro to get the current OS system time */
#define GET_CURRENT_SYSTIME(_systime_p)     {*(_systime_p) = kalGetTimeTick(); }

/* The macro to copy the system time */
#define COPY_SYSTIME(_destTime, _srcTime)   {(_destTime) = (_srcTime); }

/* The macro to get the system time difference between t1 and t2 (t1 - t2) */
/*
 * #define GET_SYSTIME_DIFFERENCE(_time1, _time2, _diffTime) \
 *   (_diffTime) = (_time1) - (_time2)
 */

/* The macro to check for the expiration, if TRUE means _currentTime >= _expirationTime */
#define CHECK_FOR_EXPIRATION(_currentTime, _expirationTime) \
	(((UINT_32)(_currentTime) - (UINT_32)(_expirationTime)) <= 0x7fffffffUL)

/* The macro to check for the timeout */
#define CHECK_FOR_TIMEOUT(_currentTime, _timeoutStartingTime, _timeout) \
	CHECK_FOR_EXPIRATION((_currentTime), ((_timeoutStartingTime) + (_timeout)))

/* The macro to set the expiration time with a specified timeout *//* Watch out for round up. */
#define SET_EXPIRATION_TIME(_expirationTime, _timeout) \
	{ \
	    GET_CURRENT_SYSTIME(&(_expirationTime)); \
	    (_expirationTime) += (OS_SYSTIME)(_timeout); \
	}

#define timerRenewTimer(adapter, tmr, interval) \
	timerStartTimer(adapter, tmr, interval, (tmr)->function, (tmr)->data)

#define MGMT_INIT_TIMER(_adapter_p, _timer, _callbackFunc) \
	timerInitTimer(_adapter_p, &(_timer), (UINT_32)(_callbackFunc))

/* for debugging the timer abnoral issue */
#define STRL(x) #x
#define STRLINE(x) STRL(x)
#define cnmTimerStartTimer(prAdapter, prTimer, u4TimeoutMs) \
	cnmTimerStartTimerx(prAdapter, prTimer, u4TimeoutMs, __FILE__ STRLINE(__LINE__))
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID cnmTimerInitialize(IN P_ADAPTER_T prAdapter);

VOID cnmTimerDestroy(IN P_ADAPTER_T prAdapter);

VOID cnmTimerInitTimerOption(IN P_ADAPTER_T prAdapter,
			     IN P_TIMER_T prTimer,
			     IN PFN_MGMT_TIMEOUT_FUNC pfFunc,
			     IN ULONG ulDataPtr,
			     IN enum ENUM_TIMER_WAKELOCK_TYPE_T eType);

VOID cnmTimerStopTimer(IN P_ADAPTER_T prAdapter, IN P_TIMER_T prTimer);

VOID cnmTimerStartTimerx(IN P_ADAPTER_T prAdapter,
			 IN P_TIMER_T prTimer, IN UINT_32 u4TimeoutMs, IN PUINT_8 fileAndLine);

VOID cnmTimerDoTimeOutCheck(IN P_ADAPTER_T prAdapter);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static __KAL_INLINE__ INT_32 timerPendingTimer(IN P_TIMER_T prTimer)
{
	ASSERT(prTimer);

	return prTimer->rLinkEntry.prNext != NULL;
}

static __KAL_INLINE__ VOID cnmTimerInitTimer(IN P_ADAPTER_T prAdapter,
					     IN P_TIMER_T prTimer,
					     IN PFN_MGMT_TIMEOUT_FUNC pfFunc,
					     IN ULONG ulDataPtr)
{
	cnmTimerInitTimerOption(prAdapter, prTimer, pfFunc, ulDataPtr, TIMER_WAKELOCK_AUTO);
}

#endif /* _CNM_TIMER_H */
