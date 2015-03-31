/*
** =========================================================================
** Copyright (c) 2007-2009  Immersion Corporation.  All rights reserved.
**                          Immersion Corporation Confidential and Proprietary
** Copyright (C) 2015 XiaoMi, Inc.
**
** File:
**     ImmVibeOSInternal.h
**
** Description:
**     OS dependant constant and type definitions for
**     the Immersion TSP API (internal use).
**
** =========================================================================
*/
#ifndef _IMMVIBEOSINTERNAL_H
#define _IMMVIBEOSINTERNAL_H

#include <ImmVibeOS.h>

typedef VibeUInt32	VibeOSTimerHandle;
typedef int		VibeOSMutexHandle;
typedef char		VibeTChar;

#define _MAX_PATH	PATH_MAX

#define VIBE_OS_MUTEX_HANDLE_INVALID	0
#define	MAX_DEVICE					    1
#define VIBE_DESIGNERBRIDGE_CHUNK_SIZE  VIBE_BRIDGE_MAX_CHUNK_SIZE

#define VIBE_TEXT(text)	text

/* Timer callback */
typedef VibeBool (*VibeOSTimerProc)(void);

/* General OS functions */
extern VibeStatus VibeOSStartTimer(VibeUInt16 intervalMs, VibeOSTimerProc timerProc, VibeOSTimerHandle *pHandle);
extern VibeStatus VibeOSIsTimerRunning(void);
extern VibeStatus VibeOSStopTimer(VibeOSTimerHandle handle);
extern VibeUInt32 VibeOSGetTimeMs(void);

extern VibeOSMutexHandle VibeOSCreateMutex(const VibeTChar* szName);
extern VibeOSMutexHandle VibeOSCreateMutexAcquired(const VibeTChar* szName);
#define VibeOSCloseMutex(hMutex)
extern void VibeOSDestroyMutex(VibeOSMutexHandle hMutex);
extern VibeStatus VibeOSAcquireMutex(VibeOSMutexHandle hMutex);
extern VibeStatus VibeOSAcquireMutexWait(VibeOSMutexHandle hMutex, VibeUInt32 nTimeOutMs);
extern VibeStatus VibeOSAcquireMutexNoWait(VibeOSMutexHandle hMutex);
extern void VibeOSReleaseMutex(VibeOSMutexHandle hMutex);

#endif /* _IMMVIBEOSINTERNAL_H */
