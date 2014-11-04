#ifndef __EVENT_H_INCLUDED__
#define __EVENT_H_INCLUDED__

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
#include "event_local.h"

#ifndef __INLINE_EVENT__
#define STORAGE_CLASS_EVENT_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_EVENT_C 
#include "event_public.h"
#else  /* __INLINE_EVENT__ */
#define STORAGE_CLASS_EVENT_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_EVENT_C STORAGE_CLASS_INLINE
#include "event_private.h"
#endif /* __INLINE_EVENT__ */

#endif /* __EVENT_H_INCLUDED__ */
