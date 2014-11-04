#ifndef __QUEUE_H_INCLUDED__
#define __QUEUE_H_INCLUDED__

/*
 * This file is included on every cell {SP,ISP,host} and is system agnostic
 *
 * System and cell specific interfaces and inline code are included
 * conditionally through Makefile path settings.
 *
 *  - .        system and cell agnostic interfaces, constants and identifiers
 *	- public:  cell specific interfaces
 *	- private: cell specific inline implementations
 *	- global:  inter cell constants and identifiers
 *	- local:   cell specific constants and identifiers
 *
 */

#include "storage_class.h"

#include "queue_local.h"

#ifndef __INLINE_QUEUE__
#define STORAGE_CLASS_QUEUE_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_QUEUE_C 
#include "queue_public.h"
#else  /* __INLINE_QUEUE__ */
#define STORAGE_CLASS_QUEUE_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_QUEUE_C STORAGE_CLASS_INLINE
#include "queue_private.h"
#endif /* __INLINE_QUEUE__ */

#endif /* __QUEUE_H_INCLUDED__ */
