#ifndef __SOCKET_H_INCLUDED__
#define __SOCKET_H_INCLUDED__

/*
 * This file is included on every cell {SP,ISP,host} and on every system
 * that uses the DMA device. It defines the API to DLI bridge
 *
 * System and cell specific interfaces and inline code are included
 * conditionally through Makefile path settings.
 *
 *  - .        system and cell agnostic interfaces, constants and identifiers
 *	- public:  system agnostic, cell specific interfaces
 *	- private: system dependent, cell specific interfaces & inline implementations
 *	- global:  system specific constants and identifiers
 *	- local:   system and cell specific constants and identifiers
 *
 */

#include "storage_class.h"

#include "system_local.h"
#include "socket_local.h"

#ifndef __INLINE_SOCKET__
#define STORAGE_CLASS_SOCKET_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_SOCKET_C 
#include "socket_public.h"
#else  /* __INLINE_SOCKET__ */
#define STORAGE_CLASS_SOCKET_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_SOCKET_C STORAGE_CLASS_INLINE
#include "socket_private.h"
#endif /* __INLINE_SOCKET__ */

#endif /* __SOCKET_H_INCLUDED__ */
