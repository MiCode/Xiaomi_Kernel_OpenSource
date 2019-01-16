#ifndef __SI_COMMON_H__
#define __SI_COMMON_H__
#if defined(__KERNEL__)
#include "sii_hal.h"
#include "si_osdebug.h"
#include "si_app_devcap.h"
#else
#include "si_debug.h"
#endif
#include "si_platform.h"
typedef enum _SiiResultCodes_t {
	PLATFORM_SUCCESS = 0,
	SII_SUCCESS = 0,
	SII_ERR_FAIL,
	SII_ERR_INVALID_PARAMETER,
	SII_ERR_IN_USE,
	SII_ERR_NOT_AVAIL,
} SiiResultCodes_t;
typedef enum {
	SiiSYSTEM_NONE = 0,
	SiiSYSTEM_SINK,
	SiiSYSTEM_SWITCH,
	SiiSYSTEM_SOURCE,
	SiiSYSTEM_REPEATER,
} SiiSystemTypes_t;
#define YES                         1
#define NO                          0
#ifdef NEVER
uint8_t SiiTimerExpired(uint8_t timer);
long SiiTimerElapsed(uint8_t index);
long SiiTimerTotalElapsed(void);
void SiiTimerWait(uint16_t m_sec);
void SiiTimerSet(uint8_t index, uint16_t m_sec);
void SiiTimerInit(void);
#endif
#endif
