/****************************************************************
 *
 * Time   : 2012-09-06, 11:16.
 * Author : zhengjie.lu@intel.com
 * Comment:
 * - Initial version.
 *
 ****************************************************************/

#ifndef __SW_EVENT_H_INCLUDED__
#define __SW_EVENT_H_INCLUDED__

/*
 * This file is included on every cell {SP,ISP,host} and on every system
 * that uses the IRQ device. It defines the API to DLI bridge
 *
 * System and cell specific interfaces and inline code are included
 * conditionally through Makefile path settings.
 *
 *  - .        system and cell agnostic interfaces, constants and identifiers
 *	- public:  system agnostic, cell specific interfaces
 *	- private: system dependent, cell specific interfaces & inline implementations
 *	- global:  system specific constants and identifiers
 *	- local:   system and cell specific constants and identifiers
 */

#include "storage_class.h"

#include "system_local.h"
#include "sw_event_local.h"

#ifndef __INLINE_SW_EVENT__
#define STORAGE_CLASS_SW_EVENT_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_SW_EVENT_C 
#include "sw_event_public.h"
#else  /* __INLINE_SW_EVENT__ */
#define STORAGE_CLASS_SW_EVENT_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_SW_EVENT_C STORAGE_CLASS_INLINE
#include "sw_event_private.h"
#endif /* __INLINE_SW_EVENT__ */

#endif /* __SW_EVENT_H_INCLUDED__ */

